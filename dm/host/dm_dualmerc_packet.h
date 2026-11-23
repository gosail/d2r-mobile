/* Strict Switch 3.3 offline C2S MercHire packet parser.
 * Packet passed to 0x632510 has a four-byte delivery header, followed by
 * [0x36][npc unit ID:u32][opaque hire row:u32][arg2:u32][arg3:u32].
 * Exact length 17 is verified by native handler at 0x6dada4. Offer identity
 * must then be checked against the live NPC and its Hireling data row.
 */
#ifndef DM_DUALMERC_PACKET_H
#define DM_DUALMERC_PACKET_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define DM_DUALMERC_PACKET_BYTES 21u
#define DM_DUALMERC_NATIVE_PACKET_BYTES 17u
typedef struct {
    uint32_t npc_unit_id, hire_row, param2, param3;
} dm_dualmerc_packet;
static inline uint32_t dm_dualmerc_pkt_u32(const uint8_t *p) {
    return ((uint32_t)p[0] | (uint32_t)p[1]<<8 |
            (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24);
}
static inline int dm_dualmerc_packet_parse(const uint8_t *buf, size_t bytes,
                                            dm_dualmerc_packet *out) {
    if (!buf || !out || bytes != DM_DUALMERC_PACKET_BYTES ||
        buf[4] != 0x36) return 0;
    dm_dualmerc_packet p;
    p.npc_unit_id = dm_dualmerc_pkt_u32(buf+5);
    p.hire_row = dm_dualmerc_pkt_u32(buf+9);
    p.param2 = dm_dualmerc_pkt_u32(buf+13);
    p.param3 = dm_dualmerc_pkt_u32(buf+17);
    if (p.npc_unit_id == UINT32_MAX || p.hire_row == UINT32_MAX) return 0;
    *out = p;
    return 1;
}
/* A deferred callback outside the stable native guest code can point into
 * a dead stack frame. Reject it rather than replay an old C2S packet. */
static inline int dm_dualmerc_stable_callback(uint64_t fn,
                                              uint64_t base, uint64_t code_size) {
    if (!fn) return 1;
    if (!base || !code_size || fn < base || (fn & 3)) return 0;
    return fn-base < code_size;
}
#endif
