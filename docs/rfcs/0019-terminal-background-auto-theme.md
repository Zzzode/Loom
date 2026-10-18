---
rfc: 19
title: Terminal Background Detection for Auto Theme
status: accepted
owners: "@Zzzode"
reviewers: ["agent:grep_review/design-rereview", "agent:claude-cli/design-rereview", "agent:/root/a11y_readiness_review"]
created: 2026-10-16
last-reviewed: 2026-10-18
tracking: local
---

# RFC 0019 — Terminal Background Detection for Auto Theme

## Summary

Make the existing `auto` theme select a concrete dark or light palette from a
bounded terminal-background query. Keep FTXUI as the only input reader and
reuse Loom's control-byte write/flush boundary, with AppAdapter owning
serialization against terminal rendering. Explicit themes always win;
unsupported, malformed and late responses fall back to dark. This is a
separate design record requested alongside accessibility bug fixes. No auto
theme implementation is authorized or included in that bug-fix diff.

## Motivation

### Evidence

Source inspection on 2026-10-16 at `42a5d172`:

| Measurement | Current | Proposed outcome | Evidence |
|---|---|---|---|
| Auto identity | `ThemeVariant::Auto = Dark` | Requested auto remains distinguishable from resolved palette | foundation/theme_provider.cppm |
| Theme round trip | `parse_variant("auto")` is named `dark` by `variant_name` | Preserve the existing `auto` setting string | Same enum alias and lookup functions |
| Auto startup queries | Zero OSC 11 encoders/callers | At most one query per interactive session while auto is requested | utils/terminal/osc, app_settings.cpp |
| Existing input ownership | FTXUI parses input; app consumes OSC 7501 events | Same reader, narrow additional OSC 11 framing | app_render_event.cpp, patched terminal_input_parser.cpp |
| Generic OSC framing | ST supported; BEL handling/bounds special-cased for 7501 | Narrow bounded handling for 11 without regressing 7501 or ordinary keys | fetched FTXUI5 parser and project CMake patch |

The default `Theme{}` remains the dark fallback. Config already defaults to
`auto`, so its effective palette can become light under this proposal; the
stored setting is unchanged. No transparent rows are filled globally.
OSC 11 reports the terminal's default background, not the composited color of
a translucent window or a guarantee about all sixteen ANSI entries.

Protocol reference: [XTerm control sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html),
Operating System Commands, dynamic colors 10–19. Querying uses `?`; OSC 11
addresses the default text background. Both BEL and ST terminators must be
accepted by the future implementation.

## Goals

- G1. Keep `auto` distinct in parsing, naming and requested settings while
  storing a concrete effective dark/light palette separately. Existing seven
  concrete enum values and persisted setting names remain compatible.
- G2. Send one `OSC 11;? ST` query only when auto is requested and stdin/stdout
  are TTYs. No query for explicit themes, headless commands, redirected output
  or tests without an injected terminal transport.
- G3. Resolve a valid RGB response within a 100 ms monotonic-clock window;
  otherwise retain dark. Startup, input and cancellation must not block.
- G4. Preserve ordinary keyboard, mouse, paste, Escape handling and OSC 7501
  negotiation outside recognized terminal-control frames. Raw OSC 11 bytes
  are reserved control data even within an exotic paste, as specified below.
  Late/duplicate OSC 11 replies are consumed as terminal control data and
  never become prompt text or reverse a later explicit theme selection.
- G5. Leave config files, session/tool formats, theme-independent accessibility
  flags, terminal palette settings, canvas transparency and OSC 52 unchanged.

## Non-Goals

No implementation in the current accessibility bug-fix task; no continuously
polled theme changes, OSC 4 palette remapping, terminal palette writes,
background fill, tmux passthrough wrapper, new settings, general query broker,
parallel stdin reader, render ticker, or custom ANSI contrast certification.
RFC 0006's other governance proposals remain provisional.

## Proposal

### Ownership and dependency direction

- `utils/terminal/osc` owns the pure query bytes and strict response parser.
  It does not read, write or choose a UI theme.
- The existing FTXUI input patch owns bounded OSC 11 event framing, with
  dedicated parser tests beside current input/program-status tests.
- `ui/app` owns the one-shot query lifecycle, deadline, requested/effective
  distinction and settings-generation guard. It writes through
  `terminal::output` at UI-thread delivery boundaries, consumes controls after
  pending settings projections but before prompt handling, and
  projects a resolved palette through the existing provider.
- `ui/foundation/theme_provider` owns concrete palette mapping and the distinct
  `Auto` identity; it never performs terminal I/O. No new library or module
  edge from foundation toward app/chrome is introduced.

Use the existing OSC 7501 startup/delivery/event boundaries as the local
composition pattern, not a second reader or a shared general-purpose service.
The new negotiation state belongs to the existing AppAdapter implementation.
Any helper extraction must be justified during the implementation gate.

### Parsing and classification

Accept only `ESC ] 11 ; rgb:R/G/B` terminated by BEL or ST, with one through
four hexadecimal digits per component. Normalize each component against its
own maximum `(16^digits - 1)`; do not assume four-digit replies or truncate
one-digit colors. Reject missing, extra, nonhex, control-injected or oversized
payloads. Bound the whole response to 128 bytes, with framing resynchronization
tests for malformed/partial sequences. Unrelated input outside these frames
is preserved. Framing and lifecycle deadlines are separate concerns.

Stream framing rules:

- Recognize only the exact OSC 11 command prefix; tentative prefixes are
  compared across reads without swallowing a different OSC command or a lone
  Escape key. Once the command is recognized, all bytes through its first
  BEL/ST terminator belong to that control frame, including invalid payloads.
- Keep at most 128 bytes. A complete valid or malformed frame produces one
  Special event. Overflow or 100 ms partial-frame expiry emits one bounded
  invalid Special event, clears the buffer and enters opaque discard mode;
  discard the remainder through BEL/ST without emitting more events, then
  resume with the first following byte. Count real monotonic age during both
  continuous Add and timeout callbacks; never depend only on idle time.
- Discard mode uses constant memory and does not block the event loop. No byte
  inside a discarded frame is replayed as a keystroke. A never-terminated
  malformed frame can therefore swallow following input until a terminator;
  this unavoidable framing ambiguity is an explicit risk and a PRR gate, not
  a claim that arbitrary corrupt streams preserve every key. The future
  implementation must demonstrate a user recovery path before it can ship.
- Frame OSC 11 before bracketed-paste interpretation, consistently with the
  existing terminal-control routing. A literal OSC 11 frame inside a paste is
  consumed as control data. Normal paste bytes and its start/end markers are
  preserved. An actual reply arriving during a paste is not inserted as text.
- Keep the existing 7501 framing and all other OSC commands unchanged. Test
  fragmentation, adjacent keys, escaped terminators, overflow followed by
  terminator plus a key, and simultaneous 11/7501/paste streams.

Normalize channels to [0,1], apply sRGB linearization and relative luminance
`0.2126 R + 0.7152 G + 0.0722 B`. Choose the palette whose reference background
is closer in luminance to the reported background; tie falls back to dark.
Test the exact threshold computed from the current dark/light references,
including its two neighboring values. This is a deterministic palette-family
heuristic, not a certification of contrast against an arbitrary canvas.

### Lifecycle and precedence

Requested-theme precedence is the existing nonempty `LOOM_THEME` environment
override, then ConfigManager's effective `display.theme` cascade (including
existing `--settings` input), then the existing `auto` default. There is no
separate `--theme` flag today; a future override must enter this same projection
before negotiation. Do not create a second settings precedence path.

Phase C introduces a small app-local theme-settings generation counter; it is
not an existing counter. Every effective requested-theme or accessibility
change in ProjectSettingsToScreenState invalidates the prior generation.
Irrelevant file reloads do not reset query attempts. Snapshot the generation
when querying, and compare it again after pending UI actions have been drained
before applying a reply. Preserve current accessibility flags, and keep
high-contrast/forced-monochrome overrides authoritative over the resolved
palette family.

| Event | UI-thread action | Effective palette / query |
|---|---|---|
| Construction/settings projection | Resolve the existing env/config cascade, capture requested name, advance generation on relevant changes | Explicit theme immediately, auto dark or cached result; no I/O before interactive startup |
| `ftxui::Loop` construction complete | PreMain/Install has installed FTXUI's reader; start once beside the existing StartProgramStatus boundary before loop.Run | If eligible auto, write one query and capture generation/deadline; otherwise zero writes |
| Query write succeeds | Set deadline from monotonic time at delivery | Auto dark until valid response, no startup wait |
| Query write fails/non-TTY | Mark attempt complete, never retry in this session | Dark fallback; explicit themes unaffected |
| Event arrives | Drain existing UI actions/settings projection before auto-control processing; process auto controls before user-input acknowledgement/dispatch | Only matching generation, auto still requested, before-deadline valid reply may update the palette and request one repaint |
| Valid reply | Cache normalized RGB separately from requested/effective state | At most one application to the live auto generation; duplicate replies consumed |
| Expiry/malformed reply | Check at control delivery and existing event-completion boundary | Dark; no ticker, wait or retry; late frames remain control data |
| Reload changes to explicit or a11y override | Advance relevant generation before applying queued controls | Explicit/override wins; in-flight auto application invalidated |
| Reload/reselect changes back to auto | Advance generation and consult successful cache | Cached result or dark; at most one attempted query in the entire session |
| Exit | Discard pending state | No terminal palette reset; Loom never changed it |

All query writes run on the same UI thread outside Draw/Render, following the
existing program-status delivery ordering; terminal::output only performs
write/flush and supplies no serialization by itself. No background reader or
writer is added. The one-attempt rule intentionally accepts a slow terminal's
session-long dark fallback to keep query traffic and recovery predictable.
A failure is not cached as a color; attempt state and successful RGB cache are
separate. No later reload invents a second query. With no more events the
visible fallback is already dark, so deadline expiry needs no wakeup.

### Compatibility and regression boundaries

Append a distinct `Auto` enum value rather than renumbering concrete variants.
Do not serialize enum ordinals or a resolved palette into config. Audit all
switches, `all_theme_names` enumeration, theme dialog choices, settings reload
and string round trips together. Keep monochrome and color-vision-deficiency
selection explicit; preserve accessibility flags when projecting the result.
Default snapshots stay dark and deterministic; a queried-light integration
case must assert foreground changes without prompt/caret/layout changes.

## Phases and graduation criteria

| Phase | Title | Scope | Status | Graduation criteria (measured) |
|---|---|---|---|---|
| A | Design gate | This record only | done | Independent design review, concrete input-ownership/compatibility/timeout findings resolved; no src edits |
| B | Protocol and framing | Pure OSC helper and narrow FTXUI patch | proposed | Every valid width/case/terminator and split byte boundary; >128 byte/malformed/late/key/paste/7501 coexistence tests pass |
| C | Session projection | Theme identity, app lifecycle and settings generation | proposed | Fake clock/writer: at most one query, no blocking, explicit override wins, headless zero writes, all seven concrete variants unchanged |
| D | Graduation | Native and PTY acceptance, docs | proposed | Dual -Werror builds and full serial CTest 0 failures, reconciled counts, graph/lint pass; Linux/macOS PTY and declared real-terminal matrix recorded |

## Production Readiness Review

This record is not at the implementable gate. Before production edits, an
independent PRR must approve the final parser patch, fake-clock seam and exact
application event ordering. Implementation must demonstrate no lost keys with
interleaved OSC 11/7501 replies, including overlong and timed-out fragments.
Existing protocols and clipboard tests remain regression gates. Inspect the
current 7501 age-expiry reset separately from its overflow discard path; do not
copy one while assuming both have identical recovery semantics. The 100 ms
query deadline is an eligibility cutoff checked on UI events, not a promise
that a busy event loop executes expiry at exactly 100 ms; responses delivered
after the cutoff always lose, including scheduler overshoot. Record BMI
fan-out and local build wall time at default Ninja parallelism; `/proc` PSS
cannot be measured on this macOS host and requires a Linux run. No memory or
real-terminal compatibility improvement is claimed by this documentation.

## Rollout and rollback

Current work delivers this proposal only. A future implementation starts only
after independent design and PRR gates and explicit implementation scope.
Ship protocol/framing before projection. Revert app projection and enum
consumers together; no stored-data migration or terminal reset is required.
Keep independently useful bounded framing only if its own tests remain green.

## Drawbacks

Supported terminals may repaint once from the dark fallback to light. Terminals
can suppress color queries; translucent/custom palettes can defeat a binary
heuristic. A narrow dependency patch adds maintenance and input-loss risk,
which is why framing and interleaving tests are an implementation gate.

## Alternatives considered

1. Keep `auto` as dark: preserves behavior but leaves the advertised setting
   unable to reflect a light terminal.
2. Read `COLORFGBG` or terminal brand: environment hints may be stale and do
   not establish the live default background. No extra fallback precedence.
3. Synchronously query before FTXUI startup or spawn a reader: risks startup
   delay and lost user input. Reuse the existing sole input owner instead.
4. Build a shared terminal-query broker or query all ANSI colors: unnecessary
   scope and complexity for one session-owned request.

## Testing and verification plan

Use native GoogleTest, the existing fetched FTXUI parser test integration and
isolated real Loom PTYs with injected replies. Record each new registration
and baseline at the implementation start; do not reuse an old RFC's counts.
Test dark/light/invalid/no-reply and explicit overrides in normal/fullscreen
at 40/80 columns, with input bytes/caret and OSC 7501/52 unaffected. Cover
Windows/non-TTY fallback where no interactive transport exists. Run debug and
release full serial CTest, RFC/message-theme/graph/inline gates. Before phase D
completion, declare real terminal and multiplexer versions and record supported
and fallback results separately; do not claim unsupported environments passed.

## Documentation impact

Add the RFC to the docs index now. Future implementation updates theme setting
documentation, terminal protocol ownership and the design-language canvas
boundary. No current CLI, default theme or application behavior changes.

## Open questions

None blocking the proposal. Exact fake-clock test exposure and dependency
patch hunks require independent PRR at the future implementation gate.

## Implementation History

| Date | Phase | Event | Commit / PR | Evidence (metrics, test totals) |
|---|---|---|---|---|
| 2026-10-16 | A | RFC opened; documentation only | local, uncommitted | Auto aliases Dark; 0 OSC11 queries; FTXUI/OSC7501 ownership inspected; no auto production edits |
| 2026-10-16 | A | Independent reviews requested clarification; draft amended | agent:grep_review/design; agent:claude-cli/design | Explicit framing/discard/paste tradeoff, env/config generation and a11y precedence, actual Loop/PreMain reader boundary, writer serialization and no-retry rationale added; future corrupt-stream recovery remains a PRR blocker; no src edits |
| 2026-10-16 | A | Independent design re-reviews approved; accepted, not implementable | agent:grep_review/design-rereview; agent:claude-cli/design-rereview | Framing/paste, generation/precedence, a11y override, writer serialization and Loop/PreMain reader boundary accepted; corrupt-stream recovery and parser expiry-path proof remain explicit future PRR blockers; no auto implementation |
| 2026-10-18 | A | User authorized design-only publication; independent review reaffirmed accepted | This design-only change | agent:/root/a11y_readiness_review; RFC lint 24/24; B-D remain proposed; no OSC 11 implementation, configuration or default behavior change |
