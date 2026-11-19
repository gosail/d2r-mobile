/*
 * Read-only Switch 3.3 offline Hireling table candidate cross-check.
 * dm_rift.c independently uses main+0x2d64a18 / table+0x558 and +0x560,
 * stride 0x150, and Class at record+8. The remaining prefix fields and
 * resolved name range offsets are described for 1.14d Hireling records.
 * Native Switch 3.3 layout and the opaque C2S 0x36 candidate token MUST be
 * independently confirmed against the player's prepared game image/log.
 *
 * Older 0x36 C2S protocol evidence treats the token as MERC NAME INDEX,
 * not Hireling.Id; other records name a subtype Id. Check both distinct
 * interpretations, never guess a preferred one, reject overlap/conflict.
 * Any success is conservative evidence of a valid NPC/Act/Class/Difficulty,
 * not proof of the exact 3.3 hire transaction ABI.
 */
#ifndef DM_DUALMERC_HIRE_ROWS_H
#define DM_DUALMERC_HIRE_ROWS_H
#include "dm_dualmerc_policy.h"
#include "dm_dualmerc_packet.h"
#include <stddef.h>
#include <stdint.h>

/* NameFirst/NameLast resolved string indices at +0x114/+0x116 are read
 * only from live compiled rows; never treat the uncompiled .bin zeros as
 * authoritative. The entire 0x118-byte prefix fits inside a 0x150 row. */
#define DM_DUALMERC_HIRE_PREFIX_BYTES 0x118u
#define DM_DUALMERC_HIRE_STRIDE 0x150u
#define DM_DUALMERC_HIRE_COUNT_MAX 512u

typedef struct {
    uint32_t native_hire_token;
    uint32_t vendor_class;
    unsigned vendor_act, game_difficulty;
} dm_dualmerc_hire_request;

typedef struct {
    unsigned subtype_matches, name_matches;
    unsigned conflicts;
} dm_dualmerc_hire_check;

static inline int dm_dualmerc_hire_request_valid(
    const dm_dualmerc_hire_request *want) {
    return want && want->native_hire_token != UINT32_MAX &&
           want->game_difficulty <= 2 &&
           dm_dualmerc_valid_act(want->vendor_act) &&
           dm_dualmerc_act_for_vendor(want->vendor_class) ==
               want->vendor_act;
}

static inline uint16_t dm_dualmerc_hire_u16(const uint8_t *p) {
    return (uint16_t)((unsigned)p[0] | (unsigned)p[1]<<8);
}

/* Input is a bounded prefix copied with tk_rd from native guest memory.
 * No guest pointer is retained. Zero/invalid name ranges never match. */
static inline int dm_dualmerc_hire_check_add(
    dm_dualmerc_hire_check *check, const dm_dualmerc_hire_request *want,
    const uint8_t row[DM_DUALMERC_HIRE_PREFIX_BYTES])
{
    if (!check || !dm_dualmerc_hire_request_valid(want) || !row)
        return 0;
    uint32_t id=dm_dualmerc_pkt_u32(row+4);
    uint32_t klass=dm_dualmerc_pkt_u32(row+8);
    uint32_t act=dm_dualmerc_pkt_u32(row+12);
    uint32_t diff=dm_dualmerc_pkt_u32(row+16);
    uint32_t seller=dm_dualmerc_pkt_u32(row+20);
    uint32_t first=dm_dualmerc_hire_u16(row+0x114);
    uint32_t last=dm_dualmerc_hire_u16(row+0x116);
    int subtype=want->native_hire_token <= 255u &&
                want->native_hire_token == id;
    int name=first && last>=first &&
             want->native_hire_token >= first &&
             want->native_hire_token <= last;
    if ((!subtype && !name) || diff != want->game_difficulty+1u)
        return 1; /* Other offered difficulties share the same name range. */
    if (act == want->vendor_act && seller == want->vendor_class &&
        dm_dualmerc_act_for_class(klass) == act) {
        if (subtype && check->subtype_matches != UINT32_MAX)
            ++check->subtype_matches;
        if (name && check->name_matches != UINT32_MAX)
            ++check->name_matches;
    } else {
        ++check->conflicts;
    }
    return 1;
}
static inline int dm_dualmerc_hire_check_ok(
    const dm_dualmerc_hire_check *check) {
    return check && check->conflicts == 0 &&
        ((check->subtype_matches > 0) != (check->name_matches > 0));
}
#endif
