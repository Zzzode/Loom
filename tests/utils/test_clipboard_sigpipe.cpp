/// @file test_clipboard_sigpipe.cpp
/// @brief RFC 0007 Phase 1: SIGPIPE handling tests.
///
/// Verifies that:
/// 1. The worker thread blocks SIGPIPE (write() returns EPIPE instead of
///    terminating the process when the backend exits early).
/// 2. The child process gets an empty signal mask (SIGPIPE not inherited
///    from the worker thread).

#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>

import std;
import loom.platform.clipboard_job;
import loom.platform.find_executable;

namespace clip = loom::utils::clipboard;

namespace {

using steady_clock = std::chrono::steady_clock;

template <typename F>
bool wait_for(F&& cond, std::chrono::milliseconds timeout = std::chrono::seconds(10)) {
    auto deadline = steady_clock::now() + timeout;
    while (steady_clock::now() < deadline) {
        if (cond()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return cond();
}

std::string find_cmd(std::string_view name) {
    auto path = loom::utils::find_executable(name);
    return path ? path->string() : std::string{};
}

std::shared_ptr<const clip::JobInput> make_input(std::string text, std::string backend_path,
                                                 std::vector<std::string> argv) {
    auto input = std::make_shared<clip::JobInput>();
    input->text = std::make_shared<const std::string>(std::move(text));
    input->backend_path = std::move(backend_path);
    input->argv = std::move(argv);
    return input;
}

}  // namespace

// ── SIGPIPE blocked in worker thread ────────────────────────────────────

TEST(ClipboardSigpipe, WorkerSurvivesBackendExitingEarly) {
    // `true` exits immediately without reading stdin. Writing to its stdin
    // would normally send SIGPIPE, terminating the process. The worker
    // blocks SIGPIPE, so write() returns -1/EPIPE and the job fails
    // gracefully.
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

    // The job should complete (not crash from SIGPIPE).
    EXPECT_TRUE(wait_for([&] {
        std::lock_guard lock(result_mutex);
        return result.has_value();
    }));

    job->join();
    ASSERT_TRUE(result.has_value());
    // The job should fail (EPIPE), not succeed and not crash.
    EXPECT_EQ(result->status, clip::CopyResult::Status::NativeFailed);
}

// ── Child signal mask is empty ──────────────────────────────────────────

TEST(ClipboardSigpipe, ChildSignalMaskIsEmpty) {
    // Use a shell script that checks its own signal mask. If SIGPIPE is
    // blocked in the child, the script detects it and exits non-zero.
    //
    // We use `sh -c 'trap - PIPE; kill -l PIPE'` to check. Actually, a
    // simpler approach: use `sh -c 'sigprocmask` is not available in sh.
    //
    // Alternative: spawn a child that writes to a closed pipe and check
    // if it gets killed by SIGPIPE. If SIGPIPE is blocked (inherited),
    // the child survives. If SIGPIPE is default (empty mask), the child
    // is killed.
    //
    // We use: sh -c 'kill -PIPE $$' — sends SIGPIPE to itself.
    // If SIGPIPE is blocked: the signal is pending but not delivered,
    //   and the process exits normally (exit 0).
    // If SIGPIPE is default: the process is killed by SIGPIPE (exit 13).
    //
    // Wait, that's not quite right. If SIGPIPE is blocked, kill -PIPE $$
    // sets it pending but the process continues. The process exits 0.
    // If SIGPIPE is default, the process is killed (signal 13).
    //
    // But we can't directly observe the child's exit status from the
    // ClipboardJob (it only reports success/failure). Instead, we verify
    // indirectly: the ClipboardJob uses POSIX_SPAWN_SETSIGMASK with an
    // empty mask, so the child should have SIGPIPE unblocked.
    //
    // For this test, we verify that the posix_spawnattr_setsigmask call
    // is made with an empty mask. This is a code-level guarantee, not
    // a runtime test. The runtime test would require a custom backend
    // that reports its signal mask, which is complex.
    //
    // Instead, we test the observable behavior: a backend that sends
    // SIGPIPE to itself should be killed (exit status non-zero), proving
    // SIGPIPE is not blocked in the child.
    auto sh = find_cmd("sh");
    if (sh.empty()) GTEST_SKIP() << "sh not found";

    std::mutex result_mutex;
    std::optional<clip::CopyResult> result;
    std::condition_variable cv;

    // sh -c 'kill -PIPE $$' — sends SIGPIPE to itself.
    // If SIGPIPE is blocked: exits 0 (NativeCopied).
    // If SIGPIPE is default: killed by signal 13 (NativeFailed).
    auto input = make_input("", sh, {"sh", "-c", "kill -PIPE $$"});
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
    // The child should be killed by SIGPIPE (not blocked), so the job
    // should report failure.
    EXPECT_EQ(result->status, clip::CopyResult::Status::NativeFailed);
}
