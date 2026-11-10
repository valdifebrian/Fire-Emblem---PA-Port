// stm.mjs — FE9/FE10 .stm (GCFE stream, = vgmstream DSP_STM): 2 mono GC-DSP streams.
// Header: u16BE@2=sr, u32BE@4=ch, u32BE@8=bytes/stream, u32BE@0xc=loop(nibbles), coefs 16*s16BE@0x5c.
// Frame 8B: [idx4|scale4][14 nibbles low-first]; sample = clamp16(((nib*scale)<<11) + 1024 + c1*h1 + c2*h2) >> 11.
import { readFileSync, writeFileSync } from "fs";

const clip = (v) => (v > 32767 ? 32767 : v < -32768 ? -32768 : v);

export function stmDecode(b) {
  const sr = b.readUInt16BE(2);
  const bytes = b.readUInt32BE(8);
  const frames = Math.floor(bytes / 8);
  const coefs = [];
  for (let i = 0; i < 16; i++) coefs.push(b.readInt16BE(0x5c + i * 2));
  const decode = (base) => {
    let h1 = 0, h2 = 0;
    const out = new Int16Array(frames * 14);
    for (let f = 0; f < frames; f++) {
      const o = base + f * 8;
      const hd = b[o];
      const scale = 1 << (hd & 15);
      const ci = Math.min((hd >> 4) & 15, 7) * 2;
      const c1 = coefs[ci], c2 = coefs[ci + 1];
      for (let i = 0; i < 14; i++) {
        const by = b[o + 1 + (i >> 1)];
        let nib = i & 1 ? by & 15 : by >> 4;
        if (nib > 7) nib -= 16;
        let s = (nib * scale) << 11;
        s = (s + 1024 + c1 * h1 + c2 * h2) >> 11;
        s = clip(s);
        out[f * 14 + i] = s;
        h2 = h1; h1 = s;
      }
    }
    return out;
  };
  const L = decode(0x8100), R = decode(0x8100 + bytes);
  const loopNib = b.readUInt32BE(0xc);
  const loopSample = loopNib % 16 ? Math.floor(loopNib / 16) * 14 + (loopNib % 16) - 2 : (loopNib / 16) * 14;
  return { sr, L, R, loopSample };
}

function rough(a) {
  const m = Math.min(a.length, 80000);
  let d1 = 0, d2 = 0;
  for (let i = 1; i < m; i++) d1 += (a[i] - a[i - 1]) ** 2;
  for (let i = 2; i < m; i++) d2 += ((a[i] - a[i - 1]) - (a[i - 1] - a[i - 2])) ** 2;
  return Math.sqrt(d2 / (m - 2)) / (Math.sqrt(d1 / (m - 1)) + 1e-9);
}

function wavPath(stmPath) {
  const b = readFileSync(stmPath);
  const { sr, L, R, loopSample } = stmDecode(b);
  const n = Math.min(L.length, R.length);
  const data = Buffer.alloc(n * 4);
  for (let i = 0; i < n; i++) { data.writeInt16LE(L[i], i * 4); data.writeInt16LE(R[i], i * 4 + 2); }
  const wav = Buffer.alloc(44 + data.length);
  wav.write("RIFF", 0); wav.writeUInt32LE(36 + data.length, 4); wav.write("WAVEfmt ", 8);
  wav.writeUInt32LE(16, 16); wav.writeUInt16LE(1, 20); wav.writeUInt16LE(2, 22);
  wav.writeUInt32LE(sr, 24); wav.writeUInt32LE(sr * 4, 28); wav.writeUInt16LE(4, 32); wav.writeUInt16LE(16, 34);
  wav.write("data", 36); wav.writeUInt32LE(data.length, 40);
  data.copy(wav, 44);
  const outP = stmPath.replace(/\.stm$/, ".wav");
  writeFileSync(outP, wav);
  console.log(outP, "sr", sr, "samples/ch", n, "dur", (n / sr).toFixed(1) + "s", "loop_smp", loopSample, "rough L", rough(L).toFixed(3), "R", rough(R).toFixed(3));
  return outP;
}

if (process.argv[1] && process.argv[1].endsWith("stm.mjs") && process.argv[2]) for (const f of process.argv.slice(2)) wavPath(f);
