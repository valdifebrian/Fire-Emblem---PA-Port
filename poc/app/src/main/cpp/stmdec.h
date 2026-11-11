/* stmdec.h — FE9 /Sound *.stm (GC ADPCM stereo) → PCM16 interleaved.
 * Exact port of stm.mjs (validated: boss1 61.1s WAV). Header:
 * u16BE@2=sr, u32BE@4=ch, u32BE@8=bytes/stream, u32BE@0xc=loop(nibbles),
 * 16 coefs s16BE@0x5c; L stream @0x8100, R @0x8100+bytes.
 * Frame 8B: hdr nibbles [idx|scale], 14 samples: nib low-first per byte,
 * s = clamp16(((nib*scale)<<11) + 1024 + c1*h1 + c2*h2) >> 11. */
#ifndef STMDEC_H_
#define STMDEC_H_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gload.h" /* be_u32/be_u16 */

typedef struct { int sr; int frames14; int16_t* pcm; int pcmFrames; /* samples per ch */
                 int loopStart; /* sample index or -1 */ } stm_file;

static void stm_free(stm_file* f) { free(f->pcm); memset(f, 0, sizeof *f); }

static int stm_decode(stm_file* f, const uint8_t* b, int size) {
  memset(f, 0, sizeof *f);
  if (size < 0x8100) return -1;
  int sr = be_u16(b + 2);
  uint32_t bytes = be_u32(b + 8);
  int frames = (int)(bytes / 8);
  if (frames <= 0 || 0x8100 + (int)bytes * 2 > size) return -1;
  int16_t coefs[16];
  for (int i = 0; i < 16; i++) coefs[i] = (int16_t)be_u16(b + 0x5c + i * 2);
  int16_t* pcm = (int16_t*)malloc((size_t)frames * 14 * 2 * sizeof(int16_t));
  if (!pcm) return -1;
  for (int ch = 0; ch < 2; ch++) {
    const uint8_t* base = b + 0x8100 + (ch ? bytes : 0);
    int16_t h1 = 0, h2 = 0;
    for (int fr = 0; fr < frames; fr++) {
      const uint8_t* o = base + fr * 8;
      uint8_t hd = o[0];
      int scale = 1 << (hd & 15);
      int ci = ((hd >> 4) & 15); if (ci > 7) ci = 7; ci *= 2;
      int16_t c1 = coefs[ci], c2 = coefs[ci + 1];
      for (int i = 0; i < 14; i++) {
        uint8_t by = o[1 + (i >> 1)];
        int nib = (i & 1) ? (by & 15) : (by >> 4);
        if (nib > 7) nib -= 16;
        int s = (nib * scale) << 11;
        s = (s + 1024 + c1 * h1 + c2 * h2) >> 11;
        if (s > 32767) s = 32767; if (s < -32768) s = -32768;
        h2 = h1; h1 = (int16_t)s;
        pcm[fr * 28 + i * 2 + ch] = (int16_t)s;
      }
    }
  }
  f->pcm = pcm; f->sr = sr; f->frames14 = frames * 14; f->pcmFrames = frames * 14;
  uint32_t loopNib = be_u32(b + 0xc);
  f->loopStart = (loopNib % 16) ? (int)(loopNib / 16) * 14 + (int)(loopNib % 16) - 2 : (int)(loopNib / 16) * 14;
  return 0;
}
#endif
