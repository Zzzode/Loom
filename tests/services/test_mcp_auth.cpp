/// @file test_mcp_auth.cpp
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

[[nodiscard]] bool c13_ancestry_clean(const fs::path& base) {
    std::error_code ec;
    for (auto d = base; ; d = d.parent_path()) {
        if (fs::exists(d / ".git", ec)) return false;
        if (d.parent_path() == d || d.parent_path().empty()) return true;
    }
}

[[nodiscard]] std::optional<fs::path> c13_clean_temp_base() {
    std::error_code ec;
    for (const char* var : {"XDG_RUNTIME_DIR", "TMPDIR"}) {
        if (const char* v = std::getenv(var);
            v != nullptr && fs::is_directory(v, ec) && !ec &&
            c13_ancestry_clean(v)) {
            return fs::path(v);
        }
        ec.clear();
    }
    if (fs::is_directory("/dev/shm", ec) && !ec &&
        c13_ancestry_clean("/dev/shm")) {
        return fs::path("/dev/shm");
    }
    if (c13_ancestry_clean(fs::temp_directory_path())) {
        return fs::temp_directory_path();
    }
    return std::nullopt;
}

[[nodiscard]] fs::path c13_make_temp_root(std::string_view name) {
    static std::atomic<unsigned> counter{0};
    const auto base = c13_clean_temp_base();
    const fs::path base_dir = base.value_or(fs::temp_directory_path());
    constexpr int kMaxAttempts = 64;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const fs::path root =
            base_dir /
            (std::string(name) + std::to_string(::getpid()) + "_" +
             std::to_string(counter.fetch_add(1,
                 std::memory_order_relaxed)) + "_" +
             std::to_string(std::chrono::system_clock::now()
                                 .time_since_epoch().count()));
        std::error_code create_ec;
        fs::create_directories(root, create_ec);
        std::error_code probe_ec;
        if (fs::is_directory(root, probe_ec) && !probe_ec) {
            return root;  // created, or a unique name that now exists
        }
        // Collision / transient: take the next unique name.
    }
    // Exhausted (effectively impossible with unique names): return the
    // system temp dir so a failure surfaces as a clear test error rather
    // than a dangling empty path.
    return fs::temp_directory_path();
}

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

int test_hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
    return -1;
}

std::string test_url_decode(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') {
            out.push_back(' ');
        } else if (value[i] == '%' && i + 2 < value.size()) {
            const auto hi = test_hex_value(value[i + 1]);
            const auto lo = test_hex_value(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(value[i]);
            }
        } else {
            out.push_back(value[i]);
        }
    }
    return out;
}

std::optional<std::string> test_query_param(std::string_view url, std::string_view key) {
    auto query_start = url.find('?');
    if (query_start == std::string_view::npos) return std::nullopt;
    auto fragment_start = url.find('#', query_start);
    auto query = url.substr(
        query_start + 1,
        fragment_start == std::string_view::npos ? std::string_view::npos : fragment_start - query_start - 1);
    std::string pattern(key);
    pattern.push_back('=');
    std::size_t pos = 0;
    while (pos < query.size()) {
        auto next = query.find('&', pos);
        auto part = query.substr(pos, next == std::string_view::npos ? std::string_view::npos : next - pos);
        if (part.starts_with(pattern)) return test_url_decode(part.substr(pattern.size()));
        if (next == std::string_view::npos) break;
        pos = next + 1;
    }
    return std::nullopt;
}

std::optional<int> localhost_url_port(std::string_view url) {
    constexpr std::string_view prefix = "http://localhost:";
    if (!url.starts_with(prefix)) return std::nullopt;
    auto port_start = prefix.size();
    auto path_start = url.find('/', port_start);
    if (path_start == std::string_view::npos) return std::nullopt;
    try {
        return std::stoi(std::string(url.substr(port_start, path_start - port_start)));
    } catch (...) {
        return std::nullopt;
    }
}

class LocalOAuthRevocationServer {
public:
    LocalOAuthRevocationServer() {
        server_.Get("/metadata", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(std::format(R"({{
              "authorization_endpoint": "{0}/authorize",
              "token_endpoint": "{0}/token",
              "revocation_endpoint": "{0}/revoke",
              "revocation_endpoint_auth_methods_supported": ["client_secret_post"],
              "scope": "tools"
            }})", base_url()), "application/json");
        });

        server_.Post("/token", [this](const httplib::Request& req, httplib::Response& res) {
            {
                std::lock_guard lock(mutex_);
                token_request_body_ = req.body;
                ++token_request_count_;
            }
            cv_.notify_all();
            if (req.body.find("grant_type=authorization_code") == std::string::npos ||
                req.body.find("code=callback-code") == std::string::npos ||
                req.body.find("client_id=client-1") == std::string::npos ||
                req.body.find("code_verifier=") == std::string::npos ||
                req.body.find("redirect_uri=http%3A%2F%2Flocalhost%3A") == std::string::npos) {
                res.status = 400;
                res.set_content(R"({"error":"invalid_request"})", "application/json");
                return;
            }
            res.set_content(R"({
              "access_token": "callback-access",
              "refresh_token": "callback-refresh",
              "expires_in": 3600,
              "scope": "tools"
            })", "application/json");
        });

        server_.Post("/revoke", [this](const httplib::Request& req, httplib::Response& res) {
            {
                std::lock_guard lock(mutex_);
                revoke_request_bodies_.push_back(req.body);
                revoke_authorization_headers_.push_back(req.get_header_value("Authorization"));
            }
            cv_.notify_all();
            res.status = 200;
            res.set_content(R"({"ok":true})", "application/json");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalOAuthRevocationServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool ready() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    [[nodiscard]] std::string metadata_url() const {
        return base_url() + "/metadata";
    }

    [[nodiscard]] std::string token_request_body() const {
        std::lock_guard lock(mutex_);
        return token_request_body_;
    }

    [[nodiscard]] std::vector<std::string> revoke_request_bodies() const {
        std::lock_guard lock(mutex_);
        return revoke_request_bodies_;
    }

    [[nodiscard]] std::vector<std::string> revoke_authorization_headers() const {
        std::lock_guard lock(mutex_);
        return revoke_authorization_headers_;
    }

    [[nodiscard]] bool wait_for_revoke_requests(
        std::size_t count,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this, count] {
            return revoke_request_bodies_.size() >= count;
        });
    }

    [[nodiscard]] bool wait_for_token_request(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return token_request_count_ > 0; });
    }

private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    int token_request_count_{0};
    std::string token_request_body_;
    std::vector<std::string> revoke_request_bodies_;
    std::vector<std::string> revoke_authorization_headers_;
};

class LocalXaaIdpServer {
public:
    LocalXaaIdpServer() {
        // PRM discovery (RFC 9728): GET /mcp/.well-known/oauth-protected-resource
        server_.Get("/mcp/.well-known/oauth-protected-resource",
            [this](const httplib::Request& req, httplib::Response& res) {
            (void)req;
            {
                std::lock_guard lock(mutex_);
                ++prm_request_count_;
            }
            cv_.notify_all();
            auto base = base_url();
            auto body = std::format(R"({{
                "resource": "{}/mcp",
                "authorization_servers": ["{}"]
            }})", base, base);
            res.set_content(body, "application/json");
        });

        // AS metadata (RFC 8414): GET /.well-known/oauth-authorization-server
        server_.Get("/.well-known/oauth-authorization-server",
            [this](const httplib::Request& req, httplib::Response& res) {
            (void)req;
            {
                std::lock_guard lock(mutex_);
                ++as_metadata_request_count_;
            }
            cv_.notify_all();
            auto base = base_url();
            auto body = std::format(R"({{
                "issuer": "{}",
                "token_endpoint": "{}/token",
                "grant_types_supported": [
                    "urn:ietf:params:oauth:grant-type:jwt-bearer",
                    "urn:ietf:params:oauth:grant-type:token-exchange"
                ],
                "token_endpoint_auth_methods_supported": ["client_secret_basic"]
            }})", base, base);
            res.set_content(body, "application/json");
        });

        // Token endpoint: handles both RFC 8693 (token-exchange) and RFC 7523 (jwt-bearer)
        server_.Post("/token",
            [this](const httplib::Request& req, httplib::Response& res) {
            auto body = req.body;
            {
                std::lock_guard lock(mutex_);
                if (body.find("grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Atoken-exchange")
                    != std::string::npos) {
                    // RFC 8693 Token Exchange (id_token → ID-JAG)
                    ++token_exchange_count_;
                    token_exchange_body_ = body;
                    cv_.notify_all();

                    // Verify expected fields
                    if (body.find("subject_token=fake-id-token-for-testing") == std::string::npos ||
                        body.find("client_id=idp-client-1") == std::string::npos) {
                        res.status = 400;
                        res.set_content(R"({"error":"invalid_request"})", "application/json");
                        return;
                    }

                    res.set_content(R"({
                        "access_token": "fake-id-jag-for-testing",
                        "issued_token_type": "urn:ietf:params:oauth:token-type:id-jag",
                        "expires_in": 3600,
                        "scope": "openid profile mcp"
                    })", "application/json");
                } else if (body.find("grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer")
                    != std::string::npos) {
                    // RFC 7523 JWT Bearer Grant (ID-JAG → access_token)
                    ++jwt_bearer_count_;
                    jwt_bearer_body_ = body;
                    jwt_bearer_auth_header_ = req.get_header_value("Authorization");
                    cv_.notify_all();

                    res.set_content(R"({
                        "access_token": "xaa-access",
                        "refresh_token": "xaa-refresh",
                        "token_type": "Bearer",
                        "expires_in": 3600,
                        "scope": "openid profile mcp"
                    })", "application/json");
                } else {
                    res.status = 400;
                    res.set_content(R"({"error":"unsupported_grant_type"})", "application/json");
                }
            }
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalXaaIdpServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool ready() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    // ── Request counters and bodies for test assertions ──────────────────

    [[nodiscard]] int prm_request_count() const {
        std::lock_guard lock(mutex_);
        return prm_request_count_;
    }

    [[nodiscard]] int as_metadata_request_count() const {
        std::lock_guard lock(mutex_);
        return as_metadata_request_count_;
    }

    [[nodiscard]] int token_exchange_count() const {
        std::lock_guard lock(mutex_);
        return token_exchange_count_;
    }

    [[nodiscard]] int jwt_bearer_count() const {
        std::lock_guard lock(mutex_);
        return jwt_bearer_count_;
    }

    [[nodiscard]] std::string token_exchange_body() const {
        std::lock_guard lock(mutex_);
        return token_exchange_body_;
    }

    [[nodiscard]] std::string jwt_bearer_body() const {
        std::lock_guard lock(mutex_);
        return jwt_bearer_body_;
    }

    [[nodiscard]] std::string jwt_bearer_auth_header() const {
        std::lock_guard lock(mutex_);
        return jwt_bearer_auth_header_;
    }

    [[nodiscard]] bool wait_for_jwt_bearer(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] {
            return jwt_bearer_count_ > 0;
        });
    }

private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    int prm_request_count_{0};
    int as_metadata_request_count_{0};
    int token_exchange_count_{0};
    int jwt_bearer_count_{0};
    std::string token_exchange_body_;
    std::string jwt_bearer_body_;
    std::string jwt_bearer_auth_header_;
};

struct C17IdpTokenCacheGuard {
    fs::path home;
    EnvironmentGuard home_guard;
    fs::path cache_file;

    explicit C17IdpTokenCacheGuard(fs::path new_home)
        : home(std::move(new_home)),
          home_guard("HOME", home.string()),
          cache_file(home / ".config" / "loom" / "xaa" / "idp_tokens.json") {}

    ~C17IdpTokenCacheGuard() {
        std::error_code ec;
        fs::remove(cache_file, ec);
        fs::remove_all(home / ".config", ec);
    }
};

struct UmaskGuard022 {
    mode_t previous;
    UmaskGuard022() : previous(::umask(022)) {}
    ~UmaskGuard022() { ::umask(previous); }
};

[[nodiscard]] mode_t mode_bits(const fs::path& path) {
    struct ::stat st {};
    return (::stat(path.c_str(), &st) == 0) ? static_cast<mode_t>(st.st_mode & 07777u)
                                            : static_cast<mode_t>(0);
}

[[nodiscard]] std::string c20_url_decode(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            auto hex_val = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hex_val(s[i + 1]);
            int lo = hex_val(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>(hi * 16 + lo));
                i += 2;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

[[nodiscard]] std::string c20_extract_query_param(
    std::string_view url, std::string_view key) {
    auto pos = url.find(key);
    if (pos == std::string_view::npos) return {};
    auto start = pos + key.size();
    auto end = url.find('&', start);
    auto value = url.substr(start,
        end == std::string_view::npos ? std::string_view::npos : end - start);
    return c20_url_decode(value);
}

[[nodiscard]] int c20_port_from_redirect_uri(std::string_view redirect_uri) {
    auto colon = redirect_uri.rfind(':');
    if (colon == std::string_view::npos) return 0;
    auto slash = redirect_uri.find('/', colon + 1);
    auto port_str = redirect_uri.substr(colon + 1,
        slash == std::string_view::npos ? std::string_view::npos : slash - colon - 1);
    int port = 0;
    for (char c : port_str) {
        if (c < '0' || c > '9') return 0;
        port = port * 10 + (c - '0');
    }
    return port;
}

class C20C1MockIdpServer {
public:
    explicit C20C1MockIdpServer(std::string secret) : secret_(std::move(secret)) {
        server_.Get("/.well-known/openid-configuration",
            [this](const httplib::Request&, httplib::Response& res) {
                auto base = base_url();
                res.set_content(std::format(R"({{
                    "issuer": "{}",
                    "authorization_endpoint": "{}/authorize",
                    "token_endpoint": "{}/token",
                    "token_endpoint_auth_methods_supported": ["client_secret_basic", "client_secret_post"]
                }})", base, base, base), "application/json");
            });
        server_.Post("/token",
            [this](const httplib::Request&, httplib::Response& res) {
                res.status = 400;
                res.set_content(std::format(
                    R"({{"error":"invalid_request","client_secret":"{}"}})", secret_),
                    "application/json");
            });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~C20C1MockIdpServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }
    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }
private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    std::string secret_;
};

class C20MockTokenEndpoint {
public:
    explicit C20MockTokenEndpoint(std::string secret) : secret_(std::move(secret)) {
        server_.Post("/token",
            [this](const httplib::Request&, httplib::Response& res) {
                res.status = 400;
                res.set_content(std::format(
                    R"({{"error":"invalid_request","client_secret":"{}"}})", secret_),
                    "application/json");
            });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }
    ~C20MockTokenEndpoint() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }
    [[nodiscard]] std::string url() const {
        return std::format("http://127.0.0.1:{}/token", port_);
    }
private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    std::string secret_;
};

} // namespace

TEST(McpAuth, RevokesOAuthTokensViaMetadataEndpointAndClearsLocalStorage) {
    LocalOAuthRevocationServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_revoke_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());

    loom::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = "https://mcp.example.test/mcp";
    auth_config.oauth = loom::services::mcp::McpOAuthConfig{
        .auth_server_metadata_url = server.metadata_url(),
        .client_id = "client-1",
    };
    const auto server_key = loom::services::mcp::get_server_key("revoke-fixture", auth_config);
    auto sanitize_key = [](std::string_view key) {
        std::string sanitized;
        sanitized.reserve(key.size());
        for (unsigned char ch : key) {
            sanitized.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
        }
        return sanitized;
    };
    const auto token_path = root / "loom" / "mcp" / (sanitize_key(server_key) + ".json");
    fs::create_directories(token_path.parent_path());
    const auto expires_at = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 3600;
    {
        std::ofstream token_file(token_path);
        token_file << std::format(R"({{
          "server_name": "revoke-fixture",
          "server_url": "{}",
          "access_token": "old-access",
          "refresh_token": "old-refresh",
          "expires_at": {},
          "scope": "tools",
          "client_id": "client-1",
          "client_secret": "secret-1",
          "discovery_state": {{}}
        }})", *auth_config.url, expires_at);
    }

    auto revoked = loom::services::mcp::revoke_server_tokens("revoke-fixture", auth_config);
    ASSERT_TRUE(revoked.has_value()) << revoked.error().message();
    ASSERT_TRUE(server.wait_for_revoke_requests(2));

    const auto bodies = server.revoke_request_bodies();
    ASSERT_EQ(bodies.size(), 2u);
    EXPECT_NE(bodies[0].find("token=old-refresh"), std::string::npos) << bodies[0];
    EXPECT_NE(bodies[0].find("token_type_hint=refresh_token"), std::string::npos) << bodies[0];
    EXPECT_NE(bodies[0].find("client_id=client-1"), std::string::npos) << bodies[0];
    EXPECT_NE(bodies[0].find("client_secret=secret-1"), std::string::npos) << bodies[0];
    EXPECT_NE(bodies[1].find("token=old-access"), std::string::npos) << bodies[1];
    EXPECT_NE(bodies[1].find("token_type_hint=access_token"), std::string::npos) << bodies[1];

    const auto auth_headers = server.revoke_authorization_headers();
    ASSERT_EQ(auth_headers.size(), 2u);
    EXPECT_TRUE(auth_headers[0].empty());
    EXPECT_TRUE(auth_headers[1].empty());
    EXPECT_FALSE(fs::exists(token_path));

    fs::remove_all(root);
}

TEST(McpAuth, CompletesOAuthBrowserCallbackFlowAndStoresTokens) {
    LocalOAuthRevocationServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_oauth_callback_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());

    loom::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = "https://mcp.example.test/mcp";
    auth_config.oauth = loom::services::mcp::McpOAuthConfig{
        .auth_server_metadata_url = server.metadata_url(),
        .client_id = "client-1",
    };

    std::mutex mutex;
    std::condition_variable cv;
    std::optional<std::string> auth_url;
    std::optional<std::string> flow_error;
    bool flow_done = false;
    std::jthread flow_thread([&](std::stop_token) {
        auto result = loom::services::mcp::perform_mcp_oauth_flow(
            "callback-fixture",
            auth_config,
            [&](const std::string& url) {
                {
                    std::lock_guard lock(mutex);
                    auth_url = url;
                }
                cv.notify_all();
            },
            std::nullopt,
            true);
        {
            std::lock_guard lock(mutex);
            if (!result) flow_error = result.error().message();
            flow_done = true;
        }
        cv.notify_all();
    });

    std::string captured_url;
    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return auth_url.has_value() || flow_done;
        }));
        ASSERT_TRUE(auth_url.has_value()) << flow_error.value_or("OAuth flow ended before authorization URL");
        captured_url = *auth_url;
    }

    auto redirect_uri = test_query_param(captured_url, "redirect_uri");
    auto state = test_query_param(captured_url, "state");
    ASSERT_TRUE(redirect_uri.has_value()) << captured_url;
    ASSERT_TRUE(state.has_value()) << captured_url;
    EXPECT_NE(captured_url.find("response_type=code"), std::string::npos) << captured_url;
    EXPECT_NE(captured_url.find("client_id=client-1"), std::string::npos) << captured_url;
    EXPECT_NE(captured_url.find("code_challenge_method=S256"), std::string::npos) << captured_url;
    EXPECT_NE(captured_url.find("scope=tools"), std::string::npos) << captured_url;

    auto callback_port = localhost_url_port(*redirect_uri);
    ASSERT_TRUE(callback_port.has_value()) << *redirect_uri;
    httplib::Client callback_client(std::format("http://localhost:{}", *callback_port));
    auto callback_response = callback_client.Get(
        std::format("/oauth/callback?code=callback-code&state={}", *state));
    ASSERT_TRUE(callback_response);
    EXPECT_EQ(callback_response->status, 200);

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(3), [&] { return flow_done; }));
        ASSERT_FALSE(flow_error.has_value()) << *flow_error;
    }
    ASSERT_TRUE(server.wait_for_token_request());
    const auto token_body = server.token_request_body();
    EXPECT_NE(token_body.find("grant_type=authorization_code"), std::string::npos) << token_body;
    EXPECT_NE(token_body.find("code=callback-code"), std::string::npos) << token_body;
    EXPECT_NE(token_body.find("client_id=client-1"), std::string::npos) << token_body;
    EXPECT_NE(token_body.find("code_verifier="), std::string::npos) << token_body;

    const auto server_key = loom::services::mcp::get_server_key("callback-fixture", auth_config);
    auto sanitize_key = [](std::string_view key) {
        std::string sanitized;
        sanitized.reserve(key.size());
        for (unsigned char ch : key) {
            sanitized.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
        }
        return sanitized;
    };
    const auto token_path = root / "loom" / "mcp" / (sanitize_key(server_key) + ".json");
    auto persisted = loom::utils::json::parse_file(token_path);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_EQ(persisted->root().get_string("server_name"), "callback-fixture");
    EXPECT_EQ(persisted->root().get_string("server_url"), *auth_config.url);
    EXPECT_EQ(persisted->root().get_string("access_token"), "callback-access");
    EXPECT_EQ(persisted->root().get_string("refresh_token"), "callback-refresh");
    EXPECT_EQ(persisted->root().get_string("scope"), "tools");
    EXPECT_EQ(persisted->root().get_string("client_id"), "client-1");

    fs::remove_all(root);
}

TEST(McpAuth, PerformsXaaIdpLoginAndStoresTokens) {
    LocalXaaIdpServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_xaa_idp_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root / ".loom");
    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());
    EnvironmentGuard xaa_enabled_guard("LOOM_ENABLE_XAA", "1");
    {
        std::ofstream idp_config(root / ".loom" / "xaa-idp.txt");
        idp_config << "idp_url=" << server.base_url() << "\n";
        idp_config << "idp_issuer=" << server.base_url() << "\n";
        idp_config << "idp_token_endpoint=" << server.base_url() << "/token\n";
        idp_config << "idp_id_token=fake-id-token-for-testing\n";
        idp_config << "client_id=idp-client-1\n";
        idp_config << "client_secret=as-secret-1\n";
        idp_config << "idp_client_id=idp-client-1\n";
        idp_config << "scope=openid profile mcp\n";
    }

    loom::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    // Point to mock server so PRM discovery succeeds
    auth_config.url = server.base_url() + "/mcp";
    auth_config.oauth = loom::services::mcp::McpOAuthConfig{
        .client_id = "as-client-1",
        .xaa = true,
    };

    bool authorization_url_called = false;
    auto result = loom::services::mcp::perform_mcp_oauth_flow(
        "xaa-fixture",
        auth_config,
        [&](const std::string&) {
            authorization_url_called = true;
        },
        std::nullopt,
        true);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    // No browser consent screen should have been shown — id_token was provided
    EXPECT_FALSE(authorization_url_called);

    // Verify the full XAA pipeline executed
    EXPECT_GE(server.prm_request_count(), 1)
        << "PRM discovery should have been called";
    EXPECT_GE(server.as_metadata_request_count(), 1)
        << "AS metadata discovery should have been called";
    EXPECT_GE(server.token_exchange_count(), 1)
        << "IdP token exchange (RFC 8693) should have been called";
    EXPECT_GE(server.jwt_bearer_count(), 1)
        << "AS jwt-bearer grant (RFC 7523) should have been called";

    // Verify the token exchange sent our fake id_token and correct client_id
    const auto exchange_body = server.token_exchange_body();
    EXPECT_NE(exchange_body.find("subject_token=fake-id-token-for-testing"), std::string::npos)
        << exchange_body;
    EXPECT_NE(exchange_body.find("client_id=idp-client-1"), std::string::npos)
        << exchange_body;
    EXPECT_NE(exchange_body.find("requested_token_type=urn%3Aietf%3Aparams%3Aoauth%3Atoken-type%3Aid-jag"),
        std::string::npos) << exchange_body;

    // Verify the jwt-bearer grant used HTTP Basic auth with AS credentials
    const auto auth_header = server.jwt_bearer_auth_header();
    EXPECT_NE(auth_header.find("Basic "), std::string::npos) << auth_header;

    // Verify token persistence
    const auto server_key = loom::services::mcp::get_server_key("xaa-fixture", auth_config);
    auto sanitize_key = [](std::string_view key) {
        std::string sanitized;
        sanitized.reserve(key.size());
        for (unsigned char ch : key) {
            sanitized.push_back(std::isalnum(ch) ? static_cast<char>(ch) : '_');
        }
        return sanitized;
    };
    const auto token_path = root / "loom" / "mcp" / (sanitize_key(server_key) + ".json");
    auto persisted = loom::utils::json::parse_file(token_path);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_EQ(persisted->root().get_string("server_name"), "xaa-fixture");
    EXPECT_EQ(persisted->root().get_string("server_url"), *auth_config.url);
    EXPECT_EQ(persisted->root().get_string("access_token"), "xaa-access");
    EXPECT_EQ(persisted->root().get_string("refresh_token"), "xaa-refresh");
    EXPECT_EQ(persisted->root().get_string("scope"), "openid profile mcp");
    EXPECT_EQ(persisted->root().get_string("client_id"), "idp-client-1");

    fs::remove_all(root);
}

TEST(McpAuth, XaaEnabledServerRequiresConfiguredIdpConnection) {
    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_xaa_missing_idp_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());
    EnvironmentGuard xaa_enabled_guard("LOOM_ENABLE_XAA", "1");

    loom::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = "https://mcp.example.test/mcp";
    auth_config.oauth = loom::services::mcp::McpOAuthConfig{
        .client_id = "as-client-1",
        .xaa = true,
    };

    auto result = loom::services::mcp::perform_mcp_oauth_flow(
        "xaa-missing-idp",
        auth_config,
        [](const std::string&) {},
        std::nullopt,
        true);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find("configured IdP connection"), std::string::npos);

    fs::remove_all(root);
}


// ===========================================================================
// RFC-0001 B followup c17 — the /mcp xaa login path must forward the
// configured fixed callbackPort to the loopback listener.
//
// Origin: the c12 review noted that settings.xaaIdp.callbackPort parsed and
// serialized correctly, and read_xaa_idp_status() surfaced it, but neither
// perform_xaa_login() nor the /mcp xaa login handler passed it through — so a
// user with a fixed callback port still got a RANDOM one, breaking an
// IdP-side redirect-URI allowlist keyed to that port.
//
// These tests are hermetic: no OIDC discovery, no socket, no browser. The
// port decision is observed through the seams the module already exposes
// (build_login_options / resolve_login_callback_port), which are the exact
// values acquire_idp_id_token() uses to bind detail::CallbackServer.
// ===========================================================================

namespace xaa_login = loom::services::mcp;



// The configured port reaches the IdpLoginOptions that acquire_idp_id_token()
// consumes (and therefore the loopback listener), instead of being dropped.

TEST(XaaIdpLoginC17, ConfiguredPortReachesListenerOptions) {
    auto opts = xaa_login::build_login_options(
        "https://idp.example.com", "loom-cli", std::optional<int>{8765});
    ASSERT_TRUE(opts.callback_port.has_value());
    EXPECT_EQ(*opts.callback_port, 8765);

    // The observable listener-resolved port is the configured one, not a
    // random ephemeral port.
    auto resolved = xaa_login::resolve_login_callback_port(opts);
    ASSERT_TRUE(resolved.has_value()) << resolved.error().message();
    ASSERT_TRUE(resolved->has_value());
    EXPECT_EQ(**resolved, 8765);

    // issuer / client_id survive the seam unchanged.
    EXPECT_EQ(opts.idp_issuer, "https://idp.example.com");
    EXPECT_EQ(opts.idp_client_id, "loom-cli");
}


// Absent port -> nullopt in the options; the resolver then picks a port via
// find_available_oauth_port(), i.e. the pre-c17 random behavior.

TEST(XaaIdpLoginC17, AbsentPortSelectsRandomPort) {
    const char* old_env = std::getenv("MCP_OAUTH_CALLBACK_PORT");
    std::optional<std::string> saved = old_env ? std::optional<std::string>(old_env)
                                               : std::nullopt;
    unsetenv("MCP_OAUTH_CALLBACK_PORT");

    auto opts = xaa_login::build_login_options(
        "https://idp.example.com", "loom-cli", std::nullopt);
    EXPECT_FALSE(opts.callback_port.has_value());

    auto resolved = xaa_login::resolve_login_callback_port(opts);
    ASSERT_TRUE(resolved.has_value()) << resolved.error().message();
    ASSERT_TRUE(resolved->has_value());
    const bool in_ephemeral = **resolved >= xaa_login::kRedirectPortRangeStart &&
                              **resolved <= xaa_login::kRedirectPortRangeEnd;
    const bool is_fallback = **resolved == xaa_login::kRedirectPortFallback;
    EXPECT_TRUE(in_ephemeral || is_fallback) << "got port " << **resolved;

    if (saved) setenv("MCP_OAUTH_CALLBACK_PORT", saved->c_str(), 1);
    else       unsetenv("MCP_OAUTH_CALLBACK_PORT");
}


// Out-of-range int -> IGNORED (nullopt), never truncated. A static_cast would
// wrap 65596 -> 60, 70000 -> 4464 and 65536 -> 0, each of which binds a port
// the user never asked for; the chosen behavior matches the rest of the
// codebase's tolerance for invalid config (config.cppm ignores a malformed
// value and keeps the default) and mirrors find_available_oauth_port, which
// rejects an impossible MCP_OAUTH_CALLBACK_PORT rather than using it.

TEST(XaaIdpLoginC17, OutOfRangePortIgnoredNotTruncated) {
    // Negative int -> ignored. (Defensive: the /mcp xaa setup parser, the only
    // writer, already rejects <= 0 before it can be stored.)
    EXPECT_FALSE(xaa_login::build_login_options(
        "https://idp.example.com", "loom-cli", std::optional<int>{-1})
        .callback_port.has_value());

    // Above uint16_t: 65536 would truncate to 0 and 70000 to 4464. Both are
    // ignored, and the resolver falls back to the random/fallback path.
    for (int bad : {65536, 70000, std::numeric_limits<int>::max()}) {
        auto opts = xaa_login::build_login_options(
            "https://idp.example.com", "loom-cli", std::optional<int>{bad});
        EXPECT_FALSE(opts.callback_port.has_value()) << "bad port " << bad;

        auto resolved = xaa_login::resolve_login_callback_port(opts);
        ASSERT_TRUE(resolved.has_value()) << resolved.error().message();
        ASSERT_TRUE(resolved->has_value());
        EXPECT_NE(**resolved, 0u) << "must never bind an ephemeral 0 from " << bad;
    }
}


// Boundaries are kept: 1 and 65535 are both valid fixed ports.

TEST(XaaIdpLoginC17, BoundaryPortsAccepted) {
    EXPECT_EQ(*xaa_login::build_login_options(
        "https://idp.example.com", "loom-cli", std::optional<int>{1}).callback_port,
        1);
    EXPECT_EQ(*xaa_login::build_login_options(
        "https://idp.example.com", "loom-cli", std::optional<int>{65535}).callback_port,
        65535);
}


// The pure counter-test for the above: validated_callback_port() is the single
// narrowing decision point (and it is constexpr, so it stays cheap).

TEST(XaaIdpLoginC17, ValidatedCallbackPortAcceptsOnlyUsableTcpPorts) {
    EXPECT_FALSE(xaa_login::validated_callback_port(std::nullopt).has_value());
    EXPECT_FALSE(xaa_login::validated_callback_port(0).has_value());
    EXPECT_FALSE(xaa_login::validated_callback_port(-1).has_value());
    EXPECT_FALSE(xaa_login::validated_callback_port(65536).has_value());
    EXPECT_EQ(*xaa_login::validated_callback_port(1), 1u);
    EXPECT_EQ(*xaa_login::validated_callback_port(8765), 8765u);
    EXPECT_EQ(*xaa_login::validated_callback_port(65535), 65535u);
}


// perform_xaa_login() accepts the port as its 4th argument and keeps the old
// 3-argument call shape compiling (scope and callback_port both default). The
// 3-arg shape is checked by an actual call at a 3-argument call site; the
// 4-arg shape by the call in CachedIdTokenShortCircuitsBeforePortUse.

TEST(XaaIdpLoginC17, PerformXaaLoginKeepsBackwardCompatibleSignature) {
    using LoginFn = std::expected<xaa_login::XaaLoginResult, std::string> (*)(
        std::string_view, std::string_view, std::optional<std::string_view>,
        std::optional<int>);
    static_assert(std::is_same_v<LoginFn,
        decltype(&xaa_login::perform_xaa_login)>);
    static_assert(std::is_same_v<
        decltype(&xaa_login::perform_xaa_login),
        std::expected<xaa_login::XaaLoginResult, std::string> (*)(
            std::string_view, std::string_view, std::optional<std::string_view>,
            std::optional<int>)>);
    (void)static_cast<LoginFn>(&xaa_login::perform_xaa_login);

    // A 3-argument call site still compiles and resolves against the same
    // function (both trailing parameters default).
    const auto three_arg_call = [](std::string_view a, std::string_view b,
                                   std::optional<std::string_view> c) {
        return xaa_login::perform_xaa_login(a, b, c);
    };
    static_assert(std::is_invocable_v<decltype(three_arg_call),
        std::string_view, std::string_view, std::optional<std::string_view>>);
    SUCCEED();
}


// A cached id_token short-circuits acquire_idp_id_token() before any port is
// selected: the port is not consulted, so a fixed port cannot make a cached
// login bind a socket. (Hermetic: seed the cache file, then run the login.)

TEST(XaaIdpLoginC17, CachedIdTokenShortCircuitsBeforePortUse) {
    const auto root = c13_make_temp_root("loom_c17_idp_cache_");
    fs::create_directories(root);
    C17IdpTokenCacheGuard guard(root);

    // A JWT-shaped token with an exp far in the future, seeded directly in the
    // same file shape detail::write_cached_idp_token() writes.
    const auto cache_dir = root / ".config" / "loom" / "xaa";
    fs::create_directories(cache_dir);
    const std::int64_t future_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() + 3'600'000;
    {
        std::ofstream out(cache_dir / "idp_tokens.json", std::ios::binary);
        out << R"JSON({"mcpXaaIdp":{"https://idp.invalid":{"idToken":"cached-id-token-c17","expiresAt":)JSON"
            << future_ms << R"JSON(}}})JSON";
    }

    // Cached token is returned without OIDC discovery (which would fail
    // against the unreachable issuer and is exactly what we are proving is
    // NOT reached). A fixed callback port is passed on purpose: it must not
    // be used, because the cache hit returns before port selection.
    auto result = xaa_login::perform_xaa_login(
        "https://idp.invalid", "loom-cli", std::nullopt, std::optional<int>{8765});
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->id_token, "cached-id-token-c17");
    EXPECT_EQ(result->access_token, "cached-id-token-c17");
    EXPECT_EQ(result->authorization_server_url, "https://idp.invalid");

    fs::remove_all(root);
}


// ---------------------------------------------------------------------------
// c17a — the callback port is a SINGLE store again: settings.xaaIdp.callbackPort,
// injected into the --xaa runtime path by the composition layer. The
// c17-added ~/.loom/xaa-idp.txt `callback_port` key (nothing in src/ ever
// wrote that file) is gone; a hand-edited line is ignored like any unknown key.
// ---------------------------------------------------------------------------

// A `callback_port` line in a legacy / hand-edited ~/.loom/xaa-idp.txt is
// inert: read_xaa_config_file() has no such field any more, so the parser must
// not invent one. This pins the removal of the c17 surface.
template <typename T>
concept HasCallbackPort = requires(T c) { c.callback_port; };


TEST(XaaConfigC17a, XaaIdpFileCallbackPortLineIsIgnored) {
    const auto root = c13_make_temp_root("loom_c17a_xaa_cfg_");
    fs::create_directories(root / ".loom");
    EnvironmentGuard home_guard("HOME", root.string());
    {
        std::ofstream f(root / ".loom" / "xaa-idp.txt");
        f << "idp_url=https://idp.example.com\n";
        f << "client_id=as-client\n";
        f << "idp_token_endpoint=https://idp.example.com/token\n";
        f << "callback_port=8765\n";
    }

    // The file still parses (the unknown key is skipped, not an error) but
    // carries no port: the port can only come from settings.xaaIdp now. The
    // concept must be dependent, so a missing member is false rather than a
    // hard error in a non-template context.
    auto cfg = loom::services::mcp::get_xaa_config("any");
    ASSERT_TRUE(cfg.has_value());
    static_assert(!HasCallbackPort<loom::services::mcp::XaaConfig>,
        "XaaConfig::callback_port must be removed; the port is injected from "
        "settings.xaaIdp.callbackPort by the composition layer");

    fs::remove(root / ".loom" / "xaa-idp.txt");
    fs::remove_all(root);
}


// ===========================================================================
// RFC-0001 B followup c17a — findings from the c17 review.
//
// FINDING 1: c17 added XaaConfig::callback_port + a `callback_port=` parse in
// ~/.loom/xaa-idp.txt, but NOTHING in src/ ever WRITES that file (only tests
// did), while the SUPPORTED surface `/mcp xaa setup --callback-port` persists
// settings.xaaIdp.callbackPort (settings.json). Two stores for one setting, and
// the CLI-writable one did not reach the --xaa runtime path.
//
// Resolution: ONE authoritative store — settings.xaaIdp.callbackPort. The
// c17 ~/.loom/xaa-idp.txt surface is REMOVED (the test above pins that), and
// the composition layer injects the settings value into authenticate_xaa()
// (see loom.commands.mcp.core_settings_loader -> CoreSettingsMcpLayer ->
// native_mcp_xaa_callback_port(), pinned in test_tools).
//
// FINDING 2: `idp_client_secret` was dropped on `/mcp xaa login` — setup
// persisted it and read_xaa_idp_status() reported has_client_secret, but
// perform_xaa_login()/build_login_options() never forwarded it, so a
// confidential IdP client silently degraded to PKCE-only on that path.
//
// These tests are hermetic: no network, no socket, no browser, no OIDC
// discovery. The port/secret decisions are observed through the exact seams
// acquire_idp_id_token() consumes.
// ===========================================================================

// build_login_options() resolves the IdP client secret from the SAME
// file-based store `/mcp xaa setup` writes and read_xaa_idp_status() reads
// (get_idp_client_secret(issuer)); an absent store leaves it unset, preserving
// the PKCE-only behavior exactly.

TEST(XaaIdpLoginC17a, ClientSecretForwardedWhenStored) {
    const auto root = c13_make_temp_root("loom_c17a_secret_");
    fs::create_directories(root);
    C17IdpTokenCacheGuard guard(root);  // redirects HOME so the store write
                                        // lands in the temp tree, never the
                                        // developer's real ~/.config/loom/xaa

    const std::string issuer = "https://idp.example.com";

    // Absent store -> no secret in the options (PKCE-only, as before).
    {
        auto opts = xaa_login::build_login_options(issuer, "loom-cli", std::nullopt);
        EXPECT_FALSE(opts.idp_client_secret.has_value());
    }

    // A stored secret (the writer `/mcp xaa setup --client-secret` uses) is
    // resolved into the options, so the token request can use basic/post auth.
    // The store file starts absent and is created by this write — see
    // FreshStoreWritesSecretAndToken for why that matters.
    xaa_login::save_idp_client_secret(issuer, "s3cr3t-c17a");
    {
        auto opts = xaa_login::build_login_options(issuer, "loom-cli", std::nullopt);
        ASSERT_TRUE(opts.idp_client_secret.has_value());
        EXPECT_EQ(*opts.idp_client_secret, "s3cr3t-c17a");

        // The observable resolver agrees (it is the helper build_login_options
        // folds in), and an unrelated issuer stays unset.
        auto resolved = xaa_login::resolve_login_client_secret(issuer);
        ASSERT_TRUE(resolved.has_value());
        EXPECT_EQ(*resolved, "s3cr3t-c17a");
        EXPECT_FALSE(
            xaa_login::resolve_login_client_secret("https://other.invalid")
                .has_value());
    }

    fs::remove_all(root);
}


// The secret and the fixed callback port are resolved together and independently:
// storing one must not disturb the other, and an out-of-range port is still
// ignored while the secret is still forwarded.

TEST(XaaIdpLoginC17a, SecretAndPortResolveIndependently) {
    const auto root = c13_make_temp_root("loom_c17a_secret_port_");
    fs::create_directories(root);
    C17IdpTokenCacheGuard guard(root);

    const std::string issuer = "https://idp.example.com";
    xaa_login::save_idp_client_secret(issuer, "both-c17a");

    auto opts = xaa_login::build_login_options(
        issuer, "loom-cli", std::optional<int>{8765});
    ASSERT_TRUE(opts.idp_client_secret.has_value());
    EXPECT_EQ(*opts.idp_client_secret, "both-c17a");
    ASSERT_TRUE(opts.callback_port.has_value());
    EXPECT_EQ(*opts.callback_port, 8765u);

    auto bad = xaa_login::build_login_options(
        issuer, "loom-cli", std::optional<int>{70000});
    ASSERT_TRUE(bad.idp_client_secret.has_value());   // secret still forwarded
    EXPECT_FALSE(bad.callback_port.has_value());      // bad port still ignored

    fs::remove_all(root);
}


// Regression, found while making the c17a forwarding reachable: both XAA store
// writers allocate the root object but (before this fix) never called
// set_root(), so yyjson_mut_write() returned NULL and to_string() was the empty
// string. On a FRESH store (the normal first `/mcp xaa setup --client-secret`)
// the file was created EMPTY — the secret was silently lost, so the forwarding
// fix could not have had any effect. A pre-existing store round-tripped only
// because the parse branch happened to set_root().

TEST(XaaIdpLoginC17a, FreshStoreWritesSecretAndToken) {
    const auto root = c13_make_temp_root("loom_c17a_fresh_store_");
    fs::create_directories(root);
    C17IdpTokenCacheGuard guard(root);

    const auto store = root / ".config" / "loom" / "xaa" / "idp_tokens.json";
    ASSERT_FALSE(fs::exists(store));  // fresh: no parse branch, no set_root

    // Secret into a fresh store is persisted and readable back.
    xaa_login::save_idp_client_secret("https://idp.example.com", "fresh-secret");
    ASSERT_TRUE(fs::exists(store));
    auto secret = xaa_login::get_idp_client_secret("https://idp.example.com");
    ASSERT_TRUE(secret.has_value());
    EXPECT_EQ(*secret, "fresh-secret");

    // The id_token writer is the same class and must be non-empty too (an
    // unreachable IdP means no login can cache a token here, so seed via the
    // external-token entry point, which is what `/mcp xaa login --id-token`
    // uses and which parses the JWT exp claim).
    xaa_login::save_idp_id_token_from_jwt("https://idp.example.com",
                                          "header.payload.sig");
    auto cached = xaa_login::get_cached_idp_id_token("https://idp.example.com");
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(*cached, "header.payload.sig");

    // Both sections survive side by side (no writer clobbers the other).
    ASSERT_TRUE(xaa_login::get_idp_client_secret("https://idp.example.com").has_value());

    fs::remove_all(root);
}


// authenticate_xaa() (the --xaa runtime login entry) gained the injected
// callback port as its 5th parameter, so the composition layer can thread
// settings.xaaIdp.callbackPort in without loom.services seeing loom.config. The
// 4-argument call shape (on_auth_url + skip_browser, no port) must keep
// compiling.

TEST(XaaIdpLoginC17a, AuthenticateXaaAcceptsInjectedCallbackPort) {
    using AuthFn = loom::services::mcp::Result<loom::services::mcp::XaaResult> (*)(
        const loom::services::mcp::XaaConfig&, std::string_view,
        std::function<void(const std::string&)>, bool, std::optional<int>);
    static_assert(std::is_same_v<AuthFn,
        decltype(&loom::services::mcp::authenticate_xaa)>);

    const auto four_arg_call = [](const loom::services::mcp::XaaConfig& c,
                                  std::string_view url,
                                  std::function<void(const std::string&)> cb,
                                  bool skip) {
        return loom::services::mcp::authenticate_xaa(c, url, std::move(cb), skip);
    };
    static_assert(std::is_invocable_v<decltype(four_arg_call),
        const loom::services::mcp::XaaConfig&, std::string_view,
        std::function<void(const std::string&)>, bool>);
    SUCCEED();
}


// The resolved single-store port is what the login seam would bind: the value
// the CLI-writable store carries reaches build_login_options() and resolves to
// that exact port (not a random one). Pairs with the CLI end-to-end test below.

TEST(XaaIdpLoginC17a, ResolvedStorePortReachesLoginSeam) {
    auto opts = xaa_login::build_login_options(
        "https://idp.example.com", "loom-cli", std::optional<int>{19485});
    auto resolved = xaa_login::resolve_login_callback_port(opts);
    ASSERT_TRUE(resolved.has_value()) << resolved.error().message();
    ASSERT_TRUE(resolved->has_value());
    EXPECT_EQ(**resolved, 19485u);

    // Unset store -> random/fallback port (pre-c17 behavior preserved).
    auto unset = xaa_login::build_login_options(
        "https://idp.example.com", "loom-cli", std::nullopt);
    EXPECT_FALSE(unset.callback_port.has_value());
}


// FINDING 1, full chain: with the REAL production core-settings loader
// installed (loom.commands.mcp.core_settings_loader, which is the one place
// allowed to see both loom.config.config and loom.orchestration.tools.mcp), a
// settings.xaaIdp.callbackPort written to the real config tiers is loaded into
// the native runtime and surfaced by native_mcp_xaa_callback_port() — the value
// McpAuthTool forwards to authenticate_xaa(). No fake loader, no network.
//
// This module reaches loom.orchestration.* through loom_core (test_tools links
// loom_orchestration directly, test_services transitively).

TEST(XaaIdpLoginC17a, ProductionLoaderPropagatesXaaCallbackPort) {
    namespace fs2 = std::filesystem;
    const auto root = c13_make_temp_root("loom_c17a_prod_loader_");
    const auto home = root / "home";
    const auto cfg = root / "cfg";
    const auto work = root / "work";
    fs2::create_directories(home);
    fs2::create_directories(cfg);
    fs2::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    const fs2::path previous_cwd = fs2::current_path();
    fs2::current_path(work);

    auto reset = [&] {
        // Clear the slot and force a full reload so the runtime (a process
        // singleton) drops BOTH the configured servers and any captured XAA
        // callback port — a plain sync({}) marks the runtime loaded and would
        // leave a stale port behind for the next test.
        loom::tools::set_core_settings_mcp_loader(nullptr);
        (void)loom::tools::reload_native_mcp_servers_from_config();
    };
    reset();

    // Write the project config tier the production loader reads.
    {
        std::ofstream f(work / ".loom" / "settings.json");
        f << R"JSON({
  "xaaIdp": { "issuer": "https://idp.example.com", "clientId": "loom-cli", "callbackPort": 19485 }
})JSON";
    }

    // Install the REAL composition-root loader and force a fresh load.
    loom::commands::install_core_settings_mcp_loader();
    ASSERT_TRUE(loom::tools::reload_native_mcp_servers_from_config().has_value());

    auto port = loom::tools::native_mcp_xaa_callback_port();
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(*port, 19485);

    reset();
    std::error_code ec;
    fs2::current_path(previous_cwd, ec);
    fs2::remove_all(root);
}


// RFC-0001 B followup c17a — REGRESSION for the review's MAJOR: the XAA
// callback port must survive the PRODUCTION ordering. McpCommand calls
// sync_native_mcp_servers() (from /mcp list, /mcp show, the MCP dialog) BEFORE
// any XAA mcp_auth; sync() sets the runtime's loaded_ flag WITHOUT running the
// core-settings loader. A cached capture was therefore nullopt in exactly that
// ordering and the login fell back to a random port — the very c17 defect this
// change fixes. xaa_callback_port() now resolves through the loader on demand,
// so a sync() first must not hide the port. No reload_to_config() here on
// purpose: that is the path that used to mask the bug.

TEST(XaaIdpLoginC17a, PortSurvivesSyncBeforeReadProductionOrdering) {
    namespace fs2 = std::filesystem;
    const auto root = c13_make_temp_root("loom_c17a_sync_order_");
    const auto home = root / "home";
    const auto cfg = root / "cfg";
    const auto work = root / "work";
    fs2::create_directories(home);
    fs2::create_directories(cfg);
    fs2::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    const fs2::path previous_cwd = fs2::current_path();
    fs2::current_path(work);

    auto reset = [&] {
        loom::tools::set_core_settings_mcp_loader(nullptr);
        (void)loom::tools::reload_native_mcp_servers_from_config();
    };
    reset();

    {
        std::ofstream f(work / ".loom" / "settings.json");
        f << R"JSON({
  "xaaIdp": { "issuer": "https://idp.example.com", "clientId": "loom-cli", "callbackPort": 19485 }
})JSON";
    }

    loom::commands::install_core_settings_mcp_loader();

    // Production ordering: sync() FIRST (this sets loaded_ = true and never
    // ran the loader), THEN read the port.
    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    auto port = loom::tools::native_mcp_xaa_callback_port();
    ASSERT_TRUE(port.has_value())
        << "sync() before the read left the port unset (the c17 random-port bug)";
    EXPECT_EQ(*port, 19485);

    // A subsequent explicit reload keeps it (and is still correct).
    ASSERT_TRUE(loom::tools::reload_native_mcp_servers_from_config().has_value());
    ASSERT_TRUE(loom::tools::native_mcp_xaa_callback_port().has_value());

    reset();
    std::error_code ec;
    fs2::current_path(previous_cwd, ec);
    fs2::remove_all(root);
}


// RFC-0001 B followup c17a — the store holds an IdP client secret and a
// cached id_token; both writers now route through
// loom.utils.atomic_replace_file(OwnerOnly) and force the containing xaa/ dir
// to 0700, so the credentials are owner-only at rest instead of the previous
// 0644 file / 0755 dir under umask 022. These tests run under umask 022 (the
// common default) for umask-independence: the mode must come from the code,
// not the umask.


// Fresh store: the file is created 0600 and its parent dir 0700, regardless
// of the process umask.

TEST(XaaIdpLoginC17a, StoreIsOwnerOnlyOnFreshWrite) {
    const auto root = c13_make_temp_root("loom_c17a_mode_fresh_");
    fs::create_directories(root);
    C17IdpTokenCacheGuard guard(root);
    UmaskGuard022 umask_guard;

    loom::services::mcp::save_idp_client_secret("https://idp.example.com", "owner-only");
    const auto store = root / ".config" / "loom" / "xaa" / "idp_tokens.json";
    ASSERT_TRUE(fs::exists(store));
    EXPECT_EQ(mode_bits(store), static_cast<mode_t>(0600u)) << "file must be owner-only";
    EXPECT_EQ(mode_bits(store.parent_path()), static_cast<mode_t>(0700u))
        << "store dir must be owner-only";

    // The secret still round-trips (0600 must not break the read path).
    auto secret = loom::services::mcp::get_idp_client_secret("https://idp.example.com");
    ASSERT_TRUE(secret.has_value());
    EXPECT_EQ(*secret, "owner-only");

    fs::remove_all(root);
}


// A pre-existing 0644 store (the pre-c17a world) is TIGHTENED to 0600 on the
// next write — the mode is corrected, not merely applied at creation.

TEST(XaaIdpLoginC17a, ExistingWideStoreTightenedToOwnerOnly) {
    const auto root = c13_make_temp_root("loom_c17a_mode_tighten_");
    fs::create_directories(root);
    C17IdpTokenCacheGuard guard(root);
    UmaskGuard022 umask_guard;

    // Seed a 0644 store with a different issuer so the rewrite is a read-
    // modify-write of a real pre-existing file.
    const auto store = root / ".config" / "loom" / "xaa" / "idp_tokens.json";
    fs::create_directories(store.parent_path());
    {
        std::ofstream f(store);
        f << R"JSON({"mcpXaaIdpConfig":{"https://existing.invalid":{"clientSecret":"old"}}})JSON";
    }
    ASSERT_EQ(::chmod(store.c_str(), 0644), 0);
    ASSERT_EQ(mode_bits(store), static_cast<mode_t>(0644u));

    loom::services::mcp::save_idp_client_secret("https://idp.example.com", "new-secret");
    EXPECT_EQ(mode_bits(store), static_cast<mode_t>(0600u)) << "mode must be corrected";

    // Both the pre-existing and the new entry survive the rewrite.
    EXPECT_TRUE(loom::services::mcp::get_idp_client_secret("https://existing.invalid").has_value());
    EXPECT_TRUE(loom::services::mcp::get_idp_client_secret("https://idp.example.com").has_value());

    fs::remove_all(root);
}


// A pre-existing stricter mode (0400) is replaced by exactly 0600 — never a
// wider mode, and the read path still works; and the store dir is 0700.

TEST(XaaIdpLoginC17a, StricterExistingModeNotLoosened) {
    const auto root = c13_make_temp_root("loom_c17a_mode_strict_");
    fs::create_directories(root);
    C17IdpTokenCacheGuard guard(root);
    UmaskGuard022 umask_guard;

    const auto store = root / ".config" / "loom" / "xaa" / "idp_tokens.json";
    fs::create_directories(store.parent_path());
    {
        std::ofstream f(store);
        f << "{}";
    }
    ASSERT_EQ(::chmod(store.c_str(), 0400), 0);

    loom::services::mcp::save_idp_client_secret("https://idp.example.com", "strict");
    const auto bits = mode_bits(store);
    EXPECT_EQ(bits, static_cast<mode_t>(0600u))
        << "replace sets exactly 0600 (owner rw); never wider";
    EXPECT_EQ(bits & static_cast<mode_t>(0077u), static_cast<mode_t>(0u))
        << "no group/other bit may survive";
    EXPECT_TRUE(loom::services::mcp::get_idp_client_secret("https://idp.example.com").has_value());

    fs::remove_all(root);
}


// A symlinked store leaf is REFUSED (no clobber-follow): atomic_replace_file's
// leaf gate rejects it, so the symlink target is left untouched and the
// symlink itself is not replaced by a regular file.

TEST(XaaIdpLoginC17a, SymlinkedStoreLeafRefused) {
    const auto root = c13_make_temp_root("loom_c17a_mode_symlink_");
    fs::create_directories(root);
    C17IdpTokenCacheGuard guard(root);

    const auto store = root / ".config" / "loom" / "xaa" / "idp_tokens.json";
    fs::create_directories(store.parent_path());
    const auto victim = root / "victim.json";
    {
        std::ofstream f(victim);
        f << "SENTINEL-VICTIM";
    }
    ASSERT_EQ(::symlink(victim.c_str(), store.c_str()), 0);
    ASSERT_TRUE(fs::is_symlink(store));

    loom::services::mcp::save_idp_client_secret("https://idp.example.com", "must-not-land");

    // The victim is byte-unchanged and the leaf is still a symlink (the write
    // was refused, not followed and not turned into a regular file).
    std::ifstream in(victim);
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(body, "SENTINEL-VICTIM");
    EXPECT_TRUE(fs::is_symlink(store));

    fs::remove_all(root);
}


// FINDING 1, is_xaa gate: the configured port is forwarded ONLY for an
// XAA-configured server. xaa_login_callback_port_for() is the extracted pure
// seam McpAuthTool consults; inverting or deleting the gate (e.g. returning
// `configured_port` unconditionally) fails this test. Hermetic: no network,
// no loader, no configured server needed.

TEST(XaaIdpLoginC17a, CallbackPortForwardedOnlyForXaaServers) {
    // The positive case: an XAA server gets the configured port.
    EXPECT_EQ(loom::tools::xaa_login_callback_port_for(true, std::optional<int>{19485}),
              std::optional<int>{19485});
    // The negative case: a NON-XAA OAuth server must NOT be pinned to it, even
    // when a port is configured — otherwise a plain OAuth login would bind the
    // IdP port.
    EXPECT_FALSE(loom::tools::xaa_login_callback_port_for(false, std::optional<int>{19485})
                     .has_value());
    // No configured port -> nullopt either way (random port preserved).
    EXPECT_FALSE(loom::tools::xaa_login_callback_port_for(true, std::nullopt).has_value());
    EXPECT_FALSE(loom::tools::xaa_login_callback_port_for(false, std::nullopt).has_value());

    // And the runtime helper the gate reads is independently observable.
    loom::tools::set_core_settings_mcp_loader(nullptr);
    EXPECT_FALSE(loom::tools::native_mcp_xaa_callback_port().has_value());
}


// ===========================================================================
// RFC-0001 followup c20 — the IdP client secret is a SINGLE store again: the
// hardened ~/.config/loom/xaa/idp_tokens.json (written by `/mcp xaa setup
// --client-secret`), injected into the --xaa runtime path through the c17a
// composition seam. The dead hand-edited ~/.loom/xaa-idp.txt
// `idp_client_secret` line is no longer parsed; a one-time guarded migration
// moves it into the store. The C1 OIDC-login token-exchange error path now
// redacts the echoed body (shared detail::redact_tokens with the C2 leg).
// ===========================================================================



// Test 2: a `idp_client_secret` line in a legacy / hand-edited
// ~/.loom/xaa-idp.txt is inert: read_xaa_config_file() no longer parses it.
// The FIELD still exists on XaaConfig (consumed on both legs, populated from
// the store via the seam); only the file parse is gone.
template <typename T>
concept HasIdpClientSecret = requires(T c) { c.idp_client_secret; };


TEST(XaaConfigC20, XaaIdpFileClientSecretLineIsIgnored) {
    const auto root = c13_make_temp_root("loom_c20_xaa_cfg_");
    fs::create_directories(root / ".loom");
    EnvironmentGuard home_guard("HOME", root.string());
    {
        std::ofstream f(root / ".loom" / "xaa-idp.txt");
        f << "idp_url=https://idp.example.com\n";
        f << "client_id=as-client\n";
        f << "idp_token_endpoint=https://idp.example.com/token\n";
        f << "idp_client_secret=legacy-secret\n";
    }

    auto cfg = loom::services::mcp::get_xaa_config("any");
    ASSERT_TRUE(cfg.has_value());
    EXPECT_FALSE(cfg->idp_client_secret.has_value())
        << "the dead idp_client_secret= file line must not be parsed";

    // The field still EXISTS on XaaConfig (the inverse of the c17a
    // !HasCallbackPort check): it is consumed on both legs and is populated
    // from the hardened store by the composition seam.
    static_assert(HasIdpClientSecret<loom::services::mcp::XaaConfig>,
        "XaaConfig::idp_client_secret must exist; it is populated from the "
        "hardened store by the composition seam");

    fs::remove(root / ".loom" / "xaa-idp.txt");
    fs::remove_all(root);
}


// Test 7: the is_xaa gate for BOTH the port and the secret. xaa_login_secrets_for()
// is the extracted pure seam McpAuthTool consults; inverting or deleting the
// gate fails this test. Hermetic: no network, no loader, no filesystem.

TEST(XaaIdpLoginC20, SecretForwardedOnlyForXaaServers) {
    // Positive: an XAA server gets both the port and the secret.
    auto [port, secret] = loom::tools::xaa_login_secrets_for(
        true, std::optional<int>{19485}, std::optional<std::string>{"s3cr3t"});
    EXPECT_EQ(port, std::optional<int>{19485});
    ASSERT_TRUE(secret.has_value());
    EXPECT_EQ(*secret, "s3cr3t");

    // Negative: a NON-XAA OAuth server must NOT receive the secret.
    auto [no_port, no_secret] = loom::tools::xaa_login_secrets_for(
        false, std::optional<int>{19485}, std::optional<std::string>{"s3cr3t"});
    EXPECT_FALSE(no_port.has_value());
    EXPECT_FALSE(no_secret.has_value());

    // No configured values -> nullopt either way.
    auto [p2, s2] = loom::tools::xaa_login_secrets_for(true, std::nullopt, std::nullopt);
    EXPECT_FALSE(p2.has_value());
    EXPECT_FALSE(s2.has_value());
    auto [p3, s3] = loom::tools::xaa_login_secrets_for(false, std::nullopt, std::nullopt);
    EXPECT_FALSE(p3.has_value());
    EXPECT_FALSE(s3.has_value());
}


// Test 5 (C1 leg): the OIDC login token-exchange error path redacts the

// The empty-issuer guard: with no configured issuer (XAA not configured, or
// after `/mcp xaa clear` reset settings.xaaIdp to {}), the migration must NOT
// run — save_idp_client_secret("", ...) would write under the phantom key
// mcpXaaIdpConfig."".clientSecret, after which get_idp_client_secret("") no
// longer misses and the secret would resurface.

TEST(XaaIdpLoginC20, LegacyMigrationSkippedWhenIssuerEmpty) {
    namespace fs2 = std::filesystem;
    const auto root = c13_make_temp_root("loom_c20_no_issuer_");
    const auto home = root / "home";
    const auto cfg = root / "cfg";
    const auto work = root / "work";
    fs2::create_directories(home / ".loom");
    fs2::create_directories(cfg);
    fs2::create_directories(work / ".loom");

    EnvironmentGuard home_guard("HOME", home.string());
    EnvironmentGuard cfg_guard("LOOM_CONFIG_DIR", cfg.string());
    const fs2::path previous_cwd = fs2::current_path();
    fs2::current_path(work);

    auto reset = [&] {
        loom::tools::set_core_settings_mcp_loader(nullptr);
        (void)loom::tools::reload_native_mcp_servers_from_config();
    };
    reset();

    // NO xaaIdp key in the config -> issuer is empty.
    {
        std::ofstream f(work / ".loom" / "settings.json");
        f << "{}\n";
    }
    // Legacy file with a secret line that must NOT be migrated.
    {
        std::ofstream f(home / ".loom" / "xaa-idp.txt");
        f << "idp_url=https://idp.example.com\n";
        f << "client_id=as-client\n";
        f << "idp_client_secret=must-not-migrate\n";
    }

    loom::commands::install_core_settings_mcp_loader();
    ASSERT_TRUE(loom::tools::reload_native_mcp_servers_from_config().has_value());

    // No phantom "" key: get_idp_client_secret("") misses.
    EXPECT_FALSE(loom::services::mcp::get_idp_client_secret("").has_value());
    // The loader yields no secret.
    EXPECT_FALSE(loom::tools::native_mcp_xaa_idp_client_secret().has_value());
    // The store file was never created.
    EXPECT_FALSE(fs2::exists(home / ".config" / "loom" / "xaa" / "idp_tokens.json"));

    reset();
    std::error_code ec;
    fs2::current_path(previous_cwd, ec);
    fs2::remove_all(root);
}


// Test 5 (C1 leg): the OIDC login token-exchange error path redacts the
// echoed body. Drives acquire_idp_id_token() with a mock IdP that returns 400
// with the secret in a JSON client_secret field; the error must not contain
// the secret bytes. idp_id_token is EMPTY (no cached token) so the C1 leg
// actually runs, with a simulated browser callback.

TEST(XaaIdpLoginC20, NoSecretBytesInC1TokenExchangeError) {
    const std::string secret = "c1-secret-must-not-leak";
    C20C1MockIdpServer server(secret);

    const auto root = c13_make_temp_root("loom_c20_c1_");
    fs::create_directories(root);
    C17IdpTokenCacheGuard cache_guard(root);

    xaa_login::IdpLoginOptions opts;
    opts.idp_issuer = server.base_url();
    opts.idp_client_id = "idp-client-1";
    opts.idp_client_secret = secret;
    opts.skip_browser_open = true;
    opts.on_authorization_url = [](const std::string& url) {
        // Parse the callback port and state from the auth URL, then simulate
        // the browser callback from a detached thread (wait_for_callback
        // blocks this thread). A raw socket is used because httplib::Client
        // re-encodes '+' in the query string, which would corrupt the
        // base64 state (the CallbackServer does not URL-decode).
        auto redirect_uri = c20_extract_query_param(url, "redirect_uri=");
        int port = c20_port_from_redirect_uri(redirect_uri);
        auto state = c20_extract_query_param(url, "state=");
        std::thread([port, state]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            int sock = ::socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) return;
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(static_cast<uint16_t>(port));
            if (::connect(sock, reinterpret_cast<struct sockaddr*>(&addr),
                          sizeof(addr)) < 0) {
                ::close(sock);
                return;
            }
            std::string req = "GET /callback?code=test-auth-code&state=" + state
                + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
            ::send(sock, req.c_str(), req.size(), 0);
            ::close(sock);
        }).detach();
    };

    auto result = xaa_login::acquire_idp_id_token(opts);
    ASSERT_FALSE(result.has_value());
    const auto& msg = result.error().message();
    EXPECT_EQ(msg.find(secret), std::string::npos)
        << "secret leaked into C1 error: " << msg;
    EXPECT_NE(msg.find("[REDACTED]"), std::string::npos)
        << "redaction wrap did not run: " << msg;

    fs::remove_all(root);
}


// Test 5 (C2 leg): the RFC 8693 token-exchange error path redacts the echoed
// body (the existing C2 wrap, now sharing detail::redact_tokens with C1).

TEST(XaaIdpLoginC20, NoSecretBytesInC2TokenExchangeError) {
    const std::string secret = "c2-secret-must-not-leak";
    C20MockTokenEndpoint server(secret);

    try {
        (void)xaa_login::request_jwt_authorization_grant(
            server.url(),
            "audience", "resource", "fake-id-token", "idp-client-1",
            std::optional<std::string_view>{secret});
        FAIL() << "expected XaaTokenExchangeError";
    } catch (const xaa_login::XaaTokenExchangeError& e) {
        const std::string msg = e.what();
        EXPECT_EQ(msg.find(secret), std::string::npos)
            << "secret leaked into C2 error: " << msg;
        EXPECT_NE(msg.find("[REDACTED]"), std::string::npos) << msg;
    }
}


// Test 8: end-to-end on the --xaa path. A secret stored in the hardened store
// (via save_idp_client_secret, the writer `/mcp xaa setup --client-secret`
// uses) is injected through perform_mcp_oauth_flow's xaa_idp_client_secret
// parameter and reaches the IdP token-exchange request body. This is the
// regression test that would have caught the original asymmetry.

TEST(McpAuth, XaaRuntimePathUsesHardenedStoreSecret) {
    LocalXaaIdpServer server;
    ASSERT_TRUE(server.ready());

    const auto suffix = std::chrono::system_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("loom_mcp_xaa_c20_" + std::to_string(suffix));
    fs::remove_all(root);
    fs::create_directories(root / ".loom");
    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard xdg_config_guard("XDG_CONFIG_HOME", root.string());
    EnvironmentGuard xaa_enabled_guard("LOOM_ENABLE_XAA", "1");
    {
        std::ofstream idp_config(root / ".loom" / "xaa-idp.txt");
        idp_config << "idp_url=" << server.base_url() << "\n";
        idp_config << "idp_issuer=" << server.base_url() << "\n";
        idp_config << "idp_token_endpoint=" << server.base_url() << "/token\n";
        idp_config << "idp_id_token=fake-id-token-for-testing\n";
        idp_config << "client_id=idp-client-1\n";
        idp_config << "client_secret=as-secret-1\n";
        idp_config << "idp_client_id=idp-client-1\n";
        idp_config << "scope=openid profile mcp\n";
        // NO idp_client_secret= line — the secret comes from the hardened store.
    }

    const std::string secret = "e2e-hardened-secret";
    // Store the secret the way `/mcp xaa setup --client-secret` does.
    loom::services::mcp::save_idp_client_secret(server.base_url(), secret);
    // The loader reads it from the same store; simulate that read here.
    auto stored = loom::services::mcp::get_idp_client_secret(server.base_url());
    ASSERT_TRUE(stored.has_value());
    EXPECT_EQ(*stored, secret);

    loom::services::mcp::McpServerConfig auth_config;
    auth_config.transport = "http";
    auth_config.url = server.base_url() + "/mcp";
    auth_config.oauth = loom::services::mcp::McpOAuthConfig{
        .client_id = "as-client-1",
        .xaa = true,
    };

    auto result = loom::services::mcp::perform_mcp_oauth_flow(
        "xaa-c20-e2e",
        auth_config,
        [](const std::string&) {},
        std::nullopt,
        true,  // skip_browser_open
        std::nullopt,  // xaa_callback_port
        *stored  // xaa_idp_client_secret — from the hardened store
    );
    ASSERT_TRUE(result.has_value()) << result.error().message();

    // The IdP token-exchange (C2) request received the stored secret.
    const auto exchange_body = server.token_exchange_body();
    EXPECT_NE(exchange_body.find("client_secret=" + secret), std::string::npos)
        << "stored secret not forwarded to the IdP token exchange: " << exchange_body;

    fs::remove_all(root);
}

// End of file
