#include "../dm_dualmerc_restore.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    dm_dualmerc_saved_state *s=calloc(1,sizeof *s);
    assert(s);
    s->native_slot=0;
    s->native_seed=0x12345678;
    s->native_d2s_checksum=0x1234;
    s->slot[0].occupied=1;
    s->slot[0].act=1;
    s->slot[0].monster_class=271;
    s->slot[0].level=20;
    s->slot[0].hireling_row=12;
    s->slot[0].name_id=8;
    s->slot[0].experience=100;
    dm_dualmerc_native_anchor a={
        .seed=s->native_seed,.checksum=s->native_d2s_checksum,
        .hireling_row=12,.name_id=8,.exp=100
    };
    unsigned chosen=99;
    assert(dm_dualmerc_restore_plan(s,&a,&chosen)==DM_DUALMERC_RESTORE_NONE);
    assert(chosen==99);
    s->slot[1].occupied=1;
    s->slot[1].act=2;
    s->slot[1].monster_class=338;
    s->slot[1].level=10;
    assert(dm_dualmerc_restore_plan(s,&a,&chosen)==DM_DUALMERC_RESTORE_SECOND);
    assert(chosen==1);
    chosen=99;
    s->slot[0].gear_count=1;
    s->slot[0].gear[0].size=3;
    s->slot[0].gear[0].body_slot=3;
    s->slot[0].gear[0].persistent_item_id=100;
    memcpy(s->slot[0].gear[0].data,"abc",3);
    s->slot[1].gear_count=1;
    s->slot[1].gear[0].size=3;
    s->slot[1].gear[0].body_slot=3;
    s->slot[1].gear[0].persistent_item_id=101;
    memcpy(s->slot[1].gear[0].data,"xyz",3);
    assert(dm_dualmerc_restore_plan(s,&a,&chosen)==DM_DUALMERC_RESTORE_SECOND);
    assert(chosen==1); /* Native slot zero is never recreated. */
    s->slot[1].gear[0].persistent_item_id=100;
    assert(dm_dualmerc_restore_plan(s,&a,&chosen)==DM_DUALMERC_RESTORE_REJECT);
    s->slot[1].gear[0].persistent_item_id=101;
    a.seed++;
    assert(dm_dualmerc_restore_plan(s,&a,&chosen)==DM_DUALMERC_RESTORE_REJECT);
    a.seed--;
    s->native_slot=1;
    a.hireling_row=0; a.name_id=0; a.exp=0;
    assert(dm_dualmerc_restore_plan(s,&a,&chosen)==DM_DUALMERC_RESTORE_SECOND);
    assert(chosen==0); /* Switch native and sidecar-only positions. */
    s->slot[1].act=1;
    assert(dm_dualmerc_restore_plan(s,&a,&chosen)==DM_DUALMERC_RESTORE_REJECT);
    assert(dm_dualmerc_restore_plan(NULL,&a,&chosen)==DM_DUALMERC_RESTORE_REJECT);
    free(s);
    puts("dualmerc native-vs-sidecar restore isolation: passed");
}
