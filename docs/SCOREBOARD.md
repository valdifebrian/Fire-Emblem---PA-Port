# SCOREBOARD — snapshot kemajuan (2 papan TERPISAH, master plan §5.1)

> Kemajuan proyek yang SESUNGGUHNYA = **Functional parity**. Asset readiness hanya kesiapan
> bahan; Decoded ≠ sistem selesai. Update tiap commit yang mengubah matriks/test/BRV.

## Papan 1 — Asset readiness (`FE9_ASSET_MATRIX.csv`, 57 keluarga) — 2026-11-10 (post-P1 model)

| Status | Keluarga | Catatan |
|---|---|---|
| Verified | **0** | belum ada aset masuk engine |
| Decoded | 22 | +**sweep SEMUA .tpl/.cms disc (0 fail, 1566 img)**: `/zu` 539✓ (battle tex), `/ending` 102✓ (fin_0 608×448 CG vision), `/etc` 16✓, `/gmap` 7✓; +`/window` UI lengkap (965 img, icon atlas vision-ok); +`/illust` 174✓ (Geoffrey); C4-CMPR GX dikunci + binding `tex_<charIdx>` + potret berlapis (BRV-GFX-001); `.gcf`, STM, Script VM, model 3D |
| Partial | 11 | Bank SE `gcfesnd` struktur (BRV-AUD-003); window `.cmp` pack semantics; dsb. |
| Investigating | 8 | **`.samp` payload ≠ DSP standar (bukti LSQ+ZCR — P2 target)**; `.bin`/`.pool` tabel; dsb. |
| Discovered | 14 | hanya magic/eksistensi |
| Deferred | 2+ | THP/ending/JP-dup dst., dengan fase penerima |

## Papan 2 — Functional parity (`FE9_TEST_MATRIX.csv` + `FE9_BEHAVIOR_SPEC.md`) — 2026-11-10

| Metrik | Nilai |
|---|---|
| Test PASS / total | **1 / 53** — T-BASE-001 PASS 2026-11-10 (fresh clone → generator → diff normalisasi-EOL nol; `.gitattributes eol=lf` ditetapkan) |
| BRV terverifikasi | 20 entri (12 seed + SCT×3 + EVT-002 + AUD-002/003 + FNT-001 + **GFX-001** binding skin+potret berlapis) — 3 domain masih `Unknown` penuh |
| Chapter parity | 0/30 |
| Milestone P1 | (1) MODEL+ANIMASI visual ✓ archer; (2) SCRIPT VM ✓ (C00 Opening0 terbaca data); (3) **BGM DECODED ✓** — `bin/stm.mjs` → WAV 32kHz stereo (boss1 61.1s); (4) **FONT DECODED ✓** — `bin/gcf.mjs` → PGM atlas+CSV, fe/talk/alpha/biggana/system(.gcf+.cms) |
| Gate P1→P2 | **font `.gcf` ✅ Decoded (AM-024)** + BGM ✅; tinggal: PoC-inputs + T-RE-CAP1 (SE gcfesnd = opsional P1, target P2) |
| Fase aktif | **P1** |

## Gap tertinggi
1. Blend-skinning `/zu` (animasi battle blend) — AM-009 binding img↔tpl SUDAH ✓ (BRV-GFX-001); raw-pack `cmp.mjs` jalan (amr1_ja.pak 9/9).
2. 404 native `.cmb` — signature/semantik per nama belum (pool names + argcount saja); sebagian butuh Ghidra xref per fungsi.
3. Domain BRV battle/AI/map/rng `Unknown` — butuh capture human (T-RE-CAP1).
4. `.samp` payload SE = BUKAN DSP standar (BRV-AUD-003) — butuh trace RAM ucode AX via
   Dolphin (capture human) ATAU reverse encoding custom; target P2.
