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

// D-429: the closed `config.packet` namespace plus the frontmatter title. The
// native projection transcribes fields, never a bit grid, so `showBits`,
// `bitOrder` and `bitsPerRow` are inert and disclosed; the frontmatter title is
// honoured as a transcribed `title <text>` line (the body form), so no source
// content is dropped silently.
struct PacketFrontmatter {
    std::optional<std::string> title;
    std::optional<std::string> show_bits;
    std::optional<std::string> bit_order;
    std::optional<std::string> bits_per_row;
};

std::optional<PacketFrontmatter> packet_frontmatter(std::string_view source) {
    PacketFrontmatter out;
    if (!source.starts_with("---\n")) return out;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string_view::npos) return out;
    const std::string_view frontmatter = source.substr(0, close);
    enum class Section { Description, Config, Packet } section = Section::Description;
    bool description_seen = false, config_seen = false, packet_seen = false;
    bool title_seen = false;
    for (std::size_t pos = 0;;) {
        const auto eol = frontmatter.find('\n', pos);
        const auto end = eol == std::string_view::npos ? frontmatter.size() : eol;
        const std::string_view line = frontmatter.substr(pos, end - pos);
        const bool last = eol == std::string_view::npos;
        pos = last ? frontmatter.size() + 1u : eol + 1u;
        const auto key_start = line.find_first_not_of(' ');
        if (key_start != std::string_view::npos) {
            const std::string_view content = line.substr(key_start);
            if (key_start == 0u) {
                if (content == "---") {
                    // opening fence
                } else if (content.starts_with("description:")) {
                    if (description_seen || config_seen) return std::nullopt;
                    description_seen = true;
                    section = Section::Description;
                } else if (content.starts_with("title:")) {
                    if (title_seen || config_seen) return std::nullopt;
                    const auto value = trim_spaces(content.substr(6u));
                    if (value.size() < 2u || value.front() != '"' || value.back() != '"')
                        return std::nullopt;
                    const auto text = value.substr(1u, value.size() - 2u);
                    if (!is_prose(text)) return std::nullopt;
                    out.title = std::string{text};
                    title_seen = true;
                } else if (content == "config:") {
                    if (config_seen) return std::nullopt;
                    config_seen = true;
                    section = Section::Config;
                } else {
                    return std::nullopt;
                }
            } else if (section == Section::Description) {
                // folded payload
            } else if (section == Section::Config) {
                if (key_start != 2u || content != "packet:" || packet_seen)
                    return std::nullopt;
                packet_seen = true;
                section = Section::Packet;
            } else {
                if (key_start != 4u) return std::nullopt;
                const auto colon = content.find(':');
                if (colon == std::string_view::npos) return std::nullopt;
                const auto key = trim_spaces(content.substr(0, colon));
                const auto value = trim_spaces(content.substr(colon + 1u));
                if (key == "showBits") {
                    if (out.show_bits || (value != "true" && value != "false"))
                        return std::nullopt;
                    out.show_bits = std::string{value};
                } else if (key == "bitOrder") {
                    if (out.bit_order || (value != "ascending" && value != "descending"))
                        return std::nullopt;
                    out.bit_order = std::string{value};
                } else if (key == "bitsPerRow") {
                    if (out.bits_per_row || !is_uint(value)) return std::nullopt;
                    const auto count = std::stoul(std::string{value});
                    if (count == 0u || count > 64u) return std::nullopt;
                    out.bits_per_row = std::string{value};
                } else {
                    return std::nullopt;
                }
            }
        }
        if (last) break;
    }
    if (config_seen && !packet_seen) return std::nullopt;
    return out;
}

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
    // D-429: the frontmatter parser owns the closed description/title/config
    // shape; everything else returns std::nullopt and keeps the whole source.
    const auto frontmatter = packet_frontmatter(source);
    if (!frontmatter) return {};
    // An %%{init}%% directive or a classDef/:::/style statement still keeps the
    // whole source on fallback (no color/bit-cell contract).
    {
        bool visual_gate = false;
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
    if (frontmatter->title) {
        if (!push_guarded("title " + *frontmatter->title)) return {};
        // The honoured title leads the frame.
        std::rotate(out.begin(), out.end() - 1, out.end());
    }
    if (frontmatter->bit_order || frontmatter->bits_per_row || frontmatter->show_bits) {
        const auto push_notice = [&](const std::string& line) -> bool {
            if (line.size() > 96u || !work.spend(line.size() * 4u + sizeof(std::string) + 32u))
                return false;
            out.push_back(line);
            return true;
        };
        if (!push_guarded("")) return {};
        if (!push_notice("Packet native projection v1")) return {};
        if (frontmatter->bit_order &&
            !push_notice("config.packet.bitOrder=" + *frontmatter->bit_order +
                         " -> bit numbering direction; not applied"))
            return {};
        if (frontmatter->bits_per_row &&
            !push_notice("config.packet.bitsPerRow=" + *frontmatter->bits_per_row +
                         " -> ruler row grouping; not applied"))
            return {};
        if (frontmatter->show_bits &&
            !push_notice("config.packet.showBits=" + *frontmatter->show_bits +
                         " -> per-bit position rulers; not applied"))
            return {};
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
