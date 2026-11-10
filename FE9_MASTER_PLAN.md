# FE9_MASTER_PLAN.md — Fire Emblem: Path of Radiance → Android Native

**Versi 2.0** (2026-11-10). Direvisi dari review arsitek/PM (7.5/10, "perlu revisi sebelum
approval"). Menggantikan `fe9-android-port-plan.md`. Dokumen ini + 3 artefak pendamping adalah
**satu-satunya sumber kebenaran proyek**:

| Artefak | Fungsi |
|---|---|
| `FE9_MASTER_PLAN.md` (file ini) | Visi, scope, arsitektur, fase, REQ, risiko, governance |
| `docs/FE9_ASSET_MATRIX.csv` | Inventaris + status seluruh keluarga aset (ID `AM-###`) |
| `docs/FE9_BEHAVIOR_SPEC.md` | Spesifikasi perilaku game (ID `BRV-###`) |
| `docs/FE9_TEST_MATRIX.csv` | Test case + hasil verifikasi (ID `T-###`) |

**Prinsip pengukuran kemajuan (revisi review §9):** kemajuan = **jumlah functional parity yang
terverifikasi terhadap FE9 asli** (pass-rate `FE9_TEST_MATRIX.csv`), BUKAN jumlah file terekstrak
atau modul tertulis. Membaca 1000 model ≠ model system selesai.

**Prinsip akurasi:** jangan mengorbankan akurasi gameplay demi demo pertama.

---

## 0. Disposisi review (10 poin → perubahan konkret)

| # Review | Disposisi |
|---|---|
| A. Jangan kunci arsitektur sebelum bukti | **Diterima.** Stack jadi "baseline terkunci-setelah-PoC". P2 = Architecture PoC Gate (5 kemampuan, §8). |
| B. RE = behavior, bukan hanya aset | **Diterima.** Dua dokumentasi terpisah: Asset Matrix (teknis) + `FE9_BEHAVIOR_SPEC.md` (perilaku). Engine tidak implement sistem tanpa BRV terisi. |
| C. Script VM prioritas utama | **Diterima.** Metodologi 7 langkah (§7); `AM-017 /Scripts` = prioritas #1 P1; chapter tidak di-hardcode. |
| D. Jangan anggap format sudah diketahui | **Diterima.** Disiplin status 4 tingkat (§5). `.pak/.gs/.stm/.cmb(script)/font/struktural /s` = Discovered/Unknown di matrix. |
| E. Data completeness matrix | **Diterima.** `FE9_ASSET_MATRIX.csv` (57 keluarga) kolom lengkap: source/format/decoder/output/status/verification/deps/consumer. |
| Arsitektur: GSM + SceneMgr + ResMgr | **Diterima.** Tiga modul first-class (§3). |
| Roadmap: PoC sebagai gate terpisah | **Diterima.** Urutan 9 fase P0–P8 versi reviewer (§8). |
| Area belum cukup diperhatikan | **Diterima.** Jadi requirement wajib ber-ID (§4), semuanya punya test di TEST_MATRIX. |
| Risiko + IP | **Diterima.** §9 (peringkat reviewer) + §10 kebijakan IP. |
| Governance: 4 artefak, DoD, task ID | **Diterima.** §11. Task ID dipakai di commit; TODO sesi lama tidak dipakai. |

---

## 1. Target akhir & constraint

**DoD proyek:** satu APK Android menjalankan campaign FE9 (USA/GFEE01, EN) utuh — title→
chapter→ending — **tanpa Dolphin/emulator saat runtime**; asset asli dipakai sebagaimana adanya
(konversi demi optimasi saja); base build privat via WhatsApp; dev loop dari PC Windows, deploy
USB ke hp fisik.

Constraint produk (regresi historis — tiap butir = test wajib CI):

1. Rebuild engine dari nol; binary PPC hanya oracle.
2. **Touch overlay wajib** dan bagian desain sejak awal (bukan add-on).
3. Keyboard default hidup tanpa config (di harness dev desktop).
4. Satu jalur input menu↔gameplay (pilih karakter → masuk stage → action hidup).
5. Rapid-fire + 4 arah simultan responsif.
6. Cold boot → **ATTRACT/demo**; main hanya setelah START eksplisit.
7. Tidak ada freeze (watchdog + frame counter, termasuk kasus bridge 1/32).
8. Android = target utama; **desktop runner hanya alat development/testing**.
9. Repo publik = kode + docs + CSV; **tidak ada aset disc** (kebijakan IP §10).

---

## 2. Keputusan fundamental (D1–D5 — **DISETUJUI 2026-11-10**)

| ID | Keputusan | Status |
|---|---|---|
| D1 | Baseline **C++20 + Kotlin + JNI + CMake/NDK + GLES3.0 + Oboe + NativeActivity**, terkunci setelah P2 PoC lulus; Vulkan ditunda sampai profiling perlu. **Setuju — dengan catatan reviewer: persetujuan BUKAN immutabilitas; jika PoC gagal, baseline dibuka kembali tanpa dianggap kegagalan proyek.** | ✅ |
| D2 | Target pertama = **Chapter 1 playable penuh** (bukan demo visual), DoD §8-P4. | ✅ |
| D3 | **Semua chapter direproduksi** (event, dialog, animasi, ending) — tanpa subset. | ✅ |
| D4 | **Aset asli dipertahankan**, konversi hanya optimasi (ASTC/OGG/JSON). | ✅ |
| D5 | Android hp = target utama; desktop = dev/test tool saja. | ✅ |

---

## 3. Arsitektur target (post-PoC)

```
┌ Android Application Layer (Kotlin)
│   Activity lifecycle · permissions · storage paths · JNI bridge · splash
├ JNI Interface (thin, satu arah, tanpa logika game)
├ FE9 Native Game Engine — C++20
│   Game State Manager      ← pusat state; satu sumber kebenaran mode
│   Scene Manager           ← transisi: title→menu→chapter→map→battle→dialog→result→next
│   Script VM               ← opcode hasil RE /Scripts (AM-017)
│   Battle Engine           ← damage/triangle/crit/arena; consumer BRV-BAT
│   Map Engine              ← grid/terrain/move cost/AI; consumer BRV-MAP
│   Character System        ← unit/inventory/growth/promotion; BRV-CHR
│   UI Engine               ← menu/status/inventory/dialog; font atlas + ikon + portrait
│   Animation System        ← .ga curve evaluator (linear, absolute), event-code consumer
│   Input Manager           ← satu action-enum; sumber: touch overlay | keyboard | gamepad
│   Audio Engine            ← mixer channel, BGM loop/transition, SFX polyphony
│   Save Manager            ← atomic write, backup, migration; BRV-SAVE
│   Resource Manager        ← fpk mmap + cache LRU + budget + async load/unload
│   ├── Graphics: OpenGL ES 3.0 (+ approximation tier GC tev)
│   ├── Audio out: Oboe
│   ├── Input: AInput (touch/key) + joystick
│   └── Storage: files dir (saves/config)
└ FE9 Resource Pack (.fpk) — hasil bake offline PC
    models · textures · animations · audio · scripts · tables · UI · font
```

Aturan modul: dependensi satu arah ke bawah; state hidup HANYA di Game State Manager +
scene object aktif; modul lain stateless-terhadap-save. Resource Manager wajib mendukung
async load tanpa halt (REQ-LOAD).

---

## 4. Requirements lintas-kutling (wajib, ber-ID; test di TEST_MATRIX)

**Waktu & determinisme**
- REQ-TIME-1 Game logic **fixed timestep 60 Hz** (akumulasi, max catch-up 4 tick); rendering
  **interpolated** dari state tick; animasi = bagian logika (bukan render); audio clock
  independen; input di-sample per event OS dan dikonsumsi pada tick. Perbedaan performa device
  TIDAK mengubah kecepatan game.
- REQ-TIME-2 Replay capture input (log + seed RNG) menghasilkan state identik di device mana pun
  (determinisme RNG = tabel asli FE9, BRV-RNG).

**Memori & loading**
- REQ-MEM-1 Texture cache LRU + budget per-kategori (target ≤ 160 MB GPU, ≤ 128 MB native heap
  mid-range); unload-on-scene-exit; object pooling untuk unit/partikel/quad UI.
- REQ-LOAD-1 Async loading dengan indikator; scene switch tidak pernah freeze UI > 2 frame.

**Ketahanan**
- REQ-ERR-1 Logging ring-buffer + crash → dump stack+state; deteksi save korup (CRC) → tawarkan
  slot backup; watchdog 5 s tanpa frame advance → laporkan sebagai BUG-REG (signature §1.7).
- REQ-SAVE-1 Atomic save (tmp+fsync+rename), slot aktif + backup rotasi 2, versi skema +
  migrator eksplisit.

**Audio**
- REQ-AUD-1 BGM: loop points benar, crossfade/duck saat dialog; SFX polyphony cap + voice
  stealing; master/BGM/SFX volume terpisah persisten.

**Teks**
- REQ-LOC-1 UTF-8 internal; glyph coverage font FE9 terverifikasi (uji string semua karakter
  .m + punctuation); word wrap + control code FE9 (`$c $s $K $w $d $E …`) penuh.

**Lifecycle & display Android**
- REQ-LIFE-1 pause→autosave-state-resume; proses di-kill OS → cold resume; ignore rotation
  (landscape fixed); handle multi-resume tanpa leak.
- REQ-DISP-1 Render internal 512×384 → integer/near-neighbor upscale, 4:3 letterbox; tablet
  safe-area; cutout handling.

**Performa & kompatibilitas**
- REQ-PERF-1 16.6 ms/frame di scene terberat (battle b.15-an), thermal soak 30 mnt tanpa
  degradasi > 15% fps; profiling hook sejak P2.
- REQ-COMP-1 Lulus di ≥2 vendor GPU (Adreno + Mali) dan 1 device mid-range low-RAM.

---

## 5. Pipeline aset & disiplin status

### 5.1 Dua scoreboard TERPISAH (revisi review v2-B)

Snapshot di `docs/SCOREBOARD.md`, digabung manual per commit:

1. **Asset readiness** — agregat status `FE9_ASSET_MATRIX.csv` (berapa keluarga per status).
   Mengukur KESIAPAN DATA, bukan selesai sistem.
2. **Functional parity** — pass-rate `FE9_TEST_MATRIX.csv` + paritas per chapter/sistem
   (BRV-### terverifikasi). **Inilah satu-satunya angka kemajuan proyek.**
   Contoh: model family = Decoded ≠ Animation System selesai — skeleton/material/anim/render
   masing-masing butuh T- sendiri.

### 5.2 Format identification by probe (2026-11-10, `docs/FE9_MAGIC_PROBE.csv`)

Setiap keluarga aset diprobes magic 16B per-file (±3 sampel/keluarga) — output matriks WAJIB
mengutip bukti ini, bukan asumsi ekstensi:

- **Semua `.cmp` disc = LZ77 → blob `pack`** (magic terverifikasi `007061636b`); `.pak` = pack
  RAW (mis. `/zu/amr1_amr1_ja.pak` varian bahasa `_ja/_no`).
- **Unit model = pack berisi**: `skeleton.g` + `body.gs` + anim `.ga` + `tex_*.tpl`
  (dibuktikan dari `/ymu/archer/pack.cmp` 8/8 entri) → `.g` unit TIDAK di root (root cuma 3
  prop); `.gs` = body/shell (spec tellius body). `/xwp/*.cmp` senjata, `/xcam/cam.cmp` kamera,
  `/yme/EID_*.cmp` efek, `/s` stage campur (sampel = TPL ground tex).
- `/Scripts/*.cmb` = magic `"cmb\0"` + self-name + blok berulang — investigasi P1 #1.
- `/Movie` = **THP** (+`dummy` THP); `/opening.bnr` = BNR1.
- `/Sound` = bank `gcfesnd` (`.slib .sdir .pool .proj .samp .stbl .etbl`) + `.stm` TANPA
  header SEMh (kemungkinan raw/STM-headerless; nama `_lp` = loop variant); `stbl` memuat
  string `SE1_EVT_*` (tabel SFX per event!).
- `/Fonts` = `.cms` (LZ77 TPL atlas) + `.gcf` (metrics, investigasi).
- `/xcam/<job>/*.dbx` teks per (anim,wpn) = tabel kamera battle. `*.bin` root/window/shop =
  tabel header `[u32 …]` pola FE8Data.
- Inventori terekonsiliasi: re-walk FST vs `docs/inventory/files.csv` = 4077↔4077 diff NOL
  (path|pos|size).

Kebijakan anti-salah-klasifikasi (reviewer A): kolom `format` hanya boleh diisi magic terbukti
`[probe]`; sisanya `UNKNOWN [probe]` + status `Discovered/Investigating` +
notes `Pending Format Investigation`. Generator = `bin/assetmatrix.mjs` (reads
`docs/inventory/files.csv`).

```
disc RVZ → tool node (fe9-re/bin) → bake → assets/*.fpk → APK (atau data pack side-load)
```

Status keluarga aset (single source: `docs/FE9_ASSET_MATRIX.csv`):

| Status | Arti |
|---|---|
| **Discovered** | File ditemukan & dihitung; format belum dibuktikan |
| **Investigating** | Decode sedang berjalan; ada bukti struktur parsial |
| **Decoded** | Decoder menulis output terdefinisi + validasi mandiri (spek/manual/statistik) |
| **Verified** | Output **dipakai engine dan cocok dengan game asli** (visual A/B vs Dolphin atau diff golden) |
| **Deferred** | Ditunda eksplisit dengan fase penerima + alasan |

Kebijakan: ekstraksi/extract-able ≠ Decoded. Verifikasi = metode tertulis di kolom `verification`
(vision-check, golden diff, A/B screenshot, byte round-trip). Naik status = commit yang
men-change row matrix + bukti dirujuk di commit message `[AM-###]`.

Iskripsi P0 saat ini (57 keluarga): 6 Decoded · 1 Partial · 39 Discovered · 11 Deferred.
Belum ada **Verified** — jujur: belum ada aset yang masuk engine.

---

## 6. Game Behavior Specification

`docs/FE9_BEHAVIOR_SPEC.md` = dokumen perilaku (terpisah dari format). Format entri `BRV-###`:
pernyataan aturan, bukti (field CSV/observasi/Ghidra/wiki FEU), confidence (Confirmed/Strong/
Reported), dan test T-### penerimanya. **Gate P3+: fitur engine tidak boleh diimplement sebelum
BRV domain terkait ≥ Strong.** Domain wajib: battle, character/growth, AI, map/terrain,
battle animation trigger/timing, chapter event, dialogue, progression, UI flow, save state
(+ RNG). Sumber bukti: tabel hasil RE (items/classes/characters), `.dbx` (lookup/param), capture
Dolphin (human), Ghidra oracle, FEU wiki — dicatat, bukan diasumsikan.

---

## 7. Metodologi Script VM (prioritas RE #1)

Urutan wajib (review §C), dieksekusi P1→P3, target: **chapter content = data, bukan kode**:

1. **Identifikasi opcode**: stat-diff 43 file `.cmb` (field sering berulang), korelasi dengan
   strings Ghidra `main.dol` (nama rutin event) + chapter 1 ter-capture di Dolphin (human).
2. **Parameter per opcode**: arg widths via scan-alignment + cross-file konstanta (unit id,
   coord, frame).
3. **Branching/conditional**: locate condition blocks (IF/ELSE/jump tables), verifikasi pada dua
   chapter bercabang (route divergence b.??).
4. **Event trigger**: bandingkan dengan state game saat capture (unit muncul, tile flag, item).
5. **Interaksi battle engine**: script ↔ unit spawn/kamera/dialog → definisi instruction set VM.
6. **Interpreter**: VM kecil di engine (C++) menjalankan script JSON hasil konverter
   (`scriptc.mjs`), 1:1 instruksi.
7. **Verifikasi**: replay chapter di engine vs capture Dolphin, step-by-step (T-SCT-###).

Anti-pattern dilarang: hardcode chapter per file C++ (kecuali escape hatch ber-ID + issue
terbuka).

---

## 8. Roadmap — 9 fase dengan gate

Setiap fase: exit gate = baris TEST_MATRIX ber-fase = PASS. Review gate (manusia) di akhir
P1, P2, P4, P6, P8. Kode engine hanya mulai ditulis setelah plan v2 approved (D1–D5) —
kecuali harness PoC milik P2 yang memang alat bukti.

### P0 — Project Audit & Baseline (agent; ±sekarang)
- Audit tool/commit/dok; reproduksi build pipeline dari nol di direktori bersih (fresh `git
  clone` + jalankan semua tool → artifacts identik hash-nya); bebankan SEMUA temuan ke matrix.
- Triage leftover dev (`.gcf/.sdir/.stbl/…` AM-###) → close.
- Output: baseline report + matrix v1 beku sebagai rujukan; daftar pertanyaan RE untuk P1.
- **Gate:** reproduksi byte-identik 5 artefak kunci (CSV, face PNG sampling, ga JSON, dbx dump,
  .m decode).

### P1 — Reverse Engineering Completion (agent + capture human)
Prioritas urut: **(1) Script VM langkah 1–5 (§7)** → (2) model/skeleton: locate skeleton unit
(.g 3 file saja di root → kandidat dalam `.pak`/`AM-004,009`), parser `.g`+body (spec tellius
ada) → (3) `.pak`+`/s` stage magic → (4) `.stm` decoder + `/Fonts` parse → (5) `dbx2json` SJIS →
(6) `/illust` batch decode → (7) `.gs`, `/zmap|/gmap`, shop/tutorial, `.yme` survey.
Human: capture Dolphin b.1–2 (input log+video+SRAM save asli) + A/B visual session terjadwal.
Behavior: isi BRV domain event/dialogue/map dari observasi + wiki (status jujur).
- **Gate:** AM baris model/script/audio/font ≥ Decoded; VM langkah 1–4 done; PoC-inputs tersedia
  (1 model+anim, 1 musik, 1 stage tileset); capture b1–2 lengkap. **Tidak perlu 100% aset —
  cukup semua input PoC & Chapter-1.**

### P2 — Architecture PoC (GATE TEKNIS; device nyata)
5 kemampuan fundamental, semua di hp fisik: (1) load+render tekstur FE9 (atlas ikon+portrait);
(2) load+render 1 model 3D asli (Ike) dengan kamera; (3) 1 animasi `.ga` benar (frame timing +
event codes ter-baca); (4) 1 BGM `.stm` + 1 SFX dengan mixing; (5) touch overlay responsif
(latency tap < 50 ms, multitap rapid, 4-arah) + keyboard di harness desktop.
Plus: fpk loader, profiling hook, meteran fps/thermal — REQ-TIME/PERF sudah aktif di PoC.
- **Gate:** kelima bukti terekam (screenshot/log/video oleh human + angka latency). **Jika gagal
  → berhenti; arsitektur dievaluasi ulang SEBELUM modul lain ditulis** (D1 bisa dibuka kembali).

### P3 — Core Engine Foundation
Modul §3 penuh di atas PoC: GSM, SceneMgr, ResMgr (cache/async/evict), Input (satu action enum
3 sumber), Audio mixer, Save Manager + skema v1, UI Engine dasar (font+ikon+wrap), Script VM
interpreter (VM langkah 6) + test harness replay desktop.
- **Gate:** T suite: boot→attract→start machine, scene round-trip title→debug-stage→title,
  save/load/reload, REQ-TIME-1/2 (two-speed sim), REQ-MEM-1 budget, REQ-ERR-1 watchdog,
  REQ-LIFE-1 lifecycle.

### P4 — First Playable Vertical Slice: CHAPTER 1 (milestone mayor; D2)
**Definition of Done Chapter 1** (review §8 — semua wajib): mulai dari menu; map tampil benar;
semua unit yang diperlukan muncul; movement+terrain benar (cost per BRV-MAP); damage calc =
referensi (golden battle cases); battle animation berjalan (trigger+timing BRV-ANM); dialog+event
berfungsi (script c01 via VM); victory/defeat bekerja; chapter completion → gmap/transisi;
save/load melanjutkan progres; no-freeze stress replay.
- **Gate:** DoD di atas = T-C1-* semua PASS + playthrough manual human.

### P5 — Complete Gameplay Systems
Battle formulas lengkap (effectiveness, crit, weapon break, arena), AI (target/move/engage/
retreat), inventory/equip/consume, growth+level-up (paritas golden vs capture + seed SRAM),
recruitment/promotion/seal, convo, shop, tutorial EN, gmap routing+branch, progression/unlock.
- **Gate:** T-SYS-* golden cases per sistem PASS; 5 golden replays state-match.

### P6 — Full Content Port (D3)
Batch 30 chapter: scriptc semua `.cmb`; stage/asset semua chapter; unit/job lengkap (gap →
fallback generik + issue); illust/CG; ending; efek `.yme` tier approximation (keputusan
shader-tingkat: default approximation, opsi port tev penuh = +2–3 minggu, restu P6).
- **Gate:** semua chapter load-able; parity scoreboard per chapter ≥ X% (angka ditetapkan di
  review P5); issue gap terdaftar.

### P7 — Full Game Parity & Optimization
Regression penuh; A/B visual (human) vs Dolphin per scene kategori; perf/thermal/mem di device
matrix (REQ-COMP-1); audio full (loop/duck); touch UX polish (haptik dsb); save integrity fuzz.
- **Gate:** TEST_MATRIX PASS-rate target (threshold disepakati di gerbang P6, default 95%
  functional parity campaign) + zero BUG-REG freeze.

### P8 — Android Release
Gradle signed release (keystore lokal, tidak di repo), versioning+changelog, install USB +
**base build privat via WhatsApp**, README install/keymap/FAQ, hook pre-commit blokir aset.
- **Gate:** build terpasang+dimainkan dari WhatsApp base di hp non-dev.

---

## 9. Risiko (peringkat reviewer + mitigasi)

| Risiko | Level | Mitigasi |
|---|---|---|
| Chapter script tidak sepenuhnya dipahami | **Critical** | §7 metodologi + VM data-driven; fallback escape hatch per-chapter ber-ID; Ghidra + Dolphin trace |
| Model/animation fidelity | **Critical** | PoC awal (P2) sebelum investasi modul; tier approximation; A/B human |
| Game logic parity | **Critical** | BRV dulu → golden replays (T-SYS); seed RNG dari save asli |
| Scope proyek | **Critical** | Gate per fase; vertical slice; gap tracker publik |
| Format belum diketahui (`.pak/.gs/script/…`) | High | Matrix status jujur; P1 berurutan prioritas |
| Audio sequencing (.mus/seq bila dipakai) | High | Pipeline STM dulu; seq diisolasi di Audio Engine |
| Android memory/perf | High | REQ-MEM/PERF aktif sejak PoC; profiling hook |
| IP (aset/kode Nintendo) | Medium-High | §10; repo tanpa aset; distribusi privat ke pemilik disc |
| Regresi input/boot historis | Medium (pasti ulang tanpa gate) | T suite §1 wajib CI dari P2 |

---

## 10. Kebijakan Intellectual Property

- Repo publik/`main`: **hanya** engine code, tooling, docs, CSV tabel hasil RE (tanpa payload
  aset disc), plan/test matrix. Tidak pernah commit: PNG/OGG/texture/model/script mentah,
  ISO/RVZ, save asli.
- Aset bake (`.fpk`) TIDAK masuk git; dihasilkan di PC dev dari disc milik sendiri; distribusi
  base build WhatsApp = privat, diarahkan ke pemilik game asli. Tooling tidak memuat payload.
- Tidak menyalin kode/decompilasi PPC ke runtime (rebuild-nol §1.1); hasil Ghidra = catatan
  riset di `fe9-re` (lokal), bukan repo publik.
- Referensi pihak ketiga (tellius-forge/mila) = baca-saja untuk spesifikasi; tidak di-vendor
  (GPL) ke repo.

---

## 11. Governance

- **Task ID:** setiap commit memakai `[AM-###]`/`[BRV-###]`/`[T-###]`/`[P#]` sesuai artefak;
  temuan baru = row baru matrix/BRV/test dulu, baru kode. TODO sesi lama tidak dipakai sebagai
  sumber keadaan proyek — status proyek = matrix + fase §8.
- **Ritual:** agent = RE/bake/code/docs/CI; human = capture Dolphin, qa hp, gate review P1/P2/
  P4/P6/P8, keputusan terbuka (mis. video cutscene: still+SFX default vs MP4; >600MB →
  side-load/split/zip; shader tier; stack bila PoC gagal).
- **Restart-session protocol:** baca `FE9_MASTER_PLAN.md` → `docs/FE9_ASSET_MATRIX.csv` →
  `docs/FE9_BEHAVIOR_SPEC.md` → `docs/FE9_TEST_MATRIX.csv` → `docs/SCOREBOARD.md`, baru lanjut;
  jangan pernah percaya memori sesi untuk status proyek.
- **Urutan kerja wajib (instruksi reviewer v2):** (1) validasi asset matrix — selesai via
  magic probe §5.2, jaga saat berubah; (2) dokumen konsisten di repo (reproducible fresh-clone,
  T-BASE-001); (3) rekonsiliasi inventori vs source (done: 4077↔4077 diff nol); (4) RE prioritas
  tinggi: model (`skeleton.g`/`body.gs`) → animasi → script `.cmb`; (5) PoC teruji di Android
  fisik; (6) **tunggu hasil PoC sebelum mengunci arsitektur** dan full engine.
  Jangan mulai full engine implementation hanya karena master plan selesai.
- Keputusan terbuka non-blocking dicatat di `docs/OPEN_DECISIONS.md` (dibuat P0).
