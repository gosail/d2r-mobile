#define _POSIX_C_SOURCE 200809L
#include "dm_pc_profile.h"
#include "dm_keyfile.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

const char *dm_save_commitdir(void);
const char *dm_save_workdir(void);
void dm_fs_profile_lock(void),dm_fs_profile_unlock(void);
unsigned dm_fs_native_key_writers(void);
void dm_fs_save_poison(int e);

static pthread_mutex_t pc_mu=PTHREAD_MUTEX_INITIALIZER;
static char selected[64];
static dm_keyfile profile;

static int valid_name(const char *name) {
    if(!name || !name[0] || strlen(name)>48 ||
       !strcmp(name,".") || !strcmp(name,".."))return 0;
    for(const unsigned char *p=(const unsigned char *)name;*p;p++)
        if(*p<' ' || *p=='/' || *p=='\\' || *p==127)return 0;
    return 1;
}
static int path(char out[1200]) {
    // The game writes its own .key in the active transactional save tree.
    // Never write the committed snapshot while a guest work tree is mounted.
    const char *root=dm_save_workdir();
    if(!root || !*root)root=dm_save_commitdir();
    if(!root || !*root || !valid_name(selected))return 0;
    int n=snprintf(out,1200,"%s/%s/%s.key",root,selected,selected);
    return n>0 && n<1200;
}
void dm_pc_profile_clear(void) {
    pthread_mutex_lock(&pc_mu);
    dm_keyfile_free(&profile);
    selected[0]=0;
    pthread_mutex_unlock(&pc_mu);
}
int dm_pc_profile_select(const char *name) {
    if(!valid_name(name))return DM_KEYFILE_INVALID;
    pthread_mutex_lock(&pc_mu);
    if(strcmp(selected,name)) {
        dm_keyfile_free(&profile);
        snprintf(selected,sizeof selected,"%s",name);
    }
    pthread_mutex_unlock(&pc_mu);
    // The guest, not this host adapter, creates the native character .key.
    // Selection must not create a guessed preset or change the save tree.
    return DM_KEYFILE_OK;
}
static int read_locked(void) {
    // A guest save can update the native profile using several writes under
    // one open handle. Never parse its intermediate bytes as a new mapping.
    // Keep the last fully validated snapshot only for the same character;
    // select_character() invalidates it before switching hero.
    if(dm_fs_native_key_writers())
        return profile.raw?DM_KEYFILE_OK:DM_KEYFILE_CONFLICT;
    char fn[1200];
    if(!path(fn))return DM_KEYFILE_NOT_FOUND;
    // Always read the active guest file, not a stale snapshot from a prior
    // commit. The original game may rewrite key bindings through its own UI.
    // Native files are small; this also detects inode swaps after Commit.
    dm_keyfile temp={0};
    int rc=dm_keyfile_read(&temp,fn);
    if(rc==DM_KEYFILE_OK) {
        dm_keyfile_free(&profile);
        profile=temp;
    } else {
        dm_keyfile_free(&temp);
        dm_keyfile_free(&profile);
    }
    return rc;
}
int dm_pc_profile_get(uint32_t id,unsigned slot,uint16_t *vk) {
    // Serialize reads against the save directory swap; the guest owns file creation.
    dm_fs_profile_lock();
    pthread_mutex_lock(&pc_mu);
    int rc=read_locked();
    if(!rc)rc=dm_keyfile_get(&profile,id,slot,vk);
    pthread_mutex_unlock(&pc_mu);
    dm_fs_profile_unlock();
    return rc;
}
int dm_pc_profile_bind(uint32_t id,unsigned slot,uint16_t vk,
                       uint32_t *conflict,unsigned *conflict_slot) {
    // Physical arrow keys are reserved for the Switch left stick / menu
    // D-pad. Reject editing them as another command: otherwise the editor
    // could report a successful binding the runtime intentionally ignores.
    if(vk>=0x25 && vk<=0x28)return DM_KEYFILE_UNSUPPORTED;
    dm_fs_profile_lock();
    // Never change the committed save snapshot. While the game's .key is
    // open for writing, the host must not race its multi-write serialization.
    if(!dm_save_workdir()) { dm_fs_profile_unlock(); return DM_KEYFILE_NOT_FOUND; }
    if(dm_fs_native_key_writers()) {
        dm_fs_profile_unlock(); return DM_KEYFILE_CONFLICT;
    }
    pthread_mutex_lock(&pc_mu);
    int rc=read_locked();
    if(!rc) {
        uint16_t old;
        rc=dm_keyfile_get(&profile,id,slot,&old);
        if(!rc) {
            rc=dm_keyfile_bind_checked(&profile,id,slot,vk,conflict,conflict_slot);
            if(!rc) {
                char fn[1200];
                rc=path(fn)?dm_keyfile_write_atomic(&profile,fn):DM_KEYFILE_INVALID;
                if(rc) {
                    // A partial native host pwrite/fsync failure must never be
                    // promoted by the game's next Commit. The original save
                    // transaction will roll back on its poisoned state.
                    if(rc==DM_KEYFILE_IO)dm_fs_save_poison(errno?errno:EIO);
                    (void)dm_keyfile_set(&profile,id,slot,old);
                }
            }
        }
    }
    pthread_mutex_unlock(&pc_mu);
    dm_fs_profile_unlock();
    return rc;
}
int dm_pc_profile_file_status(void) {
    // Do not infer validity from a particular command ID: other game-created
    // versions may omit ID 0 while still having a well-formed profile.
    dm_fs_profile_lock();
    pthread_mutex_lock(&pc_mu);
    int rc=read_locked();
    pthread_mutex_unlock(&pc_mu);
    dm_fs_profile_unlock();
    if(rc==DM_KEYFILE_OK)return 1;
    // Only a genuinely absent file may use the temporary controller
    // fallback. A present but unreadable .key is an error, not an invitation
    // to silently dispatch different gamepad-style actions.
    if(rc==DM_KEYFILE_NOT_FOUND)return 0;
    return -1;
}
int dm_pc_profile_action_for_vk(uint16_t vk,uint32_t *id,unsigned *slot) {
    dm_fs_profile_lock();
    pthread_mutex_lock(&pc_mu);
    int rc=read_locked();
    if(!rc)rc=dm_keyfile_lookup_vk(&profile,vk,id,slot);
    pthread_mutex_unlock(&pc_mu);
    dm_fs_profile_unlock();
    return rc;
}
int dm_pc_vk_from_hid(unsigned u) {
    if(u>=0x04 && u<=0x1d)return 'A'+(int)(u-0x04);
    if(u>=0x1e && u<=0x26)return '1'+(int)(u-0x1e);
    if(u==0x27)return '0';
    if(u>=0x3a && u<=0x45)return 0x70+(int)(u-0x3a);
    if(u>=0x59 && u<=0x61)return 0x61+(int)(u-0x59);
    if(u==0x62)return 0x60;
    switch(u) {
    case 0x28: return 0x0d; case 0x29: return 0x1b;
    case 0x2a: return 0x08; case 0x2b: return 0x09;
    case 0x2c: return 0x20; case 0x2d: return 0xbd;
    case 0x2e: return 0xbb; case 0x2f: return 0xdb;
    case 0x30: return 0xdd; case 0x31: return 0xdc;
    case 0x33: return 0xba; case 0x34: return 0xde;
    case 0x35: return 0xc0; case 0x36: return 0xbc;
    case 0x37: return 0xbe; case 0x38: return 0xbf;
    case 0x39: return 0x14; case 0x46: return 0x2c;
    case 0x47: return 0x91; case 0x48: return 0x13;
    case 0x49: return 0x2d; case 0x4a: return 0x24;
    case 0x4b: return 0x21; case 0x4c: return 0x2e;
    case 0x4d: return 0x23; case 0x4e: return 0x22;
    case 0x4f: return 0x27; case 0x50: return 0x25;
    case 0x51: return 0x28; case 0x52: return 0x26;
    case 0x54: return 0x6f; case 0x55: return 0x6a;
    case 0x56: return 0x6d; case 0x57: return 0x6b;
    case 0x58: return 0x0d; case 0x63: return 0x6e;
    case 0x64: return 0xe2;
    case 0xe0: case 0xe4: return 0x11;
    case 0xe1: case 0xe5: return 0x10;
    case 0xe2: case 0xe6: return 0x12;
    case 0xe3: return 0x5b; case 0xe7: return 0x5c;
    default: return -1;
    }
}
int dm_pc_profile_action_for_hid(unsigned usage,uint32_t *id,unsigned *slot) {
    int vk=dm_pc_vk_from_hid(usage);
    return vk<0?DM_KEYFILE_UNSUPPORTED:
        dm_pc_profile_action_for_vk((uint16_t)vk,id,slot);
}
