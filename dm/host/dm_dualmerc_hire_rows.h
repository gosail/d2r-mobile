/*
 * Switch 3.3 offline hire-candidate cross-check (no native mutations).
 * Existing dm_rift.c identifies the loaded Hireling table at
 * main+0x2d64a18, table+0x558 (rows) / +0x560 (count), stride 0x150,
 * and record+8 (monster Class). The prefix fields Id, Act, Difficulty,
 * Seller follow the Diablo II Hireling record descriptor, but MUST be
 * confirmed against owner-prepared Switch 3.3 data before enabling normal
 * saves. Unknown/ambiguous records are rejected.
 *
 * The C2S field is still an opaque hire token. This conservative guard
 * treats it as Hireling.Id for trial verification: a mismatch aborts the
 * trial; a match cannot by itself prove a native C2S ABI or safe rollback.
 */
#ifndef DM_DUALMERC_HIRE_ROWS_H
#define DM_DUALMERC_HIRE_ROWS_H
#include "dm_dualmerc_policy.h"
#include "dm_dualmerc_packet.h"
#include <stddef.h>
#include <stdint.h>

#define DM_DUALMERC_HIRE_PREFIX_BYTES 24u
#define DM_DUALMERC_HIRE_STRIDE 0x150u
#define DM_DUALMERC_HIRE_COUNT_MAX 512u

typedef struct {
    uint32_t native_hire_id;
    uint32_t vendor_class;
    unsigned vendor_act, game_difficulty;
} dm_dualmerc_hire_request;

typedef struct {
    unsigned matches;
    unsigned conflicts;
} dm_dualmerc_hire_check;

static inline int dm_dualmerc_hire_request_valid(
    const dm_dualmerc_hire_request *want) {
    return want && want->native_hire_id != UINT32_MAX &&
           want->game_difficulty <= 2 &&
           dm_dualmerc_valid_act(want->vendor_act) &&
           dm_dualmerc_act_for_vendor(want->vendor_class) ==
               want->vendor_act;
}

/* Inputs are bounded copies from verified native table memory. Only these
 * 24 prefix bytes are inspected; no guest pointers are ever retained. */
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
    if (id != want->native_hire_id ||
        diff != want->game_difficulty + 1u) return 1;
    if (act == want->vendor_act && seller == want->vendor_class &&
        dm_dualmerc_act_for_class(klass) == act) {
        if (check->matches != UINT32_MAX) check->matches++;
    } else {
        check->conflicts++;
    }
    return 1;
}

static inline int dm_dualmerc_hire_check_ok(
    const dm_dualmerc_hire_check *check) {
    return check && check->matches > 0 && check->conflicts == 0;
}
#endif
