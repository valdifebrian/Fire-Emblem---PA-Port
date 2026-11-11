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
| BRV terverifikasi | 26 entri (12 seed + SCT×7 (incl **SCT-004 native catalog 390/390**, SCT-005 protokol wait/arg/antrean, SCT-006 semantik native batch-2, **SCT-007 dispos_c registry always_* + loader universal .bin**) + CAP-001 input-channel+peta-800ms + EVT-002 + AUD-002/003 + FNT-001 + GFX-001 + ANM-004) — 3 domain masih `Unknown` penuh |
| Chapter parity | 0/30 |
| Milestone P1 | (1) MODEL+ANIMASI visual ✓ archer; (2) SCRIPT VM ✓ (C00 Opening0 terbaca data); (3) **BGM DECODED ✓** — `bin/stm.mjs` → WAV 32kHz stereo (boss1 61.1s); (4) **FONT DECODED ✓** — `bin/gcf.mjs` → PGM atlas+CSV, fe/talk/alpha/biggana/system(.gcf+.cms); (5) **TEXTURE+SKIN ✓** — C4 fixed, binding `tex_<idx>` + layered face + bone-skinning (AM-009/047 CLOSED, BRV-GFX-001/ANM-004) |
| Gate P1→P2 | **font `.gcf` ✅ Decoded (AM-024)** + BGM ✅; tinggal: PoC-inputs + T-RE-CAP1 (SE gcfesnd = opsional P1, target P2) |
| Fase aktif | **P1** |

## Gap tertinggi
1. ~~Blend-skinning~~ **CLOSED 2026-11-10**: bit0=bone-index (pipeline g/ga membuktikan posing; BRV-ANM-004); bit1=palette multi-slot = P2-detail.
2. 404 native `.cmb` — signature/semantik per nama belum (pool names + argcount saja); sebagian butuh Ghidra xref per fungsi.
3. Domain BRV battle/AI/map/rng `Unknown` — butuh capture human (T-RE-CAP1).
4. `.samp` payload SE = BUKAN DSP standar (BRV-AUD-003) — butuh trace RAM ucode AX via
   Dolphin (capture human) ATAU reverse encoding custom; target P2.
