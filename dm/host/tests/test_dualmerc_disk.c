#define _POSIX_C_SOURCE 200809L
#include "../dm_dualmerc_disk.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
    char root[] = "/tmp/dm-merc-XXXXXX";
    assert(mkdtemp(root));
    char hero[256];
    assert(snprintf(hero, sizeof hero, "%s/Hero", root) < (int)sizeof hero);
    assert(mkdir(hero, 0700) == 0);

    dm_dualmerc_saved_state *s = calloc(1, sizeof *s), *r = calloc(1, sizeof *r);
    assert(s && r);
    s->slot[0].occupied = 1; s->slot[0].act = 1;
    s->slot[0].monster_class = 271; s->slot[0].level = 20;
    s->slot[1].occupied = 1; s->slot[1].act = 2;
    s->slot[1].monster_class = 338; s->slot[1].level = 10;
    s->slot[1].gear_count = 1;
    s->slot[1].gear[0].body_slot = 0;
    s->slot[1].gear[0].size = 4;
    s->slot[1].gear[0].persistent_item_id = 0x12345678;
    memcpy(s->slot[1].gear[0].data, "GEAR", 4);
    s->native_slot = 0; s->generation = 7;
    s->native_d2s_checksum = 0xcafefeed;
    s->native_seed = 0x12345678;

    assert(dm_dualmerc_disk_save(root, "Hero", s) == 0);
    assert(dm_dualmerc_disk_load(root, "Hero", r) == 0);
    assert(!memcmp(s, r, sizeof *s));
    assert(dm_dualmerc_disk_save(root, "../Hero", s) == -1);
    assert(dm_dualmerc_disk_save(root, "NotPresent", s) == -1);

    r->generation = 77;
    char path[512];
    assert(snprintf(path, sizeof path, "%s/Hero.swmerc", hero) < (int)sizeof path);
    FILE *f = fopen(path, "r+b");
    assert(f);
    assert(fseek(f, 42, SEEK_SET) == 0);
    int c = fgetc(f);
    assert(c != EOF);
    assert(fseek(f, 42, SEEK_SET) == 0);
    assert(fputc(c ^ 1, f) != EOF);
    assert(fclose(f) == 0);
    assert(dm_dualmerc_disk_load(root, "Hero", r) == -1);
    assert(r->generation == 77);  /* failure cannot replace valid memory */

    assert(dm_dualmerc_disk_save(root, "Hero", s) == 0);
    assert(dm_dualmerc_disk_load(root, "Hero", r) == 0);
    assert(r->generation == 7);

    assert(unlink(path) == 0);
    assert(rmdir(hero) == 0);
    assert(rmdir(root) == 0);
    free(s); free(r);
    puts("dual mercenary atomic save-workdir sidecar: passed");
    return 0;
}
