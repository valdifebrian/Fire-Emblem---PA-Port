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

## Tools
- `bin/fpkpack.mjs` — baker (list → fpk1; bisa dipakai berdiri sendiri).
- `bin/pocbake.mjs` — resolve entri `disc:` dari RVZ + panggil pack().
- Loader C++ = bagian harness PoC (`poc/`, P2).
