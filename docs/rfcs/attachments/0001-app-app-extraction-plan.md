# RFC 0001 — Phase C follow-up: `cc.ui.app.app` extraction plan

Attached 2026-09-29. Read-only design; no `src/` changes. Companion to
RFC 0001 §4.3 (Phase C) and the §12 implementation history. This plan
finishes the one interface Phase C explicitly deferred: `cc.ui.app.app`
(`src/ui/app/app.cppm`).

**Verification gate (2026-09-29 directive):** the local dual-preset build
(`local-linux` + `local-linux-release`, `-Werror`) + serial `ctest -j1` +
truecolor goldens. GitHub CI / macos-14 is **not** a gate and must not be
required. (The §12 Phase C rows predate this directive and cite macos-14;
new work verifies locally.)

---

## (a) Body inventory

### Current state

`src/ui/app/app.cppm` is **1,137 LOC** (50,646 bytes). The semantic
inline-body count from `tools/arch/inline_def_check.py` (the same counter
the C3 ratchet uses) is **58**, frozen at 58 in
`tools/arch/inline_def_baseline.txt:12` with **no flags** (not `c1`, not
`c2-done`). The OQ-4 loose line-heuristic baseline was 83 / 1,189 LOC
(`attachments/0001-oq4-baselines-and-utils-mapping.md:72`); the file has
shrunk since because bodies were already being shed to impl units before
Phase C formalized the pattern.

The 58 semantic bodies break down as follows (line numbers are
`app.cppm`):

| Category | Count | Bodies |
|---|---:|---|
| Env/config helpers (free fns) | 4 | `non_empty_env` (88), `first_non_empty_env` (95), `parse_bool_text` (102), `parse_int_text` (108) |
| Text/UTF helpers (free + static) | 8 | `trim_ascii_copy` (116), `summarize_agent_description` (128), `lowercase_ascii` free (143), `ascii_isspace` (159), `lowercase_ascii` static (641), `utf8_continuation` (677), `utf16_code_unit_count` (681), `rough_js_token_count` (732) |
| Autocomplete helper (free fn) | 1 | `token_around_cursor` (163) |
| Skills-menu (static + member) | 7 | `skill_source_order` (661), `is_visible_skills_menu_source` (669), `skills_menu_token_estimate` (738), `collapse_home_path` (748), `skill_source_group_title` (759), `FormatSkillsMenuOutput` (776), `OpenSkillsMenu` (825) |
| Rendering / animation | 2 | `StartUiAnimationTicker` (491), `PostRenderEvent` (540) |
| Local-command / local-JSX | 5 | `AppendLocalMessagesToScreenState` (560), `AppendLocalCommandInputMessage` (582), `AppendLocalCommandMessage` (592), `ClearActiveLocalJsxCommand` (621), `DismissLocalJsxCommand` (629) |
| Statusline | 1 | `TriggerStatuslineUpdate` (861) |
| Streaming state | 1 | `is_streaming_thinking_visible` (396) |
| Screen lifecycle | 1 | `set_screen` (917) |
| Testing accessors (public) | 28 | `is_query_running_for_testing` (961) … `teams_overview_count_for_testing` (1122) |
| **Total** | **58** | |

Of these, **29 are non-testing bodies** (the first nine rows minus
`set_screen`) and **29 are trivial/testing bodies** (28 testing accessors
+ `set_screen`).

### Already out-of-line (context)

The interface has already been heavily extracted. Eleven impl units for
`cc.ui.app.app` plus one `:impl` partition exist (wired in
`src/cmake/targets/cc_ui.cmake:167-186`):

| Unit | LOC | Holds |
|---|---:|---|
| `app_autocomplete.cpp` | 1435 | `RefreshAutocompleteSuggestions`, `~AppAdapter`, `Render`, `OnEvent`, `ActiveChild` |
| `app_team.cpp` | 743 | `TeammateState` PIMPL, teammate inbox/permission, live-teams projection |
| `app_agent_menu.cpp` | 599 | agents menu, `SyncState`, `ConsumePendingResult`, `WaitForInFlightPastes`, `get_permission_callback` |
| `app_constructor.cpp` | 586 | ctor, `BuildStatuslineInputJson`, `ExecuteStatuslineCommand` |
| `app_handle_submit.cpp` | 522 | `HandleSubmit`, `HandleCommand` |
| `app_extra_methods.cpp` | 475 | `RunLocalBashCommand`, `ProjectRuntimeMetadataToScreenState`, `ApplyMessageCollapsePipeline`, `SpawnPasteWorker`, `ProcessCompletedPastes` |
| `app_message_projection.cpp` | 381 | `project_message`, `project_messages`, `RenderMessage` |
| `app_store_bridge.cpp` | 194 | `create_typed_app_store`, AppStore bridge |
| `app_settings.cpp` | 126 | `SettingsState` PIMPL, settings accessors |
| `app_run.cpp` | 118 | `RunApp`, `extern "C"` bridge |
| `app_prompt_suggestion_wiring.cpp` | 87 | `wire_prompt_suggestion_hook` |
| `app_impl.cppm` (`:impl`) | 119 | `AppImpl` struct, vim/exit-handler/AppStore accessors, `construct_impl`, `AppImplDeleter` |

The six `app_dialog_registration_*.cpp` units belong to a **separate
module** (`cc.ui.app_dialog_registration`) and are out of scope.

This plan moves the remaining 29 non-testing inline bodies (and
optionally 2-3 borderline testing accessors) into impl units, leaving
only trivial accessors inline — the same exit state every other Phase C
interface reached.

---

## (b) Proposed impl-unit partition

Four new impl units + three folds into existing units. All new units are
plain `module cc.ui.app.app;` implementation units in the `cc_ui`
PRIVATE `target_sources` block (`cc_ui.cmake:167`), never FILE_SET — the
C3 rule proven across all ten batches.

### New units

**1. `app_helpers.cpp` — 12 bodies (env + text/UTF).**
`non_empty_env`, `first_non_empty_env`, `parse_bool_text`, `parse_int_text`,
`trim_ascii_copy`, `summarize_agent_description`, `lowercase_ascii` (free),
`ascii_isspace`, `lowercase_ascii` (static member), `utf8_continuation`,
`utf16_code_unit_count`, `rough_js_token_count`.
Closure: `<cctype>`, `<cstdlib>`, `<cmath>`, `<cstdint>`, `<string>`,
`<string_view>`, `<optional>`, `<initializer_list>`. No FTXUI, no `cc.ui.*`
imports beyond the primary. (`<initializer_list>` for `first_non_empty_env`;
`<cstdint>` for `std::uint32_t` in `utf16_code_unit_count`.) The two
`lowercase_ascii` definitions (free `app.cppm:143` and static member
`app.cppm:641`) are byte-identical; move both verbatim (different scopes,
no ODR issue). Optional consolidation (static delegates to free) is a
behaviour-preserving cleanup, review-gated, not required.

**2. `app_skills_menu.cpp` — 7 bodies.**
`skill_source_order`, `is_visible_skills_menu_source`,
`skills_menu_token_estimate`, `collapse_home_path`,
`skill_source_group_title`, `FormatSkillsMenuOutput`, `OpenSkillsMenu`.
Closure: `acsrc::SkillSuggestionData` (already keep-imported in the
primary via `cc.ui.prompt.autocomplete_sources`), `<ranges>`, `<format>`,
`<cstdlib>` (`std::getenv` in `collapse_home_path`), `<cmath>`
(`std::llround` in `rough_js_token_count`), `screen_state_`. `OpenSkillsMenu` mutates `screen_state_` and calls
`PostRenderEvent()` — fine once `PostRenderEvent` is out-of-line (see
unit 3).

**3. `app_animation.cpp` — 2 bodies.**
`StartUiAnimationTicker`, `PostRenderEvent`.
`PostRenderEvent` is called from **six** impl units
(`app_handle_submit`, `app_extra_methods`, `app_team`, `app_constructor`,
`app_agent_menu`, `app_autocomplete`) plus **three** inline bodies in the
current interface (`StartUiAnimationTicker` `app.cppm:531`,
`AppendLocalCommandMessage` `app.cppm:602`, `OpenSkillsMenu` `app.cppm:835`).
Defining it out-of-line is safe: it is a private
non-static member, and every caller is itself a member function (private
access is within member functions regardless of TU). `StartUiAnimationTicker`
is the largest inline body (the `std::jthread` lambda, `app.cppm:491-538`);
move the whole function verbatim, preserving the lambda capture (`this`),
the `stop_token` loop, and the `TriggerStatuslineUpdate()` call.
Closure (explicit — impl units do not inherit the primary's GMF, and this
is the highest-risk unit): `<chrono>`, `<thread>`, `<atomic>`,
`<ftxui/component/event.hpp>` (`Event::Custom`),
`<ftxui/component/screen_interactive.hpp>` (`ScreenInteractive::Post`).

**4. `app_local_command.cpp` — 5 bodies.**
`AppendLocalMessagesToScreenState`, `AppendLocalCommandInputMessage`,
`AppendLocalCommandMessage`, `ClearActiveLocalJsxCommand`,
`DismissLocalJsxCommand`.
Closure (explicit — impl units do not inherit the primary's GMF):
`local_command_messages_`, `screen_state_`, `repl::MessageDisplayEntry`
(arrives via the `cc.ui.screens.repl_state` keep-import), `<chrono>`,
`<cstdio>`, `<cstdint>` (`std::uint64_t` in `s_local_seq`), `<utility>`
(`std::move`). Carries the function-local `static std::uint64_t s_local_seq`
(`app.cppm:566`) — stays function-local in the moved body (C3 rule:
"function-local statics stay function-local"; one strong definition in
exactly one TU).
**Default-argument hazard — `AppendLocalCommandMessage`.** Its current
in-class definition carries `bool is_error = false` (`app.cppm:592`). The
"move verbatim" recipe is a trap here: (a) keeping the in-class definition
with the default *and* writing an out-of-line definition that repeats
`= false` is ill-formed (a default argument cannot be redefined by a later
declaration in the same scope); (b) dropping the in-class declaration
entirely loses the declaration for cross-TU callers
(`app_handle_submit.cpp:433,462,507,512,515,518`, `app_agent_menu.cpp:387`).
The required split: the interface keeps the declaration
`void AppendLocalCommandMessage(std::string message, bool is_error = false);`
(default on the declaration, as today), and the impl unit defines it
out-of-line **without** the default:
`void AppAdapter::AppendLocalCommandMessage(std::string message, bool is_error) { … }`.
All current callers pass the second argument explicitly
(`app.cppm:638`, `app_agent_menu.cpp:387`,
`app_handle_submit.cpp:433/462/507/512/515/518`), so the default is not
load-bearing today; it is kept on the declaration nonetheless to preserve
the public surface.

### Folds into existing units

| Body | Target | Rationale |
|---|---|---|
| `token_around_cursor` | `app_autocomplete.cpp` | Sole caller is `app_autocomplete.cpp:81` (autocomplete cluster) |
| `TriggerStatuslineUpdate` | `app_constructor.cpp` | Statusline cluster (`BuildStatuslineInputJson`, `ExecuteStatuslineCommand` already there); called from 3 impl units + `StartUiAnimationTicker` |
| `is_streaming_thinking_visible` | `app_autocomplete.cpp` | Sole callers are `app_autocomplete.cpp:1058,1206` (Render cluster) |

### Bodies kept inline (29)

`set_screen` (2-line trivial setter) + the 28 `*_for_testing` accessors.
These are the C1-allowed "trivial accessors" (the messages_list batch kept
22; repl_screen kept 0 only because it had none). Three are borderline
non-trivial — `wait_for_local_bash_for_testing` (joins a thread,
`app.cppm:977`), `messages_for_testing` (builds a vector, `app.cppm:1029`),
`autocomplete_suggestions_for_testing` (builds a vector, `app.cppm:1014`) —
and may move to an `app_testing_accessors.cpp` follow-up if the <30 headroom
feels tight. Keeping all 29 leaves the interface at **29 < 30** (C1-level)
with one slot of ratchet headroom.

### Singleton / template-boundary handling (C3 pattern)

Unlike the C3 template (`agent_runtime`), `cc.ui.app.app` has **no
singleton function and no member template**, so the `native_agent_store` /
`NativeAgentStore::update<Fn>` techniques are N/A. The analogous anchors
that must stay single-definition:

- **Vtable / typeinfo.** `AppAdapter` has virtuals (`Render`, `OnEvent`,
  `ActiveChild`, `~AppAdapter`). The key function is `~AppAdapter`, already
  defined out-of-line in `app_autocomplete.cpp`. Under clang modules the
  vtable/typeinfo anchor is emitted in the **interface unit** (`app.cppm.o`),
  not the key-function TU: `nm` shows strong `D` `_ZTV…AppAdapter` /
  `_ZTI…AppAdapter` in `app.cppm.o` and `U` (undefined) in
  `app_autocomplete.cpp.o` — the interface-unit ownership noted in the
  batch-6 record. The key-function-TU mental model does not apply here.
  The extraction must not make any virtual function inline in the
  interface. Verify with `nm` on `app.cppm.o`: one strong `vtable`/`typeinfo`
  symbol, zero duplicates.
- **Function-local static.** `s_local_seq` in
  `AppendLocalMessagesToScreenState` moves with its body to
  `app_local_command.cpp`; stays function-local (one strong definition).
- **Static member functions.** `lowercase_ascii`, `skill_source_order`,
  etc. are static members (no `this`); defined out-of-line as
  `returntype AppAdapter::name(...)`. No static data members exist.
- **Default arguments.** `HandleSubmit`'s `repl::InputMode::Normal` default
  (`app.cppm:846`) is already declarations-only. One extracted body **does**
  carry a default: `AppendLocalCommandMessage(std::string message,
  bool is_error = false)` (`app.cppm:592`, moving to `app_local_command.cpp`).
  The default stays on the in-class declaration; the out-of-line definition
  in the impl unit omits it (a default argument must not be redefined by a
  later declaration). See unit 4 for the full hazard analysis. For any
  future default, the same rule applies: default on the declaration only
  (C3 rule).
- **`inline` on moved free functions.** All 9 free functions being moved
  carry `inline` on their current definitions (`app.cppm:88-163`:
  `non_empty_env`, `first_non_empty_env`, `parse_bool_text`, `parse_int_text`,
  `trim_ascii_copy`, `summarize_agent_description`, `lowercase_ascii` (free),
  `ascii_isspace`, `token_around_cursor`). `inline` is dropped from both the
  interface declarations and the impl-unit definitions: the declarations stay
  exported in the interface (plain, no `inline`), and the definitions are
  plain (non-`inline`) in the impl unit. The out-of-line definition is the
  single strong definition; keeping `inline` would be harmless but pointless
  once the body is out of the interface.

---

## (c) Keep-import / compiler-defect hazards

### LLVM #184957 — textual-std for new impl units (the dominant hazard)

`app.cppm`'s global module fragment textually includes six FTXUI headers
(`app.cppm:8-13`), which pull libc++ textually. Under clang 22.1.8's
reduced-BMI writer (LLVM #184957, fixed in clang 23 / PR #179178, absent
in 22.1.x), an impl unit of this primary that `import std;` can hit
`operator new is ambiguous` — the writer mis-merges the global aligned
`operator new` across the textual libc++ and the std BMI.

This is not theoretical: **5 of the 11 existing `cc.ui.app.app` impl
units are already textual-std** because of it:

| Unit | Std mode | Why |
|---|---|---|
| `app_extra_methods.cpp` | textual | Phase A #184957 opt-out (explicit comment) |
| `app_handle_submit.cpp` | textual | Phase A #184957 opt-out (explicit comment) |
| `app_prompt_suggestion_wiring.cpp` | textual | Phase A #184957 opt-out (explicit comment) |
| `app_team.cpp` | textual | Flipped in follow-up c16 (2026-09-29, today): "must NOT `import std`" |
| `app_run.cpp` | textual | C headers (`<unistd.h>`, `<termios.h>`) in GMF + FTXUI |

(The task brief's "3 textual-std impl units from Phase A" is accurate for
the Phase-A vintage; `app_team.cpp` joined today in c16 and `app_run.cpp`
is textual for C-header reasons. The six `app_dialog_registration_*.cpp`
units are a separate module and do not count.)

**Stale comment to clean up.** The top-level `CMakeLists.txt:287` comment
still says the defect "manifested only in the three cc.ui.app.app impl
units" — the count is now five (the three Phase-A units + `app_team.cpp`
in c16 + `app_run.cpp` for C-header reasons). Update that comment when
this plan is implemented (or in the same commit as batch 1).

**Rule for the four new units:** start textual-std (self-contained C++
headers in the GMF, exactly like `app_team.cpp:24-46`). Only flip to
`import std;` after an empirical compile+link+ctest verification. The
Phase C batches showed FTXUI-GMF units *can* sometimes import std
(batches 6-9 had zero flips), but `app.app`'s specific primary closure is
demonstrably toxic (5 units already forced textual). Do not assume; test.

### Keep-imports (must stay in the interface for member declarations)

These imports name types that appear in `AppAdapter`'s member-variable
declarations or method signatures, so they cannot be shed without moving
the member itself:

| Import | Used for |
|---|---|
| `cc.types.types` (`app.cppm:20`) | `Message` (project_* / RenderMessage / ApplyMessageCollapsePipeline signatures), `ImageBlock` (`pasted_contents_` member `app.cppm:322`) |
| `cc.ui.screens.repl_state` (`app.cppm:24`) | `ReplScreenState` (`screen_state_` member `app.cppm:260`) |
| `cc.ui.prompt.autocomplete_sources` (`app.cppm:25`) | `SkillSuggestionData` / `PluginCommandSuggestionData` (`cached_skills_` / `cached_plugin_commands_` members `app.cppm:312-313`) |
| `cc.ui.visual.markdown` (`app.cppm:23`) | `::cc::ui::StreamingMarkdown streaming_markdown_` member (`app.cppm:363`) — global-qualified, same keep-import pattern as messages_list batch 7 |
| `cc.ui.messages.message_pipeline` (`app.cppm:27`) | `DedupTracker event_dedup_` member (`app.cppm:409`) |

### Shed candidates (verify empirically)

These imports have no type usage in the interface body today; they may be
shed once the bodies that use them move out — but each must be
compile-verified, because an apparently-dead import can be a #184957
`operator new` reachability keep-import (the C3 batch found
`cc.utils.team_helpers` was exactly this in `agent_runtime`):

| Import | Evidence it may be dead |
|---|---|
| `cc.ui.widgets.components` (`app.cppm:21`) | No `components::` qualifier or component type named anywhere in `app.cppm` (only the `using namespace` at line 82) |
| `cc.ui.widgets.all_components` (`app.cppm:22`) | Same — import line only |
| `cc.ui.features.teams.live_teammates` (`app.cppm:28`) | Only in comments (`app.cppm:902,1117`); real usage is in `app_team.cpp` |
| `cc.ui.dialogs.system` (`app.cppm:29`) | Only `screen_state_->dialog_queue.*` calls inside `has_pending_dialog_for_testing`'s body (`app.cppm:1111-1114`); the `DialogQueue` type itself arrives via `repl_state` |

Shed one at a time, compile, and if it fails restore with a keep-import
comment (the C3 empirical rule).

---

## (d) BMI payoff re-check

### The recorded deferral rationale

Three §12 entries record the deferral:
- Line 571 (batch 7 confirmation): "app.app deferred (zero BMI win)."
- Line 573 (batch 8 confirmation): "app.app deferred (zero producer BMI win)."
- Line 583 (Phase C complete): "`cc.ui.app.app` remains explicitly DEFERRED
  (zero producer-BMI win; edit-isolation only)."

### External importers today

`grep -rln "import cc.ui.app.app" src/ tests/ benchmarks/` returns **6
files**:

| Importer | Kind |
|---|---|
| `src/ui/app/app_impl.cppm` | module-internal (`:impl` partition) |
| `src/ui/app/app_constructor.cpp` | module-internal (impl unit) |
| `tests/test_ui_e2e.cpp` | external (test TU) |
| `tests/test_ui_render_pure.cpp` | external (test TU) |
| `tests/test_ui_runtime.cpp` | external (test TU) |
| `tests/test_ui_paste.cpp` | external (test TU) |

So **4 external importers, all test TUs**. No production module imports
`cc.ui.app.app` — it is a leaf of the UI graph (the app entry point).

### Judgment: the deferral still holds for BMI; the edit-isolation win is real

**Producer BMI — still ~zero win.** `app.cppm`'s BMI is dominated by (1)
the six FTXUI textual GMF includes and (2) the `AppAdapter` class layout
(~80 member variables including `jthread`s, `mutex`es,
`condition_variable`s, `map`s, and the `StreamingMarkdown` / `DedupTracker`
members). The 58 inline bodies are a small fraction. Moving 29 of them out
shrinks the BMI marginally — an estimated ~5-15%, not the 26-42% the
text_input / repl_screen batches measured (those interfaces' bodies *were*
the bulk of their BMI). The 4 external importers are all test TUs, so even
a smaller BMI yields negligible fan-out compile-time savings. The
"zero producer-BMI win" rationale is **still accurate**.

**Edit isolation — the real, understated win.** Today, editing any of the
58 inline bodies recompiles **17 objects**, not 7. clang-scan-deps emits
`cc.ui.app.app` as a required logical-name for **every** module impl unit
regardless of an explicit `import` statement — verified in the
`build/debug` `.ddi` dyndep files: all 11 impl units
(`app_autocomplete`, `app_team`, `app_agent_menu`, `app_constructor`,
`app_handle_submit`, `app_extra_methods`, `app_message_projection`,
`app_store_bridge`, `app_settings`, `app_run`,
`app_prompt_suggestion_wiring`) plus the `app_impl.cppm` partition require
the primary BMI. True count: 1 (interface) + 12 (module-internal) + 4 (test
TUs) = **17**. (The earlier "7 objects" figure counted only the 2
module-internal units with an explicit `import cc.ui.app.app;` statement
and missed the scan-deps fan-out.) After extraction, editing a moved body
recompiles **1 object** (the impl unit). `app.cppm` is the single most
central UI orchestrator and is edited frequently; 58 inline bodies ×
17-object fan-out is a meaningful daily developer-velocity cost that the
Phase C batches eliminated for every other god interface.

**Conclusion.** Proceed with the extraction, but frame the win honestly:
this is an **edit-isolation** change, not a BMI change. The producer-BMI
metric (`tools/arch/measure_bmi.py`) should still be run pre/post for the
record, but the expected delta is small; the graduation metric is the
"edit one body → recompiled objects" count trending from 17 to 1.

---

## (e) Batch breakdown with per-batch verification

Two batches, ordered by risk. Each batch is independently verified and
agent-reviewed (the CLAUDE.md rule: reviews are agent-run, never user-run).

### Batch 1 — pure helpers (19 bodies, zero thread/lifetime risk)

- New `app_helpers.cpp` (12) + new `app_skills_menu.cpp` (7).
- No `this` capture, no threads, no FTXUI rendering, no function-local
  statics. The lowest-risk 19 bodies.
- Shed-candidate import pruning may be attempted here (components /
  all_components / live_teammates), one at a time, compile-verified.

**Verification (local gate):**
1. `cmake --build --preset local-linux -j8` — clean, `-Werror`, 0 warnings.
2. `cmake --build --preset local-linux-release -j8` — clean, `-Werror`.
3. `ctest --preset local-linux -j1` — 1706/1706 (or current count).
4. Truecolor goldens byte-identical (no golden file touched; the helpers
   are string/env logic with no rendering surface).
5. `python3 tools/arch/inline_def_check.py` — 58 → 39, ratchet green,
   re-freeze with `--update` in the same commit.
6. `nm` strong-definition check on the moved bodies (one strong `T` each,
   zero cross-unit duplicates).
7. Independent adversarial agent review (body + string/char-literal
   multiset identity, ODR, public surface).

### Batch 2 — stateful bodies + folds (10 bodies, thread/lifetime risk)

- New `app_animation.cpp` (2) + new `app_local_command.cpp` (5).
- Folds: `token_around_cursor` → `app_autocomplete.cpp`,
  `TriggerStatuslineUpdate` → `app_constructor.cpp`,
  `is_streaming_thinking_visible` → `app_autocomplete.cpp`.
- Carries the `jthread` lambda (`StartUiAnimationTicker`), the
  `s_local_seq` function-local static, and the `PostRenderEvent`
  cross-unit member.

**Verification (local gate):** same 7 steps as batch 1, plus:
8. Thread-safety focus in review: `jthread` capture / `stop_token` loop
   preserved verbatim; `s_local_seq` stays function-local (one strong
   def); `PostRenderEvent` declaration stays in the class (it does —
   `app.cppm:540`).
9. `inline_def_check.py` — 39 → 29, re-freeze with `--update`, add
   `c2-done` flag to the baseline entry (29 ≤ 100 cap enforced; the
   interface is now C1-level at <30 though it was not one of the original
   C1 six, so `c1` is not added).
10. `nm` vtable/typeinfo check: one strong `vtable for AppAdapter` /
    `typeinfo for AppAdapter` in `app.cppm.o` (the interface unit) only —
    under clang modules the anchor is emitted in the interface unit, not
    the key-function TU (`app_autocomplete.cpp.o` shows `U`).

**Post-batch-2 state:** 29 inline bodies (28 testing + `set_screen`),
all trivial accessors; `c2-done` flag set; ratchet frozen at 29. The
interface is declaration-only apart from trivial accessors — the same
exit state as every other Phase C graduate.

---

## (f) Risk list

1. **LLVM #184957 textual-std (high likelihood, known workaround).**
   New impl units of `cc.ui.app.app` may hit `operator new ambiguous` if
   they `import std;` (primary GMF pulls libc++ textually via FTXUI). 5
   existing units are already textual-std; `app_team.cpp` was flipped
   *today* in c16. Mitigation: start all four new units textual-std with
   self-contained GMF headers (mirror `app_team.cpp:24-46`); only flip to
   `import std;` after empirical compile+link+ctest. Re-evaluate after
   the clang 23 toolchain upgrade (PR #179178 fixes #184957).

2. **Vtable / typeinfo duplication (medium).** Making any virtual
   function inline in the interface would emit a second vtable. All
   virtuals (`Render`, `OnEvent`, `ActiveChild`, `~AppAdapter`) are
   already out-of-line; the extraction adds no new virtuals. Mitigation:
   `nm` verifies one strong vtable/typeinfo in `app.cppm.o` (the interface
   unit — under clang modules the anchor lives there, not in the
   key-function TU `app_autocomplete.cpp`).

3. **Function-local static `s_local_seq` (low).** Moving
   `AppendLocalMessagesToScreenState` to `app_local_command.cpp` carries
   the static with it (correct — one TU, one strong def). Mitigation:
   keep it function-local (C3 rule); `nm` verifies one strong definition.

4. **Duplicate `lowercase_ascii` (low).** Two byte-identical definitions
   (free `app.cppm:143`, static member `app.cppm:641`). Moving both to
   `app_helpers.cpp` is safe (different scopes). Mitigation: move
   verbatim per the C3 recipe; optional delegation is review-gated.

5. **Keep-import shedding breaks the build (medium).** The four shed
   candidates (components, all_components, live_teammates, dialogs.system)
   may be #184957 reachability keep-imports in disguise (the C3 batch
   found `team_helpers` was one in `agent_runtime`). Mitigation: shed one
   at a time, compile-verify, restore with a comment on failure.

6. **`StartUiAnimationTicker` thread lifetime (medium).** The `jthread`
   lambda captures `this` and calls `PostRenderEvent` +
   `TriggerStatuslineUpdate`; a non-verbatim move could break the
   stop-token loop or capture. Mitigation: whole-function move;
   thread-safety review in batch 2; the lambda body is preserved
   byte-identical.

7. **`PostRenderEvent` cross-unit visibility (low).** It is called from
   six impl units; once out-of-line it is still a private member callable
   from any member function. Mitigation: declaration stays in the class
   (`app.cppm:540`); no friend or access change needed.

8. **Testing-accessor ABI (low).** The 28 `*_for_testing` accessors stay
   inline; the 4 test TUs (160 call sites) are unaffected by moving
   non-testing bodies. Mitigation: `ctest -j1` covers all four test TUs.

9. **Source-location budget (low).** The original reason for splitting
   `app.cppm` was clang's 2 GB source-location budget. The four new units
   are small (< 400 lines each); no unit approaches the budget. The
   interface itself shrinks, which only helps.

10. **macOS CI is not a gate (process).** Per the 2026-09-29 directive,
    verification is the local dual-preset build + serial ctest + goldens.
    The textual-std opt-outs compiled on Darwin in every Phase C batch,
    but Darwin compatibility is a nice-to-have, not a requirement. Do not
    block on or require macos-14.

---

## Review history

Two adversarial design reviews on 2026-09-29, both verdict
request-changes. All required changes applied in this revision:

1. **Default-argument hazard corrected** — the categorical "no extracted
   body carries a default argument" claim was false; `AppendLocalCommandMessage`
   (`app.cppm:592`) carries `bool is_error = false`. Unit 4 now specifies the
   declaration/definition split (default on the in-class declaration, omitted
   from the out-of-line definition) and records that all current callers pass
   the second argument explicitly.
2. **Edit-isolation fan-out corrected 7 → 17** — clang-scan-deps emits the
   primary as a required logical-name for every impl unit regardless of an
   explicit `import`; verified in the `build/debug` `.ddi` dyndep files for all
   11 impl units + the `app_impl.cppm` partition. The win is 17 → 1, not 7 → 1.
3. **Closure lists completed** — added explicit closure lists for
   `app_animation.cpp` (the highest-risk unit: `<chrono>`, `<thread>`,
   `<atomic>`, FTXUI event/screen headers) and `app_local_command.cpp`;
   added the missing `<initializer_list>` / `<cstdint>` to `app_helpers.cpp`
   and `<cstdlib>` / `<cmath>` to `app_skills_menu.cpp`.
4. **`inline` keyword handling stated** — `inline` is dropped from the 9 moved
   free functions (declarations stay exported in the interface, definitions are
   plain in the impl unit).
5. **Vtable/typeinfo anchor location corrected** — under clang modules the
   anchor is a strong `D` symbol in `app.cppm.o` (the interface unit), not the
   key-function TU `app_autocomplete.cpp` (which shows `U`); the `nm`
   verification steps in §(b), batch-2 step 10, and risk #2 now target
   `app.cppm.o`.
6. **`PostRenderEvent` inline-caller count corrected 5 → 3** — the three inline
   callers are `StartUiAnimationTicker` (`app.cppm:531`),
   `AppendLocalCommandMessage` (`app.cppm:602`), and `OpenSkillsMenu`
   (`app.cppm:835`).
7. **Stale CMake comment noted** — top-level `CMakeLists.txt:287` still says
   the #184957 defect "manifested only in the three cc.ui.app.app impl units";
   the count is now five. Added to §(c) as a cleanup item.

Independent design verification (agent:design-verify, 2026-09-29): **approved**
— all six required-change items verified against the code and build artifacts
(`.ddi` dyndep fan-out of 17, `nm` vtable/typeinfo anchor in `app.cppm.o`,
default-argument caller set, closure headers). Citation drift fixed in the same
pass: `inline_def_baseline.txt:13` → `:12` after the stale-entry cleanup.
