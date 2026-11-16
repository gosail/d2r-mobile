#include "../dm_dualmerc_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static dm_dualmerc_slot merc(unsigned act, unsigned difficulty, unsigned dead) {
    dm_dualmerc_slot s = {1, (uint8_t)act, (uint8_t)difficulty,
                          (uint8_t)dead, 100 + act};
    return s;
}
int main(void) {
    dm_dualmerc_roster r = {0};
    dm_dualmerc_decision d = dm_dualmerc_plan(&r, 1, 0);
    assert(d.action == DM_DUALMERC_ADD && d.slot == 0);
    r.slot[0] = merc(1, 0, 0);
    d = dm_dualmerc_plan(&r, 2, 2);
    assert(d.action == DM_DUALMERC_ADD && d.slot == 1);
    assert(dm_dualmerc_choice_allowed(&d, 1));
    r.slot[1] = merc(2, 1, 0);
    d = dm_dualmerc_plan(&r, 5, 0);
    assert(d.action == DM_DUALMERC_CHOOSE_REPLACEMENT && d.replace_mask == 3);
    assert(dm_dualmerc_choice_allowed(&d, 0) && dm_dualmerc_choice_allowed(&d, 1));
    assert(!dm_dualmerc_choice_allowed(&d, 2));
    d = dm_dualmerc_plan(&r, 2, 2); /* Different difficulty, same Act II. */
    assert(d.action == DM_DUALMERC_REPLACE_SAME_ACT && d.slot == 1);
    assert(!dm_dualmerc_choice_allowed(&d, 0) && dm_dualmerc_choice_allowed(&d, 1));
    d = dm_dualmerc_plan(&r, 1, 2);
    assert(d.action == DM_DUALMERC_REPLACE_SAME_ACT && d.slot == 0);
    r.slot[1].dead = 1;
    assert(dm_dualmerc_plan(&r, 3, 0).action == DM_DUALMERC_CHOOSE_REPLACEMENT);
    memset(&r, 0, sizeof(r));
    r.slot[0] = merc(2, 2, 1);
    assert(dm_dualmerc_plan(&r, 2, 0).action == DM_DUALMERC_REPLACE_SAME_ACT);
    assert(dm_dualmerc_plan(&r, 5, 1).action == DM_DUALMERC_ADD);
    r.slot[1] = merc(2, 0, 0); /* Invalid duplicate act. */
    assert(!dm_dualmerc_roster_valid(&r));
    assert(dm_dualmerc_plan(&r, 3, 0).action == DM_DUALMERC_REJECT);
    r.slot[1] = merc(3, 0, 0);
    assert(dm_dualmerc_plan(&r, 4, 0).action == DM_DUALMERC_REJECT);
    assert(dm_dualmerc_plan(&r, 3, 3).action == DM_DUALMERC_REJECT);
    assert(dm_dualmerc_plan(NULL, 3, 0).action == DM_DUALMERC_REJECT);
    assert(!dm_dualmerc_choice_allowed(NULL, 0));
    puts("dual mercenary roster policy: all assertions passed");
    return 0;
}
