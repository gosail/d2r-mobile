// Synthetic fixtures only: never commit a user's personal .key bindings.
// Tests exercise the observed record layout, NOT yet full original-game parity.
#define _POSIX_C_SOURCE 200809L
#include "dm_keyfile.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
enum { N=57, LEN=2+N*2*10 };
static void put16(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void sample(uint8_t *b) {
    memset(b,0,LEN);b[0]=0x25;
    for(unsigned i=0;i<N;i++)for(unsigned s=0;s<2;s++) {
        uint8_t *p=b+2+(i*2+s)*10;
        put16(p,i);put16(p+2,0xAB00u+i); // opaque field remains unmodified
        put16(p+4,s?0xffffu:(i==7?0x0100u:65u+i));
        put16(p+6,s?0u:1u);
        put16(p+8,0xCD00u+i); // opaque tail remains unmodified
    }
}
static void read_bytes(const char *name,uint8_t *out,size_t n) {
    FILE *f=fopen(name,"rb");assert(f);
    assert(fread(out,1,n,f)==n);assert(fgetc(f)==EOF);assert(!fclose(f));
}
int main(void) {
    uint8_t original[LEN],copy[LEN];sample(original);
    dm_keyfile f={0};size_t len=0;
    assert(dm_keyfile_parse(&f,original,LEN)==DM_KEYFILE_OK);
    assert(f.records==114 && f.observed_header==0x25);
    assert(dm_keyfile_data(&f,&len) && len==LEN);
    assert(!memcmp(dm_keyfile_data(&f,NULL),original,LEN));
    assert(!dm_keyfile_dirty(&f));
    unsigned v=0;
    assert(dm_keyfile_get(&f,7,0,(uint16_t *)&v)==DM_KEYFILE_OK);
    assert((v & 0xffff)==0x0100);
    assert(dm_keyfile_get(&f,7,1,(uint16_t *)&v)==DM_KEYFILE_OK);
    assert((v & 0xffff)==0xffff);
    assert(dm_keyfile_get(&f,999,0,(uint16_t *)&v)==DM_KEYFILE_NOT_FOUND);
    assert(dm_keyfile_set(&f,7,0,0x0100)==DM_KEYFILE_OK);
    assert(!dm_keyfile_dirty(&f));
    assert(dm_keyfile_set(&f,7,1,0x0101)==DM_KEYFILE_OK);
    assert(dm_keyfile_dirty(&f));
    const uint8_t *edited=dm_keyfile_data(&f,NULL);
    for(size_t i=0;i<LEN;i++)
        if(i!=2+(7*2+1)*10+4 && i!=2+(7*2+1)*10+5)
            assert(edited[i]==original[i]);
    char root[]="/tmp/dmkeyXXXXXX";assert(mkdtemp(root));
    char path[256],bak[260];snprintf(path,sizeof path,"%s/Tester.key",root);
    snprintf(bak,sizeof bak,"%s.bak",path);
    {FILE *out=fopen(path,"wb");assert(out);
     assert(fwrite(original,1,LEN,out)==LEN);assert(!fclose(out));}
    assert(dm_keyfile_write_atomic(&f,path)==DM_KEYFILE_OK);
    read_bytes(path,copy,LEN);assert(!memcmp(copy,edited,LEN));
    read_bytes(bak,copy,LEN);assert(!memcmp(copy,original,LEN));
    assert(!dm_keyfile_dirty(&f));
    assert(dm_keyfile_write_atomic(&f,path)==DM_KEYFILE_OK); // no-op
    dm_keyfile g={0};
    assert(dm_keyfile_read(&g,path)==DM_KEYFILE_OK);
    assert(!memcmp(dm_keyfile_data(&g,NULL),dm_keyfile_data(&f,NULL),LEN));
    assert(dm_keyfile_set(&g,30,0,0xffff)==DM_KEYFILE_OK);
    assert(dm_keyfile_set(&g,31,1,300)==DM_KEYFILE_OK);
    assert(dm_keyfile_write_atomic(&g,path)==DM_KEYFILE_OK);
    dm_keyfile_free(&g);
    assert(dm_keyfile_read(&g,path)==DM_KEYFILE_OK);
    uint16_t key=0;
    assert(dm_keyfile_get(&g,30,0,&key)==DM_KEYFILE_OK && key==0xffff);
    assert(dm_keyfile_get(&g,31,1,&key)==DM_KEYFILE_OK && key==300);
    assert(dm_keyfile_set(&g,0,0,90)==DM_KEYFILE_OK);
    // Concurrent external modification must not be silently overwritten.
    {FILE *out=fopen(path,"r+b");assert(out);assert(fputc(0x24,out)!=EOF);assert(!fclose(out));}
    assert(dm_keyfile_write_atomic(&g,path)==DM_KEYFILE_CONFLICT);
    dm_keyfile_free(&g);
    // Invalid/unknown variants leave the destination untouched.
    uint8_t invalid[LEN];memcpy(invalid,original,LEN);
    invalid[0]=0x44;assert(dm_keyfile_parse(&g,invalid,LEN)==DM_KEYFILE_UNSUPPORTED);
    invalid[0]=0x25;
    assert(dm_keyfile_parse(&g,invalid,LEN-1)==DM_KEYFILE_INVALID);
    put16(invalid+2+6,2);assert(dm_keyfile_parse(&g,invalid,LEN)==DM_KEYFILE_UNSUPPORTED);
    put16(invalid+2+6,1);
    put16(invalid+2+10+6,1);assert(dm_keyfile_parse(&g,invalid,LEN)==DM_KEYFILE_INVALID);
    // Variable record counts accepted when valid pairs remain intact.
    assert(dm_keyfile_parse(&g,original,LEN-20)==DM_KEYFILE_OK);
    dm_keyfile_free(&g);
    assert(dm_keyfile_parse(&g,original,LEN)==DM_KEYFILE_OK);
    assert(dm_keyfile_set(&g,56,0,0xffff)==DM_KEYFILE_OK);
    dm_keyfile_free(&g);dm_keyfile_free(&f);
    assert(!unlink(path));assert(!unlink(bak));assert(!rmdir(root));
    puts("native .key lossless codec: PASS");
    return 0;
}
