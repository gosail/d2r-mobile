/*
 * Switch 3.3, native 0x5b equip-from-backpack, verified in dm_rift.c:
 * [0x5b][item_id][merc_unit_id][swapped_item_id][bodyloc][xy][xy].
 * The four-byte receive-pump prefix is not part of the native packet.
 * Parsing is read-only. No native equipment changes can be accepted for a
 * second hireling until its serialization AND reload are proven.
 */
#ifndef DM_DUALMERC_EQUIP_H
#define DM_DUALMERC_EQUIP_H
#include "dm_dualmerc_packet.h"
#include "dm_dualmerc_policy.h"
#define DM_DUALMERC_EQUIP_WIRE_LEN 29u
#define DM_DUALMERC_EQUIP_NATIVE_LEN 25u
typedef struct {
    uint32_t item_id;
    uint32_t merc_unit_id;
    uint32_t swapped_id;
    uint32_t bodyloc;
    uint32_t source_xy;
    uint32_t destination_xy;
} dm_dualmerc_equip_packet;
static inline int dm_dualmerc_equip_decode(const uint8_t *buf,size_t n,
                                             dm_dualmerc_equip_packet *out) {
    if (!buf || !out || n != DM_DUALMERC_EQUIP_WIRE_LEN || buf[4] != 0x5b)
        return 0;
    dm_dualmerc_equip_packet tmp={
        .item_id=dm_dualmerc_pkt_u32(buf+5),
        .merc_unit_id=dm_dualmerc_pkt_u32(buf+9),
        .swapped_id=dm_dualmerc_pkt_u32(buf+13),
        .bodyloc=dm_dualmerc_pkt_u32(buf+17),
        .source_xy=dm_dualmerc_pkt_u32(buf+21),
        .destination_xy=dm_dualmerc_pkt_u32(buf+25),
    };
    if (tmp.item_id == UINT32_MAX || tmp.merc_unit_id == UINT32_MAX ||
        tmp.bodyloc == 0 || tmp.bodyloc > 255) return 0;
    *out=tmp;
    return 1;
}
/* The original native packet includes an explicit target merc ID:
 * never infer target from Act, difficulty, or whichever panel is open. */
static inline int dm_dualmerc_equip_target(const dm_dualmerc_roster *r,
                                            uint32_t unit_id, unsigned *slot) {
    if (!r || !slot || !dm_dualmerc_roster_valid(r) ||
        unit_id == UINT32_MAX) return 0;
    int found=-1;
    for (int i=0;i<DM_DUALMERC_SLOTS;i++) {
        if (!r->slot[i].occupied || r->slot[i].runtime_unit_id != unit_id)
            continue;
        if (found != -1 || r->slot[i].dead) return 0;
        found=i;
    }
    if (found == -1) return 0;
    *slot=(unsigned)found;
    return 1;
}
/* A roster with at least two native mercenaries cannot safely take ANY equip
 * packet while the second
 * native equipment UI and per-slot save/load are disconnected. This includes
 * 0x51 (held-item) and 0x5b (backpack). The check does not block ordinary
 * one-merc games, and will be lifted only after durable item ownership tests. */
static inline int dm_dualmerc_equip_block_unpersisted(
    unsigned active_hirelings, uint8_t opcode) {
    /* 3+ is an invalid/over-cap native roster, not permission to modify
     * another mercenary's items. Fail closed on any count >= 2. */
    return active_hirelings >= DM_DUALMERC_SLOTS &&
           (opcode == 0x51 || opcode == 0x5b);
}
#endif
