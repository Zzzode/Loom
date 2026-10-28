module;

#include <signal.h>
#include <uv.h>

module loom.tools.grep;

import std;
import loom.serdes.json;

namespace loom::tools::grep {
namespace {

constexpr std::size_t kCaptureLimit = 256 * 1024;
constexpr std::size_t kResultLimit = 18'000;

// A loop per invocation keeps parallel read-only searches independent. All
// process operations, including cancellation, run on the loop's owner thread.
struct SearchProcess {
    uv_loop_t loop{};
    uv_process_t process{};
    uv_pipe_t out{};
    uv_pipe_t err{};
    uv_timer_t timer{};
    std::array<char, 8192> buffer{};
    std::string output;
    std::string diagnostic;
    std::string failure;
    std::function<bool()> is_cancelled;
    std::chrono::steady_clock::time_point deadline;
    int64_t exit_code = -1;
    int exit_signal = 0;
    bool exited = false;
    bool timed_out = false;
    bool cancelled = false;
    bool truncated = false;
    bool terminating = false;

    void terminate() {
        if (terminating) return;
        terminating = true;
        if (!exited) {
            // libuv maps SIGTERM to TerminateProcess on Windows; POSIX needs
            // SIGKILL so an unresponsive search cannot ignore its deadline.
#ifdef _WIN32
            const int rc = uv_process_kill(&process, SIGTERM);
#else
            const int rc = uv_process_kill(&process, SIGKILL);
#endif
            if (rc < 0 && rc != UV_ESRCH) failure = uv_strerror(rc);
        }
        // Close read ends as well: a descendant must not keep the loop alive
        // after a deadline or cancellation. The exit callback still reaps rg.
        if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(&out)))
            uv_close(reinterpret_cast<uv_handle_t*>(&out), nullptr);
        if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(&err)))
            uv_close(reinterpret_cast<uv_handle_t*>(&err), nullptr);
    }

    static void allocate(uv_handle_t* handle, std::size_t, uv_buf_t* buf) {
        auto& self = *static_cast<SearchProcess*>(handle->data);
        *buf = uv_buf_init(self.buffer.data(), static_cast<unsigned int>(self.buffer.size()));
    }

    static void read(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf) {
        auto& self = *static_cast<SearchProcess*>(stream->data);
        if (size > 0) {
            const bool stdout_stream = stream == reinterpret_cast<uv_stream_t*>(&self.out);
            auto& target = stdout_stream ? self.output : self.diagnostic;
            const std::size_t limit = stdout_stream ? kCaptureLimit : 4096;
            const auto bytes = static_cast<std::size_t>(size);
            const auto take = std::min(bytes, limit - target.size());
            target.append(buf->base, take);
            if (stdout_stream && take < bytes) {
                self.truncated = true;
                self.terminate();
            }
        } else if (size < 0) {
            if (size != UV_EOF) {
                self.failure = uv_strerror(static_cast<int>(size));
                self.terminate();
            } else {
                if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(stream)))
                    uv_close(reinterpret_cast<uv_handle_t*>(stream), nullptr);
            }
        }
    }

    static void tick(uv_timer_t* timer) {
        auto& self = *static_cast<SearchProcess*>(timer->data);
        if (self.is_cancelled && self.is_cancelled()) {
            self.cancelled = true;
            self.terminate();
        } else if (std::chrono::steady_clock::now() >= self.deadline) {
            self.timed_out = true;
            self.terminate();
        }
    }

    static void exit(uv_process_t* process, int64_t status, int signal) {
        auto& self = *static_cast<SearchProcess*>(process->data);
        self.exited = true;
        self.exit_code = status;
        self.exit_signal = signal;
        uv_close(reinterpret_cast<uv_handle_t*>(process), nullptr);
        // Keep the deadline active while draining pipes. Closing stdout before
        // EOF on the normal path would lose the tail of a successful result.
    }

    [[nodiscard]] std::optional<std::string> run(std::vector<std::string>& args, int timeout_ms) {
        int rc = uv_loop_init(&loop);
        if (rc < 0) return std::string(uv_strerror(rc));
        // Close every initialized handle, including partial initialization and
        // failed spawns, before releasing stack storage or closing the loop.
        auto cleanup = [this] {
            uv_walk(
                &loop,
                [](uv_handle_t* handle, void*) {
                    if (!uv_is_closing(handle)) uv_close(handle, nullptr);
                },
                nullptr);
            uv_run(&loop, UV_RUN_DEFAULT);
            uv_loop_close(&loop);
        };
        for (auto* pipe : {&out, &err}) {
            rc = uv_pipe_init(&loop, pipe, 0);
            if (rc < 0) {
                cleanup();
                return std::string(uv_strerror(rc));
            }
            pipe->data = this;
        }
        rc = uv_timer_init(&loop, &timer);
        if (rc < 0) {
            cleanup();
            return std::string(uv_strerror(rc));
        }
        timer.data = this;
        process.data = this;
        std::vector<char*> argv;
        for (auto& arg : args)
            argv.push_back(arg.data());
        argv.push_back(nullptr);
        uv_stdio_container_t stdio[3]{};
        stdio[0].flags = UV_IGNORE;
        stdio[1].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
        stdio[1].data.stream = reinterpret_cast<uv_stream_t*>(&out);
        stdio[2].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
        stdio[2].data.stream = reinterpret_cast<uv_stream_t*>(&err);
        uv_process_options_t opts{};
        opts.file = args.front().c_str();
        opts.args = argv.data();
        opts.stdio_count = 3;
        opts.stdio = stdio;
        opts.exit_cb = exit;
        rc = uv_spawn(&loop, &process, &opts);
        if (rc < 0) {
            cleanup();
            return rc == UV_ENOENT ? "ripgrep (rg) not found on PATH; install ripgrep to use Grep."
                                   : std::format("Cannot start ripgrep: {}", uv_strerror(rc));
        }
        deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        for (auto* pipe : {&out, &err}) {
            rc = uv_read_start(reinterpret_cast<uv_stream_t*>(pipe), allocate, read);
            if (rc < 0) {
                failure = uv_strerror(rc);
                terminate();
                break;
            }
        }
        rc = uv_timer_start(&timer, tick, 0, 25);
        if (rc < 0) {
            failure = uv_strerror(rc);
            terminate();
        }
        while (uv_run(&loop, UV_RUN_ONCE)) {
            if (exited && uv_is_closing(reinterpret_cast<uv_handle_t*>(&out)) &&
                uv_is_closing(reinterpret_cast<uv_handle_t*>(&err)))
                break;
        }
        cleanup();
        return std::nullopt;
    }
};

}  // namespace

loom::core::ToolDefinition GrepTool::definition() {
    using loom::core::SchemaProperty;
    return {
        .name = std::string(kName),
        .description = std::string(kDescription),
        .input_schema =
            {.properties =
                 {
                     SchemaProperty{.name = "pattern",
                                    .type = "string",
                                    .description = "Ripgrep regex pattern",
                                    .required = true},
                     SchemaProperty{.name = "path",
                                    .type = "string",
                                    .description =
                                        "File or directory; defaults to current directory"},
                     SchemaProperty{
                         .name = "output_mode",
                         .type = "string",
                         .description = "content (default), files_with_matches, or count (matching "
                                        "lines per file)",
                         .default_value = "content",
                         .enum_values =
                             std::vector<std::string>{"content", "files_with_matches", "count"}},
                     SchemaProperty{.name = "glob",
                                    .type = "string",
                                    .description =
                                        "Filter files with a ripgrep glob, e.g. **/*.cpp"},
                     SchemaProperty{.name = "type",
                                    .type = "string",
                                    .description = "Ripgrep file type, e.g. cpp or py"},
                     SchemaProperty{.name = "head_limit",
                                    .type = "integer",
                                    .description =
                                        "Maximum returned entries (1-1000); default 200"},
                     SchemaProperty{.name = "offset",
                                    .type = "integer",
                                    .description = "Entries to skip (0-100000); default 0"},
                     SchemaProperty{
                         .name = "timeout_ms",
                         .type = "integer",
                         .description =
                             "Execution deadline in milliseconds (1-120000); default 10000"},
                 }},
        .permission = loom::core::ToolPermission::ReadOnly,
        .category = "filesystem",
        .max_result_size_chars = 20'000,
    };
}

loom::utils::Result<loom::core::ToolResult> GrepTool::execute(const loom::core::ToolInput& input) {
    using loom::core::ToolResult;
    auto doc = loom::utils::json::parse(input.json());
    if (!doc || !doc->root().is_obj()) return ToolResult::error("Expected JSON object");
    const auto root = doc->root();
    const auto pattern = root.get("pattern");
    if (!pattern.is_str() || pattern.as_str().empty())
        return ToolResult::error("Missing 'pattern' field");

    std::string path = std::filesystem::current_path().string();
    std::string mode = "content";
    std::string glob;
    std::string type;
    for (auto [name, target] :
         {std::pair{"path", &path}, {"output_mode", &mode}, {"glob", &glob}, {"type", &type}}) {
        const auto value = root.get(name);
        if (!value.valid() || value.is_null()) continue;
        if (!value.is_str() || value.as_str().empty() ||
            value.as_str().find('\0') != std::string_view::npos)
            return ToolResult::error(
                std::format("'{}' must be a non-empty string without NUL bytes", name));
        *target = value.as_str();
    }
    if (pattern.as_str().find('\0') != std::string_view::npos)
        return ToolResult::error("Pattern contains NUL bytes");
    if (mode != "content" && mode != "files_with_matches" && mode != "count")
        return ToolResult::error("Invalid output_mode: use content, files_with_matches, or count");
    int head_limit = 200;
    int offset = 0;
    int timeout_ms = 10000;
    for (auto [name, target, minimum, maximum] : {std::tuple{"head_limit", &head_limit, 1, 1000},
                                                  {"offset", &offset, 0, 100000},
                                                  {"timeout_ms", &timeout_ms, 1, 120000}}) {
        const auto value = root.get(name);
        if (!value.valid() || value.is_null()) continue;
        if (!value.is_int() || value.as_int() < minimum || value.as_int() > maximum)
            return ToolResult::error(
                std::format("'{}' must be an integer between {} and {}", name, minimum, maximum));
        *target = static_cast<int>(value.as_int());
    }
    if (input.is_cancelled && input.is_cancelled()) return ToolResult::error("Grep cancelled");

    // Ignore user ripgrep config so it cannot silently enable binary searching,
    // change modes or run preprocessors. Options and operands stay separate.
    std::vector<std::string> args{"rg",     "--no-config", "--no-require-git", "--color", "never",
                                  "--sort", "path"};
    if (mode == "content")
        args.insert(args.end(), {"--line-number", "--with-filename", "--no-heading"});
    else if (mode == "files_with_matches")
        args.push_back("--files-with-matches");
    else
        args.insert(args.end(), {"--count", "--with-filename"});
    if (!glob.empty()) args.insert(args.end(), {"--glob", glob});
    if (!type.empty()) args.insert(args.end(), {"--type", type});
    args.insert(args.end(), {"--regexp", std::string(pattern.as_str()), "--", path});
    SearchProcess process;
    process.is_cancelled = input.is_cancelled;
    if (auto error = process.run(args, timeout_ms)) return ToolResult::error(*error);
    if (process.cancelled) return ToolResult::error("Grep cancelled");
    if (process.timed_out)
        return ToolResult::error(
            std::format("Grep timed out after {} ms; narrow the path or pattern", timeout_ms));
    if (!process.failure.empty()) return ToolResult::error("Grep I/O error: " + process.failure);
    if (!process.truncated &&
        (process.exit_signal || process.exit_code > 1 || process.exit_code < 0))
        return ToolResult::error(std::format("ripgrep failed (exit {}, signal {}): {}",
                                             process.exit_code, process.exit_signal,
                                             process.diagnostic));
    const auto warning = process.diagnostic.empty()
                             ? std::string{}
                             : "[ripgrep diagnostic]\n" + process.diagnostic.substr(0, 1000) + '\n';
    if (process.output.empty()) return ToolResult::success("No matches found.\n" + warning);

    std::string result;
    std::istringstream lines(process.output);
    std::string line;
    int index = 0;
    int shown = 0;
    bool truncated = process.truncated;
    while (std::getline(lines, line)) {
        // Never expose a partial entry from a bounded capture.
        if (process.truncated && lines.eof()) break;
        if (index++ < offset) continue;
        if (shown >= head_limit || result.size() + line.size() + 1 > kResultLimit) {
            truncated = true;
            break;
        }
        result += line + '\n';
        ++shown;
    }
    if (result.empty()) result = "No entries at this offset or within the output size limit.\n";
    if (truncated)
        result += "[results truncated; narrow the search or adjust offset/head_limit; counts may "
                  "be incomplete]\n";
    result += warning;
    return ToolResult::success(std::move(result));
}

}  // namespace loom::tools::grep

namespace loom::tools {
std::unique_ptr<loom::core::ITool> make_grep_tool() {
    struct Adapter final : loom::core::ITool {
        GrepTool tool;
        loom::core::ToolDefinition def = GrepTool::definition();

        const loom::core::ToolDefinition& definition() const override { return def; }

        loom::core::Result<loom::core::ToolResult>
        execute(const loom::core::ToolInput& input) override {
            auto result = tool.execute(input);
            if (result) return std::move(*result);
            return std::unexpected(loom::core::Error::make(
                loom::core::ErrorCode::ToolExecutionFailed, result.error().format()));
        }

        bool check_permission(const loom::core::ToolInput& input) const override {
            return tool.check_permission(input);
        }
    };

    return std::make_unique<Adapter>();
}
}  // namespace loom::tools
