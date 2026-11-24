module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// D-461 stage 4: the bare `info` directive is not a diagram graph. Upstream
// renders a fixed panel whose only content is the running Mermaid version
// text (the fixture descriptions state exactly that), so the terminal
// projection is a fixed banner naming the pinned upstream version. The pin
// below is the single source of that text; the info cohort gate reads
// `tests/fixtures/mermaid_official/manifest.json` and fails if the banner and
// `upstream.tag` disagree, so a manifest bump cannot drift silently.
constexpr std::string_view kPinnedMermaidVersion = "12.2.0";

constexpr std::size_t kInfoMaxFrontmatterBytes = 4096u;

std::string_view trim_space(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return value;
}

bool is_blank_or_comment(std::string_view line) {
    const auto text = trim_space(line);
    return text.empty() || text.front() == '#';
}

// The E2b non-terminal leaf whitelist, projected as disclosure text. Any other
// key or value keeps the whole source closed (fail-closed fallback).
bool info_leaf_role(std::string_view key, std::string_view value, std::string& notice) {
    if (key == "logLevel") {
        if (value != "0" && value != "1" && value != "2") return false;
        notice = "config.logLevel=";
        notice += value;
        notice += " -> logger only; not applied";
        return true;
    }
    if (key == "fontFamily") {
        if (value != "courier") return false;
        notice = "config.fontFamily=\"";
        notice += value;
        notice += "\" -> terminal font; not applied";
        return true;
    }
    if (key == "securityLevel") {
        if (value != "loose" && value != "sandbox") return false;
        notice = "config.securityLevel=\"";
        notice += value;
        notice += "\" -> no interaction layer; not applied";
        return true;
    }
    return false;
}

bool is_block_indicator(std::string_view value) {
    return value == ">" || value == ">-" || value == ">+" || value == "|" ||
           value == "|-" || value == "|+";
}

std::vector<std::string_view> info_lines(std::string_view source) {
    std::vector<std::string_view> lines;
    std::size_t pos = 0;
    while (pos <= source.size()) {
        const auto nl = source.find('\n', pos);
        auto line = source.substr(pos, nl == std::string_view::npos ? std::string_view::npos
                                                                   : nl - pos);
        if (line.ends_with('\r')) line.remove_suffix(1);
        lines.push_back(line);
        if (nl == std::string_view::npos) break;
        pos = nl + 1;
    }
    return lines;
}

struct InfoConfigLeaf {
    std::string key;
    std::string notice;
};

// Closed frontmatter grammar: a column-0 `---` fence, a YAML `description`
// (plain or block) and an optional indent-2 `config:` map limited to the E2b
// leaf whitelist. Every other shape rejects the source.
std::optional<std::vector<InfoConfigLeaf>> info_frontmatter(
    const std::vector<std::string_view>& lines, std::size_t close) {
    std::vector<InfoConfigLeaf> leaves;
    bool description_seen = false;
    bool config_seen = false;
    bool in_config = false;
    bool in_block = false;
    std::size_t bytes = 0;
    for (std::size_t n = 1; n < close; ++n) {
        const auto& line = lines[n];
        bytes += line.size() + 1u;
        if (bytes > kInfoMaxFrontmatterBytes) return std::nullopt;
        if (is_blank_or_comment(line)) continue;
        const bool indented = line.front() == ' ' || line.front() == '\t';
        if (indented) {
            if (line.front() == '\t') return std::nullopt;
            if (in_config) {
                // Exactly one indent level of plain `key: value` leaves.
                if (line.starts_with("  ") && line.size() > 2u && line[2] == ' ')
                    return std::nullopt;
                const auto text = trim_space(line);
                const auto colon = text.find(':');
                if (colon == std::string_view::npos) return std::nullopt;
                const auto key = trim_space(text.substr(0, colon));
                const auto value = trim_space(text.substr(colon + 1u));
                if (key.empty() || value.empty()) return std::nullopt;
                std::string notice;
                if (!info_leaf_role(key, value, notice)) return std::nullopt;
                for (const auto& leaf : leaves)
                    if (leaf.key == key) return std::nullopt;
                leaves.push_back({std::string(key), std::move(notice)});
                continue;
            }
            if (in_block) continue;  // folded description content
            return std::nullopt;
        }
        in_config = false;
        in_block = false;
        const auto colon = line.find(':');
        if (colon == std::string_view::npos) return std::nullopt;
        const auto key = trim_space(line.substr(0, colon));
        const auto value = trim_space(line.substr(colon + 1u));
        if (key == "description") {
            if (description_seen) return std::nullopt;
            description_seen = true;
            if (value.empty()) {
                in_block = true;
                continue;
            }
            if (is_block_indicator(value)) {
                in_block = true;
                continue;
            }
            continue;  // plain scalar description
        }
        if (key == "config") {
            if (config_seen || !value.empty()) return std::nullopt;
            config_seen = true;
            in_config = true;
            continue;
        }
        return std::nullopt;
    }
    if (config_seen && leaves.empty()) return std::nullopt;  // empty config: closed
    std::ranges::sort(leaves, {}, &InfoConfigLeaf::key);
    return leaves;
}

}  // namespace

// The projection is the fixed upstream info panel: one banner line naming the
// pinned Mermaid version, followed by the accepted-config disclosure lines.
// Nothing from the body is painted because the directive carries no content.
std::optional<std::vector<std::string>> info_layout(std::string_view source, Work& work) {
    if (source.empty()) return std::nullopt;
    // Charge the source-proportional fee before any parse or allocation; the
    // per-line charges below cover the emitted panel.
    if (!work.spend(source.size() + 64u)) return std::nullopt;
    const auto lines = info_lines(source);
    std::size_t body_start = 0;
    std::vector<InfoConfigLeaf> leaves;
    if (!lines.empty() && lines[0] == "---") {
        std::size_t close = lines.size();
        for (std::size_t n = 1; n < lines.size(); ++n) {
            if (lines[n] == "---") {
                close = n;
                break;
            }
        }
        if (close == lines.size()) return std::nullopt;  // unterminated fence
        auto parsed = info_frontmatter(lines, close);
        if (!parsed) return std::nullopt;
        leaves = std::move(*parsed);
        body_start = close + 1;
    }
    std::string_view directive;
    std::size_t directives = 0;
    for (std::size_t n = body_start; n < lines.size(); ++n) {
        const auto text = trim_space(lines[n]);
        if (text.empty() || text.starts_with("%%")) {
            // A body init/directive can alter the host's configuration; the
            // panel projection has no reviewed rule for it, so it stays
            // closed rather than silently ignored. Plain `%%` comments are
            // inert and skipped.
            if (text.starts_with("%%{")) return std::nullopt;
            continue;
        }
        directive = text;
        ++directives;
    }
    if (directives != 1u) return std::nullopt;
    if (directive != "info" && directive != "info showInfo") return std::nullopt;

    if (!work.spend(8u * sizeof(std::string) + 64u)) return std::nullopt;
    const std::string text = std::string("Mermaid ") + std::string(kPinnedMermaidVersion);
    const auto inner = static_cast<std::size_t>(ftxui::string_width(text)) + 2u;
    std::vector<std::string> out;
    out.reserve(3u + leaves.size());
    auto line = [&](std::string value) {
        if (!work.spend(value.size() + 1u)) return false;
        out.push_back(std::move(value));
        return true;
    };
    std::string rule;
    rule.reserve(inner * 3u);
    for (std::size_t n = 0; n < inner; ++n) rule += "─";
    if (!line("┌" + rule + "┐")) return std::nullopt;
    if (!line("│ " + text + " │")) return std::nullopt;
    if (!line("└" + rule + "┘")) return std::nullopt;
    for (const auto& leaf : leaves)
        if (!line(leaf.notice)) return std::nullopt;
    return out;
}

}  // namespace loom::ui::mermaid::detail
