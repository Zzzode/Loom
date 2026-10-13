// @file clipboard_job.cpp
// @brief ClipboardJob implementation: worker thread, posix_spawn, pipe I/O,
//        deadline-aware write/wait, process-group kill/reap.
//
// POSIX-only (Linux + macOS). The async pipeline is not available on Windows;
// the existing synchronous clip.exe path in clipboard.cppm is used instead.
module;

#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

module loom.platform.clipboard_job;

#ifndef _WIN32

extern "C" char** environ;

namespace loom::utils::clipboard {

namespace {

// ── Deadline helpers ────────────────────────────────────────────────────

using steady_clock = std::chrono::steady_clock;
using time_point = steady_clock::time_point;

/// Remaining milliseconds until the deadline. Returns 0 if expired.
[[nodiscard]] std::chrono::milliseconds
remaining(time_point deadline) noexcept {
    auto now = steady_clock::now();
    if (now >= deadline) return std::chrono::milliseconds{0};
    return std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
}

/// Poll timeout: min(100ms, remaining). Caller must check remaining > 0 first.
[[nodiscard]] int poll_timeout(time_point deadline) noexcept {
    auto rem = remaining(deadline);
    return static_cast<int>(
        std::min<std::chrono::milliseconds>(rem, std::chrono::milliseconds{100}).count());
}

// ── Pipe creation ───────────────────────────────────────────────────────

/// Create a pipe with close-on-exec on both ends.
/// Linux: pipe2(O_CLOEXEC). macOS: pipe() + fcntl(FD_CLOEXEC).
/// Returns {read_fd, write_fd} or {-1, -1} on failure.
[[nodiscard]] std::pair<int, int> create_pipe() noexcept {
    int pipefd[2];
#if defined(__linux__)
    if (::pipe2(pipefd, O_CLOEXEC) != 0) return {-1, -1};
#else
    if (::pipe(pipefd) != 0) return {-1, -1};
    ::fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
#endif
    return {pipefd[0], pipefd[1]};
}

// ── Kill/reap ───────────────────────────────────────────────────────────

/// Kill the process group and ensure full cleanup. Deadline-bounded:
///   1. kill(-pgid, SIGTERM)
///   2. TERM deadline (500ms): poll waitpid(WNOHANG) + kill(-pgid, 0)
///   3. After TERM deadline: kill(-pgid, 0) to check group
///   4. If group exists: kill(-pgid, SIGKILL) (unconditional)
///   5. KILL+reap deadline (500ms): poll kill(-pgid, 0) + waitpid(WNOHANG)
///   6. If leader still unreaped: set *unreaped_out = leader
///
/// Never uses blocking waitpid — if the leader is in uninterruptible sleep,
/// blocking waitpid would hang forever, violating the bounded-join guarantee.
void kill_and_reap(pid_t leader, std::optional<pid_t>* unreaped_out) noexcept {
    const pid_t pgid = leader;  // child is its own process group leader (SETPGROUP)

    // Step 1: SIGTERM the entire group.
    ::kill(-pgid, SIGTERM);

    // Step 2: TERM deadline (500ms absolute).
    auto term_deadline = steady_clock::now() + std::chrono::milliseconds{500};
    bool leader_exited = false;
    for (;;) {
        int status = 0;
        pid_t r = ::waitpid(leader, &status, WNOHANG);
        if (r == leader) { leader_exited = true; break; }
        if (r == -1 && errno == ECHILD) { leader_exited = true; break; }
        // Check if group is empty.
        if (::kill(-pgid, 0) == -1 && errno == ESRCH) {
            // Group empty. Leader may still be a zombie — try one more reap.
            r = ::waitpid(leader, &status, WNOHANG);
            if (r == leader || (r == -1 && errno == ECHILD)) leader_exited = true;
            break;
        }
        auto rem = remaining(term_deadline);
        if (rem <= std::chrono::milliseconds{0}) break;
        std::this_thread::sleep_for(
            std::min<std::chrono::milliseconds>(rem, std::chrono::milliseconds{100}));
    }

    // Step 3: After TERM deadline, check if group still exists.
    bool group_exists = (::kill(-pgid, 0) == 0);

    // Step 4: If group exists, SIGKILL unconditionally.
    if (group_exists) {
        ::kill(-pgid, SIGKILL);
    }

    // Step 5: KILL+reap deadline (500ms absolute, shared).
    auto kill_deadline = steady_clock::now() + std::chrono::milliseconds{500};
    for (;;) {
        // Reap the leader.
        if (!leader_exited) {
            int status = 0;
            pid_t r = ::waitpid(leader, &status, WNOHANG);
            if (r == leader) { leader_exited = true; }
            else if (r == -1 && errno == ECHILD) { leader_exited = true; }
        }
        // Check if group is empty.
        bool group_empty = (::kill(-pgid, 0) == -1 && errno == ESRCH);
        if (leader_exited && group_empty) return;

        auto rem = remaining(kill_deadline);
        if (rem <= std::chrono::milliseconds{0}) break;
        std::this_thread::sleep_for(
            std::min<std::chrono::milliseconds>(rem, std::chrono::milliseconds{100}));
    }

    // Step 6: Unreaped leader fallback.
    if (!leader_exited && unreaped_out) {
        *unreaped_out = leader;
    }
}

// ── Worker thread ───────────────────────────────────────────────────────

void worker_main_impl(
    std::shared_ptr<const JobInput> input,
    std::shared_ptr<CancellationState> cancel,
    std::optional<pid_t>* unreaped_pid,
    std::function<void(CopyResult)> on_complete);

void worker_main(
    std::shared_ptr<const JobInput> input,
    std::shared_ptr<CancellationState> cancel,
    std::optional<pid_t>* unreaped_pid,
    std::function<void(CopyResult)> on_complete) noexcept
{
    try {
        worker_main_impl(std::move(input), std::move(cancel),
                         unreaped_pid, std::move(on_complete));
    } catch (...) {
        // noexcept + throwing functions (std::function, std::format,
        // std::string) — report failure rather than std::terminate.
        CopyResult fail;
        fail.status = CopyResult::Status::NativeFailed;
        fail.error_detail = "worker exception";
        try { on_complete(std::move(fail)); } catch (...) {}
    }
}

void worker_main_impl(
    std::shared_ptr<const JobInput> input,
    std::shared_ptr<CancellationState> cancel,
    std::optional<pid_t>* unreaped_pid,
    std::function<void(CopyResult)> on_complete)
{
    const auto start = steady_clock::now();
    CopyResult result;
    const auto& text = *input->text;
    result.backend_name = input->argv.empty() ? input->backend_path : input->argv[0];

    auto finish = [&](CopyResult::Status status, std::string detail = {}) {
        result.status = status;
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            steady_clock::now() - start);
        result.error_detail = std::move(detail);
        on_complete(std::move(result));
    };

    // ── Block SIGPIPE (thread-local) ──────────────────────────────────
    // When the backend exits early and we write to the stdin pipe, the
    // default SIGPIPE disposition terminates the entire Loom process.
    // Blocking it makes write() return -1/EPIPE instead.
    {
        sigset_t mask;
        // No leading :: — sigemptyset/sigaddset are macros on macOS,
        // functions on Linux; unqualified calls work on both.
        sigemptyset(&mask);
        sigaddset(&mask, SIGPIPE);
        ::pthread_sigmask(SIG_BLOCK, &mask, nullptr);
    }

    // ── Create stdin pipe ─────────────────────────────────────────────
    auto [read_fd, write_fd] = create_pipe();
    if (read_fd < 0) {
        finish(CopyResult::Status::NativeFailed, "pipe creation failed");
        return;
    }

    // Set write end to non-blocking (parent writes without blocking).
    int flags = ::fcntl(write_fd, F_GETFL);
    if (flags >= 0) ::fcntl(write_fd, F_SETFL, flags | O_NONBLOCK);
    // Read end (child's stdin) remains blocking — normal tools expect
    // blocking stdin.

    // ── Open /dev/null for stdout/stderr ──────────────────────────────
    int devnull = ::open("/dev/null", O_RDWR | O_CLOEXEC);
    if (devnull < 0) {
        ::close(read_fd);
        ::close(write_fd);
        finish(CopyResult::Status::NativeFailed, "open /dev/null failed");
        return;
    }

    // ── Set up posix_spawn_file_actions ───────────────────────────────
    posix_spawn_file_actions_t actions;
    if (::posix_spawn_file_actions_init(&actions) != 0) {
        ::close(read_fd); ::close(write_fd); ::close(devnull);
        finish(CopyResult::Status::NativeFailed, "posix_spawn_file_actions_init failed");
        return;
    }
    ::posix_spawn_file_actions_adddup2(&actions, read_fd, STDIN_FILENO);
    ::posix_spawn_file_actions_adddup2(&actions, devnull, STDOUT_FILENO);
    ::posix_spawn_file_actions_adddup2(&actions, devnull, STDERR_FILENO);
    ::posix_spawn_file_actions_addclose(&actions, read_fd);
    ::posix_spawn_file_actions_addclose(&actions, write_fd);
    ::posix_spawn_file_actions_addclose(&actions, devnull);

    // ── Set up posix_spawnattr ────────────────────────────────────────
    posix_spawnattr_t attr;
    if (::posix_spawnattr_init(&attr) != 0) {
        ::posix_spawn_file_actions_destroy(&actions);
        ::close(read_fd); ::close(write_fd); ::close(devnull);
        finish(CopyResult::Status::NativeFailed, "posix_spawnattr_init failed");
        return;
    }

    // POSIX_SPAWN_SETPGROUP: child becomes its own process group leader.
    // Required for kill(-pgid, ...) to kill all descendants.
    //
    // POSIX_SPAWN_SETSIGMASK with an EMPTY mask — the child must NOT
    // inherit the worker thread's blocked SIGPIPE.
    if (::posix_spawnattr_setflags(&attr,
            static_cast<short>(POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK)) != 0) {
        ::posix_spawnattr_destroy(&attr);
        ::posix_spawn_file_actions_destroy(&actions);
        ::close(read_fd); ::close(write_fd); ::close(devnull);
        // No degraded kill(pid) path — cannot guarantee descendant cleanup.
        finish(CopyResult::Status::NativeFailed, "SETPGROUP|SETSIGMASK not supported");
        return;
    }

    sigset_t empty_mask;
    sigemptyset(&empty_mask);
    if (::posix_spawnattr_setsigmask(&attr, &empty_mask) != 0) {
        ::posix_spawnattr_destroy(&attr);
        ::posix_spawn_file_actions_destroy(&actions);
        ::close(read_fd); ::close(write_fd); ::close(devnull);
        finish(CopyResult::Status::NativeFailed, "SETSIGMASK failed");
        return;
    }

    // ── Build argv ────────────────────────────────────────────────────
    std::vector<char*> argv;
    argv.reserve(input->argv.size() + 1);
    for (auto& arg : input->argv) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    // ── Spawn ─────────────────────────────────────────────────────────
    pid_t pid = 0;
    int rc = ::posix_spawnp(&pid, input->backend_path.c_str(),
                            &actions, &attr, argv.data(), environ);
    ::posix_spawnattr_destroy(&attr);
    ::posix_spawn_file_actions_destroy(&actions);
    ::close(read_fd);   // parent doesn't need the read end
    ::close(devnull);

    if (rc != 0) {
        ::close(write_fd);
        finish(CopyResult::Status::NativeFailed,
               std::format("posix_spawnp failed: {}", std::strerror(rc)));
        return;
    }

    // ── Write text to backend stdin (non-blocking, deadline-aware) ────
    auto deadline = steady_clock::now() + std::chrono::seconds{3};
    std::size_t written = 0;
    bool write_failed = false;

    while (written < text.size()) {
        // Check cancel and deadline.
        if (cancel->cancelled.load(std::memory_order_acquire)) {
            kill_and_reap(pid, unreaped_pid);
            ::close(write_fd);
            finish(CopyResult::Status::NativeFailed, "cancelled");
            return;
        }
        auto rem = remaining(deadline);
        if (rem <= std::chrono::milliseconds{0}) {
            // Deadline expired — kill and fall through to OSC 52.
            kill_and_reap(pid, unreaped_pid);
            ::close(write_fd);
            finish(CopyResult::Status::NativeFailed, "write deadline expired");
            return;
        }

        // Poll for write availability.
        struct pollfd pfd{write_fd, POLLOUT, 0};
        int pr = ::poll(&pfd, 1, poll_timeout(deadline));
        if (pr < 0) {
            if (errno == EINTR) continue;
            write_failed = true;
            break;
        }
        if (pr == 0) continue;  // timeout, re-check cancel/deadline
        if (pfd.revents & (POLLERR | POLLHUP)) {
            // Backend closed stdin — treat as failure.
            write_failed = true;
            break;
        }
        if (!(pfd.revents & POLLOUT)) continue;

        // Non-blocking write.
        ssize_t n = ::write(write_fd,
                            text.data() + written,
                            text.size() - written);
        if (n > 0) {
            written += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;  // poll again
            if (errno == EPIPE) {
                // Backend exited early — SIGPIPE is blocked, so we get here.
                write_failed = true;
                break;
            }
            write_failed = true;
            break;
        }
    }

    ::close(write_fd);  // signal EOF on child stdin
    result.bytes_sent = written;

    if (write_failed) {
        kill_and_reap(pid, unreaped_pid);
        finish(CopyResult::Status::NativeFailed, "write failed (backend exited early?)");
        return;
    }

    // ── Wait for backend exit (deadline-aware, cancel-responsive) ─────
    for (;;) {
        if (cancel->cancelled.load(std::memory_order_acquire)) {
            kill_and_reap(pid, unreaped_pid);
            finish(CopyResult::Status::NativeFailed, "cancelled");
            return;
        }
        auto rem = remaining(deadline);
        if (rem <= std::chrono::milliseconds{0}) {
            kill_and_reap(pid, unreaped_pid);
            finish(CopyResult::Status::NativeFailed, "wait deadline expired");
            return;
        }

        int status = 0;
        pid_t r = ::waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            // Child exited.
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                finish(CopyResult::Status::NativeCopied);
            } else {
                finish(CopyResult::Status::NativeFailed,
                       std::format("backend exited with status {}", status));
            }
            return;
        }
        if (r == -1 && errno == ECHILD) {
            // Already reaped (shouldn't happen, but handle gracefully).
            finish(CopyResult::Status::NativeFailed, "ECHILD from waitpid");
            return;
        }
        // Still running — sleep with bounded timeout.
        std::this_thread::sleep_for(
            std::min<std::chrono::milliseconds>(remaining(deadline),
                                                std::chrono::milliseconds{100}));
    }
}

}  // namespace

// ── ClipboardJob ────────────────────────────────────────────────────────

ClipboardJob::ClipboardJob(
    std::shared_ptr<const JobInput> input,
    std::shared_ptr<CancellationState> cancel,
    std::function<void(CopyResult)> on_complete)
    : input_(std::move(input))
    , cancel_(std::move(cancel))
    , on_complete_(std::move(on_complete))
{
    // The worker writes to unreaped_pid_ before exiting. join() provides
    // happens-before, so the owner thread can safely read it after join().
    worker_ = std::thread(worker_main, input_, cancel_, &unreaped_pid_,
        [this](CopyResult result) {
            on_complete_(std::move(result));
        });
}

ClipboardJob::~ClipboardJob() {
    if (worker_.joinable()) {
        cancel();
        join();
    }
}

void ClipboardJob::cancel() noexcept {
    cancel_->cancelled.store(true, std::memory_order_release);
}

void ClipboardJob::join() noexcept {
    if (worker_.joinable()) {
        worker_.join();
    }
}

std::optional<pid_t> ClipboardJob::unreaped_pid() const noexcept {
    return unreaped_pid_;
}

}  // namespace loom::utils::clipboard

#else  // _WIN32

// Windows stub: the async pipeline is POSIX-only. The existing synchronous
// clip.exe path in clipboard.cppm is used instead. The coordinator's
// post() on Windows calls the callback with Unsupported immediately.

namespace loom::utils::clipboard {

ClipboardJob::ClipboardJob(
    std::shared_ptr<const JobInput>,
    std::shared_ptr<CancellationState>,
    std::function<void(CopyResult)> on_complete) {
    // Immediately report Unsupported on Windows.
    CopyResult result;
    result.status = CopyResult::Status::Unsupported;
    on_complete(std::move(result));
}

ClipboardJob::~ClipboardJob() = default;

void ClipboardJob::cancel() noexcept {}
void ClipboardJob::join() noexcept {}

std::optional<pid_t> ClipboardJob::unreaped_pid() const noexcept {
    return std::nullopt;
}

}  // namespace loom::utils::clipboard

#endif  // _WIN32
