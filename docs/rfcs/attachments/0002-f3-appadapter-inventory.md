# RFC 0002 F3 — AppAdapter inline-body inventory (F3-start snapshot)

Recorded by the F3 Prep commit. Source: `src/ui/app/app.cppm` @ `611ec13`
(F2 complete; `--target-ui9` PASS, 12 singleton SCCs, 0 back directions).
Measured with `tools/arch/inline_def_check.py` (the semantic inline-body
counter): `cc.ui.app.app` is frozen at **29** (`c2-done`), and the 29 bodies
are enumerated below with the F3 classification from
[0002-implementable-gate.md](0002-implementable-gate.md) §F3:

- **(a) composition/construction/wiring** — stays in `app.cppm` / a
  composition-root site (`construct_impl` / `construct_teammate` /
  `construct_settings`, `set_screen`, store/registry wiring);
- **(b) state projection/mutation** — moves to the new stores or their
  impl TUs;
- **(c) test seams** — move to impl TUs;
- **(d) event/render dispatch** — moves to impl TUs or the repl screen.

## Inventory (29 bodies)

| # | Body (app.cppm line) | Class | F3 resolution |
|---|---|---|---|
| 1 | `set_screen` (:570) | (a) | **stays** — composition-root screen wiring (stores the `ScreenInteractive*` in the atomic; explicitly composition in the gate's (a) list) |
| 2 | `is_query_running_for_testing` (:614) | (c) | impl TU |
| 3 | `submit_for_testing` (:619) | (c) | impl TU |
| 4 | `is_local_bash_running_for_testing` (:624) | (c) | impl TU |
| 5 | `wait_for_local_bash_for_testing` (:630) | (c) | impl TU |
| 6 | `is_loading_for_testing` (:635) | (c) | impl TU |
| 7 | `ui_animation_tick_count_for_testing` (:639) | (c) | impl TU |
| 8 | `status_message_for_testing` (:643) | (c) | impl TU |
| 9 | `status_line_enabled_for_testing` (:647) | (c) | impl TU |
| 10 | `status_line_command_for_testing` (:651) | (c) | impl TU |
| 11 | `status_line_padding_for_testing` (:655) | (c) | impl TU |
| 12 | `status_bar_model_for_testing` (:659) | (c) | impl TU |
| 13 | `autocomplete_suggestion_count_for_testing` (:663) | (c) | impl TU |
| 14 | `autocomplete_suggestions_for_testing` (:667) | (c) | impl TU |
| 15 | `autocomplete_index_for_testing` (:676) | (c) | impl TU |
| 16 | `messages_for_testing` (:682) | (c) | impl TU |
| 17 | `input_text_for_testing` (:697) | (c) | impl TU |
| 18 | `pasted_contents_size_for_testing` (:702) | (c) | impl TU |
| 19 | `has_pasted_content_for_testing` (:708) | (c) | impl TU |
| 20 | `inject_pasted_image_for_testing` (:714) | (c) | impl TU |
| 21 | `set_no_real_paste_worker_for_testing` (:724) | (c) | impl TU |
| 22 | `set_input_text_for_testing` (:730) | (c) | impl TU |
| 23 | `handle_submit_for_testing` (:737) | (c) | impl TU |
| 24 | `is_agents_view_for_testing` (:745) | (c) | impl TU |
| 25 | `is_local_jsx_command_for_testing` (:749) | (c) | impl TU |
| 26 | `active_agents_selection_position_for_testing` (:755) | (c) | impl TU |
| 27 | `agent_card_count_for_testing` (:759) | (c) | impl TU |
| 28 | `has_pending_dialog_for_testing` (:763) | (c) | impl TU |
| 29 | `teams_overview_count_for_testing` (:775) | (c) | impl TU |

## Tally

| Class | Count | Notes |
|---|---|---|
| (a) composition/construction/wiring | **1** | `set_screen` |
| (b) state projection/mutation | **0** | none inline — see below |
| (c) test seams | **28** | all `*_for_testing` bodies |
| (d) event/render dispatch | **0** | none inline — see below |

**The (b)/(d) bodies named in the gate are already out-of-line.** The F3
gate's (b) list (`AppendLocalMessagesToScreenState`,
`AppendLocalCommandInputMessage`, `AppendLocalCommandMessage`,
`ClearActiveLocalJsxCommand`, `DismissLocalJsxCommand`,
`TriggerStatuslineUpdate`, `StartUiAnimationTicker`,
`is_streaming_thinking_visible`, `OpenSkillsMenu`) and its (d) category are
declared in `app.cppm` without bodies today — the RFC 0001 Phase C batches
moved them into impl TUs (`app_local_command.cpp`, `app_constructor.cpp`,
`app_animation.cpp`, `app_autocomplete.cpp`, `app_skills_menu.cpp`). They are
not among the 29 ratchet-frozen inline bodies, so their (b)/(d) resolution is
already done; the F3 Finalize batch only needs to keep them out of the
interface as the stores land.

## Re-freeze target for the F3 Finalize batch

Move the 28 (c) test seams into impl TUs (e.g. a `app_testing_seams.cpp`
impl unit of `cc.ui.app.app`, or the existing shard impl units), then
re-freeze the ratchet at the measured (a) count:

```
python3 tools/arch/inline_def_check.py --update
```

**Target: 1** (29 → 1; the ≤ 15 expected bound is satisfied with wide
margin). `set_screen` is the sole body that stays inline. Store wiring must
add ZERO net inline bodies to `app.cppm` — all new store/selector wiring goes
in `app_*.cpp` impl units, per the F3 gate.
