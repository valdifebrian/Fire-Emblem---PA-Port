// ring.mjs — annotate a runtime op-ring dump (trace_ring.txt from the PoC's
// wedge/LOOPSTREAK auto-dump) against CMB disassembly JSON (cmb.mjs json output).
// Joins device-side pc/op/sp traces to named instructions to localize wedges.
// Usage: node bin/ring.mjs <ringfile> <C.cmb.json> [tailN] [collapse=1]
import fs from "fs";
const [ringFile, cmbJson, tailArg, collapseArg] = process.argv.slice(2);
if (!ringFile || !cmbJson) { console.log("usage: ring.mjs <ringfile> <C.json> [tailN] [collapse=0]"); process.exit(1); }
const lines = fs.readFileSync(ringFile, "utf8").split("\n").filter(l => l.includes("@0x"));
const j = JSON.parse(fs.readFileSync(cmbJson, "utf8"));
const funcs = j.funcs || j.recs || j;
const byPc = new Map();
for (const f of funcs) for (const ins of (f.ins || [])) {
  const pc = parseInt(ins[0], 16);
  if (!byPc.has(pc)) byPc.set(pc, `${ins[2]}  ${ins[3]}`.trim() + `  [${f.name || "?"}]`);
}
const collapse = collapseArg !== "collapse=0";
const tail = tailArg ? lines.slice(- +tailArg) : lines;
const out = [];
let prev = null, rep = 0;
const flush = () => { if (rep > 1) out.push(`   ...x${rep}...`); rep = 0; };
for (const line of tail) {
  const m = line.match(/@0x([0-9a-f]+) op=([0-9a-f]+)/);
  if (!m) { out.push(line); continue; }
  const key = m[1] + ":" + m[2];
  if (collapse && key === prev) { rep++; continue; }
  flush(); prev = key;
  out.push(`@${m[1].padStart(4, "0")} op=${m[2]} ${byPc.get(parseInt(m[1], 16)) || "???"}`);
}
flush();
console.log(out.join("\n"));
process.exit(0);
