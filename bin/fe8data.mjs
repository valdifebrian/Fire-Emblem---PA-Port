// fe8data.mjs — parse FE8Data.bin (from system.cmp) → character/class/item CSVs.
// Schema cracked 2026-11-10 against USA GFEE01 + FEU community notes (feuniverse t/5020).
// Layout (all BE):
//   0x00 u32 filelen | 0x04 ptr | 0x08 ptr | 0x0C u32 112 | 0x10-0x1F zeros
//   subheader @0x20: [0][ptr bigIDtable 0x14a3e][ptr jpTable 0x1c8ab][u32 nChars=340]
//   char entries @0x30, stride 0x54, n=340:
//     0x00 ptr PID | 0x04 ptr MPID | 0x08 0 | 0x0C ptr FID | 0x10 ptr JID | 0x14 ptr affil/weapon-pool
//     0x18 ptr SID1 | 0x1C ptr SID2 | 0x20 ptr SID3 | 0x24 ptr AID | 0x28 ptr AID2 | 0x2C ptr ?
//     0x30 u8[6] | 0x36 u8 level | 0x37 u8 build | 0x38 u8 weight | 0x39 u8[8] base | 0x41 u8[8] growth
//     0x49 u8[8] | 0x51 u8 | 0x52 u8[3] tail
//   class block after chars: @0x6FC0 u16 n=115; entries @0x6FC4 stride 0x64:
//     0x00 ptr JID | 0x04 ptr name-MPID | rest unknown (hex-dumped)
//   item block after classes: u16 count @0x9D74; entries stride 0x64 (first-confirmed):
//     0x00 ptr IID | 0x04 ptr name-MPID | rest unknown
// Pointers target string pools; the pointer may land a few bytes INTO the string
// (prefix skipped); we deref with sz() and report raw.
import { readFile, writeFile } from "fs/promises";

const csvEsc = (s) => /[",\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;

export function parseFE8Data(d) {
  const u32 = (o) => d.readUInt32BE(o);
  const sz = (p) => {
    if (p >= d.length) return "";
    let e = p;
    while (e < d.length && d[e]) e++;
    return d.subarray(p, e).toString("latin1");
  };
  const nChar = u32(0x2c);
  const C0 = 0x30;
  const characters = [];
  for (let i = 0; i < nChar; i++) {
    const q = C0 + i * 0x54;
    const ptr = (f) => sz(u32(q + f));
    const b = (o) => d[q + o];
    characters.push({
      idx: i,
      pid: ptr(0x00), mpid: ptr(0x04), fid: ptr(0x0c), jid: ptr(0x10), f14: ptr(0x14),
      sid1: ptr(0x18), sid2: ptr(0x1c), sid3: ptr(0x20), aid: ptr(0x24), aid2: ptr(0x28), f2c: ptr(0x2c),
      unk6: d.subarray(q + 0x30, q + 0x36).toString("hex"),
      level: b(0x36), build: b(0x37), weight: b(0x38),
      base: Array.from(d.subarray(q + 0x39, q + 0x41)),
      grow: Array.from(d.subarray(q + 0x41, q + 0x49)),
      unk8: d.subarray(q + 0x49, q + 0x51).toString("hex"),
      tail: d.subarray(q + 0x51, q + 0x54).toString("hex"),
    });
  }
  // classes
  const CLS0 = C0 + nChar * 0x54;
  const nClass = u32(CLS0) & 0xffff;
  const classes = [];
  for (let i = 0; i < nClass; i++) {
    const q = CLS0 + 4 + i * 0x64;
    classes.push({ idx: i, jid: sz(u32(q)), name: sz(u32(q + 4)), raw: d.subarray(q + 8, q + 0x64).toString("hex") });
  }
  // items
  const ITM0 = CLS0 + 4 + nClass * 0x64;
  const nItem = u32(ITM0) & 0xffff;
  const items = [];
  for (let i = 0; i < nItem; i++) {
    const q = ITM0 + 4 + i * 0x60;
    if (q + 0x60 > d.length) break;
    items.push({ idx: i, iid: sz(u32(q)), name: sz(u32(q + 4)), raw: d.subarray(q + 8, q + 0x60).toString("hex") });
  }
  return { characters, classes, items, ITM0, nItem };
}

function toCsv(rows, cols) {
  const head = cols.join(",");
  const body = rows.map((r) => cols.map((c) => {
    const v = r[c];
    return csvEsc(Array.isArray(v) ? v.join(" ") : String(v ?? ""));
  }).join(","));
  return head + "\n" + body.join("\n") + "\n";
}

const [,, inPath, outDir = "out/data"] = process.argv;
if (!inPath) {
  console.error("usage: node bin/fe8data.mjs <FE8Data.bin> [outdir]");
  process.exit(2);
}
const { characters, classes, items } = parseFE8Data(await readFile(inPath));
await writeFile(outDir + "/characters.csv", toCsv(characters, ["idx", "pid", "mpid", "fid", "jid", "f14", "sid1", "sid2", "sid3", "aid", "aid2", "f2c", "unk6", "level", "build", "weight", "base", "grow", "unk8", "tail"]));
await writeFile(outDir + "/classes.csv", toCsv(classes, ["idx", "jid", "name", "raw"]));
await writeFile(outDir + "/items.csv", toCsv(items, ["idx", "iid", "name", "raw"]));
console.log(`chars=${characters.length} classes=${classes.length} items=${items.length} -> ${outDir}`);
const ike = characters.find((c) => /(^|_)IKE($|_)/.test(c.pid));
if (ike) console.log("IKE row:", JSON.stringify(ike));
