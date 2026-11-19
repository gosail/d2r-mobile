#include "../dm_dualmerc_hire_rows.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(i*8));
}
static void record(uint8_t out[DM_DUALMERC_HIRE_PREFIX_BYTES],
                   uint32_t id, uint32_t cls,
                   uint32_t act, uint32_t diff, uint32_t seller) {
    memset(out,0,DM_DUALMERC_HIRE_PREFIX_BYTES);
    put32(out+4,id); put32(out+8,cls); put32(out+12,act);
    put32(out+16,diff); put32(out+20,seller);
}

int main(void) {
    dm_dualmerc_hire_request wanted={5,198,2,0}; /* Greiz, Normal */
    uint8_t row[DM_DUALMERC_HIRE_PREFIX_BYTES];
    dm_dualmerc_hire_check status={0};
    assert(dm_dualmerc_hire_request_valid(&wanted));
    record(row,5,338,2,1,198);
    assert(dm_dualmerc_hire_check_add(&status,&wanted,row));
    assert(dm_dualmerc_hire_check_ok(&status));

    /* Same type's other level band is still a valid match. */
    assert(dm_dualmerc_hire_check_add(&status,&wanted,row));
    assert(status.matches==2 && dm_dualmerc_hire_check_ok(&status));
    record(row,5,338,2,2,198); /* Nightmare: not our offer */
    assert(dm_dualmerc_hire_check_add(&status,&wanted,row));
    assert(status.matches==2 && !status.conflicts);
    record(row,6,338,2,1,198); /* Different hire ID */
    assert(dm_dualmerc_hire_check_add(&status,&wanted,row));
    assert(status.matches==2 && !status.conflicts);

    /* Equal ID and difficulty with another Act cannot grant a purchase. */
    record(row,5,271,1,1,150);
    assert(dm_dualmerc_hire_check_add(&status,&wanted,row));
    assert(status.conflicts==1 && !dm_dualmerc_hire_check_ok(&status));
    status=(dm_dualmerc_hire_check){0};
    record(row,5,338,2,1,150); /* Correct class, spoofed seller */
    assert(dm_dualmerc_hire_check_add(&status,&wanted,row));
    assert(!dm_dualmerc_hire_check_ok(&status));
    status=(dm_dualmerc_hire_check){0};
    record(row,5,560,2,1,198); /* Monster class is Act V, not II */
    assert(dm_dualmerc_hire_check_add(&status,&wanted,row));
    assert(!dm_dualmerc_hire_check_ok(&status));
    status=(dm_dualmerc_hire_check){0};
    record(row,5,338,2,1,198);
    assert(dm_dualmerc_hire_check_add(&status,&wanted,row));
    assert(status.matches==1 && dm_dualmerc_hire_check_ok(&status));

    wanted.vendor_class=150; /* Kashya cannot supply Act II. */
    assert(!dm_dualmerc_hire_request_valid(&wanted));
    assert(!dm_dualmerc_hire_check_add(&status,&wanted,row));
    wanted.vendor_class=198; wanted.game_difficulty=3;
    assert(!dm_dualmerc_hire_request_valid(&wanted));
    wanted.game_difficulty=0;
    wanted.native_hire_id=UINT32_MAX;
    assert(!dm_dualmerc_hire_request_valid(&wanted));
    assert(!dm_dualmerc_hire_check_ok(NULL));
    assert(!dm_dualmerc_hire_check_add(NULL,&wanted,row));
    assert(!dm_dualmerc_hire_check_add(&status,&wanted,NULL));

    puts("dualmerc guarded live Hireling record prefix policy: passed");
    return 0;
}
