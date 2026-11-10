// ga.mjs — FE9 (.ga) animation parser → JSON. Spec: tellius-forge
// research/animation/tellius-animation-file-format.md v2.0.1 (byte-exact vs whole FE9 disc).
// Header 0x30 [12×u32]: footer_off,0,game_flag,format_flags,loop,start,end,bone_count,
// bone_table_off(0x30),curve_table_off,0,keyframe_off. Bone rec 16B (idx,mask,first,n).
// Curve rec 12B (channel,fracbits<<8,end_frame,key_count,first_key). Key 4B (u16 frame, i16
// value×2^-fb). FE9 footer: 9×u32 slot ptrs; slot0 event track (frames+codes).
import { readFile, writeFile, mkdir } from "fs/promises";

export function parseGa(d, name = "") {
  const u32 = (o) => d.readUInt32BE(o), u16 = (o) => d.readUInt16BE(o), i16 = (o) => d.readInt16BE(o);
  const footerOff = u32(0), gameFlag = d[8], fmtFlags = u32(0xc);
  if (gameFlag !== 0) throw new Error(`${name}: not FE9 (game_flag=${gameFlag})`);
  const loop = u32(0x10), startF = u32(0x14), endF = u32(0x18), boneCount = u32(0x1c);
  const boneOff = u32(0x20), curveOff = u32(0x24), keyOff = u32(0x2c);
  if (boneOff !== 0x30) throw new Error(`${name}: bone_off=0x${boneOff.toString(16)}`);
  const curveCount = (keyOff - curveOff) / 12;
  const bones = [], curves = [];
  for (let i = 0; i < boneCount; i++)
    bones.push({ idx: u32(boneOff + i * 16), mask: u32(boneOff + i * 16 + 4), first: u32(boneOff + i * 16 + 8), n: u32(boneOff + i * 16 + 12) });
  for (let c = 0; c < curveCount; c++) {
    const q = curveOff + c * 12;
    const channel = u16(q), fb = u16(q + 2) >> 8, eF = u16(q + 4), kc = u16(q + 6), fk = u32(q + 8);
    const keys = [];
    for (let k = 0; k < kc; k++) {
      const o = keyOff + (fk + k) * 4;
      keys.push([u16(o), i16(o + 2) / (1 << fb)]);
    }
    curves.push({ c, channel, fb, eF, keys });
  }
  let footer = null;
  if (footerOff) {
    const slots = [];
    for (let s = 0; s < 9; s++) { const p = u32(footerOff + s * 4); if (p) slots.push({ s, p }); }
    footer = { slots: slots.map((x) => x.s), events: [] };
    const ev = slots.find((x) => x.s === 0);
    if (ev) {
      const kc = u16(ev.p);
      for (let i = 0; i < kc; i++) {
        const ko = ev.p + u16(ev.p + 8 + i * 2);
        const frame = u16(ko), cc = u16(ko + 2);
        const codes = [];
        for (let j = 0; j < cc; j++) codes.push(u16(ko + 4 + j * 2));
        footer.events.push({ frame, codes });
      }
    }
  }
  // attach curves to bones
  for (const b of bones) b.curves = curves.slice(b.first, b.first + b.n).map((c) => ({ ch: c.channel, keys: c.keys }));
  return {
    name, game: "FE9", flags: fmtFlags, loop, startFrame: startF, endFrame: endF,
    boneCount, curveCount, bones, footer,
  };
}

const [, , cmd, ...rest] = process.argv;
const { pathToFileURL } = await import("url");
const isMain = process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href;
if (isMain && cmd === "json" && rest.length >= 1) {
  await mkdir("out/ga", { recursive: true });
  for (const f of rest) {
    const d = await readFile(f);
    const base = f.split("/").pop().replace(/\.ga$/i, "");
    const ga = parseGa(d, base);
    await writeFile(`out/ga/${base}.json`, JSON.stringify(ga));
    console.log(`${base}: bones=${ga.boneCount} curves=${ga.curveCount} frames=${ga.startFrame}..${ga.endFrame} loop=${ga.loop} events=${ga.footer ? ga.footer.events.length : 0}`);
  }
} else if (isMain) console.error("usage: node bin/ga.mjs json <files...>");
