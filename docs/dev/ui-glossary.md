# UI glossary

Canonical vocabulary for the LOOM terminal UI. Use these terms consistently in
code identifiers, comments, commit messages, design docs, and review feedback.

**Why this exists:** the same concept has been called by multiple names
("渲染块" vs Block vs Row vs Element), and the same word has meant multiple
things ("Block" = markdown element / streaming fragment / thread wait / cursor
shape). A shared vocabulary prevents the cross-module miscommunication that
string- and shape-based couplings in this tree already make easy.

## Rules

1. **One concept, one term.** If you mean a markdown block-level element, say
   Block — not "render block", not "chunk", not "row".
2. **One term, one concept.** "Block" means a markdown block-level element. It
   does not mean a streaming text fragment, a UI section, or a thread wait.
3. **Framework terms are borrowed, not redefined.** Element, Component, Box,
   and Screen keep their FTXUI meanings. Do not use them for LOOM-specific
   concepts.
4. **Spec terms win.** Block follows CommonMark/GFM terminology. When in doubt,
   use the term the spec uses.

## Framework terms (FTXUI)

These are FTXUI framework concepts. Use them as-is; do not redefine or
shadow them with LOOM-specific meanings.

| Term | Meaning |
|---|---|
| **Element** | Immutable render-tree node (`ftxui::Element`). The output of rendering; has no state, handles no events. |
| **Component** | Stateful interactive unit (`ftxui::Component`). Holds state, handles events, produces Elements on render. |
| **Box** | Geometric rectangle (`ftxui::Box`): `x_min`, `x_max`, `y_min`, `y_max`. Layout geometry only — never a UI container. |
| **Screen** | Character-cell buffer (`ftxui::Screen`) that Elements are rendered into. |

## Project terms (LOOM UI)

### Content and rendering

| Term | Meaning | Canonical types | Do not use for |
|---|---|---|---|
| **Block** | A markdown block-level element: paragraph, code block, blockquote, list, heading, etc. Follows CommonMark/GFM terminology. | `BlockToken`, `render_block_*` | Rendered lines, streaming fragments, UI sections |
| **Row** | One rendered line in the message list. A Block may render as zero or more Rows. | `MessageRow`, `VisibleRow`, `RowGeometry` | Markdown Blocks |
| **Chunk** | A piece of streaming text received from the model during a response. | streaming text buffers | Markdown Blocks |
| **Envelope** | The chrome around a single message: origin dot + role label + content area + tool-call rows. | `RenderEnvelopeOptions` | — |
| **Element** | (Same as framework — an FTXUI render-tree node.) | — | — |

### Layout and UI structure

| Term | Meaning | Canonical types | Do not use for |
|---|---|---|---|
| **Panel** | A full-screen overlay UI that replaces the main content area: settings, MCP, tasks, diff, help, permissions. | `panels.cppm` panel types | Layout regions within a screen |
| **Pane** | A layout region within a screen, computed by the layout engine. | `layout.cppm` Pane geometry, `dialog_frame` Pane | Full-screen overlays |
| **Area** | One of the 12 UI module directories: `foundation/`, `chrome/`, `widgets/`, `visual/`, `messages/`, `dialogs/`, `permissions/`, `prompt/`, `screens/`, `features/`, `tools/`, `app/`. | directory structure | — |
| **Box** | (Same as framework — `ftxui::Box` geometric rectangle.) | — | UI containers |

### State

| Term | Meaning | Canonical types |
|---|---|---|
| **Store** | A state shard owned by `ReplScreenState`, grouping related fields by responsibility. | `MessagesStore`, `ChromeStore`, `DialogStore`, `PromptStore`, … |
| **Screen state** | The aggregate `ReplScreenState` that owns all Stores and drives rendering. | `ReplScreenState` |

### Message-list features

| Term | Meaning | Canonical fields |
|---|---|---|
| **Divider** | The unseen-messages anchor line, inserted on first scroll-away from bottom. | `divider_index`, `unseen_divider` |
| **Pill** | The "N new messages" indicator that appears while scrolled away from bottom. | `pill_visible`, `new_messages_pill_box` |
| **Notice** | A transient notification row inserted into the message list (stash notice, context notice, etc.). | `StashNotice`, `ContextNotice` |

## Deprecated and forbidden terms

| Term | Problem | Use instead |
|---|---|---|
| **渲染块** / "render block" | Ambiguous: could mean Block, Row, or Element. Has no corresponding code identifier. | Block (markdown), Row (rendered line), or Element (FTXUI node), depending on meaning |
| **"Box"** for a UI container | Conflicts with `ftxui::Box` (geometric rectangle). | Panel, Pane, or Section |
| **"Block"** for a streaming text fragment | Conflicts with markdown Block. | Chunk |
| **"Block"** for a UI section | Conflicts with markdown Block. | Section or Panel |
| **"Panel"** for a layout region | Conflicts with Panel = full-screen overlay. | Pane |
| **"Pane"** for a full-screen overlay | Conflicts with Pane = layout region. | Panel |

Note: "block" as a **verb** (to wait on a condition variable, e.g. "block
until a response arrives") is standard C++ and remains correct in comments.
The deprecation applies to "block" as a **noun** referring to a UI or content
concept.

## Adding new terms

When introducing a new UI concept:

1. Check this glossary — an existing term may already cover it.
2. If the concept is new, add it here in the same PR that introduces the code.
3. Prefer spec terms (CommonMark, FTXUI) over inventing new vocabulary.
4. Avoid generic words that already have framework meanings (Box, Element,
   Component, Screen) unless you are referring to the framework concept.
