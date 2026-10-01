// WebFetchTool - Fetches web content from URLs
module;

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

export module loom.tools.web_fetch;

import std;

import loom.utils.error;
import loom.tools.tool;
import loom.serdes.json;

export namespace loom::tools::web_fetch {

namespace detail {
struct CurlRunResult {
    std::string output;
    bool cancelled = false;
    int exit_status = 0;
};

[[nodiscard]] inline auto shell_quote(std::string_view s) -> std::string {
    std::string out = "'";
    for (char ch : s) {
        if (ch == '\'') out += R"('\'')";
        else out += ch;
    }
    out += "'";
    return out;
}

[[nodiscard]] inline auto run_curl_with_cancel(
    std::string_view url,
    std::function<bool()> should_cancel
) -> std::expected<CurlRunResult, std::string> {
    const auto cmd = "curl -fsSL --noproxy localhost,127.0.0.1,::1 --max-time 30 " +
        shell_quote(url) + " 2>&1";
    int pipefd[2]{-1, -1};
    if (pipe(pipefd) != 0) return std::unexpected("failed to create curl pipe");

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return std::unexpected("failed to fork curl process");
    }

    if (pid == 0) {
        (void)setpgid(0, 0);
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }

    (void)setpgid(pid, pid);
    close(pipefd[1]);
    const int flags = fcntl(pipefd[0], F_GETFL, 0);
    if (flags >= 0) (void)fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    CurlRunResult result;
    std::array<char, 4096> buf{};
    int status = 0;
    bool exited = false;
    bool cancelled = false;

    auto drain_available = [&] {
        while (true) {
            const auto n = read(pipefd[0], buf.data(), buf.size());
            if (n > 0) {
                result.output.append(buf.data(), static_cast<std::size_t>(n));
                continue;
            }
            if (n == 0) break;
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
            break;
        }
    };

    while (!exited) {
        drain_available();
        const auto wait_result = waitpid(pid, &status, WNOHANG);
        if (wait_result == pid) {
            exited = true;
            break;
        }
        if (wait_result < 0 && errno != EINTR) {
            close(pipefd[0]);
            return std::unexpected("failed to wait for curl process");
        }

        if (!cancelled && should_cancel && should_cancel()) {
            cancelled = true;
            kill(-pid, SIGTERM);
        } else if (cancelled) {
            kill(-pid, SIGKILL);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    drain_available();
    close(pipefd[0]);

    result.cancelled = cancelled;
    if (WIFEXITED(status)) {
        result.exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exit_status = 128 + WTERMSIG(status);
    } else {
        result.exit_status = 1;
    }
    return result;
}

[[nodiscard]] inline auto run_curl(std::string_view url) -> std::optional<std::string> {
    auto result = run_curl_with_cancel(url, {});
    if (!result || result->cancelled || result->exit_status != 0) return std::nullopt;
    return std::move(result->output);
}

[[nodiscard]] inline auto parse_url(std::string_view json) -> std::expected<std::string, std::string> {
    auto parsed = loom::utils::json::parse(json);
    if (!parsed || !parsed->root().is_obj()) {
        return std::unexpected("Invalid JSON input");
    }

    auto url = parsed->root().get("url");
    if (!url.is_str() || url.as_str().empty()) {
        return std::unexpected("Missing 'url' field");
    }

    return std::string(url.as_str());
}
}

using loom::core::Tool;
using loom::core::ToolInput;
using loom::core::ToolResult;
using loom::core::ToolDefinition;
using loom::core::ToolPermission;
using loom::core::InputSchema;
using loom::core::SchemaProperty;
using loom::utils::Result;

// =========================================================================
// WebFetchTool Implementation
// =========================================================================

/// WebFetchTool - Fetches web content
class WebFetchTool {
public:
    static constexpr std::string_view kName = "WebFetch";
    static constexpr std::string_view kDescription = 
        "Fetch the contents of a web page from a URL.";
    
    static ToolDefinition definition() {
        return ToolDefinition{
            .name = std::string(kName),
            .description = std::string(kDescription),
            .input_schema = InputSchema{
                .properties = {
                    SchemaProperty{
                        .name = "url",
                        .type = "string",
                        .description = "URL to fetch content from",
                        .required = true,
                        .default_value = std::nullopt,
                        .enum_values = std::nullopt
                    }
                }
            },
            .permission = ToolPermission::Network,
            .category = "network"
        };
    }
    
    WebFetchTool() = default;
    
    [[nodiscard]] bool check_permission([[maybe_unused]] const ToolInput& input) const {
        return true;
    }
    
    [[nodiscard]] Result<ToolResult> execute(const ToolInput& input) {
        auto parsed_url = detail::parse_url(input.json());
        if (!parsed_url) {
            return ToolResult::error(parsed_url.error());
        }

        const auto& url = *parsed_url;
        if (!url.starts_with("http://") && !url.starts_with("https://")) {
            return ToolResult::error("URL must start with http:// or https://");
        }
        auto content = detail::run_curl(url);
        if (!content) return ToolResult::error(std::format("Failed to fetch URL: {}", url));
        return ToolResult::success(*content);
    }
};

} // namespace loom::tools::web_fetch

// Export main tool class
export namespace loom::tools {
    using loom::tools::web_fetch::WebFetchTool;

    /// Factory: create WebFetchTool wrapped as ITool (adapts Result types across modules)
    [[nodiscard]] auto make_web_fetch_tool() -> std::unique_ptr<loom::core::ITool> {
        struct Adapter final : loom::core::ITool {
            WebFetchTool tool_;
            loom::core::ToolDefinition def_ = WebFetchTool::definition();

            const loom::core::ToolDefinition& definition() const override { return def_; }
            std::expected<loom::core::ToolResult, loom::core::Error> execute(const loom::core::ToolInput& input) override {
                auto result = tool_.execute(input);
                if (result) return std::move(*result);
                return std::unexpected(loom::core::Error::make(
                    loom::core::ErrorCode::ToolExecutionFailed, result.error().format()));
            }
            bool check_permission(const loom::core::ToolInput& input) const override {
                return tool_.check_permission(input);
            }
        };
        return std::make_unique<Adapter>();
    }
}
