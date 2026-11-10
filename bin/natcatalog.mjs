import fs from "fs";
import { execSync } from "child_process";
const raw = execSync(`grep -rh "FUN_80013ae8(" out/ghidra/decAll/*.c`, { shell: "C:\\Program Files\\Git\\bin\\bash.exe", maxBuffer: 64 * 1024 * 1024 }).toString();
const rows = new Map();
for (const m of raw.matchAll(/s_([A-Za-z0-9_]+?)_[0-9a-f]{8},(FUN_[0-9a-f]+|&LAB_[0-9a-f]+|&DAT_[0-9a-f]+),(\d+)/g)) {
  if (!rows.has(m[1])) rows.set(m[1], [m[2], m[3]]);
}
// call frequency from ALL_CALLED_NAMES.csv
let freq = new Map();
try {
  for (const l of fs.readFileSync("out/cmb/ALL_CALLED_NAMES.csv", "utf8").split("\n").slice(1)) {
    const p = l.split(",");
    if (p.length >= 2) freq.set(p[1].trim(), (freq.get(p[1].trim()) || 0) + Number(p[0] || 1));
  }
} catch {}
const out = ["name,argc,impl,script_calls"];
for (const [n, [fn, ac]] of [...rows.entries()].sort((a, b) => (freq.get(b[0]) || 0) - (freq.get(a[0]) || 0))) {
  out.push(`${n},${ac},${fn},${freq.get(n) || 0}`);
}
fs.writeFileSync("out/data/native_catalog.csv", out.join("\n"));
console.log("catalog natives:", rows.size, "with-calls:", [...rows.keys()].filter((k) => freq.get(k)).length);
console.log("TOP 30:\n" + out.slice(1, 31).join("\n"));
