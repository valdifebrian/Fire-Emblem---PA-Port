// meshview.mjs — P1 model verification renderer: FE9 unit (skeleton.g+body.gs+tex.tpl+ga)
// → PNG contact sheet, software raster (z-buffer, perspective-correct UV, Lambert).
// Usage: node bin/meshview.mjs <packDir> <out.png> [panels...]
//   panel = "<poseSpec>:<yawDeg>[/<pitchDeg>]"  poseSpec: seed | <gafile>@<frame>
import { readFile, writeFile, mkdir } from "fs/promises";
import { parseG, seedWorlds, animWorlds, mMul, mXform } from "./g.mjs";
import { parseGs } from "./gs.mjs";
import { parseTpl, encodePng } from "./tpl2png.mjs";
import { parseGa } from "./ga.mjs";

const pack = process.argv[2], out = process.argv[3];
const panels = process.argv.slice(4);
if (!pack || !out) { console.error("usage: meshview <packDir> <out.png> panel..."); process.exit(1); }

const sk = parseG(await readFile(`${pack}/skeleton.g`), "sk");
const body = parseGs(await readFile(`${pack}/body.gs`), "body");
const texSel = process.env.MESHTEX ? process.env.MESHTEX.split(",") : null;
const texNames = (await import("fs")).readdirSync(pack).filter((f) => /\.tpl$/i.test(f) && (!texSel || texSel.includes(f))).sort();
const images = [];
for (const t of texNames) for (const im of parseTpl(await readFile(`${pack}/${t}`), t)) images.push(im);
console.log(`textures: ${images.map((i) => `${i.name}#${i.idx} ${i.w}x${i.h}${i.rgba ? "" : " !dec"}`).join(", ")}`);
// merged image lookup: material img index indexes [tex_4 images..., tex_5 images...] (sorted files)

const gaCache = new Map();
async function worldFor(poseSpec) {
  if (poseSpec === "seed") return seedWorlds(sk);
  const m = poseSpec.match(/^(.*)@(\d+)$/);
  if (!m) throw new Error("pose: " + poseSpec);
  let ga = gaCache.get(m[1]);
  if (!ga) { ga = parseGa(await readFile(m[1].includes("/") ? m[1] : `${pack}/${m[1]}`), "ga"); gaCache.set(m[1], ga); }
  let f = +m[2];
  if (ga.loop) { const span = ga.endFrame - ga.startFrame + 1; f = ga.startFrame + ((f - ga.startFrame) % span); }
  return animWorlds(sk, ga.bones, f);
}

function renderPanel(world, yaw, pitch, W, H) {
  const px = new Float32Array(W * H * 4);
  const zb = new Float32Array(W * H).fill(1e30);
  // frame from body bbox
  const bb = body.bbox, c = [(bb[0] + bb[3]) / 2, (bb[1] + bb[4]) / 2, (bb[2] + bb[5]) / 2];
  const size = Math.max(bb[3] - bb[0], bb[4] - bb[1], bb[5] - bb[2]);
  const d = size * 2.6, focal = (H * 1.0) / (2 * Math.tan((35 * Math.PI) / 360));
  const cy2 = Math.cos((yaw * Math.PI) / 180), sy2 = Math.sin((yaw * Math.PI) / 180);
  const cp = Math.cos((pitch * Math.PI) / 180), sp = Math.sin((pitch * Math.PI) / 180);
  const view = (x, y, z) => { // model → camera (camera at origin looking +z; scene rotated)
    x -= c[0]; y -= c[1]; z -= c[2];
    let X = x * cy2 - z * sy2, Z = x * sy2 + z * cy2;
    let Y = y * cp - Z * sp; Z = y * sp + Z * cp;
    return [X, Y, Z + d];
  };
  const L = [0.32, 0.78, -0.54]; const ln = Math.hypot(...L); L.forEach((v, i) => (L[i] = v / ln));
  const nrot = (m, nx, ny, nz) => { let a = m[0] * nx + m[1] * ny + m[2] * nz, b = m[4] * nx + m[5] * ny + m[6] * nz, e = m[8] * nx + m[9] * ny + m[10] * nz; const q = Math.hypot(a, b, e) || 1; return [a / q, b / q, e / q]; };
  let drawn = 0, skipped = { hidden: 0, blend: 0, nopos: 0 };
  for (const s of body.submeshes) {
    if (s.flags & 0x0400) { skipped.hidden++; continue; }
    if (s.flags & 0x0001) { skipped.blend++; continue; }
    if (s.flags & 0x0002 && !s.palette) { skipped.nopos++; continue; }
    const mat = body.materials[s.mat];
    const diff = mat.diffuse.slice(0, 3).map((v) => Math.min(255, Math.round(v * 1.25)) / 255);
    const tex = mat.texRefs.length ? images[mat.texRefs[0].img] : null;
    if (mat.texRefs.length && (!tex || !tex.rgba)) { console.log(`  ! tex missing for ${mat.name} img${mat.texRefs[0].img}`); }
    const usx = mat.texRefs[0]?.usx || 1, usy = mat.texRefs[0]?.usy || 1;
    for (const tri of s.tris) {
      const P = [], C = [], UV = [];
      let bad = false;
      for (const vv of tri) {
        let M;
        if (s.flags & 0x0002) { const b = s.palette[(vv.skin / 3) | 0]; M = mMul(world[b], sk.bones[b].invBind); }
        else M = world[s.bone];
        const vx = body.positions[vv.p * 3], vy = body.positions[vv.p * 3 + 1], vz = body.positions[vv.p * 3 + 2];
        const [mx, my, mz] = mXform(M, vx, vy, vz);
        const [cx, cyy, cz] = view(mx, my, mz);
        if (cz < 1) { bad = true; break; }
        P.push([W / 2 + (cx * focal) / cz, H / 2 - (cyy * focal) / cz, cz]);
        const nx = body.normals[vv.n * 3], ny = body.normals[vv.n * 3 + 1], nz = body.normals[vv.n * 3 + 2];
        const [rx, ry, rz] = nrot(M, nx, ny, nz);
        let sh = 0.38 + 0.62 * Math.max(0, rx * L[0] + ry * L[1] + rz * L[2]);
        if (s.attr & 0x10) { const ci = vv.c; sh *= (0.5 + 0.5 * (body.colors[ci * 4] / 255)); }
        C.push(sh);
        UV.push([body.uvs[vv.u * 2] * usx, body.uvs[vv.u * 2 + 1] * usy, 1 / cz]);
      }
      if (bad) continue;
      // raster
      let minx = Math.max(0, Math.floor(Math.min(...P.map((p) => p[0])))), maxx = Math.min(W - 1, Math.ceil(Math.max(...P.map((p) => p[0]))));
      let miny = Math.max(0, Math.floor(Math.min(...P.map((p) => p[1])))), maxy = Math.min(H - 1, Math.ceil(Math.max(...P.map((p) => p[1]))));
      const [x0, y0] = P[0], [x1, y1] = P[1], [x2, y2] = P[2];
      const area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
      if (Math.abs(area) < 1e-6) continue;
      const inv = 1 / area;
      for (let y = miny; y <= maxy; y++) for (let x = minx; x <= maxx; x++) {
        const xc = x + 0.5, yc = y + 0.5;
        let w0 = ((x1 - xc) * (y2 - yc) - (x2 - xc) * (y1 - yc)) * inv;
        let w1 = ((x2 - xc) * (y0 - yc) - (x0 - xc) * (y2 - yc)) * inv;
        const w2 = 1 - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        const z = w0 * P[0][2] + w1 * P[1][2] + w2 * P[2][2];
        const idx = y * W + x;
        if (z >= zb[idx]) continue;
        const iw = w0 * UV[0][2] + w1 * UV[1][2] + w2 * UV[2][2];
        let r = 255, g = 255, b2 = 255, a = 255;
        if (tex && tex.rgba && process.env.MESHMODE !== "nortex") {
          const uu = (w0 * UV[0][0] + w1 * UV[1][0] + w2 * UV[2][0]) / iw;
          const v0v = (w0 * UV[0][1] + w1 * UV[1][1] + w2 * UV[2][1]) / iw;
          // bilinear, texel-center sampling, wrap
          const fu = uu * tex.w - 0.5, fv = v0v * tex.h - 0.5;
          const u0 = ((Math.floor(fu) % tex.w) + tex.w) % tex.w, v0 = Math.max(0, Math.min(tex.h - 1, Math.floor(fv)));
          const u1 = (u0 + 1) % tex.w, v1 = Math.max(0, Math.min(tex.h - 1, Math.ceil(fv)));
          const tu = fu - Math.floor(fu), tv = fv - Math.floor(fv);
          const T = (u, v) => (v * tex.w + u) * 4;
          const o00 = T(u0, v0), o10 = T(u1, v0), o01 = T(u0, v1), o11 = T(u1, v1);
          const mix = (a, b2, t) => a + (b2 - a) * t;
          r = mix(mix(tex.rgba[o00], tex.rgba[o10], tu), mix(tex.rgba[o01], tex.rgba[o11], tu), tv);
          g = mix(mix(tex.rgba[o00 + 1], tex.rgba[o10 + 1], tu), mix(tex.rgba[o01 + 1], tex.rgba[o11 + 1], tu), tv);
          b2 = mix(mix(tex.rgba[o00 + 2], tex.rgba[o10 + 2], tu), mix(tex.rgba[o01 + 2], tex.rgba[o11 + 2], tu), tv);
          a = mix(mix(tex.rgba[o00 + 3], tex.rgba[o10 + 3], tu), mix(tex.rgba[o01 + 3], tex.rgba[o11 + 3], tu), tv);
          if (a < 128) continue;
        }
        const sh = w0 * C[0] + w1 * C[1] + w2 * C[2];
        if (process.env.MESHMODE === "flat") {
          px[idx * 4] = 200; px[idx * 4 + 1] = 90; px[idx * 4 + 2] = 40;
          px[idx * 4 + 3] = 255; zb[idx] = z; continue;
        }
        if (process.env.MESHMODE === "uv") {
          const uu = (w0 * UV[0][0] + w1 * UV[1][0] + w2 * UV[2][0]) / iw;
          const v0v = (w0 * UV[0][1] + w1 * UV[1][1] + w2 * UV[2][1]) / iw;
          const wu = ((uu % 1) + 1) % 1, wv = ((v0v % 1) + 1) % 1;
          px[idx * 4] = wu * 255;
          px[idx * 4 + 1] = wv * 255;
          px[idx * 4 + 2] = 60;
          px[idx * 4 + 3] = 255; zb[idx] = z; continue;
        }
        const k = Math.min(1.6, sh);
        px[idx * 4] = Math.min(255, (r / 255) * diff[0] * k * 255);
        px[idx * 4 + 1] = Math.min(255, (g / 255) * diff[1] * k * 255);
        px[idx * 4 + 2] = Math.min(255, (b2 / 255) * diff[2] * k * 255);
        px[idx * 4 + 3] = 255;
        zb[idx] = z;
      }
      drawn++;
    }
    console.log(`  pass${s.pass} mat${s.mat} flags 0x${s.flags.toString(16)} attr 0x${s.attr.toString(16)} ${s.tris.length}tris`);
  }
  console.log(`  triangles rasterized: ${drawn} skipped: ${JSON.stringify(skipped)}`);
  return px;
}

const PW = 360, panelBufs = [];
for (const spec of panels.length ? panels : ["seed:0", "seed:90"]) {
  const [pose, cam] = spec.split(":");
  const [yaw, pitch = 12] = cam.split("/").map(Number);
  console.log(`panel ${pose} yaw${yaw}`);
  const w = await worldFor(pose);
  panelBufs.push(renderPanel(w, yaw, +pitch, PW, PW));
}
// contact sheet
const cols = Math.min(2, panelBufs.length), rows = Math.ceil(panelBufs.length / cols);
const sheet = new Uint8Array(cols * PW * rows * PW * 4).fill(30);
panelBufs.forEach((buf, i) => {
  const ox = (i % cols) * PW, oy = ((i / cols) | 0) * PW;
  for (let y = 0; y < PW; y++) for (let x = 0; x < PW; x++) {
    const s = (y * PW + x) * 4, dd = ((oy + y) * cols * PW + ox + x) * 4;
    if (buf[s + 3] === 255) { sheet[dd] = buf[s]; sheet[dd + 1] = buf[s + 1]; sheet[dd + 2] = buf[s + 2]; sheet[dd + 3] = 255; }
    else { sheet[dd + 3] = 255; }
  }
});
await mkdir(out.split("/").slice(0, -1).join("/") || ".", { recursive: true });
await writeFile(out, encodePng(cols * PW, rows * PW, Buffer.from(sheet.buffer)));
console.log(`-> ${out} (${cols * PW}x${rows * PW})`);
