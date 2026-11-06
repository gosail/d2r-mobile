// Character-scoped native Diablo II keyboard profile, independent of .kbm
// (legacy keyboard-to-controller emulation) and original gamepad .ctl.
#pragma once
#include <stdint.h>

// 0..56 action IDs belong to the native 0x25 .key format.
// Returns DM_KEYFILE_* codes (see dm_keyfile.h).
int dm_pc_profile_select(const char *name);
int dm_pc_profile_get(uint32_t id,unsigned slot,uint16_t *vk);
int dm_pc_profile_bind(uint32_t id,unsigned slot,uint16_t vk,
                       uint32_t *conflict,unsigned *conflict_slot);
int dm_pc_profile_reset_legacy(void);
int dm_pc_profile_reset_resurrected(void);
int dm_pc_profile_action_for_vk(uint16_t vk,uint32_t *id,unsigned *slot);
int dm_pc_profile_action_for_hid(unsigned usage,uint32_t *id,unsigned *slot);
void dm_pc_profile_clear(void);

// Portable, layout-independent HID usage -> Win32 VK for this .key variant.
// Does not convert to ASCII (e.g. top-row '1' is VK_1 regardless of Shift).
// -1 means not representable in this legacy native-key format.
int dm_pc_vk_from_hid(unsigned usage);
