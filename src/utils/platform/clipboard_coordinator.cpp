// @file clipboard_coordinator.cpp
// @brief ClipboardCoordinator implementation: background owner thread that
//        serializes cancel/reap/start of clipboard jobs.
//
// POSIX-only (Linux + macOS).
module;

#ifndef _WIN32
#include <cerrno>
#include <sys/wait.h>
#include <unistd.h>
#endif

module loom.platform.clipboard_coordinator;

#ifndef _WIN32

import loom.platform.clipboard;  // resolve_native_backend, is_ssh_session

namespace loom::utils::clipboard {

// ── Impl ────────────────────────────────────────────────────────────────

struct ClipboardCoordinator::Impl {
    // ── Queue (guarded by mutex) ──────────────────────────────────────
    std::mutex mutex;
    std::condition_variable cv;
    std::queue<CopyRequest> queue;
    std::atomic<bool> running{true};
    std::atomic<std::uint64_t> generation{0};

    // ── Owner-thread-only state ───────────────────────────────────────
    // These are ONLY touched by the owner thread. No synchronization needed.
    std::unique_ptr<ClipboardJob> current_job;
    std::vector<pid_t> pending_reaps;

    // ── Owner thread ──────────────────────────────────────────────────
    std::thread owner;

    Impl() {
        owner = std::thread([this] { owner_loop(); });
    }

    ~Impl() {
        shutdown();
    }

    void shutdown() {
        {
            std::lock_guard lock(mutex);
            if (!running.load(std::memory_order_acquire)) return;  // already shut down
            running.store(false, std::memory_order_release);
            // Discard all pending requests.
            std::queue<CopyRequest> empty;
            queue.swap(empty);
        }
        cv.notify_all();
        if (owner.joinable()) {
            owner.join();
        }
    }

    std::uint64_t post(CopyRequest request) {
        auto gen = generation.fetch_add(1, std::memory_order_acq_rel) + 1;
        {
            std::lock_guard lock(mutex);
            if (!running.load(std::memory_order_acquire)) {
                // Shutdown in progress — silently discard.
                return gen;
            }
            queue.push(std::move(request));
        }
        cv.notify_one();
        return gen;
    }

    // ── Owner thread loop ─────────────────────────────────────────────

    void owner_loop() {
        // 1-second cadence for pending_reaps_ sweep.
        constexpr auto kCadence = std::chrono::seconds{1};

        while (true) {
            // Wait for a request or timeout.
            CopyRequest request;
            bool has_request = false;
            {
                std::unique_lock lock(mutex);
                cv.wait_for(lock, kCadence, [this] {
                    return !running.load(std::memory_order_acquire) || !queue.empty();
                });
                if (!queue.empty()) {
                    request = std::move(queue.front());
                    queue.pop();
                    has_request = true;
                }
            }

            // Nonblocking pending_reaps_ sweep (runs on EVERY wake).
            sweep_pending_reaps();

            // Check for shutdown.
            if (!running.load(std::memory_order_acquire)) {
                // Final sweep + cancel/join current job.
                if (current_job) {
                    current_job->cancel();
                    current_job->join();
                    // Collect any unreaped PID from the final job.
                    if (auto pid = current_job->unreaped_pid()) {
                        pending_reaps.push_back(*pid);
                    }
                    current_job.reset();
                }
                // Final pending_reaps_ sweep (single nonblocking pass).
                sweep_pending_reaps();
                // Any remaining entries are abandoned — init reaps them.
                return;
            }

            // Process the request (cancel/reap old, start new).
            if (has_request) {
                // Re-check running_ under the lock before starting a new job.
                // This closes the race where the owner pops a request just
                // before shutdown() clears the queue.
                if (!running.load(std::memory_order_acquire)) {
                    // Shutdown began after we popped. Don't start a new job.
                    continue;
                }
                start_new_job(std::move(request));
            }
        }
    }

    /// Cancel/reap the current job, then start a new one.
    void start_new_job(CopyRequest request) {
        // Cancel and reap the old job (serialized — no parallel workers).
        if (current_job) {
            current_job->cancel();
            current_job->join();
            // Collect any unreaped PID.
            if (auto pid = current_job->unreaped_pid()) {
                pending_reaps.push_back(*pid);
            }
            current_job.reset();
        }

        // Build JobInput.
        auto input = std::make_shared<JobInput>();
        input->text = std::move(request.text);

        // Resolve backend.
        auto backend = loom::utils::clipboard::resolve_native_backend();
        if (!backend) {
            // SSH or no backend — report Unsupported immediately.
            CopyResult result;
            result.status = CopyResult::Status::Unsupported;
            result.duration = std::chrono::milliseconds{0};
            result.error_detail = loom::utils::clipboard::is_ssh_session()
                ? "ssh session" : "no native backend";
            request.on_complete(std::move(result));
            return;
        }
        input->backend_path = backend->path;
        input->argv = backend->argv;

        auto cancel = std::make_shared<CancellationState>();
        current_job = std::make_unique<ClipboardJob>(
            std::move(input), std::move(cancel), std::move(request.on_complete));
    }

    /// Nonblocking sweep of pending_reaps_. Reaped entries are removed.
    void sweep_pending_reaps() {
        auto it = pending_reaps.begin();
        while (it != pending_reaps.end()) {
            int status = 0;
            pid_t r = ::waitpid(*it, &status, WNOHANG);
            if (r == *it || (r == -1 && errno == ECHILD)) {
                // Reaped (or already reaped by someone else).
                it = pending_reaps.erase(it);
            } else {
                ++it;  // still running
            }
        }
    }
};

// ── ClipboardCoordinator public API ─────────────────────────────────────

ClipboardCoordinator::ClipboardCoordinator()
    : impl_(std::make_unique<Impl>()) {}

ClipboardCoordinator::~ClipboardCoordinator() {
    shutdown();
}

std::uint64_t ClipboardCoordinator::post(CopyRequest request) {
    return impl_->post(std::move(request));
}

void ClipboardCoordinator::shutdown() {
    impl_->shutdown();
}

}  // namespace loom::utils::clipboard

#else  // _WIN32

// Windows stub: the async pipeline is POSIX-only. The existing synchronous
// clip.exe path in clipboard.cppm is used instead. The coordinator on
// Windows immediately calls the callback with Unsupported.

namespace loom::utils::clipboard {

struct ClipboardCoordinator::Impl {
    std::uint64_t post(CopyRequest request) {
        if (request.on_complete) {
            CopyResult result;
            result.status = CopyResult::Status::Unsupported;
            request.on_complete(std::move(result));
        }
        return 0;
    }
    void shutdown() {}
};

ClipboardCoordinator::ClipboardCoordinator()
    : impl_(std::make_unique<Impl>()) {}

ClipboardCoordinator::~ClipboardCoordinator() {
    shutdown();
}

std::uint64_t ClipboardCoordinator::post(CopyRequest request) {
    return impl_->post(std::move(request));
}

void ClipboardCoordinator::shutdown() {
    impl_->shutdown();
}

}  // namespace loom::utils::clipboard

#endif  // _WIN32
