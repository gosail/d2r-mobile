// Physical-keyboard bridge: no private character configuration file.
// Guest native character .key owns key bindings and .ctl owns gamepad bindings.
#pragma once
enum { DM_KB_MOVE_UP=16, DM_KB_MOVE_LEFT=17, DM_KB_MOVE_DOWN=18, DM_KB_MOVE_RIGHT=19,
       DM_KB_ENTER=40, DM_KB_ESCAPE=41, DM_KB_MAP=42,
       DM_KB_SKILL1=48, DM_KB_SKILL8=55, DM_KB_UNBOUND=255 };
int dm_kbmap_action(unsigned usage); // built-in menu fallback
int dm_kbmap_bind(unsigned usage,unsigned action); // no second persistent mapper
int dm_kbmap_restore_defaults_checked(void); // not an original game .key writer
void dm_kbmap_restore_defaults(void);
void dm_kbmap_key_event(unsigned usage,int down);
void dm_kbmap_reset(void);
// Last explicit physical keyboard/mouse owner, independent of native .ctl.
void dm_kbmap_external_owner(int on);
int dm_kbmap_is_external_owner(void);
int dm_kbmap_pc_mode(void); // native mode on when original .key exists
int dm_kbmap_set_pc_mode(int enabled); // compatibility no-op for enabled=true
int dm_kbmap_controls_visible(void);
void dm_kbmap_controls_event(const char *category,const char *action,const char *arg);
int dm_kbmap_mouse_action(unsigned physical); // fixed PC mouse roles
int dm_kbmap_bind_mouse(unsigned physical,unsigned action); // no private storage
void dm_kbmap_select_character(const char *name);
void dm_kbmap_pre_commit(const char *workdir); // preserve guest-native .key edits during Commit
