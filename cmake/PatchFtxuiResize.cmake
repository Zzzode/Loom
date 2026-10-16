# Reject a DOM if terminal dimensions changed while its component rendered.
# Defer one internal retry instead of drawing old layout into a new pixel grid
# or injecting an application-visible Custom event.
set(_loom_ftxui_screen "${ftxui_SOURCE_DIR}/src/ftxui/component/screen_interactive.cpp")
file(READ "${_loom_ftxui_screen}" _loom_ftxui_resize)
set(_loom_ftxui_old [=[  auto document = component->Render();
  int dimx = 0;
  int dimy = 0;
  auto terminal = Terminal::Size();]=])
set(_loom_ftxui_new [=[  const auto before_render = Terminal::Size();
  auto document = component->Render();
  int dimx = 0;
  int dimy = 0;
  auto terminal = Terminal::Size();
  if (before_render.dimx != terminal.dimx ||
      before_render.dimy != terminal.dimy) {
    Post([this] { frame_valid_ = false; });
    return;
  }]=])
string(FIND "${_loom_ftxui_resize}" "${_loom_ftxui_new}" _loom_ftxui_applied)
if(_loom_ftxui_applied EQUAL -1)
    string(FIND "${_loom_ftxui_resize}" "${_loom_ftxui_old}" _loom_ftxui_expected)
    if(_loom_ftxui_expected EQUAL -1)
        message(FATAL_ERROR "FTXUI Draw changed; review terminal-size consistency before applying the resize patch.")
    endif()
    string(REPLACE "${_loom_ftxui_old}" "${_loom_ftxui_new}" _loom_ftxui_resize "${_loom_ftxui_resize}")
    file(WRITE "${_loom_ftxui_screen}" "${_loom_ftxui_resize}")
endif()
unset(_loom_ftxui_screen)
unset(_loom_ftxui_resize)
unset(_loom_ftxui_old)
unset(_loom_ftxui_new)
unset(_loom_ftxui_applied)
unset(_loom_ftxui_expected)
