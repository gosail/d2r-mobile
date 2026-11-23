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
    assert(n == 100);
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
    assert(!dm_dualmerc_state_apply_verified(&s, &candidate, 0, 0, 0));
    assert(dm_dualmerc_state_apply_verified(&s, &candidate, 0, 0, 1));
    assert(!dm_dualmerc_state_apply_verified(&s, &candidate, 1, 1, 1));

    candidate = make(2, 2, 338, 0x10002);
    assert(dm_dualmerc_state_apply_verified(&s, &candidate, 1, 1, 1));
    assert(s.native_slot == 1); /* new hire is now backed by vanilla .d2s */
    assert(s.slot[0].gear[0].persistent_item_id == 0x10001);
    assert(s.slot[1].gear[0].persistent_item_id == 0x10002);

    uint64_t gen = s.generation;
    candidate = make(2, 0, 338, 0x20002);
    assert(!dm_dualmerc_state_apply_verified(&s, &candidate, 0, 0, 1));
    /* A replacement must NOT erase an old equipped mercenary's item.
     * First remove it through the native equipment UI; this simulation
     * models the already-verified completed unequip transaction. */
    assert(!dm_dualmerc_state_apply_verified(&s, &candidate, 1, 1, 1));
    assert(s.slot[1].gear[0].persistent_item_id == 0x10002);
    assert(s.generation == gen);
    memset(s.slot[1].gear, 0, sizeof s.slot[1].gear);
    s.slot[1].gear_count = 0;
    assert(dm_dualmerc_state_apply_verified(&s, &candidate, 1, 1, 1));
    assert(s.slot[0].gear[0].persistent_item_id == 0x10001);
    assert(s.slot[1].gear[0].persistent_item_id == 0x20002);
    assert(s.generation == gen + 1);

    candidate = make(5, 0, 560, 0x50005);
    assert(!dm_dualmerc_state_apply_verified(&s, &candidate, 0, 0, 1));
    assert(s.slot[0].gear[0].persistent_item_id == 0x10001);
    memset(s.slot[0].gear, 0, sizeof s.slot[0].gear);
    s.slot[0].gear_count = 0;
    assert(dm_dualmerc_state_apply_verified(&s, &candidate, 0, 0, 1));
    assert(s.slot[1].gear[0].persistent_item_id == 0x20002);
    candidate = make(3, 0, 359, 0x20002); /* gear already belongs to Act II */
    assert(!dm_dualmerc_state_apply_verified(&s, &candidate, 0, 0, 1));
    assert(s.slot[0].act == 5);
    assert(s.slot[0].gear[0].persistent_item_id == 0x50005);
    /* Now simulate confirmed native unequip. The collision alone must still
     * reject the candidate and must not lose Act V's stable identity. */
    memset(s.slot[0].gear, 0, sizeof s.slot[0].gear);
    s.slot[0].gear_count = 0;
    assert(!dm_dualmerc_state_apply_verified(&s, &candidate, 0, 0, 1));
    assert(s.slot[0].act == 5 && s.slot[0].gear_count == 0);
    assert(dm_dualmerc_state_valid(&s));
}

static void roundtrip_and_limits(void) {
    /* The in-memory state is provisional after native roster mutations:
     * it CANNOT be persisted until vanilla .d2s has been validated. */
    size_t rejected = 0;
    assert(dm_dualmerc_state_encode(&s, wire, sizeof wire, &rejected) ==
           DM_DUALMERC_CODEC_INVALID && rejected == 0);
    s.native_d2s_checksum = 0xcafefeed;
    s.native_seed = 0x12345678;
    size_t n = encode();
    assert(dm_dualmerc_state_decode(wire, n, &decoded) == DM_DUALMERC_CODEC_OK);
    assert(!memcmp(&s, &decoded, sizeof s));
    assert(!memcmp(wire, "SWM2", 4));
    assert(dm_dualmerc_load_u64(wire+8) == s.generation);
    assert(dm_dualmerc_load_u16(wire+4) == 3);
    assert(dm_dualmerc_load_u32(wire+24) == 0xcafefeed);
    assert(dm_dualmerc_load_u32(wire+28) == 0x12345678);

    dm_dualmerc_saved_slot original = s.slot[0];
    s.slot[0] = make(2, 1, 338, 0x10001);
    assert(!dm_dualmerc_state_valid(&s)); /* same Act II, different difficulty */
    s.slot[0] = original;

    assert(dm_dualmerc_state_valid(&s));
    /* A removed item whose count was reset without clearing the backing
     * bitstream must never pass the codec or replacement preflight. */
    dm_dualmerc_saved_state clean = s;
    s.slot[0].gear[1].body_slot = 2;
    s.slot[0].gear[1].persistent_item_id = 0xbadc0fffull;
    s.slot[0].gear[1].size = 4;
    assert(!dm_dualmerc_state_valid(&s));
    size_t rejected_orphan = 17;
    assert(dm_dualmerc_state_encode(&s, wire, sizeof wire, &rejected_orphan) ==
           DM_DUALMERC_CODEC_INVALID && rejected_orphan == 0);
    s = clean;
    /* Decreasing an equipped bitstream's size without wiping its old tail
     * cannot silently pass canonical serialization. */
    s.slot[0].gear[0].data[s.slot[0].gear[0].size] = 0xa5;
    assert(!dm_dualmerc_state_valid(&s));
    size_t rejected_tail = 17;
    assert(dm_dualmerc_state_encode(&s, wire, sizeof wire, &rejected_tail) ==
           DM_DUALMERC_CODEC_INVALID && rejected_tail == 0);
    s = clean;
    /* Even gear_count==0 is not proof of an empty mercenary if the
     * in-memory former equipment record was not cleared. */
    dm_dualmerc_saved_slot gear_backup = s.slot[1];
    s.slot[1].gear_count = 0;
    assert(!dm_dualmerc_state_valid(&s));
    s.slot[1] = gear_backup;
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

static void atomic_equipment_ownership(void) {
    /* Previous tests leave one equipped Act II merc and one empty Act V.
     * Transfer its actual saved item record to the other merc as ONE
     * authorized two-container snapshot, without any intermediate duplicate. */
    assert(dm_dualmerc_state_valid(&s));
    static dm_dualmerc_saved_slot next[DM_DUALMERC_SLOTS];
    memcpy(next,s.slot,sizeof next);
    assert(s.slot[0].gear_count==0 && s.slot[1].gear_count==1);
    next[0].gear_count=1;
    next[0].gear[0]=next[1].gear[0];
    next[1].gear_count=0;
    memset(next[1].gear,0,sizeof next[1].gear);
    unchanged=s;
    assert(!dm_dualmerc_state_gears_apply_verified(&s,next,0));
    assert(!memcmp(&s,&unchanged,sizeof s));
    assert(dm_dualmerc_state_gears_apply_verified(&s,next,1));
    assert(s.slot[0].gear_count==1 && s.slot[1].gear_count==0);
    assert(s.slot[0].gear[0].persistent_item_id==0x20002);
    assert(s.generation==unchanged.generation+1);
    assert(s.native_seed==0 && s.native_d2s_checksum==0);

    /* Reject a copied equipment record present in both mercs without
     * touching the authoritative state or its generation. */
    unchanged=s;
    memcpy(next,s.slot,sizeof next);
    next[1].gear_count=1;
    next[1].gear[0]=next[0].gear[0];
    assert(!dm_dualmerc_state_gears_apply_verified(&s,next,1));
    assert(!memcmp(&s,&unchanged,sizeof s));

    memcpy(next,s.slot,sizeof next);
    next[1].act=1; /* Gear updates cannot replace hireling identities. */
    assert(!dm_dualmerc_state_gears_apply_verified(&s,next,1));
    assert(!memcmp(&s,&unchanged,sizeof s));
    next[1].act=s.slot[1].act;
    next[0].gear[0].persistent_item_id=0; /* No empty item identities. */
    assert(!dm_dualmerc_state_gears_apply_verified(&s,next,1));
    assert(!memcmp(&s,&unchanged,sizeof s));

    assert(!dm_dualmerc_state_gears_apply_verified(NULL,next,1));
    assert(!dm_dualmerc_state_gears_apply_verified(&s,NULL,1));
    /* A copy of the caller's own array is safe even with in-place alias. */
    assert(dm_dualmerc_state_gears_apply_verified(&s,s.slot,1));
    assert(!memcmp(&s,&unchanged,sizeof s)); /* No-op never rebinds. */
}

int main(void) {
    empty_and_corruption();
    recruit_and_replace();
    roundtrip_and_limits();
    atomic_equipment_ownership();
    puts("dual mercenary roster persistence, checksum and independent gear: passed");
    return 0;
}
