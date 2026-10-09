/// @file test_permission_grants.cpp
/// @brief Worker permission grants (C15/C16) concurrency tests.

#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <httplib.h>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

import std;
import loom.tools.bash;
import loom.tools.computer_use;
import loom.tools.powershell;
import loom.tools.runtime_computer_use;
import loom.tools.runtime_shared_utils;
import loom.tools.runtime_team_shared;
import loom.tools.runtime_message_delivery;
import loom.tools.send_message;
import loom.tools.web_fetch;
import loom.tools.web_search;
import loom.tools.web_browser;
import loom.orchestration.tools.mcp;
import loom.commands.mcp.core_settings_loader;
import loom.orchestration.runtime_backends;
import loom.tools.runtime_backends.port;  // RFC-0001 B15: set_/clear_skill_loader_executor slot API
import loom.tools.image_codec.port;
import loom.tools.worktree;
import loom.orchestration.agent;
import loom.tools.agent_runtime;
import loom.tools.file_read;
import loom.tools.file_write;
import loom.tools.todo_write;
import loom.tools.notebook;
import loom.tools.registry;
import loom.tools.runtime_registry;
import loom.config.config;
import loom.tools.path_validation;
import loom.tools.bash_security;
import loom.tools.bash_permissions;
import loom.tools.task;
import loom.tools.team;
import loom.tools.team_create;
import loom.tools.team_delete;
import loom.tools.tool;
import loom.serdes.json;
import loom.security.tool_deny_rules;
import loom.query.query_engine;
import loom.teams.swarm.backends;
import loom.teams.swarm.helpers;
import loom.teams.team_helpers;
import loom.hooks.tool_permissions;
import loom.services.api.client;
import loom.services.mcp.types;
import loom.services.mcp.connection_manager;  // RFC-0001 B6: svc_mcp::McpServerSnapshot
import loom.tools.repl;
import loom.tools.skill;
import loom.orchestration.agent.utils;
import loom.tools.destructive_command_warning;

namespace fs = std::filesystem;

// The agent helpers exercised by the existing tests live in
// loom::tools::agent::utils after the agent_tool split; re-expose them through
// the loom::tools::agent namespace so the historical call sites still resolve.
namespace loom::tools::agent { using namespace utils; }

namespace {

struct EnvironmentGuard {
    std::string name;
    std::optional<std::string> previous;

    EnvironmentGuard(std::string key, const std::string& value) : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        setenv(name.c_str(), value.c_str(), 1);
    }

    ~EnvironmentGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

} // namespace

// ===========================================================================
// RFC-0001 B followup c15 — WorkerPermissionGrants read-merge-write must be
// ONE region under BOTH the in-process mutex and the cross-process flock on
// the grants path sibling. Before the fix load_rules() ran OUTSIDE the
// flock that only save_rules() took, so concurrent last-writer-wins lost
// distinct rules (measured: 100/200 cross-process, 133/400 in-process).
// ===========================================================================
namespace {

// Hermetic team runtime root for c15 grant tests: every grant path
// resolves under it via LOOM_TEAM_RUNTIME_DIR, so no test touches the repo
// cwd's .loom tree.
struct C15GrantsEnv {
    fs::path root;
    EnvironmentGuard guard;

    C15GrantsEnv()
        : root(fs::temp_directory_path() /
               ("loom_c15grants_" +
                std::to_string(::getpid()) + "_" +
                std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()))),
          guard("LOOM_TEAM_RUNTIME_DIR", root.string()) {
        fs::create_directories(root);
    }
    ~C15GrantsEnv() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
};

[[nodiscard]] fs::path c15_grants_file(std::string_view team,
                                       std::string_view agent) {
    return fs::path{loom::utils::team_dir(team)} / "permissions" /
           ("worker-allow-" + std::string(agent) + ".json");
}

// One addRules/allow batch as the leader would emit it.
[[nodiscard]] std::string c15_updates_json(int producer, int count,
                                           std::string_view prefix) {
    std::string out =
        R"([{"type":"addRules","behavior":"allow","rules":[)";
    for (int i = 0; i < count; ++i) {
        if (i != 0) out += ',';
        out += R"({"toolName":")";
        out += std::format("{}-{}-{}", prefix, producer, i);
        out += "\"}";
    }
    out += "]}]";
    return out;
}

[[nodiscard]] std::set<std::string> c15_read_rule_names(const fs::path& path) {
    std::set<std::string> names;
    auto parsed = loom::utils::json::parse_file(path);
    if (!parsed) return names;
    const auto list = parsed->root().get("rules");
    if (!list.is_arr()) return names;
    list.iter([&](loom::utils::json::JsonVal rule) {
        const auto name = rule.get("tool_name");
        if (name.is_str()) names.insert(std::string(name.as_str()));
    });
    return names;
}

constexpr std::string_view kC15XTeam = "c15xteam";
constexpr std::string_view kC15XAgent = "worker";
constexpr int kC15Producers = 8;
constexpr int kC15RulesPerProducer = 25;

}  // namespace

// 8 forked processes apply 25 DISTINCT rules each (200 total) through the
// real WorkerPermissionGrants API concurrently: the file must finish with
// exactly 200 unique rules and valid JSON (the pre-fix attacker lost
// 100/200). Children re-apply until their own 25 read back — the update is
// idempotent, so this only rides out lock contention, never masks a loss.
TEST(WorkerPermissionGrantsC15, CrossProcessMergeLosesNoRules) {
    namespace sh = loom::utils::swarm_helpers;
    C15GrantsEnv env;

    auto child_loop = [](int producer) {
        const std::string updates =
            c15_updates_json(producer, kC15RulesPerProducer, "c15xtool");
        bool settled = false;
        for (int attempt = 0; attempt < 50; ++attempt) {
            {
                sh::WorkerPermissionGrants grants{std::string(kC15XTeam),
                                                  std::string(kC15XAgent)};
                grants.apply_updates(updates);
            }
            sh::WorkerPermissionGrants reader{std::string(kC15XTeam),
                                              std::string(kC15XAgent)};
            settled = true;
            for (int i = 0; i < kC15RulesPerProducer; ++i) {
                if (!reader.allows(
                        std::format("c15xtool-{}-{}", producer, i))) {
                    settled = false;
                    break;
                }
            }
            if (settled) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        _exit(settled ? 0 : 1);
    };

    pid_t pids[kC15Producers];
    for (int c = 0; c < kC15Producers; ++c) {
        const pid_t pid = ::fork();
        if (pid < 0) _exit(2);
        if (pid == 0) child_loop(c);
        pids[c] = pid;
    }
    for (int c = 0; c < kC15Producers; ++c) {
        int status = 0;
        ASSERT_EQ(::waitpid(pids[c], &status, 0), pids[c]);
        ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0)
            << "producer " << c << " never observed all its grants";
    }

    const auto grants_file = c15_grants_file(kC15XTeam, kC15XAgent);
    auto parsed = loom::utils::json::parse_file(grants_file);
    ASSERT_TRUE(parsed.has_value()) << "grant file must be valid JSON";
    const auto list = parsed->root().get("rules");
    ASSERT_TRUE(list.is_arr());

    const auto names = c15_read_rule_names(grants_file);
    EXPECT_EQ(names.size(),
              static_cast<std::size_t>(
                  kC15Producers * kC15RulesPerProducer));
    EXPECT_EQ(list.size(), names.size())
        << "no duplicate rules may be persisted";
    for (int c = 0; c < kC15Producers; ++c) {
        for (int i = 0; i < kC15RulesPerProducer; ++i) {
            EXPECT_TRUE(names.contains(
                std::format("c15xtool-{}-{}", c, i)))
                << "missing rule from producer " << c << " index " << i;
        }
    }
}

// 4 in-process threads apply 100 DISTINCT rules each (400 total) through
// separate WorkerPermissionGrants instances, as concurrent pane permission
// checks do: all 400 must persist, valid JSON (pre-fix: 133/400 lost).
TEST(WorkerPermissionGrantsC15, InProcessThreadsMergeLosesNoRules) {
    namespace sh = loom::utils::swarm_helpers;
    C15GrantsEnv env;
    const std::string team = "c15tteam";
    const std::string agent = "worker";
    constexpr int kThreads = 4;
    constexpr int kPerThread = 100;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([t, &team, &agent] {
            const std::string updates =
                c15_updates_json(t, kPerThread, "c15ttool");
            sh::WorkerPermissionGrants grants(team, agent);
            grants.apply_updates(updates);
        });
    }
    for (auto& thread : threads) thread.join();

    const auto grants_file = c15_grants_file(team, agent);
    auto parsed = loom::utils::json::parse_file(grants_file);
    ASSERT_TRUE(parsed.has_value()) << "grant file must be valid JSON";
    const auto list = parsed->root().get("rules");
    ASSERT_TRUE(list.is_arr());

    const auto names = c15_read_rule_names(grants_file);
    EXPECT_EQ(names.size(),
              static_cast<std::size_t>(kThreads * kPerThread));
    EXPECT_EQ(list.size(), names.size())
        << "no duplicate rules may be persisted";
    for (int t = 0; t < kThreads; ++t) {
        for (int i = 0; i < kPerThread; ++i) {
            EXPECT_TRUE(names.contains(
                std::format("c15ttool-{}-{}", t, i)));
        }
    }
}

// A pre-placed FIFO at the grants lock name must be rejected FAST after
// open via fstat()+S_ISREG: the update is dropped (fail closed), the data
// file is never created, and no writer waits on the (uncontended) FIFO.
TEST(WorkerPermissionGrantsC15, FifoLockNameRejectedWithoutBlocking) {
    namespace sh = loom::utils::swarm_helpers;
    C15GrantsEnv env;
    const std::string team = "c15fteam";
    const std::string agent = "worker";
    const auto grants_file = c15_grants_file(team, agent);
    fs::create_directories(grants_file.parent_path());
    const auto lock_path = grants_file.string() + ".lock";
    ASSERT_EQ(::mkfifo(lock_path.c_str(), 0600), 0);

    sh::WorkerPermissionGrants grants(team, agent);
    const auto start = std::chrono::steady_clock::now();
    grants.apply_updates(sh::build_always_allow_updates_json("Bash"));
    const auto elapsed = std::chrono::duration_cast<
        std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    EXPECT_LT(elapsed, 3000)
        << "non-regular lock name must fail fast, never await the deadline";
    EXPECT_FALSE(fs::exists(grants_file))
        << "failed lock must bail before any grant-file write";
    EXPECT_FALSE(grants.allows("Bash"));
    std::error_code ec;
    EXPECT_TRUE(fs::is_fifo(lock_path, ec))
        << "the FIFO itself must be left untouched";
}

// ===========================================================================
// RFC-0001 B followup c16 — per-path grants mutex shards and LOCK_SH rules
// reads. The c15 single process-global grants mutex coupled unrelated teams:
// one holder burning the 10 s flock timeout stalled EVERY other team's
// update for ~9.5 s. A 16-shard table indexed by a cheap FNV-1a path hash
// keeps same-file updates mutually excluded while distinct paths proceed.
// ===========================================================================
namespace {

[[nodiscard]] fs::path c16_grants_path_for(std::string_view team,
                                           std::string_view agent = "worker") {
    return fs::path{loom::utils::team_dir(team)} / "permissions" /
           ("worker-allow-" + std::string(agent) + ".json");
}

// Pipe-handshake exclusive flock holder (same idiom as the c14 tests).
struct C16GrantsHolder {
    pid_t pid = -1;
    int read_fd = -1;
};

struct C16GrantsReaper {
    pid_t pid = -1;
    C16GrantsReaper() = default;
    explicit C16GrantsReaper(pid_t child) : pid(child) {}
    C16GrantsReaper(const C16GrantsReaper&) = delete;
    C16GrantsReaper& operator=(const C16GrantsReaper&) = delete;
    ~C16GrantsReaper() {
        if (pid > 0) {
            (void)::kill(pid, SIGKILL);
            int status = 0;
            (void)::waitpid(pid, &status, 0);
        }
    }
    void release() { pid = -1; }
};

[[nodiscard]] C16GrantsHolder c16_spawn_grants_holder(
    const std::string& lock_path, int seconds) {
    int pipefd[2];
    if (::pipe(pipefd) != 0) return {-1, -1};
    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        return {-1, -1};
    }
    if (pid == 0) {
        ::close(pipefd[0]);
        const int fd = ::open(lock_path.c_str(),
                              O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        const char fail = 'E';
        if (fd < 0 || ::flock(fd, LOCK_EX) != 0) {
            (void)::write(pipefd[1], &fail, 1);
            _exit(10);
        }
        const char ready = 'R';
        if (::write(pipefd[1], &ready, 1) != 1) _exit(11);
        for (int i = 0; i < seconds * 10; ++i) {
            struct timespec ts{0, 100 * 1000 * 1000};
            ::nanosleep(&ts, nullptr);
        }
        _exit(0);
    }
    ::close(pipefd[1]);
    return {pid, pipefd[0]};
}

void c16_await_grants_ready(int fd) {
    char b = 0;
    ssize_t n = 0;
    do {
        n = ::read(fd, &b, 1);
    } while (n == -1 && errno == EINTR);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(b, 'R');
    ::close(fd);
}

[[nodiscard]] long long c16_elapsed_ms(
    const std::chrono::steady_clock::time_point& start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count();
}

}  // namespace

// The shard function is a pure function of the canonical path: same path →
// same shard (same mutex); there exist team paths on distinct shards (the
// pair the timing test uses). This makes the isolation test deterministic
// rather than relying on timing to infer which mutex was taken.
TEST(WorkerPermissionGrantsC16, ShardHashStableSamePathAndDistinctPairsExist) {
    namespace sh = loom::utils::swarm_helpers;
    C15GrantsEnv env;
    const auto path_a = c16_grants_path_for("c16hash-a");
    const auto path_a2 = c16_grants_path_for("c16hash-a");
    const auto path_b = c16_grants_path_for("c16hash-b");
    const auto i_a = sh::WorkerPermissionGrants::shard_index_for(
        path_a.string());
    EXPECT_LT(i_a, sh::WorkerPermissionGrants::kShardCount);
    EXPECT_EQ(i_a, sh::WorkerPermissionGrants::shard_index_for(
                       path_a2.string()));

    int distinct_pairs = 0;
    for (int i = 0; i < 64; ++i) {
        for (int j = i + 1; j < 64; ++j) {
            const auto pi = c16_grants_path_for(
                std::format("c16pair-{:02d}", i));
            const auto pj = c16_grants_path_for(
                std::format("c16pair-{:02d}", j));
            if (sh::WorkerPermissionGrants::shard_index_for(pi.string()) !=
                sh::WorkerPermissionGrants::shard_index_for(pj.string())) {
                ++distinct_pairs;
            }
        }
    }
    EXPECT_GT(distinct_pairs, 100)
        << "16 shards must spread distinct team paths";
    (void)path_b;
}

// Deterministic shard isolation: find two teams whose grants paths land on
// different shards; an external 30 s LOCK_EX holder blocks team A's update
// behind its bounded ~10 s flock wait, while team B's update on the other
// shard completes immediately (it never touches A's mutex). Pre-c16 the
// single global mutex made B wait the same ~10 s.
TEST(WorkerPermissionGrantsC16,
     DifferentShardGrantUpdateBypassesForeignFlockStall) {
    namespace sh = loom::utils::swarm_helpers;
    C15GrantsEnv env;

    // Search for a guaranteed different-shard pair.
    std::string team_a = "c16iso-a";
    std::string team_b = "c16iso-b";
    bool found = false;
    for (int i = 0; i < 256 && !found; ++i) {
        for (int j = 0; j < 256; ++j) {
            if (i == j) continue;
            const auto candidate_a = std::format("c16iso-{:03d}", i);
            const auto candidate_b = std::format("c16iso-{:03d}", j);
            if (sh::WorkerPermissionGrants::shard_index_for(
                    c16_grants_path_for(candidate_a).string()) !=
                sh::WorkerPermissionGrants::shard_index_for(
                    c16_grants_path_for(candidate_b).string())) {
                team_a = candidate_a;
                team_b = candidate_b;
                found = true;
                break;
            }
        }
    }
    ASSERT_TRUE(found);
    ASSERT_NE(sh::WorkerPermissionGrants::shard_index_for(
                  c16_grants_path_for(team_a).string()),
              sh::WorkerPermissionGrants::shard_index_for(
                  c16_grants_path_for(team_b).string()));

    const auto grants_a = c16_grants_path_for(team_a);
    fs::create_directories(grants_a.parent_path());
    const auto lock_a = grants_a.string() + ".lock";
    C16GrantsHolder h = c16_spawn_grants_holder(lock_a, 30);
    ASSERT_GT(h.pid, 0);
    C16GrantsReaper reaper(h.pid);
    c16_await_grants_ready(h.read_fd);

    // Team A's update takes shard A's mutex immediately, then blocks on the
    // cross-process flock for the bounded ~10 s and drops the update.
    long long a_elapsed = 0;
    std::thread blocked_a([&] {
        sh::WorkerPermissionGrants grants_a_store(team_a, "worker");
        const auto t0 = std::chrono::steady_clock::now();
        grants_a_store.apply_updates(
            sh::build_always_allow_updates_json("ToolA"));
        a_elapsed = c16_elapsed_ms(t0);
    });

    // Give A time to park in its flock poll loop (holding only shard A).
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Team B: different shard mutex, uncontended flock — must land fast.
    sh::WorkerPermissionGrants grants_b_store(team_b, "worker");
    const auto t0 = std::chrono::steady_clock::now();
    grants_b_store.apply_updates(
        sh::build_always_allow_updates_json("ToolB"));
    const auto b_elapsed = c16_elapsed_ms(t0);
    std::printf(
        "[c16] grants shard isolation: blocked team A still waiting; "
        "team B update took %lld ms (pre-c16 global mutex: ~9500 ms)\n",
        b_elapsed);
    EXPECT_LT(b_elapsed, 1000)
        << "a different-shard update must not wait for team A's flock stall";
    sh::WorkerPermissionGrants grants_b_read(team_b, "worker");
    EXPECT_TRUE(grants_b_read.allows("ToolB"))
        << "team B grant must have persisted";

    blocked_a.join();
    EXPECT_GE(a_elapsed, 9000)
        << "team A must still honor its own bounded flock wait";
    EXPECT_LE(a_elapsed, 14000);
    sh::WorkerPermissionGrants grants_a_read(team_a, "worker");
    EXPECT_FALSE(grants_a_read.allows("ToolA"))
        << "team A's update must have been dropped while the lock was held";

    ::kill(h.pid, SIGKILL);
    int status = 0;
    ASSERT_EQ(::waitpid(h.pid, &status, 0), h.pid);
    reaper.release();
}

// Same-file updates still serialize through ONE shard mutex + the flock:
// 6 processes applying 10 DISTINCT rules each (60 total) while 4 threads
// constantly read through the new LOCK_SH allows() path must finish with
// exactly 60 rules, valid JSON, and no read errors/crashes. Sized to stay
// well under a minute under serial ctest (the c15 8x25 merge tests already
// cover the heavy 200-rule identity case without concurrent readers).
TEST(WorkerPermissionGrantsC16, ApplyStormWithSharedReadersLosesNoRules) {
    namespace sh = loom::utils::swarm_helpers;
    C15GrantsEnv env;
    const std::string team = "c16stormgrants";
    constexpr int kWriters = 6;
    constexpr int kPerWriter = 10;

    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> reader_checks{0};
    constexpr int kStormMs = 1500;

    // Plain pipe (macOS-portable, no pipe2): each writer child writes one
    // 'S' byte the instant it leaves the timed storm and enters SETTLE, so
    // the parent can stop the LOCK_SH readers before children settle —
    // spinning SH readers would otherwise prolong (and on a loaded box
    // dominate) the writers' flock contention during settle.
    int signal_pipe[2];
    int go_pipe[2];
    ASSERT_EQ(::pipe(signal_pipe), 0);
    ASSERT_EQ(::pipe(go_pipe), 0);

    // Fork the 6 writer children BEFORE starting any reader thread: a
    // fork() in a multithreaded process would give the child a frozen copy
    // of unrelated locks (the c15 storm uses the same ordering). Each child
    // self-times the storm (a forked child gets a COPY of the parent's
    // atomics, so a reader-counter gate would never flip), idempotently
    // re-applies its batch for kStormMs, signals the pipe, then SETTLES
    // without readers present: it keeps re-applying until all its own rules
    // read back, so a final apply dropped by the bounded flock wait can
    // never leave a rule missing (the merge dedupes and the final file ends
    // with each distinct rule exactly once).
    auto child_loop = [&](int producer) {
        ::close(signal_pipe[0]);
        ::close(go_pipe[1]);
        const std::string updates =
            c15_updates_json(producer, kPerWriter, "c16stormtool");
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(kStormMs);
        while (std::chrono::steady_clock::now() < deadline) {
            sh::WorkerPermissionGrants grants{team, "worker"};
            grants.apply_updates(updates);
        }
        const char settling = 'S';
        (void)::write(signal_pipe[1], &settling, 1);
        // Wait for the parent to stop the LOCK_SH readers before settling,
        // so settle's EX/SH flocking never contends with them.
        char go = 0;
        ssize_t gn = 0;
        do {
            gn = ::read(go_pipe[0], &go, 1);
        } while (gn == -1 && errno == EINTR);
        if (gn != 1 || go != 'G') _exit(3);
        bool settled = false;
        for (int attempt = 0; attempt < 100; ++attempt) {
            {
                sh::WorkerPermissionGrants grants{team, "worker"};
                grants.apply_updates(updates);
            }
            sh::WorkerPermissionGrants reader{team, "worker"};
            settled = true;
            for (int i = 0; i < kPerWriter; ++i) {
                if (!reader.allows(
                        std::format("c16stormtool-{}-{}", producer, i))) {
                    settled = false;
                    break;
                }
            }
            if (settled) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        _exit(settled ? 0 : 1);
    };

    pid_t pids[kWriters];
    for (int c = 0; c < kWriters; ++c) {
        const pid_t pid = ::fork();
        if (pid < 0) _exit(2);
        if (pid == 0) child_loop(c);
        pids[c] = pid;
    }
    ::close(signal_pipe[1]);  // parent holds only the signal read end
    ::close(go_pipe[0]);      // and the go write end

    // Readers (LOCK_SH) start only after every writer child exists. They
    // yield each pass so EX writers get lock time during the storm.
    std::vector<std::thread> readers;
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&] {
            sh::WorkerPermissionGrants reader(team, "worker");
            while (!stop.load(std::memory_order_relaxed)) {
                // LOCK_SH parse on every check; must never crash/hang and
                // only ever returns a coherent (possibly empty) rule set.
                (void)reader.allows("c16stormtool-0-0");
                reader_checks.fetch_add(1, std::memory_order_relaxed);
                // Leave real windows for EX writers: the production inbox
                // poller cadence is 1500 ms, and an SH that never releases
                // starves flock-EX writers up to their bounded wait.
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    // Drain signals until every child has left the storm, then stop the
    // readers so the children settle without SH/EX lock contention. No
    // ASSERT_* here: an early return would leave the reader threads
    // joinable (std::terminate) and the children un-reaped; the signal
    // count is checked after the joins/waitpids below.
    int signals = 0;
    while (signals < kWriters) {
        char signal = 0;
        ssize_t n = 0;
        do {
            n = ::read(signal_pipe[0], &signal, 1);
        } while (n == -1 && errno == EINTR);
        if (n != 1 || signal != 'S') break;
        ++signals;
    }
    ::close(signal_pipe[0]);
    stop.store(true, std::memory_order_relaxed);
    for (auto& t : readers) t.join();
    // Readers are gone: release every blocked child into its settle phase.
    for (int c = 0; c < kWriters; ++c) {
        const char go = 'G';
        (void)::write(go_pipe[1], &go, 1);
    }
    ::close(go_pipe[1]);
    EXPECT_GT(reader_checks.load(), 50u)
        << "LOCK_SH readers must make many coherent checks during the storm";
    EXPECT_EQ(signals, kWriters);

    for (int c = 0; c < kWriters; ++c) {
        int status = 0;
        ASSERT_EQ(::waitpid(pids[c], &status, 0), pids[c]);
        ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0)
            << "writer " << c << " never applied its grants";
    }

    const auto grants_file = c16_grants_path_for(team);
    auto parsed = loom::utils::json::parse_file(grants_file);
    ASSERT_TRUE(parsed.has_value());
    const auto list = parsed->root().get("rules");
    ASSERT_TRUE(list.is_arr());
    const auto names = c15_read_rule_names(grants_file);
    EXPECT_EQ(names.size(),
              static_cast<std::size_t>(kWriters * kPerWriter));
    EXPECT_EQ(list.size(), names.size())
        << "no duplicate rules may be persisted";
}

// c16a: the LOCK_SH allows() reader opens the grants leaf ONCE
// (O_NOFOLLOW|O_NONBLOCK) and parses the buffer — a FIFO swapped over the
// leaf yields the ordinary empty rule set INSTANTLY (never blocks waiting
// for a peer), a symlink is never followed, an absent file is empty, and a
// regular grants file parses byte-identically.
TEST(WorkerPermissionGrantsC16a, ReaderSwapLeavesFailFastAndRegularParses) {
    namespace sh = loom::utils::swarm_helpers;
    C15GrantsEnv env;

    // Absent grants file: not allowed, immediate.
    {
        sh::WorkerPermissionGrants grants("c16aabsent", "worker");
        const auto t0 = std::chrono::steady_clock::now();
        EXPECT_FALSE(grants.allows("ToolA"));
        EXPECT_LT(c16_elapsed_ms(t0), 100);
    }

    const auto path = c16_grants_path_for("c16arules");
    fs::create_directories(path.parent_path());

    // FIFO: allows() stays false and returns under 100ms.
    ASSERT_EQ(::mkfifo(path.string().c_str(), 0600), 0);
    {
        sh::WorkerPermissionGrants grants("c16arules", "worker");
        const auto t0 = std::chrono::steady_clock::now();
        EXPECT_FALSE(grants.allows("ToolA"));
        EXPECT_LT(c16_elapsed_ms(t0), 100)
            << "a FIFO grants leaf must never block the shared reader";
        std::error_code ec;
        EXPECT_TRUE(fs::is_fifo(path, ec));
    }

    // Symlink: false, canary target never opened.
    const auto canary = env.root / "grants-canary.txt";
    constexpr std::string_view kCanary = "c16a-grants-canary-2956";
    {
        std::error_code ec;
        fs::remove(path, ec);
        std::ofstream out(canary, std::ios::binary);
        out << kCanary;
    }
    fs::create_symlink(canary, path);
    {
        sh::WorkerPermissionGrants grants("c16arules", "worker");
        EXPECT_FALSE(grants.allows("ToolA"));
        std::ifstream in(canary, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(bytes, std::string(kCanary))
            << "the symlinked grants target must never be opened";
        std::error_code ec;
        EXPECT_TRUE(fs::is_symlink(path, ec));
    }

    // Regular grants file: identical parse, the rule takes effect.
    {
        std::error_code ec;
        fs::remove(path, ec);
        sh::WorkerPermissionGrants writer("c16arules", "worker");
        writer.apply_updates(
            sh::build_always_allow_updates_json("ToolA"));
        sh::WorkerPermissionGrants reader("c16arules", "worker");
        EXPECT_TRUE(reader.allows("ToolA"));
        EXPECT_FALSE(reader.allows("ToolOther"));
    }
}
