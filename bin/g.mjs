// g.mjs — FE9 skeleton (.g) parser + pose builder. Spec: tellius-forge
// research/skeleton v2.0.0 (FE9-wide, byte-exact). Record 0xF4: hdr(i32 parent,sib,child;u32
// flags) + inverse bind 3x4 + 43f value block + u16 idx,u16 rotOrder,u32 nameOff.
// world[b] = chain T(tr+offset1)·T(p)·JO·Rot·RO·S·T(-p) (order-1 XYZ => R=Rz·Ry·Rx).
import { readFile, writeFile } from "fs/promises";

const DEG = Math.PI / 180;

export function parseG(d, name = "") {
  const u32 = (o) => d.readUInt32BE(o), i32 = (o) => d.readInt32BE(o), u16 = (o) => d.readUInt16BE(o);
  const f32 = (o) => d.readFloatBE(o);
  const poolOff = u32(4), boneCount = u32(8), arrOff = u32(12);
  if (arrOff !== 0x10 || poolOff !== 0x10 + boneCount * 0xf4)
    throw new Error(`${name}: header mismatch pool=0x${poolOff.toString(16)} n=${boneCount}`);
  const pool = d.subarray(poolOff);
  const cstr = (off) => { let e = off; while (e < pool.length && pool[e]) e++; return pool.toString("latin1", off, e); };
  const bones = [];
  for (let i = 0; i < boneCount; i++) {
    const b = 0x10 + i * 0xf4;
    const invBind = new Float32Array(12), seed = new Float32Array(12), vals = new Float32Array(43);
    for (let j = 0; j < 12; j++) invBind[j] = f32(b + 0x10 + j * 4);
    for (let j = 0; j < 43; j++) vals[j] = f32(b + 0x40 + j * 4);
    for (let j = 0; j < 12; j++) seed[j] = f32(b + 0xbc + j * 4);
    bones.push({
      i, parent: i32(b), sib: i32(b + 4), child: i32(b + 8), flags: u32(b + 0x0c),
      invBind, vals, seed, boneIndex: u16(b + 0xec), rotOrder: u16(b + 0xee),
      name: cstr(u32(b + 0xf0)),
    });
  }
  const rootName = cstr(0);
  return { name, boneCount, bones, rootName };
}

// 3x4 helpers (row-major, translation in last column)
const mIdent = () => Float32Array.of(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0);
export function mMul(a, b) { // a·b, both 3x4
  const o = new Float32Array(12);
  for (let r = 0; r < 3; r++) {
    for (let c = 0; c < 3; c++)
      o[r * 4 + c] = a[r * 4] * b[c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c];
    o[r * 4 + 3] = a[r * 4] * b[3] + a[r * 4 + 1] * b[7] + a[r * 4 + 2] * b[11] + a[r * 4 + 3];
  }
  return o;
}
export function mXform(m, x, y, z) {
  return [m[0] * x + m[1] * y + m[2] * z + m[3], m[4] * x + m[5] * y + m[6] * z + m[7], m[8] * x + m[9] * y + m[10] * z + m[11]];
}
const mT = (x, y, z) => Float32Array.of(1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z);
const mS = (x, y, z) => Float32Array.of(x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0);
const mRx = (d) => { const c = Math.cos(d), s = Math.sin(d); return Float32Array.of(1, 0, 0, 0, 0, c, -s, 0, 0, s, c, 0); };
const mRy = (d) => { const c = Math.cos(d), s = Math.sin(d); return Float32Array.of(c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0); };
const mRz = (d) => { const c = Math.cos(d), s = Math.sin(d); return Float32Array.of(c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0); };
const mEuler = (x, y, z) => mMul(mRz(z * DEG), mMul(mRy(y * DEG), mRx(x * DEG)));

// local matrix from value slots + channel gate (bit 3 scale,4 rotate,5 translate,6 JO,
// 7 pivot,8 scalePivot,9 off1,10 off2,11 rotAxis). v = evaluated 43-float block.
export function buildLocal(flags, v) {
  let m = mIdent();
  if (flags & 0x020 || flags & 0x200) { // translate + offset1
    const ox = flags & 0x200 ? v[18] : 0, oy = flags & 0x200 ? v[19] : 0, oz = flags & 0x200 ? v[20] : 0;
    const tx = (flags & 0x020 ? v[6] : 0) + ox, ty = (flags & 0x020 ? v[7] : 0) + oy, tz = (flags & 0x020 ? v[8] : 0) + oz;
    if (tx || ty || tz) m = mMul(m, mT(tx, ty, tz));
  }
  const p = flags & 0x080 ? [v[12], v[13], v[14]] : [0, 0, 0];
  let R = null;
  if (flags & 0x040) R = mMul(R || mIdent(), mEuler(v[9], v[10], v[11]));          // joint orient
  if (flags & 0x010) R = mMul(R || mIdent(), mEuler(v[3], v[4], v[5]));            // rotate
  if (flags & 0x800) R = mMul(R || mIdent(), mEuler(v[24], v[25], v[26]));          // rotate axis
  let S = null;
  if (flags & 0x008) S = mS(v[0], v[1], v[2]);
  if (R || S) {
    m = mMul(m, mT(p[0], p[1], p[2]));
    if (R) m = mMul(m, R);
    if (S) m = mMul(m, S);
    m = mMul(m, mT(-p[0], -p[1], -p[2]));
  }
  if (flags & 0x400) m = mMul(m, mT(v[21], v[22], v[23])); // pivot offset 2 (role open; observed 0)
  return m;
}

// Chain world matrices top-down (parent before child guaranteed on disc; roots chained via sib).
export function worldMatrices(g, localOf /* (bone)=>3x4 */) {
  const w = new Array(g.boneCount);
  for (let i = 0; i < g.boneCount; i++) {
    const b = g.bones[i];
    const L = localOf(b);
    w[i] = b.parent >= 0 ? mMul(w[b.parent], L) : Float32Array.from(L);
  }
  return w;
}

export function seedWorlds(g) {
  return worldMatrices(g, (b) => b.seed);
}

// Apply animation curves (from parseGa `bones` rows) at frame t onto skeleton values.
export function animWorlds(g, animBones, t) {
  const vmap = new Map(); // bone idx -> copy of vals
  for (const ab of animBones) {
    const vals = Float32Array.from(g.bones[ab.idx].vals);
    for (const c of ab.curves) {
      const ks = c.keys;
      if (!ks.length) continue;
      let val;
      if (t <= ks[0][0]) val = ks[0][1];
      else if (t >= ks[ks.length - 1][0]) val = ks[ks.length - 1][1];
      else {
        let j = 0; while (j < ks.length - 1 && ks[j + 1][0] < t) j++;
        const [f0, v0] = ks[j], [f1, v1] = ks[j + 1];
        val = f1 === f0 ? v1 : v0 + (v1 - v0) * (t - f0) / (f1 - f0);
      }
      vals[c.ch] = val;
    }
    vmap.set(ab.idx, vals);
  }
  return worldMatrices(g, (b) => {
    if (!vmap.has(b.i)) return b.seed;
    return buildLocal(b.flags | (animBones.find((a) => a.idx === b.i).mask & 0x38), vmap.get(b.i));
  });
}

const [, , cmd, ...rest] = process.argv;
import { fileURLToPath, pathToFileURL } from "url";
const isMain = process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href;
if (isMain && cmd === "json" && rest.length >= 1) {
  const f = rest[0];
  const g = parseG(await readFile(f), f.split("/").pop());
  const out = { ...g, bones: g.bones.map((b) => ({ ...b, invBind: [...b.invBind], vals: [...b.vals], seed: [...b.seed] })) };
  const dest = f.replace(/\.ga$/, ".g.json").replace(/\.g$/, ".g.json");
  await writeFile(dest, JSON.stringify(out));
  console.log(`${g.name}: bones=${g.boneCount} rootName="${g.rootName}" -> ${dest}`);
  const bad = g.bones.filter((b, i) => b.boneIndex !== i);
  console.log(`  boneIndex==slot: ${bad.length === 0 ? "OK" : "MISMATCH " + bad.length}`);
  // seed-vs-values consistency (spec: 56463/56836 rebuild)
  let ok = 0, badSeed = [];
  for (const b of g.bones) {
    const L = buildLocal(b.flags, b.vals);
    let d = 0; for (let j = 0; j < 12; j++) d = Math.max(d, Math.abs(L[j] - b.seed[j]));
    if (d < 0.01) ok++; else badSeed.push(b.name);
  }
  console.log(`  seed rebuild from values: ${ok}/${g.boneCount}${badSeed.length ? " outliers: " + badSeed.slice(0, 8).join(",") : ""}`);
} else if (isMain) console.error("usage: node bin/g.mjs json <file.g>");
