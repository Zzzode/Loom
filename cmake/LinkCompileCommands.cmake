# cmake/LinkCompileCommands.cmake
#
# Included via CMAKE_PROJECT_TOP_LEVEL_INCLUDES, so this runs at configure time
# immediately BEFORE the top-level project() call completes. When compile-command
# export is enabled (the base preset sets CMAKE_EXPORT_COMPILE_COMMANDS=ON), put
# a compile_commands.json symlink at the source root that points into the active
# build directory.
#
# Why a symlink and not a copy:
#   * Ninja writes the real file into <binaryDir>/compile_commands.json (it is an
#     empty stub at configure time and is populated on the first build), so a
#     copy taken at configure time can be stale/empty. A symlink always resolves
#     to the current generated file and tracks the last build automatically.
#   * A single root path is what clangd / ccls / editors expect by default.
#
# The link is relative, so it keeps working if the checkout moves, and is only
# touched for the top-level build (function-style add_subdirectory/FetchContent
# builds leave the caller's export alone).

function(loom_link_compile_commands)
    if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
        return()
    endif()

    set(_loom_cc "${CMAKE_BINARY_DIR}/compile_commands.json")
    set(_loom_link "${CMAKE_SOURCE_DIR}/compile_commands.json")

    # Honor an existing real file or a link that already points where we want.
    if(EXISTS "${_loom_link}" AND NOT IS_SYMLINK "${_loom_link}")
        message(WARNING
            "loom: ${_loom_link} exists and is not a symlink; leaving it untouched. "
            "Remove it to let CMake manage a link to ${_loom_cc}.")
        return()
    endif()

    file(RELATIVE_PATH _loom_rel "${CMAKE_SOURCE_DIR}" "${_loom_cc}")
    file(CREATE_LINK "${_loom_rel}" "${_loom_link}"
         SYMBOLIC RESULT _loom_result)
    if(_loom_result)
        message(WARNING "loom: could not link ${_loom_link} -> ${_loom_rel}: ${_loom_result}")
    else()
        message(STATUS "loom: linked compile_commands.json -> ${_loom_rel}")
    endif()
endfunction()

loom_link_compile_commands()
