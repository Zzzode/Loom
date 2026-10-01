cmake_minimum_required(VERSION 3.28)

# Toolchain selection is owned by CMakePresets.json via
# cmake/AutoToolchain.cmake.  Do not silently pick a compiler here: C++23 named
# modules require a matching clang++ and clang-scan-deps pair, and hidden
# fallback makes broken local builds hard to diagnose.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE
   AND NOT DEFINED CMAKE_C_COMPILER
   AND NOT DEFINED CMAKE_CXX_COMPILER)
    message(FATAL_ERROR
        "LOOM requires an explicit LLVM toolchain. Configure with "
        "`cmake --preset debug`, `cmake --preset release`, `cmake --preset asan`, or set "
        "CMAKE_TOOLCHAIN_FILE / CMAKE_C_COMPILER / CMAKE_CXX_COMPILER.")
endif()

project(LOOM
    VERSION 1.0.0
    DESCRIPTION "LOOM: A C++23 Modules-based CLI REPL"
    LANGUAGES CXX C
)

if(COMMAND _loom_assert_llvm21)
    _loom_assert_llvm21()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    message(FATAL_ERROR
        "AppleClang cannot build LOOM C++23 modules because it does not "
        "ship clang-scan-deps. Use a preset backed by Homebrew LLVM, for "
        "example `cmake --preset release`.")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(NOT CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)
        message(FATAL_ERROR
            "CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS is not set. Use "
            "cmake/AutoToolchain.cmake through CMakePresets.json or set it "
            "explicitly to the clang-scan-deps binary that matches clang++.")
    elseif(NOT EXISTS "${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS}")
        message(FATAL_ERROR
            "clang-scan-deps not found: ${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS}")
    endif()
endif()

message(STATUS
    "[Toolchain] CXX=${CMAKE_CXX_COMPILER} "
    "scan-deps=${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS}")

# ─── Ninja Job Pools (eliminate scan-deps BMI races without throttling the
#     rest of the build) ───────────────────────────────────────────────────────
# Background: at high -j (Ninja default = hw.ncpu), the P1689 dependency-scan
# rules (``clang-scan-deps -format=p1689``) for many sibling .cppm files under
# a single target race to (a) produce their ``.ddi.tmp → .ddi`` atomic rename
# and (b) update the shared ``CXX.dd`` dyndep file.  The symptom is transient
# ``mv: <file>.ddi.tmp: No such file or directory`` on ~3% of scans, followed
# by a successful retry.  Rather than force the entire build to -j 1/2 to work
# around it, we cap ONLY the depfile-gen rules to a small pool and leave the
# actual compile + link steps at full concurrency.
#
# NOTE: this block is defined HERE (top-level) and NOT in src/CMakeLists.txt
# because CMAKE_NINJA_JOB_POOL_* are normal CMake variables with directory
# scope; if set only inside src/, they leak to tests/CMakeLists.txt via
# add_subdirectory ordering but their value is lost when processing other
# sibling subdirectories.  Setting them at the top level guarantees every
# target under this project (src + tests + benchmarks) observes the cap.
if(CMAKE_GENERATOR MATCHES "Ninja")
    include(ProcessorCount)
    ProcessorCount(NPROC)
    if(NOT NPROC OR NPROC LESS 1)
        set(NPROC 4)
    endif()

    set(LOOM_BUILD_JOBS "0" CACHE STRING
        "Maximum compile jobs for Ninja pools; 0 means host processor count.")
    set(LOOM_LINK_JOBS "0" CACHE STRING
        "Maximum link jobs for Ninja pools; 0 means automatic memory-safe cap.")
    set(LOOM_SCAN_JOBS "4" CACHE STRING
        "Maximum C++ module depfile scan jobs. Keep this capped until Ninja #2782 is released.")

    foreach(_job_var IN ITEMS LOOM_BUILD_JOBS LOOM_LINK_JOBS LOOM_SCAN_JOBS)
        if(NOT "${${_job_var}}" MATCHES "^[0-9]+$")
            message(FATAL_ERROR "${_job_var} must be a non-negative integer.")
        endif()
    endforeach()

    set(_COMPILE_POOL ${NPROC})
    if(LOOM_BUILD_JOBS GREATER 0)
        set(_COMPILE_POOL ${LOOM_BUILD_JOBS})
    endif()

    # Link jobs are memory heavy.  Compile uses all configured build jobs.
    # Depfile scan defaults to 4 after 10 consecutive clean configure+full-build
    # runs with 0 P1689/dyndep races on 2026-06-25.  Ninja #2782 is merged into
    # Ninja main but not released, so scan jobs remain separately capped.
    if(LOOM_LINK_JOBS GREATER 0)
        set(_LINK_POOL ${LOOM_LINK_JOBS})
    elseif(_COMPILE_POOL GREATER 16)
        set(_LINK_POOL 4)
    else()
        set(_LINK_POOL 2)
    endif()

    if(LOOM_SCAN_JOBS LESS 1)
        message(FATAL_ERROR "LOOM_SCAN_JOBS must be at least 1.")
    endif()
    set(_SCAN_POOL ${LOOM_SCAN_JOBS})
    if(_SCAN_POOL GREATER _COMPILE_POOL)
        set(_SCAN_POOL ${_COMPILE_POOL})
    endif()

    set_property(GLOBAL PROPERTY JOB_POOLS
        compile_jobs=${_COMPILE_POOL}
        link_jobs=${_LINK_POOL}
        scan_modules=${_SCAN_POOL}
    )
    # Assign the pools.  Names here are the CMake canonical ones:
    #   JOB_POOL_COMPILE → every <LANG>_COMPILE_OBJECT rule
    #   JOB_POOL_LINK    → every <LANG>_LINK_* rule
    #   CMAKE_NINJA_JOB_POOL_CXX_MODULE_DEPFILE_GEN (3.28+) → depfile scans
    set(CMAKE_JOB_POOL_COMPILE compile_jobs)
    set(CMAKE_JOB_POOL_LINK    link_jobs)
    # CMake 3.28+ exposes the depfile-gen pools directly.  On older versions
    # these set() calls are harmless — the variables are simply unused.  Set at
    # this (top-level) scope so child directories (src / tests / benchmarks)
    # inherit the cap naturally via directory variable scope.
    set(CMAKE_NINJA_JOB_POOL_CXX_MODULE_DEPFILE_GEN scan_modules)
    set(CMAKE_NINJA_JOB_POOL_C_MODULE_DEPFILE_GEN   scan_modules)
    message(STATUS
        "[Ninja pools] compile=${_COMPILE_POOL} link=${_LINK_POOL} "
        "scan_modules=${_SCAN_POOL}")
    unset(_COMPILE_POOL)
    unset(_LINK_POOL)
    unset(_SCAN_POOL)
endif()

# ─── C++23 Standard ───────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Suppress macOS ld64 duplicate library warnings ──────────────────────────
if(APPLE)
    add_link_options("LINKER:-no_warn_duplicate_libraries")
endif()

# ─── C++20/23 Modules Support ─────────────────────────────────────────────────
set(CMAKE_CXX_SCAN_FOR_MODULES ON)


# Include custom module helpers
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FindModules)

# ─── Build Type Default ───────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# ─── Compiler Flags for C++23 Named Modules ───────────────────────────────────
# CMake 3.28+ with FILE_SET CXX_MODULES handles module compilation automatically.
# No additional flags are needed for named modules (export module / import).
# Note: Do NOT use -fmodules/-fbuiltin-module-map (Clang header modules) as they
# conflict with C++23 named modules.
if(MSVC)
    add_compile_options(/experimental:module)
endif()

# ─── Find System Packages ─────────────────────────────────────────────────────
# Disable module scanning temporarily for find_package to work
set(CMAKE_CXX_SCAN_FOR_MODULES_OLD ${CMAKE_CXX_SCAN_FOR_MODULES})
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
set(CMAKE_CXX_SCAN_FOR_MODULES ${CMAKE_CXX_SCAN_FOR_MODULES_OLD})

# ─── FetchContent for Header-Only / Build-From-Source Dependencies ────────────
include(FetchContent)

# Offline dependency cache. If `.deps-cache/<name>-src/` exists next to this
# file, point FetchContent at it instead of cloning. The directory is
# gitignored and machine-local; it exists because this project is developed on
# a host with no github.com access, and re-downloading six pinned tarballs on
# every cache wipe is not reasonable.
#
# This lives here rather than in CMakePresets.json because a preset only applies
# when you configure through `cmake --preset`; a bare `cmake -B build` (or a
# wiped cache mid-session) would otherwise silently fall back to the network and
# fail. Absent directory => normal FetchContent behaviour, so CI is unaffected.
set(LOOM_DEPS_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/.deps-cache")
if(EXISTS "${LOOM_DEPS_CACHE}")
    foreach(_loom_dep IN ITEMS yyjson libuv httplib ftxui benchmark googletest)
        string(TOUPPER "${_loom_dep}" _loom_dep_upper)
        set(_loom_dep_dir "${LOOM_DEPS_CACHE}/${_loom_dep}-src")
        if(EXISTS "${_loom_dep_dir}")
            set(FETCHCONTENT_SOURCE_DIR_${_loom_dep_upper} "${_loom_dep_dir}"
                CACHE PATH "Offline source for ${_loom_dep}" FORCE)
        endif()
        unset(_loom_dep_dir)
        unset(_loom_dep_upper)
    endforeach()
    unset(_loom_dep)
    message(STATUS "LOOM: using offline dependency cache at ${LOOM_DEPS_CACHE}")
endif()

# yyjson - high-performance JSON library
FetchContent_Declare(
    yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG        0.10.0
    GIT_SHALLOW    TRUE
)

# libuv - cross-platform async I/O library
FetchContent_Declare(
    libuv
    GIT_REPOSITORY https://github.com/libuv/libuv.git
    GIT_TAG        v1.48.0
    GIT_SHALLOW    TRUE
)
set(LIBUV_BUILD_TESTS OFF CACHE INTERNAL "")

# cpp-httplib - header-only HTTP library
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.0
    GIT_SHALLOW    TRUE
)

# FTXUI - terminal UI library
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v5.0.0
    GIT_SHALLOW    TRUE
)
set(FTXUI_BUILD_DOCS OFF CACHE INTERNAL "")
set(FTXUI_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(FTXUI_BUILD_TESTS OFF CACHE INTERNAL "")

# Google Benchmark - micro-benchmarking library
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.1
    GIT_SHALLOW    TRUE
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE INTERNAL "")
set(BENCHMARK_ENABLE_INSTALL OFF CACHE INTERNAL "")
set(BENCHMARK_ENABLE_WERROR OFF CACHE INTERNAL "")

# Disable module scanning for FetchContent_MakeAvailable
set(CMAKE_CXX_SCAN_FOR_MODULES_OLD2 ${CMAKE_CXX_SCAN_FOR_MODULES})
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
FetchContent_MakeAvailable(yyjson libuv httplib ftxui benchmark)
set(CMAKE_CXX_SCAN_FOR_MODULES ${CMAKE_CXX_SCAN_FOR_MODULES_OLD2})

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    foreach(_loom_benchmark_target IN ITEMS benchmark benchmark_main)
        if(TARGET ${_loom_benchmark_target})
            target_compile_options(${_loom_benchmark_target} PRIVATE
                -Wno-c2y-extensions)
        endif()
    endforeach()
    unset(_loom_benchmark_target)
endif()

# ─── Project Warning Policy ──────────────────────────────────────────────────
option(LOOM_WARNINGS_AS_ERRORS
    "Treat LOOM project warnings as errors."
    ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    # Use clang's REDUCED BMI writer everywhere. clang 22.1.8 full BMIs make
    # heavy golden test TUs pathologically slow (>30 min on
    # test_dialog_toolpermission) and raise peak PSS, and mixing full-BMI
    # producers with reduced consumers SIGSEGVs clang — so a per-target
    # -fno-modules-reduced-bmi override is NOT viable either.
    #
    # The one reduced-BMI defect (LLVM #184957, fixed on main by PR #179178,
    # absent from 22.1.x) mis-merges the global aligned operator new when an
    # implementation unit `import std;`-s while its primary's global module
    # fragment textually pulls libc++ via FTXUI. It currently manifests in
    # THREE loom.ui.app.app impl units (app_autocomplete, app_run,
    # app_team); those units opt OUT of `import std;` and keep
    # textual std headers in their GMF (a plain impl unit is allowed
    # textual includes even when its interface imports std).
    # Re-evaluate after moving to a clang that contains PR #179178.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        add_compile_options(-fmodules-reduced-bmi)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
endif()

set(CMAKE_COMPILE_WARNING_AS_ERROR ${LOOM_WARNINGS_AS_ERRORS})

# ─── Subdirectories ───────────────────────────────────────────────────────────
add_subdirectory(src)

# ─── Unit Tests (GoogleTest) ──────────────────────────────────────────────────
option(LOOM_BUILD_TESTS "Build unit tests" ON)
# Backwards-compat: CC_BUILD_TESTS was the old name.
if(DEFINED CC_BUILD_TESTS AND NOT DEFINED LOOM_BUILD_TESTS)
    set(LOOM_BUILD_TESTS "${CC_BUILD_TESTS}" CACHE BOOL "Build unit tests" FORCE)
endif()
if(LOOM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ─── Install Configuration ────────────────────────────────────────────────────
# RFC 0001 cc-sdk phase 3 (§2.6): revive the install/EXPORT for the loom::sdk
# embeddable surface. The installed set is loom_sdk + every loom_* target in its
# transitive PUBLIC link closure (computed from the link graph, not
# hand-maintained) + the FetchContent third-party targets the closure
# references. C++ module names are loom.sdk.* (cc→loom rename, 2026-10-01);
# the install
# namespace is loom::.
#
# ABI policy (S5c): source-compatible only, no ABI stability. The harness
# public surface passes by value loom::core::*/loom::hooks::* types and the
# polymorphic WireBackend; with a prebuilt static lib + consumer-rebuilt
# BMIs, the installed package is source-compatible within one toolchain
# generation (same clang++ major, same C++ stdlib, matched clang-scan-deps).
# Consumers rebuild the module BMIs from the installed .cppm files; there
# is no prebuilt .pcm/.mod and no cross-version ABI guarantee. The package
# is experimental.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install the loom executable (the "embeddable binary" half — direct-connect
# HTTP/SSE). Installed separately from the SDK export.
install(TARGETS loom
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# yyjson's own CMake only sets BUILD_INTERFACE include dirs; add
# INSTALL_INTERFACE so the LOOMTargets-exported loom::yyjson resolves its
# header in a consumer project (yyjson self-installs yyjson.h to INCLUDEDIR
# via its own install rules).
target_include_directories(yyjson PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# The embeddable surface is `loom::sdk` (design §2.6): the build-tree target
# is loom_sdk, but EXPORT_NAME drops the loom_ prefix so the exported target is
# loom::sdk. The other loom_* targets keep their names (loom::loom_utils, …) —
# the consumer links only loom::sdk; the rest are transitive.
set_target_properties(loom_sdk PROPERTIES EXPORT_NAME sdk)

# The exported loom_* targets ship C++23 module interfaces (.cppm). The
# directory-level CMAKE_CXX_STANDARD (root CMakeLists.txt:137) does NOT
# travel with the export, so without an explicit compile feature the
# consumer-generated synth module targets fail CMake's cxx_std_20+ modules
# check ("found cxx_std_17") and the installed .cppm files cannot be
# compiled (§4.4 install smoke test). Record cxx_std_23 on every exported
# loom_* target. (In-tree this is a no-op: the tree already builds C++23 via
# the directory-level standard, so no compile flag changes.)
set(LOOM_SDK_EXPORT_TARGETS
    loom_sdk
    loom_utils
    loom_types
    loom_config
    loom_constants
    loom_std
    loom_tools
    loom_skills_core
    loom_tasks
    loom_teams
    loom_hooks
    loom_state
    loom_task_types
    loom_coordinator
    loom_vim
    loom_session
    loom_query
    loom_orchestration
    loom_memdir
    loom_services
    loom_plugins
)
# Set EXPORT_NAME on every exported loom_* target so the installed name is
# loom::<short> (e.g. loom::utils, not loom::loom_utils). loom_sdk already
# has EXPORT_NAME sdk (set above); the rest get their loom_ prefix stripped.
foreach(_t IN LISTS LOOM_SDK_EXPORT_TARGETS)
    target_compile_features(${_t} PUBLIC cxx_std_23)
    if(NOT _t STREQUAL "loom_sdk")
                string(REGEX REPLACE "^loom_" "" _short "${_t}")
        set_target_properties(${_t} PROPERTIES EXPORT_NAME "${_short}")
    endif()
endforeach()

# tree-sitter is an optional PUBLIC dep of loom_utils (LOOM_ENABLE_TREE_SITTER,
# default OFF). When ON, its targets must be in the export set too or
# install(EXPORT) fails the closure check. Their headers are installed from
# src/CMakeLists.txt, where the FetchContent source dirs are in scope.
set(LOOM_TREESITTER_EXPORT_TARGETS)
if(LOOM_ENABLE_TREE_SITTER)
    list(APPEND LOOM_TREESITTER_EXPORT_TARGETS tree-sitter tree-sitter-bash)
endif()

# Install the embeddable SDK surface + its transitive PUBLIC link closure.
# The loom_* list is the transitive PUBLIC closure of loom_sdk (20 targets,
# derived by walking target_link_libraries from loom_sdk), plus loom_std: every
# target links the `import std;` module via the directory-level
# link_libraries(loom_std) in src/CMakeLists.txt, so CMake's export check
# requires loom_std in the set even though it is not a target_link_libraries
# edge. loom_std's FILE_SET installs the toolchain-provided std.cppm (staged
# into the build tree at configure time); a consumer rebuilding the std BMI
# needs the same toolchain generation (same clang++/libc++), consistent with
# the experimental ABI policy above. The FetchContent targets
# (yyjson/uv_a/httplib/screen) are installed into the same export so the set
# is self-contained — find_dependency cannot conjure a FetchContent target.
# Real target names (not aliases — CMake rejects installing ALIAS targets).
# CURL/OpenSSL stay as find_dependency in loomConfig.cmake. ftxui-screen is
# in the closure via loom_utils's PUBLIC TLL ftxui::screen (loom_utils.cmake:109).
install(TARGETS
    ${LOOM_SDK_EXPORT_TARGETS}
    yyjson
    uv_a
    httplib
    screen
    ${LOOM_TREESITTER_EXPORT_TARGETS}
    EXPORT LOOMTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/loom
)

# Generate and install the export targets (NAMESPACE loom:: — the consumer
# does find_package(loom) and links loom::sdk). The config package is
# `loom` (lowercase): CMake's config-mode search is case-sensitive, so the
# installed files are loomConfig.cmake / loomConfigVersion.cmake under
# lib/cmake/loom — find_package(LOOM) would NOT find them (verified with
# CMake 3.31.2).
install(EXPORT LOOMTargets
    FILE LOOMTargets.cmake
    NAMESPACE loom::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/loom
)

# Generate the package config file (drops Boost; keeps CURL, adds OpenSSL).
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LOOMConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/loomConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/loom
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/loomConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/loomConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/loomConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/loom
)

# CMake ships no FindBrotli module, but the installed loom::httplib target
# references Brotli::* (its compression support is enabled by default when
# the system has the libs). Install httplib's own FindBrotli.cmake next to
# loomConfig.cmake, which appends this directory to CMAKE_MODULE_PATH before
# find_dependency(Brotli). httplib_SOURCE_DIR is set at this scope by the
# FetchContent_MakeAvailable call above (offline cache or network alike).
install(FILES "${httplib_SOURCE_DIR}/cmake/FindBrotli.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/loom
)
