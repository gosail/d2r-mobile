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
static char root[1024], save_dir[1100],work_dir[1100];
static uint64_t game_buttons;
static int32_t game_right_y;
static int map_toggles,panel_open,poisoned;static unsigned native_writers;
static unsigned panel_events[57];
void dm_ui_pc_toggle_panel(unsigned id) {
    assert(id<57);
    panel_events[id]++;
}
void dm_kb_set(uint64_t b,int32_t lx,int32_t ly,int32_t rx,int32_t ry) {
    (void)lx;(void)ly;(void)rx;game_buttons=b;game_right_y=ry;
}
void dm_world_mouse_map_toggle(void) {map_toggles++;}
int dm_uiscreen_ingame(void) {return 1;}
int dm_uiscreen_panel(void) {return panel_open;}
void dm_fs_profile_lock(void) {}
void dm_fs_profile_unlock(void) {}
unsigned dm_fs_native_key_writers(void) {return native_writers;}
void dm_fs_save_poison(int e) {poisoned=e;}
const char *dm_save_commitdir(void) {return save_dir;}
const char *dm_save_workdir(void) {return work_dir;}
static void dir(const char *name) {
    char a[1200];snprintf(a,sizeof a,"%s/%s",save_dir,name);
    assert(!mkdir(a,0700));
    snprintf(a,sizeof a,"%s/%s",work_dir,name);
    assert(!mkdir(a,0700));
}
static void path(const char *name,char out[1200]) {
    snprintf(out,1200,"%s/%s/%s.key",work_dir,name,name);
}
// Simulated guest-generated file in the active save.work directory.
static void game_seed(const char *name,int resurrected) {
    dm_keyfile tmp={0};
    int rc=resurrected?dm_keyfile_new_resurrected(&tmp):
        dm_keyfile_new_legacy(&tmp);
    assert(rc==DM_KEYFILE_OK);
    char fn[1200];path(name,fn);
    size_t n=0;const uint8_t *bytes=dm_keyfile_data(&tmp,&n);
    FILE *out=fopen(fn,"wb");assert(out);
    assert(fwrite(bytes,1,n,out)==n);assert(!fclose(out));
    dm_keyfile_free(&tmp);
}
static void bytes_at(const char *name,uint8_t *dst,size_t len) {
    FILE *in=fopen(name,"rb");assert(in);
    assert(fread(dst,1,len,in)==len);assert(fgetc(in)==EOF);assert(!fclose(in));
}
int main(void) {
    strcpy(root,"/tmp/dmpcXXXXXX");assert(mkdtemp(root));
    snprintf(save_dir,sizeof save_dir,"%s/save",root);assert(!mkdir(save_dir,0700));
    snprintf(work_dir,sizeof work_dir,"%s/save.work",root);assert(!mkdir(work_dir,0700));
    dir("Alice");dir("Bob");dir("Eve");
    assert(dm_pc_profile_select("LateMount")==DM_KEYFILE_OK);
    uint16_t late_vk=0;
    assert(dm_pc_profile_get(0,0,&late_vk)==DM_KEYFILE_NOT_FOUND);
    dir("LateMount");
    assert(dm_pc_profile_get(0,0,&late_vk)==DM_KEYFILE_NOT_FOUND);
    char bad_native_path[1200];path("LateMount",bad_native_path);
    assert(!mkdir(bad_native_path,0700)); // exists, but is not a regular file
    assert(dm_pc_profile_get(0,0,&late_vk)==DM_KEYFILE_IO);
    assert(dm_pc_profile_file_status()==-1); // cannot silently use legacy
    assert(!rmdir(bad_native_path));
    assert(dm_pc_profile_file_status()==0);
    // Only the guest can generate the original native profile.
    game_seed("LateMount",1);
    assert(dm_pc_profile_get(0,0,&late_vk)==DM_KEYFILE_OK && late_vk=='A');
    char committed_late[1200];
    snprintf(committed_late,sizeof committed_late,"%s/LateMount/LateMount.key",save_dir);
    assert(access(committed_late,F_OK)!=0); // uncommitted game file stays in worktree
    game_seed("Alice",1);
    game_seed("Bob",0);
    // The codec supports guest files with different paired-record counts.
    // A missing numeric ID must not make all other commands uneditable.
    dir("Trim");
    dm_keyfile original_trim={0};
    assert(dm_keyfile_new_legacy(&original_trim)==DM_KEYFILE_OK);
    size_t original_len=0;
    const uint8_t *full=dm_keyfile_data(&original_trim,&original_len);
    assert(original_len>=42);
    char trim_path[1200];path("Trim",trim_path);
    FILE *trim_out=fopen(trim_path,"wb");assert(trim_out);
    assert(fwrite(full,1,2,trim_out)==2);
    assert(fwrite(full+22,1,20,trim_out)==20); // action ID 1 only
    assert(!fclose(trim_out));dm_keyfile_free(&original_trim);
    assert(dm_pc_profile_select("Trim")==DM_KEYFILE_OK);
    assert(dm_pc_profile_file_status()==1);
    uint16_t trimmed_vk=0;
    assert(dm_pc_profile_get(0,0,&trimmed_vk)==DM_KEYFILE_NOT_FOUND);
    assert(dm_pc_profile_get(1,0,&trimmed_vk)==DM_KEYFILE_OK &&
           trimmed_vk=='I');
    assert(dm_pc_profile_bind(1,0,'Z',NULL,NULL)==DM_KEYFILE_OK);
    assert(dm_pc_profile_get(1,0,&trimmed_vk)==DM_KEYFILE_OK &&
           trimmed_vk=='Z');

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
    // The guest may rewrite its file in several pwrite calls while the
    // original writer handle remains open. Do not expose partial bytes.
    native_writers=1;
    { char partial[1200];path("Alice",partial);
      FILE *guest=fopen(partial,"wb");assert(guest);
      assert(fwrite("partial",1,7,guest)==7);assert(!fclose(guest)); }
    assert(dm_pc_profile_get(0,0,&vk)==DM_KEYFILE_OK && vk=='A');
    assert(dm_pc_profile_file_status()==1);
    native_writers=0;
    assert(dm_pc_profile_get(0,0,&vk)==DM_KEYFILE_UNSUPPORTED);
    game_seed("Alice",1);
    assert(dm_pc_profile_get(0,0,&vk)==DM_KEYFILE_OK && vk=='A');
    assert(dm_pc_profile_get(14,0,&vk)==DM_KEYFILE_OK && vk=='1');
    assert(dm_pc_profile_get(23,0,&vk)==DM_KEYFILE_OK && vk==0xffff);
    assert(dm_pc_profile_get(49,0,&vk)==DM_KEYFILE_OK && vk==0xbb);
    game_seed("Alice",1);
    assert(dm_pc_profile_action_for_hid(0x1e,&command,&slot)==DM_KEYFILE_OK);
    assert(command==14 && slot==0);

    char alice[1200];path("Alice",alice);
    char alice_commit[1200];snprintf(alice_commit,sizeof alice_commit,
        "%s/Alice/Alice.key",save_dir);
    assert(access(alice_commit,F_OK)!=0);
    uint8_t original[1142],changed[1142];bytes_at(alice,original,sizeof original);
    assert(original[0]==0x25 && original[1]==0);
    assert(dm_pc_profile_action_for_hid(0x04,&command,&slot)==DM_KEYFILE_OK);
    assert(command==0 && slot==0);
    native_writers=1; // the original guest is rewriting its .key
    assert(dm_pc_profile_bind(1,1,'K',NULL,NULL)==DM_KEYFILE_CONFLICT);
    assert(dm_pc_profile_get(1,1,&vk)==DM_KEYFILE_OK && vk=='B');
    native_writers=0;
    assert(poisoned==0);
    assert(dm_pc_profile_bind(1,1,'A',&conflict,&conflict_slot)==DM_KEYFILE_CONFLICT);
    assert(conflict==0 && conflict_slot==0);
    struct stat native_before,native_after;
    assert(stat(alice,&native_before)==0);
    assert(dm_pc_profile_bind(1,1,0xffff,NULL,NULL)==DM_KEYFILE_OK);
    assert(dm_pc_profile_bind(1,1,'K',NULL,NULL)==DM_KEYFILE_OK);
    assert(stat(alice,&native_after)==0);
    assert(native_before.st_ino==native_after.st_ino);
    assert((native_before.st_mode&0777)==(native_after.st_mode&0777));
    assert(dm_pc_profile_get(1,1,&vk)==DM_KEYFILE_OK && vk=='K');
    assert(access(alice_commit,F_OK)!=0); // adapter writes only save.work
    bytes_at(alice,changed,sizeof changed);
    size_t diff=0;
    for(size_t i=0;i<sizeof original;i++)if(original[i]!=changed[i])diff++;
    assert(diff==1); // 'B' -> 'K': single low byte in secondary VK slot
    assert(dm_pc_profile_action_for_hid(0x0e,&command,&slot)==DM_KEYFILE_OK);
    assert(command==1 && slot==1);
    // The guest may rewrite its original .key without notifying our bridge.
    // Lookups must observe those changes on the next call (no stale cache).
    { FILE *guest=fopen(alice,"wb");assert(guest);
      assert(fwrite(original,1,sizeof original,guest)==sizeof original);
      assert(!fclose(guest)); }
    assert(dm_pc_profile_get(1,1,&vk)==DM_KEYFILE_OK && vk=='B');
    assert(dm_pc_profile_action_for_hid(0x0e,&command,&slot)==DM_KEYFILE_NOT_FOUND);
    assert(dm_pc_profile_bind(1,1,'K',NULL,NULL)==DM_KEYFILE_OK);
    { FILE *in=fopen(alice,"rb"),*out=fopen(alice_commit,"wb");
      assert(in && out);uint8_t buf[2048];size_t n;
      while((n=fread(buf,1,sizeof buf,in)))assert(fwrite(buf,1,n,out)==n);
      assert(!ferror(in));assert(!fclose(in));assert(!fclose(out)); }
    // After Commit, the original game and adapter see the same binary.
    bytes_at(alice_commit,changed,sizeof changed);
    assert(changed[2+(1*2+1)*10+4]=='K');
    dm_pc_profile_clear();
    assert(dm_pc_profile_select("Alice")==DM_KEYFILE_OK);
    assert(dm_pc_profile_get(1,1,&vk)==DM_KEYFILE_OK && vk=='K');
    assert(dm_pc_profile_select("Bob")==DM_KEYFILE_OK);
    assert(dm_pc_profile_get(1,1,&vk)==DM_KEYFILE_OK && vk=='B');
    assert(dm_pc_profile_select("Alice")==DM_KEYFILE_OK);
    game_seed("Alice",0);
    assert(dm_pc_profile_get(14,0,&vk)==DM_KEYFILE_OK && vk==0x70);
    assert(dm_pc_profile_get(23,0,&vk)==DM_KEYFILE_OK && vk=='1');
    assert(dm_pc_profile_get(1,1,&vk)==DM_KEYFILE_OK && vk=='B');
    dm_pc_input_reset();
    assert(dm_pc_input_wheel(1)==1 && dm_pc_input_wheel(-1)==-1);
    assert(dm_pc_input_wheel(0)==0);
    assert(dm_pc_profile_bind(40,0,0xffff,NULL,NULL)==DM_KEYFILE_OK);
    assert(dm_pc_profile_bind(39,0,0x104,NULL,NULL)==DM_KEYFILE_OK);
    assert(dm_pc_input_wheel(-1)==1); // wheel-down remapped to previous
    assert(dm_pc_input_wheel(1)==0); // original wheel-up now unbound
    game_seed("Alice",0);
    assert(dm_pc_profile_bind(39,0,'K',NULL,NULL)==DM_KEYFILE_OK);
    dm_pc_input_event(0x0e,1);
    assert(game_right_y==32767);
    dm_pc_input_event(0x0e,0);
    assert(game_right_y==0);
    game_seed("Alice",0);
    assert(dm_pc_input_supported_action(7) && dm_pc_input_supported_action(14));
    // In the original native command table ID 5 is Chat, not UI Enter.
    // Unsupported chat must not simulate a controller A click in any scene.
    assert(!dm_pc_input_supported_action(5));
    panel_open=1;
    dm_pc_input_reset();
    dm_pc_input_event(0x28,1);
    assert(game_buttons==0);
    assert(dm_pc_input_latched_command(0x28)==-1);
    dm_pc_input_event(0x28,0);
    panel_open=0;
    dm_pc_input_event(0x28,1);
    assert(game_buttons==0);
    dm_pc_input_event(0x28,0);
    assert(!dm_pc_input_supported_action(49) && !dm_pc_input_supported_action(50));
    dm_pc_input_event(0x3a,1); // F1 uses native skill 1 -> Switch A
    assert(game_buttons==DM_BTN_B && dm_pc_input_latched_command(0x3a)==14);
    dm_pc_input_event(0x3a,1);assert(game_buttons==DM_BTN_B);
    dm_pc_input_event(0x3a,0);assert(game_buttons==0);
    dm_pc_input_event(0x40,1); // F7 is skill7 -> first alternate slot
    assert(game_buttons==(DM_BTN_ZL|DM_BTN_Y));
    dm_pc_input_event(0x40,0);assert(game_buttons==0);
    dm_pc_input_event(0x1e,1); // potion column 1
    assert(game_buttons==DM_BTN_LEFT);
    dm_pc_input_event(0x1e,0);assert(game_buttons==0);
    int map_before=map_toggles;
    assert(dm_pc_input_mouse_special(0x100)==1);
    assert(map_toggles==map_before+1);
    assert(dm_pc_profile_bind(7,1,0xffff,NULL,NULL)==DM_KEYFILE_OK);
    assert(dm_pc_input_mouse_special(0x100)==0);
    assert(map_toggles==map_before+1);
    game_seed("Alice",0);
    dm_pc_input_event(0x2b,1); // Tab map: one toggle per keydown
    dm_pc_input_event(0x2b,1);assert(map_toggles==map_before+2);
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
    dm_pc_input_event(0x0e,1);assert(game_buttons==DM_BTN_B);
    dm_pc_input_event(0x0e,0);assert(game_buttons==0);
    game_seed("Alice",0);
    assert(dm_pc_profile_bind(14,0,'K',NULL,NULL)==DM_KEYFILE_OK);
    dm_pc_input_event(0x0e,1);assert(game_buttons==DM_BTN_B);
    dm_pc_input_reset();assert(game_buttons==0);
    assert(dm_pc_input_latched_command(0x0e)==-1);
    game_seed("Alice",0);
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
    game_seed("Alice",0);
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

    unlink(alice_commit);
    char d[1200];
    const char *names[]={"Alice","Bob","Eve","LateMount"};
    for(unsigned i=0;i<4;i++) {
        snprintf(d,sizeof d,"%s/%s",work_dir,names[i]);assert(!rmdir(d));
        snprintf(d,sizeof d,"%s/%s",save_dir,names[i]);assert(!rmdir(d));
    }
    assert(!rmdir(work_dir));assert(!rmdir(save_dir));assert(!rmdir(root));
    puts("native character keyboard profiles: PASS");
    return 0;
}
