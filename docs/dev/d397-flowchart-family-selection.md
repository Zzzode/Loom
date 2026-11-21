# D-397 — next-family selection: flowchart (measured) and the theme/look fork

Date 2026-11-21 · local, not pushed · follows RFC 0035 closure (`3b1fa1d2`)
and the Tech Lead 637 directive (pick the family with the most remaining
fallbacks, document the selection basis).

## 1. Selection basis (mechanism, not preference)

Measured from `tools/mermaid/fallback_roster.json` (874 fallbacks,
44 categories) by path prefix:

| family | fallbacks | note |
|---|---|---|
| **flowchart** | **240** | largest by far |
| requirement | 148 | RFC 0035-style config/body mix |
| git | 77 | gitGraph config semantics |
| state-diagram-v2 | 70 | config-heavy composite/notes |
| swimlanes | 46 | net-new layout |

flowchart is selected: it carries 27% of all remaining fallbacks and its
renderer already owns the largest body-feature surface.

## 2. Measured blocker composition of the 240 flowchart fallbacks

Method: a throwaway render probe (never committed; evidence logs under
/tmp/flow_probe*.log) rendered every fallback fixture through the public
`render_markdown` path and re-rendered transformed variants that delete one
class of input, then recorded the first variant that draws. Variants were
independent for the body slice and cumulative for the config slice.

| bucket | rows | what it needs |
|---|---|---|
| config keys in YAML frontmatter | 67 | theme/look 44, other tuning keys 19, elk layout 3, inert-only 1 |
| body capability (subgraph nesting, special links, shapes, collapsed views) | 82 | genuinely new geometry/features |
| edge/annotation metadata | 37–65 | `@{ animation }` (inert) mixed with `@{ shape }` / `@{ view: collapsed }` (capability) |
| style statements (`classDef`/`class`/`style`/`linkStyle`/`click`) | 41 (13 stand-alone) | styling/interactivity is not projected by the terminal |
| icon fonts (`fa:fa-*`) | 5 | glyph projection |
| `<br>` line breaks | 8 | multiline labels |
| everything else | 2 | residual |

Cross-check: stripping **only** the four inert body classes (styles,
animation, comments) recovers **13** rows. So the policy-free body slice is
small; the family's largest recoverable bucket sits in config frontmatter and
is dominated by **theme/look (44 of 67)**.

## 3. The fork

- **Option A — theme/look disposition policy (D-066 F1) first.**
  Unlocks the largest single bucket here (44) and the same class in other
  families (`look_handdrawn` 19, `theme` keys elsewhere). But it is a
  faithfulness decision: upstream applies theme colors in SVG and
  `look: handDrawn` changes stroke geometry. Admitting them means declaring
  either (a) colors are a documented non-projection (consistent with the ten
  validated-and-ignored color roles and RFC 0035's `dataLabelColor`), or
  (b) hand-drawn geometry is capability debt while color-only themes are
  admitted.
- **Option B — policy-free flowchart slice now** (13 body rows + the 19
  "other" config tuning rows after a per-key fidelity review). Small, safe,
  but leaves the family's dominant bucket untouched.
- **Option C — switch family** to one whose largest bucket is policy-free.
  The second-largest (requirement 148) is config/body mixed and its F0
  slice is already exhausted; git (77) needs gitGraph config semantics and
  the E2a cohort; state-v2 (70) is config-heavy in the same theme-sensitive
  way. No family offers a larger policy-free bucket than flowchart's 13.

Recommendation: **A then B**, i.e. a theme/look RFC scoped to the flowchart
bucket first, with color-only admission and hand-drawn geometry as an
explicit capability non-goal, then the small policy-free body slice in the
same RFC's later phases.

**Decision (recorded 2026-11-21, before RFC 0036):** follow the recommendation
under the RFC 0035 OQ-1 precedent. Color palettes and tuning metrics are the
same class as the eleven validated-and-ignored color roles — upstream applies
them in SVG and the terminal never projects color — so they are admitted as
inert input. Anything that changes projected geometry (`look: handDrawn`
strokes, `curve` routing, `elk` layout, `@{shape}`, collapsed `@{view}`,
icon fonts, `<br>`) stays capability debt, because booking it as a projection
would repeat the pattern OQ-1 rejected. No owner ruling is needed for this
split: it applies an existing ruling rather than opening a new policy
surface. `look` therefore remains a future F1 item of its own.

## 4. Preserved debts (unchanged by this memo)

1. xychart rotation 3 rows — real geometry needed; 45° differs upstream,
   120/−100 clamp only by coincidence (OQ-1 ruling).
2. narrow_40 — every family, including xychart, still falls back at 40
   columns; the wide-240 admittance is not a narrow promise.
3. ER107 B3 gates (`MATRIX_API_CONFIRMED`, `REVIEWED_HELPER_SHA`/
   `kB3ReviewStatus`, `WRITE_REVIEW_APPROVED`) — untouched; flipping needs
   owner authorization.

## 5. Addendum after the RFC 0036 design review (2026-11-21)

The independent review of RFC 0036 falsified three load-bearing measurements
in §2; the corrected picture is materially different:

1. **18 of the 67 "config frontmatter" rows are frozen E6c `official_matrix`
   mandatory-gap fixtures.** Their body_diagnostic rows report `error`
   (`unsupported-node-shape:cylinder/circle/trapezoid`, group-edge endpoints),
   so the probe's "draws after config removal" was wrong geometry, not a draw.
   Excluding them leaves 22 config candidates, and 10 of the 33 originally
   admitted rows were gap rows.
2. **Styling is already projected.** RFC 0031 FS-3a graduated `classDef`/
   `class`/direct-style to real terminal cells and a legend
   (`src/ui/visual/mermaid_impl.cpp:21-22`, `resolve_flow_styles`,
   `paint_flow_styles`, `tests/ui/test_mermaid_fs3_style.cpp`). Skipping those
   statements would change 23 already-drawn frames; the "inert body
   statements" phase is void.
3. **The 13-row body slice is not inert.** Certified body reasons are
   `unsupported-flow-statement` (9), `unsupported-statement` (3) and
   `configuration-directive` (3); the style rows are FS-3a grammar limits and
   the rest are `%%{init}%%` directives, not comments. Zero rows are recovered
   by comments or `@{animation}`.

Further row-by-row checks on the remaining candidates found that even the
clean-looking theme rows carry hidden body capabilities: the legacy-theme
graph bodies contain `fa:fa-car` icon glyphs, the redux-color-subgraph bodies
are collapsed/nested container cases, and the other candidates use
`@{shape}`, `<br>` wrapping or `wrappingWidth`/`minNodeWidth` geometry. The
existing flowchart profile already accepts most config leaves (`layout: dagre`
and `elk.rectpacking`, `fontFamily: courier`, `securityLevel: loose`,
`logLevel: 1`, `htmlLabels`, `useMaxWidth`, `titleTopMargin: 0`,
`diagramPadding: 0`, `nodeSpacing`/`rankSpacing`, `look: handDrawn` preview);
the only genuinely new leaves measured are `theme` and
`securityLevel: sandbox`, and the rows carrying them are the capability-bound
ones above.

**Corrected conclusion:** flowchart's remaining 240 fallbacks are essentially
all body capability (82+), geometry-changing input (`look`/`curve`/`elk`/
shapes/icons/`<br>`), or frozen gap fixtures. An inert-input RFC would
promote at most a handful of rows and would risk unfaithful frames; RFC 0036
as drafted is withdrawn (never accepted, never implemented).

Next-family choice therefore needs a decision that the mechanism alone cannot
make, because the largest family has no clean slice:

- (A) start flowchart **body capability** work (subgraph nesting 28, icons 27,
  special link markers 9, multiline 2) as its own program — highest volume,
  highest cost;
- (B) open the **F1 theme/look policy** RFC, whose look half is geometry
  (`look: handDrawn` strokes) and therefore needs the same fidelity test;
- (C) measure the other families' clean config slices first (requirement 148,
  git 77, state-v2 70) and pick the largest *clean* bucket rather than the
  largest raw family.
