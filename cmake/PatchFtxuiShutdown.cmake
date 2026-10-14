# FTXUI 5 restores canonical terminal input in Signal() before its reader
# stops. A concurrent read can then wait for a newline while holding the last
# task sender, preventing the loop from finishing. Use the normal exit path:
# Uninstall() joins the reader before OnExit() restores terminal attributes.
set(_loom_ftxui_screen "${ftxui_SOURCE_DIR}/src/ftxui/component/screen_interactive.cpp")
file(READ "${_loom_ftxui_screen}" _loom_ftxui_shutdown)
set(_loom_ftxui_old "void ScreenInteractive::Signal(int signal) {\n  if (signal == SIGABRT) {\n    OnExit();\n    return;\n  }")
set(_loom_ftxui_new "void ScreenInteractive::Signal(int signal) {\n  if (signal == SIGABRT) {\n    // Join the input reader before restoring canonical terminal input.\n    Exit();\n    return;\n  }")
string(FIND "${_loom_ftxui_shutdown}" "${_loom_ftxui_new}" _loom_ftxui_applied)
if(_loom_ftxui_applied EQUAL -1)
    string(FIND "${_loom_ftxui_shutdown}" "${_loom_ftxui_old}" _loom_ftxui_expected)
    if(_loom_ftxui_expected EQUAL -1)
        message(FATAL_ERROR "FTXUI signal shutdown changed; review terminal-reader teardown ordering.")
    endif()
    string(REPLACE "${_loom_ftxui_old}" "${_loom_ftxui_new}" _loom_ftxui_shutdown "${_loom_ftxui_shutdown}")
    file(WRITE "${_loom_ftxui_screen}" "${_loom_ftxui_shutdown}")
endif()
unset(_loom_ftxui_screen)
unset(_loom_ftxui_shutdown)
unset(_loom_ftxui_old)
unset(_loom_ftxui_new)
unset(_loom_ftxui_applied)
unset(_loom_ftxui_expected)
