// Standalone portable-keyboard behavioral contract; no game ROMFS needed.
#define _GNU_SOURCE
#include "dm_input.h"
#include "dm_kbmap.h"
#include "dm_pc_input.h"
#include "dm_pc_profile.h"
#include "dm_keyfile.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
static char root[1024],save_dir[1100],work_dir[1100];
static uint64_t buttons,guest_samples;
uint64_t dm_input_samples(void) { return guest_samples; }
static int32_t left_x,left_y;
static unsigned maps,pc_native_panels;
static int game_ingame=1;
void dm_fs_profile_lock(void) {}
void dm_fs_profile_unlock(void) {}
unsigned dm_fs_native_key_writers(void) {return 0;}
void dm_fs_save_poison(int e) {(void)e;}
const char *dm_save_commitdir(void) {return save_dir;}
const char *dm_save_workdir(void) {return work_dir;}
void dm_kb_clear(void) {buttons=0;left_x=left_y=0;}
void dm_kb_set(uint64_t b,int32_t lx,int32_t ly,int32_t rx,int32_t ry) {
    (void)rx;(void)ry;buttons=b;left_x=lx;left_y=ly;
}
void dm_world_mouse_map_toggle(void) {maps++;}
void dm_ui_pc_toggle_panel(unsigned id) {if(id==0)pc_native_panels++;}
int dm_uiscreen_ingame(void) {return game_ingame;}
int dm_uiscreen_panel(void) {return 0;}

static void game_seed(const char *name) {
    dm_keyfile f={0};assert(dm_keyfile_new_resurrected(&f)==DM_KEYFILE_OK);
    assert(dm_keyfile_set(&f,14,0,'X')==DM_KEYFILE_OK);
    char fn[1200];snprintf(fn,sizeof fn,"%s/%s/%s.key",work_dir,name,name);
    FILE *out=fopen(fn,"wb");assert(out);
    size_t n=0;const uint8_t *raw=dm_keyfile_data(&f,&n);
    assert(fwrite(raw,1,n,out)==n);assert(!fclose(out));
    dm_keyfile_free(&f);
}
int main(void) {
    strcpy(root,"/tmp/dmkbXXXXXX");assert(mkdtemp(root));
    snprintf(save_dir,sizeof save_dir,"%s/save",root);assert(!mkdir(save_dir,0700));
    snprintf(work_dir,sizeof work_dir,"%s/save.work",root);assert(!mkdir(work_dir,0700));
    char charsave[1200],charwork[1200],ctl[1200];
    snprintf(charsave,sizeof charsave,"%s/Alice",save_dir);assert(!mkdir(charsave,0700));
    snprintf(charwork,sizeof charwork,"%s/Alice",work_dir);assert(!mkdir(charwork,0700));
    snprintf(ctl,sizeof ctl,"%s/Alice/Alice.ctl",save_dir);
    FILE *pad=fopen(ctl,"wb");assert(pad);
    assert(fwrite("NATIVE_CTL",1,10,pad)==10);assert(!fclose(pad));
    dm_kbmap_select_character("Alice");
    assert(dm_kbmap_pc_mode()==1 && dm_kbmap_set_pc_mode(1));
    assert(!dm_kbmap_set_pc_mode(0));
    assert(dm_kbmap_action(0x52)==DM_KB_MOVE_UP);
    assert(dm_kbmap_action(0x04)==DM_KB_UNBOUND); // A never walks
    assert(dm_kbmap_action(0x1a)==DM_KB_UNBOUND); // W is native .key only
    assert(dm_kbmap_mouse_action(0)==0 && dm_kbmap_mouse_action(1)==1);
    assert(dm_kbmap_mouse_action(2)==2);
    assert(!dm_kbmap_bind(0x1b,0));
    assert(!dm_kbmap_bind_mouse(0,1));
    assert(!dm_kbmap_restore_defaults_checked());
    // Before the guest creates .key, keyboard still navigates as a pad.
    dm_kbmap_key_event(0x52,1);assert(left_y==32767);
    dm_kbmap_key_event(0x4f,1);assert(left_x==32767 && left_y==32767);
    dm_kbmap_key_event(0x4f,0);
    dm_kbmap_key_event(0x52,0);assert(left_y==0);
    dm_kbmap_key_event(0x04,1);assert(left_x==0 && left_y==0);
    dm_kbmap_key_event(0x04,0);
    // After guest .key exists, keyboard X resolves native Skill 1, then
    // uses the original SkillButton1 A binding (including its assigned skill).
    // A is the original Character action, not the legacy WASD left
    // movement. Even a previously held compatibility movement key must
    // stop before dispatching the native action.
    dm_kbmap_key_event(0x52,1);assert(left_y==32767);
    game_seed("Alice");
    dm_kbmap_key_event(0x04,1);
    assert(left_x==0 && left_y==0);
    dm_pc_input_guest_step();assert(pc_native_panels==1);
    dm_kbmap_key_event(0x04,0);
    dm_kbmap_key_event(0x52,0);
    // Native .key still keeps arrows reserved for walking. Pressing a
    // skill at the same time must not cancel the left-stick axis.
    dm_kbmap_key_event(0x52,1);assert(left_x==0 && left_y==32767);
    dm_kbmap_key_event(0x4f,1);assert(left_x==23170 && left_y==23170);
    dm_kbmap_key_event(0x1b,1);
    assert((buttons&DM_BTN_A) && left_x==23170 && left_y==23170);
    dm_kbmap_key_event(0x1b,0);assert(!buttons && left_x==23170);
    dm_kbmap_key_event(0x52,0);assert(left_x==32767 && left_y==0);
    dm_kbmap_key_event(0x4f,0);assert(left_x==0 && left_y==0);
    // Skill 5 is Switch R, and can be reassigned to a new physical key
    // by updating just the original character's Skill 5 .key command.
    assert(dm_pc_profile_bind(18,0,'K',NULL,NULL)==DM_KEYFILE_OK);
    dm_kbmap_key_event(0x3e,1);assert(buttons==0);
    dm_kbmap_key_event(0x3e,0);
    dm_kbmap_key_event(0x0e,1);assert(buttons==DM_BTN_R);
    dm_kbmap_key_event(0x0e,0);assert(buttons==0);
    dm_kbmap_key_event(0x1b,1);assert(buttons & DM_BTN_A);
    dm_kbmap_key_event(0x1b,0);assert(buttons==0);
    char worksidecar[1200],savedsidecar[1200];
    snprintf(worksidecar,sizeof worksidecar,"%s/Alice/Alice.kbm",work_dir);
    snprintf(savedsidecar,sizeof savedsidecar,"%s/Alice/Alice.kbm",save_dir);
    dm_kbmap_pre_commit(work_dir);
    assert(access(worksidecar,F_OK)!=0 && access(savedsidecar,F_OK)!=0);
    pad=fopen(ctl,"rb");assert(pad);
    char val[11]={0};assert(fread(val,1,10,pad)==10);
    assert(!memcmp(val,"NATIVE_CTL",10));assert(!fclose(pad));
    dm_kbmap_controls_event("SettingsPanelMessage","CheckChanges","KeyBindingMenu");
    assert(dm_kbmap_controls_visible());
    dm_kbmap_controls_event("PanelManager","ClosePanel","SettingsPanel");
    assert(!dm_kbmap_controls_visible());
    dm_kbmap_reset();assert(buttons==0);
    return 0;
}
