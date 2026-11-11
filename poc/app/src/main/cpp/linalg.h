/* linalg.h — exact port of validated g.mjs conventions: row-major 3x4
 * (float[12]: [r0c0 r0c1 r0c2 tx | r1.. | r2..]), p' = M * p,
 * mEuler = Rz*Ry*Rx in DEGREES. Matches meshview-verified visuals. */
#ifndef LINALG_H_
#define LINALG_H_
#include <math.h>
#include <string.h>

#define DEG2RAD (3.14159265358979323846 / 180.0)

typedef struct { float m[12]; } mat34;

static const mat34 M34_ID = {{1,0,0,0, 0,1,0,0, 0,0,1,0}};

static mat34 m34mul(mat34 a, mat34 b) {
  mat34 o;
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++)
      o.m[r * 4 + c] = a.m[r * 4] * b.m[c] + a.m[r * 4 + 1] * b.m[4 + c] + a.m[r * 4 + 2] * b.m[8 + c];
    o.m[r * 4 + 3] = a.m[r * 4] * b.m[3] + a.m[r * 4 + 1] * b.m[7] + a.m[r * 4 + 2] * b.m[11] + a.m[r * 4 + 3];
  }
  return o;
}
static void m34xform(mat34 m, float x, float y, float z, float* o) {
  o[0] = m.m[0] * x + m.m[1] * y + m.m[2] * z + m.m[3];
  o[1] = m.m[4] * x + m.m[5] * y + m.m[6] * z + m.m[7];
  o[2] = m.m[8] * x + m.m[9] * y + m.m[10] * z + m.m[11];
}
static mat34 m34t(float x, float y, float z) { mat34 r = M34_ID; r.m[3] = x; r.m[7] = y; r.m[11] = z; return r; }
static mat34 m34s(float x, float y, float z) { mat34 r = M34_ID; r.m[0] = x; r.m[5] = y; r.m[10] = z; return r; }
static mat34 m34rx(float d) { float c = cosf(d), s = sinf(d); mat34 r = M34_ID; r.m[5] = c; r.m[6] = -s; r.m[9] = s; r.m[10] = c; return r; }
static mat34 m34ry(float d) { float c = cosf(d), s = sinf(d); mat34 r = M34_ID; r.m[0] = c; r.m[2] = s; r.m[8] = -s; r.m[10] = c; return r; }
static mat34 m34rz(float d) { float c = cosf(d), s = sinf(d); mat34 r = M34_ID; r.m[0] = c; r.m[1] = -s; r.m[4] = s; r.m[5] = c; return r; }
static mat34 m34euler(float xdeg, float ydeg, float zdeg) {
  mat34 rz = m34rz((float)(zdeg * DEG2RAD));
  mat34 ry = m34ry((float)(ydeg * DEG2RAD));
  mat34 rx = m34rx((float)(xdeg * DEG2RAD));
  return m34mul(rz, m34mul(ry, rx));
}
/* row-major 3x4 -> GLSL mat4 uniform (4x4, last row 0001). */
static void m34_to_gl(mat34 m, float out[16]) {
  out[0] = m.m[0]; out[1] = m.m[4]; out[2] = m.m[8];  out[3] = 0;
  out[4] = m.m[1]; out[5] = m.m[5]; out[6] = m.m[9];  out[7] = 0;
  out[8] = m.m[2]; out[9] = m.m[6]; out[10] = m.m[10]; out[11] = 0;
  out[12] = m.m[3]; out[13] = m.m[7]; out[14] = m.m[11]; out[15] = 1;
}
#endif
