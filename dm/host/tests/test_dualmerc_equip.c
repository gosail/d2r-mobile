#include "../dm_dualmerc_equip.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put32(uint8_t *p,uint32_t v) {
    for (unsigned k=0;k<4;k++) p[k]=(uint8_t)(v>>(8*k));
}
int main(void) {
    uint8_t raw[DM_DUALMERC_EQUIP_WIRE_LEN]={0};
    raw[4]=0x5b;
    put32(raw+5,11); put32(raw+9,21); put32(raw+13,UINT32_MAX);
    put32(raw+17,3); put32(raw+21,0x10002); put32(raw+25,0x10002);
    dm_dualmerc_equip_packet p={0};
    assert(dm_dualmerc_equip_decode(raw,sizeof raw,&p));
    assert(p.item_id==11 && p.merc_unit_id==21 && p.swapped_id==UINT32_MAX);
    assert(p.bodyloc==3 && p.source_xy==0x10002 && p.destination_xy==0x10002);
    dm_dualmerc_equip_packet copy=p;
    assert(!dm_dualmerc_equip_decode(raw,sizeof raw-1,&p));
    assert(!memcmp(&copy,&p,sizeof copy));
    raw[4]=0x51;
    assert(!dm_dualmerc_equip_decode(raw,sizeof raw,&p));
    raw[4]=0x5b;
    put32(raw+9,UINT32_MAX);
    assert(!dm_dualmerc_equip_decode(raw,sizeof raw,&p));
    put32(raw+9,21);
    put32(raw+17,0);
    assert(!dm_dualmerc_equip_decode(raw,sizeof raw,&p));
    put32(raw+17,3);
    dm_dualmerc_roster roster={0};
    roster.slot[0]=(dm_dualmerc_slot){.occupied=1,.act=1,.runtime_unit_id=21};
    roster.slot[1]=(dm_dualmerc_slot){.occupied=1,.act=2,.runtime_unit_id=22};
    unsigned slot=99;
    assert(dm_dualmerc_equip_target(&roster,21,&slot) && slot==0);
    assert(dm_dualmerc_equip_target(&roster,22,&slot) && slot==1);
    assert(!dm_dualmerc_equip_target(&roster,23,&slot));
    roster.slot[1].dead=1;
    assert(!dm_dualmerc_equip_target(&roster,22,&slot));
    roster.slot[1].dead=0;
    roster.slot[1].runtime_unit_id=21;
    assert(!dm_dualmerc_equip_target(&roster,21,&slot));
    roster.slot[1].runtime_unit_id=22;
    assert(!dm_dualmerc_equip_block_unpersisted(1,0x5b));
    assert(dm_dualmerc_equip_block_unpersisted(2,0x5b));
    assert(dm_dualmerc_equip_block_unpersisted(2,0x51));
    assert(!dm_dualmerc_equip_block_unpersisted(2,0x36));
    assert(!dm_dualmerc_equip_block_unpersisted(3,0x5b));
    puts("two-merc equip packet identity and no-unsafe-gear gate: passed");
    return 0;
}
