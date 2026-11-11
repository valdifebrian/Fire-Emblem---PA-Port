# FE9 - NPC/AI & proc-engine notes (research pass 2026-11-11)

Source mining: `StanHash/FE9-DOC` (notes; all-rights-reserved -> facts distilled, not copied),
feuniverse thread 8243, plus a **fresh independent decode of `zdbx.cmp` on this machine**
(see §Validation). Addresses are FE9U (GFEE01) unless stated.

## Two layers of "NPC AI"

### 1. Data layer (extractable today)
Per-unit enemy AI config lives in the **dispos record**, not in the DBX tables
(verified: zero `ai` columns across all 449 `*.dbx` on-disc):

- `dispos` record, `sizeof = 0x6C` — the load-unit struct used by chapter/army placement
  (`zmap/bmapXXXX/` per home-machine findings):
  - `+4C/+50/+54/+58` = **4 AI words** (StanH: "ai?") -> copied into `UnitInfo+0x38/0x3C/0x40/0x44`.
  - `+5C/+5D` = map x/y; `+60` level; `+62` autolevel flag; `+64..6B` item flags (bit0 droppable, bit1 NG+).
- Decoder: `DisposFileDecode` @ `0x80107B54` (file data -> records).
- Consumer: `UnitLoadUnitInfo` @ `0x8003EAE8` (UnitInfo -> live `Unit`).
- Semantics of the 4 words (mode/trigger/target/param) = next task for the JS lane:
  correlate known chapters' dispos values with observed behaviour; FE10 army editor
  (feuniverse thread 8117) edits the same field group in FE10 -> struct family is shared.

### 2. Code layer (matching-decomp lane)
The routines that *interpret* those words (movement scoring, threat maps, difficulty
scaling `Normal/Hard/Maniac` - already named via the event-table seed). Entry points:
- ~20 `EVT_Enemy*Check*` procs named in `symbols.txt` (from the 405-name commit) cross-call into AI.
- FE9 uses **C++ with vtables** (vtable header: `+00` inheritance offset, `+04` RTTI-ish;
  methods from `+08`). AI classes likely vtable-driven -> enumerate vtables in `.data`,
  xref from dispos consumer `0x8003EAE8`.
- StanHash's `C++.txt` documents the ABI shape (same author's FOMT-DOC for the ARM analogue).

## Proc engine (battle/anim/AI choreography) — distilled
- Proc struct `sizeof = 0x70`: scr start/cursor, `onEnd/onUnk/onLoop` callbacks, name,
  parent/child/prev/next tree links, loop counter, flags (`bit1` locks parent;
  `bit5?` = discardable).
- **Proc-instruction dicts**: function-pointer tables indexed by instruction id; engine
  registers up to 9, game uses 3. Now named in our symbols:
  `gProcInstrDict0/1/2` = `0x80283828 / 0x80284098 / 0x80288AA8` (`.data`).
- Instruction header word: `[24b identifier | 4b dict id | 4b size-in-words | 16b operand]`.
- ~40 instruction kinds documented by StanH (StartChild/CallFunc/WhileFunc/Goto...):
  proc graphs = the game's animation/battle choreography; battle actions themselves:
  `BattleAction` = 4 bytes `[id byte | pad | value short]`.
- Weapon-effect procs (GBAFE-style "Proc" equivalent) are driven through these dicts —
  cross-reference when decoding `zdbx` battle data (BRV-BAT-*).

## Other durable facts mined this pass
- `hash37` @ `0x8000D598`: cstr hash (result = result*37 + c); `hash31` is the same with 31.
  Bucket id: `h - ((h * 0x80C121B3u) >> 40) * 509`.
- File-path registry: `gFileEntryArray` @ `0x802F04E0` (`.bss`, 0x1F000): `FileEntry`
  `sizeof 0x7C` = normalized path (leading `/` stripped, lowercased), hash37, flags
  (`C00`=cmp, `800`=pak, `400`=cms), last-used tick, data pointer.
- `gForgeData` @ `0x803206EC`: +10 might, +11 hit (x5), +12 crit (x3), +13 weight malus.
- `std::string` in FE9 = `{size, capacity, ptr}` (no SSO).
- r13 (GCC sdata base): FE10 note says `r13-0x69B8` holds mode bits, `bit13` = hard mode
  (kills map-affinity bonuses). Same global likely exists in FE9 - check when matching sdata.
- `zdbx/param.dbx` = "キリバト・パラメータ" (kiribato battle-solver params) + `battle.dbx`
  (394 lines) = the FE9 damage-calc table set -> direct input for BRV-BAT-ADJ.

## Validation (this machine, 2026-11-11)
- Implemented LZ77(0x10, size field ignored, decode-to-EOF) + pack walker from the
  byte-verified spec: `zdbx.cmp` -> blob `pack` **562,337 B, entry count 449** —
  matches the community-documented "449 .dbx inside zdbx.cmp" exactly, and names include
  `zdbx/battle.dbx`, `zdbx/classList.dbx`, `zdbx/jobList.dbx`, `zdbx/param.dbx`.
- Symbol application: 8 renames (`hash37`, `DisposFileDecode`, `UnitLoadUnitInfo`,
  `gFileEntryArray`, `gForgeData`, `gProcInstrDict0/1/2`) -> relinked DOL sha1 unchanged.

## Address ledger
See `FE9_STANHASH_MINING.csv` (same folder) — every mined address with confidence,
dtk cross-check status, and note. Unnamed entries: `0x802ADB28` hash-bucket array
(no dtk symbol yet), vtables `0x8028DFE8`/`0x8028E200` (ObjectA/B, still anonymous).

## Sources
- feuniverse thread 8243 (FE9 battle system/StanH); `github.com/StanHash/FE9-DOC`
  (Procs.txt, Dispos.txt, Events.txt, BigData.txt, Items.txt, Units.txt, Unk.txt,
  Battle.txt, C++.txt, ChapterState.txt, R13.txt).
- feuniverse thread 8117 (FE10 army data / enemy AI editor, same struct family).
