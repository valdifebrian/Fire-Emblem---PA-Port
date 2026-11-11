// dispos.mjs - parser zmap dispos_c.bin: both grid variants (always=108B, bmap=112B).
// Grammar authority: FUN_80107b54 (DisposFileDecode, dtk-verified):
//   20xu32 + 26xu8 + align -> 106..108B; bmap variant adds one u32 (grid 0x70).
// Layout per record (from pseudocode, BRV-SCT-008):
//   +00 u32 a | +04 u32 b | +08 u32 skip | +0C u32 | +10..2F 8xu32 | +30..37 8 items u8
//   +38..47 4 AI words | +48..5B 5xu32 | +5C x | +5D y | +5E..66 u8 flags | (+67.. pad/extra)
// File = [u32 size][u32 tableOff][u16 n1][u16 n2][hdr to 0x24][records*N][strings...][tail registry]
// usage: node dispos.mjs csv <file...> | node dispos.mjs show <file>
import fs from "fs";
import path from "path";

export function parseDispos(buf, label = "") {
  const size = buf.readUInt32BE(0), tableOff = buf.readUInt32BE(4);
  const n1 = buf.readUInt16BE(8), n2 = buf.readUInt16BE(10);
  // find string-run start: both variants begin the string region with a build
  // timestamp "2005-.." / "2005/.." (verified always + bmap01; strong boring anchor).
  let sStart = -1;
  const latin = buf.toString("latin1");
  const m = latin.match(/2005[-\/]\d\d[-\/]\d\d/);
  if (m && m.index >= 0x24) sStart = m.index;
  if (sStart < 0) {
    for (let o = 0x24; o < buf.length - 32; o++) {
      const run = buf.toString("latin1", o, o + 32);
      const mm = run.match(/[\x20-\x7e]{8,}\x00[\x20-\x7e]{6,}/);
      if (mm && mm.index === 0) { sStart = o; break; }
    }
  }
  if (sStart < 0) return { error: "no string run", size, tableOff };
  const variants = [108, 112];
  // candidate pool starts: exact anchor, and backward scan through printable/NUL run
  const cands = new Set([sStart]);
  let st = sStart;
  while (st > 0x24 && ((buf[st - 1] >= 0x20 && buf[st - 1] <= 0x7e) || buf[st - 1] === 0)) st--;
  if (st > 0x24) {
    cands.add(st);
    for (const S of variants) cands.add(st - ((st - 0x24) % S)); // u32 tail before strings (always variant)
  }
  let stride = 0, recs = 0, snap = sStart;
  outer: for (const cand of cands) {
    for (const S of variants) {
      const n = (cand - 0x24) / S;
      if (Number.isInteger(n) && n >= 0 && n <= 400) { stride = S; recs = n; snap = cand; break outer; }
    }
  }
  const strings = buf.toString("latin1", snap, buf.length).split("\x00").filter((s) => /^[\x20-\x7e]+$/.test(s) && s.length >= 3);
  const rec = (i) => {
    const o = 0x24 + i * stride;
    const u = (k) => buf.readUInt32BE(o + k);
    const b = (k) => buf[o + k];
    return {
      i, off: o,
      a: u(0), b: u(4), c: u(8), f10: u(0x10), f14: u(0x14),
      ai: [0x38, 0x3c, 0x40, 0x44].map((k) => u(k)),
      d48: [0x48, 0x4c, 0x50, 0x54, 0x58].map((k) => u(k)),
      x: b(0x5c), y: b(0x5d),
      f5e: b(0x5e), f5f: b(0x5f), f60: b(0x60), f61: b(0x61), f62: b(0x62),
      f63: b(0x63), f64: b(0x64), f65: b(0x65), f66: b(0x66),
      items: [...Array(8)].map((_, k) => b(0x30 + k)),
    };
  };
  return { label, size, tableOff, n1, n2, gridStart: 0x24, stringStart: snap, stride, recs, records: recs ? [...Array(recs)].map((_, i) => rec(i)) : [], strings };
}

const args = process.argv.slice(2);
const mode = args[0] || "show";
for (const f of args.slice(1)) {
  const p = parseDispos(fs.readFileSync(f), path.basename(f));
  if (mode === "csv") {
    console.log("file,variant,stride,rec,off,a,b,c,f10,ai0,ai1,ai2,ai3,x,y,f5e,f5f,f60,f61,f62,f63,f64,f65,f66,items");
    for (const r of p.records || [])
      console.log([p.label, p.stride, r.i, r.off.toString(16), r.a, r.b, r.c, r.f10, ...r.ai, r.x, r.y, r.f5e, r.f5f, r.f60, r.f61, r.f62, r.f63, r.f64, r.f65, r.f66, `"${r.items.join(" ")}"`].join(","));
    console.log(`# strings: ${(p.strings || []).join(" ")}`);
  } else {
    console.log(JSON.stringify(p, null, 1));
  }
}
