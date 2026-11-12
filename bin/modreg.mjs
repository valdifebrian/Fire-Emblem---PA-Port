// modreg.mjs — ekstrak REGISTRY MODUL PROC dari main.dol (ditemukan via UNKNOWN-U012, kini BRV-SCT-010).
// Fakta terverifikasi: pool deskriptor di file 0x2802E0..0x2A21C0 (segmen data, peta flat VA=file+0x80003000).
// Grammar record: [u16 param][u16 tag][?payload u32] — tag nama: 0x2003|0x2025 (+ptr string);
// tag kode handler: 0x200F (utama, 1228×), 0x2005, 0x2010; tag lain (0x201A,0x2004,0x2012,0x2014,0x2015,0x201B) = record bertipe lain.
// Output CSV: name,nHandlers,handlers(pipe VA),extras(pipe tag:VA).
// Usage: node bin/modreg.mjs [out/exec/main.dol] > out/data/module_registry.csv
import fs from 'fs';
const path = process.argv[2] || 'out/exec/main.dol';
const b = fs.readFileSync(path);
const DATA_BASE = 0x80003000; // peta data flat (terverifikasi label string Ghidra)
const POOL0 = 0x2802e0, POOL1 = 0x2a21c0;
const rd = (o) => b.readUInt32BE(o);
const NAME_TAGS = new Set([0x2003, 0x2025]);
const CODE_TAGS = new Set([0x200f, 0x2005, 0x2010]);
const out = [];
let cur = null;
for (let o = POOL0; o + 8 <= POOL1; o += 4) {
  const w = rd(o);
  const tag = w & 0xffff, hi = w >>> 16;
  if (hi !== 0) continue; // param!=0 = record bertipe param — dilewati (lihat catatan spec)
  const p = rd(o + 4);
  if (NAME_TAGS.has(tag)) {
    const off = p - DATA_BASE;
    if (off > 0x264000 && off < POOL1) {
      const s = b.subarray(off, off + 48).toString('latin1').split('\0')[0];
      if (/^[\x20-\x7e]{3,}/.test(s)) { cur = { name: s, ntag: tag.toString(16), handlers: [], extras: [] }; out.push(cur); continue; }
    }
  }
  if (!cur) continue;
  const isPtr = (p >>> 24) === 0x80;
  if (CODE_TAGS.has(tag) && isPtr) cur.handlers.push('0x' + p.toString(16));
  else if (isPtr) cur.extras.push('0x' + tag.toString(16) + ':0x' + p.toString(16));
}
const seen = new Set();
const rows = ['name,nameTag,nHandlers,handlers,extras'];
for (const r of out) {
  if (seen.has(r.name)) continue; seen.add(r.name);
  rows.push('"' + r.name + '",' + r.ntag + ',' + r.handlers.length + ',"' + r.handlers.join(' ') + '","' + r.extras.slice(0, 8).join(' ') + '"');
}
console.log(rows.join('\n'));
process.exit(0);
