# FE9_P2_GATE — paket review gate P2 (5 kemampuan, master plan §8)

Status = bukti per capability dari repo + perangkat (bukan klaim). Angka/log = artefak yang sudah ada.
Tiga keputusan manusia diminta di bawah (D-P2-A/B/C). Setelah keputusan → gate CLOSED atau gap ditutup.

## Update build b58 (2026-11-12, `[BRV-P2-001]` fix, BUILD SUCCESSFUL + terpasang)
KOREKSI dari capture sesi b57: entri `face/system.cms` yang dibake ternyata = **atlas font GCF**
(/Fonts/system.cms, magic 0x20, 20 sheet 3988 glif = AM-043) — BUKAN tekstur wajah; decode TPL-nya
gagal senyap sehingga cycle T cuma punya 1 gambar. Re-bake `poc1.fpk` (manifest kini di version
control: `fe9-re/out/poc/poc1.list`) pakai **`/Face/ANNA.cms`** asli: boot device log =
`tex[0..13] ... ok=1` (14 gambar: base 192x128 + lapisan 24x24/16x16/24x16/64x64 = atlas wajah +
ekspresi FE9, native LZ77+TPL+I8 pal di GLES3). Cycle T kini 15 gambar. Catatan harness:
`adb shell input tap` DIBLOK vivo (OriginOS) → `taps2s=0`; input perangkat = tangan manusia (aturan).
Latensi b57 valid: 8 jendela, tapLatMax terburuk 16,5 ms, median ~11 ms << 50 ms (REQ-TIME).

## Update build b57 (2026-11-12, `[BRV-P2-001]`, BUILD SUCCESSFUL)
Instrumentasi gate kini DI APK: (a) `tapLatMax=..us tapLatN=..` per-tap ACTION_DOWN→render-complete
pada baris log 2-detik; (b) `th=..C` (thermal_zone0; -1 = zona tak terbaca); (c) tombol overlay baru
**T** (kodet 7, diamond kecil kanan-atas) = cycle SEMUA gambar FE9 ter-decode (tpl unit + atlas
`face/system.cms`) ke slot tekstur unit + log `tex pick: src=.. #.. WxH (pick n/total)` — bukti kelas
"atlas ikon+portrait" tanpa shader baru. Compile-only; belum dipasang/diuji di HP (peraturan: device = human).

## Tabel kemampuan (semua uji di hp fisik)

| # | Kemampuan (§8) | Bukti | Status |
|---|---|---|---|
| 1 | Load+render tekstur FE9 (atlas ikon+portrait) | tpl unit `archer/tex_4.tpl` render di device (override AM-075 QA PASS); **b58: `/Face/ANNA.cms` 14 gambar `tex[0..13] ok=1` ter-upload+render native via cycle T — log `tex pick: src=1 #9 16x16 ok=1 (pick 11/15)` (capture device 2026-11-12 11:23, `fe9-captures/b58_texpick.png`)** | **LULUS** |
| 2 | 1 model 3D asli + kamera | Archer asli FE9 di device: `poc_gm_grid.png`, `poc_final.png`; orbit yaw + lookAt + pitch clamp 8–75° (bug "quad raksasa" CLOSED); equip view sword+bow | **LULUS** (ganti-Ike = kosmetik, bukan kelas beda) |
| 3 | 1 `.ga` benar (frame timing + event codes) | `.ga` jalan di device ~12 anim-fps; event slot-0 footer DIPARSE (`gaanim.h`) + di-log saat load (`event f=.. codes=..`) DAN saat loop fire (`evt f=.. c0=..`) — 4 set anim selectable via B | **LULUS** (bukti = baris `evt f=` di logcat capture lama; kelas = baca + timing) |
| 4 | 1 BGM `.stm` + 1 SFX mixing | `boss1.stm` loop + `boss2.stm` mixed via Oboe, gain toggle live (tombol A); decode sr/frames/loopStart di-log | **PARTIAL oleh blocker eksternal** — SFX asli = payload `.samp` (satu-satunya jalur tersisa = Dolphin AX-ucode RAM trace; semua jalur statis dibuktikan negatif). Lihat D-P2-A |
| 5 | Touch overlay responsif (<50 ms, rapid, 4-arah) + keyboard di harness desktop | Touch LIVE di device: state-based (bukan event) → rapid-fire capable; 4-arah ✓ (dpad→sumbu-dunia terbaik di yaw iso-45, regresi CLOSED); gamepad penuh ✓; **b57+capture user: 8 jendela `tapLatMax` — terburuk 16,5 ms, tipikal ~11 ms, `taps2s` s/d 6 (rapid) — `fe9-captures/b57_log.txt`**; thermal `th=35-37C` ter-log ✓ | **LULUS** utk device (<50 ms terbukti 3× margin). Keyboard desktop = lihat D-P2-C |

Plus §8: fpk loader ✓; fps meter ✓; thermal meter ✓ (`th=35-37C` ter-capture di log b57/b58);
profiling hook = baris telemetri 2-detik ini (fps/lat/thermal/pose/cam) = hook P3 tinggal naik kelas.

## Sisa aksi menuju CLOSED
1. Human: install b57 (`app/build/outputs/apk/debug/app-debug.apk`) → logcat `fe9poc` 30-60 dtk sambil:
   tap overlay cepat + jalan 4-arah + tekan T 3×; simpan log + 1 screenshot `tex pick src=1` + video singkat.
2. Laporan chat "captured" → isi angka `tapLatMax` rata2/maks ke tabel di bawah → item (1)(5) jadi LULUS.
3. Keputusan D-P2-A/B/C di bawah.

## Keputusan yang diminta (gerbang) — **TERISI 2026-11-12, delegasi manusia ("kamu aja cek sendiri")**
- **D-P2-A — item (4) SFX**: ✅ **(i) Diambil.** 2-stream `.stm` mixing = bukti kelas-mixing diterima;
  gate TIDAK ditahan. `.samp` payload tetap tercatat blocked (BRV-AUD-003; satu jalur = Dolphin AX-ucode
  RAM trace) — dieksekusi saat sesi Dolphin berikutnya, TIDAK memblokir jalur lain.
- **D-P2-B — angka latency**: ✅ **(i) Diambil.** `tapLatMax` logcat (8 jendela, worst 16,5 ms) = bukti
  resmi REQ-TIME; prosedurnya (logcat + field stabil) direplikasi di T suite P3/P7.
- **D-P2-C — harness keyboard**: ✅ **(i) Diambil.** Keyboard bukan artefak P2; jadi kontrak P3
  (action enum 3 sumber: touch/gamepad/keyboard, diuji T suite §3). Device = target resmi produk.
- **Bar visual (permintaan user 2026-11-12 "jelek banget")**: ✅ **DISETUJUI & ditulis ke master plan**:
  target = paritas FE9 GC (bukan gaya mobile modern), diuji A/B screenshot vs Dolphin; build
  internal pertama P3 WAJIB sudah asset asli (map + UI icons + font), placeholder debug DILARANG
  lewat dari gerbang P4.
- Sisa tabel: item (4) PARTIAL-by-design (lihat D-P2-A) — BUKAN penahan gate.

## HASIL: **P2 GATE CLOSED** (2026-11-12, keputusan a.i/b.i/c.i + bar visual disepakati)
Arsitektur D1 (C++20 + Kotlin + GLES3 + Oboe + NativeActivity + FPK/override) TERKUNCI.
P3 Core Engine Foundation MULAI. Artefak bukti: `fe9-captures/b57_log.txt`, `b58_boot.png`,
`b58_texpick.png`, log device 11:23 (`tex pick src=1`), build b58 terpasang di 10DD14023D0007T.

## Rekomendasi satu baris
Install b57 + 1 logcat + 1 screenshot + 1 video → terima (i)(i)(i) → gate CLOSED; arsitektur D1 terkunci; P3 mulai.

## Prosedur setelah keputusan
1. Jawaban keputusan ditulis di file ini oleh agent,
2. Bukti b57 masuk `fe9-captures/`, human lapor via chat,
3. Tabel di atas di-update LULUS, scoreboard: Milestone P2 → **GATE CLOSED**; P3 todo di-init.
