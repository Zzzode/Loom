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
    // source on fallback. The fence splitter tolerates trailing whitespace.
    {
        bool visual_gate = false;
        if (source.starts_with("---\n")) {
            std::size_t search = 4;
            while (search < source.size()) {
                const auto nl = source.find('\n', search);
                const std::string_view line =
                    source.substr(search, nl == std::string_view::npos
                                                      ? std::string_view::npos
                                                      : nl - search);
                std::size_t i = 0;
                bool is_fence = false;
                if (line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
                    i = 3;
                    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
                    is_fence = i == line.size();
                }
                const auto key_start = line.find_first_not_of(' ');
                if (!is_fence && key_start != std::string_view::npos) {
                    std::string_view key = line.substr(key_start);
                    const auto colon = key.find(':');
                    if (colon != std::string_view::npos) key = key.substr(0, colon);
                    if (key == "theme" || key == "look" || key == "config")
                        visual_gate = true;
                }
                if (is_fence) break;
                if (nl == std::string_view::npos) break;
                search = nl + 1;
            }
        }
        if (!visual_gate) {
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
                        visual_gate = true;
                }
                if (eol == std::string_view::npos) break;
            }
        }
        if (visual_gate) return {};
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
    return out;
}

}  // namespace loom::ui::mermaid::detail
