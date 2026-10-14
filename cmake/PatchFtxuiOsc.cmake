# FTXUI owns the only terminal reader. Keep slow OSC 7501 replies opaque,
# bounded and BEL-compatible without changing other OSCs or Escape timing.
function(_loom_patch_ftxui_osc_exact path old new)
    file(READ "${path}" content)
    string(FIND "${content}" "${new}" applied)
    if(applied EQUAL -1)
        string(FIND "${content}" "${old}" expected)
        if(expected EQUAL -1)
            message(FATAL_ERROR "FTXUI OSC parser changed; review OSC 7501 framing bounds.")
        endif()
        string(REPLACE "${old}" "${new}" content "${content}")
        file(WRITE "${path}" "${content}")
    endif()
endfunction()

set(_loom_ftxui_parser_header "${ftxui_SOURCE_DIR}/src/ftxui/component/terminal_input_parser.hpp")
set(_loom_ftxui_parser_source "${ftxui_SOURCE_DIR}/src/ftxui/component/terminal_input_parser.cpp")
_loom_patch_ftxui_osc_exact("${_loom_ftxui_parser_source}"
    [=[#include "ftxui/component/terminal_input_parser.hpp"]=]
    [=[#include "ftxui/component/terminal_input_parser.hpp"
#include <algorithm>  // for bounded OSC prefix comparison]=])
_loom_patch_ftxui_osc_exact("${_loom_ftxui_parser_header}"
    [=[#include <memory>  // for unique_ptr]=]
    [=[#include <chrono>  // for bounded OSC 7501 frame age
#include <memory>  // for unique_ptr]=])
_loom_patch_ftxui_osc_exact("${_loom_ftxui_parser_header}"
    [=[  std::string pending_;
};]=]
    [=[  std::string pending_;

  bool ProgramStatusPending() const;
  bool ExpireProgramStatus();
  void ResetProgramStatus();
  std::chrono::steady_clock::time_point program_status_started_{};
  int program_status_elapsed_ = 0;
  bool program_status_discarding_ = false;
  bool program_status_escape_ = false;
};]=])
_loom_patch_ftxui_osc_exact("${_loom_ftxui_parser_source}"
    [=[void TerminalInputParser::Timeout(int time) {
  timeout_ += time;
  const int timeout_threshold = 50;
  if (timeout_ < timeout_threshold) {
    return;
  }
  timeout_ = 0;
  if (!pending_.empty()) {
    Send(SPECIAL);
  }
}

void TerminalInputParser::Add(char c) {
  // A second escape starts a new sequence after a standalone Escape key.
  if (c == '\x1B' && pending_ == "\x1B") {
    Send(SPECIAL);
  }
  pending_ += c;
  timeout_ = 0;
  position_ = -1;
  Send(Parse());
}]=]
    [=[bool TerminalInputParser::ProgramStatusPending() const {
  if (pending_.size() < 2) {
    return false;
  }
  const auto length = std::min(pending_.size(), std::size_t{7});
  return pending_.compare(0, length, "\x1B]7501;", length) == 0;
}

void TerminalInputParser::ResetProgramStatus() {
  program_status_started_ = {};
  program_status_elapsed_ = 0;
  program_status_discarding_ = false;
  program_status_escape_ = false;
}

bool TerminalInputParser::ExpireProgramStatus() {
  if (program_status_started_ == std::chrono::steady_clock::time_point{}) {
    return false;
  }
  if (program_status_elapsed_ < 1000 &&
      std::chrono::steady_clock::now() - program_status_started_ <
          std::chrono::milliseconds(1000)) {
    return false;
  }
  if (!pending_.empty()) {
    Send(SPECIAL);
  }
  ResetProgramStatus();
  timeout_ = 0;
  return true;
}

void TerminalInputParser::Timeout(int time) {
  if (ProgramStatusPending() || program_status_discarding_) {
    program_status_elapsed_ += time;
    ExpireProgramStatus();
    return;
  }
  timeout_ += time;
  const int timeout_threshold = 50;
  if (timeout_ < timeout_threshold) {
    return;
  }
  timeout_ = 0;
  if (!pending_.empty()) {
    Send(SPECIAL);
  }
}

void TerminalInputParser::Add(char c) {
  // Check real frame age even when continuous input never calls Timeout().
  ExpireProgramStatus();
  if (ProgramStatusPending() && pending_.size() >= 4096) {
    program_status_escape_ = pending_.back() == '\x1B';
    Send(SPECIAL);
    program_status_discarding_ = true;
  }
  if (program_status_discarding_) {
    if (c == '\a' || (program_status_escape_ && c == '\\')) {
      ResetProgramStatus();
    } else {
      program_status_escape_ = c == '\x1B';
    }
    timeout_ = 0;
    return;
  }
  // A second escape starts a new sequence after a standalone Escape key.
  if (c == '\x1B' && pending_ == "\x1B") {
    Send(SPECIAL);
  }
  pending_ += c;
  timeout_ = 0;
  position_ = -1;
  if (ProgramStatusPending()) {
    if (program_status_started_ == std::chrono::steady_clock::time_point{}) {
      program_status_started_ = std::chrono::steady_clock::now();
    }
  } else {
    ResetProgramStatus();
  }
  Send(Parse());
  if (pending_.empty()) {
    ResetProgramStatus();
  }
}]=])
_loom_patch_ftxui_osc_exact("${_loom_ftxui_parser_source}"
    [=[TerminalInputParser::Output TerminalInputParser::ParseOSC() {
  // Parse until the string terminator ST.
  while (true) {
    if (!Eat()) {
      return UNCOMPLETED;
    }
    if (Current() != '\x1B') {]=]
    [=[TerminalInputParser::Output TerminalInputParser::ParseOSC() {
  // OSC 7501 scans each byte so malformed ESC prefixes cannot hide ST/BEL.
  if (pending_.compare(0, 7, "\x1B]7501;") == 0) {
    bool escape = false;
    while (Eat()) {
      if (Current() == '\a' || (escape && Current() == '\\')) {
        return SPECIAL;
      }
      escape = Current() == '\x1B';
    }
    return UNCOMPLETED;
  }
  // Other OSC framing remains unchanged.
  while (true) {
    if (!Eat()) {
      return UNCOMPLETED;
    }
    if (Current() != '\x1B') {]=])
unset(_loom_ftxui_parser_header)
unset(_loom_ftxui_parser_source)
