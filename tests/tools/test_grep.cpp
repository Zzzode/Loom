#include <gtest/gtest.h>
#include <cstdlib>
#include <uv.h>
#ifndef _WIN32
#include <signal.h>
#include <unistd.h>
#endif

import std;
import loom.tools.grep;
import loom.tools.tool;
import loom.query.query_engine;
import loom.types.types;
import loom.serdes.json;

namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        auto pattern = (fs::temp_directory_path() / "loom-grep-XXXXXX").string();
        uv_fs_t request{};
        const int rc = uv_fs_mkdtemp(nullptr, &request, pattern.c_str(), nullptr);
        if (rc == 0) path = request.path;
        uv_fs_req_cleanup(&request);
        if (rc < 0) throw std::runtime_error(uv_strerror(rc));
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
    void write(const fs::path& name, std::string_view data) {
        fs::create_directories((path / name).parent_path());
        std::ofstream file(path / name, std::ios::binary);
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
};

struct EnvironmentGuard {
    std::string key;
    std::optional<std::string> old;
    EnvironmentGuard(std::string name, const std::string& value) : key(std::move(name)) {
        if (const auto* previous = std::getenv(key.c_str())) old = previous;
        uv_os_setenv(key.c_str(), value.c_str());
    }
    ~EnvironmentGuard() {
        if (old) uv_os_setenv(key.c_str(), old->c_str());
        else uv_os_unsetenv(key.c_str());
    }
};

std::string quote(std::string_view value) {
    loom::utils::json::JsonMutDoc doc;
    doc.set_root(doc.string(value));
    auto quoted = doc.to_string();
    return quoted.substr(1, quoted.size() - 2);
}

loom::core::ToolInput search_input(const fs::path& path, std::string_view pattern = "needle", std::string_view options = "") {
    return loom::core::ToolInput::from_json(std::format(
        R"({{"pattern":"{}","path":"{}"{}}})", quote(pattern), quote(path.string()), options));
}

class GrepTest : public testing::Test {
protected:
    TempDir dir;
    loom::core::ToolRegistry registry;
    void SetUp() override { registry.register_tool(loom::tools::make_grep_tool()); }
    loom::core::ToolResult search(const fs::path& path, std::string_view pattern = "needle", std::string_view options = "") {
        auto result = registry.execute("Grep", search_input(path, pattern, options));
        if (!result) throw std::runtime_error(result.error().format());
        return std::move(*result);
    }
    void fake_rg(std::string_view mode) {
        std::array<char, 4096> executable{};
        auto size = executable.size();
        const int rc = uv_exepath(executable.data(), &size);
        if (rc < 0) throw std::runtime_error(uv_strerror(rc));
        fs::create_directories(dir.path / "bin");
        std::ofstream config(dir.path / "fixture-mode");
        config << mode;
        config.close();
        if (!config) throw std::runtime_error("cannot write fake ripgrep mode");
        fs::remove(dir.path / "bin/rg");
        fs::create_symlink(fs::path(std::string(executable.data(), size)), dir.path / "bin/rg");
    }
    bool wait_for_pid() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            if (fs::exists(dir.path / "pid")) {
                std::ifstream pid_file(dir.path / "pid");
                int pid = 0;
                if (pid_file >> pid) return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }
    void expect_reaped() {
#ifndef _WIN32
        std::ifstream pid_file(dir.path / "pid");
        int pid = 0;
        ASSERT_TRUE(static_cast<bool>(pid_file >> pid));
        EXPECT_EQ(::kill(pid, 0), -1) << "search child must be terminated and reaped";
#endif
    }
    void stalled_rg() {
        fake_rg("stall");
    }
};

TEST_F(GrepTest, ModesAndPagination) {
    dir.write("a.cpp", "needle one\nneedle two\nother\n");
    dir.write("b.txt", "needle three\n");
    auto content = search(dir.path);
    ASSERT_FALSE(content.is_error);
    EXPECT_NE(content.content.front().text.find("a.cpp:1:needle one"), std::string::npos);
    auto files = search(dir.path, "needle", R"(,"output_mode":"files_with_matches")");
    ASSERT_FALSE(files.is_error);
    EXPECT_NE(files.content.front().text.find("a.cpp"), std::string::npos);
    EXPECT_EQ(files.content.front().text.find("needle"), std::string::npos);
    auto counts = search(dir.path, "needle", R"(,"output_mode":"count")");
    ASSERT_FALSE(counts.is_error);
    EXPECT_NE(counts.content.front().text.find("a.cpp:2\n"), std::string::npos);
    EXPECT_NE(counts.content.front().text.find("b.txt:1\n"), std::string::npos);
    auto page = search(dir.path, "needle", R"(,"head_limit":1,"offset":1)");
    ASSERT_FALSE(page.is_error);
    EXPECT_EQ(page.content.front().text.find("needle one"), std::string::npos);
    EXPECT_NE(page.content.front().text.find("needle two"), std::string::npos);
    EXPECT_NE(page.content.front().text.find("truncated"), std::string::npos);
    auto past_end = search(dir.path, "needle", R"(,"offset":100)");
    ASSERT_FALSE(past_end.is_error);
    EXPECT_NE(past_end.content.front().text.find("No entries at this offset"), std::string::npos);
}

TEST_F(GrepTest, RespectsIgnoreFilesAndSkipsBuildBinariesAndHiddenFiles) {
    // gitignore must work even in scratch directories without a .git folder.
    dir.write(".gitignore", "build/\nignored.txt\n");
    dir.write(".ignore", "also_ignored.txt\n");
    dir.write("source.cpp", "needle\n");
    dir.write("ignored.txt", "needle\n");
    dir.write("also_ignored.txt", "needle\n");
    dir.write(".hidden", "needle\n");
    std::string binary(2 * 1024 * 1024, '\0');
    binary += "needle\n";
    dir.write("build/large.o", binary);
    dir.write("binary.dat", binary);
    const auto start = std::chrono::steady_clock::now();
    auto result = search(dir.path, "needle", R"(,"output_mode":"files_with_matches")");
    ASSERT_FALSE(result.is_error);
    EXPECT_EQ(result.content.front().text, (dir.path / "source.cpp").string() + '\n');
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(3));
    auto explicit_file = search(dir.path / "ignored.txt");
    ASSERT_FALSE(explicit_file.is_error);
    EXPECT_NE(explicit_file.content.front().text.find("needle"), std::string::npos);
}

TEST_F(GrepTest, FiltersByGlobAndType) {
    dir.write("a.cpp", "needle\n");
    dir.write("b.py", "needle\n");
    for (const auto* option : {R"(,"glob":"**/*.cpp")", R"(,"type":"cpp")"}) {
        auto result = search(dir.path, "needle", option);
        ASSERT_FALSE(result.is_error);
        EXPECT_NE(result.content.front().text.find("a.cpp"), std::string::npos);
        EXPECT_EQ(result.content.front().text.find("b.py"), std::string::npos);
    }
}

TEST_F(GrepTest, ParallelInvocationsKeepTheirResultsSeparate) {
    dir.write("a.txt", "needle first\n");
    dir.write("b.txt", "needle second\n");
    auto first = std::async(std::launch::async, [&] { return search(dir.path / "a.txt"); });
    auto second = std::async(std::launch::async, [&] { return search(dir.path / "b.txt"); });
    auto a = first.get();
    auto b = second.get();
    ASSERT_FALSE(a.is_error);
    ASSERT_FALSE(b.is_error);
    EXPECT_NE(a.content.front().text.find("needle first"), std::string::npos);
    EXPECT_EQ(a.content.front().text.find("needle second"), std::string::npos);
    EXPECT_NE(b.content.front().text.find("needle second"), std::string::npos);
    EXPECT_EQ(b.content.front().text.find("needle first"), std::string::npos);
}

TEST_F(GrepTest, RepositoryWideSearchSkipsRealBuildTrees) {
    const auto repo = fs::path(__FILE__).parent_path().parent_path().parent_path();
    const auto start = std::chrono::steady_clock::now();
    auto result = search(repo, "rfc.*lint|lint.*rfc", R"(,"output_mode":"files_with_matches")");
    ASSERT_FALSE(result.is_error);
    EXPECT_NE(result.content.front().text.find("tools/rfc/rfc_lint.py"), std::string::npos);
    EXPECT_EQ(result.content.front().text.find("/build/"), std::string::npos);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(5));
}

TEST_F(GrepTest, SeparatesNoMatchesFromRegexPathAndTypeErrors) {
    dir.write("a.txt", "needle\n");
    auto empty = search(dir.path, "absent");
    ASSERT_FALSE(empty.is_error);
    EXPECT_EQ(empty.content.front().text, "No matches found.\n");
    auto bad_regex = search(dir.path, "[");
    ASSERT_TRUE(bad_regex.is_error);
    EXPECT_NE(bad_regex.content.front().text.find("regex"), std::string::npos);
    EXPECT_TRUE(search(dir.path / "missing").is_error);
    EXPECT_TRUE(search(dir.path, "needle", R"(,"type":"not_a_known_type")").is_error);
    for (const auto* option : {R"(,"output_mode":"bad")", R"(,"timeout_ms":0)",
            R"(,"timeout_ms":1.5)", R"(,"head_limit":-1)", R"(,"offset":100001)"})
        EXPECT_TRUE(search(dir.path, "needle", option).is_error);
}

TEST_F(GrepTest, OperandsCannotInjectOptionsOrShellCommands) {
    const auto filename = "a ';touch SHOULD_NOT_EXIST;'.txt";
    dir.write(filename, "--version\n$(touch SHOULD_NOT_EXIST)\n");
    auto result = search(dir.path / filename, "--version");
    ASSERT_FALSE(result.is_error);
    EXPECT_NE(result.content.front().text.find(":1:--version"), std::string::npos);
    auto shell = search(dir.path / filename, R"(\$\(touch SHOULD_NOT_EXIST\))");
    ASSERT_FALSE(shell.is_error);
    EXPECT_NE(shell.content.front().text.find("$(touch SHOULD_NOT_EXIST)"), std::string::npos);
    EXPECT_FALSE(fs::exists("SHOULD_NOT_EXIST"));
}

TEST_F(GrepTest, IgnoresUserRipgrepConfiguration) {
    dir.write("a.txt", "needle\n");
    dir.write("config", "--count\n--no-ignore\n");
    EnvironmentGuard config("RIPGREP_CONFIG_PATH", (dir.path / "config").string());
    auto result = search(dir.path / "a.txt");
    ASSERT_FALSE(result.is_error);
    EXPECT_NE(result.content.front().text.find(":1:needle"), std::string::npos);
}

TEST_F(GrepTest, MissingExecutableIsAnExplicitError) {
    EnvironmentGuard path("PATH", (dir.path / "missing-bin").string());
    auto result = search(dir.path);
    ASSERT_TRUE(result.is_error);
    EXPECT_NE(result.content.front().text.find("not found on PATH"), std::string::npos);
}

#ifndef _WIN32
// Native fixtures and PID reap assertions exercise POSIX process lifecycle.
TEST_F(GrepTest, SuccessfulSearchPreservesMatchesAndDiagnostics) {
    fake_rg("matches");
    EnvironmentGuard path("PATH", (dir.path / "bin").string());
    auto result = search(dir.path);
    ASSERT_FALSE(result.is_error);
    EXPECT_NE(result.content.front().text.find("file:1:needle"), std::string::npos);
    EXPECT_NE(result.content.front().text.find("warning"), std::string::npos);
}

TEST_F(GrepTest, DeadlineTerminatesAndReapsChild) {
    stalled_rg();
    EnvironmentGuard path("PATH", (dir.path / "bin").string());
    const auto start = std::chrono::steady_clock::now();
    // Allow the platform to start the child before testing deadline cleanup.
    auto result = search(dir.path, "needle", R"(,"timeout_ms":1000)");
    ASSERT_TRUE(result.is_error);
    EXPECT_NE(result.content.front().text.find("timed out"), std::string::npos);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(3));
    expect_reaped();
}

TEST_F(GrepTest, CancellationTerminatesAndReapsChild) {
    stalled_rg();
    EnvironmentGuard path("PATH", (dir.path / "bin").string());
    std::stop_source cancellation;
    auto input = search_input(dir.path);
    input.is_cancelled = [token = cancellation.get_token()] { return token.stop_requested(); };
    auto work = std::async(std::launch::async, [&] { return registry.execute("Grep", input); });
    EXPECT_TRUE(wait_for_pid());
    cancellation.request_stop();
    EXPECT_EQ(work.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    auto result = work.get();
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_error);
    EXPECT_NE(result->content.front().text.find("cancelled"), std::string::npos);
    expect_reaped();
}

TEST_F(GrepTest, PreCancelledSearchDoesNotSpawn) {
    stalled_rg();
    EnvironmentGuard path("PATH", (dir.path / "bin").string());
    auto input = search_input(dir.path);
    input.is_cancelled = [] { return true; };
    auto result = registry.execute("Grep", input);
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->is_error);
    EXPECT_FALSE(fs::exists(dir.path / "pid"));
}

TEST_F(GrepTest, OutputCaptureIsBoundedAndChildIsReaped) {
    fake_rg("flood");
    EnvironmentGuard path("PATH", (dir.path / "bin").string());
    auto result = search(dir.path);
    ASSERT_FALSE(result.is_error);
    EXPECT_LT(result.content.front().text.size(), 20000U);
    EXPECT_NE(result.content.front().text.find("truncated"), std::string::npos);
    expect_reaped();
}

TEST_F(GrepTest, DeadlineStillAppliesAfterStdoutCloses) {
    fake_rg("closed-stdout");
    EnvironmentGuard path("PATH", (dir.path / "bin").string());
    auto result = search(dir.path, "needle", R"(,"timeout_ms":1000)");
    ASSERT_TRUE(result.is_error);
    EXPECT_NE(result.content.front().text.find("timed out"), std::string::npos);
    expect_reaped();
}

// Drive the public query-engine tool entry point so cancellation wiring is
// exercised in addition to the Grep subprocess itself. No remote model needed.
TEST_F(GrepTest, QueryAbortShutdownAndExternalCancellationReachRunningSearch) {
    for (const auto* kind : {"abort", "shutdown", "external"}) {
        SCOPED_TRACE(kind);
        fs::remove(dir.path / "pid");
        stalled_rg();
        EnvironmentGuard path("PATH", (dir.path / "bin").string());
        loom::core::ToolRegistry tools;
        tools.register_tool(loom::tools::make_grep_tool());
        loom::core::QueryEngineConfig config;
        config.cwd = dir.path.string();
        config.context_window.auto_compact = false;
        loom::core::QueryEngine engine(std::move(config), tools);
        std::atomic<bool> external{false};
        engine.set_external_abort_callback([&] { return external.load(); });
        loom::core::ToolUseBlock call;
        call.id.value = "grep-cancellation";
        call.name = "Grep";
        call.input_json = std::string(search_input(dir.path).json());
        auto work = std::async(std::launch::async, [&] { return engine.execute_single_tool_for_testing(call); });
        EXPECT_TRUE(wait_for_pid());
        if (std::string_view(kind) == "abort") engine.abort();
        else if (std::string_view(kind) == "shutdown") engine.request_tool_shutdown();
        else external.store(true);
        EXPECT_EQ(work.wait_for(std::chrono::seconds(3)), std::future_status::ready);
        auto result = work.get();
        EXPECT_TRUE(result.is_error);
        EXPECT_EQ(result.tool_use_id.value, "grep-cancellation");
        ASSERT_FALSE(result.content.empty());
        EXPECT_NE(std::get<loom::core::TextBlock>(result.content.front()).text.find("cancelled"), std::string::npos);
        expect_reaped();
    }
}
#endif

#ifndef _WIN32
// Run the fake backend from the same native test executable image rather
// than adding a newly generated shell script to the process-startup contract.
int run_fake_ripgrep(const fs::path& directory) {
    std::ifstream config(directory / "fixture-mode");
    std::string mode;
    if (!(config >> mode)) return 2;
    if (mode == "matches") {
        std::cout << "file:1:needle\n" << std::flush;
        std::cerr << "warning\n" << std::flush;
        return std::cout && std::cerr ? 0 : 1;
    }
    if (mode != "stall" && mode != "flood" && mode != "closed-stdout") return 2;
    std::ofstream pid(directory / "pid");
    pid << ::getpid();
    pid.close();
    if (!pid) return 1;
    if (mode == "flood") {
        ::signal(SIGPIPE, SIG_DFL);
        while (std::cout << "file:1:needle needle needle\n") {}
        return 1;
    }
    if (mode == "closed-stdout") {
        const int stdout_result = ::close(STDOUT_FILENO);
        const int stderr_result = ::close(STDERR_FILENO);
        if (stdout_result != 0 || stderr_result != 0) return 1;
    }
    std::this_thread::sleep_for(std::chrono::seconds(60));
    return 0;
}
#endif

} // namespace

int main(int argc, char** argv) {
#ifndef _WIN32
    if (argc > 1 && fs::path(argv[0]).filename() == "rg")
        return run_fake_ripgrep(fs::path(argv[argc - 1]));
#endif
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
