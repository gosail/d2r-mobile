/*
 * A verified SWM2 v3 sidecar may contain two slot records, but the original
 * .d2s ALREADY owns and restores the slot marked native_slot. Reconstructing
 * BOTH from the sidecar would duplicate the vanilla mercenary and equipment.
 *
 * This read-only plan returns at most ONE extra slot to recreate after a
 * separately validated .d2s was loaded. It NEVER spawns a native monster,
 * materializes items, or changes the original save.
 */
#ifndef DM_DUALMERC_RESTORE_H
#define DM_DUALMERC_RESTORE_H
#include "dm_dualmerc_d2s.h"
typedef enum {
    DM_DUALMERC_RESTORE_REJECT = -1,
    DM_DUALMERC_RESTORE_NONE = 0,
    DM_DUALMERC_RESTORE_SECOND = 1
} dm_dualmerc_restore_result;
static inline dm_dualmerc_restore_result dm_dualmerc_restore_plan(
    const dm_dualmerc_saved_state *state,
    const dm_dualmerc_native_anchor *native, unsigned *other_slot)
{
    if (!state || !native || !other_slot ||
        !dm_dualmerc_state_valid(state) ||
        !dm_dualmerc_d2s_matches_state(native,state))
        return DM_DUALMERC_RESTORE_REJECT;
    unsigned vanilla=state->native_slot;
    if (vanilla>=DM_DUALMERC_SLOTS || !state->slot[vanilla].occupied)
        return DM_DUALMERC_RESTORE_REJECT;
    unsigned secondary=1u-vanilla;
    if (!state->slot[secondary].occupied)
        return DM_DUALMERC_RESTORE_NONE;
    if (state->slot[secondary].act == state->slot[vanilla].act)
        return DM_DUALMERC_RESTORE_REJECT;
    *other_slot=secondary;
    return DM_DUALMERC_RESTORE_SECOND;
}
#endif
