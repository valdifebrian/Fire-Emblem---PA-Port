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
0x21 STORE_IND (pop val, pop ptr, *ptr=val, push val; label lama "OVER" salah),
0x22–0x34 aritmatika/compare/shift/bitwise (0x31/0x33 varian signed [Partial]),
0x35/0x36 STR_EQ/NE, 0x37 CALL_LOCAL u8 (indeks entries-array), 0x38 CALL u16BE pool-off
+ u8 argclean (call by NAME via hash global — native ATAU script), 0x39 RET,
0x3a–0x3e JMP rel i16-BE; branch: 0x3b=JNZ, 0x3c=JNZ_PUSH1 (push 1 bila branchTaken),
0x3d=JZ, 0x3e=JZ_PUSH0 (push 0 bila branchTaken — nilai pop hilang bila tak branch);
0x3f YIELD, 0x40 SKIP4 (+4 pc, operand 4B di-skip),
0x41 LOGF u8 n = DEBUG-printf: pop n arg → FUN_800056cc (stub retail `return 0`) — TANPA
push hasil; BUKAN dispatch native (koreksi dari case 0x41 FUN_80011edc; marga "CALLN"
salah). Default ⇒ error.
Statistik 43 script: semua 65 opcode terpakai; 0 DB non-padding di dalam code (112 byte
= string trailing setelah RET). Bukti: `FUN_80011edc.c` per-case + `cmb dis` bersih 100%.
Confidence: Confirmed (0x31/0x33 Partial). Test: T-SCT-VM.

### BRV-SCT-002b — Temuan replay VM (harness vmtest, 43 .cmb) — DECODED
Replay penuh mengoreksi 6 semantik yang sebelumnya salah di VM port:
1. **Hasil native argc-0 TETAP dip-push** (slot top-of-stack setelah call = hasil; bug
   lama: push hanya bila argc>0 ⇒ spin `while(isFading)` membaca garbage lama).
2. **0x41 = LOGF debug-printf** (pop n, FUN_800056cc stub, tanpa push) — BUKAN dispatch.
3. **0x3e = JZ_PUSH0** (branch+push 0 saat v==0; saat tak branch nilai hilang) — 0x3b/3c/3d
   terverifikasi cocok. Branch target = pc-setelah-operand + i16 − 2.
4. **RET lintas-file**: save-frame slot+1 menyimpan POINTER rec (identitas file). Port
   harus men-tag (fileIx<<20|rec+1); tanpa tag, return ke file pemanggil salah bytes
   (gejala: opcode liar 0x6c = byte string pool).
5. **CALL script & RET stack model** (FUN_80011d20 script-path + case 0x39): ARGUMEN TIDAK
   dipop — lbase callee = posisi arg pertama (argumen jadi L[0..argc-1]), L[argc..nLoc)
   di-zero, 4 slot save (pc,rec,gbase,lbase) di atas locals; **RET: pop hasil, sp = lbase
   MILIK CALLEE SENDIRI (slot arg0), hasil menimpa arg0** (bukan saved lbase pemanggil —
   salah baca byte-ptr vs slot di `SP=lbase+nLoc*4+0x10` lalu pop-4 lalu −nLoc*4 ⇒ gejala:
   sp longgar/underflow +0x21 OVER setelah helper argc<2 → UND @UnitSetPosDirByPID).
6. **PUSH_POOL interning**: satu salinan RAM per (file,poolOff) — push berulang dalam loop
   wajib reuse alamat (RAM 16MB habis kalau alloc per-push; gejala "ram exhausted").
Gate spin startup: `FadeWait` = `while(isFading) YIELD` (0x3d); `Fade(mode,time)` args=
(mode u8, durasi u16 frame — call-site: (2,500),(6,1000)); isFading=1 selama aktif
[INFERENCE arah: loop exit saat 0]. Wrapper script startup (UnitSetPosDirByPID dkk)
memanggil helper Lokal — bukan rekursi; CALL 0x38 resolve global(last-wins) lalu
native-table.
Confidence: Confirmed (bukti: FUN_80011edc per-case + trace ring 16K + replay done).
Test: T-SCT-VM sweep 128 entri Opening/Final.

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

### BRV-ANM-005 — Gramatika nama file animasi & varian senjata — DECODED (probe 374 pack)
Nama entri `.ga`/`.dbx` dalam pack = `<kelas|ref>[_<AKSI>][_<VARIAN-SENJATA>].ext` (dibuktikan
full-scan 300 `/zu|.pak` + 74 `/xcam|.cmp`, 374/374 parse). **AKSI** (tag akhir, frekuensi):
`poi` pose/stand · `mov` move · `at0/at1/at2` attack (stage/multi-hit) · `cr0/cr1/cr2` counter ·
`sk0/sk1` skill · `dam` damage · `ded` dead (kosakata FE standar) · `lda/ldc` load-A/C (draw) ·
`dog` mount (kuda — hadir di kelas berkuda amr/kni/red ✓ [INFERENCE kuat]) · `arc/lar` (bow
kelas-spesifik [INFERENCE]). **VARIAN senjata** (= nama pack `/zu/<job>/<job>_<VAR>.pak`):
`sw`(548) sword · `sp`(357) spear/lance · `ax`(261) axe · `no`(572) normal/unarmed ·
`ar`(84) bow · `ja`(21) katana · `mg` magic · `ms` staff · `kn/ha/cr/br/dog/lar/arc` (minor) —
set varian per kelas = **syarat engine: aset model per (class,weapon-type)** persis dispos JobList.
`/xcam|.cmp` = pack `.dbx` CameraAnime dgn kosakata SAMA (`at/cr/sk` × varian) — kamera battle
per aksi ✓ sinkron /xcam|.dbx 437. `/xwp|.cmp` 88 = `g+gs+tpl` per nama senjata (prop tanpa ga).
`/yme|.cmp` 292 = `g+ga+gs+tpl` per EID_* (anomi `ga curves=0` via parser unit = gap tunggal,
kemungkinan track non-bone scroll/warp — P6). Confidence: Confirmed (gramatika/vocab/frekuensi);
Partial (semantik `ldc/lar/arc/h/kn/cr/br` — perlu konteks C*.cmb call-site saat P1-map). [AM-075]

### BRV-ANM-006 — Kadensia walk world-map (anim vs kecepatan geser) — CONFIRMED (device A/B)
Clip `move_n` archer = 15 bone / 46 curve / **loop 36 frame**; pada 30fps native = 1,2 s/siklus
(2 langkah). Regresi ter-bukti di PoC: ticker anim 1 langkah/3-frame-render (≈10fps → 3,6 s
siklus) + geser 1,4 u/frame (0,23 s/ubin) → kaki 15× lebih lambat dari perpindahan = "bidak
catur geser". Perbaikan terverifikasi device: (1) anim **1:1 dengan render fps**; (2) kecepatan
translasi 0,85 u/frame ≈ 0,4 s/ubin; (3) **state machine clip per edge**: walk→`move_n`,
stop→`wait_n` (loop kedua-duanya); (4) yaw unit eased 25%/frame (FE9 belok halus, bukan snap);
(5) `wait_*` = pose idle meneguk napas kecil (bukan statues). Implikasi P3: **satukan clock anim
dengan frame engine, jangan rasio tetap** — dan kalibrasi `u/frame-per-tile` = konstanta game-feel
yang harus A/B vs rekaman GC (bukan turunan fisik). Confidence: Confirmed (render device).
Test: T-C1-MOVE walk vs diam frame-diff (wk1–4). [M2.2]

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

### BRV-INP-002 — Arsitektur input Android yang terbukti + jebakan NativeActivity — CONFIRMED
Engine Android PoC (`poc/`): **Activity biasa + SurfaceView + View.OnTouchListener → JNI
`nativeTouch(nx,ny,action,pointerId)`** (koordinator ternormalisasi view == viewport GL; letterbox
identik dua sisi). **JEBATAN MATI: `NativeActivity` (manifest `android.app.lib_name`) + SurfaceView
di Android 15 = window `touchableRegion=<empty>` (frame 0×0 di dumpsys input) → SEMUA tap senyap
dibuang dispatcher** (fokus window OK, queue ter-attach looper, callback tak pernah api; swipe/tap
adb maupun fisik). Root-cause dispatcher tak dikejar — cutover ke jalur view-tree yang terjamin.
**Kontrak input game (REQ dari produk): state ditekan, bukan edge-event** — loop engine membaca
`g_held[code]` tiap frame: panah L/R = orbit yaw ±0.9°/frame, F/B = geser ±0.09 unit/frame;
tombol aksi = edge `g_action` (consumed) + `g_tapsIn100ms` untuk rapid-fire. Multi-touch slot 16×
(`g_slotCode`), hit-test radius 0.09×1.26. Bukti perangkat: `input swipe` panah → `yaw 35→64→112`,
`z 0→3`, `taps2s=1`; panah B & tombol A/B memakai jalur kode identik. [AM-056][P2]

### BRV-INP-003 — Jalur gamepad fisik + keyboard PoC (InputState tunggal) — IMPLEMENTED (build-verified; QA perangkat menunggu)
Satu sumber input per frame: `input_poll()` → struct `InputState{TouchState,PadState}` — SEMUA
lapisan engine mengonsumsi SATU snapshot (bukan sebar atomic per fitur). Gamepad: Kotlin
`MainActivity.dispatchGenericMotionEvent` (intercept SEBELUM view tree — SurfaceView tak pernah
focus) + `onGenericMotionEvent` safety-net + `onKeyDown/Up` (dpad & key legacy; `repeatCount==0`
buang auto-repeat OS, edge dibuat di C++ via `g_padBtn.exchange` + bitmap `g_padEdge`). JNI
`nativePadButton(int,bool)`/`nativePadAxis(int,float)`; enum int PADB/PADAX = ABI dua file,
komentar larang reorder di kedua sisi. **Fallback dijamin by-design: `g_padConnected` sticky 0 →
`pad_update()` return seketika — jalur touch tak tersentuh sama sekali tanpa pad.**
Mapping harness: LEFT_STICK X = orbit yaw kontinu (deadzone 0.25, 2.2°/frame), RIGHT_STICK X =
yaw halus / Y = pitch 8..75° (global `g_camPitch` atomic BARU — hardcoded 24.f di tap_to_tile +
render diganti live, jadi raycast tap ikut pitch), DPAD/HAT = nav grid edge+auto-repeat 12 frame,
A = confirm (semantik overlay A), B = back, START = pause-stub (freeze ga+path+nav; gerbang
`g_paused` di on_input/tap/step/frameMs), L3/R3 = fit (yaw45/pitch24/fokus-unit). Keyboard ikut
jalur key: SPACE=A, ESC=B, ENTER=START, panah=DPAD. Trigger dinormalkan raw(-1..1)→0..1.
`g_camYaw` kini atomic (varargs fps log wajib `.load()`). Build: gradle 8.9 + NDK 27.2 lane `poc/`
→ `assembleDebug` SUCCESS (b52). Test: colok pad BT/USB → `adb logcat -s fe9poc` =
`pad: connected`, `pad: START -> PAUSE`, `camYaw` berubah di fps-log; tanpa pad log identik v-pre. [P2]

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
- **ADDENDUM stream-order (re-read FUN_80107b54 penuh, AM-054b)**: urutan stream per
  record = `u32 A | u32 B | u32 skip | 8×u32 | 5×u32 | 8×u8 | 4×u32 | 8×u8(scatter ke
  struct +0x08..0x0E,+0x66) | 8×u8(ke struct +0x30..37) | pad 4` = **108B stream** —
  stride bmap 112 = 108 + u32 ekstra (selalu 0 di bmap01). Struktur ≠ stream: 8 u8
  scatter ditulis ke celah +0x08..0x0F. KOREKSI penting: byte x,y stream (+64) = **0
  untuk semua record bmap01/h/m/n**; pasangan koordinat (u8,u8)×2 ada di 8 byte terakhir
  (struct +0x30): c: BOLE (0,7)(0,7), cleric (6,7)(3,9), ranger (15,13)×2, HERO_G
  (15,13)×2; h/m/n (2 record): FIGHTER (9,0)(9,7), HERO_G (10,5)(10,5) —
  kandidat (spawn, tujuan-pasca-cutscene) [INFERENCE; skrip C01 menimpa BOLE ke
  (15,13)@f1250 lalu (9,0)@f2706 — cocok pola field 1 vs 2 varian c]. Referensi string
  per-record: u32 kecil = OFFSET pool file itu sendiri + aturan prefix (full-entry
  terdekat ≤ v) + suffix — contoh rek c: `B0=PID_BOLE_MAP1`(586), `skip0=JID_CLERIC/F`,
  `w5_1=SEQ_NOATTACK`(630), `ai3=SEQ_ALLUNITATTACK100` (h) — tapi beberapa resolve
  = noise kebugungan (angka kecil kebetulan masuk range pool) ⇒ **tabel kebugungan
  field-per-field tetap butuh konsumen runtime (UnitLoadUnitInfo) = P2-validator**;
  jangan klaim penuh. Konfirmasi positif: h/m/n = 2 record (bukan 4; GREIL+1) vs c=4.

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
- **Menu aksi peta** lengkap: [Attack|Items|Shove|Wait] tempur (**"Wall" harvest Ch2 = MISA
  BACA dari "Shove" — terkonfirmasi frame jernih Ch1 t236**); **[Rescue|Items|Trade|Wait]**
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

### BRV-BTL-004 — Sampel tempur terpasang (pair-panel + outcome) — SUPERSEDED (lihat BTL-005)
Sumber: frame-accurate seek `fe9_ch1.mp4` (640×480, seek akurat pasca-temuan offset keyframe-seek
= harvest lama bisa meleset ±6 dtk; selalu `-i` dulu baru `-ss` ATAU verifikasi isi frame).
Tabel data: `docs/data/FE9_BATTLE_SAMPLES.csv` (3 sampel terpasang + kolom lengkap).

- **KOREKSI SOURCE-IDENTITAS (BTL-005, HD)**: sampel "BOYD vs Bandit" = SEBENARNYA
  **IKE (Ranger Lv2, HP20/20, 4×Iron Sword, slot E) vs Bandit (Fighter, HP24, Iron Axe)**;
  angka "Boyd 30/Mt11/Hit82 + outcome 24→13/30→23" = artefak keyframe-seek salah-tempo
  di 640p (bar 30 tidak pernah ada; HD menunjukkan bar 20/24→14/17). Angka lama DI-CABUT.
- Pair-forecast panel = dua kolom berpasangan per label `Mt | Hit | Crit` (MERAH kiri =
  musuh, BIRU kanan = pemain) + baris identitas + panah triangle — format terkonfirmasi HD.
- **Panel terrain kiri-atas live**: `[nama] Avo n | Def n | Mdf n` + objective `Clear Seize`
  + `Turn n` = HUD BRV-MAP: terrain stat field trio = Avo/Def/Mdf (magic/defense physical?).
- **Popup unit-info di peta (Oscar t251)**: `Lv 1 | HP 26/26 | Hit ~34 | Iron Lance | Lv1 Mov5
  | Avo 11 Def 5 Crit 1` = field set info panel (beda dari pair panel).
- Verifikasi ulang entri lama (frame jernih): menu = **Attack|Shove|Items|Wait** (Shove ✓),
  [Rescue|Items|Trade|Wait] ✓, banner ENEMY PHASE ✓, dialog Mila "What's going on out
  there? / Is that some sort of army?" ✓, EXP popup bar `x/19` pasca-kill ✓.
- **PANEL 6-FIELD TERBACA JERNIH (Ch2 t604.0, crop 2.5x)** — label ASLI:
  `Atk 13 | Hit 111 | Avo 19 / Crit 4 | Ddg 8 | Rng 1` ⇒ field ke-5 = **"Ddg"** dan ke-6 =
  **"Rng"** (RANGE=1 dua-duanya) — harvest lama Ch1 `"Dmg 5 | Recv 1"` = MISA-BACA
  `Ddg n | Rng 1` ⇒ klaim "damage received terbaca" DI-CABUT; tabel CSV B003 dikoreksi.
  Interpretasi Ddg: kandidat DEF-unit (Boyd-Ch1: Ddg4 ≈ DEF4 popup lama; Ike: Ddg8) —
  tapi Ddg muncul di panel penyerang; BELUM DECIDED.
- **Widget kedua**: popup compact biru (Ike) t604.5 = `[HP]22 | 12 | 100 | 4` baris tunggal
  (label terpotong) — beda angka dari panel kuning (13/111/4) = DUA widget beda sumber;
  jangan campur. Kemungkinan compact = ATK/Hit/Crit TANPA koreksi vs kuning DENGAN.
- Sampel B004 CSV: Ike(Lv5,Hp22,Inv Iron Sword 30/46+46/46×2+Steel Axe 43/45) vs
  Bandit(Lv3,HP23,Steel Axe): arena bandit 23→16→11 (dua tebas 7+5), Ike counter MISS
  (HP22 tetap). Triangle panah ↑Ike ↓Bandit terlihat di popup compact.
- **KOREKSI ENTTRI LAMA**: BTL-003 `Dmg5/Recv1` = mislabel (lihat di atas); `Wall`→`Shove`
  sudah dikoreksi di BRV-MAP section.
- **Panen unit/terrain Ch2 t546-566 (compact popups, mont_e550)**:
  - **Objektif peta Ch2 = `Clear Route`** (bukan Seize) — field objective HUD kiri-atas
    `Plain|Thicket Avo? Def? Mdf? / Clear Route / Turn n` = format universal.
  - **Terrain baru: `Thicket`** (semua terrain punya trio Avo/Def/Mdf; Plain = 0/0/0).
  - Popup compact per unit: `Rhea Lv4 HP22/22`, `Titania Lv3 HP33/33 Steel Axe`,
    `Oscar Lv3 HP26/26 Iron Lance` — format [Nama Lv HP/max + senjata] = widget BRV-BTL
    "compact" yang sama dg popup tempur (bukan panel kuning).
  - **EXP popup `Exp ▓▓ 11 → 22`** (bar + angka, kumulatif dua kill); menu unit non-tempur
    `[Items|Wait]` (tanpa Attack/Shove).
- Langkah berikutnya: harvest Ch2 window 380-400+540-560+660-690 (arena+status screen
  Oscar/Boyd terlihat di grid) cari varian triangle (lance-vs-axe) + zoom pair-panel Ch1
  fcB t237 untuk menguji Mt kolom = Ddg? ⇒ fit formula dengan ≥4 sampel.

### BRV-BTL-005 — SUMBER HD 720p: forecast panel TERBACA PENUH; Mt = kerusakan final — DECODED (observasi)
Sumber BARU: rekaman HD lokal `Fire Emblem： Path of Radiance - Full Game in HD!
[Vf53jqrC3L8] 720p.mp4` (1280×720@30, 21h55m, Jimmy Longplays; LOKAL saja — tidak
di-commit, aset berhak pihak lain). Timeline chapter = deskripsi video (tabel di bawah).
Kualitas 720p = teks panel 100% terbaca tanpa ambiguitas (menggantikan semua bacaan 640p).
**Gotcha seek: `-ss` sebelum `-i` di file ini = keyframe-jump PALSU (frame hitam/hanya
keyframe); pakai filter-stream (`-ss` + `-t` + `select`/`fps` setelah `-i`) = frame-accurate.**

- **PANEL FORECAST HD (satu-satunya widget, bukan dua)** = tabel tempur pra-serang:
  ```
  ┌──────────┬────┬──────────┐   ← badge "×2" kanan-atas (lihat bawah)
  │ 6    Mt  7 │      Ike    │
  │ 52   Hit 100│   Iron Sword│
  │ 0    Crit 3 │             │
  ├──────────┴────┴──────────┤
  │        Bandit            │
  │  🪓 Iron Axe  ↓           │   ← panah MERAH BAWAH = triangle劣势 (axe vs sword)
  └───────────────────────────┘
  ```
  Kolom MERAH = musuh (kiri), BIRU = pemain (kanan); label tengah `Mt | Hit | Crit`;
  baris bawah = identitas target + senjata terpasang + panah triangle.
- **GROUND TRUTH (Ike vs Bandit, Ch1, t_HD≈745.5):**
  - Ike: Ranger Lv2 EXP67 HP20/20; inv **4× Iron Sword** (slot aktif **E**), dur 46/46.
  - Panel kiri-bawah Ike: `Atk 10 | Hit 109 | Avo 19 / Crit 3 | Ddg 4 | Rng 1` (raw).
  - Forecast: **Bandit 6 Mt / 52 Hit / 0 Crit vs Ike 7 Mt / 100 Hit / 3 Crit**.
  - Outcome arena (bar terukur): Bandit 24→17, Ike 20→14 ⇒ **damage nyata = Mt persis**
    (7 dan 6 = ΔHP). **DECIDED: kolom `Mt` forecast = kerusakan final per-hit**
    (sudah dikurangi DEF+terrain+triangle); sampel lama "Mt11→dmg11, Mt7→dmg7" ikut cocok.
  - `Hit 109` raw → forecast **100 = cap 100%**; `Ddg 4` = DEF Ike (raw ATK musuh
    = 6+4 = 10); raw ATK Ike 10 − DEF Bandit 3 = 7 ✓.
  - Badge **`×2`** muncul di kolom Ike (HP 20 vs 24) tapi outcome 1-hit/0.5-dua-hit —
    trigger doubling FE9 = **UNKNOWN** (kandidat: ATK/HP ratio; butuh sampel ganda jelas).
- **HUD dalam arena**: label `Ike` / `Bandit` + tag senjata di atas bar HP (`Iron Sword 45`
  = durability live 46→45 pasca-attack ✓); popup inventory target = daftar 4 slot
  `🗡 Iron Sword 46/46 ×4` (durabilitas hijau).
- **INVENTARIS BANDIT = 3×Iron Sword + Iron Axe** ⇒ musuh humanoid membawa 4 item =
  bukti drop-pool/loot (perlu dicek dispos record: field item1-4).
- **Timeline chapter HD** (dari deskripsi video; HH:MM:SS → detik): Prologue 0(0);
  Ch1 08:46(526); Ch2 22:45(1365); Ch3 41:26(2486); Ch4 01:01:44(3704); Ch5 01:30:03(5403);
  Ch6 02:06:54(7614); Ch7 02:33:34(9214); Ch8 03:20:52(12052); Ch9 04:03:53(14633);
  Ch10 04:41:42(16902); Ch11 05:26:01(19561); Ch12 06:15:53(22553); Ch13 06:45:20(24320);
  Ch14 07:36:46(27406); Ch15 08:17:39(29859); Ch16 08:50:37(31837); Ch17 09:47:56(35276);
  Ch18 11:51:58(42718); Ch19 12:45:55(45955); Ch20 13:37:22(49042); Ch21 14:15:17(51317);
  Ch22 15:26:46(55606); Ch23 16:17:45(58665); Ch24 17:06:19(61579); Ch25 17:44:35(63875);
  Ch26 18:22:22(66142); Ch27 19:14:54(69294); Ch28 19:49:16(71356); Final 20:28:38(73718).
- **Status screen roster Ch2 terlihat** (contact sheet): Brom Knight Lv4 HP28/28,
  Volke Thief, Miermirrian Mage Lv9 75EXP HP21/21, dll. = jendela harvest berikutnya
  (window Ch2 1365-2486: buka status screen semua unit = ground truth stat awal roster).
- Implikasi P2: engine WAJIB menghasilkan Mt/Hit/Crit forecast identik + cap 100 + panah
  triangle; `Ddg` panel = DEF. CSV `docs/data/FE9_BATTLE_SAMPLES.csv` +bari B005. [AM-057]

### BRV-BTL-006 — HD batch-2 (window 7938-7990): level-up, inventaris, forecast ×2, Escape — DECODED (observasi)
Satu chapter penuh (bridge/river, "Clear Escape", Turn 5) = panen widget terlengkap.

- **FORECAST HD #2 (Titania vs Volke, bridge)**: `Titania Iron Axe ↓` vs
  `Mercenary Volke Steel Sword ↑`: merah(Mt21/Hit55/Crit0) ↔ biru(Mt15×2/Hit100/Crit5).
  Outcome: Volke kena 40 (CRIT kuning besar "40") → 21→0 MATI; Titania 33→21 = kena 12
  = 21(Mt)−9(Def Titania) ✓ **Mt=kerusakan-final konfirmasi ke-3**. Titania 29→0 bar
  berikutnya = fight vs Ralph terpisah (list musuh: `Ralph 30/31`, `Volke 21/21`).
- **Kelas musuh = `Mercenary`** (bukan "Bandit" saja — variasi class musuh tampil di
  forecast + status window).
- **CRIT FORMULA OPEN**: panel Mt15 → crit tampil **40** (bukan 3×15−Def sederhana;
  Volke DEF≈0-5). Butuh sampel multi-crit untuk fit.
- **×2 DOUBLING — KOREKSI (batch-3)**: Ike 22 vs Soldier 23 ikut menampilkan ×2 → teori
  rasio-HP **GUGUR**. Semua sampel ×2 = ada **unit sekutu ber-ring biru di sekitar** →
  **×N = PAIR-UP: N partner support ikut menyerang** (FE9 tidak punya doubling HP — itu
  FE10; pair-up = satu-satunya double-attack FE9). Rule eksak trigger pair (jarak/
  pasangan) masih [UNKNOWN] — butuh data support.
- **POPUP UNIT + INVENTARIS (Titania)**: `Lv 1 EX96 | HP 33/33` (kuning) + jendela item
  4-slot dengan **badge `E` slot aktif**: `E Steel Axe 30/35 / Iron Axe 34/45 /
  Iron Sword 46/46(ABU-ABU) / Iron Axe 44/45`. **ABU = tidak bisa dipilih** (konteks
  serang); durability ≥~½ = HIJAU (30/35, 34/45, 44/45 semua hijau) — threshold merah
  belum kesamples (bekas: patah 0 = merah).
- **STATUS/LVLUP SCREEN (HD native)**: header `Paladin  Lv 2` + grid 8-field
  **`HP 34(+1↑) Sp 14 / Str 12 Lck 11 / Mag 4 Def 11 / Skill 13 Res 7`** + panah hijau
  `+1 ↑` pada stat yang naik + potret besar berlapis (BRV-GFX-001 konsisten) + banner
  nama. = format level-up = widget WAJIB engine. Stat ground-truth: **Titania Paladin Lv2:
  HP34 Str12 Mag4 Skill13 Sp14 Lck11 Def11 Res7**.
- **HUD kiri-atas (format universal, terkonfirmasi ulang)**: `[terrain] | Avo 0 Def 1 Mdf 0`
  (Thicket Def=1!) + **OBJEKTIF `Clear Escape`** (tipe ke-3 setelah Clear Route/Clear Seize)
  + `Turn n`. Unit popup `nama | Lv n | HP x/y | senjata`.
- **RADIAL WHEEL penuh (HD)**: pusat **A**, ring: `Map Menu (Z) / Status (Y) /
  Next Unit (X) / Move (A) / Cond / B`; state menu aksi: hint `Cancel B / End A`.
- **Kursor ring warna**: biru=unit pemain, merah=musuh, ungu=terpilih, +bracket putih
  +panah kuning tujuan; tile jangkauan serang = overlay ORANYE.
- **"Escape!"** = efek popup saat unit keluar peta lewat tile escape (widget + objective
  Clear Escape berpasangan).
- Implikasi P2: engine wajib: 3 tipe objective (Route/Seize/Escape), tile escape,
  level-up +1↑ screen, inventaris 4-slot + E-badge + grey-out, ring kursor 4 warna. [AM-058]

### BRV-SYS-002 — MENU OPSI game (HD) = daftar setting engine — DECODED (observasi)
Opsi in-game (pause→Options), layar 9-baris scroll (+baris palet saat scroll),
navigasi **C-stick/D-pad + A**; hint `Select Topic / Select Content / B Back`:
1. `Animation:  On | Off | Individual`
2. `Combat Camera:  Normal | Fixed`
3. `Game Speed:  Normal | Fast`
4. `Message Speed:  Slow | Normal | Fast | Max`
5. `Grid Thickness:  [SLIDER nilai kontinu]` (bukan enum!)
6. `Terrain / Goal:  On | Off`
7. `Unit Window:  On | Off`
8. `Combat Window:  Simple | Detailed | Off`
9. `Message Display:  On | Off`
10. `Grid Line Color:  [palet swatch 4×4]`
11. `Cursor Color:  [palet swatch 4×4]`
Help bar = deskripsi field per-baris (`"Change type of combat window..."`).
= spesifikasi layar SETTINGS Android PoC = 1:1 dengan daftar ini [AM-058].

### BRV-BTL-007 — MODEL DISPLAY panel tempur + weapon-diamond + radial penuh — DECODED
Dari 3 forecast HD (Ike 10−3=7; Titania vs Volke 20−5=15 & Volke 21−9=12; ΔHP aktual
semua persis): **panel display punya SEMANTIK CAMPUR yang harus ditiru engine**:
- Panel raw per-unit (kuning): **`Atk | Hit | Avo / Crit | Ddg | Rng / Effect`**
  (7 field; Effect = efek senjata; Ddg = DEF).
- **Atk** = kekuatan-serang-total unit (Str+Mt+triangle, TANPA def lawan): Titania
  Iron Axe **20** = Str12+Mt8+0 ✓ → **Iron Axe Mt=8**. Volke Steel Sword 21.
- **Mt (kolom forecast) = Atk − DEF target**: 20−5=15, 21−9=12, 10−3=7 ✓✓✓ = kerusakan
  final = Mt forecast (3× terkonfirmasi + 3× ΔHP).
- **Hit (kolom) = Hit-raw − AVO target, cap [0,100]**: Volke raw101−46=55 ✓; Ike/Titania
  raw109/119 clamp→100 ✓. Panel raw `Hit` = nilai SEBELUM avo (109/119).
- **Avo (panel) = evasion-total diri** (titania 46) — tdk tergantung target.
- **Crit** kolom = final; cap/effek [OPEN]. **Rng 1** = jangkauan.
- **Pop-up unit (kuning, klik unit)**: `nama / Lv n EX n / HP x/y` + panah putih = slot
  equip + **inventaris 4-slot**: `nama_item d/d` + **baril warna** + **row ABU-ABU = item
  mati-rank-tidak-bisa-dipakai**; item aktif = panah (bukan badge E — E badge = slot
  equip di layar lain).
- **WIDGET SWITCH-SENJATA = DIAMOND 2×2** (kanan bawah, 4 ikon = 4 slot inventaris,
  D-pad) — muncul saat aksi Attack terbuka = UX signature FE9; engine WAJIB ada.
- **RADIAL WHEEL final (HD)**: pusat `A (Move)`, slot: `Map Menu (Z)` `Status (Y)`
  `Next Unit (X)` `Cond (—)` `B (Cancel)` + panah biru Z. Menu aksi: `Cancel B / End A`.
- **Ring**: biru=sekutu, merah=musuh, ungu=kursor, **bracket-putih-putus=unit terpilih**;
  overlay gerak = BIRU, overlay jangkauan serang = ORANYE, tile escape = panah+`+` biru.
- Label senjata arena kadang **`Regal Sword`** (?? kemungkinan `Rune Sword` font tipis —
  verifikasi crop nanti); popup Boyd `Lv 4 HP 31/31 Iron Axe`; objective `Clear Escape`
  Turn 6 (fase musuh mengepung jembatan).
- Implikasi: tabel Mt senjata dasar bisa dibaca dari (Atk−Str) unit dikenal → seed
  `docs/data/FE9_WEAPONS.csv` [AM-059].

### BRV-SCT-005b — SEMANTIK NATIVE batch-1 + MODEL UNIT RUNTIME — DECODED
Dari decompil impl (decAll) + katalog:
- **MODEL UNIT RUNTIME** (oracle `FUN_80016bdc`, `FUN_800bd4c0`): array **160 slot ×
  0x280B @ 0x802B14E4**; **`+0x188` = PID interned-id (0=kosong)**; **`+0x198` = BYTE
  handle-unit = nilai yang DIKEMBALIKAN native ke script** (bukan pointer!);
  **`+0x19E/+0x19F` = X,Y (i8)**; `+0x1A0` = bits state (mask 0x1C = alive filter);
  `+0x190` = ptr→tabel info kelas (byte+8 valid). 84 impl native membaca `+0x198` =
  referensi unit universal script; keluarga **`Unit*ByPID`** (SetPosDirByPID/RotateByPID/
  MoveDirByPID/…) bekerja LANGSUNG dgn PID arg (string C01 membuktikan `PID_BOLE_MAP1`
  → `UnitSetPosDirByPID`); UnitWalkDir(dir) = enqueue utk **unit AKTIF global**
  (`DAT_803307a8` ikut tersimpan di record antrean queue `DAT_80296f90`).
- **UnitGetByPID(pid)**: scan 0x188==pid → return slot[0x198]; 0 jika absen.
- **UnitGetByPos(x,y)**: scan valid (kelas ok, state&0x1C==0) x==param1 && y==param2;
  arg-parse `FUN_80015320(argc,buf)`.
- **WaitM(ms)**: jika `scriptglobal&1` (fast-forward/Min) → 0 = skip; else
  frames = (ms − tickFrac)/msPerFrame; push `frames−1` ke antrean global `DAT_8028d22c`
  (via intern-FUN_8000caa0) → return 1 (script block s/d queue kosong). = protokol
  SCT-005 terkonfirmasi dr impl.
- **MapLoad(name)**: → `FUN_8014c7c0(&DAT_80330708=BMAP, name)`; return 1 sinkron.
- Native argc & pasangan nama-impl = `native_catalog.csv` (authoritative). Lainnya dlm
  katalog ber-argc: `UnitWalkDir(1) UnitMoveWait(0) UnitFocusOff(0) UnitFadeOff(0)
  UnitFadeOut(0) UnitGetNext(1) UnitGetNext(1) UnitSetCpMoveSeq(2) UnitFadeIn(0)
  UnitSetCpAttackSeq(2) UnitGetX(1)&LAB UnitGetY(1)&LAB UnitRotate(3) UnitEscape(2)
  UnitMoveDir(2) UnitTransform(2) UnitQueryJID(2) UnitAddItem(2) UnitGetWeaponCount(1)`
  — semantik per-arg = batch berikutnya (pseudocode tersedia).
- CSV field-unit runtime: `docs/data/FE9_UNIT_RUNTIME.csv`. [AM-060]

### BRV-SCT-005c — SEMANTIK NATIVE batch-2: handle index + path digit + wrapper BMAP — DECODED
- **HANDLE = NOMOR SLOT 1-BASED**: `UnitMoveDir(h, path)`: slot = `(h&0xFF − 1)×0x280 +
  0x802B14E4` (cek PID≠0) → **handle = index+1** (0 = invalid) — genap dgn GetByPID
  mengembalikan `slot[0x198]` = nomor itu. Kontrak engine: handle = u8 index slot.
- **PATH = STRING DIGIT NOPAD**: `UnitMoveDir(h,"444442")` — digit ASCII kode arah
  (4=kiri 6=kanan 2=bawah 8=atas?? — set digit persis masih Partial; string C01 `"4"`,
  `"6666"`, `"444442"` dipakai sbg arg path). Enqueue rekaman gerak ke queue
  `DAT_80296f90` (sama spt UnitWalkDir, field 0x30-0x3C).
- **InstantDispos(pid)** → `FUN_80105f0c(pid,0,0)` = despawn instan per-PID.
- **SallySetByGroup(g)** → `FUN_8014cf48(&BMAP=0x80330708, g)` = set kelompok sally.
- **MapLoad(name)** → `FUN_8014c7c0(&BMAP, name)` (wrapper 1-liner, return 1).
- Keluarga wrapper BMAP-objek = pola `FUN_8014c***(&0x80330708, args)`; engine memodel
  BMAP sbg objek dgn metode load/sally/camera. [AM-061]

### BRV-SCT-005d — SCENARIO FLOWS 43 .cmb TEREKSTRAK LENGKAP — DECODED
- Tool bin/cmbflow.mjs: 2075 func (43 /Scripts/C???.cmb) -> docs/data/cmb_flows.csv
  (file,func,id,argc,seq; token = 'native/argc', 'i:const', 's:str', '_'=DROP, 'RET').
- 145 func berscenario-MapLoad = SEMUA peta (chapter + battle map) punya flow utuh.
- CH1 main (C01, 392 token) VERBATIM: Comeback; Fade(2); FadeWait; ENVSilentOn(0);
  MapLoad("bmap01"); SetGrid(0); SetCircle(0); SetOutLink(0); UnitFocusOff;
  InstantFocus(4,7); InstantSetMapCamera(-55,55,18); SallySetByGroup("bmap01_mikata_c");
  InstantDisposFirst("bmap01_first_c"); InstantDisposFirst("bmap01_first_g_n/h/m") x3 +
  InstantDisposFirstSetMode(n,h,m); InstantDisposFirst("bmap01_first_n/h/m"); ...;
  UnitSetPosDirByPID("PID_BOLE_MAP1",15,13,dir4); UnitRotateByPID("PID_IKE",1,0);
  UnitRotateByPID("PID_MIST",2,0); ... = spawn koordinat + rotasi eksak per unit.
- Dispos per-difficulty = pack terpisah bmap01_first_n/h/m (+_g_* group) — engine
  memuat varian n/h/m sesuai flag difficulty (pola sama shopitem_h/m/n).
- Definisi sukses PoC P2 §8 kini punya skrip referensi nyata per chapter. [AM-063]

### BRV-MAP-003  GRID SEMANTICS: MAPLINK -> 64x64 U16 ARRAY  Decoded (parsial)
- Konsumen slot = `FUN_80045604` (maplink/maplinkAt/maplinkAbs) +
  `FUN_80044450` (panelindex/uniquepanel/Roof/Base; VA string 0x8026A8CA-957,
  DELTA file->VA = +0x80003000 untuk segmen ini!).
- `maplink` record = u16 count + triples [x i8, y i8, val u8]; tulis
  `grid[y*0x40 + x] = val` (u16 array global, stride 64 = PETA 64x64 tiles,
  base global ~0x802CE5A6 area). maplinkAt = varian offset-panel, maplinkAbs
  = koordinat absolut.
- String slot blok-1 (mapbuilddesc/capacity/extra/fog/grid/light @0x8026A7D1+)
  TIDAK direferensikan kode DOL = lookup generik mapbuild (via tabel; consumer
  panelindex FUN_80044450 = yang nyata membaca grid).
**PANEL-PIPELINE (grid nyata Ch1) DECODED  FUN_80044450**:
  panelindex slot = u16[W*H] (W=*hdr, H=hdr+4) -> indeks ke uniquepanel[]
  (record 12B = 6*u16: [0..3] param panel u16, [+4] u32 -> FUN_8005b7a4 ==
  u8 flag tile). Base variant (map 0-0?): panelindexBase/uniquepanelBase.
  Tulis global: tileflag u8[y*0x40+x] @~0x802CB525; arrA u16[4]/tile
  (stride 16B) @~0x802DE7EC; arrB u16[4]/tile (8B) @~0x802E67F0.
  = engine model: panel table + W*H indeks -> flags/param per tile.
- dispos varian DIFF C/H/M/N untuk bmap01: c=4 unit (Ike/Mist/Bole/Greil,
  JID_HERO_G/CLERIC/F/FIGHTER/RANGER, AI passive NOATTACK/NOHEAL/NOMOVE,
  SID_IMMORTAL), h=2 unit + SEQ_ALLUNITATTACK100 (agresif, lebih sedikit
  musuh), m/n BYTE-IDENTIK KECUALI 11 byte string event (_m/_n).
- Grid bmap01 = slot `mapgrid` (data block 0x0b74/0x0d34/0x113c/0x12fc)
  dibaca panelindex; triple-stream 0x16e0 = kandidat maplinkAt payload.
### BRV-MAP-002  PETA bmap01 (Ch1): KONTEN + SLOT GEOMETRY  Partial
- `//zmap/bmap01/map.cmp` unpack = 13 entri: `bmap01.g/.gs` (peta 3D),
  `bmap01_water0.g/.ga/.gs`, `flower_01.g/.gs`, `tree_01/02.g/.gs` (props),
  `map.bin` 6927B (config), `texpack.tpl` 865KB (128 image pool).
- `dispos.cmp` = 4 varian: dispos_c/h/m/n.bin (c = campaign normal s.d. sudah
  didekode SCT-009; h/m/n = sulit/medium/new-comer, FORMAT SAMA).
- `map.bin` header = keluarga universal [u32 size][u32 tableOff=0x1760][u32
  cnt=0xAA][u32 sub=12]. **Tabel slot 12 pas u32 (dataOff, nameOff) TERKUNCI
  @0x1a28** = 0x60 byte TEPAT sebelum string pool 0x1a88; nameOff = pool-rel +1
  (quirk skip-karakter; konvensi FE8Data). Slot + dataOff: mapbuilddesc=0xC,
  mapbuildinst=0xAC (0xAC8 B), mapcapacity=0, mapextra=0x16C8, mapfog=0x16BC,
  mapgrid=0x16C4, maplight=0x16B0, maplinkAt=0x16D4, panelindex=0xB74,
  panelindexBase=0x113C, uniquepanel=0xD34, uniquepanelBase=0x12FC (arena
  bf_map = varian akhiran B).
- **`mapbuilddesc` = blok kamera f32** (0xC..0xAC): pola sama dengan kamera
  arena bf_map (2000.0 orbit dst) — BUKAN definisi terrain.
- **`panelindex/uniquepanel` = panel visual tileset kamera** (u16 id panel +
  sentinel 0x1757/0x175C), BUKAN jendela terrain. Nama "panel" menyesatkan.
- Stream byte-triples `03 0f 04 / 03 0f 0a ...` @~0x16e0 = terrain/grid tiles
  [GEOMETRI PRESISI = P1-map, perlu Ghidra mapbuild consumer].
- **Tabel terrain TIDAK ada di map.bin / DOL (dicek 2×)**: string
  `E_MapWeatherBridge` = KEY CUACA (E_MapWeather*), jebakan — BUKAN terrain.
  Nilai terrain (Avo/Def/MDf) tervalidasi VIA VIDEO (BRV-BTL-008: "Bridge
  Avo 0 Def 0 MDf 0"). Sumber struktural terrain masih open (kandidat:
  konsumer `mapbuildinst` atau blok .data tanpa string).
- Loader chain: MapLoad native (FUN_801a0400) -> baca cmp + map.bin slot.
- **ADDENDUM 2026-11-12 (tool `bin/mapbin.mjs`, parser general loader FUN_8003f87c)**:
  kalibrasi RELOK terukur: nilai u32 di slot ter-patch + 0x20 = offset file target
  (bukti 5/5: pointer mapbuilddesc → nama model). `mapcapacity` = [u16 W=16][u16
  H=14][u16 nInst=5][u32 0x00e644fa][u16] ⇒ GRID LOGIS Ch1 = 16×14 (bukan 29×24 —
  29×24 = overlay visual engine; tile 10u model memuat margin). `mapbuilddesc` =
  5×rec 0x20B: [? u32][f32@+0x10 pos][ptr nama@+0x1c]: (bmap01,31,240,-2),(tree_02),
  (tree_01),(bmap01_water0), (flower_01) = INSTANCE props + posisi; `mapbuildinst`
  = 5×0x14B varian u8. `panelindex` = u16[W*H] → **indeks langsung uniquepanel
  (86 rec)**: histogram bmap01 = id1×109 (daratan), id2×29 (air — cocok water0),
  id0×1, id3..85 unik ×1 (props/dekor). np uniquepanel = sentinel 0x1757/0x175C
  (region maplinkAt) = konfirmasi 003. RELOC table 170 entri tervalidasi.
### BRV-MAP-004  FE8Data.bin = REGISTER 112 NAMED-CHUNK (loader sama map.bin) + TABEL TERRAIN + KOSAKATA — DECODED (semantik field = P4)
- `FE8Data.bin` (system.cmp) = container IDENTIK format map.bin (base +0x20, hdr
  [size][tableOff][ptrCnt=6686][dirCnt=112]; dir [dataOff][nameOff]; parse via
  `bin/mapbin.mjs` — 112/112 nama valid). Chunks: **DatabaseHead** (32B master),
  12 tabel data = BattleSkyData, **BattleTerrData@0xfa44**, ChapterData, DivineData,
  GameData, GroupData, **ItemData@0x9c90**, JobData@0x6fa0, KiznaData, **PersonData@0xc**
  (0xc..0x6fa0 = 28KB stats karakter), RelianceData, SkillData@0xe374 + **100 SID_***
  (skill/efek bernama SID_*) = 112.
- **BattleTerrData hdr = [u16 77][u16 42]** ⇒ **77 terrain × 42 slot u32**
  (0xfa44..0x12d78 = 13108B = 4+77×42×4 ✓). **TERUKUR 2026-11-13**: sel =
  POINTER POOL (bukan numeric) — campuran **nama scene/interior battle**
  (`map06_fort map14_ship01 map28_room01 map09_fort01 map04_harbor01
  map16_castle01 map18_ruins map05_lake map17_room02` baris0; baris40
  village/cliff; baris70 = laut: `map14_sea SID_WEAPONDESTROY
  SID_WINGSHIELD`) + **SID_* skill grant per-terrain** (bukti MEKANISME
  bonus: DEF/AVO/weapon-destroy = SID yg di-grant terrain, bukan angka
  langsung) + nama SE (`csfxseal`). Pointer mendarat TENGAH string ⇒ wajib
  backward-walk. Distribusi: banyak baris kosong (plain = tanpa efek).
  Pemetaan baris↔terrain-id + role per-kolom-42 = P4 (grid penuh ada di
  out/system/FE8Data.bin via parseRegistryBin).
- **TerrainData@0xf538** (1204B) = blok triplet-ish [ptr pool][ptr][u32] —
  BUKAN bonus numeric (pointer saja; stride ≠12 presisi). Konsumer hash
  `FUN_8005b7a4`: h=(h+c)*0x10 per char ⇒ Nama≤4 char = packed-ASCII;
  tabel 77 runtime dibangun boot `FUN_8005b820`; special idx9/33→0x1c.
  Pemetaan record↔id presisi = P4.
- **POOL KOSAKATA TERBACA (SJIS Shift-JIS, tool decode)**:
  - `MT_*` ×52 = **nama material tileset**: 移動不可(impassable!), 平地(plains), 海,
    海岸, 川, 水路, 沼, 砂漠, 雪原, 山道, 茂み, 木, 岩, 崖, 橋, 石畳, 床, 壁, 屋根,
    建物, 城, 城門, 扉, 壊れる扉, 玉座, 砦, 柵, 柱, 台座, 土嚢, 宝箱, 遺跡, 廃墟,
    噴水, 穴, 階段, 花壇, 回復, 制圧, 訪問, 防衛, 到達, 離脱, 堀, 落石, 投石器,
    アーチ, 小物, 船... ⇒ material = pemegang peran gameplay (seize/gate/chest...).
  - `MW_*` ×14 = **kondisi menang/kalah peta**: 制圧(seize), 到達_15/_20 (survive/
    reach), 防衛_6/8/10, 防衛敵, 未到_16/_21, 全滅(rout), 離脱(escape), エ死(leader
    death), 王宮? ... ⇒ grammar objective Ch1 = REACH_15 (到達_15 ✓ sesuai C01!).
  - `MTM_*` = nama theme musik map (MTM_TRI_MAP5...); `RID_*` ×27 = background
    dialog (天幕/城内/民家...); region 0x1c7xx = nama skill SJIS (HPの歌, ぶちかまし,
    カウンター, カリスマ, エリート... = terjemahan SID_*).
- Impl engine: BattleTerrData = sumber status terrain begitu terrain-id per tile
  terpetakan (tile→MT_ via panel np + runtime table capture); MW_* = enum objective
  world-loop. [AM-050]

### BRV-MAP-005 — WATER MASK CH1 = FOOTPRINT `bmap01_water0.gs` + `maplinkAt` = PATCH TILING (bukan air) — DECODED (kelas terrain 0x0B/0x0D = Partial)
- **Kebenaran air = model `bmap01_water0.gs`** (di-load engine sbg bagian pack
  bmap01): rasterisasi footprint segitiga xz (pos/10 → tile 29×24) = **124 tile
  DANAU kiri-bawah** (baris 11..23, kolom 0..11; material `map_water:img3`).
  Bake `bin/waterbake.mjs` → `maps/bmap01_cost.bin` (696B, land=1 water=99).
- **`maplinkAt` (map.bin) = deret rekaman [u8 x][u8 y][u8 type] mulai dataOff+2**,
  berakhir terminator type=0 (rekaman terakhir (10,11,0) disambung pool nama
  `map01/bmap01_water0/flower_01/tree_01/tree_02` + string SJIS). Ch1: **27 titik**
  — type 0x0F×24 membentuk TANGGA DIAGONAL (3,3)→(12,7)→(9,10) di kuadran kiri-atas
  = BUKAN air (danau ada kiri-bawah; water0.gs membuktikan) ⇒ pita HUTAN/tile
  event [INFERENSI kuat: bentuk tangga tepi pita vegetasi, FE9 Ch1 thief di hutan];
  0x0B×2 + 0x0D×1 = kelas lain (Partial). BUKAN exit spawn — pool spawn wave
  memakai titik ini sampai dispos_c param grammar closed.
- **Panel q bukan enum**: uniquepanel 86 rec = 4 kata u16 berteks packed (keluarga
  FE8, sentinel 0xF336) + np→pool (SJIS); grid 16×14 = panel GRAFIS, bukan taktis
  — terrain taktis CH1 datang dari gs/water0 (v1), bukan panelindex. [AM-052]
- **Loader engine [P4-M1.1]**: `maps/bmap01_cost.bin` (water blokir BFS/move
  `g_cost>=99`) + `maps/bmap01_exits.bin` (pool spawn wave, u16 n + n×[x,y]) via
  rd()/override; deploy device: push /data/local/tmp → `run-as cp` ke
  `files/assets_override/maps/` (SELinux API29). Log verifikasi: `wloop: cost grid
  loaded water tiles=124/696` + `wloop: exits=27` (TERPASANG device 22:08 ✓).
- **FIX BUG Z-KONVENSI [BRV-MAP-005]**: unit/cursor/kamera memakai `tile_cx(gy)`
  (=-140+10g, konvensi X) untuk Z ⇒ unit ter-render 2.5 tile di luar sel grid;
  sumber kebenaran grid=tanah=`p[2]-120` ⇒ helper baru `tile_cz(g)=-115+10g`,
  9 call-site z dialihkan; tap inverse (`(hz+120)/10`) sudah benar → konsisten.

### BRV-SCT-005e — NATIVE SKENARIO CH1 + FLAG TABLE + COMMAND QUEUE — DECODED
- **Arg koordinat kamera = INTEGER BIASI (TERBUKTI)**: pseudocode f64-magic
  `bits{0x43300000,p^0x80000000} - bias` = identitas (uji numerik: p=4,7,-55,55,18
  hasil = p persis; bias=2^52+2^31 = artefak konversi PPC). InstantFocus(4,7)
  = tile (4,7); InstantSetMapCamera(-55,55,18) = kamera XY tile + zoom 18.
- **COMMAND QUEUE (async scene cmds)**: SetGrid/SetCircle/SetOutLink/UnitFocusOff/...
  = `FUN_8000c0c0(&DAT_80296f90)` cari / `FUN_8000caf0` alokasi record cmd di pool
  `FUN_800c0658`, isi field +0x30..0x3c (on/off + param) ${em} renderer/engine memproses
  antrian cmd visual tiap frame; SetGrid(-1) = SIMPAN grid unit aktif ke global
  DAT_803307a8 (unit+0x33 = byte grid display per-unit).
- **FLAG TABLE global script = objek @0x803307bc**: `get(name)`=FUN_800bdd08,
  `set(name)`=FUN_800bdc68, `clr(name)`=FUN_800bdbc8 = kamus flag bernama
  (dipakai `gf_canceled` skip-flag oleh Comeback). Engine: Map<string,bool>.
- **Fade(mask,timeMs)**: FUN_8008a290(mask,time), time<0 ${em} default 500ms;
  pasangannya FUN_80089650 (fade state read); Comeback() reset kamera+fade+clear
  antrean jika flag skip/canceled/queue-penuh (mode fast-forward).
- **_gi(handle,itemID)** = kasih item: slot=(h-1)*0x280 (SCT-005c), cek PID +0x188,
  konstruksi stack item (FUN_8001c76c/d780/c690) + UnitAddItem (FUN_8007386c).
- **InstantDisposFirst(pack)** = FUN_80105f0c(name,0,1) = muat pack dispos-first
  (spawn awal skenario); varian per-difficulty = nama pack beda (bmap01_first_n/h/m),
  mode di-set via InstantDisposFirstSetMode (script func startup.cmb ${em} bukan native).
- **Katalog final: 406 native (406/406), 326 terexercised disc; 154 script-func
  bernama (startup.cmb = pustaka ByPID). Semua target CALL 43 script RESOLVED.** [AM-065]

### BRV-SCT-009 — dispos_c.bin GRAMMAR CLOSED (struktur) — DECODED (semantik param = P1-gameplay)
Loader asli `FUN_8003f87c` (dispos.cmp mode9 / dispos_c.bin mode1; buffer base X,
header 0x20, data@X+0x20) + verifikasi byte-level `out/raw/dispos_c.bin`:
- **HDR BE (0x20B)**: `[u32 fileSize][u32 rel=0x3118][u32 R=639][u32 N=36][u32 v6=0]…`
- **OFFSET TABLE** @ file 0x3138 (X+0x20+rel): **639 u32 BE naik-monoton**
  (0,8,c,14,54,…,0x2a40) = **start tiap record dlm payload [0x20..0x3118)** =
  record ke-i = [PAY+off[i], PAY+off[i+1]) — **variable-length, BUKAN stride** ✓.
- **PASS-1 (pointer patch, sekali per-file)**: utk SETIAP 639 offset-table w:
  `*(X + (w&~3)) += X` = **WORD PERTAMA TIAP RECORD = pointer relatif pool →
  di-absolutkan saat load**. Semua record diawali u32 target.
- **POOL-A [0x2a54..0x3118)** = struktur + nama (PID_/JID_/IID_/SEQ_/EQ_/MTYPE_/SID_/
  always_T_*) + timestamp build `2005-05-26 14:57:20`; **pool suffix-dedup** (pointer
  bisa mendarat di tengah nama = share suffix "RIO"⊂"PID_SENERIO" dll — parsing naif
  per-null SALAH; baca pointer=apa-adanya).
- **GRUP AI TERBACA (record groups)**: pola berulang
  `[EQ_BULLHEAL][SEQ_NOMOVATK100_NOJILL_AND_TALK][EQ_NEARESTUNITMOVE]
  [MTYPE_HEROKILLER + u32 ×5: 0x5010501 0x1004800 0 0 0x1040101]`
  (grup berikutnya 0x5020502/0x6004800, 0x5030503/0x7004800 = **field id+radius aggro
  increment per-spawn**) = **kepribadian AI musuh**: EQ_* = kondisi eq, SEQ_* = urutan
  script AI, MTYPE_* = archetype (HEROKILLER!). Famili hitung: EQ 132, SEQ 95, MTYPE 93,
  PID 17, JID 23 (kelas), IID (item spawn) etc.
- **PASS-2 (intern)**: N=36 pasangan `[u32 ptrPayload][u32 i*12]` @0x3b34 = **subtabel
  36 entri** (event `always_T_05..15`, `ID_TEMP_ON_DIE`, `SID_TURNREGENE`) →
  di-intern (509-bucket `FUN_8000d3ec`) dgn nama tail-region B [0x3118+?] — record
  event script-attached.
- Record len4 tunggal = referensi nama AI/kelas/item utk unit SEBELUMnya (grup
  per-spawn); record len 40-64 = blok param penuh (class+equip+AI).
- **ISI RECORD TERBACA 2026-11-13 (verifikasi ulang pairing reloc→pool)**:
  record berselang-seling **UNIT `[PID_x][JID_kelas][IID_item ×0-3]` ↔
  AI `[SEQ_kondisi ×1-3][MTYPE_archetype]`** (212 record: 98 PID-unit,
  95 SEQ-AI, 7 SID, 5 IID-murni, 4 MTYPE-murni; sel 2c×91/4c×104).
  Contoh nyata: `PID_ASHNARD|JID_PALADIN_L/F|IID_DELPHISHIELD|
  IID_FRAMELANCE|IID_FIRE`; `PID_TANIS|JID_HALBERDIER|IID_ELFIRE|
  IID_CHANGESTONE`; `PID_ZIHARK|SEQ_ALLATK100_DIRECT|
  SEQ_ATTACKRANGEMOVE|MTYPE_HEROKILLER`; AI umum =
  `SEQ_BULLHEAL|..._NOJILL_AND_TALK|SEQ_NEARESTUNITMOVE|MTYPE_HEROKILLER`.
  ⇒ **dispos_c = katalog unit khusus (boss/legendaris) + loadout + paket AI**
  (global /zmap/always/), BUKAN penempatan per-peta (itu pack dispos per-map
  via InstantDisposFirst — lihat SCT-005f).
- **STATUS**: struktur + grammar = CLOSED; **semantik tiap u32 param** (radius aggro,
  id spawn, layer difficulty) = dikerjakan saat implementasi AI (P1-gameplay) dgn
  cross-check dispos.cmp pack per-chapter. [AM-062; isi record AM-097]

### BRV-SCT-005f — RE-AUDIT SEMANTIK BATCH-3 + KOREKSI `InstantDispos` — DECODED
Re-audit 18 impl top-callsite (pseudocode decAll) + call-site literal C00 — satu
KOREKSI kesalahan label di 005c, sisanya konfirmasi + detail layout baru:
- **KOREKSI `InstantDispos(name)`** (BUKAN despawn per-PID — klaim 005c SALAH):
  core `FUN_80105f0c(name,0,flag)` = (1) intern nama (`FUN_8000d354`); (2) baca record
  dispos: field X=+0x0A, Y=+0x0B byte; (3) jika unit PID sudah ADA (`FUN_80016bdc`
  scan 0x188) → handle-nya ditulis ke record gerak (+0x3c); (4) JIKA TIDAK → jalur
  INSTANSIASI unit baru (`FUN_80107d64(clsPtr,x,y,...)`). Call-site literal:
  `MapLoad "Map6_mikata_n"` → `SallySetByGroup "Map6"` → `InstantDispos "Map6_first_n"`
  (nama PACK dispos <map>_<scene>_<diff>, bukan PID) ⇒ semantik = **tempatkan unit
  instan dari pack dispos aktif (spawn-instant, 0=reguler/1=first per 005e)**.
  Kontrak engine: spawn-instant, bukan despawn.
- **FLAG TABLE `0x803307bc` presisi layout** (005e + impl): 96 slot ×
  `{ u32 namePtr[i] @i*4 ; u32 hash[i] @0x180+i*4 ; bit[i] @0x300+(i>>3) }` —
  `regist(n)` = isi slot kosong pertama (top-down) name+hash (return 1);
  `set(n)` = hash-lookup + strcmp → bit ON (return 0); get/clr per 005e.
  Engine: Map<string,bool> sudah memadai; capacity 96/script-context dicatat.
- **BGM slot table** = `DAT_8032ff08` stride **0x30/slot**, flag bit0 = aktif,
  vtable+8 = isPlaying; `BGMFadeOut(ch,frames)`: hanya bila playing → fade;
  **`ch==0` juga memicu `FUN_80146b00()` = clear auto-BGM global** (detail baru).
- **`TimeiShow(x,y,ptr,...)` argc6**: arg3 divalidasi pointer ROM
  (0x80000000..0x817FFFFF) — di luar rentang = no-op return 0.
- **`ENVSilentOff(0)`** = `FUN_80145520()`; `ENVSilentOn(n)` clamp ≥0 →
  `FUN_801455b8(n)` — pasangan level senyap lingkungan.
- **Konfirmasi ulang (tanpa perubahan)**: protokol wait 1-frame-queue
  `DAT_8028d22c rec+0x60`, skip-flag r13-0x7c90 bit0, flag-touch
  `FUN_80013864` (keep-alive per frame), kamera rec float (identitas int per 005e;
  InstantSetMapCamera duplikasi +0x10/+0x14 = replika), FocusHardWait 2-fase +
  wait-table `DAT_8028d19c` + focus global 0x80320060, AllUnitEscape = filter
  kelas (`FUN_80015320(3,buf)` + kelas ptr@+0x190 byte+8) → escape semua lolos. [AM-076]

**Path digit CLOSED** (005c Partial → COMPLETE, impl `FUN_801a1198`):
`UnitMoveDir(h,"…")` parse char-per-char: `'4'`→X−1 (dir **1**), `'6'`→X+1 (dir **2**),
`'2'`→Y+1 (dir **4**), `'8'`→Y−1 (dir **8**) — X=unit+0x19E, Y=+0x19F; huruf
`'D'/'L'/'R'/'U'` = OR bit arah (4/1/2/8) ke flag word TANPA gerak = **ubah facing di
tempat**; list byte diakhiri **0x10 = END**; flag dasar 0x80/0xc0/0x10/0x20/0x200 dari
state antrean (byte 0x30/0x31/0x32 pool). Navigasi akhir → `FUN_8003816c(unit,X,Y)`;
jalur normal = push cmd animasi ke pool `DAT_80296fa8` (`FUN_80184264`), jalur
skip-flag = TELEPORT instan (`FUN_8004fe98/80184750/80050c90`). Engine: kode arah
internal 1/2/4/8 (bukan 0-3) untuk seluruh API gerak. [AM-077]

### BRV-SCT-005g — LIST RUNTIME UNIT (LINKED-LIST PIHAK) + LAYAR BUTAI + PHASE 0x1C — DECODED
- **LINKED-LIST PER-PIHAK (di atas array 160-slot)**: head sekutu =
  `DAT_802CA264 + factionSlot*0xC` (stride 0x0C = 3 ptr), head musuh = `DAT_802CA2A0`;
  rantai via **`unit+0x184` = NEXT** (0 = habis). Engine WAJIB mirror dua indeks:
  array-slot (handle) + rantai list (UI/AI iterasi per-pihak).
- **Bit state `unit+0x1A0` (presisi baru)**: `0x400000` = afiliasi MUSUH; `&8||&0x10`
  = tereliminasi (tidak dihitung daftar); `bit1 (0x2)` = hidup (005b); `0x100000` =
  kebal despawn-phase; `0x4000`+`0x10000` = antrian-fade-out (005b sweep).
- **LAYAR BUTAI (formasi)** = `FUN_8012dfa8`: UI `butai.en.cmp/.tpl` + template baris
  `RectUnitList.en.bin` (loader rect universal `FUN_8003fccc`; 39 baris = layout
  baris daftar, BUKAN data unit) + RID_UNITLIST; mode byte: 0=per-faksi (pakai slot
  list), 1=semua sekutu, 2=musuh, 3=gabungan, >3=per-faksi global; rekaman daftar
  0x24B/entri `{unitPtr,...}`; count=0 → tutup jendela.
- **BMAP PHASE 0x1C** = SWEEP-ELIMINASI: filter kelas (`FUN_80015320(4,buf)` +
  byte kelas ptr@+0x190 idx+8) ∧ hidup ∧ !0x100000 → `FUN_8003ea24(unit,x,y)` buang
  dari peta + mask state `&=0xFF88C7FE` + reset substate (+0x1A4 dsb) → lalu
  `FUN_8014da88(0x80330ACC,1)`, flag 0x8033071A=1, phase++ → 0x1D, 0x8033075C=4.
  Konfirmasi mesin fase BMAP (MAP-002) dgn handler eksplisit fase 0x1C. [AM-078]

### BRV-SCT-005h — SEMANTIK NATIVE BATCH-4 (kamera/unit helpers) — DECODED
- **UnitGetByPID `FUN_800bd5ac`**: `u = lookup(pid)`; 0→return 0, else return
  `u+0x198` (HANDLE u8). Semua native ber-arg handle = indeks slot 1-based (005c).
- **Focus(x,y) `FUN_800bad9c`**: `cam_pan(x,y, BMAP@80330708)` via `FUN_8014cbd8`
  (PAN-TERINTERPOLASI); flag skrip `(-0x7c90)(r13)&1` (fast-skip) → TETAP pan +
  tambahan `FUN_8001885c()` (finalize-segera). Return 0 (non-blocking, pump ulang).
- **InstantFocus(x,y) `FUN_800bae4c`**: pan + `FUN_8001885c` TANPA cek flag = lompat
  instan kamera. **InstantSetMapCamera(x,y,z) `FUN_8019ffbc`**: tulis slot kamera
  `s=cam_slot()`: `+4=+0x10=x`, `+8=+0x14=y`, `z→FUN_800193cc(z)` (zoom/tinggi),
  lalu `FUN_800187f0()` commit. Slot = record terpisah dari BMAP (bukan 80330708).
- **ABI float skrip TERKONFIRMASI ulang di 4 native**: arg float u32 didekode
  `(double)CONCAT44(0x43300000, arg^0x80000000) − 524288.0` → pola = **f32 di-XOR
  sign-bit + bias**; integer push biasa TIDAK lewat idiom ini (WaitM pakai skala
  r2−0x6e6c utk ms→frame). Port: decoder arg skrip wajib dua jalur (int vs float-bias).
- **Iterator helper `FUN_80016498`** (phase/sweep): arg-api(3) ambil byte-flag per
  nama-unit → loop slot 1..0x9F: `pid(u+0x188)!=0 && flag[nameref+8]` → aksi
  per-unit. Pola universal command "semua unit满足 syarat" — mirror sbg iterator
  filter engine.
- `FUN_800b36d8` = pembuka bank suara (session 0x80319aa8, path "/pool") — titik
  masuk SE utk PoC Android saat AUD. [AM-079]

### BRV-SCT-005j — GOLDEN-TRACE BER-STRING VMSTR: GRAMMAR SKENARIO CH1 LENGKAP (args string ter-resolve statis) — DECODED
- **`vmtest --VMSTR`** (edit `poc/tools/vmtest.cpp` histHook; env `VMSTR=1` +
  `VMGOLD=csv`): golden trace kini menulis STRING RAM ter-resolve di samping hex
  tiap arg ⇒ **dekode arg native statis tanpa device** (intern bump base ≈0x45800,
  blok ±0x40). Replay C01 Opening0 60k frames = 4525 panggilan natives.
- **Rantai pembuka Ch1 (verbatim, frame-akurat)**: `MoviePlay("movie/s01.thp")` →
  `ChapterMoviePlay(1)` → `MapLoad("bmap01")` (bukan "map01"!) → `SetGrid(0)`
  `SetCircle(0)` `SetOutLink(0)` `UnitFocusOff` → `InstantFocus(4,7)` →
  `InstantSetMapCamera(-55,-?0x37=55,18)` → `SallySetByGroup("bmap01_mikata_c")` →
  `InstantDisposFirst("bmap01_mikata_c")` `InstantDisposFirst("bmap01_first_c")` →
  `InstantDisposFirstSetMode("bmap01_first_g_n","_g_h","_g_m")` ×2 (arg2
  `bmap01_first_n/h/m`) → `UnitSetPosDirByPID("PID_BOLE_MAP1", 15, 13, dir4)` →
  rotate×3 (`PID_IKE` dir1, `PID_MIST` dir2, `PID_GREIL_MAP1` dir8) →
  `UnitGetByPID`×3 → `UnitSetWeaponAway`×3 → f1499 walk-slow paths **numpad**:
  IKE `"4"`, MIST `"6666"`, GREIL `"444442"`; f2706 `UnitFadeIn` +
  `UnitSetPosDirByPID(BOLE, 9, 0, 4)` + path `"222224"`; kamera focus (5,6)/zoom
  0x10; BGM **bernama**: `BGMPlay(2,"BGM_EVT_IKE1")`; f3453
  `DisposContinueSetMode("bmap01_first_n","_h","_m")`; penutup f4400
  `UnitGetXByPID/UnitGetYByPID(PID_IKE)` (script baca posisi utk trigger) +
  `SetOutLink(1)` `Comeback`. ROTASI dir arg = kode numpad mentah (1/2/6/8) +
  0x10e/0x190 = f64XOR-bias (jalur float, SCT-005h) utk arg2. [BRV-MAP-005 xref]
- **dispos per-peta bmap01 (out/bmap01/dispos_{c,h,m,n}.bin, 905/586/566/566B,
  grid-112 stride variant)** pool ASCII = KEBENARAN CH1: squad mikata 4 unit =
  `PID_IKE`+`PID_MIST`+`PID_BOLE_MAP1`+`PID_GREIL_MAP1`; kelas
  `JID_FIGHTER`(Ike)+IID_PRACTICESWORD, `JID_CLERIC/F`(Mist), `JID_RANGER`(Bole),
  `JID_HERO_G`(Greil); pembuka dikunci `SEQ_NOMOVE` `SEQ_NOATTACK` `SEQ_NOHEAL` +
  `SID_IMMORTAL` (Bole/Greil abadi = reason mereka tak pernah mati di opening);
  grup event: `bmap01_mikata_c` `bmap01_first_c` (+varian h/m/n)
  `bmap01_event_bole_c` `bmap01_ending_c` `Abmap01_date_c`; build ts 2005-02-23.
- **ENEMY Ch1 BUKAN file ini** (0 PID musuh; 48 PID root dispos_c = semuanya
  karakter cerita) ⇒ gelombang bandit disuplai via `DisposContinueSetMode` yang
  menunjuk tabel kelas generik (JID_*) — koordinat spawn presisi = **butuh capture
  runtime** (blocked T-RE-CAP1); engine v1 tetap pakai pool exit maplinkAt.
- Impl: tooling replay kini jadi oracle string statis (setiap bab bisa dibaca
  tanpa device: `VMGOLD=... VMSTR=1 vmtest dir Cxx.cmb`); wave schedule engine
  diberi label data-gap yang sama. [AM-053]

### BRV-GFX-002 — Facedata SLOT-GRID EKSPRESI WAJAH CLOSED — DECODED
- **Tail facedata.bin (u32 stream @0x572C, 358×3)** = index pool gambar
  global; terukur 358/358: **slot2 = slot1+4, slot3 = slot1+28** (konstan —
  koreksi interpretasi "layer idx" awal). Base sequence = **0, 12, 44, 76,
  108, 140 … 11404** ⇒ alokasi pool: ANNA=12 slot, **32 slot per face**
  sisanya (urutan roster = urutan FST /Face/*.cms; TPL-count riel 14/img
  umumnya, maksimum observed 11436).
- Interpretasi: tiap face punya 3 SET ekspresi = {base+0, +4, +28} — 4-img
  per set konsisten dgn struktur file (BRV-GFX-001d: img0 base + lapisan
  16×32); ANNA pakai {0,4,8}. Pemetaan sub-in-file utk set +28 valid saat
  runtime-load (registry nama global), dikonfirmasi ulang saat capture.
- **Loader facedata = anggota keluarga pointer-patch**: `FUN_8007c408` =
  `FUN_8003fccc(s_face_facedata_bin)` + loop `FUN_8000d3ec` intern nama
  .cms baris 1 (baris stride 0x20, count di hdr-0) — SATU lagi konfirmasi
  REG-001 utk file keluarga [size/tabOff/cnt/sub].

### BRV-SCT-005 — SEMANTIK NATIVE TOP-6 CLOSED — DECODED (revisi: lihat KOREKSI)
- **KOREKSI (2026-11-13, supersede klaim awal "float sign-bit")**: pola
  pseudocode `CONCAT44(0x43300000, p^0x80000000) - bias` = **IDENTITAS
  integer biasa** — TERBUKTI numerik di SCT-005e (p=4,7,-55,55,18 → hasil
  persis; bias artefak decompile PPC/Ghidra float-int). Engine TIDAK perlu
  transform argumen. Baris "konvensi emas" versi awal entri ini SALAH —
  baca 005e sbg kebenaran.
- Tabel semantik (argc dari katalog; impl pseudocode decAll):
  | Native | argc | Semantik |
  |---|---|---|
  | `WaitM` | 1 | arg=ms float; `(arg - t0)/60` = FRAMES (60fps); simpan
    frames-1 di node-wait `+0x60` (`DAT_8028d22c` list); flag
    `r13-0x7c90&1` = mode fast-skip → return 0 (selesai) else 1 (yield) |
  | `MapLoad` | 1 | nama/id peta → `FUN_8014c7c0(0x80330708=base BMAP, arg)`
    loader peta; selalu return 1 (blocking-once) |
  | `UnitGetByPID` | 1 | lookup unit (ptr via `FUN_80016bdc`); kembalikan
    BYTE `unit+0x198`; unit hilang → 0 |
  | `InstantDispos` | 1 | `FUN_80105f0c(arg,0,0)` spawn dispos即时 (varian
    Instant = tanpa delay; `+0/+0` = mode/queue) |
  | `InstantSetMapCamera` | 3 | (x,y,z) float → rec kamera
    (`FUN_80018adc`): x→+0x04&+0x10, y→+0x08&+0x14 (pasangan duplikat
    = prev/cur), z→`FUN_800193cc` (zoom) + update |
  | `Focus` | 2 | (x,y) float → `FUN_8014cbd8(x, y, 0x80330708)` geser
    kamera ke titik; skip-flag &1 → +`FUN_8001885c` (snap instan); return 0
    = non-blocking |
- Helper global yang jadi kontrak engine: base BMAP `0x80330708`;
  flag fast-skip `r13-0x7c90` (mode demo/skip scene); clock t0
  `r2-0x6e68` + divisor 60fps `r2-0x6e6c`; list node wait `DAT_8028d22c`.
- Implikasi P2: wait-timer per-thread frames + kamera x/y/z + skip-flag =
  minimal-state utk replay capture (T-RE-CAP) presisi.

### BRV-SCT-006 — TUT_DATA(.bin/_en) INDEKS TUTORIAL DISC-ROOT CLOSED — DECODED
- Keluarga tabel `[u32 size][u32 tabOff][u32 cnt][u32 sub]` anggota #7/#8
  (`3990/3116/186/5` JP, `4206/3292/196/5` EN); table = u32 pool-offsets
  stride 4; pool ASCII di [0x10..tabOff).
- **50 step tutorial bernama `tut_NN_<Mechanic>`** — `NN` = chapter index
  tutorial (00..12): `tut_00_Hero/MapMenu/Turn, tut_01_Attack/Idou/Item/
  Lvup/Skill/Tackle, tut_02_3weapon/Inherit/Rescue/Riding/Trade/Visit/
  WinSeiatsu, tut_03_EnemyArea/Recovery/Terrain/WinZenmetsu,
  tut_04_Aspect/IDAttack/SFX/Setup/WinDefeat, tut_05_3Magic/Disposal/Help/
  Parameter, tut_06_Sakuteki/Trade2/WinDefense, tut_07_Defection/Equip,
  tut_08_Box/Neutrality/Talk, tut_09_Command/Support, tut_10_Equip2/Fly/
  Mutate/Request, tut_11_ClassChange/Door/Steal, tut_12_WinGoal`
  (IDou=gerak, Sakuteki=tujuan, WinSeiatsu/Zenmetsu/Defense = win-type
  JP-romaji — bocoran kondisi menang tiap tutorial!).
- **94 key pesan `MT_*`**: `MT_C00..C18` + varian `_s<paramSJIS>` (pesan
  per mekanik), `MT_ALPH_A..W` (glyph utk input nama?) → link ke
  `mess/common.m` (keluarga .m sudah Decoded).
- Engine: daftar trigger tutorial lengkap TANPA reverse runtime; EN vs JP
  cnt 196/186 = +10 entri (varian/preset param) — lokal wiring = P2.

### BRV-AUD-004 — SOUNDROOM.BIN KATALOG BGM TERBACA — DECODED
- `[624/508/21/0]`; region record stride-8 `u32 [sid][trackId]` di [0x10..)
  + pool **21 nama resource `RID_*` SJIS** + table u32 offset step 8.
- **21 judul TERDECODE SJIS (TextDecoder shift_jis)**: クリミアの復興1/2,
  ゴルドアの竜鱗族, 外観-ガリア/キルバス/クリミア/デイン/フェニキス/
  ベグニオン (6 negara!), 決意, 再会と別離-A/B, 進軍, 聖天馬騎士,
  切なる願い02, 忠誠と誓言, 塔, 崩れる城A/B/C, 明かされし過去.
- Nilai: roster BGM showcase + **track-id** utk sound-test engine (P6);
  roster penuh via SEQ_* di dbx/dispos (sudah ada); nama display = JP
  (canon) — label EN tidak ada di disc (bukan asset hilang).

### BRV-SCT-011 — TALKRESUME CLOSED — DECODED
- `TalkResume()` argc0 → `FUN_8011d650`: cari record antrian-talk global
  (`DAT_8028fe50` pool), global state talk `r13-0x7348`: **clear bit4
  (0x10 = pause/tunggu-input)**, record `+0x30 += 2` (message-stream u16
  token advance), lalu: skip-flag `(r13-0x7c90&3)==0` → pump 1 frame
  (`FUN_80013864` yield); kalau SKIP-mode → flush jalur cepat
  (`FUN_8011ded8`) + tutup record (`FUN_8000c60c`) bila bit-0x10 masih
  clear. = **resume/advance mesin dialog**: engine model = queue record dgn
  pc u16 + pause-bit + fast-forward saat skip.

### BRV-UI-001 — RECTUNITLIST = LAYOUT PANEL-UNIT PETA CLOSED (struct) — DECODED
- Konsumen: init modul panel `FUN_8012dfa8` (window module) — load
  `/window/RectUnitList(_en).bin` (loader keluarga `FUN_8003fccc`) +
  `butai_en.cmp/.tpl` (background panggung) + `RID_UNITLIST`. Fungsi:
  **panel daftar unit utk skip-target/urutan** — hitung unit aktif per
  team-list via linked-list `unit+0x184` + filter flag `unit+0x1a0`
  (bit3|bit4 = excluded; daftar `[MODE4]` versi team).
- Format = keluarga pointer-patch (R=39, N=1): **25 entry-layout rec 32B**
  (ptr-table 0x14..0x6C step4) + **14 rec template stride 36B** @0x13C
  (semua pointer → SATU nama `RID_UNITLIST` + ref `window/butai.tpl`).
- Isi rec u16-BE = pasangan rect + parameter: konstanta layar `1280`
  (2×640 hi-res), `256/528/576/240/120/560`, **pitch grid slot = 16 slot
  × 256** (field `... 0x10 0x100` berulang di semua 14 template) —
  layout list-unit: 16 slot vertikal pitch-256 per panel.
- Engine: rect panel list-unit = tabel, BUKAN hardcode; template per
  kondisi (14 varian = state/layar berbeda). Pixel-exact formula = P2-UI.

### BRV-CHR-003 — TABEL SUPPORT/RELIANCE/DIVINE FE8Data — DECODED (struct)
- **`KiznaData@0x14674` = 36×[PID_A][PID_B][u32-flag] triples (12B)** —
  **DAFTAR PASANGAN SUPPORT** dari disc (`HIBUCCHI×MAP21_DKN_3`,
  `LETHE×CEPHERAN_FAKE`, `GASIRAMA×MIST_EV`… flag 0x105xxxx/0x10axxxx =
  tier/kondisi) ⇒ prasyarat data sistem support engine (P5) TERTUTUP.
- **`RelianceData@0x12dc4` = [u32 41] + 41 rec (var-stride, region 6192B)**
  stream `[PID][u32-bonus][PID][u32-bonus]...` = efek support per pasangan
  (0x8-style bonus codes) — role presisi byte = P4-gameplay.
- **`DivineData@0xf4ac` = [u32 9] + triples [scene/fx-name][u32]**:
  `map30_castle02 eqB | sky_01 trialmap_03 | trialmap_02 breath infinity
  flutter shootonly` = sekuens FX ritual pedang pengikat (bab 30).
- `GroupData@0x145f4` = 31×[MG_/MCT group id] (grup dialog/menu) — kecil.
- `GameData@0xf51c` = **28B** blok numerik murni (global balance) — field
  map = P4; `BattleSkyData@0x12d78` = 76B [name][bmap29 refs] modifier.
- **SWEEP FE8Data SELESAI: 12/12 chunk named-chunk terkarakterisasi**
  (PersonData/JobData/ItemData/SkillData/BattleTerr/Terrain/Chapter/Sky/
  Divine/Game/Group/Kizna/Reliance + 100 SID_*).

### BRV-BTL-010 — ChapterData = TABEL SCENE TEMPUR PER-PETA CLOSED — DECODED
- `ChapterData@0x13570` = **[u16 n=44] + 44 rec × 96B** (region 4224B presisi).
- **kolom +0x04 = nama scene-tempur**: rec6.. = `bmap01 bmap02 bmap03 bmap04
  bmap06...bmap26` (+ varian `_2` = chapter replay/part2) ⇒ **lookup
  battle-background 3D per peta** (bmap01 = Ch1 ✓); rec0-5 scene non-bmap
  (beastsamul/begnion_room/bird/blackonly = tutorial & special);
  rec35-43 = ekor region SID_/help (final-only/arena?).
- **kolom +0x48/+0x4c = modifier langit/cuaca**: `cloudy_01 sandstorm_01
  fire armor finalonly blackonly beast` + cross-ref `bmap29/bmap30`
  (link BattleSkyData) ⇒ engine: scene = (background bmapNN, sky-modifier).
- Satu-satunya tabel bab struktural di FE8Data — JUDUL bab TIDAK ada di sini
  (tetap via video SYS-003); `bmap05` tak muncul sbg rec (disk punya —
  dicatat, minor).

### BRV-REG-002 — FE8DATA SUB-TABEL: SkillData/JobData STRUCT CLOSED — DECODED
- **`SkillData@0xe374` = [u32 n=98] + 98 rec × 40B**, kolom (u32):
  +0 ptr **SID_*** (nama skill), +4 ptr (alias/MSID pool), +8 ptr
  **MSID_*** (display JP), +0x10/+0x14 ptr **`Mess_Help_skill_*`** (teks
  help in-game — link langsung ke mess/common.m), +0x18 ptr Mess_Help2,
  +0x1c icon (numeric), **+0x20 flags = (recIdx<<24)|mode** (ID implisit),
  +0x24 flags2, +0x28/+0x2C = **runtime fn-ptrs (reloc)** = handler skill.
  ⇒ RANTAI BONUS TERKUNCI: terrain → BattleTerrData → SID_* → SkillData →
  handler; contoh SID_WEAPONDESTROY/SID_ABSMOVE/SID_WINGSHIELD ✓.
- **`JobData@0x6fa0` = [u32 n=115] + 115 rec × 100B**: +0 **JID_*** (kelas),
  +4 **MJID_*** (promo-target), +8 **MH_J_*** (help), +0x18 region
  konstan/kamera? +0x44 region scene — field move/range numeric BELUM
  terpetakan (P4); registry nama kelas 115 valid semua.
- ItemData@0x9c90 = **[u32 189] + 189 rec × 96B** (stride 96 TERKONFIRMASI:
  4+189×96=16948 ✓; reloc densitas tinggi; col0-3 = ptr nama IID_/MIID_/
  MH_I_/help). **Numeric zone = 0x40..0x5F (20×u8)**; kolom terdepan (diff
  IRON vs STEEL pair, 2026-11-13): **+0x41 = Mt** (IRONAXE 8 ✓ BTL-007
  ground-truth, STEELAXE 16, SILVERSWORD 15), **+0x42 = durability**
  (STEELAXE 35 ✓ capture-observed 35, IRONAXE 45 ∈ {44/45} observed ✓),
  +0x43..+0x48 = rank/wtype/hit modifiers, +0x54..+0x5B = byte-pair
  konstan-per-tier (0x05/0xFB efek?) — CATATAN: sebagian record (IRON-
  SWORD/LANCE, SILVERAXE, STEELLANCE) pola byte TIDAK selaras = walk()
  salah-tembak nama (suffix-dedup collision) ⇒ pin akhir field = **UNIP
  save capture** (SVT-002: item = [u16 id][u8 dur] — dur byte save ↔ scan
  tabel = pin instan semua durability; TAMBRAH ke sheet device: tarik
  save.bin via SVT-001 IO).

### BRV-REG-001 — KELUARGA POINTER-PATCH-REGISTRY CLOSED (FE8Anim/FE8Effect/cp_data) — DECODED
- **Semantik loader universal DEFINITIF** (`FUN_8003fccc`→`FUN_8003f87c`,
  dibaca penuh 174 baris): payload base = block+**0x20**; **tabel pointer-patch
  @ base+(hdr[4]&~3)**, `cnt` × u32 = **POSISI slot-pointer (payload-relatif,
  4-aligned)**; tiap slot berisi offset pool → di-reloc `+= base` in-place
  sekali (idempoten via checksum 7-byte + flag di block+0x18/0x1F). Pool
  strings **suffix-dedup** (pointer bisa di TENGAH string — resolver WAJIB
  walk-back ke batas pool). Ini penjelasan resmi "tabOff aneh" di semua
  anggota keluarga — `bin/tablereg.mjs` mengimplementasikan verbatim.
- **`FE8Anim.bin` 508 registry** (cnt=1016 = 2 slot/record): **AID_* nama
  anim-set ↔ DIR model unit** (`knight/ general/ mage2/ lofa/ archer2/` …
  74 dir; 2 record anomali pair-name) ⇒ **tabel routing aset
  anim↔class↔model engine**, sambungan langsung ANM-005 + roster pack.
  `docs/data/FE9_ANIM_REGISTRY.csv`.
- **`FE8Effect.bin` 316 registry**: nama **EID_*** pool efek/battle-SE
  (EID_ATTACK, EID_HIT, EID_COUNTER, EID_CLOUDOFDUST1..5, EID_CLASSCHANGE,
  EID_MACHIBUSE, EID_LIGHT_LONG_2D, 2D-varian). `docs/data/FE9_EFFECT_REGISTRY.csv`.
- **`cp_data.bin` 396 refs** (sub=134): census **SEQ_*×182 + IID_×75 +
  MTYPE_*×64 + PID_*×39 + nonprefixed×36** = **chapter-checkpoint seeds**
  (kondisi AI + item + tipe unit + PID); handle global disimpan
  **r13-0x74A0 = 0x80367800** oleh `FUN_800fc678` (lalu init `FUN_8010f970`/
  `FUN_80129374`). `docs/data/FE9_CPDATA_REFS.csv`.
- **`client.bin`** (18468B hi-entropy): dimuat `FUN_801f6260/801f6294` via
  ROMFS-path loader (FUN_8005c84c size / FUN_8005c268 read, clamp min
  0xE400=57KiB) ke struct+0x30/34 — payload opaque, BUKAN keluarga table;
  kandidat modul link/GBA-player → **Deferred P2** (tidak menyentuh Ch1).
- **`/ymu` FST census disc**: 1333 file = ga1130/cmp86/tpl117, **.bin = 0**
  (phantom-check handoff tertutup; tak ada baris matrix untuk dihapus).
- **VALIDASI ROUTING (reg↔disc)**: 71/74 dir registry = **subfolder `/ymu/`**
  persis (`/ymu/mage2/atk1_n.ga` dkk; 3 miss = 3 record anomali pair-name
  ter-inferensi; 15 ymu-extra = direktori legacy FE8-style tanpa entri AID:
  bishop/mage/thief/pegasu/dragon/tiger/dummy/crow/heron/...). ⇒ **rantai
  aset engine CLOSED: AID(anim-set) → /ymu/<dir>/ (body gs + anim ga + tex
  tpl) → model unit**; tinggal lookup AID per unit dari table unit (P4-CHR).

### BRV-SCT-005p — SEMANTIK NATIVE BATCH-9: UNIT-SNAPSHOT-STACK + RANK-SUFFIX DIFFICULTY + RECT-OFFSET — DECODED
- **UNIT SNAPSHOT-STACK CLOSED**: `UnitsPop()` `FUN_800b7410` = **capture**:
  flush unit (`FUN_800162b8`) → duplikasi buffer snapshot global
  **(r13-0x7560) = 0x80367840** → PUSH ke stack-manager `DAT_803206EC`
  (`FUN_800c2930`) → refresh unit/terrain → free;
  `UnitsPush_Uninitialize()` `FUN_800b74ac` = **restore**: alloc **0xA000**
  (40KiB, allocator-tag `DAT_8026F64E`="units") → set 0x80367840 →
  `FUN_80016a3c(&buf)` = LOAD unit-list dari buffer → `FUN_800c2980` POP-stack
  → refresh. ⇒ **rollback-state unit** untuk preview/tutorial/percobaan event;
  40KiB < 160×0x280 ⇒ snapshot = subset field (bukan dump penuh) [INFERENCE].
  Engine: stack-of-g_u-snapshots (g_u 160×record sudah kompatibel) — TODO kecil.
- **`InstantDisposRank(name)` `FUN_801a1884`**: intern → **CONCAT suffix
  difficulty** atas byte `0x803307AE`: v==1→**"_h"**, v==0→**"_n"**, v<4→**"_m"**
  (string pool 0x8027B35E/61/64 ter-verifikasi-bytes) → spawn
  `FUN_80105f0c(name',0,0)` ⇒ **variant-pack per difficulty `_n/_h/_m`** —
  SELARAS dgn pack shop `shopitem_h/m/n` (AM-049!) — engine: dispos pack
  per-difficulty + suffix lookup.
- **ENCODING DIFF-BYTE 0x803307AE = PARTIAL** (kontradiksi antar-bukti, DICATAT
  jujur): suffix ⇒ 0=N,1=H,2/3=M; predikat asm ⇒ Hard()=TRUE hanya v0,
  Maniac()=TRUE v∈{0,1}, Normal()=TRUE v∈{0,3}; AchieveSet ⇒ v2 ÷2, v3 ×1.5.
  Salah satu keluarga memakai byte utk peran LAIN (mode E3/demo?) ATAU
  predikat dipakai ter-inversi script. Engine SUDAH implement literal-asm;
  resolusi butuh capture menu-difficulty (T-RE-CAP backlog).
  **UPDATE 2026-11-13 (shop loader):** konsumen ke-3 `FUN_8016a028`/
  `FUN_8016a0cc` (pemilih `shop/shopitem_{n,h,m}.bin`) = v0→_n, v1→_h,
  v2→_m, v3→**_n**, v>3→**tidak load sama sekali**. Konsumen SAMA-SAMA byte
  ini tapi peta-suffix BERBEDA (dispos: v<4→_m incl. 3; shop: 3→_n; shop
  selaras predikat Normal()={0,3}) ⇒ **bukan kontradiksi global — PETA
  PER-MODUL**; engine WAJIB mirror switch per-pemanggil (jangan satu
  pemetaan global). Resolusi makna v3 tetap = capture menu.
- **`RectMoveOffset(...)` `FUN_800b9c14`** = tail-call `FUN_800b9cd4(f32
  GOT-0x6e5c)` — geser jendela rect dgn offset-global (UI minor; 5 argc di
  registrar vs 0 terpakai = dead-args).
- Registrasi ter-verifikasi ulang via grep registrar (UnitsPop/
  UnitsPush_Uninitialize/InstantDisposRank/RectMoveOffset + Hard/GCST nama
  DAT_ pool — pasangan nama↔impl catalog 100% benar).
- **Sisa misc freq-tinggi CLOSED**: `BMGetPhase()`/`BMGetTurn()` = getter
  byte BMAP+0x1C/+0x20 (LAB, selaras SCT-005g); `RectSet(win,x,y)`
  `FUN_800ba38c` = guard ptr-window [0x80000000,0x817fffff] →
  `FUN_801090c0` lookup → layout rect (UI pass); `BGMQuietOn()`
  `FUN_800b7c28`→`FUN_80145828` (routine ber-POSIX-eh dgn arg string pool
  0x80292558/6C = quiet-track fade; family-slot sudah SCT-005f);
  `NetuzoInit()` `LAB_800b8534` = INIT sound-subsystem: zero global
  `0x802FFDB8` (+hdr slot-BGM region) + flag +0x60/61 + clear 2 word →0.
- **GAP-ANALYSIS TUTUP**: seluruh native freq≥8 kini terekstraksi
  (kecuali keluarga TUTORIAL + GMapPoint = fitur P2 yang memang di-defer;
  &LAB sisa = freq rendah, recipe dump-asm SCT-005n tersedia).
  Cakupan semantik corpus (spec+scoreboard+engine) = **127/390 named**.

### BRV-SCT-005o — SEMANTIK NATIVE BATCH-8: KAMERA-SAVE, BANGUNAN=AKSI-TILE, RECT-UI, DEATH-TALK — DECODED
- **`CopyMapCamera()` `FUN_8019ff68`**: snapshot kamera aktif
  (`FUN_80018adc`=slot aktif, +4/+8/+0xc = x,y,z f32) → global
  **0x80330E94/98/9C** + latch `DAT_80330E90=1`.
  **`PastMapCamera()` `FUN_8019feb8`**: restore bila latch; non-FF: tulis
  +4/+8/+0xc langsung; FF (bit0): tulis +4/+8 **dan duplikat ke +0x10/+0x14**
  (start=target ⇒ tanpa-interpolasi) + `FUN_800193cc(zoom)` = terapkan zoom.
  ⇒ pasangan save/restore kamera sekitar event-camera. [AM-083]
- **`SetEventCameraPrec(p)` `FUN_801a2004`** = FIELD-WRITER antrean aksi
  `DAT_80296F90`: pola baku `FUN_8000c0c0` lookup → absent→`FUN_8000caf0`
  create-default (+0x30=1 enable, +0x33=speed-global `0x803307A8`, +0x36=1,
  +0x3c=1) lalu **rec+0x3C = p**. Keluarga satu-field: `UnitFocusOff()` =
  rec+**0x30=0** (disable), `UnitFadeOff()` = rec+**0x32=0** — koreksi
  interpretasi: +0x30 = flag-aktif rekaman (bukan "enable baru").
- **Bangunan = sistem instance + ring aksi-tile (satu sistem dgn peti/visit)**:
  `BuildInstHide(x,y)` `FUN_8019f914` / `BuildInstShow(x,y)` `FUN_8019f9b4`:
  lookup layer **0x80000** (bangunan) mask 0x10 → **ENQUEUE aksi 0x29**
  (`FUN_8010feec`) vs **DEQUEUE 0x29** (`FUN_8010fe08` = hapus pending
  serupa) + `FUN_80049c8c(rec,0/1)` = hide/show-flag instance +
  `FUN_80044450(map,1)` DIRTY-map + `FUN_80016180()` = **refresh-terrain**
  ⇒ bangunan hide/show mengubah pathing.
  `BuildDestruction(x,y)` `FUN_8019eaf4`: lookup mask **0xA00** → enqueue
  **aksi 0x25** → anim `FUN_8012adac(x,y,handle)`+re-arm (FF = instan).
  **Enum ring aksi-tile @map+0x204**: 0x23=peti-dibuka, 0x24=visit-out,
  0x25=bangunan-runtuh, 0x29=building-show/hide (+ aksi lain oleh engine).
- **`SetShooter(x,y,dir,v)` `FUN_800b82d8`**: `FUN_80019e04(v)` →
  **enqueue-varian-5-arg** `FUN_8010fdb8(&map,x,y,arg3,uVar)` = pasang
  penembak-tertandai ke instance sistem (counter-target).
- **Rect UI**: `RectKill(target)` `FUN_800ba4a8` = destroy jendela UI:
  jika arg=pointer (0x8000_0000..0x8180_0000): konstanta string
  `RID_GMAP` ⇒ jalur khusus worldmap `FUN_801cf760+FUN_801cfa44`, else
  **`FUN_80109020(ptr)`** = tutup-window-rect; arg-int = no-op.
  `RectSetCurve(rect,slot,x?,a,b,c)` `FUN_800ba298`: window lookup
  `FUN_801090c0`; **slot valid 5..7**; float XOR-bias (high-word ^0x8000,
  basis 0x4330=`175.0`) dikurangi konstanta global (-0x6e68) dan dikali
  skala (-0x6e60); sentinel `0x75BCD15`=1.234567e9 = "jangan ubah" ⇒
  kurva-animasi-rect (bezier) — Partial (peran slot 5-7 = follow-up).
- **`DefaultDieTalk()` `FUN_800ba9bc`**: global **`0x80330EAC` = unit
  TERBARU-MATI** (pid-nya via +0x188 → data unit → tabel death-talk
  `FUN_8000d354`) ⇒ engine wajib set "lastDied" tiap unit tumbang (dipakai
  dialog-kematian default + dispos event).
- **Engine**: CopyMapCamera/PastMapCamera = save/restore posisi kamera scr
  (termasuk jalur FF start=target); BuildInst*/BuildDestruction/RectKill/
  RectSetCurve/SetShooter/DefaultDieTalk = handler log/sukses-minimal —
  instance-bangunan+ring-aksi-tile masuk backlog P2-MAP (enum 0x23/24/25/29).

### BRV-SCT-005n — SEMANTIK NATIVE BATCH-7 (+DUMP-ASM STUB &LAB, r13=0x8036EDA0 TERDECODE) — DECODED
- **Metode baru**: stub `&LAB_*` (natives tanpa pseudocode decAll) di-dump via
  headless `analyzeHeadless -import main.elf -processor PowerPC:BE:32:default`
  + postScript Java (`DisassembleCommand` per alamat, resep tersimpan
  `fe9-re/tmp_stubs.ps1`+`tmp_scripts/DumpStubs.java`). **r13 (SDA base) =
  0x8036EDA0** (dari `lis r13,0x8036; ori r13,r13,0xeda0` @0x80003334) —
  semua `(r13+n)` yang selama ini `unaff` kini terkonversi konkret. [AM-082]
- **`AchieveSet(name,val)` `FUN_801b7818`**: tabel ACHIEVE = ptr @0x80368254,
  indeks-u32 @0x80368250, rekaman stride **0x40**, field nilai **+0x200**
  (⇒ rekaman extends 0x200+ — tabel per-rekaman besar); **scaling difficulty
  di sini**: mode-byte `0x803307AE`==2 ⇒ val/=2, ==3 ⇒ val+=val/2 (⇒
  objective-score EXP-like); init ptr oleh `FUN_8008794c`-family (`-0x6b4c`
  writer). lookup nama = `FUN_80234450` (intern).
- **Dialog-pilihan CLOSED**: `DialogResult` `FUN_8006f014` = **i8 di STACK**
  (r13-0x7fb8 → Ghidra-stack = state dialog global; wrapper `DialogGetRes`
  `FUN_800b8d34` = tail-call); `Dialog2Items(a,b)` `FUN_800b8e5c` = **menu 2
  item** — validasi pointer-string (0x8000_0000..0x8180_0000), `FUN_8006f10c`
  buka + `FUN_80013864` re-arm ⇒ skrip percabangan (store result→`get`).
- **`SFXPlay(se)` `FUN_800b7a2c`**: guard `(u32@0x80367210 &3)==0` lalu
  **`FUN_801475fc(se,100)`** = main SE bank-id `se` vol100 ⇒ id = entri bank
  /pool (link SEQID SVT-002 = domain sama).
- **SKIP/FF-STATE u32 @ 0x80367210 (=r13-0x7C90)**: bit0=fast-forward (gerbang
  FocusWait &1, VisitOut &1, SFXPlay &3), `DisableSkip()` `LAB_800c0ae0` |=0x4,
  `EnableSkip()` `LAB_800c0acc` &=~6. **`GCST()` `LAB_800bbc50` = read-u32 word
  ini** (state scene penuh). Gerbang SFX/BGMPlayVol &3≠0 ⇒ SE di-silent saat FF.
- **BMVars CLOSED**: `BMSetValue(i,v)`/`BMGetValue(i)` `LAB_800bcb0c/24` =
  **u16[16] @ 0x80330708** (`sthx/lhax idx<<1`) — var per-peta yang ditulis
  script; ⇒ struktur BMAP-global 0x80330708 punya **BM0..15 di +0x00..0x1E**
  (temuan "BMAP pointer" lama = field dalam struct yang sama, rekonsiliasi:
  diff-byte 0x803307AE = **+0xA6** dari base ini).
- **Mind-state**: `MindGetMe()` `LAB_800bcd58` / `MindGetTarget()`
  `LAB_800bcd34` = **handle unit (u8 +0x198)** dari `[me@0x802EF4C8,
  target@0x802EF4CC]` — pasang unit-AI aktif saat giliran mind (engine AI harus
  expose dua slot ini).
- **Event-cursor @ 0x8031FFC0**: `EventGetX/GetY` `LAB_800bcaec/adc` =
  i16 +0/+2; `Check()` `LAB_800b7200` = byte **+0xA** state event-point
  (state konfirmasi prompt event).
- **`GetBattleType()` `LAB_800bbc40`** = u32 @ **0x80331B54** (tipe battle bab;
  ditulis loader mode).
- **Predikat mode `Normal/Hard/Maniac` `LAB_800b790c/8f0/8d4`**: ketiganya baca
  byte **0x803307AE** (=BMAP+0xA6) dengan pola cmp: Normal()=byte∈{0,3},
  Maniac()=(2−v)≥0 non-overflow (v∈{0,1} true), Hard()=(1−v)≥0 (v==0) —
  ⇒ ENCODING byte masih **Partial** (AchieveSet: 2→÷2, 3→×1.5); butuh capture
  menu-difficulty + observasi runtime untuk memetakan nilai. Engine: store byte
  + 3 predikat apa-adanya.
- **BGM-variants CLOSED** (slot-record stride **0x30** base **0x8032FF08**,
  flag+0xC bit0, vtbl+0x8 method[1]=isPlaying): `BGMCont(slot,v)` = if
  playing→`FUN_801487d0(rec,v)` + slot0→stop-all + ramp vol100→v (continue
  dengan fade); `BGMMute(slot,v)` = if playing→`FUN_80148888(rec,v)` + slot0→
  master-mute `FUN_80146b00`; `BGMQuietOff()` = `FUN_801457b4` instan;
  `BGMPlayVol(slot,name,vol)` = play-with-vol `FUN_8014832c` (skip-guard &3) +
  slot0→`FUN_80146af0`; `ENVNoisyOff()` = `FUN_801453f0`.
- **`ForceGetFirst(force)` `LAB_800bd610`** = `*(0x802CA264 + f*0xC)` = HEAD
  list faksi (pasangan ForceGetCount ✓).
- **`TBoxOpen(x,y)` `FUN_8019f230`**: lookup instance peta
  `FUN_8004721c(bmap,x,y,layer=0x200,mask=0x410)` (array instance stride
  **0xB4**, bounds +4..+7 i8, mask-type **+0x10+0xBC**) → sukses:
  `FUN_8012942c(box,handle)` = ANIM-BUKA-peti + enqueue **aksi-tile 0x23**
  (`FUN_8010feec` = rekaman (x,y,act,0) @ map+0x200*8, ptr count +0x204... =
  action-ring) + re-arm; fail → `FUN_800056a0(0)` abort.
  **`VisitOut(h,x,y)` `FUN_8019ecb4`**: lookup mask **0xA00** (bangunan-visit),
  enqueue **aksi 0x24**, FF=instan-hapus `FUN_80049d08` else anim
  `FUN_800498a4` + re-arm ⇒ **peti & rumah = sistem instance + ring aksi-tile
  yang sama dengan maplinkAt** (MAP-005) — grammar lengkap: 0x23=buka-peti,
  0x24=keluar-kunjungi.
- **`RemoveBackdrop()` `FUN_800894f8`** = `FUN_8000bf00(&DAT_802899C0)` =
  hapus backdrop dari manager (pasang `AddBackdrop` family).
- **`FocusWait()` `FUN_800baccc`**: jika FF → lanjut-pan `FUN_800187f0` return
  0 (re-pump); else jika focus-selesai `FUN_800188a4(0)` → queue-pump
  `FUN_800c0658` ke list `DAT_8028D188` + re-arm + arm-pump `0x80320060`.
- **`Join(u)` `FUN_800b7140`**: `FUN_80107294(u,5)`+`FUN_801070fc(u)` =
  set state-unit 5 (join-anim) — pasangan `VisitOut/TBoxOpen` action-set.

### BRV-SCT-005m — REGISTRAR FLAG & INVENTARIS NAMA FLAG SCRIPT (501 nama, SJIS) — DECODED
- **Mekanisme registrasi**: `regist(name)` `FUN_800bb080`→`FUN_800bddcc` =
  MENDEKLARASKAN string ke slot kosong tabel 96-slot (ptr+hash) — BUKAN registry
  modul proc; `clr(name)` `FUN_800bafe4`→`FUN_800bdbc8` = clear-bit by-name;
  helper bit terverifikasi: test `FUN_800bdb9c`, set-index `FUN_800bdb78`,
  clear-index `FUN_800bdb54` (semua +idx>>3 | +0x300, mask 1<<(idx&7));
  `FUN_800bded8` = reset-seluruh-tabel (dipanggil `FUN_800b727c/800b72cc` =
  cleanup antar-bab). Nama TIDAK preloaded — dipopulasinya dari
  `regist(...)` dalam .cmb saat chapter load (≤96 per sesi; 501 unik total
  lintas 31 bab ✓ muat). C-code ikut membaca table yg sama via simbol statis
  `gf_gameover/gf_complete/gf_canceled/gf_reserved2..4` ⇒ **flag `set` script
  = kondisi victory/defeat engine (`FUN_80028724` get gf_gameover,
  `FUN_8002da30` get gf_complete)** — rantai objective resmi: script→flag→
  gameover/complete. [AM-081]
- **INVENTARIS STATIS 2121 situs / 501 nama** (`out/data/FE9_SCRIPT_FLAGS.csv`,
  mirror `docs/data/`): nama = SJIS-Jepang terekode-benar (TextDecoder shift_jis
  atas latin1). Kelas sistemik: `宝箱NN` (PETI 1-5, set+get: buka→set,
  AI/kondisi→get), `増援トリガN`/`増援イベントN` (TRIGGER/EVENT bala bantuan:
  trigger set 17x→event get 7x = dua-flag handshake spawn), `ボス死亡` (mati
  boss → get 8x = pemicu ending bab), `訪問NN` (kunjungi rumah), `情報Nフラグ`
  (1-6), `ボス戦闘会話<unit>` (dialog boss per karakter ×29 bab), `F0203`-style
  ASCII (event lintas-bab C80/C82/C95). ⇒ **peta semantik event-objective bab
  terbaca statis TANPA device**; engine wajib: flag-store persist antar-rekaman,
  handshake trigger→event, dan wiring gf_gameover/gf_complete ke win/lose.
- Engine: `set/get/clr/regist` handler map tunggal (`g_scrFlags`) — live
  terverifikasi cold-boot C00 (`scr set("DBG_…") flags=1`); grace-clock wedge
  detector di-fix (false-positive `WEDGE lastLog=0` saat begin f>>240).

### BRV-SCT-005l — SEMANTIK NATIVE BATCH-6: EVENT-FLAGS set/get + ACTION-QUEUE + EVENT-CAMERA KEYFRAME + FORCE-COUNT — DECODED
- **`set(name)`/`get(name)` `FUN_800bb018/800bb04c`** = **NAMED EVENT-FLAG BITSET**:
  tabel `DAT_803307bc` = 96 entri (u32 ptr string @stride4, hash cadangan +0x180,
  **bitset 12B @+0x300**); lookup = intern-hash `FUN_8000d598` + str-eq
  `FUN_802342b8`; set → bit[idx]|=1, get → bit; miss = no-op/0. Slot dipopulasi
  RUNTIME oleh native `regist(name)` (lihat SCT-005m — bukan preload).
  ⇒ mekanisme `always_*` conditionals dispos terhubung ke flag script INI.
  Engine: implemented (sparse map + log; live set("...")×1 terverifikasi
  cold-boot C00). [AM-081]
- **`UnitWalkDir(dir)` `FUN_801a1db8`**: bukan per-unit — **script ACTION-QUEUE
  `DAT_80296f90`**: rekaman baru auto-init (+0x30=enable, +0x33=global speed
  `DAT_803307a8`, +0x36=1, +0x3c=1) lalu **nibble dir numpad → +0x34&0xF**
  (skema sama path-digit SCT-005c). Semua "Unit*-family argc kecil" yang
  share-prefiks init identik = operasi FIELD rekaman yang sama:
  **`SetEventCameraPrec(p)`** +0x3c=p; **`UnitFocusOff()`** +0x30=0;
  **`UnitFadeOff()`** +0x32=0. Pump unit meng-konsumsi rekaman tsb.
- **`UnitMoveWait()` `FUN_801a2464`** RE-ENTRANT: cek `DAT_80296fa8` +
  `FUN_80183974()` (masih ada unit animating) → push timer `DAT_80296fcc` +
  flush antrean; return 1 selalu (bukan blok). = resep anti-wedge yang engine
  sudah pegang (scrMoveWait tidak boleh hang — catatan 2104 konsisten).
- **EVENT-CAMERA keyframe family** (×36 tiap fungsi di C*: cutscene standard):
  `CreateEventCamera`=FUN_800177e0, `StartEventCamera`=FUN_8001779c,
  `DeleteEventCamera`=FUN_80017774; **`SetEventCameraAt(tag,x,y,z)`**=
  `FUN_80017840(x,y,z, TYPE=0 POSISI, tag)` (normalisasi /(272-slot^xor));
  **`SetEventCameraRot(tag,rx,ry,rz)`** = TYPE=1; **float argumen XOR-biased
  (`^0x80000000` − bias f64) = konfirmasi ULANG dua-jalur ABI SCT-005h**;
  `EventCameraWait` = busy-check `FUN_8001772c` → re-entrant (list `DAT_80297010`).
- **`ForceGetCount(force)` `FUN_800bd5e0`**: = panjang LIST PER-FAKSI
  `DAT_802ca264 + force*0xC` (kepala list 0/4/8) → `FUN_80015680`. AI/Sally
  condition (`SEQ_ATTACK100` dkk.) memakai ini. **`AllUnitEscape()` `FUN_800ba784`**:
  2-pass 160 slot stride 0x280 (runtime SCT-005b): pass-1 filter-tabel
  `FUN_80015320(type3)` + `+0x1A0` bit1 → clear bit0; pass-2 setiap unit lolos →
  `FUN_8003dfb8()` (despawn/escape) → flush `DAT_802ca268` + camera refresh.
  **`ENVSilentOff()` `FUN_800b7990`** = `FUN_80145520()` (fungsi KE-3 keluarga
  env-audio, beda dari NoisyOn `FUN_80145488`).

### BRV-SCT-005k — SEMANTIK NATIVE BATCH-5: AUDIO-ENV + BGM NAMED + WAIT/CAMERA-HARD — DECODED
- **`BGMPlay(slot u8, ptrStr)` `FUN_800b8070`**: arg1 WAJIB pointer RAM valid
  (guard `0x80000000..0x817ffffff` ⇒ TERBUKTI string, konsisten golden
  `"BGM_EVT_IKE1"@0x45d10`); bila audio-on: `FUN_8014832c(slot,ptr,VOL=100,1)` =
  putar BGM NAMED per slot; `slot==0` → `FUN_80146af0()` ekstra (flush/layer utama);
  nama dicatat ke registry `DAT_8026f6b8`. Engine SUDAH benar (bake BGM by name ✓).
- **`BGMFadeOut(slot u8, dur i16)` `FUN_800b7db4`**: slot ×**0x30** (tabel slot BGM
  stride 0x30 = SCT-005f ✓ `DAT_8032ff14` flag bit0=aktif, objek `DAT_8032ff10`);
  vtbl+8(obj) dulu (pause/stop hook) lalu `FUN_80148980(obj,dur)` set durasi fade;
  `slot==0` → `FUN_80146b00()` fade-SEMUA. Return 1 selalu (non-yielding).
- **`ENVSilentOn(n)` `FUN_800b79b4`**: clamp n<0→0 → `FUN_801455b8(n)` = senyapkan
  lingkungan SE/ambience dengan parameter (durasi/level; naming Silent-vs-Noisy
  = pasangan). **`ENVNoisyOn()` `FUN_800b796c`**: `FUN_80145488()` = batal senyap.
  Keduanya instan (return 1) — C01 memakai `ENVSilentOn(0)` sebelum MapLoad dan
  `ENVNoisyOn` setelah unit siap = ambience dipadamkan selagi movie/peta loading.
- **`WaitF(frames)` `FUN_800b7828`**: flush antrean (`FUN_80013864` — SAME pump
  helper family WaitM); flag fast-forward (ram `-0x7c90`&1) → return 0 = SKIP
  (pola identik WaitM ff-skip SCT-005b); else entri timer ke list `DAT_8028d22c`
  dengan **`+0x60 = frames-1`** (field hitung mundur). Konfirmasi: wait via LIST
  timer, bukan blok.
- **`FocusHardWait()` `FUN_800babfc`** (argc 0, re-entrant): bila mode-hard OFF:
  tanyakan selesai-gerak-kamera `FUN_800188a4(1)`; belum → return 0 (yield);
  selesai → simpan slot kamera (`FUN_800c0658` → list `DAT_8028d19c`), flush
  antrean, SET flag hard `0x80320060`, return 1. Mode-hard sudah ON →
  `FUN_800187f0()` + return 0 (= dilewati). ⇒ setelah Focus/FocusHardWait,
  mode kamera "hard" aktif SAMBIL di-finalize. [AM-080]

### BRV-SCT-005i — KONTRAK RUNTIME NATIVE TERVERIFIKASI DEVICE (C01 end-to-end) — DECODED/VERIFIED
- **Bukti**: emulator-5554 2026-11-12 19:29–19:31 — `script: Opening0 DONE @f=2316 -> WORLD`
  penuh: movie beat → MapLoad → spawn+move → 6 dialog (`MS_01_OP_01`..`OP_04`) →
  WORLD interaktif (unit geser tile 16,18→17,18 via DPAD; screenshot
  `fe9-captures/agent/open_done_world.png`).
- **WaitM(ms)**: deadline frame = ms×60/1000; **satu slot `waitUntil` ter-latch** —
  panggilan baru MENIMPA yang lama (konsisten satu slot game); slot dibersihkan
  saat kedaluwarsa (bukan saat bangun) supaya dialog yang menutup di frame yang
  sama tak ikut makan deadline baru.
- **TalkEvent**: push baris lalu YIELD 1 frame/token sampai tombol confirm; dialog
  child KELUAR → **RESUME, BUKAN restart** — guard wajib: (a) `onEnter` scene
  SC_SCRIPT tidak restart script yang hidup; (b) resume path tidak mengosongkan
  string pending (pump berhenti saat dialog hidup; string baru boleh diganti hanya
  oleh CALL TalkEvent berikutnya).
- **QUEUE WALK/HITUNG-SATU-KAKI HARUS SCENE-AGONOSTIK**: drain `g_m.top` tiap frame
  kapan pun engine TIDAK di-pause (termasuk SC_SCRIPT/DIALOG) — karakter FE9 asli
  berjalan DI BELAKANG dialog. Gerbang lama `==SC_WORLD` = resep wedges sunyi:
  `UnitMoveDirByPID` antre saat script → `UnitMoveWait` loop → antrean tak
  pernah terdrain (ring streak 240 frame tanpa kemajuan).
- **Native gap tak dikenal engine**: auto-TERDAFTAR saat WARN pertama (stub pass,
  argc dari tapak panggilan) — rantai skrip tak pernah mati; daftar SCR_OVS menutup
  yang punya semantik (bound-intersection saat load).
- **Tool `bin/ring.mjs`**: gabung dump op-ring perangkat (`trace_ring.txt`, auto-dump
  LOOPSTREAK≥3) dengan JSON disasembler `cmb.mjs` → `@pc op CALL "nama" [fn]`;
  localize helper sunyi, mis. `#b8c` = prolog film (Fade(1,0)+Fade(2,500)+
  SetWhiteBackdrop+IsE3Version→MoviePlay) yang dipanggil Opening0 via `CALL_LOCAL 2`.
  [P4-M0]

### BRV-SCT-010 — REGISTRY MODUL PROC (pool `0x802832E0`) — DECODED (struktur; semantik per-modul = P1-fitur)  [menutup UNKNOWN-U012]
- **Pool = REGISTRY PROC/MODUL ENGINE** (framework `Proc(0x%08X:%s)` — string debug
  satu blok). Grammar record: `[u16 param][u16 tag][u32 payload]`; **0x2003/0x2025 = NAMA modul**
  (ptr string), **0x200F = handler code** (1228× total), 0x2005/0x2010 = varian handler,
  tag lain (0x201A×429, 0x2004×156, 0x2012×51, 0x2014×63, 0x2015×40) = record bertipe param.
- **Hasil ekstrak: 319 record nama → 307 modul unik, 232 punya handler code.**
  Top-handler: `E_BMapMenuSeq`(74) `GameController`(65) `BasesOutfitProc`(64)
  `BasesForgeShopProc`(42) `E_OnBtSeq`(38) `BasesProc`(36) `E_TerInfo`(32) `BasesEducateProc`(31).
  Kelas modul: mode (`GameController`,`BMapMain`), sekuens panggung (`E_AttackSeq`,
  `E_FallSeq`,`E_StealSeq`,`E_RoundEarth/Sky`,`E_OnBtSeq`), wajah (`E_Face`,`E_FaceSlider`,
  `E_FaceEyeCtrl`,... 12 modul), base (`Bases*` 6), build (`E_Build*` 4), traject proyektil
  (`E_TrajectProc/Evaluate`), token giliran (`E_TaikiToken/TurnToken/TurnAToken`),
  `E_WarpDest/E_StoneDest`, `AutoProc/DelayProc/ProgressiveProc`, cuaca (`E_MapWeather`).
- **`FUN_8002e0d0` TIDAK punya `bl` karena = update-handler `E_BMapBGMCtrl` — didispatch lewat
  tabel Proc** (ptr @0x80284FA8). Resolver teka-teki "xref tunggal".
- **Manifest boot TERBACA satu blok**: `system.cmp`,`FE8Data.bin`,`s/rect.bin`,
  `window/rectdesc.bin`,`mess/common.m` + daftar mode `TITLE/FILEMENU/TrialMenu/JOYBOOT/
  Opening0-2` + font list (4 .gcf) + nama bahasa — konfirmasi independen urutan `FUN_80005c8c`.
- **KOREKSI temuan**: registrar `FUN_80013ae8` bukan cuma 407 situs bootstrap — histogram `bl`
  main.dol = **1618 call-site** (delta kode off+0x8000C930; entri alternatif interior
  0x80013C18×88). Grep pseudocode 407 = agregat 7 fungsi bootstrap saja. Katalog 390 nama TETAP valid.
- Alat: `bin/modreg.mjs` → `docs/data/module_registry.csv` (name,handlers,extras).
  Bukti: dump word file 0x2802e0..0x2a21c0 main.dol (peta data flat +0x80003000 terverifikasi
  label string), string `E_BMapBGMCtrl`@0x80269311 = payload record 0x2025.
- Sisa P1-fitur: semantik tiap handler (init/update/draw per modul) — dikerjakan per-fitur
  engine saat implementasi, bukan blocker RE.

### BRV-BTL-008 — TAMBANG VIDEO LONGPLAY (pipeline hdframe) — PARTIAL (berjalan)
- Pipeline: `bin/hdframe.mjs scan|shot|sheet` — video longplay USA full (21,9 jam,
  `fe9-captures/playthrough.mp4`) → coarse 1 frame/60s (1315 thumb 320px) → sheet
  10×9 (concat+tile ffmpeg) → triage visual → `shot` res 1280×720 di timestamp.
- **JENDELA TERRAIN TERKONFIRMASI DI UI** (1:15:00, peta Ch6): panel judul
  `Bridge` + kolom **`Avo 0 Def 0 MDf`** = kolom yang dibutuhkan engine (bônus
  tile). Panel nempel saat cursor di tile dgn menu peta terbuka → harvest per-sheet.
- **LAYAR PENGALAMAN pascapertempuran** (6:50-6:54): `Bonus Experience 1244P`
  (kado stage), aturan UI `Add up to 100 experience points at a time` (manual
  distribusi s.d. 100/aksi), panel unit menampilkan Lv + Exp 0-99/level
  (Wyvern Rider Lv9 Exp3 → +19 → Exp22) = kurva EXP flat 100/level TERVERIFIKASI UI.
- **DURABILITAS ground-truth** (inventaris 1:09:00; unit window 6:33): Wind 40/40,
  Vulnerary 3/3, Steel Lance 33-35/35, Javelin 25/25, Laguz Stone 2/2; ikon badge
  rank E/S di kiri nama item (konfirmasi BTL-006).
- **Panel forecast satuan** (Brom Lv9, Javelin): `Atk 17 Hit 82 Avo 20 Crit 4
  Ddg 4 Rng 1-2` + kolom `Effect` = format baris forecast (konfirmasi BTL-007).
- Unit abu-abu di layar pilih-unit = tidak tersedia (cedera/penugasan) [inference].
- **HUD Exp di peta** (16:06 video): frame emas `Exp` + bar 0-100 + angka EXP
  saat itu (16) muncul atas unit setelah dapat EXP = widget level-progress.
- **Ring faksi terpetakan**: merah = musuh, biru = unit pemain (bisa gerak),
  ungu = NPC/ally (tidak dikontrol) — re-konfirmasi skema BTL-007.
- **RANGE STYLE**: move-range = petak SOLID (ungu/biru), attack-range = petak
  CHECKER oranye, zone gabungan = OUTLINE oranye kontinu (panah merah keluar
  layar saat zoom); pegasus/flyer = pola diamond besar (ignore地形). Badge
  ikon-orang kecil di unit = SEDANG MEMBAWA (rescue mount) [frame 68520].
- **UNIT WINDOW FINAL** (18:05 & 18:45): header nama + kelas + `Lv N EXnn`
  (EX = total EXP-level counter) + HP hijau cur/max; daftar senjata dengan
  **rank-badge (E/S) kiri nama** + `dur/now / max` (Brave Bow 25/30; yang
  dipegang = teks HIJAU); blok stat `Atk Hit Avo Crit Ddg Rng` + baris
  `Effect` = efek khusus senjata (ikon bulu + "Flying"). **Hit unit-window
  MENTAH (162/140) > cap-100 forecast tempur** — dua angka beda sistem.
- Widget terrain HUD SELALU tampil saat memilih unit (kiri-atas): `[Terrain]
  Avo n Def n MDf n` + objektif `Clear <Seize|Defeat Boss|Escape>` + `Turn n`.
  Sampel terrain: Bridge(0,0,0), Floor(0,0,0), Snow field(0,0,0), Thicket(Def1).
- **LAYAR SIMPAN/LOAD**: entri = `Ch.NN: <Judul Bab>` + tag **`Adv` (advance
  save) vs `Base`** + sticky `PLAY TIME hh:mm:ss`. Judul bab tervalidasi:
  Ch.4 Roadside Battle, Ch.8 Despair and Hope, Ch.21 Without a King,
  Ch.25 Strange Lands, Ch.26 Clash! (5 titik; korelasi slot=5 quicksave).
- Layar SUPPORT (1084-1090 sheet): panel support dua kolom + baris nilai
  bertingkat per-Lv (`My Lv 21 HP/Mt/Def/Avo/Hit`) — tabel support per-tier.
- Status domain: **Partial** — terrain CSV masih sampel (4), AI = observasi
  konvergensi + guard harta (54000-54090), EXP per-kill belum numerik.
- **VERDICT terrain (2× dicek)**: TIDAK ada tabel terrain struktural di
  zdbx (zbg bridge = prop 3D), FE8Data (bigIDtable = lookup msg), s_rect,
  DOL (string = cuaca). Nilai panel = data biner DOL/.data TANPA string ATAU
  per-map. Engine: default `Avo 0 Def 0 MDf 0` + exception dari sampel UI
  (Thicket Def1) SAMBIL trace runtime P2. Terrain HUD terbukti selalu-on,
  jadi validator UI mudah dibuat nanti.
- **Objektif `Clear` resmi teramati**: Seize · Defeat Boss · Escape · Rout
  (4 tipe) + widget HP-bar boss merah + label senjata (Javelin) saat target.

### BRV-SYS-003 — JUDUL BAB RESMI (dari layar Save) — PARTIAL (5/30)
- `Ch.4: Roadside Battle` · `Ch.8: Despair and Hope` · `Ch.21: Without a King`
  · `Ch.25: Strange Lands` · `Ch.26: Clash!` — teks UI, siap jadi tabel
  nama-bab engine; sisanya harvest dari layar save/sheet video serupa.


### BRV-SVT-001 — SAVE GC MEMORY-CARD (.gci GFEE01) — PARTIAL
- `fire-emblem-path-of-radiance.28559.gci` 155712 B = kartu GC: 8 slot save
  stride 0x4000 mulai 0x2080; tiap slot = header banner (judul + tanggal "%d/%02d/%02d
  %02d:%02d", satu format ISO eksplisit) lalu blok bertanda ASCII 4CC berurutan:
  `FE8J` (meta), `SYSF`, `BMST` (state peta; nama peta ASCII @slot+0x240, mis.
  `bmap18`/`bmap05`/`bmap02`), `MDST`, `UNIP`, `SOKO`, `FWEP` (palette ikon banner
  `808080ff`). Slot kosong = shell banner saja.
- `UNIP` = record stride **0x8F** (auto-korelasi 78,5%): byte+0 = kelompok
  (00/05/07), +2 = id template/slot, +0x09 = **EXP** (delta +0x14 konsisten antar
  save pasca-pertempuran s.d. byte identik lain), +0x13.. blok stat (3c 5x 0d ...),
  ekor 3-pas item/ID; record group-01 = musuh. Interpretasi PENUH masih open
  (statis antar-peta untuk beberapa record → kandidat template vs instance).
- Save I/O DOL: klaster `FUN_801dfb4c..FUN_801e32c4` (debug `MemoryCard::Open()`
  string region 0x8025AB00an; tag blob `.BMST.MDST.UNIP.SOKO.FWEP.` di DOL data
  tanpa xref pointer — dibangun via PC-rel).
- Guna untuk engine: validator ground-truth stat/EXP/inventori PER SAVE (8 titik
  waktu bmap02/05/18 tersedia). Status: Partial.

### BRV-SVT-002 — UNIP GRAMMAR DARI KODE (loader unit) — DECODED (struktur stream CLOSED; penamaan byte-per-byte = P2-validator)  [menutup UNKNOWN-U013]
- Rantai save-game DOL (semua bukti pseudocode decAll):
  `FUN_801e3654` = **urutan blok save**: set-nama `FUN_8017a6c4(obj,"BMST")→FUN_8014c1b8(0x80330708=BMAP global)`,
  `MDST→FUN_80048a90`, **`UNIP→FUN_80016628`**, `SOKO→FUN_800c3364(&803200AC)`,
  `FWEP→FUN_800c2930(&803206EC)`, +2 blok lagi (`&8032BB58`, `&802EF4C8`).
  Validasi kartu `FUN_801e047c`: header `FE8J`@slot+0x40 + **versi `0x20050201`** +
  magic `0x12345678` + checksum (loop baca FUN_800ec0a4); **2 slot cadangan @+0x1A000/+0x20000**
  (stride cluster 0x6000) — auto-pick slot checksum-valid terbaru.
- `FUN_80016628` (UNIP): 8 list grup `0x802CA264+8*c`; pass1 push semua 0x9F unit;
  pass2 per-unit `FUN_8003c830(unit,obj)`; grup dari byte `[unit+0x190→+8]`; unit mati
  (+0x180/0x184==0) masuk list grupnya; tulis semua grup (skip 4 saat pass1).
- **API stream**: u8 `FUN_8017a9dc`, u16-BE `FUN_8017a9b0`, blok-n `FUN_8017a810`
  (memcpy cursor→dst, cursor+=n); lookup template: item `base+(id−1)*0x48`
  (r13−0x7BBC), dua tabel stride-100 (karakter/class) via r13−0x7BC4/−0x7BCC.
- **GRAMMAR record UNIP (TERKOREKSI+TERVALIDASI walker `.gci` — stream 159 slot unit!)**:
  per slot unit (0x9F): flag u16; **flag==0 ⇒ record kosong = 2 byte saja** (sisa slot =
  run `0000` terbukti di blok), jika ≠0 record penuh **143 B = 0x8F tepat** (total stream
  4×143+run-zero tervalidasi slot0). Urutan stream → runtime (kode 8003c830):
  `[u16 flag/pid → FUN_80019eac → unit+0x188 (tabel-2 stride100, (id−1)*100)]` →
  `[u16 tmpl → FUN_80019e68 → unit+0x18C (tabel-1 karakter stride100)]` →
  `[u8 GRP → push unit ke list faksi DAT_802CA264+grp*0xC (8 list, skip list-4)]` →
  `[u8 h1][u8 h2][u8 h3] → ptr slot ((h−1)*0x280+0x802B14E4) di +0x180/+0x184/+0x194` —
  empiris: h1 = identitas slot unit stabil antar-save (1,5,9,13..), **h2 = anchor list
  faksi per peta (0x41=65 di bmap02; 0x10/0x11 di save slot6-7)**, h3=link cadangan;
  `[u8×7 → +0x199..0x19F]` (XY runtime di +0x19E-F = byte ke-6/7 blok ini) →
  `[4B→+0x1A0][4B→+0x1A4][u8×3→+0x1A8][8B→+0x1AB][8B→+0x1B4][16B→+0x1BC]` →
  **8 slot item @32B tetap** (`FUN_8001c15c`: flag u16 = id item (0 ⇒ kosong +skip2);
  non-kosong: `FUN_80019e24(id)` → ptr `base+(id−1)*0x48` + u16 nilai (durabilitas/pack)) —
  contoh ground-truth unit: id 7,5,0x91,0x8D,0x8E,0x94 = baris `items.csv` (self-ID
  `01 01 <id>` raw template mengunci) ✓ → `[8B→+0x20C][6×u16+2×u8→+0x228][10B→+0x214]` →
  **ekor SEQID 15B** (`FUN_8003c21c`: u8 ATK/MOV/HEAL/MTYPE lookup via
  `ATK_SEQID_LIST/MOV_SEQID_LIST/HEAL_SEQID_LIST/MTYPE_TABLEID_LIST` = **link langsung
  SE-bank ↔ nama SEQ_* dispos!** + 2 u8 flag + u32 + 2×u16 + u8) →
  `[u16→+0x2C6][u8×2 skip]` (`FUN_8004bc48`) + skip2 final = 143 ✓.
- Writer = fungsi berbeda (mirror); reader = ground truth layout.
- **JAWAB U013**: blok +0x13..0x36 = **LIVE mirror** (bukan template) — writer nyalin field
  runtime apa adanya; identik antar-save karena field BELUM dipakai (rank senjata/akumulasi),
  EXP bergerak ✓ bukti. Ekor item TERPETAKAN (`.gci` ground-truth slot0): 8 slot tetap
  4B = `[u16 id BE][u8 dur/pakai][u8]`, flag 0 = kosong: contoh unit = id 7/d15, 5/d25,
  **0x91=145/d3, 0x8D=141/d2, 0x8E=142(KNIGHTKILLER)/d2, 0x94=148/d5** — id = indeks persis `items.csv`
  (byte self-ID `01 01 <id>` dlm raw template mengunci pemetaan ✓).
- Status: Unknown → **Partial→Decoded-struktur**. Sisa P2: penamaan 7-byte + blok 4/8/16B
  via diff 8 slot save (validator engine saat implementasi). Ground truth:
  `fire-emblem-path-of-radiance.28559.gci` (Downloads; 8 slot; tag UNIP @0x2334+0x4000×n).


### BRV-BTL-009 — FORMULA TEMPUR + RNG LFSR DARI KODE (resolver netuzo/animasi) — DECODED (inti numerik terverifikasi; penamaan sebagian bit = Partial)
- Orak: seluruh 92 call-site API RNG terpetakan per-cluster (sweep byte ELF + grep 8110 pseudocode +
  dump disassembly Ghidra per-range via `bin/ghidra/DisRange.java`).
- **RNG (BRV-RNG-010 CLOSED)**: state 3×u16 @ r13−0x7C50/−0x7C4E/−0x7C4C (= 0x8032F3B0):
  `w2=(w2<<1|(w1>>15))&0xFFFF; u=((w1<<11|(w0>>5))&0xFFFF)^w2; (w2,w1,w0):=(w1,w0,u)` (LFSR Fibonacci).
  `FUN_8000cee4(n) = (n·u)>>16` ∈ [0,n−1]. `FUN_8000ce68(p)`: satu roll, v=(u·100)>>16, TRUE ⟺ v<p.
  `FUN_8000cd88(p)`: DUA roll, v = rata-rata dua v — dipakai HIT (anti-untaian ekstrim). Predikat
  `((p^v)>>1 − (p&(p^v)))>>31` diverifikasi numerik node == (v<p) ∀ p∈0..100,v∈0..99 ✓.
  Debug: `_DAT_80331B58 & 0x4000` = RNG dimatikan (mode deterministik — cocok utk harness engine).
- Resolver: native `NetuzoBattle(3)`=FUN_800b841c → rec unit=(pid−1)·0x280+0x802B14E4 →
  init FUN_801a7794(ctx 0x80330EAC, atk, def) → FUN_801a5268(ctx,0xf) → satu exchange FUN_801a4c8c
  (serang→counter; cek mati via HP +0x1A8==0). Pertarungan animasi memakai resolver yang SAMA —
  tampilan membaca log event (FUN_80033E90/80034068 kode 0x55–0x66).
- **Battle record (ctx; defender = blok ctx+0x2E0)**: +0x296 flag-efektif-dobel · +0x298 ATK ·
  +0x29A DEF · +0x29C AS · +0x29E HIT · +0x2A0 AVO · **+0x2A2 hit%-final** · +0x2A4 CRT ·
  +0x2A6 CRT-avo · **+0x2A8 crit%-final** · +0x2AA DMG · +0x2B4/+0x2B5 DEF-base · +0x2BA..0x2C0
  mirror display · +0x2C2 stat ATK · +0x2C4 stat SKL · +0x2CA forced-crit · +0x2CB/+0x5AB
  counter-flag · +0x2D6 pernah-membalas · unit: +0x1A8 HP(char), +0x1A5&7=asleep/stone→hit=100,
  +0x1BC status bitmap (ID: 0x10,0x2b,0x2e,0x30–0x3f,0x41,0x4c,0x51–0x53,0x5a,0x5f,0x60,0x61).
- **Rantai nilai per-strike (FUN_801a601c per strike →)**:
  `801a8f80`: ATK = MT senjata (FUN_8001d500&0xFF) + stat(+0x2C2) + bouns terrain/status
  (FUN_8003b258 / FUN_8003afa0); +0x296≠0 → ATK<<1 (efektif; cocok terrain via FUN_80092dc4(rec+0x2AE)).
  DEF = (char)(+0x2B4|+0x2B5 efektif) + FUN_8003aa40/8003a778.
  HIT(+0x29E) = Hit senjata(FUN_8001d47c&0xFF) + 2×status(FUN_8003af28) + SKL(+0x2C4) + luck/2 +
  luck-byte(FUN_800378cc byte1) + support(FUN_801a80d8) + (status 0x51 → +20); clamp ≥0.
  AVO(+0x2A0) = 2×(+0x29C: FUN_8003aeb0 − max(0, BV senjata(FUN_8001d37c&0xFF) − status)) + luck/2 +
  (char)(+0x2B6) + luck-byte + support + (status 0x52 → +20); ≥0; (unit+0x1A4>>7&7)≠0 → 0.
  CRT(+0x2A4) = Crit senjata(FUN_8001d3fc&0xFF) + luck/2 + luck-byte + class-bonus(FUN_801b8674 pid) +
  skill61(FUN_801f19a0); flag senjata 0x2000 → 0.  CRTA(+0x2A6) = luck/2.
  `801a8da4`: support tetangga (faksi sama, jarak<4): keenam field += ringkasan FUN_8003b8d4.
  `801a8f14`: **hit% = HIT(atk) − AVO(def); crit% = CRT(atk) − CRTA(def)**; kelas anti-crit
  (FUN_801b8550) → CRT=0.
  `801a8b48`: status 0x35 → hit%×2; status 0x3f → hit%/2 & crit%×2; def +0x1A5&7 → hit%=100;
  senjata(def) flag 0x4000 → crit%=0; status 0x5f(def) + flag senjata 0x20 → ATK=0.
  `801a8af4`: clamp hit%,crit% ke [0,100]. Debug &0x100 → hit=100,crit=0.
- **Roll hasil (FUN_801a8a28)** per strike: register `(&DAT_802EFDB8)[DAT_802EFE18]` bit0 paksa-normal,
  bit1 paksa-crit, bit2 paksa-miss; rec+0x2CA → paksa-crit. else: hit = cd88(+0x2A2);
  miss → kode 0x52 (dmg=0); roll crit = ce68(+0x2A8) → 0x51; normal 0x50.
  **CRIT = DMG×3** (801a64f8: 0x50<st<0x52 → ×3). Netuzo TIDAK roll hit lain selain ini (path sama animasi).
- **DMG = max(0, ATK−DEF)** lalu modifier FUN_801a55d4 (terrain u16 @DAT_802EFDD8[tile*2],
  tile=DAT_802EFE18): bit0 dmg=(dmg+1)/2; bit3 → dmg=127; bit4 → null-efektif (roll ce68 50%:
  dmg = ATK − DEF·3/2 clamp0 = koreksi efektif); bit5 → +AS/4; bit13(0x2000) → extra-strike
  `((lvl−1)/5+1)`× FUN_801a8010; bit15(0x8000) → set status 8 (poison-ish) di def+0x1A4;
  status: 0x2b HP≤½ → crit%+50(cap100); 0x38 → brave-ish +AS>>2 dmg; 0x39 → bisa-racun (roll/2);
  0x37 → lawan +0x2CC=1; 0x2e → guaranteed-kill dmg=HP/2; 0x53 → dmg=0+state0x50; senjata-armor
  (ctx+0xCAC&0x100) & flag → dmg/2. HP(def)=max(0,HP−dmg); HEAL-healer: recovery via +0x2CE/2.
- **Jumlah strike**: base 1; flag senjata 0x20 (brave, FUN_8001cb64+4) → 2; counter: flag +0x2CB/+0x5AB
  (senjata counter + jarak); **double AS**: `def.AS + 4 ≤ atk.AS` → pertukaran ekstra (801a4c8c:
  ctx+0x57C+4 ≤ ctx+0x29C) ✓ aturan AS+4 FE9; inisiatif: float AS FUN_8004e670(unit,1) fcmp →
  ctx+0xCAC bit 0x2000.
- **EXP (FUN_8003d710)**: rank=clamp(1..20, dari FUN_801661cc+rec+0x19A); 8 slot EXP senjata
  (rec+0x54..0x5B); per slot: exp += (rank·val)·(90+cee4(20)+10)/10000 → pengali 91..100%.
- AI: probabilitas menyerang = ce68(FUN_8003cff0(atk,def)%); "weighted selection" keluarga
  0x800FA0E8 (14× roll100 vs rec+0x4) = pemilih aksi AI (BUKAN damage) — salah tebak awal, dikoreksi.
- Validasi silang: BRV-BTL-005 Mt=kerusakan final ✓ (dmg=ATK−DEF); sampel Ch1 (Ike Lv3 Mt24 dkk)
  = seed test harness saat implementasi. Sisa: penamaan definitif tiap bit status 0x10..0x61 &
  bit terrain = tabel FE8Data/window (P1-fitur, bukan blocker formula); verifikasi device = T-CAP.
- Status: **DECODED** (formula+RNG numerik; konversi ke engine = C++ mirror, golden test LFSR wajib).
