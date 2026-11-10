# SCOREBOARD — snapshot kemajuan (2 papan TERPISAH, master plan §5.1)

> Kemajuan proyek yang SESUNGGUHNYA = **Functional parity**. Asset readiness hanya kesiapan
> bahan; Decoded ≠ sistem selesai. Update tiap commit yang mengubah matriks/test/BRV.

## Papan 1 — Asset readiness (`FE9_ASSET_MATRIX.csv`, 57 keluarga) — 2026-11-10 (post-P1 model)

| Status | Keluarga | Catatan |
|---|---|---|
| Verified | **0** | belum ada aset masuk engine |
| Decoded | 12 | +Script VM `.cmb`: 43/43 script, 2075/2075 fungsi ter-disassemble bersih, 65 opcode dari interpreter asli `FUN_80011edc`; sebelumnya `skeleton.g`+`body.gs`+pack `/ymu` pipeline (archer 3D render) |
| Partial | 14 | container/decoder jalan; isi/semantik belum (`.tpl`: CMPR benar ✓ tapi binding img↔tpl file belum fixed) |
| Investigating | 15 | bukti struktur; parser belum (audio bank, font `.gcf`, STM) |
| Discovered | 14 | hanya magic/eksistensi |
| Deferred | 2+ | THP/ending/JP-dup dst., dengan fase penerima |

## Papan 2 — Functional parity (`FE9_TEST_MATRIX.csv` + `FE9_BEHAVIOR_SPEC.md`) — 2026-11-10

| Metrik | Nilai |
|---|---|
| Test PASS / total | **1 / 53** — T-BASE-001 PASS 2026-11-10 (fresh clone → generator → diff normalisasi-EOL nol; `.gitattributes eol=lf` ditetapkan) |
| BRV terverifikasi | 16 entri (12 seed + **BRV-SCT-001..003 + BRV-EVT-002** — VM script Confirmed dari bytecode interpreter) — dari 11 domain, 3 domain masih `Unknown` penuh |
| Chapter parity | 0/30 |
| Milestone P1 | (1) **MODEL+ANIMASI TERBUKTI VISUAL** — archer 3D utuh (`out/render/archer_*.png`); (2) **SCRIPT VM TERBUKA** — bytecode FE9 terbaca penuh: `Opening0` C00 = MapLoad Map6→Fade(2,500); setup unit prolog PID_IKE/PID_WAYU+skill terbaca sebagai DATA (`bin/cmb.mjs dis`) |
| Fase aktif | **P1** |

## Gap tertinggi
1. `AM-009` binding img-index↔file `.tpl` pada pack unit + path blend-skinning `/zu` (butuh raw-pack entry di `cmp.mjs`) — P1 berikutnya (reviewer queue: audio/behavior PoC).
2. 404 native `.cmb` — signature/semantik per nama belum (pool names + argcount saja); sebagian butuh Ghidra xref per fungsi.
3. Domain BRV battle/AI/map/rng `Unknown` — butuh capture human (T-RE-CAP1).
