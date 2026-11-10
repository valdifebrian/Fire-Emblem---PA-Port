# SCOREBOARD — snapshot kemajuan (2 papan TERPISAH, master plan §5.1)

> Kemajuan proyek yang SESUNGGUHNYA = **Functional parity**. Asset readiness hanya kesiapan
> bahan; Decoded ≠ sistem selesai. Update tiap commit yang mengubah matriks/test/BRV.

## Papan 1 — Asset readiness (`FE9_ASSET_MATRIX.csv`, 57 keluarga) — 2026-11-10

| Status | Keluarga | Catatan |
|---|---|---|
| Verified | **0** | belum ada aset masuk engine |
| Decoded | 8 (+1 Decoded(container)) | format+decoder+validasi mandiri; contoh: `.ga`, `.m`, portrait `.cms`, CSV tabel, lookup `.dbx` |
| Partial | 14 | container/decoder jalan; isi/semantik belum |
| Investigating | 18 | bukti struktur; parser belum (`.g`/`.gs`, script `.cmb`, audio bank, font `.gcf`, STM) |
| Discovered | 14 | hanya magic/eksistensi |
| Deferred | 2+ | THP/ending/JP-dup dst., dengan fase penerima |

## Papan 2 — Functional parity (`FE9_TEST_MATRIX.csv` + `FE9_BEHAVIOR_SPEC.md`) — 2026-11-10

| Metrik | Nilai |
|---|---|
| Test PASS / total | **0 / 53** (semua Planned; gate berikutnya T-BASE-001) |
| BRV terverifikasi | 12 entri seed (4 Confirmed-sebagian, 8 Partial/Strong) — dari 11 domain, 4 domain masih `Unknown` penuh |
| Chapter parity | 0/30 |
| Fase aktif | **P0** (baseline+validasi; D1–D5 disetujui) |

## Gap tertinggi
1. `AM-017` Script VM (risiko kredial) — P1 #1.
2. Model: parser `skeleton.g`+`body.gs` (pack unit sudah terbukti isinya) — P1 #2.
3. Domain BRV battle/AI/map/rng `Unknown` — butuh capture human (T-RE-CAP1).
