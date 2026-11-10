# FE9_BEHAVIOR_SPEC.md — Spesifikasi Perilaku Game

Bagian dari paket master plan (lihat `FE9_MASTER_PLAN.md` §6). Terpisah dari spesifikasi
FORMAT aset (`FE9_ASSET_MATRIX.csv`). Ini dokumen "bagaimana FE9 sebenarnya bekerja".

## Aturan penulisan
- Entri = satu perilaku terverifikasi: `BRV-<domain>-<nnn>`; DILARANG mengimplement fitur
  engine yang BRV-domainnya belum ≥ Strong.
- **Confidence:** `Confirmed` = terbukti di game asli (capture/Dolphin/byte) · `Strong` =
  konsisten banyak bukti sekunder · `Reported` = wiki/komunitas, belum dicek.
- Setiap entri WAJIB punya: pernyataan, bukti, status confidence, test `T-###` penerima.
- Sumber bukti yang sah: field CSV hasil RE (`data/*.csv`), `.dbx` teks, `.ga` event codes,
  capture Dolphin (human), Ghidra oracle, FEU wiki (→ Reported dulu).

Domain (status awal): battle `Unknown` · character `Partial` · ai `Unknown` · map `Unknown` ·
animation `Partial` · event `Unknown` · dialogue `Partial` · progression `Unknown` · ui
`Unknown` · save `Unknown` · rng `Unknown`.

### Prioritas pengisian (instruksi reviewer v2-C)
1. BRV-BAT battle calculation → 2. BRV-MAP movement/terrain → 3. BRV-CHR growth/level-up →
4. BRV-AI enemy AI → 5. BRV-EVT chapter event/script → 6. BRV-PROG recruitment/progression.
Sumber bukti berjenjang: tabel hasil RE > capture Dolphin (Confirmed) > Ghidra oracle >
FEU wiki (mulai `Reported`, naik setelah golden test). **Entri `Reported` TIDAK boleh jadi
gerbang implementasi — minimal `Strong`.**


---

## Seed entri (terbukti dari data yang sudah ada; bukan asumsi)

### BRV-CHR-001 — Record karakter 0x54 byte
Record FE8Data.bin @0x30 stride 0x54 × 340: f00 PID, f0c FID(form/cls), f14 afiliasi,
f36 level, f37 build, f38 bobot, f39 base×8 stat, f41 grow×8 persen.
Bukti: diff byte vs nilai in-game Ike (lvl8, base 5 5 0 6 6 6 4 1) & Mist.
Confidence: Confirmed. Test: T-DAT-001. [AM-`characters.csv`]

### BRV-CHR-002 — Tabel class & item
Classes 115 × 0x64 (mulai 0x6FC4, count u32 @0x6FC0); Items 189 non-kosong × 0x60 (ID holes
41). Item menyimpan might/range/stat bonus/efek (field map final → kerja P1 `itemspec`).
Confidence: Confirmed (eksistensi/stride), **Partial** (semantik field per kolom).
Test: T-DAT-002. Catatan: semantik tiap kolom item = BRV baru setelah dibaca manual.

### BRV-ANM-001 — Event codes animasi battle
`.ga` footer slot0: code 0=blow lands (damage/HP), 1=swing (miss pun round lanjut),
2=throw release, 3/4 loop, 0x14–0x1B weapon trail (0x19 end), ≥1000 SFX; engine mengumpulkan
kode tiap frame yang dilewati (maks 4/update); event di luar [start,end] tak terkirim.
Bukti: tellius-forge v2.0.1 (in-game debug + byte-rebuild) + arsitek sampling archer_atk1
(code 1 @39, trail 0x17, SE 1550/1570).
Confidence: Strong (laporan + byte; belum kita cek in-game sendiri). Test: T-ANM-001.

### BRV-ANM-002 — Keyframe evaluasi
Kurva per bone-channel, nilai ABSOLUT (bukan delta), interpolasi LINEAR antar key, hold
sebelum/ sesudah; scale 0 menyembunyikan bone. Bukti: spec byte-exact + decomp FE9 (dok).
Confidence: Strong. Test: T-ANM-002 (frame sampling vs Dolphin video).

### BRV-DLG-001 — Kontrol kode dialog
`.m` message: `$c1<NAME3>` ganti speaker/potret, `$s1/$w2/$d1` dsb; teks + id 16-bit;
suffix-pool pointer (deref mentah benar). Bukti: c09.m dibaca (dialog Greil).
Confidence: Confirmed (parser), **Partial** (semantik penuh per kode — katalog wajib P1).
Test: T-DLG-001.

### BRV-DLG-002 — Speaker name 3-karakter uppercase
ID speaker internal = 3 byte (GREIL→`GRE`, `PID_LOFA=arc2` pola alias .dbx). Bukti: c09 +
jobList.dbx (`PID_...=alias`). Confidence: Strong. Test: T-DLG-002.

### BRV-MAP-001 — Lookup aset per-job
`.dbx` jobList/weaponList memetakan JID/IID → alias model/anim battle (reng, amr1, …) +
nama JP; classList job→unit. Confidence: Confirmed (teks terbaca). **Partial:** semantik
field per kolom dbx. Test: T-AM-LINK.

### BRV-EVT-001 — Kontainer script chapter
`/Scripts/C??.cmb` = magic `"cmb\0"` + nama-diri ASCII (`C01.cmb`) + blok `cmb` berulang
(probe 2026-11-10). Isi/opcode = `Unknown`; investigasi = P1 #1 (metodologi §7 master plan).
Confidence: Confirmed (kontainer), Unknown (semantik). Test: T-RE-SCRIPT1/2, T-SCT-VM.

### BRV-ANM-003 — Kamera battle per (job, animasi, senjata)
`/xcam/<job>/*.dbx` = teks SJIS satu file per variasi (mis. `fig1_cr2_ax.dbx`), header `#` +
nama kamera JP (probe). Menentukan setup kamera battle utk kombinasi job×anim×wpn;
`/xcam/*/cam.cmp` = pack data kamera. Confidence: Confirmed (eksistensi/granularitas),
Partial (semantik field). Test: T-C1-ANIM (A/B video). → masuk BRV-BAT saat dipetakan.

### BRV-AUD-001 — Pemetaan SFX per event
`/Sound/gcfesnd.stbl` memuat string `SE1_EVT_FOREST1` dst. (probe) = tabel event→SFX;
bank = `gcfesnd.{slib,sdir,pool,proj,samp}` + `.stm` (STM-headerless, `_lp`=loop).
Confidence: Confirmed (eksistensi), Unknown (format). Test: T-RE-AUD1, T-AUD-MIX.

### BRV-SYS-001 — Varian aset region/bahasa
Model pack battle punya varian sufiks `_ja/_no` (`amr1_ja.pak`, `kni2_no.pak`) — runtime memilih
set per region. Port WAJIB meniru pemilihan varian disc USA sebelum bake (bukan asal pilih).
Confidence: Strong (pola nama + disc USA). Test: T-CNT-CH varian check.

