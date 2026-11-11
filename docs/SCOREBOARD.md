# SCOREBOARD — snapshot kemajuan (2 papan TERPISAH, master plan §5.1)

> Kemajuan proyek yang SESUNGGUHNYA = **Functional parity**. Asset readiness hanya kesiapan
> bahan; Decoded ≠ sistem selesai. Update tiap commit yang mengubah matriks/test/BRV.

## Papan 1 — Asset readiness (`FE9_ASSET_MATRIX.csv`, 57 keluarga) — 2026-11-10 (post-P1 model)

| Status | Keluarga | Catatan |
|---|---|---|
| Verified | **0** | belum ada aset masuk engine |
| Decoded | 26 | +**staffroll KREDIT TERBACA**; +`Face|.bin`≡facedata CLOSED; **+NATIVE CATALOG 390/390** (BRV-SCT-004); +facedata 358×3-slot; +`/s` 198/198; sweep `.tpl/.cms` 0-fail; C4+binding+potret; `.gcf`, STM, VM, 3D |
| Partial | 18 | **keluarga tabel rekuren `[size/tableOff/cnt/sub]`** terpetakan di rect×3/RectBases/shopitem×3/staffroll/RectFinale/tut_data/soundroom; +`dispos_c` AI-stream; SE bank; dsb. |
| Investigating | **1** | **HANYA `.samp` payload** (≠ DSP standar, bukti LSQ+ZCR — butuh trace ucode AX, P2) |
| Discovered | 10 | hanya magic/eksistensi |
| Deferred | 2+ | THP/ending/JP-dup dst., dengan fase penerima |

## Papan 2 — Functional parity (`FE9_TEST_MATRIX.csv` + `FE9_BEHAVIOR_SPEC.md`) — 2026-11-10

| Metrik | Nilai |
|---|---|
| Test PASS / total | **1 / 53** — T-BASE-001 PASS 2026-11-10 (fresh clone → generator → diff normalisasi-EOL nol; `.gitattributes eol=lf` ditetapkan) |
| BRV terverifikasi | 34 entri (12 seed + SCT×8 (incl **SCT-004 native catalog 390/390**, SCT-005 protokol wait/arg/antrean, SCT-006 semantik native batch-2, **SCT-007 dispos_c payload 100×0x6C CLOSED**, **SCT-008 dispos per-peta: pack c/h/m/n + grid 108/112 + Ch1 4 unit IKE/MIST/BOLE/GREIL**) + **BTL-002 tabel inti zdbx: timing tempur+kapak-bumerang dkk+nama→ID** + **BTL-003 video harvest Ch1-2: forecast 6-field ATK/Hit/Ava/Crt/Dmg/Recv, triangle-halving TERUKUR 10v1, weapon-0 bare-hand, Y=End, radial wheel, rescue hand, MIA death** + **BTL-004 SUPERSEDED (label "Boyd" = misread 640p; koreksi: Shove ✓, pair-format ✓)** + **BTL-005 SUMBER HD 720p: forecast Mt/Hit/Crit terbaca penuh, Mt=kerusakan final DECIDED (Ike 7→Δ7, Bandit 6→Δ6), cap Hit 100, Ddg=DEF, panah triangle, badge ×2 (trigger UNKNOWN), inv Bandit 3×Iron Sword, timeline 30 chapter HD** + **BTL-006 HD batch-2: Titania vs Volke (crit40 OPEN, ×2 rasio-HP kandidat), LVLUP screen HP+1↑ 8-stat + ground truth Titania Paladin Lv2, inventaris 4-slot E-badge+grey-out, objective ke-3 `Clear Escape`, Thicket Def1, radial wheel penuh** + **SYS-002 menu OPSI 11-setting (Animation Individual, Combat Camera, Game/Message Speed, Grid Thickness slider, Terrain/Goal, Unit/Combat Window, Message Display, Grid/Cursor Color palet)** + CAP-001 input-channel+peta-800ms + **INP-002 arsitektur input Android+jebakan NativeActivity** + EVT-002 + AUD-002/003 + FNT-001 + GFX-001 + ANM-004) — 3 domain masih `Unknown` penuh |
| Chapter parity | 0/30 |
| Milestone P1 | (1) MODEL+ANIMASI visual ✓ archer; (2) SCRIPT VM ✓ (C00 Opening0 terbaca data); (3) **BGM DECODED ✓** — `bin/stm.mjs` → WAV 32kHz stereo (boss1 61.1s); (4) **FONT DECODED ✓** — `bin/gcf.mjs` → PGM atlas+CSV, fe/talk/alpha/biggana/system(.gcf+.cms); (5) **TEXTURE+SKIN ✓** — C4 fixed, binding `tex_<idx>` + layered face + bone-skinning (AM-009/047 CLOSED, BRV-GFX-001/ANM-004) |
| Milestone P2 (PoC engine) | **2026-11-11: INTERACTIVE DI PERANGKAT** — `poc/` (C++20+GLES3+Oboe+JNI, Activity biasa): fpk loader, .g/.gs/.ga + grid 10m + BGM 2-stream + touch overlay; **camera orbit+lookAt benar (bug eye-translation = "quad raksasa" CLOSED)**, **input sentuh live** (`taps2s=1, yaw 35→112, z 0→3` — swipe panah: held-state, 4-arah + putar; rapid-fire capable karena state, bukan event). **Pitfall Android-15: NativeActivity+SurfaceView = `touchableRegion=<empty>` = input hilang senyap → cutover Activity biasa + OnTouchListener (BRV-INP-002)**. **2026-11-12: GRID MOVEMENT + TAP-TO-MOVE + TRUE FULLSCREEN** — state unit (gx,gy FIFO-path cap22), dpad→sumbu-dunia-terbaik di yaw iso-45 tetap, tap raycast pitched-basis→ubinan, follow-cam focus-lerp + cursor ubinan; **edge-to-edge immersive-sticky + cutout shortEdges → view = full 2340×1080, mapping tap diverifikasi presisi (R@0.25w)**, viewport live via eglQuerySurface + cache tombol ikut resize. Device: R×2 4,4→6,4; tap→tile 5,4 walk-back; screenshot fullscreen bersih ✓ |
| Gate P1→P2 | **font `.gcf` ✅ Decoded (AM-024)** + BGM ✅; PoC **jalan**; tinggal: T-RE-CAP1 (SE gcfesnd = opsional P1, target P2) |
| Fase aktif | **P1 sisa + P2 PoC berjalan paralel** |

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
