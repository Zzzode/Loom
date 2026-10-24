# RFC 0031 E6b — Mermaid layout cache measurement record

Status: measurement complete, **preliminary verdict GO** (see Decision). No
cache code was written; E6b implementation remains gated on the RFC workflow
and an independent agent review of this record.

Date: 2026-10-24. Host: macOS Darwin 25.5 (arm64), Homebrew Clang 23.1.2
(`/opt/homebrew/opt/llvm`), Debug preset build, `steady_clock` wall time,
`loom_test_main` terminal scrubbing. The instrumentation, the temporary
gtest target and the scratch build directory were all removed before this
record was committed; `git status` is clean.

## Why this measurement exists

`render_mermaid` has exactly one production call site
(`src/ui/visual/markdown_render_code_impl.cpp:103`, once per closed mermaid
fence per `render_markdown()` call) and no cache. Two paths were suspected
of repeating identical layouts:

- **Streaming** — one app-owned `StreamingMarkdown`; `update()`
  (`src/ui/visual/markdown_api_impl.cpp`) re-renders the whole document on
  every growing delta; the 256-entry process-global token LRU is keyed on
  the full document string, so a growing stream misses it every frame.
- **Virtual scroll** — committed rows call plain `render_markdown`; above 80
  visible entries the list switches to a windowed virtual renderer and rows
  entering the viewport are rebuilt from scratch.

## Method

Temporary, compile-gated instrumentation (`-DLOOM_MERMAID_CACHE_PROBE`,
scratch build only; the block compiles to nothing and stays `-Werror` clean
under the normal debug/release presets):

- Around **both** `render_mermaid` entry points in
  `src/ui/visual/mermaid_impl.cpp`: FNV-1a 64-bit hash over the exact source
  bytes with the `use_ascii` bit folded in as a trailing 0/1 byte; per-key
  call count, drawn/rejected classification (`optional<Element>` present
  after the full body), accumulated ns, plus a per-call ns sample vector;
  `atexit` stderr dump of calls, distinct keys, repeats = calls − distinct,
  R = repeats/calls, mean/p50/p90/p99/max per-call ns and a per-key table.
- Around `StreamingMarkdown::update` in
  `src/ui/visual/markdown_api_impl.cpp`: calls + total ns, so
  mermaid_ns/update_ns is derivable.

A temporary gtest executable (`test_mermaid_cache_probe`, linked exactly
like `test_streaming_replay`) drove three workloads; each ran in its own
process (`--gtest_filter`) so the at-exit tables are per-workload.

Scratch configure command:

```
cmake -S . -B build/probe -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/AutoToolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=OFF \
  -DCMAKE_CXX_SCAN_FOR_MODULES=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=$PWD/cmake/LinkCompileCommands.cmake \
  -DLOOM_WARNINGS_AS_ERRORS=ON -DLOOM_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS=-DLOOM_MERMAID_CACHE_PROBE
```

"Draws" was verified, not assumed: each synthetic input is rendered through
`render_markdown` as a mermaid fence and as a `text` fence, captured
headlessly at 240 columns (`ftxui::Screen::Create(Fixed, Fixed)`, idiom from
`tests/ui/test_mermaid_terminal.cpp`), and counts as drawn only when the two
captures differ.

## Workloads

1. **STREAMING.** One `StreamingMarkdown`. Seed document with two closed
   mermaid fences, both verified to draw at 240 columns:
   - a labeled `flowchart TD` chain of **14 nodes / 13 edges** (`N0[Step
     0] --> N1[Step 1] --> ...`);
   - a `sequenceDiagram` with 4 participants and 16 messages.
   Then `update(seed)` (seeding frame) followed by 300 `update()` calls,
   each appending one more character of a 300-char prose suffix
   (prefix-growing frames).
2. **SCROLL.** 120 distinct fenced documents, each a verified-drawing
   labeled 4-node LR chain (`S[Start i] --> P[Process i] --> Q[Queue i] -->
   E[End i]`). Ten passes of a 12-row sliding window stepping one row at a
   time across 109 positions; `render_markdown` is called for every visible
   row on every paint — 10 × 109 × 12 = **13,080 window renders**.
3. **PINNED (excluded from the GO threshold).** All 1608 owned-family
   `.mmd` files under
   `tests/fixtures/mermaid_official/cases/{flowchart,sequence,class-diagram,er-diagram,state-diagram,state-diagram-v2,requirement,pie,xychart,git}`,
   each rendered 3× (captured at widths 240/40/240) with a text-fence
   control, replicating `test_mermaid_terminal.cpp`. The 240/40/240 captures
   were identical for every file (width independence confirmed). Key
   cardinality, per-family cost and footprint only.

### Deviation from the spec's "~40-node LR flowchart"

A connected ~40-node flowchart **cannot draw** in the current renderer, so
the STREAMING seed uses the heaviest deterministic diagram that fully
draws. Measured constraints:

- `finish_mermaid` rejects diagrams wider than 240 columns / taller than
  1024 rows (`kWidthLimit`/`kHeightLimit`).
- `Router::connection` spends `canvas.cells*2` Work per edge against the
  1,000,000 `kWorkLimit` (`mermaid_impl.cpp`), and the routing canvas
  inflates by roughly 2 columns and 3 rows per edge. Connected graphs fall
  off a Work-budget cliff at about 15–19 straight edges: a bare 14-node LR
  chain draws (230 cols), a 16-node LR chain and 16-edge TD chain reject;
  a labeled 14-node TD chain (the seed) draws, a labeled 16-node TD chain
  rejects; 3+ disconnected parallel chains reject even when short; 40-node
  trees/fans reject.
- Only **63 of 400** pinned flowchart fixtures draw at 240 columns.

The seed is therefore a near-cliff-edge flow (per-call layout 3.04 ms, see
below) — representative of a moderately heavy accepted diagram, not a
worst case (drawn-corpus p99 is 18.75 ms).

## Raw counters

### STREAMING (three runs)

| run | update calls | update total ns | mermaid calls | distinct keys | repeats | R | drawn/rejected | mermaid total ns | mermaid share of update ns |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 301 | 1,271,687,260 | 604 | 2 | 602 | 0.996689 | 604 / 0 | 1,147,278,420 | 90.2% |
| 2 | 301 | 1,241,299,339 | 604 | 2 | 602 | 0.996689 | 604 / 0 | 1,120,522,348 | 90.3% |
| 3 | 301 | 1,253,271,529 | 604 | 2 | 602 | 0.996689 | 604 / 0 | 1,130,528,491 | 90.2% |

The 604 mermaid calls are 602 in-frame renders (301 updates × 2 fences)
plus 2 one-time draw-verification renders; removing the latter changes the
share by ~0.3 percentage points (run 1: 89.9%). Average update cost
4.12–4.22 ms, of which ~3.8 ms is mermaid layout — about 11–12% of a 33 ms
(30 fps) frame on this machine, in a document with two diagrams.

Per-key, run 1: TD flow — 302 calls, 917,649,037 ns total,
**3,038.6 µs/call**; sequence — 302 calls, 229,629,383 ns total,
**760.4 µs/call**.

Per-call layout distribution (µs):

| run | mean | p50 | p90 | p99 | max |
|---|---|---|---|---|---|
| 1 | 1899.5 | 985.7 | 3144.0 | 3210.8 | 3436.4 |
| 2 | 1855.2 | 1107.8 | 3007.1 | 3127.4 | 3431.9 |
| 3 | 1871.7 | 1041.9 | 3027.1 | 3412.5 | 3741.8 |

### SCROLL (representative runs)

mermaid probe (13,080 window renders + 120 verification renders):

| metric | value |
|---|---|
| calls / distinct / repeats | 13,200 / 120 / 13,080 |
| **R** | **0.990909** |
| drawn / rejected | 13,200 / 0 |
| mermaid total ns | 4.59–4.66 ms×10⁶ across runs (timed runs 4,607,234,072 / 4,661,431,619) |

Row timing from the harness (steady_clock around the 13,080 window
`render_markdown` calls):

| run | row render total ns | mermaid-in-window ns (est.) | **mermaid share of row render** | row render avg |
|---|---|---|---|---|
| 1 | 5,120,878,777 | ~4,565 ms | **~89.1%** | 391.5 µs |
| 2 | 5,187,879,454 | ~4,619 ms | **~89.0%** | 396.6 µs |

(The ~42 ms subtraction is the 120 out-of-window verification renders.)
Per-call mermaid layout: mean 348–353 µs, p50 345–351 µs, p90 368–376 µs,
p99 394–418 µs, max 0.59–1.14 ms.

### PINNED (two runs; not threshold evidence)

| metric | value |
|---|---|
| files / mermaid calls | 1608 / 4824 |
| distinct keys / repeats / **R** | 1604 / 3220 / **0.667496** |
| files drawn vs source-fallback | **602 drawn, 1006 rejected** (call counts 1806 / 3018) |
| total mermaid ns | 3,524,934,611 / 3,509,387,211 |
| per-call p50 / p90 / p99 / max | 20.5–20.9 µs / 1.12–1.13 ms / 16.8–17.0 ms / 22.1–22.6 ms |

Per-key layout cost split (1604 distinct keys; 4 corpus files are
byte-duplicates, hence 1608 files → 1604 keys):

| class | keys | mean | p50 | p90 | p99 | max |
|---|---|---|---|---|---|---|
| drawn | 599 | 1744 µs | 457 µs | 3144 µs | 18,751 µs | 21,819 µs |
| rejected | 1005 | 108 µs | 13 µs | 34 µs | 2949 µs | 3732 µs |

Per-family draw results at 240 columns (files / drawn):

| family | files | drawn | | family | files | drawn |
|---|---|---|---|---|---|---|
| flowchart | 400 | 63 | | state-diagram | 42 | 0 |
| sequence | 193 | 193 | | state-diagram-v2 | 103 | 1 |
| class-diagram | 285 | 215 | | requirement | 172 | 12 |
| er-diagram | 206 | 68 | | pie | 24 | 7 |
| xychart | 48 | 2 | | git | 135 | 41 |

Hypothetical full-corpus positive-cache footprint (trimmed captured row
bytes, so a slight underestimate of stored `vector<string>` bytes): 599
distinct drawn sources = **2,001,810 bytes (~1.9 MiB), avg ~3.34 KiB per
diagram**.

## Decision rule and verdict

Rule (from the E6b spec):

- **GO** only if STREAMING **or** SCROLL shows R ≥ 0.5 **and** mermaid
  layout is ≥ 10% of frame/update time, with a clear per-instance owner and
  a width/theme-independent key (source bytes + ascii bit).
- **DROPPED** if **both** workloads show R < 0.2 **or** mermaid layout
  is < 2% of frame time.
- The 2–10% share / 0.2–0.5 R grey zone must be escalated to an
  independent agent performance review; it must not be decided here.
- The pinned 3-width workload can never justify the cache by itself; it is
  recorded for key cardinality, per-family cost and footprint only.

**Preliminary verdict: GO.** Both threshold workloads independently clear
the GO bar with wide margin — neither is in the grey zone:

- STREAMING: R = **0.997** (threshold 0.5); mermaid = **~90%** of update
  time (threshold 10%).
- SCROLL: R = **0.991**; mermaid = **~89%** of per-row render time.
- Key feasibility: R is computed against exactly the proposed key (exact
  source bytes + ascii bit); width/theme independence is corroborated by
  1608 files passing 240/40/240 identity and zero palette/color use in the
  mermaid sources.
- PINNED (R = 0.667 by construction) was not used for this verdict.

Timing was stable across repeated runs (ns within ~2%); the ratios are far
from any threshold boundary, so measurement noise cannot flip the verdict.
This record is preliminary in the sense that no cache has been
implemented; landing one remains subject to the RFC gate and agent review.

## Correctness constraints on any GO branch

These come from the spec hazards and the code shapes verified here; a
cache violating any of them is a correctness bug:

1. **Cache immutable string rows, never `ftxui::Element`.** `MermaidNode`
   flips its `diagram_` child in `SetBox` per box width; one node instance
   displayed in two rows or at two widths shares and mutates that state.
   `finish_mermaid` must construct a fresh node per render from cached
   rows. Introduce a narrow seam
   `optional<vector<string>> layout_mermaid_rows(string_view, bool
   use_ascii)`; `render_mermaid` keeps its signature and becomes seam +
   `finish_mermaid`.
2. **Per-instance owner, never process-global.** Measurement points at
   `StreamingMarkdown` first (streaming dominates: ~90% of update time,
   stable 2-key working set per instance); a session/`MessagesList`-owned
   LRU (~32 entries) covers SCROLL. Wire via an additive nullable field in
   `MarkdownOptions`, read only at `markdown_render_code_impl.cpp:103`.
   Do **not** extend the 256-entry process-global token LRU with diagram
   output — that would be a process-global graph cache by the back door.
3. **Key = exact `tok.code_content` bytes** (byte-for-byte, including
   CRLF; no trim/normalize) **+ the `use_ascii` bit**. The 3-arg entry
   point produces different ASCII output and must partition the key space.
   No theme or width component: verified theme independence (zero palette
   use in mermaid sources; `dim` is applied by the caller) and width
   independence (width flips happen later, inside `MermaidNode`).
4. **Cache negative results too** — rejected diagrams repeat as well.
   Benefit is small (rejected p50 is 13 µs vs drawn p50 457 µs on the
   corpus), but a missing negative entry makes rejected fenced content the
   uncached worst case.
5. **Clear at the existing reset boundaries**
   (`app_agent_menu.cpp:404,477`, `app_handle_submit.cpp:225,403`) and on
   `clear_markdown_cache()` semantics for the owner's cache.
6. **Sizing.** Corpus drawn output averages ~3.3 KiB of row bytes; an
   8–16-entry `StreamingMarkdown` LRU (~30–55 KiB typical resident) behind
   a ~1 MiB byte cap matches its tiny working set; a scroll-owner LRU of
   ~32 entries plus byte cap covers the 12-row window with hysteresis.

## Cleanup and verification

- Instrumentation removed from `mermaid_impl.cpp` and
  `markdown_api_impl.cpp`; temporary `test_mermaid_cache_probe.cpp` and
  its `tests/ui/CMakeLists.txt` target removed; scratch `build/probe`
  deleted; source-root `compile_commands.json` symlink restored to
  `build/release`. `git status` clean.
- Normal presets with the probe guards present compiled `-Werror` clean
  (debug and release); after removal both presets rebuild clean and
  `ctest -R 'MermaidTerminal|MermaidOfficial'` passes 56/56 in debug.
