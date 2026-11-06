// Keyboard-to-Switch-controller emulation (NOT native Diablo II PC actions).
// Its private per-character <hero>.kbm must never occupy native <hero>.key.
// Legacy mobile text .key files are read ONLY as migration input and never overwritten.
// Native .key lossless binary codec lives separately in dm_keyfile.{h,c}.
#include "dm_kbmap.h"
#include "dm_pc_profile.h"
#include "dm_pc_input.h"
#include "dm_input.h"
#include "dm_world.h"
#include "dm_uiscreen.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *dm_save_commitdir(void);
const char *dm_save_workdir(void);
void dm_kb_clear(void);
// Serialize profile writes with h_fs_Commit's directory swap.
void dm_fs_profile_lock(void),dm_fs_profile_unlock(void);
static pthread_mutex_t km_mu=PTHREAD_MUTEX_INITIALIZER;
static unsigned char map[256],def[256],down[256],active[256];
static unsigned char mouse_map[3]={0,1,2};
static int inited,loaded,foreign_format;
static int native_pc_mode=1; // new character: PC key semantics; legacy .kbm remains opt-in
static char hero[64]; // selected character, protected by km_mu
static void defaults(unsigned char *m) {
    memset(m,DM_KB_UNBOUND,256);
    // Same physical keys as front_mac.m: WASD, J/K/U/I, F/G, Q/E/Z/C.
    m[0x1a]=DM_KB_MOVE_UP; m[0x04]=DM_KB_MOVE_LEFT;
    m[0x16]=DM_KB_MOVE_DOWN; m[0x07]=DM_KB_MOVE_RIGHT;
    m[0x0d]=0; m[0x0e]=1; m[0x18]=2; m[0x0c]=3;
    m[0x09]=4; m[0x0a]=5; m[0x14]=6; m[0x08]=7;
    m[0x1d]=8; m[0x06]=9;
    m[0x28]=DM_KB_ENTER; m[0x29]=DM_KB_ESCAPE;
    m[0x2b]=11; m[0x2e]=10; m[0x57]=10; m[0x2c]=0;
    m[0x50]=12; m[0x52]=13; m[0x4f]=14; m[0x51]=15;
    // Mobile extras: F1–F8 native skill slots; 1–4 potion D-pad; M map.
    for (int k=0;k<8;k++) m[0x3a+k]=(unsigned char)(DM_KB_SKILL1+k);
    m[0x1e]=12; m[0x1f]=13; m[0x20]=14; m[0x21]=15;
    m[0x10]=DM_KB_MAP;
}
static void init_locked(void) {
    if (inited) return;
    defaults(def);memcpy(map,def,256);inited=1;
}
static int valid(unsigned a) {
    return a<20 || a==DM_KB_ENTER || a==DM_KB_ESCAPE ||
        a==DM_KB_MAP || (a>=DM_KB_SKILL1 && a<=DM_KB_SKILL8) || a==DM_KB_UNBOUND;
}
// Reject path separators and unselected/empty hero names. Names are copied
// from the game's own selected or in-world unit, never from UI text fields.
static int hero_name_ok(const char *name) {
    if(!name || !*name || strlen(name)>48 || !strcmp(name,".") || !strcmp(name,".."))
        return 0;
    for(const unsigned char *p=(const unsigned char *)name;*p;p++)
        if(*p<' ' || *p=='/' || *p=='\\' || *p==127) return 0;
    return 1;
}
static int filename_at(char *out,size_t n,const char *root) {
    if(!root || !*root || !hero_name_ok(hero))return 0;
    int len=snprintf(out,n,"%s/%s/%s.kbm",root,hero,hero);
    return len>0 && (size_t)len<n;
}
static int filename(char *out,size_t n) {
    return filename_at(out,n,dm_save_commitdir());
}
void dm_kbmap_select_character(const char *name) {
    if(!hero_name_ok(name))return;
    pthread_mutex_lock(&km_mu);
    init_locked();
    int changed=strcmp(hero,name)!=0;
    if(changed) {
        snprintf(hero,sizeof hero,"%s",name);
        memcpy(map,def,sizeof map);mouse_map[0]=0;mouse_map[1]=1;mouse_map[2]=2;
        memset(down,0,sizeof down);memset(active,0,sizeof active);
        loaded=foreign_format=0;native_pc_mode=1;
        // load on first read, so a newly mounted save is visible.
        dm_kb_clear();
    }
    pthread_mutex_unlock(&km_mu);
    if(changed) {
        dm_pc_input_reset(); // no stale hero-specific held command
        (void)dm_pc_profile_select(name);
    }
}
static void load_locked(void) {
    init_locked();
    if (loaded) return;
    char fn[1152];
    if (!filename(fn,sizeof fn)) return;
    loaded=1;
    FILE *f=fopen(fn,"rb");
    if(!f) {
        // Existing private .key belongs to an earlier D2R Mobile revision.
        // Probe only the exact text signature; never parse or touch native binary.
        char previous[1152],mark[128];const char *root=dm_save_commitdir();
        int n=root?snprintf(previous,sizeof previous,"%s/%s/%s.key",root,hero,hero):0;
        if(n>0 && (size_t)n<sizeof previous) {
            FILE *candidate=fopen(previous,"rb");
            if(candidate) {
                if(fgets(mark,sizeof mark,candidate) &&
                   !strcmp(mark,"D2RMobileKeyboard=1\n")) {rewind(candidate);f=candidate;}
                else fclose(candidate);
            }
        }
    }
    if(!f) {
        // Older global legacy config is also a read-only migration source.
        char old[1152];const char *root=dm_save_commitdir();
        int len=root?snprintf(old,sizeof old,"%s/../sw_keyboard.txt",root):0;
        if(len>0 && (size_t)len<sizeof old)f=fopen(old,"rb");
        if(!f)return;
    }
    native_pc_mode=0; // migrate a previous controller-emulation profile faithfully
    char line[128];
    if(!fgets(line,sizeof line,f) || strcmp(line,"D2RMobileKeyboard=1\n")) {
        // An unrecognized private .kbm belongs to its owner; refuse to overwrite.
        foreign_format=1;fclose(f);return;
    }
    while(fgets(line,sizeof line,f)) {
        if(!strcmp(line,"pcMode=1\n")){native_pc_mode=1;continue;}
        if(!strcmp(line,"pcMode=0\n")){native_pc_mode=0;continue;}
        unsigned usage=0,action=0;char extra=0;
        if(sscanf(line,"mouse%u=%u %c",&usage,&action,&extra)==2) {
            if(usage<3 && action<3) mouse_map[usage]=(unsigned char)action;
            continue;
        }
        if(sscanf(line,"%x=%u %c",&usage,&action,&extra)==2 && usage<256 && valid(action))
            map[usage]=(unsigned char)action;
    }
    fclose(f);
}
// Write to a requested save tree via atomic rename; refuse to overwrite a
// .key that was not written by this program.
static int save_to(const char *root) {
    char fn[1152],tmp[1176];
    if(foreign_format || !filename_at(fn,sizeof fn,root))return 0;
    FILE *old=fopen(fn,"rb");
    if(old) {
        char header[128];
        int ours=fgets(header,sizeof header,old) &&
                  !strcmp(header,"D2RMobileKeyboard=1\n");
        fclose(old);
        if(!ours)return 0;
    }
    if(snprintf(tmp,sizeof tmp,"%s.tmp",fn)<=0)return 0;
    FILE *f=fopen(tmp,"wb");if(!f)return 0;
    int ok=fputs("D2RMobileKeyboard=1\n",f)>=0;
    if(ok)ok=fprintf(f,"pcMode=%d\n",native_pc_mode)!=-1;
    for(unsigned i=0;ok && i<256;i++)if(map[i]!=def[i])
        ok=fprintf(f,"%02x=%u\n",i,(unsigned)map[i])>0;
    for(unsigned i=0;ok && i<3;i++)if(mouse_map[i]!=i)
        ok=fprintf(f,"mouse%u=%u\n",i,(unsigned)mouse_map[i])>0;
    if(fclose(f))ok=0;
    if(ok)ok=rename(tmp,fn)==0;
    if(!ok)remove(tmp);
    return ok;
}
static int save_locked(void) {
    const char *work=dm_save_workdir();
    // Game save transaction owns persistence. Do not write to committed
    // snapshots before the guest commits its active work tree.
    if(work && *work)return save_to(work);
    return save_to(dm_save_commitdir());
}
// .key and .ctl belong exclusively to the guest's original save pipeline.
// There is no host pre-commit copy/recovery step for either file: the game
// writes to save.work and h_fs_Commit atomically promotes the whole tree.
void dm_kbmap_pre_commit(const char *workdir) { (void)workdir; }

int dm_kbmap_pc_mode(void) {
    pthread_mutex_lock(&km_mu);
    load_locked();int v=native_pc_mode;
    pthread_mutex_unlock(&km_mu);return v;
}
int dm_kbmap_set_pc_mode(int enabled) {
    dm_fs_profile_lock();
    pthread_mutex_lock(&km_mu);
    load_locked();
    int old=native_pc_mode;native_pc_mode=!!enabled;
    int ok=save_locked();
    if(!ok)native_pc_mode=old;
    pthread_mutex_unlock(&km_mu);
    dm_fs_profile_unlock();
    if(ok)dm_kbmap_reset(); // flush held edges from the previous mapping
    return ok;
}
int dm_kbmap_action(unsigned usage) {
    if(usage>=256)return DM_KB_UNBOUND;
    pthread_mutex_lock(&km_mu);load_locked();
    int a=map[usage];pthread_mutex_unlock(&km_mu);return a;
}
int dm_kbmap_bind(unsigned usage,unsigned action) {
    if(usage>=256 || !valid(action))return 0;
    dm_fs_profile_lock();
    pthread_mutex_lock(&km_mu);load_locked();
    unsigned char before=map[usage];int before_mode=native_pc_mode;
    map[usage]=(unsigned char)action;
    native_pc_mode=0; // Editing HID->controller explicitly selects that input mode.
    int ok=save_locked();
    if(!ok){map[usage]=before;native_pc_mode=before_mode;}
    pthread_mutex_unlock(&km_mu);
    dm_fs_profile_unlock();return ok;
}
int dm_kbmap_restore_defaults_checked(void) {
    dm_fs_profile_lock();
    pthread_mutex_lock(&km_mu);load_locked();
    unsigned char previous[256],previous_mouse[3];
    int previous_mode=native_pc_mode;
    native_pc_mode=0; // The legacy mapping's Reset controls legacy mode.
    memcpy(previous,map,sizeof previous);
    memcpy(previous_mouse,mouse_map,sizeof previous_mouse);
    memcpy(map,def,256);mouse_map[0]=0;mouse_map[1]=1;mouse_map[2]=2;
    int ok=save_locked();
    if(!ok) {
        memcpy(map,previous,sizeof previous);
        memcpy(mouse_map,previous_mouse,sizeof previous_mouse);
        native_pc_mode=previous_mode;
    }
    pthread_mutex_unlock(&km_mu);
    dm_fs_profile_unlock();
    return ok;
}
void dm_kbmap_restore_defaults(void) { (void)dm_kbmap_restore_defaults_checked(); }
void dm_kbmap_reset(void) {
    pthread_mutex_lock(&km_mu);memset(down,0,256);memset(active,0,256);
    pthread_mutex_unlock(&km_mu);dm_pc_input_reset();dm_kb_clear();
}
void dm_kbmap_key_event(unsigned usage,int pressed) {
    if(usage>=256)return;
    pthread_mutex_lock(&km_mu);load_locked();
    if(native_pc_mode) {
        pthread_mutex_unlock(&km_mu);
        dm_pc_input_event(usage,pressed);
        return;
    }
    if((int)down[usage]==!!pressed) { pthread_mutex_unlock(&km_mu);return; }
    if(pressed) {
        unsigned a=map[usage];
        if(a==DM_KB_MAP) { dm_world_mouse_map_toggle(); a=DM_KB_UNBOUND; }
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
            static const int btn[4]={1,2,3,7}; // B, X, Y, R native skill slots
            if(idx>=4)bits|=DM_BTN_ZL;
            bits|=1ull<<btn[idx%4];
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
    } else if(!strcmp(cat,"PanelManager") && (!strcmp(action,"ClosePanel") || !strcmp(action,"UnloadPanel"))
              && arg && (!strcmp(arg,"SettingsPanel") || !strcmp(arg,"KeyBindingMenu"))) {
        atomic_store(&controls_visible,0);
    }
}

int dm_kbmap_mouse_action(unsigned physical) {
    if(physical>=3)return -1;
    pthread_mutex_lock(&km_mu);load_locked();
    int a=mouse_map[physical];pthread_mutex_unlock(&km_mu);return a;
}
int dm_kbmap_bind_mouse(unsigned physical,unsigned action) {
    if(physical>=3 || action>=3)return 0;
    dm_fs_profile_lock();
    pthread_mutex_lock(&km_mu);load_locked();
    unsigned char before=mouse_map[physical];mouse_map[physical]=(unsigned char)action;
    int ok=save_locked();if(!ok)mouse_map[physical]=before;
    pthread_mutex_unlock(&km_mu);
    dm_fs_profile_unlock();return ok;
}
