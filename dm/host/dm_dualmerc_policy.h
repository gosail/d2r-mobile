/*
 * Offline two-hireling roster policy. Pure decision logic only: it does NOT
 * hook the game's native hire transaction, allocate a hireling, or serialize
 * equipment. Engine integration must happen server-side before the original
 * "replace current hireling" operation; never treat a UI-only check as safe.
 */
#ifndef DM_DUALMERC_POLICY_H
#define DM_DUALMERC_POLICY_H
#include <stdint.h>
#define DM_DUALMERC_SLOTS 2

/* These are acts which actually offer mercenaries in D2R; Act IV has none. */
static inline int dm_dualmerc_valid_act(unsigned act) {
    return act == 1 || act == 2 || act == 3 || act == 5;
}

/* Observed hireling monster classes for Switch 3.3 (diag.c threat filtering).
 * 560 and 561 are two barbarian variants, both consuming the Act V slot.
 * This is diagnostic only: candidate hires must use the real Hireling row. */
static inline unsigned dm_dualmerc_act_for_class(uint32_t cls) {
    switch (cls) {
    case 271: return 1;   /* Rogue Scout */
    case 338: return 2;   /* Desert Mercenary */
    case 359: return 3;   /* Iron Wolf */
    case 560: case 561: return 5; /* Barbarian */
    default: return 0;
    }
}

typedef struct {
    uint8_t occupied;          /* Occupied even when the hireling is dead. */
    uint8_t act;               /* Act of origin, independent of difficulty. */
    uint8_t difficulty;        /* 0 normal, 1 nightmare, 2 hell. */
    uint8_t dead;
    uint32_t runtime_unit_id;  /* Diagnostic only: changes across sessions. */
} dm_dualmerc_slot;

typedef struct { dm_dualmerc_slot slot[DM_DUALMERC_SLOTS]; } dm_dualmerc_roster;
typedef enum {
    DM_DUALMERC_REJECT = 0,
    DM_DUALMERC_ADD = 1,
    DM_DUALMERC_REPLACE_SAME_ACT = 2,
    DM_DUALMERC_CHOOSE_REPLACEMENT = 3
} dm_dualmerc_action;
typedef struct {
    dm_dualmerc_action action;
    int slot;                  /* Destination or sole legal replacement. */
    unsigned replace_mask;     /* A bit for each legal replacement slot. */
} dm_dualmerc_decision;

/* Reject inconsistent state rather than risking equipment loss. */
static inline int dm_dualmerc_roster_valid(const dm_dualmerc_roster *r) {
    if (!r) return 0;
    for (unsigned i = 0; i < DM_DUALMERC_SLOTS; ++i) {
        const dm_dualmerc_slot *s = &r->slot[i];
        if (!s->occupied) continue;
        if (!dm_dualmerc_valid_act(s->act) || s->difficulty > 2) return 0;
        for (unsigned j = 0; j < i; ++j)
            if (r->slot[j].occupied && r->slot[j].act == s->act) return 0;
    }
    return 1;
}

/*
 * offered_act must come from the game's Hireling data row for the candidate.
 * Never infer it from current difficulty, town, or the hire NPC alone.
 * A decision must not directly destroy any original hireling or item.
 */
static inline dm_dualmerc_decision dm_dualmerc_plan(
    const dm_dualmerc_roster *r, unsigned offered_act, unsigned offered_difficulty)
{
    dm_dualmerc_decision d = {DM_DUALMERC_REJECT, -1, 0};
    if (!dm_dualmerc_roster_valid(r) ||
        !dm_dualmerc_valid_act(offered_act) || offered_difficulty > 2)
        return d;
    /* Same act always replaces its own slot, even with a vacant other slot. */
    for (unsigned i = 0; i < DM_DUALMERC_SLOTS; ++i) {
        if (r->slot[i].occupied && r->slot[i].act == offered_act) {
            d.action = DM_DUALMERC_REPLACE_SAME_ACT;
            d.slot = (int)i;
            d.replace_mask = 1u << i;
            return d;
        }
    }
    for (unsigned i = 0; i < DM_DUALMERC_SLOTS; ++i) {
        if (!r->slot[i].occupied) {
            d.action = DM_DUALMERC_ADD;
            d.slot = (int)i;
            return d;
        }
    }
    d.action = DM_DUALMERC_CHOOSE_REPLACEMENT;
    d.replace_mask = (1u << DM_DUALMERC_SLOTS) - 1u;
    return d;
}

/* Confirm again against a freshly computed plan when the dialog is accepted. */
static inline int dm_dualmerc_choice_allowed(const dm_dualmerc_decision *d,
                                              unsigned chosen_slot) {
    if (!d || chosen_slot >= DM_DUALMERC_SLOTS) return 0;
    if (d->action == DM_DUALMERC_ADD) return d->slot == (int)chosen_slot;
    if (d->action == DM_DUALMERC_REPLACE_SAME_ACT ||
        d->action == DM_DUALMERC_CHOOSE_REPLACEMENT)
        return !!(d->replace_mask & (1u << chosen_slot));
    return 0;
}
#endif
