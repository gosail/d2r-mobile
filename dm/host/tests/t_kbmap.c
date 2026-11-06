// Standalone portable-keyboard behavioral contract; no game ROMFS needed.
#define _GNU_SOURCE
#include "dm_input.h"
#include "dm_kbmap.h"
#include "dm_pc_input.h"
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
static uint64_t buttons;
static int32_t left_x,left_y;
static unsigned maps,pc_native_panels;
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
int dm_uiscreen_ingame(void) {return 1;}
int dm_uiscreen_panel(void) {return 0;}
int main(int argc,char **argv) {
    if(argc==2) {
        snprintf(save_dir,sizeof save_dir,"%s/save",argv[1]);
        snprintf(work_dir,sizeof work_dir,"%s/save.work",argv[1]);
        dm_kbmap_select_character("Alice");
        assert(dm_kbmap_action(0x0d)==15);
        assert(dm_kbmap_action(0x0e)==1);
        assert(dm_kbmap_mouse_action(0)==1);
        return 0;
    }
    strcpy(root,"/tmp/dmkbXXXXXX");assert(mkdtemp(root));
    snprintf(save_dir,sizeof save_dir,"%s/save",root);assert(!mkdir(save_dir,0700));
    snprintf(work_dir,sizeof work_dir,"%s/save.work",root);assert(!mkdir(work_dir,0700));
    char alice[1200],alice_work[1200],bob[1200],bob_work[1200];
    snprintf(alice,sizeof alice,"%s/Alice",save_dir);assert(!mkdir(alice,0700));
    snprintf(alice_work,sizeof alice_work,"%s/Alice",work_dir);assert(!mkdir(alice_work,0700));
    snprintf(bob,sizeof bob,"%s/Bob",save_dir);assert(!mkdir(bob,0700));
    snprintf(bob_work,sizeof bob_work,"%s/Bob",work_dir);assert(!mkdir(bob_work,0700));
    // A native PC file must never be replaced with mobile HID text.
    char native_key[1200],native_work[1200];
    unsigned char native_bytes[]={0x25,0,0,0,0,0,65,0,1,0,0,0,
                                  0,0,0,0,67,0,0,0,0,0};
    snprintf(native_key,sizeof native_key,"%s/Alice/Alice.key",save_dir);
    snprintf(native_work,sizeof native_work,"%s/Alice/Alice.key",work_dir);
    { FILE *nf=fopen(native_key,"wb");assert(nf);
      assert(fwrite(native_bytes,1,sizeof native_bytes,nf)==sizeof native_bytes);
      assert(!fclose(nf)); }
    dm_kbmap_select_character("Alice");
    assert(dm_kbmap_pc_mode()==1); // default is original game PC mode
    // The guest has not created its .key yet: all three platform bridges
    // keep physical keyboard navigation alive through legacy controller HID.
    dm_kbmap_key_event(0x1a,1);assert(left_y==32767);
    dm_kbmap_key_event(0x1a,0);assert(left_y==0);
    // The original game creates its binary .key inside save.work.
    { FILE *nf=fopen(native_work,"wb");assert(nf);
      assert(fwrite(native_bytes,1,sizeof native_bytes,nf)==sizeof native_bytes);
      assert(!fclose(nf)); }
    dm_kbmap_key_event(0x04,1);
    dm_pc_input_guest_step();
    assert(pc_native_panels==1); // HID A now means native command 0
    dm_kbmap_key_event(0x04,0);
    assert(!unlink(native_work));
    assert(dm_kbmap_set_pc_mode(0));
    assert(dm_kbmap_pc_mode()==0);
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
    dm_kbmap_key_event(0x28,1);assert(buttons & DM_BTN_PLUS);
    dm_kbmap_key_event(0x28,0);assert(buttons==0);
    dm_kbmap_key_event(0x29,1);assert(buttons & DM_BTN_PLUS);
    dm_kbmap_key_event(0x29,0);assert(buttons==0);
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
    char fn[1200],fw[1200],ctl[1200];
    snprintf(fn,sizeof fn,"%s/Alice/Alice.kbm",save_dir);
    snprintf(fw,sizeof fw,"%s/Alice/Alice.kbm",work_dir);
    snprintf(ctl,sizeof ctl,"%s/Alice/Alice.ctl",save_dir);
    { FILE *c=fopen(ctl,"wb");assert(c);assert(fwrite("NATIVE_CTL",1,10,c)==10);fclose(c); }
    // All live edits stay inside the guest work tree until Commit.
    assert(access(fn,F_OK)!=0);
    FILE *f=fopen(fw,"rb");assert(f);char line[128];
    assert(fgets(line,sizeof line,f));assert(!strcmp(line,"D2RMobileKeyboard=1\n"));
    assert(fclose(f)==0);
    // The pre-commit hook never rewrites a native .key or private .kbm.
    dm_kbmap_pre_commit(work_dir);
    f=fopen(fw,"rb");assert(f);size_t bytes=fread(line,1,sizeof(line)-1,f);line[bytes]=0;fclose(f);
    assert(strstr(line,"0d=15"));
    // Simulate the original h_fs_Commit promotion (work -> committed).
    { FILE *in=fopen(fw,"rb"),*out=fopen(fn,"wb");assert(in && out);
      unsigned char chunk[512];size_t n;
      while((n=fread(chunk,1,sizeof chunk,in)))assert(fwrite(chunk,1,n,out)==n);
      assert(!ferror(in));assert(!fclose(in));assert(!fclose(out)); }

    f=fopen(ctl,"rb");assert(f);assert(fread(line,1,10,f)==10);fclose(f);assert(!memcmp(line,"NATIVE_CTL",10));
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
    // Switch characters: no global bindings leak into another hero.
    char bobkey[1200];snprintf(bobkey,sizeof bobkey,"%s/Bob/Bob.kbm",save_dir);
    { FILE *f2=fopen(bobkey,"wb");assert(f2);fputs("LEGACY_BINARY\n",f2);fclose(f2); }
    char bobworkkbm[1200];
    snprintf(bobworkkbm,sizeof bobworkkbm,"%s/Bob/Bob.kbm",work_dir);
    // The guest mounts save.work as a clone of committed save; simulate
    // the foreign-format file already present in that active snapshot.
    { FILE *f2=fopen(bobworkkbm,"wb");assert(f2);
      assert(fputs("LEGACY_BINARY\n",f2)>=0);assert(!fclose(f2)); }
    dm_kbmap_select_character("Bob");assert(dm_kbmap_action(0x0d)==0);
    assert(!dm_kbmap_restore_defaults_checked()); // cannot erase another format's .key
    assert(!dm_kbmap_bind(0x0d,15));
    { FILE *f2=fopen(bobkey,"rb");assert(f2);assert(fgets(line,sizeof line,f2));fclose(f2);assert(!strcmp(line,"LEGACY_BINARY\n")); }
    dm_kbmap_select_character("../bad");assert(dm_kbmap_action(0x0d)==0);
    // A foreign native .key appearing in the work tree may not be
    // overwritten by the pre-commit sync.
    { FILE *w=fopen(fw,"wb");assert(w);fputs("FOREIGN_WORK\n",w);fclose(w); }
    dm_kbmap_select_character("Alice");assert(dm_kbmap_action(0x0d)==0);
    assert(!dm_kbmap_bind(0x0d,15)); // foreign worktree cannot be overwritten
    dm_kbmap_pre_commit(work_dir);
    { FILE *w=fopen(fw,"rb");assert(w);assert(fgets(line,sizeof line,w));fclose(w);assert(!strcmp(line,"FOREIGN_WORK\n")); }
    { FILE *nf=fopen(native_key,"rb");assert(nf);
      unsigned char got[sizeof native_bytes];
      assert(fread(got,1,sizeof got,nf)==sizeof got);assert(!fclose(nf));
      assert(!memcmp(got,native_bytes,sizeof got)); }
    // The original game owns native .key creation; the host never
    // fabricates a worktree copy from an older committed snapshot.
    assert(access(native_work,F_OK)!=0);
    // Worktree is authoritative if the guest changed its own .key.
    { FILE *nf=fopen(native_work,"wb");assert(nf);
      assert(fputs("GUEST_CHANGED",nf)>=0);assert(!fclose(nf)); }
    dm_kbmap_pre_commit(work_dir);
    { FILE *nf=fopen(native_work,"rb");assert(nf);
      assert(fgets(line,sizeof line,nf));assert(!fclose(nf));
      assert(!strcmp(line,"GUEST_CHANGED")); }

    remove(fw);
    // A rejected guest transaction restores save.work from the last
    // committed snapshot; simulate that rollback before selecting Alice again.
    { FILE *in=fopen(fn,"rb"),*out=fopen(fw,"wb");assert(in && out);
      unsigned char data[512];size_t n;
      while((n=fread(data,1,sizeof data,in)))assert(fwrite(data,1,n,out)==n);
      assert(!ferror(in));assert(!fclose(in));assert(!fclose(out)); }
    // Old global sw_keyboard.txt is only a read-only migration source.
    char oldglob[1200],charlie[1200],charliew[1200],charliekey[1200];
    snprintf(oldglob,sizeof oldglob,"%s/sw_keyboard.txt",root);
    { FILE *g=fopen(oldglob,"wb");assert(g);fputs("D2RMobileKeyboard=1\n0d=15\n",g);fclose(g); }
    snprintf(charlie,sizeof charlie,"%s/Charlie",save_dir);assert(!mkdir(charlie,0700));
    snprintf(charliew,sizeof charliew,"%s/Charlie",work_dir);assert(!mkdir(charliew,0700));
    snprintf(charliekey,sizeof charliekey,"%s/Charlie/Charlie.kbm",save_dir);
    dm_kbmap_select_character("Charlie");
    assert(dm_kbmap_action(0x0d)==15);
    assert(dm_kbmap_bind(0x0e,7));
    { FILE *g=fopen(oldglob,"rb");assert(g);assert(fgets(line,sizeof line,g));fclose(g);assert(!strcmp(line,"D2RMobileKeyboard=1\n")); }
    assert(access(charliekey,F_OK)!=0);
    { char cw[1200];snprintf(cw,sizeof cw,"%s/Charlie/Charlie.kbm",work_dir);
      assert(access(cw,F_OK)==0); }
    // Old private per-character .key is read-only migration input.
    char dave[1200],davew[1200],davekey[1200],davekbm[1200];
    snprintf(dave,sizeof dave,"%s/Dave",save_dir);assert(!mkdir(dave,0700));
    snprintf(davew,sizeof davew,"%s/Dave",work_dir);assert(!mkdir(davew,0700));
    snprintf(davekey,sizeof davekey,"%s/Dave/Dave.key",save_dir);
    snprintf(davekbm,sizeof davekbm,"%s/Dave/Dave.kbm",save_dir);
    { FILE *g=fopen(davekey,"wb");assert(g);
      assert(fputs("D2RMobileKeyboard=1\n0d=15\n",g)>=0);assert(!fclose(g)); }
    char daveworkkey[1200];snprintf(daveworkkey,sizeof daveworkkey,"%s/Dave/Dave.key",work_dir);
    { FILE *g=fopen(daveworkkey,"wb");assert(g);
      assert(fputs("D2RMobileKeyboard=1\n0d=15\n",g)>=0);assert(!fclose(g)); }
    dm_kbmap_select_character("Dave");
    assert(dm_kbmap_action(0x0d)==15);
    assert(dm_kbmap_bind(0x0e,7));
    { FILE *g=fopen(davekey,"rb");assert(g);
      assert(fgets(line,sizeof line,g));assert(!strcmp(line,"D2RMobileKeyboard=1\n"));
      assert(fgets(line,sizeof line,g));assert(!strcmp(line,"0d=15\n"));assert(!fclose(g)); }
    assert(access(davekbm,F_OK)!=0);
    { char dw[1200];snprintf(dw,sizeof dw,"%s/Dave/Dave.kbm",work_dir);
      assert(access(dw,F_OK)==0); }
    dm_kbmap_select_character("Alice");assert(dm_kbmap_action(0x0d)==15);
    assert(dm_kbmap_pc_mode()==0);
    assert(dm_kbmap_set_pc_mode(1));
    assert(dm_kbmap_pc_mode()==1);
    dm_kbmap_select_character("Bob");
    dm_kbmap_select_character("Alice");
    assert(dm_kbmap_pc_mode()==1); // mode persists in .kbm, distinct from native .key
    assert(dm_kbmap_set_pc_mode(0));
    dm_kbmap_restore_defaults();assert(dm_kbmap_action(0x0d)==0);
    assert(dm_kbmap_mouse_action(0)==0);
    char cw[1200];snprintf(cw,sizeof cw,"%s/Charlie/Charlie.kbm",work_dir);
    remove(fn);remove(fw);remove(ctl);remove(bobkey);remove(bobworkkbm);
    remove(native_key);remove(native_work);remove(davekey);remove(daveworkkey);remove(davekbm);
    remove(charliekey);remove(cw);remove(oldglob);
    { char dw[1200];snprintf(dw,sizeof dw,"%s/Dave/Dave.kbm",work_dir);
      remove(dw); }
    rmdir(alice);rmdir(alice_work);rmdir(bob);rmdir(bob_work);rmdir(charlie);rmdir(charliew);rmdir(dave);rmdir(davew);
    rmdir(save_dir);rmdir(work_dir);rmdir(root);
    puts("portable keyboard bindings: PASS");
    return 0;
}
