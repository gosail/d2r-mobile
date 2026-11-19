/*
 * Read-only Diablo II: Resurrected classic D2S mercenary header anchor.
 * Versions 97/98/99 use pre-v105 offsets (177 dead, 179 seed, 183 name,
 * 185 Hireling row, 187 experience). Version 105 MOVED those fields and
 * MUST NOT be accepted. Never edits or recodes an original .d2s.
 *
 * This is a consistency check for the existing SWM2 sidecar; verifying
 * the header does not prove that native second-merc items can be restored.
 * Call only on a byte-complete committed or uncommitted snapshot.
 */
#ifndef DM_DUALMERC_D2S_H
#define DM_DUALMERC_D2S_H
#include "dm_dualmerc_state.h"
#include <stddef.h>
#include <stdint.h>

#define DM_DUALMERC_D2S_HEADER_MIN 765u
#define DM_DUALMERC_D2S_MAX (16u*1024u*1024u)
typedef struct {
    uint32_t version, length, checksum, seed, exp;
    uint16_t dead, name_id, hireling_row;
} dm_dualmerc_native_anchor;

static inline uint32_t dm_dualmerc_d2s_checksum(const uint8_t *data,
                                                 size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; data && i < len; ++i) {
        uint8_t c = (i >= 12 && i < 16) ? 0 : data[i];
        sum = (sum << 1) | (sum >> 31);
        sum += c;
    }
    return sum;
}
static inline int dm_dualmerc_d2s_read(const uint8_t *buf, size_t size,
                                        dm_dualmerc_native_anchor *out) {
    if (!buf || !out || size < DM_DUALMERC_D2S_HEADER_MIN ||
        size > DM_DUALMERC_D2S_MAX ||
        dm_dualmerc_load_u32(buf) != 0xaa55aa55u) return 0;
    uint32_t ver = dm_dualmerc_load_u32(buf + 4);
    if (ver != 97 && ver != 98 && ver != 99) return 0;
    if (dm_dualmerc_load_u32(buf + 8) != size ||
        dm_dualmerc_d2s_checksum(buf, size) !=
            dm_dualmerc_load_u32(buf + 12)) return 0;
    dm_dualmerc_native_anchor temp = {
        .version=ver, .length=(uint32_t)size,
        .checksum=dm_dualmerc_load_u32(buf+12),
        .dead=dm_dualmerc_load_u16(buf+177),
        .seed=dm_dualmerc_load_u32(buf+179),
        .name_id=dm_dualmerc_load_u16(buf+183),
        .hireling_row=dm_dualmerc_load_u16(buf+185),
        .exp=dm_dualmerc_load_u32(buf+187),
    };
    /* The seed is the vanilla hireling's actual saved identity token.
     * A zero seed means it may not exist and must never be used to
     * corroborate a sidecar's claimed native-slot identity. */
    if (!temp.seed) return 0;
    *out = temp;
    return 1;
}
/* Compare the occupied SWM2 native slot with the original game's saved
 * hireling: both Hireling row and name must match. Restrict to exact XP;
 * a stale pre-commit sidecar must fail instead of duplicating equipment. */
static inline int dm_dualmerc_d2s_matches_state(
    const dm_dualmerc_native_anchor *v,
    const dm_dualmerc_saved_state *s) {
    if (!v || !s || !dm_dualmerc_state_valid(s) ||
        s->native_slot >= DM_DUALMERC_SLOTS) return 0;
    const dm_dualmerc_saved_slot *m = &s->slot[s->native_slot];
    return m->occupied && s->native_seed == v->seed &&
           s->native_d2s_checksum == v->checksum &&
           m->hireling_row == v->hireling_row &&
           m->name_id == v->name_id && m->experience == v->exp &&
           (m->dead != 0) == (v->dead != 0);
}
/* Called ONLY after the engine has completed the native .d2s save and the
 * precise post-hire identity/gear ownership has independently been confirmed.
 * Binds a provisional SWM2 state (seed/checksum cleared by apply_verified)
 * to a byte-verified original save WITHOUT editing the original .d2s.
 *
 * This is a pure preparation step; it does not read the filesystem or prove
 * that a second mercenary's item bitstream can be materialized on load.
 * The caller must still re-read the SAME .d2s under the save transaction
 * lock via dm_dualmerc_disk_save_verified before publishing the sidecar. */
static inline int dm_dualmerc_d2s_bind_after_save(
    dm_dualmerc_saved_state *state,
    const dm_dualmerc_native_anchor *native,
    int engine_hire_and_save_confirmed)
{
    if (!engine_hire_and_save_confirmed || !state || !native ||
        !dm_dualmerc_state_valid(state) ||
        !native->seed || !native->checksum ||
        (native->version != 97 && native->version != 98 &&
         native->version != 99))
        return 0;
    const dm_dualmerc_saved_slot *m = &state->slot[state->native_slot];
    if (!m->occupied || m->hireling_row != native->hireling_row ||
        m->name_id != native->name_id ||
        m->experience != native->exp ||
        ((m->dead != 0) != (native->dead != 0)))
        return 0;
    dm_dualmerc_saved_state candidate = *state;
    /* Rebinding to a NEW identity is allowed only while provisional.
     * A previously persisted, bound sidecar cannot silently switch native
     * merc identity without a separately confirmed hire mutation. */
    if ((candidate.native_seed || candidate.native_d2s_checksum) &&
        candidate.native_seed != native->seed) return 0;
    candidate.native_seed = native->seed;
    candidate.native_d2s_checksum = native->checksum;
    if (!dm_dualmerc_d2s_matches_state(native,&candidate))
        return 0;
    *state=candidate;
    return 1;
}
#endif
