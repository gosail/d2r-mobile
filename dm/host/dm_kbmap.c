// Runtime-only physical keyboard -> Switch controller bridge.
// The guest owns .key keyboard commands and .ctl gamepad bindings.
// No private per-character file and no second persisted input mode.
#include "dm_kbmap.h"
#include "dm_pc_profile.h"
#include "dm_pc_input.h"
#include "dm_input.h"
#include "dm_world.h"
#include "dm_uiscreen.h"
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t km_mu=PTHREAD_MUTEX_INITIALIZER;
static unsigned char map[256],def[256],down[256],active[256],native_held[256];
static int inited;
// Once this character has a valid guest .key, never fall back to WASD just
// because the guest is temporarily rewriting or committing its profile.
static int native_seen;
static const int native_pc_mode=1;
static char hero[64];
static void defaults(unsigned char *m) {
    memset(m,DM_KB_UNBOUND,256);
    // The arrow cluster owns character movement. In particular A must
    // NEVER be treated as legacy "move left": native .key often binds A
    // to Character, producing the reported run + open-panel collision.
    m[0x52]=DM_KB_MOVE_UP;    m[0x50]=DM_KB_MOVE_LEFT;
    m[0x51]=DM_KB_MOVE_DOWN;  m[0x4f]=DM_KB_MOVE_RIGHT;
    m[0x0d]=0; m[0x0e]=1; m[0x18]=2; m[0x0c]=3;
    m[0x09]=4; m[0x0a]=5; m[0x14]=6; m[0x08]=7;
    m[0x1d]=8; m[0x06]=9;
    m[0x28]=DM_KB_ENTER; m[0x29]=DM_KB_ESCAPE;
    m[0x2b]=11; m[0x2e]=10; m[0x57]=10; m[0x2c]=0;
    // Menu and modal arrow navigation is selected at key-down below,
    // rather than treating the arrows as potion buttons during gameplay.
    // Temporary fallback skills mimic verified Switch HUD slots; the
    // character's actual .key replaces these when available.
    for (int k=0;k<8;k++) m[0x3a+k]=(unsigned char)(DM_KB_SKILL1+k);
    m[0x1e]=12; m[0x1f]=13; m[0x20]=14; m[0x21]=15;
    m[0x10]=DM_KB_MAP;
}

static void init_locked(void) {
    if(inited)return;
    defaults(def);memcpy(map,def,sizeof map);inited=1;
}
static void load_locked(void) { init_locked(); }
static int hero_name_ok(const char *name) {
    if(!name || !*name || strlen(name)>48 ||
       !strcmp(name,".") || !strcmp(name,".."))return 0;
    for(const unsigned char *p=(const unsigned char *)name;*p;p++)
        if(*p<' ' || *p=='/' || *p=='\\' || *p==127)return 0;
    return 1;
}
void dm_kbmap_select_character(const char *name) {
    if(!hero_name_ok(name))return;
    pthread_mutex_lock(&km_mu);
    init_locked();
    int changed=strcmp(hero,name)!=0;
    if(changed) {
        snprintf(hero,sizeof hero,"%s",name);
        memcpy(map,def,sizeof map);
        memset(down,0,sizeof down);memset(active,0,sizeof active);
        memset(native_held,0,sizeof native_held);
        native_seen=0;
        dm_kb_clear();
    }
    pthread_mutex_unlock(&km_mu);
    if(changed) {
        dm_pc_input_reset(); // no stale hero-specific held command
        (void)dm_pc_profile_select(name);
    }
}
int dm_kbmap_pc_mode(void) { return native_pc_mode; }
int dm_kbmap_set_pc_mode(int enabled) {
    // Native editors may still call this after writing .key. Never make a
    // successful .key write depend on a second profile file being writable.
    return enabled!=0;
}
int dm_kbmap_action(unsigned usage) {
    if(usage>=256)return DM_KB_UNBOUND;
    pthread_mutex_lock(&km_mu);init_locked();
    int action=map[usage];
    pthread_mutex_unlock(&km_mu);return action;
}
int dm_kbmap_bind(unsigned usage,unsigned action) {
    // .key command IDs cannot encode arbitrary HID -> gamepad assignments.
    // Do not claim a change is persistent if no original-format slot exists.
    (void)usage;(void)action;return 0;
}
int dm_kbmap_restore_defaults_checked(void) { return 0; }
void dm_kbmap_restore_defaults(void) {}
void dm_kbmap_reset(void) {
    pthread_mutex_lock(&km_mu);
    memset(down,0,sizeof down);memset(active,0,sizeof active);
    memset(native_held,0,sizeof native_held);
    pthread_mutex_unlock(&km_mu);
    dm_pc_input_reset();dm_kb_clear();
}
void dm_kbmap_key_event(unsigned usage,int pressed) {
    if(usage>=256)return;
    pthread_mutex_lock(&km_mu);
    load_locked();
    int was_native=native_held[usage],was_legacy=down[usage];
    int seen_native=native_seen;
    pthread_mutex_unlock(&km_mu);
    // A release always goes to the dispatcher that owned its key-down.
    // Never reinterpret a held key as the active save file changes.
    if(was_native) {
        if(!pressed) {
            pthread_mutex_lock(&km_mu);
            native_held[usage]=0;
            pthread_mutex_unlock(&km_mu);
            dm_pc_input_event(usage,0);
        }
        return;
    }
    if(pressed && !was_legacy) {
        int status=dm_uiscreen_ingame()?dm_pc_profile_file_status():0;
        if(status==1) {
            // The guest .key is authoritative in a run. An earlier menu/
            // missing-file WASD fallback must not coexist with its native
            // action (e.g. A moving left while opening Character).
            pthread_mutex_lock(&km_mu);
            int legacy_active=0;
            for(unsigned i=0;i<256;i++)legacy_active|=down[i]!=0;
            if(legacy_active) {
                memset(down,0,sizeof down);
                memset(active,0,sizeof active);
                dm_kb_clear();
            }
            native_seen=1;
            native_held[usage]=1;
            pthread_mutex_unlock(&km_mu);
            dm_pc_input_event(usage,1);
            return;
        }
        // Unsupported/corrupt .key must never turn a PC action into a
        // different gamepad action. Nor can a temporary missing file undo
        // native mode after this character's .key was already observed.
        if(status<0 || seen_native)return;
    }
    if(!pressed && !was_legacy)return;
    pthread_mutex_lock(&km_mu);load_locked();
    if((int)down[usage]==!!pressed) { pthread_mutex_unlock(&km_mu);return; }
    if(pressed) {
        unsigned a=map[usage];
        if(a==DM_KB_MAP) { dm_world_mouse_map_toggle(); a=DM_KB_UNBOUND; }
        if(!dm_uiscreen_ingame() || dm_uiscreen_panel()) {
            // In controller menus, arrows are the Switch D-pad so the
            // original focus/highlight navigation still works.
            if(usage==0x50)a=12; // Left
            if(usage==0x52)a=13; // Up
            if(usage==0x4f)a=14; // Right
            if(usage==0x51)a=15; // Down
        }
        // Choose Enter/Esc meaning when pressed; hold it until key release.
        if(a==DM_KB_ENTER)a=dm_uiscreen_ingame()&&!dm_uiscreen_panel()?10:0; // same page semantics as Mac
        if(a==DM_KB_ESCAPE)a=dm_uiscreen_ingame()&&!dm_uiscreen_panel()?10:1;
        active[usage]=(unsigned char)a;down[usage]=1;
    } else { down[usage]=0;active[usage]=DM_KB_UNBOUND; }
    uint64_t bits=0;int up=0,left=0,dn=0,right=0;
    for(unsigned i=0;i<256;i++)if(down[i]) {
        unsigned a=active[i];
        if(a<16)bits|=1ull<<a;
        else if(a==DM_KB_MOVE_UP)up=1;
        else if(a==DM_KB_MOVE_LEFT)left=1;
        else if(a==DM_KB_MOVE_DOWN)dn=1;
        else if(a==DM_KB_MOVE_RIGHT)right=1;
        else if(a>=DM_KB_SKILL1 && a<=DM_KB_SKILL8) {
            unsigned idx=a-DM_KB_SKILL1;
            static const uint64_t btn[6]={
                DM_BTN_A,DM_BTN_B,DM_BTN_Y,DM_BTN_X,DM_BTN_R,DM_BTN_ZR
            };
            if(idx>=6)bits|=DM_BTN_ZL;
            bits|=btn[idx%6];
        }
    }
    dm_kb_set(bits,(right-left)*32767,(up-dn)*32767,0,0);
    pthread_mutex_unlock(&km_mu);
}

// The original SettingsPanel action reports which child page is selected.
// Only offer the keyboard editor while KeyBindingMenu is the active child.
static _Atomic int controls_visible;
int dm_kbmap_controls_visible(void) {return atomic_load(&controls_visible);}
void dm_kbmap_controls_event(const char *cat,const char *action,const char *arg) {
    if(!cat || !action) return;
    if(!strcmp(cat,"SettingsPanelMessage") && !strcmp(action,"CheckChanges")) {
        atomic_store(&controls_visible,arg && !strcmp(arg,"KeyBindingMenu"));
    } else if(!strcmp(cat,"PanelManager") && arg &&
              !strcmp(arg,"KeyBindingMenu") &&
              (!strcmp(action,"OpenPanel") || !strcmp(action,"OpenExclusivePanel"))) {
        atomic_store(&controls_visible,1);
    } else if(!strcmp(cat,"PanelManager") &&
              ((!strcmp(action,"UnloadAll")) ||
               (( !strcmp(action,"ClosePanel") || !strcmp(action,"UnloadPanel")) &&
                arg && (!strcmp(arg,"SettingsPanel") || !strcmp(arg,"KeyBindingMenu"))))) {
        atomic_store(&controls_visible,0);
    }
}


int dm_kbmap_mouse_action(unsigned physical) {
    return physical<3?(int)physical:-1; // PC left/right/middle semantics
}
int dm_kbmap_bind_mouse(unsigned physical,unsigned action) {
    // Unverified left/right remapping is not exposed without a native slot.
    (void)physical;(void)action;return 0;
}
void dm_kbmap_pre_commit(const char *workdir) { (void)workdir; }
