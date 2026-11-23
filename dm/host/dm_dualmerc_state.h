/*
 * Offline two-mercenary sidecar schema and safety checks (no game writes).
 * v1 is a portable canonical binary container for two independent hireling
 * identities and ten equipped-item bitstreams per hireling.
 * Consumers must supply the game's actual item codec and verify ownership.
 * This header does not hook the original game's single-merc save.
 */
#ifndef DM_DUALMERC_STATE_H
#define DM_DUALMERC_STATE_H
#include "dm_dualmerc_policy.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define DM_DUALMERC_GEAR_MAX 10
#define DM_DUALMERC_ITEM_BYTES_MAX 1024
#define DM_DUALMERC_STATE_BYTES_MAX (2u * DM_DUALMERC_GEAR_MAX * (DM_DUALMERC_ITEM_BYTES_MAX + 16u) + 256u)
#define DM_DUALMERC_STATE_VERSION 3

typedef struct {
    uint8_t body_slot;
    uint16_t size;
    uint64_t persistent_item_id; /* Stable identity from game save, NOT runtime unit ID. */
    uint8_t data[DM_DUALMERC_ITEM_BYTES_MAX];
} dm_dualmerc_equipped_item;

typedef struct {
    uint8_t occupied, act, difficulty, dead;
    uint32_t hireling_row, monster_class, name_id, level, experience, skill_profile;
    uint8_t gear_count;
    dm_dualmerc_equipped_item gear[DM_DUALMERC_GEAR_MAX];
} dm_dualmerc_saved_slot;

typedef struct {
    uint64_t generation;
    uint8_t native_slot; /* Which of the two is serialized by original .d2s */
    uint32_t native_d2s_checksum; /* Exact committed vanilla .d2s CRC checksum */
    uint32_t native_seed;         /* Vanilla merc control seed, never runtime unit ID */
    dm_dualmerc_saved_slot slot[DM_DUALMERC_SLOTS];
} dm_dualmerc_saved_state;

typedef enum {
    DM_DUALMERC_CODEC_OK = 0,
    DM_DUALMERC_CODEC_INVALID = -1,
    DM_DUALMERC_CODEC_CAPACITY = -2,
    DM_DUALMERC_CODEC_INTEGRITY = -3
} dm_dualmerc_codec_result;

static inline void dm_dualmerc_store_u16(uint8_t *d, uint16_t v) {
    d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8);
}
static inline void dm_dualmerc_store_u32(uint8_t *d, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) d[i] = (uint8_t)(v >> (i * 8));
}
static inline void dm_dualmerc_store_u64(uint8_t *d, uint64_t v) {
    for (unsigned i = 0; i < 8; ++i) d[i] = (uint8_t)(v >> (i * 8));
}
static inline uint16_t dm_dualmerc_load_u16(const uint8_t *d) {
    return (uint16_t)(d[0] | (uint16_t)d[1] << 8);
}
static inline uint32_t dm_dualmerc_load_u32(const uint8_t *d) {
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= (uint32_t)d[i] << (i * 8);
    return v;
}
static inline uint64_t dm_dualmerc_load_u64(const uint8_t *d) {
    uint64_t v = 0;
    for (unsigned i = 0; i < 8; ++i) v |= (uint64_t)d[i] << (i * 8);
    return v;
}
static inline uint32_t dm_dualmerc_crc32(const uint8_t *d, size_t n) {
    uint32_t c = UINT32_MAX;
    for (size_t i = 0; i < n; ++i) {
        c ^= d[i];
        for (int bit = 0; bit < 8; ++bit)
            c = (c >> 1) ^ ((uint32_t)0xedb88320u & (uint32_t)-(int32_t)(c & 1u));
    }
    return ~c;
}

/* Empty slots must be entirely zero. No item ownership token may appear
 * in both mercenaries or twice in the same equipment container. */
static inline int dm_dualmerc_state_valid(const dm_dualmerc_saved_state *s) {
    if (!s) return 0;
    dm_dualmerc_roster roster = {0};
    unsigned occupied = 0;
    for (unsigned a = 0; a < DM_DUALMERC_SLOTS; ++a) {
        const dm_dualmerc_saved_slot *m = &s->slot[a];
        if (!m->occupied) {
            dm_dualmerc_saved_slot empty = {0};
            if (memcmp(m, &empty, sizeof empty)) return 0;
            continue;
        }
        if (m->occupied != 1 || !dm_dualmerc_valid_act(m->act) ||
            m->difficulty > 2 || m->dead > 1 || !m->level ||
            m->gear_count > DM_DUALMERC_GEAR_MAX ||
            dm_dualmerc_act_for_class(m->monster_class) != m->act) return 0;
        ++occupied;
        roster.slot[a].occupied = m->occupied;
        roster.slot[a].act = m->act;
        roster.slot[a].difficulty = m->difficulty;
        roster.slot[a].dead = m->dead;
        unsigned used = 0;
        for (unsigned i = 0; i < m->gear_count; ++i) {
            const dm_dualmerc_equipped_item *it = &m->gear[i];
            if (it->body_slot >= DM_DUALMERC_GEAR_MAX ||
                (used & (1u << it->body_slot)) || !it->persistent_item_id ||
                !it->size || it->size > DM_DUALMERC_ITEM_BYTES_MAX) return 0;
            /* A shortened bitstream with stale nonzero trailing bytes is
             * not a canonical saved item. Do not silently discard unknown
             * payload that might be the only previous equipment record. */
            for (size_t byte = it->size; byte < sizeof it->data; ++byte)
                if (it->data[byte]) return 0;
            used |= 1u << it->body_slot;
            for (unsigned j = 0; j < a; ++j)
                for (unsigned k = 0; k < s->slot[j].gear_count; ++k)
                    if (s->slot[j].gear[k].persistent_item_id == it->persistent_item_id)
                        return 0;
            for (unsigned k = 0; k < i; ++k)
                if (m->gear[k].persistent_item_id == it->persistent_item_id) return 0;
        }
        /* Retired equipment must be explicitly cleared. A stale bitstream
         * outside gear_count is still an item-ownership claim in memory;
         * accepting it would let a later hire overwrite the sole copy. */
        for (unsigned i = m->gear_count; i < DM_DUALMERC_GEAR_MAX; ++i) {
            const dm_dualmerc_equipped_item *it = &m->gear[i];
            /* Compare fields explicitly: structure padding is not a
             * portable indication of a retained item. */
            if (it->body_slot || it->size || it->persistent_item_id) return 0;
            for (size_t byte = 0; byte < sizeof it->data; ++byte)
                if (it->data[byte]) return 0;
        }
    }
    /* Exactly one occupied slot is backed by vanilla .d2s; the other
     * is sidecar-only. Without this tag, reload may duplicate an item. */
    return dm_dualmerc_roster_valid(&roster) &&
           (!!s->native_d2s_checksum == !!s->native_seed) &&
           (occupied ? s->native_slot < DM_DUALMERC_SLOTS &&
                       s->slot[s->native_slot].occupied :
                       s->native_slot == 0 && !s->native_seed);
}

static inline dm_dualmerc_roster dm_dualmerc_state_roster(const dm_dualmerc_saved_state *s) {
    dm_dualmerc_roster r = {0};
    if (!s || !dm_dualmerc_state_valid(s)) return r;
    for (unsigned i = 0; i < DM_DUALMERC_SLOTS; ++i) {
        const dm_dualmerc_saved_slot *m = &s->slot[i];
        r.slot[i].occupied = m->occupied;
        r.slot[i].act = m->act;
        r.slot[i].difficulty = m->difficulty;
        r.slot[i].dead = m->dead;
    }
    return r;
}

/* Caller MUST first verify the native hire transaction, item custody,
 * selected target and which slot the original .d2s now serializes. No state change occurs on failed validation. */
static inline int dm_dualmerc_state_apply_verified(
    dm_dualmerc_saved_state *s, const dm_dualmerc_saved_slot *next,
    unsigned chosen_slot, unsigned native_after, int engine_confirmed)
{
    if (!engine_confirmed || !s || !next || !next->occupied ||
        native_after >= DM_DUALMERC_SLOTS ||
        !dm_dualmerc_state_valid(s) || s->generation == UINT64_MAX) return 0;
    dm_dualmerc_roster r = dm_dualmerc_state_roster(s);
    dm_dualmerc_decision d = dm_dualmerc_plan(&r, next->act, next->difficulty);
    if (!dm_dualmerc_choice_allowed(&d, chosen_slot)) return 0;
    /* The source slot can be overwritten only after the player has removed
     * its gear through the NATIVE equipment transaction. The sidecar codec
     * cannot itself transfer item ownership; overwriting occupied gear here
     * would permanently drop valid bitstreams from the only known record. */
    if (s->slot[chosen_slot].occupied &&
        s->slot[chosen_slot].gear_count != 0) return 0;
    dm_dualmerc_saved_slot old = s->slot[chosen_slot];
    uint8_t previous_native = s->native_slot;
    uint32_t old_crc = s->native_d2s_checksum, old_seed = s->native_seed;
    s->slot[chosen_slot] = *next;
    s->native_slot = (uint8_t)native_after;
    /* Native .d2s must be re-read AFTER the hire and its native save
     * transaction. A previous checksum/seed cannot attest to new gear or
     * a different mercenary record. Never keep an obsolete binding. */
    s->native_d2s_checksum = 0;
    s->native_seed = 0;
    if (!dm_dualmerc_state_valid(s)) {
        s->slot[chosen_slot] = old;
        s->native_slot = previous_native;
        s->native_d2s_checksum = old_crc;
        s->native_seed = old_seed;
        return 0;
    }
    ++s->generation;
    return 1;
}

/* Atomically update BOTH independently-owned item inventories after an
 * engine-confirmed equipment transaction. Callers must first read native
 * authoritative per-merc item containers and serialize the real saved item
 * bytes; this routine cannot prove that a reported game operation happened.
 *
 * Taking both complete slot snapshots avoids the intermediate "item in
 * both mercenaries" state during a same-transaction equipment transfer.
 * Identity/progression/Act are immutable here. No change (including to
 * generation or vanilla .d2s binding) occurs on any validation failure.
 * On success the binding is made provisional: save both native and sidecar
 * together, and rebind only AFTER byte-verifying the native .d2s.
 *
 * This is NOT wired to a live 0x51/0x5b packet or the native inventory UI.
 */
static inline int dm_dualmerc_state_gears_apply_verified(
    dm_dualmerc_saved_state *s,
    const dm_dualmerc_saved_slot next[DM_DUALMERC_SLOTS],
    int native_inventory_and_ownership_confirmed)
{
    if (!s || !next || !native_inventory_and_ownership_confirmed ||
        !dm_dualmerc_state_valid(s) || s->generation == UINT64_MAX)
        return 0;
    unsigned changed=0;
    for (unsigned k=0; k<DM_DUALMERC_SLOTS; ++k) {
        const dm_dualmerc_saved_slot *old=&s->slot[k], *new_slot=&next[k];
        if (old->occupied != new_slot->occupied ||
            old->act != new_slot->act ||
            old->difficulty != new_slot->difficulty ||
            old->dead != new_slot->dead ||
            old->hireling_row != new_slot->hireling_row ||
            old->monster_class != new_slot->monster_class ||
            old->name_id != new_slot->name_id ||
            old->level != new_slot->level ||
            old->experience != new_slot->experience ||
            old->skill_profile != new_slot->skill_profile ||
            new_slot->gear_count > DM_DUALMERC_GEAR_MAX)
            return 0;
        if (old->gear_count != new_slot->gear_count ||
            memcmp(old->gear,new_slot->gear,sizeof old->gear))
            changed=1;
    }
    /* A no-op native snapshot must neither bump the sidecar generation nor
     * invalidate a binding that still matches the exact vanilla .d2s. */
    if (!changed) return 1;
    /* Snapshot the complete proposed state before modifying the caller.
     * This also protects against next pointing directly at s->slot[0]. */
    dm_dualmerc_saved_state proposed=*s;
    for (unsigned k=0; k<DM_DUALMERC_SLOTS; ++k) {
        proposed.slot[k].gear_count=next[k].gear_count;
        memcpy(proposed.slot[k].gear,next[k].gear,
               sizeof proposed.slot[k].gear);
    }
    proposed.native_seed=0;
    proposed.native_d2s_checksum=0;
    if (!dm_dualmerc_state_valid(&proposed))
        return 0;
    ++proposed.generation;
    *s=proposed;
    return 1;
}

/* Per-mercenary lifecycle ledger for a separately VERIFIED native death,
 * revival or XP/level update. Death never frees an Act slot or any equipped
 * item. Neither a client display packet nor a guessed Act is evidence that
 * the native action took place; caller must prove the original UNIT/owner.
 *
 * An explicit flag is intentionally required even for a death. This
 * routine ONLY updates a saved snapshot and is NOT connected to an
 * unverified revival opcode or a monster-spawn function.
 */
static inline int dm_dualmerc_state_lifecycle_apply_verified(
    dm_dualmerc_saved_state *s, unsigned slot_index,
    unsigned dead, uint32_t level, uint32_t experience,
    int native_unit_lifecycle_confirmed)
{
    if (!s || !native_unit_lifecycle_confirmed ||
        !dm_dualmerc_state_valid(s) ||
        slot_index >= DM_DUALMERC_SLOTS ||
        !s->slot[slot_index].occupied ||
        dead > 1 || !level || level < s->slot[slot_index].level ||
        experience < s->slot[slot_index].experience ||
        s->generation == UINT64_MAX)
        return 0;
    const dm_dualmerc_saved_slot *old=&s->slot[slot_index];
    if (old->dead == dead && old->level == level &&
        old->experience == experience) return 1; /* No-op. */
    dm_dualmerc_saved_state proposed=*s;
    dm_dualmerc_saved_slot *dst=&proposed.slot[slot_index];
    dst->dead=(uint8_t)dead;
    dst->level=level;
    dst->experience=experience;
    proposed.native_seed=0;
    proposed.native_d2s_checksum=0;
    if (!dm_dualmerc_state_valid(&proposed)) return 0;
    ++proposed.generation;
    *s=proposed;
    return 1;
}

/* Canonical format: magic SWM2, v3, two slots, generation, total size,
 * 32-byte header including native-slot index, native vanilla .d2s
 * checksum and merc seed, then one 32-byte record per slot and up to
 * 10 item bitstreams per slot, trailer CRC32. Never silently read v2.
 * Sidecar and original d2s must be in the same atomic transaction. */
static inline dm_dualmerc_codec_result dm_dualmerc_state_encode(
    const dm_dualmerc_saved_state *s, uint8_t *dst, size_t cap, size_t *written)
{
    if (written) *written = 0;
    if (!s || !dst || !written || !dm_dualmerc_state_valid(s) ||
        ((s->slot[0].occupied || s->slot[1].occupied) &&
          (!s->native_seed || !s->native_d2s_checksum)))
        return DM_DUALMERC_CODEC_INVALID;
    if (cap > DM_DUALMERC_STATE_BYTES_MAX) cap = DM_DUALMERC_STATE_BYTES_MAX;
    /* Preflight the *entire* serialization size. On insufficient capacity,
     * do not modify even one byte of caller's buffer. */
    size_t required = 32 + 4 + 32 * DM_DUALMERC_SLOTS;
    for (unsigned k = 0; k < DM_DUALMERC_SLOTS; ++k)
        for (unsigned i = 0; i < s->slot[k].gear_count; ++i)
            required += 12 + s->slot[k].gear[i].size;
    if (required > cap) return DM_DUALMERC_CODEC_CAPACITY;
    size_t len = 32;
    for (unsigned k = 0; k < DM_DUALMERC_SLOTS; ++k) {
        const dm_dualmerc_saved_slot *m = &s->slot[k];
        if (len + 32 + 4 > cap) return DM_DUALMERC_CODEC_CAPACITY;
        uint8_t *r = dst + len;
        memset(r, 0, 32);
        r[0] = m->occupied;
        if (m->occupied) {
            r[1] = m->act; r[2] = m->difficulty; r[3] = m->dead;
            dm_dualmerc_store_u32(r + 4, m->hireling_row);
            dm_dualmerc_store_u32(r + 8, m->monster_class);
            dm_dualmerc_store_u32(r + 12, m->name_id);
            dm_dualmerc_store_u32(r + 16, m->level);
            dm_dualmerc_store_u32(r + 20, m->experience);
            dm_dualmerc_store_u32(r + 24, m->skill_profile);
            r[28] = m->gear_count;
        }
        len += 32;
        for (unsigned i = 0; i < m->gear_count; ++i) {
            const dm_dualmerc_equipped_item *it = &m->gear[i];
            if (len + 12 + it->size + 4 > cap) return DM_DUALMERC_CODEC_CAPACITY;
            dst[len] = it->body_slot;
            dst[len+1] = 0;
            dm_dualmerc_store_u16(dst + len + 2, it->size);
            dm_dualmerc_store_u64(dst + len + 4, it->persistent_item_id);
            memcpy(dst + len + 12, it->data, it->size);
            len += 12 + it->size;
        }
    }
    if (len + 4 > cap) return DM_DUALMERC_CODEC_CAPACITY;
    memcpy(dst, "SWM2", 4);
    dm_dualmerc_store_u16(dst + 4, DM_DUALMERC_STATE_VERSION);
    dm_dualmerc_store_u16(dst + 6, DM_DUALMERC_SLOTS);
    dm_dualmerc_store_u64(dst + 8, s->generation);
    dm_dualmerc_store_u32(dst + 16, (uint32_t)(len + 4));
    dst[20] = s->native_slot;
    memset(dst + 21, 0, 3);
    dm_dualmerc_store_u32(dst + 24, s->native_d2s_checksum);
    dm_dualmerc_store_u32(dst + 28, s->native_seed);
    dm_dualmerc_store_u32(dst + len, dm_dualmerc_crc32(dst, len));
    *written = len + 4;
    return DM_DUALMERC_CODEC_OK;
}

/* On ANY decode failure the caller's existing state remains intact.
 * Pure/test-only until verified game item serialization is connected. */
static inline dm_dualmerc_codec_result dm_dualmerc_state_decode(
    const uint8_t *src, size_t n, dm_dualmerc_saved_state *dst)
{
    if (!src || !dst || n < 32 + 2 * 32 + 4 ||
        n > DM_DUALMERC_STATE_BYTES_MAX || memcmp(src, "SWM2", 4) ||
        dm_dualmerc_load_u16(src + 4) != DM_DUALMERC_STATE_VERSION ||
        dm_dualmerc_load_u16(src + 6) != DM_DUALMERC_SLOTS ||
        dm_dualmerc_load_u32(src + 16) != n ||
        src[21] || src[22] || src[23])
        return DM_DUALMERC_CODEC_INVALID;
    if (dm_dualmerc_crc32(src, n - 4) != dm_dualmerc_load_u32(src + n - 4))
        return DM_DUALMERC_CODEC_INTEGRITY;
    dm_dualmerc_saved_state tmp = {0};
    tmp.generation = dm_dualmerc_load_u64(src + 8);
    tmp.native_slot = src[20];
    tmp.native_d2s_checksum = dm_dualmerc_load_u32(src + 24);
    tmp.native_seed = dm_dualmerc_load_u32(src + 28);
    size_t at = 32;
    for (unsigned k = 0; k < DM_DUALMERC_SLOTS; ++k) {
        if (at + 32 > n - 4) return DM_DUALMERC_CODEC_INVALID;
        dm_dualmerc_saved_slot *m = &tmp.slot[k];
        const uint8_t *r = src + at;
        m->occupied = r[0];
        if (m->occupied) {
            m->act = r[1]; m->difficulty = r[2]; m->dead = r[3];
            m->hireling_row = dm_dualmerc_load_u32(r + 4);
            m->monster_class = dm_dualmerc_load_u32(r + 8);
            m->name_id = dm_dualmerc_load_u32(r + 12);
            m->level = dm_dualmerc_load_u32(r + 16);
            m->experience = dm_dualmerc_load_u32(r + 20);
            m->skill_profile = dm_dualmerc_load_u32(r + 24);
            m->gear_count = r[28];
            if (m->gear_count > DM_DUALMERC_GEAR_MAX)
                return DM_DUALMERC_CODEC_INVALID;
        } else if (memcmp(r + 1, (uint8_t[31]){0}, 31))
            return DM_DUALMERC_CODEC_INVALID;
        if (r[29] || r[30] || r[31]) return DM_DUALMERC_CODEC_INVALID;
        at += 32;
        for (unsigned i = 0; i < m->gear_count; ++i) {
            if (at + 12 > n - 4) return DM_DUALMERC_CODEC_INVALID;
            dm_dualmerc_equipped_item *it = &m->gear[i];
            it->body_slot = src[at];
            if (src[at + 1]) return DM_DUALMERC_CODEC_INVALID;
            it->size = dm_dualmerc_load_u16(src + at + 2);
            it->persistent_item_id = dm_dualmerc_load_u64(src + at + 4);
            if (!it->size || it->size > DM_DUALMERC_ITEM_BYTES_MAX ||
                at + 12 + it->size > n - 4)
                return DM_DUALMERC_CODEC_INVALID;
            memcpy(it->data, src + at + 12, it->size);
            at += 12 + it->size;
        }
    }
    if (at != n - 4 || !dm_dualmerc_state_valid(&tmp) ||
        ((tmp.slot[0].occupied || tmp.slot[1].occupied) &&
         (!tmp.native_d2s_checksum || !tmp.native_seed)))
        return DM_DUALMERC_CODEC_INVALID;
    *dst = tmp;
    return DM_DUALMERC_CODEC_OK;
}
#endif
