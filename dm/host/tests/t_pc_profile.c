// Synthetic native .key profile tests. Never store a user's imported key file.
#define _POSIX_C_SOURCE 200809L
#include "dm_keyfile.h"
#include "dm_pc_profile.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static char root[1024], save_dir[1100];
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
    assert(dm_pc_profile_get(1,1,&vk)==DM_KEYFILE_OK && vk=='B');
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
    char d[1200];snprintf(d,sizeof d,"%s/Alice",save_dir);assert(!rmdir(d));
    snprintf(d,sizeof d,"%s/Bob",save_dir);assert(!rmdir(d));
    snprintf(d,sizeof d,"%s/Eve",save_dir);assert(!rmdir(d));
    assert(!rmdir(save_dir));assert(!rmdir(root));
    puts("native character keyboard profiles: PASS");
    return 0;
}
