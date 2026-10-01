# ─── loom_ftxui_headers: FTXUI header-unit pilot (RFC 0001 OQ-1) ──────────────
#
# One FTXUI header (ftxui/screen/color.hpp — a leaf: only <cstdint>/<string>/
# <vector>) built as a C++20 user header unit, consumed via
# `import <ftxui/screen/color.hpp>;` in the module purview instead of a textual
# GMF #include. Pilot consumer: utils/parsing/text_highlighting.cppm.
#
# FULL ROLLOUT ATTEMPT (2026-10-01): all 14 FTXUI headers were built as HUs and
# 144 source files were converted from textual #include to `import <ftxui/...>;`.
# The rollout was REVERTED because of transitive BMI leakage: when a module
# interface imports an FTXUI header unit, its BMI embeds the HU's std-header
# declarations (elements.hpp includes <string>, <vector>, <functional>, etc.).
# A module that imports that interface — even without touching FTXUI itself —
# then sees those std declarations as non-reachable, shadowing `import std;`
# and failing with "'vector' must be defined before it is used".  The pilot
# works because color.hpp is a lightweight leaf whose std declarations don't
# conflict; heavyweight headers (elements.hpp, component.hpp) break every
# transitive importer.  The CMake infrastructure below is kept intact for when
# the leakage is fixed (clang update or CMake CXX_MODULE_HEADERS support).
#
# TOOLCHAIN GATE — header units require Clang >= 23. On Clang 22 (the pinned
# local-linux preset, and CI's llvm@22) OQ-1 was REJECTED: LLVM #184957 made a
# reduced-BMI closure that mixed a header unit with `import std;` mis-merge
# the global aligned operator new. PR #179178 fixes it in Clang 23.1.2, so on
# older toolchains this file is a no-op: LOOM_FTXUI_HU_ENABLED stays OFF and
# the consumer keeps its textual GMF include (selected by the absence of
# LOOM_FTXUI_COLOR_HEADER_UNIT).
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 23)
    set(LOOM_FTXUI_HU_ENABLED OFF)
    message(STATUS "loom_ftxui_headers: disabled (requires Clang >= 23; "
                   "FTXUI stays textual)")
    return()
endif()
set(LOOM_FTXUI_HU_ENABLED ON)
message(STATUS "loom_ftxui_headers: enabled (Clang ${CMAKE_CXX_COMPILER_VERSION}) "
               "— ftxui/screen/color.hpp header-unit pilot")

# WHY NOT FILE_SET TYPE CXX_MODULE_HEADERS: CMake 4.4.3 (brew; CI's
# `brew install cmake`) rejects it — "target_sources File set TYPE may only
# be HEADERS, SOURCES, CXX_MODULES". The feature was reverted before CMake 4.0
# shipped and is absent through 4.4.x. The custom-command mechanism below is
# the stopgap; when CMake ships CXX_MODULE_HEADERS, replace this file with
# the form at the bottom and delete the custom command.
#
# WHY NOT AN INTERFACE LIBRARY: loom_utils is in the install(EXPORT)
# LOOMTargets set, and any target it links — even PRIVATE — must be in that
# set. The -fmodule-file path is a build-tree absolute path that must never
# reach the installed export, so the pilot applies its usage requirements as
# PRIVATE compile options/definitions directly on loom_utils (PRIVATE
# properties do not enter the export) and uses add_dependencies() for build
# ordering only.
#
# TWO HAZARDS (both validated 2026-10-01 on Clang 23.1.2 + reduced BMI):
#   1. `import <...>;` triggers -Wexperimental-header-units; -Werror promotes
#      it → the consumer carries -Wno-experimental-header-units.
#   2. clang-scan-deps emits NO p1689 edge for the HU import → Ninja will not
#      order BMI-before-consumer; add_dependencies(loom_utils
#      loom_ftxui_headers_bmi) in loom_utils.cmake is the only guarantee. That
#      is acceptable because the pilot header is a pinned third-party file
#      that never changes (no recompile-on-edit needed).
#
# The input is the ABSOLUTE header path so the BMI records it as the
# header-unit name; consumers resolve `import <ftxui/screen/color.hpp>;` via
# ftxui::screen's SYSTEM include dir to that same absolute path, so the
# keyless `-fmodule-file=<bmi>` maps it. Do NOT switch to the keyed
# `-fmodule-file=<name>=<path>` form — that is for named modules and breaks
# the HU mapping.

set(_loom_ftxui_hu_header
    "${ftxui_SOURCE_DIR}/include/ftxui/screen/color.hpp")
set(_loom_ftxui_hu_bmi_dir
    "${CMAKE_CURRENT_BINARY_DIR}/loom_ftxui_headers")
set(_loom_ftxui_hu_bmi
    "${_loom_ftxui_hu_bmi_dir}/ftxui-screen-color.pcm")
# Exposed to loom_utils.cmake (included next in src/CMakeLists.txt): the BMI
# path for its PRIVATE -fmodule-file option.
set(LOOM_FTXUI_HU_BMI "${_loom_ftxui_hu_bmi}")

add_custom_command(
    OUTPUT "${_loom_ftxui_hu_bmi}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_loom_ftxui_hu_bmi_dir}"
    COMMAND "${CMAKE_CXX_COMPILER}"
            -std=c++23
            -stdlib=libc++
            -fmodules-reduced-bmi
            -fmodule-header=user
            -isystem "${ftxui_SOURCE_DIR}/include"
            "${_loom_ftxui_hu_header}"
            -o "${_loom_ftxui_hu_bmi}"
    DEPENDS "${_loom_ftxui_hu_header}"
    COMMENT "loom_ftxui_headers: building header unit ftxui/screen/color.hpp"
    VERBATIM)
add_custom_target(loom_ftxui_headers_bmi DEPENDS "${_loom_ftxui_hu_bmi}")

# ─── FUTURE (when CMake ships CXX_MODULE_HEADERS) ─────────────────────────────
# add_library(loom_ftxui_headers INTERFACE)
# target_sources(loom_ftxui_headers
#     PUBLIC
#     FILE_SET ftxui_hu
#     TYPE CXX_MODULE_HEADERS
#     BASE_DIRS "${ftxui_SOURCE_DIR}/include"
#     FILES "${ftxui_SOURCE_DIR}/include/ftxui/screen/color.hpp")
# target_link_libraries(loom_ftxui_headers INTERFACE ftxui::screen)
# target_compile_options(loom_ftxui_headers INTERFACE
#     -Wno-experimental-header-units)
# target_compile_definitions(loom_ftxui_headers INTERFACE
#     LOOM_FTXUI_COLOR_HEADER_UNIT=1)
# # BASE_DIRS maps include/ftxui/screen/color.hpp → import name
# # "ftxui/screen/color.hpp". Consumers: target_link_libraries(<t> PRIVATE
# # loom_ftxui_headers) + `import <ftxui/screen/color.hpp>;`. CXX_MODULE_HEADERS
# # makes clang-scan-deps emit the p1689 edge, so the custom command, the
# # loom_ftxui_headers_bmi target, the add_dependencies(), and the manual
# # -fmodule-file option all go away. The INTERFACE library must then be added
# # to the install(TARGETS ... EXPORT LOOMTargets) set (a CXX_MODULE_HEADERS
# # FILE_SET is installed coherently, unlike the build-tree -fmodule-file path).
