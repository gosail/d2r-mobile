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
/* After vanilla has loaded its own ONE hireling, cross-check the live
 * native pet roster before even preparing to create the sidecar-only second
 * unit. Matching the D2S bytes is NOT enough: the current game may already
 * contain a second mercenary, may have re-used a different player's unit,
 * or the native slot may have changed Act / alive status.
 *
 * A dead native hireling still consumes its Act slot. A missing dead-unit
 * pet node is NOT proof that its slot is vacant; fail closed instead.
 * This helper does not spawn anything, read item bits or alter game state.
 */
static inline dm_dualmerc_restore_result dm_dualmerc_restore_plan_for_live(
    const dm_dualmerc_saved_state *state,
    const dm_dualmerc_native_anchor *native,
    const dm_dualmerc_roster *live,
    unsigned *other_slot)
{
    if (!state || !native || !live || !other_slot ||
        !dm_dualmerc_live_ids_valid(live) ||
        !dm_dualmerc_state_valid(state) ||
        state->native_slot >= DM_DUALMERC_SLOTS)
        return DM_DUALMERC_RESTORE_REJECT;
    unsigned seen=0;
    const dm_dualmerc_saved_slot *owned=&state->slot[state->native_slot];
    for (unsigned i=0;i<DM_DUALMERC_SLOTS;i++) {
        if (!live->slot[i].occupied) continue;
        ++seen;
        if (live->slot[i].runtime_unit_id == UINT32_MAX ||
            live->slot[i].act != owned->act ||
            live->slot[i].monster_class != owned->monster_class ||
            live->slot[i].dead != owned->dead)
            return DM_DUALMERC_RESTORE_REJECT;
    }
    if (seen != 1) return DM_DUALMERC_RESTORE_REJECT;
    return dm_dualmerc_restore_plan(state,native,other_slot);
}
#endif
