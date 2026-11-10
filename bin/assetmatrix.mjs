// assetmatrix.mjs — FE9_ASSET_MATRIX.csv generator (evidence-based).
// node bin/assetmatrix.mjs [inventory.csv] > docs/FE9_ASSET_MATRIX.csv
// Status: Discovered < Investigating < Partial < Decoded < Verified ; Deferred terpisah.
// Verified HANYA bila output dipakai engine & cocok dengan game asli.
// Format column = hasil probe magic (FE9_MAGIC_PROBE.csv, 2026-11-10) — BUKAN asumsi.
import { readFileSync, writeFileSync } from "fs";

const invPath = process.argv[2] || "inventory/files.csv";
const inv = readFileSync(invPath, "utf8").trim().split(/\r?\n/).slice(1);
const fams = new Map();
for (const line of inv) {
  const [p] = line.split(",");
  const top = "/" + p.split("/")[1];
  const ext = (p.match(/\.([a-z0-9]+)$/i) || [, "none"])[1].toLowerCase();
  const k = `${top}|.${ext}`;
  fams.set(k, (fams.get(k) || 0) + 1);
}

const S = (fmt, status, decoder, output, verification, deps, consumer, notes = "") =>
  ({ fmt, status, decoder, output, verification, deps, consumer, notes });
const PACK = (cons, notes) =>
  S("LZ77(pack) [probe]", "Partial", "bin/cmp.mjs", "isi pack per-entri",
    "pack magic 007061636b terbukti; archer pack.cmp -> skeleton.g+body.gs+ga+tpl", "cmp,lz77", cons, notes);

// key = TOPDIR|ext (topdir = "/dir" atau "/dir/subdir" utk /s dan /illust); default lookup
// menyusut: exact -> dir-only -> UNKNOWN default.
const MAP = {
  "/Face|.cms": S("LZ77(TPL) [probe]", "Decoded", "bin/tpl2png.mjs", "PNG RGBA", "vision 360/360 (Ike,Mist) + hitungan overlay", "lz77,tpl", "UI portrait", "4347 PNG; Verified menunggu engine"),
  "/window|.cmp": S("LZ77(pack) [probe]", "Partial", "bin/cmp.mjs + tpl2png", "tpl per-entri", "system.cmp byte-identik vs root copy; icon/route/xinfo vision-ok", "cmp,lz77,tpl", "UI engine", "Verified menunggu engine"),
  "/window|.cms": PACK("UI", "batch run P1"),
  "/window|.tpl": S("TPL [probe]", "Partial", "bin/tpl2png.mjs", "PNG", "decoder terbukti family tetangga; batch pending", "tpl", "UI", ""),
  "/window|.bin": S("TBL hdr[u32 size…] [probe]", "Investigating", "-", "?", "-", "-", "UI rect table", "RectBases(.en) = tabel rect UI; parse P4"),
  "/illust|.tpl": S("TPL [probe]", "Partial", "bin/tpl2png.mjs", "PNG", "probe TPL ✓ (bole/calill/merlinus 99328B); batch pending", "tpl", "Presentasi CG", ""),
  "/illust|.cms": S("LZ77(TPL)?", "Investigating", "tpl2png (auto)", "?", "belum jalan", "lz77,tpl", "Presentasi", "P1 batch"),
  "/Fonts|.cms": S("LZ77(TPL) [probe]", "Decoded", "tpl2png", "PNG atlas", "probe: system.cms 470KB LZ77→TPL", "lz77,tpl", "UI text", "atlas glyph texture"),
  "/Fonts|.gcf": S("GCF hdr 0x20.. [probe]", "Investigating", "BELUM ADA", "glyph metrics JSON", "-", "cms atlas", "UI text", "alpha/bigkana/fe_font — metrik+kode; parse P1 (REQ-LOC-1)"),
  "/Mess|.m": S("M [.len,.tlen,0,.cnt] [probe✓spec]", "Decoded", "bin/m.mjs", "JSON UTF-8 (target)", "c09 dialog + kontrol $.. ; header cocok probe", "cmp,lz77", "Dialogue UI", "konverter SJIS/escape pending"),
  "/Scripts|.cmb": S("CMB 'cmb\\0'+selfname [probe]", "Investigating", "BELUM ADA", "script JSON (VM)", "magic+selfname; blok cmb berulang di C01", "Ghidra oracle, capture", "Script VM", "RISIKO KREDIAL — P1 #1 (§7 VM)"),
  "/zdbx.cmp|.dbx": S("TEXT SJIS", "Decoded", "cmp extract", "JSON UTF-8 (target)", "jobList/param terbaca manual", "cmp", "lookup job→model/kamera", "dbx2json P1"),
  "/xcam|.dbx": S("TEXT SJIS [probe]", "Decoded", "-", "tabel kamera per serangan", "probe: header `#…` + nama kamera JP", "cmp?", "Battle camera", "satu file per (job,anim,wpn)"),
  "/xcam|.cmp": PACK("Battle camera", "cam.cmp per job"),
  "/xwp|.cmp": PACK("Weapon model", "per senjata"),
  "/yme|.cmp": S("LZ77(pack) [probe]", "Investigating", "cmp.mjs", "efek pack (EID_*)", "pack magic ✓; isi belum di-list", "cmp", "Effect System", "Deferred konten P6"),
  "/ymu|.cmp": S("LZ77(pack) [probe]", "Decoded(container)", "bin/cmp.mjs", "pack.cmp per kelas unit", "ARCHER: skeleton.g+body.gs+wait/move.ga+tex.tpl", "cmp,lz77", "Model System", "= rumah model unit MAP; .g/.gs parser P1"),
  "/ymu|.ga": S("GA [probe✓spec]", "Decoded", "bin/ga.mjs", "anim bin (target)", "header flag 0x11 ✓ archer atk1 events", "spec tellius v2.0.1", "Animation System", ""),
  "/ymu|.gs": S("GS=body [probe pack]", "Investigating", "BELUM ADA", "mesh bin", "body.gs dlm pack (archer 9272B)", "spec tellius body", "Model System", "parser P1"),
  "/ymu|.tpl": S("TPL", "Partial", "tpl2png", "PNG", "decoder ada", "tpl", "Model tex", "tex_N.tpl dlm pack"),
  "/ymu|.bin": S("?", "Discovered", "-", "?", "belum di-probe per file", "-", "?", "cek P1"),
  "/ymu|.pak": S("PACK (raw) [probe]", "Partial", "cmp blob reader", "isi pack", "magic 'pack' ✓; varian raw tanpa LZ77", "cmp-blob", "Model System", "/zu juga"),
  "/zu|.pak": S("PACK (raw) [probe]", "Partial", "cmp blob reader", "isi pack (model+anim battle)", "amr1_ja/kni2_no.pak magic 'pack'+count; varian bahasa (_ja/_no)", "cmp-blob", "Battle Model", "parser entri raw-pack P1"),
  "/zu|.ga": S("GA [probe✓spec]", "Decoded", "bin/ga.mjs", "anim bin", "sama /ymu", "spec", "Animation/battle", ""),
  "/zu|.tpl": S("TPL", "Partial", "tpl2png", "PNG", "decoder ada", "tpl", "Battle tex", ""),
  "/zu|.gs": S("GS=body", "Investigating", "-", "mesh bin", "sama /ymu", "spec body", "Model System", ""),
  "/xwp|.ga": S("GA", "Decoded", "bin/ga.mjs", "anim bin", "sampling belum", "spec", "Animation", ""),
  "/xwp|.gs": S("GS", "Investigating", "-", "?", "-", "spec body", "Weapon model", ""),
  "/s|.none": S("mixed (smp=TPL) [probe]", "Investigating", "-", "?", "sample 02g/0dg/l19 = TPL (ground tex); per-file probe P1", "tpl?", "Map Engine", "stage prop/ground; kemungkinan mixed format"),
  "/s|.bin": S("TBL hdr [probe]", "Investigating", "-", "?", "rect.bin header pola table", "-", "Map Engine", ""),
  "/s|.cmp": PACK("Map Engine", "isi per-entri menyusul"),
  "/zbg|.cmp": PACK("Battle scene bg", "param.dbx dlm zdbx sudah terbaca"),
  "/zmap|.cmp": PACK("World map", ""),
  "/gmap|.cms": S("LZ77(TPL) [probe]", "Decoded", "tpl2png", "PNG", "probe ✓ (gmap_zentai)", "lz77,tpl", "World map", ""),
  "/gmap|.tpl": S("TPL [probe]", "Partial", "tpl2png", "PNG", "probe ✓ gmap.tpl 12 img", "tpl", "World map", ""),
  "/gmap|.thp": S("THP?", "Deferred", "-", "-", "cek di P6", "-", "Presentasi", "kemungkinan video gmap"),
  "/etc|.g": S("G hdr size [probe]", "Investigating", "BELUM ADA (spec tellius)", "skeleton bin", "curb.g 0x1f8=size; spec ada", "spec tellius skeleton", "Skeleton", "parser KRITIS P1 — yg unit di pack"),
  "/etc|.ga": S("GA [probe✓]", "Decoded", "bin/ga.mjs", "anim bin", "curb parse ok (flag 0x11)", "spec", "Animation", ""),
  "/etc|.gs": S("GS=body", "Investigating", "-", "mesh", "-", "spec body", "Model", "prop curb/curt/plane"),
  "/etc|.cms": S("LZ77(TPL) [probe]", "Decoded", "tpl2png", "PNG", "probe ✓ (tobecontinued)", "lz77,tpl", "Presentasi", ""),
  "/etc|.tpl": S("TPL [probe]", "Partial", "tpl2png", "PNG", "probe ✓ background/gameover", "tpl", "Presentasi", ""),
  "/Sound|.stm": S("RAW-ADPCM? [probe: no SEMh]", "Investigating", "BELUM ADA", "OGG (musik)/PCM (SFX)", "header 02 00 7d 00 — kemungkinan STM headerless", "GC ADPCM publik", "Audio Engine", "decode P1; nama *_lp = loop variant"),
  "/Sound|.bin": S("TBL [probe]", "Investigating", "-", "?", "gcfesnd.bin", "-", "Audio", ""),
  "/Sound|.slib": S("SLIB [probe]", "Investigating", "-", "musik/seq?", "-", "-", "Audio", "bank gcfesnd; P1 audio"),
  "/Sound|.sdir": S("SDIR [probe]", "Investigating", "-", "?", "-", "-", "Audio", "index bank"),
  "/Sound|.pool": S("POOL [probe]", "Investigating", "-", "?", "-", "-", "Audio", ""),
  "/Sound|.proj": S("PROJ [probe]", "Investigating", "-", "?", "-", "-", "Audio", ""),
  "/Sound|.samp": S("SAMP [probe hi-entropy]", "Investigating", "-", "data sample (DSP ADPCM?)", "-", "-", "Audio", ""),
  "/Sound|.stbl": S("TEXT+bin [probe: 'SE1_EVT_FOREST1']", "Investigating", "-", "tabel SFX event", "-", "-", "Audio/Script", "tabel SFX per event!"),
  "/Sound|.etbl": S("TEXT+bin [probe: 'SE2_DUMMY']", "Investigating", "-", "?", "-", "-", "Audio", ""),
  "/Movie|.thp": S("THP [probe 'THP']", "Deferred", "ffmpeg (opsional PC)", "mp4/still", "-", "-", "Cutscene", "keputusan P6"),
  "/Movie|.none": S("THP [probe]", "Deferred", "-", "-", "dummy juga THP", "-", "Cutscene", ""),
  "/ending|.tpl": S("TPL [probe]", "Partial", "tpl2png", "PNG", "probe ✓ 99328B portrait", "tpl", "Ending", "P7"),
  "/ending|.bin": S("TBL [probe]", "Discovered", "-", "?", "-", "-", "Ending", "P7"),
  "/shop|.bin": S("TBL [probe]", "Investigating", "-", "tabel shop", "header pola table; shopitem_h/m", "-", "Shop system", "parse P5"),
  "/window|Rect": S("", "Discovered", "-", "?", "-", "-", "UI", ""),
  "/|tut_data_en.bin": S("TBL [probe]", "Investigating", "-", "tutorial EN", "header table", "-", "Tutorial", "P5"),
  "/|tut_data.bin": S("TBL [probe]", "Deferred", "-", "?", "-", "-", "?", "kemungkinan JP dup — P5 cek"),
  "/|soundroom.bin": S("TBL [probe]", "Investigating", "-", "soundroom table", "-", "-", "Audio menu", "P6"),
  "/|client.bin": S("hi-entropy [probe]", "Discovered", "-", "?", "-", "-", "?", "klien multiplayer/link? triase P1"),
  "/|opening.bnr": S("BNR1 [probe]", "Deferred", "-", "-", "-", "-", "Boot", "banner GC — skip (boot custom sendiri)"),
  "/|cp_data.bin": S("TBL pack [terbaca cmp]", "Decoded", "cmp.mjs pack", "cp_data (UI script table)", "pack byte-identik", "cmp", "UI/script", "isi per-field P4"),
  "/|FE8Data.bin": S("TBL [terbaca]", "Decoded", "bin/fe8data.mjs", "characters/classes/items CSV", "manual Ike/Mist cocok", "-", "Character System", ""),
  "/|FE8Anim.bin": S("TBL [terbaca cmp]", "Partial", "-", "?", "pack identik; field belum dipetakan", "-", "Animation", "battle anim table? P4"),
  "/|FE8Effect.bin": S("TBL [probe]", "Investigating", "-", "?", "-", "-", "Effects", "P6"),
  "/|Face/facedata.bin": S("TBL [probe]", "Investigating", "-", "portrait<->unit map", "-", "-", "UI portrait", "parse P4"),
  "/window|.none": S("?", "Discovered", "-", "?", "-", "-", "UI", ""),
};
const UNKNOWN = S("UNKNOWN [probe]", "Discovered", "-", "?", "-", "-", "?", "Pending Format Investigation");

function lookup(top, extD) {
  const k = `${top}|${extD}`;
  if (MAP[k]) return MAP[k];
  const dirOnly = MAP[top];
  if (dirOnly) return dirOnly;
  if (/^\.(gcf|sdir|samp|proj|pool|stbl|slib|etbl|bnr)$/.test(extD)) return UNKNOWN;
  return UNKNOWN;
}

const rows = [...fams.entries()].sort((a, b) => b[1] - a[1]);
let n = 0;
const out = ["id,source_scope,format,count,decoder,output,status,verification,dependencies,engine_consumer,notes"];
for (const [k, cnt] of rows) {
  const [top, ext] = k.split("|");
  const r = lookup(top, ext);
  out.push(`AM-${String(++n).padStart(3, "0")},"${top}",${ext},${cnt},${r.fmt},"${r.decoder}","${r.output}",${r.status},"${r.verification}","${r.deps}",${r.consumer},"${r.notes}"`);
}
writeFileSync("FE9_ASSET_MATRIX.csv", out.join("\n") + "\n");
const sc = {};
for (const [k] of rows) { const [t, e] = k.split("|"); sc[lookup(t, e).status] = (sc[lookup(t, e).status] || 0) + 1; }
console.log(`families=${n}`, JSON.stringify(sc));
