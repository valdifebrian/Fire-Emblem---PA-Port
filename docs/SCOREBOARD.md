# SCOREBOARD — snapshot kemajuan (2 papan TERPISAH, master plan §5.1)

> Kemajuan proyek yang SESUNGGUHNYA = **Functional parity**. Asset readiness hanya kesiapan
> bahan; Decoded ≠ sistem selesai. Update tiap commit yang mengubah matriks/test/BRV.

## Papan 1 — Asset readiness (`FE9_ASSET_MATRIX.csv`, 57 keluarga) — 2026-11-10 (post-P1 model)

| Status | Keluarga | Catatan |
|---|---|---|
| Verified | **0** | belum ada aset masuk engine |
| Decoded | **31** | +**/zbg cmp + /zmap cmp CLOSED (probe 241/241; bug truncate LZ77 u24-hint bohong DI-FIX cmp.mjs — 21 file peta utk-ekstraksi)**; +**/zu pak + /xwp cmp + /xcam cmp ISI CLOSED (probe 374/374 pack: matriks kelas×senjata + kosakata aksi BRV-ANM-005)**; +**staffroll KREDIT TERBACA**; +`Face|.bin`≡facedata CLOSED; **+NATIVE CATALOG 390/390** (BRV-SCT-004); +facedata 358×3-slot; +`/s` 198/198; sweep `.tpl/.cms` 0-fail; C4+binding+potret; `.gcf`, STM, VM, 3D |
| Partial | 13 | **keluarga tabel rekuren `[size/tableOff/cnt/sub]`** terpetakan di rect×3/RectBases/shopitem×3/staffroll/RectFinale/tut_data/soundroom; +`dispos_c` AI-stream; SE bank; dsb. |
| Investigating | **1** | **HANYA `.samp` payload** (≠ DSP standar, bukti LSQ+ZCR — butuh trace ucode AX, P2) |
| Discovered | 10 | hanya magic/eksistensi |
| Deferred | 2+ | THP/ending/JP-dup dst., dengan fase penerima |

## Papan 2 — Functional parity (`FE9_TEST_MATRIX.csv` + `FE9_BEHAVIOR_SPEC.md`) — 2026-11-10

| Metrik | Nilai |
|---|---|
| Test PASS / total | **1 / 53** — T-BASE-001 PASS 2026-11-10 (fresh clone → generator → diff normalisasi-EOL nol; `.gitattributes eol=lf` ditetapkan) |
| BRV terverifikasi | 53 entri (12 seed + SCT×13 (incl **SCT-004 native catalog 390/390**, SCT-005 protokol wait/arg/antrean, SCT-006 semantik native batch-2, **SCT-009 dispos_c grammar+99 unit template CLOSED**, **SCT-005d scenario flows 2075 func/145 MapLoad + CH1 verbatim**, **SCT-005e native skenario+flag-table+queue**, **SCT-005f re-audit batch-3: KOREKSI InstantDispos=spawn-instan dari pack (bukan despawn) + layout flag-table 96-slot + BGM slot table 0x30 + path-digit CLOSED 4/6/2/8→dir1/2/4/8**, **SCT-005g linked-list per-pihak 0x184 + butai UI + BMAP phase-0x1C**, **MAP-002 bmap01 slot-table + MAP-003 grid 64x64 maplink & dispos 4 varian**, **SCT-007 dispos_c payload 100×0x6C CLOSED**, **SCT-008 dispos per-peta: pack c/h/m/n + grid 108/112 + Ch1 4 unit IKE/MIST/BOLE/GREIL**) + **BTL-002 tabel inti zdbx: timing tempur+kapak-bumerang dkk+nama→ID** + **BTL-003 video harvest Ch1-2: forecast 6-field ATK/Hit/Ava/Crt/Dmg/Recv, triangle-halving TERUKUR 10v1, weapon-0 bare-hand, Y=End, radial wheel, rescue hand, MIA death** + **BTL-004 SUPERSEDED (label "Boyd" = misread 640p; koreksi: Shove ✓, pair-format ✓)** + **BTL-005 SUMBER HD 720p: forecast Mt/Hit/Crit terbaca penuh, Mt=kerusakan final DECIDED (Ike 7→Δ7, Bandit 6→Δ6), cap Hit 100, Ddg=DEF, panah triangle, badge ×2 (trigger UNKNOWN), inv Bandit 3×Iron Sword, timeline 30 chapter HD** + **BTL-006 HD batch-2: Titania vs Volke (crit40 OPEN, ×2 rasio-HP kandidat), LVLUP screen HP+1↑ 8-stat + ground truth Titania Paladin Lv2, inventaris 4-slot E-badge+grey-out, objective ke-3 `Clear Escape`, Thicket Def1, radial wheel penuh** + **BTL-007 MODEL DISPLAY: Atk=total (Iron Axe Mt8 dari Atk20-Str12), Mt=Atk−Def (3x), Hit=raw−Avo cap100, Avo=evasion-diri; xN=PAIR-UP support (HP-ratio GUGUR, Ike 22v23 x2); weapon-diamond 4-slot; radial penuh; ring biru-merah-ungu+bracket-putus+biru-gerak-oranye-range** + **SCT-005b semantik native batch-1: unit runtime 160x0x280@802B14E4, PID+0x188, handle u8+0x198, XY di +0x19E-F; WaitM ms->frames+queue+ff-skip; MapLoad->BMAP; FUN_80015320=arg-api 53 impl; FE9_UNIT_RUNTIME.csv** + **SCT-005c: handle=index slot 1-based ((h-1)x0x280), path=str digit numpad ("444442"), InstantDispos+SallySetByGroup+MapLoad = wrapper BMAP 80330708** + **SCT-009 dispos_c GRAMMAR CLOSED: offset-table 639 BE, record var-len ptr-patch pass1, pool suffix-dedup, GRUP AI EQ/SEQ/MTYPE+5param (HEROKILLER), pass2 intern 36 event** + **SYS-002 menu OPSI 11-setting (Animation Individual, Combat Camera, Game/Message Speed, Grid Thickness slider, Terrain/Goal, Unit/Combat Window, Message Display, Grid/Cursor Color palet)** + CAP-001 input-channel+peta-800ms + **INP-002 arsitektur input Android+jebakan NativeActivity** + EVT-002 + AUD-002/003 + FNT-001 + GFX-001 + ANM-004 + **BTL-008 video-mine pipeline 21,9jam: JENDELA TERRAIN `Bridge Avo0/Def0/MDf` TERKONFIRMASI, Bonus-EXP screen 1244P+cap100+exp0-99/level, durabilitas Wind40/Vuln3/SteelLance35/Javelin25, forecast Brom 6-field** + **SVT-001 save GCI GFEE01: 8 slot ×0x4000, blok FE8Data-like FE8J/SYSF/BMST(nama bmap)/MDST/UNIP(stride 0x8F, +0x09=EXP)/SOKO/FWEP, save-IO 801DFB4C-an** + **BTL-008 batch-2: unit-window final (Lv+EXn, rank-badge senjata, dur cur/max, Hit MENTAH>100, Effect Flying), move=SOLID vs attack=CHECKER+outline gabungan, badge-rescue, terrain HUD selalu-on (`terrain Avo Def MDf` + `Clear <obj>` + Turn), layar save `Ch.NN: Judul`+tag Adv/Base+PLAY TIME** + **SYS-003 judul bab resmi 5/30 (Ch4/8/21/25/26)** + **SCT-005h native batch-4: UnitGetByPID=handle+0x198, Focus=pan-interpolasi vs Instant*=instan+finalize, InstantSetMapCamera=slot kamera x/y/z+zoom, ABI float dua-jalur (int vs XOR-bias) dikonfirmasi, iterator filter unit 0x9F, pembuka bank SE /pool** + **verdict terrain: no structural table (zdbx/FE8Data/s_rect/DOL dicek) — engine default+UI-sampel; objek Seize/DefeatBoss/Escape/Rout**, **SCT-010 REGISTRY MODUL PROC CLOSED: 307 modul+handler terekstrak, U012 tuntas, bl->registrar 1618 koreksi**, **SVT-002 UNIP GRAMMAR DARI KODE CLOSED: urutan blok save 7× + stream loader (tmplID u16→tabel 100B, 3 handle→ptr unit, 7 byte live s/d EXP, blok 4/4/3/8/8/16B, 8 item [u16 id][u8 dur] → id items.csv ✓; stream 159-slot presisi (kosong 2B / penuh tepat 143B, walker-verified), ekor SEQID 15B via ATK/MOV/HEAL/MTYPE_TABLEID_LIST = link SE-bank, grp byte = list faksi, h2 = anchor list per peta; blok +0x13 = LIVE mirror, U013 tuntas**, **ANM-005 gramma nama anim+varian senjata 374/374 pack CLOSED (poi/mov/at/cr/sk/dam/ded/lda/ldc/dog × sw/sp/ax/no/ar/ja... = syarat aset per (class,weapon) engine)** — 0 domain `Unknown` penuh (sisa: penamaan byte UNIP = P2-validator) |
| Chapter parity | 0/30 |
| Milestone P1 | (1) MODEL+ANIMASI visual ✓ archer; (2) SCRIPT VM ✓ (C00 Opening0 terbaca data); (3) **BGM DECODED ✓** — `bin/stm.mjs` → WAV 32kHz stereo (boss1 61.1s); (4) **FONT DECODED ✓** — `bin/gcf.mjs` → PGM atlas+CSV, fe/talk/alpha/biggana/system(.gcf+.cms); (5) **TEXTURE+SKIN ✓** — C4 fixed, binding `tex_<idx>` + layered face + bone-skinning (AM-009/047 CLOSED, BRV-GFX-001/ANM-004) |
| Milestone P2 (PoC engine) | **2026-11-11: INTERACTIVE DI PERANGKAT** — `poc/` (C++20+GLES3+Oboe+JNI, Activity biasa): fpk loader, .g/.gs/.ga + grid 10m + BGM 2-stream + touch overlay; **camera orbit+lookAt benar (bug eye-translation = "quad raksasa" CLOSED)**, **input sentuh live** (`taps2s=1, yaw 35→112, z 0→3` — swipe panah: held-state, 4-arah + putar; rapid-fire capable karena state, bukan event). **Pitfall Android-15: NativeActivity+SurfaceView = `touchableRegion=<empty>` = input hilang senyap → cutover Activity biasa + OnTouchListener (BRV-INP-002)**. **2026-11-12: GRID MOVEMENT + TAP-TO-MOVE + TRUE FULLSCREEN** — state unit (gx,gy FIFO-path cap22), dpad→sumbu-dunia-terbaik di yaw iso-45 tetap, tap raycast pitched-basis→ubinan, follow-cam focus-lerp + cursor ubinan; **edge-to-edge immersive-sticky + cutout shortEdges → view = full 2340×1080, mapping tap diverifikasi presisi (R@0.25w)**, viewport live via eglQuerySurface + cache tombol ikut resize. Device: R×2 4,4→6,4; tap→tile 5,4 walk-back; screenshot fullscreen bersih ✓. **2026-11-12b: GAMEPAD FISIK + KEYBOARD LIVE (BRV-INP-003)** — `input_poll()`→`InputState{Touch,Pad}` satu snapshot/frame; Kotlin dispatchGenericMotionEvent+onKeyDown/Up→JNI padButton/padAxis (enum ABI dua file); LX=orbit, RX/RY=look (pitch global baru), DPAD/HAT=nav grid+auto-repeat, A=confirm, B=back, START=pause-stub, L3/R3=camera-fit; tanpa pad `pad_update` inert = touch identik (fallback by-design); keyboard SPACE/ESC/ENTER/panah ikut jalur sama; `assembleDebug` SUCCESS b52 — QA perangkat: lihat `adb logcat -s fe9poc`. **2026-11-12c: KONVERTER ASET + OVERRIDE FOLDER (jalur swap IP, AM-075)** — `bin/aiconv.mjs` PNG→TPL/CMS writer (I8/IA8/RGB5A3/RGBA8/C4/C8/CMPR + LZ77 magic-0x10 klon byte-exact decoder disc); round-trip 6 varian vs `tex_4.tpl` asli di pipeline engine sendiri: CMPR meanΔ=0.58/maxΔ=58 (visual tak terbedakan), rgb5a3 4.5, C8 6.0, C4 10.7. Loader PoC: lookup override folder SEBELUM FPK (env root + Android/data + internal, log `OVERRIDE HIT`); **pitfall SELinux API 29+ terdokumentasi: `adb push` → Android/data = `shell_data_file` tak terbaca app; resep teruji = pipe `run-as`** (berlaku emulator & HP fisik tanpa root). Fix bug laten ordo header TPL di `texfile.h` (u16 **h dulu** — salah untuk semua tekstur non-persegi). Uji diterima di emulator x86_64 API 34: `tex_4.tpl` (128×128 CMPR archer) → potret ANNA 192×128 CMPR dari folder → log HIT + texture 192×128 + unit tampil beda total (fe9-captures/override base vs hit3.png), FPK utuh sha256, g/gs/ga/stm tetap FPK; abiFilters +x86_64 (lane emulator) |
| Gate P1→P2 | **font `.gcf` ✅ Decoded (AM-024)** + BGM ✅; PoC **jalan**; tinggal: T-RE-CAP1 (SE gcfesnd = opsional P1, target P2) |
| **Gate P2 (TEKNIS)** | ✅ **CLOSED 2026-11-12** — item(1)(2)(3)(5) LULUS device, (4) PARTIAL-by-design (`.samp` followup). Bukti: b57 `tapLatMax` worst 16,5ms (3× margin), b58 `tex pick src=1` ANNA 14-img native, thermal 35-37C, rapid 6/2s. Keputusan delegasi: A-i B-i C-i + **BAR VISUAL: paritas FE9 GC, A/B vs Dolphin, build P3 pertama = aset asli** (docs/FE9_P2_GATE.md). **D1 terkunci** |
| Fase aktif | **P3 Core Engine Foundation** (P3-M1 modul + P3-M2 internal-build ber-aset-asli) |

## Gap tertinggi
1. ~~Blend-skinning~~ **CLOSED 2026-11-10**: bit0=bone-index (pipeline g/ga membuktikan posing; BRV-ANM-004); bit1=palette multi-slot = P2-detail.
2. 404 native `.cmb` — signature/semantik per nama belum (pool names + argcount saja), sebagian butuh Ghidra xref per fungsi → **sekarang ada lane objdiff/dtk (`decomp/`) utk xref+match per fungsi, 2026-11-11**.
3. Domain BRV battle/AI/map/rng `Unknown` — butuh capture human (T-RE-CAP1); kandidat match pertama = jalur BRV-BAT (battle calc), alamat dipilih via xref Ghidra→symbols.txt.
4. `.samp` payload SE = BUKAN DSP standar (BRV-AUD-003) — butuh trace RAM ucode AX via
   Dolphin (capture human) ATAU reverse encoding custom; target P2.

## Infra decomp (2026-11-11)
- `decomp/` (dtk 1.8.4 + dtk-template, CC0): split→relink **byte-identical** (`build.sha1` OK);
  9.251 fungsi / 17.554 symbols terdeteksi otomatis; CW runtime ternama (`__start` dkk) auto-named.
- Diagnosis disc: **NKit-relaxed repack** (bukti lengkap di `docs/FE9_DTK_SESSION_NOTES.md`);
  `main.dol` md5 identik lintas toolchain (JS RVZ reader vs NKit 3.2).

## Capture manusia masuk (2026-11-12, `fe9-captures/`)
- `agent/ch3|ch4|ch5.png` (Nov11 18:28-29): frame SAMA bmap stage-boss (Turn 3, "Clear / Defeat Boss"),
  panel terrain **Tree: Avo 0 / Def 0 / MDf 0** + move-range oranye + kursor gerak = dukung BRV-BTL-008
  (terrain default nol utk Tree; UI panel = oracle bonus). T-RE-CAP1 [gebang] → TERIMA parsial.
- `poc_gm_grid|final.png` (05:06-18): device PoC — archer + bow/quiver, yaw orbit, sword+bow equip view,
  tile cursor + overlay; fullscreen gesture-bar (immersive OK).
- `override/base|hit|hit2|hit3|restored.png` (07:46-08:07, uji AM-075 di HP):
  base=archer normal ✓; hit/hit2=dialog ANR **"Settings isn't responding"** di atas app redup =
  NOISE SISTEM (app hidup, overlay tampil) — bukan bug app; hit3=override KENA ke unit jenderal
  (model+sebagian tex terganti, area MAGENTA = slot tekstur tak dicakup file override = limitasi by-design);
  restored=normal ✓ → **OVR device QA PASS dgn catatan coverage**.
- `poc_emu13.png`, `agent/` 557 artefak lama tetap arsip.

## Penambahan offline mining (2026-11-12 sesi sore)
- **staffroll.bin CLOSED 105/105**: tabel offset u32 @2780 (≈4-aligned ±8) + monotonic matching → urutan kredit
  tampil penuh (tag `#F04*`, nama ASCII+SJIS 川西, duplikat bersebelahan = label role berlanjut) →
  `docs/data/staffroll_credits.csv`. Sisa row `/ending|.bin` = RectFinale (rect ending) → tetap Partial.
- **/shop shopitem_h/m/n** (tetap Partial, struktur terpetakan): `T1[16..tOff)` = u32 pointer array TER-SEPARASI 0 →
  **1013 grup** (ukuran 2-14 = daftar stok per toko); slot table `@tOff+32` (cnt u32 → entri T1 non-separator);
  pool = 41 IID_* + MDV_* + ts 2005-02-20; tail pool = hook event `WSHOP_ITEMS_C0..C31` per chapter.
  PR kecil: rule presisi pointer→nama (posisi 4-aligned meleset 0-8 dr start) + field harga/qty = P5 ekonomi.
- Counts tetap: 31 Decoded / 13 Partial / 1 Investigating / 10 Discovered / 2 Deferred.
- **P2 gate package [BRV-P2-001]**: `docs/FE9_P2_GATE.md` (tabel 5 kemampuan + bukti + 3 keputusan D-P2-A/B/C)
  + build **b57** = instrumentasi gate: `tapLatMax/tapLatN` (ACTION_DOWN→render), `th=..C` thermal_zone0,
  tombol overlay **T** = cycle semua gambar tpl+`face/system.cms` ke slot unit (`tex pick:`). BUILD SUCCESSFUL;
  install+capture = aksi human berikutnya; keputusan (i)/(ii) per D-P2-A/B/C mengunci gate.
