// Conservative reader/writer for the observed Diablo II .key record variant.
// This is intentionally separate from D2R Mobile's controller-emulation .kbm.
// Only the observed 0x0025 header variant is accepted; its meaning is unknown.
#pragma once
#include <stddef.h>
#include <stdint.h>

enum {
    DM_KEYFILE_OK=0,
    DM_KEYFILE_INVALID=-1,
    DM_KEYFILE_UNSUPPORTED=-2,
    DM_KEYFILE_NOT_FOUND=-3,
    DM_KEYFILE_IO=-4,
    DM_KEYFILE_CONFLICT=-5
};
typedef struct {
    uint8_t *raw;
    uint8_t *baseline;
    size_t size;
    size_t records;
    uint16_t observed_header;
} dm_keyfile;

// Parses two-byte header + a variable number of ten-byte records.
// Unknown/reserved bytes are retained verbatim, including records for IDs
// the UI cannot yet interpret. No source buffer is modified.
int dm_keyfile_parse(dm_keyfile *out,const void *data,size_t size);
int dm_keyfile_read(dm_keyfile *out,const char *path);
void dm_keyfile_free(dm_keyfile *out);
const uint8_t *dm_keyfile_data(const dm_keyfile *file,size_t *size);

// slot 0 = observed flag 1 (primary); slot 1 = observed flag 0 (secondary).
// The flag's exact native semantics still require controlled in-game tests.
int dm_keyfile_get(const dm_keyfile *file,uint32_t id,unsigned slot,uint16_t *code);
int dm_keyfile_set(dm_keyfile *file,uint32_t id,unsigned slot,uint16_t code);
int dm_keyfile_dirty(const dm_keyfile *file);

// Conflict-aware, backup-preserving, sibling-temp + fsync + atomic rename.
// Refuses to replace a file whose bytes differ from those originally loaded.
int dm_keyfile_write_atomic(dm_keyfile *file,const char *path);
