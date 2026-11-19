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
    /* Synthetic D2R v99 D2S with the authoritative first-merc seed.
     * Checksum and native-slot cross-validation must succeed before
     * a two-merc sidecar is allowed to write or load. */
    uint8_t d2s[DM_DUALMERC_D2S_HEADER_MIN] = {0};
    dm_dualmerc_store_u32(d2s,0xaa55aa55u);
    dm_dualmerc_store_u32(d2s+4,99);
    dm_dualmerc_store_u32(d2s+8,(uint32_t)sizeof d2s);
    dm_dualmerc_store_u32(d2s+179,0x12345678);
    dm_dualmerc_store_u32(d2s+12,dm_dualmerc_d2s_checksum(d2s,sizeof d2s));
    s->native_d2s_checksum = dm_dualmerc_load_u32(d2s+12);
    s->native_seed = 0x12345678;
    char native_path[512];
    assert(snprintf(native_path,sizeof native_path,"%s/Hero.d2s",hero)
           < (int)sizeof native_path);
    FILE *native=fopen(native_path,"wb");
    assert(native);
    assert(fwrite(d2s,1,sizeof d2s,native)==sizeof d2s);
    assert(fclose(native)==0);
    assert(dm_dualmerc_disk_save_verified(root, "Hero", s) == 0);
    assert(dm_dualmerc_disk_load_verified(root, "Hero", r) == 0);
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

    assert(dm_dualmerc_disk_save_verified(root, "Hero", s) == 0);
    assert(dm_dualmerc_disk_load_verified(root, "Hero", r) == 0);
    assert(r->generation == 7);

    /* If the vanilla save has changed without a new sidecar, the loader
     * must preserve the caller's existing in-memory state and fail. */
    r->generation = 999;
    d2s[300] ^= 1;
    native=fopen(native_path,"wb");
    assert(native && fwrite(d2s,1,sizeof d2s,native)==sizeof d2s);
    assert(fclose(native)==0);
    assert(dm_dualmerc_disk_load_verified(root,"Hero",r)==-1);
    assert(r->generation==999);
    assert(dm_dualmerc_disk_save_verified(root,"Hero",s)==-1);
    d2s[300] ^= 1;
    native=fopen(native_path,"wb");
    assert(native && fwrite(d2s,1,sizeof d2s,native)==sizeof d2s);
    assert(fclose(native)==0);
    assert(dm_dualmerc_disk_load_verified(root,"Hero",r)==0);
    assert(r->generation==7);
    s->native_seed++;
    assert(dm_dualmerc_disk_save_verified(root,"Hero",s)==-1);
    s->native_seed--;
    assert(dm_dualmerc_disk_save_verified(root,"Hero",s)==0);

    assert(unlink(path) == 0);
    assert(unlink(native_path) == 0);
    assert(rmdir(hero) == 0);
    assert(rmdir(root) == 0);
    free(s); free(r);
    puts("dual mercenary atomic save-workdir sidecar: passed");
    return 0;
}
