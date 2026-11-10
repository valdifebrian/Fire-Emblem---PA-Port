// magicscan.mjs — probe magic bytes per keluarga aset (bukti format nyata).
// node bin/magicscan.mjs  → FE9_MAGIC_PROBE.csv
import { readFileSync, writeFileSync } from "fs";
import { openRvz, readFST, walkFiles } from "./rvz.mjs";

const inv = readFileSync("inventory/files.csv", "utf8").trim().split(/\r?\n/).slice(1);
const fams = new Map();
for (const line of inv) {
  const [p, pos, size] = line.split(",");
  const top = "/" + p.split("/")[1];
  const ext = (p.match(/\.([a-z0-9]+)$/i) || [, "none"])[1].toLowerCase();
  const k = `${top}|.${ext}`;
  if (!fams.has(k)) fams.set(k, []);
  fams.get(k).push({ p, pos: +pos, size: +size });
}
const CLS = (h) =>
  h.startsWith("0020af30") ? "TPL-FE9" :
  h.startsWith("0010") || h.startsWith("10") ? "LZ77?" :
  h.startsWith("53454d68") ? "STM" :
  h.startsWith("00010000") ? "THP?" :
  h.startsWith("474346") ? "GCF" :
  h.startsWith("230d0a") || h.startsWith("230a") || h.startsWith("09") || h.startsWith("7b09") ? "TEXT?" :
  "UNKNOWN";

const rvz = process.env.FE9_RVZ ||
  "C:/Users/valdi/Downloads/Fire Emblem - Path of Radiance (USA)/Fire Emblem - Path of Radiance (USA).rvz";
const r = await openRvz(rvz);
const gc = Buffer.from(await r.read(0, 0x440));
const root = await readFST(r, gc.readUInt32BE(0x424), gc.readUInt32BE(0x428));
const files = await walkFiles(root, "", []);
const byPath = new Map(files.map((f) => [f.path, f]));

const out = ["family,sample_path,size,first16hex,guess"];
for (const [k, list] of [...fams.entries()].sort()) {
  const picks = list.slice(0, 2).concat(list.length > 4 ? [list[Math.floor(list.length / 2)]] : []);
  for (const f of picks) {
    const e = byPath.get(f.p);
    if (!e) { out.push(`${k},${f.p},${f.size},MISSING-IN-FST,-`); continue; }
    const n = Math.min(16, e.size);
    try {
      const b = Buffer.from(await r.read(e.pos, n));
      out.push(`${k},"${f.p}",${e.size},${b.toString("hex")},${CLS(b.toString("hex"))}`);
    } catch (err) { out.push(`${k},"${f.p}",${e.size},READFAIL ${err.message},-`); }
  }
}
writeFileSync("FE9_MAGIC_PROBE.csv", out.join("\n") + "\n");
console.log(out.slice(0, 3).join("\n"), "\nrows=", out.length - 1);
