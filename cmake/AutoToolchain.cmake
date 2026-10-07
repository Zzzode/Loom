# AutoToolchain.cmake - pin a uniform LLVM 21+ toolchain across platforms.
#
# Used as CMAKE_TOOLCHAIN_FILE so individual presets do not need to know
# whether the host is macOS or Linux. Resolution order per platform:
#
#   macOS  : $LOOM_LLVM_PREFIX -> /opt/homebrew/opt/llvm
#   Linux  : $LOOM_LLVM_PREFIX
#            -> /home/linuxbrew/.linuxbrew/opt/llvm  (Homebrew on Linux)
#            -> ~/.local/opt/llvm-21/usr/lib/llvm-21
#               (with `clang(++)-21-local` shim binaries on PATH)
#
# Override at configure time via:
#   - CMAKE_C_COMPILER / CMAKE_CXX_COMPILER (highest precedence)
#   - LOOM_LLVM_PREFIX env var (point at any LLVM >= 21 install)
#
# Goal: full std::jthread / std::stop_token support and identical libc++/
# libstdc++ behavior between macOS and Linux developer machines.

if(DEFINED ENV{LOOM_LLVM_PREFIX})
    set(_loom_llvm_prefix "$ENV{LOOM_LLVM_PREFIX}")
endif()

if((DEFINED CMAKE_C_COMPILER AND NOT DEFINED CMAKE_CXX_COMPILER)
   OR (DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED CMAKE_C_COMPILER))
    message(FATAL_ERROR
        "[AutoToolchain] Set CMAKE_C_COMPILER and CMAKE_CXX_COMPILER together, "
        "or let this toolchain file select the matching pair.")
endif()

# Skip compiler selection if the user already provided compilers explicitly.
if(NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED CMAKE_CXX_COMPILER)
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        if(NOT DEFINED _loom_llvm_prefix)
            set(_loom_llvm_prefix "/opt/homebrew/opt/llvm")
        endif()
        set(_loom_clang     "${_loom_llvm_prefix}/bin/clang")
        set(_loom_clangxx   "${_loom_llvm_prefix}/bin/clang++")
        set(_loom_scan_deps "${_loom_llvm_prefix}/bin/clang-scan-deps")
        set(_loom_resource_dir "")  # Homebrew layout is self-consistent

    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        # Homebrew on Linux: use the brew LLVM install directly.
        if(NOT DEFINED _loom_llvm_prefix
           AND EXISTS "/home/linuxbrew/.linuxbrew/opt/llvm/bin/clang++")
            set(_loom_llvm_prefix "/home/linuxbrew/.linuxbrew/opt/llvm")
        endif()

        if(DEFINED _loom_llvm_prefix
           AND EXISTS "${_loom_llvm_prefix}/bin/clang++")
            # Homebrew-style install: compilers directly under <prefix>/bin.
            set(_loom_clang     "${_loom_llvm_prefix}/bin/clang")
            set(_loom_clangxx   "${_loom_llvm_prefix}/bin/clang++")
            set(_loom_scan_deps "${_loom_llvm_prefix}/bin/clang-scan-deps")
            set(_loom_resource_dir "")  # Homebrew layout is self-consistent

            # libc++ headers and libraries
            set(CMAKE_CXX_FLAGS_INIT
                "${CMAKE_CXX_FLAGS_INIT} -stdlib=libc++ -isystem ${_loom_llvm_prefix}/include/c++/v1")
            set(_loom_link_flags
                "-stdlib=libc++ -L${_loom_llvm_prefix}/lib -Wl,-rpath,${_loom_llvm_prefix}/lib")

            # Homebrew glibc (system glibc may be too old for LLVM 23's libc++)
            if(EXISTS "/home/linuxbrew/.linuxbrew/opt/glibc/lib")
                string(APPEND _loom_link_flags
                    " -L/home/linuxbrew/.linuxbrew/opt/glibc/lib"
                    " -Wl,-rpath,/home/linuxbrew/.linuxbrew/opt/glibc/lib"
                    " -Wl,-dynamic-linker,/home/linuxbrew/.linuxbrew/opt/glibc/lib/ld-linux-x86-64.so.2")
            endif()

            # Use system OpenSSL/curl, not Homebrew's (ABI compatibility)
            set(CMAKE_PREFIX_PATH "/usr" CACHE PATH "" FORCE)
            set(CMAKE_IGNORE_PREFIX_PATH "/home/linuxbrew/.linuxbrew" CACHE PATH "" FORCE)
            set(OPENSSL_ROOT_DIR "/usr" CACHE PATH "" FORCE)
            string(APPEND _loom_link_flags " -Wl,--allow-shlib-undefined")

            set(CMAKE_EXE_LINKER_FLAGS_INIT
                "${CMAKE_EXE_LINKER_FLAGS_INIT} ${_loom_link_flags}")
            set(CMAKE_SHARED_LINKER_FLAGS_INIT
                "${CMAKE_SHARED_LINKER_FLAGS_INIT} ${_loom_link_flags}")
        else()
            # Fallback: project convention with 21-local shim wrappers on PATH.
            if(NOT DEFINED _loom_llvm_prefix)
                set(_loom_llvm_prefix "$ENV{HOME}/.local/opt/llvm-21/usr/lib/llvm-21")
            endif()
            set(_loom_clang     "$ENV{HOME}/.local/bin/clang-21-local")
            set(_loom_clangxx   "$ENV{HOME}/.local/bin/clang++-21-local")
            set(_loom_scan_deps "$ENV{HOME}/.local/bin/clang-scan-deps-21-local")
            set(_loom_resource_dir "${_loom_llvm_prefix}/lib/clang/21")
        endif()

    else()
        message(WARNING
            "[AutoToolchain] Unsupported host '${CMAKE_HOST_SYSTEM_NAME}'. "
            "Falling back to default compiler; std::jthread support is not guaranteed.")
        return()
    endif()

    foreach(_required_tool IN ITEMS _loom_clang _loom_clangxx _loom_scan_deps)
        if(NOT EXISTS "${${_required_tool}}")
            message(FATAL_ERROR
                "[AutoToolchain] Required LLVM tool not found: ${${_required_tool}}\n"
                "  - macOS  : install with `brew install llvm` (>= 21)\n"
                "  - Linux  : install with `brew install llvm` (>= 21),\n"
                "             or set LOOM_LLVM_PREFIX to your LLVM >= 21 install root.")
        endif()
    endforeach()

    set(CMAKE_C_COMPILER   "${_loom_clang}"     CACHE FILEPATH "" FORCE)
    set(CMAKE_CXX_COMPILER "${_loom_clangxx}"   CACHE FILEPATH "" FORCE)
    set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "${_loom_scan_deps}" CACHE FILEPATH "" FORCE)

    if(_loom_resource_dir)
        # Linux shim wrappers need an explicit -resource-dir to find the
        # matching clang headers/builtins.
        set(_rd_flag "-resource-dir ${_loom_resource_dir}")
        set(CMAKE_C_FLAGS_INIT   "${CMAKE_C_FLAGS_INIT} ${_rd_flag}")
        set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} ${_rd_flag}")
    endif()
endif()

# ─── macOS sysroot alignment ──────────────────────────────────────────────────
# Homebrew clang defaults to the CommandLineTools SDK, but CMake's find_*
# locates system packages (CURL, ZLIB) in the Xcode SDK. The path strings
# differ, so CMake's implicit-include filter does not recognize the Xcode SDK
# path as implicit and emits `-isystem <xcode-sdk>/usr/include` verbatim. That
# flag is searched ahead of libc++'s c++/v1, and the SDK ships its own
# <stddef.h>/<stdio.h>/..., so libc++'s <cstddef> includes the SDK header
# instead of its own wrapper and trips the _LIBCPP_STDDEF_H self-check. Pin
# the sysroot so compiler, find_package, and implicit-dir detection all agree
# (CI passes -isysroot explicitly for the same reason).
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT DEFINED CMAKE_OSX_SYSROOT)
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE _loom_sdkroot
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(_loom_sdkroot)
        # Normal variable, not CACHE: a FORCE'd cache entry here corrupts
        # CMake's compiler-detection try_compile ("compiler not set, after
        # EnableLanguage").
        set(CMAKE_OSX_SYSROOT "${_loom_sdkroot}")
    endif()
    unset(_loom_sdkroot)
endif()

if(NOT DEFINED CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)
    if(DEFINED _loom_llvm_prefix AND EXISTS "${_loom_llvm_prefix}/bin/clang-scan-deps")
        set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS
            "${_loom_llvm_prefix}/bin/clang-scan-deps"
            CACHE FILEPATH "" FORCE)
    elseif(DEFINED CMAKE_CXX_COMPILER)
        get_filename_component(_loom_cxx_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        if(EXISTS "${_loom_cxx_dir}/clang-scan-deps")
            set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS
                "${_loom_cxx_dir}/clang-scan-deps"
                CACHE FILEPATH "" FORCE)
        endif()
        unset(_loom_cxx_dir)
    endif()
endif()

if(DEFINED CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS
   AND NOT EXISTS "${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS}")
    message(FATAL_ERROR
        "[AutoToolchain] clang-scan-deps does not exist: "
        "${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS}")
endif()

# Enforce LLVM >= 21 once the compiler is loaded by CMake.
function(_loom_assert_llvm21)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
       AND CMAKE_CXX_COMPILER_VERSION
       AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "21.0")
        message(FATAL_ERROR
            "[AutoToolchain] Clang ${CMAKE_CXX_COMPILER_VERSION} is too old; "
            "LLVM >= 21 is required for full std::jthread / std::stop_token support.")
    endif()
endfunction()
# CMakeLists.txt calls the assertion after project(), when compiler metadata exists.
set(LOOM_TOOLCHAIN_PIN "llvm>=21" CACHE INTERNAL "")
