// RVZ v1 GameCube reader (Bun). GC raw-data disc: no partitions, no exception lists
// (verified empirically on GFEE01: decompressed groups == chunk size exactly).
// PRNG per dolphin docs/WiaAndRvz.md verbatim. All integers BE.
import { createHash } from "node:crypto";
import { mkdirSync, writeFileSync } from "node:fs";
import { open as fopen } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { pathToFileURL } from "node:url";

// zstd: node >=23.8 exposes zlib.zstdDecompressSync; fall back to Bun global.
const _zlib = await import("node:zlib");
const _zdec = _zlib.zstdDecompressSync ?? (globalThis.Bun ? (u) => Bun.zstdDecompressSync(u) : null);
if (!_zdec) throw new Error("no zstd: need node >=23.8 or Bun");
const zdec = (u) => { const r = _zdec(u); return r instanceof Uint8Array ? new Uint8Array(r.buffer, r.byteOffset, r.byteLength) : r; };

const D = (b) => new DataView(b.buffer, b.byteOffset, b.byteLength);
const u32 = (b, o) => D(b).getUint32(o, false);
const u64n = (b, o) => Number(D(b).getBigUint64(o, false));
const sha1 = (u8) => createHash("sha1").update(u8).digest("hex");
const md5 = (u8) => createHash("md5").update(u8).digest("hex");

// ---- PRNG (Lagged Fibonacci xor j=32 k=521) — spec verbatim ----
class Prng {
  constructor(seed /* Uint8Array(68) */) {
    this.buf = new Uint32Array(521);
    const sd = D(seed);
    for (let i = 0; i < 17; i++) this.buf[i] = sd.getUint32(i * 4, false); // BE on disk
    for (let i = 17; i < 521; i++)
      this.buf[i] = ((this.buf[i - 17] << 23) ^ (this.buf[i - 16] >>> 9) ^ this.buf[i - 1]) >>> 0;
    this.ptr = 0;
    for (let k = 0; k < 4; k++) this.advance(); // 4x before first output
  }
  advance() {
    const b = this.buf;
    for (let i = 0; i < 32; i++) b[i] = (b[i] ^ b[i + 521 - 32]) >>> 0;
    for (let i = 32; i < 521; i++) b[i] = (b[i] ^ b[i - 32]) >>> 0;
    this.ptr = 0;
  }
  // emit n bytes starting at disc offset `offset`
  emit(out, at, offset, n) {
    const b = this.buf;
    if (offset % 0x8000 !== 0) {
      let skip = offset % 0x8000;
      while (skip >= 4) { skip -= 4; this.word(); } // partial-word edge: documented, not seen in the wild
    }
    for (let i = 0; i < n; i++) out[at + i] = this.wordByte();
  }
  word() {
    if (this.ptr >= 521) this.advance();
    return this.buf[this.ptr++];
  }
  wordByte() {
    if (this.ptr >= 521) this.advance();
    const w = this.buf[this.ptr++];
    this._i = (this._i || 0) + 1;
    if (this._i === 1) return (w >>> 24) & 0xff;
    if (this._i === 2) return (w >>> 18) & 0xff; // NB spec: 18, not 16
    if (this._i === 3) return (w >>> 8) & 0xff;
    this._i = 0;
    return w & 0xff;
  }
}

// ---- container ----
export async function openRvz(path) {
  const fh = await fopen(path, "r");
  const fileSize = (await fh.stat()).size;
  const rd = async (o, l) => { const b = Buffer.alloc(l); const { bytesRead } = await fh.read(b, 0, l, o); return b.subarray(0, bytesRead); };
  const head = await rd(0, 0x48), disc = await rd(0x48, 0xDC);
  if (Buffer.from(head.subarray(0, 3)).toString("latin1") === "RVZ") {
    if (sha1(disc) !== Buffer.from(head.slice(0x10, 0x24)).toString("hex")) throw new Error("disc struct SHA-1 mismatch");
    if (sha1(head.subarray(0, 0x34)) !== Buffer.from(head.slice(0x34, 0x48)).toString("hex")) throw new Error("file head SHA-1 mismatch");
  } else throw new Error("not RVZ");
  const isoSize = u64n(head, 0x24), chunk = u32(disc, 0xC), comp = u32(disc, 4);
  if (u32(disc, 0) !== 1) throw new Error("not a GameCube disc");
  if (comp !== 5 && comp !== 0) throw new Error("unsupported compression " + comp);
  const numRaw = u32(disc, 0xB4), numG = u32(disc, 0xC4);
  const maybeZ = (bytes) => (comp === 0 || !bytes.length) ? bytes : zdec(bytes);
  const rawT = maybeZ(await rd(u64n(disc, 0xB8), u32(disc, 0xC0)));
  const grpT = maybeZ(await rd(u64n(disc, 0xC8), u32(disc, 0xD0)));
  if (rawT.length !== numRaw * 24) throw new Error("raw table size");
  if (grpT.length !== numG * 12) throw new Error("group table size");

  const groups = [];
  for (let i = 0; i < numG; i++) {
    const ds = u32(grpT, i * 12 + 4);
    groups.push({ off: u32(grpT, i * 12) * 4, size: ds & 0x7fffffff, comp: ds >>> 31 === 1, packed: u32(grpT, i * 12 + 8) });
  }
  const raws = [];
  for (let i = 0; i < numRaw; i++) {
    let off = u64n(rawT, i * 24), size = u64n(rawT, i * 24 + 8);
    const mis = off % 0x8000; off -= mis; size += mis; // wit alignment rule
    raws.push({ off, size, gi: u32(rawT, i * 24 + 16), ng: u32(rawT, i * 24 + 20) });
  }
  raws.sort((a, b) => a.off - b.off);

  function unpack(buf, base /* disc offset of chunk start */) {
    const parts = [];
    let pos = 0, outPos = 0;
    const dv = D(buf);
    while (pos + 4 <= buf.length) {
      const sz = dv.getUint32(pos, false); pos += 4;
      if (sz >>> 31) {
        const n = sz & 0x7fffffff;
        const seed = buf.subarray(pos, pos + 68); pos += 68;
        const seg = new Uint8Array(n);
        new Prng(seed).emit(seg, 0, base + outPos, n);
        parts.push(seg); outPos += n;
      } else if (sz > 0) {
        parts.push(buf.subarray(pos, pos + sz)); pos += sz; outPos += sz;
      }
    }
    const res = new Uint8Array(outPos);
    let p = 0; for (const s of parts) { res.set(s, p); p += s.length; }
    return res;
  }

  const gcache = new Map();
  async function chunkOf(ci, base) {
    if (gcache.has(ci)) return gcache.get(ci);
    const g = groups[ci];
    let data;
    if (g.size === 0) data = new Uint8Array(chunk);
    else {
      const raw = await rd(g.off, g.size);
      data = g.comp ? zdec(raw) : raw;
      if (g.packed) data = unpack(data, base);
    }
    if (gcache.size > 96) gcache.clear();
    gcache.set(ci, data);
    return data;
  }

  // raw-relative read
  async function read(off, len) {
    const out = new Uint8Array(len);
    let done = 0;
    while (done < len) {
      const abs = off + done, want = Math.min(len - done, chunk);
      let r = null;
      for (const e of raws) if (abs >= e.off && abs < e.off + e.size) { r = e; break; }
      if (!r) throw new Error(`unmapped disc offset 0x${abs.toString(16)}`);
      const rel = abs - r.off, ci = (rel / chunk) | 0, coff = rel % chunk;
      const base = abs - coff; // true disc offset of this chunk
      const data = await chunkOf(r.gi + ci, base);
      const take = Math.min(want, data.length - coff);
      if (take <= 0) throw new Error(`chunk ${r.gi + ci} underrun at 0x${abs.toString(16)}`);
      out.set(data.subarray(coff, coff + take), done);
      done += take;
    }
    return out;
  }

  return { path, size: fileSize, isoSize, chunk, groups, raws, discHeader: disc.slice(0x10, 0x90), numG, read, gcache };
}

// ---- GC ISO structures ----
export async function readApploader(r) {
  const b = await r.read(0, 0x2440);
  return {
    gamecode: Buffer.from(b.slice(0, 4)).toString("latin1"),
    makerId: Buffer.from(b.slice(4, 6)).toString("latin1"),
    discNum: b[6], discMax: b[7],
    dolOff: u32(b, 0x420), dolSize: u32(b, 0x424),
    fstOff: u32(b, 0x428), fstSize: u32(b, 0x42C),
    appEntry: u32(await r.read(0x2440, 4), 0),
    appTrailer: u32(await r.read(0x2444, 4), 0),
    appSize: u32(await r.read(0x2448, 4), 0),
  };
}

// FST scheme as empirically verified on GFEE01 (FE9 USA) — see docs/notes:
// dir entry = first u32 has bit 0x01000000 set (bit31 NOT used); file entry = that bit clear.
// Dir "size" field = ABSOLUTE entry index where its subtree ENDS (exclusive).
// String table starts at total*12; root name string is absent (root rendered "").
export async function readFST(r, fstOff, fstSize) {
  const fst = await r.read(fstOff, fstSize);
  const dv = D(fst);
  const total = dv.getUint32(8, false);
  if ((dv.getUint32(0, false) & 0x01000000) === 0 || total < 2 || total * 12 > fst.length)
    throw new Error("bad FST header (root dir / total)");
  const strOff = total * 12;
  const cstr = (o) => { let s = ""; while (fst[o] && o < fst.length) s += String.fromCharCode(fst[o++]); return s; };
  function build(i, je) {
    const e = dv.getUint32(i * 12, false), nm = cstr(strOff + (e & 0x00ffffff));
    if ((e & 0x01000000) === 0)
      return [{ name: nm, isFile: true, pos: dv.getUint32(i * 12 + 4, false), size: dv.getUint32(i * 12 + 8, false) }, i + 1];
    const end = dv.getUint32(i * 12 + 8, false);
    if (end <= i || end > je) throw new Error(`FST dir overflow '${nm}' #${i} end=${end} je=${je}`);
    const node = { name: nm, isFile: false, pos: dv.getUint32(i * 12 + 4, false), size: end - i, children: [] };
    let j = i + 1;
    while (j < end) { const [c, nj] = build(j, end); node.children.push(c); j = nj; }
    return [node, end];
  }
  const [root] = build(0, total);
  root.name = "";
  return root;
}

export async function walkFiles(node, prefix, out) {
  for (const c of node.children || []) {
    const p = prefix + "/" + c.name;
    if (c.isFile) out.push({ path: p, pos: c.pos, size: c.size });
    else await walkFiles(c, p, out);
  }
  return out;
}

export async function extract(r, pos, size, outPath) {
  mkdirSync(dirname(outPath), { recursive: true });
  const CHUNK = 4 << 20;
  const w = await fopen(outPath, "w");
  const h = createHash("md5");
  for (let o = 0; o < size; o += CHUNK) {
    const b = await r.read(pos + o, Math.min(CHUNK, size - o));
    h.update(b);
    await w.write(b);
  }
  await w.close();
  return h.digest("hex");
}

// ---- DOL header offline validator (per YAGCD/tockdom) ----
export function validateDol(dol) {
  const dv = D(dol);
  const segs = [];
  for (let i = 0; i < 18; i++) {
    const foff = u32(dol, i * 4), addr = u32(dol, 0x48 + i * 4), size = u32(dol, 0x90 + i * 4);
    if (size > 0) segs.push({ i, foff, addr, size });
  }
  const bss = { addr: u32(dol, 0xd8), size: u32(dol, 0xdc) };
  const entry = u32(dol, 0xe0);
  const mem = segs.map((s) => [s.addr, s.addr + s.size]).sort((a, b) => a[0] - b[0]);
  const overlaps = [];
  for (let i = 1; i < mem.length; i++) if (mem[i][0] < mem[i - 1][1]) overlaps.push([mem[i - 1], mem[i]]);
  const fileSegs = [...segs].sort((a, b) => a.foff - b.foff);
  const fOverlaps = [];
  for (let i = 1; i < fileSegs.length; i++) if (fileSegs[i].foff < fileSegs[i - 1].foff + fileSegs[i - 1].size) fOverlaps.push([fileSegs[i - 1], fileSegs[i]]);
  const inMem1 = (a) => a >= 0x80000000 && a < 0x81800000;
  const entrySeg = segs.find((s) => entry >= s.addr && entry < s.addr + s.size);
  return {
    segs, bss, entry, overlaps, fOverlaps,
    entryInSeg: !!entrySeg, entryIsText: !!entrySeg && entrySeg.i < 7,
    allInMem1: segs.every((s) => inMem1(s.addr) && inMem1(s.addr + s.size - 1)),
    maxAddr: segs.length ? Math.max(...segs.map((s) => s.addr + s.size)) : 0,
  };
}

// ---- CLI: node bin/rvz.mjs <cmd> [args] ----
//   ls [isoDir]          list FST dir (default /)
//   fstinfo              FST header + counts
//   extract <isoPath> <outFile>
//   dolinfo              validate main.dol header
//   read <discOff> <len> [outFile]
// RVZ path: $FE9_RVZ or the default user disc below.
async function main() {
  const [cmd, ...a] = process.argv.slice(2);
  const rvzPath = process.env.FE9_RVZ ||
    "C:/Users/valdi/Downloads/Fire Emblem - Path of Radiance (USA)/Fire Emblem - Path of Radiance (USA).rvz";
  const r = await openRvz(rvzPath);
  const gc = await r.read(0, 0x440);
  const fstOff = u32(gc, 0x424), fstSize = u32(gc, 0x428);
  if (!fstOff || !fstSize) throw new Error("header FST fields zero — pass offsets manually");
  const root = await readFST(r, fstOff, fstSize);
  const find = (p) => {
    let n = root;
    for (const seg of p.split("/").filter(Boolean)) {
      n = (n.children || []).find((c) => c.name === seg);
      if (!n) throw new Error("no such path: " + p);
    }
    return n;
  };
  if (cmd === "ls") {
    const n = find(a[0] || "/");
    if (n.isFile) { console.log(`${n.name} file pos=0x${n.pos.toString(16)} size=0x${n.size.toString(16)}`); return; }
    for (const c of n.children || [])
      console.log(`${c.isFile ? "f" : "d"} ${String(c.size).padStart(12)} 0x${c.pos.toString(16).padStart(8, "0")} /${(a[0] === "/" ? "" : (a[0] || "").replace(/^\//, "") + "/")}${c.name}`);
  } else if (cmd === "fstinfo") {
    const files = await walkFiles(root, "", []);
    console.log(JSON.stringify({ fstOff, fstSize, entries: u32(await r.read(fstOff, 12), 8), topEntries: root.children.length, files: files.length }));
  } else if (cmd === "extract") {
    if (!a[0] || !a[1]) throw new Error("usage: extract <isoPath> <outFile>");
    const n = find(a[0]);
    if (!n.isFile) throw new Error("not a file");
    const h = await extract(r, n.pos, n.size, a[1]);
    console.log(`md5=${h} ${a[0]} -> ${a[1]} (${n.size} B)`);
  } else if (cmd === "dolinfo") {
    console.log(JSON.stringify(validateDol(await r.read(u32(gc, 0x420), 0x120))));
  } else if (cmd === "read") {
    const b = await r.read(parseInt(a[0], 16) >>> 0, parseInt(a[1], 16) >>> 0);
    if (a[2]) { writeFileSync(a[2], b); console.log(`wrote ${b.length} B -> ${a[2]}`); }
    else console.log(Buffer.from(b).toString("hex"));
  } else {
    console.log("commands: ls [dir] | fstinfo | extract <isoPath> <out> | dolinfo | read <off> <len> [out]");
    process.exitCode = 2;
  }
}
if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  main().catch((e) => { console.error("error:", e.message); process.exitCode = 1; });
}
