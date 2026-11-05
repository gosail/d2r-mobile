// D2R Mobile portable physical-keyboard mapping (USB HID usage IDs).
// Config is independent of the PC .key/.ctl format.
#pragma once
enum { DM_KB_MOVE_UP=16, DM_KB_MOVE_LEFT=17, DM_KB_MOVE_DOWN=18, DM_KB_MOVE_RIGHT=19,
       DM_KB_ENTER=40, DM_KB_ESCAPE=41, DM_KB_MAP=42,
       DM_KB_SKILL1=48, DM_KB_SKILL8=55, DM_KB_UNBOUND=255 };
int dm_kbmap_action(unsigned usage);
int dm_kbmap_bind(unsigned usage,unsigned action);
void dm_kbmap_restore_defaults(void);
void dm_kbmap_key_event(unsigned usage,int down);
void dm_kbmap_reset(void);
