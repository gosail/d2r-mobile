#include "../dm_dualmerc_trial.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
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
    puts("dual merc native keep-one trial: guarded and full-roster fail-closed");
    return 0;
}
