module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;
import loom.serdes.json;

namespace loom::ui::mermaid::detail {
// Class-only token ownership. Dots remain literal Class identity; namespace
// components are split only by the namespace declaration below. Backtick IDs
// preserve physical LF rows without relaxing shared scalar validation.
std::optional<std::string> class_identifier(std::string_view& rest, Work& work, bool* backtick) {
    rest = trim_relation(rest);
    if (rest.empty()) return {};
    if (backtick) *backtick = false;
    if (!work.spend(rest.size() * (8 + sizeof(std::string_view) + sizeof(std::string)) + 1))
        return {};
    if (rest.front() == '`') {
        const auto end = rest.find('`', 1);
        if (end == std::string_view::npos || end == 1) return {};
        const auto id = rest.substr(1, end - 1);
        std::size_t cells = 0;
        for (const auto row : label_rows(id)) {
            if (row.empty() || !unicode_text(row)) return {};
            cells += ftxui::Utf8ToGlyphs(std::string(row)).size() + 1;
        }
        if (cells > kLabelLimit) return {};
        if (backtick) *backtick = true;
        std::string result(id);
        rest = trim_relation(rest.substr(end + 1));
        return result;
    }
    std::size_t n = 0;
    while (n < rest.size()) {
        const auto c = static_cast<unsigned char>(rest[n]);
        if ((c == '.' || c == '-') && n + 1 < rest.size() && rest[n + 1] == c)
            break;  // relationship, not Class identity
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '.' || c == '-' || c >= 0x80))
            break;
        ++n;
    }
    if (!n || !unicode_text(rest.substr(0, n))) return {};
    std::string result(rest.substr(0, n));
    rest = trim_relation(rest.substr(n));
    return result;
}

// A suffix is source-owned only when physically adjacent to an unquoted ID.
std::optional<ClassOperand> class_operand(std::string_view& rest, Work& work,
                                          std::string_view source, bool& source_checked) {
    if (!work.spend(4u * sizeof(ClassOperand) + 8u * sizeof(std::string_view) + 8u * sizeof(bool) +
                    64u))
        return {};
    const auto original = trim_relation(rest);
    bool backtick = false;
    auto id = class_identifier(rest, work, &backtick);
    if (!id) return {};
    ClassOperand result{std::move(*id), {}};
    // class_identifier trims its remainder. Inspect the original token boundary
    // rather than accidentally accepting a separated `A ~U~` operand.
    if (backtick || original.size() <= result.id.size() || original[result.id.size()] != '~')
        return result;
    if (!source_checked) {
        if (source.size() > kSourceLimit ||
            !work.spend(source.size() * 2u + 8u * sizeof(std::size_t) + 64u))
            return {};
        // The Class walker owns physical LF rows. A bare CR is an upstream
        // statement boundary, never whitespace joining an operand and edge.
        for (std::size_t index = 0; index < source.size(); ++index)
            if (source[index] == '\r' &&
                (index + 1u == source.size() || source[index + 1u] != '\n'))
                return {};
        source_checked = true;
    }
    if (rest.size() > kSourceLimit ||
        !work.spend(rest.size() * (16u + 4u * sizeof(std::string_view) + 4u * sizeof(std::string)) +
                    256u))
        return {};
    const auto end = rest.find('~', 1);
    if (end == std::string_view::npos || end == 1) return {};
    const auto payload = rest.substr(1, end - 1);
    if (payload.find_first_of("~<>[]{}\"'`\\\r\n") != std::string_view::npos) return {};
    const auto checked = class_display_label(payload, work, nullptr);
    if (!checked || *checked != payload || checked->find('\n') != std::string::npos) return {};
    result.type = payload;
    rest = trim_relation(rest.substr(end + 1));
    return result;
}

std::optional<std::vector<std::string_view>> class_statement_lines(std::string_view source,
                                                                   Work& work) {
    if (!work.spend(source.size() * (4 + 2 * sizeof(std::string_view)) + 1)) return {};
    std::vector<std::string_view> result;
    bool member_body = false;
    while (!source.empty()) {
        const auto physical = source.find('\n');
        const auto first = trim_relation(source.substr(0, physical));
        // Exclusive MEMBER owns quotes/backticks as literal physical-row data.
        if (first.starts_with("%%")) {
            if (physical == std::string_view::npos) break;
            source.remove_prefix(physical + 1);
            continue;
        }
        std::size_t end = 0;
        if (member_body) {
            end = physical == std::string_view::npos ? source.size() : physical;
        } else {
            if (!work.spend(8u * sizeof(bool) + 32u)) return {};
            const bool declaration = first.starts_with("class ");
            bool backtick = false, alias = false, label = false;
            const bool note = first.size() > 4u && first.starts_with("note") &&
                              (first[4] == ' ' || first[4] == '\t');
            bool note_string = false, note_closed = false;
            for (; end < source.size(); ++end) {
                const char c = source[end];
                if (note_string) {
                    if (c == '"') {
                        note_string = false;
                        note_closed = true;
                    }
                    continue;
                }
                if (alias) {
                    if (c == '"') alias = false;
                    continue;
                }
                if (backtick) {
                    if (c == '`') backtick = false;
                    continue;
                }
                if (c == '\n') break;
                if (label) continue;
                if (c == '`') {
                    backtick = true;
                    continue;
                }
                if (note && !note_closed && c == '"') {
                    note_string = true;
                    continue;
                }
                if (declaration && c == '[' && end + 1 < source.size() && source[end + 1] == '"') {
                    alias = true;
                    ++end;
                    continue;
                }
                if (c == ':') label = true;
            }
            if (backtick || alias || note_string) return {};
        }
        const auto line = trim_relation(source.substr(0, end));
        if (!line.empty()) result.push_back(line);
        if (member_body && line == "}")
            member_body = false;
        else if (!member_body && line.starts_with("class ") && line.ends_with("{"))
            member_body = true;
        if (end == source.size()) break;
        source.remove_prefix(end + 1);
    }
    return result;
}

// Class NOTE STR is literal source text, not LABEL/HTML/Markdown decoding.
// Only physical CRLF normalization is shared with pinned preprocessing.
std::optional<std::string> class_note_text(std::string_view raw, std::string_view original,
                                           bool& source_checked, Work& work) {
    if (raw.empty() || raw.size() > kSourceLimit ||
        !work.spend(raw.size() * (8u * sizeof(std::string) + 4u * sizeof(std::string_view) + 32u) +
                    512u))
        return {};
    if (!source_checked) {
        if (!work.spend(original.size() * 2u + 8u * sizeof(std::size_t) + 64u)) return {};
        for (std::size_t n = 0; n < original.size(); ++n)
            if (original[n] == '\r' && (n + 1u == original.size() || original[n + 1u] != '\n'))
                return {};
        source_checked = true;
    }
    std::string text;
    text.reserve(raw.size());
    for (std::size_t n = 0; n < raw.size(); ++n) {
        if (raw[n] == '\r') {
            if (n + 1u == raw.size() || raw[n + 1u] != '\n') return {};
            continue;
        }
        text.push_back(raw[n]);
    }
    if (text.size() > 256u) return {};
    const auto rows = label_rows(text);
    std::size_t cells = 4u;
    if (rows.size() > static_cast<std::size_t>(kHeightLimit - 3)) return {};
    for (const auto row : rows) {
        if (!unicode_text(row)) return {};
        for (const auto token :
             {std::string_view("\xe2\x80\x8b"), std::string_view("\xe2\x80\x8c"),
              std::string_view("\xe2\x80\x8d"), std::string_view("\xe2\x80\x8e"),
              std::string_view("\xe2\x80\x8f"), std::string_view("\xef\xbb\xbf")})
            if (row.find(token) != std::string_view::npos) return {};
        const auto glyphs = ftxui::Utf8ToGlyphs(std::string(row));
        std::string roundtrip;
        for (const auto& glyph : glyphs)
            roundtrip += glyph;
        if (roundtrip != row || cells > kLabelLimit || glyphs.size() > kLabelLimit - cells)
            return {};
        cells += glyphs.size();
    }
    return text;
}

// Class STR delimiters are removed by their declaration owner. LABEL/MEMBER
// outer quotes and Markdown punctuation remain data. No HTML/entity registry.
std::optional<std::string> class_display_label(std::string_view value, Work& work,
                                               const std::string_view* plain_angle_source) {
    if (value.size() > kSourceLimit ||
        !work.spend(
            value.size() * (32u + 2u * sizeof(std::string) + 2u * sizeof(std::string_view)) + 128u))
        return {};
    std::string decoded;
    decoded.reserve(value.size());
    bool physical_checked = false;
    for (std::size_t at = 0; at < value.size();) {
        const auto tail = value.substr(at);
        std::size_t used = 0;
        for (const auto token : {"<br>", "<br/>", "<br />"})
            if (tail.starts_with(token)) {
                used = std::string_view(token).size();
                break;
            }
        if (used) {
            decoded += '\n';
            at += used;
            continue;
        }
        const char c = value[at];
        if (c == '<' || c == '>') {
            // Only central LABEL opts into source-literal standalone angles.
            // Existing lowercase BR tokens were consumed above; other tags,
            // entities and text owners keep their closed profile.
            if (!plain_angle_source || !work.spend(8)) return {};
            if (!physical_checked) {
                // Raw CR is normalized by primary preprocessing before grammar.
                // Admit ordinary CRLF, but do not merge unnormalized bare-CR
                // statements after an operand trim. Decoded BR is not raw CR.
                if (plain_angle_source->size() > kSourceLimit ||
                    !work.spend(2u * plain_angle_source->size() + sizeof(bool) + 1u))
                    return {};
                for (std::size_t n = 0; n < plain_angle_source->size(); ++n)
                    if ((*plain_angle_source)[n] == '\r' && (n + 1 == plain_angle_source->size() ||
                                                             (*plain_angle_source)[n + 1] != '\n'))
                        return {};
                physical_checked = true;
            }
            const auto boundary = [](char byte) { return byte == ' ' || byte == '\t'; };
            if ((at && !boundary(value[at - 1])) ||
                (at + 1 < value.size() && !boundary(value[at + 1])))
                return {};
        }
        if (c == '\\' || c == '\r') return {};
        if (c == '&' || c == '#') {
            auto end = at + 1;
            while (end < value.size()) {
                const auto next = static_cast<unsigned char>(value[end]);
                if (!((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') ||
                      (next >= '0' && next <= '9') || next == '#'))
                    break;
                ++end;
            }
            if (end > at + 1 && end < value.size() && value[end] == ';') return {};
            decoded.append(value.substr(at, end - at));
            at = end;
            continue;
        }
        decoded += c;
        ++at;
    }
    const auto rows = label_rows(decoded);
    std::size_t cells = rows.empty() ? 0 : rows.size() - 1;
    bool visible = false;
    for (const auto row : rows) {
        if (row.empty()) continue;
        if (row.find_first_not_of(' ') == std::string_view::npos)
            cells += row.size();
        else {
            const auto checked = validated_plain_label(row);
            if (!checked) return {};
            cells += static_cast<std::size_t>(ftxui::string_width(*checked));
        }
        visible = true;
    }
    if (!visible || cells > kLabelLimit) return {};
    return decoded;
}

const ClassEndpointText* class_caption(const Relationship& edge, bool source) {
    if (!edge.endpoint_texts) return nullptr;
    const auto& caption = source ? edge.endpoint_texts->source : edge.endpoint_texts->target;
    return caption ? &*caption : nullptr;
}

bool class_endpoint_text(std::string_view& rest, Relationship& edge, bool source, Work& work) {
    // Preserve the existing zero-visible-token STR behavior without inventing
    // visible text or discharging foreign presence/summary predicates.
    if (rest.starts_with("\"\"")) {
        rest = trim_relation(rest.substr(2));
        return true;
    }
    if (rest.empty() || rest.front() != '"' || rest.size() > kSourceLimit ||
        !work.spend(rest.size() + 1))
        return false;
    const auto close = rest.find('"', 1);
    if (close == std::string_view::npos) return false;
    auto decoded = class_display_label(trim_relation(rest.substr(1, close - 1)), work);
    if (!decoded) return false;
    if (!work.spend(decoded->size() * (8u * sizeof(std::string_view) + 4u) +
                    3u * sizeof(ClassEndpointText) + 16u))
        return false;
    const int width = label_width(*decoded), height = static_cast<int>(label_rows(*decoded).size());
    if (width < 1 || width > kWidthLimit - 4 || height < 1 || height > kHeightLimit - 2)
        return false;
    if (!edge.endpoint_texts) {
        if (!work.spend(2u * sizeof(ClassEndpointTexts) + 16u)) return false;
        edge.endpoint_texts = std::make_unique<ClassEndpointTexts>();
    }
    auto& caption = source ? edge.endpoint_texts->source : edge.endpoint_texts->target;
    if (caption) return false;
    caption.emplace(ClassEndpointText{std::move(*decoded), width, height});
    rest = trim_relation(rest.substr(close + 1));
    return true;
}

namespace {
// Class owns its finite metadata schema. ER scalar/leaf/envelope grammar stays
// unchanged; this stack never retries an ER or Sequence parser.
struct ClassMeta {
    std::optional<bool> hide;
    std::optional<bool> root_html, class_html;
    std::optional<bool> hierarchical_namespaces;
};

bool class_meta_leaf(std::string_view owner, std::string_view key, const ErMetaValue& value,
                     unsigned& consumed, ClassMeta& meta, Work& work) {
    if (!work.spend(owner.size() + key.size() + sizeof(ClassMeta) * 4u + 32u)) return false;
    if (value.kind == ErMetaValue::Kind::Text && !er_config_text(value.text, false, work))
        return false;
    unsigned bit = 0;
    bool valid = false;
    const bool text = value.kind == ErMetaValue::Kind::Text,
               number = value.kind == ErMetaValue::Kind::Number;
    const bool boolean = value.kind == ErMetaValue::Kind::Boolean;
    const bool positive = number && std::isfinite(value.number) && value.number > 0;
    if (owner.empty()) {
        if (key == "logLevel") {
            bit = 1u;
            constexpr std::array<std::string_view, 6> names{
                {"trace", "debug", "info", "warn", "error", "fatal"}};
            valid = text ? std::ranges::find(names, value.text) != names.end()
                         : number && value.number >= 0 && value.number <= 5 &&
                               std::trunc(value.number) == value.number;
        } else if (key == "htmlLabels") {
            bit = 2u;
            valid = boolean;
        } else if (key == "fontSize") {
            bit = 4u;
            valid = positive;
        } else if (key == "theme") {
            bit = 8u;
            constexpr std::array<std::string_view, 12> themes{
                {"default", "base", "dark", "forest", "neutral", "neo", "neo-dark", "redux",
                 "redux-dark", "redux-color", "redux-dark-color", "null"}};
            valid = text && std::ranges::find(themes, value.text) != themes.end();
        } else if (key == "look") {
            bit = 16u;
            valid = text &&
                    (value.text == "classic" || value.text == "handDrawn" || value.text == "neo");
        } else if (key == "layout") {
            bit = 32u;
            valid = text && (value.text == "elk" || value.text == "dagre");
        }
    } else if (owner == "class") {
        if (key == "hideEmptyMembersBox") {
            bit = 64u;
            valid = boolean;
        } else if (key == "htmlLabels") {
            bit = 128u;
            valid = boolean;
        } else if (key == "hierarchicalNamespaces") {
            bit = 65536u;
            valid = boolean;
        }
    } else if (owner == "flowchart" && key == "htmlLabels") {
        bit = 256u;
        valid = boolean;
    } else if (owner == "themeVariables") {
        if (key == "fontSize") {
            bit = 512u;
            valid = positive;
            if (text && value.text.ends_with("px")) {
                const auto digits = value.text.substr(0, value.text.size() - 2);
                // Finite decimal pixels, not an arbitrary CSS expression.
                bool decimal = !digits.empty();
                bool dot = false;
                for (const unsigned char c : digits) {
                    if (c == '.' && !dot) {
                        dot = true;
                        continue;
                    }
                    if (c < '0' || c > '9') decimal = false;
                }
                double pixels = 0;
                const auto parsed =
                    std::from_chars(digits.data(), digits.data() + digits.size(), pixels);
                valid = decimal && parsed.ec == std::errc{} &&
                        parsed.ptr == digits.data() + digits.size() && std::isfinite(pixels) &&
                        pixels > 0;
            }
        } else {
            constexpr std::array<std::string_view, 6> colors{{"primaryColor", "primaryTextColor",
                                                              "primaryBorderColor", "lineColor",
                                                              "secondaryColor", "tertiaryColor"}};
            for (std::size_t n = 0; n < colors.size(); ++n)
                if (key == colors[n]) bit = 1024u << n;
            if (bit && text && (value.text.size() == 4 || value.text.size() == 7) &&
                value.text.front() == '#') {
                valid = true;
                for (const unsigned char c : value.text.substr(1))
                    valid &=
                        (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            }
        }
    }
    if (!bit || !valid || (consumed & bit)) return false;
    consumed |= bit;
    if (bit == 64u) meta.hide = value.boolean;
    if (bit == 65536u) meta.hierarchical_namespaces = value.boolean;
    if (owner.empty() && key == "htmlLabels") meta.root_html = value.boolean;
    if (owner == "class" && key == "htmlLabels") meta.class_html = value.boolean;
    return true;
}

bool class_meta_namespace(std::string_view name) {
    return name == "class" || name == "flowchart" || name == "themeVariables";
}

bool class_json_meta(loom::utils::json::JsonVal object, ClassMeta& meta, Work& work,
                     std::string_view owner = {}) {
    if (!work.spend(8u * sizeof(void*) + 8u * sizeof(unsigned) + 64u) || !object.is_obj() ||
        object.size() == 0 || object.size() > 16)
        return false;
    unsigned consumed = 0, namespaces = 0;
    bool valid = true;
    object.iter_obj([&](loom::utils::json::JsonVal name, loom::utils::json::JsonVal value) {
        if (!valid) return;
        const auto key = name.as_str();
        if (!work.spend(key.size() * 4u + 16u)) {
            valid = false;
            return;
        }
        if (owner.empty() && class_meta_namespace(key)) {
            const unsigned bit = key == "class" ? 1u : key == "flowchart" ? 2u : 4u;
            if (namespaces & bit) {
                valid = false;
                return;
            }
            namespaces |= bit;
            valid = class_json_meta(value, meta, work, key);
            return;
        }
        ErMetaValue decoded;
        if (value.is_str())
            decoded = {ErMetaValue::Kind::Text, value.as_str()};
        else if (value.is_num())
            decoded = {ErMetaValue::Kind::Number, {}, value.as_double()};
        else if (value.is_bool())
            decoded = {ErMetaValue::Kind::Boolean, {}, 0, value.as_bool()};
        valid = class_meta_leaf(owner, key, decoded, consumed, meta, work);
    });
    return valid;
}

bool class_init_meta(std::string_view& source, ClassMeta& meta, Work& work) {
    if (!work.spend(source.size() * 4u + 128u) || !source.starts_with("%%{")) return false;
    std::size_t close = std::string_view::npos;
    char quote = 0;
    bool escaped = false;
    for (std::size_t at = 3; at + 2 < source.size(); ++at) {
        const char c = source[at];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (quote && c == '\\') {
            escaped = true;
            continue;
        }
        if (quote) {
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (source.substr(at, 3) == "}%%") {
            close = at;
            break;
        }
    }
    if (close == std::string_view::npos) return false;
    auto argument = source.substr(3, close - 3);
    const auto first = argument.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return false;
    argument.remove_prefix(first);
    const auto colon = argument.find(':');
    if (colon == std::string_view::npos || trim_relation(argument.substr(0, colon)) != "init")
        return false;
    argument.remove_prefix(colon + 1);
    // Wrapper/tail scanning, normalization/copy and JSON parsing have separate
    // prepaid bounds. No decoded JsonVal escapes this local JsonDoc.
    if (!work.spend(argument.size() * 12u + 64u)) return false;
    std::string normalized;
    normalized.reserve(argument.size() * 2u);
    for (std::size_t at = 0; at < argument.size();) {
        const char q = argument[at];
        if (q != '\'' && q != '"') {
            normalized += q;
            ++at;
            continue;
        }
        const auto begin = at++;
        bool escape = false;
        for (; at < argument.size(); ++at) {
            const char c = argument[at];
            if (q == '\'' && c == '\\') return false;
            if (escape) {
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            if (c == q) break;
            if (static_cast<unsigned char>(c) < 0x20) return false;
        }
        if (at == argument.size()) return false;
        if (q == '"')
            normalized.append(argument.substr(begin, at - begin + 1));
        else {
            const auto text = argument.substr(begin + 1, at - begin - 1);
            if (!unicode_text(text)) return false;
            normalized += '"';
            for (const char c : text) {
                if (c == '"') normalized += '\\';
                normalized += c;
            }
            normalized += '"';
        }
        ++at;
    }
    if (!work.spend(normalized.size() * 8u + 64u)) return false;
    const auto document = loom::utils::json::parse(normalized);
    if (!document || !class_json_meta(document->root(), meta, work)) return false;
    auto tail = source.substr(close + 3);
    const auto newline = tail.find('\n');
    auto suffix = tail.substr(0, newline);
    if (suffix.ends_with('\r')) suffix.remove_suffix(1);
    if (!trim_relation(suffix).empty()) return false;
    source = newline == std::string_view::npos ? std::string_view{} : tail.substr(newline + 1);
    return true;
}

// Description is opaque frontmatter documentation in the pinned Class owner.
// It never contributes a class/member/title or a visible diagram caption.
bool class_description_scalar(std::string_view value, Work& work) {
    if (!work.spend(value.size() * 6u + 64u)) return false;
    if (value.find("\xef\xbb\xbf") != std::string_view::npos ||
        value.find("\xe2\x80\x8b") != std::string_view::npos)
        return false;
    while (true) {
        const auto tab = value.find('\t');
        if (!unicode_text(value.substr(0, tab))) return false;
        if (tab == std::string_view::npos) return true;
        value.remove_prefix(tab + 1);
    }
}
}  // namespace

std::optional<ClassEnvelope> class_source_envelope(std::string_view source, Work& work) {
    if (source.size() > kSourceLimit ||
        !work.spend(8u * sizeof(ClassEnvelope) + 8u * sizeof(ClassMeta) + 128u))
        return {};
    ClassEnvelope envelope;
    ClassMeta meta;
    std::string_view body = source;
    if (source.starts_with("---")) {
        const auto parsed = detail::parse_envelope(source, work);
        if (!parsed || !parsed->has_frontmatter) return {};
        bool config_seen = false, title_seen = false, description_seen = false;
        std::map<std::string, unsigned> owner_seen;
        std::vector<std::string> namespaces;
        for (const auto& event : parsed->events) {
            if (event.blank || event.comment) continue;
            if (event.top_level) {
                if (event.path == "title") {
                    if (title_seen) return {};
                    title_seen = true;
                } else if (event.path == "description") {
                    if (description_seen) return {};
                    description_seen = true;
                } else if (event.path == "config") {
                    if (config_seen) return {};
                    config_seen = true;
                } else
                    return {};
                continue;
            }
            if (event.namespace_entry) {
                if (event.path.find('.') != std::string_view::npos ||
                    !class_meta_namespace(event.path))
                    return {};
                if (std::ranges::find(namespaces, event.path) != namespaces.end()) return {};
                namespaces.push_back(event.path);
                continue;
            }
            const auto dot = event.path.rfind('.');
            const std::string_view owner = dot == std::string_view::npos
                                               ? std::string_view{}
                                               : std::string_view(event.path).substr(0, dot);
            const std::string_view key = dot == std::string_view::npos
                                             ? std::string_view(event.path)
                                             : std::string_view(event.path).substr(dot + 1u);
            if (!owner.empty() && !class_meta_namespace(owner)) return {};
            const auto decoded = er_yaml_value(er_strip_comment(event.value), work);
            auto& consumed = owner_seen[std::string(owner)];
            if (!class_meta_leaf(owner, key, decoded, consumed, meta, work)) return {};
        }
        if (config_seen && parsed->config.empty() && namespaces.empty()) return {};
        for (const auto& name : namespaces) {
            bool child = false;
            for (const auto& entry : parsed->config) {
                if (entry.path.size() > name.size() && entry.path.starts_with(name) &&
                    entry.path[name.size()] == '.') {
                    child = true;
                    break;
                }
            }
            if (!child) return {};
        }
        if (parsed->title) {
            const auto decoded = er_yaml_value(er_strip_comment(parsed->title->header), work);
            if (decoded.kind != ErMetaValue::Kind::Text || decoded.text.empty() ||
                decoded.text.size() > kLabelLimit || !work.spend(decoded.text.size() * 4u + 1u))
                return {};
            bool ink = false;
            for (const unsigned char c : decoded.text) {
                if (c < 0x20 || c > 0x7e) return {};
                ink |= c != ' ';
            }
            if (!ink) return {};
            envelope.title = std::string(decoded.text);
        }
        if (parsed->description) {
            const auto& description = *parsed->description;
            const auto description_header = er_strip_comment(description.header);
            if (!(description.form == EnvelopeScalarForm::Folded &&
                  (description_header == ">" || description_header == ">-")))
                return {};
            std::optional<std::size_t> indentation;
            std::size_t leading_blank_indent = 0;
            std::size_t at = 0;
            while (at < description.text.size()) {
                const auto newline = description.text.find('\n', at);
                const auto line = description.text.substr(
                    at, newline == std::string_view::npos ? std::string_view::npos : newline - at);
                at = newline == std::string_view::npos ? description.text.size() : newline + 1u;
                const auto indent = line.find_first_not_of(' ');
                if (indent == std::string_view::npos) {
                    leading_blank_indent = std::max(leading_blank_indent, line.size());
                    continue;
                }
                if (line[indent] == '\t') return {};
                if (!indentation) {
                    if (indent < leading_blank_indent) return {};
                    indentation = indent;
                }
                if (indent < *indentation ||
                    !class_description_scalar(line.substr(*indentation), work))
                    return {};
            }
        }
        body = parsed->raw_body;
    } else if (!source.starts_with("%%{")) {
        return {};
    }
    auto skip = [&]() {
        while (!body.empty()) {
            const auto end = body.find('\n');
            auto raw = body.substr(0, end);
            if (!work.spend(raw.size() + 1u)) return false;
            if (raw.ends_with('\r')) raw.remove_suffix(1);
            const auto line = trim_relation(raw);
            if (!line.empty() && (!line.starts_with("%%") || line.starts_with("%%{"))) return true;
            body = end == std::string_view::npos ? std::string_view{} : body.substr(end + 1);
        }
        return true;
    };
    if (!skip()) return {};
    if (body.starts_with("%%{")) {
        if (!class_init_meta(body, meta, work) || !skip()) return {};
    }
    const auto end = body.find('\n');
    const auto header = trim_relation(body.substr(0, end));
    if (!work.spend(header.size() + sizeof(ClassMeta) * 4u + 32u) ||
        (header != "classDiagram" && header != "classDiagram-v2"))
        return {};
    const bool effective_html = meta.root_html.value_or(meta.class_html.value_or(false));
    static_cast<void>(effective_html);
    envelope.body = body;
    envelope.hide_empty_members_box = meta.hide;
    envelope.hierarchical_namespaces = meta.hierarchical_namespaces;
    return envelope;
}

// Decode only the complete annotation delimiter owned by Class grammar.
// Other entities, HTML and double encoding keep their existing source boundary.
std::optional<std::string> class_encoded_annotation(std::string_view text, Work& work) {
    if (text.size() < 16u || text.size() - 16u + 4u > kLabelLimit ||
        !text.starts_with("&lt;&lt;") || !text.ends_with("&gt;&gt;"))
        return {};
    const auto token = text.substr(8u, text.size() - 16u);
    if (!work.spend(text.size() * (8u + 4u * sizeof(std::string)) +
                    token.size() * (token.size() + 1u) + 128u) ||
        !unicode_text(token))
        return {};
    const auto label = relation_label(token);
    if (!label || label->find('\n') != std::string::npos) return {};
    return "<<" + std::string(token) + ">>";
}

namespace {

// ClassTypes field whitespace is ECMAScript whitespace. The enclosing member
// operation prepays every byte pass before calling these private view helpers.
std::string_view class_member_trim(std::string_view value) {
    while (const auto size = er_metadata_space(value))
        value.remove_prefix(size);
    std::size_t last = 0;
    for (std::size_t at = 0; at < value.size();) {
        if (const auto size = er_metadata_space(value.substr(at)))
            at += size;
        else
            last = ++at;
    }
    return value.substr(0, last);
}

std::size_t class_member_tildes(std::string_view value) {
    return static_cast<std::size_t>(std::ranges::count(value, '~'));
}

std::optional<std::string> class_member_generic_field(std::string_view value, bool& changed) {
    std::string out;
    out.reserve(value.size());
    auto append_set = [&](std::string_view set) {
        std::size_t count = class_member_tildes(set);
        if (count <= 1u) {
            out.append(set);
            return;
        }
        if (count % 2u && set.starts_with('~')) {
            out += '~';
            set.remove_prefix(1u);
            --count;
        }
        const auto half = count / 2u;
        std::size_t ordinal = 0;
        for (const char byte : set) {
            if (byte != '~') {
                out += byte;
                continue;
            }
            ++ordinal;
            if (ordinal <= half) {
                out += '<';
                changed = true;
            } else if (ordinal > count - half) {
                out += '>';
                changed = true;
            } else
                out += '~';
        }
    };
    for (std::size_t at = 0;;) {
        const auto comma = value.find(',', at);
        const auto end = comma == std::string_view::npos ? value.size() : comma;
        auto next_end = end;
        if (comma != std::string_view::npos) {
            const auto next = value.find(',', comma + 1u);
            next_end = next == std::string_view::npos ? value.size() : next;
            if (class_member_tildes(value.substr(at, end - at)) == 1u &&
                class_member_tildes(value.substr(comma + 1u, next_end - comma - 1u)) == 1u) {
                if (next != std::string_view::npos) {
                    const auto third = value.find(',', next + 1u),
                               third_end = third == std::string_view::npos ? value.size() : third;
                    // The fixed common output.pop would discard an already
                    // combined business prefix. Keep this finite profile closed.
                    if (class_member_tildes(value.substr(next + 1u, third_end - next - 1u)) == 1u)
                        return {};
                }
                append_set(value.substr(at, next_end - at));
                if (next_end == value.size()) break;
                out += ',';
                at = next_end + 1u;
                continue;
            }
        }
        append_set(value.substr(at, end - at));
        if (comma == std::string_view::npos) break;
        out += ',';
        at = comma + 1u;
    }
    return out;
}
}  // namespace

std::optional<Member> class_member_display(std::string_view raw, Work& work) {
    if (raw.size() > kSourceLimit) return {};
    const auto open = raw.find('('), close = raw.rfind(')');
    const bool method = open != std::string_view::npos;
    if ((!method && close != std::string_view::npos) ||
        (method && (close == std::string_view::npos || open > close)))
        return {};
    auto literal = class_display_label(raw, work);
    if (!literal || literal->find('\n') != std::string::npos) return {};
    const auto n = literal->size();
    if (!work.spend(n + 32u)) return {};
    const bool typed_suffix =
        !literal->empty() && (literal->back() == '$' || literal->back() == '*');
    const auto decoded_close = literal->rfind(')');
    const bool typed_slot =
        method && decoded_close != std::string::npos && decoded_close + 1u < literal->size() &&
        ((*literal)[decoded_close + 1u] == '$' || (*literal)[decoded_close + 1u] == '*');
    if (class_member_tildes(*literal) <= 1u && !typed_suffix && !typed_slot)
        return Member{std::move(*literal), method};
    // All field/segment passes and retained strings are paid before scanning or
    // allocating. Final plain validation has its own glyph/storage tariff below.
    if (!work.spend(64u * n + 8u * (n + 4u) + 8u * sizeof(Member) + 12u * sizeof(std::string_view) +
                    8u * sizeof(std::size_t) + 6u * sizeof(std::string) + 512u))
        return {};
    const auto value = class_member_trim(*literal);
    std::string_view id = value, parameters, returned;
    char visibility = 0, classifier = 0;
    const auto visible = [](char byte) {
        return byte == '#' || byte == '+' || byte == '~' || byte == '-';
    };
    if (method) {
        const auto last = value.rfind(')');
        if (last == std::string_view::npos) return {};
        const auto first = value.rfind('(', last);
        if (first == std::string_view::npos || first == 0u) return {};
        if (visible(value.front()) && first > 1u) {
            visibility = value.front();
            id = value.substr(1u, first - 1u);
        } else
            id = value.substr(0, first);
        parameters = class_member_trim(value.substr(first + 1u, last - first - 1u));
        auto tail = value.substr(last + 1u);
        if (!tail.empty()) {
            if (const auto size = er_metadata_space(tail))
                tail.remove_prefix(size);
            else if (tail.front() == '$' || tail.front() == '*') {
                classifier = tail.front();
                tail.remove_prefix(1u);
            }
        }
        returned = class_member_trim(tail);
        if (!classifier && !returned.empty() &&
            (returned.back() == '$' || returned.back() == '*')) {
            classifier = returned.back();
            returned.remove_suffix(1u);
        }
    } else {
        if (!value.empty() && visible(value.front())) {
            visibility = value.front();
            id.remove_prefix(1u);
        }
        if (!id.empty() && (id.back() == '$' || id.back() == '*')) {
            classifier = id.back();
            id.remove_suffix(1u);
        }
    }
    if (classifier && class_member_trim(id).empty()) return {};
    const bool leading_space = !id.empty() && id.front() == ' ';
    id = class_member_trim(id);
    std::string normalized_id;
    normalized_id.reserve(id.size() + 1u);
    if (leading_space) normalized_id += ' ';
    normalized_id.append(id);
    bool changed = false;
    auto shown_id = class_member_generic_field(normalized_id, changed);
    if (!shown_id) return {};
    auto shown_parameters = class_member_generic_field(parameters, changed);
    if (!shown_parameters) return {};
    auto shown_return = class_member_generic_field(returned, changed);
    if (!shown_return) return {};
    if (!changed) return Member{std::move(*literal), method};
    std::string shown;
    shown.reserve(n + 4u);
    if (visibility) shown += visibility;
    shown += *shown_id;
    if (method) {
        shown += '(';
        shown += *shown_parameters;
        shown += ')';
        if (!returned.empty()) {
            shown += " : ";
            shown += *shown_return;
        }
    }
    const auto display = class_member_trim(shown);
    if (display.size() != shown.size()) shown = std::string(display);
    if (classifier) shown += classifier;
    // Utf8ToGlyphs reserves one string slot per UTF8 byte. This pays storage,
    // construction/moves, roundtrip/copy buffers and scans before plain checking.
    if (!work.spend(shown.size() * (4u * sizeof(std::string) + 32u) + 256u)) return {};
    auto safe = validated_plain_label(shown);
    if (!safe) return {};
    return Member{std::move(*safe), method};
}

namespace {
// This finite Class profile retains declarations only when the pinned
// defineClass operation cannot affect any implicit-default class in this source.
// Accepted classes have exactly the implicit CSS class string "default";
// explicit CSS associations remain closed in their owning parser branches.
}  // namespace

bool class_inactive_definition(std::string_view line, std::string_view original,
                               std::vector<std::string_view>& definitions, bool& source_checked,
                               Work& work) {
    if (!work.spend(line.size() * 12u + 16u * sizeof(std::string_view) + 8u * sizeof(std::size_t) +
                    128u))
        return false;
    if (!source_checked) {
        if (original.size() > kSourceLimit ||
            !work.spend(original.size() * 2u + 8u * sizeof(std::size_t) + 64u))
            return false;
        for (std::size_t index = 0; index < original.size(); ++index)
            if (original[index] == '\r' &&
                (index + 1u == original.size() || original[index + 1u] != '\n'))
                return false;
        source_checked = true;
    }
    auto rest = trim_relation(line.substr(8u));
    const auto split = rest.find_first_of(" \t");
    if (split == std::string_view::npos || split == 0u || split > 64u) return false;
    const auto name = rest.substr(0, split);
    for (std::size_t n = 0; n < name.size(); ++n) {
        const auto c = static_cast<unsigned char>(name[n]);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
              (n > 0u && (c >= '0' && c <= '9'))))
            return false;
    }
    // INITIAL classList requires ALPHA. Fixed lexer words (including call's
    // state transition and the single-letter aggregation token) are not IDs.
    if (!work.spend(4u * 17u * sizeof(std::string_view) + 128u)) return false;
    constexpr std::array<std::string_view, 17> reserved{
        {"classDiagram", "style", "classDef", "namespace", "class", "cssClass", "callback", "link",
         "click", "note", "href", "call", "_self", "_blank", "_parent", "_top", "o"}};
    for (const auto keyword : reserved) {
        if (!work.spend(name.size() + keyword.size() + 16u) || name == keyword) return false;
    }
    rest = trim_relation(rest.substr(split));
    if (rest.empty() || definitions.size() >= 64u) return false;
    for (const auto previous : definitions) {
        if (!work.spend(previous.size() + name.size() + 16u) || previous == name) return false;
    }
    unsigned seen = 0;
    while (!rest.empty()) {
        const auto comma = rest.find(',');
        const auto item = trim_relation(rest.substr(0, comma));
        const auto colon = item.find(':');
        if (colon == std::string_view::npos) return false;
        const auto key = trim_relation(item.substr(0, colon));
        const auto value = trim_relation(item.substr(colon + 1u));
        const unsigned bit = key == "fill"           ? 1u
                             : key == "stroke"       ? 2u
                             : key == "color"        ? 4u
                             : key == "stroke-width" ? 8u
                                                     : 0u;
        if (!bit || (seen & bit) || value.empty() || (bit == 8u && seen == 0u)) return false;
        seen |= bit;
        if (bit == 8u) {
            if (!value.ends_with("px")) return false;
            const auto digits = value.substr(0, value.size() - 2u);
            if (digits.empty() || digits.size() > 4u || digits.front() == '0') return false;
            unsigned pixels = 0;
            for (const char c : digits) {
                if (c < '0' || c > '9') return false;
                pixels = pixels * 10u + static_cast<unsigned>(c - '0');
            }
            if (pixels == 0u || pixels > 1024u) return false;
        } else {
            if ((value.size() != 4u && value.size() != 7u) || value.front() != '#') return false;
            for (const char c : value.substr(1u))
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                    return false;
        }
        if (comma == std::string_view::npos) break;
        rest = trim_relation(rest.substr(comma + 1u));
        if (rest.empty()) return false;
    }
    // Conservative whole-source profile: a default-matching ID is closed even
    // before the first class. Do not infer future stylesheet behavior merely
    // from the absence of current vertices at the declaration statement.
    if (!work.spend(name.size() + 32u) ||
        std::string_view("default").find(name) != std::string_view::npos)
        return false;
    if (definitions.empty()) {
        // Empty reserve: allocation only; later writes fit this finite bound.
        if (!work.spend(64u * sizeof(std::string_view) + alignof(std::string_view) + 64u))
            return false;
        definitions.reserve(64u);
    }
    if (!work.spend(sizeof(std::string_view) * 2u + 16u)) return false;
    definitions.push_back(name);
    return true;
}

// Source-only two-face decision. No layout, label, cardinality or route is copied
// from output, and no future endpoint is reserved in the occupancy canvas.
bool reciprocal_plan(const RelationDiagram& diagram, const RelationLayout& layout, int owner,
                     Work& work, std::optional<ReciprocalPlan>& result) {
    // Only constant-time facts participate in the zero-new-scan exit.
    const bool class_profile =
        diagram.family == RelationFamily::Class && layout.class_reciprocal && owner < 0;
    if (!class_profile && (diagram.family != RelationFamily::Er || diagram.direction != "TB" ||
                           owner >= 0 || diagram.edges.size() < 8))
        return true;
    const auto vertices = diagram.entities.size(), edges = diagram.edges.size();
    if (vertices > 64 || edges > 128 || !diagram.notes.empty() || layout.ranked_peers) return true;
    if (!class_profile)
        for (std::size_t n = 0; n < vertices; ++n) {
            if (!work.spend()) return false;
            const auto& entity = diagram.entities[n];
            if (entity.owner != -1 || entity.shape != EntityShape::Box || entity.er_alias ||
                !entity.er_attributes.empty() || !entity.members.empty() ||
                !entity.annotation.empty() || layout.degrees[n] == 0)
                return true;
        }
    if (layout.ordered[0].size() != vertices || layout.scope_edges[0].size() != edges) return true;

    struct Pair {
        int lo = -1, hi = -1;
        std::array<int, 2> edge{-1, -1};
        int count = 0, color = -1;
    };

    // The bound64 is validated before storage, including inapplicable inputs
    // with more than64 distinct endpoint pairs. Every initialization is prepaid.
    if (!work.spend(64 * 10 + edges * 3 + 16)) return false;
    std::array<Pair, 64> pairs{};
    std::array<int, 64> positions{}, queue{}, incident{};
    positions.fill(-1);
    std::size_t pair_count = 0;
    int previous_bottom = -1;
    for (std::size_t n = 0; n < vertices; ++n) {
        const auto order = class_profile && diagram.direction == "BT" ? vertices - 1 - n : n;
        if (!work.spend()) return false;
        const int id = layout.ordered[0][order];
        if (id < 0 || static_cast<std::size_t>(id) >= vertices ||
            positions[static_cast<std::size_t>(id)] != -1)
            return false;
        if (class_profile) {
            if (!work.spend(4)) return false;
            const auto& box = layout.boxes[static_cast<std::size_t>(id)];
            if (box.y < 0 || box.bottom() >= layout.canvas_height || box.y <= previous_bottom)
                return false;
            previous_bottom = box.bottom();
        }
        positions[static_cast<std::size_t>(id)] = static_cast<int>(n);
    }
    for (const int index : layout.scope_edges[0]) {
        if (!work.spend()) return false;
        const auto& edge = diagram.edges[static_cast<std::size_t>(index)];
        if (edge.from < 0 || edge.to < 0 || static_cast<std::size_t>(edge.from) >= vertices ||
            static_cast<std::size_t>(edge.to) >= vertices)
            return false;
        if (edge.from == edge.to) return true;
        const int a = positions[static_cast<std::size_t>(edge.from)],
                  b = positions[static_cast<std::size_t>(edge.to)];
        const int lo = std::min(a, b), hi = std::max(a, b);
        std::size_t pair = 0;
        for (; pair < pair_count; ++pair) {
            if (!work.spend()) return false;
            if (pairs[pair].lo == lo && pairs[pair].hi == hi) break;
        }
        if (pair == pair_count) {
            if (pair_count == pairs.size()) return true;
            if (!work.spend(6)) return false;
            pairs[pair_count++] = {lo, hi, {-1, -1}, 0, -1};
        }
        auto& entry = pairs[pair];
        if (entry.count == 2) return true;
        if (entry.count == 1) {
            const auto& prior = diagram.edges[static_cast<std::size_t>(entry.edge[0])];
            if (prior.from != edge.to || prior.to != edge.from) return true;
        }
        if (!work.spend()) return false;
        entry.edge[static_cast<std::size_t>(entry.count++)] = index;
    }
    for (std::size_t n = 0; n < pair_count; ++n) {
        if (!work.spend()) return false;
        if (pairs[n].count != 2) return true;
    }
    auto conflict = [](const Pair& a, const Pair& b) {
        return (a.lo < b.lo && b.lo < a.hi && a.hi < b.hi) ||
               (b.lo < a.lo && a.lo < b.hi && b.hi < a.hi);
    };
    bool interleaves = false;
    for (std::size_t seed = 0; seed < pair_count; ++seed) {
        if (!work.spend()) return false;
        if (pairs[seed].color != -1) continue;
        if (!work.spend(2)) return false;
        std::size_t begin = 0, end = 1;
        queue[0] = static_cast<int>(seed);
        pairs[seed].color = 0;
        while (begin < end) {
            if (!work.spend()) return false;
            const int at = queue[begin++];
            for (std::size_t n = 0; n < pair_count; ++n) {
                if (!work.spend()) return false;
                if (!conflict(pairs[static_cast<std::size_t>(at)], pairs[n])) continue;
                interleaves = true;
                const int color = 1 - pairs[static_cast<std::size_t>(at)].color;
                if (pairs[n].color == -1) {
                    if (end >= queue.size() || !work.spend(2)) return false;
                    pairs[n].color = color;
                    queue[end++] = static_cast<int>(n);
                } else if (pairs[n].color != color)
                    return true;
            }
        }
    }
    if (!interleaves && !class_profile) return true;
    // The first three fields were prepaid before eligibility; pay the new
    // fourth field only for an active plan, before allocation/initialization.
    if (!work.spend(edges)) return false;
    ReciprocalPlan plan;
    plan.edges.resize(edges);
    int minimum = layout.canvas_width, maximum = -1, marker = 0;
    for (std::size_t n = 0; n < vertices; ++n) {
        if (!work.spend()) return false;
        minimum = std::min(minimum, layout.boxes[n].x);
        maximum = std::max(maximum, layout.boxes[n].right());
    }
    for (const auto& edge : diagram.edges) {
        if (!work.spend()) return false;
        marker = std::max({marker, marker_size(edge.start), marker_size(edge.end)});
    }
    int offset = marker + 1;
    if (layout.class_band_rows)
        for (const auto& edge : diagram.edges) {
            if (!work.spend(4)) return false;
            for (const bool source : {true, false})
                if (const auto* caption = class_caption(edge, source))
                    offset = std::max(offset,
                                      marker_size(source ? edge.start : edge.end) + caption->w + 4);
        }
    plan.left_spine = minimum - offset;
    plan.right_spine = maximum + offset;
    if (plan.left_spine <= 0 || plan.right_spine >= layout.canvas_width - 1) return false;
    for (std::size_t vertex = 0; vertex < vertices; ++vertex)
        for (const int face : {-1, 1}) {
            if (class_profile && !work.spend(2)) return false;
            // The qualified Class measure reserves D blank rows immediately above
            // bottom, after all H header rows; ER keeps its existing y+1 protocol.
            const int capacity =
                layout.class_band_rows ? (*layout.class_band_rows)[vertex] : layout.degrees[vertex];
            const int first_row = class_profile ? layout.boxes[vertex].bottom() - capacity
                                                : layout.boxes[vertex].y + 1;
            int band_cursor = 0;
            std::size_t used = 0;
            const int position = positions[vertex];
            for (std::size_t n = 0; n < pair_count; ++n) {
                if (!work.spend()) return false;
                const auto& pair = pairs[n];
                if ((pair.color == 0 ? -1 : 1) != face ||
                    (pair.lo != position && pair.hi != position))
                    continue;
                if (!work.spend()) return false;
                incident[used++] = static_cast<int>(n);
            }
            auto before = [&](int a, int b) {
                const auto& first = pairs[static_cast<std::size_t>(a)];
                const auto& second = pairs[static_cast<std::size_t>(b)];
                const int other_a = first.lo == position ? first.hi : first.lo,
                          other_b = second.lo == position ? second.hi : second.lo;
                // Above: nearest first. Below: farthest first. Pair source order
                // resolves ties; keys are independent of any current sort state.
                return std::tuple{other_a < position ? 0 : 1, -other_a, a} <
                       std::tuple{other_b < position ? 0 : 1, -other_b, b};
            };
            for (std::size_t n = 1; n < used; ++n) {
                if (!work.spend()) return false;
                const int value = incident[n];
                std::size_t at = n;
                while (at > 0) {
                    if (!work.spend()) return false;
                    if (!before(value, incident[at - 1])) break;
                    if (!work.spend()) return false;
                    incident[at] = incident[at - 1];
                    --at;
                }
                if (!work.spend()) return false;
                incident[at] = value;
            }
            for (std::size_t block = 0; block < used; ++block) {
                const auto& pair = pairs[static_cast<std::size_t>(incident[block])];
                const auto& box = layout.boxes[vertex];
                for (int local = 0; local < 2; ++local) {
                    const int rail =
                        layout.class_band_rows ? (position == pair.hi ? local : 1 - local) : local;
                    int row = first_row + static_cast<int>(block * 2) +
                              (position == pair.hi ? rail : 1 - rail);
                    if (layout.class_band_rows) {
                        if (!work.spend(8)) return false;
                        const auto& edge = diagram.edges[static_cast<std::size_t>(
                            pair.edge[static_cast<std::size_t>(rail)])];
                        const auto* caption =
                            class_caption(edge, edge.from == static_cast<int>(vertex));
                        const int band = caption ? caption->h + 2 : 1;
                        if (band > capacity - band_cursor) return false;
                        row = first_row + band_cursor + (caption ? 1 : 0);
                        band_cursor += band;
                    }
                    if (!work.spend(3)) return false;
                    if (row <= box.y || row >= box.bottom()) return false;
                    const int edge_index = pair.edge[static_cast<std::size_t>(rail)];
                    auto& port = plan.edges[static_cast<std::size_t>(edge_index)];
                    port.face = face;
                    if (diagram.edges[static_cast<std::size_t>(edge_index)].from ==
                        static_cast<int>(vertex))
                        port.from_row = row;
                    else
                        port.to_row = row;
                }
            }
        }
    // Count actual same-page port intervals strictly enclosing this arc.
    // These source slots are fixed before routing; no canvas occupancy probe.
    for (std::size_t n = 0; n < edges; ++n) {
        if (!work.spend()) return false;
        auto& arc = plan.edges[n];
        const int lo = std::min(arc.from_row, arc.to_row), hi = std::max(arc.from_row, arc.to_row);
        for (const auto& outer : plan.edges) {
            if (!work.spend()) return false;
            if (arc.face == outer.face && std::min(outer.from_row, outer.to_row) < lo &&
                hi < std::max(outer.from_row, outer.to_row)) {
                if (!work.spend()) return false;
                ++arc.inset;
            }
        }
    }
    for (std::size_t n = 0; n < edges; ++n) {
        if (!work.spend(diagram.edges[n].label.size() * 2 + 2)) return false;
        const auto& port = plan.edges[n];
        const int room =
            port.face < 0 ? plan.left_spine : layout.canvas_width - 1 - plan.right_spine;
        if (!port.face || !port.from_row || !port.to_row || port.inset >= room ||
            port.from_row < port.inset || port.from_row >= layout.canvas_height - port.inset ||
            port.to_row < port.inset || port.to_row >= layout.canvas_height - port.inset ||
            (!diagram.edges[n].label.empty() &&
             label_width(diagram.edges[n].label) + 4 > room - port.inset))
            return false;
    }
    result = std::move(plan);
    return true;
}

// Source-owned endpoint rectangles interrupt the actual selected route. They
// never replace or concatenate the optional central business label.
bool connect_class_caption_pair(Canvas& canvas, RouteSearch& search, const Relationship& edge,
                                const EndpointPort& p, const EndpointPort& q,
                                const ReciprocalEdgePlan* planned, int spine) {
    const auto* source = class_caption(edge, true);
    const auto* target = class_caption(edge, false);
    if ((!source && !target) || (source && !p.normal.x) || (target && !q.normal.x) ||
        search.compound_reservations)
        return false;

    struct Caption {
        const ClassEndpointText* text = nullptr;
        LabelPlacement box{};
        Point near{}, far{};
    };

    if (!canvas.work.spend(2u * sizeof(Caption) + 2u * sizeof(std::array<Point, 2>) + 64u))
        return false;
    auto endpoint = [&](const ClassEndpointText* text, const EndpointPort& port) -> Caption {
        if (!text) return {};
        const Point near = port.route + port.normal,
                    far{port.route.x + port.normal.x * (text->w + 2), port.route.y};
        const int left = std::min(near.x, far.x);
        return {text,
                {{left, near.y}, {left + text->w + 1, near.y}, left + 1, near.y, text->w, text->h},
                near,
                far};
    };
    const Caption from = endpoint(source, p), to = endpoint(target, q);
    auto overlap = [](const LabelPlacement& a, const LabelPlacement& b) {
        return a.x - 2 <= b.x + b.w + 1 && b.x - 2 <= a.x + a.w + 1 && a.y - 1 <= b.y + b.h &&
               b.y - 1 <= a.y + a.h;
    };
    auto contains = [](const LabelPlacement& box, Point point) {
        return point.x >= box.x - 2 && point.x <= box.x + box.w + 1 && point.y >= box.y - 1 &&
               point.y <= box.y + box.h;
    };
    if (source && target && overlap(from.box, to.box)) return false;
    auto physical_free = [&](Point point) {
        if (!canvas.work.spend(search.layout.diagram.entities.size() + 2)) return false;
        return search.free(point);
    };
    // Check complete physical endpoint clearance before any page clipping.
    for (const auto* caption : {&from, &to})
        if (caption->text) {
            const auto& box = caption->box;
            if (box.w < 1 || box.w > canvas.width - 4 || box.h < 1 || box.h > canvas.height - 2 ||
                box.x < 2 || box.x + box.w + 1 >= canvas.width || box.y < 1 ||
                box.y + box.h >= canvas.height)
                return false;
            for (int y = box.y - 1; y <= box.y + box.h; ++y)
                for (int x = box.x - 2; x <= box.x + box.w + 1; ++x) {
                    const Point point{x, y};
                    if (!physical_free(point) ||
                        (search.compound_route && search.compound_route->structural(point)))
                        return false;
                }
        }
    // A bare opposite endpoint's marker is also a real role, even though it
    // has no caption rectangle. No text clearance may consume that marker.
    for (const auto& port : {p, q})
        for (int n = 1; n <= marker_size(port.marker); ++n) {
            if (!canvas.work.spend(3)) return false;
            const Point point{port.border.x + port.normal.x * n, port.border.y + port.normal.y * n};
            if ((source && contains(from.box, point)) || (target && contains(to.box, point)) ||
                !physical_free(point))
                return false;
        }
    const Point source_outer = source ? from.far : p.route,
                target_outer = target ? to.far : q.route;
    const Point start = planned ? Point{spine, p.route.y} : source_outer,
                goal = planned ? Point{spine, q.route.y} : target_outer;
    if (start == goal) return false;
    auto connector = [&](Point first, Point last,
                         const Caption* foreign) -> std::optional<std::vector<Point>> {
        if (first.y != last.y || !canvas.inside(first) || !canvas.inside(last)) return {};
        const auto count = static_cast<std::size_t>(std::abs(last.x - first.x)) + 1;
        if (count > static_cast<std::size_t>(canvas.width) ||
            !canvas.work.spend(count * (sizeof(Point) + 4u) + sizeof(std::vector<Point>)))
            return {};
        std::vector<Point> points;
        points.reserve(count);
        const int direction = last.x >= first.x ? 1 : -1;
        for (std::size_t n = 0; n < count; ++n) {
            if (!canvas.work.spend()) return {};
            const Point point{first.x + static_cast<int>(n) * direction, first.y};
            if ((foreign->text && contains(foreign->box, point)) || !physical_free(point) ||
                (n && !search.step(points.back(), point)))
                return {};
            points.push_back(point);
        }
        return points;
    };
    // A fixed outward continuation cannot double back through its own text.
    if (planned && ((source && (start.x - source_outer.x) * p.normal.x < 1) ||
                    (target && (goal.x - target_outer.x) * q.normal.x < 1)))
        return false;
    const auto prefix = connector(source_outer, start, &to),
               suffix = connector(goal, target_outer, &from);
    if (!prefix || !suffix) return false;
    const std::array<Point, 2> source_near{p.route, source ? from.near : p.route},
        target_near{target ? to.near : q.route, q.route};
    if (source)
        for (const auto point : source_near)
            if ((target && contains(to.box, point)) || !physical_free(point)) return false;
    if (target)
        for (const auto point : target_near)
            if ((source && contains(from.box, point)) || !physical_free(point)) return false;

    struct Reservations {
        RouteSearch& search;
        const std::vector<Point>* before;
        std::vector<Point> cells;
        std::vector<unsigned char> previous;

        ~Reservations() {
            release(0);
            search.compound_reservations = before;
        }

        void release(std::size_t keep) {
            while (cells.size() > keep) {
                const Point point = cells.back();
                search.canvas.at(point).blocked = previous.back() != 0;
                cells.pop_back();
                previous.pop_back();
            }
        }

        bool add(Point point) {
            auto& canvas = search.canvas;
            if (!canvas.inside(point)) return false;
            for (const auto own : cells) {
                if (!canvas.work.spend()) return false;
                if (own == point) return true;
            }
            if (!search.free(point) ||
                !canvas.work.spend(sizeof(Point) + sizeof(unsigned char) + 8u))
                return false;
            if (cells.size() == cells.capacity()) {
                if (cells.size() >= canvas.cells.size()) return false;
                const auto capacity =
                    std::min(canvas.cells.size(), std::max(std::size_t{1}, cells.size() * 2));
                if (!canvas.work.spend(
                        (capacity + cells.size()) * (sizeof(Point) + sizeof(unsigned char)) + 4u))
                    return false;
                cells.reserve(capacity);
                previous.reserve(capacity);
            }
            previous.push_back(static_cast<unsigned char>(canvas.at(point).blocked));
            cells.push_back(point);
            canvas.at(point).blocked = true;
            return true;
        }
    };

    if (!canvas.work.spend(sizeof(Reservations))) return false;
    Reservations reservations{search, search.compound_reservations, {}, {}};
    search.compound_reservations = &reservations.cells;
    auto reserve = [&](Point point) {
        return point == start || point == goal || reservations.add(point);
    };
    for (const auto& port : {p, q})
        for (int n = 1; n <= marker_size(port.marker); ++n) {
            if (!canvas.work.spend() ||
                !reserve({port.border.x + port.normal.x * n, port.border.y + port.normal.y * n}))
                return false;
        }
    for (const auto* caption : {&from, &to})
        if (caption->text) {
            const auto& box = caption->box;
            for (int y = box.y - 1; y <= box.y + box.h; ++y)
                for (int x = box.x - 1; x <= box.x + box.w; ++x) {
                    const Point point{x, y};
                    if (!canvas.work.spend()) return false;
                    if (point == caption->near || point == caption->far) continue;
                    if (point == start || point == goal || !reserve(point)) return false;
                }
        }
    if (source)
        for (const auto point : source_near)
            if (!reserve(point)) return false;
    if (target)
        for (const auto point : target_near)
            if (!reserve(point)) return false;
    for (const auto points : {std::span<const Point>(*prefix), std::span<const Point>(*suffix)})
        for (const auto point : points)
            if (!reserve(point)) return false;

    struct PageScope {
        RouteSearch& search;
        int face, spine, inset;
        Point start, goal;

        ~PageScope() {
            search.reciprocal_face = face;
            search.reciprocal_spine = spine;
            search.reciprocal_inset = inset;
            search.reciprocal_start = start;
            search.reciprocal_goal = goal;
        }
    };

    if (!canvas.work.spend(sizeof(PageScope))) return false;
    PageScope page{search,
                   search.reciprocal_face,
                   search.reciprocal_spine,
                   search.reciprocal_inset,
                   search.reciprocal_start,
                   search.reciprocal_goal};
    if (planned) {
        search.reciprocal_face = planned->face;
        search.reciprocal_spine = spine;
        search.reciprocal_inset = planned->inset;
        search.reciprocal_start = start;
        search.reciprocal_goal = goal;
    }
    auto joined = [&](std::span<const Point> before, std::span<const Point> middle,
                      std::span<const Point> after) -> std::optional<std::vector<Point>> {
        if (middle.empty() || (!before.empty() && before.back() != middle.front()) ||
            (!after.empty() && after.front() != middle.back()))
            return {};
        const auto limit = canvas.cells.size();
        if (before.size() > limit || middle.size() > limit || after.size() > limit ||
            before.size() > std::numeric_limits<std::size_t>::max() - middle.size() ||
            before.size() + middle.size() > std::numeric_limits<std::size_t>::max() - after.size())
            return {};
        const auto count = before.size() + middle.size() + after.size() -
                           (before.empty() ? 0u : 1u) - (after.empty() ? 0u : 1u);
        if (count > canvas.cells.size() ||
            !canvas.work.spend(count * (sizeof(Point) + 2u) + sizeof(std::vector<Point>)))
            return {};
        std::vector<Point> result;
        result.reserve(count);
        for (const auto point : before) {
            if (!canvas.work.spend()) return {};
            result.push_back(point);
        }
        for (std::size_t n = before.empty() ? 0 : 1; n < middle.size(); ++n) {
            if (!canvas.work.spend()) return {};
            result.push_back(middle[n]);
        }
        for (std::size_t n = after.empty() ? 0 : 1; n < after.size(); ++n) {
            if (!canvas.work.spend()) return {};
            result.push_back(after[n]);
        }
        return result;
    };
    const Point before = source ? source_outer - p.normal : p.route - p.normal,
                after = target ? target_outer - q.normal : q.route - q.normal;
    auto paint_roles = [&](const LabelPlacement* central) -> bool {
        for (const auto* caption : {source, target})
            if (caption &&
                !canvas.work.spend(caption->text.size() * (4u * sizeof(std::string_view) +
                                                           4u * sizeof(std::string) + 4u) +
                                   16u))
                return false;
        if (central && !canvas.work.spend(edge.label.size() * (4u * sizeof(std::string_view) +
                                                               4u * sizeof(std::string) + 4u) +
                                          16u))
            return false;
        if (source && (!paint_path(canvas, source_near, p.route - p.normal, from.near + p.normal,
                                   edge.dotted, search.compound_route) ||
                       !canvas.multiline({from.box.x, from.box.y}, source->text, source->w)))
            return false;
        if (target && (!paint_path(canvas, target_near, to.near + q.normal, q.route - q.normal,
                                   edge.dotted, search.compound_route) ||
                       !canvas.multiline({to.box.x, to.box.y}, target->text, target->w)))
            return false;
        if (central && !canvas.multiline({central->x, central->y}, edge.label, central->w))
            return false;
        if (!paint_port(canvas, p, false) || !paint_port(canvas, q, false)) return false;
        for (const auto* box : {source ? &from.box : nullptr, target ? &to.box : nullptr, central})
            if (box) {
                const auto width = static_cast<std::size_t>(box->w + 2),
                           height = static_cast<std::size_t>(box->h + 2);
                if (width > static_cast<std::size_t>(canvas.width) ||
                    height > static_cast<std::size_t>(canvas.height) ||
                    !canvas.work.spend(width * height))
                    return false;
                for (int y = box->y - 1; y <= box->y + box->h; ++y)
                    for (int x = box->x - 1; x <= box->x + box->w; ++x)
                        canvas.at({x, y}).blocked = true;
            }
        return true;
    };
    if (edge.label.empty()) {
        const auto path = search.path(start, goal);
        if (!path) return false;
        const auto complete = joined(*prefix, *path, *suffix);
        if (!complete) return false;
        // No temporary destructor may clear newly painted persistent cells.
        reservations.release(0);
        if (!paint_path(canvas, *complete, before, after, edge.dotted, search.compound_route))
            return false;
        return paint_roles(nullptr);
    }
    if (!search.path(start, goal)) return false;
    LabelCursor cursor;
    const std::array<Point, 2> body_anchors{start, goal};
    while (const auto label =
               label_place(canvas, search, edge, p, q, {}, -1, cursor, &body_anchors)) {
        if ((source && overlap(label.value(), from.box)) ||
            (target && overlap(label.value(), to.box)))
            continue;
        const auto base = reservations.cells.size();

        struct CandidateScope {
            Reservations& reservations;
            std::size_t base;

            ~CandidateScope() { reservations.release(base); }
        };

        if (!canvas.work.spend(sizeof(CandidateScope))) return false;
        CandidateScope candidate_scope{reservations, base};
        bool allowed = true;
        for (int y = label->y - 1; allowed && y <= label->y + label->h; ++y)
            for (int x = label->x - 1; x <= label->x + label->w; ++x) {
                const Point point{x, y};
                if (!canvas.work.spend()) return false;
                if (point == label->left || point == label->right) continue;
                if (point == start || point == goal) {
                    allowed = false;
                    break;
                }
                if (!reserve(point)) return false;
            }
        if (!allowed) continue;
        if (!canvas.work.spend(8)) return false;
        const auto distance = [](Point a, Point b) {
            return std::abs(a.x - b.x) + std::abs(a.y - b.y);
        };
        bool reverse = goal.y < start.y;
        const int forward = distance(start, label->left) + distance(label->right, goal),
                  backward = distance(start, label->right) + distance(label->left, goal);
        if (forward != backward) reverse = backward < forward;
        const Point entrance = reverse ? label->right : label->left,
                    exit = reverse ? label->left : label->right,
                    across = reverse ? Point{-1, 0} : Point{1, 0};
        // The actual body goal must use the authoritative dynamic reservation
        // list as well as blocked, including namespace structural exemptions.
        const auto target_base = reservations.cells.size();
        if (goal != entrance && !reservations.add(goal)) return false;
        const auto left = search.path(start, entrance);
        reservations.release(target_base);
        if (!left) continue;
        for (const auto point : *left)
            if (!reservations.add(point)) return false;
        const auto right = search.path(exit, goal);
        if (!right) continue;
        const auto first = joined(*prefix, *left, {}), last = joined({}, *right, *suffix);
        if (!first || !last) return false;
        // paint_path needs the actual one-cell direction across the central gap;
        // keep placement contacts untouched and supply that direction explicitly.
        reservations.release(0);
        if (!paint_path(canvas, *first, before, entrance + across, edge.dotted,
                        search.compound_route) ||
            !paint_path(canvas, *last, exit - across, after, edge.dotted, search.compound_route))
            return false;
        return paint_roles(&*label);
    }
    return false;
}

// Fixed source ports and real normal connectors reuse the same A*/label/paint
// primitives. The default all-pair connector above is deliberately unchanged.
bool connect_reciprocal_relation(Canvas& canvas, RouteSearch& search,
                                 const RelationDiagram& diagram, const RelationLayout& layout,
                                 const Relationship& edge, const ReciprocalPlan& plan,
                                 std::size_t edge_index) {
    if (edge_index >= plan.edges.size()) return false;
    const auto& route = plan.edges[edge_index];
    const auto& a = layout.boxes[static_cast<std::size_t>(edge.from)];
    const auto& b = layout.boxes[static_cast<std::size_t>(edge.to)];
    const std::size_t candidates = 2 * static_cast<std::size_t>(a.w + a.h + b.w + b.h - 8);
    // Prepay both original physical port scans and their vector growth/copies.
    // EndpointPort has three Points and one marker. Per-candidate scope checks
    // visit at most V child boxes and at most three marker/normal positions.
    if (!canvas.work.spend(candidates * (64 + 8 * diagram.entities.size()) + 32)) return false;
    const auto from =
        ports_for(diagram.entities[static_cast<std::size_t>(edge.from)], a, edge.start, search);
    const auto to =
        ports_for(diagram.entities[static_cast<std::size_t>(edge.to)], b, edge.end, search);
    auto selected = [&](const std::vector<EndpointPort>& ports, int row,
                        std::optional<EndpointPort>& result) {
        for (const auto& port : ports) {
            if (!canvas.work.spend()) return false;
            if (port.normal.x != route.face || port.normal.y || port.border.y != row) continue;
            if (result || !canvas.work.spend(7)) return false;
            result = port;
        }
        return result.has_value();
    };
    std::optional<EndpointPort> from_port, to_port;
    if (!selected(from, route.from_row, from_port) || !selected(to, route.to_row, to_port))
        return false;
    const auto p = *from_port, q = *to_port;
    const int spine = route.face < 0 ? plan.left_spine : plan.right_spine;
    if (edge.endpoint_texts)
        return connect_class_caption_pair(canvas, search, edge, p, q, &route, spine);
    const Point start{spine, p.route.y}, goal{spine, q.route.y};
    if (start == goal) return false;
    auto connector = [&](Point first, Point last) -> std::optional<std::vector<Point>> {
        if (first.y != last.y || !canvas.inside(first) || !canvas.inside(last)) return {};
        const std::size_t count = static_cast<std::size_t>(std::abs(last.x - first.x)) + 1;
        if (count > static_cast<std::size_t>(canvas.width) ||
            !canvas.work.spend(count * (6 + diagram.entities.size()) + 2))
            return {};
        std::vector<Point> points;
        points.reserve(count);
        const int step = last.x >= first.x ? 1 : -1;
        for (std::size_t n = 0; n < count; ++n) {
            const Point point{first.x + static_cast<int>(n) * step, first.y};
            if (!canvas.work.spend() || !search.free(point)) return {};
            points.push_back(point);
        }
        return points;
    };
    // All original physical checks happen before the active page clip, including
    // genuine normal route points between the entity and common spine.
    const auto prefix = connector(p.route, start);
    if (!prefix) return false;
    const auto suffix = connector(goal, q.route);
    if (!suffix) return false;

    struct OwnReservations {
        Canvas& canvas;
        std::vector<Point> cells;

        ~OwnReservations() { release(0); }

        void release(std::size_t keep) {
            while (cells.size() > keep) {
                const auto point = cells.back();
                cells.pop_back();
                canvas.at(point).blocked = false;
            }
        }

        bool add(Point point) {
            if (!canvas.inside(point) || canvas.at(point).blocked || !canvas.work.spend(8))
                return false;
            if (cells.size() == cells.capacity()) {
                if (cells.size() == canvas.cells.size()) return false;
                const auto capacity =
                    std::min(canvas.cells.size(), std::max(std::size_t{1}, cells.size() * 2));
                if (!canvas.work.spend(2 * capacity + 2 * cells.size())) return false;
                cells.reserve(capacity);
            }
            cells.push_back(point);
            canvas.at(point).blocked = true;
            return true;
        }
    } reservations{canvas, {}};

    auto reserve = [&](Point point) {
        return point == start || point == goal || reservations.add(point);
    };
    for (const auto& port : {p, q})
        for (int n = 1; n <= marker_size(port.marker); ++n)
            if (!reserve({port.border.x + port.normal.x * n, port.border.y + port.normal.y * n}))
                return false;
    for (const auto& points : {std::cref(*prefix), std::cref(*suffix)})
        for (const auto point : points.get())
            if (!reserve(point)) return false;

    struct PageScope {
        RouteSearch& search;
        int old_face, old_spine, old_inset;
        Point old_start, old_goal;

        ~PageScope() {
            search.reciprocal_face = old_face;
            search.reciprocal_spine = old_spine;
            search.reciprocal_inset = old_inset;
            search.reciprocal_start = old_start;
            search.reciprocal_goal = old_goal;
        }
    } page{search,
           search.reciprocal_face,
           search.reciprocal_spine,
           search.reciprocal_inset,
           search.reciprocal_start,
           search.reciprocal_goal};

    search.reciprocal_face = route.face;
    search.reciprocal_spine = spine;
    search.reciprocal_inset = route.inset;
    search.reciprocal_start = start;
    search.reciprocal_goal = goal;
    auto joined = [&](std::span<const Point> before, const std::vector<Point>& middle,
                      std::span<const Point> after) -> std::optional<std::vector<Point>> {
        if (middle.empty() || (!before.empty() && before.back() != middle.front()) ||
            (!after.empty() && after.front() != middle.back()))
            return {};
        const auto count = before.size() + middle.size() + after.size() - (before.empty() ? 0 : 1) -
                           (after.empty() ? 0 : 1);
        if (count > canvas.cells.size() || !canvas.work.spend(count * 4 + 4)) return {};
        std::vector<Point> result;
        result.reserve(count);
        for (const auto point : before) {
            if (!canvas.work.spend()) return {};
            result.push_back(point);
        }
        for (std::size_t n = before.empty() ? 0 : 1; n < middle.size(); ++n) {
            if (!canvas.work.spend()) return {};
            result.push_back(middle[n]);
        }
        for (std::size_t n = after.empty() ? 0 : 1; n < after.size(); ++n) {
            if (!canvas.work.spend()) return {};
            result.push_back(after[n]);
        }
        return result;
    };
    if (edge.label.empty()) {
        const auto body = search.path(start, goal);
        if (!body) return false;
        const auto complete = joined(*prefix, *body, *suffix);
        if (!complete) return false;
        reservations.release(0);
        return paint_path(canvas, *complete, p.route - p.normal, q.route - q.normal, edge.dotted) &&
               paint_port(canvas, p, diagram.family == RelationFamily::Er) &&
               paint_port(canvas, q, diagram.family == RelationFamily::Er);
    }
    if (!search.path(start, goal)) return false;
    LabelCursor cursor;
    while (const auto label = label_place(canvas, search, edge, p, q, {}, -1, cursor)) {
        const auto base = reservations.cells.size();

        struct LabelReservations {
            OwnReservations& own;
            std::size_t base;

            ~LabelReservations() { own.release(base); }
        } label_scope{reservations, base};

        for (int y = label->y - 1; y <= label->y + label->h; ++y)
            for (int x = label->x - 1; x <= label->x + label->w; ++x) {
                const Point point{x, y};
                if (point == label->left || point == label->right) continue;
                if (!canvas.work.spend() || !reserve(point)) return false;
            }
        bool reverse = goal.y < start.y;
        if (!canvas.work.spend(8)) return false;
        const auto distance = [](Point first, Point last) {
            return std::abs(first.x - last.x) + std::abs(first.y - last.y);
        };
        const int forward_cost = distance(start, label->left) + distance(label->right, goal),
                  reverse_cost = distance(start, label->right) + distance(label->left, goal);
        if (forward_cost != reverse_cost) reverse = reverse_cost < forward_cost;
        const Point entrance = reverse ? label->right : label->left,
                    exit = reverse ? label->left : label->right,
                    across = reverse ? Point{-1, 0} : Point{1, 0};

        struct GoalReservation {
            Canvas& canvas;
            Point point;
            bool active = false;

            ~GoalReservation() {
                if (active) canvas.at(point).blocked = false;
            }
        } target{canvas, goal};

        // Protect the actual search goal, never the clipped-out normal point.
        if (!canvas.work.spend(2) || !search.free(goal)) return false;
        canvas.at(goal).blocked = true;
        target.active = true;
        auto left = search.path(start, entrance);
        canvas.at(goal).blocked = false;
        target.active = false;
        if (!left) continue;
        // These are real first-half cells, temporarily owned to prevent reuse.
        for (const auto point : *left)
            if (!canvas.work.spend() || !reservations.add(point)) return false;
        auto right = search.path(exit, goal);
        if (!right) continue;
        const auto first = joined(*prefix, *left, {}), last = joined({}, *right, *suffix);
        if (!first || !last) return false;
        // No temporary guard may undo newly painted path/label cells on success.
        reservations.release(0);
        if (!paint_path(canvas, *first, p.route - p.normal, entrance + across, edge.dotted) ||
            !paint_path(canvas, *last, exit - across, q.route - q.normal, edge.dotted) ||
            !canvas.multiline({label->x, label->y}, edge.label, label->w) ||
            !paint_port(canvas, p, diagram.family == RelationFamily::Er) ||
            !paint_port(canvas, q, diagram.family == RelationFamily::Er))
            return false;
        const auto width = static_cast<std::size_t>(label->w + 2),
                   height = static_cast<std::size_t>(label->h + 2);
        if (width > static_cast<std::size_t>(canvas.width) ||
            height > static_cast<std::size_t>(canvas.height) || !canvas.work.spend(width * height))
            return false;
        for (int y = label->y - 1; y <= label->y + label->h; ++y)
            for (int x = label->x - 1; x <= label->x + label->w; ++x)
                canvas.at({x, y}).blocked = true;
        return true;
    }
    return false;
}
}  // namespace loom::ui::mermaid::detail
