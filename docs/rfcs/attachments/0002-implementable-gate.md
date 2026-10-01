# RFC 0002 — Implementable gate package (PRR + thresholds + sharpened scope)

Status of this document: the `implementable`-gate artifact for RFC 0002. It
fills the PRR, fixes numeric graduation thresholds, and sharpens the per-phase
scope against the **live** tree. It supersedes the numbers in
[0002-ui9-edge-inventory.md](0002-ui9-edge-inventory.md) where they differ:
that attachment was measured 2026-09-26, before the RFC 0001 Phase-B graveyard
cleanup waves (2026-09-28) deleted ~70 `cc.ui.*` modules and dissolved part of
the UI9 SCC. Every number below was re-measured on 2026-09-29 from the live
tree with `tools/arch/graph_check.py`'s own loader.

**Verification gate (directive 2026-09-29):** the LOCAL `local-linux` /
`local-linux-release` presets — dual-preset `-Werror` build + serial
`ctest -j1`. GitHub CI (macos-14) is NOT a gate and MUST NOT be required or
waited on. Where the RFC body says "macos-14 gate", read "local dual-preset
gate".

---

## 0. Live graph baseline (re-measured 2026-09-29)

Source: `python3 tools/arch/graph_check.py --json` (635 modules, 761 units,
0 module cycles) plus a Tarjan run over the `cc.ui.<area>` area graph using
`graph_check.load_units()` / `module_deps()` / `tarjan_scc()`.

| Metric | 2026-09-26 (attachment) | 2026-09-29 (live, this gate) |
|---|---|---|
| Total modules | 849+ | **635** |
| `cc.ui.*` modules | 218 (219 decls) | **148** |
| `cc.ui` areas | 12 | 12 (unchanged set) |
| Area SCCs > 1 | 1 SCC of **9** areas | **2 SCCs: a 7-area SCC + a 2-area SCC** |
| Internal area-directions | 34 (32 cppm + 2 cpp) | **19** (17 in the 7-area SCC + 2 chrome↔foundation) |
| Minimum FAS | 7 dirs / 10 module edges | **5 dirs / 7 module edges** (4+6 for the 7-area SCC, +1 for foundation→chrome) |
| ctest total | 1706 | **1836** (commit 1a83330, 2026-09-29) |
| `ReplScreenState` | 739 LOC | **691 LOC** (`src/ui/screens/repl_state.cppm`) |
| AppAdapter inline bodies | 58 | **58** (`cc.ui.app.app` frozen in `tools/arch/inline_def_baseline.txt`) |
| TLL graph | acyclic | **acyclic** (0 TLL SCCs; `cc_ui` links 12 libs) |

The 7-area SCC is `{dialogs, features, messages, permissions, prompt,
screens, widgets}`; the 2-area SCC is `{chrome, foundation}`. The three
singleton areas are `app`, `tools`, `visual`.

**The RFC-declared total order is still the minimum-FAS order on the live
graph.** An exhaustive 7! search over the 7-area SCC confirms the order
`screens > dialogs > features > messages > permissions > widgets > prompt`
yields the global minimum of **4 back area-directions / 6 module edges**
(no other order does better). Under that order, plus `chrome > foundation`
for the 2-area SCC, the complete back-edge set to sever is:

| # | Back direction | Module edge(s) | RFC row | Mechanism |
|---|---|---|---|---|
| 1 | `foundation → chrome` | `logo → chrome.layout` | 1 | delete dead import |
| 2 | `widgets → dialogs` | `all_components → feature_dialogs` | 3 | delete umbrella `export import` |
| 3 | `dialogs → screens` | `default_renderers → doctor_screen` | 4 | registry inversion |
| 4 | `features → dialogs` | `agent_wizard → wizard_dialog`; `plugin_install_flow → trust_dialog`; `plugin_install_flow → wizard_dialog` | 6 | registration-driven |
| 5 | `prompt → messages` | `prompt_input_footer → message_tool_result` | 8 | extract render converter |

Rows 2, 7, 9 of the RFC's original 10-row table are **already gone or
downward-legal on the live tree**: row 2's file (`foundation/design_extras.cppm`)
and row 7's site (`messages/messages_interactions.cppm`) were deleted in the
graveyard cleanup; row 9 (`widgets → prompt`) is a legal downward edge under
the declared order (`widgets` ranks above `prompt`). Rows 5 and 10 are
decoupling/ranking, not cuts. The remaining **13** of the 17 internal
directions in the 7-area SCC, and `chrome → foundation`, are legal downward
edges once the order is declared.

Per-area module counts (for the F4 split map): app 2, chrome 8, dialogs 21,
features 12, foundation 10, messages 33, permissions 10, prompt 13, screens 5,
tools 17, visual 4, widgets 13.

---

## (a) Production Readiness Review — RFC 0002

### 1. Correctness and tests

- [x] **Every new behaviour has a new test (unit / module / e2e named).**
  This RFC is behaviour-preserving; the only *new* behaviour is the F0 lint.
  New tests: (i) F0 — a negative test that adds a synthetic 20th back edge to
  a temp tree and asserts `graph_check.py --target-ui9` exits 1 (no
  `--target-core8` test exists in `tests/`; this negative test is genuinely
  new); (ii) F2 — a
  composition-root test that the doctor renderer and the wizard/trust flows
  render through `DialogRendererRegistry` / the protocol leaf after the
  concrete imports are deleted (the dialog renders identically, asserted by
  the existing `test_dialog_*` / `test_cost_threshold_dialog` goldens);
  (iii) F3 — per-store selector tests (cross-store read through a selector,
  never a field reach-up). The 1836 existing tests + 56 golden suites are the
  behaviour net for the refactors.
- [x] **Failure paths covered (empty input, error result, timeout, abort).**
  N/A for runtime behaviour — no new runtime code paths are introduced (the
  refactors move/extract existing code). The lint's failure path (exit 1 on a
  new back edge or an unranked area) is the negative test above. Reason:
  behaviour-preserving refactor.
- [x] **Golden suites assessed: `test_ui_*`, `test_dialog_*`,
  `test_prompt_dialog`, `test_cost_threshold_dialog`; if rendering changes,
  goldens regenerated and manually reviewed.** Rendering does NOT change. All
  56 truecolor / E2E golden suites must be **byte-identical every commit**; no
  `UPDATE_GOLDENS` is run in this RFC. The F1 row-8 extraction moves
  `sgr_color_value_to_ftxui` + `apply_sgr_run` + `ansi_to_ftxui_elements`
  verbatim (the function bodies are byte-identical in the new leaf), so the
  prompt-footer golden path is unchanged.
- [x] **Known timing flake list respected; no new wall-clock assertions with
  tight upper bounds.** No new wall-clock assertions. Serial `ctest -j1` is
  the signal. The known flakes (`McpStdio.WatchdogKillsSilentChild`,
  `tools_smoke`, the `impl_bash` watchdog race — all pre-existing, unrelated
  to UI module structure) are respected; a single non-reproducing timing
  failure is re-run serially, not patched.
- [x] **Expected serial ctest total stated; deletions reconcile exactly.**
  **1836 / 1836 @ 2026-09-29** (commit 1a83330, verified by `ctest -N` on the
  configured local-linux build). Provenance: RFC 0001 §12 recorded 1828/1828
  from a macos-14 run at 03c7f92; 048b311 then added exactly 6 `TEST()` macros
  (1828 + 6 = 1834). The live local-linux count is 1836 — a 2-test delta
  between the macos-14 discovery and the local-linux discovery (unreconciled;
  the macos number is historical evidence only per the 2026-09-29 directive).
  This RFC deletes dead imports and umbrella re-exports, not tests; the total
  stays 1836 unless a test is added or deleted. Any delta — whether from
  baseline drift on the phase's base commit or from an RFC-introduced change —
  is reconciled exactly in the phase's Implementation History row, measured by
  `ctest -N` on local-linux (never macos numbers; the 1706→1699 voice-removal
  and 1699→1828 follow-up precedents).
- [x] **String/shape-based cross-module couplings changed on BOTH emitter and
  consumer sides; `docs/decisions/design-decisions.md` consulted.** The
  `<task_notification>` / `<status>` / `<summary>` tag shapes are **untouched**
  (no message-shape change in any phase). The one new registry contract is
  the F2 protocol-leaf `ViewKind` descriptor keys: the emitter (feature-side
  registration) and the consumer (dialogs-side dispatch) land in the SAME
  commit, and `design-decisions.md` gets a row cataloguing the key set (per
  the CLAUDE.md hazard note that string/shape couplings break silently).

**Notes:** The behaviour net is the 1836-test serial run + 56 byte-identical
golden suites, run on BOTH local presets every commit.

### 2. Build system and module discipline

- [x] **Affected producer-TU BMI PSS measured before AND after (PSS from
  `/proc/<pid>/smaps_rollup`, not RSS), numbers recorded.** Instrument:
  `tools/arch/measure_bmi.py` (RFC 0001 Phase E, commit 35bbf48). F1 measures
  `message_tool_result.cppm` + the new `ansi_render` leaf; F3 measures
  `repl_state.cppm` + each new store; F4 measures the heaviest producer of
  each area library. Numbers are recorded in the phase's Implementation
  History row (the §12 precedent: query_engine 50.0→3.2 MB, repl_screen
  −42.3%, text_input −26.3%).
- [x] **No Ninja concurrency reduction anywhere; memory handled by TU
  splitting / type erasure.** Standing rule. No `-j` cap is introduced; the
  F1/F3 extractions split TUs (the Phase-C recipe) rather than throttling.
- [x] **Interface files gain declarations, not definitions; god-interface
  inline-body count does not increase.** `tools/arch/inline_def_check.py`
  ratchet stays green. `cc.ui.app.app` is frozen at **58** semantic bodies;
  F3/F4 must only decrease it (composition-root resolution). No interface
  exceeds 100 inline bodies (the Phase-C2 exit invariant: 0 over 100 today).
- [x] **After RFC 0001 Phase A: no new textual standard-library or
  third-party includes in module units.** All new module units use
  `import std;`. FTXUI stays textual in the GMF (Phase-A precedent); the F1
  row-8 leaf needs only `SgrAttr` + ftxui, so it follows the
  `import std` + FTXUI-GMF pattern of the Phase-C UI batches.
- [x] **Named partitions used only for PIMPL internals, not for cross-area
  layering.** No new named partitions are introduced for layering; the F3
  stores are separate modules, not partitions.
- [x] **No new upward dependency edges; directory-level SCCs do not grow
  (architecture graph lint result attached).** `--target-ui9` + the frozen
  `ui_back_edge_baseline.txt` (F0) enforce this: the 19 live internal
  directions are frozen, additions fail, and the SCC count only shrinks
  (2 SCCs → 1 → 0). The lint result is attached per phase.

**Notes:** The F4 split does not change per-TU PSS (same compiles) but bounds
the recompile closure to one area — that is the fan-out win, measured by
ninja dry-run, not by PSS.

### 3. Rollback and compatibility

- [x] **Commits are atomic per phase; each phase revertible independently.**
  F0 is lint-only (no `src/` behaviour change). F1/F2 land one cut per commit.
  F3 lands one store per commit. F4 is one CMake-structure commit. Each is
  independently revertible (`git revert`).
- [x] **No flag-day interface changes: PIMPL / type erasure / re-export shims
  keep importers building during migration.** F3 uses re-export shims (one per
  store, deleted by end of F3). F1/F2 keep a temporary re-export /
  registration shim so a cut can be reverted without a flag-day. The F2
  protocol leaf uses `std::function` / `shared_ptr<void>` slots (the
  `repl_state.cppm` opaque-handle pattern), so consumers do not flag-day.
- [x] **Persisted data compatibility considered (`~/.loom/sessions`, settings
  cascade, config schema) with migration or read-tolerance.** N/A — no
  persisted-data shape changes. `messages.jsonl` block coverage and
  `dump-prompts/<id>.jsonl` are byte-identical (no engine/wire change).
  Reason: pure UI-module refactoring.
- [x] **Wire-protocol compatibility (`wire_anthropic` / `wire_openai`) —
  field additions are additive; removals justified.** N/A — no wire change.
  Reason: no `src/query/wire_*` or `src/tools/` schema is touched.

**Notes:** Rollback detail per phase is in section (d).

### 4. Observability

- [x] **Session traces remain valid: `messages.jsonl` block coverage and
  `dump-prompts/<id>.jsonl` request/response dumps.** Untouched — no engine,
  tool, or wire code changes. The trace-dependent golden suites
  (`E2E_Gate.FullConversationGoldenSnapshot` et al.) pass byte-identical.
- [x] **New background workers / threads are event-driven; no constant-rate
  render ticker; teardown/abort paths defined.** **No new threads.** F3
  explicitly keeps all worker threads, staged queues, mutexes and CVs in the
  AppImpl composition layer; the one mutex currently in `repl_state.cppm`
  (`pending_at_mention_mutex`, line 531) moves OUT to AppImpl with its staged
  queue. Stores are UI-thread-affined plain data.
- [x] **New diagnostics log through the existing debug channels; no new ad-hoc
  print paths.** The only new diagnostic is the `--target-ui9` lint output,
  which runs in the arch-check workflow (the `--target-core8` precedent), not
  as an ad-hoc print.
- [x] **Build/performance metrics from this PRR recorded in the RFC.** Yes —
  per-phase Implementation History rows record PSS (measure_bmi.py), fan-out
  (ninja dry-run), wall time, ctest total, and lint results.

**Notes:** None.

### 5. Documentation and deletion

- [x] **`CLAUDE.md` updated if conventions, build layout, or paths change.**
  When F4 lands, the `cc_ui` single-target / SCC note in CLAUDE.md is updated
  (the "nine directories collapse into one SCC" warning is removed; the
  area-library layout is documented).
- [x] **Non-obvious decisions added to `docs/decisions/design-decisions.md`.**
  Rows for: the registry-inversion pattern (F2 protocol leaf), the type-sink /
  render-helper-extract distinction (F1), the corrected link-cycle rationale
  (ld.lld/ld64 resolve cyclic archives to a fixpoint; the F invariant is
  graph+TLL acyclicity, not linker rejection), and the F2 `ViewKind` key set.
- [x] **Dead code made obsolete by the work is deleted in the SAME phase.**
  F1 deletes the `logo → chrome.layout` dead import and the
  `all_components → feature_dialogs` umbrella re-export in the same commit as
  the cut. F3 deletes every re-export shim by end-of-phase.
- [x] **Deprecated modules/shapes have a stated removal trigger and migration
  note; no silent shape drift.** F3 shims carry a declared removal ("deleted
  by end of F3"); the F2 protocol-leaf keys are catalogued in
  `design-decisions.md` so a silent key drift is a reviewable diff.
- [x] **All comments/docs in English.** Yes (project rule).

**Notes:** None.

### 6. Platform readiness (macos-14 / Linux)

- [x] **Debug + release presets build `-Werror` clean on the Linux dev box.**
  Yes — `local-linux` + `local-linux-release`, the verification gate
  (directive 2026-09-29). Both presets every commit.
- [x] **macos-14 CI green at default parallelism (3 vCPU / 14 GB); no swap.**
  **NOT A GATE** per directive 2026-09-29: GitHub CI is too slow and MUST NOT
  be required, waited on, or monitored as a gate. The local dual-preset build
  is the verification gate. Cross-platform source correctness is maintained by
  inspection (this RFC's diff touches no Darwin-specific code).
- [x] **Termios / signals / Apple-only guards correct for both platforms.**
  N/A — this RFC touches no termios/signal/Apple-guard code (UI module
  refactoring only). Reason: no platform-specific surface in the diff.
- [x] **truecolor (`COLORTERM=truecolor TERM=xterm-256color`) golden path
  intact.** Yes — byte-identical every commit on both presets.
- [x] **Offline dependency cache unaffected; no new network fetch required.**
  Yes — no new dependencies; `.deps-cache/` is untouched.

**Notes:** The macos-14 rows are answered as "not a gate" per the directive;
the local dual-preset gate is the sole build/test verification.

### Review sign-off

| Role | Reviewer | Date | Verdict |
|---|---|---|---|
| Design | agent:design-review#1 | 2026-09-26 | approve (SOUND with required changes — all applied; recorded in RFC frontmatter) |
| Design (gate package) | agent:design-review#2, agent:design-review#3, agent:design-review#4 (three adversarial reviews of this gate package) | 2026-09-29 | request-changes → all required changes applied; independent agent:design-verify approved (2026-09-29) |
| Production readiness | agent:prr-review | 2026-09-29 | approve — 30/30 checklist rows; one blocking finding (ctest arithmetic 1828+6≠1836) fixed and re-verified (`ctest -N` on local-linux = 1836) |
| Code (per phase) | per-phase independent adversarial agent | per phase | approve (required before each phase `done`) |

---

## (b) Numeric graduation thresholds (F0–F4)

Every threshold is falsifiable and measured on the live tree. Instruments:
**PSS/BMI/wall** = `tools/arch/measure_bmi.py` (PSS from
`/proc/<pid>/smaps_rollup`, never RSS); **graph** = `tools/arch/graph_check.py
--target-ui9` (area-SCC count, back-edge count vs frozen baseline);
**fan-out** = ninja dry-run (`ninja -n`) recompile-closure count for a
one-body vs one-interface edit; **inline bodies** =
`tools/arch/inline_def_check.py`; **tests** = serial `ctest -j1` on both
local presets.

Baselines carried from RFC 0001 §12 (the measured anchors this gate derives
from): heaviest producer `ui/app/app.cppm` peak PSS **3,222 MB** (reduced
BMI); the Phase-C body-extraction wins (query_engine BMI 50.0→3.2 MB;
repl_screen −42.3%; text_input −26.3%; selectors −65.8%; json −77.2%;
swarm_backends −65.2%); the G1 invariant "a body edit recompiles exactly one
object file (fan-out = 1) for a converted interface"; ctest **1836**
(@ 1a83330);
AppAdapter frozen at **58** inline bodies; `repl_state.cppm` **691 LOC**.

### F0 — lint + freeze

| Threshold | Value | Instrument |
|---|---|---|
| `--target-ui9` exists and FAILS today | exit 1, prints the 2 SCCs (7-area + 2-area) | `graph_check.py --target-ui9` |
| Frozen baseline size | exactly **19** SCC-internal area-directions (17 + 2) | `ui_back_edge_baseline.txt` diff vs live Tarjan |
| Back-edge baseline (rank-based) | exactly **5** back directions / **7** module edges under the declared 12-area rank table | `ui_back_edge_baseline.txt` rank-check section |
| New-edge detection | a synthetic 20th SCC-internal direction OR a synthetic upward edge under the rank table → exit 1 | negative test (temp tree) |
| Default gate stays green | 0 new upward edges, 0 new dead imports, 0 module cycles | `graph_check.py` (no flag) |
| ctest | **1836/1836** (no `src/` change; lint-only) | serial `ctest -j1` both presets |

### F1 — deletes + render-helper extract (rows 1, 3, 8)

| Threshold | Value | Instrument |
|---|---|---|
| Back-edge directions cut | **3** (foundation→chrome, widgets→dialogs, prompt→messages); 5 → **2** remaining | `--target-ui9` vs baseline |
| chrome↔foundation SCC | **dissolved** (2-area SCC → 0; `chrome → foundation` is downward-legal) | Tarjan area SCC count |
| 7-area SCC | shrinks to a 3-area SCC {dialogs,features,screens} — messages, permissions, widgets and prompt drop out (prompt's only SCC-outgoing `prompt→messages` is cut; the full shrinkage was verified by simulation) | Tarjan |
| Dead-import row-1 verification | the `import cc.ui.chrome.layout` line is deleted from `logo.cppm:11`; the dead-import detector reports **0 NEW** dead imports (the `logo → chrome.layout` edge is NOT in `dead_imports_baseline.txt` — the detector does not flag it because `chrome.layout` exports the common tokens `mode`/`empty`/`render` that coincidentally appear in `logo.cppm`, the detector's documented false-negative bias; 126 dead imports detected today, none involving logo). The cut is OBSERVED by `--target-ui9` (the `foundation→chrome` direction disappears) and the deletion-safety check is the dual-preset build green (watch LLVM #184957 keep-imports — never text-only deletion). The baseline file is unchanged by this cut. | `--target-ui9`; `dead_imports_baseline.txt` diff (expect no change); dual-preset build |
| `message_tool_result` producer PSS | after **≤** before (no regression); target −10% (bodies moved to the leaf) | `measure_bmi.py` before/after |
| New `ansi_render` leaf PSS | measured and recorded (expected small: std + ftxui + `SgrAttr` only) | `measure_bmi.py` |
| Fan-out: body edit in `ansi_render` | **= 1** object recompiled (the leaf impl), not `message_tool_result`'s importers | `ninja -n` |
| Inline bodies | no interface > 100; `cc.ui.app.app` **≤ 58** (no increase) | `inline_def_check.py` |
| ctest / goldens | **1836/1836**; 56 goldens byte-identical | serial `ctest -j1` both presets |
| Build | dual-preset `-Werror` clean | local-linux + local-linux-release |

### F2 — registry inversions (rows 4, 6)

| Threshold | Value | Instrument |
|---|---|---|
| Back-edge directions cut | **2** (dialogs→screens, features→dialogs); 2 → **0** remaining | `--target-ui9` vs baseline |
| `--target-ui9` | **PASSES** — all 12 areas singleton SCCs | `graph_check.py --target-ui9` exit 0 |
| Area SCCs > 1 | **0** | Tarjan |
| Concrete casts confined | `static_pointer_cast` in the registration path appears ONLY in composition-root TUs (`src/ui/app/*`) | grep over `src/` |
| dialog↔features mutual reach | **0** (no path dialogs→features→dialogs) | Tarjan / reachability |
| Inline bodies | `cc.ui.app.app` **≤ 58** (registration wiring must not net-increase; resolve 1:1 or better) | `inline_def_check.py` |
| ctest / goldens | **1836/1836**; 56 goldens byte-identical | serial `ctest -j1` both presets |
| Build | dual-preset `-Werror` clean | both presets |

### F3 — state sharding (RFC F3 + composition-root resolution)

| Threshold | Value | Instrument |
|---|---|---|
| Stores landed | one per commit; **≥ 6** domain stores (Messages, Prompt, TaskView, Permission, Dialog, McpStatus + Chrome/Shell as needed) — all homed in `cc.ui.screens.*` (see placement map below) | commit log |
| Re-export shims | **0 remaining** at end of F3 (every shim deleted) | grep for `export import` shims referencing `repl_state` |
| State reach-up lint | **0** UI areas reach "up" for state; stores imported only by app (composition root) or screens-area modules; store modules import no area ranked above screens | store-naming/import lint (new in F3; spec below) |
| Threading hygiene | **0** `mutex`/`jthread`/`condition_variable` in any store module (the `pending_at_mention_mutex` moves to AppImpl) | grep over store modules |
| `repl_state.cppm` | reduced to a thin composition facade or deleted; producer PSS after **≤** before | `measure_bmi.py`; wc |
| AppAdapter inline bodies | **58 → ≤ 15** (composition only); the ≤ 15 target is derived from the F3-start per-body inventory (below), not asserted a priori; the ratchet is re-frozen at the measured result | `inline_def_check.py` + baseline edit |
| Fan-out: store accessor body edit | **= 1** object (the store impl) | `ninja -n` |
| ctest / goldens | **1836/1836**; 56 goldens byte-identical | serial `ctest -j1` both presets |
| Build | dual-preset `-Werror` clean | both presets |

### F4 — cc_ui library split (RFC F5)

| Threshold | Value | Instrument |
|---|---|---|
| `--target-ui9` | **PASS** (12 singletons) — prerequisite, re-asserted | `graph_check.py --target-ui9` |
| TLL lint | **0 TLL SCCs** (acyclic) — new lint asserting `target_link_libraries` graph acyclic | TLL parser + Tarjan (new flag) |
| Area libraries | **~12** (`cc_ui_foundation` … `cc_ui_app`), grouped by module-name area | CMake diff |
| File→lib grouping | every `cc.ui.<area>.*` module in exactly `cc_ui_<area>`; 0 cross-area file ownership | CMake diff vs module-name map |
| Fan-out: body edit in one area | recompiles only that area's objects (≤ the area's module count), NOT the whole `cc_ui` closure | `ninja -n` |
| Per-area producer PSS | heaviest area producer **≤ before** (no regression) — the split does not change per-TU PSS (same compiles), so this is a no-regression check, not a reduction; the `app.cppm` 3,222 MB peak is a property of F3's composition-root resolution (F3 has no app.cppm PSS threshold because F3 moves bodies OUT of app.cppm into impl TUs/stores, which can only lower or hold its PSS — recorded, not gated) | `measure_bmi.py` |
| Cold build wall time | after **≤** before (no regression) | wall clock, both presets |
| ctest / goldens | **1836/1836**; 56 goldens byte-identical | serial `ctest -j1` both presets |
| Build | dual-preset `-Werror` clean | both presets |

---

## (c) Sharpened per-phase scope

### F0 — lint + freeze (no `src/` behaviour change)

**Goal:** promote the 2026-09-26 one-off inventory computation into
`graph_check.py` so the UI9 target is reproducible, and freeze the live
back-edge set so it can only shrink.

**Exact `--target-ui9` lint spec:**

- **Globs:** `src/**/*.cppm` + `src/**/*.cpp` (module-impl units included —
  the +2 `screens→permissions` / `screens→widgets` edges in the attachment
  came from `.cpp` units; the live re-measurement confirms the loader already
  globs both, `graph_check.py:188-189`).
- **Area mapping:** `area(m) = ".".join(m.split(".")[:3])` for `cc.ui.*`
  (2nd-level area), matching the attachment's methodology and
  `graph_check.area_of` (which uses `[:2]` for the core graph; the UI9 flag
  uses `[:3]`).
- **Target set:** the 12 `cc.ui.<area>` areas. `--target-ui9` builds the area
  subgraph (edges between the 12 areas only), runs Tarjan, and asserts every
  area is a singleton SCC. It **FAILS today** (2 SCCs: 7-area + 2-area) and
  **PASSES after F2**.
- **Declared total order (12-area rank table).** "Back edge" is defined
  operationally against this table: an area-direction `A → B` is a **back
  edge** iff `rank(A) < rank(B)` (A imports a higher-ranked area). The table
  is the minimum-FAS order confirmed by the exhaustive 7! search:

  | Rank | Area | | Rank | Area |
  |---|---|---|---|---|
  | 11 | app | | 5 | widgets |
  | 10 | screens | | 4 | prompt |
  | 9 | dialogs | | 3 | chrome |
  | 8 | features | | 2 | foundation |
  | 7 | messages | | 1 | visual (leaf, 0 cc.ui imports) |
  | 6 | permissions | | 1 | tools (leaf, 0 cc.ui imports) |

  `visual`/`tools` are pure leaves (verified: zero `cc.ui` imports), so they
  rank below every importer; their mutual rank is irrelevant. Under this
  table the live back-edge set is exactly the **5 directions / 7 module
  edges** in section 0 (foundation→chrome, widgets→dialogs, dialogs→screens,
  features→dialogs, prompt→messages).
- **Baseline file:** `tools/arch/ui_back_edge_baseline.txt`, one
  `from -> to` area-direction per line, frozen at the **19** live
  SCC-internal directions (17 in the 7-area SCC + 2 chrome↔foundation).
  Format matches `upward_edge_baseline.txt` (`load_pair_baseline` with
  `" -> "`). The file also records the 5-direction back-edge subset (the
  rank-based baseline) so the lint can run BOTH checks below.
- **Two frozen sets, two checks (precise coverage).** The lint guards:
  (a) **Tarjan + subset freeze** — the 19 SCC-internal directions are
  frozen; a 20th SCC-internal direction fails (this is what the default
  gate cannot see: `cc.ui` is one rank-12 area in `graph_check.py`'s
  `TARGET_RANK`, so the default rank-based gate is blind to every
  cc.ui-internal area edge — the UI9 lint is the sole guard).
  (b) **Rank-based order-conformance** — under the 12-area table above, the
  5 back directions (7 module edges) are baselined; ANY other upward edge
  fails, including a new NON-SCC-forming one (e.g. `visual → foundation`
  would pass check (a) — visual is a singleton — but fails check (b) since
  rank(visual)=1 < rank(foundation)=2). After F2 severs all 5 back
  directions, check (b) enforces **0 upward edges**, which
  machine-enforces the declared total order. "Fail-on-addition, like the
  default gate" refers to check (b); check (a) is the SCC-shape ratchet.
- **Deterministic-fail behaviour:** exit 1 + a printed `TARGET UI9 SCC:` line
  per non-singleton SCC (the `--target-core8` print pattern,
  `graph_check.py:737-741`); a NEW back edge not in the baseline fails even
  before the target is met (fail-on-addition). Removals are fine and shrink
  the snapshot.
- **Wiring:** runs in the lightweight arch-check workflow (never behind the
  mac build — and per directive 2026-09-29, CI is not a gate at all; the lint
  is a local pre-merge check). The `--target-ui9` flag is additive; the
  default gate (module DAG, no new upward/dead edge) is unchanged.

**F0 deliverables:** the flag, the baseline file, the negative test, and a
re-measured Implementation History row (635 modules, 2 SCCs, 19 directions).

### F1 — deletes + render-helper extract (rows 1, 3, 8)

Concrete shared types / edges moving downward, source → destination:

| Cut | Source | Destination | What moves |
|---|---|---|---|
| Row 1 | `cc.ui.foundation.logo` (`logo.cppm:11`) | — (deleted) | the `import cc.ui.chrome.layout` dead import (no body reference — verified by reading the full module; the dead-import detector does NOT flag it, see the F1 threshold row). Deletion-safety: dual-preset build green (watch LLVM #184957 keep-imports — never text-only deletion); the cut is observed by `--target-ui9` |
| Row 3 | `cc.ui.widgets.all_components` (`all_components.cppm:19`) | — (deleted) | the `export import cc.ui.dialogs.feature_dialogs` umbrella re-export. Sole importer is `app.cppm` (verified: 1 importer); it adds a direct `import cc.ui.dialogs.feature_dialogs` if it references the symbols. After the umbrella delete `feature_dialogs` has ZERO importers (verified: no other module imports it), so per PRR §5 it is **deleted in the same commit** (dead code made obsolete by the cut) |
| Row 8 | `cc.ui.messages.message_tool_result` (`message_tool_result.cppm:38,63,142`) | new leaf `cc.ui.chrome.ansi_render` | `sgr_color_value_to_ftxui` (`:38`, called by `ansi_to_ftxui_elements` at `:160,163` — no other users), `apply_sgr_run` + `ansi_to_ftxui_elements` extracted verbatim into a leaf that imports only `std` + `cc.ui.chrome.terminal_io` (for `SgrAttr`, `terminal_io.cppm:118` — itself a std-only leaf). The leaf is homed in **chrome**, not messages: `prompt_input_footer`'s only messages import is `message_tool_result` (`prompt_input_footer.cppm:52`), so a messages-area leaf would leave `prompt → messages` intact and fail the F1 gate. `message_tool_result` imports the leaf back (messages→chrome, downward-legal); `prompt_input_footer` (`:52`,`:488`) imports the leaf instead of `message_tool_result` (prompt→chrome, downward-legal under the rank table) |

After F1: `foundation→chrome`, `widgets→dialogs`, `prompt→messages` are cut;
the chrome↔foundation SCC dissolves; the 7-area SCC loses `prompt`.

### F2 — registry inversions (rows 4, 6)

| Cut | Source | Destination | Mechanism |
|---|---|---|---|
| Row 4 | `cc.ui.dialogs.default_renderers` (`dialog_default_renderers.cppm:40`) | `cc.ui.screens.doctor_screen` side / composition root | doctor registers its renderer into `DialogRendererRegistry` (already exists; `register_default_renderers` at `:534`) from a screens-side TU or the app composition root; `default_renderers` drops the `doctor_screen` import. `doctor_screen.cppm` is already a leaf (imports only `cc.utils.json`) |
| Row 6 | `cc.ui.features.agents.agent_wizard` (`:49`), `cc.ui.features.plugins.plugin_install_flow` | a below-both protocol leaf + composition root | `wizard_dialog` is already a std-only leaf (`wizard_dialog.cppm:38`); the inversion makes the feature consumers **registration-driven**. The protocol leaf is **`cc.ui.foundation.feature_dialog_protocol`** (foundation area, rank 2 — below both features(8) and dialogs(9)); it imports only `std`. **Descriptor contract:** a `ViewKind` enum (agent-wizard, task-wizard, plugin-install, plugin-trust, …) + typed `std::function` descriptor/factory slots keyed by `ViewKind`; features register a factory for their content; the dialogs side (or the composition root) resolves the descriptor and constructs the dialog. Concrete `static_pointer_cast` is confined to composition-root TUs (`src/ui/app/*`). **trust_dialog's own imports** (`cc.plugins.plugin`, `cc.commands.plugin.plugin_trust`, `trust_dialog.cppm:46-48`) are NOT routed through the leaf: they are trust_dialog's direct dependencies and are downward-legal under the default rank gate (`cc.ui` rank 12 > `cc.plugins` rank 7, `cc.commands` rank 11) — they are not cc.ui-internal edges and do not change. **Open question resolved (registry vs re-rank):** the exhaustive 7! FAS search confirms the declared total order is already the global minimum (4 dirs/6 edges), so re-ranking cannot reduce the cut; **registry inversion is the chosen mechanism** (row 6), not re-ranking. |

After F2: `dialogs→screens` and `features→dialogs` are cut; `--target-ui9`
passes (12 singletons).

### F3 — ReplScreenState sharding (RFC F3 + composition-root resolution)

**Threading model (UI-thread affinity):** stores are UI-thread-affined plain
data; NO per-field mutexes are retrofitted. The one mutex currently in
`repl_state.cppm` (`pending_at_mention_mutex`, `:531`) and its staged
at-mention queue move OUT to the AppImpl composition layer (worker threads
stage results; the UI thread drains them — the invariant preserved, not
relocated). All of AppImpl's threads (`query_thread_`, `spinner_thread_`,
`bash_thread_`, `leader_inbox_thread_`, `statusline_thread_`) and mutex/CV
pairs stay in AppImpl.

**Shard inventory (source: `repl_state.cppm`, 691 LOC, 11 cc imports at
`:19-29`):**

| Store | Fields (by section comment) | Cross-area type dependency (classification — live types re-verified 2026-09-29) |
|---|---|---|
| `MessagesStore` | messages[] (`:260`), unseen-divider anchor (`:270`), virtual-list handle (`:278`), scroll/chrome state (`:287`), message projections (content/result preview, system/api-error, image-paste ids) | `messages_list.UnseenDivider`, `virtual_list.JumpHandle/VirtualListState` (concrete types, same-area or below screens). `visual.markdown.StreamingMarkdown` is a **raw non-owning pointer** `StreamingMarkdown* streaming_md` (`:688`, in `ReplScreenCallbacks`, not `ReplScreenState`) — NOT a `shared_ptr<void>`; the store imports `visual.markdown` (a leaf below screens), downward-legal, no erasure |
| `PromptStore` | input mode (`:79`), stashed prompt (`:356`), pasted contents (`:375`), placeholder (`:382`), submissions/hint counts (`:395,399`), prompt-suggestions flag (`:403`), command queue (`:406`), vim state, viewing agent/teammate (`:391`), teammate prefix color (`:410`) | `foundation.ui_types.PromptInputMode` (sink-to-foundation — already a leaf), `prompt.prompt_input_footer` footer projection types (same-area after prompt ranks below screens) |
| `TaskViewStore` | spinner mode (`:89`), task notifications, agent/teammate live state | `features.agents.agent_cards.AgentCardData`, `features.teams.live_teammates.LiveTeammate` are **by-value concrete vectors** (`std::vector<AgentCardData> agent_cards` `:608`; `std::vector<LiveTeammate> live_teammates` `:587`) — NOT opaque handles. Acceptable because the store lives in screens (screens→features is downward-legal, rank 10 > 8); no type erasure or sinking needed |
| `PermissionStore` | tool kind (`:182`), permission-prompt subset (`:203`), bash (`:231`), file edit/write/read (`:237`), web (`:246`), skill (`:249`) | `types.types` primitives only |
| `DialogStore` | overlay dialogs (`:50`), inline panels (`:70`), wizard draft (`:223`), trust payload (`:225`) | `dialogs.system.DialogQueue` (same-area). The genuinely opaque fields are `wizard_agent` (`:607`) and `wizard_trust` (`:611`), both `std::shared_ptr<void>` (lazy-created dialog component handles). `WizardDraft` is **not a stored field** — it appears only in the `save_agent_from_wizard` callback signature `std::function<void(const WizardDraft&)>` (`:668`); the callback parameter type is fine as-is (the callback is wired by the composition root) |
| `McpStatusStore` | pending at-mention inserts (drained, `:528-531`), MCP status | the mutex moves to AppImpl; the store holds only drained data |
| `ChromeStore` | git branch (`:423`), status-bar projection (`:101`), welcome header (`:415`), billing type (`:418`), oauth account (`:428`), feed content (`:431`), chrome layout state | `chrome.fullscreen_layout.StickyPrompt` (same-area after chrome ranks low) |

The cross-area state fields are **classified up front against their live
types** (re-verified 2026-09-29 by reading `repl_state.cppm`): by-value
concrete types (`AgentCardData`, `LiveTeammate` vectors; `DialogQueue`;
footer types) are acceptable **because the stores live in screens** —
screens→features/dialogs/prompt is downward-legal under the rank table, so a
concrete by-value field recreates no up-edge. Only the genuinely-erased
fields (`wizard_agent` :607, `wizard_trust` :611, both `shared_ptr<void>`)
are opaque handles; the `StreamingMarkdown*` raw pointer (`:688`) is a
non-owning view, not an erased handle. Mis-classifying one silently recreates
an area up-edge, which is why the live types — not the field names — are the
classification authority.

**Shim deletion plan:** a re-export shim keeps call sites compiling during
each move; **one store lands per commit**; every shim is deleted by end of F3
(gate: 0 shims remaining). `AppAdapter` (`app.cppm:238`) constructs and
connects the stores; cross-store reads go through selectors, never a direct
field reach-up. The 58 frozen AppAdapter inline bodies are resolved to
composition-only (≤ 15) and the ratchet re-frozen.

**Store placement map (module name + area).** All stores are homed in
`cc.ui.screens.*` — the only coherent reading: today `repl_state` is imported
only by `app.cppm`, `app_settings.cpp`, `app_team.cpp` (app area) and
`repl_screen.cppm` (screens, re-export), so screens is the lowest area that
already owns the state and screens→features/dialogs/prompt is downward-legal
(rank 10 > 8/9/4). Placing a store in its data's own area would turn
cross-store reads into up-edges (e.g. a features-area TaskViewStore imported
by screens would be screens→features, which is downward, but a
messages-area MessagesStore imported by a features store would be
features→messages, an up-edge). The F4 split map has a slot for them:
`cc_ui_screens` grows from 5 to 5 + (number of stores landed) modules.

| Store | Module | Area |
|---|---|---|
| `MessagesStore` | `cc.ui.screens.messages_store` | screens |
| `PromptStore` | `cc.ui.screens.prompt_store` | screens |
| `TaskViewStore` | `cc.ui.screens.task_view_store` | screens |
| `PermissionStore` | `cc.ui.screens.permission_store` | screens |
| `DialogStore` | `cc.ui.screens.dialog_store` | screens |
| `McpStatusStore` | `cc.ui.screens.mcp_status_store` | screens |
| `ChromeStore` | `cc.ui.screens.chrome_store` | screens |

**Store-naming/import lint (new in F3).** A second lint (alongside
`--target-ui9`) enforces the placement invariant:

- **Globs:** `src/ui/screens/*_store.cppm` (module names
  `cc.ui.screens.<name>_store`).
- **Out-of-store import rule:** a store module's `cc.ui.*` imports must
  target only areas ranked **below** screens (rank < 10) — i.e. a store must
  not import `cc.ui.app.*` or any area above screens (only app is above).
  Equivalently: no store imports the composition root.
- **Into-store importer rule:** a module importing `cc.ui.screens.*_store`
  must be in the **app area** (composition root) or **screens area**
  (same-area). No features/dialogs/messages/prompt/etc. module imports a
  store directly — cross-store reads go through selectors wired by AppAdapter.
- **Threading rule:** 0 `mutex`/`jthread`/`condition_variable` tokens in any
  store module (grep over the globs).
- **Rank table:** the F0 12-area rank table (above); screens = 10.
- **Baseline:** empty (0 violations) at F3 end; the lint is introduced with
  the first store and must pass from that commit.

**AppAdapter 58 → ≤ 15 (inventory-derived target).** The ≤ 15 target is not
asserted a priori; it is derived from the F3-start per-body inventory of the
58 frozen bodies. The inventory classifies each body as:
(a) **composition/construction/wiring** (stays in `app.cppm` or moves to a
composition-root site — `construct_impl`/`construct_teammate`/
`construct_settings`, `set_screen`, store/registry wiring);
(b) **state projection / mutation** (moves to the new stores or their impl
TUs — `AppendLocalMessagesToScreenState`, `AppendLocalCommandInputMessage`,
`AppendLocalCommandMessage`, `ClearActiveLocalJsxCommand`,
`DismissLocalJsxCommand`, `TriggerStatuslineUpdate`, `StartUiAnimationTicker`,
`is_streaming_thinking_visible`, `OpenSkillsMenu`);
(c) **test seams** (move to impl TUs — `submit_for_testing`,
`wait_for_local_bash_for_testing`, `inject_pasted_image_for_testing`,
`set_no_real_paste_worker_for_testing`, `set_input_text_for_testing`,
`handle_submit_for_testing`);
(d) **event/render dispatch** (moves to impl TUs or the repl screen).
The F3-start inventory enumerates all 58 bodies by name into (a)–(d);
the ratchet is re-frozen at the measured count of (a) bodies (expected ≤ 15).
If the inventory counts more than 15 genuine composition bodies, the target
is the measured count, recorded in the F3 Implementation History row.

### F4 — cc_ui library split (RFC F5)

**Acyclicity proof that must precede the split (both graphs):**

1. **Import graph:** `--target-ui9` PASS — all 12 `cc.ui` areas are singleton
   SCCs (the F2 exit state, re-asserted).
2. **TLL graph:** a new lint parses `target_link_libraries` from
   `src/cmake/targets/*.cmake`, builds the target graph, runs Tarjan, and
   asserts **0 TLL SCCs**. The TLL graph is already acyclic today (0 SCCs;
   `cc_ui` links 12 libs); the split must keep it that way.

**Target split map** (area → library, grouped by **module-name** area
`cc.ui.<area>`, the same key F0 uses — name/path decoupling means the rule is
stated, not inferred from directory):

| Library | Area | Modules (live count) |
|---|---|---|
| `cc_ui_foundation` | foundation | 10 |
| `cc_ui_chrome` | chrome | 8 |
| `cc_ui_visual` | visual | 4 |
| `cc_ui_tools` | tools | 17 |
| `cc_ui_widgets` | widgets | 13 |
| `cc_ui_prompt` | prompt | 13 |
| `cc_ui_permissions` | permissions | 10 |
| `cc_ui_messages` | messages | 33 |
| `cc_ui_features` | features | 12 |
| `cc_ui_dialogs` | dialogs | 21 |
| `cc_ui_screens` | screens | 5 (+ the F3 stores, 7 expected: `messages_store`, `prompt_store`, `task_view_store`, `permission_store`, `dialog_store`, `mcp_status_store`, `chrome_store`) |
| `cc_ui_app` | app | 2 |

Dependency order follows the declared total order
(`app > screens > dialogs > features > messages > permissions > widgets >
prompt > chrome > foundation`, with `visual`/`tools` as leaves). Each library
is an `include()`-per-target file under `src/cmake/targets/` (the RFC 0001
one-scope discipline), in dependency order in `src/CMakeLists.txt`.

---

## (d) Per-phase rollback story

- **F0:** lint-only, no `src/` change. Revert = remove the flag + baseline
  file. Zero behaviour risk.
- **F1:** each cut is one atomic commit. The row-1/row-3 deletes are
  revertible by re-adding the import (the row-1 edge is not in the dead-import
  baseline, so re-adding it does not trip the detector; `--target-ui9`
  re-reports the `foundation→chrome` direction). The row-8 extraction keeps
  `message_tool_result` importing the leaf; reverting moves the three
  functions back (the leaf is deleted). No flag-day: `prompt_input_footer`
  imports the leaf in both directions.
- **F2:** each inversion is one commit with a temporary registration shim.
  Revert = restore the concrete import and delete the registration; the
  protocol leaf is additive and can stay (it is below both areas, so it
  creates no up-edge).
- **F3:** one store per commit, each behind a re-export shim. Revert = restore
  the shim and move the fields back to `repl_state`. Because shims keep call
  sites compiling, a mid-F3 revert does not flag-day. The `pending_at_mention`
  mutex move is revertible independently (it moves back into the facade).
- **F4:** the only CMake-structure change, lands last. Revert restores the
  single `cc_ui` target (one `cc_ui.cmake`) without touching source — the
  module files are unchanged by the split (only their target ownership moves).

---

## (e) Tracking issue list

- **F0 — `--target-ui9` lint + freeze:** add the flag to `graph_check.py`,
  freeze `ui_back_edge_baseline.txt` at 19 SCC-internal directions + the
  5-direction back-edge subset under the 12-area rank table, add the negative
  test; gate: flag fails deterministically on the 2 live SCCs and on any new
  edge (SCC-internal or rank-upward).
- **F1 — deletes + row-8 extract:** cut `foundation→chrome` (dead import),
  `widgets→dialogs` (umbrella + delete the zero-importer `feature_dialogs`),
  `prompt→messages` (ansi converter leaf homed in **chrome**);
  gate: 3 back directions cut, chrome↔foundation SCC dissolved,
  `message_tool_result` PSS ≤ before, ansi-leaf fan-out = 1, 1836 + goldens.
- **F2 — registry inversions:** cut `dialogs→screens` (doctor registration)
  and `features→dialogs` (wizard/trust via `cc.ui.foundation.feature_dialog_protocol`);
  gate: `--target-ui9` PASS (12 singletons), concrete casts confined to
  composition root, 1836 + goldens.
- **F3 — state sharding:** land ≥ 6 domain stores one per commit (all in
  `cc.ui.screens.*`), move the at-mention mutex to AppImpl, resolve AppAdapter
  58→≤15 (inventory-derived), delete all shims; gate: 0 shims, 0 state
  reach-up (store-naming/import lint), 0 mutex in stores, 1836 + goldens.
- **F4 — cc_ui split:** add the TLL-acyclicity lint, split `cc_ui` into ~12
  area libraries by module-name area; gate: `--target-ui9` + TLL lint pass,
  per-area fan-out bounded, per-area producer PSS ≤ before (no regression),
  1836 + goldens.

---

## Review history

**2026-09-29 — three adversarial design reviews of this gate package**
(agent:design-review#2, agent:design-review#3, agent:design-review#4), verdict
**request-changes** on all three. All required changes applied in this
revision:

1. **Self-approving PRR sign-off removed** — the Production-readiness row is
   now `_pending_` (a document cannot review itself); the three design
   reviewers' identities and verdicts are recorded in the sign-off table.
2. **Tracking issue added to the RFC body** (frontmatter `tracking:`) so the
   status flip passes `rfc_lint.py:136`.
3. **RFC body reconciled with this gate** — evidence numbers corrected to
   148 modules / 2 SCCs / 19 directions / 5-dir-7-edge FAS / 1836 ctest;
   phase table sharpened to F0–F4; open questions marked resolved;
   correction row appended to Implementation History.
4. **F0 back-edge semantics defined** — operational definition (rank-based),
   full 12-area rank table, and the two frozen sets (19 SCC-internal
   directions + 5 back directions under the rank table) stated precisely;
   the declared order is machine-enforced after F2 via the rank check.
5. **F1 dead-import row corrected** — `logo → chrome.layout` is not in
   `dead_imports_baseline.txt` and the detector does not flag it; the cut is
   observed by `--target-ui9` and the deletion-safety check is the dual-preset
   build.
6. **F3 store placement map + store-naming/import lint specified** — all
   stores homed in `cc.ui.screens.*`; globs, rank table, and baseline stated.
7. **F4 PSS threshold restated** — "per-area producer PSS ≤ before (no
   regression)" (the split does not change per-TU PSS; the 3,222 MB peak is
   an F3 property, not an F4 action).
8. **F3 shard inventory re-classified against live types** —
   `AgentCardData`/`LiveTeammate` are by-value vectors (not opaque handles);
   `StreamingMarkdown` is a raw pointer (not `shared_ptr<void>`); `WizardDraft`
   is a callback parameter (not a stored field); only `wizard_agent`/
   `wizard_trust` are genuinely opaque.
9. **F1 row-8 extraction set completed** — `sgr_color_value_to_ftxui` added
   (called by `ansi_to_ftxui_elements` at `:160,163`).
10. **F1 row-8 leaf homed in chrome, not messages** — `cc.ui.chrome.ansi_render`
    (a messages-area leaf would leave `prompt → messages` intact and fail the
    F1 gate).
11. **F3 AppAdapter 58→≤15 target derived from a per-body inventory** (not
    asserted a priori).
12. **ctest baseline re-pinned to HEAD** — 1836 @ 1a83330 (was 1828 @
    03c7f92); the reconciliation rule now covers baseline drift (additions),
    not just deletions.
13. **F2 row-6 protocol leaf specified** — module name
    (`cc.ui.foundation.feature_dialog_protocol`), area, imports, descriptor
    contract; trust_dialog's `cc.plugins`/`cc.commands` imports are
    downward-legal and stay direct; the registry-vs-re-rank open question is
    resolved (registry inversion; the FAS order is already optimal).
14. **"12 of 17" arithmetic corrected to 13 of 17.**
15. **Minor issues folded in** — the `--target-core8` test parenthetical
    corrected (no such test exists; the F0 negative test is genuinely new);
    F1 deletes the zero-importer `feature_dialogs` in the same commit as the
    umbrella cut (PRR §5); the row-7 citation now names both deleted sites
    (`messages_interactions.cppm` + `permission_advanced_prompts.cppm`).

Independent design verification (agent:design-verify, 2026-09-29): **approved**
— all 20 required changes across the three reviews confirmed against the live
tree. Three residual M3 spots in the RFC body were fixed in the same pass
(row-6 `task_wizard.cppm:23` citation — the file was deleted in the graveyard
cleanup; row-8 `:491` → `:488`; 739 → 691 LOC in the state-sharding section)
and the F1 shrinkage is now stated in full (7-area SCC → 3-area SCC
{dialogs,features,screens}; messages/permissions/widgets/prompt drop out).

**2026-09-30 — F1 implementation corrections (agent:abcf1fc17c185d03f re-review, approved):**

16. **F1 row-3 sole-importer claim was stale.** The gate package states
    "Sole importer is `app.cppm` (verified: 1 importer)". At the cut-2 parent
    the RFC-0001 Phase-C followups had dropped that import; the live importers
    of `all_components` are 5 test TUs and ZERO src modules. The cut is
    unchanged (the umbrella re-export and the zero-importer `feature_dialogs`
    module are still deleted in the same commit); only the importer claim is
    corrected.
17. **`cc.ui.app.app` inline count is 29, not 58.** The F1 threshold table's
    "≤ 58" was written pre-extraction; the RFC-0001 Phase-C followups re-froze
    the ratchet at 29 (`c2-done`). The threshold is satisfied (29 ≤ 58) and
    the live ratchet (29) is the binding constraint for F2/F3 wiring.
18. **F1 row-8 PSS "expected small" assumption corrected.** The gate package
    expected the new leaf's BMI to be "small (std + ftxui + `SgrAttr` only)".
    A declarations-only interface that NAMES ftxui types (`ftxui::Color` /
    `Element` in the signatures) embeds ~10.8 MB of ftxui declarations in the
    leaf BMI, which regressed `message_tool_result` producer PSS +7.3%
    (463→497 MB) and violated the "after ≤ before" threshold. The remediation
    (commit 109d546) redesigned the interface to name NO ftxui type:
    `sgr_color_value_to_ftxui` is internal to the impl unit, and
    `ansi_to_ftxui_elements` returns type-erased `shared_ptr<void>` (callers
    cast back). Leaf BMI 10.8 MB→21 KB; PSS 497→461.8 MB (≤ 463 MB threshold
    met; re-measured 462.1 MB). The "after ≤ before" threshold is achievable
    ONLY via this erasure — a ftxui-naming interface cannot meet it.
19. **Fan-out=1 instrument caveat.** The F1 threshold's instrument
    (`ninja -n`) reports 328 objects for a one-line body edit to
    `ansi_render.cpp` — but the same 328 objects appear for an edit to an
    UNRELATED `.cpp` file. The cause is a pre-existing CMake dyndep cascade
    (`CXX.dd` is regenerated on any `.cpp` edit and is an implicit dependency
    of all cc_ui objects), not the cut. Module-level fan-out=1 (body in `.cpp`,
    interface in `.cppm`; `.pcm` unchanged on body edit) is the correct Phase-C
    measure and was confirmed by a real build probe (exactly 1 object
    recompiled).
