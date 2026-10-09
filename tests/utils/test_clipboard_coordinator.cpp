/// @file test_clipboard_coordinator.cpp
/// @brief RFC 0007 Phase 1: ClipboardCoordinator cancel/reap/start tests.
///
/// Tests the background owner thread serialization: cancel/reap old job
/// before starting new one, generation counter, pending_reaps_ sweep,
/// and shutdown behavior.

#include <chrono>
#include <gtest/gtest.h>

import std;
import loom.platform.clipboard_coordinator;
import loom.platform.clipboard_job;
import loom.platform.find_executable;

namespace clip = loom::utils::clipboard;

namespace {

using steady_clock = std::chrono::steady_clock;

/// Wait for a condition with a timeout.
template <typename F>
bool wait_for(F&& cond, std::chrono::milliseconds timeout = std::chrono::seconds(10)) {
    auto deadline = steady_clock::now() + timeout;
    while (steady_clock::now() < deadline) {
        if (cond()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return cond();
}

/// Find a command in PATH.
std::string find_cmd(std::string_view name) {
    auto path = loom::utils::find_executable(name);
    return path ? path->string() : std::string{};
}

/// RAII helper to create a temp directory with a fake `xclip` that sleeps.
/// This lets the coordinator's resolve_native_backend() find a slow backend.
struct FakeSlowBackend {
    std::filesystem::path dir;
    std::string old_path;
    bool wayland_was_set = false;
    std::string old_wayland;
    bool ssh_conn_was_set = false;
    std::string old_ssh_conn;
    bool ssh_tty_was_set = false;
    std::string old_ssh_tty;

    FakeSlowBackend() {
        auto base = std::filesystem::temp_directory_path() /
            ("loom_fake_xclip_" +
             std::to_string(steady_clock::now().time_since_epoch().count()));
        dir = base;
        std::filesystem::create_directories(dir);
        // Fake xclip: sleep forever (until killed).
        auto script = dir / "xclip";
        {
            std::ofstream f(script);
            f << "#!/bin/sh\nexec sleep 100\n";
        }
        std::filesystem::permissions(script,
            std::filesystem::perms::owner_all |
            std::filesystem::perms::group_read |
            std::filesystem::perms::others_read);
        // Prepend our dir to PATH so find_executable finds our fake xclip.
        old_path = std::getenv("PATH") ? std::getenv("PATH") : "";
        auto new_path = dir.string() + ":" + old_path;
        ::setenv("PATH", new_path.c_str(), 1);
        // Unset WAYLAND_DISPLAY so resolve_native_backend falls through to xclip.
        if (const char* wd = std::getenv("WAYLAND_DISPLAY")) {
            wayland_was_set = true;
            old_wayland = wd;
            ::unsetenv("WAYLAND_DISPLAY");
        }
        // Unset SSH env vars so is_ssh_session() returns false and the
        // coordinator actually resolves and starts the fake backend.
        if (const char* sc = std::getenv("SSH_CONNECTION")) {
            ssh_conn_was_set = true;
            old_ssh_conn = sc;
            ::unsetenv("SSH_CONNECTION");
        }
        if (const char* st = std::getenv("SSH_TTY")) {
            ssh_tty_was_set = true;
            old_ssh_tty = st;
            ::unsetenv("SSH_TTY");
        }
    }

    ~FakeSlowBackend() {
        ::setenv("PATH", old_path.c_str(), 1);
        if (wayland_was_set) {
            ::setenv("WAYLAND_DISPLAY", old_wayland.c_str(), 1);
        } else {
            ::unsetenv("WAYLAND_DISPLAY");
        }
        if (ssh_conn_was_set) {
            ::setenv("SSH_CONNECTION", old_ssh_conn.c_str(), 1);
        } else {
            ::unsetenv("SSH_CONNECTION");
        }
        if (ssh_tty_was_set) {
            ::setenv("SSH_TTY", old_ssh_tty.c_str(), 1);
        } else {
            ::unsetenv("SSH_TTY");
        }
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
};

/// Make a copy request with a callback that stores the result.
clip::CopyRequest make_request(
    std::string text,
    std::mutex& mutex,
    std::optional<clip::CopyResult>& result,
    std::condition_variable& cv) {
    clip::CopyRequest req;
    req.text = std::make_shared<const std::string>(std::move(text));
    req.on_complete = [&](clip::CopyResult r) {
        {
            std::lock_guard lock(mutex);
            result = std::move(r);
        }
        cv.notify_one();
    };
    return req;
}

}  // namespace

// ── Basic post/shutdown ─────────────────────────────────────────────────

TEST(ClipboardCoordinator, PostAndShutdown) {
    clip::ClipboardCoordinator coordinator;
    coordinator.shutdown();  // should not hang
}

TEST(ClipboardCoordinator, PostAfterShutdownIsDiscarded) {
    clip::ClipboardCoordinator coordinator;
    coordinator.shutdown();

    std::mutex mutex;
    std::optional<clip::CopyResult> result;
    std::condition_variable cv;

    auto cat = find_cmd("cat");
    if (cat.empty()) GTEST_SKIP() << "cat not found";

    auto req = make_request("test", mutex, result, cv);
    auto gen = coordinator.post(std::move(req));
    EXPECT_GT(gen, 0u);

    // Result should never arrive (request discarded).
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::lock_guard lock(mutex);
    EXPECT_FALSE(result.has_value());
}

// ── Generation counter ──────────────────────────────────────────────────

TEST(ClipboardCoordinator, GenerationCounterIncrements) {
    clip::ClipboardCoordinator coordinator;

    auto cat = find_cmd("cat");
    if (cat.empty()) { coordinator.shutdown(); GTEST_SKIP() << "cat not found"; }

    std::mutex mutex;
    std::optional<clip::CopyResult> result;
    std::condition_variable cv;

    auto gen1 = coordinator.post(
        make_request("first", mutex, result, cv));
    auto gen2 = coordinator.post(
        make_request("second", mutex, result, cv));

    EXPECT_GT(gen2, gen1);

    coordinator.shutdown();
}

// ── Cancel/reap/start serialization ─────────────────────────────────────

TEST(ClipboardCoordinator, SlowJobCancelledBeforeNewJobStarts) {
    // Use a fake slow xclip so the first job takes a long time.
    // The coordinator should cancel/reap it before starting the second.
    FakeSlowBackend fake;

    clip::ClipboardCoordinator coordinator;

    std::mutex mutex;
    std::optional<clip::CopyResult> result1;
    std::optional<clip::CopyResult> result2;
    std::condition_variable cv;
    std::atomic<int> completed_count{0};

    // Post a slow job (fake xclip sleeps 100s).
    clip::CopyRequest slow_req;
    slow_req.text = std::make_shared<const std::string>("slow");
    slow_req.on_complete = [&](clip::CopyResult r) {
        completed_count.fetch_add(1, std::memory_order_acq_rel);
        {
            std::lock_guard lock(mutex);
            result1 = std::move(r);
        }
        cv.notify_one();
    };
    [[maybe_unused]] auto gen_slow = coordinator.post(std::move(slow_req));

    // Give the owner thread time to start the first job.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Post a second job — this should cancel/reap the first.
    clip::CopyRequest fast_req;
    fast_req.text = std::make_shared<const std::string>("fast");
    fast_req.on_complete = [&](clip::CopyResult r) {
        completed_count.fetch_add(1, std::memory_order_acq_rel);
        {
            std::lock_guard lock(mutex);
            result2 = std::move(r);
        }
        cv.notify_one();
    };
    [[maybe_unused]] auto gen_fast = coordinator.post(std::move(fast_req));

    // Wait for at least one result.
    EXPECT_TRUE(wait_for([&] {
        return completed_count.load(std::memory_order_acquire) >= 1;
    }, std::chrono::seconds(10)));

    // The first job should have been cancelled (NativeFailed, not NativeCopied).
    // The second job may or may not have completed yet (it also uses the fake
    // slow backend, so it will also be cancelled on shutdown).
    {
        std::lock_guard lock(mutex);
        if (result1.has_value()) {
            EXPECT_NE(result1->status, clip::CopyResult::Status::NativeCopied)
                << "first (slow) job should have been cancelled, not copied";
        }
    }

    coordinator.shutdown();
}

// ── Shutdown during active copy ─────────────────────────────────────────

TEST(ClipboardCoordinator, ShutdownDuringActiveCopyDoesNotHang) {
    // Use a fake slow xclip so the job is still running when we shut down.
    FakeSlowBackend fake;

    clip::ClipboardCoordinator coordinator;

    std::mutex mutex;
    std::optional<clip::CopyResult> result;
    std::condition_variable cv;

    auto req = make_request("test", mutex, result, cv);
    [[maybe_unused]] auto gen = coordinator.post(std::move(req));

    // Give the owner thread time to start the job.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Shutdown should cancel the in-flight job and complete within ~1.1s.
    auto start = steady_clock::now();
    coordinator.shutdown();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        steady_clock::now() - start);

    // Generous watchdog: 5s (the internal bound is ~1.1s).
    EXPECT_LT(elapsed.count(), 5000);

    // The job should have been cancelled (not copied).
    std::lock_guard lock(mutex);
    if (result.has_value()) {
        EXPECT_NE(result->status, clip::CopyResult::Status::NativeCopied)
            << "job should have been cancelled on shutdown";
    }
}
