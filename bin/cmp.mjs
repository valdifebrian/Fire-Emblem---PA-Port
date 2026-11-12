// cmp.mjs — GFEE01 (FE9) .cmp/.cms container unpacker.
//
// Format (cracked 2026-11-10, verified against uncompressed root copies):
//   Container:  [u8 type=0x10][u24 BE total-unpacked-size][LZ77 stream chain]
//   LZ77 stream: classic Nintendo LZ77 "0x10" variant:
//     flag byte, MSB-first; bit=0 → literal byte; bit=1 → 2-byte copy token
//       off = ((b0 & 0xF) << 8 | b1) + 1 ; len = (b0 >> 4) + 3  (nibble 0..0xF,
//       NO length-escape byte in this variant)
//   Streams are concatenated: when one stream's source runs out (or emits an
//   invalid ref), the next LZ77 stream begins immediately after. Header bytes
//   for later streams may or may not be present — the chain probe skips a
//   plausible [0x10][u24] header when seen.
//   Decompressed blob = "pack":
//     0x00 "pack" | 0x04 u16BE fileCount | 0x08 entries[0x10 each]:
//       u32 zero | u32 namePtr | u32 dataPtr | u32 len
//     (Pointers are relative to the first file's data start = data-base.)
import { readFile, writeFile, mkdir } from "fs/promises";
import { dirname, join, resolve } from "path";

export function lzStream(buf, p) {
  // Decompress one LZ77 stream starting at byte p until source exhaustion.
  let out = Buffer.allocUnsafe(64 << 10);
  let o = 0, flags = 0, bit = 8;
  const grow = (need) => {
    if (need <= out.length) return;
    let n = out.length;
    while (n < need) n *= 2;
    const nb = Buffer.allocUnsafe(n);
    out.copy(nb, 0, 0, o);
    out = nb;
  };
  for (;;) {
    if (bit === 8) {
      if (p >= buf.length) break;
      flags = buf[p++];
      bit = 0;
    }
    if ((flags & (0x80 >> bit++)) === 0) {
      if (p >= buf.length) { p++; break; }
      grow(o + 1);
      out[o++] = buf[p++];
    } else {
      if (p + 2 > buf.length) { p = buf.length; break; }
      const b0 = buf[p++], b1 = buf[p++];
      const off = (((b0 & 0xf) << 8) | b1) + 1;
      const len = (b0 >> 4) + 3;
      if (off > o) break; // end of this stream (terminator / next boundary)
      grow(o + len);
      const src = o - off;
      for (let i = 0; i < len; i++) out[o++] = out[src + i];
    }
  }
  return { out, produced: o, consumed: p };
}

export function unpackCmp(buf) {
  if (buf.subarray(0, 4).toString("latin1") === "pack") {
    // raw (uncompressed) pack — /zu/*.pak etc: same entry format, no LZ77 container
    return parsePack(buf, buf.length);
  }
  if (buf[0] !== 0x10) throw new Error("not a 0x10 container (type=0x" + buf[0].toString(16) + ")");
  // FE9 .cmp = SATU stream LZ77; u24 header = HINT total tapi BOHONG pada sebagian
  // file (mis. /zbg/map02_royal: hint 48145 vs blob actual 1162242) — decode natural,
  // kumpulkan output penuh, JANGAN cap pada hint (bug truncate 21/241 file zmap/zbg).
  const hint = buf.readUInt32BE(0) & 0xffffff;
  const parts = [];
  let p = 4, filled = 0, streams = 0;
  while (p < buf.length) {
    const r = lzStream(buf, p);
    if (r.produced === 0) {
      if (buf[p] === 0x10 && p + 4 <= buf.length) { p += 4; continue; }
      break;
    }
    streams++;
    parts.push(r.out);
    filled += r.produced;
    if (p === r.consumed) break; // no progress guard
    p = r.consumed;
  }
  const data = Buffer.concat(parts);
  if (data.subarray(0, 4).toString("latin1") !== "pack") throw new Error("blob is not a pack");
  return parsePack(data, filled, streams);
}

function parsePack(data, filled, streams = 0) {
  const total = data.length;
  const count = data.readUInt16BE(4);
  const base = data.readUInt32BE(0x10 + 8); // first file data pointer (info)
  const entries = [];
  for (let i = 0; i < count; i++) {
    const q = 8 + i * 0x10;
    let name = "";
    const nameOff = data.readUInt32BE(q + 4);
    for (let k = nameOff; k + 0 < data.length && data[k]; k++) name += String.fromCharCode(data[k]);
    entries.push({
      zero: data.readUInt32BE(q),
      name,
      dataOff: data.readUInt32BE(q + 8),
      len: data.readUInt32BE(q + 12),
    });
  }
  const need = entries.length ? Math.max(...entries.map((e) => e.dataOff + e.len)) : 0;
  if (filled < need) console.error(`cmp: only ${filled}/${need} needed bytes recovered`);
  return { data, entries, base, total, streams, filled };
}

export async function extractDir(inPath, outDir, prefix = "") {
  const { data, entries } = unpackCmp(await readFile(inPath));
  let n = 0;
  for (const e of entries) {
    if (prefix && !e.name.toLowerCase().startsWith(prefix.toLowerCase())) continue;
    const path = join(outDir, e.name.replace(/\\/g, "/"));
    await mkdir(dirname(path), { recursive: true });
    const view = data.subarray(Math.min(e.dataOff, data.length), Math.min(e.dataOff + e.len, data.length));
    await writeFile(path, view);
    n++;
    console.log(`  ${e.name}  ${view.length} B @0x${e.dataOff.toString(16)}`);
  }
  console.log(`extracted ${n}/${entries.length} -> ${outDir}`);
  return entries;
}

if (process.argv[1] && import.meta.url === new URL("file:///" + resolve(process.argv[1]).replace(/\\/g, "/")).href) {
  const [cmd, ...rest] = process.argv.slice(2) || [];
  if (cmd === "ls") {
    const { entries } = unpackCmp(await readFile(rest[0]));
    for (const e of entries) console.log(`${String(e.len).padStart(9)}  0x${e.dataOff.toString(16)}  ${e.name}`);
  } else if (cmd === "extract") {
    await extractDir(rest[0], rest[1], rest[2] || "");
  } else {
    console.error("usage: node cmp.mjs ls <file.cmp> | extract <file.cmp> <outdir> [namePrefix]");
    process.exitCode = 2;
  }
}
