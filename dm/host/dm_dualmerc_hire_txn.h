/*
 * Safe two-stage offline hire contract: inspect -> choose -> revalidate.
 *
 * No engine call, gold debit, pet deletion or item ownership change occurs
 * here. The native hire path must consume a granted lease exactly once,
 * AFTER rechecking the candidate's actual Hireling data row and roster.
 *
 * A dead hireling still occupies its Act slot. Difficulty never gives a
 * second slot to another hireling from the same Act.
 */
#ifndef DM_DUALMERC_HIRE_TXN_H
#define DM_DUALMERC_HIRE_TXN_H
#include "dm_dualmerc_policy.h"
#include <string.h>
#define DM_DUALMERC_TXN_TTL 1500u /* 60s at 25 ticks/sec */
typedef enum {
    DM_MERC_TXN_REJECT = 0,
    DM_MERC_TXN_READY = 1,
    DM_MERC_TXN_CHOOSE = 2,
    DM_MERC_TXN_COMMITTED = 3
} dm_merc_txn_status;
typedef struct {
    uint64_t game_session; /* Never a persistent ID or a saved pointer. */
    uint32_t owner_id, npc_id, hire_row;
    unsigned offered_act, difficulty;
} dm_merc_txn_offer;
typedef struct {
    dm_merc_txn_offer offer;
    dm_dualmerc_roster roster;
    dm_dualmerc_decision plan;
    uint32_t opened_tick;
    uint32_t choice;
    unsigned state;
} dm_merc_txn;

/* Do not retain a raw packet pointer or guest memory in this object. */
static inline dm_merc_txn_status dm_merc_txn_begin(
    dm_merc_txn *t, dm_merc_txn_offer offer,
    const dm_dualmerc_roster *r, uint32_t now_tick)
{
    if (!t) return DM_MERC_TXN_REJECT;
    memset(t, 0, sizeof *t);
    if (!offer.game_session || offer.owner_id == UINT32_MAX ||
        offer.npc_id == UINT32_MAX || offer.hire_row == UINT32_MAX ||
        !r || !dm_dualmerc_live_ids_valid(r)) return DM_MERC_TXN_REJECT;
    dm_dualmerc_decision p = dm_dualmerc_plan(r,offer.offered_act,
                                              offer.difficulty);
    if (p.action == DM_DUALMERC_REJECT) return DM_MERC_TXN_REJECT;
    t->offer = offer;
    t->roster = *r;
    t->plan = p;
    t->opened_tick = now_tick;
    t->choice = UINT32_MAX;
    if (p.action == DM_DUALMERC_CHOOSE_REPLACEMENT) {
        t->state = DM_MERC_TXN_CHOOSE;
        return DM_MERC_TXN_CHOOSE;
    }
    t->choice = (unsigned)p.slot;
    t->state = DM_MERC_TXN_READY;
    return DM_MERC_TXN_READY;
}

static inline int dm_merc_txn_choose(dm_merc_txn *t, unsigned slot) {
    if (!t || t->state != DM_MERC_TXN_CHOOSE ||
        !dm_dualmerc_choice_allowed(&t->plan,slot)) return 0;
    t->choice = slot;
    t->state = DM_MERC_TXN_READY;
    return 1;
}

static inline void dm_merc_txn_cancel(dm_merc_txn *t) {
    if (t) memset(t,0,sizeof *t);
}

/* A stale popup cannot select a different merc, NPC, candidate row, player,
 * difficulty, or even the same two Acts with changed runtime mercenary IDs.
 * Validates the exact pre-dialog authoritative roster again. This is an
 * authorization check, NOT actual native transaction success.
 */
static inline int dm_merc_txn_authorize(
    dm_merc_txn *t, const dm_merc_txn_offer *live,
    const dm_dualmerc_roster *current, uint32_t tick, unsigned *slot_out)
{
    if (!t || t->state != DM_MERC_TXN_READY ||
        !live || !current || !slot_out ||
        tick - t->opened_tick > DM_DUALMERC_TXN_TTL ||
        t->offer.game_session != live->game_session ||
        t->offer.owner_id != live->owner_id ||
        t->offer.npc_id != live->npc_id ||
        t->offer.hire_row != live->hire_row ||
        t->offer.offered_act != live->offered_act ||
        t->offer.difficulty != live->difficulty ||
        !dm_dualmerc_live_ids_valid(current)) {
        dm_merc_txn_cancel(t); return 0;
    }
    /* Native pet-list insertion can reorder the two nodes while an iPad
     * choice is open. Preserve the chosen MERCENARY identity, not its list
     * index. An unknown, duplicated, or altered unit ID still fails closed. */
    if (t->choice >= DM_DUALMERC_SLOTS) {
        dm_merc_txn_cancel(t); return 0;
    }
    unsigned matched_mask = 0;
    unsigned live_for_saved[DM_DUALMERC_SLOTS] = {
        DM_DUALMERC_SLOTS, DM_DUALMERC_SLOTS
    };
    for (unsigned i = 0; i < DM_DUALMERC_SLOTS; ++i) {
        const dm_dualmerc_slot *old = &t->roster.slot[i];
        if (!old->occupied) continue;
        if (old->runtime_unit_id == UINT32_MAX) {
            dm_merc_txn_cancel(t); return 0;
        }
        for (unsigned j = 0; j < DM_DUALMERC_SLOTS; ++j) {
            const dm_dualmerc_slot *now = &current->slot[j];
            if (!now->occupied ||
                now->runtime_unit_id != old->runtime_unit_id) continue;
            if (live_for_saved[i] != DM_DUALMERC_SLOTS ||
                (matched_mask & (1u << j)) ||
                old->act != now->act ||
                old->monster_class != now->monster_class ||
                old->difficulty != now->difficulty ||
                old->dead != now->dead) {
                dm_merc_txn_cancel(t); return 0;
            }
            live_for_saved[i] = j;
            matched_mask |= 1u << j;
        }
        if (live_for_saved[i] == DM_DUALMERC_SLOTS) {
            dm_merc_txn_cancel(t); return 0;
        }
    }
    unsigned live_occupied = 0;
    for (unsigned j = 0; j < DM_DUALMERC_SLOTS; ++j)
        if (current->slot[j].occupied) live_occupied |= 1u << j;
    if (live_occupied != matched_mask) {
        dm_merc_txn_cancel(t); return 0;
    }
    unsigned chosen = DM_DUALMERC_SLOTS;
    if (t->roster.slot[t->choice].occupied) {
        chosen = live_for_saved[t->choice];
    } else if (t->plan.action == DM_DUALMERC_ADD) {
        for (unsigned j = 0; j < DM_DUALMERC_SLOTS; ++j)
            if (!current->slot[j].occupied) {
                chosen = j; break;
            }
    }
    dm_dualmerc_decision p = dm_dualmerc_plan(current,
        live->offered_act,live->difficulty);
    if (chosen >= DM_DUALMERC_SLOTS ||
        p.action != t->plan.action ||
        !dm_dualmerc_choice_allowed(&p,chosen) ||
        ((p.action == DM_DUALMERC_ADD ||
          p.action == DM_DUALMERC_REPLACE_SAME_ACT) &&
         p.slot != (int)chosen)) {
        dm_merc_txn_cancel(t); return 0;
    }
    *slot_out = chosen;
    dm_merc_txn_cancel(t); /* one-shot; replayed confirmation denied */
    return 1;
}
#endif
