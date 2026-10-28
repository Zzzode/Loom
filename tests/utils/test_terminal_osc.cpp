/// @file test_terminal_osc.cpp
/// @brief Exact wire bytes and pure OSC payload validation.

#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdio>

import std;
import loom.terminal.osc;
import loom.utils.error;

namespace osc = loom::terminal::osc;

namespace {

void expect_clipboard(std::string_view text, std::string_view encoded) {
    EXPECT_EQ(osc::clipboard_request(text), "\033]52;c;" + std::string(encoded) + "\a");
}

void expect_title(std::string_view title, std::string_view expected) {
    const auto result = osc::window_title(title);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, expected);
}

void expect_hyperlink(std::string_view uri, std::string_view text, std::string_view expected) {
    const auto result = osc::hyperlink(uri, text);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, expected);
}

class StdoutCapture {
public:
    StdoutCapture() {
        std::fflush(stdout);
        saved_fd_ = ::dup(STDOUT_FILENO);
        file_ = std::tmpfile();
        if (saved_fd_ >= 0 && file_) {
            ready_ = ::dup2(::fileno(file_), STDOUT_FILENO) >= 0;
            std::clearerr(stdout);
        }
    }

    ~StdoutCapture() {
        std::fflush(stdout);
        if (saved_fd_ >= 0) {
            (void)::dup2(saved_fd_, STDOUT_FILENO);
            ::close(saved_fd_);
        }
        std::clearerr(stdout);
        if (file_) std::fclose(file_);
    }

    [[nodiscard]] bool ready() const { return ready_; }

    [[nodiscard]] long size() {
        std::fflush(stdout);
        if (std::fseek(file_, 0, SEEK_END) != 0) return -1;
        return std::ftell(file_);
    }

private:
    int saved_fd_ = -1;
    std::FILE* file_ = nullptr;
    bool ready_ = false;
};

}  // namespace

// These seven cases migrate the original OSC 52 exact-byte coverage.
TEST(TerminalOsc, ClipboardEmptyText) {
    expect_clipboard("", "");
}

TEST(TerminalOsc, ClipboardOneBytePadding) {
    expect_clipboard("a", "YQ==");
}

TEST(TerminalOsc, ClipboardTwoBytePadding) {
    expect_clipboard("ab", "YWI=");
}

TEST(TerminalOsc, ClipboardThreeBytesWithoutPadding) {
    expect_clipboard("abc", "YWJj");
}

TEST(TerminalOsc, ClipboardUtf8Text) {
    expect_clipboard("中文🙂", "5Lit5paH8J+Zgg==");
}

TEST(TerminalOsc, ClipboardEmbeddedNull) {
    expect_clipboard(std::string_view("a\0b", 3), "YQBi");
}

TEST(TerminalOsc, ClipboardControlsAreEncodedRatherThanEmitted) {
    expect_clipboard("\033]52;c;\a\n\r\t", "G101MjtjOwcKDQk=");
}

TEST(TerminalOsc, WindowTitleAscii) {
    expect_title("Loom", "\033]2;Loom\033\\");
}

TEST(TerminalOsc, WindowTitleEmpty) {
    expect_title("", "\033]2;\033\\");
}

TEST(TerminalOsc, WindowTitleUtf8) {
    expect_title("中文🙂", "\033]2;中文🙂\033\\");
}

TEST(TerminalOsc, WindowTitleUsesStringViewRange) {
    const std::string title = "[Loom]";
    expect_title(std::string_view(title).substr(1, 4), "\033]2;Loom\033\\");
}

TEST(TerminalOsc, HyperlinkAscii) {
    expect_hyperlink("https://example.com/a;b", "Example",
                     "\033]8;;https://example.com/a;b\033\\Example\033]8;;\033\\");
}

TEST(TerminalOsc, HyperlinkEmptyUri) {
    expect_hyperlink("", "plain", "\033]8;;\033\\plain\033]8;;\033\\");
}

TEST(TerminalOsc, HyperlinkEmptyText) {
    expect_hyperlink("file:///tmp/example", "", "\033]8;;file:///tmp/example\033\\\033]8;;\033\\");
}

TEST(TerminalOsc, HyperlinkUtf8AndStringViewRange) {
    const std::string uri = "[file:///tmp/中文🙂]";
    expect_hyperlink(std::string_view(uri).substr(1, uri.size() - 2), "中文🙂",
                     "\033]8;;file:///tmp/中文🙂\033\\中文🙂\033]8;;\033\\");
}

TEST(TerminalOsc, HyperlinkDisplayTextIsOpaque) {
    constexpr char raw_text[] = "a\0\033[31m\a\177b";
    const std::string text(raw_text, sizeof(raw_text) - 1);
    const auto result = osc::hyperlink("https://example.com", text);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "\033]8;;https://example.com\033\\" + text + "\033]8;;\033\\");
}

TEST(TerminalOsc, WindowTitleRejectsEveryC0AndDelControl) {
    for (unsigned int control = 0; control <= 0x7f; ++control) {
        if (control >= 0x20 && control != 0x7f) continue;
        SCOPED_TRACE(control);
        const std::string title = "before" + std::string(1, static_cast<char>(control)) + "after";
        const auto result = osc::window_title(title);
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().code(), loom::utils::ErrorCode::invalid_argument);
    }
}

TEST(TerminalOsc, HyperlinkRejectsEveryC0AndDelControlInUri) {
    for (unsigned int control = 0; control <= 0x7f; ++control) {
        if (control >= 0x20 && control != 0x7f) continue;
        SCOPED_TRACE(control);
        const std::string uri =
            "https://example.com/" + std::string(1, static_cast<char>(control)) + "tail";
        const auto result = osc::hyperlink(uri, "Label");
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().code(), loom::utils::ErrorCode::invalid_argument);
    }
}

TEST(TerminalOsc, ProgramStatusQuery) {
    EXPECT_EQ(osc::program_status_query(), "\033]7501;?\033\\");
}

TEST(TerminalOsc, ProgramStatusIdle) {
    const auto encoded = osc::program_status(osc::ProgramStatusState::Idle);
    ASSERT_TRUE(encoded);
    EXPECT_EQ(*encoded, "\033]7501;state=idle:app=loom\033\\");
}

TEST(TerminalOsc, ProgramStatusWorkingAndTerminalOutcomes) {
    for (const auto& [state, name] : {std::pair{osc::ProgramStatusState::Working, "working"},
                                      std::pair{osc::ProgramStatusState::Done, "done"},
                                      std::pair{osc::ProgramStatusState::Error, "error"}}) {
        const auto encoded = osc::program_status(state);
        ASSERT_TRUE(encoded);
        EXPECT_EQ(*encoded, "\033]7501;state=" + std::string(name) + ":app=loom\033\\");
    }
}

TEST(TerminalOsc, ProgramStatusBlockedKinds) {
    const auto untyped = osc::program_status(osc::ProgramStatusState::Blocked);
    ASSERT_TRUE(untyped);
    EXPECT_EQ(*untyped, "\033]7501;state=blocked:app=loom\033\\");
    for (const auto& [kind, name] : {std::pair{osc::ProgramStatusKind::Permission, "permission"},
                                     std::pair{osc::ProgramStatusKind::Question, "question"},
                                     std::pair{osc::ProgramStatusKind::Auth, "auth"}}) {
        const auto encoded = osc::program_status(osc::ProgramStatusState::Blocked, kind);
        ASSERT_TRUE(encoded);
        EXPECT_EQ(*encoded,
                  "\033]7501;state=blocked:kind=" + std::string(name) + ":app=loom\033\\");
    }
}

TEST(TerminalOsc, ProgramStatusRejectsInvalidState) {
    const auto encoded = osc::program_status(static_cast<osc::ProgramStatusState>(999));
    ASSERT_FALSE(encoded);
    EXPECT_EQ(encoded.error().code(), loom::utils::ErrorCode::invalid_argument);
}

TEST(TerminalOsc, ProgramStatusRejectsInvalidKind) {
    const auto encoded = osc::program_status(osc::ProgramStatusState::Blocked,
                                             static_cast<osc::ProgramStatusKind>(999));
    ASSERT_FALSE(encoded);
    EXPECT_EQ(encoded.error().code(), loom::utils::ErrorCode::invalid_argument);
}

TEST(TerminalOsc, ProgramStatusRejectsKindOnNonBlockedState) {
    for (const auto state : {osc::ProgramStatusState::Idle, osc::ProgramStatusState::Working,
                             osc::ProgramStatusState::Done, osc::ProgramStatusState::Error}) {
        EXPECT_FALSE(osc::program_status(state, osc::ProgramStatusKind::Permission));
    }
}

TEST(TerminalOsc, EncodingAndValidationNeverWriteStdout) {
    StdoutCapture capture;
    ASSERT_TRUE(capture.ready());
    EXPECT_TRUE(osc::window_title("Loom").has_value());
    EXPECT_TRUE(osc::hyperlink("https://example.com", "Example").has_value());
    EXPECT_FALSE(osc::window_title("\033").has_value());
    EXPECT_FALSE(osc::hyperlink("\a", "Example").has_value());
    EXPECT_FALSE(osc::clipboard_request("text").empty());
    EXPECT_FALSE(osc::program_status_query().empty());
    EXPECT_TRUE(osc::program_status(osc::ProgramStatusState::Done).has_value());
    EXPECT_EQ(capture.size(), 0);
}
