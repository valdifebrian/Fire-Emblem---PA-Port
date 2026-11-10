// gs.mjs — FE9 body (.gs) parser. Spec: tellius-forge research/body v2.0.1 (FE9 byte-exact).
// Pointers = raw+0x20. Header 0x84; tables pos i16x3/nrm i8x3/uv i16x2/col u8x4 (frac bits
// hdr 0x7C..7E); mat 32B; texref 28B (img idx u16@+4); mesh rec 36B; submesh chain 32B.
// FINDING 2026-11-10 (archer pack): texref.img = indeks gambar; pack punya BEBERAPA tpl
// (tex_4,tex_5). Konvensi loader FE9 (diduga): img index = daftar image SATU tpl yang
// di-bind engine via nama file pack entry (bukan merge-sort semua tpl). Render proof:
// CMPR decode benar (wajah terbaca) tapi body tex_4 & tex_5 sama2 atlas low-res speckle
// (quirk ymu) — binding final img↔tpl menunggu jejak pemanggilan di Ghidra / capture.
// Display list: 0x98,u16 cnt,verts[skin?][pos][nrm][col?][uv][uv2?]. Bone palette 32B.
import { readFile, writeFile } from "fs/promises";
import { pathToFileURL } from "url";

export function parseGs(d, name = "") {
  const u32 = (o) => d.readUInt32BE(o), u16 = (o) => d.readUInt16BE(o), i16 = (o) => d.readInt16BE(o), i8 = (o) => d.readInt8(o);
  const f32 = (o) => d.readFloatBE(o);
  const P = (o) => { const p = u32(o); return p ? p + 0x20 : 0; };
  const fileSize = u32(0), relocOff = P(4), relocCnt = u32(8);
  const bb = [0, 1, 2, 3, 4, 5].map((k) => f32(0x2c + k * 4));
  const off = { pos: P(0x44), nrm: P(0x48), uv: P(0x4c), col: P(0x50), mat: P(0x54), mesh: P(0x58),
    list: [P(0x5c), P(0x60), P(0x64)], blend: P(0x68) };
  const cnt = { pos: u16(0x6c), nrm: u16(0x6e), uv: u16(0x70), col: u16(0x72), mat: u16(0x74), mesh: u16(0x76), sub: u16(0x78) };
  const fb = { pos: d[0x7c], nrm: d[0x7d], uv: d[0x7e] };

  const positions = new Float32Array(cnt.pos * 3);
  for (let i = 0; i < cnt.pos; i++) for (let k = 0; k < 3; k++) positions[i * 3 + k] = i16(off.pos + i * 6 + k * 2) / (1 << fb.pos);
  const normals = new Float32Array(cnt.nrm * 3);
  for (let i = 0; i < cnt.nrm; i++) for (let k = 0; k < 3; k++) normals[i * 3 + k] = i8(off.nrm + i * 3 + k) / (1 << fb.nrm);
  const uvs = new Float32Array(cnt.uv * 2);
  for (let i = 0; i < cnt.uv; i++) for (let k = 0; k < 2; k++) uvs[i * 2 + k] = i16(off.uv + i * 4 + k * 2) / (1 << fb.uv);
  const colors = new Uint8Array(cnt.col * 4);
  if (off.col) colors.set(d.subarray(off.col, off.col + cnt.col * 4));

  const cstr = (o) => { let e = o; while (e < d.length && d[e]) e++; return d.toString("latin1", o, e); };
  const materials = [];
  for (let i = 0; i < cnt.mat; i++) {
    const m = off.mat + i * 32;
    const texRefs = [];
    for (let j = 0; j < d[m + 6]; j++) {
      const tp = u32(m + 0x14) + 0x20 + j * 28;
      texRefs.push({ img: u16(tp + 4), usx: f32(tp + 0x10), usy: f32(tp + 0x14) });
    }
    materials.push({
      name: u32(m) ? cstr(u32(m) + 0x20) : "",
      diffuse: [d[m + 8], d[m + 9], d[m + 10], d[m + 11]],
      color2: [d[m + 0x0c], d[m + 0x0d], d[m + 0x0e], d[m + 0x0f]],
      texRefs,
    });
  }
  const meshes = [];
  for (let i = 0; i < cnt.mesh; i++) {
    const m = off.mesh + i * 36;
    meshes.push({ name: u32(m) ? cstr(u32(m) + 0x20) : "", bboxMin: [f32(m + 4), f32(m + 8), f32(m + 12)], bboxMax: [f32(m + 0x10), f32(m + 0x14), f32(m + 0x18)], bone: u16(m + 0x1c) });
  }
  const submeshes = [];
  for (let li = 0; li < 3; li++) {
    let p = off.list[li];
    while (p) {
      const rec = {
        pass: li, mesh: u32(p) ? u32(p) + 0x20 : 0, flags: u16(p + 8), mat: u16(p + 0x0a), bone: u16(p + 0x0c),
        attr: d[p + 0x12], dl: u32(p + 0x14) ? u32(p + 0x14) + 0x20 : 0, dlSize: u32(p + 0x18), pal: u32(p + 0x1c) ? u32(p + 0x1c) + 0x20 : 0,
      };
      rec.meshIdx = meshes.findIndex((_, k) => off.mesh + k * 36 === rec.mesh);
      if (rec.pal) {
        const n = d[rec.pal + 1];
        rec.palette = [];
        for (let j = 0; j < n; j++) rec.palette.push(d[rec.pal + 2 + j]);
      }
      rec.tris = decodeDL(d, rec);
      submeshes.push(rec);
      p = u32(p + 4) ? u32(p + 4) + 0x20 : 0;
    }
  }
  return { name, fileSize, relocOff, relocCnt, bbox: bb, off, cnt, fb, positions, normals, uvs, colors, materials, meshes, submeshes };
}

// Display list → triangles: each vertex = {p,n,c,u,v2,skin} indices.
export function decodeDL(d, rec) {
  const tris = [];
  let o = rec.dl, v = [];
  const end = rec.dl + rec.dlSize;
  const skin = rec.flags & 0x0002, colA = rec.attr & 0x10, uv2A = rec.attr & 0x80;
  const stride = 2 * (3 + (colA ? 1 : 0) + (uv2A ? 1 : 0)) + (skin ? 1 : 0);
  while (o < end) {
    const op = d[o];
    if (op === 0 && !v.length) { o++; continue; } // 32B pad zeros
    if (op !== 0x98) throw new Error(`submesh pass${rec.pass}: opcode 0x${op.toString(16)} @${o - rec.dl}`);
    const n = d.readUInt16BE(o + 1);
    o += 3;
    const strip = [];
    for (let i = 0; i < n; i++) {
      let q = o;
      const sk = skin ? d[q] : 0; if (skin) q += 1;
      const p = d.readUInt16BE(q); q += 2;
      const nm = d.readUInt16BE(q); q += 2;
      let c = 0; if (colA) { c = d.readUInt16BE(q); q += 2; }
      const u = d.readUInt16BE(q); q += 2;
      let v2 = 0; if (uv2A) { v2 = d.readUInt16BE(q); q += 2; }
      strip.push({ p, n: nm, c, u, v2, skin: sk });
      o += stride;
    }
    for (let i = 2; i < strip.length; i++) tris.push(i % 2 ? [strip[i - 1], strip[i], strip[i - 2]] : [strip[i - 2], strip[i - 1], strip[i]]);
  }
  return tris;
}

const isMain = process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href;
if (isMain && process.argv[2] === "json") {
  const f = process.argv[3];
  const b = parseGs(await readFile(f), f.split("/").pop());
  let vis = 0, hidden = 0, triN = 0, modes = {};
  for (const s of b.submeshes) {
    if (s.flags & 0x0400) hidden++; else vis++;
    triN += s.tris.length;
    const mode = s.flags & 0x0001 ? "blend" : s.flags & 0x0002 ? "palette" : "single";
    modes[mode] = (modes[mode] || 0) + 1;
  }
  console.log(`${b.name}: verts=${b.cnt.pos} nrm=${b.cnt.nrm} uv=${b.cnt.uv} mat=${b.cnt.mat} mesh=${b.cnt.mesh} sub=${b.submeshes.length} (vis ${vis}/hid ${hidden}) tris=${triN} modes=${JSON.stringify(modes)}`);
  console.log(`  fb pos=${b.fb.pos} nrm=${b.fb.nrm} uv=${b.fb.uv} bbox=[${b.bbox.map((x) => x.toFixed(1)).join(",")}]`);
  console.log("  materials:", b.materials.map((m, i) => `${i}:${m.name}×${m.texRefs.length ? m.texRefs.map((t) => "img" + t.img).join("") : "0tex"}`).join(" "));
  console.log("  texrefs(img idxs):", [...new Set(b.materials.flatMap((m) => m.texRefs.map((t) => t.img)))].join(","));
  await writeFile(f.replace(/\.gs$/, ".gs.json"), JSON.stringify({
    ...b, positions: [...b.positions], normals: [...b.normals], uvs: [...b.uvs], colors: [...b.colors],
    submeshes: b.submeshes.map((s) => ({ ...s, tris: s.tris.map((t) => t.map((x) => [x.p, x.n, x.c, x.u, x.skin])) })),
  }));
  console.log("  -> " + f.replace(/\.gs$/, ".gs.json"));
} else if (isMain) console.error("usage: node bin/gs.mjs json <file.gs>");
