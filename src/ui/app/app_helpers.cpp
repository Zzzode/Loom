// app_helpers.cpp — plain impl unit for cc.ui.app.app. Owns the env/config
// and text/UTF helper bodies (RFC 0001 Phase C batch 1):
//   free functions: non_empty_env, first_non_empty_env, parse_bool_text,
//     parse_int_text, trim_ascii_copy, summarize_agent_description,
//     lowercase_ascii, ascii_isspace
//   AppAdapter static members: lowercase_ascii, utf8_continuation,
//     utf16_code_unit_count, rough_js_token_count
//
// Declarations stay exported in app.cppm (inline dropped from the free
// functions — the out-of-line definition here is the single strong
// definition). The two lowercase_ascii definitions (free function and
// static member) are byte-identical but live in different scopes, so there
// is no ODR issue.
//
// LLVM #184957: like app_extra_methods.cpp / app_handle_submit.cpp /
// app_prompt_suggestion_wiring.cpp / app_team.cpp / app_run.cpp, this unit
// must NOT `import std;` — under the reduced-BMI writer a cold module cache
// mis-merges the global aligned operator new when an app impl unit imports
// std while the primary's GMF pulls libc++ textually via FTXUI. Keep textual
// std headers in the global module fragment (see CMakeLists.txt:283-290).
module;

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

module loom.ui.app.app;

namespace cc::ui {

// ============================================================
// Env / config helpers (free functions)
// ============================================================

[[nodiscard]] std::optional<std::string> non_empty_env(const char* name) {
    if (const char* value = std::getenv(name); value && *value) {
        return std::string(value);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> first_non_empty_env(std::initializer_list<const char*> names) {
    for (const auto* name : names) {
        if (auto value = non_empty_env(name)) return value;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<bool> parse_bool_text(const std::string& value) {
    if (value == "true" || value == "1" || value == "yes" || value == "on") return true;
    if (value == "false" || value == "0" || value == "no" || value == "off") return false;
    return std::nullopt;
}

[[nodiscard]] std::optional<int> parse_int_text(const std::string& value) {
    try {
        return std::stoi(value);
    } catch (...) {
        return std::nullopt;
    }
}

// ============================================================
// Text / UTF helpers (free functions)
// ============================================================

[[nodiscard]] std::string trim_ascii_copy(std::string_view value) {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return std::string(value);
}

[[nodiscard]] std::string summarize_agent_description(
    std::string_view description) {
    auto newline = description.find('\n');
    if (newline != std::string_view::npos) {
        description = description.substr(0, newline);
    }
    std::string out = trim_ascii_copy(description);
    constexpr std::size_t kMaxSummaryBytes = 160;
    if (out.size() > kMaxSummaryBytes) {
        out.resize(kMaxSummaryBytes);
        out += "...";
    }
    return out;
}

[[nodiscard]] std::string lowercase_ascii(std::string_view value) {
    std::string out(value);
    for (char& ch : out) {
        ch = static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

[[nodiscard]] bool ascii_isspace(char ch) {
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

// ============================================================
// Text / UTF helpers (AppAdapter static members)
// ============================================================

[[nodiscard]] std::string AppAdapter::lowercase_ascii(std::string_view value) {
    std::string out(value);
    for (char& ch : out) {
        ch = static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

[[nodiscard]] bool AppAdapter::utf8_continuation(unsigned char ch) {
    return (ch & 0xC0) == 0x80;
}

[[nodiscard]] std::size_t AppAdapter::utf16_code_unit_count(
    std::string_view value) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < value.size();) {
        const auto c0 = static_cast<unsigned char>(value[i]);
        std::uint32_t codepoint = c0;
        std::size_t length = 1;

        if (c0 < 0x80) {
            codepoint = c0;
        } else if ((c0 & 0xE0) == 0xC0 &&
                   i + 1 < value.size() &&
                   utf8_continuation(static_cast<unsigned char>(value[i + 1]))) {
            codepoint =
                (static_cast<std::uint32_t>(c0 & 0x1F) << 6) |
                static_cast<std::uint32_t>(
                    static_cast<unsigned char>(value[i + 1]) & 0x3F);
            length = 2;
        } else if ((c0 & 0xF0) == 0xE0 &&
                   i + 2 < value.size() &&
                   utf8_continuation(static_cast<unsigned char>(value[i + 1])) &&
                   utf8_continuation(static_cast<unsigned char>(value[i + 2]))) {
            codepoint =
                (static_cast<std::uint32_t>(c0 & 0x0F) << 12) |
                (static_cast<std::uint32_t>(
                     static_cast<unsigned char>(value[i + 1]) & 0x3F) << 6) |
                static_cast<std::uint32_t>(
                    static_cast<unsigned char>(value[i + 2]) & 0x3F);
            length = 3;
        } else if ((c0 & 0xF8) == 0xF0 &&
                   i + 3 < value.size() &&
                   utf8_continuation(static_cast<unsigned char>(value[i + 1])) &&
                   utf8_continuation(static_cast<unsigned char>(value[i + 2])) &&
                   utf8_continuation(static_cast<unsigned char>(value[i + 3]))) {
            codepoint =
                (static_cast<std::uint32_t>(c0 & 0x07) << 18) |
                (static_cast<std::uint32_t>(
                     static_cast<unsigned char>(value[i + 1]) & 0x3F) << 12) |
                (static_cast<std::uint32_t>(
                     static_cast<unsigned char>(value[i + 2]) & 0x3F) << 6) |
                static_cast<std::uint32_t>(
                    static_cast<unsigned char>(value[i + 3]) & 0x3F);
            length = 4;
        }

        count += codepoint > 0xFFFF ? 2 : 1;
        i += length;
    }
    return count;
}

[[nodiscard]] std::size_t AppAdapter::rough_js_token_count(
    std::string_view value) {
    return static_cast<std::size_t>(
        std::llround(static_cast<double>(utf16_code_unit_count(value)) / 4.0));
}

} // namespace cc::ui
