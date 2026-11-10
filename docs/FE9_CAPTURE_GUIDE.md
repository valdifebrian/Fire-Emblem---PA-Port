# FE9 T-RE-CAP1 — Panduan Capture Dolphin (untuk manusia)

Target: merekam perilaku game ASLI (disc USA GFEE01) di Dolphin 2609b portable
(`C:\Dolphin`) sebagai oracle perilaku engine. Semua hasil akhir harus masuk
`C:\Kodingan\fe9-captures\`.

## Persiapan (5 menit, sekali)
1. Buka `C:\Dolphin\Dolphin.exe` (sudah terinstal; game sudah terdeteksi:
   *Fire Emblem: Path of Radiance (GFEE01)*).
2. **Config → Controller Settings → GameCube Controller 1 = Keyboard**
   (default sudah keyboard; catat mapping tombol yang kamu pakai, atau ganti
   dulu ke yang nyaman: arah + A/B/X/Y + Start + Z + R).
3. View → Show Menu Bar (kalau belum ada).
4. Buat folder: `C:\Kodingan\fe9-captures\`

## CAP-1 — Cold boot → ATTRACT (regression guard boot)
1. Close game dulu (jangan resume dari state).
2. **Tools → Movie → Create New Input Recording** → nama: `cap1_boot.dmo` → mulai.
3. Boot game, JANGAN sentuh keyboard 60 detik.
4. Harus masuk attract/demo intro (bukan langsung gameplay). Kalau langsung
   masuk gameplay → catat: `CAP1 FAIL: boot straight to gameplay`.
5. Biarkan attract jalan 90 detik → **Stop Recording** (Tools → Movie → Stop).
6. Screenshot 2×: judul + attract: View → Screenshot (gambar masuk
   `C:\Dolphin\User\Dump\Screenshots\`).

## CAP-2 — Input sequence penuh ke awal Chapter 1
1. Recording baru `cap2_start.dmo` (dari posisi game masih jalan, boot continue
   dari awal baru: tekan Start → pilih New Game → Difficulty Normal → konfirmasi).
2. Main sampai **peta Chapter 1 (Prologue)** tampil dengan unit awal.
3. Stop recording. Screenshot 1× (layar peta Ch1).

## CAP-3 — Battle turn-by-turn (oracle AI + combat)
Lanjut recording baru `cap3_battle.dmo` dari posisi unit bisa gerak:
1. **SAVE STATE F1** = Shift+F1 (sebelum menyentuh apa pun). Nama state bebas,
   catat urutan.
2. Gerakkan 1 unit (panah + A), serang 1 musuh (pilih serang, biarkan damage
   preview muncul → **screenshot preview damage** → tekan A untuk eksekusi).
   **SAVE STATE F2** (setelah animasi).
3. Akhiri turn kamu (Enter/Y atau menu End). Musuh jalan. Rekam **semua** turn
   musuh sampai turn kamu lagi. **SAVE STATE F3**.
4. Bunuh 1 musuh sampai HP-nya habis (mati di layar). **SAVE STATE F4** +
   screenshot momen musuh mati.
5. Lanjut sampai turn kamu lagi 1× penuh + gerak-gerakin unit sambil FIRE
   berulang 10× (spam tombol serang sambil gerak 4 arah) → Stop recording.
6. Ulangi CAP-3 persis sekali lagi dari F1 dengan **Save Load State F1**
   (Shift+F1 load) → rekam `cap3b_replay.dmo` — untuk bukti determinisme.

## CAP-4 — Kamera & UI
Dari mana saja: rekam `cap4_ui.dmo`, di dalam map Chapter 1: zoom/rotate kamera
(kalau ada), buka menu unit (info panel), buka inventory, buka menu pause,
tutup semua. Stop + 3 screenshot (info panel, inventory, pause).

## Setelah selesai — lapor
Copy SEMUA file ini ke `C:\Kodingan\fe9-captures\`:
- `C:\Dolphin\User\Movie\GFEE01\*.dmo` (semua recording input)
- `C:\Dolphin\User\StateStates\GFEE01\*` (semua save state F1..F4)
- `C:\Dolphin\User\Dump\Screenshots\*GFEE01*.png` (screenshot hari ini)

Lalu chat: `captured, di C:\Kodingan\fe9-captures`

## Checklist (centang saat kerjain)
| ID | Isi | File | OK |
|----|-----|------|----|
| CAP-1 | boot→attract 90s | cap1_boot.dmo + 2 png | ☐ |
| CAP-2 | start sampai peta Ch1 | cap2_start.dmo + 1 png | ☐ |
| CAP-3 | battle lengkap + F1-F4 + spam-fire | cap3_battle.dmo + states + png | ☐ |
| CAP-3b | replay identik dari F1 | cap3b_replay.dmo | ☐ |
| CAP-4 | kamera/UI panel | cap4_ui.dmo + 3 png | ☐ |
