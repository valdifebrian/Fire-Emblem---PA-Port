#!/usr/bin/env node
// gcf.mjs — Fire Emblem .gcf font decoder [AM-024][BRV-FNT-001]
//
// Format (all big-endian), verified against fe_font/talk/alpha/system/bigkana.gcf:
//   0x00 u8  magic 0x20
//   0x08 u16 ascent   0x0a u16 descent   0x0c u16 line height
//   0x14 u32 sheet bytes (0x2000 = 128x128 I4, 0x8000 = 256x256 I4)
//   0x18 u32 sheet count           0x1c u32 sheet width = height (px)
//   0x22 u16 metrics offset        0x24 u32 metrics end = glyph region start
//   0x28 u32 glyph region bytes (sheet count x sheet bytes)
// Metrics: records of 16 bytes:
//   [0]  u16 code (ASCII or Shift-JIS)
//   [2]  u16 sheet index            -> region base = metrics_end + sheet * sheet_bytes
//   [4]  u16 x  [6] u16 y            -> ink top-left in texture coords (px)
//   [8]  u8  w  [9]  u8 h            -> ink size
//   [10] s8  dx [11] u8 dy           -> pen offsets
//   [12] u8  adv                    -> advance
//   [13..15] zero
//   trailing records with code == 0 are padding.
//   NOTE fe_font.gcf: A-Z records (w42 h28) are placeholder junk; that font is
//   digits/punctuation/lowercase only.
//
// Glyph texture = plain GX I4 (4-bit intensity), GX_TL_NONE, standard 8x8 block
// raster (block pitch = width/8 blocks, rows contiguous inside block, MSB pixel
// first). No compression, no slicing:
//   byte = region + sheet*sb + ((y>>3)*W8 + (x>>3))*32 + (y&7)*4 + (x>>1 & 3)
//   nib  = (x&1) ? byte&0xf : byte>>4 ; lum = nib*17
//
// .cms variant (system.cms): [u8 tag][u24 size?] then raw LZ77 (cmp.mjs
// lzStream) with no stream header; the decompressed stream is a full .gcf.

import fs from "fs";
import path from "path";
import { lzStream } from "./cmp.mjs";

export function gcfParse(buf) {
  const u16 = (o) => buf.readUInt16BE(o);
  const u32 = (o) => buf.readUInt32BE(o);
  if (u32(0) !== 0x20) throw new Error("not a .gcf (magic " + u32(0).toString(16) + ")");
  const hdr = {
    ascent: u16(0x08), descent: u16(0x0a), lineH: u16(0x0c),
    sheetBytes: u32(0x14), sheetCount: u32(0x18), texW: u32(0x1c),
    metricsOff: u16(0x22), metricsEnd: u32(0x24), regionBytes: u32(0x28),
  };
  const recs = [];
  for (let o = hdr.metricsOff; o + 16 <= hdr.metricsEnd; o += 16) {
    const code = u16(o);
    const r = {
      code, sheet: u16(o + 2), x: u16(o + 4), y: u16(o + 6),
      w: buf[o + 8], h: buf[o + 9],
      dx: buf[o + 10] << 24 >> 24, dy: buf[o + 11], adv: buf[o + 12],
    };
    if (r.w > 0x20 || r.h > 0x20) r.placeholder = true; // junk A-Z cells
    recs.push(r);
  }
  while (recs.length && recs[recs.length - 1].code === 0) recs.pop();

  const W8 = hdr.texW / 8;
  const base = hdr.metricsEnd;
  const pixel = (x, y, sheet) => {
    const off = base + sheet * hdr.sheetBytes +
      ((y >> 3) * W8 + (x >> 3)) * 32 + (y & 7) * 4 + ((x & 7) >> 1);
    if (off < 0 || off >= buf.length) return 0;
    const b = buf[off];
    return ((x & 1) ? (b & 15) : (b >> 4)) * 17;
  };
  const byCode = new Map();
  for (const r of recs) if (!byCode.has(r.code)) byCode.set(r.code, r);
  return { hdr, recs, pixel, byCode, buf };
}

// system.cms -> decompressed .gcf buffer
export function cmsToGcf(buf) {
  const r = lzStream(buf, 4);
  const out = Buffer.from(r.out.buffer, r.out.byteOffset, r.produced);
  if (out.readUInt32BE(0) !== 0x20) throw new Error("cms payload not .gcf");
  return out;
}

const ramp = (v) => v > 220 ? "@" : v > 160 ? "#" : v > 96 ? "+" : v > 32 ? "-" : ".";

export function glyphAscii(f, code) {
  const r = f.byCode.get(code);
  if (!r) return null;
  const lines = [];
  for (let j = 0; j < r.h; j++) {
    let s = "";
    for (let i = 0; i < r.w; i++) s += ramp(f.pixel(r.x + i, r.y + j, r.sheet));
    lines.push(s);
  }
  return lines;
}

export function sheetPgm(f, sheet) {
  const { texW } = f.hdr;
  const pgm = Buffer.alloc(16 + texW * texW); // header <=16B
  let n = pgm.write(`P5\n${texW} ${texW}\n255\n`);
  for (let y = 0; y < texW; y++)
    for (let x = 0; x < texW; x++) pgm[n++] = f.pixel(x, y, sheet);
  return pgm.subarray(0, n);
}

// CLI
if (process.argv[1] && path.resolve(process.argv[1]) === path.resolve(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1"))) {
  const args = process.argv.slice(2);
  const mode = args[0];
  const files = [];
  let optName = null;
  const opts = {};
  for (const a of args.slice(1)) {
    if (optName) { opts[optName] = a; optName = null; continue; }
    if (a.startsWith("--")) optName = a.slice(2);
    else files.push(a);
  }
  const opt = (k) => opts[k] ?? null;
  const codes = (opt("codes") || "").split(",").filter(Boolean).map((s) => parseInt(s, 16));
  const outDir = opt("out");
  const cms = !!opts.cms;
  for (let file of files) {
    let raw = fs.readFileSync(file);
    let label = path.basename(file);
    if (/\.cms$/i.test(file) || cms) raw = cmsToGcf(raw);
    const f = gcfParse(raw);
    console.log(`== ${label}: sheets=${f.hdr.sheetCount} tex=${f.hdr.texW}px sheetBytes=0x${f.hdr.sheetBytes.toString(16)} metrics=${f.recs.length} asc/desc=${f.hdr.ascent}/${f.hdr.descent}`);
    if (mode === "info") continue;
    if (outDir) {
      fs.mkdirSync(outDir, { recursive: true });
      const stem = label.replace(/\.[^.]+$/, "");
      fs.writeFileSync(path.join(outDir, stem + "_metrics.csv"),
        "code,code_hex,sheet,x,y,w,h,dx,dy,adv\n" + f.recs.map((r) =>
          [r.code, r.code.toString(16), r.sheet, r.x, r.y, r.w, r.h, r.dx, r.dy, r.adv].join(",")).join("\n"));
      for (let s = 0; s < f.hdr.sheetCount; s++)
        fs.writeFileSync(path.join(outDir, `${stem}_sheet${s}.pgm`), sheetPgm(f, s));
      console.log(`   wrote ${stem}_metrics.csv + ${f.hdr.sheetCount} sheet PGMs -> ${outDir}`);
    }
    for (const c of codes) {
      const g = glyphAscii(f, c);
      if (!g) { console.log(`   0x${c.toString(16)}: absent`); continue; }
      const r = f.byCode.get(c);
      console.log(`   0x${c.toString(16)} '${c > 255 ? "SJIS" : String.fromCharCode(c)}' ${r.w}x${r.h} adv${r.adv} sheet${r.sheet}:`);
      for (const l of g) console.log("     " + l);
    }
  }
}
