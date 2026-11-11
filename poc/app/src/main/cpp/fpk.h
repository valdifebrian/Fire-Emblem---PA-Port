/* fpk.h — fpk1 bundle loader (docs/FPK_FORMAT.md). C20, no deps beyond libc.
 * Single .c file: #define FPK_IMPL before include in exactly one TU. */
#ifndef FPK_H_
#define FPK_H_
#include <stdint.h>
#include <stddef.h>

typedef struct {
  const char* name;    /* points into mapped file */
  uint64_t off;        /* payload offset from bundle start */
  uint32_t len;
  uint32_t fmt;
  uint32_t crc32;
} fpk_entry;

typedef struct {
  const uint8_t* data; /* whole-file image (malloc'd) */
  size_t size;
  uint32_t count;
  const fpk_entry* entries; /* points into data */
} fpk_file;

int fpk_open(fpk_file* f, const uint8_t* image, size_t size); /* 0 ok, <0 reason */
const fpk_entry* fpk_find(const fpk_file* f, const char* name);
uint32_t fpk_crc32(const uint8_t* p, size_t n);

#ifdef FPK_IMPL
#include <string.h>
#include <stdlib.h>

uint32_t fpk_crc32(const uint8_t* p, size_t n) {
  static uint32_t tab[256]; static int init = 0;
  if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1; tab[i] = c; } init = 1; }
  uint32_t c = 0xffffffffu;
  for (size_t i = 0; i < n; i++) c = tab[(c ^ p[i]) & 0xff] ^ (c >> 8);
  return c ^ 0xffffffffu;
}

/* minimal SHA-256 */
typedef struct { uint32_t h[8]; uint64_t len; uint8_t buf[64]; size_t nl; } sha256_ctx;
static const uint32_t K256[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))
static void sha256_block(sha256_ctx* s, const uint8_t* p) {
  uint32_t w[64];
  for (int i = 0; i < 16; i++) w[i] = ((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
  for (int i = 16; i < 64; i++) { uint32_t a=w[i-15],b=w[i-2]; uint32_t s0=ROR(a,7)^ROR(a,18)^(a>>3), s1=ROR(b,17)^ROR(b,19)^(b>>10); w[i]=w[i-16]+s0+w[i-7]+s1; }
  uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
  for (int i = 0; i < 64; i++) {
    uint32_t S1=ROR(e,6)^ROR(e,11)^ROR(e,25), ch=(e&f)^((~e)&g), t1=h+S1+ch+K256[i]+w[i];
    uint32_t S0=ROR(a,2)^ROR(a,13)^ROR(a,22), mj=(a&b)^(a&c)^(b&c), t2=S0+mj;
    h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
  }
  s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void sha256(sha256_ctx* s, const uint8_t* p, size_t n, uint8_t out[32]) {
  if (!s->nl && !s->len) { memcpy(s->h, (uint32_t[]){0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}, 32); }
  s->len += n;
  while (n) { size_t take = 64 - s->nl; if (take > n) take = n; memcpy(s->buf + s->nl, p, take); s->nl += take; p += take; n -= take;
    if (s->nl == 64) { sha256_block(s, s->buf); s->nl = 0; } }
  (void)0;
}
static void sha256_final(sha256_ctx* s, uint8_t out[32]) {
  uint64_t bits = s->len * 8; uint8_t z = 0x80;
  sha256(s, &z, 1, out); z = 0;
  while (s->nl % 64 != 56) sha256(s, &z, 1, out);
  uint8_t lb[8]; for (int i=0;i<8;i++) lb[i] = (uint8_t)(bits >> (56 - i*8));
  sha256(s, lb, 8, out);
  for (int i = 0; i < 8; i++) { out[i*4]=(uint8_t)(s->h[i]>>24); out[i*4+1]=(uint8_t)(s->h[i]>>16); out[i*4+2]=(uint8_t)(s->h[i]>>8); out[i*4+3]=(uint8_t)s->h[i]; }
}

int fpk_open(fpk_file* f, const uint8_t* image, size_t size) {
  memset(f, 0, sizeof(*f));
  if (size < 8 + 28 + 32 || memcmp(image, "fpk1", 4) != 0) return -1;
  uint32_t count; memcpy(&count, image + 4, 4);
  const uint8_t* p = image + 8;
  for (uint32_t i = 0; i < count; i++) {
    size_t namepad = 4;
    while (p[namepad - 1] != 0 && namepad < 4096) namepad += 4; /* C-string pad-4 */
    p += namepad + 20;
    if ((size_t)(p - image) > size) return -2;
  }
  /* payload start = p; verify sha256 over [4, payload_end) vs footer */
  size_t total = size;
  sha256_ctx s; memset(&s, 0, sizeof s);
  uint8_t digest[32];
  sha256(&s, image + 4, total - 32 - 4, digest);
  sha256_final(&s, digest);
  if (memcmp(digest, image + total - 32, 32) != 0) return -3;
  f->data = image; f->size = size; f->count = count; f->entries = (const fpk_entry*)(image + 8);
  /* entry iteration via namepad walk helper */
  return 0;
}

const fpk_entry* fpk_find(const fpk_file* f, const char* name) {
  static fpk_entry e; /* single static: find one-at-a-time use in PoC harness */
  const uint8_t* p = f->data + 8;
  for (uint32_t i = 0; i < f->count; i++) {
    const char* nm = (const char*)p;
    size_t namepad = 4;
    while (p[namepad - 1] != 0 && namepad < 4096) namepad += 4;
    const uint8_t* rec = p + namepad;
    if (strcmp(nm, name) == 0) {
      memcpy(&e.off, rec, 8); memcpy(&e.len, rec + 8, 4); memcpy(&e.fmt, rec + 12, 4); memcpy(&e.crc32, rec + 16, 4);
      e.name = nm;
      return &e;
    }
    p = rec + 20;
  }
  return 0;
}
#endif /* FPK_IMPL */
#endif /* FPK_H_ */
