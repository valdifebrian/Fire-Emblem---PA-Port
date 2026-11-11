/* texfile.h — LZ77(0x10) + TPL(0x20AF30) + .cms container decode → RGBA.
 * Ports lz77.mjs + tpl2png.mjs parseTpl (byte-verified 2026-11-10). */
#ifndef TEXFILE_H_
#define TEXFILE_H_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cmsdec.h"
#include "gload.h" /* be_u32/be_u16 helpers */

typedef struct { int w, h, fmt, ok; uint8_t* rgba; } tex_img; /* rgba malloc'd */

/* LZ77-0x10: decompress whole stream; returns malloc'd out + *outSize. */
static uint8_t* lz77_0x10(const uint8_t* buf, int size, int* outSize) {
  if (!size || buf[0] != 0x10) return 0;
  int cap = (int)(be_u32(buf) & 0xffffff); if (cap < 64) cap = 64;
  uint8_t* ob = (uint8_t*)malloc((size_t)cap);
  int p = 4, o = 0, flags = 0, bit = 8;
  while (p < size) {
    if (bit == 8) { if (p >= size) break; flags = buf[p++]; bit = 0; }
    int set = (flags >> (7 - bit++)) & 1;
    if (!set) {
      if (o + 1 > cap) { cap = cap ? cap * 2 : 1024; ob = (uint8_t*)realloc(ob, (size_t)cap); }
      if (p >= size) break;
      ob[o++] = buf[p++];
    } else {
      if (p + 2 > size) break;
      int b0 = buf[p++], b1 = buf[p++];
      int off = (((b0 & 0x0f) << 8) | b1) + 1;
      int len = (b0 >> 4) + 3;
      if (off > o) { free(ob); return 0; }
      if (o + len > cap) { while (o + len > cap) cap *= 2; ob = (uint8_t*)realloc(ob, (size_t)cap); }
      int src = o - off;
      for (int i = 0; i < len; i++) ob[o++] = ob[src + i];
    }
  }
  *outSize = o;
  return ob;
}

#define TPL_MAX_IMGS 64
typedef struct { int count; tex_img imgs[TPL_MAX_IMGS]; } tpl_file;

static void tpl_free(tpl_file* t) { for (int i = 0; i < t->count; i++) free(t->imgs[i].rgba); t->count = 0; }

/* decode all images in a TPL blob. pal formats: 0 IA8,1 RGB565,2 RGB5A3. */
static int tpl_decode(tpl_file* t, const uint8_t* d) {
  memset(t, 0, sizeof *t);
  if (be_u32(d) != 0x0020af30) return -1;
  int count = (int)be_u32(d + 4);
  uint32_t tableOff = be_u32(d + 8);
  if (count > TPL_MAX_IMGS) count = TPL_MAX_IMGS;
  t->count = count;
  for (int i = 0; i < count; i++) {
    uint32_t ip = be_u32(d + tableOff + (uint32_t)i * 8);
    uint32_t pp = be_u32(d + tableOff + (uint32_t)i * 8 + 4);
    int w = be_u16(d + ip), h = be_u16(d + ip + 2);
    uint32_t fmt = be_u32(d + ip + 4), dp = be_u32(d + ip + 8);
    tex_img* im = &t->imgs[i];
    im->w = w; im->h = h; im->fmt = (int)fmt;
    uint8_t pal[256 * 4]; memset(pal, 0, sizeof pal);
    if (pp != 0) {
      int pn = be_u16(d + pp); int pf = (int)be_u32(d + pp + 4); uint32_t pdp = be_u32(d + pp + 8);
      if (pn > 256) pn = 256;
      for (int j = 0; j < pn; j++) palFmt(pf, be_u16(d + pdp + (uint32_t)j * 2), pal + j * 4);
    }
    uint8_t* rgba = (uint8_t*)malloc((size_t)w * h * 4);
    if (!rgba) return -1;
    if (fe9tex_decode((int)fmt, w, h, d + dp, (pp != 0) ? pal : NULL, rgba) == 0) { im->rgba = rgba; im->ok = 1; }
    else free(rgba);
  }
  return 0;
}

/* .cms/.cmp/.tpl: if magic 0x10 → lz77 then tpl; if 0x20af30 → direct. */
static int texfile_decode(const uint8_t* data, int size, tpl_file* out) {
  if (size >= 4 && be_u32(data) == 0x0020af30) return tpl_decode(out, data);
  int osz = 0;
  uint8_t* raw = lz77_0x10(data, size, &osz);
  if (!raw) return -1;
  int r = tpl_decode(out, raw);
  free(raw);
  return r;
}
#endif
