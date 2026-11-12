# FE9_P2_GATE — paket review gate P2 (5 kemampuan, master plan §8)

Status = bukti per capability dari repo + perangkat (bukan klaim). Angka/log = artefak yang sudah ada.
Tiga keputusan manusia diminta di bawah (D-P2-A/B/C). Setelah keputusan → gate CLOSED atau gap ditutup.

## Update build b57 (2026-11-12, `[BRV-P2-001]`, BUILD SUCCESSFUL)
Instrumentasi gate kini DI APK: (a) `tapLatMax=..us tapLatN=..` per-tap ACTION_DOWN→render-complete
pada baris log 2-detik; (b) `th=..C` (thermal_zone0; -1 = zona tak terbaca); (c) tombol overlay baru
**T** (kodet 7, diamond kecil kanan-atas) = cycle SEMUA gambar FE9 ter-decode (tpl unit + atlas
`face/system.cms`) ke slot tekstur unit + log `tex pick: src=.. #.. WxH (pick n/total)` — bukti kelas
"atlas ikon+portrait" tanpa shader baru. Compile-only; belum dipasang/diuji di HP (peraturan: device = human).

## Tabel kemampuan (semua uji di hp fisik)

| # | Kemampuan (§8) | Bukti | Status |
|---|---|---|---|
| 1 | Load+render tekstur FE9 (atlas ikon+portrait) | tpl unit `archer/tex_4.tpl` render di device (override AM-075 QA PASS); `face/system.cms` sudah DI-DECODE di device (`tex[i] WxH fmt ok` di log) — upload-nya baru via cycle T (b57, belum di-capture) | **SUBSTANSIAL** — tutup = 1 logcat + screenshot `tex pick src=1` di b57 |
| 2 | 1 model 3D asli + kamera | Archer asli FE9 di device: `poc_gm_grid.png`, `poc_final.png`; orbit yaw + lookAt + pitch clamp 8–75° (bug "quad raksasa" CLOSED); equip view sword+bow | **LULUS** (ganti-Ike = kosmetik, bukan kelas beda) |
| 3 | 1 `.ga` benar (frame timing + event codes) | `.ga` jalan di device ~12 anim-fps; event slot-0 footer DIPARSE (`gaanim.h`) + di-log saat load (`event f=.. codes=..`) DAN saat loop fire (`evt f=.. c0=..`) — 4 set anim selectable via B | **LULUS** (bukti = baris `evt f=` di logcat capture lama; kelas = baca + timing) |
| 4 | 1 BGM `.stm` + 1 SFX mixing | `boss1.stm` loop + `boss2.stm` mixed via Oboe, gain toggle live (tombol A); decode sr/frames/loopStart di-log | **PARTIAL oleh blocker eksternal** — SFX asli = payload `.samp` (satu-satunya jalur tersisa = Dolphin AX-ucode RAM trace; semua jalur statis dibuktikan negatif). Lihat D-P2-A |
| 5 | Touch overlay responsif (<50 ms, rapid, 4-arah) + keyboard di harness desktop | Touch LIVE di device: state-based (bukan event) → rapid-fire capable; `taps2s` counter live; 4-arah ✓ (dpad→sumbu-dunia terbaik di yaw iso-45, regresi CLOSED); gamepad penuh ✓; **b57: `tapLatMax` per-window kini ter-log** — angka belum pernah direkam | **PARTIAL**: butuh 1 logcat b57 dengan tap cepat. Keyboard desktop = lihat D-P2-C |

Plus §8: fpk loader ✓; fps meter ✓; **thermal meter: b57 (belum di-capture)**; profiling hook = baris
telemetri 2-detik ini (fps/lat/thermal/pose/cam) = hook P3 tinggal naik kelas.

## Sisa aksi menuju CLOSED
1. Human: install b57 (`app/build/outputs/apk/debug/app-debug.apk`) → logcat `fe9poc` 30-60 dtk sambil:
   tap overlay cepat + jalan 4-arah + tekan T 3×; simpan log + 1 screenshot `tex pick src=1` + video singkat.
2. Laporan chat "captured" → isi angka `tapLatMax` rata2/maks ke tabel di bawah → item (1)(5) jadi LULUS.
3. Keputusan D-P2-A/B/C di bawah.

## Keputusan yang diminta (gerbang)
- **D-P2-A — item (4) SFX**:
  - (i) **RECOMMENDED**: terima 2-stream `.stm` mixing sebagai bukti kelas-mixing; gate tidak ditahan; `.samp` = P2-followup ber-tag blocked (hanya RAM trace, kapan pun ada waktu Dolphin).
  - (ii) tahan gate sampai SFX asli (menahan P3 di blocker yang aksesibilitasnya rendah).
- **D-P2-B — angka latency**: (i) **RECOMMENDED**: angka `tapLatMax` logcat b57 = bukti resmi (<50 ms diuji vs klaim §8); ATAU (ii) proxy `taps2s` saja (lebih lemah dari teks §8).
- **D-P2-C — harness keyboard**: (i) **RECOMMENDED**: potong dari P2; keyboard jadi kontrak P3 (T suite §3 menguji action enum dari 3 sumber; device = target resmi produk, sesuai "Android-primary"); ATAU (ii) tetap sesuai teks §8: harness desktop dibangun sebelum gate closed (tambah kerja, nilai bukti kecil karena P3 akan membuatnya ulang sebagai modul resmi).

## Rekomendasi satu baris
Install b57 + 1 logcat + 1 screenshot + 1 video → terima (i)(i)(i) → gate CLOSED; arsitektur D1 terkunci; P3 mulai.

## Prosedur setelah keputusan
1. Jawaban keputusan ditulis di file ini oleh agent,
2. Bukti b57 masuk `fe9-captures/`, human lapor via chat,
3. Tabel di atas di-update LULUS, scoreboard: Milestone P2 → **GATE CLOSED**; P3 todo di-init.
