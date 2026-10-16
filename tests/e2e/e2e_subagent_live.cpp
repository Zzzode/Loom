/// Opt-in live endpoint workflows through the real interactive terminal.
/// Each scenario owns scratch files and retains bounded, redacted evidence.
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

namespace {
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
std::string read_file(const fs::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), {}};
}
void write_file(const fs::path& path, const std::string& text) {
    std::ofstream output(path);
    require(static_cast<bool>(output << text), "Could not write scratch evidence");
}
long long timestamp() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
std::string json_escape(std::string_view text) {
    std::string result;
    for (unsigned char ch : text) {
        switch (ch) {
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        default:
            if (ch < 32) {
                constexpr char digits[] = "0123456789abcdef";
                result += "\\u00";
                result += digits[ch >> 4];
                result += digits[ch & 15];
            } else result += static_cast<char>(ch);
        }
    }
    return result;
}
std::string shell_quote(std::string_view text) {
    std::string result = "'";
    for (char ch : text) result += ch == '\'' ? "'\\''" : std::string(1, ch);
    return result + "'";
}

// Read JSON strings lexically so keys inside prompt strings cannot impersonate
// record status. Record fields needed here are strings or primitive booleans.
std::optional<std::string> json_string(std::string_view text, std::size_t& at) {
    if (at >= text.size() || text[at] != '"') return std::nullopt;
    ++at;
    std::string result;
    while (at < text.size()) {
        char ch = text[at++];
        if (ch == '"') return result;
        if (ch != '\\') { result += ch; continue; }
        if (at == text.size()) return std::nullopt;
        ch = text[at++];
        switch (ch) {
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 't': result += '\t'; break;
        case 'b': result += '\b'; break;
        case 'f': result += '\f'; break;
        case 'u':
            if (at + 4 > text.size()) return std::nullopt;
            result += "\\u";
            result.append(text.substr(at, 4));
            at += 4;
            break;
        default: result += ch; break;
        }
    }
    return std::nullopt;
}
std::string field(std::string_view text, std::string_view key) {
    int depth = 0;
    for (std::size_t at = 0; at < text.size();) {
        if (text[at] == '{' || text[at] == '[') { ++depth; ++at; continue; }
        if (text[at] == '}' || text[at] == ']') { --depth; ++at; continue; }
        if (text[at] != '"') { ++at; continue; }
        const auto token = json_string(text, at);
        if (!token) return {};
        if (depth != 1 || *token != key) continue;
        while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at;
        if (at == text.size() || text[at++] != ':') continue;
        while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at;
        if (at < text.size() && text[at] == '"') return json_string(text, at).value_or("");
        const auto start = at;
        while (at < text.size() && text[at] != ',' && text[at] != '}' &&
               !std::isspace(static_cast<unsigned char>(text[at]))) ++at;
        return std::string(text.substr(start, at - start));
    }
    return {};
}

class Redactor {
public:
    Redactor() {
        for (const char* name : {"LOOM_API_KEY", "LOOM_AUTH_TOKEN", "ANTHROPIC_API_KEY", "OPENAI_API_KEY"})
            if (const char* value = ::getenv(name); value && *value) secrets_.emplace_back(value);
        if (const char* dir = ::getenv("LOOM_CONFIG_DIR")) {
            const auto settings = read_file(fs::path(dir) / "settings.json");
            for (std::size_t at = 0; at < settings.size();) {
                if (settings[at] != '"') { ++at; continue; }
                auto key = json_string(settings, at);
                if (!key) break;
                std::transform(key->begin(), key->end(), key->begin(), [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                });
                while (at < settings.size() && std::isspace(static_cast<unsigned char>(settings[at]))) ++at;
                if (at == settings.size() || settings[at++] != ':') continue;
                if (key->find("key") == std::string::npos && key->find("token") == std::string::npos &&
                    key->find("password") == std::string::npos && key->find("secret") == std::string::npos) continue;
                while (at < settings.size() && std::isspace(static_cast<unsigned char>(settings[at]))) ++at;
                if (auto secret = json_string(settings, at); secret && !secret->empty()) secrets_.push_back(*secret);
            }
        }
    }
    std::string operator()(std::string text) const {
        for (const auto& secret : secrets_) {
            for (std::size_t at = 0; (at = text.find(secret, at)) != std::string::npos; at += 10)
                text.replace(at, secret.size(), "[REDACTED]");
        }
        return text;
    }
private:
    std::vector<std::string> secrets_;
};

class Evidence {
public:
    explicit Evidence(fs::path directory) : root(std::move(directory)) {
        fs::create_directories(root / "work");
        fs::create_directories(root / "home");
    }
    void stage(const std::string& name, const std::string& detail = "") {
        std::ofstream(root / "stages.jsonl", std::ios::app)
            << "{\"stage\":\"" << json_escape(name) << "\",\"time_ns\":" << timestamp()
            << ",\"detail\":\"" << json_escape(redact(detail)) << "\"}\n";
        std::cout << name << '\n';
    }
    void snapshot(const std::string& name, const std::string& content) const {
        write_file(root / name, redact(content));
    }
    fs::path root;
    Redactor redact;
};

class Terminal {
public:
    Terminal(const fs::path& binary, Evidence& evidence, std::string label, Clock::time_point deadline)
        : evidence_(evidence), label_(std::move(label)), deadline_(deadline) {
        winsize dimensions{};
        dimensions.ws_row = 42;
        dimensions.ws_col = 160;
        pid_ = ::forkpty(&master_, nullptr, nullptr, &dimensions);
        require(pid_ >= 0, "forkpty failed");
        if (pid_ == 0) {
            if (::chdir((evidence_.root / "work").c_str()) != 0) ::_exit(126);
            ::setenv("HOME", (evidence_.root / "home").c_str(), 1);
            ::setenv("TERM", "xterm-256color", 1);
            ::setenv("COLORTERM", "truecolor", 1);
            ::setenv("LOOM_AGENT_RUNTIME_DIR", (evidence_.root / "runtime").c_str(), 1);
            ::unsetenv("LOOM_TEAM_NAME");
            ::unsetenv("LOOM_AGENT_NAME");
            ::unsetenv("LOOM_NON_INTERACTIVE");
            ::unsetenv("LOOM_SIMPLE_UI");
            ::signal(SIGPIPE, SIG_DFL);
            ::execl(binary.c_str(), binary.c_str(), "--dangerously-skip-permissions", nullptr);
            ::_exit(127);
        }
        ::fcntl(master_, F_SETFL, ::fcntl(master_, F_GETFL) | O_NONBLOCK);
    }
    void start() {
        require(wait([&] { return output_.find("\x1b[?1049h") != std::string::npos &&
            output_.find("\x1b[?25") != std::string::npos; }, 15s), "FTXUI terminal startup not observed");
        evidence_.stage(label_ + "-terminal-installed", "pid=" + std::to_string(pid_));
    }
    ~Terminal() {
        pump();
        if (!exited_) {
            ::kill(pid_, SIGTERM);
            const auto until = Clock::now() + 3s;
            while (!exited_ && Clock::now() < until) { pump(); ::poll(nullptr, 0, 20); }
        }
        if (!exited_) {
            ::kill(pid_, SIGKILL);
            ::close(master_);
            master_ = -1;
            for (int attempt = 0; attempt < 100; ++attempt) {
                if (::waitpid(pid_, nullptr, WNOHANG) == pid_ || errno == ECHILD) break;
                ::poll(nullptr, 0, 20);
            }
        }
        if (master_ >= 0) ::close(master_);
        try { evidence_.snapshot(label_ + "-terminal.log", output_); }
        catch (...) { std::cerr << "Could not preserve terminal evidence\n"; }
    }
    void input(std::string_view bytes) {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto count = ::write(master_, bytes.data() + offset, bytes.size() - offset);
            if (count > 0) offset += static_cast<std::size_t>(count);
            else if (errno == EINTR || errno == EAGAIN) { pump(); ::poll(nullptr, 0, 10); }
            else throw std::runtime_error("PTY input failed");
            require(Clock::now() < deadline_, "PTY input deadline exceeded");
        }
    }
    bool wait(const std::function<bool()>& ready, std::chrono::seconds timeout = 120s) {
        const auto until = std::min(deadline_, Clock::now() + timeout);
        do {
            pump();
            if (ready()) return true;
            if (exited_) return false;
            pollfd descriptor{master_, POLLIN, 0};
            ::poll(&descriptor, 1, 25);
        } while (Clock::now() < until);
        return false;
    }
    void exit(bool hangup = false) {
        if (hangup) require(::kill(pid_, SIGHUP) == 0, "SIGHUP failed");
        else input("\x04");
        require(wait([&] { return exited_; }, 15s), "Terminal did not drain and exit");
        require(WIFEXITED(status_) && WEXITSTATUS(status_) == (hangup ? 1 : 0), "Unexpected terminal exit status");
        require(output_.find("\x1b[?1049l") != std::string::npos, "Terminal alternate screen was not restored");
        evidence_.stage(label_ + "-terminal-exited", "status=" + std::to_string(WEXITSTATUS(status_)));
    }
private:
    void pump() {
        std::array<char, 8192> bytes{};
        for (;;) {
            const auto count = ::read(master_, bytes.data(), bytes.size());
            if (count <= 0) break;
            output_.append(bytes.data(), static_cast<std::size_t>(count));
            if (output_.size() > 4 * 1024 * 1024) output_.erase(0, output_.size() - 4 * 1024 * 1024);
        }
        if (!exited_ && ::waitpid(pid_, &status_, WNOHANG) == pid_) exited_ = true;
    }
    Evidence& evidence_;
    std::string label_;
    Clock::time_point deadline_;
    int master_ = -1;
    pid_t pid_ = -1;
    int status_ = 0;
    bool exited_ = false;
    std::string output_;
};

fs::path record_path(const Evidence& evidence, std::string_view id) {
    return evidence.root / "runtime" / (std::string(id) + ".json");
}
std::string record(const Evidence& evidence, std::string_view id) {
    return read_file(record_path(evidence, id));
}
std::string marker_command(const fs::path& helper, const Evidence& evidence,
                           const std::string& name, int milliseconds, const std::string& output) {
    return shell_quote(helper.string()) + " --work-marker " + shell_quote((evidence.root / "work").string()) +
        " " + shell_quote(name) + " " + std::to_string(milliseconds) + " " + shell_quote(output);
}
std::string child_task(const std::string& command, const std::string& output) {
    return "Do this task directly without delegation: call Bash once with command " + command +
        " and timeout 120000. Do not run Bash in background. Make no other file changes. "
        "Wait for Bash to finish, then return exactly " + output + ".";
}
std::string spawn_prompt(const std::string& name, const std::string& task) {
    return "Call Agent using exactly this JSON, without rewriting its prompt: {\"subagent_type\":\"general-purpose\","
        "\"name\":\"" + json_escape(name) + "\",\"run_in_background\":true,"
        "\"description\":\"live-terminal-check\",\"prompt\":\"" + json_escape(task) + "\"}. ";
}

void check_completed(Evidence& evidence, const std::string& id, const std::string& output) {
    const auto text = record(evidence, id);
    require(field(text, "status") == "completed", "Agent did not persist completed status: " + id);
    require(field(text, "background") == "true", "Agent was not executed in background: " + id);
    require(field(text, "output").find(output) != std::string::npos, "Agent final output missing: " + id);
    require(field(text, "team_name").empty(), "Unexpected teammate path: " + id);
    evidence.snapshot(id + "-completed.json", text);
}
void parallel(const fs::path& binary, const fs::path& helper, Evidence& evidence, Clock::time_point deadline) {
    Terminal terminal(binary, evidence, "parallel", deadline);
    terminal.start();
    const auto first = child_task(marker_command(helper, evidence, "one", 8000, "LIVE_ONE_DONE"), "LIVE_ONE_DONE");
    const auto second = child_task(marker_command(helper, evidence, "two", 8000, "LIVE_TWO_DONE"), "LIVE_TWO_DONE");
    terminal.input("This is an authorized scratch-only live subagent verification. "
        "Use exactly two Agent calls in the SAME response; do not wait after the first. "
        "Do not call Bash yourself, do not create a team, and do not modify configuration. " +
        spawn_prompt("live-parallel-one", first) + " " + spawn_prompt("live-parallel-two", second) +
        " After both are spawned, respond briefly and let both run.\r");
    evidence.stage("parallel-prompt-submitted");
    require(terminal.wait([&] { return fs::exists(evidence.root / "work/one.start") &&
        fs::exists(evidence.root / "work/two.start"); }), "Both child Bash commands did not start");
    evidence.stage("parallel-both-commands-started");
    require(terminal.wait([&] { return field(record(evidence, "live-parallel-one"), "status") == "completed" &&
        field(record(evidence, "live-parallel-two"), "status") == "completed" &&
        field(record(evidence, "live-parallel-one"), "output").find("LIVE_ONE_DONE") != std::string::npos &&
        field(record(evidence, "live-parallel-two"), "output").find("LIVE_TWO_DONE") != std::string::npos;
    }), "Parallel agents did not complete");
    check_completed(evidence, "live-parallel-one", "LIVE_ONE_DONE");
    check_completed(evidence, "live-parallel-two", "LIVE_TWO_DONE");
    const auto first_start = std::stoll(read_file(evidence.root / "work/one.start"));
    const auto second_start = std::stoll(read_file(evidence.root / "work/two.start"));
    const auto first_end = std::stoll(read_file(evidence.root / "work/one.end"));
    const auto second_end = std::stoll(read_file(evidence.root / "work/two.end"));
    require(std::max(first_start, second_start) < std::min(first_end, second_end),
        "Child command execution intervals did not overlap");
    evidence.stage("parallel-overlap-verified", "overlap_ns=" +
        std::to_string(std::min(first_end, second_end) - std::max(first_start, second_start)));
    terminal.exit();
}
void cancel_resume(const fs::path& binary, const fs::path& helper, Evidence& evidence,
                   Clock::time_point deadline, bool hangup) {
    const std::string id = "live-cancel-resume";
    const std::string proof = "HISTORY_" + std::to_string(timestamp());
    {
        Terminal terminal(binary, evidence, "cancel", deadline);
        terminal.start();
        // Put the proof in the required command itself, so executing that
        // command establishes the history precondition before cancellation.
        const auto unfinished = "SHOULD_NOT_FINISH_" + proof;
        const auto task = child_task(marker_command(helper, evidence, "cancel", 90000, unfinished),
            unfinished) + " Remember the history proof token " + proof + " for a later message.";
        terminal.input("This is an authorized scratch-only lifecycle check. Do not call Bash yourself. "
            "Do not create a team or modify configuration. " + spawn_prompt(id, task) +
            " After spawning respond briefly and let the task run.\r");
        evidence.stage("cancel-prompt-submitted");
        require(terminal.wait([&] { return fs::exists(evidence.root / "work/cancel.start") &&
            field(record(evidence, id), "status") == "running"; }, 75s), "Cancelable child command did not start");
        evidence.stage("cancel-child-command-running");
        terminal.exit(hangup);
    }
    const auto cancelled = record(evidence, id);
    require(field(cancelled, "status") == "cancelled" && field(cancelled, "cancel_requested") == "true",
        "Cancellation was not persisted");
    require(!fs::exists(evidence.root / "work/cancel.end"), "Cancelled command finished instead of stopping");
    const auto child_pid = static_cast<pid_t>(std::stol(read_file(evidence.root / "work/cancel.pid")));
    require(child_pid > 1, "Invalid child command PID");
    for (int attempt = 0; attempt < 100 && ::kill(child_pid, 0) == 0; ++attempt) ::poll(nullptr, 0, 25);
    require(::kill(child_pid, 0) < 0 && errno == ESRCH, "Cancelled Bash child process remained alive");
    const auto cancelled_command_start = read_file(evidence.root / "work/cancel.start");
    const auto history_path = field(cancelled, "sidechain_jsonl_path");
    const auto history = read_file(history_path);
    require(!history.empty() && history.find(proof) != std::string::npos, "Original structured child history missing");
    evidence.snapshot("cancelled-record.json", cancelled);
    evidence.snapshot("history-before.jsonl", history);
    evidence.stage("cancelled-record-and-process-drain-verified", "child_pid=" + std::to_string(child_pid));
    {
        Terminal terminal(binary, evidence, "resume", deadline);
        terminal.start();
        const auto resume_task = "Do this task directly: call Bash once with command " +
            marker_command(helper, evidence, "resumed", 0, "LIVE_RESUMED") +
            " and timeout 120000. After it succeeds, return LIVE_RESUMED followed by the "
            "HISTORY_ proof token from your original task. Make no other file changes. "
            "The earlier long command was cancelled: do not repeat it.";
        terminal.input("This is an authorized continuation of a persisted cancelled subagent. "
            "Call the installed send_message tool using exactly this JSON: {\"to\":\"" + id +
            "\",\"content\":\"" + json_escape(resume_task) + "\"}. "
            "Do not call Agent to create a replacement. Do not call Bash yourself. "
            "Do not create a team or change configuration. After sending respond briefly.\r");
        evidence.stage("resume-prompt-submitted");
        require(terminal.wait([&] {
            require(read_file(evidence.root / "work/cancel.start") == cancelled_command_start &&
                !fs::exists(evidence.root / "work/cancel.end"),
                "Resumed agent replayed the cancelled command");
            return fs::exists(evidence.root / "work/resumed.end") &&
            field(record(evidence, id), "status") == "completed" &&
            field(record(evidence, id), "output").find(proof) != std::string::npos;
        }), "Persisted agent did not resume and complete with its original history");
        check_completed(evidence, id, "LIVE_RESUMED");
        require(field(record(evidence, id), "output").find(proof) != std::string::npos,
            "Resumed agent did not recall the original history proof token");
        const auto restored = read_file(history_path);
        require(restored.starts_with(history) && restored.size() > history.size(), "Structured history was replaced during resume");
        evidence.snapshot("history-after.jsonl", restored);
        evidence.stage("resume-output-and-history-verified");
        terminal.exit();
    }
}
int marker(int argc, char** argv) {
    require(argc == 6, "Marker helper requires directory, name, delay and output");
    const fs::path directory = argv[2];
    const std::string name = argv[3];
    require(name == "one" || name == "two" || name == "cancel" || name == "resumed", "Unknown marker name");
    const int milliseconds = std::stoi(argv[4]);
    require(milliseconds >= 0 && milliseconds <= 90000, "Marker helper delay outside bounds");
    write_file(directory / (name + ".pid"), std::to_string(::getpid()));
    write_file(directory / (name + ".start"), std::to_string(timestamp()));
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    write_file(directory / (name + ".end"), std::to_string(timestamp()));
    std::cout << argv[5] << '\n';
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--work-marker") {
        try { return marker(argc, argv); }
        catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    }
    if (const char* enabled = ::getenv("LOOM_LIVE_SUBAGENT_TEST"); !enabled || std::string_view(enabled) != "1") {
        std::cout << "Live endpoint verification is opt-in (LOOM_LIVE_SUBAGENT_TEST=1).\n";
        return 125;
    }
    std::unique_ptr<Evidence> evidence;
    try {
        require(argc == 2, "Choose parallel, cancel-resume, or cancel-resume-sighup");
        const std::string scenario = argv[1];
        require(scenario == "parallel" || scenario == "cancel-resume" || scenario == "cancel-resume-sighup", "Unknown live scenario");
        const char* binary_env = ::getenv("LOOM_BINARY");
        const char* config = ::getenv("LOOM_CONFIG_DIR");
        require(binary_env && fs::exists(binary_env), "LOOM_BINARY must point to the real Loom executable");
        require(config && fs::path(config).is_absolute() && fs::exists(fs::path(config) / "settings.json"),
            "LOOM_CONFIG_DIR must point to staged current endpoint settings");
        const auto base = ::getenv("LOOM_LIVE_EVIDENCE_DIR")
            ? fs::path(::getenv("LOOM_LIVE_EVIDENCE_DIR")) : fs::temp_directory_path() / "loom-live-subagent-evidence";
        const auto root = fs::absolute(base / (scenario + "-" + std::to_string(::getpid()) + "-" + std::to_string(timestamp())));
        evidence = std::make_unique<Evidence>(root);
        std::cout << "Evidence: " << root << '\n';
        evidence->stage("scenario-started", scenario);
        const auto deadline = Clock::now() + 180s;
        if (scenario == "parallel") parallel(fs::absolute(binary_env), fs::canonical(argv[0]), *evidence, deadline);
        else cancel_resume(fs::absolute(binary_env), fs::canonical(argv[0]), *evidence, deadline, scenario.ends_with("sighup"));
        evidence->stage("passed");
        return 0;
    } catch (const std::exception& error) {
        if (evidence) evidence->stage("failed", error.what());
        else std::cerr << error.what() << '\n';
        return 1;
    }
}
