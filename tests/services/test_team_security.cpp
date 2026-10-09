/// @file test_team_security.cpp
/// @brief Service layer tests split from test_services.cpp.

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif

#include <gtest/gtest.h>
#include <httplib.h>
#include <limits>



import std;
import loom.cli.ccr_client;
import loom.cli.sse_transport;
import loom.bridge.core;
import loom.config.config;
import loom.constants.paths;
import loom.services.api.client;
import loom.services.api.errors;
import loom.services.api.session_ingress;
import loom.services.api.streaming;
import loom.services.compact.api_microcompact;
import loom.services.lsp.LSPServerManager;
import loom.services.lsp.client;
import loom.services.mcp.client;
import loom.services.mcp.auth;
import loom.services.mcp.channel_permissions;
import loom.services.mcp.config;
import loom.services.mcp.connection_manager;
import loom.services.mcp.elicitation_handler;
import loom.services.mcp.headers_helper;
import loom.services.mcp.vscode_sdk_mcp;
import loom.services.memory.sessionMemory;
import loom.services.extract_memories;
import loom.services.mcp.types;
import loom.services.mcp.xaa;
import loom.services.mcp.xaa_idp_login;
import loom.services.mcp.oauth_port;
import loom.services.rate_limit;
import loom.services.token_estimation;
import loom.services.prompt_suggestion;
import loom.server.server_routes;
import loom.server.server_main;
import loom.session.storage;
import loom.session.history;
import loom.commands.config;
import loom.commands.mcp.core_settings_loader;
import loom.orchestration.tools.mcp;
import loom.query.query_engine;
import loom.memdir.paths;
import loom.tools.agent_runtime;
import loom.tools.team;
import loom.tools.tool;
import loom.types.types;
import loom.utils.error;
import loom.services.ide_integration;
import loom.serdes.json;
import loom.teams.team_helpers;
import loom.fs.atomic_replace;
import loom.daemon.worker_registry;
import loom.server.types;

namespace fs = std::filesystem;

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

struct C14TeamEnv {
    fs::path root;
    EnvironmentGuard team_dir_guard;

    C14TeamEnv()
        : root([] {
              static std::atomic<unsigned> counter{0};
              const auto suffix =
                  std::chrono::system_clock::now().time_since_epoch().count();
              return fs::temp_directory_path() /
                     (std::string("loom_c14_") +
                      std::to_string(::getpid()) + "_" +
                      std::to_string(counter.fetch_add(
                          1, std::memory_order_relaxed)) +
                      "_" + std::to_string(suffix));
          }()),
          team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string()) {
        fs::create_directories(root);
    }
    ~C14TeamEnv() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }
};

[[nodiscard]] fs::path c14_inbox_path(
    std::string_view agent = "worker",
    std::string_view team = "c14team") {
    return fs::path{loom::utils::get_inbox_path(
        agent, std::optional<std::string_view>{team})};
}

[[nodiscard]] std::string c14_read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

struct C14Holder {
    pid_t pid;
    int read_fd;
};

struct C14HolderReaper {
    pid_t pid = -1;
    C14HolderReaper() = default;
    explicit C14HolderReaper(pid_t child) : pid(child) {}
    C14HolderReaper(const C14HolderReaper&) = delete;
    C14HolderReaper& operator=(const C14HolderReaper&) = delete;
    ~C14HolderReaper() {
        if (pid > 0) {
            (void)::kill(pid, SIGKILL);
            int status = 0;
            (void)::waitpid(pid, &status, 0);
        }
    }
    void release() { pid = -1; }
};

[[nodiscard]] C14Holder c14_spawn_holder(const std::string& lock_path,
                                         int seconds) {
    int pipefd[2];
    if (::pipe(pipefd) != 0) {
        ADD_FAILURE() << "pipe: " << std::strerror(errno);
        return {-1, -1};
    }
    const pid_t pid = ::fork();
    if (pid < 0) {
        ADD_FAILURE() << "fork: " << std::strerror(errno);
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        return {-1, -1};
    }
    if (pid == 0) {
        ::close(pipefd[0]);
        const int fd = ::open(lock_path.c_str(),
                              O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        const char fail = 'E';
        if (fd < 0) {
            (void)::write(pipefd[1], &fail, 1);
            _exit(10);
        }
        if (::flock(fd, LOCK_EX) != 0) {
            (void)::write(pipefd[1], &fail, 1);
            _exit(11);
        }
        const char ready = 'R';
        if (::write(pipefd[1], &ready, 1) != 1) _exit(12);
        for (int i = 0; i < seconds * 10; ++i) {
            struct timespec ts{0, 100 * 1000 * 1000};
            ::nanosleep(&ts, nullptr);
        }
        _exit(0);
    }
    ::close(pipefd[1]);  // parent's only handle is the read end
    return {pid, pipefd[0]};
}

void c14_await_ready(int fd) {
    char b = 0;
    ssize_t n = 0;
    do {
        n = ::read(fd, &b, 1);
    } while (n == -1 && errno == EINTR);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(b, 'R') << "holder child failed to acquire the lock";
    ::close(fd);
}

[[nodiscard]] long long c14_elapsed_ms(
    const std::chrono::steady_clock::time_point& start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start)
        .count();
}

[[nodiscard]] mode_t c16_mode_bits(const fs::path& path) {
    struct ::stat st {};
    EXPECT_EQ(::lstat(path.c_str(), &st), 0) << path.string();
    return st.st_mode & 07777;
}

[[nodiscard]] bool c16_no_tmp_debris(const fs::path& dir) {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.path().filename().string().find(".tmp.") !=
            std::string::npos) {
            return false;
        }
    }
    return true;
}

void c16_raw_truncating_writer(const fs::path& path,
                               std::string half1,
                               std::string half2,
                               int duration_ms) {
    constexpr struct timespec kGap1{0, 300 * 1000};   // 300us
    constexpr struct timespec kGap2{0, 200 * 1000};   // 200us
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(duration_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        const int fd = ::open(path.c_str(),
                              O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) _exit(20);
        ::nanosleep(&kGap1, nullptr);  // post-truncate empty window
        const ssize_t n1 =
            ::write(fd, half1.data(), half1.size());
        ::nanosleep(&kGap2, nullptr);  // partial-content window
        const ssize_t n2 =
            ::write(fd, half2.data(), half2.size());
        if (n1 < 0 || n2 < 0) {
            ::close(fd);
            _exit(21);
        }
        ::close(fd);
    }
    _exit(0);
}

struct C16TornCounters {
    std::atomic<std::uint64_t> attempts{0};
    std::atomic<std::uint64_t> torn{0};
};

void c16_raw_array_reader(const fs::path& path,
                          C16TornCounters& counters,
                          std::atomic<bool>* stop,
                          std::uint64_t max_attempts) {
    while (!stop->load(std::memory_order_relaxed) &&
           counters.attempts.load(std::memory_order_relaxed) < max_attempts) {
        auto parsed = loom::utils::json::parse_file(path);
        counters.attempts.fetch_add(1, std::memory_order_relaxed);
        if (!parsed || !parsed->root().is_arr()) {
            counters.torn.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

void c16_storm_writer_child(std::string teams_root,
                            std::string team,
                            std::string agent,
                            int duration_ms) {
    ::setenv("LOOM_TEAM_RUNTIME_DIR", teams_root.c_str(), 1);
    const auto inbox =
        fs::path{loom::utils::get_inbox_path(agent,
                    std::optional<std::string_view>{team})};
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(duration_ms);
    std::uint64_t seq = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            loom::utils::ScopedInboxLock flock(inbox);
            if (!flock.locked()) _exit(30);
            std::vector<loom::utils::TeammateMessage> messages;
            for (int m = 0; m < 3; ++m) {
                messages.push_back(loom::utils::TeammateMessage{
                    .from = std::format("w{}", m),
                    .text = std::format("seq-{}-msg-{}", seq, m),
                    .timestamp = std::to_string(seq),
                    .read = (m == 0),
                    .color = std::nullopt,
                    .summary = std::nullopt,
                });
            }
            if (!loom::utils::detail::write_messages(inbox, messages)) {
                _exit(31);
            }
        }
        {
            loom::utils::TeamFileRecord record;
            record.name = team;
            record.lead_agent_id = "team-lead@" + team;
            record.created_at = static_cast<std::int64_t>(seq);
            for (int m = 0; m < 2; ++m) {
                loom::utils::TeamMemberRecord member;
                member.agent_id = std::format("w{}@{}", m, team);
                member.name = std::format("w{}", m);
                member.tmux_pane_id = std::to_string((seq + m) % 7);
                member.cwd = "/tmp";
                member.joined_at = static_cast<std::int64_t>(seq);
                record.members.push_back(std::move(member));
            }
            if (!loom::utils::write_team_file(team, record)) _exit(32);
        }
        ++seq;
    }
    _exit(0);
}

[[nodiscard]] long long c16a_elapsed_ms(
    const std::chrono::steady_clock::time_point& start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start)
        .count();
}

} // namespace


// ===========================================================================
// RFC-0001 B followup c14 — ScopedInboxLock hard wall-clock deadline and
// O_NOFOLLOW. The old loop advertised "200 attempts ~10s" but called a
// BLOCKING flock(LOCK_EX): while contended it never returns EWOULDBLOCK, so
// the counter advanced only on EINTR and the wait was effectively unbounded.
// These tests pin the same steady_clock deadline pattern/constants as
// ConfigFileLock (c13i) and refusal of a symlinked lock name.
// ===========================================================================


// A 2s external holder makes acquisition WAIT (blocking-style behavior) and
// then succeed once the holder exits and the kernel releases the flock.

TEST(ScopedInboxLockC14, BoundedWaitSucceedsAfterHolderReleases) {
    C14TeamEnv env;
    const auto inbox = c14_inbox_path();
    fs::create_directories(inbox.parent_path());
    const std::string lock_path = inbox.string() + ".lock";

    C14Holder h = c14_spawn_holder(lock_path, 2);
    ASSERT_GT(h.pid, 0);
    C14HolderReaper reaper(h.pid);
    c14_await_ready(h.read_fd);

    const auto start = std::chrono::steady_clock::now();
    loom::utils::ScopedInboxLock lock(inbox);
    const auto elapsed = c14_elapsed_ms(start);
    ASSERT_TRUE(lock.locked());
    EXPECT_GE(elapsed, 1000) << "acquisition should wait on the holder";
    EXPECT_LE(elapsed, 6000)
        << "lock should be granted shortly after the 2s holder exits";

    int status = 0;
    ASSERT_EQ(::waitpid(h.pid, &status, 0), h.pid);
    reaper.release();  // holder exited and is reaped
}


// A 30s holder trips the ~10s WALL-CLOCK bound: direct construction reports
// locked()==false within [9s, 14s], and the production guarded mailbox
// writer fails closed in the same contention window without touching the
// inbox. Both contenders run concurrently so the suite pays one 10s wait,
// not two; holder readiness is the pipe handshake, never a sleep guess.

TEST(ScopedInboxLockC14, BoundedWaitTimesOutAndMailboxWriteFailsClosed) {
    C14TeamEnv env;
    const auto inbox = c14_inbox_path();
    fs::create_directories(inbox.parent_path());
    const std::string canary =
        R"JSON([{"from":"lead","text":"keep","timestamp":"1","read":true}])JSON";
    {
        std::ofstream out(inbox, std::ios::binary);
        out << canary;
    }
    const std::string lock_path = inbox.string() + ".lock";

    C14Holder h = c14_spawn_holder(lock_path, 30);
    ASSERT_GT(h.pid, 0);
    C14HolderReaper reaper(h.pid);
    c14_await_ready(h.read_fd);

    struct DirectResult {
        bool locked = true;
        long long elapsed_ms = 0;
    } direct;
    std::thread contender([&] {
        const auto t0 = std::chrono::steady_clock::now();
        loom::utils::ScopedInboxLock lock(inbox);
        direct.elapsed_ms = c14_elapsed_ms(t0);
        direct.locked = lock.locked();
    });

    const auto write_start = std::chrono::steady_clock::now();
    auto written = loom::utils::write_to_mailbox(
        "worker",
        loom::utils::TeammateMessage{
            .from = "lead",
            .text = "must-not-land",
            .timestamp = "2",
            .read = false,
            .color = std::nullopt,
            .summary = std::nullopt,
        },
        std::optional<std::string_view>{"c14team"});
    const auto write_elapsed = c14_elapsed_ms(write_start);
    contender.join();

    EXPECT_FALSE(direct.locked);
    EXPECT_GE(direct.elapsed_ms, 9000) << "should wait up to the ~10s bound";
    EXPECT_LE(direct.elapsed_ms, 14000)
        << "deadline must hold under scheduler load";

    ASSERT_FALSE(written.has_value());
    EXPECT_NE(written.error().find("lock"), std::string::npos)
        << written.error();
    EXPECT_GE(write_elapsed, 9000) << "guarded writer should share the bound";
    EXPECT_LE(write_elapsed, 14000) << "guarded writer must hold the deadline";

    ::kill(h.pid, SIGKILL);
    int status = 0;
    ASSERT_EQ(::waitpid(h.pid, &status, 0), h.pid);
    reaper.release();  // holder killed and reaped

    // Inbox bytes unmodified; only the inbox and its lock file exist.
    EXPECT_EQ(c14_read_file(inbox), canary);
    bool saw_lock = false;
    for (const auto& entry :
         fs::directory_iterator(inbox.parent_path())) {
        const auto name = entry.path().filename().string();
        if (name == "worker.json") continue;
        if (name == "worker.json.lock") {
            saw_lock = true;
            continue;
        }
        ADD_FAILURE() << "unexpected inbox-dir debris: " << name;
    }
    EXPECT_TRUE(saw_lock);
}


// A pre-placed symlink at the lock name fails the O_NOFOLLOW open
// immediately: locked()==false, the guarded writer fails closed, and neither
// the symlink nor its target is ever followed or modified.

TEST(ScopedInboxLockC14, SymlinkedLockNameRejectedAndTargetUntouched) {
    C14TeamEnv env;
    const auto inbox = c14_inbox_path();
    fs::create_directories(inbox.parent_path());
    const fs::path lock_path = fs::path{inbox} += ".lock";
    const auto canary_path = env.root / "canary.txt";
    const std::string canary = "symlink-target-canary\n";
    {
        std::ofstream out(canary_path, std::ios::binary);
        out << canary;
    }
    fs::create_symlink(canary_path, lock_path);
    ASSERT_TRUE(fs::is_symlink(lock_path));

    {
        loom::utils::ScopedInboxLock lock(inbox);
        EXPECT_FALSE(lock.locked());
    }
    EXPECT_TRUE(fs::is_symlink(lock_path))
        << "failed open must not unlink or replace the symlink";
    EXPECT_EQ(c14_read_file(canary_path), canary);

    // The production guarded writer fails closed on the same lock name,
    // without a deadline wait and without creating the inbox.
    auto written = loom::utils::write_to_mailbox(
        "worker",
        loom::utils::TeammateMessage{
            .from = "lead",
            .text = "must-not-land",
            .timestamp = "2",
            .read = false,
            .color = std::nullopt,
            .summary = std::nullopt,
        },
        std::optional<std::string_view>{"c14team"});
    ASSERT_FALSE(written.has_value());
    EXPECT_NE(written.error().find("lock"), std::string::npos)
        << written.error();
    EXPECT_TRUE(fs::is_symlink(lock_path));
    EXPECT_EQ(c14_read_file(canary_path), canary);
    EXPECT_FALSE(fs::exists(inbox))
        << "failed lock acquisition must bail before any inbox write";
}


// RFC-0001 B followup c15 — a pre-placed FIFO at the lock name opens
// successfully (O_RDWR on a FIFO needs no reader/writer peer) and flock(2)
// works on any fd type, so without fstat()+S_ISREG the lock would be taken
// on a FIFO instead of rejected. Acquisition must fail FAST (no deadline
// wait), the guarded mailbox writer must fail closed, and the FIFO itself
// must be left in place.

TEST(ScopedInboxLockC15, FifoLockNameRejectedWithoutBlocking) {
    C14TeamEnv env;
    const auto inbox = c14_inbox_path();
    fs::create_directories(inbox.parent_path());
    const fs::path lock_path = fs::path{inbox} += ".lock";
    ASSERT_EQ(::mkfifo(lock_path.c_str(), 0600), 0);

    const auto direct_start = std::chrono::steady_clock::now();
    bool direct_locked = true;
    {
        loom::utils::ScopedInboxLock lock(inbox);
        direct_locked = lock.locked();
    }
    const auto direct_elapsed = c14_elapsed_ms(direct_start);
    EXPECT_FALSE(direct_locked);
    EXPECT_LT(direct_elapsed, 3000)
        << "non-regular lock name must fail fast, never await the deadline";

    const auto write_start = std::chrono::steady_clock::now();
    auto written = loom::utils::write_to_mailbox(
        "worker",
        loom::utils::TeammateMessage{
            .from = "lead",
            .text = "must-not-land",
            .timestamp = "2",
            .read = false,
            .color = std::nullopt,
            .summary = std::nullopt,
        },
        std::optional<std::string_view>{"c14team"});
    const auto write_elapsed = c14_elapsed_ms(write_start);
    ASSERT_FALSE(written.has_value());
    EXPECT_NE(written.error().find("lock"), std::string::npos)
        << written.error();
    EXPECT_LT(write_elapsed, 3000)
        << "guarded writer must fail fast on a non-regular lock";
    EXPECT_FALSE(fs::exists(inbox))
        << "failed lock acquisition must bail before any inbox write";
    std::error_code ec;
    EXPECT_TRUE(fs::is_fifo(lock_path, ec))
        << "the FIFO itself must never be unlinked or replaced";
}


// ===========================================================================
// RFC-0001 B followup c16 — atomic, symlink/FIFO-safe team-data replaces;
// shared-lock readers; per-path grants mutex shards.
// ===========================================================================


// The replace primitive itself: new files follow umask, rewrites preserve
// the existing mode, OwnerOnly forces 0600, payload bytes are exact, and no
// tmp debris remains after success.

TEST(AtomicReplaceC16, ModePolicyBytesAndNoDebris) {
    ::umask(0022);
    C14TeamEnv env;
    const auto path = env.root / "data.json";

    ASSERT_TRUE(loom::utils::atomic_replace_file(path, "[]").has_value());
    EXPECT_EQ(c14_read_file(path), "[]");
    EXPECT_EQ(c16_mode_bits(path), 0644u);

    ASSERT_TRUE(loom::utils::atomic_replace_file(path, "[1]").has_value());
    EXPECT_EQ(c14_read_file(path), "[1]");
    EXPECT_EQ(c16_mode_bits(path), 0644u);

    ASSERT_EQ(::chmod(path.string().c_str(), 0600), 0);
    ASSERT_TRUE(loom::utils::atomic_replace_file(path, "[2]").has_value());
    EXPECT_EQ(c16_mode_bits(path), 0600u)
        << "PreserveOrUmask must keep a pre-existing 0600 mode";

    ASSERT_EQ(::chmod(path.string().c_str(), 0640), 0);
    ASSERT_TRUE(loom::utils::atomic_replace_file(path, "[3]").has_value());
    EXPECT_EQ(c16_mode_bits(path), 0640u)
        << "PreserveOrUmask must keep a pre-existing 0640 mode";

    ASSERT_TRUE(loom::utils::atomic_replace_file(
        path, "[4]", loom::utils::AtomicMode::OwnerOnly).has_value());
    EXPECT_EQ(c16_mode_bits(path), 0600u)
        << "OwnerOnly must force 0600 regardless of the prior mode";
    EXPECT_EQ(c14_read_file(path), "[4]");
    EXPECT_TRUE(c16_no_tmp_debris(env.root));
}


// Symlinked data leaves (to a canary, or dangling) and a FIFO at the data
// name are refused SPECIFICALLY and fast: no follow, no clobber, no block.

TEST(AtomicReplaceC16, SymlinkDanglingAndFifoLeavesRefusedFast) {
    C14TeamEnv env;
    const auto path = env.root / "data.json";
    const auto canary = env.root / "canary.txt";
    constexpr std::string_view kCanary = "c16-canary-7741";
    { std::ofstream out(canary, std::ios::binary); out << kCanary; }

    // Symlink to a real victim.
    fs::create_symlink(canary, path);
    auto t0 = std::chrono::steady_clock::now();
    auto result = loom::utils::atomic_replace_file(path, "[]");
    auto elapsed = c14_elapsed_ms(t0);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("symbolic link"), std::string::npos)
        << result.error();
    EXPECT_LT(elapsed, 2000);
    EXPECT_TRUE(fs::is_symlink(path));
    EXPECT_EQ(c14_read_file(canary), std::string(kCanary));

    // Dangling symlink.
    std::error_code ec;
    fs::remove(path, ec);
    fs::create_symlink(env.root / "missing", path);
    result = loom::utils::atomic_replace_file(path, "[]");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("symbolic link"), std::string::npos)
        << result.error();
    EXPECT_TRUE(fs::is_symlink(path));
    EXPECT_FALSE(fs::exists(path, ec)) << "the dangling target stays uncreated";

    // FIFO at the data name: the lstat gate refuses before any open(), so
    // the O_RDWR/FIFO blocking rule never comes into play.
    fs::remove(path, ec);
    ASSERT_EQ(::mkfifo(path.string().c_str(), 0600), 0);
    t0 = std::chrono::steady_clock::now();
    result = loom::utils::atomic_replace_file(path, "[]");
    elapsed = c14_elapsed_ms(t0);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("not a regular file"), std::string::npos)
        << result.error();
    EXPECT_LT(elapsed, 2000) << "a FIFO leaf must never block the writer";
    EXPECT_TRUE(fs::is_fifo(path, ec));
}


// BEFORE/AFTER measurement. The same 8-process rewrite storm, read 4-way:
//   * raw truncating writers + unlocked reads (the pre-c16 hazard) measure
//     a non-zero torn/parse-failure rate and log it;
//   * the production c16 writers (LOCK_EX + unique-tmp atomic replace) with
//     the production LOCK_SH readers must show ZERO torn reads across at
//     least 100 000 production read attempts, and finish with valid data.

TEST(TeamDataC16, StormReadersMeasureZeroTornAcrossInboxAndTeamFile) {
    constexpr int kWriters = 8;
    constexpr std::uint64_t kMinReads = 100'000;

    // ── Baseline: old-style truncating storm (measurement only) ──────────
    {
        C14TeamEnv env;
        const auto path = c14_inbox_path("raw", "c16rawteam");
        fs::create_directories(path.parent_path());
        const std::string whole =
            "[{\"from\":\"a\",\"text\":\"" + std::string(220, 'x') +
            "\",\"timestamp\":\"1\",\"read\":false}]";
        const std::string h1 = whole.substr(0, whole.size() / 2);
        const std::string h2 = whole.substr(whole.size() / 2);
        {
            std::ofstream seed(path, std::ios::binary);
            seed << whole;
        }
        constexpr int kRawMs = 1500;
        pid_t pids[kWriters];
        for (int w = 0; w < kWriters; ++w) {
            const pid_t pid = ::fork();
            if (pid < 0) _exit(2);
            if (pid == 0) {
                c16_raw_truncating_writer(path, h1, h2, kRawMs);
            }
            pids[w] = pid;
        }
        C16TornCounters raw;
        std::atomic<bool> stop{false};
        std::vector<std::thread> readers;
        for (int r = 0; r < 4; ++r) {
            readers.emplace_back([&] {
                c16_raw_array_reader(path, raw, &stop,
                                     std::numeric_limits<std::uint64_t>::max());
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kRawMs));
        stop.store(true, std::memory_order_relaxed);
        for (auto& t : readers) t.join();
        for (pid_t pid : pids) {
            int status = 0;
            ASSERT_EQ(::waitpid(pid, &status, 0), pid);
            ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0);
        }
        const auto attempts = raw.attempts.load();
        const auto torn = raw.torn.load();
        const double rate = attempts
            ? (100.0 * static_cast<double>(torn) /
               static_cast<double>(attempts))
            : 0.0;
        std::printf(
            "[c16] PRE-FIX raw truncating storm: %llu torn / %llu reads "
            "(%.2f%%) — the transient parse-failure window c16 removes\n",
            static_cast<unsigned long long>(torn),
            static_cast<unsigned long long>(attempts), rate);
        // Informational baseline: never flake the suite if the window was
        // not sampled on a fast scheduler; the post-fix assertion is the
        // load-bearing one below.
        EXPECT_GT(attempts, 1000u);
    }

    // ── Production c16 storm: locked atomic writers, SH readers ──────────
    C14TeamEnv env;
    const std::string team = "c16storm";
    const auto inbox = c14_inbox_path("worker", team);
    // Seed both files so a read never legitimately returns empty/absent.
    {
        loom::utils::TeammateMessage seed{
            .from = "lead", .text = "seed", .timestamp = "0", .read = false, .color = std::nullopt, .summary = std::nullopt};
        ASSERT_TRUE(loom::utils::detail::write_messages(inbox, {seed}));
        loom::utils::TeamFileRecord record;
        record.name = team;
        record.lead_agent_id = "team-lead@" + team;
        ASSERT_TRUE(loom::utils::write_team_file(team, record));
    }

    constexpr int kStormMs = 6000;
    pid_t pids[kWriters];
    for (int w = 0; w < kWriters; ++w) {
        const pid_t pid = ::fork();
        if (pid < 0) _exit(2);
        if (pid == 0) {
            c16_storm_writer_child(
                (env.root / "teams").string(), team, "worker", kStormMs);
        }
        pids[w] = pid;
    }

    C16TornCounters inbox_counts;
    C16TornCounters team_counts;
    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&] {
            while (!stop.load(std::memory_order_relaxed)) {
                auto msgs = loom::utils::read_inbox(
                    "worker", std::optional<std::string_view>{team});
                inbox_counts.attempts.fetch_add(1, std::memory_order_relaxed);
                if (!msgs || msgs->empty()) {
                    inbox_counts.torn.fetch_add(1,
                        std::memory_order_relaxed);
                }
                // Leave EX writers real lock windows (production polls on
                // a 1.5 s cadence); a non-yielding SH spin can starve
                // flock-EX writers up to their bounded wait.
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
    for (int r = 0; r < 4; ++r) {
        readers.emplace_back([&] {
            while (!stop.load(std::memory_order_relaxed)) {
                auto file = loom::utils::read_team_file(team);
                team_counts.attempts.fetch_add(1, std::memory_order_relaxed);
                if (!file) team_counts.torn.fetch_add(1,
                    std::memory_order_relaxed);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
    // Reap the storm writers first (they self-exit after kStormMs). The
    // LOCK_SH readers keep running; once the writers are gone the readers
    // are uncontended, so the 100k floor is reached deterministically
    // regardless of machine speed or load (the torn count is what the
    // storm-under-contention phase proves; extra uncontended reads simply
    // keep being torn-free).
    for (pid_t pid : pids) {
        int status = 0;
        ASSERT_EQ(::waitpid(pid, &status, 0), pid);
        ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0)
            << "storm writer failed: " << WEXITSTATUS(status);
    }
    const auto read_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < read_deadline) {
        const std::uint64_t total =
            inbox_counts.attempts.load() + team_counts.attempts.load();
        if (total >= kMinReads) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    stop.store(true, std::memory_order_relaxed);
    for (auto& t : readers) t.join();

    const auto inbox_attempts = inbox_counts.attempts.load();
    const auto team_attempts = team_counts.attempts.load();
    std::printf(
        "[c16] POST-FIX locked atomic storm: inbox %llu torn / %llu reads; "
        "team file %llu torn / %llu reads (0 expected)\n",
        static_cast<unsigned long long>(inbox_counts.torn.load()),
        static_cast<unsigned long long>(inbox_attempts),
        static_cast<unsigned long long>(team_counts.torn.load()),
        static_cast<unsigned long long>(team_attempts));
    EXPECT_GE(inbox_attempts + team_attempts, kMinReads)
        << "storm must exercise at least 100k production reads";
    EXPECT_EQ(inbox_counts.torn.load(), 0u);
    EXPECT_EQ(team_counts.torn.load(), 0u);

    // Final content is valid.
    auto final_inbox = loom::utils::read_inbox(
        "worker", std::optional<std::string_view>{team});
    ASSERT_TRUE(final_inbox.has_value());
    EXPECT_FALSE(final_inbox->empty());
    auto final_team = loom::utils::read_team_file(team);
    ASSERT_TRUE(final_team.has_value());
    EXPECT_EQ(final_team->name, team);
    EXPECT_EQ(final_team->members.size(), 2u);
    EXPECT_TRUE(c16_no_tmp_debris(inbox.parent_path()));
    EXPECT_TRUE(c16_no_tmp_debris(
        fs::path{loom::utils::team_file_path(team)}.parent_path()));
}


// Data-leaf attacks on the live inbox RMW path: a symlink (dangling or to
// a canary) or a FIFO at the inbox name makes write_to_mailbox fail clean,
// fast, and leaves the victim and the leaf untouched — never block, never
// follow, never clobber.

TEST(TeamDataC16, InboxLeafAttacksFailCleanFastWithoutFollowing) {
    struct Case { const char* name; int kind; };  // 0 symlink, 1 dangling, 2 fifo
    for (const auto& tc : std::array<Case, 3>{
            Case{"symlink", 0}, Case{"dangling", 1}, Case{"fifo", 2}}) {
        C14TeamEnv env;
        const auto inbox = c14_inbox_path("worker", "c16attack");
        fs::create_directories(inbox.parent_path());
        const auto canary = env.root / "canary.txt";
        constexpr std::string_view kCanary = "c16-inbox-canary-3180";

        if (tc.kind == 0) {
            { std::ofstream out(canary, std::ios::binary); out << kCanary; }
            fs::create_symlink(canary, inbox);
        } else if (tc.kind == 1) {
            fs::create_symlink(env.root / "gone", inbox);
        } else {
            ASSERT_EQ(::mkfifo(inbox.string().c_str(), 0600), 0);
        }

        const auto t0 = std::chrono::steady_clock::now();
        auto written = loom::utils::write_to_mailbox(
            "worker",
            loom::utils::TeammateMessage{
                .from = "lead",
                .text = "must-not-land",
                .timestamp = "9",
                .read = false,
                .color = std::nullopt,
                .summary = std::nullopt,
            },
            std::optional<std::string_view>{"c16attack"});
        const auto elapsed = c14_elapsed_ms(t0);
        ASSERT_FALSE(written.has_value()) << tc.name;
        EXPECT_LT(elapsed, 3000)
            << tc.name << ": non-regular leaf must fail fast, never block";

        std::error_code ec;
        if (tc.kind == 0) {
            EXPECT_TRUE(fs::is_symlink(inbox, ec)) << tc.name;
            EXPECT_EQ(c14_read_file(canary), std::string(kCanary)) << tc.name;
        } else if (tc.kind == 1) {
            EXPECT_TRUE(fs::is_symlink(inbox, ec)) << tc.name;
        } else {
            EXPECT_TRUE(fs::is_fifo(inbox, ec)) << tc.name;
        }
        // The unlocked/locked reads also fail safe (empty) and never block.
        auto msgs = loom::utils::read_inbox(
            "worker", std::optional<std::string_view>{"c16attack"});
        ASSERT_TRUE(msgs.has_value()) << tc.name;
        EXPECT_TRUE(msgs->empty()) << tc.name;
        EXPECT_TRUE(c16_no_tmp_debris(inbox.parent_path())) << tc.name;
    }
}


// The same leaf gate for the canonical team config.json writer.

TEST(TeamDataC16, TeamFileLeafAttacksFailCleanFast) {
    C14TeamEnv env;
    const fs::path path{loom::utils::team_file_path("c16tattack")};
    fs::create_directories(path.parent_path());
    const auto canary = env.root / "canary.txt";
    constexpr std::string_view kCanary = "c16-team-canary-6204";

    loom::utils::TeamFileRecord record;
    record.name = "c16tattack";
    record.lead_agent_id = "team-lead@c16tattack";

    { std::ofstream out(canary, std::ios::binary); out << kCanary; }
    fs::create_symlink(canary, path);
    auto t0 = std::chrono::steady_clock::now();
    EXPECT_FALSE(loom::utils::write_team_file("c16tattack", record));
    EXPECT_LT(c14_elapsed_ms(t0), 3000);
    EXPECT_TRUE(fs::is_symlink(path));
    EXPECT_EQ(c14_read_file(canary), std::string(kCanary));

    std::error_code ec;
    fs::remove(path, ec);
    ASSERT_EQ(::mkfifo(path.string().c_str(), 0600), 0);
    t0 = std::chrono::steady_clock::now();
    EXPECT_FALSE(loom::utils::write_team_file("c16tattack", record));
    EXPECT_LT(c14_elapsed_ms(t0), 3000)
        << "a FIFO config leaf must never block the writer";
    EXPECT_TRUE(fs::is_fifo(path, ec));
}


// Mode preservation across the live inbox and team-config writers, and the
// 0644-under-022 mode for brand-new files.

TEST(TeamDataC16, InboxAndTeamFileModesPreservedAcrossRewrites) {
    ::umask(0022);
    C14TeamEnv env;
    const auto inbox = c14_inbox_path("worker", "c16mode");

    loom::utils::TeammateMessage m{
        .from = "lead", .text = "one", .timestamp = "1", .read = false, .color = std::nullopt, .summary = std::nullopt};
    ASSERT_TRUE(loom::utils::write_to_mailbox(
        "worker", m, std::optional<std::string_view>{"c16mode"}).has_value());
    EXPECT_EQ(c16_mode_bits(inbox), 0644u);

    ASSERT_EQ(::chmod(inbox.string().c_str(), 0600), 0);
    m.text = "two";
    m.timestamp = "2";
    ASSERT_TRUE(loom::utils::write_to_mailbox(
        "worker", m, std::optional<std::string_view>{"c16mode"}).has_value());
    EXPECT_EQ(c16_mode_bits(inbox), 0600u)
        << "inbox rewrite must preserve a pre-existing 0600 mode";

    ASSERT_EQ(::chmod(inbox.string().c_str(), 0640), 0);
    m.text = "three";
    m.timestamp = "3";
    ASSERT_TRUE(loom::utils::write_to_mailbox(
        "worker", m, std::optional<std::string_view>{"c16mode"}).has_value());
    EXPECT_EQ(c16_mode_bits(inbox), 0640u)
        << "inbox rewrite must preserve a pre-existing 0640 mode";

    const fs::path config{loom::utils::team_file_path("c16mode")};
    loom::utils::TeamFileRecord record;
    record.name = "c16mode";
    record.lead_agent_id = "team-lead@c16mode";
    ASSERT_TRUE(loom::utils::write_team_file("c16mode", record));
    EXPECT_EQ(c16_mode_bits(config), 0644u);
    ASSERT_EQ(::chmod(config.string().c_str(), 0600), 0);
    record.created_at = 7;
    ASSERT_TRUE(loom::utils::write_team_file("c16mode", record));
    EXPECT_EQ(c16_mode_bits(config), 0600u)
        << "team config rewrite must preserve a pre-existing 0600 mode";
}


// Shared-lock reader semantics: while an exclusive holder owns the inbox
// sibling, read_inbox fails closed (empty) at the ~10s bound (it never
// sees partial bytes), and many concurrent LOCK_SH readers on an
// uncontended inbox all succeed simultaneously.

TEST(ScopedInboxLockC16, SharedReaderFailsClosedAndSharedReadersConcur) {
    C14TeamEnv env;
    const auto inbox = c14_inbox_path("worker", "c16sh");
    fs::create_directories(inbox.parent_path());
    const std::string canary =
        R"JSON([{"from":"lead","text":"keep","timestamp":"1","read":true}])JSON";
    { std::ofstream out(inbox, std::ios::binary); out << canary; }

    const std::string lock_path = inbox.string() + ".lock";
    C14Holder h = c14_spawn_holder(lock_path, 30);
    ASSERT_GT(h.pid, 0);
    C14HolderReaper reaper(h.pid);
    c14_await_ready(h.read_fd);

    struct ReadResult {
        bool has_value = false;
        bool empty = true;
        long long elapsed_ms = 0;
    };
    ReadResult reader;
    std::thread contended([&] {
        const auto t0 = std::chrono::steady_clock::now();
        auto msgs = loom::utils::read_inbox(
            "worker", std::optional<std::string_view>{"c16sh"});
        reader.elapsed_ms = c14_elapsed_ms(t0);
        reader.has_value = msgs.has_value();
        reader.empty = msgs && msgs->empty();
    });

    // Meanwhile, 8 concurrent LOCK_SH readers on a DIFFERENT, uncontended
    // inbox must all succeed at once (shared locks do not exclude each
    // other).
    const auto other = c14_inbox_path("worker", "c16sh-other");
    fs::create_directories(other.parent_path());
    {
        loom::utils::TeammateMessage seed{
            .from = "lead", .text = "hi", .timestamp = "1", .read = false, .color = std::nullopt, .summary = std::nullopt};
        ASSERT_TRUE(loom::utils::detail::write_messages(other, {seed}));
    }
    std::atomic<int> shared_ok{0};
    std::vector<std::thread> shared_readers;
    for (int i = 0; i < 8; ++i) {
        shared_readers.emplace_back([&] {
            loom::utils::ScopedInboxLock lock(
                other, loom::utils::LockKind::Shared);
            if (lock.locked()) shared_ok.fetch_add(1);
        });
    }
    for (auto& t : shared_readers) t.join();
    EXPECT_EQ(shared_ok.load(), 8)
        << "concurrent shared locks must all be granted";

    contended.join();
    EXPECT_TRUE(reader.has_value);
    EXPECT_TRUE(reader.empty)
        << "reader must fail closed to empty, never return partial bytes";
    EXPECT_GE(reader.elapsed_ms, 9000)
        << "shared acquisition shares the 10s bounded wait";
    EXPECT_LE(reader.elapsed_ms, 14000);

    // The contended read never parsed partial bytes: the canary is intact.
    EXPECT_EQ(c14_read_file(inbox), canary);

    ::kill(h.pid, SIGKILL);
    int status = 0;
    ASSERT_EQ(::waitpid(h.pid, &status, 0), h.pid);
    reaper.release();
}


// ===========================================================================
// RFC-0001 B followup c16a — hardened fd-based team-data reader
// (read_regular_file): no stat-gate-then-reopen TOCTOU.
// ===========================================================================



// The primitive itself: exact bytes for a regular file, Absent for a
// missing path, and INSTANT Unreadable (never blocked, never followed)
// for a FIFO, a trailing symlink, or a directory at the leaf.

TEST(ReadRegularFileC16a, ShapesPresentAbsentFifoSymlinkDirectory) {
    C14TeamEnv env;

    // Regular file: byte-identical contents.
    const auto regular = env.root / "regular.json";
    constexpr std::string_view kBytes =
        "{\"team\":\"\xc3\xa9\",\"n\":7}\n";  // UTF-8 + trailing newline
    { std::ofstream out(regular, std::ios::binary); out << kBytes; }
    {
        const auto r = loom::utils::read_regular_file(regular);
        EXPECT_EQ(r.status, loom::utils::RegularReadStatus::Present);
        EXPECT_EQ(r.contents, std::string(kBytes));
    }

    // Absent.
    {
        const auto r = loom::utils::read_regular_file(env.root / "missing.json");
        EXPECT_EQ(r.status, loom::utils::RegularReadStatus::Absent);
        EXPECT_TRUE(r.contents.empty());
    }

    // FIFO with no writer peer: O_NONBLOCK open returns immediately and
    // fstat rejects it; the call must never block.
    const auto fifo = env.root / "fifo.json";
    ASSERT_EQ(::mkfifo(fifo.string().c_str(), 0600), 0);
    {
        const auto t0 = std::chrono::steady_clock::now();
        const auto r = loom::utils::read_regular_file(fifo);
        const auto elapsed = c16a_elapsed_ms(t0);
        EXPECT_EQ(r.status, loom::utils::RegularReadStatus::Unreadable);
        EXPECT_LT(elapsed, 100) << "FIFO leaf must be rejected instantly";
        std::error_code ec;
        EXPECT_TRUE(fs::is_fifo(fifo, ec));
    }

    // Trailing symlink: rejected without touching the canary target.
    const auto canary = env.root / "canary.txt";
    constexpr std::string_view kCanary = "c16a-read-canary-9041";
    { std::ofstream out(canary, std::ios::binary); out << kCanary; }
    const auto link = env.root / "link.json";
    fs::create_symlink(canary, link);
    {
        const auto r = loom::utils::read_regular_file(link);
        EXPECT_EQ(r.status, loom::utils::RegularReadStatus::Unreadable);
        EXPECT_TRUE(r.contents.empty());
        EXPECT_EQ(c14_read_file(canary), std::string(kCanary))
            << "symlink target must never be opened";
        std::error_code ec;
        EXPECT_TRUE(fs::is_symlink(link, ec));
    }

    // Directory leaf: open may succeed, fstat rejects.
    const auto dir = env.root / "adir";
    fs::create_directories(dir);
    {
        const auto r = loom::utils::read_regular_file(dir);
        EXPECT_EQ(r.status, loom::utils::RegularReadStatus::Unreadable);
    }
}


// The inbox reader: FIFO/symlink swaps at the data leaf give the ordinary
// empty inbox INSTANTLY (and never follow the link), an absent file is
// empty, and a regular inbox array parses identically.

TEST(TeamDataC16a, InboxReaderSwapLeavesFailFastAndRegularParses) {
    C14TeamEnv env;
    const auto inbox = c14_inbox_path("worker", "c16ainbox");
    fs::create_directories(inbox.parent_path());

    // Absent: historical empty inbox.
    {
        auto msgs = loom::utils::read_inbox(
            "worker", std::optional<std::string_view>{"c16ainbox"});
        ASSERT_TRUE(msgs.has_value());
        EXPECT_TRUE(msgs->empty());
    }

    // FIFO at the leaf: empty result, under 100ms, FIFO untouched.
    ASSERT_EQ(::mkfifo(inbox.string().c_str(), 0600), 0);
    {
        const auto t0 = std::chrono::steady_clock::now();
        auto msgs = loom::utils::read_inbox(
            "worker", std::optional<std::string_view>{"c16ainbox"});
        const auto elapsed = c16a_elapsed_ms(t0);
        ASSERT_TRUE(msgs.has_value());
        EXPECT_TRUE(msgs->empty());
        EXPECT_LT(elapsed, 100) << "FIFO inbox read must never block";
        std::error_code ec;
        EXPECT_TRUE(fs::is_fifo(inbox, ec));
    }

    // Symlink at the leaf: empty result, canary target never opened.
    const auto canary = env.root / "inbox-canary.txt";
    constexpr std::string_view kCanary = "c16a-inbox-canary-7723";
    {
        std::error_code ec;
        fs::remove(inbox, ec);
        std::ofstream out(canary, std::ios::binary);
        out << kCanary;
    }
    fs::create_symlink(canary, inbox);
    {
        auto msgs = loom::utils::read_inbox(
            "worker", std::optional<std::string_view>{"c16ainbox"});
        ASSERT_TRUE(msgs.has_value());
        EXPECT_TRUE(msgs->empty());
        EXPECT_EQ(c14_read_file(canary), std::string(kCanary));
    }

    // Regular array: parsed byte-identically through the fd buffer.
    {
        std::error_code ec;
        fs::remove(inbox, ec);
        ASSERT_TRUE(loom::utils::write_to_mailbox(
            "worker",
            loom::utils::TeammateMessage{
                .from = "lead",
                .text = "hello-c16a",
                .timestamp = "42",
                .read = false,
                .color = std::optional<std::string>{"cyan"},
                .summary = std::nullopt,
            },
            std::optional<std::string_view>{"c16ainbox"})
                        .has_value());
        auto msgs = loom::utils::read_inbox(
            "worker", std::optional<std::string_view>{"c16ainbox"});
        ASSERT_TRUE(msgs.has_value());
        ASSERT_EQ(msgs->size(), 1u);
        EXPECT_EQ((*msgs)[0].from, "lead");
        EXPECT_EQ((*msgs)[0].text, "hello-c16a");
        EXPECT_EQ((*msgs)[0].timestamp, "42");
        EXPECT_FALSE((*msgs)[0].read);
        ASSERT_TRUE((*msgs)[0].color.has_value());
        EXPECT_EQ(*(*msgs)[0].color, "cyan");
    }
}


// The canonical team config.json reader: the same four shapes.

TEST(TeamDataC16a, TeamFileReaderSwapLeavesFailFastAndRegularParses) {
    C14TeamEnv env;
    const fs::path path{loom::utils::team_file_path("c16ateam")};
    fs::create_directories(path.parent_path());

    // Absent: nullopt.
    EXPECT_FALSE(loom::utils::read_team_file("c16ateam").has_value());

    // FIFO: nullopt instantly.
    ASSERT_EQ(::mkfifo(path.string().c_str(), 0600), 0);
    {
        const auto t0 = std::chrono::steady_clock::now();
        auto rec = loom::utils::read_team_file("c16ateam");
        const auto elapsed = c16a_elapsed_ms(t0);
        EXPECT_FALSE(rec.has_value());
        EXPECT_LT(elapsed, 100) << "FIFO team file must never block";
        std::error_code ec;
        EXPECT_TRUE(fs::is_fifo(path, ec));
    }

    // Symlink: nullopt, canary untouched.
    const auto canary = env.root / "team-canary.txt";
    constexpr std::string_view kCanary = "c16a-team-canary-5188";
    {
        std::error_code ec;
        fs::remove(path, ec);
        std::ofstream out(canary, std::ios::binary);
        out << kCanary;
    }
    fs::create_symlink(canary, path);
    {
        auto rec = loom::utils::read_team_file("c16ateam");
        EXPECT_FALSE(rec.has_value());
        EXPECT_EQ(c14_read_file(canary), std::string(kCanary));
    }

    // Regular record: identical parse.
    {
        std::error_code ec;
        fs::remove(path, ec);
        loom::utils::TeamFileRecord record;
        record.name = "c16ateam";
        record.description = std::optional<std::string>{"the c16a team"};
        record.created_at = 123;
        record.lead_agent_id = "team-lead@c16ateam";
        ASSERT_TRUE(loom::utils::write_team_file("c16ateam", record));
        auto rec = loom::utils::read_team_file("c16ateam");
        ASSERT_TRUE(rec.has_value());
        EXPECT_EQ(rec->name, "c16ateam");
        EXPECT_EQ(rec->lead_agent_id, "team-lead@c16ateam");
        EXPECT_EQ(rec->created_at, 123);
        ASSERT_TRUE(rec->description.has_value());
        EXPECT_EQ(*rec->description, "the c16a team");
    }
}

// End of file
