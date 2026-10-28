/// Native agent store change notifications invoke clients outside store locks.
#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"

#include <gtest/gtest.h>

#include <cstdlib>

import std;
import loom.tools.agent_runtime;
import loom.tools.team;
import loom.types.tool_types;

namespace {
namespace runtime = loom::tools::agent_runtime;

struct ObserverSandbox {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("loom_agent_observers_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::optional<std::string> previous;
    runtime::NativeAgentStore store;

    ObserverSandbox() {
        if (const auto* value = std::getenv("LOOM_AGENT_RUNTIME_DIR")) previous = value;
        std::filesystem::create_directories(path);
        ::setenv("LOOM_AGENT_RUNTIME_DIR", path.c_str(), 1);
    }

    ~ObserverSandbox() {
        store.wait_for_background_agents();
        if (previous)
            ::setenv("LOOM_AGENT_RUNTIME_DIR", previous->c_str(), 1);
        else
            ::unsetenv("LOOM_AGENT_RUNTIME_DIR");
        std::filesystem::remove_all(path);
    }
};
}  // namespace

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
    const auto throwing =
        store.subscribe_changed([] { throw std::runtime_error("observer failure"); });
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

        ~ReleaseGuard() {
            if (promise) promise->set_value();
        }
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
            !delivered.exchange(true))
            completed.set_value();
    });
    store.run_background_agent(
        "background", [&] { store.finish_agent("background", std::string("background result")); });
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
    EXPECT_THROW(store.begin_execution(runtime::NativeAgentRecord{.agent_id = "rejected"},
                                       owner.get_token()),
                 std::runtime_error);
    EXPECT_EQ(calls, 1);
    store.unsubscribe_changed(token);
}

TEST(NativeAgentObservers, PersistenceRefreshKeepsTheStoresOriginalCreationTime) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    const auto original = std::chrono::system_clock::time_point{std::chrono::seconds{123}};
    store.upsert(runtime::NativeAgentRecord{
        .agent_id = "stable-order",
        .background = true,
        .created_at = original,
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

        ~CallbackReleaseGuard() {
            if (promise) promise->set_value();
        }
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
    destruction =
        std::async(std::launch::async, [owned = std::move(store)]() mutable { owned.reset(); });
    ASSERT_EQ(destruction_observed_future.wait_for(std::chrono::seconds(6)),
              std::future_status::ready);
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

TEST(NativeAgentAdmission, ReservationRejectsConcurrentResumeWithoutRecordMutation) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    runtime::NativeAgentRecord record{.agent_id = "reserved",
                                      .status = runtime::NativeAgentStatus::Failed};
    record.error = "retained error";
    record.transcript = {"retained transcript"};
    record.notification_delivered = true;
    store.upsert(record);
    std::barrier start(3);
    std::atomic<int> accepted{0};
    std::atomic<int> attempted{0};
    auto reserve = [&] {
        start.arrive_and_wait();
        const auto result = store.reserve_resume("reserved");
        if (result) ++accepted;
        ++attempted;
        while (attempted.load() != 2)
            std::this_thread::yield();
        if (result) store.release_resume("reserved");
    };
    std::jthread first(reserve);
    std::jthread second(reserve);
    start.arrive_and_wait();
    first.join();
    second.join();
    EXPECT_EQ(accepted.load(), 1);
    const auto retained = store.get("reserved");
    ASSERT_TRUE(retained);
    EXPECT_EQ(retained->error, record.error);
    EXPECT_EQ(retained->transcript, record.transcript);
    EXPECT_TRUE(retained->notification_delivered);
    ASSERT_TRUE(store.reserve_resume("reserved"));
    store.release_resume("reserved");
}

TEST(NativeAgentAdmission, TerminalPublicationStillBlocksResumeUntilAllExecutionCleanupEnds) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    std::stop_source owner;
    store.begin_execution(runtime::NativeAgentRecord{.agent_id = "cleanup"}, owner.get_token());
    store.begin_execution("cleanup", owner.get_token());
    store.mark_completed("cleanup", "completed before cleanup");
    EXPECT_FALSE(store.reserve_resume("cleanup"));
    store.end_execution("cleanup");
    EXPECT_FALSE(store.reserve_resume("cleanup"));
    store.end_execution("cleanup");
    ASSERT_TRUE(store.reserve_resume("cleanup"));
    store.release_resume("cleanup");
}

TEST(NativeAgentOwnership, HostStopRequiresActualSessionAndRegistryOwner) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    std::stop_source owner;
    std::stop_source foreign;
    runtime::NativeAgentRecord record{.agent_id = "owned",
                                      .status = runtime::NativeAgentStatus::Running};
    record.parent_session_id = "session";
    store.upsert(record);
    EXPECT_FALSE(store.request_owned_cancel("owned", "session", owner.get_token()));
    EXPECT_FALSE(store.get("owned")->cancel_requested);
    store.begin_execution("owned", owner.get_token());
    EXPECT_FALSE(store.request_owned_cancel("owned", "other", owner.get_token()));
    EXPECT_FALSE(store.request_owned_cancel("owned", "session", foreign.get_token()));
    EXPECT_FALSE(store.request_owned_cancel("owned", "", owner.get_token()));
    EXPECT_FALSE(store.get("owned")->cancel_requested);
    ASSERT_TRUE(store.request_owned_cancel("owned", "session", owner.get_token()));
    ASSERT_TRUE(store.request_owned_cancel("owned", "session", owner.get_token()));
    EXPECT_TRUE(store.get("owned")->cancel_requested);
    EXPECT_EQ(store.get("owned")->status, runtime::NativeAgentStatus::Running);
    store.finish_agent("owned", std::string("late success"));
    EXPECT_EQ(store.get("owned")->status, runtime::NativeAgentStatus::Cancelled);
    EXPECT_FALSE(store.request_owned_cancel("owned", "session", owner.get_token()));
    store.end_execution("owned");
}

TEST(NativeAgentAdmission, MailboxDuringTerminalCleanupDoesNotResetResult) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    store.begin_execution(runtime::NativeAgentRecord{.agent_id = "mailbox-cleanup"});
    store.mark_completed("mailbox-cleanup", "retained result");
    store.enqueue_resume_message("mailbox-cleanup", "follow-up");
    auto record = store.get("mailbox-cleanup");
    ASSERT_TRUE(record);
    EXPECT_EQ(record->status, runtime::NativeAgentStatus::Completed);
    EXPECT_EQ(record->output, "retained result");
    EXPECT_EQ(record->pending_messages, std::vector<std::string>{"follow-up"});
    store.end_execution("mailbox-cleanup");
    ASSERT_TRUE(store.reserve_resume("mailbox-cleanup"));
    store.enqueue_resume_message("mailbox-cleanup", "second follow-up");
    EXPECT_EQ(store.get("mailbox-cleanup")->output, "retained result");
    store.release_resume("mailbox-cleanup");
}

TEST(NativeAgentAdmission, HostConstraintRechecksOwnerAndIdentityUnderReservationMutex) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    runtime::NativeAgentRecord record{.agent_id = "host-target",
                                      .agent_type = "general-purpose",
                                      .status = runtime::NativeAgentStatus::Failed};
    record.parent_session_id = "owner";
    record.transcript = {"retained context"};
    store.upsert(record);
    const loom::core::HostAgentResumeConstraint expected{"host-target", "general-purpose", "owner"};
    record.parent_session_id = "foreign";
    store.upsert(record);
    EXPECT_FALSE(store.reserve_resume("host-target", expected));
    record.parent_session_id = "owner";
    record.agent_type = "other-type";
    store.upsert(record);
    EXPECT_FALSE(store.reserve_resume("host-target", expected));
    record.agent_type = "general-purpose";
    record.teammate_pane_id = "";
    store.upsert(record);
    EXPECT_FALSE(store.reserve_resume("host-target", expected));
    record.teammate_pane_id.reset();
    store.upsert(record);
    ASSERT_TRUE(store.reserve_resume("host-target", expected));
    store.release_resume("host-target");
}

TEST(NativeAgentInput, StartupAndFinalizationHaveAtomicAdmissionBoundaries) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    std::stop_source owner;
    auto record = runtime::make_native_agent_record("interactive", "general-purpose");
    record.parent_session_id = "owner";
    store.begin_execution(record, owner.get_token());
    EXPECT_FALSE(
        store.enqueue_owned_message("interactive", "owner", owner.get_token(), "too early"));
    store.mark_running("interactive");
    ASSERT_TRUE(store.open_input("interactive"));
    ASSERT_TRUE(store.enqueue_owned_message("interactive", "owner", owner.get_token(), "accepted"));
    EXPECT_FALSE(store.close_input_if_idle("interactive"));
    EXPECT_EQ(store.take_pending_messages("interactive"), (std::vector<std::string>{"accepted"}));
    EXPECT_TRUE(store.close_input_if_idle("interactive"));
    EXPECT_FALSE(
        store.enqueue_owned_message("interactive", "owner", owner.get_token(), "too late"));
    EXPECT_TRUE(store.get("interactive")->pending_messages.empty());
    store.finish_agent("interactive", std::string("finished"));
    store.end_execution("interactive");
}

TEST(NativeAgentInput, SimultaneousSendAndEndTurnNeverLoseAnAcceptedMessage) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    std::stop_source owner;
    auto record = runtime::make_native_agent_record("boundary", "general-purpose");
    record.parent_session_id = "owner";
    store.begin_execution(record, owner.get_token());
    store.mark_running("boundary");
    ASSERT_TRUE(store.open_input("boundary"));
    std::barrier boundary(2);
    bool accepted = false;
    bool closed = false;
    std::jthread sender([&] {
        boundary.arrive_and_wait();
        accepted = store.enqueue_owned_message("boundary", "owner", owner.get_token(), "late input")
                       .has_value();
    });
    boundary.arrive_and_wait();
    closed = store.close_input_if_idle("boundary");
    sender.join();
    const auto pending = store.get("boundary")->pending_messages;
    EXPECT_EQ(accepted, !closed);
    EXPECT_EQ(pending.size(), accepted ? 1u : 0u);
    if (accepted) EXPECT_EQ(pending.front(), "late input");
    store.close_input("boundary");
    store.end_execution("boundary");
}

TEST(NativeAgentInput, RegistrySessionAndCancellationScopeAreRecheckedBeforeEnqueue) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    std::stop_source owner, sibling;
    auto record = runtime::make_native_agent_record("target", "general-purpose");
    record.parent_session_id = "owner";
    store.begin_execution(record, owner.get_token());
    store.mark_running("target");
    ASSERT_TRUE(store.open_input("target"));
    EXPECT_FALSE(
        store.enqueue_owned_message("target", "other-session", owner.get_token(), "foreign"));
    EXPECT_FALSE(
        store.enqueue_owned_message("target", "owner", sibling.get_token(), "foreign registry"));
    ASSERT_TRUE(store.enqueue_owned_message("target", "owner", owner.get_token(), "kept"));
    ASSERT_TRUE(store.request_owned_cancel("target", "owner", owner.get_token()));
    EXPECT_FALSE(store.enqueue_owned_message("target", "owner", owner.get_token(), "cancelled"));
    EXPECT_EQ(store.get("target")->pending_messages, (std::vector<std::string>{"kept"}));
    store.finish_agent("target", std::unexpected(std::string("cancelled")));
    store.end_execution("target");
}

TEST(NativeAgentLiveText, BoundedUtf8TailIsTransientAndSurvivesDiskRefresh) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    store.upsert(runtime::make_native_agent_record("live", "general-purpose"));
    std::string text;
    for (int i = 0; i < 30000; ++i)
        text += "界";
    store.append_live_text("live", text);
    const auto current = store.get("live")->live_text;
    EXPECT_LE(current.size(), 64u * 1024u);
    EXPECT_TRUE(current.starts_with("[Earlier live output truncated]\n"));
    const auto tail = current.substr(current.find('\n') + 1);
    EXPECT_EQ(tail.size() % 3, 0u);
    EXPECT_TRUE(tail.ends_with("界"));
    ASSERT_EQ(store.list().size(), 1u);
    EXPECT_EQ(store.get("live")->live_text, current);
    EXPECT_TRUE(runtime::load_native_agent_record("live")->live_text.empty());
    (void)store.take_pending_task_events("owner");
    EXPECT_EQ(store.get("live")->live_text, current);
    store.append_sidechain_message("live", "assistant", R"([{"type":"text","text":"committed"}])",
                                   "committed");
    EXPECT_TRUE(store.get("live")->live_text.empty());
}

TEST(NativeAgentInput, ResumeStartupMergesTheLatestRetainedMailboxUnderLock) {
    ObserverSandbox sandbox;
    auto& store = sandbox.store;
    auto record = runtime::make_native_agent_record("resume-mailbox", "general-purpose");
    record.status = runtime::NativeAgentStatus::Failed;
    record.pending_messages = {"A"};
    store.upsert(record);
    ASSERT_TRUE(store.reserve_resume("resume-mailbox"));
    std::barrier boundary(2);
    std::jthread startup([&] {
        const auto stale = *store.get("resume-mailbox");
        boundary.arrive_and_wait();
        boundary.arrive_and_wait();
        store.upsert(stale);
    });
    boundary.arrive_and_wait();
    store.enqueue_pending_message("resume-mailbox", "B");
    boundary.arrive_and_wait();
    startup.join();
    EXPECT_EQ(store.get("resume-mailbox")->pending_messages, (std::vector<std::string>{"A", "B"}));
    store.release_resume("resume-mailbox");
}

namespace {
struct TeamObserverSandbox {
    ObserverSandbox runtime;
    std::optional<std::string> previous;
    loom::tools::TeamStore store;

    TeamObserverSandbox() {
        if (const char* value = std::getenv("LOOM_TEAM_RUNTIME_DIR")) previous = value;
        ::setenv("LOOM_TEAM_RUNTIME_DIR", (runtime.path / "teams").c_str(), 1);
    }

    ~TeamObserverSandbox() {
        if (previous)
            ::setenv("LOOM_TEAM_RUNTIME_DIR", previous->c_str(), 1);
        else
            ::unsetenv("LOOM_TEAM_RUNTIME_DIR");
    }
};
}  // namespace

TEST(TeamStoreConcurrency, PublicSnapshotsCannotMutateStoreAndRefreshExplicitly) {
    TeamObserverSandbox sandbox;
    auto created = sandbox.store.create("snapshot", "snapshot",
                                        {loom::tools::TeamMember{.agent_id = "member"}});
    ASSERT_TRUE(created);
    created->members.front().status = loom::tools::MemberStatus::Error;
    EXPECT_EQ(sandbox.store.get("snapshot")->members.front().status,
              loom::tools::MemberStatus::Idle);
    ASSERT_TRUE(sandbox.store.update_member_status("snapshot", "member",
                                                   loom::tools::MemberStatus::Working));
    EXPECT_EQ(created->members.front().status, loom::tools::MemberStatus::Error);
    const auto refreshed = sandbox.store.get_by_id_or_name("snapshot");
    ASSERT_TRUE(refreshed);
    EXPECT_EQ(refreshed->members.front().status, loom::tools::MemberStatus::Working);
    EXPECT_EQ(loom::tools::load_team_record("snapshot")->members.front().status,
              loom::tools::MemberStatus::Working);
}

TEST(TeamStoreConcurrency, SiblingStatusWritersAndSnapshotsPreserveBothPersistedResults) {
    TeamObserverSandbox sandbox;
    auto& store = sandbox.store;
    ASSERT_TRUE(store.create(
        "siblings", "siblings",
        {loom::tools::TeamMember{.agent_id = "A"}, loom::tools::TeamMember{.agent_id = "B"}}));
    std::barrier start(4);
    const auto update = [&](const std::string& id) {
        start.arrive_and_wait();
        for (int i = 0; i < 24; ++i) {
            EXPECT_TRUE(
                store.update_member_status("siblings", id, loom::tools::MemberStatus::Working));
            EXPECT_TRUE(store.update_member_status("siblings", id, loom::tools::MemberStatus::Done,
                                                   id + "-" + std::to_string(i)));
        }
    };
    std::jthread first([&] { update("A"); });
    std::jthread second([&] { update("B"); });
    std::jthread reader([&] {
        start.arrive_and_wait();
        for (int i = 0; i < 48; ++i) {
            const auto snapshot = store.get_by_id_or_name("siblings");
            EXPECT_TRUE(snapshot);
            if (snapshot) EXPECT_EQ(snapshot->members.size(), 2u);
            const auto persisted = loom::tools::load_team_record("siblings");
            EXPECT_TRUE(persisted);
            if (persisted) EXPECT_EQ(persisted->members.size(), 2u);
        }
    });
    start.arrive_and_wait();
    first.join();
    second.join();
    reader.join();
    const auto snapshot = store.get("siblings");
    const auto persisted = loom::tools::load_team_record("siblings");
    ASSERT_TRUE(snapshot && persisted);
    EXPECT_EQ(snapshot->members.front().last_result, "A-23");
    EXPECT_EQ(snapshot->members.back().last_result, "B-23");
    EXPECT_EQ(persisted->members.front().last_result, "A-23");
    EXPECT_EQ(persisted->members.back().last_result, "B-23");
}

TEST(TeamStoreConcurrency, RemovedMembershipBetweenSnapshotAndWorkingCannotBeRecreated) {
    TeamObserverSandbox sandbox;
    auto& store = sandbox.store;
    ASSERT_TRUE(
        store.create("removed", "removed", {loom::tools::TeamMember{.agent_id = "member"}}));
    std::barrier boundary(2);
    std::expected<void, loom::tools::TeamError> admitted;
    std::jthread resume([&] {
        const auto before = store.get("removed");
        EXPECT_TRUE(before && before->members.size() == 1u);
        boundary.arrive_and_wait();
        boundary.arrive_and_wait();
        admitted =
            store.update_member_status("removed", "member", loom::tools::MemberStatus::Working);
    });
    boundary.arrive_and_wait();
    EXPECT_TRUE(store.remove("removed"));
    EXPECT_TRUE(store.create("removed", "removed", {}));
    boundary.arrive_and_wait();
    resume.join();
    ASSERT_FALSE(admitted);
    EXPECT_EQ(admitted.error(), loom::tools::TeamError::MemberNotFound);
    EXPECT_TRUE(store.get("removed")->members.empty());
}

TEST(TeamStoreConcurrency, ConcurrentAddRemoveLookupKeepsOwnedSnapshotsIndependent) {
    TeamObserverSandbox sandbox;
    auto& store = sandbox.store;
    ASSERT_TRUE(
        store.create("changing", "changing", {loom::tools::TeamMember{.agent_id = "stable"}}));
    const auto retained = store.get("changing");
    std::barrier start(3);
    std::jthread writer([&] {
        start.arrive_and_wait();
        for (int i = 0; i < 24; ++i) {
            EXPECT_TRUE(store.remove("changing"));
            EXPECT_TRUE(store.create("changing", "changing", {}));
            EXPECT_TRUE(store.add_member("changing", loom::tools::TeamMember{.agent_id = "new"}));
        }
    });
    std::jthread reader([&] {
        start.arrive_and_wait();
        for (int i = 0; i < 96; ++i) {
            const auto snapshot = store.get_by_id_or_name("changing");
            if (snapshot) {
                EXPECT_LE(snapshot->members.size(), 1u);
                if (!snapshot->members.empty())
                    EXPECT_TRUE(snapshot->members.front().agent_id == "stable" ||
                                snapshot->members.front().agent_id == "new");
            } else
                EXPECT_EQ(snapshot.error(), loom::tools::TeamError::TeamNotFound);
        }
    });
    start.arrive_and_wait();
    writer.join();
    reader.join();
    ASSERT_TRUE(retained);
    ASSERT_EQ(retained->members.size(), 1u);
    EXPECT_EQ(retained->members.front().agent_id, "stable");
    EXPECT_EQ(store.get("changing")->members.front().agent_id, "new");
}

TEST(NativeAgentAdmission, RemovedTeamMemberBeforeHostAdmissionPropagatesWithoutStarting) {
    TeamObserverSandbox sandbox;
    auto& teams = loom::tools::global_team_store();
    teams.clear_for_testing();

    struct ClearTeams {
        ~ClearTeams() { loom::tools::global_team_store().clear_for_testing(); }
    } clear_teams;

    ASSERT_TRUE(
        teams.create("admission", "admission", {loom::tools::TeamMember{.agent_id = "member"}}));
    auto& store = sandbox.runtime.store;
    auto record = runtime::make_native_agent_record("member", "general-purpose");
    record.parent_session_id = "owner";
    record.team_name = "admission";
    record.teammate_backend = "in-process";
    record.status = runtime::NativeAgentStatus::Failed;
    record.transcript = {"retained context"};
    store.upsert(record);
    const loom::core::HostAgentResumeConstraint target{"member",
                                                       "general-purpose",
                                                       "owner",
                                                       record.cwd,
                                                       record.team_name,
                                                       record.name,
                                                       record.teammate_backend,
                                                       record.teammate_task_id,
                                                       record.teammate_pane_id};
    std::barrier boundary(2);
    std::expected<void, std::string> admitted;
    std::jthread caller([&] {
        const auto observed = teams.get("admission");
        EXPECT_TRUE(observed && observed->members.size() == 1u);
        boundary.arrive_and_wait();
        boundary.arrive_and_wait();
        admitted = store.reserve_resume("member", target);
    });
    boundary.arrive_and_wait();
    EXPECT_TRUE(teams.remove("admission"));
    EXPECT_TRUE(teams.create("admission", "admission", {}));
    boundary.arrive_and_wait();
    caller.join();
    ASSERT_FALSE(admitted);
    EXPECT_NE(admitted.error().find("no longer a member"), std::string::npos);
    EXPECT_TRUE(store.resume_unavailable_reason("member").empty());
    EXPECT_EQ(store.get("member")->status, runtime::NativeAgentStatus::Failed);
    EXPECT_TRUE(teams.get("admission")->members.empty());
}
