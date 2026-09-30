---
rfc: 2
title: UI state sharding and breaking the UI9 SCC
status: implementable
owners: "@Zzzode"
reviewers: ["agent:design-review#1 (SOUND with required changes - numbers regenerated, minimum-cut corrected to 7 dirs/10 edges, row 8 extraction, F3 threading model; all applied 2026-09-26)", "agent:design-review#2/#3/#4 (three adversarial reviews of the implementable gate package, 2026-09-29, request-changes - all required changes applied; see attachments/0002-implementable-gate.md Review history)", "agent:design-verify (independent verification of the revised gate package + body, 2026-09-29, approved)", "agent:prr-review (independent PRR of the implementable gate, 2026-09-29, approve - 30/30 checklist rows)"]
created: 2026-09-26
last-reviewed: 2026-09-29
tracking: "https://github.com/Zzzode/Loom/issues/2"
---

# RFC 0002 — UI state sharding and breaking the UI9 SCC

## Summary

The 148 `cc.ui.*` named modules compile to a module-level DAG but
collapse at the **directory responsibility** level into **two** strongly
connected components (a 7-area SCC `{dialogs, features, messages,
permissions, prompt, screens, widgets}` and a 2-area SCC `{chrome,
foundation}`; the 2026-09-26 measurement was one 9-area SCC of 218 modules,
before the RFC 0001 Phase-B graveyard cleanup deleted ~70 modules — the live
re-measurement is in the implementable gate package). This RFC severs that SCC
by moving shared **types downward** into foundation/leaf modules, extracting
cross-cutting render helpers into lower leaves, inverting
framework→consumer dependencies via registries bound at the composition
root, and sharding the monolithic `ReplScreenState` (plain, UI-thread
affined data) into domain-owned stores. Only after the import graph and
`target_link_libraries` are both provably acyclic do we split the
deliberately-single `cc_ui` static library into ~12 area libraries. No UI
behaviour changes; the 56 truecolor golden suites gate every commit.

This is the RFC 0001 Phase F follow-up RFC. **Implementation does not
start until this RFC is accepted and RFC 0001 Phase B lands.**

## Motivation

Module-level acyclicity is necessary but not sufficient: the named module
graph is a DAG, yet the UI directories are mutually reachable, so they
cannot be separate libraries and a change in one UI area forces a BMI
recompile across the UI closure. Measured 2026-09-29 (live re-measurement in
[attachments/0002-implementable-gate.md](attachments/0002-implementable-gate.md),
section 0; the 2026-09-26 32-edge inventory in
[attachments/0002-ui9-edge-inventory.md](attachments/0002-ui9-edge-inventory.md)
predates the graveyard cleanup and is superseded where they differ):

### Evidence

| Metric | Current (2026-09-29 live) | Target | How measured |
|---|---|---|---|
| `cc.ui.*` modules | 148 | unchanged (moves, no deletes) | `export module cc.ui` count |
| UI areas (2nd-level) | 12 (7+2 in SCCs + app/tools/visual) | 12 singleton areas | area-level Tarjan over cc.ui imports |
| UI area SCCs > 1 | **2 SCCs: a 7-area SCC + a 2-area SCC** | all 12 singleton | Tarjan (independent 7! FAS search) |
| Back edges inside the SCCs | **19** SCC-internal area-directions (17 in the 7-area SCC + 2 chrome↔foundation); 5 are back edges under the declared order (7 module edges) | 0 | gate package section 0; F0 lints `*.cppm + *.cpp` |
| Minimum feedback arc set | **5 area directions (7 module edges)** sever the SCCs | 0 | exhaustive 7! order search |
| Already-singleton UI areas | 3 (`app`, `tools`, `visual`) | all 12 | same SCC run |
| `ReplScreenState` | 691 LOC, ~100 fields, 11 cc-imports | UI-thread-affined domain stores | wc / field count |
| `target_link_libraries` graph | **already acyclic (0 TLL SCCs, no --start-group)** today | stays acyclic, asserted by lint | parsed TLL graph |
| `cc_ui` static libraries | 1 (intentionally; name-only SCC via cross-target file ownership) | ~12, after graph+TLL acyclic | file→lib grouped by module-name area |
| Edit fan-out / PSS | large UI closure BMI | per-area, measured: PSS/PCM/wall via `measure_bmi.py`; fan-out via import-closure/ninja | both tools; numeric thresholds in the gate package |

The link rationale in earlier notes is corrected here: ld.lld / ld64
resolve cyclic static archives to a fixpoint (the cyclic Core8 archives
already link once, without `--start-group`). Therefore the F invariant is
**graph acyclicity plus acyclic `target_link_libraries`, both asserted by
lint** — not "the linker rejects cycles". Linking today does not make the
SCC harmless: the cost is BMI fan-out, recompilation blast radius, and the
inability to ship independently-built area libraries.

## Goals

- G1. `tools/arch/graph_check.py --target-ui9` passes: the 7+2 SCC areas
  plus `app/tools/visual` are pairwise singleton SCCs (12 total).
- G2. A new lint asserts the corresponding area `target_link_libraries`
  graph is acyclic.
- G3. `ReplScreenState` is sharded into UI-thread-affined domain stores
  homed in `cc.ui.screens.*` (e.g. `cc.ui.screens.messages_store` — the only
  coherent placement, since today only app/screens import `repl_state` and
  screens→features/dialogs/prompt is downward-legal); a store-naming/import
  lint proves no UI area reaches "up" for state and the composition root
  wires, not owns, the stores.
- G4. `cc_ui` splits into ~12 static libraries (grouped by **module-name
  area**, the same key F0 uses — name/path decoupling means the grouping
  rule must be explicit) only after G1–G2; each area library builds with
  an acyclic dependency set. The `target_link_libraries` graph is already
  acyclic today; the split must keep it that way.
- G5. Producer PSS / BMI / wall measured with `measure_bmi.py` and edit
  fan-out measured as the recompiled-closure on a one-body vs one-interface
  edit (ninja/`-n`), before/after per phase; concrete pass thresholds set
  at the implementable gate from the F0 baseline.

## Non-Goals

- No user-visible behaviour, render output, colour, spacing or event change.
- No rewrite of the FTXUI event model or the component-state convention
  ("FTXUI components must be held by state").
- No re-architecture of non-UI code (that is RFC 0001 Phase B).
- No textual FTXUI header adoption (revisit at clang ≥ 23 + cmake 4 per
  RFC 0001 Phase A).
- No C++ namespace churn beyond what a module/type move strictly needs.

## Proposal

Apply, per back edge, exactly one of three mechanisms, chosen by what the
import actually carries:

1. **Type sink.** When area A imports area B only for a plain data type /
   enum (`RiskLevel`, `PastePreview`, a primitive), move that type to a
   lower leaf both already depend on (foundation or a new shared-types
   module). The producer keeps behaviour; consumers import the leaf.
2. **Registry inversion.** When A imports B to *invoke* B concrete UI
   (a dialog renderer, a wizard, feature panels), define a type-erased
   registration point (`std::function` / `shared_ptr<void>` slots — the
   pattern already proven by `repl_state.cppm` opaque handles) on the lower
   side; the concrete implementation registers from the composition root.
3. **Delete the edge.** Some back edges are dead `import`s or umbrella
   `export import`s kept only for include convenience; after a full
   dual-preset build proves no use, remove them (the RFC 0001 Phase C/B
   pattern; watch LLVM #184957 keep-imports — never text-only deletion).

```
after F (area edges point downward only):

  app ──▶ screens ──▶ features ──▶ dialogs ──▶ widgets ──▶ foundation
   │           │            │             │
   └──────────▶┴──▶ prompt ──┴──▶ messages ─┴──▶ permissions ──▶ shared-types
        chrome / visual / tools are leaves or independently ordered
```

The exact 19 SCC-internal area-direction edges are in the implementable gate
package (section 0). An exhaustive 7! feedback-arc-set search over the live
7-area SCC gives a **minimum cut of 5 area directions / 7 module edges** —
RFC rows **{1,3,4,6,8}** sever the SCCs under the total order
`screens > dialogs > features > messages > permissions > widgets > prompt
> chrome > foundation` (rows 2 and 7 are already gone or downward-legal on
the live tree). Rows **5 and 9 are NOT required for acyclicity**:
they are deliberate *decoupling* (stop a framework importing concrete
feature panels; re-home leaf modules) and can either be included for
independence or left as legal downward edges. The FAS is recomputed by the
F0 lint after every phase since moving types changes the graph.

### Detailed design — back edge → ownership change

| # | Edge (verified site) | Carries | Mechanism |
|---|---|---|---|
| 1 | foundation→chrome `foundation/logo.cppm:11` | import with no body reference | **delete** after dual-preset build (possible #184957 keep-import; the dead-import detector does not flag it — see gate package F1) |
| 2 | foundation→widgets `foundation/design_extras.cppm:26` (`custom_select`, used at `:143`) | a custom widget primitive | **GONE on the live tree** — `design_extras.cppm` was deleted in the RFC 0001 Phase-B graveyard cleanup (2026-09-28) |
| 3 | widgets→dialogs `widgets/all_components.cppm:19` (`export import feature_dialogs`) | umbrella re-export | **delete** umbrella; point importers at the app composition layer; delete the zero-importer `feature_dialogs` in the same commit |
| 4 | dialogs→screens `dialogs/dialog_default_renderers.cppm:40` (`doctor_screen`) | concrete screen renderer | **registry inversion**: doctor registers its renderer from the screens side into `DialogRendererRegistry` |
| 5 *(decoupling, not in the 5-dir minimum)* | dialogs→features `plugin_dialog.cppm:41-44` (4 panels), `hooks_dialog_renderer_impl.cpp:23` | concrete feature panels | **registry inversion** via a registration *protocol leaf BELOW both areas* (`cc.ui.foundation.feature_dialog_protocol` — ViewKind enum keys + typed `std::function` descriptors/factories); concrete `static_pointer_cast` confined to composition-root TUs. Not needed for G1; do it to stop the framework importing concrete panels |
| 6 | features→dialogs `agent_wizard.cppm:49`, `plugin_install_flow.cppm:25-26` (`task_wizard.cppm:23` was deleted in the graveyard cleanup) | generic wizard/trust framework | framework stays dialogs-side (`wizard_dialog` is already a zero-cc.ui-import leaf); make consumers **registration-driven** through the row-5 protocol leaf. **Open question resolved (registry vs re-rank):** the FAS order is already optimal, so registry inversion is the mechanism, not re-ranking |
| 7 | messages/permissions→dialogs `messages_interactions.cppm:52,58`, `permission_advanced_prompts.cppm:39,46` | **only** `RiskLevel` | **GONE on the live tree** — both sites were deleted in the graveyard cleanup (the `RiskLevel` extraction is no longer needed) |
| 8 | prompt→messages `prompt_input_footer.cppm:488` calls `msgs::ansi_to_ftxui_elements` from `message_tool_result.cppm` | a *render converter* that pulls in chrome.terminal_io + messages.message_components + visual.markdown | **EXTRACT, do not move the module**: lift `ansi_to_ftxui_elements` + `apply_sgr_run` + `sgr_color_value_to_ftxui` (which need only `SgrAttr` + ftxui) into a new leaf **`cc.ui.chrome.ansi_render`** (chrome, not messages — a messages-area leaf would leave `prompt → messages` intact); moving `message_tool_result` wholesale would recreate the cycle |
| 9 *(decoupling, not in the 5-dir minimum)* | widgets→prompt: `text_input.cppm:30` uses `PastePreview` (prompt_paste_handler), `text_input_widget.cppm:26-27` placeholder + it CALLS `build_combined_highlights()` (`:493`; the prompt side of that edge also originates in `text_input_render.cpp:25`) | type **and behaviour** | **move the whole leaf modules** (they import only foundation/cc.utils, so already lower-able), not type-only pieces. Legal to leave under the minimum total order |
| 10 | messages→ui.tools.registry/generic (2 edges); widgets→visual.markdown (1; markdown also imported by dialogs 3, messages 5, permissions 3, screens 2, features 1) | registry is a zero-import leaf; markdown is a pure visual leaf | **rank** ui.tools below messages; order visual as a pure leaf below all six importer areas |

The gate package (section 0) lists all 19 SCC-internal directions / module-edge
pairs; the **rows {1,3,4,6,8} are the minimum cut** (5 directions / 7 module
edges) and rows 5/9 are optional decoupling. The remaining 13 of the 17
internal directions in the 7-area SCC, and `chrome → foundation`, are legal
downward edges under the verified total order.

### State sharding (F3)

`ReplScreenState` (691 LOC, ~100 top-level fields) currently co-locates
fields for messages, prompt, tasks, permissions, dialogs, MCP and shell in
one plain struct. **Threading model (corrected after review):** the state
struct itself holds only one mutex (`pending_at_mention_mutex`) and no
jthreads/condition variables. The concurrency lives in **AppImpl (the app
composition area)**: `query_thread_`, `spinner_thread_`, `bash_thread_`,
`leader_inbox_thread_`, `statusline_thread_` plus ~7 mutex/CV pairs
(`result_mutex_`, `paste_mutex_`, `bash_result_mutex_`, `permission_mutex_`/
cv, `elicitation_*`, `ask_user_*`, `statusline_*`). The invariant today is
**plain stores mutated only on the UI thread; worker threads stage results
into composition-owned queues and surface them with `PostRenderEvent`**.
State sharding must preserve, not relocate, that model:

1. Stores are **UI-thread-affined plain data**; do NOT retrofit per-field
   mutexes into them. Cross-store selectors/accessors are UI-thread-only.
2. The typed cross-area state fields are **classified up front against their
   live types** (re-verified 2026-09-29): by-value concrete vectors
   (`AgentCardData` `repl_state.cppm:608`, `LiveTeammate` `:587`) are
   acceptable **because the stores live in `cc.ui.screens.*`** —
   screens→features is downward-legal under the declared order, so a concrete
   by-value field recreates no up-edge. Only the genuinely-erased fields
   (`wizard_agent` `:607`, `wizard_trust` `:611`, both `shared_ptr<void>`)
   are opaque handles; `StreamingMarkdown` is a raw non-owning pointer
   (`:688`, in `ReplScreenCallbacks`), and `WizardDraft` appears only in a
   callback signature (`:668`), not as a stored field. Mis-classifying one
   silently recreates an area up-edge.
3. All worker threads, staged queues, mutexes and CVs stay in the AppImpl
   composition layer; they never move into a store.

Split incrementally into domain stores (`MessagesStore`, `PromptStore`,
`TaskViewStore`, `PermissionStore`, `DialogStore`, `McpStatusStore`, …),
**all homed in `cc.ui.screens.*`** (the only coherent placement: today
`repl_state` is imported only by app/screens, and screens→features/dialogs/
prompt is downward-legal; placing a store in its data's own area would turn
cross-store reads into up-edges). `AppAdapter` constructs and connects them.
Cross-store reads go through selectors, never a direct field reach-up. A
**re-export shim** keeps call sites compiling during the move; each shim has
a declared removal and the gate is one store landed per commit with all
shims deleted by end of F3.

### Library split (F4, last)

Once `--target-ui9` and the TLL lint pass, split `cc_ui` into ~12 area
static libraries (`cc_ui_foundation`, `cc.ui_chrome`, `cc_ui_widgets`,
…) in dependency order, using the same `include()`-per-target /
one-scope CMake discipline as RFC 0001. **File→library grouping is by
module-name area (`cc.ui.<area>`), exactly the F0 mapping** — module names
are decoupled from paths, so the rule must be stated, not inferred from
directory. Note the `target_link_libraries` graph is **already acyclic
today** (0 TLL SCCs, no `--start-group`); the residual Core4/UI9 cycles
are module-*name*-level only because targets already own files across name
prefixes. F4 must keep TLL acyclic and add a lint asserting it.
CLAUDE.md's warning that the UI directories collapse SCC-wise
(foundation↔chrome, dialogs↔widgets, messages↔prompt) is exactly what
F1–F3 removes first.

## Phases and graduation criteria

The phase table below is the RFC-level plan; the **sharpened, measurable
graduation thresholds** (numeric, falsifiable, with instruments) are in the
implementable gate package
[attachments/0002-implementable-gate.md](attachments/0002-implementable-gate.md)
section (b), which supersedes the numbers here where they differ. The gate
package merges RFC F3+F4 into gate F3 (state sharding + composition-root
resolution) and renumbers RFC F5 → gate F4 (library split).

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| F0 | Lint + freeze | add `--target-ui9` gate (globs `*.cppm` + module-impl `*.cpp`) and `ui_back_edge_baseline.txt` (19 SCC-internal directions + 5 back directions under the 12-area rank table); no code yet | proposed | gate FAILS deterministically on the 2 live SCCs and on any new edge (SCC-internal or rank-upward) |
| F1 | Deletes + render-helper extract | rows 1, 3, 8 (row 2/7 sites already deleted) | proposed | 3 back directions cut (5→2); chrome↔foundation SCC dissolved; 7-area SCC shrinks to a 3-area SCC {dialogs,features,screens} (messages/permissions/widgets/prompt drop out — verified by simulation); no new interface over 100 inline bodies; 1836 + goldens |
| F2 | Registry inversion | rows 4, 6 (+ optional decoupling row 5) via `cc.ui.foundation.feature_dialog_protocol` | proposed | 2 back directions cut (2→0); `--target-ui9` PASS (12 singletons); concrete casts confined to composition-root TUs; 1836 + goldens |
| F3 | State sharding + composition root | UI-thread-affined domain stores (all in `cc.ui.screens.*`); typed cross-area fields classified by live type; resolve AppAdapter 58→≤15 (inventory-derived) | proposed | 0 state reach-up (store-naming/import lint); one store per commit, all re-export shims removed by end-of-F3; 0 mutex in stores; 1836 + goldens |
| F4 | Split cc_ui | ~12 acyclic area libraries by module-name area | proposed | `--target-ui9` + TLL lint pass (12 singletons); per-area fan-out bounded; per-area producer PSS ≤ before; 1836 + goldens |

Per change: dual-preset `-Werror` green, serial ctest -j1 green, producer
PSS/fan-out recorded with `measure_bmi.py`, independent adversarial
agent review, local dual-preset gate (per directive 2026-09-29, GitHub CI
is not a gate).

## Production Readiness Review

Filled at the `implementable` gate (this is `provisional`); the full PRR is
in the gate package section (a). Known PRR points: correctness tests
(1836 baseline @ 1a83330) and truecolor golden suites are the behaviour
net; rollback per phase is a revert; observability traces
(messages.jsonl / dump-prompts) and the `<task_notification>` /
`<status>` / `<summary>` tag shapes must remain byte-identical.

## Rollout and rollback

- Each phase is an independent, revertible commit sequence on master
  behind its own local dual-preset gate (per directive 2026-09-29, GitHub
  CI is not a gate); F0 ships a failing-frozen lint with no behaviour change.
- Type sinks and registry inversions keep a temporary re-export /
  registration shim so a phase can be reverted without a flag-day.
- F4 library split is the only CMake-structure change and lands last,
  after both graph and TLL are acyclic; reverting it restores the single
  `cc_ui` target without touching source.
- Ordering: F0 → F1 → F2 → F3 → F4. **F0, F1 and F2 are lint/type/
  registration work and bind at the existing app root (it already imports
  every UI area), so they do NOT wait on RFC 0001 Phase B.** **F3**
  (state stores and composition-root resolution) **gated on RFC 0001
  Phase B** (`cc.orchestration`) — Phase B is now complete, so the
  dependency is satisfied; Phase E (the `measure_bmi.py` sampler) is
  already landed.

## Drawbacks

- Registries replace some direct typed calls with type-erased slots,
  adding a small indirection and moving some wiring errors from compile
  time to the composition root (mitigated by strong single-root tests).
- The F3 state split touches shared mutable state read by worker threads;
  it is the highest-risk phase and must move one store at a time.
- More libraries and a slightly larger CMake surface.

## Alternatives considered

- **Do nothing.** Leaves the UI area SCCs, full-UI BMI fan-out, and blocks
  any area-level library/incremental build. Loses on the measured metrics.
- **Split cc_ui across the real SCC now.** Explicitly rejected in CLAUDE.md
  and RFC 0001: cyclic libraries need link groups and hide the structural
  problem; graph acyclicity comes first.
- **Build `-j` throttling / textual-header revert.** Pre-decided losing
  moves (memory is solved by splitting TUs; FTXUI header units fail on
  clang 22).
- **Big-bang rewrite of UI state.** Too risky against 56 golden suites and
  threaded queues; the incremental shim plan wins.

## Testing and verification plan

- ctest baseline **1836 @ 2026-09-29** (commit 1a83330; reconcile every
  change — any delta from baseline drift or an RFC-introduced change is
  reconciled exactly in the phase's Implementation History row); every
  phase serial `-j1` on debug and release, dual `-Werror`.
- The 56 truecolor / E2E golden suites must be byte-identical each commit
  (no UPDATE_GOLDENS in this RFC — output does not change).
- New lint: `--target-ui9` and the acyclic-TLL assertion run in the
  lightweight arch-check workflow, never behind the mac build.
- Concurrency: targeted tests for prompt/messages/permission/observer
  paths through the sharded stores; no new timing assumptions.

## Documentation impact

- [x] `CLAUDE.md` — the `cc_ui` single-target / SCC note updates when F4 lands
- [ ] `docs/decisions/design-decisions.md` — registry-inversion and
  type-sink decisions, and the corrected link-cycle rationale
- [ ] New module/area doc headers for the shared-types leaf and stores

## Open questions

All four open questions are **resolved** (2026-09-29, gate package):

| Question | Owner | Resolution |
|---|---|---|
| Row 6: invert features→dialogs via registry vs re-rank areas by dependency weight | @Zzzode | **Resolved — registry inversion.** The exhaustive 7! FAS search confirms the declared total order is already the global minimum (4 dirs/6 edges), so re-ranking cannot reduce the cut; the mechanism is the `cc.ui.foundation.feature_dialog_protocol` registry leaf (gate package F2) |
| Name/home of the new shared-types leaf (foundation vs a new `cc.ui.shared`) | @Zzzode | **Resolved — the row-8 ansi leaf is `cc.ui.chrome.ansi_render`** (chrome, not messages: a messages-area leaf would leave `prompt → messages` intact); the row-5/6 protocol leaf is `cc.ui.foundation.feature_dialog_protocol` |
| Exact store boundaries for F3 (one per area vs fewer) | @Zzzode | **Resolved — ≥ 6 domain stores, all homed in `cc.ui.screens.*`** (Messages, Prompt, TaskView, Permission, Dialog, McpStatus + Chrome as needed); the only coherent placement since today only app/screens import `repl_state` |
| Minimum sever set is 10 in the current graph — re-verify after F1/F2 change it | agent | **Resolved — re-verified 2026-09-29:** the live minimum is 5 dirs / 7 module edges (not 10); the F0 lint re-runs the cut analysis per phase |

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-09-26 | — | RFC opened (provisional) after RFC 0001 A/C/D; measured 219 modules, 9-area SCC, 32 back edges, repl_state 739 LOC; awaits RFC 0001 Phase B + E before any code | — | discovery workflow wvjr74s34; graph inventory |
| 2026-09-29 | — | **Correction row (SKILL §5):** body reconciled with the implementable gate package after three adversarial design reviews (agent:design-review#2/#3/#4, request-changes, all required changes applied). Evidence numbers corrected to the live 2026-09-29 re-measurement: 148 `cc.ui.*` modules (was 218), 2 SCCs (7-area + 2-area, was 1 of 9), 19 SCC-internal directions (was 34), minimum FAS 5 dirs / 7 module edges (was 7/10), `repl_state.cppm` 691 LOC (was 739), ctest 1836 @ 1a83330 (was 1706). Phase table sharpened to F0–F4 (RFC F3+F4 merged into gate F3; RFC F5 → gate F4). Rows 2 and 7 marked gone (sites deleted in the graveyard cleanup). Row-8 leaf homed in chrome (`cc.ui.chrome.ansi_render`), not messages. F3 stores placed in `cc.ui.screens.*`; shard inventory re-classified against live field types. Open questions all resolved. Tracking issue added (frontmatter `tracking:`). | — | gate package section 0; `graph_check.py --json`; `ctest -N` |
| 2026-09-29 | — | **RFC promoted provisional → implementable.** Gate package (attachments/0002-implementable-gate.md) supplies the PRR, per-phase numeric thresholds and rollback story. Design gate: three adversarial reviews (agent:design-review#2/#3/#4, request-changes, all required changes applied) + independent verification (agent:design-verify, approved). PRR gate: agent:prr-review, approve (30/30 checklist rows; one blocking ctest-arithmetic finding fixed and re-verified — local-linux `ctest -N` = 1836). `rfc_lint.py` green. | 0bd6aad | gate package Review history + sign-off table; `ctest --preset local-linux -N` = 1836 |
| 2026-09-30 | F0 | **F0 implemented — `--target-ui9` lint gate (commit 8e6f1ae).** `tools/arch/graph_check.py` gains the UI9 gate: the 12-area `cc.ui.<area>` subgraph (area = first 3 dot-segments) with two frozen sets — (a) Tarjan + subset freeze of the 19 SCC-internal area-directions (17 in the 7-area SCC + 2 chrome↔foundation), the sole guard for cc.ui-internal edges since the default rank gate is blind to them (cc.ui is one rank-12 area); (b) rank-order conformance under the declared 12-area UI9_RANK total order — the 5 back directions / 7 module edges are baselined and ANY other upward edge fails, including a non-SCC-forming one. Fails today (exit 1, 2 TARGET UI9 SCC lines), passes after F2; additions fail, removals shrink the snapshot. Baseline `tools/arch/ui_back_edge_baseline.txt` generated from the live tree with the lint's own loader and re-measured independently: 635 modules / 761 units / 0 module cycles; 148 cc.ui modules; 12 areas; 2 SCCs; 19 internal directions; 5 back dirs / 7 module edges — all match the gate package. Negative coverage: `tools/arch/test_target_ui9.py` (a synthetic tools↔visual SCC fails check (a) in isolation; a synthetic visual→foundation edge fails check (b) in isolation; default gate stays green). Default gate and `--target-core8` unchanged. Review: agent:ac64c4004a5d99c99 (review:F0, workflow wf_af4130ef-07a) — approved; one MAJOR (this re-measured Implementation History row was a declared F0 deliverable — added here) + two minors (CI wiring ambiguity — CI is not a gate per the 2026-09-29 directive, the lint is a local pre-merge check documented in tools/arch/README.md; post-F0 drift from c24's import swap broke the default gate at HEAD — reconciled in the RFC 0001 2026-09-30 review-closure row). | 8e6f1ae | `graph_check.py --target-ui9`; `test_target_ui9.py`; gate package §F0 |
| 2026-09-30 | F1 | **F1 implemented — rows 1/3/8 cuts (commits e37a7f7, 232baff, c14a95b; PSS remediation 109d546).** Three atomic cuts, each dual-preset + serial-ctest green: (1) deleted the dead `import cc.ui.chrome.layout` from `logo.cppm` — the chrome↔foundation SCC dissolved (2 SCC lines → 1), 5→4 back directions; deletion-safety verified by the dual-preset build (the dead-import detector does not flag this edge — documented false-negative bias — and it is not an LLVM #184957 keep-import). (2) Deleted the `export import cc.ui.dialogs.feature_dialogs` umbrella re-export from `all_components.cppm` + the zero-importer `feature_dialogs` module (1464 LOC) in the same commit (PRR §5) — 4→3 back directions. **Live-tree correction to the gate package:** app.cppm was NOT the sole importer of all_components (the RFC-0001 Phase-C followups dropped that import); the real importers are 5 test TUs, 0 src — verified by module-name grep. (3) Extracted `sgr_color_value_to_ftxui`/`apply_sgr_run`/`ansi_to_ftxui_elements` verbatim from `message_tool_result` into the new chrome leaf `cc.ui.chrome.ansi_render` (bodies in an impl unit for fan-out=1); `prompt_input_footer` swapped its `message_tool_result` import for the leaf, severing prompt→messages — 3→2 back directions; the 7-area SCC shrank to the 3-area {dialogs,features,screens}. **Cut-3 PSS remediation (109d546):** the initial declarations-only interface named ftxui types (`ftxui::Color`/`Element`), embedding 10.8 MB of ftxui in the leaf BMI and regressing `message_tool_result` producer PSS +7.3% (463→497 MB), violating the F1 "after ≤ before" threshold. Redesigned the interface to name NO ftxui type — `sgr_color_value_to_ftxui` internal to the impl unit (called only by `ansi_to_ftxui_elements`), `ansi_to_ftxui_elements` returns type-erased `shared_ptr<void>` (5 call sites cast back with `static_pointer_cast<std::vector<Element>>`; behavior-preserving — `vbox` of the per-line vector is identical to the old multi-line path). Leaf BMI 10.8 MB→21 KB; PSS 497→461.8 MB (≤ 463 MB pre-cut threshold — met; re-measured 462.1 MB by the re-reviewer). **Fan-out=1** confirmed at module level by real build probe (a body edit to `ansi_render.cpp` recompiles exactly 1 object); `ninja -n` reports 328 objects due to a PRE-EXISTING CMake dyndep cascade (`CXX.dd` regenerates on any `.cpp` edit, rebuilding all cc_ui objects) — not introduced by F1, affects all `.cpp` body edits. **ctest reconciled:** 1851 live (1853 discovered, 2 disabled) vs 1836 @1a83330 — the +15 delta is from pre-F1 RFC-0001 c20–c24 followups (eddb713, 46d4d94, d27a224); F1 touches no `tests/` files. Baseline `ui_back_edge_baseline.txt` re-frozen to the post-F1 tree (scc-internal 19→5, back 5→2). Review: cut1 agent:ace2465e8c33e7129 (approved), cut2 agent:a8d8c2b85f90606a6 (approved), cut3 agent:a018fe92c0af420eb (request-changes — PSS regression) → remediated in 109d546 → re-review agent:abcf1fc17c185d03f (approved; PSS 462.1 MB reproduces, type erasure behavior-preserving, fan-out cascade confirmed pre-existing). | e37a7f7, 232baff, c14a95b, 109d546 | `graph_check.py --target-ui9`; `measure_bmi.py`; gate package §F1 |
| 2026-09-30 | F2 | **F2 implemented — rows 4/6 registry inversions (commits a1a827d, f77774c; composition-root tests 2f3e77b).** Two atomic cuts, each dual-preset + serial-ctest green: (4) moved the doctor dialog renderer registration out of `cc.ui.dialogs.default_renderers` into a new screens-side module `cc.ui.screens.doctor_dialog_registration` (imports `cc.ui.dialogs.system` for `dsys::DialogRendererRegistry` — screens→dialogs downward-legal, rank 10 > 9; `default_renderers` drops the `doctor_screen` import, the `doctor_detail` holder, and the Doctor case), severing dialogs→screens. The composition root (`app_dialog_registration_default.cpp`, a `.cpp` impl unit so the frozen app inline-body ratchet at 29 is untouched) calls `register_doctor_renderer()` alongside `register_default_renderers()`. (6) added the foundation leaf `cc.ui.foundation.feature_dialog_protocol` (imports only std + textual ftxui, zero cc.* imports) defining `ViewKind{AgentWizard,PluginInstall,PluginTrust}`, neutral `FeatureWizardStep`/`FeatureWizardRequest`/`FeatureTrustRequest`/`TrustChoice`, and a `register/resolve_dialog_factory` registry (`DialogFactory = std::function<Component(std::shared_ptr<void>)>`, function-local static map). `agent_wizard` and `plugin_install_flow` dropped their `wizard_dialog`/`trust_dialog` imports and now build neutral requests resolved by ViewKind (returns Component() on a miss); a dialogs-side generic adapter `cc.ui.dialogs.feature_wizard_adapter` converts the neutral request to `wizard_dialog` props and calls the existing `WizardComponent` (imports only the protocol leaf — no features import); the composition root (`app_feature_dialog_registration.cpp`) registers the concrete factories and holds the ONLY `static_pointer_cast` of the erased requests (verified by grep). The neutral `TrustChoice` maps 1:1 to `td::TrustChoice` in the app impl unit (a choice added on one side without the other is a compile error there, not a silent fallthrough). `trust_dialog`'s own imports (`cc.plugins.plugin`, `cc.commands.plugin.plugin_trust`) are unchanged — downward-legal non-cc.ui-internal edges. Severed features→dialogs. **After F2: `--target-ui9` PASSES (exit 0) — 12 singleton cc.ui areas, 0 SCC-internal directions, 0 back directions; the declared 12-area total order is machine-enforced.** Baseline `ui_back_edge_baseline.txt` re-frozen to EMPTY [scc-internal]/[back]. Composition-root tests (2f3e77b, 6 tests in `tests/test_f2_composition_root.cpp`): doctor renderer absent from `register_default_renderers` alone but renders through the registry after `register_doctor_renderer`; AgentWizard/PluginTrust factories render through the protocol leaf; the adapter converts a neutral request; the miss path returns null. **ctest reconciled:** 1859 live (1853 + 6 new F2 tests) vs 1836 @1a83330 — the +17 pre-F2 delta is from RFC-0001 c20–c24 followups; F2's own test addition is +6. Review: cut4 agent:a39e71394c6b72976 (approved; major: composition-root test not delivered — remediated in 2f3e77b), cut6 agent:a5ff7e5eadf6cf7ea (approved; major: zero protocol-leaf test coverage — remediated in 2f3e77b). | a1a827d, f77774c, 2f3e77b | `graph_check.py --target-ui9`; `test_f2_composition_root.cpp`; gate package §F2 |
| 2026-09-30 | F3 | **F3 implemented — ReplScreenState sharding into 7 stores + AppAdapter resolution (commits 228f9ed, 7ff03ca, 0bea571, ac03716, 4b5447b, dd26c67, 8fc1fd5, cb3b8e8, ff09c7b).** Prep (228f9ed) added the `--store-lint` placement gate to `graph_check.py` — four rules over `src/ui/screens/*_store.cppm`: naming (declares `cc.ui.screens.<name>_store`), out-of-store (cc.ui.* imports only from areas ranked below screens, UI9_RANK < 10; no store imports app or another store), into-store (only app-area or screens-area modules import a store), threading (0 mutex/jthread/condition_variable) — with a negative test (`test_store_lint.py`, 6 assertions) and the AppAdapter body inventory (`0002-f3-appadapter-inventory.md`): the gate's "58" was stale — the RFC-0001 Phase-C followups re-froze `cc.ui.app.app` at 29, and the 29 live bodies are 1 (a) composition (`set_screen`) + 28 (c) test seams (all `*_for_testing`); the (b)/(d) bodies named in the gate were already moved out-of-line by Phase C. Seven stores then landed one per commit, each dual-preset + serial-ctest green with `--store-lint` PASS after each: MessagesStore (7ff03ca; messages vector, unseen-divider anchor, virtual-list handle, scroll/chrome state), PromptStore (0bea571; input mode, stashed prompt, paste buffers, suggestions), TaskViewStore (ac03716; spinner mode, task-notification footer counts, agent/teammate live state), PermissionStore (4b5447b; PermissionToolKind, PermissionRequestInfo, permission_request), DialogStore (dd26c67; DialogQueue + DialogRendererRegistry, overlay/inline panel handles), McpStatusStore (8fc1fd5; drained at-mention queue — the pending mutex + staging queue moved OUT to AppImpl, drained by `DrainPendingAtMentionInserts`), ChromeStore (cb3b8e8; StatusBarData, status bar, feed-content fields). `repl_state.cppm` held each store by value behind a re-export shim during migration; Finalize (ff09c7b) deleted all 7 shims (`repl_state` is now a 393-LOC thin composition facade), resolved AppAdapter 29→1 (28 test seams moved verbatim to the new impl unit `app_testing_seams.cpp`, textual-std per LLVM #184957; `set_screen` is the sole inline body), re-froze the inline ratchet at 1, and added direct store imports at every call site naming a store type (10 repl_screen impl units, 9 app impl units, 6 test files). **Measurements:** repl_state producer PSS 1853.7→1853.8 MB (flat within ~1 MB sampling noise, ≤ before — the no-regression check met); BMI bytes 17914648→17914400; stores are pure-data structs with 0 inline bodies, so no body-edit fan-out surface exists (the F3 fan-out=1 threshold is vacuously satisfied — if stores later gain accessors, their bodies must land in `.cpp` impl units). **ctest reconciled:** 1857/1857 passed (1859 discovered, 2 disabled — unchanged from F2; F3 made zero test-registration changes; the 1857-vs-1859 delta is discovered-vs-runnable, not a discrepancy). All gates green: debug+release builds `-Werror` clean, `ctest -j1` 1857/1857, `graph_check` default OK, `--target-ui9` PASS (12 singleton SCCs, 0 back directions), `--store-lint` PASS (7 stores, 4 rules), `inline_def_check` OK (app.app frozen at 1), 0 shims, 0 threading primitives in stores. Review: 9/9 approved — prep agent:ac323b69fd310ecac, messages agent:af434068fa5933a05, prompt agent:a4684f02e7b2cd49a, task-view agent:a241a46c78fb8d15b, permission agent:af41ce8c122163d52, dialog agent:a4fb1a001b6d43906, mcp-status agent:a8416efa97e1d97eb, chrome agent:aae4073c16ce0cda9, finalize agent:a3747f663ae7cfb62. Minor findings: `task_view_store.cppm` header-comment filename typo (fixed); `test_store_lint.py` ~12% flake reported by the chrome reviewer under the 18-agent concurrent-build load — non-reproducing on a quiet box (500/500 focused temp-tree scenario-runs + 7/7 full runs; the lint is deterministic — no cache, no global mutation, `/tmp` not symlinked — so the flake was load-induced IO pressure), and `_is_store_path` was hardened to resolve both sides of the `relative_to` (a symlinked SRC could silently fail-open a store on macOS-style `/tmp`); the at-mention staging/drain path has no direct automated test (pre-existing gap — the old `DrainPendingAtMentionInserts` was also untested; not introduced by F3); transient clang frontend SIGABRT/Bus-error flakes during the workflow (documented Homebrew LLVM 22 instability — serial rebuilds green). | 228f9ed, 7ff03ca, 0bea571, ac03716, 4b5447b, dd26c67, 8fc1fd5, cb3b8e8, ff09c7b | `graph_check.py --store-lint`; `test_store_lint.py`; `0002-f3-appadapter-inventory.md`; gate package §F3 |
| 2026-09-30 | F4 | **F4 implemented — cc_ui split into 12 area libraries + INTERFACE aggregate (commits 53fcaf1, a468b46, d2e6cb2, 4000834, 3452b25, 6463687, e7b3574, f752606, eb9a3b3, 1885203, 4cbcf3e, d68c943; finalize this commit).** The single cc_ui target (one FILE_SET, ~217 ui modules) was split into twelve cc_ui_<area> libraries grouped by MODULE-NAME area (cc.ui.<area>.*), included in UI9_RANK dependency order (visual/tools rank 1 → app rank 11); cc_ui is now a source-less INTERFACE aggregate linking the 12 area libs + non-cc.ui deps PUBLIC, so loom/cc_server/tests keep a single cc_ui link edge. Each area owns its CXX.dd dyndep, bounding body-edit fan-out. The --tll-lint gate (53fcaf1) parses target_link_libraries, runs Tarjan (0 SCCs) and enforces file->lib grouping (every cc.ui.<area>.* module in exactly cc_ui_<area>; 0 cross-area ownership; 0 files in the aggregate). Finalize: cc_ui.cmake stale "during the staged split" comments removed (the split is complete); test_tll_lint.py case 1 updated from pre-split expectations (vacuous grouping, 0 area targets, 15 cc_ui deps) to post-split (12 area targets, 27 aggregate deps); CLAUDE.md build-layout note + layout-table row updated to the aggregate reality; the CLAUDE.md documentation-impact checkbox checked. **Measurements:** fan-out — a body edit in visual/messages/app recompiles exactly 1 object each (real build: cc_ui_visual/cc_ui_messages/cc_ui_app only; ninja -n dry-run over-reports 211 due to the pre-existing CMake dyndep cascade documented in the F1 row — the real-build count is the true fan-out, and each is ≤ the area's own object count, never the closure); PSS — heaviest producer app.cppm peak 2534.9 MB ≤ 3,222 MB baseline (no regression; F4 changes no per-TU compiles); ctest 1857/1857 serial, 0 failed; 56 goldens byte-identical (ctest-covered); lints — graph_check default OK, --target-ui9 PASS (12 singletons, 0 back), --store-lint PASS (7 stores), --tll-lint PASS (56 targets, 0 SCCs, 0 grouping violations), inline_def_check OK; dual-preset -Werror clean (debug + release). Cold-build wall time: 441s (no pre-F4 baseline recorded on this box; the split changes no compile work — same TUs/flags, +12 archive steps — so cold-build cannot regress beyond noise). | 53fcaf1, a468b46, d2e6cb2, 4000834, 3452b25, 6463687, e7b3574, f752606, eb9a3b3, 1885203, 4cbcf3e, d68c943 | graph_check --tll-lint; measure_bmi.py; ninja dry-run + real build; gate package §F4 |
