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
/* Live pet nodes expose the mercenary Act and runtime ID but not a
 * validated HIRE-ORIGIN difficulty. 3 is UNKNOWN in runtime snapshots,
 * never a game difficulty and never legal in persisted SWM2 records. */
#define DM_DUALMERC_DIFFICULTY_UNKNOWN 3u

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

/* Switch 3.3 Hireling Seller MonStats class indices. These are unique
 * vendors for actual hirelist interactions; they are invariant across game
 * difficulty and must be checked against the live resolved NPC unit. */
static inline unsigned dm_dualmerc_act_for_vendor(uint32_t npc_class) {
    switch (npc_class) {
    case 150: return 1; /* Kashya */
    case 198: return 2; /* Greiz */
    case 252: return 3; /* Asheara */
    case 515: return 5; /* Qual-Kehk */
    default: return 0;
    }
}

/* Decode only a direct AArch64 BL (not BLR / indirect calls).
 * Supports both forward and negative branch offsets without touching code. */
static inline int dm_dualmerc_bl_target(uint32_t insn, uint64_t at_rva,
                                        uint64_t text_end, uint64_t *target) {
    if (!target || (insn & 0xfc000000u) != 0x94000000u ||
        at_rva >= text_end) return 0;
    int64_t delta = (int64_t)(insn & 0x03ffffffu);
    if (delta & 0x02000000) delta -= 0x04000000;
    int64_t dest = (int64_t)at_rva + 4 * delta;
    if (dest < 0 || (uint64_t)dest >= text_end || (dest & 3)) return 0;
    *target = (uint64_t)dest;
    return 1;
}

typedef struct {
    uint8_t occupied;          /* Occupied even when the hireling is dead. */
    uint8_t act;               /* Act of origin, independent of difficulty. */
    uint8_t difficulty;        /* 0/1/2 verified origin, 3 unknown live origin. */
    uint8_t dead;
    uint32_t runtime_unit_id;  /* Diagnostic only: changes across sessions. */
    uint32_t monster_class;    /* Verified live UNIT+4; not a stable hire ID. */
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
        if (!dm_dualmerc_valid_act(s->act) ||
            s->difficulty > DM_DUALMERC_DIFFICULTY_UNKNOWN ||
            s->dead > 1 ||
            (s->monster_class &&
             dm_dualmerc_act_for_class(s->monster_class) != s->act))
            return 0;
        for (unsigned j = 0; j < i; ++j)
            if (r->slot[j].occupied && r->slot[j].act == s->act) return 0;
    }
    return 1;
}

/* Persisted SWM2 rosters deliberately have no runtime IDs. Call this
 * stricter helper ONLY for an actual live server pet list, before granting
 * any native hire/gear mutation or claiming two distinct native units. */
static inline int dm_dualmerc_live_ids_valid(const dm_dualmerc_roster *r) {
    if (!dm_dualmerc_roster_valid(r)) return 0;
    for (unsigned i=0; i<DM_DUALMERC_SLOTS; ++i) {
        if (!r->slot[i].occupied) continue;
        if (r->slot[i].runtime_unit_id == UINT32_MAX) return 0;
        for (unsigned j=0; j<i; ++j)
            if (r->slot[j].occupied &&
                r->slot[j].runtime_unit_id == r->slot[i].runtime_unit_id)
                return 0;
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
