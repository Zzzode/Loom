// @file clipboard_job.cppm
// @brief ClipboardJob: a single async clipboard write job.
//
// A ClipboardJob owns a worker thread that:
//   1. Spawns a clipboard backend (pbcopy / xclip / wl-copy) via posix_spawn
//   2. Writes the selected text to the backend's stdin (non-blocking, polled)
//   3. Waits for the backend to exit (waitpid WNOHANG, polled)
//   4. On timeout/cancel: kills the process group (SIGTERM → SIGKILL + reap)
//
// The worker thread blocks SIGPIPE at thread start so that a backend exiting
// early does not terminate the Loom process. The child process gets an empty
// signal mask via POSIX_SPAWN_SETSIGMASK so it does NOT inherit the blocked
// SIGPIPE.
//
// POSIX-only (Linux + macOS). Windows keeps the existing synchronous
// clip.exe path (see clipboard.cppm write_text()).
module;

#include <sys/types.h>  // pid_t

export module loom.platform.clipboard_job;

import std;

export namespace loom::utils::clipboard {

/// Immutable input for a clipboard write job.
struct JobInput {
    std::shared_ptr<const std::string> text;  ///< Text to write to backend stdin.
    std::string backend_path;                 ///< Resolved absolute path to the backend executable.
    std::vector<std::string> argv;            ///< argv[0] = backend name, rest = args.
};

/// Mutable cancellation state shared between coordinator and worker.
/// Separate from JobInput (which is immutable/const) so the cancel atomic
/// has its own mutable object.
struct CancellationState {
    std::atomic<bool> cancelled{false};
};

/// Result of a clipboard write job.
struct CopyResult {
    enum class Status {
        NativeCopied,  ///< Backend exited 0 — clipboard is set.
        NativeFailed,  ///< Backend failed, timed out, or was cancelled.
        Unsupported,   ///< SSH session or no backend found — native path skipped.
    };
    Status status = Status::NativeFailed;
    std::string backend_name;    ///< e.g. "pbcopy", "xclip", "wl-copy".
    std::size_t bytes_sent = 0;  ///< Bytes written to backend stdin.
    std::chrono::milliseconds duration{0};
    std::string error_detail;  ///< Non-empty on failure (for diagnostics).
};

/// A single clipboard write job.
///
/// Created by ClipboardCoordinator on the owner thread. The worker thread
/// runs independently. The coordinator calls cancel() + join() before
/// destroying the job (serialized cancel/reap/start).
///
/// The on_complete callback is called on the WORKER thread when the job
/// finishes. The caller is responsible for marshaling to the UI thread.
class ClipboardJob {
public:
    ClipboardJob(std::shared_ptr<const JobInput> input, std::shared_ptr<CancellationState> cancel,
                 std::function<void(CopyResult)> on_complete);
    ~ClipboardJob();

    ClipboardJob(const ClipboardJob&) = delete;
    ClipboardJob& operator=(const ClipboardJob&) = delete;
    ClipboardJob(ClipboardJob&&) = delete;
    ClipboardJob& operator=(ClipboardJob&&) = delete;

    /// Signal cancellation (non-blocking). The worker checks this flag
    /// every ~100ms (poll wakeup) and enters the kill sequence.
    void cancel() noexcept;

    /// Wait for the worker thread to finish. Bounded to ~1.1s worst case
    /// (100ms poll wakeup + 500ms TERM deadline + 500ms KILL+reap deadline).
    void join() noexcept;

    /// PID of the leader process if it was not reaped during the kill
    /// sequence. Only valid after join() returns. The coordinator reads
    /// this during reset() and adds the PID to pending_reaps_.
    [[nodiscard]] std::optional<pid_t> unreaped_pid() const noexcept;

private:
    std::shared_ptr<const JobInput> input_;
    std::shared_ptr<CancellationState> cancel_;
    std::function<void(CopyResult)> on_complete_;
    std::thread worker_;
    std::optional<pid_t> unreaped_pid_;
};

}  // namespace loom::utils::clipboard
