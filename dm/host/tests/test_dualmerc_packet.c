#include "../dm_dualmerc_packet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    uint8_t b[DM_DUALMERC_PACKET_BYTES] = {0};
    dm_dualmerc_packet v = {0}, old;
    b[4]=0x36;
    b[5]=0x11; b[6]=0x22; b[7]=0x33; b[8]=0x44;
    b[9]=0x05; b[13]=0x01; b[17]=0x02;
    assert(dm_dualmerc_packet_parse(b,sizeof b,&v));
    assert(v.npc_unit_id == 0x44332211 && v.hire_row == 5);
    assert(v.param2 == 1 && v.param3 == 2);
    old=v;
    assert(!dm_dualmerc_packet_parse(b,20,&v));
    assert(!memcmp(&old,&v,sizeof v));
    b[4]=0x38;
    assert(!dm_dualmerc_packet_parse(b,sizeof b,&v));
    b[4]=0x36;
    memset(b+5,0xff,4);
    assert(!dm_dualmerc_packet_parse(b,sizeof b,&v));
    assert(!dm_dualmerc_packet_parse(NULL,sizeof b,&v));
    assert(!dm_dualmerc_packet_parse(b,sizeof b,NULL));
    puts("dualmerc exact Switch 3.3 hire packet parser: passed");
}
