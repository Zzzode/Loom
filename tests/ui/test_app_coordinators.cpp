// Regression tests for the UI/worker handoff and worker shutdown boundary.
#include <gtest/gtest.h>

import std;
import loom.ui.app.statusline_coordinator;
import loom.ui.app.permission_coordinator;

namespace {
using namespace std::chrono_literals;
using loom::ui::StatuslineCoordinator;

class Gate {
public:
    void Wait() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return open_; });
    }

    void Open() {
        std::lock_guard lock(mutex_);
        open_ = true;
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool open_ = false;
};

struct OpenOnExit {
    Gate& gate;

    ~OpenOnExit() { gate.Open(); }
};

class Notifications {
public:
    void Post() {
        std::lock_guard lock(mutex_);
        ++count_;
        cv_.notify_all();
    }

    bool WaitFor(int count) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, 2s, [this, count] { return count_ >= count; });
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_ = 0;
};
}  // namespace

TEST(StatuslineCoordinator, ImmutableInputAndResultStayBehindUiHandoff) {
    Gate release;
    Notifications notifications;
    std::promise<void> entered;
    auto started = entered.get_future();
    const auto ui_thread = std::this_thread::get_id();
    std::thread::id execution_thread;
    std::string observed_command;
    std::string observed_input;
    StatuslineCoordinator coordinator;
    OpenOnExit unblock{release};
    coordinator.Start(
        [&](std::string_view command, std::string json, int timeout_ms, std::string& output) {
            execution_thread = std::this_thread::get_id();
            observed_command = command;
            observed_input = json;
            EXPECT_EQ(timeout_ms, 5000);
            entered.set_value();
            release.Wait();
            output = "ready";
            return true;
        },
        [&] { notifications.Post(); }, 0);

    std::string command = "status-command";
    std::string input = R"({"model":"original"})";
    coordinator.TriggerUpdate(command, input);
    command = "changed";
    input = R"({"model":"changed"})";
    ASSERT_EQ(started.wait_for(2s), std::future_status::ready);
    EXPECT_NE(execution_thread, ui_thread);
    EXPECT_EQ(observed_command, "status-command");
    EXPECT_EQ(observed_input, R"({"model":"original"})");
    EXPECT_FALSE(coordinator.TakeResult().has_value());
    release.Open();
    ASSERT_TRUE(notifications.WaitFor(1));
    EXPECT_EQ(coordinator.TakeResult(), std::optional<std::string>{"ready"});
    EXPECT_FALSE(coordinator.TakeResult().has_value());
}

TEST(StatuslineCoordinator, NewRequestDiscardsInFlightResult) {
    Gate release;
    Notifications notifications;
    std::promise<void> entered;
    auto started = entered.get_future();
    std::atomic<int> calls{0};
    StatuslineCoordinator coordinator;
    OpenOnExit unblock{release};
    coordinator.Start(
        [&](std::string_view, std::string json, int, std::string& output) {
            if (++calls == 1) {
                entered.set_value();
                release.Wait();
            }
            output = std::move(json);
            return true;
        },
        [&] { notifications.Post(); }, 0);
    coordinator.TriggerUpdate("cmd", "old");
    ASSERT_EQ(started.wait_for(2s), std::future_status::ready);
    coordinator.TriggerUpdate("cmd", "new");
    release.Open();
    ASSERT_TRUE(notifications.WaitFor(1));
    EXPECT_EQ(coordinator.TakeResult(), std::optional<std::string>{"new"});
    EXPECT_EQ(calls.load(), 2);
}

TEST(StatuslineCoordinator, ClearingCommandInvalidatesInFlightResult) {
    Gate release;
    Notifications notifications;
    std::promise<void> entered;
    auto started = entered.get_future();
    std::atomic<int> calls{0};
    StatuslineCoordinator coordinator;
    OpenOnExit unblock{release};
    coordinator.Start(
        [&](std::string_view, std::string, int, std::string& output) {
            ++calls;
            entered.set_value();
            release.Wait();
            output = "stale";
            return true;
        },
        [&] { notifications.Post(); }, 0);
    coordinator.TriggerUpdate("cmd", "input");
    ASSERT_EQ(started.wait_for(2s), std::future_status::ready);
    coordinator.TriggerUpdate({}, {});
    release.Open();
    ASSERT_TRUE(notifications.WaitFor(1));
    EXPECT_EQ(coordinator.TakeResult(), std::optional<std::string>{""});
    EXPECT_EQ(calls.load(), 1);
}

TEST(StatuslineCoordinator, NewRequestInvalidatesUndrainedResult) {
    Notifications notifications;
    StatuslineCoordinator coordinator;
    coordinator.Start(
        [](std::string_view, std::string json, int, std::string& output) {
            output = std::move(json);
            return true;
        },
        [&] { notifications.Post(); }, 0);
    coordinator.TriggerUpdate("cmd", "old");
    ASSERT_TRUE(notifications.WaitFor(1));
    coordinator.TriggerUpdate({}, {});
    // Either no result is ready yet, or the clear result is already staged.
    const auto result = coordinator.TakeResult();
    EXPECT_TRUE(!result || result->empty());
    ASSERT_TRUE(notifications.WaitFor(2));
}

TEST(StatuslineCoordinator, MemoUsesIntactInputAndCachesOutput) {
    Notifications notifications;
    std::atomic<int> calls{0};
    StatuslineCoordinator coordinator;
    coordinator.Start(
        [&](std::string_view command, std::string json, int, std::string& output) {
            ++calls;
            output = std::string(command) + ":" + json;
            json.clear();
            return true;
        },
        [&] { notifications.Post(); }, 0);
    coordinator.TriggerUpdate("cmd", "input");
    ASSERT_TRUE(notifications.WaitFor(1));
    EXPECT_EQ(coordinator.TakeResult(), std::optional<std::string>{"cmd:input"});
    coordinator.TriggerUpdate("cmd", "input");
    ASSERT_TRUE(notifications.WaitFor(2));
    EXPECT_EQ(coordinator.TakeResult(), std::optional<std::string>{"cmd:input"});
    EXPECT_EQ(calls.load(), 1);
    coordinator.TriggerUpdate("different", "input");
    ASSERT_TRUE(notifications.WaitFor(3));
    EXPECT_EQ(calls.load(), 2);
    coordinator.TriggerUpdate("different", "changed");
    ASSERT_TRUE(notifications.WaitFor(4));
    EXPECT_EQ(calls.load(), 3);
}

TEST(StatuslineCoordinator, FailureStagesEmptyResult) {
    Notifications notifications;
    StatuslineCoordinator coordinator;
    coordinator.Start(
        [](std::string_view, std::string, int, std::string& output) {
            output = "partial failure";
            return false;
        },
        [&] { notifications.Post(); }, 0);
    coordinator.TriggerUpdate("cmd", "input");
    ASSERT_TRUE(notifications.WaitFor(1));
    EXPECT_EQ(coordinator.TakeResult(), std::optional<std::string>{""});
}

TEST(StatuslineCoordinator, StopJoinsExecutingWorkerAndSuppressesResult) {
    Gate release;
    std::promise<void> entered;
    auto started = entered.get_future();
    std::atomic<bool> completed{false};
    std::atomic<int> renders{0};
    StatuslineCoordinator coordinator;
    OpenOnExit unblock{release};
    coordinator.Start(
        [&](std::string_view, std::string, int, std::string& output) {
            entered.set_value();
            release.Wait();
            output = "finished";
            completed.store(true);
            return true;
        },
        [&] { ++renders; }, 0);
    coordinator.TriggerUpdate("cmd", "input");
    ASSERT_EQ(started.wait_for(2s), std::future_status::ready);
    std::promise<void> stopping;
    auto stopping_started = stopping.get_future();
    auto stopped = std::async(std::launch::async, [&] {
        stopping.set_value();
        coordinator.Stop();
    });
    // Ensure an assertion failure can unblock the worker before waiting for
    // the asynchronous Stop future's destructor.
    OpenOnExit unblock_stop{release};
    ASSERT_EQ(stopping_started.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(stopped.wait_for(20ms), std::future_status::timeout);
    EXPECT_FALSE(completed.load());
    release.Open();
    ASSERT_EQ(stopped.wait_for(2s), std::future_status::ready);
    stopped.get();
    EXPECT_TRUE(completed.load());
    EXPECT_EQ(renders.load(), 0);
    EXPECT_FALSE(coordinator.TakeResult().has_value());
    coordinator.Stop();
}

TEST(PermissionCoordinator, ShutdownStaysCancelledAfterLateResetAndResponse) {
    loom::ui::PermissionCoordinator permission;
    permission.add_always_allowed("Bash");
    permission.abort_all();
    const auto tool_token = permission.begin_permission_request();
    EXPECT_EQ(tool_token, 0);
    EXPECT_FALSE(permission.resolve_permission(tool_token, true));
    auto tool = std::async(std::launch::async,
                           [&] { return permission.wait_permission_response(tool_token); });
    EXPECT_EQ(tool.wait_for(2s), std::future_status::ready);
    EXPECT_FALSE(tool.get());
    EXPECT_FALSE(permission.is_always_allowed("Bash"));

    const auto elicitation_token = permission.begin_elicitation_request();
    EXPECT_EQ(elicitation_token, 0);
    EXPECT_FALSE(permission.resolve_elicitation(elicitation_token, true));
    auto elicitation = std::async(std::launch::async, [&] {
        return permission.wait_elicitation_response(elicitation_token);
    });
    EXPECT_EQ(elicitation.wait_for(2s), std::future_status::ready);
    EXPECT_FALSE(elicitation.get());

    permission.reset_ask_user_response();
    permission.resolve_ask_user(std::string{"late"});
    auto ask = std::async(std::launch::async, [&] { return permission.wait_ask_user_response(); });
    EXPECT_EQ(ask.wait_for(2s), std::future_status::ready);
    EXPECT_FALSE(ask.get().has_value());
}
