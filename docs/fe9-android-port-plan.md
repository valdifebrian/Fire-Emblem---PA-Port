# FE9 (GFEE01) → Android Native — Master Plan Port

Status: draft v1 (2026-11-10). Disusun dari state RE M0–M4 di `fe9-re-plan.md`.
Aturan main proyek (menangguhkan default): **rebuild engine dari nol** berdasarkan pengetahuan
game + data hasil RE (BUKAN patch emulator/terjemah PPC), **satu app = build touch-overlay**,
**plan di-review sebelum kode ditulis**, eksekusi device di tangan human, distribusi base build
lewat WhatsApp.

---

## 0. Reviu: yang sudah selesai

| Milestone | Status | Bukti (commit/commit-repo `valdifebrian/Fire-Emblem---PA-Port`) |
|---|---|---|
| M0 RVZ reader | ✅ | `bin/rvz.mjs` v1 penuh (chunk dir, bitmap, LZ4/zstd/btrfs); ISO terbuka tanpa tool pihak ketiga |
| M1 Inventori FST | ✅ | 4077 file, path+offset+size → `inventory/files.csv` |
| M2 main.dol | ✅ | `dol2elf.mjs` + Ghidra headless PPC: 8125 fungsi, decompilasi parsial (oracles, bukan kode sumber engine) |
| M3 Data tables | ✅ | LZ77 GC-0x10 di-crack ground-truth; `cmp.mjs` (container pack), `fe8data.mjs` → `data/{characters(340,0x54),classes(115,0x64),items(189,0x60)}.csv` — nilai Ike/Mist cocok manual; `m.mjs` → `.m` message bank (dialog Greil c09 utuh + control codes) |
| M4 (partial) | 🟡 | `tpl2png.mjs`: tpl `0x0020AF30` + CMPR + `.cms`=LZ77(tpl) → **360/360 portrait (4347 PNG, visual-verified)**, ikon atlas 768×216 match semantik paragon; `ga.mjs` .ga→JSON (spec tellius byte-exact, validasi archer atk1); **`.dbx` = teks Shift-JIS** (job/weapon→alias model + tuning battle), zdbx.cmp 449/449 extracted |
| M5 docs | 🟡 | semua temuan terdokumentasi §10–§10d; `docs/formats/*.md` formal belum ditulis |
| M6 boot | ⏸ | human-run (Dolphin + capture) — belum dimulai |

**Kesimpulan reviu:** fondasi data (tabel, teks, tekstur, portrait, animasi, lookup .dbx) sudah
satu arah pipeline PC→JSON/PNG/CSV. Yang tersisa sebelum engine bisa jalan: **model 3D, script
chapter, audio, font, dan katalog mekanisme gameplay.**

---

## 1. Target akhir & constraint produk

**Definition of Done:** satu APK Android yang boot → title/attract → start → main seluruh
campaign FE9 Path of Radiance (EN) sampai ending, di hp fisik via deploy USB dev machine.

Constraint (diwarisi dari keputusan proyek; tiap butir dipetakan ke acceptance di §6):

1. **Rebuild dari nol** — engine baru; tidak fork/patch emulator; PPC code cuma oracle.
2. **Touch overlay wajib** — tombol layar (d-pad + A/B/X/Y-equivalent + L/R + Start/Select/Z +
   pause); bukan Bluetooth-controller-only.
3. **Keyboard primary di desktop dev** — playable out-of-the-box dengan keymap default.
4. **Satu jalur input untuk menu & gameplay** — tombol yang memilih karakter di menu WAJIB hidup
   di dalam stage; `start` → select → bisa menembak. (regression signature historis)
5. **Rapid-fire + 4 arah simultan responsif** — spam serang sambil gerak fwd/back/up/down.
6. **Cold boot → attract/demo, main hanya setelah start eksplisit** — boot langsung masuk
   gameplay = regresi.
7. **Tidak ada freeze** — framebuffer berjalan terus, progress counter maju; watchdog CI.
8. Build base didistribusikan **lewat WhatsApp** (bukan release publik); game data mentah tetap
   di mesin SSH dev, hasilnya di-bake ke repo sebagai pack.
9. Repo publik = **kode + docs + CSV saja**; aset disc (PNG/model/audio) tidak di-commit.

---

## 2. Arsitektur besar

### 2.1 Dua dunia, satu arah

```
[PC dev (Windows)]                     [Android (hp fisik)]
 disc RVZ ─┐                            
           ├─ fe9-re/bin/*.mjs  ── bake ──► assets/*.fpk ── APK/assets zip
 tabel/csv │   (ekstrak, decode,            (texture ASTC/RGBA8, mesh,
 .dbx→json ┘    konversi, kompresi)          anim bin, audio OGG/PCM,
                                             table JSON, .m → JSON,
                                             script JSON, font atlas)
                          build gradle+NDK (CMake) ──► app .so + Kotlin shell ─► adb install
```

Runtime **tidak pernah** menyentuh format disc (kecuali .fpk reader). Semua kerumitan
(SJIS→UTF-8, LZ77, CMPR, ADPCM, tiling GX) selesai di PC.

### 2.2 Engine (keputusan boring, alasan singkat)

| Area | Pilihan | Alasan |
|---|---|---|
| Bahasa core | **C++20** | NDK first-class, kontrol memori, ekosistem tooling; Rust = opsi valid tapi toolchain/game-loop example lebih jarang |
| Render | **OpenGL ES 3.0+** | semua device target punya; Vulkan overkill untuk GC-look; fallback EGL config 8888+depth24 |
| Window/input/audio shell | **NativeActivity + AInput + Oboe** | minim dependency; SDL2 boleh kalau JNI input bermasalah (keputusan P2) |
| Shell app | Kotlin + JNI (surface, lifecycle, permission, save path) | pola standar |
| Build | Gradle + CMake + NDK r26+; CI lokal (script) | boring |
| Data pack | **`.fpk`** (FE pack: TOC u32 hash→{off,len,fmt}, payload terkompresi zstd) | satu file baca-acak, mmap-able, mudah debug (tool `pack.mjs`/`unpack.mjs`) |
| Audio | musik: decode → OGG Vorbis di bake; SFX: PCM16 mono 32k di-pack | STM/seq decode di PC saja |
| Save | JSON + versioning di `getFilesDir()/saves/`, autosave slot | mudah di-inspect saat QA |
| Resolusi | render 512×384 (GC native) → upscale integer/near-nearest + UI layer 1080p | look akurat + murah; 4:3 letterbox; 16:9 safe area |
| Font | atlas glyph dari `/Fonts/*` (6 file) → JSON metrics + PNG | teks = .m UTF-8 hasil konversi |

### 2.3 Modul engine (dependency satu arah ke bawah)

```
platform (android: surface, input, audio-out, file, lifecycle)
core    (alloc, vec/mat, log, rng, taskio fpk, json, mem-pool)
assets  (fpk reader → Texture/Mesh/Anim/AudioClip/Font/Table/Message/Script)
render  (gles state, GC-func approximation: texenv/tev-like blend, vertex color,
         fog stage battle, UI sprite batch, letterbox)
game    (scene mgr, map scene, battle scene, unit/inv/item rules, growth/level,
         weapon triangle/effect/crit/damage, AI, script VM, dialogue UI, menus,
         save/load, event codes .ga consumer)
ui-input (input map abstrak ← touch overlay | keyboard | gamepad; action buffer
         rapid-fire; 4+8 arah; focus menu)
```

---

## 3. Matriks cakupan aset (semua tipe di inventori, tanpa skip)

Lajur **Aksi**: ✅ selesai · 🔧 tool tinggal jalan · 🔬 perlu RE baru · 🎨 bake only · ⏭ defer M6+.

| Lokasi (jumlah) | Isi | Status | Aksi → output port |
|---|---|---|---|
| `/ymu` (1333) | model map-unit `.g/.gs` + anim `.ga` per job | .ga ✅, .g/.gs 🔬 | .ga→anim bin; **P0: parser `.g` (skeleton+value block, spec tellius ada)** + `.gs` (shader/settings, survey) → mesh bin |
| `/zu` (839) | model+anim battle unit (`.ga` + body) | sama | body dari `.cmb`/spec `research/body/` → mesh |
| `/xwp` (88) | model senjata | 🔬 | sama管线 senjata |
| `/xcam` (80) | setup kamera battle | 🔬 (kemungkinan .dbx/txt) | → JSON kamera |
| `/Face` (360) | portrait `.cms` | ✅ 4347 PNG | bake RGBA8/ETC2 |
| `/window` (125) + `/cmp/system/window` | UI tpl | ✅ | bake + atlas manifest |
| `/illust` (174) | CG besar (cms/tpl) | 🔧 (cms decoder jalan) | bake + cek format non-cms |
| `/Fonts` (6) | font | 🔬 | atlas glyph + metrics JSON |
| `/Mess` (36) + common.m | dialog `.m` | ✅ | → JSON UTF-8 (konverter escape `\\x`) |
| `/zdbx.cmp` (449 `.dbx`) | lookup job/weapon/skill/param/kamera | ✅ extracted, SJIS | → JSON UTF-8 (P1, `dbx2json.mjs`) |
| `/Scripts` (43 `.cmb`) | **script chapter/scene** | 🔬 **RE TERBESAR tersisa** | decode → JSON VM instruction set (P3 spike) |
| `/s` (199) + `/pak` (300) | stage map packs | 🔬 magic .pak | unpack → tileset/model/scene JSON |
| `/zbg` (127) | background battle (+param.dbx ✅) | 🔬 | bake background + param |
| `/zmap` (114) + `/gmap` (8) | peta dunia/overworld | 🔬 | mesh + lookup gmap |
| `/Sound` (97 `.stm`+lain) | musik/SFX GC ADPCM | 🔬 (format STM publik, decoder mudah) | decode → OGG/PCM |
| `/Movie` (9 `.thp`) | cutscene video | ⏭ | **keputusan P6**: tampilkan sbg still CG+SFX atau transcode mp4 (ffmpeg portable di PC) |
| `/etc` (25) | curb/curt/plane .g/.ga/.gs, bin | 🔧 | masuk pipeline |
| `/ending` (106) | credits/ending | ⏭ P7 | bake |
| `/shop` (3), `/tut_data_en.bin` (1) | shop + tutorial EN | 🔬 | tabel shop JSON |
| `main.dol` (ELF+Ghidra) | kode PPC | ✅ sebagai oracle | **tidak** dipakai untuk generate kode runtime |
| `.gcf` (5), `.sdir/.samp/.proj/.pool` (3), `.stbl/.slib/.etbl/.bnr` (1) | project/toolchain leftovers | 🚫 | abaikan (dev leftovers; cek cepat P0, lalu close) |

**Gap pengetahuan gameplay (di luar format file)** — katalog dari wiki FEU + observasi Dolphin
(human) + tabel: damage/might triangle, effectiveness, crit, arena, convoy, forge/synthesis,
class change/seal, support?, convo, shop, tutorial popups (EN), cut rules, gmap movement.
→ deliverable **P2**: `docs/systems/*.md` satu file per sistem, tiap klaim cite tabel/field CSV
atau rekaman. Engine TIDAK mengimplement sistem yang belum ada doknya.

---

## 4. Fase kerja

Urutan = dependency. **Setiap fase punya acceptance test yang jalan otomatis** (desktop harness
headless = replay input capture → assert state/frame counter) — signature regresi proyek
(attract boot, input penuh, rapid-fire, 4-arah, no-freeze) jadi gerbang CI semua fase.

### P0 — Lengkapi RE aset (agent) + capture reference (human)
- Parser `.g` (skeleton: bone array, value block 43 float, flags — spec `research/skeleton/`),
  body `.cmb`/`.body` (spec `research/body/`), `.gs`; viewer debug: render wireframe 1 model
  ymu/zu → screenshot → vision-check (IKE, KNIGHT).
- Magic `.pak` (300) + struktur `/s` stage; `.gcf`/`stbl`/dll di-triage 30 menit.
- Script `.cmb` `/Scripts` (43): hexsurvey + korelasi dengan Ghidra strings + chapter 1 di
  Dolphin (human capture event) → hipotesis VM → **spike decode script c01**.
- `.stm` decoder + dither table (format publik); `/Fonts` parse; dbx2json SJIS→UTF-8.
- Human: Dolphin full-run capture Bab 1–2 + input log + video 60fps (baseline perilaku) +
  save SRAM asli (untuk seed save-format RE nanti).
- **Acceptance:** matriks §3 semua baris ✅/🔬→✅ atau ⏭ ber-alasan; `docs/formats/*.md` M5 final.

### P1 — Offline pipeline & pack
- `bake.mjs` end-to-end: disc → `assets/*.fpk` (textures ASTC-6x6 fallback RGBA8, meshes,
  anims bin, audio OGG/PCM, tables+dbx+mess JSON, font atlas, script JSON bila sudah).
- Manifest + hash; size budget ≤ 600 MB APK (lebih: split assets zip di repo data SSH +
  side-load `sdcard/FEPA/` — fallback terdokumentasi).
- **Acceptance:** `pack.mjs check` 0 error; ekstrak ulang byte-identical; screenshot test scene
  dari pack di desktop harness.

### P2 — Native core on device (MILESTONE TAMPIL)
- Gradle/NDK/CMake + NativeActivity + Kotlin shell; fpk mmap reader; GLES3: blit tekstur +
  sprite batch + ortho cam; input layer penuh (overlay tombol + keyboard + gamepad, satu action
  enum); Oboe audio mixer; watchdog + log ring; **cold-boot state machine**: BOOT→ATTRACT→(START)→TITLE.
- Test scene di hp: atlas ikon + 360 portrait scroll + Ike 3D idle anim + musik stm + teks
  dialog c09 + tombol overlay semuanya hidup; rapid-fire tap A + d-pad 4 arah diukur responsif.
- **Acceptance:** constraint §1 butir 2,3,4,5,6,7 lulus di hp fisik (human) + harness desktop.

### P3 — Vertical slice Bab 1 (MILESTONE GAMEPLAY)
- Map scene: ground `/s` stage, unit ymu, kamera, gerak grid, cursor; battle scene: model zu +
  anim + event codes (hit frame .ga) + damage; script VM c01 (dialog .m + moves + camera);
  UI minim (HP bar, action menu serang/tunggu, portrait, fast text).
- Damage formula + weapon triangle + miss/crit dari dok P2; enemy AI 1 tingkat (pursue/attack).
- **Acceptance:** alur capture c01 replay-able: cutscene Greil→Ike→battle→tutup chapter;
  no-freeze stress 10 menit spam input.

### P4 — Sistem penuh + save
- Inventory/equip/consume, growth+level-up (seed RNG dicocokkan ke capture = **paritas angka
  FE9**, golden diff vs log capture), class change/seal item, effectiveness (axe-horse dll),
  arenas/shop/tutorial EN popups, convo (c?? script), save/load multi-slot + autosave +
  resume, options (speed teks, bgm, sound, vibrasi?).
- **Acceptance:** 5 golden replays (babi berdarah: level-up growth, crit, effectiveness, item
  break, game over) byte-state-match vs capture.

### P5 — Konten penuh campaign (30 chapter)
- Pipeline batch: semua chapter script/stage/asset; gmap overworld + pilih route + branch
  route; semua job model/anim (gap = fallback job generik + issue list); ending sequence.
- **Acceptance:** playthrough manusia sampai Bab 15 tanpa blocker; tracker gap terbuka.

### P6 — Presentasi & audio penuh
- `/illust` CG, `/Movie`: **keputusan** still+SFX (default) vs mp4 transcode; efek .yme
  (trails/burst) approximation; musik loop titik benar; voice? (FE9 minim voice) — polish.
- **Acceptance:** semua scene presentasi punya representasi atau issue terdaftar.

### P7 — QA & perf device
- Device matrix (minimal 1 mid-range Adreno + 1 Mali), 60fps di semua scene, RAM ≤ 256 MB,
  battery sanity, touch UX (haptik, safe area, landscape), keyboard/gamepad di desktop.
- Stress: 1 jam autoplay monkey + freeze detector; boot cold 20× → selalu attract.
- **Acceptance:** CI hijau semua signature + laporan device human.

### P8 — Rilis internal
- Signing key repo-lokal, versioning, changelog; base build → distribusi WhatsApp;
  prosedur update (delta pack); docs: README install + keymap + FAQ; repo code-only bersih
  (pre-commit hook: blokir aset disc).
- **Acceptance:** build terpasang + main dari WhatsApp base di hp non-dev (human lain).

---

## 5. Risiko & mitigasi

| Risiko | Dampak | Mitigasi |
|---|---|---|
| Script VM `.cmb` lebih rumit (state machine + C callback) | P3 molor | spike pertama P0; fallback: interpreter per-chapter hardcode + extractor semi-otomatis (43 file saja), Ghidra oracle + Dolphin trace |
| Model `.g/.gs` shader `tev` kompleks (battle matcap/rim) | visual beda | tier approximation: unlit→vertex-color→fog; A/B screenshot vs Dolphin oleh human |
| Audio seq format (.mus/.seq) dalam | musik absent | fallback: STM streams dulu; seq decode belakangan; musik bukan gerbang fase |
| Growth RNG FE9 = tabel+seed tersembunyi | paritas level-up meleset | golden replay capture save SRAM; Ghidra locate RNG sebagai oracle; paritas boleh di-degrade ke "stat match rata-rata" dengan persetujuan |
| Size assets > budget APK | distribusi WhatsApp ribet | ASTC+OGG; split data zip; side-load dir fallback (§P1) |
| Device GL driver quirky (Mali precision) | glitch | shader hint mediump; test matrix P7; reference device = hp kamu |
| Scope campaign penuh | molor selamanya | vertical slice dulu; gerbang per fase; gap tracker publik di repo |
| Regresi input/boot yang historis | ulang luka lama | acceptance §1-butters di CI sejak P2, tidak pernah opsional |

---

## 6. Peta acceptance ↔ constraint produk

| Butir §1 | Dites di | Test |
|---|---|---|
| 1 rebuild-nol | review kode | tidak ada kode hasil translasi/patch; repo tree engine baru |
| 2 touch overlay | P2→semua | screenshot overlay + manual device |
| 3 keyboard primary | P2 | keymap default di harness tanpa config |
| 4 jalur input tunggal | P2–P5 | script harness: SELECT di menu → masuk stage → tembakan terproses |
| 5 rapid-fire 4 arah | P2–P7 | replay 600 tap + hold arah; latency assert |
| 6 attract-first | CI semua fase | cold boot assert state==ATTRACT sampai event START |
| 7 no-freeze | CI semua fase | frame counter & watchdog 10 mnt |
| 8 WhatsApp base | P8 | instal dari file WhatsApp hp kedua |
| 9 repo bersih | semua commit | hook + `git ls-files | grep -v '\.\(png\|ogg\|fpk\)$'` |

---

## 7. Pembagian kerja & ritual

- **Agent:** semua tool RE/bake, engine code, docs, CI lokal (script), commit+push tiap fase.
- **Human:** Dolphin capture (P0) & A/B visual, instalasi/qa hp fisik, review plan tiap gerbang
  fase, keputusan ⏭ (video, dll).
- Gerbang review: akhir P0, P2, P3, P5 (kode TIDAK ditulis sebelum plan ini approved).
- Repo: `fe9-port` (engine+tools+docs), branch `main`; branch per fase panjang → merge.

## 8. Keputusan yang butuh restu kamu (boleh centang sekarang atau di review)

1. **Video cutscene**: (a) still CG + SFX + teks (default, ringan) atau (b) transcode MP4
   (butuh ffmpeg portable di PC dev).
2. **Fallback data besar**: side-load folder vs split APK vs data-only zip — kalau bake
   ternyata > 600 MB.
3. **Grafis battle**: approximation bertingkat (default) vs port shader tev penuh ke GLSL
   (investasi +2–3 minggu di P6).
4. Engine: **C++20 + GLES3 + Oboe + NativeActivity** (default boring) — tolak? sebutkan
   alternatif.
