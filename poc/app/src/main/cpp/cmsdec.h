/* cmsdec.h — FE9 texture decode ported 1:1 from validated bin/tpl2png.mjs
 * (BRV-GFX-001). Implements the block-linear GX layout used by .cms/.tpl.
 * Formats used by PoC: 4 = RGB565 (block 4x4, BE), 3 = IA8 (block 4x4),
 * 6 = RGBA8 (block 4x4). One TU with CMSDEC_IMPL. */
#ifndef CMSDEC_H_
#define CMSDEC_H_
#include <stdint.h>

/* fmt: 3|4|6. w,h pixel dims; src = decoded block-linear stream (size
 * ceil(w/bw)*bw*ceil(h/bh)*bh*bpp bytes); dst = w*h*4 RGBA. 0 on ok. */
static int fe9tex_decode(int fmt, int w, int h, const uint8_t* src, uint8_t* dst);

#ifdef CMSDEC_IMPL
#include <string.h>

static int blk_dims(int fmt, int* bw, int* bh) {
  switch (fmt) { case 3: case 4: case 6: *bw = 4; *bh = 4; return 0; default: return -1; }
}

static uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }

/* block-linear byte offset for pixel (x,y); size = bytes per pixel unit.
   mirror of tpl2png blockLinear with alignment dims aw,ah */
static long blockLinear(int x, int y, int size, int aw, int bw, int bh) {
  int bpr = aw / bw;
  int bl = x / bw + (y / bh) * bpr;
  return (long)(bl * bw * bh + (y % bh) * bw + (x % bw)) * size;
}

static int fe9tex_decode(int fmt, int w, int h, const uint8_t* src, uint8_t* dst) {
  int bw, bh; if (blk_dims(fmt, &bw, &bh)) return -1;
  int size = (fmt == 6) ? 4 : 2;
  int aw = ((w + bw - 1) / bw) * bw, ah = ((h + bh - 1) / bh) * bh;
  for (int y = 0; y < ah; y++) for (int x = 0; x < aw; x++) {
    if (x >= w || y >= h) continue;
    long o = blockLinear(x, y, size, aw, bw, bh);
    uint8_t *d = dst + ((size_t)y * w + x) * 4;
    switch (fmt) {
      case 4: { uint16_t v = be16(src + o);
        d[0] = (uint8_t)((v >> 11) * 255 / 31);
        d[1] = (uint8_t)((((v >> 5) & 63) * 255) / 63);
        d[2] = (uint8_t)((v & 31) * 255 / 31);
        d[3] = 255; break; }
      case 3: { uint16_t b = be16(src + o); d[0] = d[1] = d[2] = (uint8_t)(b >> 8); d[3] = (uint8_t)(b & 255); break; }
      case 6: { d[0] = src[o]; d[1] = src[o + 1]; d[2] = src[o + 2]; d[3] = src[o + 3]; break; }
      default: return -1;
    }
  }
  return 0;
}
#endif /* CMSDEC_IMPL */
#endif /* CMSDEC_H_ */
