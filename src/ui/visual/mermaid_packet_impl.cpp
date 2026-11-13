module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A packet diagram is a title plus a list of bit fields, each either a
// single bit position (`0: "URG"`) or a range (`0-15: "Source Port"`). The
// terminal has no bit-cell geometry or color contract, so the projection is
// a bounded native outline: each statement is shape-validated and the
// physical lines are transcribed verbatim, preserving the indentation. No
// geometry, no color. Both `packet` and the `packet-beta` alias route here.

constexpr int kPacketMaxStatements = 512;
constexpr int kPacketMaxRows = 1024;
constexpr int kPacketMaxTextBytes = 512;
constexpr int kPacketIndent = 2;

bool packet_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

// A closing fence is "---" optionally followed by spaces/tabs.
std::string_view packet_body(std::string_view source) {
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

std::string_view trim_spaces(std::string_view value) {
    while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
    while (!value.empty() && value.back() == ' ') value.remove_suffix(1);
    return value;
}

bool is_uint(std::string_view token) {
    return !token.empty() && std::ranges::all_of(token, [](char c) {
        return c >= '0' && c <= '9';
    });
}

// Printable, single-line ASCII prose (titles and quoted labels). Control
// bytes, DEL and non-ASCII are rejected rather than emitted as invisible
// cells.
bool is_prose(std::string_view text) {
    if (text.empty() || text.size() > static_cast<std::size_t>(kPacketMaxTextBytes))
        return false;
    return std::ranges::all_of(text, [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u >= 0x20 && u <= 0x7E;
    });
}

// A bit position prefix: a single unsigned integer or two joined by exactly
// one '-' (a range). No signs, no spaces, no empty side.
bool is_position(std::string_view prefix) {
    const auto dash = prefix.find('-');
    if (dash == std::string_view::npos) return is_uint(prefix);
    if (prefix.find('-', dash + 1) != std::string_view::npos) return false;
    return is_uint(prefix.substr(0, dash)) &&
           is_uint(prefix.substr(dash + 1));
}

// A field line: `<position>: "<label>"` with a non-empty printable label and
// no token outside the quotes.
bool is_field(std::string_view content) {
    const auto colon = content.find(':');
    if (colon == std::string_view::npos) return false;
    if (!is_position(trim_spaces(content.substr(0, colon)))) return false;
    std::string_view rest = trim_spaces(content.substr(colon + 1));
    if (rest.size() < 2 || rest.front() != '"' || rest.back() != '"')
        return false;
    const std::string_view label = rest.substr(1, rest.size() - 2);
    if (label.find('"') != std::string_view::npos) return false;
    return is_prose(label);
}

bool is_packet_header(std::string_view header) {
    return header == "packet" || header == "packet-beta";
}

}  // namespace

std::optional<std::vector<std::string>> packet_layout(std::string_view source,
                                                      Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!packet_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = packet_body(source);
    // No bit-cell/color contract: a frontmatter theme/look/config key, an
    // %%{init}%% directive or a classDef/:::/style statement keeps the whole
    // source on fallback. Trim the key before comparing so the YAML-legal
    // "config : 42" spacing shape still gates.
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
                    if (colon != std::string_view::npos)
                        key = trim_spaces(key.substr(0, colon));
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
    int statement_count = 0;
    std::vector<std::string> out;
    auto push_guarded = [&](const std::string& row) -> bool {
        if (out.size() >= kPacketMaxRows || ftxui::string_width(row) > kWidthLimit ||
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
            if (first_non_space != 0 || !is_packet_header(content)) return {};
            saw_header = true;
            continue;
        }

        // Every statement sits on the strict two-space grid.
        if (first_non_space != static_cast<std::size_t>(kPacketIndent)) return {};
        if (++statement_count > kPacketMaxStatements) return {};

        const bool accepted = content.starts_with("title ")
                                  ? is_prose(content.substr(6))
                                  : is_field(content);
        if (!accepted) return {};
        if (!push_guarded(std::string{line})) return {};  // verbatim, indent preserved
    }

    if (!saw_header) return {};
    if (out.empty()) {
        const auto nl = body.find('\n');
        if (!push_guarded(std::string{body.substr(0, nl == std::string_view::npos
                                                            ? body.size()
                                                            : nl)}))
            return {};  // header-only frame
    } else {
        std::vector<std::string> combined;
        combined.reserve(out.size() + 1);
        const auto nl = body.find('\n');
        combined.push_back(std::string{body.substr(0, nl == std::string_view::npos
                                                           ? body.size()
                                                           : nl)});
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
