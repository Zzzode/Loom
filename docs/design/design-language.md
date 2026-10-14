# Loom terminal design language

Loom uses FTXUI's cell grid. The terminal controls font size, line height and
the colors behind transparent rows. Hierarchy comes from whitespace, grouping,
glyphs, restrained foreground accents and explicit interaction surfaces.

## Readability

Use `text` for body text and `muted` for secondary information that must remain
readable. Reserve `subtle` and `border_subtle` for decorative boundaries. A
completed task keeps its checkmark and strikethrough; it still needs a readable
label. Do not dim task counts, teammate status or output tails.

For truecolor themes, affected informational text must reach 4.5:1 contrast on
its actual filled background. Test normal, hover and selected states separately.
Transparent rows are checked against the theme's `background` reference;
this does not certify an arbitrary terminal canvas. ANSI color values are
terminal-controlled, so tests check distinct color indices and readable glyphs
instead of asserting a universal RGB contrast ratio.

## Attention by state

The REPL derives attention from current work, the latest failed model request
and the actual rendered decision surface. It does not introduce a second
execution state machine. A visible actionable permission takes precedence over
a failed current model request, which takes precedence over working and normal.

| State | Primary focus | Subordinate presentation |
|---|---|---|
| Normal | Prompt and conversation | Neutral model/path/branch/context/cost; completed chains remain quiet |
| Working | Live chain summary in `primary` and bold; current target in `text` | Canonical work glyph in `primary`, readable neutral spinner label and metadata |
| Awaiting permission | Existing risk frame, bold decision title and focused allow/deny actions | Keep target, elapsed and details; neutralize chain summary and omit bottom work spinner |
| Failure | Warning/danger title for the failed model request, diagnosis and permitted recovery actions | No competing work emphasis; attempts, IDs and response metadata remain readable |

Authorization requires a real visible top layer with registered rendering/input
capabilities and a response callback. Queued, suppressed, placeholder and null
panels do not claim decision focus. Outer Settings, Agents and Trust views own
both presentation and input: a hidden tool request must not receive their
decision keys. Other visible modal owners suppress underlying work emphasis.

Single-request permission footers keep complete scope labels and key/action
pairs together and wrap between controls. Compact actions and a fixed footer
preserve decision access in short terminals. PageUp/PageDown scroll only the
request body, so long details remain reachable without moving decision focus
or triggering an authorization. The panel clears underlying transcript cells.

An old tool error or denial remains inline; it does not turn a corrected tool's
approval or a later running query into global failure. Automatic retry remains
work while the query runs. Recovery shortcuts retain their existing request
ownership and availability.

## Terminal hierarchy

The terminal controls font metrics. Use weight, ink and existing row grouping
consistently instead of inventing a font-size scale.

| Rank | Ink and weight |
|---|---|
| Decision/recovery title | Existing warning/danger, bold, explicit literal title |
| Current activity summary | `primary`, bold while work owns focus; otherwise neutral |
| Body, diagnosis, editable input and activity target | `text`, regular; retain Markdown semantics |
| Metadata and required instructions | Readable `muted`, regular; model/key labels may use bold `text`; no dim |
| Decoration | `subtle`/border tokens; dim only for noninformational separators and ornamental dots |

The compressed-chain title is the complete first-line summary, including elapsed
and tool breakdown; preserve its existing ellipsization and disclosure budget.
Quiet summaries use `muted`; expanded blocks use the theme canvas `background`
to preserve contrast for both neutral ink and the live `primary` accent.
The activity target is the second bounded line. Emphasis must never affect stage
timing, activity text, expansion identity, row height or scheduled wake time.

Built-in statusline metadata is neutral. Context still uses warning at50% and
danger at80%. Positive sub-cent cost reads `<$0.01`; otherwise show two decimals.
Keep existing field priorities, masks and defaults. Custom status commands own
their explicit ANSI styles; Loom does not apply an extra dim decorator.

These rules apply to RFC0015's bounded consumers: built-in statusline, compressed
activity, work spinners, single permission requests and model-error metadata.
They do not claim to restyle all historical/help/toast surfaces in one change.

## Activity height

The REPL shares a finite row budget between todos and live teammates. Fixed
prompt, footer, spinner and suggestion requirements take priority. The budget
also accounts for the pinned header, scroll spacing and fullscreen bottom limit.
The held prompt viewport follows the caret when its visible row count shrinks.

Task headers include completed/total counts and the number without a visible
detail row. In-progress tasks receive detail space first; rendering does not
reorder or mutate the underlying task state. Teammate detail retains its input
order. A very small budget shows summaries; zero available rows hides activity.
Overflow remains in the complete state and existing task/team tools.

## Canonical tokens

Use the canonical palette field for a concrete rendering surface or foreground.
Use an existing `Role` when the caller already accepts a semantic role. Do not
add a second role/field spelling for the same concept.

| Need | Palette field |
|---|---|
| Secondary text or icons | `muted` |
| Links and selected prompt glyphs | `suggestion` |
| Brand emphasis | `primary` |
| Prompt rules | `prompt_border` |
| Hovered user surface | `user_message_background_hover` |
| Selected action surface | `message_actions_background` |
| Bash or memory surface | `bash_message_background`, `memory_background` |
| Decorative separators | `border_subtle` |

Status, diff, permissions, selection and identity tokens may have similar
values in one theme while differing in others. Compare all seven palettes and
their actual consumers before merging fields. Keyword highlights use indexed
arrays; the historical lookup strings remain supported.

## Brand and identity

The existing `primary` token owns live-work emphasis. Terracotta and amber/gold
remain brand and mode accents on their existing surfaces. Light themes
use darker equivalents for foreground contrast. The weave artwork keeps its
identity color. Dark body text uses warm off-white as a visual choice.

Success, warning and danger retain their meanings and shape/label cues. Agent
identities retain eight named hues and their assignment rules; their colors
must work on the palette canvas reference and selected agent-card surface.
Color-vision-deficiency themes keep explicit status and identity palettes.
Touched monochrome accents remain achromatic; touched ANSI accents use named
terminal colors.

## Interaction and motion

Keep held FTXUI components and the event-driven render loop. Preserve prompt
selection, wrapping, caret geometry and submission bytes. Reduced motion uses
the existing static spinner glyph in touched work spinners. Ordinary REPL and
widget work share the ten canonical braille frames in `design_figures`; the
centralized four-frame bridge connection indicator is an explicit exception.
These rules do not introduce a render ticker
or an animation framework, and do not change StatusLine defaults.

Implementation scope and measurable acceptance criteria are recorded in
[RFC 0013](../rfcs/0013-readable-bounded-brand-ui.md) and
[RFC 0015](../rfcs/0015-state-led-attention-and-hierarchy.md).
