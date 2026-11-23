#include "../dm_dualmerc_hire_txn.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static dm_dualmerc_slot merc(unsigned act, unsigned diff, unsigned id,
                            int dead) {
    dm_dualmerc_slot m = {0};
    m.occupied = 1; m.act = act; m.difficulty = diff;
    m.runtime_unit_id = id; m.dead = dead;
    return m;
}
/* The native pet-list may reorder nodes without changing either
 * hireling. The selected UNIT must remain the same, independently of index. */
static void test_pet_list_reorder(void) {
    dm_dualmerc_roster before={0}, swapped={0};
    dm_merc_txn lease;
    dm_merc_txn_offer offer={55,100,200,300,5,0};
    unsigned chosen=99;
    before.slot[0]=merc(1,0,101,0);
    before.slot[1]=merc(2,1,102,1);
    swapped.slot[0]=before.slot[1];
    swapped.slot[1]=before.slot[0];

    assert(dm_merc_txn_begin(&lease,offer,&before,500)==DM_MERC_TXN_CHOOSE);
    assert(dm_merc_txn_choose(&lease,0));
    assert(dm_merc_txn_authorize(&lease,&offer,&swapped,501,&chosen));
    assert(chosen==1); /* Act I ID 101 moved from slot 0 to slot 1. */
    assert(!dm_merc_txn_authorize(&lease,&offer,&swapped,501,&chosen));

    assert(dm_merc_txn_begin(&lease,offer,&before,500)==DM_MERC_TXN_CHOOSE);
    assert(dm_merc_txn_choose(&lease,1));
    assert(dm_merc_txn_authorize(&lease,&offer,&swapped,501,&chosen));
    assert(chosen==0); /* The dead Act II ID 102 moved to slot 0. */

    offer.offered_act=2; offer.difficulty=2;
    assert(dm_merc_txn_begin(&lease,offer,&before,500)==DM_MERC_TXN_READY);
    assert(dm_merc_txn_authorize(&lease,&offer,&swapped,501,&chosen));
    assert(chosen==0); /* Same Act always targets the original merc ID. */

    offer.offered_act=5; offer.difficulty=0;
    dm_dualmerc_roster altered=swapped;
    altered.slot[0].dead=0; /* Changed death state invalidates the lease. */
    assert(dm_merc_txn_begin(&lease,offer,&before,500)==DM_MERC_TXN_CHOOSE);
    assert(dm_merc_txn_choose(&lease,1));
    assert(!dm_merc_txn_authorize(&lease,&offer,&altered,501,&chosen));
    assert(!lease.state);
    altered=swapped;
    altered.slot[0].runtime_unit_id=103;
    assert(dm_merc_txn_begin(&lease,offer,&before,500)==DM_MERC_TXN_CHOOSE);
    assert(dm_merc_txn_choose(&lease,1));
    assert(!dm_merc_txn_authorize(&lease,&offer,&altered,501,&chosen));

    altered=swapped;
    altered.slot[0].runtime_unit_id=101; /* Duplicate runtime ID is ambiguous. */
    assert(dm_merc_txn_begin(&lease,offer,&before,500)==DM_MERC_TXN_CHOOSE);
    assert(dm_merc_txn_choose(&lease,1));
    assert(!dm_merc_txn_authorize(&lease,&offer,&altered,501,&chosen));

    /* The sole hireling can move between linked-list indices as well. */
    before.slot[1]=(dm_dualmerc_slot){0};
    swapped.slot[0]=(dm_dualmerc_slot){0};
    swapped.slot[1]=before.slot[0];
    assert(dm_merc_txn_begin(&lease,offer,&before,500)==DM_MERC_TXN_READY);
    assert(dm_merc_txn_authorize(&lease,&offer,&swapped,501,&chosen));
    assert(chosen==0); /* ADD must select the refreshed empty slot. */

    /* Timer wrap still uses modulo-32-bit tick arithmetic safely. */
    assert(dm_merc_txn_begin(&lease,offer,&before,UINT32_MAX-10)==DM_MERC_TXN_READY);
    assert(dm_merc_txn_authorize(&lease,&offer,&before,5,&chosen));
    assert(chosen==1);
    assert(dm_merc_txn_begin(&lease,offer,&before,UINT32_MAX-10)==DM_MERC_TXN_READY);
    assert(!dm_merc_txn_authorize(&lease,&offer,&before,1600,&chosen));
}

int main(void) {
    dm_dualmerc_roster r = {0};
    dm_merc_txn txn;
    dm_merc_txn_offer o = {55,100,200,300,1,0};
    unsigned chosen = 999;
    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_READY);
    assert(dm_merc_txn_authorize(&txn,&o,&r,101,&chosen) && chosen==0);
    assert(!dm_merc_txn_authorize(&txn,&o,&r,101,&chosen));

    r.slot[0] = merc(1,0,101,0);
    o.offered_act = 2;
    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_READY);
    assert(dm_merc_txn_authorize(&txn,&o,&r,102,&chosen) && chosen==1);

    r.slot[1] = merc(2,1,102,1); /* dead occupies a second Act slot */
    o.offered_act = 5;
    r.slot[1].runtime_unit_id=r.slot[0].runtime_unit_id;
    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_REJECT);
    r.slot[1].runtime_unit_id=UINT32_MAX;
    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_REJECT);
    r.slot[1].runtime_unit_id=102;
    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_CHOOSE);
    assert(!dm_merc_txn_authorize(&txn,&o,&r,110,&chosen));
    assert(!dm_merc_txn_choose(&txn,2));
    assert(!dm_merc_txn_choose(&txn,UINT32_MAX));
    assert(dm_merc_txn_choose(&txn,1));
    assert(!dm_merc_txn_choose(&txn,0));
    assert(dm_merc_txn_authorize(&txn,&o,&r,125,&chosen) && chosen==1);
    assert(!dm_merc_txn_authorize(&txn,&o,&r,125,&chosen));

    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_CHOOSE);
    assert(dm_merc_txn_choose(&txn,0));
    dm_dualmerc_roster mutated=r;
    mutated.slot[0].runtime_unit_id++;
    assert(!dm_merc_txn_authorize(&txn,&o,&mutated,100,&chosen));
    assert(!txn.state);
    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_CHOOSE);
    assert(dm_merc_txn_choose(&txn,0));
    dm_merc_txn_offer changed=o;
    changed.hire_row++;
    assert(!dm_merc_txn_authorize(&txn,&changed,&r,110,&chosen));
    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_CHOOSE);
    assert(dm_merc_txn_choose(&txn,0));
    assert(!dm_merc_txn_authorize(&txn,&o,&r,1601,&chosen));

    o.offered_act = 2; o.difficulty=2; /* Nightmare/Hell: same Act II */
    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_READY);
    assert(!dm_merc_txn_choose(&txn,0));
    assert(dm_merc_txn_authorize(&txn,&o,&r,110,&chosen) && chosen==1);
    o.offered_act=5; o.difficulty=0;
    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_CHOOSE);
    dm_merc_txn_cancel(&txn);
    assert(!dm_merc_txn_authorize(&txn,&o,&r,102,&chosen));
    r.slot[1].act=1;
    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_REJECT);
    r.slot[1].act=2;
    o.offered_act=4;
    assert(dm_merc_txn_begin(&txn,o,&r,100)==DM_MERC_TXN_REJECT);
    test_pet_list_reorder();
    puts("dualmerc hire transaction: identity/reorder/expiry assertions passed");
}
