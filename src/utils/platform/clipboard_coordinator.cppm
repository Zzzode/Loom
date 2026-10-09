// @file clipboard_coordinator.cppm
// @brief ClipboardCoordinator: serializes cancel/reap/start of clipboard
//        jobs on a dedicated background owner thread.
//
// The UI thread posts copy requests via post() (non-blocking). The owner
// thread pops requests, cancels/reaps any current job, and starts a new one.
// This prevents parallel workers from overwriting each other's clipboard
// content and keeps all blocking joins off the UI thread.
//
// POSIX-only (Linux + macOS). Windows keeps the existing synchronous path.
module;

#include <sys/types.h>  // pid_t

export module loom.platform.clipboard_coordinator;

import std;

// Re-export the job module so CopyResult/CopyRequest types are visible
// to importers of the coordinator.
export import loom.platform.clipboard_job;

export namespace loom::utils::clipboard {

/// A request to copy text to the native clipboard.
struct CopyRequest {
    std::shared_ptr<const std::string> text;
    /// Called on the WORKER thread when the job completes.
    /// The caller is responsible for marshaling to the UI thread.
    std::function<void(CopyResult)> on_complete;
};

/// Coordinates async clipboard writes on a background owner thread.
///
/// Thread safety:
///   - post() and shutdown() may be called from any thread.
///   - The owner thread is the only thread that touches current_job_ and
///     pending_reaps_.
///   - The generation counter is atomic.
///
/// Lifetime:
///   - shutdown() must be called before destruction (typically in the
///     AppAdapter destructor). It stops accepting requests, cancels any
///     active job, and joins the owner thread. Bounded to ~1.1s worst case.
class ClipboardCoordinator {
public:
    ClipboardCoordinator();
    ~ClipboardCoordinator();

    ClipboardCoordinator(const ClipboardCoordinator&) = delete;
    ClipboardCoordinator& operator=(const ClipboardCoordinator&) = delete;
    ClipboardCoordinator(ClipboardCoordinator&&) = delete;
    ClipboardCoordinator& operator=(ClipboardCoordinator&&) = delete;

    /// Post a copy request (non-blocking). Returns the generation counter
    /// for this request. The UI thread should re-check the generation in
    /// the callback to discard stale results.
    [[nodiscard]] std::uint64_t post(CopyRequest request);

    /// Shutdown: stop accepting requests, discard pending queue, cancel any
    /// active job, join the owner thread. Bounded to ~1.1s worst case.
    /// This is the only place a blocking wait occurs on the calling thread,
    /// and it is explicitly a shutdown-path exception.
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace loom::utils::clipboard
