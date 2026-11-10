# SCOREBOARD — snapshot kemajuan (2 papan TERPISAH, master plan §5.1)

> Kemajuan proyek yang SESUNGGUHNYA = **Functional parity**. Asset readiness hanya kesiapan
> bahan; Decoded ≠ sistem selesai. Update tiap commit yang mengubah matriks/test/BRV.

## Papan 1 — Asset readiness (`FE9_ASSET_MATRIX.csv`, 57 keluarga) — 2026-11-10 (post-P1 model)

| Status | Keluarga | Catatan |
|---|---|---|
| Verified | **0** | belum ada aset masuk engine |
| Decoded | 14 | +**Font `.gcf`**: metrics 16B + sheet GX I4 8×8-block (6/6 file render glif terbaca — BRV-FNT-001); sebelumnya **STM BGM `.stm`**: GC-DSP predictor 8B/14smp koef-di-header (vgmstream DSP_STM), 3 file terbukti rough+spektrum+loop; Script VM `.cmb` (2075/2075 fungsi, 65 opcode `FUN_80011edc`) + pipeline model/archer 3D |
| Partial | 14 | container/decoder jalan; isi/semantik belum (`.tpl`: CMPR benar ✓ tapi binding img↔tpl file belum fixed) |
| Investigating | 13 | bukti struktur; parser belum (audio bank `gcfesnd.*`) |
| Discovered | 14 | hanya magic/eksistensi |
| Deferred | 2+ | THP/ending/JP-dup dst., dengan fase penerima |

## Papan 2 — Functional parity (`FE9_TEST_MATRIX.csv` + `FE9_BEHAVIOR_SPEC.md`) — 2026-11-10

| Metrik | Nilai |
|---|---|
| Test PASS / total | **1 / 53** — T-BASE-001 PASS 2026-11-10 (fresh clone → generator → diff normalisasi-EOL nol; `.gitattributes eol=lf` ditetapkan) |
| BRV terverifikasi | 18 entri (12 seed + BRV-SCT-001..003 + BRV-EVT-002 + BRV-AUD-002 STM + **BRV-FNT-001** GCF Confirmed) — dari 11 domain, 3 domain masih `Unknown` penuh |
| Chapter parity | 0/30 |
| Milestone P1 | (1) MODEL+ANIMASI visual ✓ archer; (2) SCRIPT VM ✓ (C00 Opening0 terbaca data); (3) **BGM DECODED ✓** — `bin/stm.mjs` → WAV 32kHz stereo (boss1 61.1s); (4) **FONT DECODED ✓** — `bin/gcf.mjs` → PGM atlas+CSV, fe/talk/alpha/biggana/system(.gcf+.cms) |
| Gate P1→P2 | **font `.gcf` ✅ Decoded (AM-024)** + BGM ✅; tinggal: PoC-inputs + T-RE-CAP1 (SE gcfesnd = opsional P1, target P2) |
| Fase aktif | **P1** |

## Gap tertinggi
1. `AM-009` binding img-index↔file `.tpl` pada pack unit + path blend-skinning `/zu` (raw-pack entry `cmp.mjs` SUDAH jalan: amr1_ja.pak 9/9).
2. 404 native `.cmb` — signature/semantik per nama belum (pool names + argcount saja); sebagian butuh Ghidra xref per fungsi.
3. Domain BRV battle/AI/map/rng `Unknown` — butuh capture human (T-RE-CAP1).
