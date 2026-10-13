// statusline_coordinator.cppm — async statusline execution with UI-owned input
// snapshots and a mutex-protected result handoff.

export module loom.ui.app.statusline_coordinator;

import std;

export namespace loom::ui {

/// Executes immutable statusline requests off the UI thread. The UI submits
/// snapshots and drains results; callbacks never read or write screen state.
class StatuslineCoordinator {
public:
    using ExecuteCommandFn = std::function<bool(
        std::string_view cmd, std::string json, int timeout_ms,
        std::string& output)>;
    using PostRenderFn = std::function<void()>;

    StatuslineCoordinator() = default;
    ~StatuslineCoordinator() { Stop(); }

    StatuslineCoordinator(const StatuslineCoordinator&) = delete;
    StatuslineCoordinator& operator=(const StatuslineCoordinator&) = delete;

    /// Start on the UI thread, then submit the initial request explicitly.
    void Start(ExecuteCommandFn execute_command, PostRenderFn post_render,
               int debounce_ms = 300) {
        execute_command_ = std::move(execute_command);
        post_render_ = std::move(post_render);
        debounce_ms_ = debounce_ms;
        thread_ = std::jthread([this](std::stop_token st) { Run(st); });
    }

    /// Submit an immutable UI snapshot. A newer request invalidates any
    /// unconsumed or in-flight result, including when the command is cleared.
    void TriggerUpdate(std::string command, std::string input_json) {
        {
            std::lock_guard lk(mutex_);
            pending_ = Request{std::move(command), std::move(input_json),
                               ++generation_};
            result_.reset();
        }
        cv_.notify_one();
    }

    /// Drain on the UI thread before applying the text to the screen state.
    std::optional<std::string> TakeResult() {
        std::lock_guard lk(mutex_);
        return std::exchange(result_, std::nullopt);
    }

    /// Stop wakes a debouncing worker and joins an executing worker before
    /// any callback or private state can be destroyed. Idempotent.
    void Stop() {
        if (thread_.joinable()) {
            {
                std::lock_guard lk(mutex_);
                thread_.request_stop();
            }
            cv_.notify_all();
            thread_.join();
        }
    }

    [[nodiscard]] bool is_running() const noexcept {
        return running_.load();
    }

private:
    struct Request {
        std::string command;
        std::string input_json;
        std::uint64_t generation;
    };

    void Run(std::stop_token st) {
        while (!st.stop_requested()) {
            Request request;
            {
                std::unique_lock lk(mutex_);
                cv_.wait(lk, [this, &st] {
                    return pending_.has_value() || st.stop_requested();
                });
                if (st.stop_requested()) break;
                const auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(debounce_ms_);
                cv_.wait_until(lk, deadline, [&st] {
                    return st.stop_requested();
                });
                if (st.stop_requested()) break;
                request = std::move(*pending_);
                pending_.reset();
            }

            std::string output;
            if (!request.command.empty()) {
                const auto now = std::chrono::steady_clock::now();
                const bool memo_hit = request.command == last_cmd_ &&
                    request.input_json == last_input_json_ &&
                    last_run_ != std::chrono::steady_clock::time_point{} &&
                    now - last_run_ < std::chrono::seconds(30);
                if (memo_hit) {
                    output = last_output_;
                } else {
                    running_.store(true);
                    // Keep the request input intact for the memo dependency
                    // tuple; ExecuteCommandFn takes its own string by value.
                    const bool ok = execute_command_(request.command,
                        request.input_json, 5000, output);
                    running_.store(false);
                    if (!ok) output.clear();
                    last_cmd_ = request.command;
                    last_input_json_ = request.input_json;
                    last_output_ = output;
                    last_run_ = now;
                }
            }

            {
                std::lock_guard lk(mutex_);
                if (st.stop_requested()) break;
                if (request.generation != generation_) continue;
                result_ = std::move(output);
            }
            post_render_();
        }
    }

    std::jthread thread_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Request> pending_;
    std::optional<std::string> result_;
    std::uint64_t generation_ = 0;
    int debounce_ms_ = 300;

    // Memo state is worker-owned; both the input tuple and output survive
    // handoff so a repeated request can publish the cached result.
    std::string last_cmd_;
    std::string last_input_json_;
    std::string last_output_;
    std::chrono::steady_clock::time_point last_run_{};
    ExecuteCommandFn execute_command_;
    PostRenderFn post_render_;
};

}  // namespace loom::ui
