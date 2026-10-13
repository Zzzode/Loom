/// @file test_terminal_output.cpp
/// @brief Length-aware stdout delivery and write/flush error reporting.

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>

import std;
import loom.terminal.output;
import loom.utils.error;

namespace output = loom::terminal::output;

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
        // Discard failed buffered bytes before restoring the real terminal.
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

void expect_output(std::string_view bytes) {
    StdoutCapture capture;
    ASSERT_TRUE(capture.ready());
    ASSERT_TRUE(output::write_control_sequence(bytes).has_value());
    EXPECT_EQ(capture.bytes(), bytes);
}

// Buffer-mode changes stay in the death-test child, preserving the runner.
void expect_output_failure(bool buffered, std::string_view message) {
    bool expected_failure = false;
    {
        StdoutCapture capture;
        if (!capture.ready() || !capture.redirect_read_only()) std::_Exit(2);
        if (std::setvbuf(stdout, nullptr, buffered ? _IOFBF : _IONBF, BUFSIZ) != 0) {
            std::_Exit(3);
        }
        const auto result = output::write_control_sequence("\033]52;c;dGVzdA==\a");
        expected_failure = !result.has_value() &&
            result.error().code() == loom::utils::ErrorCode::io_error &&
            result.error().message() == message;
    }
    std::_Exit(expected_failure ? 0 : 1);
}

}  // namespace

TEST(TerminalOutput, EmptyBytes) { expect_output(std::string_view{}); }
TEST(TerminalOutput, ExactControlBytes) { expect_output("\033]52;c;YQ==\a"); }
TEST(TerminalOutput, EmbeddedNullIsNotTruncated) {
    expect_output(std::string_view("a\0b", 3));
}
TEST(TerminalOutput, WritesOnlyStringViewRange) {
    const std::string bytes = "prefix\033]2;Loom\033\\suffix";
    expect_output(std::string_view(bytes).substr(6, 10));
}

// These two cases migrate the old OSC 52 emission failure coverage.
TEST(TerminalOutputDeathTest, ReportsWriteFailure) {
    EXPECT_EXIT(expect_output_failure(false, "Terminal output: fwrite failed"),
                testing::ExitedWithCode(0), "");
}
TEST(TerminalOutputDeathTest, ReportsFlushFailure) {
    EXPECT_EXIT(expect_output_failure(true, "Terminal output: fflush failed"),
                testing::ExitedWithCode(0), "");
}
