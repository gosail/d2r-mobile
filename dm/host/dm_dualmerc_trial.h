/*
 * Strictly disposable-save-only server hire experiment. Pure logic.
 * Only the special offline trial Guest (SVC 0x94) may enable this code.
 * No persistence and no replacement-choice UI exist yet.
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

/* Until the two-slot native UI picker is implemented, refuse ALL further
 * hires rather than silently replacing the wrong hireling or losing gear.
 * The original game does not support selecting which of two gets replaced.
 */
static inline int dm_dualmerc_trial_block_equipped(
    unsigned count, unsigned inventory_verified, unsigned equipped_items) {
    return count == 1 && (!inventory_verified || equipped_items > 0);
}
static inline int dm_dualmerc_trial_block_full(unsigned count) {
    return count >= DM_DUALMERC_SLOTS;
}
#endif
