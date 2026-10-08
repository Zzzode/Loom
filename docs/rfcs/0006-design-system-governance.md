---
rfc: 6
title: Design System Governance and Terminal UI Overhaul
status: provisional
owners: "@Zzzode"
created: 2026-10-08
last-reviewed: 2026-10-08
tracking: local
---

# RFC 0006 — Design System Governance and Terminal UI Overhaul

## 1. Summary

Loom's terminal UI has a first-class engineering architecture (12 acyclic
area libraries, event-driven rendering, seven theme variants) but its
**design system is accumulation-driven, not designed**. A design audit on
2026-10-08 found:

- **1039 hard-coded ANSI color literals** (`Color::Blue`, `Color::GrayDark`,
  `Color::DarkBlue`, …) across `src/ui/`, bypassing the 89-field palette
  entirely. ANSI named colors resolve to *whatever the terminal theme
  defines*, so the light/dark/daltonized palettes are silently ignored by
  the highest-frequency surfaces (message list, dialogs, screens).
- **Token names are implementation leaks**: `loomBlue_FOR_SYSTEM_SPINNER`,
  `red_FOR_SUBAGENTS_ONLY`, `clawd_background`, `professional_blue`,
  `chrome_yellow`. Comments in `design_tokens.cppm` record that batches of
  tokens were added by gap-analysis audits (`GAP: clr-missing-42-tokens-
  struct`), not design intent.
- **The root canvas paints `rgb(32,33,36)`** while padding rows show the
  terminal's real background, producing a visible seam ("白条") that is
  papered over with a comment in `fullscreen_layout.cppm` rather than fixed.
- **`auto` theme is a lie**: `ThemeVariant::Auto = Dark` — no OSC-11
  detection exists, yet `auto` is the documented default.
- **Four border styles** (Light/Rounded/Double/Heavy) are chosen ad hoc per
  call site with no semantic rule; `resume_screen.cppm` mixes Light and
  Rounded within one screen.
- **Emoji (15 sites)** used as semantic glyphs (👤 for the user label)
  render inconsistently across terminal fonts and clash with the curated
  braille/figures glyph system.
- **No design-language documentation**: spacing, surface vocabulary, color
  usage rules, and motion principles exist nowhere. Token comments describe
  *what* a color is ("bright pink"), never *when to use it*.

This RFC establishes design governance (a design-language document, a lint
test that gates CI, semantic token naming) and executes the overhaul in
phases: token cleanup, the hard-coded-color sweep, the background-model
fix with real OSC-11 detection, border semantics, emoji replacement, and a
motion policy. Command-information-architecture work (grouped `/help`) is
included as a final, low-risk phase.

## 2. Motivation

### 2.1 The palette is a facade

The project convention (`AGENTS.md`) states "No hardcoded RGB — use
palette/design tokens". The intent is that every rendered color flows
through `Palette`, so the seven theme variants (dark, light,
dark-daltonized, light-daltonized, monochrome, dark-ansi, light-ansi)
actually re-theme the application. In practice:

```
$ grep -rn "Color::Blue\|Color::Red\|Color::Green\|Color::Yellow\
  │Color::White\|Color::GrayDark\|Color::DarkBlue" src/ui \
  │ grep -v design_tokens | grep -v theme_provider | wc -l
1039
```

The message list — the surface users look at all day — renders the "You"
label as `text(" 👤 You") | color(Color::Blue)`
(`user_text_message.cppm:153`). In a light terminal theme, `Color::Blue`
is a dark blue on white (readable but off-palette); in a reversed or
custom theme it can be near-invisible. The carefully maintained
`brief_label_you = rgb(122,180,232)` token is never consulted.

A prior automation attempt exists (`scripts/tokenize_colors.py`): it maps
hard-coded colors to the *nearest palette entry by Euclidean distance*.
That approach is wrong — it maps by appearance, not semantics (a blue used
for "You" must map to `brief_label_you`, not to whichever blue token is
closest in RGB space) — and its embedded palette copy is already stale
relative to `design_tokens.cppm`. This RFC supersedes it with a semantic,
human-reviewed sweep and a lint gate that prevents regression.

### 2.2 The background seam

`fullscreen_layout.cppm` paints `palette.background = rgb(32,33,36)` on the
scroll region but cannot paint the 1-row top padding spacer (FTXUI `vbox`
has no padding; the spacer is an empty element). On terminals whose
background is not exactly `rgb(32,32,32)`-ish black, the padding row shows
the terminal's real background as a faint horizontal bar. The current
"fix" is a comment explaining why the spacer must not be painted. Any
terminal with a non-black default background (light themes, custom
schemes, many macOS setups) exhibits the seam.

### 2.3 `auto` does not detect

`theme_provider.cppm:35`: `Auto = Dark`. The README and `/theme` command
present `auto` as the default and recommended theme. It performs no
detection. Users on light terminals get a dark palette with no way for the
app to know better.

### 2.4 Design decisions are not recorded

`docs/design/` documents the dialog system and the no-DI decision, but no
document states the visual language: there is no spacing scale, no surface
vocabulary, no color-usage rules, no motion principles. Without it, every
new component re-derives (or ignores) the conventions, which is exactly how
1039 literals and four ad hoc border styles accumulated.

## 3. Goals

- **G1.** Every color rendered by `src/ui/` resolves through a `Palette`
  token. Zero ANSI/hex/RGB literals outside the two token-definition
  modules, enforced by a CI lint test.
- **G2.** Token names are semantic roles (`brief_label_you`,
  `surface_overlay_border`), not component-provenance labels
  (`loomBlue_FOR_SYSTEM_SPINNER`).
- **G3.** The root canvas is transparent: the application paints no
  full-screen background, and the background seam is impossible by
  construction. Filled surfaces (bubbles, selections, dialogs) remain
  intentional, token-driven shapes.
- **G4.** `auto` theme performs real terminal-background detection via
  OSC-11, best-effort with a documented fallback.
- **G5.** A three-level surface vocabulary (Canvas / Inset / Overlay)
  replaces ad hoc border selection; every bordered call site expresses a
  semantic level.
- **G6.** No emoji in `src/ui/`; semantic glyphs come from
  `design_figures`.
- **G7.** A motion policy: animation signals state change only, and
  `Accessibility::reduced_motion` is honored at every consumer.
- **G8.** `docs/design/design-language.md` records the visual language and
  is referenced from `AGENTS.md`.
- **G9.** `/help` output is grouped by functional category with
  frequency-weighted ordering.

## 4. Non-Goals

- **No visual redesign of the product.** The overhaul is governance +
  consistency. Where a hard-coded literal already matches a token's
  designed value, the sweep is appearance-preserving. Where it does not,
  the token wins (that is the point), but no new visual concepts (new
  colors, new components, new layouts) are introduced.
- **No new theme variants.** Seven variants are enough; the work makes the
  existing ones actually apply.
- **No re-theming of `src/services/`, `src/tools/`, or non-UI code.** Only
  `src/ui/` renders to the terminal.
- **No replacement of FTXUI.** All constraints (no padding primitive,
  component lifetime rules, event-driven model) stand.
- **No migration of user settings.** Theme names stay identical; only
  internal token identifiers change.

## 5. Proposal

### 5.1 Design-language document (`docs/design/design-language.md`)

The governance anchor. Contents:

1. **Color usage rules**
   - Color is never the sole information channel. Every color-coded state
     (subagent identity, diff polarity, permission level, mode) has a
     glyph, label, or text prefix carrying the same semantics.
   - Semantic token tiers: `text_*` (typography), `surface_*` (fills),
     `border_*` (edges), `accent_*` (brand/state), `diff_*`, `mode_*`.
     Every token documents its *usage* ("user message bubble fill"), not
     just its value.
   - The 8 subagent identity colors become an indexed palette
     (`subagent_identity[8]`); identity is additionally carried by the
     agent's name prefix, never hue alone.
2. **Surface vocabulary** — three levels (see 5.4).
3. **Spacing scale** — vertical rhythm in terminal rows: transcript rows
   separated by 0 or 1 blank rows (documented per message class); dialog
   padding 1 row/2 cols; no other ad hoc spacers.
4. **Typography** — weight/emphasis conventions (bold for labels, dim for
   metadata, no underline outside links).
5. **Motion policy** (see 5.6).
6. **Glyph policy** — `design_figures` is the single glyph source; emoji
   banned; new glyphs require a doc entry.

### 5.2 Token governance and cleanup

**Renames (semantic names, deprecated aliases kept one release):**

| Current | Replacement | Rationale |
|---|---|---|
| `loom_blue` / `loom_blue_shimmer` | `spinner` / `spinner_shimmer` | The only consumer is the system spinner |
| `subagent_red`…`subagent_cyan` (8 fields) | `subagent_identity[8]` array | Indexed palette; name prefix carries identity |
| `clawd_background` | `surface_canvas` | Provenance name → role |
| `professional_blue`, `chrome_yellow` | fold into nearest semantic token or delete if unused | No consumer survives the sweep |
| `rainbow_red`…`rainbow_violet` (7 fields) | `rainbow[7]` array only | Triple storage (array + fields + shimmer fields) collapses to array + shimmer array |
| `auto_accept` | `mode_auto_accept` | Mode tier naming |
| `plan_mode`, `ide`, `remember`, `fast_mode` | `mode_*` tier | Consistency |

**Alias audit:** `text_muted = muted`, `text_link = suggestion`,
`permission == suggestion == info` (dark), `auto_accept == merged` (all
variants). Aliases that exist only "so future palettes can diverge"
(speculative generality) are deleted; divergence, when needed, happens by
adding a token with a documented consumer. `border_subtle/default/accent/
error` and `surface_hover/selected/bash/memory` are kept — they have real
call sites after the sweep.

**Lint test** (`tools/arch/test_design_tokens_lint.py`, wired into ctest
like the existing `test_tll_lint.py`):

- Forbidden outside `design_tokens.cppm` / `theme_provider.cppm`:
  `Color::RGB(`, `Color::Palette16`, `Color::Palette256`, and the ANSI
  named-color set (`Color::Black`…`Color::White`, `*Light`, `GrayDark`,
  `DarkBlue`, …).
- Forbidden in `src/ui/`: emoji literals (Unicode emoji ranges), with an
  allowlist for test fixtures and docs.
- Token-struct rule: no field whose name contains `FOR_` (the provenance
  suffix may not appear in new code).
- The two token-definition modules are the *only* files allowed to name
  concrete colors.

### 5.3 Hard-coded color sweep

The 1039 sites are migrated area by area. **Mapping is semantic, not
distance-based**: for each call site the reviewer identifies the UI role
(user label, tool border, diff addition, metadata, selection, …) and maps
to the corresponding token. Where no token exists, one is added with a
usage comment (this is the *only* sanctioned way the token count grows).

Sweep order (by blast radius, smallest first to keep the tree green):

1. `src/ui/widgets/`, `src/ui/chrome/`
2. `src/ui/messages/` (largest surface; includes the `👤 You` label →
   `brief_label_you`)
3. `src/ui/dialogs/`, `src/ui/permissions/`
4. `src/ui/screens/`
5. `src/ui/features/` (agents, teams, tasks, plugins, mcp, hooks)
6. `src/ui/prompt/`, `src/ui/app/`

Each area is one commit: lint-clean for that area, debug build green,
affected golden snapshots regenerated via `LOOM_UPDATE_SNAPSHOTS=1` with
the diff reviewed (snapshot changes are expected where a literal's
effective value differed from the token's designed value; each such change
is listed in the commit message).

`scripts/tokenize_colors.py` is deleted — its distance-based approach and
stale palette copy are superseded.

### 5.4 Surface vocabulary

Three semantic levels, expressed by one helper
(`surface_box(element, Surface::…)`, replacing the radius enum):

| Level | Style | Usage |
|---|---|---|
| `Canvas` | no border, no fill | Transcript rows, root regions |
| `Inset` | light border, token fill optional | Secondary content inside a flow: tool results, attachments, collapsed cards |
| `Overlay` | rounded border, token fill | Floating/layered content: dialogs, menus, popovers, pills |

`borderDouble` and `borderHeavy` are removed from the vocabulary. The two
remaining call sites (`plugin_settings_dialog.cppm`, `plugin_dialog.cppm`)
migrate to `Overlay`. Rationale: in a character grid, double/heavy borders
read as "important box", which is a judgment every call site currently
makes ad hoc; two levels of "box" (Inset vs Overlay) cover the real
distinction (in-flow vs layered).

### 5.5 Background model and OSC-11

**Root canvas goes transparent.** No module paints a full-screen
background. The scroll-region spacer row needs no special case because
nothing paints the rows around it either — the seam is impossible by
construction. Filled surfaces (user bubble, selection, dialog body, bash
output card) keep explicit `bgcolor(token)` — they are intentional shapes
floating on the terminal's own background.

**OSC-11 detection** (`src/ui/chrome/terminal_io.cppm`):

- On startup (interactive TTY only; skipped under `--headless`,
  `--simple-ui`, CI, or when `NO_COLOR` is set), emit
  `ESC ] 11 ; ? ESC \` and read the response with a 100 ms deadline on a
  side thread.
- Parse the response (`rgb:RR/GG/BB`). Compute relative luminance; pick
  `ThemeVariant::Light` for luminance > 0.5, else `Dark`.
- On timeout / unsupported / parse failure: fall back to `Dark` and record
  a one-line debug trace. The fallback is documented in `--help` and the
  design doc.
- The detected variant feeds the existing `set_theme()` path; nothing else
  changes. `auto` becomes a real variant: `palette_for_variant(Auto)`
  resolves through the detected value stored at startup.

Daltonized/monochrome/ansi variants remain explicit user choices — auto
detection never overrides an explicit `--theme`.

### 5.6 Emoji replacement

15 sites (audited via the lint's emoji detector). Each emoji is replaced
by a `design_figures` glyph or a styled text label:

- `👤` (user label) → removed; the label is the styled text `You` in
  `brief_label_you` (the glyph adds nothing the label doesn't say).
- Status/state emoji (✅ ❌ ⚠️ 🔧 in tool/permission/status contexts) →
  existing figures glyphs (`kCheck`, `kCross`, `kWarning`, `kWrench`-class
  glyphs; missing glyphs are added to `design_figures` with doc entries).

### 5.7 Motion policy

- Animation is permitted **only** to signal ongoing state change:
  spinner (work in progress), shimmer sweep (progress/streaming), pulse
  (attention required). Static decorative shimmer on idle elements is
  removed.
- `Accessibility::reduced_motion` is checked at every consumer: spinner
  renders a static glyph, shimmers render their base color. A lint test
  enumerates shimmer/spinner consumer sites and asserts each has a
  `reduced_motion` branch (structural check on the call sites).
- Shimmer tokens with no remaining consumer are deleted.

### 5.8 Command information architecture

`/help` output is grouped into categories (Core, Sessions & Context, Git &
Review, Agents & Planning, UI & Customization, MCP & Integrations), each
sorted by a documented frequency weight (core session commands first).
The command registry gains an optional `category` field; uncategorized
commands sort last under "Other". No command is renamed or removed.

## 6. Alternatives

### 6.1 Keep the palette facade, just fix the worst sites

Rejected. Without a lint gate, the literals regress within weeks — the
1039 count accumulated *with* the "no hardcoded RGB" convention already
written down. The convention needs enforcement, and enforcement needs the
sweep to reach zero first.

### 6.2 Distance-based auto-tokenization (`tokenize_colors.py`)

Rejected (see 2.1). Appearance-nearest mapping loses semantics: `Color::Blue`
for "You" and `Color::Blue` for a hyperlink are different roles. The
script's stale embedded palette demonstrates the second problem: any
automated copy of the palette drifts from the source of truth.

### 6.3 Keep the painted background, fix the spacer

Rejected. Painting `rgb(32,33,36)` on a terminal whose background is
anything else is wrong by design — the app fights the user's environment.
Transparency + OSC-11 adapts to the environment instead. The spacer
comment in `fullscreen_layout.cppm` is deleted along with the paint.

### 6.4 Full OSC-4/OSC-11 palette adaptation (query all 16 ANSI colors)

Considered, deferred. Querying the full 16-color palette and generating a
matched variant is a research project with diminishing returns: the
palette already defines explicit RGB values for every token, so only the
light/dark *decision* needs the environment. OSC-11 alone answers it.

### 6.5 Remove shimmer entirely

Considered. Rejected for now: the streaming shimmer and spinner carry real
state (work in progress), and `reduced_motion` already exists for users
who want it off. The policy (5.7) trims decorative shimmer while keeping
stateful motion.

## 7. Phases and graduation criteria

| Phase | Scope | Graduation criterion |
|---|---|---|
| P0 | This RFC + `design-language.md` + lint test (rules only, not yet enforced) | RFC implementable; doc merged; lint runs in ctest as non-blocking warning |
| P1 | Token renames + alias audit + array collapse | Debug+release+asan green; no `FOR_` token names; token count documented |
| P2 | Color sweep, 6 area commits (5.3) | Lint enforced (blocking): zero literals outside token modules; snapshots reviewed |
| P3 | Transparent canvas + OSC-11 + real `auto` (5.5) | Manual verification matrix (5 terminals × light/dark); seam comment deleted; headless unaffected |
| P4 | Surface vocabulary (5.4) + emoji (5.6) + motion policy (5.7) | No borderDouble/Heavy call sites; lint emoji-clean; reduced_motion branches asserted |
| P5 | `/help` grouping (5.8) | All commands categorized or under Other; help snapshot updated |

Each phase is independently revertible (one commit per phase, P2 split
into its six area commits).

## 8. Production Readiness Review

**Performance:** the sweep is a pure refactor of render-time color
selection — no new allocations on the render path (tokens are static
`Palette` objects; lookups are pointer derefs). OSC-11 adds one 100 ms
bounded startup probe on a side thread, off the render path, skipped in
headless/CI. The lint test is Python stdlib, ~1 s.

**Compatibility:** user-facing theme names unchanged. Internal token
identifiers are not a public API (no settings file references them).
Snapshot fixtures change where literals diverged from token values; each
change is reviewed, not blind-regenerated.

**Risks:**

| Risk | Mitigation |
|---|---|
| Semantic sweep maps a site to the wrong role | Area commits are small; snapshot diffs reviewed per commit; the design doc's token usage table is the reference |
| OSC-11 mis-detects (e.g. translucent background) | Luminance threshold conservative (0.5); explicit `--theme` always wins; fallback is Dark + debug trace |
| Terminal hangs on OSC query | 100 ms deadline on a side thread; probe disabled when stdin is not a TTY |
| Lint false positives on legit literal (e.g. a test) | Allowlist for tests/docs; token modules exempt by path |
| Snapshot churn hides a real regression | Snapshot diffs are attached to each area commit; any non-color diff fails the commit |

**Observability:** OSC-11 result/fallback logged via the existing debug
trace; lint failures name file:line and the forbidden token.

## 9. Rollout and rollback

- Rollout: phases land on `master` in order; P2's six commits land across
  separate CI runs so a regression bisects to one area.
- Rollback: `git revert` of the phase commit. P1 (renames) and P2 (sweep)
  are the only phases with cross-tree blast radius; both are pure
  refactors with no state migration, so revert is clean.
- Feature flags: none needed — no behavior changes except `auto`
  detection, which falls back to the previous behavior (Dark) on any
  uncertainty.

## 10. Testing and verification plan

- **Lint test** (`test_design_tokens_lint`): new ctest case; blocking from
  P2.
- **Existing suite (2058 cases)**: must stay green at every phase commit.
  Snapshot updates go through `LOOM_UPDATE_SNAPSHOTS` with reviewed diffs.
- **New unit tests**: OSC-11 response parser (rgb:RR/GG/BB variants,
  malformed input, luminance threshold); surface-level helper (each level
  emits the documented border style); reduced_motion consumer check.
- **Manual matrix (P3)**: dark terminal, light terminal, non-black
  background (iTerm custom), tmux (OSC unsupported → fallback),
  `--headless` (no probe). Verify: no seam, correct variant, explicit
  `--theme light` honored even on dark background.
- **Accessibility spot check**: monochrome + daltonized variants render
  the swept surfaces correctly (tokens, not literals, make this automatic).

## 11. Implementation History

| Date | Event |
|---|---|
| 2026-10-08 | Initial draft (provisional). Design audit of `src/ui/`: 1039 hard-coded color literals, `FOR_`-suffixed token names, transparent-canvas seam, `Auto = Dark` stub, ad hoc border styles, 15 emoji sites. |
