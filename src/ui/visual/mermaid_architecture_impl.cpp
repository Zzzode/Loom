module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// An architecture-beta diagram declares groups, services and junctions
// (optionally nested through `in`), connects them with L/R/T/B port edges and
// aligns them with row/column directives. The terminal has no fcose layout,
// edge routing or icon/color contract, so the projection is a bounded native
// outline that validates every statement and transcribes the accepted DSL
// lines verbatim — no geometry, no routing, no color.

constexpr int kArchMaxStatements = 512;
constexpr int kArchMaxIdentBytes = 128;
constexpr int kArchMaxLabelBytes = 256;
constexpr int kArchWrapWidth = 64;
constexpr int kArchMaxRows = 1024;

bool arch_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

std::string_view arch_body(std::string_view source) {
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

// architecture-beta ids are [A-Za-z0-9_]+ and may start with a digit
// (the corpus uses ids such as "1Leftofmid").
bool is_identifier(std::string_view token) {
    token = trim_view(token);
    if (token.empty() || token.size() > kArchMaxIdentBytes) return false;
    return std::ranges::all_of(token, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    });
}

bool is_port(char c) { return c == 'L' || c == 'R' || c == 'T' || c == 'B'; }

bool arch_wrap(const std::string& label, Work& work, std::vector<std::string>& out) {
    std::size_t byte = 0;
    std::string current;
    int cells = 0;
    auto flush = [&]() -> bool {
        if (current.empty()) return true;
        if (out.size() >= kArchMaxRows || !work.spend(8)) return false;
        out.push_back(std::move(current));
        current.clear();
        cells = 0;
        return true;
    };
    while (byte < label.size()) {
        const std::size_t start = byte;
        while (byte < label.size() && label[byte] != ' ') ++byte;
        std::string word = label.substr(start, byte - start);
        while (byte < label.size() && label[byte] == ' ') ++byte;
        const int word_cells = ftxui::string_width(word);
        if (word_cells < 1) return false;
        if (word_cells > kArchWrapWidth) return false;
        const int gap = cells == 0 ? 0 : 1;
        if (cells + gap + word_cells > kArchWrapWidth) {
            if (!flush()) return false;
        }
        if (!current.empty()) current.push_back(' ');
        current += word;
        cells += word_cells + gap;
    }
    return flush();
}

// Validate an optional "(icon)" and/or "[label]" suffix and an optional
// " in parent" containment clause on a group/service declaration. Junction
// allows only the id and the optional containment clause. The id runs up to
// the first '(', '[' or space (the corpus writes "id(icon)[label]" with no
// spaces around the brackets).
bool validate_declaration(std::string_view keyword, std::string_view rest) {
    rest = trim_view(rest);
    std::size_t id_end = 0;
    while (id_end < rest.size()) {
        const char c = rest[id_end];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_')
            ++id_end;
        else
            break;
    }
    if (id_end == 0) return false;
    const std::string_view id = rest.substr(0, id_end);
    if (id.size() > kArchMaxIdentBytes) return false;
    std::string_view tail = trim_view(rest.substr(id_end));

    if (keyword == "junction") {
        if (tail.empty()) return true;
        return tail.starts_with("in ") && is_identifier(trim_view(tail.substr(3)));
    }

    // group/service: optional (icon), optional [label], optional " in parent".
    if (!tail.empty() && tail.front() == '(') {
        const auto close = tail.find(')');
        if (close == std::string_view::npos || !is_identifier(tail.substr(1, close - 1)))
            return false;
        tail = trim_view(tail.substr(close + 1));
    }
    if (!tail.empty() && tail.front() == '[') {
        const auto close = tail.find(']');
        if (close == std::string_view::npos || close <= 1) return false;
        const std::string label = trim_copy(tail.substr(1, close - 1));
        if (label.empty() || label.size() > kArchMaxLabelBytes || !unicode_text(label))
            return false;
        tail = trim_view(tail.substr(close + 1));
    }
    if (tail.empty()) return true;
    return tail.starts_with("in ") && is_identifier(trim_view(tail.substr(3)));
}

// Source endpoint: name[{group}]:P — identifier first, compass port last.
bool validate_source_endpoint(std::string_view text, char& port) {
    text = trim_view(text);
    const auto colon = text.find(':');
    if (colon == std::string_view::npos || colon + 1 != text.size() - 1) return false;
    port = text[colon + 1];
    if (!is_port(port)) return false;
    std::string_view head = trim_view(text.substr(0, colon));
    if (head.ends_with("{group}")) head = trim_view(head.substr(0, head.size() - 7));
    return is_identifier(head);
}

// Target endpoint: P:name[{group}] — compass port first, identifier last.
bool validate_target_endpoint(std::string_view text, char& port) {
    text = trim_view(text);
    if (text.size() < 3 || text[1] != ':') return false;
    port = text[0];
    if (!is_port(port)) return false;
    std::string_view head = trim_view(text.substr(2));
    if (head.ends_with("{group}")) head = trim_view(head.substr(0, head.size() - 7));
    return is_identifier(head);
}

// An edge joins two endpoints with one of "-->" (directed), "--"
// (undirected) or "-[label]-" (labeled).
bool validate_edge(std::string_view content) {
    std::size_t arrow_pos;
    std::size_t arrow_len;
    const auto label_open = content.find("-[");
    const auto directed = content.find("-->");
    const auto undirected = content.find(" -- ");
    if (label_open != std::string_view::npos &&
        (directed == std::string_view::npos || label_open < directed) &&
        (undirected == std::string_view::npos || label_open < undirected)) {
        const auto label_close = content.find("]-", label_open + 2);
        if (label_close == std::string_view::npos) return false;
        const std::string label =
            trim_copy(content.substr(label_open + 2, label_close - label_open - 2));
        if (label.empty() || label.size() > kArchMaxLabelBytes || !unicode_text(label))
            return false;
        arrow_pos = label_open;
        arrow_len = (label_close + 2) - label_open;
    } else if (directed != std::string_view::npos) {
        arrow_pos = directed;
        arrow_len = 3;
    } else if (undirected != std::string_view::npos) {
        arrow_pos = undirected + 1;  // point at the "--"
        arrow_len = 2;
    } else {
        return false;
    }
    char left_port = 0, right_port = 0;
    if (!validate_source_endpoint(content.substr(0, arrow_pos), left_port)) return false;
    if (!validate_target_endpoint(content.substr(arrow_pos + arrow_len), right_port))
        return false;
    return true;
}

bool validate_align(std::string_view rest) {
    rest = trim_view(rest);
    const auto space = rest.find(' ');
    if (space == std::string_view::npos) return false;
    const std::string_view direction = rest.substr(0, space);
    if (direction != "row" && direction != "column") return false;
    bool any = false;
    std::size_t pos = space + 1;
    while (pos < rest.size()) {
        while (pos < rest.size() && rest[pos] == ' ') ++pos;
        const auto next = rest.find(' ', pos);
        const std::string_view id =
            next == std::string_view::npos ? rest.substr(pos) : rest.substr(pos, next - pos);
        if (id.empty()) break;
        if (!is_identifier(id)) return false;
        any = true;
        pos = next == std::string_view::npos ? rest.size() : next + 1;
    }
    return any;
}

}  // namespace

std::optional<std::vector<std::string>> architecture_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!arch_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = arch_body(source);
    // No layout/color contract: a frontmatter theme/look/config key, an
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
    std::string title;
    int statement_count = 0;
    std::vector<std::string> title_rows;
    std::vector<std::string> out;
    // One guarded push for the title and every statement row (rule 22).
    auto push_guarded = [&](std::vector<std::string>& target, const std::string& row) -> bool {
        if (title_rows.size() + out.size() >= kArchMaxRows ||
            ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return false;
        target.push_back(row);
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
            if (content != "architecture-beta") return {};
            saw_header = true;
            continue;
        }
        if (++statement_count > kArchMaxStatements) return {};

        if (content.starts_with("title ")) {
            if (!title.empty()) return {};  // duplicate title
            title = trim_copy(content.substr(6));
            if (title.empty() || title.size() > kArchMaxLabelBytes ||
                !unicode_text(title))
                return {};
            std::vector<std::string> wrapped;
            if (!arch_wrap(title, work, wrapped)) return {};
            for (const auto& row : wrapped)
                if (!push_guarded(title_rows, row)) return {};
            continue;
        }

        // Accessibility metadata is accepted but not emitted as a frame row.
        if (content.starts_with("accTitle:") || content.starts_with("accDescr:"))
            continue;

        if (content.starts_with("group ") || content.starts_with("service ") ||
            content.starts_with("junction ")) {
            const auto sp = content.find(' ');
            const std::string_view keyword = content.substr(0, sp);
            if (!validate_declaration(keyword, content.substr(sp + 1))) return {};
            if (!push_guarded(out, std::string{content})) return {};
            continue;
        }

        if (content.starts_with("align ")) {
            if (!validate_align(content.substr(6))) return {};
            if (!push_guarded(out, std::string{content})) return {};
            continue;
        }

        // Every remaining statement must be a port edge.
        if (!validate_edge(content)) return {};
        if (!push_guarded(out, std::string{content})) return {};
    }

    if (!saw_header) return {};
    // A header-only chart is a valid empty frame: transcribe the keyword.
    if (title_rows.empty() && out.empty()) {
        if (!push_guarded(out, "architecture-beta")) return {};
    }
    if (!title_rows.empty()) {
        std::vector<std::string> combined;
        combined.reserve(title_rows.size() + out.size());
        combined.insert(combined.end(), title_rows.begin(), title_rows.end());
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    if (out.empty()) return {};
    return out;
}

}  // namespace loom::ui::mermaid::detail
