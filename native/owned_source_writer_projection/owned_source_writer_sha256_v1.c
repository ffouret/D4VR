/* New checked one-shot implementation. Algorithm reference: the pinned C mock
 * fixture's ordinary-buffer SHA-256; no fixture code is linked or promoted. */
#include "owned_source_writer_sha256_v1.h"
#include <limits.h>

static const uint32_t sha_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static const uint32_t sha_iv[8] = {
    0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
};
static uint32_t sha_rotate(uint32_t x, unsigned n) { return (x>>n)|(x<<(32u-n)); }
static int sha_span(const void *p, uint64_t n, uintptr_t alignment) {
    return p && !((uintptr_t)p&(alignment-1u)) && n<=UINTPTR_MAX-(uintptr_t)p;
}
/* Only called after both nonempty spans passed the checked-end test. */
static int sha_overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
    return (uintptr_t)a<(uintptr_t)b+(uintptr_t)bn && (uintptr_t)b<(uintptr_t)a+(uintptr_t)an;
}

#define X(i) (w->schedule.w[(i)])
#define V(i) (w->schedule.working[(i)])
#define I (w->schedule.cursor)
static void sha_block(d4vr_osw_sha256_workspace_v1 *w) {
    for(I=0;I<16;I++) {
        X(I)=((uint32_t)w->state.block[4*I]<<24)|((uint32_t)w->state.block[4*I+1]<<16)
                |((uint32_t)w->state.block[4*I+2]<<8)|w->state.block[4*I+3];
    }
    for(I=16;I<64;I++) {
        X(I)=X(I-16)+(sha_rotate(X(I-15),7)^sha_rotate(X(I-15),18)^(X(I-15)>>3))
                +X(I-7)+(sha_rotate(X(I-2),17)^sha_rotate(X(I-2),19)^(X(I-2)>>10));
    }
    for(I=0;I<8;I++) { V(I)=w->state.h[I]; }
    for(I=0;I<64;I++) {
        w->schedule.t1=V(7)+(sha_rotate(V(4),6)^sha_rotate(V(4),11)^sha_rotate(V(4),25))
                +((V(4)&V(5))^(~V(4)&V(6)))+sha_k[I]+X(I);
        w->schedule.t2=(sha_rotate(V(0),2)^sha_rotate(V(0),13)^sha_rotate(V(0),22))
                +((V(0)&V(1))^(V(0)&V(2))^(V(1)&V(2)));
        V(7)=V(6);V(6)=V(5);V(5)=V(4);V(4)=V(3)+w->schedule.t1;
        V(3)=V(2);V(2)=V(1);V(1)=V(0);V(0)=w->schedule.t1+w->schedule.t2;
    }
    for(I=0;I<8;I++) { w->state.h[I]+=V(I); }
}

uint32_t d4vr_osw_source_sha256_v1(const uint8_t *source,uint64_t source_bytes,
        d4vr_osw_sha256_workspace_v1 *w,osw_sha256_v1 *digest_out) {
    /* Ownership/readability is a SOURCE caller precondition, not this test. */
    if(!sha_span(w,sizeof(*w),8)||!sha_span(digest_out,sizeof(*digest_out),1)) { return OSW_SHA256_INVALID; }
    if(sha_overlap(w,sizeof(*w),digest_out,sizeof(*digest_out))) { return OSW_SHA256_INVALID; }
    if(source_bytes>D4VR_OSW_SHA256_MAX_BYTES||source_bytes>SIZE_MAX||source_bytes>UINT64_MAX/8) { return OSW_SHA256_LIMIT; }
    /* NULL+0 never reaches source arithmetic, memcpy or dereference. */
    if(source_bytes) {
        if(!sha_span(source,source_bytes,1)) { return OSW_SHA256_INVALID; }
        if(sha_overlap(source,source_bytes,w,sizeof(*w))||sha_overlap(source,source_bytes,digest_out,sizeof(*digest_out))) { return OSW_SHA256_INVALID; }
    }
    memset(w,0,sizeof(*w));
    w->state.phase=1;w->state.original_bytes=source_bytes;
    w->driver.source=source;w->driver.source_bytes=source_bytes;
    for(I=0;I<8;I++) { w->state.h[I]=sha_iv[I]; }
    while(w->driver.consumed<w->driver.source_bytes) {
        if(w->state.used>=64) { w->state.phase=3;w->driver.status=OSW_SHA256_INTERNAL;return OSW_SHA256_INTERNAL; }
        w->state.block[w->state.used++]=w->driver.source[w->driver.consumed++];
        if(w->state.used==64) { sha_block(w);w->state.used=0; }
    }
    if(w->state.used>=64) { w->state.phase=3;w->driver.status=OSW_SHA256_INTERNAL;return OSW_SHA256_INTERNAL; }
    w->state.block[w->state.used++]=0x80;
    if(w->state.used>56) {
        while(w->state.used<64) { w->state.block[w->state.used++]=0; }
        sha_block(w);w->state.used=0;
    }
    while(w->state.used<56) { w->state.block[w->state.used++]=0; }
    for(I=0;I<8;I++) { w->state.block[63-I]=(uint8_t)((w->state.original_bytes*8)>>(8*I)); }
    sha_block(w);
    w->state.phase=2;
    for(I=0;I<32;I++) { digest_out->digest[I]=(uint8_t)(w->state.h[I/4]>>(24-8*(I%4))); }
    return OSW_SHA256_OK;
}
#undef X
#undef V
#undef I
