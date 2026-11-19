#include "../dm_dualmerc_d2s.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const size_t n=DM_DUALMERC_D2S_HEADER_MIN;
    uint8_t *buf=calloc(1,n);
    assert(buf);
    dm_dualmerc_native_anchor a={0},saved={0};
    dm_dualmerc_store_u32(buf,0xaa55aa55u);
    dm_dualmerc_store_u32(buf+4,99);
    dm_dualmerc_store_u32(buf+8,(uint32_t)n);
    dm_dualmerc_store_u32(buf+179,0xdeadbeefu);
    dm_dualmerc_store_u16(buf+183,9);
    dm_dualmerc_store_u16(buf+185,4);
    dm_dualmerc_store_u32(buf+187,10000);
    dm_dualmerc_store_u32(buf+12,dm_dualmerc_d2s_checksum(buf,n));
    assert(dm_dualmerc_d2s_read(buf,n,&a));
    assert(a.version==99 && a.seed==0xdeadbeefu);
    assert(a.hireling_row==4 && a.name_id==9 && a.exp==10000);
    saved=a;
    dm_dualmerc_saved_state *state=calloc(1,sizeof *state);
    assert(state);
    state->slot[0].occupied=1; state->slot[0].act=1;
    state->slot[0].monster_class=271;
    state->slot[0].level=20; state->slot[0].hireling_row=4;
    state->slot[0].name_id=9; state->slot[0].experience=10000;
    assert(!dm_dualmerc_d2s_matches_state(&a,state));
    state->native_seed = a.seed;
    state->native_d2s_checksum = a.checksum;
    assert(dm_dualmerc_d2s_matches_state(&a,state));
    state->slot[1].occupied=1; state->slot[1].act=2;
    state->slot[1].monster_class=338; state->slot[1].level=12;
    state->slot[1].hireling_row=20;
    assert(dm_dualmerc_d2s_matches_state(&a,state));
    state->native_slot=1;
    assert(!dm_dualmerc_d2s_matches_state(&a,state));
    state->native_slot=0;
    state->slot[0].experience=9999;
    assert(!dm_dualmerc_d2s_matches_state(&a,state));
    state->slot[0].experience=10000;
    state->native_seed++;
    assert(!dm_dualmerc_d2s_matches_state(&a,state));
    state->native_seed--;
    state->native_d2s_checksum++;
    assert(!dm_dualmerc_d2s_matches_state(&a,state));
    state->native_d2s_checksum--;
    buf[300]^=1;
    assert(!dm_dualmerc_d2s_read(buf,n,&a));
    assert(!memcmp(&a,&saved,sizeof a));
    buf[300]^=1;
    dm_dualmerc_store_u32(buf+4,105);
    dm_dualmerc_store_u32(buf+12,dm_dualmerc_d2s_checksum(buf,n));
    assert(!dm_dualmerc_d2s_read(buf,n,&a)); /* shifted v105 offsets */
    dm_dualmerc_store_u32(buf+4,99);
    dm_dualmerc_store_u32(buf+179,0);
    dm_dualmerc_store_u32(buf+12,dm_dualmerc_d2s_checksum(buf,n));
    assert(!dm_dualmerc_d2s_read(buf,n,&a)); /* no hired merc */
    assert(!dm_dualmerc_d2s_read(buf,n-1,&a));
    assert(!dm_dualmerc_d2s_read(NULL,n,&a));
    assert(!dm_dualmerc_d2s_read(buf,n,NULL));
    free(state); free(buf);
    puts("D2R legacy d2s native merc identity, checksum, stale sidecar guard: passed");
}
