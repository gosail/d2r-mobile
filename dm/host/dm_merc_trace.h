/* Read-only hireling inventory snapshot helpers.
 * Fingerprints contain runtime item IDs and are valid only for comparisons
 * within one game session. NOT a durable item identity or proof of ownership.
 */
#ifndef DM_MERC_TRACE_H
#define DM_MERC_TRACE_H
#include <stdint.h>

enum {
    DM_MERC_GEAR_ABSENT = 0,    /* no inventory pointer exposed */
    DM_MERC_GEAR_OK = 1,        /* inventory signature and entire chain readable */
    DM_MERC_GEAR_INVALID = 2,   /* one of the memory reads/signatures failed */
    DM_MERC_GEAR_TRUNCATED = 3  /* chain exceeded the 64-item safety limit */
};
typedef struct {
    uint32_t fingerprint;
    uint16_t count;
    uint8_t status;
} dm_merc_trace_gear;

/* Order-invariant, non-cryptographic fingerprint for quick change detection.
 * A match is not proof of unchanged equipment (hash collisions are possible).
 */
static inline uint32_t dm_merc_trace_item_mix(uint32_t id) {
    id ^= id >> 16;
    id *= UINT32_C(0x7feb352d);
    id ^= id >> 15;
    id *= UINT32_C(0x846ca68b);
    return id ^ (id >> 16);
}
static inline void dm_merc_trace_add(dm_merc_trace_gear *t, uint32_t id) {
    if (!t) return;
    if (t->count < UINT16_MAX) t->count++;
    t->fingerprint += dm_merc_trace_item_mix(id);
}
#endif
