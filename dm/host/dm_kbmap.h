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

// UI source: original Controls tab, independent from controller mapping data.
int dm_kbmap_controls_visible(void);
void dm_kbmap_controls_event(const char *category,const char *action,const char *arg);

// Physical mouse button (0 left, 1 right, 2 middle) -> logical
// 0 primary/click, 1 secondary/use, 2 toggle map. Same local config.
int dm_kbmap_mouse_action(unsigned physical);
int dm_kbmap_bind_mouse(unsigned physical,unsigned action);

// Called on the guest game thread when the active/selected character changes.
void dm_kbmap_select_character(const char *name);
// Invoked before a transactional game save; never touches native .ctl.
void dm_kbmap_pre_commit(const char *workdir);
