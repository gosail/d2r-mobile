#include "../dm_dualmerc_trial.h"
#include <assert.h>
#include <stdio.h>

/* Before overriding the native singleton lookup, a two-merc roster must
 * have two exact identities and two verified empty equipment lists.
 * Reordering is harmless; an equipped peer, dead target or wrong Act
 * is NOT harmless and must fail before native unlink. */
static void selected_native_replacement(void) {
    dm_dualmerc_roster r={0};
    dm_merc_trace_gear gear[DM_DUALMERC_SLOTS]={
        {0,0,DM_MERC_GEAR_OK}, {0,0,DM_MERC_GEAR_OK}
    };
    unsigned chosen=99;
    r.slot[0]=(dm_dualmerc_slot){.occupied=1,.act=1,
        .runtime_unit_id=101,.dead=0};
    r.slot[1]=(dm_dualmerc_slot){.occupied=1,.act=2,
        .runtime_unit_id=202,.dead=0};
    assert(dm_dualmerc_trial_selected_target(&r,gear,101,202,5,404,560,&chosen));
    assert(chosen==0);
    assert(dm_dualmerc_trial_selected_target(&r,gear,202,101,2,404,338,&chosen));
    assert(chosen==1);
    dm_dualmerc_slot swap=r.slot[0];
    r.slot[0]=r.slot[1]; r.slot[1]=swap;
    assert(dm_dualmerc_trial_selected_target(&r,gear,101,202,5,404,560,&chosen));
    assert(chosen==1);
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,202,5,404,338,&chosen));
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,202,5,101,560,&chosen));
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,202,5,202,560,&chosen));
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,202,5,UINT32_MAX,560,&chosen));
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,101,5,404,560,&chosen));
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,303,5,404,560,&chosen));
    assert(!dm_dualmerc_trial_selected_target(&r,gear,UINT32_MAX,202,5,404,560,&chosen));
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,202,4,404,560,&chosen));
    gear[0].count=1; /* The OTHER merc now holds equipment. */
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,202,5,404,560,&chosen));
    gear[0].count=0;
    gear[0].status=DM_MERC_GEAR_INVALID;
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,202,5,404,560,&chosen));
    gear[0].status=DM_MERC_GEAR_OK;
    gear[1].status=DM_MERC_GEAR_ABSENT;
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,202,5,404,560,&chosen));
    gear[1].status=DM_MERC_GEAR_OK;
    r.slot[1].dead=1;
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,202,5,404,560,&chosen));
    r.slot[1].dead=0;
    r.slot[0].dead=1; /* Dead peer still occupies its Act slot. */
    assert(dm_dualmerc_trial_selected_target(&r,gear,101,202,5,404,560,&chosen));
    r.slot[0].dead=0;
    r.slot[0].act=1; /* Duplicate Act makes roster invalid. */
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,202,5,404,560,&chosen));
    r.slot[0].act=2;
    assert(!dm_dualmerc_trial_selected_target(NULL,gear,101,202,5,404,560,&chosen));
    assert(!dm_dualmerc_trial_selected_target(&r,NULL,101,202,5,404,560,&chosen));
    assert(!dm_dualmerc_trial_selected_target(&r,gear,101,202,5,404,560,NULL));
}

/* The server can reorder its pet list during native replacement;
 * validate by runtime identity, not node position. */
static void completed_native_replacement(void) {
    dm_dualmerc_roster r={0};
    r.slot[0]=(dm_dualmerc_slot){.occupied=1,.act=1,
        .runtime_unit_id=101,.dead=1};
    r.slot[1]=(dm_dualmerc_slot){.occupied=1,.act=5,
        .runtime_unit_id=303,.dead=0};
    assert(dm_dualmerc_trial_replacement_complete(&r,202,101,303,1,1,5));
    assert(!dm_dualmerc_trial_replacement_complete(&r,202,101,304,1,1,5));
    dm_dualmerc_slot first=r.slot[0];
    r.slot[0]=r.slot[1]; r.slot[1]=first;
    assert(dm_dualmerc_trial_replacement_complete(&r,202,101,303,1,1,5));
    assert(!dm_dualmerc_trial_replacement_complete(&r,303,101,303,1,1,5));
    assert(!dm_dualmerc_trial_replacement_complete(&r,202,303,303,1,1,5));
    assert(!dm_dualmerc_trial_replacement_complete(&r,202,101,303,1,1,2));
    assert(!dm_dualmerc_trial_replacement_complete(&r,101,101,303,1,1,5));
    assert(!dm_dualmerc_trial_replacement_complete(&r,UINT32_MAX,101,303,1,1,5));
    r.slot[0].dead=1; /* Newly hired unit must be living. */
    assert(!dm_dualmerc_trial_replacement_complete(&r,202,101,303,1,1,5));
    r.slot[0].dead=0;
    r.slot[1].dead=0; /* Survivor became alive without being replaced. */
    assert(!dm_dualmerc_trial_replacement_complete(&r,202,101,303,1,1,5));
    r.slot[1].dead=1;
    r.slot[1].act=3; /* Survivor unexpectedly changed its class/Act. */
    assert(!dm_dualmerc_trial_replacement_complete(&r,202,101,303,1,1,5));
    r.slot[1].act=1;
    r.slot[0].runtime_unit_id=202; /* Old target was never removed. */
    assert(!dm_dualmerc_trial_replacement_complete(&r,202,101,303,1,1,5));
    r.slot[0].runtime_unit_id=101; /* Duplicate IDs, no new unit. */
    assert(!dm_dualmerc_trial_replacement_complete(&r,202,101,303,1,1,5));
    r.slot[0].runtime_unit_id=303;
    r.slot[1].occupied=0;
    assert(!dm_dualmerc_trial_replacement_complete(&r,202,101,303,1,1,5));
    assert(!dm_dualmerc_trial_replacement_complete(NULL,202,101,303,1,1,5));
}

/* A second hire may not destroy the original Act slot even if the native
 * pet list order changes, or if a different unit of the same Act appears. */
static void completed_second_hire(void) {
    dm_dualmerc_roster r={0};
    r.slot[0]=(dm_dualmerc_slot){.occupied=1,.act=1,.dead=0,.runtime_unit_id=101};
    r.slot[1]=(dm_dualmerc_slot){.occupied=1,.act=2,.dead=0,.runtime_unit_id=202};
    assert(dm_dualmerc_trial_add_complete(&r,101,1,0,202,2));
    assert(!dm_dualmerc_trial_add_complete(&r,101,1,0,203,2));
    assert(!dm_dualmerc_trial_add_complete(&r,101,1,0,202,5));
    assert(!dm_dualmerc_trial_add_complete(&r,101,1,0,101,2));
    dm_dualmerc_slot tmp=r.slot[0];r.slot[0]=r.slot[1];r.slot[1]=tmp;
    assert(dm_dualmerc_trial_add_complete(&r,101,1,0,202,2));
    r.slot[1].dead=1; /* Original merc mysteriously died. */
    assert(!dm_dualmerc_trial_add_complete(&r,101,1,0,202,2));
    r.slot[1].dead=0;
    r.slot[0].dead=1; /* New merc cannot be a dead unit. */
    assert(!dm_dualmerc_trial_add_complete(&r,101,1,0,202,2));
    r.slot[0].dead=0;
    r.slot[0].runtime_unit_id=303; /* Replacement was NOT the candidate. */
    assert(!dm_dualmerc_trial_add_complete(&r,101,1,0,202,2));
    r.slot[0].runtime_unit_id=202;
    r.slot[1].act=3; /* Original's Act cannot change. */
    assert(!dm_dualmerc_trial_add_complete(&r,101,1,0,202,2));
    r.slot[1].act=1;
    r.slot[0].occupied=0;
    assert(!dm_dualmerc_trial_add_complete(&r,101,1,0,202,2));
    assert(!dm_dualmerc_trial_add_complete(NULL,101,1,0,202,2));
}

int main(void) {
    completed_second_hire();
    completed_native_replacement();
    selected_native_replacement();
    dm_dualmerc_trial_conditions c = {0};
    c.server_hirelings = 1;
    c.old_act = 1; c.new_act = 2;
    c.old_alive = 1; c.native_result_matches_roster = 1;
    c.guest_classes_valid = 1;
    c.old_gear_status = DM_MERC_GEAR_OK;
    c.new_gear_status = DM_MERC_GEAR_ABSENT;
    assert(dm_dualmerc_trial_keep_one(&c));
    /* The native result is a UNIT pointer and the roster is a PET NODE.
     * Match their unit IDs, never compare these two allocation addresses. */
    assert(dm_dualmerc_trial_pet_id_matches(0x1234, 0x1234));
    assert(!dm_dualmerc_trial_pet_id_matches(0x1234, 0x1235));
    assert(!dm_dualmerc_trial_pet_id_matches(UINT32_MAX, UINT32_MAX));
    assert(dm_dualmerc_trial_mode_live(1));
    assert(dm_dualmerc_trial_mode_live(11));
    assert(!dm_dualmerc_trial_mode_live(0));
    assert(!dm_dualmerc_trial_mode_live(12));
    assert(!dm_dualmerc_trial_mode_live(13));
    c.new_gear_status = DM_MERC_GEAR_OK;
    assert(dm_dualmerc_trial_keep_one(&c));
    c.new_gear_count = 1;
    assert(!dm_dualmerc_trial_keep_one(&c));
    c.new_gear_count = 0;
    c.old_gear_count = 1;
    assert(!dm_dualmerc_trial_keep_one(&c));
    c.old_gear_count = 0;
    c.old_gear_status = DM_MERC_GEAR_INVALID;
    assert(!dm_dualmerc_trial_keep_one(&c));
    c.old_gear_status = DM_MERC_GEAR_OK;
    c.old_act = 2; /* Nightmare Act II -> Hell Act II remains SAME act. */
    assert(!dm_dualmerc_trial_keep_one(&c));
    c.old_act = 1;
    c.server_hirelings = 2;
    assert(!dm_dualmerc_trial_keep_one(&c));
    c.server_hirelings = 1;
    c.old_alive = 0;
    assert(!dm_dualmerc_trial_keep_one(&c));
    c.old_alive = 1;
    c.native_result_matches_roster = 0;
    assert(!dm_dualmerc_trial_keep_one(&c));
    c.native_result_matches_roster = 1;
    c.guest_classes_valid = 0;
    assert(!dm_dualmerc_trial_keep_one(&c));
    assert(!dm_dualmerc_trial_keep_one(NULL));
    assert(!dm_dualmerc_trial_block_equipped(0, 0, 12));
    assert(dm_dualmerc_trial_block_equipped(1, 0, 0));
    assert(dm_dualmerc_trial_block_equipped(1, 1, 1));
    assert(!dm_dualmerc_trial_block_equipped(1, 1, 0));
    assert(!dm_dualmerc_trial_block_full(0));
    assert(!dm_dualmerc_trial_block_full(1));
    assert(dm_dualmerc_trial_block_full(2));
    assert(dm_dualmerc_trial_block_full(3));
    assert(dm_dualmerc_trial_both_present(2, 10, 20, 10, 20));
    assert(dm_dualmerc_trial_both_present(2, 20, 10, 10, 20));
    assert(!dm_dualmerc_trial_both_present(1, 10, 20, 10, 20));
    assert(!dm_dualmerc_trial_both_present(2, 10, 10, 10, 20));
    assert(!dm_dualmerc_trial_both_present(2, 10, 20, 10, 10));
    assert(!dm_dualmerc_trial_both_present(2, 10, 20, UINT32_MAX, 20));
    puts("dual merc native keep-one trial: guarded and full-roster fail-closed");
    return 0;
}
