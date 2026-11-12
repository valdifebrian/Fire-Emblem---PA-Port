// throwaway 5b: bake maps/bmap01_cost.bin + bmap01_exits.bin — water = bmap01_water0.gs
// face footprint (ground truth model the engine loads); exits = maplinkAt rec pts.
import fs from 'node:fs';
import { parseGs } from './bin/gs.mjs';
import { parseRegistryBin } from './bin/mapbin.mjs';
const TW = 29, TH = 24;
const cost = Buffer.alloc(TW * TH, 1);
let nW = 0;
const w = parseGs(fs.readFileSync('out/bmap01/bmap01_water0.gs'), 'water');
for (const sm of w.submeshes) for (const t of sm.tris || []) {
  const ps = t.map((vv) => w.positions.subarray(vv.p * 3, vv.p * 3 + 3));
  const xs = ps.map((p) => p[0]), zs = ps.map((p) => p[2]);
  for (let tx = Math.floor(Math.min(...xs) / 10); tx < Math.ceil(Math.max(...xs) / 10); tx++)
    for (let tz = Math.floor(Math.min(...zs) / 10); tz < Math.ceil(Math.max(...zs) / 10); tz++)
      if (tx >= 0 && tx < TW && tz >= 0 && tz < TH && cost[tz * TW + tx] < 99) { cost[tz * TW + tx] = 99; nW++; }
}
console.log('water tiles', nW);
const raw = fs.readFileSync('out/bmap01/map.bin');
const base = 0x20, fsize = raw.readUInt32BE(0);
const r = parseRegistryBin(raw, base);
const offs = r.chunks.map((c) => base + c.dataOff).sort((a, b) => a - b);
const end = (o) => offs.find((x) => x > o) ?? fsize + base;
const byName = {}; for (const c of r.chunks) byName[c.name] = base + c.dataOff;
const l = byName.maplinkAt;
const exits = [];
for (let i = l + 2; i + 3 <= l + (end(l) - l); i += 3) {
  const x = raw[i], y = raw[i + 1], t = raw[i + 2];
  if (t === 0 || x > 0x30 || y > 0x30) break;
  if (x < TW && y < TH && cost[y * TW + x] < 99) exits.push([x, y]);
}
console.log('exits kept', exits.length);
fs.mkdirSync('out/pocmaps', { recursive: true });
fs.writeFileSync('out/pocmaps/bmap01_cost.bin', cost);
const eb = Buffer.alloc(2 + exits.length * 2);
eb.writeUInt16LE(exits.length, 0);
exits.forEach(([x, y], i) => { eb[2 + i * 2] = x; eb[3 + i * 2] = y; });
fs.writeFileSync('out/pocmaps/bmap01_exits.bin', eb);
let s = '   ' + Array.from({ length: TW }, (_, i) => i % 10).join('') + '\n';
for (let y = 0; y < TH; y++) s += String(y).padStart(2) + ' ' + Array.from({ length: TW }, (_, x) =>
  cost[y * TW + x] >= 99 ? '~' : exits.some((e) => e[0] === x && e[1] === y) ? 'E' : '.').join('') + '\n';
console.log(s);
process.exit(0);
