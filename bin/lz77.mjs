// Nintendo LZ77 (GameCube "0x10" variant, as used by GFEE01 FE9 .cmp/.cms containers).
// Cracked + byte-verified 2026-11-10 against uncompressed root copies (FE8Data/FE8Anim/cp_data).
// Header: [0x10][24-bit BE size field]. NOTE: for system.cmp the size field (0xA0A61A)
// is NOT the decompressed blob size — do not use it as an output bound; decompress until
// the source is exhausted (see cmp.mjs for the stream-chain driver).
// Stream: 8 items per flag byte, MSB-first:
//   flag bit = 0 -> literal byte
//   flag bit = 1 -> 2-byte back-reference: b0 = (len4<<4)|(off>>8), b1 = off&0xFF
//     off = ((b0 & 0xF) << 8 | b1) + 1  (1-based distance back into output)
//     len = (b0 >> 4) + 3               (nibble 0..0xF -> length 3..18; NO escape byte
//                                        in this variant — nibble 0 is a valid length 3)
//   Overlapping copies allowed (byte-by-byte), offset 1 = RLE run.
export function lz77ExDecode(buf) {
  if (buf[0] !== 0x10) throw new Error("not LZ77-0x10 (magic): 0x" + buf[0].toString(16));
  const size = buf.readUInt32BE(0) & 0xffffff;
  const out = Buffer.allocUnsafe(size);
  let ob = out; // realloc-safe reference
  let p = 4, o = 0, flags = 0, bit = 8;
  const grow = (need) => {
    if (need <= ob.length) return;
    let n = ob.length;
    while (n < need) n *= 2;
    const nb = Buffer.allocUnsafe(n);
    ob.copy(nb, 0, 0, o);
    ob = nb;
  };
  while (p < buf.length) {
    if (bit === 8) { flags = buf[p++]; bit = 0; }
    if ((flags & (0x80 >> bit++)) === 0) {
      grow(o + 1);
      ob[o++] = buf[p++];
    } else {
      if (p + 2 > buf.length) throw new Error("truncated ref at out " + o);
      const b0 = buf[p++], b1 = buf[p++];
      const off = (((b0 & 0x0f) << 8) | b1) + 1;
      const len = (b0 >> 4) + 3;
      if (off > o) throw new Error("bad ref at out " + o + " off " + off);
      grow(o + len);
      const src = o - off;
      for (let i = 0; i < len; i++) ob[o++] = ob[src + i];
    }
  }
  return ob.subarray(0, o);
}
