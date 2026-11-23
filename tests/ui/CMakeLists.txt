# ui/ — FTXUI interface tests (dialogs, streaming, markdown, rendering)

find_program(LOOM_UI_LINT_PYTHON NAMES python3 REQUIRED)
add_test(NAME MessageThemeLint
    COMMAND "${LOOM_UI_LINT_PYTHON}"
        "${CMAKE_SOURCE_DIR}/tools/arch/test_message_theme_lint.py")

# ── test_ui split into 4 TUs to stay under clang's 31-bit SourceLocation budget ──
# (2.14GB hard limit; original 43 imports in one TU overflowed at 99%).
# Each TU imports only the loom.* modules its tests actually need.

set(_ui_test_libs
    PRIVATE
        loom_core
        loom_ui           # defense-in-depth: UI modules must be discoverable
        GTest::gtest
        loom_test_main    # deterministic terminal env for golden snapshots
)

add_executable(test_ui_light test_ui_light.cpp test_prompt_wrap.cpp ${LOOM_TEST_SUPPORT_DIR}/test_ui_helpers.h)
target_link_libraries(test_ui_light ${_ui_test_libs})
gtest_discover_tests(test_ui_light DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_ui_messages test_ui_messages.cpp test_message_interactions.cpp ${LOOM_TEST_SUPPORT_DIR}/test_ui_helpers.h)
target_link_libraries(test_ui_messages ${_ui_test_libs})
gtest_discover_tests(test_ui_messages DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_ui_runtime
    test_app_runtime.cpp
    test_app_coordinators.cpp
    test_app_program_status.cpp
    test_generation_tps.cpp
    test_todo_completion.cpp
    test_ui_render.cpp
    test_ui_render_state.cpp
    test_ui_render_pure.cpp
    test_ui_paste.cpp
    test_ui_e2e.cpp
    ${LOOM_TEST_SUPPORT_DIR}/test_ui_helpers.h)
target_link_libraries(test_ui_runtime ${_ui_test_libs})
gtest_discover_tests(test_ui_runtime DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_program_status test_program_status.cpp)
target_include_directories(test_program_status PRIVATE "${ftxui_SOURCE_DIR}/src")
target_link_libraries(test_program_status ${_ui_test_libs})
gtest_discover_tests(test_program_status DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_ui_features test_ui_features.cpp ${LOOM_TEST_SUPPORT_DIR}/test_ui_helpers.h)
target_link_libraries(test_ui_features ${_ui_test_libs})
gtest_discover_tests(test_ui_features DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_design_system test_design_readability.cpp test_chrome_budget.cpp test_design_contracts.cpp
    test_attention_hierarchy.cpp
    test_settings_layout.cpp
    test_reduced_motion.cpp
    test_reduced_motion_cursor.cpp
    test_text_contrast.cpp
    test_text_contrast_controls.cpp
    test_text_contrast_dialogs.cpp
    ${LOOM_TEST_SUPPORT_DIR}/test_ui_helpers.h)
target_link_libraries(test_design_system ${_ui_test_libs})
gtest_discover_tests(test_design_system DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_ui_dialogs test_ui_dialogs.cpp ${LOOM_TEST_SUPPORT_DIR}/test_ui_helpers.h)
target_link_libraries(test_ui_dialogs ${_ui_test_libs})
gtest_discover_tests(test_ui_dialogs DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# RFC 0003: streaming payload replay harness smoke tests.
# ---------------------------------------------------------------------------
add_executable(test_streaming_replay test_streaming_replay.cpp)
target_link_libraries(test_streaming_replay ${_ui_test_libs})
gtest_discover_tests(test_streaming_replay DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# RFC 0003 Phase 4: App-side E2E tests migrated to fixture-driven streaming
# replay.
# ---------------------------------------------------------------------------
add_executable(test_streaming_migration test_streaming_migration.cpp)
target_link_libraries(test_streaming_migration ${_ui_test_libs})
gtest_discover_tests(test_streaming_migration DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# RFC 0004: fixture serializer round-trip tests.
# ---------------------------------------------------------------------------
add_executable(test_fixture_serializer test_fixture_serializer.cpp)
target_link_libraries(test_fixture_serializer ${_ui_test_libs})
gtest_discover_tests(test_fixture_serializer DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# CommonMark 0.31.2 + GFM conformance suites against the markdown parser's
# HTML serializer (render_markdown_to_html).
# ---------------------------------------------------------------------------
add_executable(test_markdown_conformance test_markdown_conformance.cpp)
target_link_libraries(test_markdown_conformance
    PRIVATE
        loom_ui
        loom_utils
        GTest::gtest
        # No loom_test_main: this test has its own main() to support
        # --conformance-json <path> for the HTML dashboard.
)
gtest_discover_tests(test_markdown_conformance DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# FTXUI terminal renderer tests for the markdown module.
# ---------------------------------------------------------------------------
add_executable(test_markdown_render test_markdown_render.cpp)
target_link_libraries(test_markdown_render
    PRIVATE
        loom_ui
        loom_utils
        GTest::gtest
        loom_test_main
)
gtest_discover_tests(test_markdown_render DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# RFC 0030: pinned upstream Mermaid corpus through the native terminal renderer.
add_executable(test_mermaid_official test_mermaid_official.cpp)
target_link_libraries(test_mermaid_official
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_official PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(test_mermaid_official
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    # OfficialMatrix renders five frames for all 2145 pinned originals in one
    # thread (~250s unloaded); it is the serial batch gate and needs headroom
    # when the parallel suite oversubscribes CPU on a loaded machine.
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_terminal test_mermaid_terminal.cpp)
target_link_libraries(test_mermaid_terminal
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_terminal PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(test_mermaid_terminal
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_flow_semantics test_mermaid_flow_semantics.cpp)
target_link_libraries(test_mermaid_flow_semantics
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_flow_semantics PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(test_mermaid_flow_semantics
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# RFC 0030: GitGraph source-bound terminal first-red assertions.
add_executable(test_mermaid_gitgraph_semantics
    test_mermaid_gitgraph_semantics.cpp
    test_mermaid_gitgraph_upstream.cpp)
target_link_libraries(test_mermaid_gitgraph_semantics
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_gitgraph_semantics PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(test_mermaid_gitgraph_semantics
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_charts_semantics test_mermaid_charts_semantics.cpp)
target_link_libraries(test_mermaid_charts_semantics
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_charts_semantics PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(test_mermaid_charts_semantics
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_sequence_semantics test_mermaid_sequence_semantics.cpp)
target_link_libraries(test_mermaid_sequence_semantics
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_sequence_semantics PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
# Measured: MermaidOfficialConfig110Native.EveryWholeSourceIsObservedBeforeDecodeAndAllProfilesRemainMandatory
# runs 213-260 s in isolation and exceeded the old 300 s budget under a
# 28-way full-suite load in three consecutive closures. The budget is raised
# for the binary (functional timeouts carry no assertion semantics) so a
# loaded machine reports the real verdict instead of a timeout.
gtest_discover_tests(test_mermaid_sequence_semantics
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_relations_semantics
    test_mermaid_relations_semantics.cpp
    test_mermaid_class_styles.cpp)
target_link_libraries(test_mermaid_relations_semantics
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_relations_semantics PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(test_mermaid_relations_semantics
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# The post-CTest join is separate from both consumers and reads metadata only.
add_test(NAME MermaidMindmapAdmissionJoin.RejectsIncompleteOrBorrowedVerdicts
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/verify_mindmap10_admission.py" --self-test)
set_tests_properties(MermaidMindmapAdmissionJoin.RejectsIncompleteOrBorrowedVerdicts
    PROPERTIES LABELS "mermaid" TIMEOUT 30)

add_test(NAME MermaidShapesAdmissionJoin.RejectsIncompleteOrBorrowedVerdicts
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/verify_shapes4_admission.py" --self-test)
set_tests_properties(MermaidShapesAdmissionJoin.RejectsIncompleteOrBorrowedVerdicts
    PROPERTIES LABELS "mermaid" TIMEOUT 30)

# RFC 0030 Block58: approved test-only B2 preflight, never drawing admission.
add_executable(test_mermaid_block_semantics test_mermaid_block_semantics.cpp)
target_link_libraries(test_mermaid_block_semantics
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_block_semantics PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(test_mermaid_block_semantics
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_test(NAME MermaidBlock58AdmissionJoin.RejectsUnreviewedIncompleteOrBorrowedVerdicts
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/verify_block58_admission.py" --self-test)
# Measured: the self-test finishes in ~25 s in isolation but reached 51.84 s
# (and once timed out) under full-suite load; 180 s keeps a real failure legible.
set_tests_properties(MermaidBlock58AdmissionJoin.RejectsUnreviewedIncompleteOrBorrowedVerdicts
    PROPERTIES LABELS "mermaid" TIMEOUT 180)

# RFC 0030 ER107: current source/projection pins plus fail-closed join controls.
add_test(NAME MermaidEr107AdmissionJoin.RejectsUnreviewedIncompleteOrBorrowedVerdicts
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/verify_er107_admission.py" --self-test)
set_tests_properties(MermaidEr107AdmissionJoin.RejectsUnreviewedIncompleteOrBorrowedVerdicts
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# RFC 0033 ER group-edge: six-case owned-proof cohort, four-part cohort
# assertion, protected-file drift and fail-closed helper review controls.
add_test(NAME MermaidErGroupEdgeAdmission.RejectsUnreviewedOrDriftedCohort
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/verify_er_groupedge_admission.py" --self-test)
set_tests_properties(MermaidErGroupEdgeAdmission.RejectsUnreviewedOrDriftedCohort
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# TechLead D-325: reproducible whole-source fallback attribution. Hermetic
# self-test proves every fallback re-classifies from the live fixture bytes and
# the roster stays total/count-stable; the live --matrix drift gate runs
# at closure and rejects any unregistered or reclassified fallback.
add_test(NAME MermaidFallbackAttribution.RosterIsReproducibleAndTotal
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/fallback_attribution.py" --self-test)
set_tests_properties(MermaidFallbackAttribution.RosterIsReproducibleAndTotal
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-443: the requirement/ishikawa/venn inert-leaf cohort ledger is a generator
# constant; the check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidD443InertLeavesCohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_d443_inert_leaves_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidD443InertLeavesCohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-442: the agentflow handDrawn-look cohort ledger is a generator constant;
# the check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidAgentflowHanddrawnD442Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_agentflow_handdrawn_d442_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidAgentflowHanddrawnD442Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-441: the state closed-layout cohort ledger is a generator constant; the
# check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidStateLayoutD441Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_state_layout_d441_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidStateLayoutD441Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-440: the flowchart closed-theme cohort ledger is a generator constant; the
# check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidFlowchartThemeD440Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_flowchart_theme_d440_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidFlowchartThemeD440Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-439: the mindmap closed-layout cohort ledger is a generator constant; the
# check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidMindmapTidyD439Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_mindmap_tidy_d439_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidMindmapTidyD439Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-437: the wardley closed-theme cohort ledger is a generator constant; the
# check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidWardleyThemeD437Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_wardley_theme_d437_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidWardleyThemeD437Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-436: the gitGraph main-branch cohort ledger is a generator constant; the
# check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidGitMainBranchD436Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_git_mainbranch_d436_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidGitMainBranchD436Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-435: the gitGraph frontmatter-title cohort ledger is a generator constant;
# the check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidGitTitleD435Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_git_title_d435_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidGitTitleD435Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-434: the tree-view closed icon-map cohort ledger is a generator constant;
# the check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidTreeviewIconsD434Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_treeview_icons_d434_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidTreeviewIconsD434Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-433: the tree-view closed-namespace cohort ledger is a generator constant;
# the check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidTreeviewNamespaceD433Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_treeview_ns_d433_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidTreeviewNamespaceD433Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-432: the c4 config.wrap cohort ledger is a generator constant; the check
# fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidC4WrapD432Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_c4_wrap_d432_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidC4WrapD432Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-431: the cynefin closed-frontmatter cohort ledger is a generator constant;
# the check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidCynefinNamespaceD431Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_cynefin_ns_d431_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidCynefinNamespaceD431Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-430: the radar closed-frontmatter cohort ledger is a generator constant; the
# check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidRadarNamespaceD430Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_radar_ns_d430_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidRadarNamespaceD430Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-429: the packet closed-namespace cohort ledger is a generator constant; the
# check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidPacketNamespaceD429Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_packet_ns_d429_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidPacketNamespaceD429Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-427: the treemap valueFormat cohort ledger is a generator constant; the check
# fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidTreemapFormatD427Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_treemap_fmt_d427_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidTreemapFormatD427Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-426: the pie accessibility-no-op cohort ledger is a generator constant; the
# check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidPieAccessibilityD426Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_pie_acc_d426_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidPieAccessibilityD426Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# D-425: the pie closed config-namespace cohort ledger is a generator constant;
# the check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidPieNamespaceD425Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_pie_ns_d425_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidPieNamespaceD425Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# RFC 0039: the reviewed gitGraph cohort pin is a generator constant; this
# hermetic check fails if the ledger drifts from it or was hand-edited.
add_test(NAME MermaidGit0039Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_git_0039_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidGit0039Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# RFC 0045: the parallelCommits cohort pin and the synthetic-twin rule are
# generator constants; this hermetic check fails if the ledger drifts from
# them or was hand-edited.
add_test(NAME MermaidGit0045Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_git_0045_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidGit0045Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# RFC 0046: the layout-tuning cohort pin and the corpus/synthetic twin split are
# generator constants; this hermetic check fails if the ledger drifts from them
# or was hand-edited.
add_test(NAME MermaidLayouttuning0046Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_layouttuning_0046_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidLayouttuning0046Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# RFC 0040: the journey-family cohort ledger and the three family ledgers it
# partitions are generator constants; these hermetic checks fail if any ledger
# drifts from its generator or was hand-edited.
add_test(NAME MermaidJourneyFam0040Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_journeyfam_0040_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
add_test(NAME MermaidJourneyFam0040Cohort.TimelineLedgerIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_timeline_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
add_test(NAME MermaidJourneyFam0040Cohort.GanttLedgerIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_gantt_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
add_test(NAME MermaidJourneyFam0040Cohort.JourneyLedgerIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_journey_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
# RFC 0047: the journey-family init-directive cohort ledger and its
# single-source probe artifact are generator constants; the check fails on a
# hand edit or an unreviewed expectation change.
add_test(NAME MermaidJourneyFamInit0047Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_journeyfam_init_0047_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidJourneyFamInit0047Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)
# D-424: the gitGraph closed-boolean namespace cohort ledger is a generator
# constant; the check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidGitBoolD424Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_git_bool_d424_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidGitBoolD424Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# RFC 0048: the State concurrent-region cohort ledger is a generator constant;
# the check fails on a hand edit or an unreviewed expectation change.
add_test(NAME MermaidStateRegion0048Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_state_region_0048_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidStateRegion0048Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

set_tests_properties(
    MermaidJourneyFam0040Cohort.GeneratorIsReproducible
    MermaidJourneyFam0040Cohort.TimelineLedgerIsReproducible
    MermaidJourneyFam0040Cohort.GanttLedgerIsReproducible
    MermaidJourneyFam0040Cohort.JourneyLedgerIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# RFC 0041: the agentflow admission ledger and the family ledger it partitions
# are generator constants; these hermetic checks fail on drift or hand edits.
add_test(NAME MermaidAgentflow0041Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_agentflow_0041_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
add_test(NAME MermaidAgentflow0041Cohort.FamilyLedgerIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_agentflow_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(
    MermaidAgentflow0041Cohort.GeneratorIsReproducible
    MermaidAgentflow0041Cohort.FamilyLedgerIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# RFC 0042: the pie admission ledger is a generator constant; this hermetic
# check fails on drift or a hand edit.
add_test(NAME MermaidPie0042Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_pie_0042_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MermaidPie0042Cohort.GeneratorIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# RFC 0043: the small-family admission ledger and the three family ledgers it
# partitions are generator constants; these hermetic checks fail on drift.
add_test(NAME MermaidSmallfam0043Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_smallfam_0043_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
add_test(NAME MermaidSmallfam0043Cohort.IshikawaLedgerIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_ishikawa_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
add_test(NAME MermaidSmallfam0043Cohort.VennLedgerIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_venn_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
add_test(NAME MermaidSmallfam0043Cohort.RailroadLedgerIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_railroad_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
# RFC 0044: the requirement ELK admission ledger and the two requirement
# ledgers it reconciles are generator constants; these hermetic checks fail on
# drift or hand edits.
add_test(NAME MermaidRequirement0044Cohort.GeneratorIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_requirement_0044_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
add_test(NAME MermaidRequirement0044Cohort.Cohort0037IsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_requirement_0037_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
add_test(NAME MermaidRequirement0044Cohort.BaseCohortIsReproducible
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/generate_requirement_cohort.py" --check
        --repo-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(
    MermaidRequirement0044Cohort.GeneratorIsReproducible
    MermaidRequirement0044Cohort.Cohort0037IsReproducible
    MermaidRequirement0044Cohort.BaseCohortIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

set_tests_properties(
    MermaidSmallfam0043Cohort.GeneratorIsReproducible
    MermaidSmallfam0043Cohort.IshikawaLedgerIsReproducible
    MermaidSmallfam0043Cohort.VennLedgerIsReproducible
    MermaidSmallfam0043Cohort.RailroadLedgerIsReproducible
    PROPERTIES LABELS "mermaid" TIMEOUT 120)

# RFC 0030 Flow400 Extra7: metadata-only post-CTest admission join.
add_test(NAME MermaidFlow400Extra7AdmissionJoin.RejectsIncompleteOrBorrowedVerdicts
    COMMAND "${LOOM_UI_LINT_PYTHON}" -I
        "${CMAKE_SOURCE_DIR}/tools/mermaid/verify_flow400_group_endpoint_extra7_admission.py" --self-test)
set_tests_properties(MermaidFlow400Extra7AdmissionJoin.RejectsIncompleteOrBorrowedVerdicts
    PROPERTIES LABELS "mermaid" TIMEOUT 60)

# RFC 0030 Block58: independent complete-original public captures; no join.
add_executable(test_mermaid_block_source58 test_mermaid_block_source58.cpp)
target_link_libraries(test_mermaid_block_source58
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_block_source58 PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(test_mermaid_block_source58
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# RFC 0030 ER107: independently owned complete-source Native captures.
add_executable(test_mermaid_er107_source107 test_mermaid_er107_source107.cpp)
target_link_libraries(test_mermaid_er107_source107
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_er107_source107 PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(test_mermaid_er107_source107
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# RFC 0030 ER107: exact public-factory Work feasibility, no drawing credit.
add_executable(test_mermaid_er107_runtime test_mermaid_er107_runtime.cpp)
target_link_libraries(test_mermaid_er107_runtime
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_er107_runtime
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# RFC 0030 ER107: source-only B1 binding; no rendering or drawing admission.
add_executable(test_mermaid_er107_semantics test_mermaid_er107_semantics.cpp)
target_link_libraries(test_mermaid_er107_semantics
    PRIVATE loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_er107_semantics
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# D248 ER107 emitter stage 1: byte-exact canonical contract differential lock.
add_executable(test_mermaid_er107_emitter_contract test_mermaid_er107_emitter_contract.cpp)
target_link_libraries(test_mermaid_er107_emitter_contract
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_er107_emitter_contract
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# RFC 0033 slice 2 phase 1: six-case group-edge proposal record generator and
# the hermetic self-check that re-derives every embedded digest.
add_executable(test_mermaid_er_groupedge_proposal test_mermaid_er_groupedge_proposal.cpp)
target_link_libraries(test_mermaid_er_groupedge_proposal
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_er_groupedge_proposal
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# RFC 0033 slice 2 phase 2: owned-proof support headers, six-case ownership and
# the tampered-byte negative gate.
add_executable(test_mermaid_er_groupedge_owned test_mermaid_er_groupedge_owned.cpp)
target_link_libraries(test_mermaid_er_groupedge_owned
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_er_groupedge_owned
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# RFC 0033 section 1.4: the relabeled D295 char-difference gate retained ONLY as
# a non-authoritative renderer regression smoke. It is never referenced by
# run_official_er206_case or any proof path; the owned cohort headers are the
# sole matrix authority for these paths.
add_executable(test_mermaid_er_group_edge_cohort test_mermaid_er_group_edge_cohort.cpp)
target_link_libraries(test_mermaid_er_group_edge_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_er_group_edge_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# RFC 0034 phase 2 increment 1: one-row ER198 cohort source identity, fail-closed
# selector, dual-cohort path disjointness and the unique two-debt prior pin.
# Test-only; the owned() proof gate lands with the proposal/support header.
add_executable(test_mermaid_er198_source test_mermaid_er198_source.cpp)
target_link_libraries(test_mermaid_er198_source
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_er198_source
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# RFC 0034 phase 2 increment 2a: one-row ER198 complete-proposal record-mode
# generator and the hermetic digest self-check / regeneration tripwire.
add_executable(test_mermaid_er198_proposal test_mermaid_er198_proposal.cpp)
target_link_libraries(test_mermaid_er198_proposal
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_er198_proposal
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# RFC 0034 phase 2 increment 2b: the one-row ER198 owned-proof run()/owned()
# gate with tampered-byte and not-ready fail-closed negatives. Footer-aware
# geometry uses the er_projection_notices test seam to separate the diagram
# from the projection notice block.
add_executable(test_mermaid_er198_owned test_mermaid_er198_owned.cpp)
target_link_libraries(test_mermaid_er198_owned
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_er198_owned
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# RFC 0030 ER107: independent source-derived projection primitives, no admission.
add_executable(test_mermaid_er107_projection test_mermaid_er107_projection.cpp)
target_link_libraries(test_mermaid_er107_projection
    PRIVATE loom_utils GTest::gtest loom_test_main ftxui::screen)
gtest_discover_tests(test_mermaid_er107_projection
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# RFC 0030 Mindmap10: complete originals plus independent typed geometry.
add_executable(test_mermaid_mindmap_semantics test_mermaid_mindmap_semantics.cpp)
target_link_libraries(test_mermaid_mindmap_semantics
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_mindmap_semantics PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(test_mermaid_mindmap_semantics
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_envelope test_mermaid_envelope.cpp)
target_link_libraries(test_mermaid_envelope
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_envelope
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 120 LABELS "mermaid")

add_executable(test_mermaid_probe test_mermaid_probe.cpp)
target_link_libraries(test_mermaid_probe
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_probe
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 120 LABELS "mermaid")

add_executable(test_mermaid_e2a_cohort test_mermaid_e2a_cohort.cpp)
target_link_libraries(test_mermaid_e2a_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_e2a_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_e2b_cohort test_mermaid_e2b_cohort.cpp)
target_link_libraries(test_mermaid_e2b_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_e2b_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_e2c_xychart_cohort test_mermaid_e2c_xychart_cohort.cpp)
target_link_libraries(test_mermaid_e2c_xychart_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_e2c_xychart_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_xychart_0035_cohort test_mermaid_xychart_0035_cohort.cpp)
target_link_libraries(test_mermaid_xychart_0035_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_xychart_0035_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs1_cohort test_mermaid_fs1_cohort.cpp)
target_link_libraries(test_mermaid_fs1_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs1_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs1g_cohort test_mermaid_fs1g_cohort.cpp)
target_link_libraries(test_mermaid_fs1g_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs1g_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs6_cohort test_mermaid_fs6_cohort.cpp)
target_link_libraries(test_mermaid_fs6_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs6_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs4h_cohort test_mermaid_fs4h_cohort.cpp)
target_link_libraries(test_mermaid_fs4h_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs4h_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5a_cohort test_mermaid_fs5a_cohort.cpp)
target_link_libraries(test_mermaid_fs5a_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5a_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5b_cohort test_mermaid_fs5b_cohort.cpp)
target_link_libraries(test_mermaid_fs5b_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5b_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5b_divrect test_mermaid_fs5b_divrect.cpp)
target_link_libraries(test_mermaid_fs5b_divrect
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5b_divrect
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5c_cohort test_mermaid_fs5c_cohort.cpp)
target_link_libraries(test_mermaid_fs5c_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5c_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5c_fliptri test_mermaid_fs5c_fliptri.cpp)
target_link_libraries(test_mermaid_fs5c_fliptri
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5c_fliptri
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5d_cohort test_mermaid_fs5d_cohort.cpp)
target_link_libraries(test_mermaid_fs5d_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5d_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5d_slopedrect test_mermaid_fs5d_slopedrect.cpp)
target_link_libraries(test_mermaid_fs5d_slopedrect
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5d_slopedrect
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5e_cohort test_mermaid_fs5e_cohort.cpp)
target_link_libraries(test_mermaid_fs5e_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5e_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5e_winpane test_mermaid_fs5e_winpane.cpp)
target_link_libraries(test_mermaid_fs5e_winpane
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5e_winpane
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g1_cohort test_mermaid_fs5g1_cohort.cpp)
target_link_libraries(test_mermaid_fs5g1_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g1_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g1_rectgroup test_mermaid_fs5g1_rectgroup.cpp)
target_link_libraries(test_mermaid_fs5g1_rectgroup
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g1_rectgroup
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g2_cohort test_mermaid_fs5g2_cohort.cpp)
target_link_libraries(test_mermaid_fs5g2_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g2_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g2_cylgroup test_mermaid_fs5g2_cylgroup.cpp)
target_link_libraries(test_mermaid_fs5g2_cylgroup
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g2_cylgroup
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g3_cohort test_mermaid_fs5g3_cohort.cpp)
target_link_libraries(test_mermaid_fs5g3_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g3_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g3_polygroup test_mermaid_fs5g3_polygroup.cpp)
target_link_libraries(test_mermaid_fs5g3_polygroup
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g3_polygroup
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g4_cohort test_mermaid_fs5g4_cohort.cpp)
target_link_libraries(test_mermaid_fs5g4_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g4_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g4_offsetgroup test_mermaid_fs5g4_offsetgroup.cpp)
target_link_libraries(test_mermaid_fs5g4_offsetgroup
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g4_offsetgroup
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g5_wavegroup test_mermaid_fs5g5_wavegroup.cpp)
target_link_libraries(test_mermaid_fs5g5_wavegroup
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g5_wavegroup
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g5_cohort test_mermaid_fs5g5_cohort.cpp)
target_link_libraries(test_mermaid_fs5g5_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g5_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g6_circlegroup test_mermaid_fs5g6_circlegroup.cpp)
target_link_libraries(test_mermaid_fs5g6_circlegroup
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g6_circlegroup
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g6_cohort test_mermaid_fs5g6_cohort.cpp)
target_link_libraries(test_mermaid_fs5g6_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g6_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g7_miscgroup test_mermaid_fs5g7_miscgroup.cpp)
target_link_libraries(test_mermaid_fs5g7_miscgroup
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g7_miscgroup
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5g7_cohort test_mermaid_fs5g7_cohort.cpp)
target_link_libraries(test_mermaid_fs5g7_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5g7_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs5a_shapedata test_mermaid_fs5a_shapedata.cpp)
target_link_libraries(test_mermaid_fs5a_shapedata
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs5a_shapedata
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs4_html_label test_mermaid_fs4_html_label.cpp)
target_link_libraries(test_mermaid_fs4_html_label
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs4_html_label
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs6_icon test_mermaid_fs6_icon.cpp)
target_link_libraries(test_mermaid_fs6_icon
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs6_icon
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs3_style test_mermaid_fs3_style.cpp)
target_link_libraries(test_mermaid_fs3_style
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs3_style
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs3_cohort test_mermaid_fs3_cohort.cpp)
target_link_libraries(test_mermaid_fs3_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs3_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs2_click test_mermaid_fs2_click.cpp)
target_link_libraries(test_mermaid_fs2_click
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs2_click
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs2_cohort test_mermaid_fs2_cohort.cpp)
target_link_libraries(test_mermaid_fs2_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs2_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# ---------------------------------------------------------------------------
# Text-content property tests for the FTXUI terminal renderer.
# ---------------------------------------------------------------------------
add_executable(test_markdown_render_conformance test_markdown_render_conformance.cpp)
target_link_libraries(test_markdown_render_conformance
    PRIVATE
        loom_ui
        loom_utils
        GTest::gtest
        loom_test_main
)
gtest_discover_tests(test_markdown_render_conformance
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# Style and layout tests for the FTXUI terminal markdown renderer.
# ---------------------------------------------------------------------------
add_executable(test_markdown_render_styles test_markdown_render_styles.cpp)
target_link_libraries(test_markdown_render_styles
    PRIVATE
        loom_ui
        loom_utils
        GTest::gtest
        loom_test_main
)
gtest_discover_tests(test_markdown_render_styles
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# Layer 5: golden file snapshot tests for the FTXUI terminal markdown renderer.
# Regenerate with: LOOM_UPDATE_SNAPSHOTS=1 ctest -R MarkdownRenderSnapshot
# ---------------------------------------------------------------------------
add_executable(test_markdown_render_snapshot test_markdown_render_snapshot.cpp)
target_link_libraries(test_markdown_render_snapshot
    PRIVATE
        loom_ui
        loom_utils
        GTest::gtest
        loom_test_main
)
gtest_discover_tests(test_markdown_render_snapshot
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# M7 Dialog #1 (ToolPermission) renderer + event handler regression suite.
# ---------------------------------------------------------------------------
add_executable(test_dialog_toolpermission test_dialog_toolpermission.cpp)
target_link_libraries(test_dialog_toolpermission
    PRIVATE
        loom_core
        loom_ui
        GTest::gtest
        loom_test_main
)
gtest_discover_tests(test_dialog_toolpermission
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# M7 Dialog System — split into 4 TUs by domain (SLOC budget)
# test_dialog_core: DialogSystem, DialogQueue, DialogRendererRegistry,
#   DialogFrame, DefaultRenderers, FullDialogRegistry
# test_dialog_triggers: DialogTriggers, QuickOpen
# test_dialog_renderers: DialogRenderers, SandboxPermissionEvents, ReplScreenIntegration
# test_session_picker: SessionPicker
# ---------------------------------------------------------------------------
add_executable(test_dialog_core test_dialog_core.cpp)
target_link_libraries(test_dialog_core
    PRIVATE
        loom_core
        loom_ui
        GTest::gtest
        loom_test_main
)
gtest_discover_tests(test_dialog_core
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_dialog_triggers test_dialog_triggers.cpp)
target_link_libraries(test_dialog_triggers
    PRIVATE
        loom_core
        loom_ui
        GTest::gtest
        loom_test_main
)
gtest_discover_tests(test_dialog_triggers
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_dialog_renderers
    test_dialog_renderers.cpp
    test_question_panel.cpp
)
target_link_libraries(test_dialog_renderers
    PRIVATE
        loom_core
        loom_ui
        GTest::gtest
        loom_test_main
)
gtest_discover_tests(test_dialog_renderers
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_session_picker test_session_picker.cpp)
target_link_libraries(test_session_picker
    PRIVATE
        loom_core
        loom_ui
        GTest::gtest
        loom_test_main
)
gtest_discover_tests(test_session_picker
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# M7 Dialog #3 (PromptDialog SELECT/FORM) renderer + event handling tests
# ---------------------------------------------------------------------------
add_executable(test_prompt_dialog test_prompt_dialog.cpp)
target_link_libraries(test_prompt_dialog
    PRIVATE
        loom_core
        loom_ui
        GTest::gtest_main
)
gtest_discover_tests(test_prompt_dialog
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# ---------------------------------------------------------------------------
# M7 Dialog #5 (CostThreshold) — enum Action, callback wiring, visual goldens
# ---------------------------------------------------------------------------
add_executable(test_cost_threshold_dialog test_cost_threshold_dialog.cpp)
target_link_libraries(test_cost_threshold_dialog
    PRIVATE
        loom_core
        loom_ui
        GTest::gtest_main
)
gtest_discover_tests(test_cost_threshold_dialog
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# Native regressions for semantic colors, held components, and caller overrides.
add_executable(test_ui_theme_conformance
    test_theme_messages_prompt.cpp
    test_theme_dialogs_permissions.cpp
    test_theme_widgets_features.cpp
    test_theme_screens_visual.cpp)
target_link_libraries(test_ui_theme_conformance ${_ui_test_libs})
gtest_discover_tests(test_ui_theme_conformance
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# RFC 0021: ordinary runtime instances, session projection and bounded chrome.
add_executable(test_runtime_agents
    test_runtime_agent_activity.cpp
    test_instance_browser.cpp
    test_app_runtime_agents.cpp
    test_permission_coordinator.cpp
    ${LOOM_TEST_SUPPORT_DIR}/test_ui_helpers.h)
target_link_libraries(test_runtime_agents ${_ui_test_libs})
gtest_discover_tests(test_runtime_agents DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

# Closed classic Flowchart shape first-red target: three full upstream sources,
# test-private Models, five-frame native capture, and no admission fallback.
add_executable(test_mermaid_flow_classic_shapes test_mermaid_flow_classic_shapes.cpp)
target_link_libraries(test_mermaid_flow_classic_shapes
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
target_compile_definitions(test_mermaid_flow_classic_shapes PRIVATE
    LOOM_MERMAID_REPORT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
gtest_discover_tests(test_mermaid_flow_classic_shapes
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

# ASCII Flow Grid18 source-first public profile. It remains in the existing
# Flow semantic target so its Flow400 authority and per-source ledgers stay local.
target_sources(test_mermaid_flow_semantics PRIVATE test_mermaid_flow_ascii.cpp)

# Source-bound group endpoints share the existing Flow semantic target.
target_sources(test_mermaid_flow_semantics PRIVATE test_mermaid_flow_group_endpoints.cpp)

add_executable(test_mermaid_multiline_classic test_mermaid_multiline_classic.cpp)
target_link_libraries(test_mermaid_multiline_classic
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_multiline_classic
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_multiline_stadium_cohort test_mermaid_multiline_stadium_cohort.cpp)
target_link_libraries(test_mermaid_multiline_stadium_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_multiline_stadium_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs7_subgraph test_mermaid_fs7_subgraph.cpp)
target_link_libraries(test_mermaid_fs7_subgraph
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs7_subgraph
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs7a_cohort test_mermaid_fs7a_cohort.cpp)
target_link_libraries(test_mermaid_fs7a_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs7a_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs7b_cohort test_mermaid_fs7b_cohort.cpp)
target_link_libraries(test_mermaid_fs7b_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs7b_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs7b_directive test_mermaid_fs7b_directive.cpp)
target_link_libraries(test_mermaid_fs7b_directive
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs7b_directive
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fs7c_cohort test_mermaid_fs7c_cohort.cpp)
target_link_libraries(test_mermaid_fs7c_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fs7c_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_fselk_cohort test_mermaid_fselk_cohort.cpp)
target_link_libraries(test_mermaid_fselk_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_fselk_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_journey_cohort test_mermaid_journey_cohort.cpp)
target_link_libraries(test_mermaid_journey_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_journey_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_timeline_cohort test_mermaid_timeline_cohort.cpp)
target_link_libraries(test_mermaid_timeline_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_timeline_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_gantt_cohort test_mermaid_gantt_cohort.cpp)
target_link_libraries(test_mermaid_gantt_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_gantt_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_treemap_cohort test_mermaid_treemap_cohort.cpp)
target_link_libraries(test_mermaid_treemap_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_treemap_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_c4_cohort test_mermaid_c4_cohort.cpp)
target_link_libraries(test_mermaid_c4_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_c4_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_smallfam_0043_cohort test_mermaid_smallfam_0043_cohort.cpp)
target_link_libraries(test_mermaid_smallfam_0043_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_smallfam_0043_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_venn_cohort test_mermaid_venn_cohort.cpp)
target_link_libraries(test_mermaid_venn_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_venn_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_quadrant_cohort test_mermaid_quadrant_cohort.cpp)
target_link_libraries(test_mermaid_quadrant_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_quadrant_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_architecture_cohort test_mermaid_architecture_cohort.cpp)
target_link_libraries(test_mermaid_architecture_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_architecture_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_kanban_cohort test_mermaid_kanban_cohort.cpp)
target_link_libraries(test_mermaid_kanban_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_kanban_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_usecase_cohort test_mermaid_usecase_cohort.cpp)
target_link_libraries(test_mermaid_usecase_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_usecase_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_treeview_cohort test_mermaid_treeview_cohort.cpp)
target_link_libraries(test_mermaid_treeview_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_treeview_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_ishikawa_cohort test_mermaid_ishikawa_cohort.cpp)
target_link_libraries(test_mermaid_ishikawa_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_ishikawa_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_cynefin_cohort test_mermaid_cynefin_cohort.cpp)
target_link_libraries(test_mermaid_cynefin_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_cynefin_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_wardley_cohort test_mermaid_wardley_cohort.cpp)
target_link_libraries(test_mermaid_wardley_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_wardley_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_railroad_cohort test_mermaid_railroad_cohort.cpp)
target_link_libraries(test_mermaid_railroad_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_railroad_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_radar_cohort test_mermaid_radar_cohort.cpp)
target_link_libraries(test_mermaid_radar_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_radar_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_packet_cohort test_mermaid_packet_cohort.cpp)
target_link_libraries(test_mermaid_packet_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_packet_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_eventmodeling_cohort test_mermaid_eventmodeling_cohort.cpp)
target_link_libraries(test_mermaid_eventmodeling_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_eventmodeling_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_requirement_cohort test_mermaid_requirement_cohort.cpp)
target_link_libraries(test_mermaid_requirement_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_requirement_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_requirement_0044_cohort test_mermaid_requirement_0044_cohort.cpp)
target_link_libraries(test_mermaid_requirement_0044_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_requirement_0044_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_requirement_0037_cohort test_mermaid_requirement_0037_cohort.cpp)
target_link_libraries(test_mermaid_requirement_0037_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_requirement_0037_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_gitgraph_0039_cohort test_mermaid_gitgraph_0039_cohort.cpp)
target_link_libraries(test_mermaid_gitgraph_0039_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_gitgraph_0039_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_git_0045_cohort test_mermaid_git_0045_cohort.cpp)
target_link_libraries(test_mermaid_git_0045_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_git_0045_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_layouttuning_0046_cohort test_mermaid_layouttuning_0046_cohort.cpp)
target_link_libraries(test_mermaid_layouttuning_0046_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_layouttuning_0046_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_journeyfam_0040_cohort test_mermaid_journeyfam_0040_cohort.cpp)
target_link_libraries(test_mermaid_journeyfam_0040_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_journeyfam_0040_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_journeyfam_init_0047_cohort test_mermaid_journeyfam_init_0047_cohort.cpp)
target_link_libraries(test_mermaid_journeyfam_init_0047_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_journeyfam_init_0047_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# RFC 0048: State concurrent-region cohort gate and its renderer-free ledger.
# D-424: gitGraph closed-boolean namespace cohort gate and its ledger.
# D-437: wardley closed-theme cohort gate and its ledger.
add_executable(test_mermaid_wardley_theme_d437_cohort
    test_mermaid_wardley_theme_d437_cohort.cpp)
target_link_libraries(test_mermaid_wardley_theme_d437_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_wardley_theme_d437_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-442: agentflow handDrawn-look cohort gate and its ledger.
# D-448: xychart negative-bar label cohort gate and its ledger.
add_executable(test_mermaid_xychart_negative_labels_d448_cohort
    test_mermaid_xychart_negative_labels_d448_cohort.cpp)
target_link_libraries(test_mermaid_xychart_negative_labels_d448_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_xychart_negative_labels_d448_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-447: usecase accessibility + namespace cohort gate and its ledger.
add_executable(test_mermaid_usecase_accessibility_d447_cohort
    test_mermaid_usecase_accessibility_d447_cohort.cpp)
target_link_libraries(test_mermaid_usecase_accessibility_d447_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_usecase_accessibility_d447_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-446: state leading-comment cohort gate and its ledger.
add_executable(test_mermaid_state_leading_comment_d446_cohort
    test_mermaid_state_leading_comment_d446_cohort.cpp)
target_link_libraries(test_mermaid_state_leading_comment_d446_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_state_leading_comment_d446_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-445: c4 UpdateLayoutConfig cohort gate and its ledger.
add_executable(test_mermaid_c4_layout_config_d445_cohort
    test_mermaid_c4_layout_config_d445_cohort.cpp)
target_link_libraries(test_mermaid_c4_layout_config_d445_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_c4_layout_config_d445_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-443: requirement/ishikawa/venn inert-leaf cohort gate and its ledger.
add_executable(test_mermaid_d443_inert_leaves_cohort
    test_mermaid_d443_inert_leaves_cohort.cpp)
target_link_libraries(test_mermaid_d443_inert_leaves_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_d443_inert_leaves_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-442: agentflow handDrawn-look cohort gate and its ledger.
add_executable(test_mermaid_agentflow_handdrawn_d442_cohort
    test_mermaid_agentflow_handdrawn_d442_cohort.cpp)
target_link_libraries(test_mermaid_agentflow_handdrawn_d442_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_agentflow_handdrawn_d442_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-441: state closed-layout cohort gate and its ledger.
add_executable(test_mermaid_state_layout_d441_cohort
    test_mermaid_state_layout_d441_cohort.cpp)
target_link_libraries(test_mermaid_state_layout_d441_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_state_layout_d441_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-440: flowchart closed-theme cohort gate and its ledger.
add_executable(test_mermaid_flow_theme_d440_cohort
    test_mermaid_flow_theme_d440_cohort.cpp)
target_link_libraries(test_mermaid_flow_theme_d440_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_flow_theme_d440_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-439: mindmap closed-layout cohort gate and its ledger.
add_executable(test_mermaid_mindmap_tidy_d439_cohort
    test_mermaid_mindmap_tidy_d439_cohort.cpp)
target_link_libraries(test_mermaid_mindmap_tidy_d439_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_mindmap_tidy_d439_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-436: gitGraph main-branch cohort gate and its ledger.
add_executable(test_mermaid_git_mainbranch_d436_cohort
    test_mermaid_git_mainbranch_d436_cohort.cpp)
target_link_libraries(test_mermaid_git_mainbranch_d436_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_git_mainbranch_d436_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-435: gitGraph frontmatter-title cohort gate and its ledger.
add_executable(test_mermaid_git_title_d435_cohort test_mermaid_git_title_d435_cohort.cpp)
target_link_libraries(test_mermaid_git_title_d435_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_git_title_d435_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-434: tree-view closed icon-map cohort gate and its ledger.
add_executable(test_mermaid_treeview_icons_d434_cohort
    test_mermaid_treeview_icons_d434_cohort.cpp)
target_link_libraries(test_mermaid_treeview_icons_d434_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_treeview_icons_d434_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-433: tree-view closed-namespace cohort gate and its ledger.
add_executable(test_mermaid_treeview_ns_d433_cohort
    test_mermaid_treeview_ns_d433_cohort.cpp)
target_link_libraries(test_mermaid_treeview_ns_d433_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_treeview_ns_d433_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-432: c4 config.wrap cohort gate and its ledger.
add_executable(test_mermaid_c4_wrap_d432_cohort test_mermaid_c4_wrap_d432_cohort.cpp)
target_link_libraries(test_mermaid_c4_wrap_d432_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_c4_wrap_d432_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-431: cynefin closed-frontmatter cohort gate and its ledger.
add_executable(test_mermaid_cynefin_ns_d431_cohort test_mermaid_cynefin_ns_d431_cohort.cpp)
target_link_libraries(test_mermaid_cynefin_ns_d431_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_cynefin_ns_d431_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-430: radar closed-frontmatter cohort gate and its ledger.
add_executable(test_mermaid_radar_ns_d430_cohort test_mermaid_radar_ns_d430_cohort.cpp)
target_link_libraries(test_mermaid_radar_ns_d430_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_radar_ns_d430_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-429: packet closed-namespace cohort gate and its ledger.
add_executable(test_mermaid_packet_ns_d429_cohort test_mermaid_packet_ns_d429_cohort.cpp)
target_link_libraries(test_mermaid_packet_ns_d429_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_packet_ns_d429_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-427: treemap valueFormat cohort gate and its ledger.
add_executable(test_mermaid_treemap_fmt_d427_cohort test_mermaid_treemap_fmt_d427_cohort.cpp)
target_link_libraries(test_mermaid_treemap_fmt_d427_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_treemap_fmt_d427_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-426: pie accessibility no-op cohort gate and its ledger.
add_executable(test_mermaid_pie_acc_d426_cohort test_mermaid_pie_acc_d426_cohort.cpp)
target_link_libraries(test_mermaid_pie_acc_d426_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_pie_acc_d426_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

# D-425: pie closed config-namespace cohort gate and its ledger.
add_executable(test_mermaid_pie_ns_d425_cohort test_mermaid_pie_ns_d425_cohort.cpp)
target_link_libraries(test_mermaid_pie_ns_d425_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_pie_ns_d425_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_git_bool_d424_cohort test_mermaid_git_bool_d424_cohort.cpp)
target_link_libraries(test_mermaid_git_bool_d424_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_git_bool_d424_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_state_region_0048_cohort test_mermaid_state_region_0048_cohort.cpp)
target_link_libraries(test_mermaid_state_region_0048_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_state_region_0048_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_agentflow_0041_cohort test_mermaid_agentflow_0041_cohort.cpp)
target_link_libraries(test_mermaid_agentflow_0041_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_agentflow_0041_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_pie_0042_cohort test_mermaid_pie_0042_cohort.cpp)
target_link_libraries(test_mermaid_pie_0042_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_pie_0042_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_state_0038_cohort test_mermaid_state_0038_cohort.cpp)
target_link_libraries(test_mermaid_state_0038_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_state_0038_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 600 LABELS "mermaid")

add_executable(test_mermaid_agentflow_cohort test_mermaid_agentflow_cohort.cpp)
target_link_libraries(test_mermaid_agentflow_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_agentflow_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_sankey_cohort test_mermaid_sankey_cohort.cpp)
target_link_libraries(test_mermaid_sankey_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_sankey_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_aws_icon test_mermaid_aws_icon.cpp)
target_link_libraries(test_mermaid_aws_icon
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_aws_icon
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_aws_icon_cohort test_mermaid_aws_icon_cohort.cpp)
target_link_libraries(test_mermaid_aws_icon_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_aws_icon_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_sq_label test_mermaid_sq_label.cpp)
target_link_libraries(test_mermaid_sq_label
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_sq_label
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_init_directive test_mermaid_init_directive.cpp)
target_link_libraries(test_mermaid_init_directive
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_init_directive
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_init_loglevel_cohort test_mermaid_init_loglevel_cohort.cpp)
target_link_libraries(test_mermaid_init_loglevel_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_init_loglevel_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

add_executable(test_mermaid_spacing_cohort test_mermaid_spacing_cohort.cpp)
target_link_libraries(test_mermaid_spacing_cohort
    PRIVATE loom_ui loom_utils GTest::gtest loom_test_main)
gtest_discover_tests(test_mermaid_spacing_cohort
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")
