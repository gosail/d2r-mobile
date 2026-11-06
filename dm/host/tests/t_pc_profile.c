// Synthetic native .key profile tests. Never store a user's imported key file.
#define _POSIX_C_SOURCE 200809L
#include "dm_keyfile.h"
#include "dm_pc_profile.h"
#include "dm_pc_input.h"
#include "dm_input.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static char root[1024], save_dir[1100];
static uint64_t game_buttons;
static int map_toggles,panel_open;
static unsigned panel_events[57];
void dm_ui_pc_toggle_panel(unsigned id) {
    assert(id<57);
    panel_events[id]++;
}
void dm_kb_set(uint64_t b,int32_t lx,int32_t ly,int32_t rx,int32_t ry) {
    (void)lx;(void)ly;(void)rx;(void)ry;game_buttons=b;
}
void dm_world_mouse_map_toggle(void) {map_toggles++;}
int dm_uiscreen_ingame(void) {return 1;}
int dm_uiscreen_panel(void) {return panel_open;}
void dm_fs_profile_lock(void) {}
void dm_fs_profile_unlock(void) {}
const char *dm_save_commitdir(void) {return save_dir;}
static void dir(const char *name) {
    char a[1200];snprintf(a,sizeof a,"%s/%s",save_dir,name);
    assert(!mkdir(a,0700));
}
static void path(const char *name,char out[1200]) {
    snprintf(out,1200,"%s/%s/%s.key",save_dir,name,name);
}
static void bytes_at(const char *name,uint8_t *dst,size_t len) {
    FILE *in=fopen(name,"rb");assert(in);
    assert(fread(dst,1,len,in)==len);assert(fgetc(in)==EOF);assert(!fclose(in));
}
int main(void) {
    strcpy(root,"/tmp/dmpcXXXXXX");assert(mkdtemp(root));
    snprintf(save_dir,sizeof save_dir,"%s/save",root);assert(!mkdir(save_dir,0700));
    dir("Alice");dir("Bob");dir("Eve");
    assert(dm_pc_profile_select("LateMount")==DM_KEYFILE_OK);
    uint16_t late_vk=0;
    assert(dm_pc_profile_get(0,0,&late_vk)==DM_KEYFILE_NOT_FOUND);
    dir("LateMount"); // previously cached IO would remain invalid forever
    assert(dm_pc_profile_get(0,0,&late_vk)==DM_KEYFILE_OK && late_vk=='A');

    assert(dm_pc_vk_from_hid(0x04)=='A');
    assert(dm_pc_vk_from_hid(0x1e)=='1');
    assert(dm_pc_vk_from_hid(0x27)=='0');
    assert(dm_pc_vk_from_hid(0x3a)==0x70);
    assert(dm_pc_vk_from_hid(0x45)==0x7b);
    assert(dm_pc_vk_from_hid(0x59)==0x61);
    assert(dm_pc_vk_from_hid(0xe0)==0x11);
    assert(dm_pc_vk_from_hid(0xe5)==0x10);
    assert(dm_pc_vk_from_hid(0xe6)==0x12);
    assert(dm_pc_vk_from_hid(0x57)==0x6b);
    assert(dm_pc_vk_from_hid(0xffff)==-1);
    assert(dm_pc_profile_select("../evil")==DM_KEYFILE_INVALID);
    assert(dm_pc_profile_select("Alice")==DM_KEYFILE_OK);
    uint16_t vk=0;uint32_t command=999,conflict=999;unsigned slot=99,conflict_slot=99;
    assert(dm_pc_profile_get(0,0,&vk)==DM_KEYFILE_OK && vk=='A');
    assert(dm_pc_profile_get(14,0,&vk)==DM_KEYFILE_OK && vk=='1');
    assert(dm_pc_profile_get(23,0,&vk)==DM_KEYFILE_OK && vk==0xffff);
    assert(dm_pc_profile_get(49,0,&vk)==DM_KEYFILE_OK && vk==0xbb);
    assert(dm_pc_profile_reset_resurrected()==DM_KEYFILE_OK);
    assert(dm_pc_profile_action_for_hid(0x1e,&command,&slot)==DM_KEYFILE_OK);
    assert(command==14 && slot==0);

    char alice[1200];path("Alice",alice);
    uint8_t original[1142],changed[1142];bytes_at(alice,original,sizeof original);
    assert(original[0]==0x25 && original[1]==0);
    assert(dm_pc_profile_action_for_hid(0x04,&command,&slot)==DM_KEYFILE_OK);
    assert(command==0 && slot==0);
    assert(dm_pc_profile_bind(1,1,'A',&conflict,&conflict_slot)==DM_KEYFILE_CONFLICT);
    assert(conflict==0 && conflict_slot==0);
    assert(dm_pc_profile_bind(1,1,0xffff,NULL,NULL)==DM_KEYFILE_OK);
    assert(dm_pc_profile_bind(1,1,'K',NULL,NULL)==DM_KEYFILE_OK);
    assert(dm_pc_profile_get(1,1,&vk)==DM_KEYFILE_OK && vk=='K');
    bytes_at(alice,changed,sizeof changed);
    size_t diff=0;
    for(size_t i=0;i<sizeof original;i++)if(original[i]!=changed[i])diff++;
    assert(diff==1); // 'B' -> 'K': single low byte in secondary VK slot
    assert(dm_pc_profile_action_for_hid(0x0e,&command,&slot)==DM_KEYFILE_OK);
    assert(command==1 && slot==1);
    dm_pc_profile_clear();
    assert(dm_pc_profile_select("Alice")==DM_KEYFILE_OK);
    assert(dm_pc_profile_get(1,1,&vk)==DM_KEYFILE_OK && vk=='K');
    assert(dm_pc_profile_select("Bob")==DM_KEYFILE_OK);
    assert(dm_pc_profile_get(1,1,&vk)==DM_KEYFILE_OK && vk=='B');
    assert(dm_pc_profile_select("Alice")==DM_KEYFILE_OK);
    assert(dm_pc_profile_reset_legacy()==DM_KEYFILE_OK);
    assert(dm_pc_profile_get(14,0,&vk)==DM_KEYFILE_OK && vk==0x70);
    assert(dm_pc_profile_get(23,0,&vk)==DM_KEYFILE_OK && vk=='1');
    assert(dm_pc_profile_get(1,1,&vk)==DM_KEYFILE_OK && vk=='B');
    dm_pc_input_reset();
    assert(dm_pc_input_supported_action(7) && dm_pc_input_supported_action(14));
    assert(dm_pc_input_supported_action(49) && !dm_pc_input_supported_action(50));
    dm_pc_input_event(0x3a,1); // F1 uses native skill 1 -> Switch A
    assert(game_buttons==DM_BTN_A && dm_pc_input_latched_command(0x3a)==14);
    dm_pc_input_event(0x3a,1);assert(game_buttons==DM_BTN_A);
    dm_pc_input_event(0x3a,0);assert(game_buttons==0);
    dm_pc_input_event(0x40,1); // F7 is skill7 -> first alternate slot
    assert(game_buttons==(DM_BTN_ZL|DM_BTN_A));
    dm_pc_input_event(0x40,0);assert(game_buttons==0);
    dm_pc_input_event(0x1e,1); // potion column 1
    assert(game_buttons==DM_BTN_LEFT);
    dm_pc_input_event(0x1e,0);assert(game_buttons==0);
    dm_pc_input_event(0x2b,1); // Tab map: one toggle per keydown
    dm_pc_input_event(0x2b,1);assert(map_toggles==1);
    dm_pc_input_event(0x2b,0);assert(game_buttons==0);
    dm_pc_input_event(0x29,1);
    assert(game_buttons==DM_BTN_PLUS);
    panel_open=1;
    dm_pc_input_event(0x1e,1); // Escape must remain latched when panel appears.
    assert((game_buttons&DM_BTN_PLUS)!=0);
    dm_pc_input_event(0x1e,0);
    dm_pc_input_event(0x29,0);panel_open=0;assert(game_buttons==0);
    assert(dm_pc_profile_bind(14,0,'K',NULL,NULL)==DM_KEYFILE_OK);
    dm_pc_input_event(0x3a,1);assert(game_buttons==0); // old F1 no longer fires
    dm_pc_input_event(0x3a,0);
    dm_pc_input_event(0x0e,1);assert(game_buttons==DM_BTN_A);
    dm_pc_input_event(0x0e,0);assert(game_buttons==0);
    assert(dm_pc_profile_reset_legacy()==DM_KEYFILE_OK);
    assert(dm_pc_profile_bind(14,0,'K',NULL,NULL)==DM_KEYFILE_OK);
    dm_pc_input_event(0x0e,1);assert(game_buttons==DM_BTN_A);
    dm_pc_input_reset();assert(game_buttons==0);
    assert(dm_pc_input_latched_command(0x0e)==-1);
    assert(dm_pc_profile_reset_legacy()==DM_KEYFILE_OK);
    dm_pc_input_event(0x1a,1); // W: original guest weapon-swap action
    dm_pc_input_guest_step();assert(panel_events[44]==1);
    dm_pc_input_event(0x1a,0);
    dm_pc_input_event(0x0c,1); // I -> inventory cmd 1
    dm_pc_input_event(0x0c,1);
    dm_pc_input_guest_step();
    assert(panel_events[1]==1);
    dm_pc_input_event(0x0c,0);
    assert(dm_pc_profile_bind(1,0,'K',NULL,NULL)==DM_KEYFILE_OK);
    dm_pc_input_event(0x0c,1); // old physical key must be inert
    dm_pc_input_guest_step();assert(panel_events[1]==1);
    dm_pc_input_event(0x0c,0);
    dm_pc_input_event(0x0e,1); // new physical K must open native inventory panel
    dm_pc_input_guest_step();assert(panel_events[1]==2);
    dm_pc_input_event(0x0e,0);
    dm_pc_input_event(0x0e,1);
    dm_pc_input_reset(); // character change/focus loss removes queued UI work
    dm_pc_input_guest_step();assert(panel_events[1]==2);
    assert(dm_pc_profile_reset_legacy()==DM_KEYFILE_OK);
    char backup[1220];snprintf(backup,sizeof backup,"%s.bak",alice);
    assert(access(backup,F_OK)==0);
    // Existing unknown format is read but never overwritten or converted.
    char eve[1200];path("Eve",eve);
    { FILE *f=fopen(eve,"wb");assert(f);
      assert(fputs("D2RMobileKeyboard=1\n",f)>=0);assert(!fclose(f)); }
    assert(dm_pc_profile_select("Eve")==DM_KEYFILE_OK);
    assert(dm_pc_profile_get(0,0,&vk)==DM_KEYFILE_UNSUPPORTED);
    assert(dm_pc_profile_bind(0,0,'K',NULL,NULL)==DM_KEYFILE_UNSUPPORTED);
    {FILE *f=fopen(eve,"rb");assert(f);char line[80];
     assert(fgets(line,sizeof line,f));assert(!strcmp(line,"D2RMobileKeyboard=1\n"));
     assert(!fclose(f));}
    dm_pc_profile_clear();
    char bob[1200];path("Bob",bob);
    unlink(alice);unlink(backup);unlink(bob);unlink(eve);
    char late[1200];path("LateMount",late);
    assert(!unlink(late));

    char d[1200];snprintf(d,sizeof d,"%s/Alice",save_dir);assert(!rmdir(d));
    snprintf(d,sizeof d,"%s/Bob",save_dir);assert(!rmdir(d));
    snprintf(d,sizeof d,"%s/Eve",save_dir);assert(!rmdir(d));
    snprintf(d,sizeof d,"%s/LateMount",save_dir);assert(!rmdir(d));
    assert(!rmdir(save_dir));assert(!rmdir(root));
    puts("native character keyboard profiles: PASS");
    return 0;
}
