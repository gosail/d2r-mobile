// Native PC keyboard actions decoded from the selected character's .key.
// It uses the guest's original .key while controller .ctl remains independent.
#pragma once
#include <stdint.h>
void dm_pc_input_event(unsigned hid,int pressed);
void dm_pc_input_reset(void);
// Guest-frame dispatch of menu panel requests; input/UI threads only enqueue.
void dm_pc_input_guest_step(void);
// Resolve physical wheel delta via 0x103/0x104 .key records. Returns -1/0/+1
// guest right-stick scroll direction, dispatching known one-shot commands.
int dm_pc_input_wheel(int delta);
// Native mouse pseudo-VK 0x100 (middle) and optional extra buttons 0x101/2.
// Only well-understood one-shot commands are forwarded, no fake left click.
int dm_pc_input_mouse_special(unsigned pseudo_vk);
// Keep the established PC middle-map and wheel skill-selection defaults if
// the selected native .key genuinely has no corresponding pseudo-VK record.
int dm_pc_input_middle_or_default(void);
int dm_pc_input_wheel_or_default(int delta);
int dm_pc_input_supported_action(uint32_t id);
// Test/diagnostic: most recently resolved native command for this physical key.
int dm_pc_input_latched_command(unsigned hid);
