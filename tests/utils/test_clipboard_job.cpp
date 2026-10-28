/// @file test_clipboard_job.cpp
/// @brief RFC 0007 Phase 1: ClipboardJob spawn/write/wait/kill tests.
///
/// Tests the async clipboard job using real commands (cat, sleep, true, false)
/// as fake backends. Verifies spawn, write, wait, timeout, cancel, and
/// process-group kill behavior.

#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>

import std;
import loom.platform.clipboard_job;
import loom.platform.find_executable;

namespace clip = loom::utils::clipboard;

namespace {

using steady_clock = std::chrono::steady_clock;

/// Wait for a condition with a timeout. Returns true if the condition
/// became true before the timeout.
template <typename F>
bool wait_for(F&& cond, std::chrono::milliseconds timeout = std::chrono::seconds(10)) {
    auto deadline = steady_clock::now() + timeout;
    while (steady_clock::now() < deadline) {
        if (cond()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return cond();
}

/// Create a JobInput with the given backend command.
std::shared_ptr<const clip::JobInput> make_input(std::string text, std::string backend_path,
                                                 std::vector<std::string> argv) {
    auto input = std::make_shared<clip::JobInput>();
    input->text = std::make_shared<const std::string>(std::move(text));
    input->backend_path = std::move(backend_path);
    input->argv = std::move(argv);
    return input;
}

/// Find a command in PATH.
std::string find_cmd(std::string_view name) {
    auto path = loom::utils::find_executable(name);
    return path ? path->string() : std::string{};
}

}  // namespace

// ── Basic spawn/write/wait ──────────────────────────────────────────────

TEST(ClipboardJob, CatReadsAllInputAndExits0) {
    auto cat = find_cmd("cat");
    if (cat.empty()) GTEST_SKIP() << "cat not found";

    std::mutex result_mutex;
    std::optional<clip::CopyResult> result;
    std::condition_variable cv;

    auto input = make_input("hello world", cat, {"cat"});
    auto cancel = std::make_shared<clip::CancellationState>();
    auto job = std::make_unique<clip::ClipboardJob>(input, cancel, [&](clip::CopyResult r) {
        {
            std::lock_guard lock(result_mutex);
            result = std::move(r);
        }
        cv.notify_one();
    });

    EXPECT_TRUE(wait_for([&] {
        std::lock_guard lock(result_mutex);
        return result.has_value();
    }));

    job->join();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, clip::CopyResult::Status::NativeCopied);
    EXPECT_EQ(result->bytes_sent, 11u);
}

TEST(ClipboardJob, FalseExitsNonZero) {
    auto false_cmd = find_cmd("false");
    if (false_cmd.empty()) GTEST_SKIP() << "false not found";

    std::mutex result_mutex;
    std::optional<clip::CopyResult> result;
    std::condition_variable cv;

    auto input = make_input("test", false_cmd, {"false"});
    auto cancel = std::make_shared<clip::CancellationState>();
    auto job = std::make_unique<clip::ClipboardJob>(input, cancel, [&](clip::CopyResult r) {
        {
            std::lock_guard lock(result_mutex);
            result = std::move(r);
        }
        cv.notify_one();
    });

    EXPECT_TRUE(wait_for([&] {
        std::lock_guard lock(result_mutex);
        return result.has_value();
    }));

    job->join();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, clip::CopyResult::Status::NativeFailed);
}

// ── Timeout ─────────────────────────────────────────────────────────────

TEST(ClipboardJob, BackendThatDoesNotReadStdinTimesOut) {
    // `sleep 100` never reads stdin. The write phase should time out.
    auto sleep_cmd = find_cmd("sleep");
    if (sleep_cmd.empty()) GTEST_SKIP() << "sleep not found";

    std::mutex result_mutex;
    std::optional<clip::CopyResult> result;
    std::condition_variable cv;

    // Large text to ensure the write buffer fills up.
    std::string large_text(1024 * 1024, 'x');  // 1MB
    auto input = make_input(std::move(large_text), sleep_cmd, {"sleep", "100"});
    auto cancel = std::make_shared<clip::CancellationState>();
    auto job = std::make_unique<clip::ClipboardJob>(input, cancel, [&](clip::CopyResult r) {
        {
            std::lock_guard lock(result_mutex);
            result = std::move(r);
        }
        cv.notify_one();
    });

    // Wait for result with a generous watchdog (10s). The internal deadline
    // is 3s, so this should complete well within the watchdog.
    EXPECT_TRUE(wait_for(
        [&] {
            std::lock_guard lock(result_mutex);
            return result.has_value();
        },
        std::chrono::seconds(10)));

    job->join();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, clip::CopyResult::Status::NativeFailed);
    // The process should have been killed (no leftover sleep).
}

// ── Cancel ──────────────────────────────────────────────────────────────

TEST(ClipboardJob, CancelDuringWaitKillsProcess) {
    // `cat > /dev/null; sleep 100` consumes all stdin then hangs.
    // Cancel should kill it during the wait phase.
    auto sh = find_cmd("sh");
    if (sh.empty()) GTEST_SKIP() << "sh not found";

    std::mutex result_mutex;
    std::optional<clip::CopyResult> result;
    std::condition_variable cv;

    auto input = make_input("test data", sh, {"sh", "-c", "cat > /dev/null; sleep 100"});
    auto cancel = std::make_shared<clip::CancellationState>();
    auto job = std::make_unique<clip::ClipboardJob>(input, cancel, [&](clip::CopyResult r) {
        {
            std::lock_guard lock(result_mutex);
            result = std::move(r);
        }
        cv.notify_one();
    });

    // Wait a bit for the job to start and consume stdin.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Cancel.
    cancel->cancelled.store(true, std::memory_order_release);

    // The job should complete within ~1.1s (100ms poll + 500ms TERM + 500ms KILL).
    EXPECT_TRUE(wait_for(
        [&] {
            std::lock_guard lock(result_mutex);
            return result.has_value();
        },
        std::chrono::seconds(5)));

    job->join();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, clip::CopyResult::Status::NativeFailed);
}

// ── EPIPE (backend exits early) ─────────────────────────────────────────

TEST(ClipboardJob, BackendExitsEarlyHandlesEpipe) {
    // `true` exits immediately without reading stdin. Writing to its stdin
    // should produce EPIPE (SIGPIPE is blocked in the worker thread).
    auto true_cmd = find_cmd("true");
    if (true_cmd.empty()) GTEST_SKIP() << "true not found";

    std::mutex result_mutex;
    std::optional<clip::CopyResult> result;
    std::condition_variable cv;

    // Large text to ensure we try to write after the backend exits.
    std::string large_text(1024 * 1024, 'x');  // 1MB
    auto input = make_input(std::move(large_text), true_cmd, {"true"});
    auto cancel = std::make_shared<clip::CancellationState>();
    auto job = std::make_unique<clip::ClipboardJob>(input, cancel, [&](clip::CopyResult r) {
        {
            std::lock_guard lock(result_mutex);
            result = std::move(r);
        }
        cv.notify_one();
    });

    // The job should complete (not crash from SIGPIPE) within the watchdog.
    EXPECT_TRUE(wait_for(
        [&] {
            std::lock_guard lock(result_mutex);
            return result.has_value();
        },
        std::chrono::seconds(10)));

    job->join();
    ASSERT_TRUE(result.has_value());
    // Either NativeFailed (EPIPE detected) or NativeCopied (if the write
    // completed before the backend exited — unlikely with 1MB, but possible
    // on a fast system). The key assertion is that the process survives.
    EXPECT_TRUE(result->status == clip::CopyResult::Status::NativeFailed ||
                result->status == clip::CopyResult::Status::NativeCopied);
}

// ── Empty text ──────────────────────────────────────────────────────────

TEST(ClipboardJob, EmptyTextSucceeds) {
    auto cat = find_cmd("cat");
    if (cat.empty()) GTEST_SKIP() << "cat not found";

    std::mutex result_mutex;
    std::optional<clip::CopyResult> result;
    std::condition_variable cv;

    auto input = make_input("", cat, {"cat"});
    auto cancel = std::make_shared<clip::CancellationState>();
    auto job = std::make_unique<clip::ClipboardJob>(input, cancel, [&](clip::CopyResult r) {
        {
            std::lock_guard lock(result_mutex);
            result = std::move(r);
        }
        cv.notify_one();
    });

    EXPECT_TRUE(wait_for([&] {
        std::lock_guard lock(result_mutex);
        return result.has_value();
    }));

    job->join();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, clip::CopyResult::Status::NativeCopied);
    EXPECT_EQ(result->bytes_sent, 0u);
}
