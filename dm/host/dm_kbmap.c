// Shared keyboard binding/persistence layer for macOS defaults, iOS and Android.
// File: <save root>/../sw_keyboard.txt. Never touches character .key/.ctl files.
#include "dm_kbmap.h"
#include "dm_input.h"
#include "dm_world.h"
#include "dm_uiscreen.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *dm_save_commitdir(void);
static pthread_mutex_t km_mu=PTHREAD_MUTEX_INITIALIZER;
static unsigned char map[256],def[256],down[256],active[256];
static int inited,loaded;
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
static int filename(char *out,size_t n) {
    const char *root=dm_save_commitdir();
    if (!root || !*root) return 0; // mount is not ready; retry on next event
    int len=snprintf(out,n,"%s/../sw_keyboard.txt",root);
    return len>0 && (size_t)len<n;
}
static void load_locked(void) {
    init_locked();
    if (loaded) return;
    char fn[1152];
    if (!filename(fn,sizeof fn)) return;
    loaded=1;
    FILE *f=fopen(fn,"r");if(!f)return;
    char line[128];
    if(!fgets(line,sizeof line,f) || strcmp(line,"D2RMobileKeyboard=1\n")) {fclose(f);return;}
    while(fgets(line,sizeof line,f)) {
        unsigned usage=0,action=0;char extra=0;
        if(sscanf(line,"%x=%u %c",&usage,&action,&extra)==2 && usage<256 && valid(action))
            map[usage]=(unsigned char)action;
    }
    fclose(f);
}
static int save_locked(void) {
    char fn[1152],tmp[1176];
    if(!filename(fn,sizeof fn))return 0;
    if(snprintf(tmp,sizeof tmp,"%s.tmp",fn)<=0)return 0;
    FILE *f=fopen(tmp,"w");if(!f)return 0;
    int ok=fputs("D2RMobileKeyboard=1\n",f)>=0;
    for(unsigned i=0;ok && i<256;i++)if(map[i]!=def[i])
        ok=fprintf(f,"%02x=%u\n",i,(unsigned)map[i])>0;
    if(fclose(f))ok=0;
    if(ok)ok=rename(tmp,fn)==0;
    if(!ok)remove(tmp);
    return ok;
}
int dm_kbmap_action(unsigned usage) {
    if(usage>=256)return DM_KB_UNBOUND;
    pthread_mutex_lock(&km_mu);load_locked();
    int a=map[usage];pthread_mutex_unlock(&km_mu);return a;
}
int dm_kbmap_bind(unsigned usage,unsigned action) {
    if(usage>=256 || !valid(action))return 0;
    pthread_mutex_lock(&km_mu);load_locked();
    unsigned char before=map[usage];map[usage]=(unsigned char)action;
    int ok=save_locked();if(!ok)map[usage]=before;
    pthread_mutex_unlock(&km_mu);return ok;
}
void dm_kbmap_restore_defaults(void) {
    pthread_mutex_lock(&km_mu);load_locked();
    memcpy(map,def,256);save_locked();
    pthread_mutex_unlock(&km_mu);
}
void dm_kbmap_reset(void) {
    pthread_mutex_lock(&km_mu);memset(down,0,256);memset(active,0,256);
    pthread_mutex_unlock(&km_mu);dm_kb_set(0,0,0,0,0);
}
void dm_kbmap_key_event(unsigned usage,int pressed) {
    if(usage>=256)return;
    pthread_mutex_lock(&km_mu);load_locked();
    if((int)down[usage]==!!pressed) { pthread_mutex_unlock(&km_mu);return; }
    if(pressed) {
        unsigned a=map[usage];
        if(a==DM_KB_MAP) { dm_world_mouse_map_toggle(); a=DM_KB_UNBOUND; }
        // Choose Enter/Esc meaning when pressed; hold it until key release.
        if(a==DM_KB_ENTER)a=0; // confirm, matching Mac keyboard's common action
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
