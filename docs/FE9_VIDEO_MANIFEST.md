# FE9 Video Manifest — sumber perilaku offline (YouTube longplay)

Longplay NTSC GameCube (rilis 2005-10-17), dimainkan penuh tanpa emulator-side
rekaman khusus. Dipakai sebagai oracle perilaku offline untuk BRV yang sulit
direpro lewat injeksi input (damage angka, AI fasa musuh, rescue, terrain,
level-up, boss phase). Replay deterministik manusia = valid untuk observasi,
BUKAN ground-truth engine (tetap butuh konfirmasi disassembly untuk formula).

## Playlist (3 part + re-record part 1)
| # | ID | Durasi | Cakupan |
|---|----|--------|---------|
| 1/3 | `1B95v-cnXVo` | 10:53:25 | Start s/d Ch15 (opening, Prologue: Mercenaries, Ch01-15) |
| 2/3 | `GyvZuM51j2s` | 8:24:10 | Ch16 The Atonement s/d Ch23 The Great Bridge |
| 3/3 | `Txvg-_4Ax0k` | 10:59:03 | Ch24 Battle Reunion s/d Credits + 6 Trial Maps |
| re-record 1 | `PuNWbWbB_7A` | (var) | Versi baru/improved part 1 (cek sebelum pakai) |

## Indeks timestamp part 1 (`1B95v-cnXVo`) — prioritas RE
| Waktu | Segmen | Nilai RE |
|-------|--------|----------|
| 0:00:23 | Opening | urutan scene boot/attract |
| 0:02:16 | **Prologue: Mercenaries** | combat pertama + **damage numbers** + tutorial popup |
| 0:09:25 | **Ch01 The Battle Begins** | Greil combat, weapon triangle, terrain |
| 0:22:35 | Ch02 Rescue | **mekanik rescue** (pickup/deliver/penalty) |
| 0:41:53 | Ch03 Pirates Aground | water terrain cost, pirate AI |
| 1:06:57 | Ch04 Roadside Battle | reinforcement wave |
| 1:33:42 | Ch05 Flight! | escape map (segel peta) |
| dst. | (deskripsi video = indeks lengkap bab per part) | |

## Indeks ringkas part 3 (`Txvg-_4Ax0k`)
Trial Maps (post-game): Hillside Battle 6:22, Lonely Isle 6:58, Strange Turn
7:44, Desperation 8:26, Escape 9:12, Trapped 9:57 = stres-test AI/pressure
time — kandidat benchmark dispos/AI engine port.

## Pipeline analisis (tooling)
- Unduh: `bin/yt-dlp.exe -f "bv*[height<=480]+ba/b[height<=480]" --download-sections "*MM:SS-MM:SS" --force-keyframes-at-cuts --ffmpeg-location <winget ffmpeg>`
- Frame: `ffmpeg -ss ... -i segmen.mp4 -vf fps=2 out/%05d.png` → montage → vision.
- Simpan hasil antara di `fe9-re/out/yt/` (BUKAN repo; repo = kode+dokumen saja).
- Temuan damage/UI → seed validasi BRV-BTL-002; cross-check vs emulator CAP1
  (Myrmidon Iron Sword Mt4 Hit91v100 Crit2 HP19, durability 46/35/35).

Status pipeline: **SELESAI batch-1** — prologue/Ch1/Ch2 terunduh+ter harvest
(`fe9-re/out/yt/`, hasil → BRV-BTL-002 validasi + BRV-BTL-003 harvest).
Bagian 2/3 (Ch16-23) & 3/3 (Ch24-end+trials) = DEFERRED sampai butuh (late-game AI/boss).
