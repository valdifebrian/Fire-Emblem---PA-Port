# Fire Emblem: Path of Radiance — RE & Port workspace

Reverse-engineering frontend for *Fire Emblem - Path of Radiance* (GFEE01, NTSC-U, Disc 1)
toward an independent port. Disc container support: **RVZ v1** (zstd, PRNG-packed).

## Status (2026-11-11)

| Milestone | State |
|---|---|
| M0 RVZ v1 reader (JS, runtime-neutral node/Bun) | done, all 11139 groups audited |
| M1 FST inventory + extraction (`main.dol`, `opening.bnr`, samples) | done — 4077 files / 545 dirs / 1297 MB |
| M2 `main.dol` PPC analysis (DOL→ELF→Ghidra headless) | done — 8125 functions, 5690 strings, entry=`__start` verified |
| M2b matching-decomp infra (`decomp/`, dtk 1.8.4) | done — split→relink **byte-identical** (build.sha1 OK), 9251 functions auto-detected |
| M3 `.cmp`/`system.cmp` data tables | pending |
| M4 assets (tpl/thp/ga/…) | pending |
| M5 format docs | pending |
| M6 runtime verification | human-run |

Details & decision log: [`docs/fe9-re-plan.md`](docs/fe9-re-plan.md) (§8 locked decisions, §10 field findings).
Path C (100% matching decomp) roadmap: [`docs/FE9_PATH_C_PLAN.md`](docs/FE9_PATH_C_PLAN.md).

## Notable field findings (GFEE01)

> **Context correction (2026-11-11):** this disc image is an **NKit-relaxed repack** (RVZ wraps
> an nkit-relaxed ISO). The header/FST deviations below are NKit Relaxed layout conventions, not
> original-disc behavior — file *contents* are intact (cross-verified: `main.dol` md5 identical
> across the JS RVZ reader and NKit 3.2 extraction). Evidence chain:
> [`docs/FE9_DTK_SESSION_NOTES.md`](docs/FE9_DTK_SESSION_NOTES.md).

- Disc header: `0x420`=DOL off, **`0x424`=FST off**, **`0x428`=FST size** (YAGCD's "0x424=dol size" is wrong for this disc; DOL size derived from sections).
- **FST uses a non-standard scheme on this disc**: dir flag = bit `0x01000000` of word-0 (not `0x80000000`); the dir "size" field is the **absolute exclusive end-entry index** of its subtree. Validated with 0 anomalies over all 4623 entries.
- DOL: 10 sections, entry `0x80003154`, max addr inside MEM2 (0x8036c400) — MEM2 sections exist.
- No `.rel` files in the FST: single executable (no streaming module relocation).

## Tooling

```
node bin/rvz.mjs ls /                       # list disc
node bin/rvz.mjs extract <isoPath> <out>    # extract one file (md5 printed)
node bin/rvz.mjs fstinfo | dolinfo          # structures
node bin/dol2elf.mjs <in.dol> <out.elf>     # DOL -> PPC32 BE ELF (for analyzers)
```
Requires Node ≥ 23.8 (`zlib.zstdDecompressSync`) or Bun.

`FE9_RVZ` env overrides the disc path (default: the user's RVZ path; edit `bin/rvz.mjs` CLI block).

Ghidra path: `bin/ghidra/DumpFns.java` postScript dumps `functions.tsv` + `strings.tsv`
from a headless run of the converted ELF (`-processor PowerPC:BE:32:default`).
Note: the `rea`/REA tooling's Windows Ghidra lane accepts only x86/ARM targets, so PPC analysis runs `analyzeHeadless.bat` directly (Ghidra 12.2 + JDK 25).

Matching-decomp lane (`decomp/`): `python configure.py --dtk tools/dtk.exe && ninja`
— dtk split/analyze/link; objdiff per function. Setup & status: `decomp/README.md`,
[`docs/FE9_DTK_SESSION_NOTES.md`](docs/FE9_DTK_SESSION_NOTES.md).

## Research notes (2026-11-11)

- [`docs/FE9_AI_NOTES.md`](docs/FE9_AI_NOTES.md) — NPC/AI two-layer model (per-unit AI =
  4 words in the `dispos` record at `+4C..+58`; behaviour engine = matching-decomp lane),
  proc-instruction dicts, forge/hash/path-registry structs; all facts mined from
  `StanHash/FE9-DOC` + feuniverse 8243 and cross-checked against our DOL symbol table.
- [`docs/FE9_STANHASH_MINING.csv`](docs/FE9_STANHASH_MINING.csv) — address ledger with
  dtk cross-check status; names 3 functions + 5 data symbols in `symbols.txt`
  (`hash37`, `DisposFileDecode`, `UnitLoadUnitInfo`, `gFileEntryArray`, `gForgeData`,
  `gProcInstrDict0/1/2`); relink stays byte-identical.
- Split targets #1-3: `0x80011D2C` (VM interpreter, hidden inside `fn_80011D20`),
  `0x8006D437` (dispatcher table, data misread as code in `fn_8006D378`),
  `0x8011F351` (function-name registry inside `fn_8011F184`).

## Licensing

- Code in `bin/`, plan in `docs/`: MIT (project default; adjust if needed).
- `docs/DiscUtils.cpp`, `docs/DirectoryBlob.cpp`, `docs/WiaAndRvz.md`: excerpts from the
  **Dolphin Emulator** project (GPLv3), kept verbatim as format reference under GPL.
- `inventory/files.csv`: filenames/sizes extracted from the game's FST for research; contains no disc content.
- **No copyrighted disc content** is committed (DOL, ELF, banner, samples are git-ignored).

## Legal

Research/preservation of a disc you own. Do not redistribute ROM content.
