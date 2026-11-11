# FE9 Path C — 100% matching-decomp plan (v1.1, 2026-11-11 — post-audit; audit log §14)

Goal: reconstruct the entire FE9U (GFEE01) DOL as compilable C/asm that links
**byte-identical** to `main.dol` (sha1 `9d00d7d407750b82cb1cfccc56f8aa8a89cc8851`),
then, much later, re-target that C for other platforms (native port = separate plan, out of scope here).

Ground truth today (measured from `decomp/config/GFEE01/symbols.txt`):

| Metric | Value |
|---|---|
| Functions detected (analyzer) | 9,251 (provisional — blobs hide functions; the VM trio already proves ≥3 hidden) |
| Total code bytes | **2,499,552 fixed denominator** (DOL `.text` measured from header; analyzer sum 2,500,828 agrees within padding) |
| Functions named (community seeds) | 714 (4.4% of code bytes) |
| Functions matching from C | **0** |
| Build status | relink from original asm = sha1 MATCH |
| Compiler archives on disk | **YES** — `build/compilers/GC/` = CW GC 1.0 … 3.0a5.2 (20+ revs, auto-downloaded by scaffold) |

Comparables: TWW needed ~5 years and ~30 contributors for a same-generation GC DOL.
This plan optimizes for *correct pipeline + measurable weekly progress*, not heroics.

---

## §0 Locked decisions

- **D-C1** Compiler: CodeWarrior PPC 3.x family; exact revision picked **empirically** (M-C0), pinned in `tools/` + recorded here. Never mix versions.
- **D-C2** Structure: per-subsystem source directories (`src/cm/`, `src/dispos/`, ...); asm splits live under `asm/`; one file per logical module, multiple functions per file allowed once boundaries are trusted.
- **D-C3** Naming: keep community names; new names = PascalCase like existing seeds; `EVT_` prefix already reserved for event scripts; statics `s*`, globals `g*`.
- **D-C4** A function is "done" only at **100% objdiff on the matched compiler**, committed with its source, queue CSV updated. No "functionally equivalent" claims in the scoreboard.
- **D-C5** Every merged commit must keep DOL sha1 MATCH (CI-enforced once M-C1 lands).
- **D-C6** Two-PC discipline: `symbols.txt` single writer — pull before touching; decomp build runs on one machine; JS/data lane on the other (unchanged rule).
- **D-C7** Function priority order is *this* doc's queue (M-C3..M-C7), not "whatever looks fun".
- **D-C8** Asm islands are legal escape hatches (`#pragma push/...` or `.s` kept), but each costs a tracked `-1` in the ledger; goal 0 at "done".

---

## §1 M-C0 — Toolchain lock (blocking; ~1 day — **downloading already done**)

**Audit correction (2026-11-11):** `build/compilers/GC/` already contains CodeWarrior GC
1.0, 1.1, 1.1p1, 1.2.5(n), 1.3(.2/.2r), 2.0(p1), 2.5-2.7, 3.0a3(.2-.4/p1), 3.0a5(.2)
(+ linker `mwldeppc`). Active default in `build.ninja` is `GC\1.3.2` — an **arbitrary
scaffold choice**; the sha-MATCH relink only proves linker tolerance for blob relink,
NOT a matching toolchain. FE9U (2006/07) most likely sits on 3.0a3.x–3.0a5.x.

Steps (probe only, no downloads):
1. Harness `tools/probe_mwcc.py`: compile probe corpus with every 3.x revision (then
   2.x if needed), normalize (strip addresses), score = first mismatching instruction.
2. Probe corpus (all on-disk, zero new RE work):
   - P1 init prologue pattern (`auto_*_init.s`)
   - P2 `hash37` @ `0x8000D598` (0x30 B pure loop, documented algorithm) — **the probe**
   - P3 one float-using function from the named event set
3. Winner recorded: binary path, sha256, flags (`-proc G2 -fp hard -O2,p -str reuse -pool off` starting point), set as `mw_version` in `configure.py`.

Gate: P2 compiles byte-identical. If NO on-disk revision reproduces P2, suspect the
probe corpus / flags before blaming the compilers — **do not proceed past M-C0 without this gate.**

## §2 M-C1 — Split pipeline (mostly pre-wired; ~1 day)

**Audit correction:** `configure.py` (zeldaret/doldecomp-lineage scaffold) already contains
rules `split` (= `dtk dol split`), `mwcc[_sjis][_extab]`, `as`, `link`, `elf2dol`, `check`,
`report`, and **writes `objdiff.json`** (line ~334). `dtk 1.8.4` has NO `analyze` command —
the historical note "dtk analyze updates symbols.txt" is wrong for this version; symbol
provenance must be recorded per batch instead (see §14).

1. Verify `ninja split` → `build/GFEE01` relocatable objects; `dtk dol split` regenerates
   asm at build time, so **asm stays uncommitted** (correct for dtk workflow, unchanged policy).
2. Wire the missing piece: `objdiff` CLI binary (encounter/objdiff releases) into `tools/`,
   one-command `ninja report` → matched-bytes into `docs/SCOREBOARD.md`.
3. ldscript covers all 10 sections incl. MEM2 (ceiling `0x8036C400`).

Gate: `configure.py + ninja` from clean tree = sha1 MATCH; `objdiff.json` loads in objdiff
CLI and reports `0 / 2,499,552 B` code.

## §3 M-C2 — First match: `hash37` (day ~5)

Loop (this exact loop forever after):
```
m2c draft (pip install m2c)   -> src/cm/hash37.c
mwcc compile (flags from §1)  -> build/.../hash37.o
objdiff hash37                -> iterate: -O levels, -str, -pool, register pressure hints, explicit u8/char
done when: 100% + DOL sha1 MATCH + queue CSV flips to `matched`
```
Deliverables: `hash37` matched; `docs/FE9_MATCH_QUEUES.md` documents the loop; `tools/match.ps1`/`.mjs` one-command wrapper (compile+diff+report).

## §3.5 M-C2.5 — SDK/runtime free wins (audit addition; same week)

`configure.py` already declares `Runtime.PPCEABI.H/global_destructor_chain.c` and
`__init_cpp_exceptions.cpp` as **NonMatching stubs** — these are CodeWarrior runtime
files whose matching sources exist verbatim across the GC/Wii decomp ecosystem
(zeldaret/doldecomp projects). First real matches will come from here, not from game code:
flip `NonMatching`→`Matching` with community sources, validate, then move to `hash37`.
Also: SDK names already in `symbols.txt` (`__check_pad3`, `__init_registers`, …) tell us
the analyzer detected REVO init sequences — matching those is near-free.

Gate: ≥1 SDK file Matching + DOL sha1 MATCH.

## §4 M-C3 — Utility ladder (weeks ~2-6; ~150 KB target)

Auto-generated queue first: `tools/mkqueue.py` walks `symbols.txt`:
candidates = size < 0x80 B, no external `lbl_/data_` refs in asm, not a jumptable stub
→ `docs/FE9_MATCH_QUEUE.csv` with `bytes, refs, priority, state` columns, re-sortable.

Expected early wins (already named, so semantics known):
1. hash37 (§3)
2. std::string helpers (`{size,capacity,ptr}` layout documented)
3. Unit accessors: `UnitGetByPID` + the 700-odd named event-adjacent leaves
4. `hash31` clones (same algorithm, constant 31 — verify at each site, some are inlined)

Gate: ≥3% code bytes matched + CI green (M-C9) + at least 1 non-self contributor doing §3 loops.

## §5 M-C4 — Event VM trio (weeks ~6-12)

Split targets already mapped (`FE9_STANHASH_MINING.csv`):
- `0x80011D2C` inside `fn_80011D20` (+0xC) → split, name `CmVM_Interpret`
- `0x8006D437` inside `fn_8006D378` (+0xBF) → dispatcher **data** table, not code
- `0x8011F351` inside `fn_8011F184` (+0x1CD) → function-name registry data

Procedure: add sub-symbols with sizes to `symbols.txt` → dtk re-analyze/split → asm boundaries
must now reproduce: (a) VM opcode table order matching the community opcode list,
(b) registry string cross-check vs `Cxx.cmb` name pool. Then match functions
(VM interpreter first; dispatcher data needs no match beyond correct section/bytes).

Gate: `bin/cmb.mjs` (JS lane) opcode names cross-validate against decompiled VM switch — two independent implementations agreeing.

## §6 M-C5 — Dispos/Unit layer (weeks ~12-16)

`DisposFileDecode` (`0x80107B54`, 0x210 B) + `UnitLoadUnitInfo` (`0x8003EAE8`, 0x6C4 B).
High value: they *define* the `UnitInfo`/`dispos` layout used by the AI-data lane;
a byte-match turns StanH's "ai?" words into authoritative field semantics.

Gate: decompiled decoder confirms 4 AI words @ +4C..+58 and UnitInfo+38..44 mapping.

## §7 M-C6 — Proc engine (weeks ~16-24)

Anchors: `gProcInstrDict0/1/2`, proc struct 0x70, instruction header word layout.
Work: proc lifecycle functions (start/loop/end/tree ops) + dict0 entries one by one.
Cross-check: proc scripts found in game data disassemble coherently with the instruction semantics.

Gate: any 3 dict0 instructions matched end-to-end.

## §8 M-C7 — Battle/AI core (months 6+; the long grind)

`battle.dbx` + `param.dbx` (394/116 rows SJIS) give damage/AI constants for free.
Order: BattleAction (4 B) → damage calc chain → AI eval entry points reached from
`EVT_Enemy*Check*` xrefs → movement/threat scoring. Each match feeds the port spec docs.

Gate (stated honestly): no time-box; percentage milestones only (10/20/50% of code bytes).

## §9 M-C9 — CI & community infra (do alongside M-C3)

- GitHub Actions: ubuntu-latest, cache `tools/dtk` + `build/compilers` (same public mirror the scaffold already downloads from — `download_tool.py` — no private bucket needed; LICENSE notes tool ≠ project code), `pip install m2c ninja`, run configure+ninja, objdiff report job posts matched-bytes.
- `docs/FE9_MATCH_QUEUE.csv` is the contribution board; good-first rows = queue head.
- README section "Contributing to the decomp" = the §3 loop, 10 lines.

## §10 Accounting & cadence

`docs/SCOREBOARD.md` per merge: `Code: <matched>/<2499552> B (<x>%) | +N funcs | asm-islands: M`.
Weekly roll-up comment on repo (agent or human) — one paragraph, no ceremony.

## §11 Risks

| Risk | Mitigation |
|---|---|
| No MWCC rev reproduces `hash37` | probe P2 *is* the gate; widen rev set (Nintendo internal builds circulate for GC); never hand-forge the "close enough" |
| r13/sdata coupling breaks split | keep sdata section layout exactly as dtk produces; first matches must include one r13-referencing leaf |
| FP-heavy funcs diverge | `-fp hard/cons`, explicit `fmaf` avoidance, MWCC builtins (`__frsp` etc.) — catalog in MATCH_QUEUES doc |
| Two-PC clobber of `symbols.txt` | D-C6 + CI job that re-splits from committed config and checks sha; conflicts are merge conflicts, not silent |
| Team morale (0→1 cliff) | §4 auto-queue keeps a fresh top-10 list of <0x80B wins at all times |
| Compiler redistribution legality | CI pulls from a bucket we control; LICENSE notes tool ≠ project code |

## §12 Explicit non-goals (v1)

- Behavior changes, bugfixes, optimization, asset edits — this lane only *reproduces*.
- Native/Android re-targeting — separate plan (`FE9_PATH_D_PLAN.md`, only open it after ≥80% matched AND M-C6 done).
- `asm/` committed to git — deferred (D-C9) until outside contributors need stable asm baselines.

## §13 Immediate next actions (this week, post-audit)

1. `tools/probe_mwcc.py`: sweep on-disk revs (3.0a3p1, 3.0a3.4, 3.0a5, 3.0a5.2 first; then 2.7/3.0a3.x; 2.x last) against P2 `hash37`; flip `mw_version`. — me
2. Download objdiff CLI; validate `objdiff.json` round-trip. — me
3. `ninja split` verify from clean tree; confirm sha MATCH + `0/2,499,552` report. — me
4. M-C2.5: pull SDK runtime sources (global_destructor_chain, __init_cpp_exceptions) from a sibling GC project and flip Matching. — me
5. `tools/mkqueue.py` + first `FE9_MATCH_QUEUE.csv`. — me
6. Message to home agent: pull; JS lane keeps P1 gates; do NOT enable split mode there; CI infra (M-C9) assigned to whichever machine gets GH Actions secrets first.

## §14 Plan audit log (v1.1, 2026-11-11 — skeptical review)

Verified against reality before trusting v1:

| v1 claim | Verdict | Evidence |
|---|---|---|
| "Download compiler archives (zeldaret/compilers GC/)" | **WRONG twice** | repo path 404; and `build/compilers/GC/` already holds 20+ CW revs (scaffold's `download_tool.py` fetched them) → M-C0 is probe-only now |
| m2c PPC support | TRUE | README: "32-bit MIPS, ARM, PowerPC, SuperH", `-t ppc`, `MWCC_CC` env; built for byte-match projects |
| "dtk emits per-function objdiff JSON" | half-true | objdiff.json is emitted by **configure.py** (already wired); `objdiff` CLI binary still missing |
| "Current build = relink, need split pipeline" | TRUE but understated | `build.ninja` has `split/mwcc/as/link/elf2dol/check/report` rules pre-wired; but **zero** compile rules fired for asm and 0 `.c` in `src/` today; `dtk 1.8.4` has no `analyze` (help audited) — historical "dtk analyze" notes are wrong for this version |
| Active `mw_version = GC\1.3.2` | ARBITRARY | scaffold default; sha-MATCH proves only linker tolerance for blob relink. FE9U likely 3.0a3–3.0a5.x |
| Denominator 2,500,828 B | ≈OK, fixed to 2,499,552 | DOL header `.text` size at offset 0x88 area (YAGCD layout; GBA-boot field at 0x2C tricks naive parsers) |
| asm/ not committed | TRUE for dtk | `dtk dol split` regenerates at build time |
| SDK stubs already scaffolded as NonMatching | MISSED in v1 | `configure.py:294-295` → added M-C2.5 (first wins likely from SDK, not game code) |
| symbols.txt provenance | GAP | SDK-style names (`__check_pad3`…) + analyzer names coexist; no batch provenance recorded → new rule: name batches must ship a source row in the ledger CSV |

Net effect: M-C0 shrinks from days to hours; first match realistic inside week 1;
largest remaining unknown = exact MWCC revision (probed next) and probe corpus quality.
