/* ui.h — P3-M1.6 [P3-UI-001]: GCF text font + glyph quad batch + word-wrap.
 * ui_font_build ports the byte-verified gcf.mjs [BRV-FNT-001] parser: I4 glyph
 * sheets rasterized ONCE into an RGBA atlas (white ink, I4*17 alpha) consumed
 * by the shared UI quad batch. Text layout is FE9-faithful: per-glyph advance
 * (adv/dx/dy from metrics), space-break word wrap, line height from the font
 * header. Quads are batched in screen px (y-down) with normalized uv. */
#ifndef UI_H_
#define UI_H_
#include <GLES3/gl3.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gload.h" /* be_u16 / be_u32 */

#define UI_CP0 32
#define UI_CPN 95 /* 32..126 */
#define UI_ATLAS 512
#define UI_MAXQ 1536

typedef struct { int16_t w, h, dx, dy; float adv; float u0, v0, u1, v1; } UiGlyph;
typedef struct {
  GLuint tex;
  UiGlyph g[UI_CPN];
  int has[UI_CPN];
  int lineH, ascent;
  int ok;
} UiFont;

typedef struct { float v[UI_MAXQ * 6 * 4]; int n; /* floats written */ } UiBatch;

static void ui_batch_reset(UiBatch* b) { b->n = 0; }

static void ui_quad(UiBatch* b, float x0, float y0, float x1, float y1,
                    float u0, float v0, float u1, float v1) {
  if (b->n + 24 > UI_MAXQ * 24) return; /* batch full: drop (telemetry elsewhere) */
  float* p = b->v + b->n;
  p[0]=x0; p[1]=y0; p[2]=u0; p[3]=v0;
  p[4]=x1; p[5]=y0; p[6]=u1; p[7]=v0;
  p[8]=x1; p[9]=y1; p[10]=u1; p[11]=v1;
  p[12]=x0; p[13]=y0; p[14]=u0; p[15]=v0;
  p[16]=x1; p[17]=y1; p[18]=u1; p[19]=v1;
  p[20]=x0; p[21]=y1; p[22]=u0; p[23]=v1;
  b->n += 24;
}

/* Solid-color quad via uv=0,0 — bind a 1x1 white tex, tint with uColor. */
static void ui_fill(UiBatch* b, float x0, float y0, float x1, float y1) {
  ui_quad(b, x0, y0, x1, y1, 0.001f, 0.001f, 0.002f, 0.002f);
}

/* rasterize usable ASCII glyphs of a .gcf into a GL atlas (GL context must be
 * current); F->tex is created here. returns 1 on success */
static int ui_font_build(UiFont* F, const uint8_t* b, int size) {
  static uint8_t atlas[UI_ATLAS * UI_ATLAS * 4];
  memset(F, 0, sizeof *F);
  memset(atlas, 0, sizeof atlas);
  if (size < 0x30 || be_u32(b) != 0x20) return 0;
  int sheetBytes = (int)be_u32(b + 0x14), sheetCount = (int)be_u32(b + 0x18),
      texW = (int)be_u32(b + 0x1c), metricsOff = (int)be_u16(b + 0x22),
      metricsEnd = (int)be_u32(b + 0x24);
  if (!sheetBytes || !sheetCount || texW < 8 || metricsEnd <= metricsOff ||
      metricsEnd + (int64_t)sheetCount * sheetBytes > size)
    return 0;
  F->ascent = (int)be_u16(b + 0x08);
  F->lineH = (int)be_u16(b + 0x0c);
  int W8 = texW / 8;
  int cx = 1, cy = 1, rowH = 1, built = 0;
  for (int o = metricsOff; o + 16 <= metricsEnd; o += 16) {
    int code = (int)be_u16(b + o);
    if (code < UI_CP0 || code >= UI_CP0 + UI_CPN) continue;
    int sheet = (int)be_u16(b + o + 2), x = (int)be_u16(b + o + 4), y = (int)be_u16(b + o + 6);
    int w = b[o + 8], h = b[o + 9], dx = (int8_t)b[o + 10], dy = b[o + 11], adv = b[o + 12];
    if (!w || !h || w > 64 || h > 64 || sheet >= sheetCount) continue; /* junk/placeholder */
    if (cx + w + 1 >= UI_ATLAS) { cx = 1; cy += rowH + 1; rowH = 1; }
    if (cy + h + 1 >= UI_ATLAS) break; /* atlas full (95 glyphs fit 512 easily) */
    int gi = code - UI_CP0;
    int base = metricsEnd + sheet * sheetBytes;
    for (int py = 0; py < h; py++)
      for (int px = 0; px < w; px++) {
        int sx = x + px, sy = y + py;
        int off = base + ((sy >> 3) * W8 + (sx >> 3)) * 32 + (sy & 7) * 4 + ((sx >> 1) & 3);
        if (off >= size) continue;
        uint8_t byt = b[off];
        int nib = (sx & 1) ? (byt & 0xf) : (byt >> 4);
        uint8_t* d = atlas + ((size_t)(cy + py) * UI_ATLAS + (cx + px)) * 4;
        d[0] = d[1] = d[2] = 255;
        d[3] = (uint8_t)(nib * 17);
      }
    UiGlyph* G = &F->g[gi];
    G->w = (int16_t)w; G->h = (int16_t)h; G->dx = (int16_t)dx; G->dy = (int16_t)dy;
    G->adv = (float)adv;
    G->u0 = (float)cx / UI_ATLAS; G->v0 = (float)cy / UI_ATLAS;
    G->u1 = (float)(cx + w) / UI_ATLAS; G->v1 = (float)(cy + h) / UI_ATLAS;
    F->has[gi] = 1;
    cx += w + 1;
    if (h + 1 > rowH) rowH = h + 1;
    built++;
  }
  glGenTextures(1, &F->tex);
  glBindTexture(GL_TEXTURE_2D, F->tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, UI_ATLAS, UI_ATLAS, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  F->ok = built > 32;
  return F->ok;
}

static float ui_text_w(const UiFont* F, const char* s, float sc) {
  float w = 0;
  for (; *s; s++) {
    int c = (uint8_t)*s - UI_CP0;
    if (c < 0 || c >= UI_CPN) continue;
    w += F->g[c].adv * sc;
  }
  return w;
}

/* Word-wrapped text as quads; pen (x,y) = top-left of first line baseline box.
 * `visible` limits chars drawn (typewriter). Returns lines used (<= maxLines
 * clamped — extra lines dropped, caller shows page arrow). */
static int ui_text(UiBatch* B, const UiFont* F, float x, float y, float sc,
                   const char* s, float maxW, int maxLines, int visible) {
  int line = 0, seen = 0, stop = 0;
  float penY = y;
  const char* start = s;
  while (*start && line < maxLines && !stop) {
    const char* e = start;
    const char* sp = 0;
    float w = 0;
    int brk = -1;
    while (*e && *e != '\n') {
      float cw = 0;
      int c = (uint8_t)*e;
      if (c >= UI_CP0 && c < UI_CP0 + UI_CPN) cw = F->g[c - UI_CP0].adv * sc;
      if (*e == ' ') sp = e;
      if (w + cw > maxW && e != start) brk = (sp && sp != start) ? (int)(sp - start) : (int)(e - start);
      if (brk >= 0) break;
      w += cw;
      e++;
    }
    int len = brk >= 0 ? brk : (int)(e - start);
    if (len <= 0 && *e) len = 1; /* pathological long word: hard-break */
    float px = x;
    for (int i = 0; i < len; i++) {
      if (seen++ >= visible) { stop = 1; break; }
      char ch = start[i];
      int c = (uint8_t)ch - UI_CP0;
      if (c < 0 || c >= UI_CPN) continue;
      if (F->has[c] && F->g[c].w > 0) {
        const UiGlyph* G = &F->g[c];
        /* dy = glyph-top distance ABOVE the baseline (verified against talk.gcf
         * metrics: A=17/h17 cap sits on baseline, '.'=3/h3, '''=16/h6 high) */
        ui_quad(B, px + G->dx * sc, penY + (F->ascent - G->dy) * sc,
                px + (G->dx + G->w) * sc, penY + (F->ascent - G->dy + G->h) * sc,
                G->u0, G->v0, G->u1, G->v1);
      }
      px += F->g[c].adv * sc;
    }
    line++;
    penY += F->lineH * sc;
    if (brk >= 0) {
      start += len;
      if (*start == ' ') start++;
    } else {
      start = e;
      if (*start == '\n') start++;
    }
  }
  return line;
}
#endif /* UI_H_ */
