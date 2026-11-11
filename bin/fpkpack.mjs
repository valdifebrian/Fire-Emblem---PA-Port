#!/usr/bin/env node
// fpkpack.mjs — bake PoC asset bundle (format fpk1). Repo policy: fpk output is
// NEVER committed (gitignored); only this tool + format doc live in the repo.
//
// fpk1 layout (little-endian):
//   magic   "fpk1"          u8[4]
//   count   u32             entry count
//   entries: name[u8z padded to 4], off u64, len u32, fmt u32, crc32 u32
//   payload: blob data, each 16-aligned, zero padded
//   footer: sha256 of [count..payload_end) appended after payload (32 B)
// fmt codes: 0 raw, 1 tpl, 2 g, 3 ga, 4 stm, 5 png, 6 gcf_atlas, 7 csv, 8 fs
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';

const FMT = { raw: 0, tpl: 1, g: 2, ga: 3, stm: 4, png: 5, gcf_atlas: 6, csv: 7, fs: 8 };

function crc32(buf) {
  let c, t = crc32.t || (crc32.t = (() => {
    const a = new Uint32Array(256);
    for (let n = 0; n < 256; n++) { c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; a[n] = c >>> 0; }
    return a;
  })());
  c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = t[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

function guessFmt(p) {
  const e = path.extname(p).toLowerCase();
  return ({ '.tpl': 'tpl', '.g': 'g', '.ga': 'ga', '.stm': 'stm', '.png': 'png', '.csv': 'csv', '.fs': 'fs' })[e] || 'raw';
}

export function pack(listPath, outPath) {
  const spec = fs.readFileSync(listPath, 'utf8').split(/\r?\n/)
    .map(l => l.trim()).filter(l => l && !l.startsWith('#'))
    .map(l => { const [src, name, fmt] = l.split(/\s+/); return { src, name: name || path.basename(src), fmt: fmt || guessFmt(src) }; });
  const blobs = spec.map(s => {
    const b = fs.readFileSync(s.src);
    return { ...s, b };
  });
  let headSize = 8;
  for (const e of blobs) headSize += Math.ceil((Buffer.byteLength(e.name) + 1 + 20) / 4) * 4;
  let off = headSize;
  for (const e of blobs) { e.off = off; off += Math.ceil(e.b.length / 16) * 16; }
  const total = off + 32;
  const buf = Buffer.alloc(total);
  buf.write('fpk1', 0, 'ascii');
  buf.writeUInt32LE(blobs.length, 4);
  let p = 8;
  for (const e of blobs) {
    const nb = Buffer.from(e.name, 'ascii');
    buf.write(nb.toString('ascii'), p, nb.length, 'ascii'); p += Math.ceil((nb.length + 1) / 4) * 4;
    buf.writeBigUInt64LE(BigInt(e.off), p); p += 8;
    buf.writeUInt32LE(e.b.length, p); p += 4;
    buf.writeUInt32LE(FMT[e.fmt] ?? 0, p); p += 4;
    buf.writeUInt32LE(crc32(e.b), p); p += 4;
  }
  for (const e of blobs) e.b.copy(buf, e.off);
  const h = crypto.createHash('sha256').update(buf.subarray(4, total - 32)).digest();
  h.copy(buf, total - 32);
  fs.mkdirSync(path.dirname(outPath), { recursive: true });
  fs.writeFileSync(outPath, buf);
  console.log(`fpk1: ${blobs.length} entries, ${total} B -> ${outPath}`);
  for (const e of blobs) console.log(`  ${e.name} fmt=${e.fmt} len=${e.b.length} crc=${crc32(e.b).toString(16)}`);
}

if (process.argv[1] && process.argv[1].endsWith('fpkpack.mjs')) {
  const [listPath, outPath] = process.argv.slice(2);
  if (!listPath || !outPath) { console.error('usage: fpkpack <list.txt> <out.fpk>'); process.exit(1); }
  pack(listPath, outPath);
  process.exit(0);
}
