module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A kanban diagram is a flat list of cards: each line declares an id, an
// optional [label] and an optional @{ key: value } annotation block. The
// terminal has no board/column geometry or color contract, so the projection
// is a bounded native outline that validates every card and transcribes the
// accepted DSL lines verbatim — no layout, no color.

constexpr int kKanbanMaxCards = 512;
constexpr int kKanbanMaxIdentBytes = 128;
constexpr int kKanbanMaxLabelBytes = 256;
constexpr int kKanbanMaxAnnotationBytes = 512;
constexpr int kKanbanMaxRows = 1024;

bool kanban_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

std::string_view kanban_body(std::string_view source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string_view::npos) return source;
    return source.substr(close + 5);
}

std::string trim_copy(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return std::string(value);
}

// View-only trim; never binds a temporary, so it is safe to reassign a
// string_view parameter inside the validators.
std::string_view trim_view(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return value;
}

// kanban ids are [A-Za-z0-9_]+.
bool is_identifier(std::string_view token) {
    token = trim_view(token);
    if (token.empty() || token.size() > kKanbanMaxIdentBytes) return false;
    return std::ranges::all_of(token, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    });
}

// Validate one card: id, optional [label], optional @{ annotation }, with
// nothing trailing. Brackets are not nested in the grammar.
bool validate_card(std::string_view content) {
    std::size_t id_end = 0;
    while (id_end < content.size()) {
        const char c = content[id_end];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_')
            ++id_end;
        else
            break;
    }
    if (id_end == 0 || !is_identifier(content.substr(0, id_end))) return false;
    std::string_view tail = trim_view(content.substr(id_end));

    if (!tail.empty() && tail.front() == '[') {
        const auto close = tail.find(']');
        if (close == std::string_view::npos || close <= 1) return false;
        const std::string label = trim_copy(tail.substr(1, close - 1));
        if (label.empty() || label.size() > kKanbanMaxLabelBytes ||
            label.find('[') != std::string::npos || label.find(']') != std::string::npos ||
            !unicode_text(label))
            return false;
        tail = trim_view(tail.substr(close + 1));
    }

    if (!tail.empty()) {
        // The only remaining accepted shape is a single @{ ... } block.
        if (tail.size() < 4 || !tail.starts_with("@{") || tail.back() != '}') return false;
        const std::string annotation = trim_copy(tail.substr(2, tail.size() - 3));
        if (annotation.empty() || annotation.size() > kKanbanMaxAnnotationBytes ||
            annotation.find('{') != std::string::npos ||
            annotation.find('}') != std::string::npos)
            return false;
        if (!unicode_text(annotation)) return false;
        tail = trim_view(tail.substr(tail.size()));
    }
    return tail.empty();
}

}  // namespace

std::optional<std::vector<std::string>> kanban_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!kanban_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = kanban_body(source);
    // No color/layout contract: a frontmatter theme/look/config key, an
    // %%{init}%% directive or a classDef/:::/style statement keeps the whole
    // source on fallback. Key-based so prose words are not false gates.
    {
        bool visual_gate = false;
        if (source.starts_with("---\n")) {
            const auto close = source.find("\n---\n", 4);
            if (close != std::string_view::npos) {
                const std::string_view frontmatter(source.data(), close);
                for (std::size_t pos = 0;;) {
                    const auto eol = frontmatter.find('\n', pos);
                    std::string_view line = frontmatter.substr(
                        pos, eol == std::string_view::npos ? std::string_view::npos
                                                           : eol - pos);
                    pos = eol == std::string_view::npos ? frontmatter.size() : eol + 1;
                    const auto key_start = line.find_first_not_of(' ');
                    if (key_start != std::string_view::npos) {
                        std::string_view key = line.substr(key_start);
                        const auto colon = key.find(':');
                        if (colon != std::string_view::npos) key = key.substr(0, colon);
                        if (key == "theme" || key == "look" || key == "config")
                            visual_gate = true;
                    }
                    if (eol == std::string_view::npos) break;
                }
            }
        }
        if (!visual_gate) {
            for (std::size_t pos = 0;;) {
                const auto eol = body.find('\n', pos);
                std::string_view line = body.substr(
                    pos, eol == std::string_view::npos ? std::string_view::npos
                                                       : eol - pos);
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
    int card_count = 0;
    std::vector<std::string> out;
    // One guarded push for every emitted row (rule 22).
    auto push_guarded = [&](const std::string& row) -> bool {
        if (out.size() >= kKanbanMaxRows || ftxui::string_width(row) > kWidthLimit ||
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
        const auto trimmed_pos = line.find_first_not_of(' ');
        if (trimmed_pos == std::string_view::npos) continue;
        const std::string_view content = line.substr(trimmed_pos);
        if (content.starts_with("%%")) continue;

        if (!saw_header) {
            if (content != "kanban") return {};
            saw_header = true;
            continue;
        }
        if (++card_count > kKanbanMaxCards) return {};
        if (!validate_card(content)) return {};
        // A card line is short enough to emit directly; a long label is still
        // bounded by kKanbanMaxLabelBytes and the 240-cell guard.
        if (!push_guarded(std::string{content})) return {};
    }

    if (!saw_header) return {};
    // A header-only board is a valid empty frame: transcribe the keyword.
    if (out.empty()) {
        if (!push_guarded("kanban")) return {};
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
