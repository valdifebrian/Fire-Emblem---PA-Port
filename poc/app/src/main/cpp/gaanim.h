/* gaanim.h — FE9 .ga animation loader + evaluator, exact port of ga.mjs.
 * Header 0x30: footer u32@0, gameFlag u8@8, fmtFlags@0xc, loop@0x10,
 * startF@0x14, endF@0x18, boneCount@0x1c, boneOff@0x20(==0x30),
 * curveOff@0x24, 0, 0, keyOff@0x2c. Bone rec 16B: idx,mask,first,n.
 * Curve rec 12B: channel u16, fb<<8 u16, endFrame u16, keyCount u16,
 * firstKey u32. Key 4B: u16 frame, i16 val / 2^fb. FE9 footer: 9 slot ptrs,
 * slot0 = events: u16 cnt, u16 offsets[], key: u16 frame, u16 cnt, u16 codes[].
 * Evaluate(frame): per anim bone vals=copy(vals); vals[channel]=lerp(keys);
 * local=buildLocal(flags | (mask & 0x38), vals); unanimated bones: seed. */
#ifndef GAANIM_H_
#define GAANIM_H_
#include <stdint.h>
#include <string.h>
#include "linalg.h"
#include "gload.h"

#define GA_MAX_BONES 256
#define GA_MAX_CURVES 1024
#define GA_MAX_KEYS 8192

typedef struct { int boneIdx; uint32_t mask; int firstCurve, nCurves; } ga_bone;
typedef struct { int channel; int fb; int frame; float v; } ga_key;
typedef struct { int nkeys; const ga_key* keys; } ga_curve;
typedef struct { int frame; int ncodes; uint16_t codes[8]; } ga_event;

typedef struct {
  int loop, startF, endF;
  int nBones; ga_bone bones[GA_MAX_BONES];
  int nCurves; ga_curve curves[GA_MAX_CURVES];
  int nKeys; ga_key keys[GA_MAX_KEYS];
  int nEvents; ga_event events[64];
} ga_anim;

static int ga_load(ga_anim* a, const uint8_t* d, int size) {
  memset(a, 0, sizeof *a);
  uint32_t footerOff = be_u32(d + 0), startF = be_u32(d + 0x14), endF = be_u32(d + 0x18);
  a->loop = be_u32(d + 0x10) != 0; a->startF = (int)startF; a->endF = (int)endF;
  uint32_t boneOff = be_u32(d + 0x20), curveOff = be_u32(d + 0x24), keyOff = be_u32(d + 0x2c);
  if (boneOff != 0x30) return -1;
  uint32_t nB = be_u32(d + 0x1c);
  if (nB > GA_MAX_BONES) return -2;
  a->nBones = (int)nB;
  for (uint32_t i = 0; i < nB; i++) {
    a->bones[i].boneIdx = (int)be_u32(d + boneOff + i * 16 + 0);
    a->bones[i].mask = be_u32(d + boneOff + i * 16 + 4);
    a->bones[i].firstCurve = (int)be_u32(d + boneOff + i * 16 + 8);
    a->bones[i].nCurves = (int)be_u32(d + boneOff + i * 16 + 12);
  }
  int nC = (int)((keyOff - curveOff) / 12);
  if (nC > GA_MAX_CURVES) nC = GA_MAX_CURVES;
  a->nCurves = nC;
  int kn = 0;
  for (int c = 0; c < nC; c++) {
    uint32_t q = curveOff + (uint32_t)c * 12;
    int channel = be_u16(d + q), fb = be_u16(d + q + 2) >> 8, kc = be_u16(d + q + 6);
    uint32_t fk = be_u32(d + q + 8);
    ga_curve* cv = &a->curves[c];
    cv->keys = &a->keys[kn];
    for (int k = 0; k < kc && kn < GA_MAX_KEYS; k++) {
      uint32_t o = fk + (uint32_t)k * 4;
      if ((int)o > size - 4) break;
      a->keys[kn + k].channel = channel;
      a->keys[kn + k].frame = be_u16(d + o);
      a->keys[kn + k].v = (float)(int16_t)be_u16(d + o + 2) / (float)(1 << fb);
    }
    cv->nkeys = kc < GA_MAX_KEYS - kn ? kc : GA_MAX_KEYS - kn;
    kn += cv->nkeys;
  }
  a->nKeys = kn;
  /* footer events (slot 0) */
  if (footerOff && footerOff + 36 <= (uint32_t)size) {
    uint32_t ev0 = be_u32(d + footerOff);
    if (ev0) {
      int kc = be_u16(d + ev0);
      for (int i = 0; i < kc && a->nEvents < 64; i++) {
        uint32_t ko = ev0 + be_u16(d + ev0 + 2 + i * 2);
        ga_event* e = &a->events[a->nEvents++];
        e->frame = be_u16(d + ko);
        int cc = be_u16(d + ko + 2);
        if (cc > 8) cc = 8;
        e->ncodes = cc;
        for (int j = 0; j < cc; j++) e->codes[j] = be_u16(d + ko + 4 + j * 2);
      }
    }
  }
  return 0;
}

/* evaluated local matrices for frame: out[nbones]; bones not in ga use seed. */
static void ga_eval(const ga_anim* a, const g_model* g, int frame, mat34* local) {
  int used[G_MAX_BONES]; memset(used, 0, sizeof used);
  /* seed for all */
  for (int i = 0; i < g->n; i++) memcpy(local[i].m, g->bones[i].seed, sizeof local[i].m);
  for (int bi = 0; bi < a->nBones; bi++) {
    const ga_bone* ab = &a->bones[bi];
    if (ab->boneIdx < 0 || ab->boneIdx >= g->n) continue;
    float vals[43];
    memcpy(vals, g->bones[ab->boneIdx].vals, sizeof vals);
    for (int ci = 0; ci < ab->nCurves; ci++) {
      const ga_curve* cv = &a->curves[ab->firstCurve + ci];
      if (cv->nkeys <= 0) continue;
      /* linear interp on frame; clamp ends */
      const ga_key* k0 = &cv->keys[0], *k1 = &cv->keys[cv->nkeys - 1];
      for (int kk = 0; kk + 1 < cv->nkeys; kk++)
        if (frame >= cv->keys[kk].frame && frame <= cv->keys[kk + 1].frame) { k0 = &cv->keys[kk]; k1 = &cv->keys[kk + 1]; break; }
      float val;
      if (frame <= k0->frame || k1 == k0 || k1->frame <= k0->frame) val = k0->v;
      else if (frame >= k1->frame) val = k1->v;
      else {
        float t = (float)(frame - k0->frame) / (float)(k1->frame - k0->frame);
        val = k0->v + (k1->v - k0->v) * t;
      }
      if (cv->keys[0].channel >= 0 && cv->keys[0].channel < 43) vals[cv->keys[0].channel] = val;
    }
    local[ab->boneIdx] = g_buildLocal(g->bones[ab->boneIdx].flags | (ab->mask & 0x38), vals);
    used[ab->boneIdx] = 1;
  }
  (void)used;
}
#endif
