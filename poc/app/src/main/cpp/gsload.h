/* gsload.h — FE9 body .gs loader + displaylist decode + skinning, exact port
 * of gs.mjs + meshview.mjs (verified visually). See docs/FPK_FORMAT.md links.
 * Header 0x84: table ptrs (stored value +0x20 = real off) @0x44 pos, 0x48 nrm,
 * 0x4c uv, 0x54 mat, 0x5c/0x60/0x64 submesh chains; counts u16 @0x6c pos...;
 * frac bits u8 @0x7c pos, 0x7d nrm, 0x7e uv.
 * Submesh rec: u32 name, u32 next, u16 flags@8, u16 mat@0xa, u16 bone@0xc,
 * u8 attr@0x12, u32 dl@0x14, u32 dlSize@0x18, u32 pal@0x1c (+0x20 real).
 * DL: [u32 0x98][u16 n] indexed strip; vert [skin? u8][p u16][nrm u16]
 * [col u16?][u u16][v u16?]; skin -> palette(u8 cnt@+1, u8 bones@+2),
 * bone = palette[skin/3], M = world[bone] * invBind[bone] (meshview L68). */
#ifndef GSLOAD_H_
#define GSLOAD_H_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "linalg.h"
#include "gload.h"

typedef struct { uint16_t p, n, c, u, v2; uint8_t sk; } gs_vert;
typedef struct { gs_vert v[3]; } gs_tri;

typedef struct {
  gs_tri* tris; int triCount;
  int bone; uint16_t flags; uint8_t attr;
  const uint8_t* palette; /* NULL or u8 cnt@+1 + bones */
  const char* name;
} gs_submesh;

typedef struct {
  const uint8_t* d; int size;
  int npos;
  int fbPos, fbNrm, fbUv;
  const uint8_t* posT; /* raw BE i16 stream (n*6) */
  const uint8_t* nrmT; const uint8_t* uvT;
  gs_submesh subs[64]; int nsub;
} gs_body;

static uint32_t gs_be32(const uint8_t* p, uint32_t o) { return be_u32(p + o); }
static int16_t gs_bei16(const uint8_t* p, uint32_t o) { return (int16_t)be_u16(p + o); }

static int gs_decode_dl(gs_submesh* s, const uint8_t* p, uint32_t dl, uint32_t dlSize) {
  int cap = 256, cnt = 0;
  gs_tri* tris = (gs_tri*)malloc(sizeof(gs_tri) * (size_t)cap);
  if (!tris) return -1;
  uint32_t o = dl, end = dl + dlSize;
  int skin = s->flags & 0x0002, colA = s->attr & 0x10, uv2A = s->attr & 0x80;
  int stride = 2 * (3 + (colA ? 1 : 0) + (uv2A ? 1 : 0)) + (skin ? 1 : 0);
  gs_vert strip[1024]; int vn = 0, parity = 0;
  while (o + 3 <= end) {
    uint8_t op = p[o];
    if (op == 0) { o++; continue; } /* 32B pad zeros: always skipped (gs.mjs v[] never pushed) */
    if (op != 0x98) { free(tris); return -1; }
    int n = (p[o + 1] << 8) | p[o + 2]; o += 3;
    vn = 0; parity = 0;
    for (int i = 0; i < n; i++) {
      uint32_t q = o;
      gs_vert v; memset(&v, 0, sizeof v);
      if (skin) { v.sk = p[q]; q += 1; }
      v.p = be_u16(p + q); q += 2;
      v.n = be_u16(p + q); q += 2;
      if (colA) { v.c = be_u16(p + q); q += 2; }
      v.u = be_u16(p + q); q += 2;
      if (uv2A) { v.v2 = be_u16(p + q); q += 2; }
      if (vn < 1024) strip[vn++] = v;
      if (vn >= 3) {
        if (cnt >= cap) { cap *= 2; tris = (gs_tri*)realloc(tris, sizeof(gs_tri) * (size_t)cap); if (!tris) return -1; }
        gs_vert a = strip[vn - 3], b = strip[vn - 2], c = strip[vn - 1];
        gs_tri* t = &tris[cnt++];
        /* gs.mjs: i%2 ? [i-1,i,i-2] : [i-2,i-1,i] with i = strip position */
        if (parity++ & 1) { t->v[0] = b; t->v[1] = c; t->v[2] = a; }
        else { t->v[0] = a; t->v[1] = b; t->v[2] = c; }
      }
      o += stride;
    }
  }
  s->tris = tris; s->triCount = cnt;
  return 0;
}

static int gs_load(gs_body* b, const uint8_t* d, int size) {
  memset(b, 0, sizeof *b); b->d = d; b->size = size;
  const uint8_t* p = d;
#define PTR(o) (gs_be32(p, (o)) ? gs_be32(p, (o)) + 0x20 : 0)
  uint32_t oPos = PTR(0x44), oNrm = PTR(0x48), oUv = PTR(0x4c);
  uint32_t oList[3] = { PTR(0x5c), PTR(0x60), PTR(0x64) };
#undef PTR
  b->npos = be_u16(p + 0x6c);
  b->fbPos = p[0x7c]; b->fbNrm = p[0x7d]; b->fbUv = p[0x7e];
  b->posT = p + oPos; b->nrmT = p + oNrm; b->uvT = p + oUv;
  int nsub = 0;
  for (int li = 0; li < 3; li++) {
    uint32_t rec = oList[li];
    while (rec) {
      if (nsub >= 64) return -1;
      gs_submesh* s = &b->subs[nsub++];
      uint32_t nm = gs_be32(p, rec);
      s->name = nm ? (const char*)(p + nm + 0x20) : "";
      s->flags = be_u16(p + rec + 0x08);
      s->attr = p[rec + 0x12];
      s->bone = be_u16(p + rec + 0x0c);
      uint32_t pal = gs_be32(p, rec + 0x1c); s->palette = pal ? p + pal + 0x20 : 0;
      uint32_t dl = gs_be32(p, rec + 0x14), dlSize = gs_be32(p, rec + 0x18);
      if (dl && dlSize) { if (gs_decode_dl(s, p, dl + 0x20, dlSize) < 0) { if (!s->tris) s->triCount = 0; } }
      uint32_t nx = gs_be32(p, rec + 4);
      rec = nx ? nx + 0x20 : 0;
    }
  }
  b->nsub = nsub;
  return 0;
}

/* Skinned vertex: per-vertex bone via palette (flag 0x0002) else single bone.
 * Matches meshview L68: M = world[b] * invBind[b]; pos from i16/frac table. */
static void gs_vertex(const gs_body* b, const gs_submesh* s, gs_vert v,
                      const mat34* world, const g_model* g, float out[3], float nrm[3]) {
  float fx = (float)gs_bei16(b->posT, (uint32_t)v.p * 6 + 0) / (float)(1 << b->fbPos);
  float fy = (float)gs_bei16(b->posT, (uint32_t)v.p * 6 + 2) / (float)(1 << b->fbPos);
  float fz = (float)gs_bei16(b->posT, (uint32_t)v.p * 6 + 4) / (float)(1 << b->fbPos);
  int bone = s->bone;
  if ((s->flags & 0x0002) && s->palette) {
    int n = s->palette[1];
    int idx = v.sk / 3;
    if (idx >= n) idx = n ? n - 1 : 0;
    bone = s->palette[2 + idx];
  }
  if (bone < 0 || bone >= g->n) { out[0] = fx; out[1] = fy; out[2] = fz; nrm[0] = 0; nrm[1] = 1; nrm[2] = 0; return; }
  mat34 inv; memcpy(inv.m, g->bones[bone].invBind, sizeof inv.m);
  mat34 M = m34mul(world[bone], inv);
  m34xform(M, fx, fy, fz, out);
  int8_t nx = (int8_t)b->nrmT[v.n * 3 + 0], ny = (int8_t)b->nrmT[v.n * 3 + 1], nz = (int8_t)b->nrmT[v.n * 3 + 2];
  float nlx = (float)nx / (float)(1 << b->fbNrm), nly = (float)ny / (float)(1 << b->fbNrm), nlz = (float)nz / (float)(1 << b->fbNrm);
  mat34 rot = M; rot.m[3] = rot.m[7] = rot.m[11] = 0;
  m34xform(rot, nlx, nly, nlz, nrm);
}
static void gs_uv(const gs_body* b, gs_vert v, float out[2]) {
  out[0] = (float)gs_bei16(b->uvT, (uint32_t)v.u * 4 + 0) / (float)(1 << b->fbUv);
  out[1] = (float)gs_bei16(b->uvT, (uint32_t)v.u * 4 + 2) / (float)(1 << b->fbUv);
}
#endif
