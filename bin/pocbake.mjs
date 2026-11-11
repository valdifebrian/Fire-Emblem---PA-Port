#!/usr/bin/env node
// pocbake.mjs — bake poc1.fpk for the P2 Architecture PoC from disc + work-dir
// sources. Entries: "<src> <name> <fmt>" where src = local path or "disc:/path".
// Usage: FE9_RVZ=<disc> node bin/pocbake.mjs <manifest.txt> <out.fpk>
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { openRvz, readFST, walkFiles } from './rvz.mjs';
import { pack } from './fpkpack.mjs';

const [listPath, outPath] = process.argv.slice(2);
if (!listPath || !outPath) { console.error('usage: pocbake <manifest> <out.fpk>'); process.exit(1); }
const spec = fs.readFileSync(listPath, 'utf8').split(/\r?\n/).map(l => l.trim()).filter(l => l && !l.startsWith('#'));
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'pocbake-'));
const lines = [];
const disc = spec.some(l => l.startsWith('disc:'));
let r, root;
if (disc) {
  r = await openRvz(process.env.FE9_RVZ);
  const gc = Buffer.from(await r.read(0, 0x440));
  root = await readFST(r, gc.readUInt32BE(0x424), gc.readUInt32BE(0x428));
}
for (const l of spec) {
  const [src, name, fmt] = l.split(/\s+/);
  let local = src;
  if (src.startsWith('disc:')) {
    const dp = src.slice(5); // /Sound/...
    const files = await walkFiles(root, '', []);
    const hit = files.find(f => f.path === dp);
    if (!hit) { console.error('DISC MISS ' + dp); process.exit(1); }
    const buf = await r.read(hit.pos, hit.size);
    local = path.join(tmp, name.replace(/[\/]/g, '_'));
    fs.writeFileSync(local, buf);
    console.log(`disc:${dp} -> ${hit.size} B`);
  }
  lines.push(`${local} ${name} ${fmt}`);
}
const rl = path.join(tmp, 'resolved.txt');
fs.writeFileSync(rl, lines.join('\n'));
pack(rl, outPath);
process.exit(0);
