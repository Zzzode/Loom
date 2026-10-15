---
rfc: 17
title: Editable Question Panel
status: implemented
owners: "@Zzzode"
reviewers:
  - "agent:osc_design_review (independent design and PRR)"
  - "Claude Code CLI (supplementary design and implementation review)"
created: 2026-10-14
last-reviewed: 2026-10-15
tracking: local
---

# RFC 0017 — Editable Question Panel

## Summary

Complete the actual ask-user question panel with a persistent native FTXUI
editor. A default answer becomes editable initial text; Enter returns the
current string (including an intentionally empty answer), while Escape returns
cancellation. Clear only the question panel's allocated pixels before painting
so underlying prompt separators cannot show through its word gaps or padding.

## Motivation

### Evidence

| Measurement | Current | Target |
|---|---|---|
| Actual question edit component | None: default renderer displays default_value; handler accepts only Return/Escape | One persistent native Input per payload |
| Actual Return without a default | nullopt, incorrectly indistinguishable from cancellation | Present empty string |
| Actual production question body | PTY recorded PTY─status─question instead of PTY status question during RFC 0016 acceptance | Exact word spaces, no underlying chrome |
| Existing reusable standalone prompt | No production consumers; byte-wise input/deletion | Do not route production through this unsafe editor |
| Native CTest discovery | 2666, including unrelated concurrent branding tests | Account for every addition and deletion |

The live path is App ask-user responder → dialog_system::PromptDialogPayload →
default_renderers::RenderPromptDialog/HandlePromptDialogEvent. The separate
prompt_dialog::MakePromptDialog is not on this path. The actual question layer
is dbox-composed over base chrome in LayerAllDialogs without clearing its box.
Paragraph paints words but leaves gaps transparent. Permission panels already
apply native clear_under for the same composition model. Changing text color,
adding background color or rewriting wrapping cannot erase existing characters.

## Goals

- G1. Edit actual ask-user answers with UTF-8 character insertion, native
  glyph-aware cursor movement, Home/End, Backspace and Delete.
- G2. Preserve the buffer/cursor through Render, resize, temporary suppression
  and higher-priority dialogs. Initialize defaults exactly once; each queued
  question owns its state. First input may precede first Render.
- G3. Return submits current text once, including empty text. Escape cancels
  once. Ctrl-C/Ctrl-D retain existing App interrupt/exit ownership.
- G4. Question text, input and hints are legible with exact spaces at 40/80/120
  columns. Question rendering clears its own rectangle, without changing
  other dialogs, the terminal canvas or the UI theme.
- G5. Real PTY exercises actual permissions, question editing, successful tool
  results, cancellation and OSC 7501 transitions without REPL input leakage.

## Non-Goals

No new stdin reader, render ticker, terminal protocol, permissions, settings,
wire/schema/session format, clipboard backend, history/suggestions, standalone
prompt component rewrite, global dialog-frame redesign or unrelated branding.
The answer is a single logical line with horizontal cursor scrolling; adding
multiline composition and a general bracketed-paste subsystem is out of scope.

## Proposal

### Ownership and shared-change gate

The owning area is ui/dialogs. Add defaulted mutable ephemeral type-erased ui_state to
its existing PromptDialogPayload, following ToolPermissionPayload. It is not
serialized. Keep its editor details in a new implementation unit of the
existing default_renderers module; replace the two inline question bodies with
declarations. Register and dispatch through existing entry points. Add the
implementation source to loom_ui_dialogs; no new module or dependency area.

A private question state owns string, byte cursor, native FTXUI Input and a
resolved guard. Native Input handles UTF-8 glyph navigation/deletion and whole
Character batches. Do not copy its editing algorithm or instantiate components
on each render. The unused standalone prompt's byte editor and the full REPL
editor (suggestions/history/modes) are respectively unsafe and excessive.

Payload presentation storage is the smallest necessary shared addition:
renderer-local temporary state loses edits; a registry-global cache would
conflate questions or require eviction and IDs; storing a concrete component
in the shared payload would leak the editing implementation. Existing payload
producers are the App responder and tests, and need no changes. Defaulted
storage preserves existing construction and callback signatures. Permission,
model request and session contracts remain intact.

### Input and completion

Initialize value from default_value.value_or("") and cursor at its end once.
Apply the same normalization to this initial value and every Character batch:
CRLF becomes one space; lone CR/LF, Tab and Unicode U+2028/U+2029 become spaces;
other C0 bytes and DEL are removed. Do not remove bytes 0x80..0x9F, which can be
UTF-8 continuation bytes. Preserve ordinary Unicode and single-line defaults.
Ensure state in both Render and HandleEvent so early keys work. Editing never
writes REPL input_text or is_prompt_input_active, which would suppress the
question. Return returns the current value; deleting a default and submitting
must not restore the original. Escape returns nullopt. Set the one-shot guard
before invoking a copied callback/value, because the callback may remove the
payload. Do not capture shared state strongly from callbacks owned by Input.
Persist lazy state even on a const Render call through the mutable presentation
field; no const_cast or temporary-only initialization. After the guard fires,
consume delayed keyboard input without further mutation/callbacks until removal;
the reserved Ctrl-C/Ctrl-D/Ctrl-L upper-layer events remain exempt.

Use single-line Input and retain its native horizontal frame. Consume editing
keys even at buffer boundaries so they cannot fall through to the main prompt.
Tab/Shift-Tab do not insert or leak. Control characters are not answer text;
Ctrl-C/Ctrl-D and Ctrl-L remain upper-layer interrupt/exit/redraw events.
Printable Character batches pass to native Input intact; line separators in a
batch are folded to spaces for this single-line editor. No control bytes can
enter the answer. Existing App shortcut precedence and dialog suppression
remain unchanged; do not introduce a second clipboard/paste pipeline.

### Rendering and overlay correctness

Render the held Input with current palette text/muted styles, one visible
input row, native cursor and native horizontal scrolling.
Native FTXUI 5 ComponentBase::Active returns true for a parentless component;
Focused walks its ancestors and returns Focusable, which Input implements as
true. The held Input therefore already owns native focus when rendered;
TakeFocus has no ancestors to change and is unnecessary. Default renderers
only render the currently visible payload, so suppressed panels do not paint
a cursor. Verify Screen.cursor has BarBlinking shape on the input row.
FTXUI 5's empty-content branch renders its placeholder through focus, which
sets a hidden cursor. The question's existing InputOption transform adds
focusCursorBarBlinking only for a focused placeholder; this makes the empty
answer's caret visible at the same input origin without changing FTXUI or its
editing behavior. Verify empty, typed and cleared-default cursor states.
The Input element is xflex inside the prefix/input hbox, itself one row high;
DialogFrame/terminal allocation bounds its width. Test that the native frame
keeps the cursor and the long-answer tail visible at 40 columns and on resize.
Compose question,
input and the existing Enter/Escape hints through DialogFrame. Apply
clear_under at the returned question element only, so FTXUI clears exactly the
allocated panel box before drawing its descendants. This restores transparent
word gaps to actual blanks and preserves the underlying transcript outside
the question. Do not change shared DialogFrame, ThinDivider or LayerAllDialogs.

The default value is visible as editable text, not a second bracketed display.
The returned DOM also retains that same state through a small private owner
node, following the existing ComponentHolderNode pattern. Native Input's
rendered reflect nodes refer to component-owned boxes and its buffer references
state storage; retaining only the component or only the payload is insufficient
if the payload is removed before a returned Element is rendered. This is a
local lifetime owner, not a new shared component abstraction or cache.
The native Input remains alive through payload-owned state even when the
question is not currently focused or visible. No render-time callbacks or
terminal writes are introduced.

The handler intercepts Return/Escape and copies callback/value before invocation;
InputOption on_enter/on_change remain no-ops. Other editing events go to the
held input. Mouse events are forwarded to Input and may fall through outside
its box; existing App mouse-selection precedence is unchanged. The mutable
presentation cache preserves the const RenderPromptDialog signature. Golden
rendering uses the existing normalization harness and a fixed theme; native
cursor state is asserted separately from deterministic screen pixels.

### Compatibility and observability

Present-string versus nullopt is the existing responder contract: runtime
ask_user_question returns a successful tool result for any present string and
an error/cancellation for nullopt. This implementation repairs the missing
editor rather than changing that contract. Existing defaults still submit
unchanged if they are ordinary single-line text and no edit occurs. Defaults
containing line separators or control bytes use the normalization rule above.
Arbitrary typed answers now reach that same tool
result and existing messages.jsonl/dump-prompts traces. OSC reporting remains
blocked:question until submission/cancellation; no protocol changes.

Native golden impact is local to real question rendering. The old
Golden_PromptDialog test registers a substitute renderer and cannot count as
production coverage. Replace it with default registration and update only its
question golden after inspection; preserve unrelated dirty branding snapshots.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Design gate | RFC, root evidence and agent design/PRR review | done | Independent design/PRR APPROVE; normalized defaults/batches, mutable state and DOM ownership pinned; native pre-fix PTY exact-space case failed and raw frame retained; RFC lint passes |
| B | Editable question | Payload presentation storage, held native editor, local clear-under, native regression tests | done | Final debug/release focused41/41 each include19 native question cases, real-renderer golden,7 question PTY registrations and14 OSC registrations; empty/typed/cleared-default cursor states pass; independent local cursor-delta reviews APPROVE |
| C | Native terminal graduation | Actual Loom PTYs, dual presets and docs | done | Both isolated full serial suites: 2668 passed, 20 existing skips, 2 existing disabled, 0 failures each; 10 real question sessions per preset; source/binary hashes stable; independent final evidence APPROVE |

## Production Readiness Review

- Correctness: exercise real default registration, not the unused standalone
  prompt. Native tests cover empty versus nullopt, one-shot callback/lifetime,
  UTF-8 glyph operations, normalized control-bearing/multiline defaults and
  batches, edit boundaries, multiple queued requests, repeated
  Render/resize and suppression. Real PTY checks actual model-request tool
  result, exact visible spaces, no REPL leakage, and OSC lifecycle.
- Build: one implementation unit, declarations replace inline bodies. No new
  module, exported editor type, dependency area, thread or concurrency cap.
  Run graph/inline ratchets and both preset builds at default Ninja parallelism.
- Metrics: measure local build/test wall time and discovery deltas. Local
  macOS cannot provide /proc PSS or hosted macos-14 cold CI measurements;
  explicitly report those limits and do not substitute RSS.
- Compatibility: only defaulted in-memory presentation storage is added.
  Defaults, cancellation, permission/exit rules, config, persisted and model
  formats remain stable. No migration or feature flag is needed.
- Lifecycle: payload owns the editor; no shared-pointer callback cycle; guard
  callbacks before invocation. Existing worker cancellation/terminal handoff
  stays unchanged. No asynchronous work is introduced.
- Docs: update terminal-status user guidance (default confirmation limitation
  becomes editable answers), decision catalog and index. No new AGENTS rules.
- Platform: real macOS forkpty acceptance is required. Linux/Windows and an
  installed terminal application's GUI are not inferred from local PTY tests.
- Rollback: revert question implementation, payload field, registration target,
  local tests/docs together. No persisted migration or dependency patch.

## Rollout and rollback

Proceed after agent design/PRR approval; keep all changes uncommitted as
requested. Capture existing dirty files and preserve branding hunks in shared
E2E files. Reproduce the question-space failure before fixing production.
Freeze source/tests before dual full suites. No public release or push.

## Drawbacks

One native component and small transient state object per question add bounded
presentation state. Single-line editing scrolls horizontally, so long answers
are not entirely visible at once. Editing defaults changes user-visible
behavior from a confirmation-only panel to an editor, intentionally.

## Alternatives considered

1. Keep confirmation-only: does not satisfy user answers or fix transparent
   overlay holes.
2. Route through standalone MakePromptDialog: no production integration and
   byte-wise editing is not UTF-8 safe; fixing that unrelated SELECT component
   would widen scope.
3. Use the full REPL text editor: brings history, modes and suggestions into
   a small dialog and risks changing focus/suppression behavior.
4. Rewrite shared dialog/frame/layout opacity: affects every dialog while the
   requirement can be closed at the question renderer. Local clear_under is
   the existing native overlay pattern and smallest correction.

## Testing and verification plan

Add native default-renderer tests for input and dbox-over-underlay rendering,
with default, empty, cancel, repeated decisions, early events, independent
payloads, UTF-8 batches, editing boundaries and render/resize retention.
Extend actual App/ReplScreen dispatch coverage to prove answers do not enter
REPL input and suppressed dialogs do not consume edits. Keep the legacy
standalone prompt tests unchanged as separate compatibility coverage.

Extend the existing E2E PTY executable with actual ask_user_question responses,
strict tool_use_id/is_error/content assertions and first-frame exact-space
checks at 40/80/120 columns. Assert editing does not submit prematurely, buffers
survive resize, deleting defaults submits empty, Escape returns cancellation,
and OSC state remains blocked:question until the answer resolves. Preserve
raw terminal evidence. Reuse existing two real permission gates without
altering approval policy. Run native focused checks first, then debug/release
full serial CTest on frozen source. Do not weaken unrelated clipboard or HUP
coverage when an independent flake occurs. Native discovery starts at 2666;
record precise additions/deletions when tests are written.

## Validation results

The completed gate uses committed baseline
3cfa239149bb197d2efbae0cac00e22a19ed99ba plus only this question feature in the
managed question-panel-validation worktree. It excludes concurrent uncommitted
branding changes. No source or test assertion was weakened to obtain a pass.

| Gate | Debug | Release |
|---|---|---|
| Cold build, -Werror | 140.23 s, pass | 253.06 s, pass |
| Focused native/golden/PTY | 41/41, 25.57 s | 41/41, 24.09 s |
| Full serial CTest | 2668 passed, 20 skipped, 2 disabled, 0 failures; 441.60 s | 2668 passed, 20 skipped, 2 disabled, 0 failures; 390.09 s |
| Actual question sessions | 10, exact answers and first-frame spaces | 10, exact answers and first-frame spaces |
| Source/fixture and binary drift | None | None |

Discovery is 2664 committed baseline tests plus 26 new question tests: 2690.
Graph checks pass with 637 modules / 796 units and no cycles, upward edges or
dead imports; 12 UI areas, 8 stores and 59 targets remain valid. Inline checks
pass: 638 interfaces, 49 frozen, none over 100. Independent final code/evidence
reviewer agent:osc_design_review approved graduation; supplementary Claude Code
CLI design/implementation and empty-cursor reviews also approved.

Validation covers local macOS real PTYs. Linux, Windows, an installed terminal
GUI, /proc PSS and hosted cold CI were not verified. It does not certify the
changing branding combination in the main checkout. The origin of the earlier
unrelated subprocess termination remains unknown; five isolated repetitions
and both final full suites passed without changing that test or its business
code. Raw diagnostic and terminal evidence is retained. All feature changes
remain uncommitted; no push was performed.

## Documentation impact

- docs/README.md: index RFC 0017 and update terminal-status guide description.
- docs/user/terminal-status.md: default prefill, editing, Enter and Esc behavior.
- docs/decisions/design-decisions.md: held editor and local overlay clearing.
- Existing RFC 0016 remains a historical record of default-only acceptance.

## Open questions

None. The validation scope and remaining limitations are recorded above.

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-14 | A | RFC drafted; no production edits | local, uncommitted | Input path verified by independent explorer; actual renderer accepts no edits. Prior real PTY captured separator showing through word gaps; source LayerAllDialogs uses transparent dbox and production question lacks clear_under. Baseline 2666 registrations and 2566 source/build/test hashes captured; unrelated branding retained. |
| 2026-10-14 | A | Design/PRR requested changes, then APPROVE | agent:osc_design_review | One P2 closed: defaults and Character batches share explicit single-line/control normalization while preserving UTF-8. Mutable first-render state, post-decision inertness and DOM-owned lifetime accepted. Native pre-fix E2E_Pty_Question_layout failed in 1.37s: first 40-column frame has PTY─exact─question─words and underlying main-input text. No production changes preceded approval. Supplementary Claude artifact review runs independently. |
| 2026-10-14 | B | Implementation starts | local, uncommitted | Main owns dialogs implementation/CMake/docs; native worker owns 19 question regressions and one real-renderer golden; PTY worker owns 7 new registrations/10 live sessions. Only main builds/runs; preserve unrelated dirty branding. |
| 2026-10-14 | B | Native implementation and independent static review pass | agent:osc_design_review | Debug 19 question tests plus actual-renderer golden 20/20 in 0.49s; release focused native/golden/PTY 41/41 in 24.10s. Cursor test corrected UTF-8 byte offsets to display columns without changing production. Graph 637 modules/796 units, zero cycles/upward edges/dead imports, 12 singleton UI areas, 8 stores/59 targets; inline 638 interfaces/49 frozen/0 over100. |
| 2026-10-14 | C | Real terminal regression passes; full suites start | local, uncommitted | Debug question/OSC 21/21 in 37.43s. Exact first-frame spaces at 40/80/120 columns now pass against the previously failing case; real model requests contain edited/default/empty/cancel outcomes and restored terminal state. Discovery 2666 to 2694: this feature adds 26, unrelated concurrent branding adds 3/removes 1. Question source/tests remain frozen. The first full debug attempt was discarded when concurrent branding rebuilt the binaries. Both presets were rechecked with no work pending before the fresh full run; discovery includes the unrelated branding delta. Branding snapshot refreshes completed while the fresh run was still below test800, before their UI consumers starting at test1385; those changes were captured separately, with no question code changes. |
| 2026-10-14 | B | Supplementary Claude design/code review APPROVE | Claude Code CLI | Earlier focus P1 source-rebutted: parentless Input is active/focusable; width P1 closed with native viewport/cursor assertions. Both presets now pass the pending cursor/runtime gates. A non-blocking placeholder observation was source-rebutted: vendored input.cpp lines109-118 has an explicit empty-content placeholder path with is_placeholder=true; no workaround needed. The review's native-count typo is corrected here:19 new panel cases, plus1 existing golden (20 focused), and7 new PTY registrations. |
| 2026-10-14 | B | Empty-input cursor gap reproduced and corrected locally | local, uncommitted | Follow-up source inspection found placeholder focus sets Hidden cursor. Two existing native tests extended without new registrations; pre-fix both fail with Hidden0 versus BarBlinking5 at40/80/120 and after clearing a default. Existing InputOption transform now adds the native cursor decorator only for focused placeholders. Second full debug attempt was stopped at1601 before graduation; both full suites restart after final code/test freeze. |
| 2026-10-14 | B | Final cursor delta approved; stable focused gates pass | agent:osc_design_review and Claude Code CLI | Debug body-delta build52.64s, release150.28s, -Werror clean. During the release LTO relink an intermediate focused run had4 startup timeouts; all4 failed-session raw traces contain only71-byte skills initialization, no UI install or OSC probe (layout failed in its80-column child after40 passed). No timeout or assertion changed. After relinking, full41-case serial runs pass: debug23.40s/release23.68s. Frozen2567 source/build/test files and106 executable hashes per preset precede the final full serial runs; no concurrent build remains. |
| 2026-10-15 | C | Shared-checkout run retained as diagnostic, not graduation | local, uncommitted | Further concurrent branding changes invalidated the whole-source freeze. Completed debug run:2641 passed,20 skipped,2 disabled,30 failed (29 streaming snapshots and1 welcome fixture),1 subprocess killed. Follow-up discovery of the same feature-test executable was also killed before executing test bodies; no evidence proves OOM, kill sender or security-scanner termination. Original failure and system-log evidence retained. These runs do not count as green; no timeout, assertion or unrelated production code was changed. |
| 2026-10-15 | C | Isolate the authorized feature for reproducible graduation | agent:osc_pty_tests (patch extraction), main integration | Managed worktree question-panel-validation starts at3cfa239149bb197d2efbae0cac00e22a19ed99ba. Copy12 feature-only files; apply a2-file question-only E2E patch independently checked against that base. No branding hunks/strings included; original shared files unchanged by extraction. Clone6 pinned offline dependency sources; freeze2567 source/build/test files. Build both presets in independent directories, then run the native/PTY/full serial gates. The main worktree and unrelated branding remain untouched. |
| 2026-10-15 | C | Isolated debug build and focused checks pass | local, uncommitted | Cold debug build140.23s, -Werror clean;41 focused native/golden/PTY cases pass in25.57s. The unrelated teammate-permission case passes5 consecutive isolated repetitions in10.68s without code changes. Its earlier process-termination origin remains unproved; do not label it OOM or a question-path defect. Isolated discovery2691 equals committed-base2665 plus26 new question cases; concurrent branding tests are intentionally absent. |
| 2026-10-15 | C | Correct isolated discovery arithmetic against actual names | local, uncommitted | Actual ctest -N reports2690, correcting the preceding2691 estimate. Compare original2666-name baseline: exactly26 question additions and2 absent uncommitted branding cases (WelcomeMonogram and LargeMarkDoublesCompactPixelsAndColors). Therefore the committed base has2664 and feature-only discovery is2690; no committed test was removed. |
| 2026-10-15 | C | Isolated release and final focused gates pass | agent:osc_design_review (isolation/scope APPROVE) | Cold release build253.06s, -Werror clean; focused41/41 pass in24.09s. Independent review confirms question production/native/golden bytes and PTY question logic match the main checkout exactly; no branding included. Frozen2567 source/build/fixture files and104 executable hashes per preset; both discover2690. Full debug then release run serially on that fixed feature-only baseline; these do not certify the changing branding combination in the main checkout. |
| 2026-10-15 | C | Isolated full debug serial gate passes | local, uncommitted | 2690 registrations:2668 passed,20 existing skips,2 existing disabled,0 failures in441.60s. New question cases and all actual question/OSC PTYs pass. All2567 source/fixture hashes and both104-executable manifests remain unchanged. Release full serial starts on the same frozen feature-only source. |
| 2026-10-15 | C | Full isolated release and graduation APPROVE | agent:osc_design_review | Release 2690 registrations:2668 passed,20 existing skips,2 existing disabled,0 failures in390.09s. Debug same totals in441.60s. Independent reviewer recomputed2567 source/fixture and104 binaries per preset: zero drift. Both presets include10 real question sessions,19 native cases and all question/OSC registrations. Graph/inline pass. Phase C done; RFC implemented. Sync final docs and lint only; no code changes, commits or pushes. |
