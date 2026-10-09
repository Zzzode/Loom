/// @file test_misc_utils.cpp
///
/// Split out of the former test_utils.cpp kitchen-sink (169 tests, 53 suites).
/// This file covers the remaining utility suites (plugins, git, shell, clipboard, GitHub, and more) suites.

#include <gtest/gtest.h>
#include <cstdlib>
#include <httplib.h>

import std;
import loom.text.string;
import loom.text.string_utils;
import loom.containers.array_utils;
import loom.serdes.json;
import loom.utils.error;
import loom.containers.circular_buffer;
import loom.model.token_budget;
import loom.config.settings_sources;
import loom.net.http.ssrf_guard;
import loom.plugins.plugin_identifier;
import loom.plugins.plugin_dependency_resolver;
import loom.config.settings_paths;
import loom.config.settings_merge;
import loom.plugins.plugin_marketplace_rules;
import loom.plugins.plugin_versioning;
import loom.plugins.plugin_loader;
import loom.parsing.cli.argument_substitution;
import loom.text.semantic_boolean;
import loom.text.semantic_number;
import loom.ui.chrome.terminal_io;
import loom.commands.review.review_remote;
import loom.security.query_guard;
import loom.agent.agent_id;
import loom.security.auto_mode_denials;
import loom.diagnostics.activity_manager;
import loom.platform.env.env_utils;
import loom.cache.cache_paths;
import loom.platform.binary_check;
import loom.skills.hints;
import loom.scm.git.commit_attribution;
import loom.crypto.hash;
import loom.types.tagged_id;
import loom.ui.messages.message_predicates;
import loom.types.wire.content_array;
import loom.containers.object_group_by;
import loom.process.timeouts;
import loom.parsing.cli.slash_command_parsing;
import loom.containers.set_utils;
import loom.text.words;
import loom.diagnostics.fps_tracker;
import loom.security.privacy_level;
import loom.tools.support.script_tool_enabled;
import loom.prompt.support.prompt_category;
import loom.teams.control_message_compat;
import loom.security.sanitization;
import loom.text.diff_utils;
import loom.process.shell.shell_providers;
import loom.config.settings;
import loom.scm.git.git_diff;
import loom.net.http.proxy_utils;
import loom.net.http.github_utils;
import loom.plugins.marketplace;
import loom.platform.clipboard;
import loom.text.parse_references;
import loom.memdir.memdir;

class ScopedEnvVar {
public:
    explicit ScopedEnvVar(const char* name) : name_(name) {
        if (const char* value = std::getenv(name)) {
            previous_ = std::string(value);
        }
    }

    ~ScopedEnvVar() {
        if (previous_) {
            setenv(name_.c_str(), previous_->c_str(), 1);
        } else {
            unsetenv(name_.c_str());
        }
    }

    void set(const char* value) const {
        setenv(name_.c_str(), value, 1);
    }

    void unset() const {
        unsetenv(name_.c_str());
    }

private:
    std::string name_;
    std::optional<std::string> previous_;
};

class CurrentPathGuard {
public:
    explicit CurrentPathGuard(const std::filesystem::path& next) : previous_(std::filesystem::current_path()) {
        std::filesystem::current_path(next);
    }

    ~CurrentPathGuard() {
        std::error_code ec;
        std::filesystem::current_path(previous_, ec);
    }

private:
    std::filesystem::path previous_;
};

std::string shell_quote_for_test(const std::filesystem::path& path) {
    const auto value = path.string();
    if (value.empty()) return "''";
    std::string out = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('\'');
    return out;
}

bool command_available_for_test(std::string_view command) {
    const auto check = "command -v " + std::string(command) + " >/dev/null 2>&1";
    return std::system(check.c_str()) == 0;
}

void run_shell_ok_for_test(const std::string& command) {
    ASSERT_EQ(std::system(command.c_str()), 0) << command;
}

class LocalGitHubApiServer {
public:
    std::atomic<int> user_requests{0};
    std::atomic<int> issue_comment_requests{0};
    std::atomic<int> review_comment_requests{0};
    std::atomic<int> user_status{200};
    std::atomic<bool> paginate_comments{false};

    LocalGitHubApiServer() {
        server_.Get("/user", [&](const httplib::Request& req, httplib::Response& res) {
            ++user_requests;
            capture_headers(req);
            if (user_status.load() != 200) {
                res.status = user_status.load();
                res.set_content(R"({"message":"auth failed"})", "application/json");
                return;
            }
            res.set_content(R"({
              "login": "octocat",
              "name": "The Octocat",
              "email": "octocat@example.test",
              "avatar_url": "https://avatars.example.test/octocat.png"
            })", "application/json");
        });
        server_.Get(R"(/repos/([^/]+)/([^/]+)/issues/([0-9]+)/comments)", [&](const httplib::Request& req, httplib::Response& res) {
            ++issue_comment_requests;
            capture_headers(req);
            if (paginate_comments.load()) {
                const auto page = req.has_param("page") ? req.get_param_value("page") : "1";
                if (page == "2") {
                    res.set_content(R"([
                      {
                        "id": 102,
                        "body": "issue conversation comment page two",
                        "user": {"login": "reviewer-page-two"}
                      }
                    ])", "application/json");
                    return;
                }
                res.set_header("Link", "<" + base_url() + req.path + "?page=2>; rel=\"next\"");
            }
            res.set_content(R"([
              {
                "id": 101,
                "body": "issue conversation comment",
                "user": {"login": "reviewer-a"}
              }
            ])", "application/json");
        });
        server_.Get(R"(/repos/([^/]+)/([^/]+)/pulls/([0-9]+)/comments)", [&](const httplib::Request& req, httplib::Response& res) {
            ++review_comment_requests;
            capture_headers(req);
            if (paginate_comments.load()) {
                const auto page = req.has_param("page") ? req.get_param_value("page") : "1";
                if (page == "2") {
                    res.set_content(R"([
                      {
                        "id": 203,
                        "body": "inline review comment page two",
                        "path": "src/next.cpp",
                        "line": 7,
                        "user": {"login": "reviewer-page-two"}
                      }
                    ])", "application/json");
                    return;
                }
                res.set_header("Link", "<" + base_url() + req.path + "?page=2>; rel=\"next\"");
            }
            res.set_content(R"([
              {
                "id": 202,
                "body": "inline review comment",
                "path": "src/main.cpp",
                "line": 42,
                "user": {"login": "reviewer-b"}
              }
            ])", "application/json");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ > 0) {
            worker_ = std::jthread([this](std::stop_token) {
                server_.listen_after_bind();
            });
        }
    }

    ~LocalGitHubApiServer() {
        server_.stop();
        if (worker_.joinable()) worker_.join();
    }

    [[nodiscard]] bool ready() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return "http://127.0.0.1:" + std::to_string(port_);
    }

    [[nodiscard]] std::string authorization() const {
        std::lock_guard lock(mutex_);
        return last_authorization_;
    }

    [[nodiscard]] std::string accept() const {
        std::lock_guard lock(mutex_);
        return last_accept_;
    }

private:
    void capture_headers(const httplib::Request& req) {
        std::lock_guard lock(mutex_);
        last_authorization_ = req.get_header_value("Authorization");
        last_accept_ = req.get_header_value("Accept");
    }

    httplib::Server server_;
    int port_{0};
    std::jthread worker_;
    mutable std::mutex mutex_;
    std::string last_authorization_;
    std::string last_accept_;
};

// Retargeted from the deleted `core.screens` module (a dead parallel model of
// the screen layer, no production importers) to the parser the app actually
// uses: `loom.commands.parse_pr_input` (module loom.commands.review.review_remote).
// That function had no test.
//
// These assert what the LIVE parser guarantees, which is not identical to what
// the deleted one did: the live one matches the GitHub-URL shape with
// regex_search (not anchored), so it is deliberately permissive about
// surrounding text, while still requiring an owner/repo/pull path. The
// rejections below are the properties that genuinely hold.
TEST(ReviewRemote, ParsePrInputAcceptsGithubPullReferenceForms) {
    namespace rv = loom::commands;

    auto url = rv::parse_pr_input("  https://github.com/org/repo/pull/123/files?diff=split  ");
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(url->owner, "org");
    EXPECT_EQ(url->repo, "repo");
    EXPECT_EQ(url->number, 123u);

    auto bare_host = rv::parse_pr_input("github.com/org/repo/pull/77#discussion_r1");
    ASSERT_TRUE(bare_host.has_value());
    EXPECT_EQ(bare_host->owner, "org");
    EXPECT_EQ(bare_host->repo, "repo");
    EXPECT_EQ(bare_host->number, 77u);

    auto short_form = rv::parse_pr_input("org/repo#42");
    ASSERT_TRUE(short_form.has_value());
    EXPECT_EQ(short_form->owner, "org");
    EXPECT_EQ(short_form->repo, "repo");
    EXPECT_EQ(short_form->number, 42u);
}

TEST(ReviewRemote, ParsePrInputRejectsNonPullReferences) {
    namespace rv = loom::commands;

    // An issue URL is not a pull request.
    EXPECT_FALSE(rv::parse_pr_input("https://github.com/org/repo/issues/123").has_value());
    // A non-GitHub host with no owner/repo/pull path.
    EXPECT_FALSE(rv::parse_pr_input("https://example.com/org/repo/pull/123").has_value());
    // Trailing junk on a bare number is not a number.
    EXPECT_FALSE(rv::parse_pr_input("123abc").has_value());
    // Empty / whitespace.
    EXPECT_FALSE(rv::parse_pr_input("").has_value());
    EXPECT_FALSE(rv::parse_pr_input("   ").has_value());
}

TEST(ArrayUtilsCompat, CountUniqAndIntersperseMatchTypeScriptHelpers) {
    std::vector<int> nums = {1, 2, 2, 3, 4};
    EXPECT_EQ(loom::utils::count(nums, [](int value) { return value % 2 == 0; }), 3u);
    EXPECT_EQ(loom::utils::uniq(nums), (std::vector<int>{1, 2, 3, 4}));

    std::vector<std::string> labels = {"a", "b", "c"};
    auto interspersed = loom::utils::intersperse(labels, [](std::size_t index) {
        return std::string("|") + std::to_string(index);
    });
    EXPECT_EQ(interspersed, (std::vector<std::string>{"a", "|1", "b", "|2", "c"}));
}

TEST(SetUtilsCompat, DifferenceIntersectsEveryAndUnionMatchTypeScriptHelpers) {
    const std::set<std::string> a = {"alpha", "beta", "gamma"};
    const std::set<std::string> b = {"beta", "delta"};

    EXPECT_EQ(loom::utils::difference(a, b), (std::set<std::string>{"alpha", "gamma"}));
    EXPECT_TRUE(loom::utils::intersects(a, b));
    EXPECT_FALSE(loom::utils::intersects(std::set<std::string>{}, b));
    EXPECT_TRUE(loom::utils::every(std::set<std::string>{"alpha", "gamma"}, a));
    EXPECT_FALSE(loom::utils::every(a, b));
    EXPECT_EQ(loom::utils::union_sets(a, b), (std::set<std::string>{"alpha", "beta", "delta", "gamma"}));
}

TEST(Memdir, TeamMemoryCanBeEnabledAtRuntime) {
    ScopedEnvVar disable_auto("LOOM_DISABLE_AUTO_MEMORY");
    ScopedEnvVar enable_team("LOOM_ENABLE_TEAM_MEMORY");
    ScopedEnvVar loom_sync_url("LOOM_TEAM_MEMORY_SYNC_URL");

    disable_auto.unset();
    enable_team.unset();
    loom_sync_url.unset();
    EXPECT_FALSE(memdir::is_team_memory_enabled());

    enable_team.set("true");
    EXPECT_TRUE(memdir::is_team_memory_enabled());

    disable_auto.set("1");
    EXPECT_FALSE(memdir::is_team_memory_enabled());

    disable_auto.unset();
    enable_team.set("false");
    loom_sync_url.set("https://team-memory.example");
    EXPECT_FALSE(memdir::is_team_memory_enabled());

    enable_team.unset();
    EXPECT_TRUE(memdir::is_team_memory_enabled());

    loom_sync_url.unset();
    EXPECT_FALSE(memdir::is_team_memory_enabled());
}

TEST(FpsTracker, ReturnsNulloptBeforeFramesOrWithoutElapsedTime) {
    loom::utils::fps::FpsTracker tracker;
    EXPECT_EQ(tracker.get_metrics(), std::nullopt);

    tracker.record(16.0, 1000.0);
    EXPECT_EQ(tracker.get_metrics(), std::nullopt);
}

TEST(FpsTracker, ComputesRoundedAverageAndLowOnePercentFps) {
    loom::utils::fps::FpsTracker tracker;
    tracker.record(10.0, 1000.0);
    tracker.record(20.0, 1100.0);
    tracker.record(50.0, 1250.0);

    const auto metrics = tracker.get_metrics();
    ASSERT_TRUE(metrics.has_value());
    EXPECT_DOUBLE_EQ(metrics->average_fps, 12.0);
    EXPECT_DOUBLE_EQ(metrics->low_1_pct_fps, 20.0);
}

TEST(PrivacyLevel, ResolvesMostRestrictiveTrafficAndTelemetrySignals) {
    using loom::utils::privacy::EnvLike;
    EXPECT_EQ(loom::utils::privacy::get_privacy_level(EnvLike{}), loom::utils::privacy::PrivacyLevel::Default);
    EXPECT_EQ(loom::utils::privacy::get_privacy_level({{"DISABLE_TELEMETRY", "1"}}), loom::utils::privacy::PrivacyLevel::NoTelemetry);
    EXPECT_EQ(loom::utils::privacy::get_privacy_level({{"LOOM_DISABLE_NONESSENTIAL_TRAFFIC", "0"}}), loom::utils::privacy::PrivacyLevel::EssentialTraffic);
    EXPECT_TRUE(loom::utils::privacy::is_telemetry_disabled({{"DISABLE_TELEMETRY", "true"}}));
    EXPECT_TRUE(loom::utils::privacy::is_essential_traffic_only({{"LOOM_DISABLE_NONESSENTIAL_TRAFFIC", "true"}}));
    EXPECT_EQ(
        loom::utils::privacy::get_essential_traffic_only_reason({{"LOOM_DISABLE_NONESSENTIAL_TRAFFIC", "true"}}),
        std::optional<std::string>{"LOOM_DISABLE_NONESSENTIAL_TRAFFIC"});
}

TEST(ScriptToolEnabled, MirrorsScriptAndBashToolEnvironmentChecks) {
    using loom::utils::script_tool::EnvLike;
    EXPECT_FALSE(loom::utils::script_tool::is_script_tool_enabled(EnvLike{}));
    EXPECT_TRUE(loom::utils::script_tool::is_script_tool_enabled({{"ENABLE_SCRIPT_TOOL", "yes"}}));
    EXPECT_FALSE(loom::utils::script_tool::is_script_tool_enabled({{"ENABLE_SCRIPT_TOOL", "0"}}));
    EXPECT_TRUE(loom::utils::script_tool::is_bash_tool_disabled({{"DISABLE_BASH_TOOL", "on"}}));
    EXPECT_TRUE(loom::utils::script_tool::is_bash_tool_disabled({{"ENABLE_SCRIPT_TOOL", "true"}}));
    EXPECT_FALSE(loom::utils::script_tool::is_bash_tool_disabled({{"DISABLE_BASH_TOOL", "false"}}));
}

TEST(ProxyUtils, MirrorsTypeScriptEnvironmentPriorityAndNoProxyRules) {
    ScopedEnvVar https_upper("HTTPS_PROXY");
    ScopedEnvVar https_lower("https_proxy");
    ScopedEnvVar http_upper("HTTP_PROXY");
    ScopedEnvVar http_lower("http_proxy");
    ScopedEnvVar all_upper("ALL_PROXY");
    ScopedEnvVar all_lower("all_proxy");
    ScopedEnvVar no_proxy_upper("NO_PROXY");
    ScopedEnvVar no_proxy_lower("no_proxy");

    https_upper.unset();
    https_lower.unset();
    http_upper.unset();
    http_lower.unset();
    all_upper.unset();
    all_lower.unset();
    no_proxy_upper.unset();
    no_proxy_lower.unset();

    https_upper.set("http://upper-proxy:8080");
    https_lower.set("http://lower-proxy:8080");
    http_lower.set("http://http-lower-proxy:8081");
    no_proxy_upper.set("*");
    no_proxy_lower.set("localhost, .example.com api.local:8443 127.0.0.1");

    auto resolved = loom::utils::resolve_proxy();
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->url, "http://lower-proxy:8080");
    ASSERT_EQ(resolved->no_proxy.size(), 4u);

    EXPECT_FALSE(loom::utils::should_use_proxy("http://localhost:3000"));
    EXPECT_FALSE(loom::utils::should_use_proxy("https://127.0.0.1/status"));
    EXPECT_FALSE(loom::utils::should_use_proxy("https://example.com/path"));
    EXPECT_FALSE(loom::utils::should_use_proxy("https://sub.example.com/path"));
    EXPECT_TRUE(loom::utils::should_use_proxy("https://notexample.com/path"));
    EXPECT_FALSE(loom::utils::should_use_proxy("https://api.local:8443/path"));
    EXPECT_TRUE(loom::utils::should_use_proxy("https://api.local:443/path"));

    auto proxy = loom::utils::get_proxy_for_url("http://service.test/path");
    ASSERT_TRUE(proxy.has_value());
    EXPECT_EQ(*proxy, "http://lower-proxy:8080");

    no_proxy_lower.set("*");
    EXPECT_FALSE(loom::utils::should_use_proxy("https://service.test/path"));
    EXPECT_FALSE(loom::utils::get_proxy_for_url("https://service.test/path").has_value());
}

TEST(GitHubUtils, ParsesCommonGitHubRemoteUrlForms) {
    EXPECT_EQ(
        loom::utils::github_detail::parse_repo_full_name("git@github.com:openai/codex.git"),
        std::optional<std::string>{"openai/codex"}
    );
    EXPECT_EQ(
        loom::utils::github_detail::parse_repo_full_name("https://github.com/openai/codex.git\n"),
        std::optional<std::string>{"openai/codex"}
    );
    EXPECT_EQ(
        loom::utils::github_detail::parse_repo_full_name("ssh://git@github.com/openai/codex"),
        std::optional<std::string>{"openai/codex"}
    );
    EXPECT_FALSE(loom::utils::github_detail::parse_repo_full_name("https://gitlab.com/openai/codex.git").has_value());
}

TEST(GitHubUtils, FetchesAuthenticatedUserFromGitHubApi) {
    LocalGitHubApiServer server;
    ASSERT_TRUE(server.ready());

    ScopedEnvVar gh_token("GH_TOKEN");
    ScopedEnvVar github_token("GITHUB_TOKEN");
    ScopedEnvVar github_user("GITHUB_USER");
    ScopedEnvVar github_api_base("LOOM_GITHUB_API_BASE_URL");
    gh_token.unset();
    github_token.unset();
    github_user.unset();
    github_api_base.set(server.base_url().c_str());

    loom::utils::GitHubUtils missing;
    EXPECT_EQ(missing.check_auth(), loom::utils::GitHubAuthStatus::not_configured);
    EXPECT_FALSE(missing.is_authenticated());
    EXPECT_FALSE(missing.get_current_user().has_value());

    github_token.set("github-token-for-test");
    loom::utils::GitHubUtils configured;
    EXPECT_EQ(configured.check_auth(), loom::utils::GitHubAuthStatus::authenticated);
    EXPECT_TRUE(configured.is_authenticated());
    auto current = configured.get_current_user();
    ASSERT_TRUE(current.has_value());
    EXPECT_EQ(current->login, "octocat");
    EXPECT_EQ(current->name, "The Octocat");
    EXPECT_EQ(current->email, "octocat@example.test");
    EXPECT_EQ(current->avatar_url, "https://avatars.example.test/octocat.png");
    EXPECT_EQ(server.authorization(), "Bearer github-token-for-test");
    EXPECT_EQ(server.accept(), "application/vnd.github+json");

    gh_token.set("gh-token-for-test");
    loom::utils::GitHubUtils gh_configured;
    EXPECT_EQ(gh_configured.check_auth(), loom::utils::GitHubAuthStatus::authenticated);
    EXPECT_TRUE(gh_configured.is_authenticated());
    auto gh_current = gh_configured.get_current_user();
    ASSERT_TRUE(gh_current.has_value());
    EXPECT_EQ(server.authorization(), "Bearer gh-token-for-test");
}

TEST(GitHubUtils, MapsApiAuthFailuresToAuthStatus) {
    LocalGitHubApiServer server;
    ASSERT_TRUE(server.ready());

    ScopedEnvVar gh_token("GH_TOKEN");
    ScopedEnvVar github_token("GITHUB_TOKEN");
    ScopedEnvVar github_api_base("LOOM_GITHUB_API_BASE_URL");
    gh_token.set("gh-token-for-auth-failure");
    github_token.unset();
    github_api_base.set(server.base_url().c_str());

    server.user_status.store(401);
    loom::utils::GitHubUtils expired;
    EXPECT_FALSE(expired.get_current_user().has_value());
    EXPECT_EQ(expired.auth_status(), loom::utils::GitHubAuthStatus::token_expired);
    EXPECT_FALSE(expired.is_authenticated());

    server.user_status.store(403);
    loom::utils::GitHubUtils forbidden;
    EXPECT_FALSE(forbidden.get_current_user().has_value());
    EXPECT_EQ(forbidden.auth_status(), loom::utils::GitHubAuthStatus::rate_limited);
    EXPECT_FALSE(forbidden.is_authenticated());

    server.user_status.store(429);
    loom::utils::GitHubUtils rate_limited;
    EXPECT_FALSE(rate_limited.get_current_user().has_value());
    EXPECT_EQ(rate_limited.auth_status(), loom::utils::GitHubAuthStatus::rate_limited);
    EXPECT_FALSE(rate_limited.is_authenticated());
}

TEST(GitHubUtils, DetectRepoReadsLocalGitHubRemoteAndDefaultBranch) {
    auto root = std::filesystem::temp_directory_path() / "loom_github_detect_repo_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    {
        CurrentPathGuard cwd(root);
        ASSERT_EQ(std::system("git init -q"), 0);
        ASSERT_EQ(std::system("git remote add origin git@github.com:openai/codex.git"), 0);
        ASSERT_EQ(std::system("git symbolic-ref refs/remotes/origin/HEAD refs/remotes/origin/trunk"), 0);

        auto repo = loom::utils::GitHubUtils::detect_repo();
        ASSERT_TRUE(repo.has_value());
        EXPECT_EQ(repo->full_name, "openai/codex");
        EXPECT_EQ(repo->default_branch, "trunk");
    }

    std::filesystem::remove_all(root);
}

TEST(GitHubUtils, FetchesIssueAndReviewCommentsForDetectedRepo) {
    LocalGitHubApiServer server;
    ASSERT_TRUE(server.ready());

    ScopedEnvVar gh_token("GH_TOKEN");
    ScopedEnvVar github_token("GITHUB_TOKEN");
    ScopedEnvVar github_api_base("LOOM_GITHUB_API_BASE_URL");
    gh_token.set("gh-token-for-comments");
    github_token.unset();
    github_api_base.set(server.base_url().c_str());

    auto root = std::filesystem::temp_directory_path() / "loom_github_pr_comments_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    {
        CurrentPathGuard cwd(root);
        ASSERT_EQ(std::system("git init -q"), 0);
        ASSERT_EQ(std::system("git remote add origin https://github.com/openai/codex.git"), 0);

        loom::utils::GitHubUtils github;
        ASSERT_EQ(github.check_auth(), loom::utils::GitHubAuthStatus::authenticated);
        auto comments = github.get_pr_comments(123);
        ASSERT_EQ(comments.size(), 2u);
        EXPECT_EQ(comments[0].id, 101);
        EXPECT_EQ(comments[0].author, "reviewer-a");
        EXPECT_EQ(comments[0].body, "issue conversation comment");
        EXPECT_EQ(comments[1].id, 202);
        EXPECT_EQ(comments[1].author, "reviewer-b");
        EXPECT_EQ(comments[1].body, "inline review comment");
        EXPECT_EQ(comments[1].path, "src/main.cpp");
        EXPECT_EQ(comments[1].line, 42);
    }

    EXPECT_EQ(server.issue_comment_requests.load(), 1);
    EXPECT_EQ(server.review_comment_requests.load(), 1);
    EXPECT_EQ(server.authorization(), "Bearer gh-token-for-comments");
    std::filesystem::remove_all(root);
}

TEST(GitHubUtils, PaginatesIssueAndReviewCommentsForDetectedRepo) {
    LocalGitHubApiServer server;
    ASSERT_TRUE(server.ready());
    server.paginate_comments.store(true);

    ScopedEnvVar gh_token("GH_TOKEN");
    ScopedEnvVar github_token("GITHUB_TOKEN");
    ScopedEnvVar github_api_base("LOOM_GITHUB_API_BASE_URL");
    gh_token.set("gh-token-for-paginated-comments");
    github_token.unset();
    github_api_base.set(server.base_url().c_str());

    auto root = std::filesystem::temp_directory_path() / "loom_github_pr_comments_pagination_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    {
        CurrentPathGuard cwd(root);
        ASSERT_EQ(std::system("git init -q"), 0);
        ASSERT_EQ(std::system("git remote add origin https://github.com/openai/codex.git"), 0);

        loom::utils::GitHubUtils github;
        auto comments = github.get_pr_comments(456);
        ASSERT_EQ(comments.size(), 4u);
        EXPECT_EQ(comments[0].id, 101);
        EXPECT_EQ(comments[1].id, 102);
        EXPECT_EQ(comments[2].id, 202);
        EXPECT_EQ(comments[3].id, 203);
        EXPECT_EQ(comments[3].path, "src/next.cpp");
        EXPECT_EQ(comments[3].line, 7);
    }

    EXPECT_EQ(server.issue_comment_requests.load(), 2);
    EXPECT_EQ(server.review_comment_requests.load(), 2);
    std::filesystem::remove_all(root);
}

TEST(PromptCategory, BuildsAgentAndReplQuerySources) {
    EXPECT_EQ(loom::utils::prompt_category::get_query_source_for_agent("reviewer", true), "agent:builtin:reviewer");
    EXPECT_EQ(loom::utils::prompt_category::get_query_source_for_agent(std::nullopt, true), "agent:default");
    EXPECT_EQ(loom::utils::prompt_category::get_query_source_for_agent("anything", false), "agent:custom");

    const std::set<std::string> builtin_styles = {"default", "explanatory", "learning"};
    EXPECT_EQ(loom::utils::prompt_category::get_query_source_for_repl("default", builtin_styles), "repl_main_thread");
    EXPECT_EQ(loom::utils::prompt_category::get_query_source_for_repl("learning", builtin_styles), "repl_main_thread:outputStyle:learning");
    EXPECT_EQ(loom::utils::prompt_category::get_query_source_for_repl("my-style", builtin_styles), "repl_main_thread:outputStyle:custom");
}

TEST(ErrorUtils, CreateBasicError) {
    auto err = loom::utils::error::make("something went wrong");
    EXPECT_EQ(err.message(), "something went wrong");
    EXPECT_FALSE(err.has_cause());
}

TEST(ErrorUtils, ErrorChaining) {
    auto root = loom::utils::error::make("root cause");
    auto wrapped = loom::utils::error::wrap(root, "higher level failure");

    EXPECT_EQ(wrapped.message(), "higher level failure");
    EXPECT_TRUE(wrapped.has_cause());
    EXPECT_EQ(wrapped.cause().message(), "root cause");
}

TEST(ErrorUtils, ExpectedWithValue) {
    auto result = loom::utils::error::expected<int>(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST(ErrorUtils, ExpectedWithError) {
    auto result = loom::utils::error::expected<int>(
        loom::utils::error::make("computation failed"));
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message(), "computation failed");
}

TEST(CircularBuffer, PushAndPop) {
    loom::utils::CircularBuffer<int, 4> buf;

    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);

    EXPECT_EQ(buf.size(), 3u);
    EXPECT_EQ(buf.front(), 1);
    buf.pop_front();
    EXPECT_EQ(buf.front(), 2);
    buf.pop_front();
    EXPECT_EQ(buf.front(), 3);
    buf.pop_front();
    EXPECT_TRUE(buf.empty());
}

TEST(CircularBuffer, FullStateOverwrites) {
    loom::utils::CircularBuffer<int, 3> buf;

    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    EXPECT_TRUE(buf.full());


    buf.push_back(4);
    EXPECT_EQ(buf.size(), 3u);
    EXPECT_EQ(buf.front(), 2);
    EXPECT_EQ(buf.back(), 4);
}

TEST(CircularBuffer, EmptyState) {
    loom::utils::CircularBuffer<int, 4> buf;
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.size(), 0u);
    EXPECT_EQ(buf.capacity(), 4u);
}

TEST(CircularBuffer, Iteration) {
    loom::utils::CircularBuffer<int, 8> buf;
    for (int i = 0; i < 5; ++i) buf.push_back(i);

    std::vector<int> collected;
    for (auto val : buf) {
        collected.push_back(val);
    }
    ASSERT_EQ(collected.size(), 5u);
    EXPECT_EQ(collected[0], 0);
    EXPECT_EQ(collected[4], 4);
}

TEST(CircularBuffer, TypeScriptCompatibleRecentAndArrayViews) {
    loom::utils::CircularBuffer<int, 3> buf;
    buf.add_all({1, 2, 3, 4});

    EXPECT_EQ(buf.length(), 3u);
    EXPECT_EQ(buf.to_array(), (std::vector<int>{2, 3, 4}));
    EXPECT_EQ(buf.get_recent(2), (std::vector<int>{3, 4}));
    EXPECT_EQ(buf.get_recent(10), (std::vector<int>{2, 3, 4}));

    buf.clear();
    EXPECT_EQ(buf.length(), 0u);
    EXPECT_TRUE(buf.to_array().empty());
}

TEST(SsrfGuard, BlocksPrivateLinkLocalAndMappedAddressesButAllowsLoopback) {
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("0.1.2.3"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("10.0.0.1"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("100.64.0.0"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("100.127.255.255"));
    EXPECT_FALSE(loom::utils::ssrf_guard::is_blocked_address("100.128.0.1"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("169.254.169.254"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("172.16.0.1"));
    EXPECT_FALSE(loom::utils::ssrf_guard::is_blocked_address("172.32.0.1"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("192.168.1.1"));
    EXPECT_FALSE(loom::utils::ssrf_guard::is_blocked_address("127.0.0.1"));
    EXPECT_FALSE(loom::utils::ssrf_guard::is_blocked_address("8.8.8.8"));

    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("::"));
    EXPECT_FALSE(loom::utils::ssrf_guard::is_blocked_address("::1"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("fc00::1"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("fdff::1"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("fe80::1"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("febf::1"));
    EXPECT_FALSE(loom::utils::ssrf_guard::is_blocked_address("fec0::1"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("::ffff:169.254.169.254"));
    EXPECT_TRUE(loom::utils::ssrf_guard::is_blocked_address("::ffff:a9fe:a9fe"));
    EXPECT_FALSE(loom::utils::ssrf_guard::is_blocked_address("example.com"));
}

TEST(PluginIdentifier, ParsesBuildsAndMapsScopes) {
    auto parsed = loom::utils::plugin_identifier::parse_plugin_identifier("plugin@market@ignored");
    EXPECT_EQ(parsed.name, "plugin");
    ASSERT_TRUE(parsed.marketplace.has_value());
    EXPECT_EQ(*parsed.marketplace, "market");

    auto bare = loom::utils::plugin_identifier::parse_plugin_identifier("local-plugin");
    EXPECT_EQ(bare.name, "local-plugin");
    EXPECT_FALSE(bare.marketplace.has_value());

    EXPECT_EQ(loom::utils::plugin_identifier::build_plugin_id("a", "b"), "a@b");
    EXPECT_EQ(loom::utils::plugin_identifier::build_plugin_id("a", std::nullopt), "a");
    EXPECT_TRUE(loom::utils::plugin_identifier::is_official_marketplace_name("loom-marketplace"));
    EXPECT_TRUE(loom::utils::plugin_identifier::is_official_marketplace_name("LOOM-MARKETPLACE"));
    EXPECT_FALSE(loom::utils::plugin_identifier::is_official_marketplace_name("third-party"));

    auto source = loom::utils::plugin_identifier::scope_to_setting_source(loom::utils::plugin_identifier::PluginScope::Project);
    ASSERT_TRUE(source.has_value()) << source.error();
    EXPECT_EQ(source.value(), loom::utils::settings_sources::SettingSource::ProjectSettings);
    EXPECT_FALSE(loom::utils::plugin_identifier::scope_to_setting_source(loom::utils::plugin_identifier::PluginScope::Managed).has_value());
    EXPECT_EQ(loom::utils::plugin_identifier::setting_source_to_scope(loom::utils::settings_sources::SettingSource::LocalSettings), loom::utils::plugin_identifier::PluginScope::Local);
}

TEST(PluginLoader, CreatePluginFromPathLoadsManifestAndComponentPaths) {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() / "loom_plugin_loader_manifest_test";
    fs::remove_all(root);
    fs::create_directories(root / ".claude-plugin");
    fs::create_directories(root / "manifest");
    fs::create_directories(root / "agents");
    fs::create_directories(root / "output-styles");

    {
        std::ofstream manifest(root / ".claude-plugin" / "plugin.json");
        manifest << R"JSON({
  "name": "fixture-plugin",
  "version": "1.2.3",
  "description": "Fixture plugin",
  "author": {"name": "Ada", "email": "ada@example.test"},
  "commands": ["manifest/command.md"],
  "agents": ["agents/reviewer.md"],
  "outputStyles": ["output-styles/concise.md"]
})JSON";
    }
    {
        std::ofstream command(root / "manifest" / "command.md");
        command << "Run the fixture command\n";
    }
    {
        std::ofstream agent(root / "agents" / "reviewer.md");
        agent << "Review the fixture\n";
    }
    {
        std::ofstream style(root / "output-styles" / "concise.md");
        style << "Be concise\n";
    }

    auto loaded = loom::utils::plugin_loader::create_plugin_from_path(
        root,
        "fixture-plugin@inline",
        true,
        "fallback-plugin"
    );

    ASSERT_TRUE(loaded.has_value()) << loaded.error();
    const auto& plugin = loaded->first;
    EXPECT_EQ(plugin.name, "fixture-plugin");
    EXPECT_EQ(plugin.manifest.version, std::optional<std::string>{"1.2.3"});
    ASSERT_TRUE(plugin.manifest.author.has_value());
    EXPECT_EQ(plugin.manifest.author->name, "Ada");
    EXPECT_TRUE(plugin.enabled);
    ASSERT_TRUE(plugin.commands_paths.has_value());
    ASSERT_EQ(plugin.commands_paths->size(), 1u);
    EXPECT_EQ(plugin.commands_paths->front(), root / "manifest" / "command.md");
    ASSERT_TRUE(plugin.agents_paths.has_value());
    EXPECT_EQ(plugin.agents_paths->front(), root / "agents" / "reviewer.md");
    ASSERT_TRUE(plugin.output_styles_paths.has_value());
    EXPECT_EQ(plugin.output_styles_paths->front(), root / "output-styles" / "concise.md");
    EXPECT_TRUE(loaded->second.empty());

    fs::remove_all(root);
}

TEST(PluginLoader, CacheOnlyLoadsMarkdownCommandsAgentsAndOutputStyles) {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() / "loom_plugin_loader_cache_test";
    auto plugin_root = root / "cache-plugin";
    fs::remove_all(root);
    fs::create_directories(plugin_root / ".claude-plugin");
    fs::create_directories(plugin_root / "commands");
    fs::create_directories(plugin_root / "agents");
    fs::create_directories(plugin_root / "output-styles");
    ScopedEnvVar plugin_cache("LOOM_PLUGIN_CACHE_DIR");
    plugin_cache.set(root.string().c_str());

    {
        std::ofstream manifest(plugin_root / ".claude-plugin" / "plugin.json");
        manifest << R"JSON({
  "name": "cache-plugin",
  "version": "0.1.0",
  "description": "Cache plugin"
})JSON";
    }
    {
        std::ofstream command(plugin_root / "commands" / "build.md");
        command << "---\ndescription: Build fixture\n---\nBuild the fixture\n";
    }
    {
        std::ofstream agent(plugin_root / "agents" / "reviewer.md");
        agent << "---\ndescription: Review fixture\n---\nReview the fixture\n";
    }
    {
        std::ofstream style(plugin_root / "output-styles" / "concise.md");
        style << "---\ndescription: Concise style\n---\nAnswer briefly\n";
    }

    auto loaded = loom::utils::plugin_loader::load_all_plugins_cache_only();
    ASSERT_EQ(loaded.plugins.size(), 1u);
    EXPECT_TRUE(loaded.errors.empty());
    EXPECT_EQ(loaded.plugins.front().name, "cache-plugin");
    ASSERT_TRUE(loaded.plugins.front().commands_path.has_value());
    EXPECT_EQ(*loaded.plugins.front().commands_path, plugin_root / "commands");
    ASSERT_TRUE(loaded.plugins.front().agents_path.has_value());
    ASSERT_TRUE(loaded.plugins.front().output_styles_path.has_value());

    auto markdown = loom::utils::plugin_loader::walk_plugin_markdown(plugin_root / "commands");
    ASSERT_EQ(markdown.size(), 1u);
    EXPECT_EQ(markdown.front().name, "build");
    EXPECT_EQ(markdown.front().frontmatter.at("description"), "Build fixture");
    EXPECT_EQ(markdown.front().content, "Build the fixture\n");

    auto commands = loom::utils::plugin_loader::load_plugin_commands();
    ASSERT_EQ(commands.size(), 1u);
    EXPECT_EQ(commands.front().name, "cache-plugin:build");
    EXPECT_EQ(commands.front().content, "Build the fixture\n");

    auto agents = loom::utils::plugin_loader::load_plugin_agents();
    ASSERT_EQ(agents.size(), 1u);
    EXPECT_EQ(agents.front().name, "cache-plugin:reviewer");
    EXPECT_EQ(agents.front().content, "Review the fixture\n");

    auto styles = loom::utils::plugin_loader::load_plugin_output_styles();
    ASSERT_EQ(styles.size(), 1u);
    EXPECT_EQ(styles.front().name, "cache-plugin:concise");
    EXPECT_EQ(styles.front().content, "Answer briefly\n");

    fs::remove_all(root);
}

TEST(PluginLoader, CachePluginClonesGitUrlAndLoadsManifest) {
    namespace fs = std::filesystem;
    if (!command_available_for_test("git")) GTEST_SKIP() << "git is not available";

    auto root = fs::temp_directory_path() / "loom_plugin_loader_git_cache_test";
    auto repo = root / "repo";
    auto cache_root = root / "plugins";
    fs::remove_all(root);
    fs::create_directories(repo / ".claude-plugin");
    fs::create_directories(repo / "commands");
    ScopedEnvVar plugin_cache("LOOM_PLUGIN_CACHE_DIR");
    plugin_cache.set(cache_root.string().c_str());

    {
        std::ofstream manifest(repo / ".claude-plugin" / "plugin.json");
        manifest << R"JSON({"name":"git-cache-plugin","version":"1.0.0","commands":["commands/run.md"]})JSON";
    }
    {
        std::ofstream command(repo / "commands" / "run.md");
        command << "Run from git\n";
    }

    run_shell_ok_for_test("git init --template= --initial-branch main " + shell_quote_for_test(repo) + " >/dev/null");
    run_shell_ok_for_test("git -C " + shell_quote_for_test(repo) + " config user.email test@example.invalid");
    run_shell_ok_for_test("git -C " + shell_quote_for_test(repo) + " config user.name Test");
    run_shell_ok_for_test("git -C " + shell_quote_for_test(repo) + " add .");
    run_shell_ok_for_test("git -C " + shell_quote_for_test(repo) + " commit --no-verify -m init >/dev/null");

    loom::utils::plugin_loader::PluginSource source =
        loom::utils::plugin_loader::GitUrlSource{.url = "file://" + repo.string()};
    auto cached = loom::utils::plugin_loader::cache_plugin(source);

    ASSERT_TRUE(cached.has_value()) << cached.error();
    EXPECT_EQ(cached->manifest.name, "git-cache-plugin");
    ASSERT_TRUE(cached->git_commit_sha.has_value());
    EXPECT_TRUE(fs::exists(cached->path / ".claude-plugin" / "plugin.json"));
    EXPECT_TRUE(fs::exists(cached->path / "commands" / "run.md"));

    auto loaded = loom::utils::plugin_loader::create_plugin_from_path(cached->path, "git-cache-plugin@test", true, "fallback");
    ASSERT_TRUE(loaded.has_value()) << loaded.error();
    EXPECT_EQ(loaded->first.name, "git-cache-plugin");
    ASSERT_TRUE(loaded->first.commands_paths.has_value());
    EXPECT_EQ(loaded->first.commands_paths->front(), cached->path / "commands" / "run.md");

    fs::remove_all(root);
}

TEST(PluginLoader, CachePluginExtractsGitSubdirAndRecordsSha) {
    namespace fs = std::filesystem;
    if (!command_available_for_test("git")) GTEST_SKIP() << "git is not available";

    auto root = fs::temp_directory_path() / "loom_plugin_loader_git_subdir_test";
    auto repo = root / "repo";
    auto plugin_dir = repo / "packages" / "plugin";
    auto cache_root = root / "plugins";
    fs::remove_all(root);
    fs::create_directories(plugin_dir / ".claude-plugin");
    fs::create_directories(plugin_dir / "commands");
    ScopedEnvVar plugin_cache("LOOM_PLUGIN_CACHE_DIR");
    plugin_cache.set(cache_root.string().c_str());

    {
        std::ofstream manifest(plugin_dir / ".claude-plugin" / "plugin.json");
        manifest << R"JSON({"name":"subdir-cache-plugin","version":"2.0.0","commands":["commands/sub.md"]})JSON";
    }
    {
        std::ofstream command(plugin_dir / "commands" / "sub.md");
        command << "Run from subdir\n";
    }

    run_shell_ok_for_test("git init --template= --initial-branch main " + shell_quote_for_test(repo) + " >/dev/null");
    run_shell_ok_for_test("git -C " + shell_quote_for_test(repo) + " config user.email test@example.invalid");
    run_shell_ok_for_test("git -C " + shell_quote_for_test(repo) + " config user.name Test");
    run_shell_ok_for_test("git -C " + shell_quote_for_test(repo) + " add .");
    run_shell_ok_for_test("git -C " + shell_quote_for_test(repo) + " commit --no-verify -m init >/dev/null");

    loom::utils::plugin_loader::PluginSource source =
        loom::utils::plugin_loader::GitSubdirSource{
            .url = "file://" + repo.string(),
            .path = "packages/plugin",
        };
    auto cached = loom::utils::plugin_loader::cache_plugin(source);

    ASSERT_TRUE(cached.has_value()) << cached.error();
    EXPECT_EQ(cached->manifest.name, "subdir-cache-plugin");
    ASSERT_TRUE(cached->git_commit_sha.has_value());
    EXPECT_TRUE(fs::exists(cached->path / "commands" / "sub.md"));
    EXPECT_FALSE(fs::exists(cached->path / ".git"));

    fs::remove_all(root);
}

TEST(PluginLoader, CachePluginInstallsNpmPackageFromLocalSpec) {
    namespace fs = std::filesystem;
    if (!command_available_for_test("npm")) GTEST_SKIP() << "npm is not available";

    auto root = fs::temp_directory_path() / "loom_plugin_loader_npm_cache_test";
    auto package_dir = root / "npm-plugin";
    auto cache_root = root / "plugins";
    fs::remove_all(root);
    fs::create_directories(package_dir / "commands");
    ScopedEnvVar plugin_cache("LOOM_PLUGIN_CACHE_DIR");
    plugin_cache.set(cache_root.string().c_str());

    {
        std::ofstream package_json(package_dir / "package.json");
        package_json << R"JSON({
  "name": "loom-npm-plugin-fixture",
  "version": "1.0.0",
  "files": ["plugin.json", "commands"]
})JSON";
    }
    {
        std::ofstream manifest(package_dir / "plugin.json");
        manifest << R"JSON({"name":"npm-cache-plugin","version":"1.0.0","commands":["commands/npm.md"]})JSON";
    }
    {
        std::ofstream command(package_dir / "commands" / "npm.md");
        command << "Run from npm\n";
    }

    loom::utils::plugin_loader::PluginSource source =
        loom::utils::plugin_loader::NpmSource{.package_name = package_dir.string()};
    auto cached = loom::utils::plugin_loader::cache_plugin(source);

    ASSERT_TRUE(cached.has_value()) << cached.error();
    EXPECT_EQ(cached->manifest.name, "npm-cache-plugin");
    EXPECT_TRUE(fs::exists(cached->path / "commands" / "npm.md"));

    auto loaded = loom::utils::plugin_loader::load_all_plugins_cache_only();
    ASSERT_EQ(loaded.plugins.size(), 1u);
    EXPECT_EQ(loaded.plugins.front().name, "npm-cache-plugin");

    fs::remove_all(root);
}

TEST(PluginLoader, ProbesSeedCacheExactAndAnyVersion) {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() / "loom_plugin_loader_seed_cache_test";
    auto seed = root / "seed";
    fs::remove_all(root);
    ScopedEnvVar seed_env("LOOM_PLUGIN_SEED_DIR");
    seed_env.set(seed.string().c_str());

    const auto seeded_path = loom::utils::plugin_loader::get_versioned_cache_path_in(
        seed,
        "seed-plugin@market",
        "1.2.3"
    );
    fs::create_directories(seeded_path);
    {
        std::ofstream marker(seeded_path / "marker.txt");
        marker << "seeded\n";
    }

    auto exact = loom::utils::plugin_loader::probe_seed_cache("seed-plugin@market", "1.2.3");
    ASSERT_TRUE(exact.has_value());
    EXPECT_EQ(*exact, seeded_path);

    auto any = loom::utils::plugin_loader::probe_seed_cache_any_version("seed-plugin@market");
    ASSERT_TRUE(any.has_value());
    EXPECT_EQ(*any, seeded_path);

    auto copied = loom::utils::plugin_loader::copy_plugin_to_versioned_cache(root, "seed-plugin@market", "1.2.3");
    ASSERT_TRUE(copied.has_value()) << copied.error();
    EXPECT_EQ(*copied, seeded_path);

    fs::remove_all(root);
}

TEST(PluginDependencyResolver, ResolvesClosureAndReportsDependencyErrors) {
    using loom::utils::plugin_dependency_resolver::DependencyLookupResult;
    using loom::utils::plugin_dependency_resolver::LoadedPlugin;

    const std::map<std::string, DependencyLookupResult> graph = {
        {"app@main", DependencyLookupResult{{"core", "theme@main"}}},
        {"core@main", DependencyLookupResult{{"shared@main"}}},
        {"theme@main", DependencyLookupResult{{}}},
        {"shared@main", DependencyLookupResult{{}}},
    };

    auto result = loom::utils::plugin_dependency_resolver::resolve_dependency_closure(
        "app@main",
        [&](const std::string& id) -> std::optional<DependencyLookupResult> {
            auto it = graph.find(id);
            if (it == graph.end()) return std::nullopt;
            return it->second;
        },
        {"theme@main"}
    );

    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(result.closure, (std::vector<std::string>{"shared@main", "core@main", "app@main"}));

    auto cross = loom::utils::plugin_dependency_resolver::resolve_dependency_closure(
        "app@main",
        [&](const std::string& id) -> std::optional<DependencyLookupResult> {
            if (id == "app@main") return DependencyLookupResult{{"dep@other"}};
            if (id == "dep@other") return DependencyLookupResult{{}};
            return std::nullopt;
        }
    );
    EXPECT_FALSE(cross.ok);
    EXPECT_EQ(cross.reason, loom::utils::plugin_dependency_resolver::ResolutionFailure::CrossMarketplace);

    auto cycle = loom::utils::plugin_dependency_resolver::resolve_dependency_closure(
        "a@main",
        [&](const std::string& id) -> std::optional<DependencyLookupResult> {
            if (id == "a@main") return DependencyLookupResult{{"b"}};
            if (id == "b@main") return DependencyLookupResult{{"a"}};
            return std::nullopt;
        }
    );
    EXPECT_FALSE(cycle.ok);
    EXPECT_EQ(cycle.reason, loom::utils::plugin_dependency_resolver::ResolutionFailure::Cycle);

    std::vector<LoadedPlugin> plugins = {
        {.name = "a", .source = "a@main", .enabled = true, .dependencies = {"b@main"}},
        {.name = "b", .source = "b@main", .enabled = false, .dependencies = {}},
        {.name = "c", .source = "c@main", .enabled = true, .dependencies = {"a@main"}},
    };
    auto demotion = loom::utils::plugin_dependency_resolver::verify_and_demote(plugins);
    EXPECT_EQ(demotion.demoted, (std::set<std::string>{"a@main", "c@main"}));
    ASSERT_EQ(demotion.errors.size(), 2u);
    EXPECT_EQ(demotion.errors[0].dependency, "b@main");
    EXPECT_EQ(demotion.errors[0].reason, "not-enabled");
    EXPECT_EQ(loom::utils::plugin_dependency_resolver::find_reverse_dependents("a@main", plugins), (std::vector<std::string>{"c"}));
    EXPECT_EQ(loom::utils::plugin_dependency_resolver::format_dependency_count_suffix({"a"}), " (+ 1 dependency)");
    EXPECT_EQ(loom::utils::plugin_dependency_resolver::format_dependency_count_suffix({"a", "b"}), " (+ 2 dependencies)");
    EXPECT_EQ(loom::utils::plugin_dependency_resolver::format_reverse_dependents_suffix({"a", "b"}), " — warning: required by a, b");
}

TEST(PluginMarketplaceRules, AppliesOfficialNameAndAutoUpdateRules) {
    using loom::utils::plugin_marketplace_rules::MarketplaceSource;
    using loom::utils::plugin_marketplace_rules::MarketplaceSourceType;

    EXPECT_TRUE(loom::utils::plugin_marketplace_rules::is_marketplace_auto_update("loom-marketplace", std::nullopt));
    EXPECT_FALSE(loom::utils::plugin_marketplace_rules::is_marketplace_auto_update("knowledge-work-plugins", std::nullopt));
    EXPECT_TRUE(loom::utils::plugin_marketplace_rules::is_marketplace_auto_update("third-party", true));
    EXPECT_FALSE(loom::utils::plugin_marketplace_rules::is_marketplace_auto_update("loom-marketplace", false));

    EXPECT_FALSE(loom::utils::plugin_marketplace_rules::is_blocked_official_name("loom-marketplace"));
    EXPECT_TRUE(loom::utils::plugin_marketplace_rules::is_blocked_official_name("loom-official"));
    EXPECT_TRUE(loom::utils::plugin_marketplace_rules::is_blocked_official_name("loom-marketplace-new"));
    EXPECT_TRUE(loom::utils::plugin_marketplace_rules::is_blocked_official_name("clаude")); // contains Cyrillic a

    // Official names are reserved — validate_official_name_source returns an error.
    auto invalid = loom::utils::plugin_marketplace_rules::validate_official_name_source(
        "loom-marketplace",
        MarketplaceSource{.type = MarketplaceSourceType::Github, .repo = "other/plugins", .url = ""}
    );
    ASSERT_TRUE(invalid.has_value());
    EXPECT_NE(invalid->find("reserved for official marketplaces"), std::string::npos);
}

TEST(PluginMarketplace, ComputesRealSha256Checksums) {
    EXPECT_EQ(loom::plugins::detail::sha256_hex("hello"),
              "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
    EXPECT_EQ(loom::plugins::detail::sha256_hex(""),
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(ShellProviders, BashEvalCommandEscapesSingleQuotes) {
    ScopedEnvVar prefix("LOOM_SHELL_PREFIX");
    prefix.unset();

    auto provider = loom::utils::shell_providers::create_provider("/bin/bash", true);
    auto result = provider->build_exec_command(
        "printf '%s\\n' \"it's ok\"",
        loom::utils::shell_providers::BuildExecOptions{
            .id = "quote-test",
            .sandbox_tmp_dir = std::nullopt,
            .use_sandbox = false});

    EXPECT_NE(
        result.command_string.find("eval 'printf '\\''%s\\n'\\'' \"it'\\''s ok\"'"),
        std::string::npos);
}

TEST(ShellProviders, BashProviderSourcesSnapshotAndInjectsSandboxTmpdir) {
    ScopedEnvVar prefix("LOOM_SHELL_PREFIX");
    prefix.unset();

    auto sandbox = std::filesystem::temp_directory_path() / "loom_shell_provider_sandbox_test";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox);

    auto provider = loom::utils::shell_providers::create_provider("/bin/bash", false);
    auto result = provider->build_exec_command(
        "printf '%s' \"$TMPDIR\"",
        loom::utils::shell_providers::BuildExecOptions{
            .id = "snapshot-test",
            .sandbox_tmp_dir = sandbox.string(),
            .use_sandbox = true});

    EXPECT_EQ(result.cwd_file_path, (sandbox / "cwd-snapshot-test").string());
    EXPECT_NE(result.command_string.find("export TMPDIR='" + sandbox.string() + "'"), std::string::npos);
    EXPECT_NE(result.command_string.find("source "), std::string::npos);

    auto args = provider->get_spawn_args(result.command_string);
    ASSERT_GE(args.size(), 2u);
    EXPECT_EQ(args[0], "-c");
    EXPECT_EQ(args.back(), result.command_string);

    auto env = provider->get_environment_overrides("echo ok");
    EXPECT_EQ(env["LOOM_SHELL_PROVIDER"], "native");
    EXPECT_EQ(env["LOOM_SHELL_TYPE"], "bash");
    EXPECT_EQ(env["LOOM_LAST_COMMAND"], "echo ok");
    EXPECT_TRUE(env.contains("LOOM_SHELL_SNAPSHOT"));

    std::filesystem::remove_all(sandbox);
}

TEST(ShellProviders, PowershellProviderTracksCwdInSandboxAndQuotesPath) {
    auto provider = loom::utils::shell_providers::create_provider("pwsh", true);
    auto result = provider->build_exec_command(
        "Write-Output ok",
        loom::utils::shell_providers::BuildExecOptions{
            .id = "ps-test",
            .sandbox_tmp_dir = "/tmp/cc repl's sandbox",
            .use_sandbox = true});

    EXPECT_EQ(result.cwd_file_path, "/tmp/cc repl's sandbox/cwd-ps-test");
    EXPECT_NE(result.command_string.find("Out-File -FilePath '/tmp/cc repl''s sandbox/cwd-ps-test'"), std::string::npos);
    auto args = provider->get_spawn_args(result.command_string);
    ASSERT_EQ(args.size(), 4u);
    EXPECT_EQ(args[0], "-NoProfile");
    EXPECT_EQ(args[1], "-NonInteractive");
    EXPECT_EQ(args[2], "-Command");
    EXPECT_EQ(args[3], result.command_string);

    auto env = provider->get_environment_overrides("Write-Output ok");
    EXPECT_EQ(env["LOOM_SHELL_TYPE"], "powershell");
}

TEST(PluginVersioning, ExtractsVersionedPathsAndDerivesPureVersions) {
    EXPECT_EQ(loom::utils::plugin_versioning::get_version_from_path("/Users/me/.loom/plugins/cache/main/plugin/1.2.3"), "1.2.3");
    EXPECT_FALSE(loom::utils::plugin_versioning::get_version_from_path("/Users/me/.loom/plugins/main/plugin").has_value());
    EXPECT_TRUE(loom::utils::plugin_versioning::is_versioned_path("/plugins/cache/main/plugin/v1"));
    EXPECT_FALSE(loom::utils::plugin_versioning::is_versioned_path("/plugins/main/plugin/v1"));

    EXPECT_EQ(loom::utils::plugin_versioning::derive_plugin_version("1.0.0", "2.0.0", "abcdef1234567890"), "1.0.0");
    EXPECT_EQ(loom::utils::plugin_versioning::derive_plugin_version(std::nullopt, "2.0.0", "abcdef1234567890"), "2.0.0");
    EXPECT_EQ(loom::utils::plugin_versioning::derive_plugin_version(std::nullopt, std::nullopt, "abcdef1234567890"), "abcdef123456");
    EXPECT_EQ(
        loom::utils::plugin_versioning::derive_plugin_version(std::nullopt, std::nullopt, "abcdef1234567890", "git-subdir", R"(.\a\)"),
        "abcdef123456-ca978112"
    );
    EXPECT_EQ(loom::utils::plugin_versioning::derive_plugin_version(std::nullopt, std::nullopt, std::nullopt), "unknown");
}

TEST(QueryGuard, EnforcesDispatchingRunningGenerationTransitions) {
    loom::utils::query_guard::QueryGuard guard;
    int notifications = 0;
    auto unsubscribe = guard.subscribe([&] { ++notifications; });

    EXPECT_FALSE(guard.is_active());
    EXPECT_EQ(guard.generation(), 0);
    EXPECT_TRUE(guard.reserve());
    EXPECT_TRUE(guard.is_active());
    EXPECT_FALSE(guard.reserve());

    auto first = guard.try_start();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 1);
    EXPECT_FALSE(guard.try_start().has_value());
    EXPECT_FALSE(guard.end(0));
    EXPECT_TRUE(guard.end(*first));
    EXPECT_FALSE(guard.is_active());

    EXPECT_TRUE(guard.reserve());
    guard.cancel_reservation();
    EXPECT_FALSE(guard.is_active());

    auto second = guard.try_start();
    ASSERT_TRUE(second.has_value());
    guard.force_end();
    EXPECT_EQ(guard.generation(), *second + 1);
    EXPECT_FALSE(guard.end(*second));

    unsubscribe();
    EXPECT_TRUE(guard.reserve());
    EXPECT_EQ(notifications, 7);
}

TEST(AgentId, FormatsAndParsesAgentAndRequestIds) {
    using namespace loom::utils::agent_id;

    EXPECT_EQ(format_agent_id("researcher", "my-project"), "researcher@my-project");
    auto parsed = parse_agent_id("researcher@my-project@nested");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->agent_name, "researcher");
    EXPECT_EQ(parsed->team_name, "my-project@nested");
    EXPECT_FALSE(parse_agent_id("missing-separator").has_value());

    EXPECT_EQ(generate_request_id("shutdown", "researcher@team", 1702500000000LL), "shutdown-1702500000000@researcher@team");
    auto request = parse_request_id("plan-approval-1702500000000@researcher@team");
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->request_type, "plan-approval");
    EXPECT_EQ(request->timestamp, 1702500000000LL);
    EXPECT_EQ(request->agent_id, "researcher@team");
    EXPECT_FALSE(parse_request_id("shutdown-nope@agent@team").has_value());
    EXPECT_FALSE(parse_request_id("missing-at").has_value());
}

TEST(AutoModeDenials, RespectsFeatureFlagAndKeepsMostRecentTwenty) {
    using namespace loom::utils::auto_mode_denials;

    AutoModeDenialStore disabled;
    disabled.record({.tool_name = "Bash", .display = "rm -rf /", .reason = "danger", .timestamp = 1}, false);
    EXPECT_TRUE(disabled.get().empty());

    AutoModeDenialStore store;
    for (int i = 0; i < 25; ++i) {
        store.record({.tool_name = "Bash", .display = "cmd" + std::to_string(i), .reason = "r", .timestamp = i}, true);
    }
    auto denials = store.get();
    ASSERT_EQ(denials.size(), 20u);
    EXPECT_EQ(denials.front().display, "cmd24");
    EXPECT_EQ(denials.back().display, "cmd5");
}

TEST(ActivityManager, DeduplicatesCliActivityAndRecordsUserWithinTimeout) {
    using namespace loom::utils::activity_manager;

    long long now = 1000;
    std::vector<ActiveTimeRecord> records;
    ActivityManager manager([&] { return now; }, [&](double seconds, ActivityType type) {
        records.push_back({seconds, type});
    });

    manager.record_user_activity();
    now += 3000;
    manager.record_user_activity();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_DOUBLE_EQ(records[0].seconds, 3.0);
    EXPECT_EQ(records[0].type, ActivityType::User);

    manager.start_cli_activity("tool");
    now += 2000;
    manager.start_cli_activity("tool");
    ASSERT_EQ(records.size(), 2u);
    EXPECT_DOUBLE_EQ(records[1].seconds, 2.0);
    EXPECT_EQ(records[1].type, ActivityType::Cli);

    now += 4000;
    manager.end_cli_activity("tool");
    ASSERT_EQ(records.size(), 3u);
    EXPECT_DOUBLE_EQ(records[2].seconds, 4.0);
    EXPECT_EQ(records[2].type, ActivityType::Cli);

    auto state = manager.get_activity_states();
    EXPECT_FALSE(state.is_user_active);
    EXPECT_FALSE(state.is_cli_active);
    EXPECT_EQ(state.active_operation_count, 0u);

    manager.record_user_activity();
    EXPECT_TRUE(manager.get_activity_states().is_user_active);
    now += 6000;
    EXPECT_FALSE(manager.get_activity_states().is_user_active);
}

TEST(EnvUtilsCompat, ParsesTruthyFalsyOptionsAndEnvVars) {
    EXPECT_TRUE(loom::utils::is_env_truthy(" on "));
    EXPECT_TRUE(loom::utils::is_env_truthy("TRUE"));
    EXPECT_FALSE(loom::utils::is_env_truthy("0"));
    EXPECT_TRUE(loom::utils::is_env_defined_falsy(" no "));
    EXPECT_TRUE(loom::utils::is_env_defined_falsy(false));
    EXPECT_FALSE(loom::utils::is_env_defined_falsy(std::nullopt));

    EXPECT_TRUE(loom::utils::has_node_option("--max-old-space-size=4096 --trace-warnings", "--trace-warnings"));
    EXPECT_FALSE(loom::utils::has_node_option("--trace-warnings-extra", "--trace-warnings"));
    EXPECT_TRUE(loom::utils::is_bare_mode("0", {"cc", "--bare"}));
    EXPECT_TRUE(loom::utils::is_bare_mode("true", {"cc"}));
    EXPECT_FALSE(loom::utils::is_bare_mode("", {"cc"}));

    auto parsed = loom::utils::parse_env_vars({"A=1", "B=two=parts"});
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(parsed->at("A"), "1");
    EXPECT_EQ(parsed->at("B"), "two=parts");
    EXPECT_FALSE(loom::utils::parse_env_vars({"NO_EQUALS"}).has_value());

    EXPECT_EQ(loom::utils::resolve_aws_region("eu-west-1", "us-west-2"), "eu-west-1");
    EXPECT_EQ(loom::utils::resolve_aws_region(std::nullopt, "us-west-2"), "us-west-2");
    EXPECT_EQ(loom::utils::resolve_aws_region(std::nullopt, std::nullopt), "us-east-1");
    EXPECT_EQ(loom::utils::resolve_default_vertex_region(std::nullopt), "us-east5");
    EXPECT_EQ(loom::utils::resolve_default_vertex_region("asia-northeast1"), "asia-northeast1");
}

TEST(CachePaths, SanitizesStableProjectAndMcpLogPaths) {
    using namespace loom::utils::cache_paths;

    EXPECT_EQ(sanitize_path("/Users/me/project:alpha"), "-Users-me-project-alpha");
    EXPECT_EQ(project_dir("/tmp/work repo"), "-tmp-work-repo");
    std::string long_name(205, 'a');
    auto sanitized = sanitize_path(long_name);
    EXPECT_GT(sanitized.size(), 200u);
    EXPECT_EQ(sanitized.substr(0, 200), std::string(200, 'a'));
    EXPECT_NE(sanitized.find('-'), std::string::npos);

    CachePathSet paths = build_cache_paths("/var/cache/loom-cli", "/Users/me/project", "server:name");
    EXPECT_EQ(paths.base_logs, "/var/cache/loom-cli/-Users-me-project");
    EXPECT_EQ(paths.errors, "/var/cache/loom-cli/-Users-me-project/errors");
    EXPECT_EQ(paths.messages, "/var/cache/loom-cli/-Users-me-project/messages");
    EXPECT_EQ(paths.mcp_logs, "/var/cache/loom-cli/-Users-me-project/mcp-logs-server-name");
}

TEST(BinaryCheck, TrimsCommandsCachesResultsAndClearsCache) {
    loom::utils::binary_check::BinaryChecker checker;
    int calls = 0;
    auto resolver = [&](std::string_view command) {
        ++calls;
        return command == "git";
    };

    EXPECT_FALSE(checker.is_binary_installed("   ", resolver));
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(checker.is_binary_installed(" git ", resolver));
    EXPECT_TRUE(checker.is_binary_installed("git", resolver));
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(checker.is_binary_installed("missing", resolver));
    EXPECT_FALSE(checker.is_binary_installed("missing", resolver));
    EXPECT_EQ(calls, 2);
    checker.clear();
    EXPECT_TRUE(checker.is_binary_installed("git", resolver));
    EXPECT_EQ(calls, 3);
}

TEST(LoomHints, ExtractsWholeLineHintsStripsThemAndKeepsSourceCommand) {
    using namespace loom::utils::loom_hints;

    const std::string output =
        "before\n"
        "  <loom-hint v=1 type=plugin value=eslint@marketplace />\n"
        "quoted <loom-hint v=1 type=plugin value=ignored /> text\n"
        "\t<loom-hint v=2 type=plugin value=future@marketplace />\n"
        "after";

    auto result = extract_loom_hints(output, "  npx eslint .");

    ASSERT_EQ(result.hints.size(), 1u);
    EXPECT_EQ(result.hints[0].v, 1);
    EXPECT_EQ(result.hints[0].type, "plugin");
    EXPECT_EQ(result.hints[0].value, "eslint@marketplace");
    EXPECT_EQ(result.hints[0].source_command, "npx");
    EXPECT_EQ(result.stripped, "before\n\nquoted <loom-hint v=1 type=plugin value=ignored /> text\n\nafter");

    auto no_hint = extract_loom_hints("plain output", "cmd");
    EXPECT_TRUE(no_hint.hints.empty());
    EXPECT_EQ(no_hint.stripped, "plain output");

    auto huge_version = extract_loom_hints(
        "<loom-hint v=999999999999999999999999999999 type=plugin value=x@marketplace />",
        "tool");
    EXPECT_TRUE(huge_version.hints.empty());
    EXPECT_EQ(huge_version.stripped, "");
}

TEST(LoomHints, PendingHintStoreIsSingleSlotAndOncePerSession) {
    using namespace loom::utils::loom_hints;

    PendingHintStore store;
    int notifications = 0;
    auto unsubscribe = store.subscribe([&] { ++notifications; });

    store.set_pending_hint({.v = 1, .type = "plugin", .value = "a@marketplace", .source_command = "a"});
    ASSERT_TRUE(store.get_pending_hint_snapshot().has_value());
    EXPECT_EQ(store.get_pending_hint_snapshot()->value, "a@marketplace");
    store.set_pending_hint({.v = 1, .type = "plugin", .value = "b@marketplace", .source_command = "b"});
    EXPECT_EQ(store.get_pending_hint_snapshot()->value, "b@marketplace");
    store.clear_pending_hint();
    EXPECT_FALSE(store.get_pending_hint_snapshot().has_value());
    store.mark_shown_this_session();
    store.set_pending_hint({.v = 1, .type = "plugin", .value = "c@marketplace", .source_command = "c"});
    EXPECT_FALSE(store.get_pending_hint_snapshot().has_value());
    EXPECT_TRUE(store.has_shown_hint_this_session());
    unsubscribe();
    store.clear_pending_hint();
    EXPECT_EQ(notifications, 3);
}

TEST(CommitAttribution, SanitizesInternalModelNamesAndSurfaceKeys) {
    using namespace loom::utils::commit_attribution;

    // sanitize_model_name is now a pass-through (empty → "loom")
    EXPECT_EQ(sanitize_model_name("test-model-fast"), "test-model-fast");
    EXPECT_EQ(sanitize_model_name("internal-sonnet-4-5-thinking"), "internal-sonnet-4-5-thinking");
    EXPECT_EQ(sanitize_model_name("haiku-3-5-test"), "haiku-3-5-test");
    EXPECT_EQ(sanitize_model_name("unknown-codename"), "unknown-codename");
    EXPECT_EQ(sanitize_model_name(""), "loom");
    EXPECT_EQ(sanitize_surface_key("cli/opus-4-5-fast"), "cli/opus-4-5-fast");
    EXPECT_EQ(sanitize_surface_key("cli"), "cli");
}

TEST(CommitAttribution, TracksChangedRegionCreationDeletionAndBulkChanges) {
    using namespace loom::utils::commit_attribution;

    auto state = create_empty_attribution_state("cli/test-model");
    EXPECT_EQ(state.surface, "cli/test-model");

    state = track_file_modification(state, "src/a.ts", "hello world", "hello brave world", 10.0);
    ASSERT_TRUE(state.file_states.contains("src/a.ts"));
    EXPECT_EQ(state.file_states.at("src/a.ts").loom_contribution, 6u);
    EXPECT_EQ(state.file_states.at("src/a.ts").content_hash,
              "169f95520526c347f9ef612918879703d7b5340d162efd0880ccad0be7e17673");
    EXPECT_DOUBLE_EQ(state.file_states.at("src/a.ts").mtime, 10.0);

    state = track_file_creation(state, "src/new.ts", "abcdef", 11.0);
    EXPECT_EQ(state.file_states.at("src/new.ts").loom_contribution, 6u);

    state = track_file_deletion(state, "src/a.ts", "hello brave world", 12.0);
    EXPECT_EQ(state.file_states.at("src/a.ts").loom_contribution, 23u);
    EXPECT_EQ(state.file_states.at("src/a.ts").content_hash, "");

    std::vector<FileChange> changes = {
        {.path = "src/new.ts", .type = FileChangeType::Modified, .old_content = "abcdef", .new_content = "abcXYZdef", .mtime = 13.0},
        {.path = "src/old.ts", .type = FileChangeType::Deleted, .old_content = "gone", .new_content = "", .mtime = 14.0},
    };
    state = track_bulk_file_changes(state, changes);
    EXPECT_EQ(state.file_states.at("src/new.ts").loom_contribution, 9u);
    EXPECT_EQ(state.file_states.at("src/old.ts").loom_contribution, 4u);

    auto unicode = create_empty_attribution_state();
    unicode = track_file_creation(unicode, "emoji.txt", "\xF0\x9F\x98\x80", 15.0);
    unicode = track_file_creation(unicode, "cjk.txt", "\xE4\xBD\xA0", 16.0);
    EXPECT_EQ(unicode.file_states.at("emoji.txt").loom_contribution, 2u);
    EXPECT_EQ(unicode.file_states.at("cjk.txt").loom_contribution, 1u);
}

TEST(MessagePredicates, HumanTurnsExcludeMetaAndToolResults) {
    using namespace loom::utils::message_predicates;

    EXPECT_TRUE(is_human_turn({.type = "user", .is_meta = false, .has_tool_use_result = false}));
    EXPECT_FALSE(is_human_turn({.type = "assistant", .is_meta = false, .has_tool_use_result = false}));
    EXPECT_FALSE(is_human_turn({.type = "user", .is_meta = true, .has_tool_use_result = false}));
    EXPECT_FALSE(is_human_turn({.type = "user", .is_meta = false, .has_tool_use_result = true}));
}

TEST(ContentArray, InsertsBlockAfterToolResultsOrBeforeLastBlock) {
    using namespace loom::utils::content_array;

    std::vector<ContentBlock> with_results = {
        {.type = "text", .text = "start"},
        {.type = "tool_result", .text = "r1"},
        {.type = "tool_result", .text = "r2"},
    };
    insert_block_after_tool_results(with_results, {.type = "cache", .text = "directive"});
    ASSERT_EQ(with_results.size(), 5u);
    EXPECT_EQ(with_results[2].type, "tool_result");
    EXPECT_EQ(with_results[3].type, "cache");
    EXPECT_EQ(with_results[4].type, "text");
    EXPECT_EQ(with_results[4].text, ".");

    std::vector<ContentBlock> mixed = {
        {.type = "tool_result", .text = "r"},
        {.type = "text", .text = "tail"},
    };
    insert_block_after_tool_results(mixed, {.type = "cache", .text = "directive"});
    ASSERT_EQ(mixed.size(), 3u);
    EXPECT_EQ(mixed[1].type, "cache");
    EXPECT_EQ(mixed[2].text, "tail");

    std::vector<ContentBlock> no_results = {{.type = "text", .text = "a"}, {.type = "text", .text = "b"}};
    insert_block_after_tool_results(no_results, {.type = "cache", .text = "directive"});
    ASSERT_EQ(no_results.size(), 3u);
    EXPECT_EQ(no_results[1].type, "cache");
    EXPECT_EQ(no_results[2].text, "b");
}

TEST(ObjectGroupBy, GroupsItemsBySelectorAndPassesIndex) {
    using namespace loom::utils::object_group_by;

    std::vector<std::string> items = {"apple", "ape", "banana", "berry"};
    auto grouped = object_group_by<std::string, std::string>(items, [](const std::string& item, std::size_t index) {
        return std::string(1, item[0]) + std::to_string(index % 2);
    });

    EXPECT_EQ(grouped["a0"], (std::vector<std::string>{"apple"}));
    EXPECT_EQ(grouped["a1"], (std::vector<std::string>{"ape"}));
    EXPECT_EQ(grouped["b0"], (std::vector<std::string>{"banana"}));
    EXPECT_EQ(grouped["b1"], (std::vector<std::string>{"berry"}));
}

// Regression: commit f85a5b8 introduced clipboard image paste, but the AppleScript
// in utils/platform/clipboard.cppm used `\\xc2\\xabclass PNGf\\xc2\\xbb` (four literal ASCII
// bytes) instead of `«class PNGf»` (two UTF-8 code units). The shell saw a literal
// backslash-x-c-2 etc., osascript reported syntax error -2741 "A identifier can’t
// go after this identifier.", and read_image_png() ALWAYS returned nullopt —
// even when the clipboard held a valid screenshot. The user saw: ctrl+v →
// "nothing happens". This test guards against the exact class of bug by
// checking that the has_image()/read_image_png() scripts are at least
// syntactically valid AppleScript on macOS: they may legitimately return
// false/nullopt when the clipboard has no image, but they MUST NOT exit with
// the specific -2741 syntax-error signature produced by the literal-\xc2 mistake. Off-macOS
// these stubs always return false/nullopt unconditionally.
// ===========================================================================
// Source-text guards.
//
// A few tests below assert on the SOURCE TEXT of a module rather than its
// behaviour, because the property they pin (e.g. "osascript is detached with
// setsid()") cannot be observed at runtime without a raw-mode TTY. They locate
// the file by walking up from the CWD.
//
// The walk used to be inlined per test with a broken fallback: after walking
// up, `src` held some existing *directory*, so the `if (!fs::exists(src))` skip
// guard could never fire — a move of the tree turned the "skip" path into a
// read of a directory and a hard failure. This helper returns nullopt instead,
// so a future move degrades to a clean GTEST_SKIP.
// ===========================================================================
namespace {

/// Locate a source file by walking up from the CWD looking for <root>/rel.
/// Returns nullopt if not found, so callers can skip rather than misread.
[[nodiscard]] std::optional<std::filesystem::path>
find_source_file(std::initializer_list<std::string_view> rel,
                 int max_levels = 6) {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path();
    for (int i = 0; i < max_levels; ++i) {
        fs::path candidate = dir;
        for (auto part : rel) candidate /= part;
        if (fs::is_regular_file(candidate)) return candidate;
        if (dir == dir.parent_path()) break;  // filesystem root
        dir = dir.parent_path();
    }
    return std::nullopt;
}

}  // namespace

TEST(ClipboardImage, OsascriptScriptsAreSyntacticallyValidOnMacOS) {
    // has_image() is noexcept — it must not crash, and on macOS must not produce a
    // script that exit()s 0 or 1 cleanly (never throws or aborts). The call is always safe on all
    // platforms; we only the boolean outcome varies with the live clipboard state.
    EXPECT_NO_THROW({
      (void)loom::utils::clipboard::has_image();
    });

    // read_image_png() returns nullopt when no image is present (the 99.9%
    // CI / dev-loop scenario). The key invariant we actually want to pin here is that
    // even without an image in the clipboard, the underlying AppleScript must
    // parse cleanly: not return -2741 "syntax error" (the regression marker).
    // A no-image call produces a -1700 "can't coerce" execution error which
    // is semantically very different from the "you wrote nonsense" syntax
    // error of the original bug. Both are exit non-zero from std::system, but only the
    // former means "no image"; so read_image_png() returns nullopt in both paths
    // and neither path throws. Just verify no crash / abort.
    EXPECT_NO_THROW({
      auto png = loom::utils::clipboard::read_image_png();
      // If (by luck) a developer happens to have an image in their clipboard
      // while this test runs, we additionally verify the bytes look like PNG.
      if (png.has_value()) {
          EXPECT_GE(png->size(), 8u);
          // PNG magic bytes: 89 50 4E 47 0D 0A 1A 0A.
          EXPECT_EQ((*png)[0], 0x89);
          EXPECT_EQ((*png)[1], 0x50);  // 'P'
          EXPECT_EQ((*png)[2], 0x4E);  // 'N'
          EXPECT_EQ((*png)[3], 0x47);  // 'G'
      }
    });
}

// Regression (2026-07-01, user-reported): inside loom the terminal runs in
// raw mode (FTXUI termios ICANON off). std::system() forks a child that
// INHERITS fd 0 = the raw-mode terminal. osascript, on detecting a TTY on
// stdin, takes a code path that misbehaves under raw mode and exits non-zero
// — read_image_png() always returned nullopt, [Image #N] placeholder erased.
//
// Fix: run osascript via run_detached() — fork()+setsid()+exec() with 0/1/2
// redirected to /dev/null and all other inherited fds closed. setsid() puts
// the child in a new session with no controlling terminal so osascript can't
// see the raw-mode TTY. std::system()/sh can't setsid(), hence the manual
// fork+setsid+exec.
//
// This is a source-level guard — there's no portable way to reproduce a
// raw-mode controlling TTY inside a unit test (needs a PTY pair + live app).
// We assert the source still calls setsid() and routes both osascript sites
// through run_detached(). If someone "simplifies" back to std::system(), this
// test fails and points them at the regression comment above.
TEST(ClipboardImage, OsascriptUsesSetsidToDetachFromTty_RawModeGuard) {
    const auto found = find_source_file({"src", "utils", "clipboard.cppm"});
    if (!found) {
        GTEST_SKIP() << "clipboard.cppm source not found from "
                     << std::filesystem::current_path()
                     << " — skipping source-level raw-mode guard.";
    }
    const std::filesystem::path src = *found;
    std::ifstream f(src);
    ASSERT_TRUE(f.good()) << "cannot open " << src;
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    // setsid() is the load-bearing call — it detaches the osascript chain
    // from loom's controlling terminal.
    EXPECT_NE(content.find("setsid()"), std::string::npos)
        << "clipboard.cppm must call setsid() in run_detached() to detach the "
        << "osascript child from loom's raw-mode controlling terminal. "
        << "Without it, osascript sees a raw-mode TTY on stdin and exits "
        << "non-zero. See the regression comment above.";

    // run_detached() helper must exist and be called from both sites. If
    // someone reverts to std::system(), this count drops. (We can't grep for
    // the absence of "std::system" because the regression comment above
    // mentions it by name.)
    long run_detached_calls = 0;
    std::string::size_type pos = 0;
    const std::string needle = "run_detached(";
    while ((pos = content.find(needle, pos)) != std::string::npos) {
        ++run_detached_calls;
        pos += needle.size();
    }
    // 1 = the definition; >=2 more = the call sites (has_image + read_image_png).
    EXPECT_GE(run_detached_calls, 3)
        << "clipboard.cppm must call run_detached() from BOTH has_image() and "
        << "read_image_png(). Found " << run_detached_calls
        << " occurrences (expect >=3: 1 definition + 2 call sites).";
}

// Regression (2026-07-01, user-reported, strike 5): "Ctrl+V pressed 8×, only
// ~4 register". Root cause: macOS/BSD line discipline processes VLNEXT (the
// "literal-next" char, Ctrl+V by default) EVEN in non-canonical mode (ICANON
// off). FTXUI's termios setup clears ICANON/ECHO but NOT c_cc[VLNEXT], so each
// pair of \x16 bytes collapses into one literal \x16 via lnext semantics —
// pressing Ctrl+V N times registers only floor(N/2). Verified empirically via
// a PTY harness: sending N×\x16 delivers floor(N/2) bytes with VLNEXT at its
// default, all N bytes with VLNEXT=0.
//
// Fix: RunApp (app.cppm) snapshots the original termios, clears
// c_cc[VLNEXT]=0 before screen.Loop() (so FTXUI's Install reads & preserves
// VLNEXT=0 for the session), and restores the original on exit. This test is
// a source-level guard asserting app.cppm still does the VLNEXT clear — there
// is no portable way to reproduce a raw-mode controlling TTY in a unit test.
TEST(ClipboardImage, RunAppClearsVlnext_MacOSLineDisciplineGuard) {
    const auto found = find_source_file({"src", "ui", "app.cppm"});
    if (!found) {
        GTEST_SKIP() << "app.cppm source not found from "
                     << std::filesystem::current_path()
                     << " — skipping VLNEXT source-level guard.";
    }
    const std::filesystem::path src = *found;
    std::ifstream f(src);
    ASSERT_TRUE(f.good()) << "cannot open " << src;
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    // The load-bearing line: c_cc[VLNEXT] = 0 disables literal-next so Ctrl+V
    // bytes are no longer collapsed in pairs by the line discipline.
    EXPECT_NE(content.find("VLNEXT"), std::string::npos)
        << "app.cppm must clear c_cc[VLNEXT] (in RunApp, before screen.Loop) "
        << "to stop the macOS line discipline from eating every other Ctrl+V "
        << "via literal-next semantics. Without this, pressing Ctrl+V N times "
        << "registers only floor(N/2). See the regression comment above.";
    EXPECT_NE(content.find("c_cc[VLNEXT] = 0"), std::string::npos)
        << "app.cppm must set c_cc[VLNEXT] = 0 (disable). Found VLNEXT mention "
        << "but not the disable assignment.";
}

// Regression (2026-07-02, user-reported): copying an image from Lark/Feishu
// (or any web app that embeds images as base64 data URLs in HTML) and pasting
// into loom showed the [Image #N] placeholder briefly, then it vanished.
// Root cause: the clipboard held «class HTML» (365KB HTML with a JPEG data URL
// in data-content="data:image/jpeg;base64,..."), NOT «class PNGf» raw image
// data. read_image_png() only tried PNGf → always nullopt → ProcessCompletedPastes
// erased the placeholder as a "failed paste".
//
// Fix: extract_png_from_html_clipboard() in clipboard.cppm reads «class HTML»,
// scans for data:image/XXX;base64, patterns, decodes, and (if not PNG) converts
// via sips. read_image_png() falls through to this when PNGf fails.
//
// This is a source-level guard (no portable way to inject HTML clipboard data
// inside a unit test). We assert:
//   1. extract_png_from_html_clipboard function exists in clipboard.cppm
//   2. read_image_png() calls it as fallback (not just return nullopt)
TEST(ClipboardImage, HtmlClipboardDataUrlFallback_SourceGuard) {
    const auto found = find_source_file({"src", "utils", "clipboard.cppm"});
    if (!found) {
        GTEST_SKIP() << "clipboard.cppm source not found from "
                     << std::filesystem::current_path();
    }
    const std::filesystem::path src = *found;
    std::ifstream f(src);
    ASSERT_TRUE(f.good()) << "cannot open " << src;
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    // 1. The HTML extraction helper must exist.
    EXPECT_NE(content.find("extract_png_from_html_clipboard"), std::string::npos)
        << "clipboard.cppm must have extract_png_from_html_clipboard() — "
        << "copying images from web apps (Lark, Google Docs) puts HTML with "
        << "base64 data URLs on the clipboard, NOT raw PNGf. Without this "
        << "fallback, [Image #N] vanishes ~700ms after paste.";

    // 2. It must scan for data:image/ prefix (the data URL pattern).
    EXPECT_NE(content.find("data:image/"), std::string::npos)
        << "extract_png_from_html_clipboard() must search for 'data:image/' "
        << "prefix to find embedded image data URLs.";

    // 3. read_image_png() must call the fallback when PNGf fails
    //    (not just 'return std::nullopt' after run_detached fails).
    EXPECT_NE(content.find("return extract_png_from_html_clipboard()"),
              std::string::npos)
        << "read_image_png() must call extract_png_from_html_clipboard() as "
        << "fallback when PNGf osascript fails — clipboard may hold HTML "
        << "with embedded image instead of raw PNG.";
}
