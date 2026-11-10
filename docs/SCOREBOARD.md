# SCOREBOARD — snapshot kemajuan (2 papan TERPISAH, master plan §5.1)

> Kemajuan proyek yang SESUNGGUHNYA = **Functional parity**. Asset readiness hanya kesiapan
> bahan; Decoded ≠ sistem selesai. Update tiap commit yang mengubah matriks/test/BRV.

## Papan 1 — Asset readiness (`FE9_ASSET_MATRIX.csv`, 57 keluarga) — 2026-11-10 (post-P1 model)

| Status | Keluarga | Catatan |
|---|---|---|
| Verified | **0** | belum ada aset masuk engine |
| Decoded | 11 | +`skeleton.g`+`body.gs`+pack `/ymu .cmp` pipeline lengkap (parser+render terbukti visual: archer 3D, 26 bone, 590 tri, pose anim beda); contoh lain: `.ga` (+fix keyframe), `.m`, portrait `.cms`, CSV tabel, lookup `.dbx` |
| Partial | 14 | container/decoder jalan; isi/semantik belum (`.tpl`: CMPR benar ✓ tapi binding img↔tpl file belum fixed) |
| Investigating | 16 | bukti struktur; parser belum (script `.cmb`, audio bank, font `.gcf`, STM) |
| Discovered | 14 | hanya magic/eksistensi |
| Deferred | 2+ | THP/ending/JP-dup dst., dengan fase penerima |

## Papan 2 — Functional parity (`FE9_TEST_MATRIX.csv` + `FE9_BEHAVIOR_SPEC.md`) — 2026-11-10

| Metrik | Nilai |
|---|---|
| Test PASS / total | **1 / 53** — T-BASE-001 PASS 2026-11-10 (fresh clone → generator → diff normalisasi-EOL nol; `.gitattributes eol=lf` ditetapkan) |
| BRV terverifikasi | 12 entri seed (4 Confirmed-sebagian, 8 Partial/Strong) — dari 11 domain, 4 domain masih `Unknown` penuh |
| Chapter parity | 0/30 |
| Milestone P1 | **MODEL+ANIMASI TERBUKTI VISUAL** 2026-11-10: satu unit FE9 (archer) tampil 3D utuh — skeleton 26 bone, skinning palette→bone, animasi asli (`wait_N/wait_BW/move_N`) menghasilkan pose berbeda (`bin/meshview.mjs` → `out/render/archer_*.png`); flat/lit mode membuktikan rasterizer+normal bersih |
| Fase aktif | **P1** |

## Gap tertinggi
1. `AM-017` Script VM `.cmb` (risiko kredial) — P1 berikutnya (reviewer order: model→anim→**script**).
2. `AM-009` binding img-index↔file `.tpl` pada pack unit (body atlas mana) + path blend-skinning `/zu` (butuh raw-pack entry di `cmp.mjs`).
3. Domain BRV battle/AI/map/rng `Unknown` — butuh capture human (T-RE-CAP1).
