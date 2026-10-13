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

Terracotta and amber/gold guide normal brand and mode accents. Light themes
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
the existing static spinner glyph. These rules do not introduce a render ticker
or an animation framework, and do not change StatusLine defaults.

Implementation scope and measurable acceptance criteria are recorded in
[RFC 0013](../rfcs/0013-readable-bounded-brand-ui.md).
