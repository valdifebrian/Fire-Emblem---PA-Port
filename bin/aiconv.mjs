// aiconv.mjs — PNG RGBA -> native tile format (TPL / .cms) for the FE9 port engine.
// Counterpart of tpl2png.mjs (decode); this is the ENCODE direction, required so
// commercial builds can replace rights-holder art asset-for-asset without touching
// logic (PoC loads overrides at runtime: see docs/FPK_FORMAT.md §override).
//
// Layout authority = tpl2png.mjs parseTpl decode (byte-verified vs GFEE01 files):
//   TPL  BE: [u32 0x20AF30][u32 count][u32 tableOff=0x0C]
//          table[i] = (u32 imgHdr, u32 palHdr|0)
//          imgHdr = [u16 h][u16 w][u32 fmt][u32 dataOff][u32 wrapS][u32 wrapT]
//                   [u32 minF][u32 magF][f32 lodBias][u8 x4]          (36 B)
//          palHdr = [u16 n][u8 unpacked=0][u8 pad=0][u32 palFmt][u32 palData] (8 B)
//          pixels in GX block-linear (or addr4 for 4bpp); offsets 16B-aligned.
//   .cms = LZ77-0x10 stream over a TPL (same decoder as texfile.h lz77_0x10).
// Formats written (all decodable by poc/app/src/main/cpp/cmsdec.h fe9tex_decode):
//   1 I8 · 3 IA8 · 5 RGB5A3 · 6 RGBA8 · 8 C4(+pal16) · 9 C8(+pal256) · 14 CMPR
import { readFile, writeFile } from "fs/promises";
import { inflateSync, deflateSync } from "zlib";

// ---------------- PNG decode (RGBA8) ----------------
function paeth(a, b, c) {
  const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
  return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}
export function decodePng(buf) {
  if (buf.readUInt32BE(0) !== 0x89504e47) throw new Error("not png");
  let p = 8, w = 0, h = 0, ct = 0, bd = 0, idat = [];
  let plte = null, trns = null;
  while (p < buf.length) {
    const len = buf.readUInt32BE(p), type = buf.toString("latin1", p + 4, p + 8);
    const data = buf.subarray(p + 8, p + 8 + len);
    if (type === "IHDR") { w = data.readUInt32BE(0); h = data.readUInt32BE(4); bd = data[8]; ct = data[9];
      if (data[12] !== 0) throw new Error("interlaced png unsupported");
      if (bd !== 8 && bd !== 16) throw new Error("bit depth " + bd + " unsupported"); }
    else if (type === "IDAT") idat.push(data);
    else if (type === "PLTE") plte = data;
    else if (type === "tRNS") trns = data;
    p += 12 + len;
  }
  const raw = inflateSync(Buffer.concat(idat));
  const ch = ct === 6 ? 4 : ct === 2 ? 3 : 1;
  const bpp = (ch * bd) >> 3;
  const stride = Math.ceil((w * ch * bd) / 8);
  const out = Buffer.alloc(w * h * 4);
  let prev = Buffer.alloc(stride), q = 0;
  for (let y = 0; y < h; y++) {
    const f = raw[q++];
    const line = Buffer.from(raw.subarray(q, q + stride)); q += stride;
    for (let i = 0; i < stride; i++) {
      const a = i >= bpp ? line[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
      if (f === 1) line[i] = (line[i] + a) & 255;
      else if (f === 2) line[i] = (line[i] + b) & 255;
      else if (f === 3) line[i] = (line[i] + ((a + b) >> 1)) & 255;
      else if (f === 4) line[i] = (line[i] + paeth(a, b, c)) & 255;
      else if (f !== 0) throw new Error("bad filter " + f);
    }
    for (let x = 0; x < w; x++) {
      let r, g, b, a;
      const o = x * bpp;
      if (ct === 6) { r = line[o]; g = line[o + 1]; b = line[o + 2]; a = bd === 16 ? line[o + 4] : line[o + 3]; }
      else if (ct === 2) { r = line[o]; g = line[o + 1]; b = line[o + 2]; a = 255;
        if (bd === 16) { r = line[o]; g = line[o + 2]; b = line[o + 4]; } }
      else if (ct === 0) { r = g = b = bd === 16 ? line[o] : line[o]; a = 255; }
      else { // palette
        const idx = ct === 3 && bd !== 8 ? 0 : line[o];
        if (!plte) throw new Error("png missing PLTE");
        r = plte[idx * 3]; g = plte[idx * 3 + 1]; b = plte[idx * 3 + 2];
        a = trns && idx < trns.length ? trns[idx] : 255;
      }
      const d = (y * w + x) * 4;
      out[d] = r; out[d + 1] = g; out[d + 2] = b; out[d + 3] = a;
    }
    prev = line;
  }
  return { w, h, rgba: out };
}

// ---------------- GX encode primitives ----------------
const q565 = (r, g, b) => ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
const d565 = (v) => [((v >> 11) & 31) << 3, ((v >> 5) & 63) << 2, (v & 31) << 3];
const q5a3 = (r, g, b, a) => a <= 224
  ? ((a >> 5) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)
  : 0x8000 | ((r >> 3) << 10) | ((g >> 5) << 7) | (b >> 4); // opaque flag + RGB
// decode-side inverse for round-trip honesty (matches palFmt/r5a3 truncation):
const d5a3 = (v) => v & 0x8000
  ? [((v >> 10) & 31) << 3, ((v >> 7) & 7) * 255 / 7 | 0, (v & 15) * 255 / 15 | 0, 255]
  : [((v >> 8) & 15) * 255 / 15 | 0, ((v >> 4) & 15) * 255 / 15 | 0, (v & 15) * 255 / 15 | 0, ((v >> 12) & 7) * 255 / 7 | 0];

const blOff = (x, y, aw, bw, bh, size) => {
  const bpr = aw / bw, bl = ((x / bw) | 0) + ((y / bh) | 0) * bpr;
  return (bl * bw * bh + (y % bh) * bw + (x % bw)) * size;
};
const a4Off = (x, y, aw, bw, bh) => {
  const bpr = aw / bw, bl = ((x / bw) | 0) + ((y / bh) | 0) * bpr;
  return { o: bl * 32 + (y % bh) * 4 + ((x % bw) >> 1), lo: (x & 1) === 1 };
};

function medianCut(rgba, n) {
  const pix = [];
  for (let i = 0; i < rgba.length; i += 4)
    if (rgba[i + 3] > 8) pix.push([rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]]);
  if (!pix.length) pix.push([0, 0, 0, 0]);
  const mean = (b) => {
    let r = 0, g = 0, bA = 0, a = 0;
    for (const p of b.px) { r += p[0]; g += p[1]; bA += p[2]; a += p[3]; }
    const m = b.px.length;
    return [r / m | 0, g / m | 0, bA / m | 0, a / m | 0];
  };
  let boxes = [{ px: pix }];
  while (boxes.length < n) {
    boxes.sort((A, B) => {
      const rg = (b) => { let lo = 999, hi = -1;
        for (const ch of [0, 1, 2]) { let l2 = 999, h2 = -1;
          for (const p of b.px) { if (p[ch] < l2) l2 = p[ch]; if (p[ch] > h2) h2 = p[ch]; }
          lo = Math.min(lo, l2); hi = Math.max(hi, h2); }
        return (hi - lo) * Math.log2(b.px.length + 2); };
      return rg(B) - rg(A);
    });
    const b = boxes.find((x) => x.px.length > 1);
    if (!b) break;
    // widest channel
    let chBest = 0, rBest = -1;
    for (let ch = 0; ch < 4; ch++) {
      let lo = 999, hi = -1;
      for (const p of b.px) { if (p[ch] < lo) lo = p[ch]; if (p[ch] > hi) hi = p[ch]; }
      if (hi - lo > rBest) { rBest = hi - lo; chBest = ch; }
    }
    b.px.sort((p, q) => p[chBest] - q[chBest]);
    const mid = b.px.length >> 1;
    const i = boxes.indexOf(b);
    boxes.splice(i, 1, { px: b.px.slice(0, mid) }, { px: b.px.slice(mid) });
  }
  const pal = [];
  for (const b of boxes) pal.push(mean(b));
  while (pal.length < n) pal.push([0, 0, 0, 0]);
  return pal;
}

// encode RGBA -> {fmt, data, palette?} — palette = RGBA array, encoded as RGB5A3 u16s
export function encodePixels(w, h, rgba, palMode) {
  let fmt, data;
  const lum = (i) => (rgba[i] * 77 + rgba[i + 1] * 151 + rgba[i + 2] * 28) >> 8;
  let allOpaque = true, allGray = true;
  for (let i = 0; i < rgba.length; i += 4) {
    if (rgba[i + 3] !== 255) allOpaque = false;
    if (!(Math.abs(rgba[i] - rgba[i + 1]) < 3 && Math.abs(rgba[i + 1] - rgba[i + 2]) < 3)) allGray = false;
  }
  if (palMode === "auto")
    palMode = allGray ? (allOpaque ? "gray" : "ia8") : (allOpaque ? "rgb5a3" : "rgba8");
  const [bw, bh] = { gray: [8, 4], ia8: [4, 4], rgb5a3: [4, 4], rgba8: [4, 4], c4: [8, 8], c8: [8, 4], cmpr: [8, 8] }[palMode];
  const aw = Math.ceil(w / bw) * bw, ah = Math.ceil(h / bh) * bh;
  if (palMode === "gray") {
    fmt = 1; data = Buffer.alloc(aw * ah);
    for (let y = 0; y < ah; y++) for (let x = 0; x < aw; x++)
      if (x < w && y < h) data[blOff(x, y, aw, 8, 4, 1)] = rgba[(y * w + x) * 4]; // pure I8 value
  } else if (palMode === "ia8") {
    fmt = 3; data = Buffer.alloc(aw * ah * 2);
    for (let y = 0; y < ah; y++) for (let x = 0; x < aw; x++) {
      const o = blOff(x, y, aw, 4, 4, 2), i = (y * w + x) * 4;
      if (x < w && y < h) { data.writeUInt16BE((rgba[i] << 8) | rgba[i + 3], o); }
    }
  } else if (palMode === "rgb5a3") {
    fmt = 5; data = Buffer.alloc(aw * ah * 2);
    for (let y = 0; y < ah; y++) for (let x = 0; x < aw; x++) {
      const o = blOff(x, y, aw, 4, 4, 2), i = (y * w + x) * 4;
      if (x < w && y < h) data.writeUInt16BE(q5a3(rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]), o);
    }
  } else if (palMode === "rgba8") {
    fmt = 6; data = Buffer.alloc(aw * ah * 4);
    for (let y = 0; y < ah; y++) for (let x = 0; x < aw; x++) {
      const o = blOff(x, y, aw, 4, 4, 4), i = (y * w + x) * 4;
      if (x < w && y < h) { data[o] = rgba[i]; data[o + 1] = rgba[i + 1]; data[o + 2] = rgba[i + 2]; data[o + 3] = rgba[i + 3]; }
    }
  } else if (palMode === "c4" || palMode === "c8") {
    const n = palMode === "c4" ? 16 : 256;
    fmt = palMode === "c4" ? 8 : 9;
    const pal = medianCut(rgba, n);
    data = Buffer.alloc(palMode === "c4" ? (aw * ah) >> 1 : aw * ah);
    for (let y = 0; y < ah; y++) for (let x = 0; x < aw; x++) {
      const i = (y * w + x) * 4;
      let best = 0, bd = 1e18;
      if (x < w && y < h)
        for (let k = 0; k < n; k++) {
          const dr = rgba[i] - pal[k][0], dg = rgba[i + 1] - pal[k][1],
                db = rgba[i + 2] - pal[k][2], da = rgba[i + 3] - pal[k][3];
          const d = dr * dr + dg * dg + db * db + da * da * 0.25;
          if (d < bd) { bd = d; best = k; }
        }
      if (palMode === "c4") {
        const { o, lo } = a4Off(x, y, aw, 8, 8);
        if (lo) data[o] = (data[o] & 0xf0) | best; else data[o] = (data[o] & 0x0f) | (best << 4);
      } else data[blOff(x, y, aw, 8, 4, 1)] = best;
    }
    return { fmt, data, palette: pal };
  } else { // cmpr (DXT1-GX): opaque only (FE9 decoder = idx3 OPAQUE midpoint)
    fmt = 14; data = Buffer.alloc((aw >> 3) * (ah >> 3) * 32);
    const bpr = aw / 8;
    for (let by = 0; by < ah; by += 8) for (let bx = 0; bx < aw; bx += 8) {
      const tile = (bx / 8 + (by / 8) * bpr) * 32;
      for (let sy = 0; sy < 8; sy += 4) for (let sx = 0; sx < 8; sx += 4) {
        const q = tile + (((sy >> 2) * 2) + (sx >> 2)) * 8;
        let lo = 0x7fff, hi = 0, loL = 1e9, hiL = -1;
        for (let y = 0; y < 4; y++) for (let x = 0; x < 4; x++) {
          const i = ((by + sy + y) * w + Math.min(bx + sx + x, w - 1)) * 4;
          const rr = bx + sx + x < w && by + sy + y < h ? rgba[i] : 0,
                gg = bx + sx + x < w && by + sy + y < h ? rgba[i + 1] : 0,
                bb = bx + sx + x < w && by + sy + y < h ? rgba[i + 2] : 0;
          const v = q565(rr, gg, bb), L = 77 * rr + 151 * gg + 28 * bb;
          if (L < loL) { loL = L; lo = v; } if (L > hiL) { hiL = L; hi = v; }
        }
        let c0 = hi, c1 = lo;
        if (c0 === c1) c1 = hi === 0 ? 1 : hi - 1; // keep c0>c1 numeric -> 4-opaque mode
        if (c0 < c1) { const t = c0; c0 = c1; c1 = t; }
        data.writeUInt16BE(c0, q); data.writeUInt16BE(c1, q + 2);
        const A = d565(c0), B = d565(c1);
        const cols = [A, B,
          [(3 * B[0] + 5 * A[0]) >> 3, (3 * B[1] + 5 * A[1]) >> 3, (3 * B[2] + 5 * A[2]) >> 3],
          [(3 * A[0] + 5 * B[0]) >> 3, (3 * A[1] + 5 * B[1]) >> 3, (3 * A[2] + 5 * B[2]) >> 3]];
        for (let y = 0; y < 4; y++) {
          let byte = 0;
          for (let x = 0; x < 4; x++) {
            const gi = ((by + sy + y) * w + Math.min(bx + sx + x, w - 1)) * 4;
            const rr = bx + sx + x < w && by + sy + y < h ? rgba[gi] : 0,
                  gg = bx + sx + x < w && by + sy + y < h ? rgba[gi + 1] : 0,
                  bb = bx + sx + x < w && by + sy + y < h ? rgba[gi + 2] : 0;
            let best = 0, bd = 1e18;
            for (let k = 0; k < 4; k++) {
              const dr = rr - cols[k][0], dg = gg - cols[k][1], db = bb - cols[k][2];
              const d = dr * dr + dg * dg + db * db;
              if (d < bd) { bd = d; best = k; }
            }
            byte |= best << ((3 - x) * 2);
          }
          data[q + 4 + y] = byte;
        }
      }
    }
    if (!allOpaque) console.log("aiconv: WARN cmpr ignores alpha (FE9 decoder has no CMPR alpha)");
  }
  return { fmt, data, palette: null };
}

// ---------------- LZ77-0x10 encode (pairs with texfile.h lz77_0x10) ----------------
export function lz77_0x10_encode(src) {
  const out = [];
  const hdr = (0x10 << 24) | ((src.length & 0xffffff)); // C reads be32 & 0xffffff as cap hint
  out.push((hdr >>> 24) & 255, (hdr >>> 16) & 255, (hdr >>> 8) & 255, hdr & 255);
  // hash4 index chains
  const head = new Map(); const prev = new Int32Array(src.length).fill(-1);
  for (let i = 0; i < src.length; i++) {
    const k = src.length - i >= 4 ? (src[i] << 24 | src[i + 1] << 16 | src[i + 2] << 8 | src[i + 3]) : -1;
    if (k >= 0) { prev[i] = head.get(k) ?? -1; head.set(k, i); }
  }
  let i = 0, pend = [];
  const flush = () => {
    if (!pend.length) return;
    while (pend.length) {
      const grp = pend.slice(0, 8); pend = pend.slice(8);
      let flags = 0;
      grp.forEach((g, j) => { if (g.m) flags |= 1 << (7 - j); });
      out.push(flags);
      for (const g of grp) { if (g.m) out.push(g.b0, g.b1); else out.push(g.v); }
    }
  };
  while (i < src.length) {
    let best = 0, bestOff = 0;
    if (src.length - i >= 3) {
      const k = src[i] << 24 | src[i + 1] << 16 | src[i + 2] << 8 | (src.length - i >= 4 ? src[i + 3] : 0);
      let cand = head.get(k);
      let guard = 64;
      while (cand !== undefined && cand !== -1 && cand < i && i - cand <= 4096 && guard--) {
        let l = 0;
        while (l < 18 && i + l < src.length && src[cand + l] === src[i + l]) l++;
        if (l > best) { best = l; bestOff = i - cand; if (l === 18) break; }
        cand = prev[cand];
      }
    }
    if (best >= 3) {
      const b0 = (((best - 3) << 4) | ((bestOff - 1) >> 8)) & 0xff, b1 = (bestOff - 1) & 0xff;
      pend.push({ m: true, b0, b1 });
      i += best;
    } else { pend.push({ m: false, v: src[i] }); i++; }
  }
  flush();
  return Buffer.from(out);
}
/* decoder replica of texfile.h lz77_0x10 (round-trip self-check) */
export function lz77_0x10_decode(buf) {
  if (!buf.length || buf[0] !== 0x10) throw new Error("lz77: no magic");
  let cap = buf.readUInt32BE(0) & 0xffffff; if (cap < 64) cap = 64;
  let ob = Buffer.alloc(cap); let p = 4, o = 0, flags = 0, bit = 8;
  const need = (n) => { if (o + n > ob.length) { let c = ob.length; while (o + n > c) c *= 2; const nb = Buffer.alloc(c); ob.copy(nb); ob = nb; } };
  while (p < buf.length) {
    if (bit === 8) { if (p >= buf.length) break; flags = buf[p++]; bit = 0; }
    if (!((flags >> (7 - bit++)) & 1)) { need(1); if (p >= buf.length) break; ob[o++] = buf[p++]; }
    else {
      if (p + 2 > buf.length) break;
      const b0 = buf[p++], b1 = buf[p++];
      const off = (((b0 & 0x0f) << 8) | b1) + 1, len = (b0 >> 4) + 3;
      if (off > o) throw new Error("lz77 bad off");
      need(len); const src = o - off;
      for (let k = 0; k < len; k++) ob[o++] = ob[src + k];
    }
  }
  return ob.subarray(0, o);
}

// ---------------- TPL assembly ----------------
export function buildTpl(imgs) {
  const align = (v) => (v + 15) & ~15;
  let off = 12 + imgs.length * 8;
  const recs = imgs.map((im) => {
    const r = { im, imgHdr: off }; off += 36;
    if (im.palette) { r.palHdr = off; off += 12; } // [u16 n][u8 unpacked][u8 0][u32 fmt][u32 ptr]
    return r;
  });
  for (const r of recs) { if (r.palHdr) { r.palData = align(off); off = r.palData + (r.im.palette.length ? 2 * r.im.palette.length : 0); } }
  for (const r of recs) { r.data = align(off); off = r.data + r.im.data.length; }
  const buf = Buffer.alloc(off);
  buf.writeUInt32BE(0x20af30, 0); buf.writeUInt32BE(imgs.length, 4); buf.writeUInt32BE(12, 8);
  recs.forEach((r, i) => {
    buf.writeUInt32BE(r.imgHdr, 12 + i * 8); buf.writeUInt32BE(r.palHdr ?? 0, 12 + i * 8 + 4);
    const ip = r.imgHdr;
    buf.writeUInt16BE(r.im.h, ip); buf.writeUInt16BE(r.im.w, ip + 2); // byte-verified FE9 order: h first
    buf.writeUInt32BE(r.im.fmt, ip + 4); buf.writeUInt32BE(r.data, ip + 8);
    buf.writeUInt32BE(0, ip + 12); buf.writeUInt32BE(0, ip + 16); // wrapS/T = repeat
    buf.writeUInt32BE(0, ip + 20); buf.writeUInt32BE(0, ip + 24); // minF/magF = nearest
    buf.writeFloatBE(0, ip + 28); // lodBias
    // ip+32: loopS loopT shiftS shiftT = 0
    if (r.palHdr) {
      buf.writeUInt16BE(r.im.palette.length, r.palHdr);
      buf[r.palHdr + 2] = 0; buf[r.palHdr + 3] = 0;
      buf.writeUInt32BE(2, r.palHdr + 4); // palFmt RGB5A3
      buf.writeUInt32BE(r.palData, r.palHdr + 8);
      r.im.palette.forEach((c, j) => buf.writeUInt16BE(q5a3(c[0], c[1], c[2], c[3]), r.palData + j * 2));
    }
    r.im.data.copy(buf, r.data);
  });
  return buf;
}

// ---------------- decode own output (round-trip check; = engine pipeline) ----------------
function parseTplCheck(d) { // minimal parse mirroring buildTpl layout reader
  if (d.readUInt32BE(0) !== 0x0020af30) throw new Error("out not tpl");
  const n = d.readUInt32BE(4), t = d.readUInt32BE(8), out = [];
  for (let i = 0; i < n; i++) {
    const ip = d.readUInt32BE(t + i * 8), pp = d.readUInt32BE(t + i * 8 + 4);
    const h = d.readUInt16BE(ip), w = d.readUInt16BE(ip + 2), fmt = d.readUInt32BE(ip + 4), dp = d.readUInt32BE(ip + 8);
    let pal = null;
    if (pp) { const pn = d.readUInt16BE(pp), pdp = d.readUInt32BE(pp + 8);
      pal = []; for (let j = 0; j < pn; j++) pal.push(d5a3(d.readUInt16BE(pdp + j * 2))); }
    out.push({ w, h, fmt, dp, pal });
  }
  return out;
}

// ---------------- CLI ----------------
const USAGE = "usage: node bin/aiconv.mjs --in x.png --out y.bin [--fmt tpl|cms] [--pal auto|4bpp|8bpp|cmpr|gray|ia8|rgb5a3|rgba8]";
const isMain = process.argv[1] && import.meta.url === (await import("url")).pathToFileURL(process.argv[1]).href;
if (isMain) {
  const args = {};
  { const a = process.argv.slice(2);
    for (let i = 0; i < a.length; i += 2) {
      if (!a[i].startsWith("--")) { console.error("want --key value, got " + a[i]); process.exit(2); }
      args[a[i].slice(2)] = a[i + 1];
    }
  }
  if (!args.in || !args.out) { console.error(USAGE); process.exit(2); }
  const fmt = args.fmt ?? "tpl";
  const pal = args.pal ?? "auto";
  if (!["tpl", "cms"].includes(fmt)) { console.error("--fmt tpl|cms"); process.exit(2); }
  const png = decodePng(await readFile(args.in));
  const palMode = { auto: "auto", "4bpp": "c4", "8bpp": "c8", cmpr: "cmpr", gray: "gray", ia8: "ia8", rgb5a3: "rgb5a3", rgba8: "rgba8" }[pal];
  if (!palMode) { console.error("bad --pal (auto|4bpp|8bpp|cmpr|gray|ia8|rgb5a3|rgba8)"); process.exit(2); }
  const enc = encodePixels(png.w, png.h, png.rgba, palMode);
  console.log(`aiconv: ${args.in} ${png.w}x${png.h} -> fmt=${enc.fmt} data=${enc.data.length}B${enc.palette ? " pal=" + enc.palette.length : ""}`);
  let bin = buildTpl([{ ...enc, w: png.w, h: png.h }]);
  if (fmt === "cms") {
    const comp = lz77_0x10_encode(bin);
    const back = lz77_0x10_decode(comp);
    if (!back.equals(bin)) { console.error("LZ77 round-trip FAILED"); process.exit(1); }
    console.log(`aiconv: cms lz77 ${bin.length} -> ${comp.length}B (${(100 * comp.length / bin.length) | 0}%)`);
    bin = comp;
  } else {
    parseTplCheck(bin); // sanity: our own writer reads back
  }
  await writeFile(args.out, bin);
  console.log(`aiconv: wrote ${args.out} (${bin.length}B, fmt=${fmt})`);
  process.exit(0);
}
