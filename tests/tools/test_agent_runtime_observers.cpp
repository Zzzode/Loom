/// Native agent store change notifications invoke clients outside store locks.
#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"

#include <gtest/gtest.h>
#include <cstdlib>

import std;
import loom.tools.agent_runtime;

namespace {
namespace runtime = loom::tools::agent_runtime;

struct ObserverSandbox {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("loom_agent_observers_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::optional<std::string> previous;
    runtime::NativeAgentStore store;
    ObserverSandbox() {
        if (const auto* value = std::getenv("LOOM_AGENT_RUNTIME_DIR")) previous = value;
        std::filesystem::create_directories(path);
        ::setenv("LOOM_AGENT_RUNTIME_DIR", path.c_str(), 1);
    }
    ~ObserverSandbox() {
        store.wait_for_background_agents();
        if (previous) ::setenv("LOOM_AGENT_RUNTIME_DIR", previous->c_str(), 1);
        else ::unsetenv("LOOM_AGENT_RUNTIME_DIR");
        std::filesystem::remove_all(path);
    }
};
} // namespace

TEST(NativeAgentObservers, PublishesPersistedLifecycleAndProgressAfterUnlocking) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    std::vector<runtime::NativeAgentRecord> snapshots;
    const auto token = store.subscribe_changed([&] {
        auto records = store.list();
        ASSERT_EQ(records.size(), 1u);
        const auto persisted = runtime::load_native_agent_record(records.front().agent_id);
        ASSERT_TRUE(persisted);
        EXPECT_EQ(persisted->status, records.front().status);
        EXPECT_EQ(persisted->progress, records.front().progress);
        snapshots.push_back(std::move(records.front()));
    });
    store.begin_execution(runtime::NativeAgentRecord{.agent_id = "lifecycle"});
    ASSERT_EQ(snapshots.back().status, runtime::NativeAgentStatus::Queued);
    store.mark_running("lifecycle");
    EXPECT_EQ(snapshots.back().status, runtime::NativeAgentStatus::Running);
    store.update_progress("lifecycle", 0.5);
    EXPECT_EQ(snapshots.back().progress, 0.5);
    store.finish_agent("lifecycle", std::string("verified result"));
    EXPECT_EQ(snapshots.back().status, runtime::NativeAgentStatus::Completed);
    store.end_execution("lifecycle");
    store.enqueue_resume_message("lifecycle", "continue");
    EXPECT_EQ(snapshots.back().status, runtime::NativeAgentStatus::Queued);
    store.mark_failed("lifecycle", "failed result");
    EXPECT_EQ(snapshots.back().status, runtime::NativeAgentStatus::Failed);
    store.mark_cancelled("lifecycle", "cancelled result");
    EXPECT_EQ(snapshots.back().status, runtime::NativeAgentStatus::Cancelled);
    store.unsubscribe_changed(token);
}

TEST(NativeAgentObservers, CanReenterListAndUnsubscribeItself) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    int calls = 0;
    std::size_t token = 0;
    token = store.subscribe_changed([&] {
        ++calls;
        EXPECT_EQ(store.list().size(), 1u);
        store.unsubscribe_changed(token);
    });
    store.upsert(runtime::NativeAgentRecord{.agent_id = "self-unsubscribe"});
    store.mark_running("self-unsubscribe");
    EXPECT_EQ(calls, 1);
    store.unsubscribe_changed(token);
    store.unsubscribe_changed(0);
}

TEST(NativeAgentObservers, ObserverExceptionsCannotFailMutationsOrOtherObservers) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    int calls = 0;
    const auto throwing = store.subscribe_changed([] { throw std::runtime_error("observer failure"); });
    const auto healthy = store.subscribe_changed([&] { ++calls; });
    EXPECT_NO_THROW(store.upsert(runtime::NativeAgentRecord{.agent_id = "exception"}));
    EXPECT_NO_THROW(store.mark_completed("exception", "stored result"));
    EXPECT_EQ(calls, 2);
    EXPECT_EQ(store.get("exception")->output, "stored result");
    store.unsubscribe_changed(throwing);
    store.unsubscribe_changed(healthy);
}

TEST(NativeAgentObservers, UnsubscribeDoesNotWaitForAnAdmittedBlockedCallback) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    std::promise<void> entered;
    auto entered_future = entered.get_future();
    auto release = std::make_shared<std::promise<void>>();
    auto released = release->get_future().share();
    std::atomic<int> calls{0};
    const auto token = store.subscribe_changed([&] {
        if (calls.fetch_add(1) == 0) entered.set_value();
        released.wait();
        EXPECT_EQ(store.list().size(), 1u);
    });
    auto mutation = std::async(std::launch::async, [&] {
        store.upsert(runtime::NativeAgentRecord{.agent_id = "blocked"});
    });
    struct ReleaseGuard {
        std::shared_ptr<std::promise<void>> promise;
        ~ReleaseGuard() { if (promise) promise->set_value(); }
    } guard{release};
    ASSERT_EQ(entered_future.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    auto unsubscribe = std::async(std::launch::async, [&] { store.unsubscribe_changed(token); });
    const auto status = unsubscribe.wait_for(std::chrono::seconds(3));
    EXPECT_EQ(status, std::future_status::ready);
    // Release even when the non-blocking guarantee failed, before future teardown.
    release->set_value();
    guard.promise.reset();
    unsubscribe.get();
    mutation.get();
    store.mark_running("blocked");
    EXPECT_EQ(calls.load(), 1);
}

TEST(NativeAgentObservers, BackgroundWorkerTerminalMutationWakesAnIdleSubscriber) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    store.upsert(runtime::NativeAgentRecord{.agent_id = "background", .background = true});
    std::promise<void> completed;
    auto completed_future = completed.get_future();
    std::atomic<bool> delivered{false};
    const auto token = store.subscribe_changed([&] {
        const auto records = store.list();
        if (!records.empty() && records.front().status == runtime::NativeAgentStatus::Completed &&
            !delivered.exchange(true)) completed.set_value();
    });
    store.run_background_agent("background", [&] {
        store.finish_agent("background", std::string("background result"));
    });
    EXPECT_EQ(completed_future.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    store.wait_for_background_agents();
    store.unsubscribe_changed(token);
    EXPECT_EQ(store.get("background")->status, runtime::NativeAgentStatus::Completed);
}

TEST(NativeAgentObservers, RejectedExecutionPublishesPersistedCancellationIntent) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    std::stop_source owner;
    owner.request_stop();
    int calls = 0;
    const auto token = store.subscribe_changed([&] {
        ++calls;
        const auto records = store.list();
        ASSERT_EQ(records.size(), 1u);
        EXPECT_TRUE(records.front().cancel_requested);
    });
    EXPECT_THROW(store.begin_execution(runtime::NativeAgentRecord{.agent_id = "rejected"}, owner.get_token()), std::runtime_error);
    EXPECT_EQ(calls, 1);
    store.unsubscribe_changed(token);
}

TEST(NativeAgentObservers, PersistenceRefreshKeepsTheStoresOriginalCreationTime) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    const auto original = std::chrono::system_clock::time_point{std::chrono::seconds{123}};
    store.upsert(runtime::NativeAgentRecord{
        .agent_id = "stable-order", .background = true, .created_at = original,
        .parent_session_id = std::string("notification-session"),
    });
    EXPECT_EQ(store.list().front().created_at, original);
    store.mark_completed("stable-order", "done");
    (void)store.take_pending_task_events("notification-session");
    EXPECT_EQ(store.get("stable-order")->created_at, original);
    EXPECT_EQ(store.list().front().created_at, original);
}

TEST(NativeAgentObservers, DestructionJoinsAnOwnedWorkerWithAnAdmittedUnsubscribedCallback) {
    ObserverSandbox sandbox;
    auto store = std::make_unique<runtime::NativeAgentStore>();
    auto* const raw_store = store.get();
    raw_store->upsert(runtime::NativeAgentRecord{.agent_id = "owned-callback", .background = true});
    std::promise<void> start_worker;
    auto worker_started = start_worker.get_future().share();
    std::promise<void> entered;
    auto entered_future = entered.get_future();
    std::promise<bool> destruction_observed;
    auto destruction_observed_future = destruction_observed.get_future();
    auto release = std::make_shared<std::promise<void>>();
    auto released = release->get_future().share();
    std::atomic<bool> admitted{false};
    std::atomic<bool> callback_finished{false};
    std::atomic<bool> worker_finished{false};
    // Declare the future before the guard so failed assertions release the
    // callback before the future destructor waits for store destruction.
    std::future<void> destruction;
    struct CallbackReleaseGuard {
        std::shared_ptr<std::promise<void>> promise;
        ~CallbackReleaseGuard() { if (promise) promise->set_value(); }
    } guard{release};
    const auto token = raw_store->subscribe_changed([&] {
        const auto record = raw_store->get("owned-callback");
        if (!record || record->progress != 0.5 || admitted.exchange(true)) return;
        entered.set_value();
        // Cancellation is set only by the destructor in this fixture. This
        // handshake proves destruction entered before checking its join below.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!raw_store->is_cancel_requested("owned-callback") &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
        destruction_observed.set_value(raw_store->is_cancel_requested("owned-callback"));
        released.wait();
        EXPECT_EQ(raw_store->list().size(), 1u);
        callback_finished.store(true);
    });
    raw_store->run_background_agent("owned-callback", [&] {
        worker_started.wait();
        raw_store->update_progress("owned-callback", 0.5);
        worker_finished.store(true);
    });
    start_worker.set_value();
    EXPECT_EQ(entered_future.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    raw_store->unsubscribe_changed(token);
    destruction = std::async(std::launch::async, [owned = std::move(store)]() mutable {
        owned.reset();
    });
    ASSERT_EQ(destruction_observed_future.wait_for(std::chrono::seconds(6)), std::future_status::ready);
    ASSERT_TRUE(destruction_observed_future.get());
    // No scheduling-time assumption: the destructor has entered, and its owned
    // worker cannot exit until this already-admitted callback is released.
    EXPECT_EQ(destruction.wait_for(std::chrono::seconds(0)), std::future_status::timeout);
    release->set_value();
    guard.promise.reset();
    destruction.get();
    EXPECT_TRUE(callback_finished.load());
    EXPECT_TRUE(worker_finished.load());
}
