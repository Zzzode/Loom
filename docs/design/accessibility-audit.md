# Reduced motion and text contrast audit

This audit follows RFCs 0013 and 0015 and records the remaining accessibility
repairs against `42a5d172` on 2026-10-16. These are fixes to existing renderer
contracts, not a new animation, theme or configuration system. Automatic theme
selection is a separate documentation-only proposal in RFC 0019.

## Acceptance boundary

- The existing global `Theme::a11y.reduced_motion` suppresses decorative glyph
  cycling and blink in active and independently callable UI renderers. Local
  animation suppression still wins. Progress, elapsed time, operation status,
  content updates, focus and user input continue normally.
- Ordinary affected truecolor text reaches 4.5:1 on its declared filled
  background, including hover, selection, gutter and diff-word surfaces where
  applicable. Default transparent rows use the palette canvas as a reference,
  without certifying the user's actual transparent terminal background.
- Named syntax themes retain their names and semantic colors, with readable
  token/background pairs. They are independent palettes, not aliases of the
  current UI theme. A named foreground must not silently inherit the opposite
  UI theme's filled background.
- Terminal SGR2 dim is not numerically predictable. Ordinary instructions and
  operational feedback use readable ink without dim. Deliberately historical,
  completed or decorative elements and user-supplied ANSI are classified
  separately instead of being silently restyled.

## Motion ownership

The main work spinner, component primitive, shared SpinnerElement and logo
already implement reduced motion or are static. Existing public frame lookup
tables remain pure data. Missing glyph/attribute gates are fixed in their owning renderers. Native
FTXUI cursor shape is normalized after child rendering by the existing
application-root CursorResetNode: all three blinking shapes become steady,
while coordinates, hidden cursors and existing steady shapes remain unchanged.
Standalone upstream FTXUI Input is not patched or given a new option.

| Renderer family | Missing behavior | Repair boundary |
|---|---|---|
| Actual tool payload and compressed chain | Frame-based status-dot disappearance | Tool/chain renderer; preserve running/result/disclosure data |
| Thinking and legacy message envelope | Cycling braille glyph; terminal blink | Local glyph/attribute choice; retain duration and state |
| Hook progress | Cycling header and blink attribute | Held component render; hook updates still apply |
| Agent status dot/card/badge and labeled spinner | Direct array indexing | Existing shared spinner-frame helper; named states preserved |
| Doctor and MCP resolution | Local frame lookup | Local glyph helper; progress remains live |
| Assistant stream, Vim, resume/log/plugin/trust inputs | Pixel blink or manually hidden caret | Disable blink only; retain glyph, focus and input ownership |
| Native Input and queued Question | Blinking Screen::Cursor shape, separate from pixel attributes | Existing application-root cursor_reset post-pass; preserve child-selected x/y and focus |

Native tests compare complete FTXUI pixel attributes across caller frames,
check actual message-list payload and chain composition, and verify that
normal animation resumes with the flag off. Terminal-driven blink is checked
as an emitted pixel attribute rather than inferred from a screenshot.

## Contrast ownership and audit ledger

The baseline inventory contains 1302 dim-related matching lines, not 1302
defects. Callers, state and actual filled backgrounds determine whether a
match is ordinary current text, deliberate hierarchy or dormant code. Existing
RFC 0013/0015 coverage remains valid; this pass does not claim whole-UI
certification from a source search.

| Active family | Finding and repair | Native evidence |
|---|---|---|
| Default syntax and diff hunk headings | Light info was 4.039:1 on its canvas and 3.662:1 on its info tint; LightDaltonized was 4.294:1 / 3.894:1. Calibrate the existing info token within its blue family. | Four RGB variants, token/gutter/row/word/hunk pairs; seven-theme structural checks. |
| Named code themes | Dark-derived ANSI colors and faint comments/gutters failed on declared RGB surfaces; named Light also inherited unsuitable Dark foregrounds. Complete the five existing named palettes with readable RGB pairs. | All fourteen token types on normal/add/remove surfaces and gutter ink on its actual fill. |
| Explicit Markdown and named diff | Explicit code foregrounds inherited the opposite UI canvas; named diff headings used a global tint. Paint an explicitly requested code theme's own background. Give the existing diff theme local optional context and hunk-header backgrounds. | Opposite-global-theme rendering, unknown-name Dark fallback, default transparency and caller-owned context/selection tests. |
| Footer and plugin UI | Required shortcuts, current metadata, errors and recovery instructions inherited terminal-dependent dim; IDE filename ink was 3.638:1 on Light. Use readable neutral or existing semantic ink without dim. | Actual footer, queued notification, IDE, plugin dialog/manage/settings and all five installation steps. |
| Agent cards and wizard | Current tags, descriptions, tools, status and navigation were dim; selected Mono cards/name fields had identical foreground/background indices. Remove informational dim and substitute the existing message-actions fill only for the colliding Mono states. | Actual normal/selected Card, MiniCard, LargeCard, timeline and four wizard stages across seven themes. All eight resolved avatar hues pass the RGB matrix unchanged. |
| Permission rules and scope editor | Current editable metadata and path badges inherited dim; several chip/toolbar/ANSI status fills collided or missed 4.5:1. Use existing readable ink and local fills. | Normal/hover/selected/disabled rows, policy/picker/toolbar states, all five scope badges and selected inversion. |
| Workspace trust | Risk details and consent instructions inherited dim. Remove informational dim while retaining disabled Allow, countdown and typed-confirmation semantics. | Actual full trust dialog, expanded risk details, critical confirmation and disabled actions. |
| Settings | Enabled descriptions, current MCP metadata and navigation inherited dim; selected Mono MCP rows had identical foreground/background indices. Use readable text and a local Mono fill. | Actual settings dialog, sidebar, dirty/error states, enabled/disabled rows and selected MCP metadata. |
| Doctor | Running/queued/result diagnosis, expanded reasons, environment metadata and active-lock instructions inherited dim. Remove informational dim without changing check execution. | Actual DoctorScreen running, results and summary renderers. |
| Dialog frame and actual MCP elicitation | Shared subtitles and the last-registered faithful renderer's request explanation, trace ID and cancel instruction inherited dim. Use readable text/muted ink without changing y/n/Esc behavior. | Seven-theme panel/modal frame and default-then-faithful registry tests, named/empty server, request data and callbacks. |
| Actual /resume session picker | Session age, message count, cwd, search/count/pagination and empty-state text inherited dim. Use muted ink and preserve selection/event behavior. | Seven-theme default-registry picker, selected/normal fill, filtering, visible upper pagination, empty state and Enter/Esc callbacks; lower marker ink is source-audited only. |

The Trust path is `get_trust_dialog` → `MakeWorkspaceTrustDialog` →
`RenderTrustDialogFull`. Settings commands select `SettingsView` →
`get_settings_component` → `MakeSettingsDialog`. Doctor's `UI:doctor` command
is handled by its dialog registration and `DoctorScreen`. AgentWizard and the
five plugin installation stages have connected production factories. These
paths are distinguished from similarly named legacy renderers.
In particular, AppAdapter registers `RegisterElicitationDialog` after all
default registrations: `elicitation_dialog.cppm` is active. The initial
inventory's dormant classification was incorrect and was withdrawn during
the final independent caller review; a passing default-renderer golden does
not prove coverage of this override.

Agent-card repairs also make an existing deliberate semantic choice explicit:
the permissions heading uses calibrated info ink, permission-mode/tool-count
metadata uses neutral muted ink, and enabled bordered action labels use text
ink while their border keeps the action hue. These are foreground repairs,
not merely removal of dim; labels, actions and state are unchanged.

### Preserved and excluded paths

- Disabled actions, completed/Idle/Disabled timeline history and decorative
  separators retain intentional dim. Current editable disabled rule data stays
  readable and retains its disabled-state glyphs.
- Direct subtle text remains in dormant Grove/Passes helpers; their builders
  have no production constructors. Motion guards also cover independently
  callable helpers, but contrast claims are limited to documented active
  paths. Registration precedence, rather than a filename or a lack of direct
  calls to the renderer, determines whether a dialog is active. MCP dialogs,
  the old prompt dialog, resume_screen and log_selector have no production
  imports/callers; active prompt is dialog_question, active resume is
  session_picker. Frame WorkerBadge helpers have only test constructors;
  their dim is not a current visible identity path.
- Explicit user ANSI/literal colors and custom status commands remain
  authoritative. There is no global recoloring or contrast engine.
- Selected-line diff helper combinations without actual production setters
  are not a certificate for arbitrary syntax-token/surface cross-products.
  Existing default selection and caller context contracts are tested.
- `RenderNotifications` has a separate pre-existing fallthrough defect: absent
  IDE returns a non-null empty element, so later overage/verbose fallbacks are
  unreachable. Their source ink was corrected, but no pixel coverage is
  claimed for these branches. Queue/dynamic/IDE paths are tested. Notification
  priority and reachability are unchanged and need a separate behavior fix.
- The existing SessionPicker height budget omits part of the surrounding frame
  overhead and clips its lower pagination marker at the tested 24 rows. The
  lower branch now uses muted/no-dim ink by source inspection, but has no
  visible-pixel acceptance claim. Independent review accepted deferring that
  separate geometry repair; visible upper pagination remains tested.
- The fixed-width Trust frame already clips the last character of its footer
  suffix in the tested size. Tests check the visible instruction; this pass
  does not claim to fix that layout defect.

## Independent review disposition

P1 means a required correctness/coverage repair before acceptance; P2 marks a
bounded regression or evidence gap. Both independent source reviewers approved
(`agent:grep_review`, `agent:claude-cli`). The final independent evidence
review closed the scoped gate using the complete suites plus the documented
unchanged retries; it does not certify a first-pass zero-failure full run.

| Finding | Evidence and impact | Resolution |
|---|---|---|
| P1 native cursor blink bypasses pixel gates | FTXUI Input chooses Screen::Cursor independently of Pixel::blink. | Existing root CursorResetNode normalizes only three blinking shapes after child rendering; four native cursor cases preserve position, Unicode editing and dispatch. |
| P1 final MCP registration was misclassified | AppAdapter registers faithful elicitation last; default-renderer goldens use another path. | Fix faithful renderer and shared subtitles; test the real default-then-faithful ordering and callback semantics. Withdraw initial dormant claim. |
| P1 active resume metadata still dim | /resume pushes the default-registry SessionPicker; age/count/cwd distinguish current choices. | Local muted/no-dim repair, selected-background matrix and filter/empty/Enter/Esc regression. |
| P2 selected path badge and actual role surfaces | Helper-only tests did not establish the selected Allowlist/card caller. | Assert actual selected [R] ink/inversion and actual Card/LargeCard role text. |
| Native Mono failures | Selected card/name/MCP fills reused the foreground index. | Substitute existing message-actions fill only in those Mono states; all other variants preserved. |
| P2 lower picker pagination pixel claim | Native 24-row frame clips the lower marker due to existing height arithmetic. | Reviewer accepted the evidence-based rebuttal: source-only ink audit, visible upper proof, documented separate layout defect; no artificial test dimensions or layout scope expansion. |
| Golden review | Eight refreshed fixtures affected by calibrated info and shared subtitle dim removal. | Independent per-character SGR review approved unchanged text/layout/background/other attributes; preserve fixed-width spaces. |

## Historical verification and limits (2026-10-16)

The results below belong to the earlier `42a5d172` isolated verification.
They are retained as historical evidence, including first-run failures and
unchanged retries, and do not certify the current `1056e07b` integration.
Current verification and publication authorization are recorded separately below.

The baseline registers 2711 tests in each debug/release preset. Five new
native test translation units add 59 regressions: 12 motion, four native cursor,
13 syntax/diff/footer/plugin, 20 agent/permission/install controls and ten
active-dialog/Doctor tests. The final isolated focused 59/59 gate passes. The related
snapshot/theme/question/layout gate passes 163 tests with two existing disabled
registrations.

Eight existing goldens were refreshed through their native updater. A
per-character SGR audit against the baseline proves identical text, layout,
backgrounds, bold, blink, inversion and other attributes. Differences are
limited to the Light info foreground calibration in three files (193 cells)
and removal of subtitle dim in eight files (728 cells). Fixed-width trailing
spaces remain unchanged; excluding these audited fixtures, source/document
`git diff --check` passes. No global whitespace policy was changed.

Independent reviewers `agent:grep_review` and `agent:claude-cli` approved the
source/test design. The final independent review accepted the completed build
and full-suite-plus-retry evidence with the explicit limitations below. Their review
caught the missing native cursor-shape gate and extended current text coverage
to installation, path badges and AgentWizard. Native regressions then exposed
local Mono fill collisions, now repaired without changing other variants.

Final validation uses an isolated managed worktree at the immutable baseline
plus this task's owned patch. Concurrent RFC 0020 source/build work in the
shared checkout is preserved and excluded from this validation tree. The
pre-amendment full debug run passed 2745 tests, skipped 20 and retained two
disabled tests (2767 registrations); it is intermediate evidence, not final
proof for the added MCP/frame/session-picker fixes.

The first isolated full debug attempt encountered three intermittent failures
outside the changed production owners. A background-agent test exceeded its
three-second status wait; a Grep timeout fixture could not read its mock PID.
Both passed unchanged native reruns with a task-specific temporary root; their
precise startup/collision cause is not established and no timeout was relaxed.
The OSC 7501 question fixture completed the answer and final model response,
but its exact sequence lacked one transient working state between question
and done. Independent source/trace review established that ProgramStatusReporter
emits a current snapshot only at UI flush boundaries: completion can precede
the next flush. The strict E2E event-list expectation therefore races with the
existing snapshot implementation. This task changes neither that producer nor
the question callback; the native scenario also passed unchanged rerun. A
separate status owner must reconcile that observability/test contract. Complete
debug/release results and unchanged native retries are reported separately;
the failed attempt and raw terminal evidence are retained with the verification
artifacts.

A subsequent full-run temporary-directory override changed the welcome path
normalization and produced unrelated replay snapshot differences. That
environment override was withdrawn; no replay fixture or normalizer was
changed. Final full suites use the ordinary project preset environment.

The final standard-environment debug full run passed 2747 tests, skipped 20,
retained two disabled registrations and failed one `E2E_Pty_Selection_wide`
scenario (2770 registrations, 469.32 seconds). The unchanged native command
`ctest --preset debug -j1 --rerun-failed` then passed that one scenario in
0.89 seconds. This is full-suite evidence plus an isolated retry, not a claim
of zero first-pass failures. Independent source/trace review found that the
failed trace already contains the correct wide-character selection and the
copy-to-terminal fallback message. The failure is in the asynchronous native
clipboard/backend fixture path; the trace cannot distinguish native failure
from unsupported-backend fallback. Neither selection/clipboard production code
nor that test was changed by this task. The exact backend subcause remains
unresolved and needs a separate owner; the accessibility source review retains
APPROVE with this limitation recorded.

The standard-environment release full run passed 2747 tests, skipped 20,
retained two disabled registrations and failed one
`GrepTest.CancellationTerminatesAndReapsChild` scenario (2770 registrations,
401.81 seconds). The fixture did not observe its mock shell PID file within
three seconds; its cancellation-result assertion did not fail. The precise
startup/fixture cause is unproven. Grep production and test sources are
unchanged by this task. The identical binary and standard environment passed
`ctest --preset release -j1 --rerun-failed` (1/1, 0.25 seconds). No timeout,
fixture, production tool or snapshot was relaxed to obtain that result.

| Final standard-environment execution | Passed | Failed | Skipped | Disabled | Native unchanged retry |
|---|---:|---:|---:|---:|---|
| Debug full, 469.32 s | 2747 | 1 | 20 | 2 | Selection_wide: 1/1 passed, 0.89 s |
| Release full, 401.81 s | 2747 | 1 | 20 | 2 | Grep cancellation: 1/1 passed, 0.25 s |

All 59 added regressions passed in both complete suites. After the documented
isolated retries, each preset has 2748 passing, 20 skipped, two disabled and
zero unresolved failing registrations. Neither full invocation was first-pass
green; intermittent failures remain independently documented. Both fresh
`cmake --build --preset debug` and `cmake --build --preset release` completed
successfully with the default Ninja parallelism. Module/UI9 graph, inline
conformance, message-theme lint (232 modules, including negative controls),
RFC lint and source/document whitespace gates passed. The final validation
manifest verifies 2556 frozen source/fixture/build-input hashes and four
executed binary hashes unchanged between the recorded runs.

No commits or external configuration changes are part of this task.

Numeric contrast tests require actual serialized RGB and assert that parsing
succeeds; they never silently skip a quantized color. ANSI modes are checked
for meaningful indices and visible text, not universal RGB ratios. Native
screens do not certify physical terminal brightness, custom ANSI palettes,
color-vision-deficiency clinical outcomes or Linux execution on this macOS
host.


## Current integration verification (2026-10-18)

The user authorized fresh independent review, Debug/Release complete suites,
real-terminal validation and logical commits/push for the accessibility and
readability repairs against `1056e07b`. The 54 owned paths exclude the unrelated
`.gitignore` delta, which remains untouched. RFC 0019 remains accepted and
design-only; no automatic terminal-background detection is implemented.

Independent source review by `agent:/root/a11y_code_review` approved the 35 UI
source files, five added native test units/registration and eight goldens, with
no unresolved source P1/P2. Independent documentation/readiness review by
`agent:/root/a11y_readiness_review` required the historical/current distinction
above; final graduation by `agent:/root/a11y_readiness_review` is APPROVED
after both complete current-source suites and the native terminal evidence in
`build/a11y-readability-validation-20261018/`.

The reduced-motion flag is an existing internal `Theme::a11y` API. Production
settings currently do not expose a user toggle. This repair does not introduce
a new configuration contract: native UI tests exercise both flag values, while
public CLI acceptance exercises the existing launch, theme, dialog, input and
status paths. Do not claim a CLI reduced-motion setting was added or toggled.

The first incremental Debug attempt failed because overlay copying preserved
source mtimes older than existing BMI outputs: the new contrast test saw stale
`DiffSyntaxTheme` without its new fields. Refreshing overlaid input mtimes
invalidates those artifacts without changing bytes, flags or concurrency.
The original failed log and diagnosis are retained separately.

Real native Release `/config` acceptance exposed a further local readability
defect: `SettingsView` composes the Settings component over the transcript
with `dbox`, but its window left blank cells transparent. The actual screenshot
retains underlying Logo/privacy text. Independent review approved applying the
existing `bgcolor(pal.background) | clear_under` convention to the Settings
window alone. A composed-surface regression (seven themes, 40/80 columns,
inside/outside pixels and held-component events) failed before the correction.
The interrupted pre-correction complete-suite attempt is not graduation
evidence. Final source adds 60 registrations over the 2923-test baseline.

### Final current-source validation

| Preset | Registered | Passed | Skipped | Disabled | Failed | Wall seconds |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 2983 | 2958 | 23 | 2 | 0 | 504.66 |
| Release | 2983 | 2958 | 23 | 2 | 0 | 433.52 |

These are fresh, unfiltered, default-environment `ctest --preset <preset> -j1`
runs on the corrected final source, Debug followed by Release. Earlier failed
builds, the pre-fix red regression and the deliberately interrupted full run
remain separate; they are not relabeled as first-pass success. The final source
retains all 2923 baseline names and adds exactly 60. Focused regressions pass
60/60 in each preset. Default-Ninja builds pass with `-Werror` (55.51 seconds
Debug, 181.84 seconds Release). Graph/core8/UI9/store/TLL, inline, message-theme
and RFC lint pass. Only the eight audited fixed-width goldens are excluded
from the trailing-whitespace check.

The source freeze covers 2695 Git-visible inputs and 218 executed binary hashes.
The exact Release SHA is
`dd3c30f21b7d82840201bb5e68a7686f88bbba44c7abf79241c69b2b468b46ac`.
Independent source and motion-prefix review: `agent:/root/a11y_code_review`.
Independent design/readiness and live-evidence review:
`agent:/root/a11y_readiness_review`. Full reports, final graduation decision,
complete XML/logs, source preservation and publication results are retained in
`build/a11y-readability-validation-20261018/`.

Real native CLI/PTY acceptance uses the user's configured endpoint: exactly one
Bash `sleep 8`, a nonerror result and completed table/C++ output. Light/dark
Settings are opaque, Chinese insertion/cursor movement/clear and a 40-column
main conversation/status are verified. Both owned CLI sessions exit 0; temporary
credentials and servers are removed; original user settings retain hash, mtime
and mode. xterm displays the actual native PTY byte stream; no REA or synthetic
UI is used. The file `running-light-80.png` is the running proof;
`tool-running-light-80.png` is a misleadingly named completed diagnostic frame.

Limits remain explicit: the internal reduced-motion flag has no production
user toggle; screenshots do not prove no blink, so native pixel/cursor tests
cover that contract. Existing 40-column Settings sidebar/content/footer clipping
is not redesigned. Shift+Right main-prompt selection was not verified. Actual
dark acceptance uses a new owned process with explicit `LOOM_THEME=dark`, not
a claim that external config editing immediately reprojects the running theme.
Physical/custom/translucent terminal palettes, ASan/TSan, Linux/Windows and
hosted CI timing remain unverified. No persistent format, public CLI option,
configuration contract, permission policy or external protocol is changed.

Final independent graduation: **APPROVED** by
`agent:/root/a11y_readiness_review`; no unresolved material findings. The
previously approved four-part series may be committed and pushed with normal
Git hooks, while preserving `.gitignore` and the stated validation limits.


### Follow-up: Settings at forty columns (2026-10-18)

The preceding forty-column Settings limitation is addressed by a local bug fix
in `loom.ui.dialogs.settings_dialog`. The dialog negotiates its allocated FTXUI
box; below 72 columns a section/index navigator replaces the fixed sidebar.
Labels, controls, descriptions and long UTF-8 values wrap by terminal cells.
Usage cards and action groups pack into complete rows before scroll-height
measurement. The document uses the full allocated content width, follows selected
fields, and offers independent PageUp/PageDown/Home/End reading. Save/Reset/Close
and navigation hints stay outside the scroll area; reflected action bounds
support mouse activation through the same keyboard callbacks. Draft and focus
state survive resizing. No global terminal-size query, render ticker, shared UI
abstraction, configuration key or persisted/public contract is introduced.

MCP navigation now distinguishes lifecycle help from selected servers. Lifecycle
management is explicitly delegated to `/mcp`: the old draft-only Auto-start
boolean had no configuration reader/writer and is no longer offered as a saved
setting. Enter opens lifecycle guidance without marking a draft dirty; removal
targets the highlighted entry, and `a` displays
the existing `/mcp add` delegation instead of being intercepted by About.
Failed-save diagnostics remain available until another notification, with long
details in the document and a bounded footer reading hint.

Independent design/source review: `agent:/root/settings_narrow_review`.
Independent native regression author: `agent:/root/settings_narrow_tests`.
The baseline reproduced eight failures in nine tests. Twelve strengthened native
`SettingsLayout.*` regressions now cover seven themes, all thirteen sections,
40x18/28, held-component resizing, complete long-value chunks, field focus,
hermetic writes/unknown-key preservation, footer mouse actions, exact MCP
selection, permission-list focus and long failed-save diagnostics. Review findings about iterative
layout lifetime, scrollbar background clearing and transient error lifetime
were resolved. A provisional wider layout must never mutate the user's scroll
position; its regression includes the final Usage cost and error-path tail.

Current validation artifacts and terminal captures are retained in
`build/settings-narrow-validation-20261018/`. Final dual-preset and live
acceptance results are recorded with that evidence bundle.


Real keyboard acceptance uncovered an App-level shortcut priority defect that
pure Settings component tests could not observe. The global Ctrl+S prompt
stash checked queued dialogs only; SettingsView uses a held component outside
that queue. Actual PTY input hex13 arrived with XON/XOFF disabled, but the App
returned before forwarding to Settings. The minimal App-local exclusion now
includes SettingsView. Two public AppAdapter workflow regressions exercise
project-file saving with unknown-key preservation and prompt retention, and
unchanged Normal stash/pop. The old App fails the Settings case while passing
Normal; this is a required cross-module root-cause fix, with no new public or
persisted contract. Final acceptance requires a fresh freeze and both complete
suites after this correction; earlier 2995-case results are intermediate.


Final follow-up validation on the corrected App route:

| Preset | Registered | Passed | Skipped | Disabled | Failed | Wall seconds |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 2997 | 2972 | 23 | 2 | 0 | 539.37 |
| Release | 2997 | 2972 | 23 | 2 | 0 | 435.95 |

These fresh unfiltered default-environment `ctest --preset <preset> -j1`
runs retain all 2983 baseline names and add fourteen, with no duplicates or
removed names. Final default-preset builds pass with `-Werror`; 77 focused
regressions pass in each preset. Graph/core8/UI9/store/TLL, inline-definition,
theme and RFC checks pass. The frozen input set covers 2697 Git-visible files
and 218 executed binary hashes; only this final documentation append is allowed
a separately recorded post-test hash. Eight owned files match between primary
and the isolated checkout. The original unowned `.gitignore` and user settings
retain their recorded content and permissions; user settings also retain mtime.

The final native Release SHA-256 is
`e7c172c03d888f2049e4619f80861c9aafe15c491671e15b69d4fb9e3735ed0e`.
Actual PTY input and fixture project files prove Ctrl+S saves, Ctrl+R restores,
and the visible Save mouse action persists its value at 40x18. Captures show
complete fixed action/navigation rows in light and dark themes, API Home/End
including the long endpoint tail and final field at 40x28, and held selection
with restored sidebar at 80x34. Dark MCP `a` delegates to `/mcp add`; lifecycle
Enter-help/no-dirty is verified by the focused native component regression.
Both CLI processes exit 0; owned staged configs and servers are removed.
xterm renders the native PTY stream, without REA, synthetic product UI or a
provider endpoint call. Current screenshots use dummy configuration only.

Earlier attempts remain explicit: the pre-App-correction 2995-case Release
full had one scroll-copy failure; an unchanged isolated retry passed. The
complete selected text existed in its OSC52 fallback, but the specific native
clipboard backend failure cause was not logged. This is not a proven Settings
regression or a proven timeout. That intermediate first-run failure is retained
separately from both final zero-failure full suites. Initial red layout tests,
interim contrast/opacity regressions and the corrected fixture-isolation failure
also remain in the evidence bundle. No failed run is relabeled as first-pass
success.

Independent source/design review is recorded by
`agent:/root/settings_narrow_review`; final readiness and evidence review by
`agent:/root/settings_narrow_readiness`. Detailed reports, raw PTY records,
input hex logs, screenshot metadata, full XML and preservation audit live in
`build/settings-narrow-validation-20261018/`. Cell wrapping may split English
words; all cells remain accessible. Physical/custom/translucent palettes,
ASan/TSan, Linux/Windows, hosted CI timing and the skipped live-provider tests
remain unverified. No new public configuration, persisted format, CLI option
or wire contract is introduced. The App event-entry guard is the necessary
shared routing correction described above. These eight follow-up files remain
uncommitted; no push was performed.


### Follow-up: word-aware Settings text (2026-10-18)

The remaining forty-column English-word split is corrected inside the existing
SettingsText implementation unit. Greedy runs prefer ASCII-space word boundaries,
with wide glyphs providing CJK break points. Only a token wider than the allocated
line falls back to terminal-cell splitting, so long URLs, paths and hashes remain
fully readable. Combining glyphs, explicit hard lines, blank/trailing lines and
fitting initial indentation are retained; separating spaces are replaced by an
inserted soft break. Configuration values are never modified by display wrapping.
The pinned FTXUI paragraph helper was considered but uses space-split flex layout
without the required explicit-line/oversized-token behavior. No shared abstraction,
module interface, configuration, persisted data or public contract is changed.

Three native SettingsLayout regressions check complete words on physical Screen
rows, mixed-width/combining glyphs and hard lines, and exact long-URL reconstruction
at seven and forty cells. The old wrapper fails two of these three cases while
retaining URL completeness. Existing section/focus/footer/write/opacity/contrast
regressions remain required. Independent source reviewer:
`agent:/root/settings_wordwrap_review`. Current source review is approved; final
build, full-suite and native terminal evidence is recorded in
`build/settings-wordwrap-validation-20261018/` before graduation. No commit or push
is part of this follow-up request.


Final word-wrap validation:

| Preset | Registered | Passed | Skipped | Disabled | Failed | Wall seconds |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 3000 | 2975 | 23 | 2 | 0 | 557.40 |
| Release | 3000 | 2975 | 23 | 2 | 0 | 470.36 |

Both are fresh, unfiltered serial default-environment runs. Complete XML names
match registration exactly; all 2997 baseline tests and the three additions
remain, with no omissions or duplicates. Default Debug/Release builds and all
80 focused cases per preset pass with -Werror; graph/core8/UI9/store/TLL,
inline-definition, theme and RFC lint pass. The frozen inputs cover 2697
Git-visible files and 218 executable hashes. This result-only documentation
append is separately hashed after execution; no product/test/binary changed.

Real native Release PTY frames show complete English prose in light/dark
40x18, an intact fixed footer, actual Ctrl+S persistence, full URL including
ENDPOINT-TAIL at 40x28, document End access, and an 80-column sidebar restored
through the held component. xterm displays the native PTY stream without REA;
fixtures use dummy configuration and no provider endpoint. Both sessions exit
0, owned staged configuration and servers are removed, and original user
settings retain hash/mode/mtime. URI path segments can still split inside one
oversized URL; no segment-atomic policy is claimed. General Unicode line-break
rules, other platforms, custom physical palettes, ASan/TSan and skipped live
provider cases remain outside this acceptance. All follow-up changes remain
uncommitted; final independent evidence verdict is recorded in
`build/settings-wordwrap-validation-20261018/source-review.md`.
