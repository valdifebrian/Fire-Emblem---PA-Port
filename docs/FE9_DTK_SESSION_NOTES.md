# Catatan sesi 2026-11-11: diagnosis NKit-relaxed + infrastruktur matching-decomp (dtk)

## TL;DR
1. Disc container (RVZ) = **repack NKit-relaxed**, bukan layout Redump standar. Semua deviasi yang didokumentasikan di `fe9-re-plan.md` §10 (DOL @0x20300, header 0x420–0x42C, FST flag non-standar) = **konvensi format NKit Relaxed**, bukan perilaku disc asli. Isi file TETAP utuh — diverifikasi silang dua toolchain independen (§3).
2. Infrastruktur **matching-decomp** (encounter/dtk + dtk-template) sudah berdiri dan teruji byte-identical; analisis awal mendeteksi **9.251 fungsi**. Ada di `decomp/` (commit ini).

## 1. Rantai bukti NKit-relaxed

| # | Bukti | Detail |
|---|---|---|
| 1 | Scan record internal | Image memuat `nkit.xml` `<ScanRecord>` dengan `JunkID="GFEE"` — identitas NKit 3.x scrubber; format scan-record hanya ditulis NKit |
| 2 | Relokasi DOL | `main.dol` @`0x20300` (bukan 0x2430), apploader region @0x20298, data dirampat rapat ke awal + header disc stub `01 01 01` @0x30 — persis pola NKit "Relaxed" |
| 3 | Junk identik | 0x42C duplikat FST-size dan fill 16-byte berulang = pola junk-replacement NKit (bukan bytes original Redump) |
| 4 | NKit Fix DB miss | NKit punya fix-file untuk `GFEP01` (PAL) tapi TIDAK ada `GFEE01` → ekspansi byte-perfect ke layout Redump tidak mungkin dilakukan lokal |
| 5 | FST | Flag dir bit `0x01000000` + field size = end-index subtree (absolut, exclusive) + string-table di akhir = skema FST Relaxed. Parser FST standar (bit31) menghasilkan 0 file di image ini; parser JS-mu dan parser NKit 3.2 keduanya sukses — konsisten |

**Konsekuensi:** verifikasi final terhadap hash ISO Redump butuh ISO original. Untuk analisis, integritas sudah aman: file hasil ekstraksi NKit ter-CRC-match vs record `Full` di scan record; DOL diverifikasi silang (§3).

Klarifikasi klaim "Disc TIDAK scrubbed/patched" (plan §10): region junk/padding memang di-replace NKit (bukti #3), tapi **isi file game tidak tersentuh** — jadi klaim itu benar utk konten, salah utk byte container di luar file-area.

## 2. Layout internal `main.dol` = standar

Header DOL itu sendiri **standar penuh** (per `decomp-toolkit/src/util/dol.rs` = ground truth parser: 7 text + 11 data, entry @0xE0):

- entry `0x80003154` = `__start`; BSS `0x802A51C0` size `0xC7374`
- 2 section text + 8 section data aktif (10 section; segmen akhir s/d MEM2 `0x8036C400` — MEM2 confirmed)
- 90 file bytes di antara header dan teks pertama = slack standard NKit; bukan korup

Yang menyimpang dari Redump HANYA penempatan file di disc, bukan isi DOL. (Catatan jujur: parser DOL draft saya di sesi ini sempat memakai 16 slot data — bug sendiri; format benar 7+11.)

## 3. Verifikasi silang dua toolchain

| Artefak | Toolchain kamu (JS RVZ reader) | Toolchain sesi ini (NKit 3.2 extract) | Hasil |
|---|---|---|---|
| `main.dol` md5 | `74da02bee130b88bedc0ade4f20dda98` | `74da02bee130b88bedc0ade4f20dda98` | **identik** |
| `main.dol` sha1 | — | `9d00d7d407750b82cb1cfccc56f8aa8a89cc8851` | tercatat di `decomp/config/GFEE01/` |
| Entri FST | 4077 file + 545 dir (4623 entri) | 4082 file | konsisten (+5 = selisih klasifikasi dir/file antar parser) |

## 4. Infrastruktur `decomp/` (lane matching-decomp)

- Template: [`encounter/dtk-template`](https://github.com/encounter/dtk-template) (CC0) — scaffold resmi project decomp GC/Wii.
- Tooling: **dtk 1.8.4** (`decomp/tools/dtk.exe`, tidak di-commit — unduh dari release `encounter/decomp-toolkit`), **ninja 1.13.2** (pip), Python 3.12. `configure.py` sudah diisi `VERSIONS["GFEE01"] = "1.16"` (placeholder sampai toolchain CodeWarrior terkonfirmasi; tidak dipakai langkah analisis).
- Build graph terbukti jalan penuh: analyze → split → link → DOL → **`CHECK config/GFEE01/build.sha1: OK`** = split→relink **byte-identical** dengan aslinya.
- Output analisis awal (dtk CFA): **9.251 fungsi** / 2.500.828 B code / 1.067.700 B data / 98 unit awal / **17.554 symbols** — termasuk runtime CodeWarrior ternama (`__start`, `__init_registers`, `__init_data`, `__init_hardware`, `__flush_cache`, `gTRKInterruptVectorTable`) → runtime = CW for GameCube standar; versi pasti via `dtk version` nanti.
- vs Ghidra (M2 kamu, 8.125 fungsi): dtk CFA menemukan ~1.126 boundary lebih banyak (Ghidra merge sebagian). Dual-source: Ghidra utk pseudocode cepat & string-xref, objdiff+dtk utk ground truth byte-per-fungsi.
- Loop kerja standar: `ninja` (analyze iteratif memecah unit kasar → TU per file + split `.s`) → objdiff/decomp.me per fungsi → rename di `symbols.txt` → match → komit. Toolchain CodeWarrior (mwcceppc GC) baru dibutuhkan saat compile C pertama; skrip CI template mengunduh otomatis.

## 5. Fit dengan roadmap (master plan & scoreboard)

### Temuan pencarian perluasan "satu mesin" (2026-11-11)

- **`StanHash/FE9-DOC`** (FEU: @nat5/StanH, thread 6143) = vault pengetahuan FE9: format event-script
  CMB penuh (VM stack-based, tabel opcode 0x00–0x47), interpreter `0x80011EDC`, loader CMB
  `0x80013E24`, dispatcher `0x80011D20`, + notes Procs/Units/Dispos/BigData/C++ vtable.
- **Tabel fungsi event FE9 (406 entri: alamat+argc+nama)** → di seed ke `docs/FE9_EVENT_FUNCTIONS.csv`.
  Cross-check: 351/406 alamat cocok dengan boundary fungsi hasil deteksi dtk; 4/4 sampel
  (`Focus`, interpreter, loader, dispatcher) tepat sama ukurannya. Artinya: naming event-layer
  bisa langsung dari tabel ini tanpa decompile satu-satu.
- FE10: format script IDENTIK (hanya +opcode 42–47 & call.loc 2-byte); FE10GameData tools
  (FEU 8117) & blog Evan Zhang = referensi struktur tabel FE10 yang sama dgn FE9 (`FE10Data.cms`).
- Tellius-forge `research/`: format anim/skeleton dua-game (FE9+FE10) — sudah dipakai plan M4.

- Gap tertinggi #2 (404 native `.cmb`, signature per fungsi butuh xref): `decomp/` memberi split per fungsi + xref data→code otomatis; `functions.tsv` hasil Ghidra bisa di-seed ke `symbols.txt`.
- Gap #3 (battle/AI/map `Unknown`): matching decomp = bukti semantik terkuat setelah capture human; mulai dari entri BRV-BAT (battle calc) prioritas-1 di BEHAVIOR_SPEC — pilih fungsinya lewat xref Ghidra (`strings.tsv`/`functions.tsv`) lalu kunci dengan match byte-per-fungsi.
- Lane ini TIDAK mengubah frontend JS (rvz/cmp/tpl/ga/stm/gcf tetap jalur aset/data); dia menambah lane "logika" yang rigorous dan bisa menghasilkan C compilable-identik yang langsung terpakai utk port.

## 6. Setup dari clone

```
pip install ninja
# unduh dtk release → decomp/tools/dtk.exe
# taruh main.dol hasil ekstraksi sendiri → decomp/orig/GFEE01/main.dol (sha1 §3)
cd decomp && python configure.py --dtk tools/dtk.exe && ninja
```

Konten disc (`orig/`, `build/`, `tools/`, `*.dol`) di-gitignore. `decomp/` di repo ini = scaffold + config + symbols/splits hasil analisis (metadata, tanpa konten disc). Scaffold diturunkan dari dtk-template (CC0).
