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
gtest_discover_tests(test_mermaid_sequence_semantics
    DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT}
    PROPERTIES TIMEOUT 300 LABELS "mermaid")

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
set_tests_properties(MermaidBlock58AdmissionJoin.RejectsUnreviewedIncompleteOrBorrowedVerdicts
    PROPERTIES LABELS "mermaid" TIMEOUT 60)

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
