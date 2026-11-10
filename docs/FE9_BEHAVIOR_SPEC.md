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
`/Scripts/C??.cmb` = magic `"cmb\0"` + selfname ASCII + header LE: pool-off @0x24,
entries-off @0x28 (u32 stride4 terminasi-0). Record 0x14 field: name-ptr u32 (0=anonim,
nama inline tepat setelah field), code-ptr u32, +08 u16, +0a u16, +0c flag u8,
+0d argc i8, +0e u8, +0f u8, +0x10 id u16, +0x12 nLocals u16; lalu nama inline (opsional)
dan stream bytecode sampai batas record berikutnya (ptr terdekat). Engine BE PPC me-reverse
pointer u32 + += base saat load (FUN_800140b4). Bukti: `bin/cmb.mjs` parse 43/43 script —
2075/2075 fungsi ter-disassemble bersih, SEMUA berakhir RET (0x39); C00 walkthrough
(`dis out/raw/C00.cmb 11` = "Opening0": MapLoad("Map6")→InstantDispos→Fade(2,500)→FadeWait).
Confidence: Confirmed. Test: T-RE-SCRIPT1/2, T-SCT-VM. [AM-017]

### BRV-SCT-001 — Model frame VM script
Interpreter = switch opcode 1 byte (FUN_80011edc, 902 baris, 65 case + default-err).
Frame: +0x04, +0x08 record-ptr, +0x0C PC(u8 stream), +0x0D i8 (argc callee), +0x10 globals
window base, +0x12 nLocals(i16), +0x14 LP (locals u32[]), +0x18 SP (u32 push/pop),
+0x1C yield flag. Global array G base = slot global (r13-0x7c08); pump (FUN_800138f4)
push frame: PC=rec+0x04, window = G + u16(rec+0x20)*4 (endianness varian bertopeng nama
[INFERENCE]), locals nol-kan dari rec+0xd*4 s/d rec+0x12*4. RET restore frame; PC==0 ⇒
exit script. Error code via FUN_800142b0: 0=ok, 5=named-func-not-found, 8=locals-overflow.
Bukti: decompile `out/ghidra/dec/FUN_80011edc.c`, `FUN_800138f4.c`.
Confidence: Confirmed (switch bytecode langsung). Test: T-SCT-VM.

### BRV-SCT-002 — Set opcode (65 ops, operand BE)
Tabel lengkap di komentar `bin/cmb.mjs`; ringkas: 0x01–0x18 akses locals/globals
(window-indexed: push/load/deref/add-idx; operand u8 / u16-BE), 0x19/1a/1b push imm
(i8/i16-BE/i32-BE), 0x1c/1d/1e push pool+off→cstring (i8/u16/u32-BE), 0x1f DUP, 0x20 DROP,
0x21 OVER, 0x22–0x34 aritmatika/compare/shift/bitwise (0x31/0x33 varian signed [Partial]),
0x35/0x36 STR_EQ/NE, 0x37 CALL_LOCAL u8 (indeks entries-array), 0x38 CALL u16BE pool-off
+ u8 argclean (call by NAME via hash global — native ATAU script), 0x39 RET,
0x3a–0x3e JMP/JZ/JNZ rel i16-BE (0x3c/0x3e push 1/0 saat branch), 0x3f YIELD, 0x40 SKIP4,
0x41 CALLN u8 argcount 1..9 (target ptr dari stack — jalur GameBind). Default ⇒ error.
Statistik 43 script: semua 65 opcode terpakai; 0 DB non-padding di dalam code (112 byte
= string trailing setelah RET). Bukti: `FUN_80011edc.c` per-case + `cmb dis` bersih 100%.
Confidence: Confirmed (0x31/0x33 Partial). Test: T-SCT-VM.

### BRV-SCT-003 — Interning nama & native GameBind
Saat load (FUN_800140b4): tiap record di-intern ke hash global 509 bucket (FUN_8000d3ec,
key = C-string) — registrasi ulang oleh CmSleepScript (tulis 'S'@block+3, re-init);
call-by-name (FUN_80013a90→FUN_8000d354) miss ⇒ error 5. Native terikat via registry
GameBind (FUN_801262f0: CmLoadScript/CmSleepScript/ArcLoad/TutorialCall dll).
Katalog native hasil `cmb all`: **404 nama** unik (Fade, FadeWait, set, MapLoad,
UnitGetByPID, UnitAddSkill, InstantDispos, SallySetByGroup, VisitIn/Out, MindGetItem,
NetuzoInit, IID_*/PID_*/SID_* pool string). Test: T-SCT-VM (replay C00 Opening0).
Confidence: Confirmed.

### BRV-EVT-002 — Data chapter C00 (prolog) terbaca penuh
C00 func[11] "Opening0" = setup scene pembuka: MapLoad Map6 → disposisi grup
`Map6_mikata_n`/`Map6_first_n` → Fade(2,500)+FadeWait. func[12] = unit setup:
UnitGetByPID PID_TIAMAT/PID_BOLE/PID_IKE + UnitAddSkill SID_TRI_A; PID_WAYU +
SID_IMMORTAL. Menunjukkan skrip chapter = data posisi-unit+skill+scene — engine wajib
menyajinya (bukan hardcode). Confidence: Confirmed (disassembly). [AM-017][BRV-PROG seed]

### BRV-ANM-003 — Kamera battle per (job, animasi, senjata)
`/xcam/<job>/*.dbx` = teks SJIS satu file per variasi (mis. `fig1_cr2_ax.dbx`), header `#` +
nama kamera JP (probe). Menentukan setup kamera battle utk kombinasi job×anim×wpn;
`/xcam/*/cam.cmp` = pack data kamera. Confidence: Confirmed (eksistensi/granularitas),
Partial (semantik field). Test: T-C1-ANIM (A/B video). → masuk BRV-BAT saat dipetakan.

### BRV-AUD-001 — Pemetaan SFX per event
`/Sound/gcfesnd.stbl` memuat string `SE1_EVT_FOREST1` dst. (probe) = tabel event→SFX;
bank = `gcfesnd.{slib,sdir,pool,proj,samp}` + `.stm` (STM-headerless, `_lp`=loop).
Confidence: Confirmed (eksistensi), Unknown (format). Test: T-RE-AUD1, T-AUD-MIX.
UPDATE 2026-11-10: container `.stm` DIKUNCI → lihat BRV-AUD-002; bank `gcfesnd.*` masih
Investigating (`.sdir` stride16 `[?][len][@?]`, `.samp` 6.7MB block, `.etbl` nama `SE2_*`).

### BRV-AUD-002 — `.stm` = GC-DSP ADPCM stereo (dua mono terkonkatenasi) — DECODED
Kontainer (semua BE): `u16@2` sample-rate (0x7d00=32000); `u32@4`=2 (ch); `u32@8` = BYTES
per stream mono (total data = 2×, L di `0x8100`, R di `0x8100+len`); `u32@0xc` = loop point
dalam NIBBLE (konversi vgmstream `nib/16*14 + max(0,nib%16−2)`); `0x40/0x44` = record loop
(intro-vs-`_lp`); **`0x5c..0x7f` = 8 pasang s16 = tabel koefisien IIR** (idx≤7).
Frame 8B: byte0 = `[idx4|scale4]` (skala = `1<<nibble_bawah`, teramati 2..11; idx 0..7),
byte1..7 = 14 nibble (nibble TINGGI dulu per byte), 14 sample/frame.
Rumus per sample (predictor, BUKAN integrator — identitas vgmstream `decode_ngc_dsp`/DSP_STM):
`sample = clamp16(((nib_signed*scale)<<11) + 1024 + c0*hist1 + c1*hist2) >> 11; hist2=hist1; hist1=sample`.
Konsistensi: `0x1dd7a0 (field@0x40) = frames×14` exact; jumlah frame = bytes/8 exact.
Game-side: `FUN_800b080c` buka `"%s%s.stm"` via `FUN_800b4a84` dua kali (intro+loop record) —
decode di ucode DSP AX (tak ada decoder CPU di 8110 fungsi; grep `>>4`=0) → engine Android
WAJIB decode CPU-side (vgmstream-style) lalu mix. Bukti decode: boss1/ashnard_lp/classchg —
roughness 0.43–0.87 vs noise 1.35–1.55, L≈R pada `_lp`, energi bass 42% (noise 6%),
loop point kontinu (step 605–1377 ≈ RMS lokal), durasi 47–61 s. Confidence: Confirmed.
Test: T-RE-AUD1 (dengar vs Dolphin), T-AUD-MIX. [AM-015]

### BRV-FNT-001 — `.gcf` = font metrics + sheet tekstur GX I4 polos — DECODED
Header (semua BE): **u32@0x00 = magic 0x20**; u16@0x08 ascent / @0x0a descent / @0x0c lineH
(fe 29/3, talk 22/5, alpha 28/6); u32@0x14 = BYTES per sheet (0x2000 = 128×128 I4,
0x8000 = 256×256 I4); u32@0x18 = jumlah sheet (fe 3, talk/system 20, alpha/biggana 1);
u32@0x1c = lebar=tinggi sheet px; u16@0x22 = offset metrics; u32@0x24 = END metrics = awal
region glyph; u32@0x28 = bytes region = sheet-count×sheet-bytes exact.
Record metrics 16 B di @0x22 stride 16: `[u16 code (ASCII/SJIS)][u16 sheet][u16 x][u16 y]
[u8 w][u8 h][s8 dx][u8 dy][u8 adv][3×0]` — `x,y` = ink top-left LANGSUNG dalam koordinat
sheet; trailing `code==0` = padding. Trailing records.
Sheet = **tekstur GX I4 (GX_TL_NONE, tanpa kompresi)**: block 8×8, pitch = lebar/8 block,
baris kontigu dalam block, nibble TINGGI = piksel genap:
`byte = region + sheet·sb + ((y>>3)·(W/8) + (x>>3))·32 + (y&7)·4 + ((x&7)>>1)`;
`lum = nibble·17`. Upload = blok `[u8 0x10][u24]` + LZ77 tanpa tag stream (`cmp.mjs
lzStream(buf,4)`); hasil = .gcf utuh (system.cms = varian font, 3988 glyph ≠ system.gcf 4005).
Verifikasi render (tool `bin/gcf.mjs`, PGM+CSV per sheet): fe_font `0 1 7 9 ？ a` (sheet
dipilih field[2]: `1`/`7`/`9` sheet1, `！` sheet2); talk `1 A 7` (sheet15/13/18 — field[2]
= indeks sheet s.d. 19 ✓); alpha `0 7 &` = elips/7/ampersand bersih; bigkana = font ASCII
BOLD 95 glyph (kana diremap engine dari kode script, bukan SJIS); system.gcf + system.cms
= 4005/3988 glyph SJIS penuh (kanji/kana 16×17 terverifikasi `0x82a0`).
Catatan: `fe_font.gcf` = font angka/lowercase (damage/stats) — record A–Z = placeholder
junk (42×28, duplikat posisi) — ABAIKAN. Engine Android: render via LUT 16→255 (n·17)
ke atlas GL; atlas + CSV = out/gcf. Confidence: Confirmed (6/6 file render glif terbaca).
Test: T-C1-UI teks layar + angka damage vs Dolphin. [AM-024]

### BRV-SYS-001 — Varian aset region/bahasa
Model pack battle punya varian sufiks `_ja/_no` (`amr1_ja.pak`, `kni2_no.pak`) — runtime memilih
set per region. Port WAJIB meniru pemilihan varian disc USA sebelum bake (bukan asal pilih).
Confidence: Strong (pola nama + disc USA). Test: T-CNT-CH varian check.

