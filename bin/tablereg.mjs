// tablereg.mjs — universal pointer-patch table registry parser (family
// [u32 size][u32 tabOff][u32 cnt][u32 sub] loaded by FUN_8003f87c:
// table @ 0x20+(tabOff&~3), cnt x u32 SLOT POSITIONS (payload-relative),
// value at each slot = pool offset (suffix-dedup pool; resolver walks back).
// Usage: node bin/tablereg.mjs <file> [--pairs]   (--pairs: 2 slots/record)
import fs from "fs";
const [rel, ...flags] = process.argv.slice(2);
const PAIRS = flags.includes("--pairs");
const b = fs.readFileSync(rel);
const size = b.readUInt32BE(0), tabOff = b.readUInt32BE(4), cnt = b.readUInt32BE(8), sub = b.readUInt32BE(12);
const T = 0x20 + (tabOff & 0xfffffffc);
if (T + cnt * 4 > b.length) { console.error("table past EOF"); process.exit(1); }
const name = at => {
  const v = b.readUInt32BE(at);
  if (v < 0x20 || v >= size) return "";
  let st = v;
  while (st > 0x20 && b[st - 1] >= 0x20 && b[st - 1] <= 0x7e) st--;
  let e = v;
  while (e < size && b[e] >= 0x20 && b[e] <= 0x7e) e++;
  return b.subarray(st, e).toString("latin1");
};
const out = [];
const nrec = PAIRS ? cnt / 2 : cnt;
for (let i = 0; i < nrec; i++) {
  const s0 = (b.readUInt32BE(T + i * (PAIRS ? 8 : 4)) & 0xfffffffc) + 0x20;
  const s1 = PAIRS ? (b.readUInt32BE(T + i * 8 + 4) & 0xfffffffc) + 0x20 : 0;
  out.push(PAIRS ? [name(s0), name(s1)] : [name(s0)]);
}
console.log(`# ${rel} size=${size} tabOff=0x${tabOff.toString(16)} cnt=${cnt} sub=${sub} records=${out.length}`);
for (const r of out) console.log(r.join(","));
process.exit(0);
