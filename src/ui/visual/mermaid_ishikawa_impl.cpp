module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// An ishikawa-beta (fishbone) diagram is a relative-indentation tree: the
// first 2-space node is the effect, and deeper indented bare labels are cause
// categories and nested sub-causes. The terminal has no fishbone spine,
// branch geometry or color contract, so the projection is a bounded native
// outline that validates each node and transcribes the line verbatim,
// preserving its source indentation. No geometry, no color.

constexpr int kIshikawaMaxNodes = 512;
constexpr int kIshikawaMaxLabelBytes = 512;
constexpr int kIshikawaMaxRows = 1024;
constexpr int kIshikawaMinIndent = 2;

bool ishikawa_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

// A closing fence is "---" optionally followed by spaces/tabs.
std::string_view ishikawa_body(std::string_view source) {
    if (!source.starts_with("---\n")) return source;
    std::size_t search = 4;
    while (true) {
        const auto nl = source.find('\n', search);
        const std::string_view line =
            source.substr(search, nl == std::string_view::npos ? std::string_view::npos
                                                                : nl - search);
        std::size_t i = 0;
        if (line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
            i = 3;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            if (i == line.size()) {
                if (nl == std::string_view::npos) return std::string_view{};
                return source.substr(nl + 1);
            }
        }
        if (nl == std::string_view::npos) return source;
        search = nl + 1;
    }
}

// A bare cause label: non-empty text limited to the ishikawa label character
// set (letters, digits, spaces and the path/hyphen punctuation seen in the
// corpus). Any structural or visual marker means this is not a bare label and
// the source must fall back rather than be mis-rendered.
bool is_bare_label(std::string_view label) {
    if (label.empty() || label.size() > static_cast<std::size_t>(kIshikawaMaxLabelBytes))
        return false;
    if (!unicode_text(std::string{label})) return false;
    for (char c : label) {
        const bool allowed =
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-' || c == '.';
        if (!allowed) return false;
    }
    return true;
}

// RFC 0043: a column-0 `config:` block whose sole leaf is an indent-2 `theme`
// inside the closed corpus table is accepted and disclosed through a bounded
// footer; every other frontmatter shape keeps the previous whole-source
// fallback. The renderer never reads `theme`, so the footer states that the
// palette was not applied.
constexpr std::size_t kIshikawaNoticeLineLimit = 96u;
constexpr std::string_view kIshikawaNoticeHeader = "Ishikawa native projection v1";
constexpr std::array<std::string_view, 2> kIshikawaThemeTable{{"dark", "forest"}};

// Trim, strip a trailing YAML comment outside quotes, then dequote a matched
// pair; an unmatched quote is a mismatch, never a value. TU-local copy of the
// agentflow helper (RFC 0041) so no shared module is widened.
std::optional<std::string> ishikawa_scalar(std::string_view raw, Work& work) {
    const auto trim_view = [](std::string_view value) {
        const auto first = value.find_first_not_of(" \t");
        if (first == std::string_view::npos) return std::string_view{};
        const auto last = value.find_last_not_of(" \t");
        return value.substr(first, last - first + 1u);
    };
    auto value = trim_view(raw);
    char quote = 0;
    for (std::size_t n = 0; n < value.size(); ++n) {
        const char c = value[n];
        if (quote) {
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '#' && (n == 0u || value[n - 1u] == ' ' || value[n - 1u] == '\t')) {
            value = trim_view(value.substr(0, n));
            break;
        }
    }
    if (!work.spend(value.size() + 1u)) return std::nullopt;
    if (value.size() >= 2u &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\'')))
        value = value.substr(1u, value.size() - 2u);
    return std::string(value);
}

bool ishikawa_notice_line(std::vector<std::string>& notices, std::string_view line, Work& work) {
    if (line.size() > kIshikawaNoticeLineLimit ||
        !work.spend(line.size() * 4u + sizeof(std::string) + 32u))
        return false;
    notices.emplace_back(line);
    return true;
}

// Returns true when the whole source must fall back. Otherwise `notices` holds
// the bounded disclosure footer (empty when no theme leaf was admitted).
bool ishikawa_frontmatter_gated(std::string_view source, std::vector<std::string>& notices,
                                Work& work) {
    if (!source.starts_with("---\n")) return false;
    bool in_config = false;
    bool saw_config = false;
    std::optional<std::string> theme;
    std::size_t search = 4;
    while (search < source.size()) {
        const auto nl = source.find('\n', search);
        const std::string_view line =
            source.substr(search, nl == std::string_view::npos ? std::string_view::npos
                                                               : nl - search);
        std::size_t i = 0;
        bool is_fence = false;
        if (line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
            i = 3;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            is_fence = i == line.size();
        }
        if (is_fence) break;
        const auto key_start = line.find_first_not_of(' ');
        if (key_start == std::string_view::npos) {  // blank line
            if (nl == std::string_view::npos) break;
            search = nl + 1;
            continue;
        }
        if (key_start != 0) {
            const std::string_view leaf = line.substr(key_start);
            if (in_config) {
                if (!leaf.starts_with('#')) {  // YAML comment
                    const auto colon = leaf.find(':');
                    if (colon == std::string_view::npos ||
                        leaf.substr(0, colon) != "theme" || key_start != 2u || theme)
                        return true;
                    auto value = ishikawa_scalar(leaf.substr(colon + 1u), work);
                    if (!value ||
                        std::ranges::find(kIshikawaThemeTable, *value) ==
                            kIshikawaThemeTable.end())
                        return true;
                    theme = std::move(*value);
                }
            } else {
                // Outside a config block the previous flat scan still gates
                // every indented visual key; other indented lines are ignored.
                const auto colon = leaf.find(':');
                const auto key = leaf.substr(0, colon);
                if (key == "theme" || key == "look" || key == "config") return true;
            }
            if (nl == std::string_view::npos) break;
            search = nl + 1;
            continue;
        }
        const std::string_view column0 = line;
        if (column0.starts_with('#')) {  // YAML comment
            if (nl == std::string_view::npos) break;
            search = nl + 1;
            continue;
        }
        const auto colon = column0.find(':');
        const auto key = column0.substr(0, colon);
        if (key == "config") {
            if (saw_config) return true;
            saw_config = true;
            in_config = true;
        } else {
            // `description` and every other column-0 key keep the previous
            // passthrough and close an open config block; a bare visual key
            // still gates.
            if (key == "theme" || key == "look") return true;
            in_config = false;
        }
        if (nl == std::string_view::npos) break;
        search = nl + 1;
    }
    if (saw_config && !theme) return true;
    if (!theme) return false;
    if (!ishikawa_notice_line(notices, kIshikawaNoticeHeader, work)) return true;
    std::string line = "config.theme=";
    line += *theme;
    line += " -> colour palette; not applied";
    return !ishikawa_notice_line(notices, line, work);
}

}  // namespace

std::optional<std::vector<std::string>> ishikawa_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!ishikawa_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = ishikawa_body(source);
    // No fishbone/color contract: a frontmatter theme/look/config key, an
    // %%{init}%% directive or a classDef/:::/style statement keeps the whole
    // source on fallback. The single admitted exception is RFC 0043's
    // config-block theme leaf, disclosed through `notices`.
    std::vector<std::string> notices;
    if (ishikawa_frontmatter_gated(source, notices, work)) return {};
    for (std::size_t pos = 0;;) {
        const auto eol = body.find('\n', pos);
        std::string_view line = body.substr(
            pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
        pos = eol == std::string_view::npos ? body.size() : eol + 1;
        const auto trimmed = line.find_first_not_of(' ');
        if (trimmed != std::string_view::npos) {
            const std::string_view content = line.substr(trimmed);
            if (content.starts_with("%%{init") ||
                content.starts_with("classDef") ||
                content.starts_with("style ") ||
                content.find(":::") != std::string_view::npos)
                return {};
        }
        if (eol == std::string_view::npos) break;
    }

    bool saw_header = false;
    int node_count = 0;
    std::vector<std::string> out;
    auto push_guarded = [&](const std::string& row) -> bool {
        if (out.size() >= kIshikawaMaxRows || ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return false;
        out.push_back(row);
        return true;
    };

    std::size_t line_start = 0;
    while (line_start <= body.size()) {
        std::size_t line_end = body.find('\n', line_start);
        std::string_view line = body.substr(
            line_start, line_end == std::string_view::npos ? std::string_view::npos
                                                            : line_end - line_start);
        line_start = line_end == std::string_view::npos ? body.size() + 1 : line_end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        const auto first_non_space = line.find_first_not_of(' ');
        if (first_non_space == std::string_view::npos) continue;  // blank line
        const std::string_view content = line.substr(first_non_space);
        if (content.starts_with("%%")) continue;

        if (!saw_header) {
            if (content != "ishikawa-beta") return {};
            saw_header = true;
            continue;
        }
        // Every node carries a relative indent of at least two spaces (tabs
        // were pre-rejected); the depth itself is relative and not grid-locked.
        if (first_non_space < static_cast<std::size_t>(kIshikawaMinIndent)) return {};
        if (++node_count > kIshikawaMaxNodes) return {};
        if (!is_bare_label(content)) return {};
        if (!push_guarded(std::string{line})) return {};  // verbatim, indent preserved
    }

    if (!saw_header) return {};
    if (out.empty()) {
        if (!push_guarded("ishikawa-beta")) return {};  // header-only frame
    } else {
        std::vector<std::string> combined;
        combined.reserve(out.size() + 1);
        combined.push_back("ishikawa-beta");
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    // RFC 0043: disclose the validated, not-applied theme leaf below the art,
    // with one blank separator, under the same row/Work budget.
    if (!notices.empty()) {
        std::size_t bytes = 0;
        for (const auto& notice : notices) bytes += notice.size();
        if (out.size() + 1u + notices.size() > kIshikawaMaxRows ||
            !work.spend((1u + notices.size()) * (4u * sizeof(std::string) + 256u) +
                        bytes * 2u + 256u))
            return {};
        out.emplace_back();
        for (const auto& notice : notices) out.push_back(notice);
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
