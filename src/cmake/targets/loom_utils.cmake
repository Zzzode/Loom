# ─── loom_utils: Utilities (no internal deps) ───────────────────────────────────
add_library(loom_utils)
target_sources(loom_utils
    PUBLIC FILE_SET CXX_MODULES FILES
        utils/diagnostics/activity_manager.cppm
        utils/agent/agent_id.cppm
        utils/parsing/argument_substitution.cppm
        utils/containers/array_utils.cppm
        utils/process/async.cppm
        utils/security/auto_mode_denials.cppm
        utils/bash/bash_execution.cppm
        utils/bash/bash_security.cppm
        utils/bash/bash_shell_quoting.cppm
        utils/platform/binary_check.cppm
        utils/cache/cache_paths.cppm
        utils/containers/circular_buffer.cppm
        utils/git/commit_attribution.cppm
        utils/crypto/crypto.cppm
        utils/diagnostics/debug.cppm
        utils/diagnostics/debug_filter.cppm
        utils/git/detect_repository.cppm
        utils/text/diff_utils.cppm
        utils/model/effort.cppm
        utils/env/env.cppm
        utils/env/env_utils.cppm
        utils/error/error.cppm
        utils/process/exec_sync.cppm
        utils/fs/file.cppm
        utils/fs/file_edit_utils.cppm
        utils/fs/file_persistence.cppm
        utils/fs/atomic_replace.cppm
        utils/fs/file_read_cache.cppm
        utils/platform/find_executable.cppm
        utils/diagnostics/fps_tracker.cppm
        utils/text/format.cppm
        utils/serdes/frontmatter_parser.cppm
        utils/git/git.cppm
        utils/git/git_diff.cppm
        utils/git/git_filesystem.cppm
        utils/http/github_utils.cppm
        utils/git/gitignore.cppm
        utils/crypto/hash.cppm
        utils/platform/clipboard.cppm
        utils/http/http.cppm
        utils/http/http_encoding.cppm
        utils/platform/hyperlink.cppm
        utils/serdes/json.cppm
        utils/diagnostics/log.cppm
        utils/text/markdown_utils.cppm
        utils/fs/memory_file_detection.cppm
        utils/model/model.cppm
        utils/model/providers.cppm
        utils/model/model_cost.cppm
        utils/containers/object_group_by.cppm
        utils/fs/path.cppm
        utils/fs/path_utils.cppm
        utils/text/parse_int.cppm
        utils/text/parse_references.cppm
        utils/security/permissions.cppm
        utils/security/permissions_engine.cppm
        utils/process/process.cppm
        utils/security/privacy_level.cppm
        utils/prompt/prompt_category.cppm
        utils/http/proxy_utils.cppm
        utils/security/query_guard.cppm
        utils/fs/read_file_in_range.cppm
        utils/text/semantic_boolean.cppm
        utils/text/semantic_number.cppm
        utils/security/sanitization.cppm
        utils/containers/set_utils.cppm
        utils/shell/shell.cppm
        utils/shell/shell_parser.cppm
        utils/shell/shell_rule_matching.cppm
        utils/parsing/slash_command_parsing.cppm
        utils/http/ssrf_guard.cppm
        utils/text/string.cppm
        utils/text/string_utils.cppm
        utils/platform/terminal_helpers.cppm
        utils/parsing/text_highlighting.cppm
        utils/process/timeouts.cppm
        utils/model/token_budget.cppm
        utils/security/tool_deny_rules.cppm
        utils/text/words.cppm
        utils/crypto/uuid_utils.cppm
        utils/platform/xdg.cppm
        utils/serdes/yaml.cppm
        utils/shell/shell_providers.cppm
        utils/media/image_store.cppm
        utils/platform/platform_paths.cppm
        utils/tree_sitter/tree_sitter.cppm
        utils/tree_sitter/bash/ast.cppm
)
target_sources(loom_utils
    PRIVATE
        utils/serdes/json_impl.cpp
        utils/serdes/json_val_read.cpp
        utils/serdes/json_parse.cpp
        utils/serdes/json_mut_val.cpp
        utils/serdes/json_mut_doc.cpp
        utils/serdes/json_iter.cpp
        utils/serdes/json_builders.cpp
)
target_link_libraries(loom_utils
    PUBLIC
        yyjson
        uv_a
        httplib::httplib
        ftxui::screen
)
# RFC 0001 OQ-1 pilot: consume ftxui/screen/color.hpp as a header unit in
# text_highlighting.cppm. LOOM_FTXUI_HU_ENABLED / LOOM_FTXUI_HU_BMI are set by
# loom_ftxui_headers.cmake (included just before this file in
# src/CMakeLists.txt); both are unset on Clang < 23, where the source keeps
# its textual GMF include and nothing below applies.
#
# PRIVATE properties, not a target_link_libraries: loom_utils is in the
# install(EXPORT) LOOMTargets set, and any linked target must be in that set.
# The -fmodule-file path is build-tree-only, so it must not enter the export.
if(LOOM_FTXUI_HU_ENABLED)
    # clang-scan-deps emits no p1689 edge for the HU import, so this target
    # dependency is the only BMI-before-consumer ordering guarantee.
    add_dependencies(loom_utils loom_ftxui_headers_bmi)
    target_compile_definitions(loom_utils PRIVATE
        LOOM_FTXUI_COLOR_HEADER_UNIT=1)
    # CMake 4.x builds module BMIs via a synthesized target@synth precompile
    # rule that does NOT inherit target PRIVATE compile options (same hazard
    # documented in loom_std.cmake). Per-source COMPILE_FLAGS reach every
    # compile of this source — including its BMI — on both the scanned-object
    # and synth rules, so the -fmodule-file mapping is present wherever the
    # HU import is parsed.
    set_source_files_properties(utils/parsing/text_highlighting.cppm PROPERTIES
        COMPILE_FLAGS "-fmodule-file=${LOOM_FTXUI_HU_BMI} -Wno-experimental-header-units")
endif()
if(LOOM_ENABLE_TREE_SITTER)
    target_link_libraries(loom_utils PUBLIC tree-sitter tree-sitter-bash)
    target_compile_definitions(loom_utils PUBLIC LOOM_HAS_TREE_SITTER=1)
else()
    target_compile_definitions(loom_utils PUBLIC LOOM_HAS_TREE_SITTER=0)
endif()
if(APPLE)
    target_link_libraries(loom_utils PUBLIC "-framework CoreFoundation")
endif()
