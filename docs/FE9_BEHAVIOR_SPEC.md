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

### BRV-SCT-004 — Katalog native penuh (name→impl→argc→calls) — DECODED
Seluruh native GameBind terdaftar via pola statis `FUN_80013ae8(name, impl, argc)` —
**407 call-site ⇒ 390 native unik** (17 duplikat/nama-hash). Ekstraktor `bin/natcatalog.mjs`
→ `out/data/native_catalog.csv` sorted-by-call-count; **312/390 terlihat dipanggil script disc**
(Top: WaitM 41, InstantDispos 41, MapLoad 41, UnitGetByPID 40, BGMFadeOut 39,
SallySetByGroup 39, InstantSetMapCamera 39, FocusHardWait 38, regist 38, ENVSilentOn 38).
Impl = alamat ELF (pseudocode per fungsi tersedia di `out/ghidra/decAll/`) ⇒ implementasi
engine = translasi per-fungsi, BUKAN riset terbuka lagi.argc native = argcount switch
FUN_80011a5c (0..9). Confidence: Confirmed (registry grep exhaustive 407/407).
Test: T-SCT-NATIVE (replay C00 call-sites vs impl semantik).

### BRV-SCT-005 — Protokol & semantik native top-40 — DECODED
Hasil baca impl (pseudocode decAll) 40 native teratas, pola protokol universal:

**P0 Protokol wait**: native blocking (WaitF/WaitM/*Wait) = self-register timer ke antrean
`DAT_8028d22c` (rec+0x60 = frame tersisa) via `FUN_8000caa0(getSelf)`; return 1=lanjut,
0=sleep; pump `FUN_800138f4` memanggil ulang sampai timer habis. WaitM/WaitF = int frames
(PUSH16 750 = 750 frame; konversi float internal hanya utk skala kecepatan scene). Flag
skrip busy `(-0x7c90)(r13) & 1` skip register saat replay skip.

**P1 Bentuk argumen (verified call-site nyata C01)**: operand di-push sebagai `PUSH8/16/32`
(int, spt `PUSH16 750; CALL WaitM 1` = **WaitM(750 frames)**) atau `PUSH_POOL` (string SJIS
dr name-pool: MapLoad "Map6", regist "<nama script>" — regist mendaftarkan script by-name).
Native float (Focus/SetMapCamera) memakai idiom i2f `2^52+u − bias` (Ghidra) + konstanta
skala runtime (r2−0x6e68/6c) — dekode float raw utk native ini diverifikasi ulang saat
porting (P2); koordinat unit = int grid, float hanya utk kamera/gerakan halus.

**P2 Antrean aksi unit** (`DAT_80296f90` dsb): UnitWalkDir(dir nibble→rec+0x34 bit0-3;
grid-visi DAT_803307a8→+0x33; +0x3c=1), SetCircle(f radius outline), UnitFadeOut/Off,
UnitMoveWait (self-register ke `DAT_80296fcc` selama gerak aktif) = command-queue per-frame
bukan langsung-eksekusi.

**Tabel semantik (name argc ⇒ semantik terverifikasi)**:
MapLoad(1=str/nama map)→BMAP init 0x80330708; InstantDispos(1)→spawn/despawn instan
(0x80105f0c(a,0,0)); Dispos(1)=spawn antre fallback unit aktif; UnitGetByPID(1)→flag
existensi rec+0x198; Focus(2 x,y float)→kamera lerp (return 0 = non-blocking);
InstantFocus(2); UnitFocus(1 unit); SetMapCamera(3 xyz float→rec+4/8/c ++0x70);
InstantSetMapCamera(3)→+4/+8 (+0x10 duplikasi bug-asli port-replika) +FN_800193cc(z)+
apply; SallySetByGroup(1 group); regist(1)=daftarkan skrip ctx ke list 0x803307bc;
BGMPlay(2: ch u8, f32 track)→bank ch stride 0x30 vol=100 +record last; BGMFadeIn(3:
ch,f,flag); BGMFadeOut(2: ch,frames)→fun(vtable+8) isPlaying?+fade; BGMSetVol(3: ch,u8,
s16); BGMQuietOn/Off(0/1); ENVSilentOn/Off(0/1 flag); Create/Set/Start/Delete/SetAt/
SetRot/SetPrec/Past/Copy/Wait EventCamera ( kamera event stack 0x8019fc08-fce);
RectBuild(1=nama→lookup RID_GMAP→RectBases+callback), RectLeave/Kill(1 nama), RectSet/
SetCurve/SetPos/Move/MoveOffset/FadeIn/Out/Delete — Rect = nama→RID→tabel RectBases(.bin);
TalkEvent(1=nama E_Talk intern→dispatch), TalkExist/Resume(0), TalkEventDirect(1);
SetGrid(int %0-100; negatif=restore dari rec+0x33); SetCircle/SetTerrain(3);
MindGetMe/Target/GetMoney(0-1 getter mind ctx), BMGetPhase/BMGetTurn(0 getter BMAP state);
ForceGetCount/First(idx→array 0x802ca264 stride 0xC); AllUnitEscape/UnitEscape(2);
Comeback; TimeiShow; ChapterMoviePlay; DisableSkip/EnableSkip(flag skip bit 0x80013864).
Konfirmasi 4 pola global di atas = prasyarat translasi 390 native engine (P2 PoC script runner).
Confidence: Confirmed (40 impl dibaca penuh; pola P0-P2 konsisten di semua yang dibaca).
Test: T-SCT-PROTO (WaitF(30) replay; MapLoad C00; BGMPlay ch0).

### BRV-EVT-002 — Data chapter C00 (prolog) terbaca penuh
C00 func[11] "Opening0" = setup scene pembuka: MapLoad Map6 → disposisi grup
`Map6_mikata_n`/`Map6_first_n` → Fade(2,500)+FadeWait. func[12] = unit setup:
UnitGetByPID PID_TIAMAT/PID_BOLE/PID_IKE + UnitAddSkill SID_TRI_A; PID_WAYU +
SID_IMMORTAL. Menunjukkan skrip chapter = data posisi-unit+skill+scene — engine wajib
menyajinya (bukan hardcode). Confidence: Confirmed (disassembly). [AM-017][BRV-PROG seed]

### BRV-ANM-003 — Kamera battle per (job, animasi, senjata)
`/xcam/<job>/*.dbx` = teks SJIS satu file per variasi (mis. `fig1_cr2_ax.dbx`), header `#` +
nama kamera JP (probe). Menentukan setup kamera battle utk kombinasi job×anim×wpn;
`/xcam/*/cam.cmp` = pack data kamera. **UPDATE 2026-11-10: SEMUA 437 `.dbx` ter-JSON-kan
(`bin/dbx.mjs`, 708 record) — `class CameraAnime {name,caption,camera,pos[],rot[],dist,time}`
field penuh; JobList = JID_*→model-dir+PID override; Param scene = roomFlag/footEffect/
footSound.** Confidence: Confirmed (field), Partial (semantik nilai — butuh A/B vs Dolphin).
Test: T-C1-ANIM (A/B video). → masuk BRV-BAT saat dipetakan.

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

### BRV-AUD-003 — Arsitektur bank SE `gcfesnd.*` (sequencer internal) — STRUKTUR DECODED, PAYLOAD `.samp` OPEN
Bank = 7-file satu engine SE proprietary (BUKAN CRI/SQS standar): `.stbl` (32B-rec: nama
`SE1_EVT_*` 24B + trailer — event→slot), `.etbl` (32B-rec 2 string null-term: event+part,
mis. `SE2_DUMMY`+`AGON_ROAR2_0`, 70 recs), `.proj` (chain record `[u32 delta-byte][u16 id]
[payload]` sentinel −1; rec id6 = list u32 308×; id≥0x251 = token map 10B
`[ff0a][rate][3c00][streamId][?]` — token = id stream, walk proven via pola `FUN_80258d94/
80258ae8`), `.sdir` (hdr 12B + record 16B×850, tipe di u16[0]: **0x3C00** = fmt
`[rate=0x7d00(32k)][ch 0/1/2][?][u32][u32]`; **0x0000** = stream `[u16 libOff +0x28·i]
[id sequential][0][u32 cumOffset .samp]` — 425 stream, offset kumulatif TERBUKTI (stream1
@78912 ✓); **0x0008** = blok `[id][0][0]+16×s16` di tail — kandidat tabel koef), `.slib`
(hdr chain `[u32 next@0][FF×]` →0xe40→0x2ee0; per-stream sequence `[u16 op,param]` @libOff
stride 40B), `.pool`/`.bin` (tabel rantai; `.bin` u32@0 = size), `.samp` (4.26MB payload).
**PAYLOAD `.samp` BUKAN DSP IIR standar:** byte-header frame (stride 8) lolos uji
scale≤12/idx≤7 2000/2000 DI OFFSET 0 (stream sorted, start@0) TAPI (a) LSQ-fit 16 koef
per-idx konvergen kosong (model IIR tidak konsisten), (b) decode koef stm/standar →
ZCR 3.8k/s (noise), rough 40–72 (audio asli 0.4–3), (c) tak ada 32B koef valid di
sdir/slib/proj (scan sum-pair Q11 = 0 hit). Hipotesis: format delta custom ucode AX FE9
atau interleaving pra-ucode. **P1 dihentikan di sini (gate SE = opsional) → target P2
(butuh: trace ucode AX buffer / capture RAM via Dolphin).** Confidence: Confirmed (struktur
directory/chain/stride), Unknown (encoding payload). Test: T-RE-AUD1 (SE play vs Dolphin).
[AM-048..AM-055 partial]

### BRV-GFX-001 — Binding skin unit & potret berlapis — CONFIRMED (render bukti)
(a) **`.tpl` FE9** = magic `0x0020AF30`; hdr [u32 count][u32 tableOff=0x0C]; entry (u32 imgPtr,
u32 palPtr); imgHdr 16B [u16 h][u16 w][u32 fmt][u32 dataOff][…]. (b) **`/ymu/<jobdir>/tex_<N>.tpl`
N = IDX baris tabel karakter FE8Data** (`out/data/characters.csv` kolom idx): `archer2/tex_195`
= row195 = `ID_LOFA` ✓; job→dir + override via JobList.dbx (`JID_ARCHER: PID_LOFA=arc2`);
IDX sama beda byte per karakter = varian skin per-PID (md5 archer/tex_32 ≠ archer2/tex_32 ✓);
dalam `pack.cmp` = `tex_4/tex_5` (slot base pack; aturan pack-relatif vs eksternal = minor open).
(c) **C4/CMPR GX (terkoreksi 2026-11-10)**: tiled 8×8 = 4 makro TL,TR,BL,BR 8B; warna **BE
RGB565** di `+0`/`+2` (BUG lama: off-by-one `+1` — merusak SEMUA render C4), indeks 2-bit/px
baris `+4..7`; blend 3/8 `(3B+5A)>>3`; **idx3 OPAQUE midpoint** (tanpa transparansi DXT1 —
asset FE9 pakai slot itu sebagai warna; alpha 0 = wajah terensor blok putih). Bukti: skor
smoothness 27.6 vs 87.7 terburuk dari 48 hipotesis + atlas Lofa terbaca (kulit sawo matang
coklat ✓ kanonik). (d) **Potret `/Face` = BERLAPIS**: image0 kanvas 192×128 (orb socket
putih), image1..13 = lapisan ekspresi kecil (16×32 potongan mata dll) — engine = base +
komposit lapisan + mapping ekspresi (kerja P2). Confidence: Confirmed (a–d render).
Test: T-C1-GFX (skin unit & potret vs Dolphin).

### BRV-ANM-004 — Mode skinning submesh `.gs` — DECODED (flag bit0/bit1)
Submesh flags: **bit0 = BLEND** — byte `skin` per vertex = indeks bone skeleton `.g`;
matriks akhir = `animWorlds(g,ga)` (meshview membuktikan posing berjalan — thus "blend-
skinning gap" AM-009 = SUDAH TERJALANI lewat pipeline g/ga; amr1 28 submesh semua bit0 ✓).
**bit1 = PALETTE** — submesh menyimpan array u8 `palette` (mis. archer body `[2,4]` dan array
13..24); byte skin vertex = indeks array tsb → slot tekstur/kedua-gabungan (multi-texture per-
vertex, TCI0-style). arketipe 0x3802=palette, 0x3801/0x380d=blend. Confidence: Confirmed (blend,
render); Partial (palette-semantik detail). Test: T-C1-MODEL battle pose A/B.

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

### BRV-CAP-001 — Saluran input replika demo + resep reproduksi wedge — CONFIRMED
Input disuntik host→Dolphin via `keybd_event` (Win32, `bin/sendkey.ps1`) ke jendela game;
peta tombol GCPadNew (DInput Keyboard Mouse, GC pad 1): **A=`X`, B=`Z`, X=`C`, Y=`S`, Z=`D`,
Start=`RETURN`, L=`Q`, R=`W`, D-pad=`T G F H`, stick=arrows (Modifier=`Shift`), C-stick=`I J K L`**.
Temuan lintas sesi: semua tombol mendarat di layer hotkey QT (F9 screenshot = bukti); layer pad
game menerima SEMUA tombol termasuk stick saat menu/peta; kegagalan awal "stick mati" = salah
interpretasi, bukan jalur rusak. Wajib `SetForegroundWindow` + verifikasi `GetForegroundWindow`
per tap (kunci foreground Windows bisa gagal senyap = tap nyasar; kirim tap ALT bila verifikasi
gagal). Protocol aman dialog: jeda ≥ 4 s antar tap (typewriter). **Durasi press Krusial: di mode peta
(map state) game TIDAK membaca tap pendek — `keybd_event` hold ≥ 800 ms WAJIB agar tombol
terbaca gate dialog/parchment tutorial; di menu/dialog cinematic tap 150-300 ms cukup. Bukti:
parchment `"As you play the game, tutorial windows..."` tak bergerak >6 menit terhadap tap 300 ms
(SendKeys WinForms, durasi ±40 ms juga gagal) lalu LANJUT dalam <8 s setelah hold 800 ms.
Artinya kegagalan-tap selama ini BUKAN wedge pad — ambang polling map-mode.**
**Wedge terdokumentasi (2026-11-11, GFEE01 USA, Dolphin 2609b, jendela 1042×786)**: demo
Demonstration — setelah unit sekutu mengonfirmasi Attack→Iron Sword dengan tooltip script
`"This is the display shown here on the left. It's called the combat-information window."`
muncul: framebuffer statis > 3 menit; pad handler game tampak mati (A/B/X/R/Start tanpa efek),
emulator + hotkey layer hidup (F9 PNG baru). **KOREKSI pasca-temuan hold-800ms: state ini
kemungkinan besar BUKAN wedge, hanya gate yang tak terbaca tap 200 ms; repro ulang wajib
dengan hold ≥800 ms sebelum diklasifikasi wedge.** Hotkey save F1/F2 default TIDAK membuat
berkas di `User/StateSaves` (kosong).
Signature observasi = kriteria freeze governance: layar beku, logika beku, shell hidup —
tapi klasifikasi final tertunda sampai replay memakai hold 800 ms.
Repro untuk harness: sequence input manusia terekam = boot disc → lewati opening (tap A per 4 s)
→ New Game → skip prologue → demo Demonstration → gerak unit ke musuh (stick, screenshot di
antara gerak dan confirm) → menu action → Attack → tooltip combat-info muncul → **spam A ±40×
interval 3 s pada tooltip = Wedge** (hipotesis terkuat: script demo mengonsumsi tap ekstra lalu
menunggu event yang tak pernah datang; status: Strong, belum Confirmed sampai trace ucode).
Konsekuensi engine: parser script port WAJIB mengabaikan input tak terduga di sela message,
bukan mengonsumsinya. Catatan demo: window preview tempur menampilkan pasangan
[Ally|Enemy]: HP 19/19, Mt 4/4, Hit 100/91, Crit 2/2 + senjata Iron/Slim/Steel Sword (HP 19
Myrmidon sekutu, durability 46/35/35) — seed validasi BRV-BTL damage.
Test: T-STA-FRZ replay sekuens di harness; T-C1-UI angka damage vs tangkapan. [AM-050]

**Update sesi 2026-11-11 (grind 100-loop, Turn 7 Demonstration)**: di peta, `B` (cancel),
`T G F H` (kursor bergerak, kamera pan — DOWN d-pad terkonfirmasi hidup; stick bawah mati),
A/B pada MENU aksi & dialog = responsif dengan hold 800 ms. **TAPI A setelah select unit
(mode "arrow" aktif, move-range tampil) tidak pernah mengonfirmasi move** — siklus
`B,A,A,G,A` berulang 100× (11 mnt) tanpa turn ever advance (101 frame identik Turn 7).
Hipotesis: konfirmasi move butuh C-stick/press-analog atau gate "arrow settled" yang tak
terpenuhi tap diskrit; WEDGE-LIKE kelas baru — kandidat workaround: hold A > 1500 ms,
A dua-kali-jarak-3s, atau Z (tombol tidak terpakai FE9) sbg alternate-confirm.
Status lama Open -> **RESOLVED sesi 2026-11-11 (battle Ch1 real-time, Turn 8-9)**:
konfirmasi move via A=X `keybd_event` BEKERJA; yang terjadi sebelumnya = ambang durasi +
antrean input, bukan mode mati. Atang ambang terkalibrasi ulang:
- **ACTION (A/B/X/Y/Z/Start): hold >= 2100 ms WAJIB** — 150/450/900/1500 ms semua TERBUKTI
  lost di peta (uji X di 4 state); 2100 ms selalu mendarat (deselect, menu, Wait, move-confirm).
- **D-pad T/G/F/H: 900-1000 ms cukup** (kursor peta + menunya responsif; panah MAIN-STICK
  tetap mati total - jangan dipakai).
- **Input ter-ANTRE selama animasi**: burst 3×2100 saat animasi move-range -> game memproses
  confirm BEBERAPA DETIK kemudian (Ike ternyata pindah 2 tile lalu menu aksi muncul). Jangan
  spam ulang sebelum screenshot konfirmasi state (double-confirm risk = aksi salah).
- **Dead-state scripted**: kursor + semua tombol mati sementara marker panah boss kuning
  berputar-rotasi = sekuens event (boss mendekati unit / dialog AI "Yaaaawn..." nunggu A);
  BUKAN wedge — tunggu; fasa musuh lanjut sendiri setelah dialog di-A.
Peta fakta battle Ch1 (Turn 8-9, GFEE01): misi "Defeat Boss"; Ike = kelas **RANGER Lv1**
HP19/19, senjata Trainer: **Atk 11, Hit 173, Crit 0, Ddg 6**, EXP 0; jangkauan Serang Trainer
= tidak ada target pada jarak yang ada (boss + bandit axe di luar range 1-2); menu aksi =
[Items|Wait] saja tanpa target; terrain panel [nama|Avo/Def/MDf]; panah kuning = pointer
boss-world (bukan musuh terseleksi); fasa musuh punya dialog idle AI per unit.
Harness KINI BISA main-peta penuh (gerak+menu+fasa). Capture animasi tempur masih menunggu
engagement (boss scripted mendekat) — burst 70 s aktif. [AM-055]

### BRV-SCT-006 — Semantik native batch-2 (unit/map/camera/env) — CONFIRMED (pseudocode)
Dari `out/ghidra/decAll/FUN_*.c` (impl ptr katalog), argc = switch `FUN_80011a5c`:
- **UnitGetByPID** (1): `FUN_80016bdc(pid)` lookup unit global; NOT-found ⇒ 0; ketemu ⇒
  **byte unit+0x198** (flag eksistensi/status, BUKAN pointer). Script pakai sbg truthy.
- **MapLoad** (1): `FUN_8014c7c0(BMAP=&DAT_80330708, name)` → intern nama peta
  (`map06_fort` dkk, cocok `bf_map.bin`), load model g/gs; lalu `FUN_80013864()` flush scene.
- **InstantDispos** (1): `FUN_80105f0c(pid,0,0)` = spawn instant dispos sesuai rekaman
  posisi di `dispos_c.bin` (arg = nama PID_/IID_ pool).
- **SallySetByGroup** (1): `FUN_8014cf48(BMAP, group)` = aktifkan sally per nomor grup.
- **InstantSetMapCamera** (3): arg x,y,z = **fixed-point** — `(0x43300000_full | (u32^0x80000000))
  − basis` = konversi int→float klasik; x→cam+4/+0x10, y→+8/+0x14, z→`FUN_800193cc(f)`
  (zoom/tinggi); lalu `FUN_800187f0()` commit kamera. Port: parse arg kamera sbg 0x10000-scale.
- **ENVSilentOn** (1): clamp ≥0 → `FUN_801455b8(n)` level senyap ambient (kanal/level n).
Kesamaan pola: argc1 "setter" = `fn(GLOBAL, arg)` satu baris; return 1 = sukses (result slot
mesin script). Global BMAP `0x80330708` dipakai MapLoad+SallySet+RectBuild (SCT-005) ⇒ SATU
konteks peta. Confidence: Strong (pseudocode langsung, tanpa runtime trace).
Test: engine — MapLoad("map06_fort") + InstantSetMapCamera(bf_map.bin f32 × 0x10000) harus
reproduce kamera intro arena (75c50c6).
**Batch-3 kamera (impl `FUN_800bad9c/800bae4c/800babfc`)**: Focus(x,y)/InstantFocus(x,y) =
fixed-point kord → `FUN_8014cbd8(fx,fy,BMAP)`; Focus di-resume tiap frame (flag&1 ⇒ commit
`FUN_8001885c`) = **gerak kamera gradual via re-exec script per-frame**; InstantFocus = lompat
langsung. FocusHardWait(0) = **koroutine 2-fase**: fase-1 tunggu settle→snapshot frame→
pasang timer + gate `FUN_80013874`→return 1; fase-2 (resume, flag&1) commit kamera→return 0.
Pola "HardWait" = blocking sleep lewat pump-flag, identik semantics SCT-005 wait-queue. [AM-051]

### BRV-SCT-007 — dispos_c.bin tail registry DECODED; loader universal .bin terbukti
Parser aktual = `FUN_8003fccc→FUN_8003f87c(name,0)` (chain: boot `FUN_80005f04`→
`FUN_8002d548`→load zmap/always/dispos.cmp(mode 8)+dispos_c.bin(mode 0)+zmap/common.cmp).
Struktur file (semua BE): hdr [size=0x3E97][tailOff=0x3118][cA=639][cB=36][0] lalu payload
[0x20..0x2A54) u32[0x20]=0x2A54 len + rekaman biner dengan string inline (PID_/IID_/SEQ_/
MTYPE_) dan u32-ref ke pool-A (0x2A54..0x3118). TAIL: [pool-A strings "ID_TEMP_ON_DIE",
"SID_TURNREGENE"][tabel-639-u32 = INDEKS SORTED 631 offset string → payload (gap dominan
72×82 + 20×64 = jarak antar string dlm rekaman)][B-table 36 pasang (key→offset rekaman,
nameOff→pool-B @0x3C54 = B0+cB*8+0x20)][pool-B 36 nama script].
**pool-B = registry script per-karakter**: always_T_03/05/07/10/15, always_c16, always_date_c,
always_dbg_12, always_dbg_all_c, always_dbg_mik_02..37 (Micaiah), always_kariru, always_lete
(Lethe), always_mordy (Mordehaus), always_rarugo, always_saza (Sica) — **nama kode internal
dev** terkonfirmasi; tiap B-pair mengikat satu rekaman unit payload ↔ script always_* via
intern `FUN_8000d3ec`. Loader me-relok pointer-slot list (A) saat first-parse per nama file
(cache-flag 7-byte nama di hdr+0x18). Grammar rekaman payload (120B/stride tak konsisten;
S=72 refHits 94/147 terbaik) = OPEN → perlu consumer runtime (FUN_80016498 stride-0x280).
Confidence: Strong (parser pseudocode + verifikasi byte-exact 631/639 + B0 presisi).
**UPDATE grammar payload CLOSED (0x6C)**: payload [0x24..0x2A54) = 10.800 B = **tepat
100 record × sizeof 0x6C** (cocok `FE9_AI_NOTES.md`/StanH `dispos`); 100/100 record
ber-u32-ref ke pool-A (0x2A54..0x3118) = validasi independent dua sumber. "overlap" lama
= stride salah. Field x/y +5C/+5D = 0 di `zmap/always` layer (bukan placement peta —
file ini layer always, wajar); semantik per-byte final = gate M-C6 decomp lane
(`DisposFileDecode @0x80107B54` byte-match). Status: Partial→**Decoded (structure)**,
field-semantics menunggu M-C6. [AM-052]



### BRV-BTL-002 — Tabel inti `zdbx/zdbx/` DECODED: timing/animasi tempur + fisika proyektil + tabel nama — CONFIRMED (data)
Sumber: `//zdbx.cmp` (133.524 B) → 449 entri; 11 tabel inti `zdbx/zdbx/` sudah TIDAK ikut
sweep loose 437 lalu. Semua ter-parse `bin/dbx.mjs` → `out/dbx_core/*.json`.

- **param.dbx = 1 record 99 key SJIS-commented = spesifikasi lengkap penyajian tempur**
  (satuan frame [INFERENCE: 60fps GC]): `WaitInit=120` (2 s intro tempur), `WaitDie1=20`,
  `WaitLvUp1=20`, `WaitExp=30`, `WaitShoot1=20`, `WaitAtk2=20` (jeda pra-counter 2005-01-20);
  quake kamera: atk 0.5 / crit & skill 1.8 / magic 1.2 / armor-walk 0.2; gauge HP:
  scale-num 2.5@0.4, shrink 0.16, shake normal 4.0@1.0×60f, shake crit 8.0@1.2×120f,
  flash 255@−8.4×74f, flicker 0.04; damage-recoil 3.0@0.4 + flash 204@−12.4×90f;
  avoid retreat 0.8@1.0; **finishDist 24.0** (menembus lawan saat finish);
  **proyektil**: knife 2.8; panah 6.0; tombak balistik (v0 5.0, g −0.05, spin 0.5);
  **kapak bumerang** (v0 4.5, accel −0.07, rot 70/f, wave 3.2@−0.12, kembali 1.0);
  hitDist 2.8; blend animasi: default 15f, stand 20f, run 5f (input BRV-ANM);
  magic fade 60@20f; fade dialog boss/mati/levelup ±0.06; jump-flag hewan 51/52/53/54;
  sword-flash flag 58/59 → `EID_LENSFLARES2/1`; kamera samping +40, zoom 60..500;
  InitPos L/R ±20.0; wallDist 90; AttackDistScale 1.0.
- **skill.dbx** = nama skill SJIS → id trigger (`奥義・流星→EID_K_SP_SSTAR`, 太陽→SP_SUN,
  月光→SP_MOONL, カウンター→EID_K_COUNTER, 見切り…; `混沌/デビル/勇者→"no"` = non-EID).
- **classList.dbx** = kelas SJIS → kode model (`重歩兵→amr1`, `ソードマスター(女)→smf1`;
  cocok folder `out/amr1x`), **jobList.dbx** = `JID_* → model(+PID override)` 
  (`JID_ARMOR:"PID_CHAP=amr2"`), **weaponList.dbx** = nama → `IID_*` (ラグネル→RAGNELL),
  **effectList.dbx** = efek → `EID_*`, **camera.dbx** = 6 preset cam tempur
  (cam0 eyeCenter, camCharaL0 dist50 offs[0,7,0] rot[6,110,0]), **battle/system.dbx** =
  daftar layer 2D (lensflare/Window/Caption/desktop), bgList/lensflare.dbx = aset.

- **VALIDASI VIDEO OFFLINE (longplay NTSC `1B95v-cnXVo`, pipeline yt-dlp+ffmpeg,
  2026-11-11)** — oracle perilaku non-emulator, segmen Prologue (02:00-09:40):
  - UI tempur: bar HP dua-sisi + label + **ikon senjata & durabilitas** ("Trainer
    96/114"); TIDAK ADA angka damage mengambang — damage = delta bar per exchange
    (konsisten param.dbx `gauge-*`/`dmg-*` tanpa float-damage).
  - **Popup EXP terlihat: `Exp 11/11` → sparkle putih di kepala pasca-combat** =
    weapon-rank exp (baris exp kecil) + unit-exp; urutan exchange:
    player-hit → (counter WaitAtk2=20f) → gauge shake → Exp popup → sparkle →
    (LvUp bila cukup: `Levelling Up...` banner → **Ranger Lv2: HP+20 (19→39),
    Sp+3, Sk+6, Spd+3, Lck+1, Def+1, Res+0** → "Level Up!" flash).
  - Sample damage prologue: Ike(Trainer) vs Boyd(Practice Axe) 19/18→19/9 =
    9 dmg/hit-trainer terakumulasi; Greil vs Myrm 11/7 delta — cocok kelas
    Mt4-def; validasi formula penuh tetap butuh frame-per-exchange (P2 opsional,
    formula sudah punya Direct Combat CAP1 + zdbx Wait*).
  - Banner **ENEMY PHASE** full-screen merah (fade-in/out ~2s) = transisi fasa;
    menu aksi peta terbukti [Attack|Items|Wall|...] + **Voluntary (retreat)** +
    [Use Item|Skills] saat item; **option: Combat Window Default, Message Speed**.
  - Save: format entri `Prologue: Mercenaries` / `Ch.N: <judul>` + timestamp main.
  - Demo sesi emulator = Ch1-flavored tutorial; prologue longplay = map nyata
    (Ike vs Boyd + Greil vs Myrm) — dua-duanya valid untuk BRV-BTL.
- Gapatis: rumus damage angka TETAP butuh decomp (gForgeData ×5/×3) — tabel ini menutup
  *timing, reaksi, kamera, fisik, pemetaan nama* tanpa capture. [AM-053]

### BRV-SCT-008 — dispos per-peta (zmap/bmapNN) : container pack + grid record per-varian — PARTIAL (structure CONFIRMED)
- `//zmap/bmapNN/dispos.cmp` (LZ77) → unpack = container **`pack`** berisi
  `dispos_c/h/m/n.bin` (4 varian; bmap01: 905/586/566/566 B — m≡n ukuran). 58 folder bmap + `always`.
- Decoder resmi = **`FUN_80107b54` (= `DisposFileDecode` dtk-verified)** baca stream per-record:
  **20×u32 + 26×u8 + align(+3&~3) = 106..108 B** → cocok presisi varian `always` (payload
  10.800 = 100×108, semua record align penuh). Field terpetakan dari pseudocode:
  `+0x00/+0x04` u32, `+0x08` u32 (skip), `+0x10..2F` 8×u32, `+0x30..37` 8 u8 (slot item),
  `+0x38..47` **4 kata AI** (→UnitInfo+38/3C/40/44, StanH ✓), `+0x48..5B` 5×u32,
  `+0x5C=x, +0x5D=y` (u8 koord), `+0x5E..66` u8 flag, `+0x67` runtime.
- Varian bmap pakai **grid 0x70** (terbukti independen: triplet konstanta @+0x50/54/58
  berjarak 0x70 ×4; 4 record×0x70 = 0x244 = awal pool string pas). bmap01_c = **4 unit**
  (pool: PID_BOLE_MAP1, PID_GREIL_MAP1, PID_IKE, PID_MIST = cast masuk Bab 1 ✓) + string
  region pra-pool: `IID_PRACTICESWORD, JID_CLERIC/F, JID_FIGHTER, JID_HERO_G, JID_RANGER,
  MTYPE_NORMAL` + timestamp `2005-02-23`.
- OPEN (serahkan ke M-C6 decomp lane, mereka pegang decoder + UnitLoadUnitInfo 0x6C4):
  semantik u32 kecil (~470..600) di b/c/f10 — kandidat indeks intern registry gabung
  always+bmap (uji indeks-langsung ke pool-A 136 string gagal, jadi tabel lain);
  plus kenapa header u16 record-count tak cocok jumlah grid.
- Semua 58 bmap ter-parse parser generik `bin/dispos.mjs` (NEW, mirror fe9-port/bin) —
  CSV spawn/pool; auto-deteksi grid (108/112) + region u32 sebelum string (anchor
  timestamp + scan + kandidat align). Tervalidasi: always 100×108; bmap01 4×112;
  bmap02 3; bmap05 6; bmap12 **107 unit**; bmap20 49 (semua stride 108 varian umum). [AM-054]

### BRV-BTL-003 — Harvest video Ch1+Ch2: forecast tempur, triangle damage, rescue UI — CONFIRMED (observasi)
Sumber: longplay `1B95v-cnXVo` segmen `fe9_ch1.mp4` (video 09:25-22:35) + `fe9_ch2.mp4`
(22:35-35:00), 480p, frame-accurate via ffmpeg fps=4 montase. Semua angka = pembacaan
layar (OCR-vision), konsisten silang antar-frame.

- **Combat forecast panel lengkap: `ATK 13 | Hit 92 | Ava 10 | Crt 3 | Dmg 5 | Recv 1`**
  + Exp bar per-unit — 6 field itu = output formula yang engine port WAJIB hasilkan identik.
- **Triangle damage TERUKUR**: Oscar Iron Lance HP26 vs Bandit Iron Sword HP18:
  lance 1 hit = **10 dmg** (18→8); counter sword = **1 dmg** (26→25) — sword vs lance
  = disadvantaged (atk halved, Mt kecil) → konfirmasi teks tutorial Ch1 VERBATIM:
  *"Sword vs axe: advantage. Axe vs lance: advantage. Lance vs sword: advantage."*
- **Durability → 0**: bar petang "14/0 Iron Axe" tampil + combat tetap jalan
  (17/26, 16/0, 13/0, 0/26) = senjata patah = **fight bare-handed** (bukan auto-equip).
- **HUD unit-info panel (map)**: `nama | Atk | HP cur/max | Def? | senjata` live +
  toggle "Car Stat"/"Cursor Info"; `Plain Avo 0` / `Th... 0/14` terrain readout kiri-bawah.
- **Roda radial peta** (tombol hint): pusat **A**; sekeliling **X, Y, L, R, Z, Start, stick**;
  label: **Map Menu | Status | Next Unit | Move | Cond | End | Back**.
  **Y = END TURN instant** ("End" hijau) — shortcut belum ada di emulator (Y→key `S`).
- **Menu aksi peta** lengkap: [Attack|Items|Wall|Wait] tempur; **[Rescue|Items|Trade|Wait]**
  unit bebas; **[Staff|Show|Item|Trade|Wait]** pemegang staff; **Voluntary** = retreat.
- **Transisi fase**: banner **ENEMY PHASE** (merah) / **PLAYER PHASE** (biru) full-screen
  fade ~2 dtk; AI musuh **converging**: 3 brigand bergerak mengelilingi Oscar (target
  lingkaran biru), multi-attack dari 4 arah fasa musuh.
- **Death**: unit kalah = efek **hijau "MIA!"** + partikel (bukan sprite jatuh) + "MIA" text.
- **Rescue**: ikon **tangan hijau** di unit rescued; Mila NPC punya panel status sendiri
  (Atk 15, HP 22/10 tampil saat rescuer info dibuka).
- **Level-up**: popup `Ranger Lv 3` **HP22 Str6 Sk5? Spd? Lck5 Def5 Res1 Mov5** + animasi
  panah-ke-naik + angka hijau (data RNG sample: Lv2→HP22 dari Lv2 HP21).
- **Roster counter**: teks "You have used 5 of your 6 units" (Ch2 = limited-unit).
- **Cannon**: objek peta bertuliskan "Cannon" + **panah rute putih lebar** preview move
  (BRV-MAP: path arrow = polyline dengan belokan 90°).
- Implikasi P2: forecast 6-field + triangle halving + durability-0 bare-hand = tiga
  perilaku yang selama ini belum punya bukti visual; kini punya frame referensi di
  `fe9-re/out/yt/fx_*.png` (work-dir, tidak di-commit). [AM-056]
