// Standalone portable-keyboard behavioral contract; no game ROMFS needed.
#define _GNU_SOURCE
#include "dm_input.h"
#include "dm_kbmap.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
static char root[1024],save_dir[1100];
static uint64_t buttons;
static int32_t left_x,left_y;
static unsigned maps;
const char *dm_save_commitdir(void) {return save_dir;}
void dm_kb_clear(void) {buttons=0;left_x=left_y=0;}
void dm_kb_set(uint64_t b,int32_t lx,int32_t ly,int32_t rx,int32_t ry) {
    (void)rx;(void)ry;buttons=b;left_x=lx;left_y=ly;
}
void dm_world_mouse_map_toggle(void) {maps++;}
int dm_uiscreen_ingame(void) {return 1;}
int dm_uiscreen_panel(void) {return 0;}
int main(int argc,char **argv) {
    if(argc==2) {
        snprintf(save_dir,sizeof save_dir,"%s/save",argv[1]);
        assert(dm_kbmap_action(0x0d)==15);
        assert(dm_kbmap_action(0x0e)==1);
        assert(dm_kbmap_mouse_action(0)==1);
        return 0;
    }
    strcpy(root,"/tmp/dmkbXXXXXX");assert(mkdtemp(root));
    snprintf(save_dir,sizeof save_dir,"%s/save",root);assert(!mkdir(save_dir,0700));
    assert(dm_kbmap_action(0x1a)==DM_KB_MOVE_UP);
    assert(dm_kbmap_action(0x0d)==0);
    dm_kbmap_key_event(0x1a,1);assert(left_y==32767);
    dm_kbmap_key_event(0x07,1);assert(left_x==32767 && left_y==32767);
    dm_kbmap_key_event(0x1a,0);assert(left_x==32767 && left_y==0);
    dm_kbmap_key_event(0x07,0);assert(left_x==0 && left_y==0);
    dm_kbmap_key_event(0x0d,1);assert(buttons & DM_BTN_A);
    dm_kbmap_key_event(0x0e,1);assert((buttons & (DM_BTN_A|DM_BTN_B))==(DM_BTN_A|DM_BTN_B));
    dm_kbmap_key_event(0x0d,0);assert((buttons & DM_BTN_B) && !(buttons & DM_BTN_A));
    dm_kbmap_key_event(0x0e,0);assert(buttons==0);
    dm_kbmap_key_event(0x3e,1);assert((buttons & (DM_BTN_ZL|DM_BTN_B))==(DM_BTN_ZL|DM_BTN_B));
    dm_kbmap_reset();assert(buttons==0);
    dm_kbmap_key_event(0x1e,1);assert(buttons & DM_BTN_LEFT);
    dm_kbmap_key_event(0x1e,0);
    dm_kbmap_key_event(0x10,1);assert(maps==1);
    dm_kbmap_key_event(0x10,1);assert(maps==1);
    dm_kbmap_key_event(0x10,0);
    assert(dm_kbmap_bind(0x0d,15));
    assert(dm_kbmap_bind_mouse(0,1));
    assert(dm_kbmap_mouse_action(0)==1);
    assert(!dm_kbmap_bind_mouse(0,3));
    assert(dm_kbmap_action(0x0d)==15);
    assert(!dm_kbmap_bind(256,1));
    assert(!dm_kbmap_bind(2,99));
    char fn[1200];snprintf(fn,sizeof fn,"%s/sw_keyboard.txt",root);
    FILE *f=fopen(fn,"r");assert(f);char line[128];assert(fgets(line,sizeof line,f));
    assert(!strcmp(line,"D2RMobileKeyboard=1\n"));fclose(f);
    pid_t pid=fork();assert(pid>=0);
    if(!pid){execl(argv[0],argv[0],root,(char*)0);_exit(127);}
    int status;assert(waitpid(pid,&status,0)==pid);assert(WIFEXITED(status)&&WEXITSTATUS(status)==0);
    dm_kbmap_controls_event("SettingsPanelMessage","CheckChanges","KeyBindingMenu");
    assert(dm_kbmap_controls_visible());
    dm_kbmap_controls_event("SettingsPanelMessage","CheckChanges","VideoOptionsPanel");
    assert(!dm_kbmap_controls_visible());
    dm_kbmap_controls_event("SettingsPanelMessage","CheckChanges","KeyBindingMenu");
    dm_kbmap_controls_event("PanelManager","ClosePanel","SettingsPanel");
    assert(!dm_kbmap_controls_visible());
    dm_kbmap_restore_defaults();assert(dm_kbmap_action(0x0d)==0);
    assert(dm_kbmap_mouse_action(0)==0);
    remove(fn);rmdir(save_dir);rmdir(root);
    puts("portable keyboard bindings: PASS");
    return 0;
}
