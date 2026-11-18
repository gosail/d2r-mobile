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
    puts("dualmerc hire transaction: 20+ assertions passed");
}
