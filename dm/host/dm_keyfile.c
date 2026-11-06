#define _POSIX_C_SOURCE 200809L
#include "dm_keyfile.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

enum { HEADER_BYTES=2, RECORD_BYTES=10, MAX_BYTES=1024*1024, MAX_RECORDS=4096 };
static uint16_t le16(const uint8_t *p) {
    return (uint16_t)((unsigned)p[0] | ((unsigned)p[1]<<8));
}
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)le16(p) | ((uint32_t)le16(p+2)<<16);
}
static void store16(uint8_t *p,uint16_t x) {
    p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);
}
void dm_keyfile_free(dm_keyfile *f) {
    if(!f)return;
    free(f->raw);free(f->baseline);memset(f,0,sizeof *f);
}
static int validate(const uint8_t *raw,size_t len) {
    if(!raw || len<HEADER_BYTES || len>MAX_BYTES)return DM_KEYFILE_INVALID;
    // Check format header before checking record count, including old mobile text files.
    if(le16(raw)!=0x0025)return DM_KEYFILE_UNSUPPORTED;
    if(len<HEADER_BYTES+2*RECORD_BYTES ||
       (len-HEADER_BYTES)%RECORD_BYTES)return DM_KEYFILE_INVALID;
    size_t n=(len-HEADER_BYTES)/RECORD_BYTES;
    if(n%2)return DM_KEYFILE_INVALID;
    if(n>MAX_RECORDS)return DM_KEYFILE_UNSUPPORTED;
    // Every observed operation has exactly one flag 1 and one flag 0.
    // Reject ambiguous/unknown layouts instead of guessing and corrupting them.
    for(size_t i=0;i<n;i++) {
        const uint8_t *p=raw+HEADER_BYTES+i*RECORD_BYTES;
        uint32_t id=le32(p),flag=le32(p+6);
        if(flag>1)return DM_KEYFILE_UNSUPPORTED;
        unsigned matches=0,seen=0;
        for(size_t j=0;j<n;j++) {
            const uint8_t *q=raw+HEADER_BYTES+j*RECORD_BYTES;
            if(le32(q)==id){matches++;uint32_t slot=le32(q+6);if(slot>1)return DM_KEYFILE_UNSUPPORTED;seen|=1u<<slot;}
        }
        if(matches!=2 || seen!=3)return DM_KEYFILE_INVALID;
    }
    return DM_KEYFILE_OK;
}
int dm_keyfile_parse(dm_keyfile *out,const void *data,size_t len) {
    if(!out)return DM_KEYFILE_INVALID;
    int ok=validate(data,len);
    if(ok)return ok;
    uint8_t *a=malloc(len),*b=malloc(len);
    if(!a || !b){free(a);free(b);return DM_KEYFILE_IO;}
    memcpy(a,data,len);memcpy(b,data,len);
    // Caller must pass an empty or previously freed object.
    out->raw=a;out->baseline=b;out->size=len;
    out->records=(len-HEADER_BYTES)/RECORD_BYTES;
    out->observed_header=le16(a);
    return DM_KEYFILE_OK;
}
int dm_keyfile_read(dm_keyfile *out,const char *path) {
    if(!out || !path)return DM_KEYFILE_INVALID;
    struct stat st;
    if(lstat(path,&st))return errno==ENOENT?DM_KEYFILE_NOT_FOUND:DM_KEYFILE_IO;
    if(!S_ISREG(st.st_mode) || st.st_size<0 ||
       (uintmax_t)st.st_size>MAX_BYTES)return DM_KEYFILE_IO;
    FILE *in=fopen(path,"rb");if(!in)return DM_KEYFILE_IO;
    size_t n=(size_t)st.st_size;
    uint8_t *p=malloc(n?n:1);
    if(!p){fclose(in);return DM_KEYFILE_IO;}
    int ok=fread(p,1,n,in)==n && fgetc(in)==EOF && !ferror(in);
    if(fclose(in))ok=0;
    int ret=ok?dm_keyfile_parse(out,p,n):DM_KEYFILE_IO;
    free(p);return ret;
}
const uint8_t *dm_keyfile_data(const dm_keyfile *f,size_t *n) {
    if(n)*n=f?f->size:0;
    return f?f->raw:NULL;
}
static uint8_t *record(const dm_keyfile *f,uint32_t id,unsigned slot) {
    if(!f || !f->raw || slot>1)return NULL;
    for(size_t i=0;i<f->records;i++) {
        uint8_t *p=f->raw+HEADER_BYTES+i*RECORD_BYTES;
        if(le32(p)==id && le32(p+6)==(uint32_t)(slot?0:1))return p;
    }
    return NULL;
}
int dm_keyfile_get(const dm_keyfile *f,uint32_t id,unsigned slot,uint16_t *code) {
    if(!code || slot>1)return DM_KEYFILE_INVALID;
    uint8_t *p=record(f,id,slot);
    if(!p)return DM_KEYFILE_NOT_FOUND;
    *code=le16(p+4);return DM_KEYFILE_OK;
}
int dm_keyfile_set(dm_keyfile *f,uint32_t id,unsigned slot,uint16_t code) {
    if(slot>1)return DM_KEYFILE_INVALID;
    uint8_t *p=record(f,id,slot);
    if(!p)return DM_KEYFILE_NOT_FOUND;
    store16(p+4,code);return DM_KEYFILE_OK;
}
// Indexed by original D2Client command ID (0..56), NOT by row position in
// any imported file. Independent classic/D2R Legacy documented baseline.
// 0xffff means unbound, 0x100..0x104 are game-private mouse/wheel codes.
typedef struct { uint16_t primary,secondary; } dm_legacy_pair;
static const dm_legacy_pair dm_legacy[57]={
    {0x41,0x43}, {0x49,0x42}, {0x50,0xffff}, {0x4d,0xffff},
    {0x51,0xffff}, {0x0d,0xffff}, {0x48,0xffff}, {0x09,0x100},
    {0x78,0xffff}, {0x79,0xffff}, {0x7a,0xffff}, {0x7b,0xffff},
    {0x54,0xffff}, {0x53,0xffff},
    {0x70,0xffff}, {0x71,0xffff}, {0x72,0xffff}, {0x73,0xffff},
    {0x74,0xffff}, {0x75,0xffff}, {0x76,0xffff}, {0x77,0xffff},
    {0xc0,0xffff}, {0x31,0xffff}, {0x32,0xffff}, {0x33,0xffff},
    {0x34,0xffff}, {0x60,0xffff}, {0x61,0xffff}, {0x62,0xffff},
    {0x63,0xffff}, {0x64,0xffff}, {0x65,0xffff}, {0x66,0xffff},
    {0x11,0xffff}, {0x52,0x102}, {0x10,0xffff}, {0x12,0x101},
    {0x20,0xffff}, {0x103,0xffff}, {0x104,0xffff},
    {0x4e,0xffff}, {0x2c,0xffff}, {0x5a,0xffff}, {0x57,0xffff},
    {0x56,0xffff},
    {0xffff,0xffff}, {0xffff,0xffff}, {0xffff,0xffff}, {0xffff,0xffff},
    {0xffff,0xffff}, {0xffff,0xffff}, {0xffff,0xffff}, {0xffff,0xffff},
    {0x4f,0xffff}, {0x67,0xffff}, {0x1b,0xffff}
};
int dm_keyfile_new_legacy(dm_keyfile *out) {
    if(!out)return DM_KEYFILE_INVALID;
    enum { COUNT=sizeof dm_legacy/sizeof dm_legacy[0], BYTES=2+COUNT*2*10 };
    uint8_t buf[BYTES];memset(buf,0,sizeof buf);
    store16(buf,0x25);
    for(unsigned id=0;id<COUNT;id++)for(unsigned slot=0;slot<2;slot++) {
        uint8_t *p=buf+2+(id*2+slot)*10;
        // command ID and DWORD slot are little-endian.
        store16(p,(uint16_t)id);
        store16(p+4,slot?dm_legacy[id].secondary:dm_legacy[id].primary);
        store16(p+6,slot?0:1);
    }
    return dm_keyfile_parse(out,buf,sizeof buf);
}
int dm_keyfile_new_resurrected(dm_keyfile *out) {
    int rc=dm_keyfile_new_legacy(out);
    if(rc)return rc;
    // Resurrected skill profile: keys 1..0, - and = for Skill 1..12.
    // The original 1.13c Legacy potion 1..4 bindings must be cleared first.
    for(uint32_t id=23;id<=26;id++) {
        rc=dm_keyfile_set(out,id,0,0xffff);
        if(rc)break;
    }
    for(uint32_t i=0;!rc && i<12;i++) {
        uint32_t id=i<8?14+i:46+(i-8);
        uint16_t vk=i<9?(uint16_t)('1'+i):
            i==9?'0':i==10?0xbd:0xbb;
        rc=dm_keyfile_set(out,id,0,vk);
    }
    if(!rc)memcpy(out->baseline,out->raw,out->size);
    if(rc)dm_keyfile_free(out);
    return rc;
}
int dm_keyfile_lookup_vk(const dm_keyfile *f,uint16_t vk,uint32_t *id,unsigned *slot) {
    if(!f || !f->raw || !id || !slot || vk==0xffff)return DM_KEYFILE_INVALID;
    int found=0;uint32_t command=0;unsigned binding=0;
    for(size_t i=0;i<f->records;i++) {
        const uint8_t *p=f->raw+HEADER_BYTES+i*RECORD_BYTES;
        if(le16(p+4)==vk) {
            if(found)return DM_KEYFILE_CONFLICT; // do not dispatch ambiguous imported files
            command=le32(p);binding=le32(p+6)==1?0u:1u;
            found=1;
        }
    }
    if(!found)return DM_KEYFILE_NOT_FOUND;
    *id=command;*slot=binding;
    return DM_KEYFILE_OK;
}
int dm_keyfile_bind_checked(dm_keyfile *f,uint32_t id,unsigned slot,uint16_t vk,
                            uint32_t *conflicting_id,unsigned *conflicting_slot) {
    if(!f || !f->raw || slot>1)return DM_KEYFILE_INVALID;
    if(!record(f,id,slot))return DM_KEYFILE_NOT_FOUND;
    if(vk!=0xffff)for(size_t i=0;i<f->records;i++) {
        const uint8_t *p=f->raw+HEADER_BYTES+i*RECORD_BYTES;
        uint32_t other=le32(p);
        unsigned other_slot=le32(p+6)==1?0u:1u;
        if(le16(p+4)==vk && (other!=id || other_slot!=slot)) {
            if(conflicting_id)*conflicting_id=other;
            if(conflicting_slot)*conflicting_slot=other_slot;
            return DM_KEYFILE_CONFLICT;
        }
    }
    return dm_keyfile_set(f,id,slot,vk);
}
int dm_keyfile_dirty(const dm_keyfile *f) {
    return f && f->raw && f->baseline &&
        memcmp(f->raw,f->baseline,f->size)!=0;
}
static int write_all(int fd,const uint8_t *p,size_t n) {
    while(n){ssize_t w=write(fd,p,n);
        if(w<0 && errno==EINTR)continue;
        if(w<=0)return 0;
        p+=(size_t)w;n-=(size_t)w;
    }return 1;
}
static int matches_original(const char *path,const dm_keyfile *f,int *exists) {
    struct stat st;
    if(lstat(path,&st)) {
        if(errno==ENOENT){*exists=0;return DM_KEYFILE_OK;}
        return DM_KEYFILE_IO;
    }
    if(!S_ISREG(st.st_mode))return DM_KEYFILE_CONFLICT;
    *exists=1;
    if(st.st_size<0 || (uintmax_t)st.st_size!=f->size)return DM_KEYFILE_CONFLICT;
    FILE *in=fopen(path,"rb");if(!in)return DM_KEYFILE_IO;
    uint8_t buf[4096];size_t pos=0;int equal=1;
    while(pos<f->size && equal) {
        size_t take=f->size-pos<sizeof buf?f->size-pos:sizeof buf;
        if(fread(buf,1,take,in)!=take || memcmp(buf,f->baseline+pos,take))equal=0;
        pos+=take;
    }
    if(fgetc(in)!=EOF || ferror(in))equal=0;
    if(fclose(in))return DM_KEYFILE_IO;
    return equal?DM_KEYFILE_OK:DM_KEYFILE_CONFLICT;
}
static int backup_once(const char *path,const dm_keyfile *f) {
    size_t n=strlen(path);char *backup=malloc(n+5);
    if(!backup)return 0;
    memcpy(backup,path,n);memcpy(backup+n,".bak",5);
    int fd=open(backup,O_CREAT|O_EXCL|O_WRONLY,0600);
    if(fd<0){int ok=errno==EEXIST;free(backup);return ok;}
    int ok=write_all(fd,f->baseline,f->size) && fsync(fd)==0;
    if(close(fd))ok=0;
    if(!ok)unlink(backup); // never keep a partial backup
    free(backup);
    return ok;
}
int dm_keyfile_write_atomic(dm_keyfile *f,const char *path) {
    if(!f || !f->raw || !f->baseline || !path)return DM_KEYFILE_INVALID;
    int rv=validate(f->raw,f->size);if(rv)return rv;
    int exists=0;rv=matches_original(path,f,&exists);
    if(rv)return rv;
    if(exists && !dm_keyfile_dirty(f))return DM_KEYFILE_OK;
    if(exists && !backup_once(path,f))return DM_KEYFILE_IO;
    size_t n=strlen(path);char *temp=malloc(n+12);
    if(!temp)return DM_KEYFILE_IO;
    memcpy(temp,path,n);memcpy(temp+n,".tmp.XXXXXX",12);
    int fd=mkstemp(temp);
    if(fd<0){free(temp);return DM_KEYFILE_IO;}
    int ok=write_all(fd,f->raw,f->size) && fsync(fd)==0;
    if(close(fd))ok=0;
    if(ok) {
        if(exists)ok=rename(temp,path)==0;
        else ok=link(temp,path)==0; // no-clobber if another actor creates it
    }
    int error=errno;
    if(!exists || !ok)unlink(temp);
    free(temp);
    if(!ok)return exists?DM_KEYFILE_IO:
        (error==EEXIST?DM_KEYFILE_CONFLICT:DM_KEYFILE_IO);
    // Sync the containing directory, where supported.
    const char *slash=strrchr(path,'/');
    if(slash) {
        size_t dlen=(size_t)(slash-path);
        char *dir=malloc(dlen+1);
        if(dir){memcpy(dir,path,dlen);dir[dlen]=0;
            int dfd=open(dlen?dir:"/",O_RDONLY);
            if(dfd>=0){(void)fsync(dfd);close(dfd);}
            free(dir);
        }
    }
    memcpy(f->baseline,f->raw,f->size);
    return DM_KEYFILE_OK;
}
