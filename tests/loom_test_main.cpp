// Loom test main: scrubs terminal-identifying environment variables so that
// rendered ANSI output is deterministic across developer machines.
//
// OSC 8 hyperlink emission (loom::utils::supports_hyperlinks) is gated on
// TERM_PROGRAM / LC_TERMINAL / WT_SESSION / VTE_VERSION. A developer's
// terminal (ghostty, iTerm2, vscode, ...) would otherwise leak hyperlink
// escape sequences into golden snapshots, which are recorded in a
// terminal-free environment. Tests that exercise hyperlink detection set
// these vars explicitly (see test_commands.cpp), so scrubbing the inherited
// environment only changes the default.
//
// COLORTERM is forced to "truecolor" so FTXUI always emits 24-bit RGB escape
// sequences (38;2;R;G;B) rather than quantizing to the 256-color palette
// (38;5;N) on terminals that don't advertise truecolor support (e.g. tmux
// without COLORTERM set). Golden snapshots are captured in truecolor mode.
//
// We also redirect LOOM_HISTORY_FILE to a temp file so that tests creating
// an AppAdapter (which can write to the prompt history via submit or Esc
// double-press) don't pollute the developer's real ~/.loom/history.jsonl.
// Tests that need their own history file override this via ScopedEnvVar
// (e.g. AppendAndReadHistoryRoundTrip).

#include <chrono>
#include <cstdlib>
#include <filesystem>

#include <ftxui/screen/terminal.hpp>
#include <gtest/gtest.h>

namespace {

void scrub_terminal_env() {
#ifdef _WIN32
    ::_putenv_s("TERM_PROGRAM", "");
    ::_putenv_s("LC_TERMINAL", "");
    ::_putenv_s("WT_SESSION", "");
    ::_putenv_s("VTE_VERSION", "");
    ::_putenv_s("COLORTERM", "truecolor");
#else
    ::unsetenv("TERM_PROGRAM");
    ::unsetenv("LC_TERMINAL");
    ::unsetenv("WT_SESSION");
    ::unsetenv("VTE_VERSION");
    ::setenv("COLORTERM", "truecolor", 1);
#endif
    // Force FTXUI into truecolor mode.  COLORTERM alone is not enough because
    // Terminal::ColorSupport() caches its result on first call, which can
    // happen during static initialization before main() runs.
    ftxui::Terminal::SetColorSupport(ftxui::Terminal::Color::TrueColor);
}

} // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    scrub_terminal_env();

    // Redirect prompt history to a temp file for the entire test run.
    namespace fs = std::filesystem;
    const auto hist_path = fs::temp_directory_path() /
        ("loom_test_history_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".jsonl");
    ::setenv("LOOM_HISTORY_FILE", hist_path.string().c_str(), 1);

    int result = RUN_ALL_TESTS();

    // Clean up the temp history file.
    fs::remove(hist_path);

    return result;
}
