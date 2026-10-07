// paste_coordinator.cppm — P4-1b: async clipboard paste state + worker
// extracted from AppAdapter.
//
// Owns the paste-id counter, the pasted-content maps, the async
// pending-result maps, and the in-flight tracking set.  SpawnPasteWorker
// runs the clipboard read on a background thread; the result lands in
// pending_paste_results_ / pending_paste_text_results_ / failures, and
// ProcessCompletedPastes (called on the render thread) drains them.
//
// AppAdapter keeps ProcessCompletedPastes and WaitForInFlightPastes
// (they need screen_state_ for input_text / input_cursor mutation) and
// accesses the coordinator's state through the accessors below.

export module loom.ui.app.paste_coordinator;

import std;

import loom.types.types;  // loom::core::ImageBlock, loom::core::ImageBlockSource

export namespace loom::ui {

/// Owns the async clipboard paste state.  The background worker thread
/// is detached (AppAdapter outlives any paste worker — the app object
/// lives for the whole session).
class PasteCoordinator {
public:
    PasteCoordinator() = default;
    ~PasteCoordinator() = default;

    // Non-copyable, non-movable (owns mutex + detached threads).
    PasteCoordinator(const PasteCoordinator&) = delete;
    PasteCoordinator& operator=(const PasteCoordinator&) = delete;

    /// Allocate a new paste-id (monotonically increasing).
    [[nodiscard]] int allocate_paste_id() noexcept {
        return next_paste_id_++;
    }

    /// Spawn a background thread that reads the clipboard and posts the
    /// result to pending_paste_results_ / pending_paste_text_results_ /
    /// pending_paste_failures_.  Calls post_render on completion so the
    /// render thread drains the result.
    ///
    /// Testing: when no_real_paste_worker is true, injects a fake image
    /// synchronously (no thread).
    void SpawnPasteWorker(int id,
                          std::function<void()> post_render,
                          bool no_real_paste_worker = false);

    // ── State accessors (for AppAdapter::ProcessCompletedPastes) ──────

    [[nodiscard]] std::mutex& paste_mutex() noexcept { return paste_mutex_; }

    /// Swap out all pending results/failures (render thread drains them).
    void drain_pending(
        std::unordered_map<int, loom::core::ImageBlock>& results,
        std::unordered_set<int>& failures,
        std::unordered_map<int, std::string>& text_results) {
        std::lock_guard lk(paste_mutex_);
        results.swap(pending_paste_results_);
        failures.swap(pending_paste_failures_);
        text_results.swap(pending_paste_text_results_);
    }

    // ── Pasted content (render-thread-only, no lock needed) ───────────

    [[nodiscard]] std::unordered_map<int, loom::core::ImageBlock>& pasted_contents() noexcept {
        return pasted_contents_;
    }
    [[nodiscard]] const std::unordered_map<int, loom::core::ImageBlock>& pasted_contents() const noexcept {
        return pasted_contents_;
    }
    [[nodiscard]] std::unordered_map<int, std::string>& pasted_text_contents() noexcept {
        return pasted_text_contents_;
    }
    [[nodiscard]] const std::unordered_map<int, std::string>& pasted_text_contents() const noexcept {
        return pasted_text_contents_;
    }

    // ── In-flight tracking (main-thread-only, no lock needed) ─────────

    void mark_in_flight(int id) { in_flight_pastes_.insert(id); }
    void mark_completed(int id) { in_flight_pastes_.erase(id); }
    [[nodiscard]] bool is_in_flight(int id) const noexcept {
        return in_flight_pastes_.contains(id);
    }
    [[nodiscard]] bool has_in_flight() const noexcept {
        return !in_flight_pastes_.empty();
    }

private:
    // Pasted content (render-thread-only).
    std::unordered_map<int, loom::core::ImageBlock> pasted_contents_;
    std::unordered_map<int, std::string> pasted_text_contents_;
    int next_paste_id_ = 1;

    // Async paste state (background thread → render thread).
    std::mutex paste_mutex_;
    std::unordered_map<int, loom::core::ImageBlock> pending_paste_results_;
    std::unordered_set<int> pending_paste_failures_;
    std::unordered_map<int, std::string> pending_paste_text_results_;

    // In-flight tracking (main-thread-only).
    std::unordered_set<int> in_flight_pastes_;
};

}  // namespace loom::ui
