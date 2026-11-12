/* build: harvest (name,argc) from all /Scripts/*.cmb.json disasm -> CSV
 * [BRV-SCT-005] engine-side native registry = union(catalog, call-sites).
 * argc truth = CALL operand (script-side declaration mirror). */
import fs from "fs";
const dir = "out/cmb";
const calls = new Map(); // name -> Map(argc -> count)
for (const f of fs.readdirSync(dir).filter((x) => x.endsWith(".cmb.json"))) {
  const j = JSON.parse(fs.readFileSync(`${dir}/${f}`, "utf8"));
  for (const fn of j.funcs) for (const [at, op, label, cmt] of fn.ins || []) {
    if (!label.startsWith("CALL ")) continue;
    const m = label.match(/^CALL (\d+) (\d+)/);
    if (!m) continue;
    const name = (cmt || "").replace(/^"|"$/g, "");
    if (!name) continue;
    if (!calls.has(name)) calls.set(name, new Map());
    const am = calls.get(name);
    am.set(m[2], (am.get(m[2]) || 0) + 1);
  }
}
const cat = new Map();
for (const line of fs.readFileSync("out/data/native_catalog.csv", "utf8").split("\n").slice(1)) {
  const [n, a] = line.split(",");
  if (n) cat.set(n, +a);
}
let rows = [];
let conflict = 0, missing = 0;
for (const [name, am] of calls) {
  const argcCounts = [...am.entries()].sort((a, b) => b[1] - a[1]);
  const top = +argcCounts[0][0];
  const c = cat.get(name);
  if (c !== undefined && c !== top) { conflict++; rows.push(`#CONFLICT ${name} catalog=${c} callsites=${argcCounts.map(x=>x.join("/")).join(" ")}`); }
  const argc = c !== undefined ? c : top;
  if (c === undefined) missing++;
  rows.push(`${name},${argc},${argcCounts.map(x=>x.join(":")).join(" ")}`);
}
fs.writeFileSync("out/data/script_native_registry.csv", "name,argc,argc_hist\n" + rows.sort().join("\n") + "\n");
console.log(`registry=${rows.length} catalog=${cat.size} missing-from-catalog=${missing} argc-conflicts=${conflict}`);
for (const r of rows) if (r.startsWith("#CONFLICT")) console.log(r);
