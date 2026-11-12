// pxref.mjs — PPC raw cross-reference scanner on main.dol (GC DOL header @0).
// Menemukan xref yang tidak tampak di pseudocode (indirect bl / pointer data).
// Pakai: node bin/pxref.mjs 8002e0d0 [8014d2b0 ...] [--win 0x10] [--dump]
// Output: per target: daftar `bl@VA`, `b@VA`, `ptr@VA` (kata data == VA target).
// --win N: juga dump N word di sekitar tiap ptr@ (deteksi tabel {op,u32 ptr}).
import fs from "fs";
const args = process.argv.slice(2);
const dolPath = process.env.FE9_DOL || "out/exec/main.dol";
const dumpWin = args.includes("--win") ? parseInt(args[args.indexOf("--win") + 1], 16) || 0x20 : 0;
const doDump = args.includes("--dump");
const targets = args.filter((a) => /^[0-9a-fA-F]{5,}$/.test(a)).map((s) => parseInt(s, 16));
if (!targets.length) { console.log("usage: node bin/pxref.mjs <hexVA>... [--win N] [--dump]"); process.exit(1); }
const b = fs.readFileSync(dolPath);
const segs = [];
for (let t = 0; t < 18; t++) {
  const o = b.readUInt32BE(4 * t), a = b.readUInt32BE(0x48 + 4 * t), sz = b.readUInt32BE(0x90 + 4 * t);
  if (sz > 0 && sz < 0x2000000 && a >= 0x80000000) segs.push({ o, a, sz });
}
const segOf = (va) => segs.find((s) => va >= s.a && va < s.a + s.sz);
for (const va of targets) {
  const out = [];
  for (const s of segs) {
    for (let off = s.o; off + 4 <= s.o + s.sz; off += 4) {
      const w = b.readUInt32BE(off);
      const addr = s.a + (off - s.o);
      const op = w >>> 26, lk = w & 1, aa = (w >>> 1) & 1;
      const li = w & 0x03ffffff;
      const rel = (li & 0x02000000) ? li - 0x04000000 : li;
      const m = (x) => (x & ~3) >>> 0;
      if (op === 0x12 && lk === 1) {
        if ((aa === 0 && m(addr + rel) === va) || (aa === 1 && m(s.a + rel) === va)) out.push("bl@" + addr.toString(16));
      } else if (op === 0x05 && lk === 0) {
        if ((aa === 0 && m(addr + rel) === va) || (aa === 1 && m(s.a + rel) === va)) out.push("b@" + addr.toString(16));
      } else if (w === va) {
        out.push("ptr@" + addr.toString(16));
        if (doDump) {
          const S = segOf(addr - 0x40) || s;
          for (let d = -dumpWin; d <= dumpWin; d += 4) {
            const v = addr + d;
            if (v >= S.a && v < S.a + S.sz) console.log("   " + v.toString(16) + " " + b.readUInt32BE(S.o + (v - S.a)).toString(16));
          }
        }
      }
    }
  }
  console.log(va.toString(16) + " -> " + (out.join(" ") || "(none)"));
}
process.exit(0);
