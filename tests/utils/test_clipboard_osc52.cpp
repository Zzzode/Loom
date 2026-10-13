/// @file test_clipboard_osc52.cpp
/// @brief Exact bytes and output failures for terminal clipboard requests.

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

import std;
import loom.platform.clipboard;
import loom.utils.error;

namespace clip = loom::utils::clipboard;

namespace {

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
        // A failed flush must not replay pending OSC bytes to the real terminal.
        if (std::ferror(stdout)) {
            (void)std::freopen("/dev/null", "w", stdout);
        } else {
            std::fflush(stdout);
        }
        if (saved_fd_ >= 0) {
            (void)::dup2(saved_fd_, STDOUT_FILENO);
            ::close(saved_fd_);
        }
        std::clearerr(stdout);
        if (file_) std::fclose(file_);
    }

    [[nodiscard]] bool ready() const { return ready_; }

    [[nodiscard]] std::string bytes() {
        std::fflush(stdout);
        std::rewind(file_);
        std::string result;
        std::array<char, 512> buffer;
        while (const auto count = std::fread(buffer.data(), 1, buffer.size(), file_)) {
            result.append(buffer.data(), count);
        }
        return result;
    }

    [[nodiscard]] bool redirect_read_only() {
        std::fflush(stdout);
        const int fd = ::open("/dev/null", O_RDONLY);
        if (fd < 0) return false;
        const bool redirected = ::dup2(fd, STDOUT_FILENO) >= 0;
        ::close(fd);
        std::clearerr(stdout);
        return redirected;
    }

private:
    int saved_fd_ = -1;
    std::FILE* file_ = nullptr;
    bool ready_ = false;
};

void expect_request(std::string_view text, std::string_view encoded) {
    StdoutCapture capture;
    ASSERT_TRUE(capture.ready());
    const auto result = clip::emit_osc52(text);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(capture.bytes(), "\033]52;c;" + std::string(encoded) + "\a");
}

// Buffer modes are changed only in the death-test child, so the test runner's
// stdout mode is preserved. The RAII guard restores stdout and its error state.
void expect_output_failure(bool buffered, std::string_view message) {
    bool expected_failure = false;
    {
        StdoutCapture capture;
        if (!capture.ready() || !capture.redirect_read_only()) std::_Exit(2);
        if (std::setvbuf(stdout, nullptr, buffered ? _IOFBF : _IONBF, BUFSIZ) != 0) {
            std::_Exit(3);
        }
        const auto result = clip::emit_osc52("test");
        expected_failure = !result.has_value() &&
            result.error().code() == loom::utils::ErrorCode::io_error &&
            result.error().message() == message;
    }
    std::_Exit(expected_failure ? 0 : 1);
}

}  // namespace

TEST(ClipboardOsc52, EmptyText) { expect_request("", ""); }
TEST(ClipboardOsc52, OneBytePadding) { expect_request("a", "YQ=="); }
TEST(ClipboardOsc52, TwoBytePadding) { expect_request("ab", "YWI="); }
TEST(ClipboardOsc52, ThreeBytesWithoutPadding) { expect_request("abc", "YWJj"); }
TEST(ClipboardOsc52, Utf8Text) { expect_request("中文🙂", "5Lit5paH8J+Zgg=="); }
TEST(ClipboardOsc52, EmbeddedNull) {
    expect_request(std::string_view("a\0b", 3), "YQBi");
}
TEST(ClipboardOsc52, ControlsAreEncodedRatherThanEmitted) {
    expect_request("\033]52;c;\a\n\r\t", "G101MjtjOwcKDQk=");
}

TEST(ClipboardOsc52DeathTest, ReportsWriteFailure) {
    EXPECT_EXIT(expect_output_failure(false, "OSC 52: fprintf failed"),
                testing::ExitedWithCode(0), "");
}

TEST(ClipboardOsc52DeathTest, ReportsFlushFailure) {
    EXPECT_EXIT(expect_output_failure(true, "OSC 52: fflush failed"),
                testing::ExitedWithCode(0), "");
}
