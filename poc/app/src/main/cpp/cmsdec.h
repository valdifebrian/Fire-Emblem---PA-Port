/* cmsdec.h — GX TPL texture decode -> RGBA8888. Exact port of tpl2png.mjs
 * decodeFmt (formats 0,1,2,3,4,5,6,8,9,14; block-linear addressing; CMPR
 * proven 2026-11-10 smoothness metric). */
#ifndef CMSDEC_H_
#define CMSDEC_H_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CMSDEC_IMPL
#ifdef CMSDEC_IMPL
static const int kBlkW[15] = {8, 8, 8, 4, 4, 4, 4, 0, 8, 8, 4, 0, 0, 0, 8};
static const int kBlkH[15] = {8, 4, 4, 4, 4, 4, 4, 0, 8, 4, 4, 0, 0, 0, 8};

static int be16(const uint8_t* p) { return (p[0] << 8) | p[1]; }

/* GX scale mul(x,n,m) = x*n/(m-1) truncated */
static void r565(int v, uint8_t o[4]) {
  o[0] = (uint8_t)((v >> 11 & 31) * 255 / 31);
  o[1] = (uint8_t)((v >> 5 & 63) * 255 / 63);
  o[2] = (uint8_t)((v & 31) * 255 / 31);
  o[3] = 255;
}
static void r5a3(int v, uint8_t o[4]) {
  if (v & 0x8000) {
    o[0] = (uint8_t)(((v >> 10) & 31) << 3);
    o[1] = (uint8_t)(((v >> 7) & 7) * 255 / 7);
    o[2] = (uint8_t)((v & 15) * 255 / 15);
    o[3] = 255;
  } else {
    o[0] = (uint8_t)(((v >> 8) & 15) * 255 / 15);
    o[1] = (uint8_t)(((v >> 4) & 15) * 255 / 15);
    o[2] = (uint8_t)((v & 15) * 255 / 15);
    o[3] = (uint8_t)(((v >> 12) & 7) * 255 / 7);
  }
}
static void palFmt(int f, int v, uint8_t o[4]) {
  if (f == 0) { o[0] = o[1] = o[2] = (uint8_t)((v >> 8) & 255); o[3] = (uint8_t)(v & 255); }
  else if (f == 1) r565(v, o);
  else r5a3(v, o);
}
static uint32_t blockLinear(int x, int y, int aw, int bw, int bh, int size) {
  int bpr = aw / bw;
  uint32_t bl = (uint32_t)(x / bw) + (uint32_t)(y / bh) * bpr;
  return (bl * bw * bh + (y % bh) * bw + (x % bw)) * (uint32_t)size;
}
static uint32_t addr4(int x, int y, int aw, int bw, int bh, int* lo) {
  int bpr = aw / bw;
  uint32_t bl = (uint32_t)(x / bw) + (uint32_t)(y / bh) * bpr;
  *lo = (x & 1) == 1;
  return bl * 32 + (y % bh) * 4 + ((x % bw) >> 1);
}

/* pal: RGBA8888 array (256 entries) or NULL. Returns 0 ok, -1 unsupported fmt. */
static int fe9tex_decode(int fmt, int w, int h, const uint8_t* src, const uint8_t* pal, uint8_t* dst) {
  if (fmt < 0 || fmt > 14 || !kBlkH[fmt]) return -1;
  int bw = kBlkW[fmt], bh = kBlkH[fmt];
  int aw = ((w + bw - 1) / bw) * bw, ah = ((h + bh - 1) / bh) * bh;
  memset(dst, 0, (size_t)w * h * 4);
  for (int y = 0; y < ah; y++) for (int x = 0; x < aw; x++) {
    if (x >= w || y >= h) continue;
    uint8_t c[4] = {0, 0, 0, 0};
    int lo = 0; uint32_t o;
    switch (fmt) {
      case 0: { /* I4 */
        o = addr4(x, y, aw, bw, bh, &lo);
        int b = src[o]; int v = (lo ? (b & 15) : (b >> 4)) * 17;
        c[0] = c[1] = c[2] = (uint8_t)v; c[3] = 255; break; }
      case 1: { o = blockLinear(x, y, aw, bw, bh, 1); c[0] = c[1] = c[2] = src[o]; c[3] = 255; break; }
      case 2: { o = blockLinear(x, y, aw, bw, bh, 1); int b = src[o];
        c[0] = c[1] = c[2] = (uint8_t)((b >> 4) * 17); c[3] = (uint8_t)((b & 15) * 17); break; }
      case 3: { o = blockLinear(x, y, aw, bw, bh, 2); uint16_t b = (uint16_t)be16(src + o);
        c[0] = c[1] = c[2] = (uint8_t)(b >> 8); c[3] = (uint8_t)(b & 255); break; }
      case 4: { o = blockLinear(x, y, aw, bw, bh, 2); r565(be16(src + o), c); break; }
      case 5: { o = blockLinear(x, y, aw, bw, bh, 2); r5a3(be16(src + o), c); break; }
      case 6: { o = blockLinear(x, y, aw, bw, bh, 4);
        c[0] = src[o]; c[1] = src[o + 1]; c[2] = src[o + 2]; c[3] = src[o + 3]; break; }
      case 8: case 9: {
        int idx;
        if (fmt == 9) { o = blockLinear(x, y, aw, bw, bh, 1); idx = src[o]; }
        else { o = addr4(x, y, aw, bw, bh, &lo); int b = src[o]; idx = lo ? (b & 15) : (b >> 4); }
        if (pal) { memcpy(c, pal + idx * 4, 4); }
        break; }
      case 14: { /* CMPR / DXT1-GX, 8x8 tile, colors BIG-endian */
        int bpr = aw / 8;
        uint32_t bl = (uint32_t)(x / 8) + (uint32_t)(y / 8) * bpr;
        int sub = ((y & 7) >> 2) * 2 + ((x & 7) >> 2);
        uint32_t q = bl * 32 + sub * 8;
        int c0 = be16(src + q), c1 = be16(src + q + 2);
        int idx = (src[q + 4 + (y & 3)] >> ((3 - (x & 3)) * 2)) & 3;
        uint8_t A[4], B[4];
        r565(c0, A); r565(c1, B);
        if (idx == 0) memcpy(c, A, 4);
        else if (idx == 1) memcpy(c, B, 4);
        else if (c0 > c1) {
          const uint8_t* L = idx == 2 ? B : A; const uint8_t* S = idx == 2 ? A : B;
          for (int k = 0; k < 3; k++) c[k] = (uint8_t)((3 * L[k] + 5 * S[k]) >> 3);
          c[3] = 255;
        } else {
          for (int k = 0; k < 3; k++) c[k] = (uint8_t)((A[k] + B[k]) >> 1);
          c[3] = 255; /* FE9 idx3 opaque */
        }
        break; }
      default: break;
    }
    memcpy(dst + ((size_t)y * w + x) * 4, c, 4);
  }
  return 0;
}
#endif /* CMSDEC_IMPL */
#endif
