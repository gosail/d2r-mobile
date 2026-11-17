#include "../dm_dualmerc_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static dm_dualmerc_saved_state s, decoded, unchanged;
static uint8_t wire[DM_DUALMERC_STATE_BYTES_MAX];
static dm_dualmerc_saved_slot candidate;

static dm_dualmerc_saved_slot make(unsigned act, unsigned diff,
                                   unsigned cls, uint64_t stable_item_id) {
    dm_dualmerc_saved_slot m = {0};
    m.occupied = 1; m.act = (uint8_t)act;
    m.difficulty = (uint8_t)diff; m.monster_class = cls;
    m.hireling_row = cls; m.name_id = act; m.level = 20;
    m.experience = 10000; m.skill_profile = 3;
    if (stable_item_id) {
        m.gear_count = 1;
        m.gear[0].body_slot = 3;
        m.gear[0].persistent_item_id = stable_item_id;
        m.gear[0].size = 4;
        memcpy(m.gear[0].data, "ITEM", 4);
    }
    return m;
}

static size_t encode(void) {
    size_t n = 0;
    assert(dm_dualmerc_state_encode(&s, wire, sizeof wire, &n) == DM_DUALMERC_CODEC_OK);
    return n;
}

static void empty_and_corruption(void) {
    memset(&s, 0, sizeof s);
    assert(dm_dualmerc_state_valid(&s));
    size_t n = encode();
    assert(n == 92);
    assert(dm_dualmerc_state_decode(wire, n, &decoded) == DM_DUALMERC_CODEC_OK);
    assert(!memcmp(&s, &decoded, sizeof s));
    assert(dm_dualmerc_state_decode(wire, n - 1, &decoded) == DM_DUALMERC_CODEC_INVALID);
    wire[n-5] ^= 1;
    unchanged = decoded;
    assert(dm_dualmerc_state_decode(wire, n, &decoded) == DM_DUALMERC_CODEC_INTEGRITY);
    assert(!memcmp(&decoded, &unchanged, sizeof s));
    wire[n-5] ^= 1;
}

static void recruit_and_replace(void) {
    candidate = make(1, 0, 271, 0x10001);
    assert(!dm_dualmerc_state_apply_verified(&s, &candidate, 0, 0));
    assert(dm_dualmerc_state_apply_verified(&s, &candidate, 0, 1));
    assert(!dm_dualmerc_state_apply_verified(&s, &candidate, 1, 1));

    candidate = make(2, 2, 338, 0x10002);
    assert(dm_dualmerc_state_apply_verified(&s, &candidate, 1, 1));
    assert(s.slot[0].gear[0].persistent_item_id == 0x10001);
    assert(s.slot[1].gear[0].persistent_item_id == 0x10002);

    uint64_t gen = s.generation;
    candidate = make(2, 0, 338, 0x20002);
    assert(!dm_dualmerc_state_apply_verified(&s, &candidate, 0, 1));
    assert(dm_dualmerc_state_apply_verified(&s, &candidate, 1, 1));
    assert(s.slot[0].gear[0].persistent_item_id == 0x10001);
    assert(s.slot[1].gear[0].persistent_item_id == 0x20002);
    assert(s.generation == gen + 1);

    candidate = make(5, 0, 560, 0x50005);
    assert(dm_dualmerc_state_apply_verified(&s, &candidate, 0, 1));
    assert(s.slot[1].gear[0].persistent_item_id == 0x20002);
    candidate = make(3, 0, 359, 0x20002); /* gear already belongs to Act II */
    assert(!dm_dualmerc_state_apply_verified(&s, &candidate, 0, 1));
    assert(s.slot[0].act == 5);
    assert(s.slot[0].gear[0].persistent_item_id == 0x50005);
    assert(dm_dualmerc_state_valid(&s));
}

static void roundtrip_and_limits(void) {
    size_t n = encode();
    assert(dm_dualmerc_state_decode(wire, n, &decoded) == DM_DUALMERC_CODEC_OK);
    assert(!memcmp(&s, &decoded, sizeof s));
    assert(!memcmp(wire, "SWM2", 4));
    assert(dm_dualmerc_load_u64(wire+8) == s.generation);

    dm_dualmerc_saved_slot original = s.slot[0];
    s.slot[0] = make(2, 1, 338, 0x10001);
    assert(!dm_dualmerc_state_valid(&s)); /* same Act II, different difficulty */
    s.slot[0] = original;

    assert(dm_dualmerc_state_valid(&s));
    s.native_slot = 1;
    assert(dm_dualmerc_state_valid(&s)); /* either equipped merc may be native */
    n = encode();
    assert(dm_dualmerc_state_decode(wire, n, &decoded) == DM_DUALMERC_CODEC_OK);
    assert(decoded.native_slot == 1);
    s.native_slot = 0;
    s.slot[0] = (dm_dualmerc_saved_slot){0};
    assert(!dm_dualmerc_state_valid(&s)); /* native slot must be occupied */
    s.slot[0] = original;
    assert(dm_dualmerc_state_valid(&s));
    n = encode();
    size_t not_written = 99;
    assert(dm_dualmerc_state_encode(&s, wire, n-1, &not_written) ==
           DM_DUALMERC_CODEC_CAPACITY);
    assert(not_written == 0);
}

int main(void) {
    empty_and_corruption();
    recruit_and_replace();
    roundtrip_and_limits();
    puts("dual mercenary roster persistence, checksum and independent gear: passed");
    return 0;
}
