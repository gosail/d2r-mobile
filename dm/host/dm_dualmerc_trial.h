/*
 * Strictly disposable-save-only server hire experiment. Pure logic.
 * Only the special offline trial Guest (SVC 0x94) may enable this code.
 * No second-merc persistence/equipment UI exists; iPad has an experimental
 * replacement-choice alert, but it only chooses the native target.
 * In the game's native 0x687648 get-existing-pet call, a valid "keep" result
 * substitutes a NULL *only to that one caller*, preserving the original pet.
 * Game creation of a second pet still requires live engine validation.
 */
#ifndef DM_DUALMERC_TRIAL_H
#define DM_DUALMERC_TRIAL_H
#include "dm_dualmerc_policy.h"
#include "dm_merc_trace.h"

typedef struct {
    unsigned server_hirelings;   /* Must be exactly one; count validated. */
    unsigned old_act;            /* Native old hireling's monster class. */
    unsigned new_act;            /* Native candidate's monster class. */
    unsigned old_gear_status;    /* Require a valid complete empty inventory. */
    unsigned old_gear_count;
    unsigned new_gear_status;    /* Pre-registration candidate may have no inventory. */
    unsigned new_gear_count;
    unsigned old_alive;          /* Must be a live unit, not a corpse/dead flag. */
    unsigned native_result_matches_roster;
    unsigned guest_classes_valid;
} dm_dualmerc_trial_conditions;

/* 0x6c3130 returns the existing UNIT; player pet-list +0x98 stores
 * PET NODES. The only safe cross-reference is the unit ID, not a pointer
 * equality across these unrelated allocation types. */
static inline int dm_dualmerc_trial_pet_id_matches(uint32_t native_unit_id,
                                                    uint32_t pet_node_id) {
    return native_unit_id != UINT32_MAX && native_unit_id == pet_node_id;
}

/* Verified monster modes 12/13 represent dying/dead. A dead hireling
 * consumes its slot but is never eligible for this keep-alive experiment. */
static inline int dm_dualmerc_trial_mode_live(uint32_t mode) {
    return mode > 0 && mode < 12;
}

static inline int dm_dualmerc_trial_keep_one(const dm_dualmerc_trial_conditions *c) {
    if (!c || c->server_hirelings != 1 ||
        !c->native_result_matches_roster || !c->guest_classes_valid ||
        !c->old_alive || !dm_dualmerc_valid_act(c->old_act) ||
        !dm_dualmerc_valid_act(c->new_act) || c->old_act == c->new_act ||
        c->old_gear_status != DM_MERC_GEAR_OK || c->old_gear_count ||
        (c->new_gear_status != DM_MERC_GEAR_OK &&
         c->new_gear_status != DM_MERC_GEAR_ABSENT) ||
        c->new_gear_count) return 0;
    return 1;
}

/* The native singleton equipment and persistent-save paths are still unsafe
 * even though an iPad-only replacement-choice UI exists. Preserve the
 * equipped-merc rejection guard until the actual engine item bridge exists.
 */
static inline int dm_dualmerc_trial_block_equipped(
    unsigned count, unsigned inventory_verified, unsigned equipped_items) {
    return count == 1 && (!inventory_verified || equipped_items > 0);
}
/* After 0x6c1b00 completes, prove both distinct runtime unit IDs are
 * still present. A count of two alone can include unrelated stale records. */
static inline int dm_dualmerc_trial_both_present(
    unsigned count, uint32_t first, uint32_t second,
    uint32_t expected_old, uint32_t expected_new) {
    return count == DM_DUALMERC_SLOTS &&
           expected_old != UINT32_MAX && expected_new != UINT32_MAX &&
           expected_old != expected_new &&
           ((first == expected_old && second == expected_new) ||
            (first == expected_new && second == expected_old));
}
static inline int dm_dualmerc_trial_block_full(unsigned count) {
    return count >= DM_DUALMERC_SLOTS;
}

/* Verify a completed replacement by exact runtime IDs, independently of
 * native pet linked-list order. Count alone is insufficient: the original
 * selected ID must be gone, the other merc must still be present, and a
 * different live unit with the requested Act must have appeared.
 *
 * Call only after a synchronous native packet returned. This verifies
 * neither gold/item rollback nor persistence beyond this game session. */
static inline int dm_dualmerc_trial_replacement_complete(
    const dm_dualmerc_roster *after,
    uint32_t removed_id, uint32_t retained_id,
    unsigned retained_act, unsigned retained_dead, unsigned new_act)
{
    if (!after || !dm_dualmerc_roster_valid(after) ||
        !dm_dualmerc_valid_act(new_act) ||
        !dm_dualmerc_valid_act(retained_act) ||
        retained_act == new_act || retained_dead > 1 ||
        removed_id == UINT32_MAX || retained_id == UINT32_MAX ||
        removed_id == retained_id) return 0;
    unsigned retained=0, inserted=0;
    for (unsigned i=0; i<DM_DUALMERC_SLOTS; ++i) {
        const dm_dualmerc_slot *slot=&after->slot[i];
        if (!slot->occupied || slot->runtime_unit_id == UINT32_MAX ||
            slot->runtime_unit_id == removed_id) return 0;
        if (slot->runtime_unit_id == retained_id) {
            /* The surviving native unit must also preserve its exact
             * original Act and dead/alive state across the transaction.
             * A dead peer occupies a slot, but may not mutate here. */
            if (slot->act != retained_act ||
                slot->dead != retained_dead) return 0;
            ++retained;
        } else {
            /* The new hire must be a fresh, living native unit. */
            if (slot->act != new_act || slot->dead) return 0;
            ++inserted;
        }
    }
    return retained == 1 && inserted == 1;
}

/* A native singleton getpet call is about to unlink ONE of the two units.
 * Before overriding that result, establish exact target AND surviving peer
 * identities, both inventories' empty/verified status, target liveness and
 * candidate class. The original roster may be reordered; IDs are stable
 * only inside the current session. No mutation or guest pointer deref here.
 *
 * Peer may be dead: its Act slot still exists and must be preserved.
 * The selected target MUST be alive until a native dead-unit replacement
 * transaction has been independently verified. */
static inline int dm_dualmerc_trial_selected_target(
    const dm_dualmerc_roster *roster,
    const dm_merc_trace_gear gear[DM_DUALMERC_SLOTS],
    uint32_t selected_id, uint32_t peer_id, unsigned offered_act,
    uint32_t candidate_id, uint32_t candidate_class,
    unsigned *selected_slot)
{
    if (!roster || !gear || !selected_slot ||
        !dm_dualmerc_roster_valid(roster) ||
        !dm_dualmerc_valid_act(offered_act) ||
        dm_dualmerc_act_for_class(candidate_class) != offered_act ||
        selected_id == UINT32_MAX || peer_id == UINT32_MAX ||
        candidate_id == UINT32_MAX ||
        selected_id == peer_id || candidate_id == selected_id ||
        candidate_id == peer_id) return 0;
    int selected = -1, peer = -1;
    for (unsigned i=0; i<DM_DUALMERC_SLOTS; ++i) {
        const dm_dualmerc_slot *slot=&roster->slot[i];
        if (!slot->occupied || slot->runtime_unit_id == UINT32_MAX ||
            gear[i].status != DM_MERC_GEAR_OK || gear[i].count)
            return 0;
        if (slot->runtime_unit_id == selected_id) selected=(int)i;
        if (slot->runtime_unit_id == peer_id) peer=(int)i;
    }
    if (selected < 0 || peer < 0 || selected == peer ||
        roster->slot[selected].dead) return 0;
    dm_dualmerc_decision p=dm_dualmerc_plan(roster,offered_act,0);
    if (p.action != DM_DUALMERC_REPLACE_SAME_ACT &&
        p.action != DM_DUALMERC_CHOOSE_REPLACEMENT) return 0;
    if (!dm_dualmerc_choice_allowed(&p,(unsigned)selected)) return 0;
    *selected_slot=(unsigned)selected;
    return 1;
}
#endif
