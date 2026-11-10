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
  "/Face|.cms": S("LZ77(TPL) [probe]", "Decoded", "bin/tpl2png.mjs", "PNG RGBA", "BERLAPIS (BRV-GFX-001d): img0 192x128 base + img1..13 lapisan ekspresi 16x32; C4 fix 2026-11-10 (sebelumnya blok putih sensor); vision 360/360", "lz77,tpl", "UI portrait", "4347 PNG; komposit+mapping ekspresi = P2"),
  "/Face|.bin": S("TBL ✓ (facedata)", "Decoded", "-", "face layer table", "IDENTIK /|Face/facedata.bin (root copy): 358 face ×3 layer-slot u16-idx; loader FUN_8007c408 + intern FUN_8000d3ec ✓", "FUN_8007c408", "Portrait", "lihat /|Face/facedata.bin"),
  "/window|.cmp": S("LZ77(pack) ✓", "Partial", "bin/cmp.mjs + tpl2png", "tpl+RectUnitList.bin", "butai.cmp: butai.tpl(21img render✓)+RectUnitList.bin(39 rect grid); butai_en varian", "cmp,lz77,tpl", "UI engine", "rect semantics P4"),
  "/system.cmp|.cmp": S("LZ77(pack) 16/16✓", "Decoded", "bin/cmp.mjs", "master bundle", "FE8Data/FE8Anim/FE8Effect/cp_data/s/rect/window/rectdesc/face/facedata + route/xinfo/icon tpl + mess/common.m + 4 gcf — root copies byte-identik", "cmp,lz77", "System", "sumber tunggal tabel sistem"),
  "/title.cmp|.cmp": S("LZ77(pack) 2/2✓", "Decoded", "bin/cmp.mjs + tpl2png", "title.tpl f0", "logo title 3img render", "cmp,lz77,tpl", "Title", ""),
  "/window|.cms": S("LZ77(TPL) 85/85✓", "Decoded", "bin/tpl2png.mjs", "PNG (585 img, fmt 0-6,8,9,14)", "semua render; bg/icon/menu UI — icon atlas vision-ok", "lz77,tpl", "UI engine", "bagian 965 img UI window"),
  "/window|.tpl": S("TPL FE9 34/34✓", "Decoded", "bin/tpl2png.mjs", "PNG (380 img)", "back/icon/cardicon/exp/filemenu dll; icon_0 768x216 atlas vision-ok; back = gradient (kanonik)", "BRV-GFX-001", "UI", ""),
  "/window|.bin": S("TBL hdr ✓", "Partial", "-", "rect table", "hdr = [u32 size][u32 tableOff][u32 cnt][u32 sub]: RectUnitList(1181/972/39/1), s_rect(28869/20840/827/195), rectdesc(11036/8524/369/51) — koordinat pixel grid; semantik penuh P4", "-", "UI rect table", "RectBases(.en) + RectUnitList(butai.cmp)"),
  "/illust|.tpl": S("TPL FE9 C4 [174/174✓]", "Decoded", "bin/tpl2png.mjs", "PNG", "CG card 528x376 cube/gba1-3 ✓ (Geoffrey terbaca full); semua fmt14 C4 — butuh fix CMPR 2026-11-10", "tpl", "Presentasi CG", "batch selesai"),
  "/Fonts|.cms": S("LZ77 raw stream@4 [probe]", "Decoded", "gcf.mjs+cmp.mjs", ".gcf stream", "system.cms = LZ77(no tag)→.gcf 20sh 3988g", "lz77,gcf", "UI text", "variant system font"),
  "/Fonts|.gcf": S("GCF u32 magic 0x20 [probe]", "Decoded", "gcf.mjs", "PGM atlas+metrics CSV", "BRV-FNT-001: I4 GX sheets, 6/6 fonts render", "gcf,i4,gx", "UI text", "fe/talk/alpha/biggana/system(.gcf+.cms) — AM-024 done"),
  "/Mess|.m": S("M [.len,.tlen,0,.cnt] [probe✓spec]", "Decoded", "bin/m.mjs", "JSON UTF-8 (target)", "c09 dialog + kontrol $.. ; header cocok probe", "cmp,lz77", "Dialogue UI", "konverter SJIS/escape pending"),
  "/Scripts|.cmb": S("CMB 'cmb\\0'+selfname [probe✓VM]", "Decoded", "bin/cmb.mjs + natcatalog.mjs", "script JSON + listing bytecode + native catalog", "43/43 script: 2075/2075 fns disasm bersih; VM 65 opcode penuh; NATIVE CATALOG 390/390 (name→impl→argc, 312 dipanggil; top WaitM/InstantDispos/MapLoad/UnitGetByPID)", "Ghidra oracle FUN_80013ae8 407-site", "Script VM", "BRV-SCT-004; Verified menunggu engine replay"),
  "/zdbx.cmp|.dbx": S("TEXT SJIS key-val, rec '¥'", "Decoded", "bin/dbx.mjs", "JSON UTF-8 ✓", "437/437→708 rec: Param209 Model163 Weapon91 GUI(Button43/Combo27…) JobList/SkillList/TerrainList", "cmp", "lookup job→model/kamera", "JobList=JID_*→modeldir+PID; Param scene=roomFlag/footSound"),
  "/xcam|.dbx": S("TEXT SJIS key-val, rec '¥'", "Decoded", "bin/dbx.mjs", "JSON UTF-8 ✓", "CameraAnime rec: class/name/caption/camera/pos[]/rot[]/dist/time", "cmp", "Battle camera", "satu file per (job,anim,wpn) — BRV-ANM-003 data ready"),
  "/xcam|.cmp": PACK("Battle camera", "cam.cmp per job"),
  "/xwp|.cmp": PACK("Weapon model", "per senjata"),
  "/yme|.cmp": S("LZ77(pack) ✓786/786", "Partial", "bin/cmp.mjs", "efek pack (EID_*)", "EID_ANGER: g(31bone)+ga+gs(12mat)+tpl(4img f14) — SEMUA parser lolos; 292 pack", "cmp,lz77,gs,g,ga,tpl", "Effect System", "ga curves=0 utk efek (field/anim-type beda) — cek P6"),
  "/ymu|.cmp": S("LZ77(pack) [probe]", "Decoded", "bin/cmp.mjs", "pack.cmp per kelas unit", "ARCHER full pipeline: skeleton.g+body.gs+ga+tex tpl → meshview 3D render (out/render/archer_*.png) humanoid+pose-nya beda per anim", "cmp,lz77,g,gs,ga,tpl2png,meshview", "Model System", "= rumah model unit MAP; P1 MILESTONE tercapai"),
  "/ymu|.ga": S("GA [probe✓spec]", "Decoded", "bin/ga.mjs", "anim bin (target)", "keyframe=[u16 frame][i16 value] FIXED; wait_N 15bone/34curv loop1..60; atk1 events 13=>SE,41=>DAMAGE; posing via meshview verified visually", "spec tellius v2.0.1", "Animation System", ""),
  "/ymu|.gs": S("GS=body [probe pack✓parser]", "Decoded", "bin/gs.mjs", "mesh JSON", "archer 303v/293n/199uv/590tri, 3 submesh palette-skin flags 0x3002/300e/3802; DL entry [skin][pos][nrm][uv] stride7; fb pos10/nrm6/uv14; render proof out/render/archer_*.png", "spec tellius body + empiris", "Model System", "blend-vertex path stub (butuh pack zu); img↔tpl binding lihat note tpl"),
  "/ymu|.tpl": S("TPL FE9 [0x20AF30 entry-tbl]", "Decoded", "bin/tpl2png.mjs", "PNG", "BRV-GFX-001: C4 BE+idx3-opaque TERKUNCI (bug off-by-1 c1 diperbaiki); binding tex_<charIdx> ✓ (archer2/tex_195=LOFA); Lofa atlas terbaca", "BRV-GFX-001", "Model tex", "pack tex_4/5 = slot base (aturan relatif minor open)"),
  "/ymu|.bin": S("?", "Discovered", "-", "?", "belum di-probe per file", "-", "?", "cek P1"),
  "/ymu|.pak": S("PACK raw [✓same fmt]", "Partial", "bin/cmp.mjs (raw passthrough)", "isi pack", "magic 'pack' ✓; varian raw tanpa LZ77; sama /zu", "cmp", "Model System", ""),
  "/zu|.pak": S("PACK raw [✓same fmt /ymu]", "Partial", "bin/cmp.mjs (raw passthrough)", "isi pack (model+anim battle)", "amr1_ja.pak 9/9 entry: amr1.gs(body 134KB)+amr1.g+6 anim .ga (poi/lda/ldc/dam/ded/dog); entries @0x08 [zero,nameOff,dataOff,len] identik cmp-pack; varian _ja/_no/_sp/_sw", "cmp", "Battle Model", "gs parser jalan; img↔tpl binding ✓ BRV-GFX-001; blend-skinning = gap AM-009 tersisa"),
  "/zu|.ga": S("GA [probe✓spec]", "Decoded", "bin/ga.mjs", "anim bin", "sama /ymu", "spec", "Animation/battle", ""),
  "/zu|.tpl": S("TPL FE9 539/539✓", "Decoded", "bin/tpl2png.mjs", "PNG (1353 img)", "battle tex ter-render semua (amr1 shield panel koheren)", "BRV-GFX-001", "Battle tex", ""),
  "/zu|.gs": S("GS=body ✓", "Decoded", "bin/gs.mjs", "mesh JSON", "amr1.gs 134KB parse ✓ (dlm /zu|.pak); xwp/yme/zbg/zmap gs pun lolos", "spec body", "Model System", ""),
  "/xwp|.ga": S("GA", "Decoded", "bin/ga.mjs", "anim bin", "sampling belum", "spec", "Animation", ""),
  "/xwp|.gs": S("GS ✓", "Decoded", "bin/gs.mjs", "mesh JSON", "alondite.gs parse OK 3 mat (+g 5bone+tpl f14 dalam pack)", "spec body", "Weapon model", ""),
  "/s|.none": S("TPL FE9 (198/198✓)", "Decoded", "bin/tpl2png.mjs", "PNG (319 img)", "semua magic 0x20AF30; 608x448/296 full-screen sky/scene (0xw cloudscape vision-ok); map scene bg", "BRV-GFX-001", "Map Engine", ""),
  "/s|.bin": S("TBL hdr ✓", "Partial", "-", "rect tables (map)", "rect.bin = dlm system.cmp identik; hdr [28869/20840/827/195] keluarga [size/tableOff/cnt/sub] ✓; +rectdesc (11036/8524/369/51) dlm window/ — parse semantik P4", "-", "Map Engine", ""),
  "/zbg|.cmp": S("LZ77(pack) ✓", "Partial", "bin/cmp.mjs", "map.bin+g+gs+tpl+ga per arena", "bf.cmp: map.bin 253B = [hdr u32×6][f32 kamera/terrain (2000,-49.9..)][nama 'map06_fort'=g/gs][slot str: mapbuilddescB,mapbuildinstB,mapcapacityB,mapextraB,mapfogB,maplightB]+rect; map06_fort.g/gs(7mat)+texpack.tpl(7img) parse ✓", "cmp,lz77,g,gs,tpl,ga", "Battle scene bg", "BRV-BAT arena cfg P4"),
  "/zmap|.cmp": S("CMP pack ✓", "Partial", "cmp.mjs", "stream scripting", "dispos_c.bin = [hdr u32 size/tableOff][u32 stream][pool 136 str PID_/SEQ_/IID_/MTYPE_ + ts build '2005-05-26'] — record variable-length: PID_<nama> spawn + SEQ_<kondisi AI> (NOMOVE/ATTACK100/BEARHEAL..) + param u32 + pool B event (ID_TEMP_ON_DIE,always_T_*). Grammar penuh = P1-map (Ghidra)", "cmp,lz77", "World map", "BRV-MAP"),
  "/gmap|.cms": S("LZ77(TPL) [probe]", "Decoded", "tpl2png", "PNG", "probe ✓ (gmap_zentai)", "lz77,tpl", "World map", ""),
  "/gmap|.tpl": S("TPL FE9 7/7✓", "Decoded", "bin/tpl2png.mjs", "PNG (27 img)", "gmap.tpl atlas render", "BRV-GFX-001", "World map", ""),
  "/gmap|.thp": S("THP?", "Deferred", "-", "-", "cek di P6", "-", "Presentasi", "kemungkinan video gmap"),
  "/etc|.g": S("G skeleton [probe✓parser]", "Decoded", "bin/g.mjs", "skeleton JSON+matrix", "archer skeleton.g 26 bone; seed rebuild 26/26 vs invBind ✓; buildLocal/animWorlds dipakai render", "spec tellius skeleton + empiris", "Skeleton", "parser juga untuk .g dlm pack unit"),
  "/etc|.ga": S("GA [probe✓]", "Decoded", "bin/ga.mjs", "anim bin", "curb parse ok (flag 0x11)", "spec", "Animation", ""),
  "/etc|.gs": S("GS=body [parser]", "Decoded", "bin/gs.mjs", "mesh JSON", "format sama dgn /ymu .gs (parser sama)", "spec body", "Model", "prop curb/curt/plane"),
  "/etc|.cms": S("LZ77(TPL) [probe]", "Decoded", "tpl2png", "PNG", "probe ✓ (tobecontinued)", "lz77,tpl", "Presentasi", ""),
  "/etc|.tpl": S("TPL FE9 16/16✓", "Decoded", "bin/tpl2png.mjs", "PNG (20 img)", "background/gameover render", "BRV-GFX-001", "Presentasi", ""),
  "/Sound|.stm": S("GC-DSP stereo 2×mono (vgmstream DSP_STM): frame 8B [idx4|scale4]+14 nibble; koef 8psgn16@0x5c", "Decoded", "bin/stm.mjs", "OGG (musik)", "u16sr@2 u32bytes@8 loop@0xc(nib) coefs@0x5c data@0x8100 L,R", "BRV-AUD-002", "Audio Engine", "3 file dibuktikan (rough 0.43-0.87 << noise 1.35; spektrum bass-dominan; loop-kontinu); *_lp = intro+loop record ganda"),
  "/Sound|.bin": S("TBL [u32@0=size]", "Partial", "-", "bank master", "gcfesnd.bin 81863 = master bank; role map bank 8-file: .bin master/.etbl+.stbl tabel string nama SE/.pool directory stream/.proj event-map/.sdir directory sample 16B/.slib sequencer/.samp payload 4.2MB", "-", "Audio", "BRV-AUD-003"),
  "/Sound|.pool": S("POOL [probe]", "Partial", "-", "directory stream", "hdr [u32 16][u32 14380/14432 region][u32 0x48][0x390000...] — entri [u32 x][u32 y][u32 off][u32 sampOff]-ish; link ke .samp pending", "-", "Audio", ""),
  "/Sound|.slib": S("SEQ [probe+Ghidra]", "Partial", "-", "sequence ops", "BRV-AUD-003: hdr u32 next-chain@0/0xe40/0x2ee0; stream seq @sdir.off ×16B-entry [u16 op,param] stride 0x28; op semantics pending", "-", "Audio", "bank gcfesnd sequencer"),
  "/Sound|.sdir": S("SDIR [probe+Ghidra✓]", "Partial", "-", "directory", "BRV-AUD-003: 12B hdr + 16B-records: 0x3c00=fmt[rate,ch][?][u32 u32]; 0x0000=stream[id][?][sampOff u32][libOff +0x28i]; 0x0008=16×s16 coef blk", "-", "Audio", "425 stream recs; samp cum-offset✓(r1=78912)"),
  "/Sound|.proj": S("PROJ chain [Ghidra✓ FUN_80258d94]", "Partial", "-", "event→stream map", "rec=[u32 byte-delta][u16 id][payload]; id6=u32-list 308×; id0x251+=10B-tok[ff0a][rate][3c00][streamid][?]", "-", "Audio/AI", "walk proven; mapping semantics pending"),
  "/Sound|.samp": S("RAW-frames [probe+fit]", "Investigating", "-", "?", "BRV-AUD-003: @0 header-byte structure scale/idx valid 2000/2000 frame — BUKAN DSP IIR standar (LSQ fit konvergen kosong, ZCR 3.8k/s=noise dgn koef stm); encoding = custom/AX-ucode", "-", "Audio", "PAYLOAD BELUM — P2 target"),
  "/Sound|.stbl": S("32B-rec [probe✓]", "Partial", "-", "tabel SFX event", "'SE1_EVT_*' 24B nama+pad + u32 trailing; stride 32 exact (3360/32=105)", "-", "Audio/Script", "tabel SFX per event!"),
  "/Sound|.etbl": S("32B-rec 2-str [probe✓]", "Partial", "-", "tabel nama SE2", "2 nama null-term/record (event+part 'AGON_ROAR2_0'); 2240/32=70", "-", "Audio", ""),
  "/Movie|.thp": S("THP [probe 'THP']", "Deferred", "ffmpeg (opsional PC)", "mp4/still", "-", "-", "Cutscene", "keputusan P6"),
  "/Movie|.none": S("THP [probe]", "Deferred", "-", "-", "dummy juga THP", "-", "Cutscene", ""),
  "/ending|.tpl": S("TPL FE9 102/102✓", "Decoded", "bin/tpl2png.mjs", "PNG (156 img)", "fin_0 608x448 full-scene CG vision-ok (candle table); portrait + fullres", "BRV-GFX-001", "Ending", "P7"),
  "/shop|.bin": S("TBL ✓[8B rec+IID pool]", "Partial", "-", "tabel shop", "shopitem_h/m/n (difficulty!) [hdr size/tableOff/cnt=2340|2513|2591/sub=96] + pool IID_* ('IID_DAGGER..'+ts 2005-02-20); rec stream u32 ptr→IID string (0x34xx-0x36xx) pair-ish; field harga/qty belum dipisahkan", "-", "Shop system", "P5 ekonomi"),
  "/ending|.bin": S("TBL ✓", "Partial", "-", "kredit+rect ending", "staffroll.bin 3255B = KREDIT TERBACA (#F04 tags 'BG Works/Motion Capture', nama staff ASCII+SJIS, ts 2005-06-29) [hdr size/tableOff/cnt=105/2]+offset table; RectFinale(.en) 563/566 rec rect ending", "-", "Presentasi", "staffroll = teks penuh; layout P4"),
  "/|tut_data_en.bin": S("TBL hdr ✓", "Partial", "-", "tutorial EN", "tut_data(.en) [size 3990/4206][tableOff 3116/3292][cnt 186/196][sub=5] — sama keluarga tabel [size/tableOff/cnt/sub] (recurring ×6+ file) — biner murni (0 ASCII di head), record A~16.7B", "-", "Tutorial", "P5"),
  "/window|Rect": S("TBL hdr ✓", "Partial", "-", "rect bases UI", "RectBases(.en) [2172/1620/65/9][2180/1628/65/9] — 65 rect-base ×9 sub; keluarga tabel sama", "-", "UI", "P4 layout"),
  "/|tut_data.bin": S("TBL [probe]", "Deferred", "-", "?", "-", "-", "?", "kemungkinan JP dup — P5 cek"),
  "/|soundroom.bin": S("TBL hdr ✓", "Partial", "-", "soundroom table", "[624/508/21/0] — keluarga tabel [size/tableOff/cnt/sub] ✓ (21 entri ×24.2B A-region)", "-", "Audio menu", "P6"),
  "/|client.bin": S("hi-entropy [probe]", "Discovered", "-", "?", "-", "-", "?", "klien multiplayer/link? triase P1"),
  "/|opening.bnr": S("BNR1 [probe]", "Deferred", "-", "-", "-", "-", "Boot", "banner GC — skip (boot custom sendiri)"),
  "/|cp_data.bin": S("TBL pack [terbaca cmp]", "Decoded", "cmp.mjs pack", "cp_data (UI script table)", "pack byte-identik", "cmp", "UI/script", "isi per-field P4"),
  "/|FE8Data.bin": S("TBL [terbaca]", "Decoded", "bin/fe8data.mjs", "characters/classes/items CSV", "manual Ike/Mist cocok", "-", "Character System", ""),
  "/|FE8Anim.bin": S("TBL [terbaca cmp]", "Partial", "-", "?", "pack identik; field belum dipetakan", "-", "Animation", "battle anim table? P4"),
  "/|FE8Effect.bin": S("TBL [probe]", "Investigating", "-", "?", "-", "-", "Effects", "P6"),
  "/|Face/facedata.bin": S("TBL ✓", "Decoded", "-", "face layer table", "hdr[u32 size][u32 0x5710][u32 cnt=1075] + rows[0x10..0x2CE8) (358×~32B) + pool 358 nama .cms + tail EXACT 1075 u32 = 358 face ×3 SLOT = [u16 idx<<16] index layer ke pool global image .cms (ANNA 0,4,8 / ANTIQUE 12,16,40; spacing ~24 img/face) = pemilih set ekspresi", "-", "UI portrait", "BRV-GFX-001d"),
  "/window|.none": S("?", "Discovered", "-", "?", "-", "-", "UI", ""),
};
const UNKNOWN = S("UNKNOWN [probe]", "Discovered", "-", "?", "-", "-", "?", "Pending Format Investigation");

function lookup(top, extD) {
  const k = `${top}|${extD}`;
  if (MAP[k]) return MAP[k];
  const dirOnly = MAP[top];
  if (dirOnly) return dirOnly;
  if (/^\.(bnr)$/.test(extD)) return UNKNOWN;
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
