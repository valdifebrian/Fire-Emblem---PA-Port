// m.mjs — GFEE01 .m (message/text) parser -> CSV.
// Header: u32 len | u32 tableHint | u32 0 | u32 count(upper bound)
// Entries live in a [u32 strOff][u32 msgId] interleaved table (stride 8) somewhere
// just after the hint; ids ascend. Text region = 0x20..table; strings are NUL-
// terminated with embedded control bytes (0x01..0x1f tokens, e.g. p/t/l/x lines)
// and suffix-pool reuse (some offsets point mid-string of a longer neighbour).
import { readFile, writeFile } from "fs/promises";
const [,, inPath, outPath] = process.argv;
if (!inPath || !outPath) { console.error("usage: node bin/m.mjs <in.m> <out.csv>"); process.exit(2); }
const d = await readFile(inPath);
const u32 = (o) => d.readUInt32BE(o);
const len = u32(0), hint = u32(4), count = u32(0xc);
if (len !== d.length) console.error(`m: declared len 0x${len.toString(16)} != file 0x${d.length.toString(16)}`);
// find table start: hint..hint+0x200 where interleaved off/id pattern holds
let T = -1;
for (let q = hint; q < Math.min(hint + 0x200, d.length - count * 8); q += 4) {
  let good = 0;
  for (let i = 0; i < 64; i++) {
    const off = u32(q + i * 8), id = u32(q + i * 8 + 4);
    if (off >= 0x18 && off < hint && id < 0x8000000) good++; else break;
  }
  if (good >= 60) { T = q; break; }
}
if (T < 0) throw new Error("table not found near hint 0x" + hint.toString(16));
console.log("table @0x" + T.toString(16) + " (hint 0x" + hint.toString(16) + " +0x" + (T - hint).toString(16) + ")");
const sz = (p) => { let e = p; while (e < d.length && d[e]) e++; return d.subarray(p, e); };
const rows = [];
for (let i = 0; i < count; i++) {
  const off = u32(T + i * 8), id = u32(T + i * 8 + 4);
  rows.push({ id, off, text: sz(off).toString("latin1"), raw: sz(off).toString("hex") });
}
const esc = (s) => /[",\n\r]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
await writeFile(outPath, "id,off,text\n" + rows.map((r) => `${r.id},0x${r.off.toString(16)},${esc(r.text)}`).join("\n") + "\n");
console.log(`${count} messages -> ${outPath}`);
