/* gload.h — FE9 skeleton .g loader, exact port of g.mjs (skeleton v2.0.0).
 * Record 0xF4: i32 parent,sib,child; u32 flags; f32 invBind[12]@0x10;
 * f32 vals[43]@0x40; f32 seed[12]@0xBC (== buildLocal(flags,vals));
 * u16 boneIndex,rotOrder; u32 nameOff. Header: [?][boneCount]... arrOff=0x10,
 * poolOff=0x10+n*0xF4. */
#ifndef GLOAD_H_
#define GLOAD_H_
#include <stdint.h>
#include "linalg.h"

#define G_MAX_BONES 256

typedef struct {
  int32_t parent, sib, child;
  uint32_t flags;
  float invBind[12]; /* 3x4 row-major, @0x10 */
  float vals[43];
  float seed[12];
  uint16_t boneIndex, rotOrder;
  const char* name; /* into mapped data */
} g_bone;

typedef struct {
  const uint8_t* d;
  int n;
  g_bone bones[G_MAX_BONES];
} g_model;

static int be_i32(const uint8_t* p) { return (int32_t)((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]); }
static uint32_t be_u32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint16_t be_u16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
static float be_f32(const uint8_t* p) { uint32_t v = be_u32(p); float f; __builtin_memcpy(&f, &v, 4); return f; }

static int g_load(g_model* g, const uint8_t* data, int size) {
  (void)size;
  memset(g, 0, sizeof *g);
  g->d = data;
  uint32_t poolOff = be_u32(data + 4), n = be_u32(data + 8), arrOff = be_u32(data + 12);
  if (n <= 0 || n > G_MAX_BONES) return -1;
  if (arrOff != 0x10 || poolOff != 0x10 + n * 0xF4) return -2;
  g->n = (int)n;
  for (uint32_t i = 0; i < n; i++) {
    int b = 0x10 + (int)i * 0xF4;
    g_bone* B = &g->bones[i];
    B->parent = be_i32(data + b); B->sib = be_i32(data + b + 4); B->child = be_i32(data + b + 8);
    B->flags = be_u32(data + b + 0x0c);
    for (int j = 0; j < 12; j++) B->invBind[j] = be_f32(data + b + 0x10 + j * 4);
    for (int j = 0; j < 43; j++) B->vals[j] = be_f32(data + b + 0x40 + j * 4);
    for (int j = 0; j < 12; j++) B->seed[j] = be_f32(data + b + 0xbc + j * 4);
    B->boneIndex = be_u16(data + b + 0xec); B->rotOrder = be_u16(data + b + 0xee);
    const char* nm = (const char*)(data + be_u32(data + b + 0xf0));
    while (*nm) nm++; /* ensure NUL-terminated pool (absolute offset) */
    B->name = (const char*)(data + be_u32(data + b + 0xf0));
  }
  return 0;
}

/* buildLocal port (g.mjs): gates bit5 translate, bit4 rotate, bit3 scale,
 * bit6 JO, bit7 pivot, bit9 off1, bit10 off2, bit11 rotAxis. */
static mat34 g_buildLocal(uint32_t flags, const float* v) {
  mat34 m = M34_ID;
  if (flags & 0x020 || flags & 0x200) {
    float ox = (flags & 0x200) ? v[18] : 0, oy = (flags & 0x200) ? v[19] : 0, oz = (flags & 0x200) ? v[20] : 0;
    float tx = ((flags & 0x020) ? v[6] : 0) + ox, ty = ((flags & 0x020) ? v[7] : 0) + oy, tz = ((flags & 0x020) ? v[8] : 0) + oz;
    if (tx || ty || tz) m = m34mul(m, m34t(tx, ty, tz));
  }
  float p0 = (flags & 0x080) ? v[12] : 0, p1 = (flags & 0x080) ? v[13] : 0, p2 = (flags & 0x080) ? v[14] : 0;
  int haveR = 0; mat34 R = M34_ID;
  if (flags & 0x040) { R = m34mul(R, m34euler(v[9], v[10], v[11])); haveR = 1; }
  if (flags & 0x010) { R = m34mul(R, m34euler(v[3], v[4], v[5])); haveR = 1; }
  if (flags & 0x800) { R = m34mul(R, m34euler(v[24], v[25], v[26])); haveR = 1; }
  int haveS = (flags & 0x008) != 0;
  mat34 S = haveS ? m34s(v[0], v[1], v[2]) : M34_ID;
  if (haveR || haveS) {
    m = m34mul(m, m34t(p0, p1, p2));
    if (haveR) m = m34mul(m, R);
    if (haveS) m = m34mul(m, S);
    m = m34mul(m, m34t(-p0, -p1, -p2));
  }
  if (flags & 0x400) m = m34mul(m, m34t(v[21], v[22], v[23]));
  return m;
}

/* world = parent.world * local; roots (parent<0) = local. */
static void g_worlds(const g_model* g, const mat34* local, mat34* world) {
  for (int i = 0; i < g->n; i++) {
    int par = g->bones[i].parent;
    world[i] = (par >= 0 && par < i) ? m34mul(world[par], local[i]) : local[i];
  }
}
#endif
