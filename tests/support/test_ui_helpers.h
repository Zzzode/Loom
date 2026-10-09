/// @file test_ui_helpers.h
/// @brief Shared test utilities extracted from test_ui.cpp (SLOC budget split).
/// All helpers only use #include'd types (ftxui, httplib, std), no module imports.

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <expected>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <gtest/gtest.h>
#include <httplib.h>


namespace fs = std::filesystem;

inline void expect_element(const ftxui::Element& element) {
    EXPECT_NE(element, nullptr);
}

inline std::string render_to_plain_text(ftxui::Element element, int width = 80, int height = 20) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, element);
    return screen.ToString();
}

/// Strip ANSI escape sequences from a rendered string so that assertions
/// compare semantic content rather than color/style codes.
inline std::string strip_ansi(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
            // CSI sequence: skip until we find a non-digit/param byte
            i += 2;
            while (i < s.size() && (s[i] < 0x40 || s[i] > 0x7E)) ++i;
            if (i < s.size()) ++i;  // skip the final byte
            continue;
        }
        out.push_back(s[i]);
        ++i;
    }
    return out;
}

inline std::size_t max_line_width_bytes(std::string_view s) {
    std::size_t max_width = 0;
    std::size_t line_width = 0;
    for (char ch : s) {
        if (ch == '\n') {
            max_width = std::max(max_width, line_width);
            line_width = 0;
        } else {
            ++line_width;
        }
    }
    return std::max(max_width, line_width);
}

inline bool same_rendered_line_contains(std::string_view s,
                                 std::string_view first,
                                 std::string_view second) {
    std::size_t line_start = 0;
    while (line_start <= s.size()) {
        auto line_end = s.find('\n', line_start);
        if (line_end == std::string_view::npos) line_end = s.size();
        auto line = s.substr(line_start, line_end - line_start);
        if (line.find(first) != std::string_view::npos &&
            line.find(second) != std::string_view::npos) {
            return true;
        }
        if (line_end == s.size()) break;
        line_start = line_end + 1;
    }
    return false;
}

inline bool wait_until(std::function<bool()> predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

class ScopedEnvVar {
public:
    explicit ScopedEnvVar(std::string name) : name_(std::move(name)) {
        if (const char* value = std::getenv(name_.c_str())) {
            previous_ = std::string(value);
        }
        unsetenv(name_.c_str());
    }

    ~ScopedEnvVar() {
        if (previous_) {
            setenv(name_.c_str(), previous_->c_str(), 1);
        } else {
            unsetenv(name_.c_str());
        }
    }

    void set(std::string_view value) const {
        setenv(name_.c_str(), std::string(value).c_str(), 1);
    }

    void unset() const {
        unsetenv(name_.c_str());
    }

private:
    std::string name_;
    std::optional<std::string> previous_;
};

class LocalChunkedMessagesStreamServer {
public:
    LocalChunkedMessagesStreamServer() {
        server_.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
            {
                std::lock_guard lock(mutex_);
                ++request_count_;
                last_body_ = req.body;
            }
            cv_.notify_all();

            res.set_header("x-usage-input-tokens", "7");
            auto phase = std::make_shared<int>(0);
            res.set_chunked_content_provider(
                "text/event-stream",
                [this, phase](size_t /*offset*/, httplib::DataSink& sink) {
                    if (*phase == 0) {
                        sink.os <<
                            "event: message_start\n"
                            "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_ui_cancel\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                            "event: content_block_start\n"
                            "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
                            "event: content_block_delta\n"
                            "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"partial UI stream\"}}\n\n";
                        {
                            std::lock_guard lock(mutex_);
                            first_delta_sent_ = true;
                        }
                        cv_.notify_all();
                        ++(*phase);
                        return true;
                    }

                    {
                        std::unique_lock lock(mutex_);
                        if (!cv_.wait_for(lock, std::chrono::seconds(3), [this] {
                                return continue_after_cancel_;
                            })) {
                            return false;
                        }
                    }

                    sink.os <<
                        "event: content_block_delta\n"
                        "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\" after cancel\"}}\n\n"
                        "event: content_block_stop\n"
                        "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                        "event: message_delta\n"
                        "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":3}}\n\n"
                        "event: message_stop\n"
                        "data: {\"type\":\"message_stop\"}\n\n";
                    sink.done();
                    ++(*phase);
                    return true;
                });
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalChunkedMessagesStreamServer() {
        release_after_cancel();
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool valid() const noexcept {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return "http://127.0.0.1:" + std::to_string(port_);
    }

    void release_after_cancel() {
        {
            std::lock_guard lock(mutex_);
            continue_after_cancel_ = true;
        }
        cv_.notify_all();
    }

    [[nodiscard]] bool wait_for_first_delta(
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] {
            return first_delta_sent_;
        });
    }

    [[nodiscard]] std::optional<std::string> last_body() const {
        std::lock_guard lock(mutex_);
        return last_body_;
    }

private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t request_count_{0};
    std::optional<std::string> last_body_;
    bool first_delta_sent_{false};
    bool continue_after_cancel_{false};
};

struct ReleaseAfterCancelGuard {
    LocalChunkedMessagesStreamServer& server;

    ~ReleaseAfterCancelGuard() {
        server.release_after_cancel();
    }
};

class LocalErrorMessagesStreamServer {
public:
    LocalErrorMessagesStreamServer() {
        server_.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
            (void)req;
            {
                std::lock_guard lock(mutex_);
                ++request_count_;
            }
            cv_.notify_all();

            res.status = 400;
            res.set_content(
                R"({"type":"error","error":{"type":"invalid_request_error","message":"bad model"}})",
                "application/json");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalErrorMessagesStreamServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool valid() const noexcept {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return "http://127.0.0.1:" + std::to_string(port_);
    }

    [[nodiscard]] bool wait_for_requests(
        std::size_t count,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this, count] {
            return request_count_ >= count;
        });
    }

private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t request_count_{0};






};
// Render an FTXUI component to plain text (ANSI stripped).
inline std::string render_component_to_text(ftxui::Component c, int w = 120, int h = 40) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(w),
                                        ftxui::Dimension::Fixed(h));
    ftxui::Render(screen, c->Render());
    return strip_ansi(screen.ToString());
}



// Bring helpers into global scope for test bodies.

/// Golden snapshot helpers — shared across test files.
namespace sticky_prompt_test {

// Resolve tests/golden/ relative to LOOM_TESTS_DIR (defined via CMake).
inline std::string golden_dir() {
    return std::string(LOOM_TESTS_DIR) + "/golden/";
}
inline std::string normalize_line_endings(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) if (c != '\r') out.push_back(c);
    return out;
}

/// Golden snapshot check.  Set UPDATE_GOLDENS=1 env var to rewrite files.
inline void check_golden(const std::string& name, const std::string& actual) {
    const std::string path = golden_dir() + name + ".txt";
    if (std::getenv("UPDATE_GOLDENS") != nullptr) {
        std::ofstream out(path, std::ios::binary);
        ASSERT_TRUE(out.good()) << "cannot write golden: " << path;
        out << actual;
        SUCCEED() << "golden updated: " << path;
        return;
    }
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.good()) << "golden missing: " << path
                           << " (run UPDATE_GOLDENS=1 to create)";
    std::string expected((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    EXPECT_EQ(normalize_line_endings(actual),
              normalize_line_endings(expected))
        << "golden mismatch for '" << name
        << "' (run UPDATE_GOLDENS=1 to refresh)";
}

/// Render an Element to a fixed-size terminal buffer (includes ANSI codes).
inline std::string render_ansi(ftxui::Element element, int width, int height) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                       ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, element);
    return screen.ToString();
}

} // namespace sticky_prompt_test
