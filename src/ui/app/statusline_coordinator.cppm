// statusline_coordinator.cppm — P3-1c: async statusline worker extracted
// from AppAdapter.
//
// Owns the jthread + debounce/memo state for the user-configurable
// statusline command.  Context-dependent operations (building the input
// JSON, executing the command) are injected as callbacks at Start() time,
// so the coordinator needs no knowledge of AppAdapter internals.
//
// Debounced doUpdate() pattern:
//   1. TriggerUpdate() sets dirty_ and notifies the cv.
//   2. The worker wakes, debounces (sleeps debounce_ms), re-checks dirty_.
//   3. Builds the input JSON, checks the 30s memo cache.
//   4. Executes the command, updates the status line text, posts a render.

export module loom.ui.app.statusline_coordinator;

import std;

export namespace loom::ui {

/// Async statusline worker: owns the jthread + debounce/memo state.
/// AppAdapter constructs this as a member and injects callbacks that
/// supply the command, build the JSON payload, execute the shell command,
/// and write the result back to the screen state.
class StatuslineCoordinator {
public:
    using GetCommandFn      = std::function<std::string()>;
    using BuildInputJsonFn  = std::function<std::string()>;
    using ExecuteCommandFn  = std::function<bool(
        std::string_view cmd, std::string json, int timeout_ms,
        std::string& output)>;
    using SetStatusTextFn   = std::function<void(std::string)>;
    using PostRenderFn      = std::function<void()>;

    StatuslineCoordinator() = default;
    ~StatuslineCoordinator() { Stop(); }

    // Non-copyable, non-movable (owns a jthread).
    StatuslineCoordinator(const StatuslineCoordinator&) = delete;
    StatuslineCoordinator& operator=(const StatuslineCoordinator&) = delete;

    /// Start the worker thread.  Call once from AppAdapter::construct().
    void Start(GetCommandFn get_command,
               BuildInputJsonFn build_input_json,
               ExecuteCommandFn execute_command,
               SetStatusTextFn set_status_text,
               PostRenderFn post_render,
               int debounce_ms = 300) {
        get_command_       = std::move(get_command);
        build_input_json_  = std::move(build_input_json);
        execute_command_   = std::move(execute_command);
        set_status_text_   = std::move(set_status_text);
        post_render_       = std::move(post_render);
        debounce_ms_       = debounce_ms;

        thread_ = std::jthread([this](std::stop_token st) {
            while (!st.stop_requested()) {
                std::unique_lock lk(mutex_);
                cv_.wait(lk, [this, &st] {
                    return dirty_.load() || st.stop_requested();
                });
                if (st.stop_requested()) break;

                lk.unlock();
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(debounce_ms_));
                if (st.stop_requested()) break;
                if (!dirty_.exchange(false)) continue;

                std::string cmd = get_command_();
                if (cmd.empty()) continue;

                running_.store(true);

                std::string new_json = build_input_json_();
                const auto now = std::chrono::steady_clock::now();

                // Memo cache: skip re-exec when command + JSON are identical
                // and last run was < 30s ago (memo dependency tuple).
                const bool memo_hit =
                    (cmd == last_cmd_) &&
                    (new_json == last_input_json_) &&
                    (last_run_ != std::chrono::steady_clock::time_point{} &&
                     (now - last_run_) < std::chrono::seconds(30));

                if (memo_hit) {
                    running_.store(false);
                    continue;
                }

                std::string sl_output;
                const bool sl_ok =
                    execute_command_(cmd, std::move(new_json), 5000, sl_output);

                running_.store(false);

                last_cmd_         = std::move(cmd);
                last_input_json_  = std::move(new_json);
                last_run_         = now;

                if (sl_ok && !sl_output.empty()) {
                    set_status_text_(std::move(sl_output));
                } else {
                    set_status_text_({});
                }

                if (!st.stop_requested()) {
                    post_render_();
                }
            }
        });

        // Trigger an initial update on mount.
        TriggerUpdate();
    }

    /// Trigger an async statusline update (debounced).
    /// Safe to call from any thread.
    void TriggerUpdate() {
        dirty_.store(true);
        cv_.notify_one();
    }

    /// Request stop and join the worker thread.  Idempotent.
    void Stop() {
        if (thread_.joinable()) {
            thread_.request_stop();
            {
                std::lock_guard lk(mutex_);
                dirty_.store(true);  // wake the worker so it sees stop
            }
            cv_.notify_all();
            thread_.join();
        }
    }

    /// True when the worker is currently executing a statusline command.
    [[nodiscard]] bool is_running() const noexcept {
        return running_.load();
    }

private:
    std::jthread thread_;
    std::atomic<bool> dirty_{false};
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    int debounce_ms_ = 300;

    // Memo cache (30s)
    std::string last_cmd_;
    std::string last_input_json_;
    std::chrono::steady_clock::time_point last_run_{};

    // Callbacks
    GetCommandFn     get_command_;
    BuildInputJsonFn build_input_json_;
    ExecuteCommandFn execute_command_;
    SetStatusTextFn  set_status_text_;
    PostRenderFn     post_render_;
};

}  // namespace loom::ui
