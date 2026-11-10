# RE Plan — Fire Emblem: Path of Radiance (GFEE01, NTSC-U, Disc 1)

Status: DRAFT untuk review. Tidak ada kode implementasi (di luar draft scratch `fe9-rvz/rvz.mjs`) yang ditulis sebelum plan disetujui.

## 0. Tujuan & non-goal

**Tujuan:** pipeline reverse-engineering lokal yang reproducible atas disc ini:
1. Baca kontainer RVZ → ISO virtual (random access) tanpa tool eksternal.
2. Enumerasi filesystem (FST) + ekstraksi selektif yang terverifikasi.
3. Analisis `main.dol` (PPC Gekko) → peta kode/struktur.
4. Decode lapisan data game (pack `.cmp`, teks `.m`, tabel `system.cmp`, aset) sampai level dokumen format.

**Non-goal (sesuai keputusan proyek):** bukan bikin mod/rebalance; bukan distribusi konten berhak cipta; runtime verification (boot di emulator/hardware) dilakukan manusia, bukan agent.

## 1. Fakta terverifikasi hari ini

| Item | Nilai | Sumber |
|---|---|---|
| File | `Fire Emblem - Path of Radiance (USA).rvz`, 1,109,615,104 B | disk |
| Magic | `RVZ\x01` v1.0, compat 0x30000 | hexdump |
| ISO virtual | 1,459,978,240 B (miniDVD 1.36 GiB) | file head |
| Disc struct | type=1 (GC), compression=5 (**zstd**, level 19), chunk=**0x20000** (128 KiB) | hexdump |
| Tabel | raw=1 entry (whole-disc), partitions=0 (GC), groups=**11,139** (compressed 0x10218) | hexdump |
| Integritas | SHA-1 `disc struct` ✓, SHA-1 `file head` ✓. **KOREKSI AUDIT:** spec v1 TIDAK mendefinisikan SHA-1 utk raw/group table (hanya `part_hash` yg tak dipakai GC) — draft `rvz.mjs` sebelumnya membaca slot hash yang tidak ada → harus dibuang | spec WiaAndRvz |
| Game code | `GFEE01` = FE9 USA; disc-number byte 0x06=0 → **disc 1 ✓**. Klaim "disc 2 = GFEQ01" [UNVERIFIED, dari memori] — jangan dikutip sebagai fakta | disc header |
| Runtime | Bun 1.4.4 (native `zstdDecompress`, sha1/sha256), node 22+ tersedia; **Python TIDAK terpasang** | cek |
| REA | v6.8.0 terpasang; `rea inspect --target-format gamecube-dol` tersedia | `rea --help` |
| Ghidra | **TIDAK terpasang** (`GHIDRA_INSTALL_DIR` kosong) — prasyarat M2 | cek |
| Java | **JDK 21.0.13 LTS (Temurin) TERPASANG** — syarat runtime Ghidra sudah terpenuhi | `java -version` |

## 2. Referensi otoritatif (bibliografi kerja)

**Kontainer RVZ**
- [Dolphin `WiaAndRvz.md`](https://raw.githubusercontent.com/dolphin-emu/dolphin/master/docs/WiaAndRvz.md) — spec WIA/RVZ kanonik: struct, exception list, **RVZ packing + PRNG padding lengkap dengan kode** (sudah difetch; padding PRNG Lagged-Fibonacci xor j=32 k=521 wajib diimplementasi, karena disc GC penuh padding pseudorandom).
- [RVZSharp docs/format/rvz.md](https://github.com/purelogiccode/RVZSharp/blob/master/docs/format/rvz.md) — ringkasan layout terverifikasi implementasi (sudah difetch).

**Format disc GC**
- [YAGCD ch.14](https://hitmen.c02.at/files/yagcd/yagcd/chap14.html) — BNR, DOL (header 256 B: 7 text + 11 data, entry @0xE0) (sudah difetch).
- [Custom Mario Kart: DOL format](https://old.tockdom.com/wiki/DOL_(File_Format)) & [Retro Modding Wiki: DOL](https://www.metroid2002.com/retromodding/wiki/DOL_(File_Format)) — detail slot/BSS.
- [Retro Modding Wiki: REL format](https://www.metroid2002.com/retromodding/wiki/REL_(File_Format)) — bila FST ternyata memuat REL (dicek M1).
- [Console Research Wiki: GameCube](https://wiki.raregamingdump.ca/index.php?title=GameCube) — FST/DI/AXART latar belakang.

**Analisis PPC Gekko**
- [REA docs ghidra-dol.md](https://github.com/morluto/rea/docs/ghidra-dol.md) (`morluto/rea`) — lane import DOL via Ghidra: format `gamecube-dol`, provider **Ghidra saja**, window `0x80000000..0x81800000`, default language `PowerPC:BE:32:default`, opsi `REA_GHIDRA_LANGUAGE_ID=PowerPC:BE:32:Gekko_Broadway` (butuh extension GameCubeLoader), REL **tidak didukung** (sudah difetch penuh).

**FE9 spesifik**
- [[FE9] File structure notes (Jespoke, FEU)](https://feuniverse.us/t/fe9-file-structure-notes/5020) — peta folder (`zmap/xcam/Mess/Fonts/illust/Movie/gmap/Face/etc/s/Scripts/shop/Sound/xwp/yme/ymu/zbg/zu`), format `.cmp` ("pack" header), `.m` teks, `.dbx` (plaintext Shift-JIS), `.cmb`, `.tlp`, `.gcf`, struktur entri karakter/kelas/item `system.cmp` + `dispos.cmp` (0x54/0x64/0x6C-byte entries) (sudah difetch penuh).
- [tellius-forge](https://github.com/ltra043/tellius-forge) — riset + tool `.gs/.g/.ga` (model/skeleton/anim overworld `ymu/model*/pack.cmp`, `zmap/map*/map.cmp`), folder `research/{animation,body,skeleton}` = dokumen format; tool pack/unpack `.cmp` python di `tools/` = ground-truth codec (sudah difetch README + listing research/).
- [Zheneq/Noesis-Plugins](https://github.com/Zheneq/Noesis-Plugins) — cikal bakal importer FE9/FE10.
- [Universal FE Randomizer — FE9Data.java](https://github.com/lushen124/Universal-FE-Randomizer/blob/master/Universal%20FE%20Randomizer/src/fedata/gcnwii/fe9/FE9Data.java) — tabel ID known-good untuk cross-check.
- Lumina (thane98), BrawlCrate — referensi BRRES/BRLYT bila `.tlp` = BRLYT (dicek magic di M4).

## 3. Keputusan tooling

| Kebutuhan | Pilihan | Alasan |
|---|---|---|
| Container RVZ + FS | **JS/Bun** (satu-satunya runtime lengkap: zstd+crypto native) | tanpa install; Python tidak ada |
| Disasm/decomp PPC | **REA + Ghidra `gamecube-dol`** | jalur resmi; fallback: install rizin (`--target-format raw-binary` REA tidak dukung PPC) |
| Codec `.cmp`/.gs/etc | port dari python tellius-forge → JS (kecil, deterministik) | jaga satu runtime; verifikasi vs output tool asli |
| Verifikasi visual aset | Blender + tellius-forge (opsional, user-side) | sudah supported upstream |

## 4. Fase

### M0 — Reader RVZ final (JS) — **probes dulu, implementasi belakangan**
**M0-probe (langsung, murah, jawab ambiguitas spec sebelum nulis kode):**
1. Layout exception-list: grup pertama → cek di mana magic zstd `28 B5 2F FD` (offset 0 = nol list [interpretasi WIA utk raw-data]; offset 2 dengan `00 00` di depan = satu list kosong [interpretasi RVZSharp `max(1,...)`]). WIA vs RVZSharp **kontradiktif** untuk raw-data GC; zstd frame-checksum = oracle otomatis.
2. `rvz_packed_size`: hitung berapa grup packed ≠ 0 → tentukan apakah jalur PRNG dibutuhkan ATAU self-check padding mustahil (maka pakai bukti tak langsung: FST+DOL+BNR valid).
3. Dekode 1 grup packed manual: verifikasi parser packing + PRNG verbatim spec (catatan ganjil: output word `>>24, >>18 // NB: 18, bukan 16` — implement literal; byte-exact gagal → coba `>>16`, laporkan temuan).
4. Validasi raw-entry pertama vs aturan wit (offset dibulatkan turun ke 0x8000-an; size 0x80→0).
5. Probe `Bun.zstdDecompressSync` pada frame asli (frame-content-checksum xxhash): gagal → fallback wuzstd (pure JS, no-deps).
**Implementasi** (hanya setelah probe hijau): decode grup final (zero-group, flag uncompressed+padding-align, packing/PRNG bila packed>0), buang pemeriksaan hash tabel fiktif dari draft, LRU cache, CLI `info | ls | cat | extract | padcheck`.
Verifikasi: apploader region == `dhead`; jumlah grup ter-decode = 11,139; ≥ 100 grup random round-trip tanpa error frame zstd.

### M1 — Inventaris ISO
- **Rewrite bersih parser FST** (draft lama punya sisa kode kacau; aturan kanonik: entry 12 B BE, `0x80000000` = file, dir size = jumlah entry incl. diri sendiri, nama = C-string rel. string-table setelah akhir tabel entry).
- Dump disc header **penuh 0x430** dan dokumentasikan byte maker/disc/maks-disc (byte 0x08=0x01 belum diinterpretasi — jangan menebak).
- Parse apploader (entry point utk verifikasi silang M2), ekstrak: `main.dol`, `opening.bnr` (cek magic BNR1/2 + teks gamename), sampel 1 file per ekstensi unik.
- Sanity: semua entri in-bounds, FST end ≤ first file offset, total byte terpetakan ≤ ISO.
Output: `inventory/` + ringkasan jumlah file/ekstensi/folder vs peta folder Jespoke (deviasi = temuan).

### M2 — Analisis main.dol (prasyarat: Ghidra + JDK 21+)
- Install: **JDK 21 sudah ada ✓** — tinggal Ghidra 11.x/12 (download resmi; ±600 MB; user approve) — opsional GameCubeLoader extension untuk Gekko/Broadway (URL release belum diverifikasi).
- **Pra-validasi offline:** parse 256 B header DOL sendiri (≤18 slot, rentang file monotonic, entry ∈ segmen text, cek apakah ada slot di luar `0x80000000..0x81800000` — bila YA, REA lane bisa menolak; fallback: rizin raw-PPC atau normalisasi header).
- `rea inspect out/exec/main.dol --target-format gamecube-dol --provider ghidra --json`; lanjut MCP `open_binary {format:"gamecube-dol", provider_id:"ghidra"}`.
- Deliverable: `docs/code-map.md` — entry `__start`, segmen text/data/BSS, fungsi terbesar, tabel vtable/RTTI-ish, string region; seed nama dari `FE9Data.java` + string debug dalam DOL.
Verifikasi: entry == header; decompile salah satu fungsi yang string-nya muncul di game (user cek).

### M3 — Lapisan data game
- Decoder `.cmp` (LZ77 variant dari tool tellius-forge) + parser header "pack" → unpack `system.cmp`, 1 sampel `dispos.cmp`, `Mess/*.m`.
- Dump tabel karakter/kelas/item (`system.cmp`) → CSV; cross-check 10 record vs `FE9Data.java`.
- `.m`: tentukan encoding (UTF-16BE/Shift-JIS/ASCII) empiris → `docs/text-format.md`.
Verifikasi: jumlah/nama file dalam pack konsisten; ID tabel match known-good.

### M4 — Aset
- Magic-survey semua sampel: `.tlp`(→BRLYT/CMPR?), `.cms`, `.gcf`, `.dbx`(plaintext?), `.cmb`, `.thp`, `.pak`, `.ga/.g/.gs`.
- 1 render-path per format: tekstur → PNG (gctex/BRLYT reader JS atau tool existing), model `.gs` → obj (port dari tellius-forge), `.thp` → playable via ffmpeg (cek demuxer tersedia).
Verifikasi: PNG/obj/vid bisa dibuka manusia (bukan sekadar header parse).

### M5 — Dokumentasi & reproduksi
- `docs/formats/*.md` (hasil M0–M4, semua temuan terverifikasi + yang masih open), `inventory/`, tool `bin/` CLI rapi.
- Repro-check dari nol: hapus `out/`, jalankan ulang satu command, byte-identik.

### M6 — Runtime verification (user-side, opsional)
- Install Dolphin, boot GFEE01 + Gecko capture untuk memvalidasi hipotesis tabel (mis. breakpoint baca `system.cmp`). Diserahkan sebagai langkah terdokumentasi, dijalankan manusia.

## 5. Workspace

```
fe9-re/
  bin/         rvz.mjs fst.mjs cmp.mjs ...   (CLI JS)
  inventory/   files.csv, magic-survey.csv
  out/disc/    mirror FST (hasil extract)
  out/exec/    main.dol + artifacts REA
  out/assets/  sampel per format
  docs/        code-map.md, text-format.md, formats/*, notes.md
```

## 6. Risiko & mitigasi

| Risiko | Mitigasi |
|---|---|
| `.cmp` bukan LZ77 standar (variant IS) | **verifikasi dulu** tool pack/unpack benar-benar ada di tree `tools/` tellius-forge (belum dicek) → port, lalu diff output vs tool |
| RVZ packing/PRNG salah implement | self-check M0 (padding byte-exact vs generator spec) |
| Ghidra belum ada / download 600MB | disetujui user; fallback rizin raw PPC (tanpa decompiler) |
| REA DOL lane tolak file: slot di luar window MEM1 (PoR mungkin pakai MEM2 utk suatu segmen) — **belum diketahui sebelum header DOL dibaca** | parser header offline M2; fallback rizin / normalisasi header (dengan catatan deviasi) |
| Derivative license: port codec/dok tellius-forge = **GPL-3.0** | OK utk use pribadi; WAJIB sertakan sumber+lisensi bila tool pernah didistribusikan |
| FE9 pakai REL (REA tak dukung) | cek FST M1; kalau ada → loader REL→flat-bin sendiri (spec REL lengkap ada) |
| Disc 2 tidak tersedia | semua analisis disc1; fitur lintas-disc (endgame) ditandai OPEN |
| paired-single Gekko tak ter-decode (language stock) | pasang extension Gekko_Broadway (opsional) |

## 7. Kriteria selesai (acceptance)

1. `node bin/rvz.mjs ls /` menampilkan FST penuh tanpa error; bukti integritas: padding self-check byte-exact **jika** packed>0, ATAU rantai bukti tak-langsung (apploader==dhead, FST in-bounds, magic BNR/DOL valid) — dinyatakan eksplisit di notes mana yang dipakai.
2. `inventory/files.csv` ≥ seluruh FST, 0 entri out-of-bounds.
3. `main.dol` termuat di REA-Ghidra; ≥ 3 fungsi ter-decompile dengan nama/string yang dikenali komunitas.
4. Tabel karakter `system.cmp` → CSV match spot-check vs FE9Data.
5. ≥ 1 tekstur PNG + 1 model + 1 video + 1 file teks terbaca manusia.
6. Semua di atas reproducible dari nol dengan command tercatat.

## 8. Keputusan user (terkunci sesi ini)

- [x] Ghidra: **boleh download** (M2 jadi jalan; JDK 21 sudah ada).
- [x] Disc 2: **tidak ada — disc 1 saja**; analisis lintas-disc = OPEN, jangan dijanjikan.
- [ ] Persetujuan final plan pasca-audit — status: **menunggu review kamu**.

## 9. Audit changelog (pass skeptis #1, temuan yang sudah ditanam di atas)

1. **Fiktif:** verifikasi SHA-1 raw/group table — field-nya tidak ada di spec; hapus dari desain (draft kode juga salah baca slice).
2. **Kontradiksi antar-sumber** (WIA: list utk Wii-partition saja vs RVZSharp: `max(1,..)` utk semua): layout exception-list GC raw = **probe empiris** M0-probe#1, bukan asumsi.
3. **Claim tak diverifikasi** dibuang/di-mark: "disc2=GFEQ01", byte maker/disc header, ffmpeg-thp, URL release GameCubeLoader, keberadaan script cmp di repo tellius.
4. **Risiko baru:** REA ghidra-dol hanya terima window MEM1 — slot MEM2 pada DOL PoR belum diketahui → pra-validasi offline + fallback.
5. **Spec quirk** PRNG output `>>18 // NB: not 16` — implement literal, fallback terdokumentasi bila byte-exact gagal.
6. **Bukti integritas M0** tidak boleh dijanjikan absolut (tergantung packed>0) → kriteria acceptance dibuat kondisional + rantai alternatif.
7. **License** tellius-forge GPL-3.0 → konsekuensi distribusi tool dicatat.
8. Draft `fe9-rvz/rvz.mjs` = scratch pra-plan; dianggap **buang-atau-rewrite** setelah approval, bukan fondasi. (scratch sudah dihapus; API final = `fe9-re/bin/rvz.mjs`)

## 10. Record M0+M1 SELESAI (2026-11-10) — koreksi & temuan lapangan

**Koreksi interpretasi disc header** (YAGCD menyesatkan; bukti empiris + `DiscUtils.cpp` Dolphin):
- `0x420` = DOL offset ✓ (0x20300). `0x424` = **FST offset** (0x2C6100) — BUKAN "dol size". `0x428` = **FST size** (0x1A2DA). `0x42C` = unused (di disc ini duplikat 0x1A2DA — asal "junk identical").
- Ukuran DOL tidak disimpan di header: dihitung dari max(section foff+size) = **0x2A5DC0** (DOL berakhir 0x2C60C0; FST mulai 0x2C6100, slack 0x40).

**Temuan skema FST GFEE01 — menyimpang dari textbook** (divalidasi: 0 anomali, tiling 100%, 4623 entri; 4077 file + 545 dir; layout pos file sequential gap ≤ 3 B):
- Flag direktori = bit **`0x01000000`** pada word-0 (BUKAN `0x80000000`; bit31 tidak pernah terlihat).
- Field size direktori = **indeks entri akhir-subtree (absolut, exclusive)** — bukan jumlah entry rel. diri sendiri.
- String table mulai di `total*12`; string nama root tidak ada (root = "").
- Semua penemuan string (`system.cmp`, `zmap`, dst.) selama ini memang ADA di region string-table (0x2D3974–0x2E03DA) — scan negatif dulu murni karena salah offset FST. Disc TIDAK scrubbed/patched.

**Artefak:** `out/exec/main.dol` (2.776.512 B, md5 `74da02bee130b88bedc0ade4f20dda98`, validateDol: 0 overlap, entry 0x80003154 ∈ text, semua ≤ MEM2). `out/banner/opening.bnr` (magic **BNR1** ✓, 6496 B, md5 `656161b9517e75c63aab8a8002eadbc6`). `out/samples/<ext>/` 22 ekstensi (thp trunc 8 MB). `inventory/files.csv` 4077 baris.
**Oracle gagal:** wit 3.05a (2022) tidak baca RVZ (format lahir 2023) — ganti oracle: source Dolphin (`DiscUtils.cpp`, `FileSystemGCWii.cpp`) + self-consistency.

**Status kriteria §7:** #2 ✓ tercapai. #1 sebagian (API `readFST`/`walkFiles` ✓; CLI `ls` belum ditulis). #3 berikutnya (M2).

## 10b. Record M2 SELESAI (2026-11-10) — jalur analisis final

**Kematian jalur REA di Windows (semua terdokumentasi empiris, `out/rea_m2_probe.json` + `out/rea_elf_probe.json`):**
- REA 6.8.0 ghidra-lane butuh Ghidra **12.1.x** + **JDK 25**; dan `gamecube-dol` **POSIX-only** (hard rule, versi benar pun tetap tolak).
- `rea inspect main.elf` (PPC32 BE) → `unsupported ELF architecture` — lane Windows hanya x86/x64/ARMNT/ARM64. Skill `reverse-engineer-anything` mengonfirmasi boundary ini. **Kesimpulan: PPC = selalu `analyzeHeadless.bat` langsung.**

**Pipeline final (terbukti jalan, simpan untuk re-produksi):**
```
JAVA_HOME=<jdk-25.0.5+8>   # Ghidra 12.2 WAJIB JDK 25+ (bukan 21!)
node bin/dol2elf.mjs out/exec/main.dol out/exec/main.elf
analyzeHeadless <projdir> FE9m4 -import out/exec/main.elf -processor PowerPC:BE:32:default \
  -scriptPath bin/ghidra -postScript DumpFns.java <fns.tsv> <strings.tsv>
# dump ulang tanpa analisis: -process main.elf -noanalysis -postScript ...
```
- Tanpa `-processor`, Ghidra 12.2 gagal "No load spec found" (ELF→PPC tak auto-map).
- `-deleteProject` = proyek **sementara** (dihapus saat selesai) — jangan dipakai kalau ingin simpan rep.
- API Ghidra 12.2: `Address.getUnsignedOffset()`, `AddressRange.getLength()/getMinAddress()` (getValue/getStart/getEnd hilang).
- Auto-analysis penuh 2,7 MB PPC = **65 detik**.

**Hasil:** 8125 fungsi (`out/ghidra/functions.tsv`), 5690 string (`strings.tsv`), 4 fungsi ter-decompile (`out/ghidra/dec/`). **Penerimaan §7#3 ✓**: `entry` @0x80003154 = `__start` khas Nintendo GC (koreksi r13/r2 `_SDA2_BASE_/_SDA_BASE_`, cek BootFlag, rantai OSInit); `FUN_80005c8c` = pemanggil literal `"system.cmp"` (xref string→fungsi via `DecompileSome.java s:`); literal lain: `FE8Data.bin` @0x80267c26, `mess/common.m` @0x80267c51.
**Proyek Ghidra persisten:** `out/ghidra/FE9m4` (jangan dihapus; `-process` untuk query ulang).
**Repo publik:** kerja dipush ke `github.com/valdifebrian/Fire-Emblem---PA-Port` (kode+docs+inventori saja; konten disc di-gitignore).

## 10c. Record M3 SELESAI (2026-11-10) — LZ77 + .cmp + FE8DATA tables + .m

**Crack LZ77 (varian GC "0x10")** — metode: ground-truth diff. File `/FE8Data.bin`, `/FE8Anim.bin`,
`/cp_data.bin` ADA UTUH (uncompressed) di root disc → decode sebagian dari `system.cmp` lalu diff
vs salinan root menemukan aturan yg salah. Format terverifikasi byte-identical:
- Header: `[u8 0x10][u24 BE size]` — **field size di system.cmp (0xA0A61A) PALSU/lebih besar dari
  blob nyata** (~1.75 MB); decoder jalan sampai sumber habis, bukan sampai size.
- Stream: flag byte 8 item MSB-first. bit=0 → literal. bit=1 → 2 byte: `off=((b0&0xF)<<8|b1)+1`,
  `len=(b0>>4)+3` (nibble 0..0xF semua valid 3..18; TIDAK ADA escape 0xF). Overlap copy byte-per-byte,
  off=1 → RLE.
- Kegagalan awal = bug sendiri (4 item/flag byte), bukan format.

**Container `.cmp`** (`bin/cmp.mjs`): `[pack-header?]` tidak — langsung LZ77 stream-chain: tiap blok
`[0x10][size][...]`; hasil decompress = blob `"pack"`: u32 magic @0, u16BE count @4, entri @8 stride
0x10: `[u32 0][u32 namePtr][u32 dataPtr][u32 len]`, pointer absolut dari awal blob. Terverifikasi:
system.cmp 16 entri; isi FE8Data.bin/FE8Anim.bin/cp_data.bin byte-identical dgn salinan root;
title.cmp + zdbx.cmp (449 entri) ikut decode. CLI: `node bin/cmp.mjs ls|extract <file> <outdir> [prefix]`.

**FE8Data.bin (di dlm system.cmp) — tabel data** (`bin/fe8data.mjs` → CSV):
- Header 0x20: `[u32 len][ptr][ptr][112]`; subheader @0x20 `[0][ptr bigID 0x14a3e][ptr JP 0x1c8ab][u32 nChars]`.
- KARAKTER @0x30 stride **0x54 ×340** ✓ (entri terakhir PID_DAYNE_PLD_1; entri 50 = IKE lvl 8, base 5 5 0 6 6 6 4 1,
  grow 60 40 30 45 45 25 35 30 — cocok dokumen FEU). Field: f00 PID, f04 MPID, f08=0, f0c FID, f10 JID,
  f14 affil, f18-f28 SID/AID ptr, f36 lvl, f37 build, f38 weight, f39 base×8, f41 grow×8, f49 u8[8], f51 tail.
- KELAS: u32 count @0x6FC0 = 0x73 (115), entri @0x6FC4 stride **0x64** (f00 JID, f04 MJID nama, dst hex-dump).
- ITEM: u32 count @0x9CB0 = 0xBD (189), entri @0x9CB4 stride **0x60** (bukan 0x64!) — f00 IID, f04 MJID.
- **String pool = suffix pool**: pointer boleh mendarat di tengah string tetangga ("PEGASUSKNIGHT/F"
  ⊃ "NIGHT/F"; "STEELSWORD" ⊃ "EELSWORD") — ini normal, bukan bug; game baca dari pointer s/d NUL.
  41 slot item kosong (iid=0) = ID bolong.
- Output: `out/data/{characters,classes,items}.csv` (340/115/189 baris).

**Format `.m`** (`bin/m.mjs`): header `[u32 len][u32 tableHint][0][u32 count]`; text region @0x20..tabel;
tabel = pasangan interleave **`[u32 strOff][u32 msgId]` stride 8**, id ascending; lokasi tabel =
hint + 0x20..0x40 (auto-scan; common.m +0x20, c09.m +0x34). Entri = string NUL-terminated dengan
control-code: `$c1GREIL3|` (portrait/speaker), `$s1` (nama), `$K` (baris), `$w2/$w4` (wait), `$d1` (selesai).
Validasi: common.m 2530 pesan UI ("Restart Chapter", "Sound Room"); `/Mess/c09.m` (extract disc, 62779 B)
75 pesan → dialog Greil prolog ch.9 utuh. File `/Mess/cNN.m` per chapter (c01.. dst, di inventory).

**Koreksi tools:** `bin/lz77.mjs` ditulis ulang sesuai aturan terverifikasi (escape 0xF dihapus);
`bin/fe8data.mjs` count u32 (bukan u16) + stride item 0x60.

## 10d. Record M4 SEBAGIAN (2026-11-10) — tpl/.cms/.ga ter crack + pipeline aset

**`.tpl` GFEE01 = magic `0x0020AF30`** (spesifikasi cocok dgn `mila` crates/thane98):
- Header: `[u32 magic][u32 count][u32 tableOff(=0xC)]`; tabel = count×`(u32 imgPtr, u32 palPtr)`.
- Image: `[u16 h][u16 w][u32 fmt][u32 dataPtr][u32 wrapS wrapT minF magF][f32 lodBias][u8×4]`.
- Palette: `[u16 n][u8 unpacked pad][u32 fmt][u32 dataPtr]` (fmt IA8/RGB565/RGB5A3).
- Format GX: 0=I4,1=I8,2=IA4,3=IA8,4=RGB565,5=RGB5A3,6=RGBA8,8=CI4,9=CI8,10=CI14X2,14=CMPR.
- **Tiling per format** (blok linier → baris): I4/CI4/CMPR 8×8; I8/IA4/CI8 8×4; IA8/RGB565/
  RGB5A3/RGBA8 4×4; alamat unit per-pixel `block*bsize+(y%bh)*bw+(x%bw)`, I4/CI4 nibble
  MSB-first 2px/byte (`(y%bh)*4+(x%bw)>>1`). RGB5A3 opaque=RGB5G3B4, transparent=a3r4g4b4.
- Validasi visual (vision model): icon.tpl atlas 768×216 → grid ikon FE bersih tajam;
  crop match paragon (affinity 216×24 @y0, item 24×24 mulai y=72, skill 32×32 di tex1 1024×96).
- **CMPR belum diimplementasi** (stub hitam) — route/xinfo sebagian pakai fmt14. TODO M4b.
- Tool: `bin/tpl2png.mjs png <file...>` (auto-LZ77 utk .cms) → `out/png/`.

**`.cms` = LZ77(`.tpl`)** — 1 blok 0x10 (field size bisa iseng, sama spt .cmp). Verified:
`/Face/IKE.cms` → tpl 14 image (base 192×128 + overlay mata/mulut 24×24/16×16 + 64×64) = sistem
wajah FE (base blank + part overlays; blok putih di base = BUKAN bug). Batch **360/360 portrait
`/Face/*.cms` → 4347 PNG, 5.7 detik, 0 gagal** (`node bin/tpl2png.mjs face out/faces`).

**`.ga` (animasi)**: format SUDAH terdokumentasi byte-exact → `mila`(exalt) & **tellius-forge
research/animation/tellius-animation-file-format.md v2.0.1** (klg-tellius rebuild byte-perfect
seluruh disc FE9 — TIDAK perlu riset ulang; reader .g/.gs juga ada: TelliusModding klg-tellius).
Struktur: header 0x30 (footer_off, game_flag@0x08: 00=FE9, fmt=0x11, loop, start/end, bone_count,
ptr3 section); bone 16B (idx, mask s/r/t=8/16/32, first_curve, n); curve 12B (channel 0-8/12-14,
frac_bits<<8, end_frame, key_count, first_key); key 4B (u16 frame, i16 value×2^-fb, ABSOLUT,
interpolasi linear); footer FE9 = 9 slot ptr (slot0 = event track: frame+codes; code 0/1 hit/swing,
2 throw, 3/4 loop, 0x14-0x1B trail, ≥1000 SE). Tool: `bin/ga.mjs json <file...>` → `out/ga/*.json`
(header+bone+curve+key+events tervalidasi: `/ymu/archer/atk1_bw.ga` 19 bones/58 curves/6 events,
bone0=hip idx1 mask0x30, code 1 @39 + SE @1550/1570 + trail 0x17 ✓ cocok semantik dokumen).

**Pending M4**: CMPR decoder; `.g`/`.gs` parser (specs tellius ada di repo third_party); `.stm`
(audio GC ADPCM); `.pak`; `.cmb`; `.thp` (ffmpeg TIDAK terpasang di dev box — defer ke M6/human
atau portable ffmpeg). 786 `.cmp` lainnya (di /bmd dll) = container pack yg sama → sudah bisa
`bin/cmp.mjs extract` lalu tpl/ga decode per isi.
