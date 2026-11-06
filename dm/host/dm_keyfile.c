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

enum { HEADER_BYTES=2, RECORD_BYTES=10, MAX_BYTES=1024*1024 };
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
    if(!raw || len<HEADER_BYTES+2*RECORD_BYTES || len>MAX_BYTES ||
       (len-HEADER_BYTES)%RECORD_BYTES)return DM_KEYFILE_INVALID;
    // Empirically observed variant, NOT an assumed record-count header.
    if(le16(raw)!=0x0025)return DM_KEYFILE_UNSUPPORTED;
    size_t n=(len-HEADER_BYTES)/RECORD_BYTES;
    if(n%2)return DM_KEYFILE_INVALID;
    // Every observed operation has exactly one flag 1 and one flag 0.
    // Reject ambiguous/unknown layouts instead of guessing and corrupting them.
    for(size_t i=0;i<n;i++) {
        const uint8_t *p=raw+HEADER_BYTES+i*RECORD_BYTES;
        uint32_t id=le32(p);uint16_t flag=le16(p+6);
        if(flag>1)return DM_KEYFILE_UNSUPPORTED;
        unsigned matches=0,seen=0;
        for(size_t j=0;j<n;j++) {
            const uint8_t *q=raw+HEADER_BYTES+j*RECORD_BYTES;
            if(le32(q)==id){matches++;seen|=1u<<le16(q+6);}
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
    if(lstat(path,&st) || !S_ISREG(st.st_mode) || st.st_size<0 ||
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
        if(le32(p)==id && le16(p+6)==(uint16_t)(slot?0:1))return p;
    }
    return NULL;
}
int dm_keyfile_get(const dm_keyfile *f,uint16_t id,unsigned slot,uint16_t *code) {
    if(!code || slot>1)return DM_KEYFILE_INVALID;
    uint8_t *p=record(f,id,slot);
    if(!p)return DM_KEYFILE_NOT_FOUND;
    *code=le16(p+4);return DM_KEYFILE_OK;
}
int dm_keyfile_set(dm_keyfile *f,uint16_t id,unsigned slot,uint16_t code) {
    if(slot>1)return DM_KEYFILE_INVALID;
    uint8_t *p=record(f,id,slot);
    if(!p)return DM_KEYFILE_NOT_FOUND;
    store16(p+4,code);return DM_KEYFILE_OK;
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
    free(backup);
    if(fd<0)return errno==EEXIST; // preserve an earlier backup
    int ok=write_all(fd,f->baseline,f->size) && fsync(fd)==0;
    if(close(fd))ok=0;
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
    if(ok)ok=rename(temp,path)==0;
    if(!ok)unlink(temp);
    free(temp);
    if(!ok)return DM_KEYFILE_IO;
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
