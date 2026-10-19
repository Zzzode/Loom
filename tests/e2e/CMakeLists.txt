# tests/e2e/CMakeLists.txt
#
# End-to-end integration tests for Phase 3.
#
#   E2E_SSE_Mock         — feed a synthetic SSE byte stream into the parser
#                          and validate event/delta counts (no network).
#   E2E_Bash_File        — compose BashTool (find) + File* (read/edit/write)
#                          on a real temporary directory.
#   E2E_REPL_DryRun      — fork/exec the real `loom --dry-run` binary
#                          and assert stdout keywords (StatusBar/ReplScreen/Esc).
#
# All three tests are plain .cpp files (not C++23 named modules) with their
# own main() so they remain reproducible without module BMI visibility
# between TUs.

get_filename_component(_E2E_REAL_BIN "${CMAKE_BINARY_DIR}/bin/loom" ABSOLUTE)
find_package(Threads REQUIRED)

# ─── E2E-1: SSE mock parser ──────────────────────────────────────────────────
add_executable(e2e_sse_mock e2e_sse_mock.cpp)
set_target_properties(e2e_sse_mock PROPERTIES CXX_SCAN_FOR_MODULES OFF)
target_compile_features(e2e_sse_mock PRIVATE cxx_std_20)
add_test(NAME E2E_SSE_Mock COMMAND e2e_sse_mock)
set_tests_properties(E2E_SSE_Mock PROPERTIES TIMEOUT 10)

# ─── E2E-2: REPL --dry-run (fork/exec) ───────────────────────────────────────
add_executable(e2e_repl_dry_run e2e_repl_dry_run.cpp)
set_target_properties(e2e_repl_dry_run PROPERTIES CXX_SCAN_FOR_MODULES OFF)
target_compile_features(e2e_repl_dry_run PRIVATE cxx_std_20)
target_link_libraries(e2e_repl_dry_run PRIVATE Threads::Threads)
if(TARGET loom)
  add_dependencies(e2e_repl_dry_run loom)
endif()
# Pass the resolved binary location through env so the test doesn't need to
# guess relative paths (the argv0-based heuristic is backup).
add_test(NAME E2E_REPL_DryRun COMMAND e2e_repl_dry_run)
set_tests_properties(E2E_REPL_DryRun PROPERTIES
    TIMEOUT                          30
    ENVIRONMENT                      "LOOM_BINARY=${_E2E_REAL_BIN}"
    SKIP_RETURN_CODE                 125)

# ─── E2E-3: BashTool + FileTool composition ──────────────────────────────────
add_executable(e2e_bash_file e2e_bash_file_integration.cpp)
set_target_properties(e2e_bash_file PROPERTIES CXX_SCAN_FOR_MODULES OFF)
target_compile_features(e2e_bash_file PRIVATE cxx_std_20)
target_link_libraries(e2e_bash_file PRIVATE Threads::Threads)
add_test(NAME E2E_Bash_File COMMAND e2e_bash_file)
set_tests_properties(E2E_Bash_File PROPERTIES TIMEOUT 30)

# ─── E2E-4: PTY DECSET byte-level tests ─────────────────────────────────────
# Spawn loom in a real PTY (forkpty) and verify the terminal DECSET escape
# sequences emitted by FTXUI Install() at startup, based on the mouseMode
# config setting (RFC 0007 Phase 2).
add_executable(e2e_pty_decset e2e_pty_decset.cpp)
set_target_properties(e2e_pty_decset PROPERTIES CXX_SCAN_FOR_MODULES OFF)
target_compile_features(e2e_pty_decset PRIVATE cxx_std_20)
target_link_libraries(e2e_pty_decset PRIVATE Threads::Threads util)
if(TARGET loom)
  add_dependencies(e2e_pty_decset loom)
endif()
add_test(NAME E2E_Pty_Decset COMMAND e2e_pty_decset)
set_tests_properties(E2E_Pty_Decset PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
    SKIP_RETURN_CODE 125)

# Real terminal exit must drain model requests and Agent/permission waiters.
add_executable(e2e_pty_shutdown e2e_pty_shutdown.cpp)
set_target_properties(e2e_pty_shutdown PROPERTIES CXX_SCAN_FOR_MODULES OFF)
target_compile_features(e2e_pty_shutdown PRIVATE cxx_std_20)
target_link_libraries(e2e_pty_shutdown PRIVATE Threads::Threads util OpenSSL::SSL OpenSSL::Crypto)
if(TARGET loom)
  add_dependencies(e2e_pty_shutdown loom)
endif()
foreach(_pty_exit_case IN ITEMS
    idle-ctrl-c idle-sighup
    foreground-ctrl-d foreground-sigterm
    background-ctrl-d background-sighup permission-ctrl-d)
  add_test(NAME E2E_Pty_Shutdown_${_pty_exit_case}
      COMMAND e2e_pty_shutdown ${_pty_exit_case})
  set_tests_properties(E2E_Pty_Shutdown_${_pty_exit_case} PROPERTIES
      TIMEOUT 20
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()

# Startup branding survives the first actual resized frame at 40/80/120 columns.
add_test(NAME E2E_Pty_WelcomeMonogram COMMAND e2e_pty_shutdown welcome-monogram)
set_tests_properties(E2E_Pty_WelcomeMonogram PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
    SKIP_RETURN_CODE 125)

# RFC 0018: estimated output speed lives only beside foreground generation.
foreach(_generation_case IN ITEMS messages cancel)
  add_test(NAME E2E_Pty_GenerationTps_${_generation_case}
      COMMAND e2e_pty_shutdown generation-tps-${_generation_case})
  set_tests_properties(E2E_Pty_GenerationTps_${_generation_case} PROPERTIES
      TIMEOUT 35
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()

# RFC 0012: actual prompt wrapping and SIGWINCH reflow preserve submitted bytes.
add_test(NAME E2E_Pty_PromptWrap COMMAND e2e_pty_shutdown prompt-wrap)
set_tests_properties(E2E_Pty_PromptWrap PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
    SKIP_RETURN_CODE 125)

# RFC 0009: the default passes 20 rounds; an explicit budget preserves progress
# across keyboard/mouse Continue and a subsequent independent API Retry.
foreach(_pty_round_case IN ITEMS round-unbounded round-budget round-click round-retry round-cli)
  add_test(NAME E2E_Pty_RoundBudget_${_pty_round_case}
      COMMAND e2e_pty_shutdown ${_pty_round_case})
  set_tests_properties(E2E_Pty_RoundBudget_${_pty_round_case} PROPERTIES
      TIMEOUT 30
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()

# RFC 0014: actual collapsed/expanded stage timing, current target and reflow.
add_test(NAME E2E_Pty_CompressedChain COMMAND e2e_pty_shutdown compressed-chain-live)
set_tests_properties(E2E_Pty_CompressedChain PROPERTIES
    TIMEOUT 45
    ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
    SKIP_RETURN_CODE 125)

add_test(NAME E2E_Pty_TaskResponseLayout COMMAND e2e_pty_shutdown task-response-layout)
set_tests_properties(E2E_Pty_TaskResponseLayout PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
    SKIP_RETURN_CODE 125)

add_test(NAME E2E_Pty_TaskCompletion COMMAND e2e_pty_shutdown task-completion)
set_tests_properties(E2E_Pty_TaskCompletion PROPERTIES
    TIMEOUT 45
    ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
    SKIP_RETURN_CODE 125)

# Permission footer labels and atomic key/action hints reflow in the first
# resized frame, including short terminals, without resolving the request.
add_test(NAME E2E_Pty_PermissionFooter COMMAND e2e_pty_shutdown permission-footer)
set_tests_properties(E2E_Pty_PermissionFooter PROPERTIES
    TIMEOUT 30
    ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
    SKIP_RETURN_CODE 125)

# Real Bash errors remain visible and recover inside the original turn; an
# actual R key retries only the failed request after a committed side effect.
foreach(_pty_recovery_case IN ITEMS bash-failure permission-denial api-retry api-retry-network)
  add_test(NAME E2E_Pty_Recovery_${_pty_recovery_case}
      COMMAND e2e_pty_shutdown ${_pty_recovery_case})
  set_tests_properties(E2E_Pty_Recovery_${_pty_recovery_case} PROPERTIES
      TIMEOUT 30
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()

# Parent and delegated Agent requests share the final model after precedence
# resolution; runtime changes must not leave the registry's startup snapshot.
foreach(_pty_model_source IN ITEMS file env settings cli runtime)
  foreach(_pty_model_kind IN ITEMS foreground background)
    set(_pty_model_case "model-${_pty_model_source}-${_pty_model_kind}-ctrl-d")
    add_test(NAME E2E_Pty_Model_${_pty_model_source}_${_pty_model_kind}
        COMMAND e2e_pty_shutdown ${_pty_model_case})
    set_tests_properties(E2E_Pty_Model_${_pty_model_source}_${_pty_model_kind} PROPERTIES
        TIMEOUT 20
        ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
        SKIP_RETURN_CODE 125)
  endforeach()
endforeach()

# The direct runtime-tool entry resolves models before registering Agent too.
foreach(_pty_direct_source IN ITEMS file settings cli)
  add_test(NAME E2E_Pty_Model_${_pty_direct_source}_direct
      COMMAND e2e_pty_shutdown model-${_pty_direct_source}-direct)
  set_tests_properties(E2E_Pty_Model_${_pty_direct_source}_direct PROPERTIES
      TIMEOUT 20
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()

# RFC 0026: the executable's separate loader must supply persisted model budgets
# to the real terminal and direct Agent entry, including strict load errors.
foreach(_pty_context_case IN ITEMS switch direct invalid fallback tolerance metadata metadata-direct)
  add_test(NAME E2E_Pty_ModelContext_${_pty_context_case}
      COMMAND e2e_pty_shutdown model-context-${_pty_context_case})
  set_tests_properties(E2E_Pty_ModelContext_${_pty_context_case} PROPERTIES
      TIMEOUT 40
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()

# RFC 0028: real main.cpp generation defaults and ordinary settings saves.
foreach(_pty_generation_case IN ITEMS auto explicit manual)
  add_test(NAME E2E_Pty_GenerationDefaults_${_pty_generation_case}
      COMMAND e2e_pty_shutdown generation-defaults-${_pty_generation_case})
  set_tests_properties(E2E_Pty_GenerationDefaults_${_pty_generation_case} PROPERTIES
      TIMEOUT 40
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()

# RFC 0027: current working context stays distinct from cumulative request usage.
add_test(NAME E2E_Pty_ContextOccupancy_RepeatedAndClear
    COMMAND e2e_pty_shutdown context-occupancy-repeated)
add_test(NAME E2E_Pty_ContextOccupancy_CompactionSwitchRestore
    COMMAND e2e_pty_shutdown context-occupancy-compaction)
set_tests_properties(
    E2E_Pty_ContextOccupancy_RepeatedAndClear
    E2E_Pty_ContextOccupancy_CompactionSwitchRestore PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
    SKIP_RETURN_CODE 125)

# Drag selection travels through real SGR input and flushed ANSI frames.
# Clipboard commands are isolated fixture executables; no host clipboard writes.
foreach(_pty_selection_case IN ITEMS
    basic wide release-only streaming streaming-transcript wrap-down wrap-up wrap-batched code-down code-up)
  add_test(NAME E2E_Pty_Selection_${_pty_selection_case}
      COMMAND e2e_pty_shutdown selection-${_pty_selection_case})
  set_tests_properties(E2E_Pty_Selection_${_pty_selection_case} PROPERTIES
      TIMEOUT 30
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()

# Esc is cancellation, and a mouse report in the same stdin write must not
# swallow it or reach the next prompt. Loom inherits SIGPIPE's default action.
foreach(_pty_escape_case IN ITEMS
    idle stream tool selection idle-sgr idle-urxvt stream-sgr stream-urxvt)
  add_test(NAME E2E_Pty_Escape_${_pty_escape_case}
      COMMAND e2e_pty_shutdown escape-${_pty_escape_case})
  set_tests_properties(E2E_Pty_Escape_${_pty_escape_case} PROPERTIES
      TIMEOUT 30
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()

# Real zsh consumes stdin after Loom exits. Reports are queued while tracking
# is still enabled; the emulator never emits reports after receiving reset.
foreach(_pty_handoff_case IN ITEMS ctrl-d exit term hup)
  add_test(NAME E2E_Pty_Handoff_${_pty_handoff_case}
      COMMAND e2e_pty_shutdown handoff-${_pty_handoff_case})
  set_tests_properties(E2E_Pty_Handoff_${_pty_handoff_case} PROPERTIES
      TIMEOUT 30
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()

# Exercise the former signal/reader teardown race across independent real
# shells. Every iteration checks termios, exit status and queued mouse input.
add_test(NAME E2E_Pty_Handoff_HupStress COMMAND e2e_pty_shutdown handoff-hup-stress)
set_tests_properties(E2E_Pty_Handoff_HupStress PROPERTIES
    TIMEOUT 90
    ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
    SKIP_RETURN_CODE 125)

# RFC 0016: negotiate through the real FTXUI reader and inspect raw status
# ordering, real tool/question/Continue decisions, cancellation and zsh handoff.
foreach(_pty_status_case IN ITEMS
    st bel fragmented unsupported late restricted-states restricted-kind
    truncated permission question error cancel budget handoff)
  add_test(NAME E2E_Pty_ProgramStatus_${_pty_status_case}
      COMMAND e2e_pty_shutdown osc7501-${_pty_status_case})
  set_tests_properties(E2E_Pty_ProgramStatus_${_pty_status_case} PROPERTIES
      TIMEOUT 45
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()

# RFC 0017: actual question text spaces, native editing/default/empty/cancel
# decisions and retained long buffers through the first resized frame.
foreach(_pty_question_case IN ITEMS layout edit unicode resize default empty cancel)
  add_test(NAME E2E_Pty_Question_${_pty_question_case}
      COMMAND e2e_pty_shutdown question-${_pty_question_case})
  set_tests_properties(E2E_Pty_Question_${_pty_question_case} PROPERTIES
      TIMEOUT 45
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
      SKIP_RETURN_CODE 125)
endforeach()


if(APPLE)
  # The test CA interposer only installs the existing fixture certificate in
  # SSL_new's client context. Verification stays enabled and system trust is
  # untouched; socket creation, cancellation, and signals use product code.
  add_library(e2e_tls_test_ca SHARED tls_test_ca.cpp)
  set_target_properties(e2e_tls_test_ca PROPERTIES CXX_SCAN_FOR_MODULES OFF)
  target_compile_features(e2e_tls_test_ca PRIVATE cxx_std_20)
  target_link_libraries(e2e_tls_test_ca PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  add_dependencies(e2e_pty_shutdown e2e_tls_test_ca)
  add_test(NAME E2E_Pty_Escape_TLS COMMAND e2e_pty_shutdown escape-tls)
  set_tests_properties(E2E_Pty_Escape_TLS PROPERTIES
      TIMEOUT 30
      ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN};LOOM_PTY_TLS_CA_LIBRARY=$<TARGET_FILE:e2e_tls_test_ca>;LOOM_PTY_TLS_CA_CERT=${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/sse_cert.pem;LOOM_PTY_TLS_CA_KEY=${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/sse_key.pem"
      SKIP_RETURN_CODE 125)
endif()

# Opt-in live model acceptance in the real FTXUI terminal. Default runs skip
# rather than contacting external services; staged credentials stay local.
add_executable(e2e_subagent_live e2e_subagent_live.cpp)
set_target_properties(e2e_subagent_live PROPERTIES CXX_SCAN_FOR_MODULES OFF)
target_compile_features(e2e_subagent_live PRIVATE cxx_std_20)
target_link_libraries(e2e_subagent_live PRIVATE Threads::Threads util)
add_dependencies(e2e_subagent_live loom)
foreach(_live_case IN ITEMS parallel cancel-resume cancel-resume-sighup)
    add_test(NAME E2E_SubagentLive_${_live_case} COMMAND e2e_subagent_live ${_live_case})
    set_tests_properties(E2E_SubagentLive_${_live_case} PROPERTIES
        TIMEOUT 190
        ENVIRONMENT "LOOM_BINARY=${_E2E_REAL_BIN}"
        SKIP_RETURN_CODE 125
        RUN_SERIAL TRUE)
endforeach()
