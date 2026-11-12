# FPK1 — format bundle aset PoC/engine

Bundle SATU-FILE yang dibaca engine di perangkat (NativeActivity mmap).
Kebijakan repo: file `.fpk` TIDAK PERNAH di-commit (berisi aset rights-holder);
hanya tool + format ini yang hidup di repo. Bake dari disc pakai
`bin/pocbake.mjs` (FE9_RVZ=<disc>; daftar isinya file teks per baris
`<src|disc:/Path> <nama-di-bundle> <fmt>`).

## Layout (little-endian)
| Offset | Isi |
|---|---|
| 0 | magic `fpk1` (4 B ASCII) |
| 4 | u32 `count` entri |
| 8 | tabel entri ×N: `name` C-string di-pad ke 4 B, lalu u64 `off`, u32 `len`, u32 `fmt`, u32 `crc32` |
| header_end | payload; tiap blob di-align 16 B, nol-padding |
| akhir-32 | sha256 dari byte 4 s.d. akhir-payload (verifikasi utuh saat load) |

## Kode `fmt`
0 raw · 1 tpl · 2 g · 3 ga · 4 stm · 5 png · 6 gcf_atlas · 7 csv · 8 fs

Aturan: `name` di bundle = path logis engine (mis. `bgm/boss1.stm`);
`crc32` per-blob (poly 0xEDB88320) untuk deteksi korosi parsial;
loader engine = validasi sha256 footer SEKALI saat open, lalu akses per-entri.

## Entri poc1.fpk (PoC P2, bake 2026-11-11)
```
archer/skeleton.g   g   6614 B   arketipe model unit (dari pack /zu/amr1)
archer/move_n.ga    ga  3408 B   animasi move (frame timing + event codes)
archer/body.gs      raw 9272 B   mesh (303v/590tri, skin flags)
face/anna.cms       raw 16792 B  portrait CMPR multi-image (texture proof)
bgm/boss1.stm       stm 2.27 MB  musik A (PCM 32k stereo)
bgm/boss2.stm       stm 2.13 MB  musik B (mixing 2 stream + gain)
font/talk.gcf       gcf_atlas 719488 B  font dialog (atlas+metrics)
```
Catatan jujur: SFX asli = `.samp` payload masih P2-blocked (BRV-AUD-003);
bukti "mixing" PoC = 2 stream STM + gain/pan — deviasi dicatat di SCOREBOARD.

## Override aset eksternal (jalur swap IP komersial)
Sebelum lookup FPK, loader cek folder override per nama entri:
`<root>/<nama-entri>` (mis. `archer/tex_4.tpl`; leading `/` diterima).
Root dicek berurutan: env `FE9_OVERRIDE_DIR`, lalu DUA default:
`/storage/emulated/0/Android/data/<pkg>/files/assets_override` dan
`/data/user/0/<pkg>/files/assets_override`. Folder/file tidak ada → perilaku
identik 100% dengan FPK. Ganti aset = taruh file di path yang sama, tanpa
rebuild FPK/kode; `OVERRIDE HIT` dicatat di logcat.
**Pitfall SELinux (API 29+): `adb push` langsung ke Android/data memberi label
`shell_data_file` yang TIDAK terbaca app (gagal senyap fopen) — resep deploy
yang teruji (emulator + HP fisik, tanpa root):**
```
adb push out/rt/anna_override.tpl /data/local/tmp/tex_4.tpl
adb shell "cat /data/local/tmp/tex_4.tpl | run-as com.fe9.poc sh -c \
  'mkdir -p files/assets_override/archer && cat > files/assets_override/archer/tex_4.tpl'"
```
Uji diterima (2026-11-12, emulator API 34 x86_64): potret ANNA 192×128
(aiconv CMPR) menggantikan `archer/tex_4.tpl` → `OVERRIDE HIT` + texture
`192x128` + unit tampil beda total (fe9-captures/override base.png vs hit3.png);
FPK utuh (sha256 lolos; g/gs/ga/stm/gcf tetap dari FPK). Ordo-dimensi juga
membuktikan fix h/w: file 192×128 terbaca 192×128 (loader lama bakal 128×192).

## Konversi aset (PNG → TPL/CMS) — `bin/aiconv.mjs`
- TPL writer: I8 IA8 RGB5A3 RGBA8 C4 C8 CMPR; CMS = LZ77-magic-0x10 dari TPL.
- Verifikasi round-trip (`tex_4.tpl` asli 128×128 CMPR, 6 varian): hasil encode
  di-decode pipeline engine sendiri (texfile.h ≡ tpl2png.mjs); CMPR→CMPR
  meanΔ=0.58 maxΔ=58 (endpoint block ±karena encoder median-cut vs CM
  proprietary) — visual tak terbedakan. auto/rgb5a3 meanΔ 4.5; 4bpp 10.7 (banding
  wajar palet 16 warna).
- LZ77 = klon byte-Exact decoder disc (`texfile.h`): blok 8B [flag][8×LZ77 0x11],
  backref max back=4096 len 3..18, match ≥3, literal 0x00 → write 0x11 (bug
  tersembunyi decoder disc yang direplikasi sengaja).
- Layout header image TPL FE9 (verified byte, 1300+ image): u16 **h dulu**, baru u16 w.
  Loader C++ lama (w-first) sudah diselaraskan — bug laten di tekstur non-persegi.

## Tools
- `bin/fpkpack.mjs` — baker (list → fpk1; bisa dipakai berdiri sendiri).
- `bin/pocbake.mjs` — resolve entri `disc:` dari RVZ + panggil pack().
- `bin/aiconv.mjs` — PNG→TPL/CMS writer + LZ77 (di atas; dipakai jalur override).
- Loader C++ = bagian harness PoC (`poc/`, P2).
