// assetmatrix.mjs — generate FE9_ASSET_MATRIX.csv from inventory/files.csv.
// Status disiplin: Discovered < Investigating < Decoded < Verified ; Deferred terpisah.
// "Verified" HANYA bila asset sudah dipakai engine dan cocok dengan game asli (belum ada -> 0).
import { readFileSync, writeFileSync } from "fs";

const inv = readFileSync("inventory/files.csv", "utf8").trim().split(/\r?\n/).slice(1);
const fams = new Map();
for (const line of inv) {
  const [p] = line.split(",");
  const top = "/" + p.split("/")[1];
  const ext = (p.match(/\.([a-z0-9]+)$/i) || [, "none"])[1].toLowerCase();
  const k = `${top}|.${ext}`;
  fams.set(k, (fams.get(k) || 0) + 1);
}

// decoder/output/verification/consumer/notes per family key; default = unknown
const S = (status, decoder, output, verification, deps, consumer, notes = "") =>
  ({ status, decoder, output, verification, deps, consumer, notes });
const MAP = {
  "/Face|.cms": S("Decoded", "bin/tpl2png.mjs (lz77+tpl)", "PNG RGBA", "vision-check 360/360 (Ike,Mist) + hitungan overlay", "lz77,tpl", "UI portrait", "4347 PNG; Verified menunggu tampil di engine"),
  "/window|.cmp": S("Decoded", "bin/cmp.mjs + tpl2png", "PNG RGBA", "vision-check ikon atlas match paragon", "cmp,lz77,tpl", "UI engine", "icon/route/xinfo"),
  "/illust|.cms": S("Discovered", "(cms decoder sudah ada)", "PNG", "belum dijalankan batch", "lz77,tpl", "presentasi CG", "jalankan P1"),
  "/illust|.tpl": S("Partial", "bin/tpl2png.mjs", "PNG", "decoder terbukti di family lain; /illust belum jalan", "tpl,lz77", "Presentasi CG", "batch run P1"),
  "/yme|.cmp": S("Discovered", "container ok", "efek (mesh+tex+anim)", "-", "cmp", "Effect System", "deferred P6 (approximation tier)"),
  "/s|.none": S("Discovered", "BELUM ADA", "stage files", "-", "-", "Map Engine", "file tanpa ekstensi — magic survey P1"),
  "/Sound|.mus": S("Discovered", "-", "-", "-", "-", "Audio", "cek ekstensi lain di /Sound"),
  "/ending|.cmp": S("Deferred", "container ok", "?", "-", "cmp", "Ending", "P7"),
  "/ending|.cms": S("Deferred", "tpl2png", "PNG", "-", "lz77,tpl", "Ending", "P7"),
  "/gmap|.cmp": S("Discovered", "container ok", "world map pack", "-", "cmp", "World map", ""),
  "/xcam|.dbx": S("Decoded", "cmp extract", "JSON UTF-8", "teks terbaca", "cmp", "Battle camera", ""),
  "/Fonts|.cmp": S("Discovered", "(container ok)", "atlas glyph", "-", "cmp,lz77,tpl", "UI text", "parse glyph metrics P1"),
  "/Fonts|.tpl": S("Discovered", "tpl2png", "PNG", "-", "tpl", "UI text", ""),
  "/Mess|.m": S("Decoded", "bin/m.mjs", "JSON UTF-8 (nanti)", "dialog c09 dibaca manual + kode kontrol $c1..", "cmp,lz77", "Dialogue UI", "konverter SJIS/escape pending"),
  "/Scripts|.cmb": S("Discovered", "BELUM ADA", "script JSON (target)", "-", "Ghidra oracle, Dolphin capture", "Script VM", "RISIKO KREDIT — P1 prioritas #1"),
  "/zdbx.cmp|.dbx": S("Decoded", "bin/cmp.mjs extract", "JSON UTF-8 (target)", "jobList/param terbaca (SJIS), nilai masuk akal", "cmp", "lookup job→model, kamera, skill", "dbx2json P1"),
  "/Sound|.stm": S("Discovered", "BELUM ADA", "OGG (musik) / PCM (SFX)", "-", "ADPCM GC (publik)", "Audio Engine", "decoder P1"),
  "/s|.pak": S("Discovered", "BELUM ADA", "stage pack JSON+mesh", "-", "-", "Map Engine", "magic survey P1"),
  "/s|.cmp": S("Partial", "bin/cmp.mjs", "isi per-file", "container terbukti di zdbx/system/title", "cmp", "Map Engine", "isi per-file menyusul"),
  "/ymu|.ga": S("Decoded", "bin/ga.mjs", "anim bin (target)", "struktur vs spec byte-exact (archer atk1: events 0x17/SE/1)", "spec tellius v2.0.1", "Animation System", "Verified menunggu render"),
  "/ymu|.gs": S("Discovered", "BELUM ADA", "?", "-", "-", "Renderer (shader settings?)", "survey P1"),
  "/ymu|.bin": S("Discovered", "?", "?", "-", "-", "?", "cek magic P1"),
  "/zu|.ga": S("Decoded", "bin/ga.mjs", "anim bin", "sama /ymu", "spec tellius", "Animation/battle", ""),
  "/zu|.gs": S("Discovered", "-", "?", "-", "-", "-", ""),
  "/xwp|.ga": S("Decoded", "bin/ga.mjs", "anim bin", "sampling belum", "spec tellius", "Animation", ""),
  "/xwp|.gs": S("Discovered", "-", "?", "-", "-", "-", ""),
  "/xcam|.cmp": S("Discovered", "container ok", "kamera JSON", "-", "cmp", "Battle camera", "isi menyusul"),
  "/zbg|.cmp": S("Discovered", "container ok", "bg + param.dbx", "param.dbx sudah terbaca (zdbx)", "cmp", "Battle scene bg", ""),
  "/zmap|.cmp": S("Discovered", "container ok", "?", "-", "cmp", "World map", ""),
  "/gmap|.thp": S("Deferred", "-", "-", "-", "-", "-", "kemungkinan video gmap — keputusan P6"),
  "/etc|.g": S("Discovered", "BELUM ADA (spec ada)", "skeleton bin (target)", "-", "spec tellius skeleton", "Model/Skeleton", "parser .g P1 — KRITIS"),
  "/etc|.ga": S("Decoded", "bin/ga.mjs", "anim bin", "curb.ga parse ok", "spec", "Animation", ""),
  "/etc|.gs": S("Discovered", "-", "?", "-", "-", "-", ""),
  "/Movie|.thp": S("Deferred", "-", "mp4 / still", "-", "ffmpeg portable (opsional)", "Presentasi cutscene", "keputusan user (D1 plan lama)"),
  "/ending|.bin": S("Deferred", "-", "?", "-", "-", "Ending sequence", "P6/P7"),
}
const DEF = {
  "/ymu": S("Discovered", "BELUM ADA", "mesh bin (target)", "-", "spec body/skeleton tellius", "Model System", ".g/.body parser P1"),
  "/zu": S("Discovered", "BELUM ADA", "mesh bin", "-", "spec body tellius", "Model System", ""),
  "/xwp": S("Discovered", "-", "mesh bin", "-", "spec body", "Model System", ""),
  "/s": S("Discovered", "-", "?", "-", "-", "Map Engine", ""),
  "/Face": S("Decoded", "tpl2png", "PNG", "360/360", "-", "UI", ""),
}
function lookup(top, ext) {
  const k = `${top}|${ext}`;
  if (MAP[k]) return MAP[k];
  if (top === "/ymu" || top === "/zu" || top === "/xwp") return (DEF[top]);
  if (/\.(gcf|sdir|samp|proj|pool|stbl|slib|etbl|bnr)$/.test(ext))
    return S("Deferred", "-", "-", "-", "-", "-", "dev-leftover: triase 30 mnt P0, lalu close");
  return S("Discovered", "-", "?", "-", "-", "?", "");
}

const rows = [...fams.entries()].sort((a, b) => b[1] - a[1]);
let n = 0;
const out = ["id,source_scope,format,count,decoder,output,status,verification,dependencies,engine_consumer,notes"];
for (const [k, cnt] of rows) {
  const [top, ext] = k.split("|");
  const r = lookup(top, ext);
  out.push(`AM-${String(++n).padStart(3, "0")},"${top}/*",${ext},${cnt},"${r.decoder}","${r.output}",${r.status},"${r.verification}","${r.deps}",${r.consumer},"${r.notes}"`);
}
writeFileSync("FE9_ASSET_MATRIX.csv", out.join("\n") + "\n");
const sc = {};
for (const [k] of rows) { const [t, e] = k.split("|"); sc[lookup(t, e).status] = (sc[lookup(t, e).status] || 0) + 1; }
console.log(`families=${n}`, sc);
