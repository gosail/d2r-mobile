// Native PC keyboard actions decoded from the selected character's .key.
// This is a separate input source from gamepad/touch and legacy .kbm.
#pragma once
#include <stdint.h>
void dm_pc_input_event(unsigned hid,int pressed);
void dm_pc_input_reset(void);
int dm_pc_input_supported_action(uint32_t id);
// Test/diagnostic: most recently resolved native command for this physical key.
int dm_pc_input_latched_command(unsigned hid);
