// tpl2png.mjs — decode GFEE01 .tpl (magic 0x0020AF30, GC block-tiled) → RGBA PNGs.
// Format per mila (thane98, exalt): header [u32 magic][u32 count][count×u32 itemPtr];
// item = [u32 imgPtr][u32 palPtr]; img = [u16 h][u16 w][u32 fmt][u32 dataPtr][u32 wrapS]
// [u32 wrapT][u32 minF][u32 magF][f32 lodBias][u8×4]; pal = [u16 n][u8 unpacked][u8 pad]
// [u32 fmt][u32 dataPtr]. Pixels live in blocks (per-format block dims) laid linear;
// block stream -> rows via block_to_sequential (texture_utils.rs). Full GX decode here.
import { readFile, writeFile, mkdir } from "fs/promises";
import { deflateSync } from "zlib";

// ---------- PNG ----------
const CRC = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c; } return t; })();
const crc32 = (b) => { let c = ~0; for (const x of b) c = CRC[(c ^ x) & 0xff] ^ (c >>> 8); return ~c >>> 0; };
const chunk = (type, data) => { const len = Buffer.alloc(4); len.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const cr = Buffer.alloc(4); cr.writeUInt32BE(crc32(td)); return Buffer.concat([len, td, cr]); };
export function encodePng(w, h, rgba) {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; ihdr[9] = 6; // 8-bit RGBA
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) {
    raw[y * (w * 4 + 1)] = 0;
    rgba.copy(raw, y * (w * 4 + 1) + 1, y * w * 4, (y + 1) * w * 4);
  }
  return Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk("IHDR", ihdr), chunk("IDAT", deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]);
}

// ---------- GX color ----------
const mul = (x, n, m) => (x * n / (m - 1)) | 0; // scale x*(255/(m-1)) truncated like GX tables approx
const r565 = (v) => [mul(v >> 11 & 31, 255, 32), mul(v >> 5 & 63, 255, 64), mul(v & 31, 255, 32), 255];
const r5a3 = (v) => v & 0x8000
  ? [((v >> 10) & 31) << 3, mul((v >> 7) & 7, 255, 8), mul(v & 15, 255, 16), 255]
  : [mul((v >> 8) & 15, 255, 16), mul((v >> 4) & 15, 255, 16), mul(v & 15, 255, 16), mul((v >> 12) & 7, 255, 8)];

const BLK = { 0: [8, 8], 1: [8, 4], 2: [8, 4], 3: [4, 4], 4: [4, 4], 5: [4, 4], 6: [4, 4], 8: [8, 8], 9: [8, 4], 10: [4, 4], 14: [8, 8] }; // fmt -> [bw,bh]
const BPP = { 0: 0.5, 1: 1, 2: 1, 3: 2, 4: 2, 5: 2, 6: 4, 8: 0.5, 9: 1, 10: 2, 14: 0.5 };
const palFmt = (f, v) => f === 0 ? [(v >> 8) & 255, (v >> 8) & 255, (v >> 8) & 255, v & 255] : f === 1 ? r565(v) : r5a3(v);

function decodeFmt(fmt, w, h, data, pal) {
  const [bw, bh] = BLK[fmt];
  const aw = Math.ceil(w / bw) * bw, ah = Math.ceil(h / bh) * bh;
  const out = Buffer.alloc(w * h * 4);
  const px = (x, y, r, g, b, a) => { if (x < w && y < h) { const o = (y * w + x) * 4; out[o] = r; out[o + 1] = g; out[o + 2] = b; out[o + 3] = a; } };
  // byte/unit address of pixel (x,y) in block-linear stream, returns offset into
  // "unit stream" where 1 unit = sizePerPixel bytes handled per format below.
  const blockLinear = (x, y, size) => {
    const bpr = aw / bw, bl = Math.floor(x / bw) + Math.floor(y / bh) * bpr;
    return (bl * bw * bh + (y % bh) * bw + (x % bw)) * size;
  };
  const addr4 = (x, y) => { // byte offset + nibble for 4bpp (8x8 block, 32B)
    const bpr = aw / bw;
    const bl = Math.floor(x / bw) + Math.floor(y / bh) * bpr;
    return { o: bl * 32 + (y % bh) * 4 + ((x % bw) >> 1), lo: (x & 1) === 1 };
  };
  const u16 = (o) => (data[o] << 8) | data[o + 1];
  for (let y = 0; y < ah; y++) for (let x = 0; x < aw; x++) {
    switch (fmt) {
      case 0: { // I4
        const { o, lo } = addr4(x, y);
        const b = data[o] ?? 0;
        const v = (lo ? b & 15 : b >> 4) * 17; px(x, y, v, v, v, 255); break;
      }
      case 1: { const o = blockLinear(x, y, 1); const v = data[o] ?? 0; px(x, y, v, v, v, 255); break; }
      case 2: { const o = blockLinear(x, y, 1); const b = data[o] ?? 0; const i = (b >> 4) * 17, a = (b & 15) * 17; px(x, y, i, i, i, a); break; }
      case 3: { const o = blockLinear(x, y, 2); const b = u16(o); px(x, y, b >> 8, b >> 8, b >> 8, b & 255); break; }
      case 4: { const o = blockLinear(x, y, 2); const c = r565(u16(o)); px(x, y, ...c); break; }
      case 5: { const o = blockLinear(x, y, 2); const c = r5a3(u16(o)); px(x, y, ...c); break; }
      case 6: { const o = blockLinear(x, y, 4); px(x, y, data[o], data[o + 1], data[o + 2], data[o + 3]); break; }
      case 8: case 9: {
        let idx;
        if (fmt === 9) { idx = data[blockLinear(x, y, 1)] ?? 0; }
        else { const { o, lo } = addr4(x, y); const b = data[o] ?? 0; idx = lo ? b & 15 : b >> 4; }
        const p = pal && pal[idx] ? pal[idx] : [0, 0, 0, 0]; px(x, y, ...p); break;
      }
      case 10: break; // CI14X2 unused by window textures
      case 14: { // CMPR (DXT1): 8x8 block = 4 sub-blocks linear, 8B each; GC swaps c0/c1 order
        const bpr = aw / 8;
        const bl = Math.floor(x / 8) + Math.floor(y / 8) * bpr;
        const sub = ((y & 7) >> 2) * 2 + ((x & 7) >> 2);
        const q = bl * 32 + sub * 8;
        const c0 = u16(q), c1 = u16(q + 1);
        const idx = (data[q + 2 + (y & 3)] >> ((3 - (x & 3)) * 2)) & 3;
        const A = r565(c0), B = r565(c1);
        let p;
        if (idx === 0) p = A;
        else if (idx === 1) p = B;
        else if (c0 > c1) p = idx === 2 ? [(2 * A[0] + B[0]) >> 1, (2 * A[1] + B[1]) >> 1, (2 * A[2] + B[2]) >> 1, 255] : [(A[0] + 2 * B[0]) >> 1, (A[1] + 2 * B[1]) >> 1, (A[2] + 2 * B[2]) >> 1, 255];
        else p = idx === 2 ? [(A[0] + B[0]) >> 1, (A[1] + B[1]) >> 1, (A[2] + B[2]) >> 1, 255] : [0, 0, 0, 0];
        px(x, y, ...p); break;
      }
    }
  }
  return out;
}

// ---------- TPL parse ----------
export function parseTpl(d, name = "tex") {
  if (d.readUInt32BE(0) !== 0x0020af30) throw new Error("not tpl: 0x" + d.readUInt32BE(0).toString(16));
  const u32 = (o) => d.readUInt32BE(o);
  const count = u32(4); // u32@8 = table offset (=0x0C); table = count x (u32 imgPtr, u32 palPtr)
  const tableOff = u32(8);
  const out = [];
  for (let i = 0; i < count; i++) {
    const ip = u32(tableOff + i * 8);
    const pp = u32(tableOff + i * 8 + 4);
    const h = d.readUInt16BE(ip), w = d.readUInt16BE(ip + 2), fmt = u32(ip + 4), dp = u32(ip + 8);
    let pal = null;
    if (pp !== 0) {
      const pn = d.readUInt16BE(pp), pf = u32(pp + 4), pdp = u32(pp + 8);
      pal = [];
      for (let j = 0; j < pn; j++) pal.push(palFmt(pf, (d[pdp + j * 2] << 8) | d[pdp + j * 2 + 1]));
    }
    let data;
    try { data = decodeFmt(fmt, w, h, d.subarray(dp), pal); } catch (e) { data = null; console.log(`${name}#${i}: decode fail fmt=${fmt} ${e.message}`); }
    out.push({ name, idx: i, w, h, fmt, ok: !!data, rgba: data });
  }
  return out;
}

const [, , mode, ...rest] = process.argv;
const { pathToFileURL } = await import("url");
const isMain = process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href;
if (isMain && mode === "png" && rest.length >= 1) {
  const { lz77ExDecode } = await import("./lz77.mjs");
  const files = rest;
  for (const f of files) {
    let d = await readFile(f);
    if (d[0] === 0x10) d = lz77ExDecode(d); // .cms = LZ77-compressed tpl
    const base = f.split("/").pop().replace(/\.cms$|\.tpl$/i, "");
    const texs = parseTpl(d, base);
    console.log(`${f}: ${texs.length} images`);
    let n = 0;
    for (const t of texs) {
      if (!t.ok) continue;
      const outp = `out/png/${base}_${t.idx}_${t.w}x${t.h}_f${t.fmt}.png`;
      await mkdir("out/png", { recursive: true });
      await writeFile(outp, encodePng(t.w, t.h, t.rgba));
      n++;
    }
    console.log(`  -> ${n} png in out/png/`);
  }
} else if (isMain && mode === "face" && rest.length === 1) {
  // node bin/tpl2png.mjs face <outdir> — extract+decode all /Face/*.cms from disc
  const { openRvz, readFST, walkFiles } = await import("./rvz.mjs");
  const { lz77ExDecode } = await import("./lz77.mjs");
  const rvzPath = process.env.FE9_RVZ;
  if (!rvzPath) throw new Error("set FE9_RVZ");
  const r = await openRvz(rvzPath);
  const gc = Buffer.from(await r.read(0, 0x440));
  const root = await readFST(r, gc.readUInt32BE(0x424), gc.readUInt32BE(0x428));
  const files = await walkFiles(root, "", []);
  const faces = files.filter((f) => /^\/Face\/.*\.cms$/i.test(f.path));
  const outDir = rest[0];
  await mkdir(outDir, { recursive: true });
  console.log(`${faces.length} face files`);
  let done = 0;
  for (const fe of faces) {
    const raw = Buffer.from(await r.read(fe.pos, fe.size));
    let d;
    try { d = lz77ExDecode(raw); } catch (e) { console.log(`lz fail ${fe.path}: ${e.message}`); continue; }
    const base = fe.path.split("/").pop().replace(/\.cms$/i, "");
    let texs;
    try { texs = parseTpl(d, base); } catch (e) { console.log(`tpl fail ${fe.path}: ${e.message}`); continue; }
    for (const t of texs) if (t.ok) await writeFile(`${outDir}/${base}_${t.idx}.png`, encodePng(t.w, t.h, t.rgba));
    done++;
    if (done % 25 === 0) console.log(`  ${done}/${faces.length}`);
  }
  console.log(`done: ${done} portraits -> ${outDir}`);
} else if (isMain) {
  console.error("usage: node bin/tpl2png.mjs png <files...> | face <outdir>");
}
