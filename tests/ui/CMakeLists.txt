# ui/ — FTXUI interface tests (dialogs, streaming, markdown, rendering)

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

add_executable(test_ui_light test_ui_light.cpp ${LOOM_TEST_SUPPORT_DIR}/test_ui_helpers.h)
target_link_libraries(test_ui_light ${_ui_test_libs})
gtest_discover_tests(test_ui_light DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_ui_messages test_ui_messages.cpp ${LOOM_TEST_SUPPORT_DIR}/test_ui_helpers.h)
target_link_libraries(test_ui_messages ${_ui_test_libs})
gtest_discover_tests(test_ui_messages DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_ui_runtime
    test_app_runtime.cpp
    test_ui_render.cpp
    test_ui_render_state.cpp
    test_ui_render_pure.cpp
    test_ui_paste.cpp
    test_ui_e2e.cpp
    ${LOOM_TEST_SUPPORT_DIR}/test_ui_helpers.h)
target_link_libraries(test_ui_runtime ${_ui_test_libs})
gtest_discover_tests(test_ui_runtime DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

add_executable(test_ui_features test_ui_features.cpp ${LOOM_TEST_SUPPORT_DIR}/test_ui_helpers.h)
target_link_libraries(test_ui_features ${_ui_test_libs})
gtest_discover_tests(test_ui_features DISCOVERY_TIMEOUT ${LOOM_TEST_DISCOVERY_TIMEOUT})

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

add_executable(test_dialog_renderers test_dialog_renderers.cpp)
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
