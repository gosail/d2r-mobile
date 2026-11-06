// Native PC keyboard actions decoded from the selected character's .key.
// This is a separate input source from gamepad/touch and legacy .kbm.
#pragma once
#include <stdint.h>
void dm_pc_input_event(unsigned hid,int pressed);
void dm_pc_input_reset(void);
// Guest-frame dispatch of menu panel requests; input/UI threads only enqueue.
void dm_pc_input_guest_step(void);
// Resolve physical wheel delta via 0x103/0x104 .key records. Returns -1/0/+1
// guest right-stick scroll direction, dispatching known one-shot commands.
int dm_pc_input_wheel(int delta);
int dm_pc_input_supported_action(uint32_t id);
// Test/diagnostic: most recently resolved native command for this physical key.
int dm_pc_input_latched_command(unsigned hid);
