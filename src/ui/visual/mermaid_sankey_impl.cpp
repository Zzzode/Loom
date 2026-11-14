module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;

import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A sankey-beta diagram is a header followed by CSV link rows
// (`source,target,value`). The terminal has no sankey flow geometry or color
// contract, so the projection is a bounded native outline: each row is
// validated against the closed three-field grammar and transcribed as a
// labeled link line. Any malformed row, quote, embedded comma or non-numeric
// value rejects the whole source rather than drawing an approximate graph.

constexpr std::size_t kSkMaxLinks = 512;
constexpr std::size_t kSkMaxRows = 1024;
constexpr std::size_t kSkMaxValueDigits = 9;

std::string_view sk_trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return value;
}

bool sk_printable_label(std::string_view value) {
    if (value.empty()) return false;
    for (const unsigned char c : value)
        if (c < 0x20 || c == 0x7f || c == '"' || c == ',') return false;
    return true;
}

bool sk_uint(std::string_view token, std::string& normalized) {
    if (token.empty() || token.size() > kSkMaxValueDigits) return false;
    std::size_t value = 0;
    for (const char c : token) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + static_cast<std::size_t>(c - '0');
        if (value > 999'999'999u) return false;
    }
    if (value == 0) return false;  // a sankey link must carry positive flow
    normalized = std::string(token);
    return true;
}

// Key-based visual gate over a leading frontmatter block. A non-description
// top-level key (config/theme/...) keeps the whole source on fallback because
// the text surface cannot honor a sankey layout/color contract.
bool sk_frontmatter_gated(std::string_view source, std::string_view& body) {
    if (!source.starts_with("---\n")) {
        body = source;
        return false;
    }
    std::size_t search = 4;
    std::optional<std::size_t> close_nl;
    std::size_t close_line_start = 0;
    while (true) {
        const auto nl = source.find('\n', search);
        const auto line = source.substr(
            search, nl == std::string_view::npos ? std::string_view::npos : nl - search);
        if (line == "---") {
            close_nl = nl;
            close_line_start = search;
            break;
        }
        // An indented or whitespace-suffixed fence cannot close the envelope;
        // mirrors the renderer-free generator's malformed-fence classification.
        // Deliberately stricter than the generator (fail-safe, reviewed NIT):
        // any non-exact line containing "---" — including prose such as
        // "description: a --- b" — is gated here. The divergence can only
        // reject, never mis-draw, and is unreachable on the pinned corpus.
        if (line.find("---") != std::string_view::npos) return true;
        if (nl == std::string_view::npos) return true;  // unclosed fence
        search = nl + 1;
    }
    if (!close_nl) return true;
    if (*close_nl == std::string_view::npos) {
        body = std::string_view{};
        return false;
    }
    const auto frontmatter = source.substr(4, close_line_start - 4);
    for (std::size_t pos = 0;;) {
        const auto eol = frontmatter.find('\n', pos);
        const auto line = frontmatter.substr(
            pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
        pos = eol == std::string_view::npos ? frontmatter.size() : eol + 1;
        const auto key_start = line.find_first_not_of(' ');
        if (key_start == std::string_view::npos) {
            if (eol == std::string_view::npos) break;
            continue;
        }
        if (key_start != 0) {
            if (eol == std::string_view::npos) break;
            continue;
        }
        const auto column0 = line.substr(key_start);
        if (column0.starts_with('#')) {
            if (eol == std::string_view::npos) break;
            continue;
        }
        const auto colon = column0.find(':');
        if (colon == std::string_view::npos) return true;
        if (sk_trim(column0.substr(0, colon)) != "description") return true;
        if (eol == std::string_view::npos) break;
    }
    body = source.substr(*close_nl + 1);
    return false;
}

struct SkLink {
    std::string from;
    std::string to;
    std::string value;
};

std::optional<SkLink> sk_link_row(std::string_view line) {
    // Exactly three comma-separated fields; labels keep internal spaces but
    // reject quotes, embedded commas and control bytes.
    const auto first = line.find(',');
    if (first == std::string_view::npos) return std::nullopt;
    const auto second = line.find(',', first + 1);
    if (second == std::string_view::npos) return std::nullopt;
    if (line.find(',', second + 1) != std::string_view::npos) return std::nullopt;
    const auto from = sk_trim(line.substr(0, first));
    const auto to = sk_trim(line.substr(first + 1, second - first - 1));
    const auto value_token = sk_trim(line.substr(second + 1));
    if (!sk_printable_label(from) || !sk_printable_label(to)) return std::nullopt;
    std::string value;
    if (!sk_uint(value_token, value)) return std::nullopt;
    return SkLink{std::string(from), std::string(to), std::move(value)};
}

}  // namespace

std::optional<std::vector<std::string>> sankey_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return std::nullopt;
    if (!work.spend(source.size() * 2 + 256)) return std::nullopt;
    // Reject tabs/CR/control bytes anywhere, not just as leading indentation:
    // stricter than the renderer-free generator but fail-safe (reject-only),
    // and unreachable on the pinned corpus. Reviewed divergence, D-211.
    for (const unsigned char c : source)
        if (c == '\t' || c == '\r' || c == 0x7f || (c < 0x20 && c != '\n'))
            return std::nullopt;

    std::string_view body;
    if (sk_frontmatter_gated(source, body)) return std::nullopt;

    std::vector<SkLink> links;
    bool header_seen = false;
    std::size_t at = 0;
    while (at <= body.size()) {
        const auto nl = body.find('\n', at);
        auto raw = body.substr(at, nl == std::string_view::npos
                                          ? std::string_view::npos : nl - at);
        at = nl == std::string_view::npos ? body.size() + 1 : nl + 1;
        const auto content_pos = raw.find_first_not_of(' ');
        if (content_pos == std::string_view::npos) continue;
        if (content_pos % 2 != 0) return std::nullopt;  // strict two-space indent
        const auto content = raw.substr(content_pos);
        // Plain comments are ignored, but an engine `%%{init ...}%%` directive
        // is a visual/layout contract the text surface cannot honor: reject
        // it explicitly rather than silently dropping it.
        if (content.starts_with("%%")) {
            if (content.starts_with("%%{")) return std::nullopt;
            continue;
        }

        if (!header_seen) {
            if (content != "sankey-beta") return std::nullopt;
            header_seen = true;
            continue;
        }
        const auto link = sk_link_row(content);
        if (!link) return std::nullopt;
        if (links.size() >= kSkMaxLinks) return std::nullopt;
        links.push_back(std::move(*link));
    }
    if (!header_seen || links.empty()) return std::nullopt;

    std::vector<std::string> rows;
    if (!work.spend(32)) return std::nullopt;
    rows.push_back("sankey-beta");
    for (const auto& link : links) {
        const std::string row = link.from + " -> " + link.to + ": " + link.value;
        if (rows.size() >= kSkMaxRows || ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return std::nullopt;
        rows.push_back(row);
    }
    return rows;
}

}  // namespace loom::ui::mermaid::detail
