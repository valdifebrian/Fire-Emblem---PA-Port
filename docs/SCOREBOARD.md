# SCOREBOARD — snapshot kemajuan (2 papan TERPISAH, master plan §5.1)

> Kemajuan proyek yang SESUNGGUHNYA = **Functional parity**. Asset readiness hanya kesiapan
> bahan; Decoded ≠ sistem selesai. Update tiap commit yang mengubah matriks/test/BRV.

## Papan 1 — Asset readiness (`FE9_ASSET_MATRIX.csv`, 57 keluarga, 13 kolom) — 2026-11-12 (regen; scope/ekstensi terpisah kolom)

| Status | Keluarga | Catatan |
|---|---|---|
| Verified | **0** | belum ada aset masuk engine |
| Decoded | **33** | **BUG KEY MATRIX DIPERBAIKI**: 9 entri MAP berkey `"/|X"` tak pernah match family `"/X|.ext"` → status diam-diam jatuh UNKNOWN; kini pulih (FE8Data register, cp_data, FE8Anim, tut_data×2, soundroom, client, opening.bnr). +**/zbg cmp + /zmap cmp CLOSED (probe 241/241; bug truncate LZ77 u24-hint bohong DI-FIX cmp.mjs — 21 file peta utk-ekstraksi)**; +**/zu pak + /xwp cmp + /xcam cmp ISI CLOSED (probe 374/374 pack: matriks kelas×senjata + kosakata aksi BRV-ANM-005)**; +**staffroll KREDIT TERBACA**; +`Face|.bin`≡facedata CLOSED; **+NATIVE CATALOG 390/390** (BRV-SCT-004); +facedata 358×3-slot; +`/s` 198/198; sweep `.tpl/.cms` 0-fail; C4+binding+potret; `.gcf`, STM, VM, 3D |
| Partial | **16** | +tut_data_en+soundroom+FE8Anim pulih dari UNKNOWN. **keluarga tabel rekuren `[size/tableOff/cnt/sub]`** terpetakan di rect×3/RectBases/shopitem×3/staffroll/RectFinale/tut_data/soundroom; +`dispos_c` AI-stream; SE bank; dsb. |
| Investigating | **2** | `.samp` payload (≠ DSP standar, bukti LSQ+ZCR — butuh trace ucode AX, P2); FE8Effect.bin (P6) |
| Discovered | **2** | client.bin + 1 UNKNOWN riil — hanya magic/eksistensi |
| Deferred | **4** | THP/ending/JP-dup/opening.bnr dst., dengan fase penerima |

## Papan 2 — Functional parity (`FE9_TEST_MATRIX.csv` + `FE9_BEHAVIOR_SPEC.md`) — 2026-11-10

| Metrik | Nilai |
|---|---|
| Test PASS / total | **3 / 53** — T-BASE-001 PASS 2026-11-10; **T-RE-SCRIPT1 + T-SCT-VM PASS 2026-11-12** (ISA 66-opcode diverifikasi asm PPC + replay golden 40/40 skenario `done=1 depth=0`, nol hang — `poc/tools/`) |
| BRV terverifikasi | 61 entri (**SCT-005o batch-8: CopyMapCamera/PastMapCamera=snapshot kamera @0x80330E90..9C+FF-jalur-tanpa-interpolasi(+0x10/14 start=target)+zoom FUN_800193cc; SetEventCameraPrec/UnitFocusOff/UnitFadeOff=field-writer antrean (+0x30=flag-aktif!, +0x32, +0x3C); BuildInstHide/Show=instance layer-0x80000 + enqueue/dequeue AKSI-0x29 + dirty-map + REFRESH-TERRAIN (bangunan↔pathing), BuildDestruction=mask0xA00 aksi-0x25+anim+rearm; ENUM RING AKSI-TILE: 0x23 peti, 0x24 visit, 0x25 runtuh, 0x29 bangunan; SetShooter=enq-varian-5-arg FUN_8010fdb8; RectKill=FUN_80109020 tutup-window + jalur RID_GMAP; RectSetCurve=slot 5..7 XOR-bias -konstanta + sentinel 0x75BCD15 (Partial); DefaultDieTalk=GLOBAL 0x80330EAC lastDied→pid→tabel death-talk — engine WAJIB set lastDied; ENGINE: cam save/restore + 10 handler bound=84 LIVE** + **SCT-005n batch-7 + dump-asm stub &LAB (headless -processor PowerPC:BE:32:default, script): r13=0x8036EDA0 TERDECODE → seluruh (r13+n) terkonkret; AchieveSet=tabel achieve stride 0x40 nilai@+0x200 + scaling diff-byte 0x803307AE (2→/2, 3→×1.5); DialogResult=i8 global + Dialog2Items menu-2-item; SFXPlay→FUN_801475fc(id,100) bank-pool; SKIP-WORD u32@0x80367210 (bit0=FF, DisableSkip|=4, GCST=read; SFX/BGM-guard &3); BMSetValue/Get=u16[16]@BMAP+0x00 (BM0..15 script vars; diff-byte=+0xA6); MindGetMe/Target=handle dari [802EF4C8/CC] mind-AI pasang aktif; EventGet X/Y=i16+Check=byte+0xA @0x8031FFC0; GetBattleType=u32@0x80331B54; Normal/Hard/Maniac=predikat byte+0xA6 ENCODING PARTIAL (v0 semua-true); BGM slots 0x30@8032FF08: Cont(vtbl isPlaying+ramp100→v)/Mute/PlayVol(slot,name,vol)/QuietOff instan; ForceGetFirst=list@802CA264+f*0xC head; TBoxOpen=instance-lookup stride0xB4 mask-0x410 + anim + AKSI-TILE 0x23 ring(map+0x204); VisitOut=mask 0xA00 aksi-0x24 (peti+rumah = sistem instance+ring SAMA maplinkAt); FocusWait re-entrant; Join=state-5. ENGINE: +27 handlers bound74 live, boot→dialog ✓** + **SCT-005m: regist(name)=registrar tabel-flag 96-slot (FUN_800bb080→bddcc), clr=clear-bit byname, helper bit bdb9c/bdb78/bdb54 (+idx>>3|+0x300), bded8=reset antar-bab, FLAG JEMBATAN OBJECTIVE: gf_gameover@FUN_80028724 + gf_complete@FUN_8002da30 BACA table yang sama; INVENTARIS 2121 situs/501 nama SJIS (docs/data/FE9_SCRIPT_FLAGS.csv): 宝箱N, 増援トリガ/イベントN handshake, ボス死亡 pemicu ending, 訪問NN, 情報Nフラグ, ボス戦闘会話&lt;unit&gt;×29bab — sistem event-objective terbaca STATIS** + engine: set/get/clr/regist map + wedge-grace-fix + **SCT-005l batch-6: set/get=NAMED EVENT-FLAG bitset 96-slot @803307bc+bitset+0x300 (mesin always_* dispos!), UnitWalkDir/dkk=field action-queue 80296F90 (dir nibble +0x34&0xF, speed global 803307A8), UnitMoveWait re-entrant, EventCamera keyframe At/Rot type0/1 FUN_80017840 XOR-bias ULANG konfirmasi, ForceGetCount=list faksi 802CA264+f*0xC, AllUnitEscape 2-pass 160-slot +0x1A0-bit1→despawn, ENVSilentOff=fungsi-3 env-audio** + **SCT-005k batch-5: BGMPlay=guard-ptr+named per-slot vol100, BGMFadeOut slot×0x30, ENVSilent/Noisy instan, WaitF=timer-list +0x60 ff-skip, FocusHardWait=re-entrant latch mode-hard 0x80320060** + **SCT-005j golden-trace VMSTR: grammar C01 ber-string TANPA device (MapLoad bmap01; grup bmap01_mikata_c/first_*; path numpad "444442"; BGM bernama BGM_EVT_IKE1; dispos bmap01: squad IKE/MIST/BOLE/GREIL + JID_* + SEQ_NOATTACK/NOMOVE/NOHEAL+SID_IMMORTAL; enemy Ch1 = butuh capture)** + **MAP-005 water-mask bmap01 = footprint water0.gs 124 tile + maplinkAt=rekaman (x,y,type) patch (bukan air) + loader cost/exits di device (water=124/696, exits=27) + FIX z-konvensi tile_cz (unit selama ini off 2.5 tile di Z)** + 12 seed + SCT×13 (incl **SCT-004 native catalog 390/390**, SCT-005 protokol wait/arg/antrean, SCT-006 semantik native batch-2, **SCT-009 dispos_c grammar+99 unit template CLOSED**, **SCT-005d scenario flows 2075 func/145 MapLoad + CH1 verbatim**, **SCT-005e native skenario+flag-table+queue**, **SCT-005f re-audit batch-3: KOREKSI InstantDispos=spawn-instan dari pack (bukan despawn) + layout flag-table 96-slot + BGM slot table 0x30 + path-digit CLOSED 4/6/2/8→dir1/2/4/8**, **SCT-005g linked-list per-pihak 0x184 + butai UI + BMAP phase-0x1C**, **MAP-002 bmap01 slot-table + MAP-003 grid 64x64 maplink & dispos 4 varian**, **SCT-007 dispos_c payload 100×0x6C CLOSED**, **SCT-008 dispos per-peta: pack c/h/m/n + grid 108/112 + Ch1 4 unit IKE/MIST/BOLE/GREIL**) + **BTL-002 tabel inti zdbx: timing tempur+kapak-bumerang dkk+nama→ID** + **BTL-003 video harvest Ch1-2: forecast 6-field ATK/Hit/Ava/Crt/Dmg/Recv, triangle-halving TERUKUR 10v1, weapon-0 bare-hand, Y=End, radial wheel, rescue hand, MIA death** + **BTL-004 SUPERSEDED (label "Boyd" = misread 640p; koreksi: Shove ✓, pair-format ✓)** + **BTL-005 SUMBER HD 720p: forecast Mt/Hit/Crit terbaca penuh, Mt=kerusakan final DECIDED (Ike 7→Δ7, Bandit 6→Δ6), cap Hit 100, Ddg=DEF, panah triangle, badge ×2 (trigger UNKNOWN), inv Bandit 3×Iron Sword, timeline 30 chapter HD** + **BTL-006 HD batch-2: Titania vs Volke (crit40 OPEN, ×2 rasio-HP kandidat), LVLUP screen HP+1↑ 8-stat + ground truth Titania Paladin Lv2, inventaris 4-slot E-badge+grey-out, objective ke-3 `Clear Escape`, Thicket Def1, radial wheel penuh** + **BTL-007 MODEL DISPLAY: Atk=total (Iron Axe Mt8 dari Atk20-Str12), Mt=Atk−Def (3x), Hit=raw−Avo cap100, Avo=evasion-diri; xN=PAIR-UP support (HP-ratio GUGUR, Ike 22v23 x2); weapon-diamond 4-slot; radial penuh; ring biru-merah-ungu+bracket-putus+biru-gerak-oranye-range** + **SCT-005b semantik native batch-1: unit runtime 160x0x280@802B14E4, PID+0x188, handle u8+0x198, XY di +0x19E-F; WaitM ms->frames+queue+ff-skip; MapLoad->BMAP; FUN_80015320=arg-api 53 impl; FE9_UNIT_RUNTIME.csv** + **SCT-005c: handle=index slot 1-based ((h-1)x0x280), path=str digit numpad ("444442"), InstantDispos+SallySetByGroup+MapLoad = wrapper BMAP 80330708** + **SCT-009 dispos_c GRAMMAR CLOSED: offset-table 639 BE, record var-len ptr-patch pass1, pool suffix-dedup, GRUP AI EQ/SEQ/MTYPE+5param (HEROKILLER), pass2 intern 36 event** + **SYS-002 menu OPSI 11-setting (Animation Individual, Combat Camera, Game/Message Speed, Grid Thickness slider, Terrain/Goal, Unit/Combat Window, Message Display, Grid/Cursor Color palet)** + CAP-001 input-channel+peta-800ms + **INP-002 arsitektur input Android+jebakan NativeActivity** + EVT-002 + AUD-002/003 + FNT-001 + GFX-001 + ANM-004 + **BTL-008 video-mine pipeline 21,9jam: JENDELA TERRAIN `Bridge Avo0/Def0/MDf` TERKONFIRMASI, Bonus-EXP screen 1244P+cap100+exp0-99/level, durabilitas Wind40/Vuln3/SteelLance35/Javelin25, forecast Brom 6-field** + **SVT-001 save GCI GFEE01: 8 slot ×0x4000, blok FE8Data-like FE8J/SYSF/BMST(nama bmap)/MDST/UNIP(stride 0x8F, +0x09=EXP)/SOKO/FWEP, save-IO 801DFB4C-an** + **BTL-008 batch-2: unit-window final (Lv+EXn, rank-badge senjata, dur cur/max, Hit MENTAH>100, Effect Flying), move=SOLID vs attack=CHECKER+outline gabungan, badge-rescue, terrain HUD selalu-on (`terrain Avo Def MDf` + `Clear <obj>` + Turn), layar save `Ch.NN: Judul`+tag Adv/Base+PLAY TIME** + **SYS-003 judul bab resmi 5/30 (Ch4/8/21/25/26)** + **SCT-005h native batch-4: UnitGetByPID=handle+0x198, Focus=pan-interpolasi vs Instant*=instan+finalize, InstantSetMapCamera=slot kamera x/y/z+zoom, ABI float dua-jalur (int vs XOR-bias) dikonfirmasi, iterator filter unit 0x9F, pembuka bank SE /pool** + **verdict terrain: no structural table (zdbx/FE8Data/s_rect/DOL dicek) — engine default+UI-sampel; objek Seize/DefeatBoss/Escape/Rout**, **SCT-010 REGISTRY MODUL PROC CLOSED: 307 modul+handler terekstrak, U012 tuntas, bl->registrar 1618 koreksi**, **SVT-002 UNIP GRAMMAR DARI KODE CLOSED: urutan blok save 7× + stream loader (tmplID u16→tabel 100B, 3 handle→ptr unit, 7 byte live s/d EXP, blok 4/4/3/8/8/16B, 8 item [u16 id][u8 dur] → id items.csv ✓; stream 159-slot presisi (kosong 2B / penuh tepat 143B, walker-verified), ekor SEQID 15B via ATK/MOV/HEAL/MTYPE_TABLEID_LIST = link SE-bank, grp byte = list faksi, h2 = anchor list per peta; blok +0x13 = LIVE mirror, U013 tuntas**, **ANM-005 gramma nama anim+varian senjata 374/374 pack CLOSED (poi/mov/at/cr/sk/dam/ded/lda/ldc/dog × sw/sp/ax/no/ar/ja... = syarat aset per (class,weapon) engine)** + **SCT-005i kontrak runtime native device-CLOSED: drain-antrean-walk scene-agonostik (resep wedge sunyi MoveWait), WaitM satu-slot latch+clear-on-expiry, TalkEvent serial+resume-guard tanpa restart, auto-registrasi native-gap, tool ring.mjs (ring↔disasm)** — 0 domain `Unknown` penuh (sisa: penamaan byte UNIP = P2-validator); **+ SCT-002b temuan replay VM: 0x3f=YIELD (bukan terminasi), 0x40=SKIP4, CMP/branch push bit1 PPC (`0`=equal → JZ/JNZ terbalik dari int-==), rotl/clz helper, walk-hanya-opcode-tervalidasi; golden replay 40/40 skenario `done=1`** |
| Chapter parity | 0/30 |
| Milestone P1 | (1) MODEL+ANIMASI visual ✓ archer; (2) SCRIPT VM ✓ (C00 Opening0 terbaca data); (3) **BGM DECODED ✓** — `bin/stm.mjs` → WAV 32kHz stereo (boss1 61.1s); (4) **FONT DECODED ✓** — `bin/gcf.mjs` → PGM atlas+CSV, fe/talk/alpha/biggana/system(.gcf+.cms); (5) **TEXTURE+SKIN ✓** — C4 fixed, binding `tex_<idx>` + layered face + bone-skinning (AM-009/047 CLOSED, BRV-GFX-001/ANM-004) |
| Milestone P2 (PoC engine) | **2026-11-11: INTERACTIVE DI PERANGKAT** — `poc/` (C++20+GLES3+Oboe+JNI, Activity biasa): fpk loader, .g/.gs/.ga + grid 10m + BGM 2-stream + touch overlay; **camera orbit+lookAt benar (bug eye-translation = "quad raksasa" CLOSED)**, **input sentuh live** (`taps2s=1, yaw 35→112, z 0→3` — swipe panah: held-state, 4-arah + putar; rapid-fire capable karena state, bukan event). **Pitfall Android-15: NativeActivity+SurfaceView = `touchableRegion=<empty>` = input hilang senyap → cutover Activity biasa + OnTouchListener (BRV-INP-002)**. **2026-11-12: GRID MOVEMENT + TAP-TO-MOVE + TRUE FULLSCREEN** — state unit (gx,gy FIFO-path cap22), dpad→sumbu-dunia-terbaik di yaw iso-45 tetap, tap raycast pitched-basis→ubinan, follow-cam focus-lerp + cursor ubinan; **edge-to-edge immersive-sticky + cutout shortEdges → view = full 2340×1080, mapping tap diverifikasi presisi (R@0.25w)**, viewport live via eglQuerySurface + cache tombol ikut resize. Device: R×2 4,4→6,4; tap→tile 5,4 walk-back; screenshot fullscreen bersih ✓. **2026-11-12b: GAMEPAD FISIK + KEYBOARD LIVE (BRV-INP-003)** — `input_poll()`→`InputState{Touch,Pad}` satu snapshot/frame; Kotlin dispatchGenericMotionEvent+onKeyDown/Up→JNI padButton/padAxis (enum ABI dua file); LX=orbit, RX/RY=look (pitch global baru), DPAD/HAT=nav grid+auto-repeat, A=confirm, B=back, START=pause-stub, L3/R3=camera-fit; tanpa pad `pad_update` inert = touch identik (fallback by-design); keyboard SPACE/ESC/ENTER/panah ikut jalur sama; `assembleDebug` SUCCESS b52 — QA perangkat: lihat `adb logcat -s fe9poc`. **2026-11-12c: KONVERTER ASET + OVERRIDE FOLDER (jalur swap IP, AM-075)** — `bin/aiconv.mjs` PNG→TPL/CMS writer (I8/IA8/RGB5A3/RGBA8/C4/C8/CMPR + LZ77 magic-0x10 klon byte-exact decoder disc); round-trip 6 varian vs `tex_4.tpl` asli di pipeline engine sendiri: CMPR meanΔ=0.58/maxΔ=58 (visual tak terbedakan), rgb5a3 4.5, C8 6.0, C4 10.7. Loader PoC: lookup override folder SEBELUM FPK (env root + Android/data + internal, log `OVERRIDE HIT`); **pitfall SELinux API 29+ terdokumentasi: `adb push` → Android/data = `shell_data_file` tak terbaca app; resep teruji = pipe `run-as`** (berlaku emulator & HP fisik tanpa root). Fix bug laten ordo header TPL di `texfile.h` (u16 **h dulu** — salah untuk semua tekstur non-persegi). Uji diterima di emulator x86_64 API 34: `tex_4.tpl` (128×128 CMPR archer) → potret ANNA 192×128 CMPR dari folder → log HIT + texture 192×128 + unit tampil beda total (fe9-captures/override base vs hit3.png), FPK utuh sha256, g/gs/ga/stm tetap FPK; abiFilters +x86_64 (lane emulator). **2026-11-12 M2.2: PETA ASLI CH1 DI DEVICE** — bmap01.gs 6028tris + texpack.tpl img0 512×1024 (F14) → padang I dira hijau A/B vs referensi ✓; grid 29×24 selaras peta (tile_cx=-140+10g); **BUGS CLOSED: `step_tile` bound era-M1 10×10 (semua gerak ditolak diam-diam), white-tex tanpa mip+filter default=incomplete→kursor HITAM, uDim semantics terbalik→dunia hitam permanen post-fade, `input tap` Xiaomi=koordinat fisik tanpa rotasi (patch Kotlin rotasi DIKEMBALIKAN — jari asli sudah dirotasi framework, BRV-INP-002 tetap valid)**; **WALK feel [BRV-ANM-006]: state-machine anim move_n↔wait_n per edge walk/stop (36f loop), ticker anim 3x-lambat→1:1 30fps (desync kaki = efek "bidak catur"), speed 1.4→0.85 u/frame (~0.4s/tile selaras stride), yaw easing 25%/frame; B debug-cycle tetap** — device verified wk1-4 ✓. **2026-11-12 M2.3: DIALOG BOX ASLI** — face/anna.cms (14-img, img0 192×128) dibake ke fpk (`poc_list.txt` + rebake), layout rasio GC (box .033–.967 × .713–.96, dark-glass .82 + frame ganda, nameplate FUKIDASI[1] krem asli menindih tepi atas potret, teks putih offset potret, cursor arrow.tpl asli); **BUG GLIF KLOPEN: `.gcf dy` = jarak PUNCAK glyph DI ATAS baseline (bukan offset-dari-atas) — pembuktian metrik A(17/h17), '.'(3/h3), '''(16/h6); sebelumnya koma=titik-tinggi & huruf melorot**; pemicu dialog dev-key = T (KEYCODE 48), X/X-tombol fisik = PADB_X ✓; device verified dlg0-2 ✓ |
| Gate P1→P2 | **font `.gcf` ✅ Decoded (AM-024)** + BGM ✅; PoC **jalan**; tinggal: T-RE-CAP1 (SE gcfesnd = opsional P1, target P2) |
| **Gate P2 (TEKNIS)** | ✅ **CLOSED 2026-11-12** — item(1)(2)(3)(5) LULUS device, (4) PARTIAL-by-design (`.samp` followup). Bukti: b57 `tapLatMax` worst 16,5ms (3× margin), b58 `tex pick src=1` ANNA 14-img native, thermal 35-37C, rapid 6/2s. Keputusan delegasi: A-i B-i C-i + **BAR VISUAL: paritas FE9 GC, A/B vs Dolphin, build P3 pertama = aset asli** (docs/FE9_P2_GATE.md). **D1 terkunci** |
| Fase aktif | **P3 Core Engine Foundation** (P3-M1 modul ✅ + P3-M2 internal-build ber-aset-asli [sisa QA human] + **P3-M3 VM+replay ✅ CLOSED 2026-11-12**) + **P4-M0 script-in-engine ✅ CLOSED 2026-11-12: Opening0 C01 end-to-end DI DEVICE — ATTRACT→start→movie→MapLoad ch01→spawn/move→6 dialog `MS_01_OP_01..04` (font+potret asli)→`DONE @f=2316`→WORLD interaktif (DPAD menggeser unit 16,18→17,18; `open_done_world.png`); bug closed: wedge sunyi drain-off saat dialog (SCT-005i)** |

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

## P3-M1 progress (2026-11-12 malam)
- **M1.6 UI Engine CLOSED** — `poc/.../ui.h`: atlas talk.gcf runtime (95 glyph RGBA, atlas 512²), krom TPL asli
  (fukidasi img1 = name banner, arrow img0 = 12-frame cursor), dialog scene di stack SceneMgr:
  word-wrap width-aware, typewriter 2 char/frame, A=reveal-all→A=halaman berikutnya→pop otomatis.
  Device proof: attract→world(ESC)→dialog(T)→3 halaman ANNA/IAN/ANNA (SPACE=CONFIRM)→pop, log
  `scene: dialog -> world`; capture ui_dlg4..8. Font/metrik 100% dari disc (ascent=22 lineH=19).
- Papan-1 tak berubah (UI = kerja P3); gate T-RE-CAP1 tetap menunggu laporan human.

## P3-M3 VM penuh + replay golden — CLOSED (2026-11-12 larut)
- **`poc/app/src/main/cpp/scriptvm.h`** = VM 66-opcode (tabel dispatch 0x00-0x41; beyond = data, guard STEP_ERROR).
  Koreksi ISA diverifikasi bit-exact vs asm PPC interpreter `FUN_80011edc` (dtk build byte-identical):
  **0x3f = YIELD** (`stb 1,0x1c(ctx)`, pump re-entry pc+1 — BUKAN terminasi; gejala lama: loop abadi),
  **0x40 = SKIP4**, **0x3b/0x3d = JNZ/JZ** (label lama terbalik), CMP family push **bit1 PPC** (`0`=equal),
  rotl/clz helper, 0x41 = LOGF (retail stub, bukan CALLN). Hook `Vm::onNative` (observer, default null).
- **Harness `poc/tools/`**: `vmtest.cpp` (desktop replay: load .cmb raw + registry 406 native
  `docs/data/script_native_registry.csv` via `bin/scriptreg.mjs`, stub WaitM=yield-N, Fade=isFading-timer,
  UnitGetByPID=pid+0x1000, RNG LCG fixed-seed; `--entry/--frames/--call/VMHIST=1`),
  `build_vmtest.ps1` (LLVM-MinGW UCRT clang++, host-only; NDK tetap utk APK), `run_replay.ps1`.
- **GOLDEN 40/40** (`run_replay.ps1`, 60k frame): C00-C31 + C80-C82 + C91-C96 SEMUA `done=1 depth=0`,
  nol `vm ERR`/nol hang; rentang frame 1→56.9k; **C01 Opening0 selesai 4401f/96-native termasuk
  `[dlg] bmap01_mikata_c`** (rantai TalkEvent+Fade+UnitWalkDir+Comeback = alur prolog Ch1 jalan di harness);
  C06 (13.089f/214-nat, UnitEscapeByPID chain), C13 (31.820f, GMap pen-draw), C91-C96 interlude 1.8-9.3kf.
  Histogram native per bab = oracle semantik SCT berikutnya (top: WaitM dominan → timer benar).
- startup.cmb = pustaka fungsi bantu (rek pertama `DisposSetMode` helper; jalankan sebagai skenario => ERR by-design,
  entry nyata = fungsi yang dipanggil bab via name-call). `ending.cmb` entry non-Opening (follow-up P4).
- Test matrix: **T-SCT-VM + T-RE-SCRIPT1 → PASS-2026-11-12** (3/53). Catatan toolchain: git-bash gagal
  exec exe LLVM-MinGW di path WinGet (junction) — build/ jalankan via PowerShell.

## BRV-BTL-009 formula tempur + RNG — CLOSED (2026-11-12 dini hari, offline mining)
- **RNG LFSR FE9 CLOSED (BRV-RNG-010)**: state 3×u16 @0x8032F3B0; `cee4(n)=(n·u)>>16`,
  `ce68(p)` roll-1 `<p`, `cd88(p)` roll-RATA-DUA `<p (hit pakai rata2 = anti-untaian). Predikat
  `((p^v)>>1−(p&(p^v)))>>31` diverifikasi numerik node == (v<p) ∀p,v — bukan tebakan.
  Debug-flag `&0x4000` matikan RNG = hadiah harness deterministik.
- **Formula tempur penuh dari kode** (bukan rumus komunitas): ATK/DEF/HIT/AVO/CRT per-strike
  (rantai FUN_801a601c→801a8f80/da4/f14/b48/af4), **hit% = HIT−AVO; crit% = CRT−CRTA**, clamp
  [0,100], koreksi status 0x35(×2)/0x3f(/2,&crit×2)/tidur→100/anti-crit→0; hasil FUN_801a8a28:
  **0x50 normal / 0x51 CRIT(×3!) / 0x52 MISS**, forced via register DAT_802EFDB8; DMG=max(0,ATK−DEF)
  + modifier terrain/status (801a55d4); strike: brave-flag 0x20→2, **double = def.AS+4 ≤ atk.AS**,
  inisiatif float-AS; EXP rank(1..20)×slotEXP×(91..100%).
- Netuzo & battle animasi = RESOLVER SAMA (animasi baca event-log 0x55–0x66) — pertanyaan lama
  "netuzo selalu kena?" TERJAWAB: tidak; path hit-roll identik, cache lama +0x29E = display.
- AI: `ce68(FUN_8003cff0%)` = keputusan serang; keluarga 0x800FA0E8 = pemilih aksi (bukan damage).
- Impl engine: mirror numerik C++ + golden LFSR test wajib (seed capture saat T-CAP masuk).
  Test T-C1-CALC/T-SYS-EFF tetap Planned (golden case butuh capture) — BRV 54.

## BRV-MAP-002/004 map.bin + FE8Data register — CLOSED (2026-11-12, offline mining)
- **Parser universal loader `FUN_8003f87c`** (tool `bin/mapbin.mjs`): hdr [size|relTbl|
  ptrCnt|dirCnt] base+0x20, dir [dataOff|nameOff], pool nama, RELOC `val+0x20=offset file`
  (kalibrasi terukur 5/5 pointer→nama model).
- **map.bin bmap01 CLOSED**: capacity **16×14 grid logis** + 5 instance (bmap01, tree_01/02,
  water0, flower_01 + posisi f32); panelindex u16[16×14] → uniquepanel 86 rec (109 daratan,
  29 air, sisanya dekor per-tile). Grid engine lama 29×24 = overlay visual (bukan logis).
- **FE8Data.bin = 112 named-chunk CLOSED** (reloc 6686): 12 tabel data + 100 SID_*;
  **BattleTerrData hdr [u16 77][u16 42] = bonus tempur per terrain-id** (sumber BRV-BTL-008);
  TerrainData 77 entri (hash `FUN_8005b7a4` = packed-ASCII ≤4 char). POOL SJIS TERBACA:
  **MT_×52 material** (平地/移動不可/橋/玉座/宝箱…), **MW_×14 objective** (制圧/到達_15 ✓
  win-cond C01/全滅/離脱…), MTM_ musik, RID_ bg dialog — kosakata grammar world-loop.
- Impl P4-M1: movement-cost per tile dari material MT_ + BattleTerrData utk bonus; terrain-id
  presisi per-tile nunggu capture (blocked P3-M2.4) — Ch1 pakai pemetaan panel (109/29/…) dlu.
  BRV 55.
