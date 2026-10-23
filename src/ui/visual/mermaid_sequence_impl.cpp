module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import loom.serdes.json;

namespace loom::ui::mermaid::detail {
namespace {
std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return value;
}

bool id_start(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_';
}

bool id_char(char value) { return id_start(value) || (value >= '0' && value <= '9'); }

std::optional<std::string> sequence_display_row(std::string_view input, Work& work) {
    std::string out;
    bool base = false;
    for (std::size_t at = 0; at < input.size();) {
        const auto start = at;
        const auto lead = static_cast<unsigned char>(input[at++]);
        std::uint32_t scalar = lead;
        int trailing = 0;
        if (lead >= 0xc2 && lead <= 0xdf) { scalar = lead & 0x1f; trailing = 1; }
        else if (lead >= 0xe0 && lead <= 0xef) { scalar = lead & 0x0f; trailing = 2; }
        else if (lead >= 0xf0 && lead <= 0xf4) { scalar = lead & 0x07; trailing = 3; }
        else if (lead >= 0x80) return {};
        if (at + static_cast<std::size_t>(trailing) > input.size()) return {};
        for (int n = 0; n < trailing; ++n) {
            const auto next = static_cast<unsigned char>(input[at++]);
            if ((next & 0xc0) != 0x80) return {};
            scalar = (scalar << 6) | (next & 0x3f);
        }
        if ((trailing == 1 && scalar < 0x80) || (trailing == 2 && scalar < 0x800) ||
            (trailing == 3 && scalar < 0x10000) || scalar > 0x10ffff ||
            (scalar >= 0xd800 && scalar <= 0xdfff)) return {};
        const auto unit = std::string(input.substr(start, at - start));
        if (!work.spend(unit.size() * 4 + 16)) return {};
        const bool unsafe = scalar < 0x20 || (scalar >= 0x7f && scalar <= 0x9f) ||
            scalar == 0xfeff || scalar == 0x200b || scalar == 0x061c ||
            scalar == 0x200e || scalar == 0x200f || scalar == 0x2028 || scalar == 0x2029 ||
            (scalar >= 0x202a && scalar <= 0x202e) || (scalar >= 0x2066 && scalar <= 0x2069);
        std::string isolated;
        if (!base && !unsafe) for (const auto& glyph : ftxui::Utf8ToGlyphs(unit)) isolated += glyph;
        if (unsafe || (!base && (isolated != unit || ftxui::string_width(unit) == 0))) {
            out += std::format(R"(\u{{{:04X}}})", scalar);
            base = false;
        } else if (scalar == '\\') {
            out += R"(\\)"; base = true;
        } else {
            out += unit;
            if (ftxui::string_width(unit) > 0) base = true;
        }
    }
    if (!work.spend(out.size() * 5)) return {};
    // Blank decoded rows retain their exact padding. The complete label must
    // still contain text and obey the aggregate display limit.
    if (out.find_first_not_of(' ') == std::string::npos) return out;
    return validated_plain_label(out);
}

// Protect a complete primary-style ASCII entity atom before statement syntax.
// Its spelling remains raw until a supported text field decodes it once.
std::optional<std::size_t> sequence_atom_size(std::string_view input, Work& work) {
    if (input.empty() || input.front() != '#') return 0;
    if (!work.spend()) return {};
    std::size_t end = 1;
    while (end < input.size() && id_char(input[end])) {
        if (!work.spend()) return {};
        ++end;
    }
    if (end < input.size() && !work.spend()) return {};
    return end > 1 && end < input.size() && input[end] == ';' ? end + 1 : 0;
}

std::optional<std::string> sequence_text_row(std::string_view input, Work& work) {
    if (input.find('#') == std::string_view::npos) return sequence_display_row(input, work);
    // Decoding never expands the raw byte length. Charge the copy/scan before
    // allocating, then keep the same Work for atom scans and Safe display.
    if (!work.spend(input.size())) return {};
    std::string decoded;
    decoded.reserve(input.size());
    for (std::size_t at = 0; at < input.size();) {
        if (input[at] != '#') { decoded += input[at++]; continue; }
        const auto length = sequence_atom_size(input.substr(at), work);
        if (!length || *length == 0) return {};
        const auto atom = input.substr(at + 1, *length - 2);
        if (atom == "35") decoded += '#';
        else if (atom == "59") decoded += ';';
        else if (atom == "38") decoded += '&';
        else if (atom == "9829") decoded += "♥";
        else if (atom == "lt") decoded += '<';
        else if (atom == "gt") decoded += '>';
        else if (atom == "infin") decoded += "∞";
        else return {};
        at += *length;
    }
    // Decoded angle/hash/semicolon data cannot re-enter BR or source syntax.
    return sequence_display_row(decoded, work);
}

enum class LabelSyntax { Plain, Text, Metadata, BoxCaption, Identity };
std::optional<std::string_view> source_nowrap_text(std::string_view input,Work& work) {
    input=trim(input);
    if (input.starts_with("wrap:")||input.starts_with(":wrap:")) return {};
    const auto count=input.starts_with(":nowrap:")?8u:input.starts_with("nowrap:")?7u:0u;
    if (count&&!work.spend(count)) return {};
    return trim(input.substr(count));
}
// Complete raw BR ownership precedes entity decoding. This local predicate is
// shared by the two Text/Metadata label paths; other label roles stay closed.
std::optional<std::size_t> sequence_raw_break_size(std::string_view input, Work& work) {
    if (input.empty() || input.front() != '<') return 0;
    if (!work.spend(4)) return {};
    if (input.size() < 3 || (input[1] != 'b' && input[1] != 'B') ||
        (input[2] != 'r' && input[2] != 'R')) return 0;
    std::size_t at = 3;
    while (at < input.size()) {
        if (!work.spend()) return {};
        const auto byte = static_cast<unsigned char>(input[at]);
        if (byte != 0x20 && (byte < 0x09 || byte > 0x0d)) break;
        ++at;
    }
    if (!work.spend(2)) return {};
    if (at < input.size() && input[at] == '/') ++at;
    if (at == input.size() || input[at] != '>') return 0;
    return at + 1;
}

std::optional<std::string> sequence_label(std::string_view input, Work& work,
                                        LabelSyntax syntax = LabelSyntax::Plain) {
    input = trim(input);
    const bool text_syntax=syntax==LabelSyntax::Text||syntax==LabelSyntax::Metadata;
    // AS/TXT quotes are literal data. Only raw parseMessage fields consume a
    // false wrap override; a metadata-selected alias is already source data.
    if (syntax!=LabelSyntax::Metadata) {
        const auto payload=source_nowrap_text(input,work);
        if (!payload) return {};
        input=*payload;
    }
    if ((syntax!=LabelSyntax::BoxCaption&&input.find('&')!=std::string_view::npos) ||
        (!text_syntax && input.find('#') != std::string_view::npos) ||
        input.find(static_cast<char>(96)) != std::string_view::npos) return {};
    if (syntax==LabelSyntax::BoxCaption&&input.find('&')!=std::string_view::npos) {
        if (!work.spend(input.size())) return {};
        // Bare caption punctuation is literal; no HTML entity decoder is added.
        for (auto at=input.find('&');at!=std::string_view::npos;at=input.find('&',at+1)) {
            const auto end=input.find_first_of("; \t&",at+1);
            if (end!=std::string_view::npos&&input[end]==';') return {};
        }
    }
    std::string decoded;
    std::size_t glyphs = 0, cells = 0;
    bool have_text = false;
    for (;;) {
        auto end = input.find('<');
        std::size_t raw_break = 0;
        if (text_syntax) while (end != std::string_view::npos) {
            const auto size = sequence_raw_break_size(input.substr(end), work);
            if (!size) return {};
            if (*size != 0) { raw_break = *size; break; }
            // Keep all non-BR bytes in one row; splitting at an arbitrary '<'
            // could turn an attached combining scalar into a false new base.
            do {
                if (!work.spend()) return {};
                ++end;
            } while (end < input.size() && input[end] != '<');
            if (end == input.size()) end = std::string_view::npos;
        }
        const auto raw_row = input.substr(0, end);
        const auto row = text_syntax ? sequence_text_row(raw_row, work) : sequence_display_row(raw_row, work);
        if (!row) return {};
        have_text |= row->find_first_not_of(' ') != std::string::npos;
        glyphs += ftxui::Utf8ToGlyphs(*row).size();
        cells += static_cast<std::size_t>(ftxui::string_width(*row));
        decoded += *row;
        if (glyphs > kLabelLimit || cells > kLabelLimit) return {};
        if (end == std::string_view::npos) break;
        input.remove_prefix(end);
        if (text_syntax) {
            input.remove_prefix(raw_break); decoded += '\n'; ++glyphs;
            continue;
        }
        if (!input.starts_with("<br>") && !input.starts_with("<br/>") && !input.starts_with("<br />")) return {};
        const auto close = input.find('>');
        input.remove_prefix(close + 1); decoded += '\n'; ++glyphs;
    }
    return have_text ? std::optional<std::string>{std::move(decoded)} : std::nullopt;
}

bool sequence_id(std::string_view id, bool message_endpoint = false) {
    // sequenceDiagram.jison's lexer is case-insensitive. Conservatively keep
    // its declaration/control words out of both explicit and implicit
    // participant IDs. Four metadata words are guarded at INITIAL grammar
    // positions instead of being global identity restrictions.
    // The pinned Go terminal grammar additionally permits
    // activation words as unambiguous complete message endpoints.
    // Label text is checked separately and remains unaffected.
    // https://github.com/mermaid-js/mermaid/blob/develop/packages/mermaid/src/diagrams/sequence/parser/sequenceDiagram.jison
    constexpr std::string_view reserved[]{"sequencediagram", "participant", "actor", "as",
        "create", "destroy", "box", "loop", "rect", "opt", "alt", "else", "par",
        "par_over", "and", "critical", "option", "break", "end", "over", "note",
        "activate", "deactivate", "title", "acctitle", "accdescr", "autonumber",
        "off"};
    std::string folded;
    folded.reserve(id.size());
    for (const auto ch : id) folded += ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
    if (message_endpoint && (folded == "activate" || folded == "deactivate")) return true;
    return std::ranges::find(reserved, folded) == std::end(reserved);
}

enum class Arrow { Filled, Unheaded, Async, Cross, Both,
    FilledHalfTop, FilledHalfBottom, OpenHalfTop, OpenHalfBottom,
    FilledHalfTopReverse, FilledHalfBottomReverse, OpenHalfTopReverse, OpenHalfBottomReverse };
struct Operator { std::string_view spelling; Arrow kind; bool dashed; };
constexpr Operator kOperators[]{
    {"--|\\", Arrow::FilledHalfTop, true},
    {"--|/", Arrow::FilledHalfBottom, true},
    {"--\\\\", Arrow::OpenHalfTop, true},
    {"--//", Arrow::OpenHalfBottom, true},
    {"/|--", Arrow::FilledHalfTopReverse, true},
    {"\\|--", Arrow::FilledHalfBottomReverse, true},
    {"//--", Arrow::OpenHalfTopReverse, true},
    {"\\\\--", Arrow::OpenHalfBottomReverse, true},
    {"-|\\", Arrow::FilledHalfTop, false},
    {"-|/", Arrow::FilledHalfBottom, false},
    {"-\\\\", Arrow::OpenHalfTop, false},
    {"-//", Arrow::OpenHalfBottom, false},
    {"/|-", Arrow::FilledHalfTopReverse, false},
    {"\\|-", Arrow::FilledHalfBottomReverse, false},
    {"//-", Arrow::OpenHalfTopReverse, false},
    {"\\\\-", Arrow::OpenHalfBottomReverse, false},
    {"<<-->>", Arrow::Both, true}, {"<<->>", Arrow::Both, false},
    {"-->>", Arrow::Filled, true}, {"->>", Arrow::Filled, false},
    {"-->", Arrow::Unheaded, true}, {"->", Arrow::Unheaded, false},
    {"--x", Arrow::Cross, true}, {"-x", Arrow::Cross, false},
    {"--)", Arrow::Async, true}, {"-)", Arrow::Async, false}};

std::pair<std::size_t, const Operator*> message_operator(std::string_view text,
                                                        bool declaration_context = false) {
    bool quoted = false;
    for (std::size_t n = 0; n < text.size(); ++n) {
        if (text[n] == '"') quoted = !quoted;
        if (quoted) continue;
        // The native database declaration shares its INITIAL spelling with
        // legacy message IDs. AS/CONFIG own all subsequent literal data; only
        // a business operator encountered before those states selects a message.
        if (declaration_context && (text.substr(n).starts_with("@{") ||
            (n > 0 && (text[n-1] == ' ' || text[n-1] == '\t') && n+2 < text.size() &&
             (text[n] == 'a' || text[n] == 'A') && (text[n+1] == 's' || text[n+1] == 'S') &&
             (text[n+2] == ' ' || text[n+2] == '\t')))) break;
        for (const auto& op : kOperators)
            if (text.substr(n).starts_with(op.spelling)) return {n, &op};
    }
    return {std::string_view::npos, nullptr};
}

std::size_t delimiter(std::string_view text, char separator) {
    bool quoted = false;
    for (std::size_t n = 0; n < text.size(); ++n) {
        if (text[n] == '"') quoted = !quoted;
        if (!quoted && text[n] == separator) return n;
    }
    return std::string_view::npos;
}

enum class IdentityContext { Initial, Declaration, Message };
std::optional<std::string> participant_name(std::string_view raw,
                                           IdentityContext context=IdentityContext::Initial) {
    raw = trim(raw);
    if (raw.empty()) return {};
    const bool quoted = raw.front() == '"';
    if (quoted) {
        if (raw.size() < 3 || raw.back() != '"') return {};
        raw = raw.substr(1, raw.size() - 2);
        if (raw.find('"') != std::string_view::npos) return {};
    } else if (raw.find_first_of(context==IdentityContext::Declaration?"\"<>:,;\\/#{@}":
                               context==IdentityContext::Message?"\"<>:,;()\\/+#{}":"\"<>:,;()\\/+#{@}") != std::string_view::npos ||
               (context!=IdentityContext::Declaration&&(raw.front()=='-'||raw.front()=='+')) ||
               !sequence_id(raw.substr(0, raw.find_first_of(" \t")),
                            context==IdentityContext::Message&&raw.find_first_of(" \t")==std::string_view::npos) ||
               (context!=IdentityContext::Declaration&&message_operator(raw).second)) return {};
    // Quoted identifier compatibility follows the pinned Go terminal parser.
    // Its default header is literal text, never the sequence label decoder.
    if (raw.find_first_of("\n\r\t") != std::string_view::npos || !unicode_text(raw)) return {};
    return validated_plain_label(raw);
}

std::pair<std::string_view, std::optional<std::string_view>> declaration_parts(std::string_view text) {
    bool quoted = false;
    for (std::size_t n = 1; n + 2 < text.size(); ++n) {
        if (text[n - 1] == '"') quoted = !quoted;
        if (!quoted && (text[n - 1] == ' ' || text[n - 1] == '\t') &&
            (text[n] == 'a' || text[n] == 'A') && (text[n + 1] == 's' || text[n + 1] == 'S') &&
            (text[n + 2] == ' ' || text[n + 2] == '\t')) return {trim(text.substr(0, n)), trim(text.substr(n + 2))};
    }
    return {trim(text), std::nullopt};
}

struct Scanner {
    std::string_view rest;
    void space() { rest = trim_front(rest); }
    static std::string_view trim_front(std::string_view value) {
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
        return value;
    }
    bool take(std::string_view text) {
        if (!rest.starts_with(text)) return false;
        rest.remove_prefix(text.size());
        return true;
    }
    std::optional<std::string> id() {
        space();
        if (rest.empty() || !id_start(rest.front())) return {};
        std::size_t length = 1;
        while (length < rest.size() && id_char(rest[length])) ++length;
        auto result = std::string(rest.substr(0, length));
        rest.remove_prefix(length);
        return result;
    }
};


constexpr std::string_view kSequenceMetadataWords[]{"links", "link", "properties", "details"};
bool sequence_initial_prefix_allowed(std::string_view text, Work& work, std::string_view* metadata = nullptr) {
    // Each INITIAL caller has already front-trimmed its syntax operand.
    // Quoted IDs keep the existing native Go literal-identity profile. Prefix
    // probes are fixed-size, allocation-free and add no old-path Work charge.
    for (const auto word : kSequenceMetadataWords) {
        if (text.size() <= word.size() ||
            (text[word.size()] != ' ' && text[word.size()] != '\t')) continue;
        bool matches = true;
        for (std::size_t n = 0; n < word.size(); ++n) {
            const char ch = text[n];
            const char folded = ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
            if (folded != word[n]) { matches = false; break; }
        }
        if (!matches) continue;
        if (!work.spend(word.size())) return false;
        std::size_t at = word.size();
        while (at < text.size() && (text[at] == ' ' || text[at] == '\t')) {
            if (!work.spend()) return false;
            ++at;
        }
        if (at == text.size()) return true;
        if (!work.spend()) return false;
        const auto next = static_cast<unsigned char>(text[at]);
        // INITIAL's four metadata tokens exclude these operator/delimiter
        // characters. Unknown non-ASCII separator ambiguity stays whole source;
        // this is not a complete JavaScript Unicode-whitespace lexer.
        if (next >= 0x80) return false;
        const bool business = std::string_view("/\\+()<>:\n,;\r\f\v-").find(static_cast<char>(next)) != std::string_view::npos;
        if (metadata && !business) { *metadata = word; return true; }
        return business;
    }
    return true;
}


// These atoms exist only for effective true-wrap labels. False labels continue
// through sequence_label with its original normalization and Work sequence.
struct SequenceWrapAtom {
    std::string text;
    int cells = 0;
    bool separator = false;
    bool disclosure = false;
};
struct SequenceWrappedLabel {
    std::vector<std::vector<SequenceWrapAtom>> paragraphs;
};
struct SequenceSourceLabel {
    std::string text;
    std::optional<SequenceWrappedLabel> wrapped = std::nullopt;
};
struct SequenceWrapSource { std::string_view text; bool wrap; std::size_t prefix; };
SequenceWrapSource sequence_wrap_source(std::string_view input, bool inherited) {
    input = trim(input);
    constexpr std::pair<std::string_view, bool> prefixes[]{
        {":wrap:", true}, {"wrap:", true}, {":nowrap:", false}, {"nowrap:", false}};
    for (const auto& [prefix, value] : prefixes) {
        // Source wrap setters are case-sensitive. Uppercase lookalikes stay
        // literal label data on the existing false-wrap route.
        if (input.starts_with(prefix))
            return {trim(input.substr(prefix.size())), value, prefix.size()};
    }
    return {input, inherited, 0};
}
std::optional<SequenceSourceLabel> sequence_source_label(std::string_view input, bool inherited,
                                                       Work& work, LabelSyntax syntax = LabelSyntax::Text) {
    const bool identity = syntax == LabelSyntax::Identity;
    const bool text_syntax = syntax == LabelSyntax::Text || syntax == LabelSyntax::Metadata;
    const auto selected = identity ? SequenceWrapSource{input, inherited, 0} :
        syntax == LabelSyntax::Metadata ? SequenceWrapSource{trim(input), inherited, 0} :
            sequence_wrap_source(input, inherited);
    if (!selected.wrap) {
        // Retain legacy prefix processing/charges on its established spellings.
        auto label = identity ? sequence_display_row(input, work) : sequence_label(input, work, syntax);
        if (!label) return {};
        return SequenceSourceLabel{std::move(*label), std::nullopt};
    }
    input = selected.text;
    if (!work.spend(selected.prefix + input.size() * 24 + 128)) return {};
    if (!identity && (input.find('`') != std::string_view::npos ||
        (syntax != LabelSyntax::BoxCaption && input.find('&') != std::string_view::npos))) return {};
    if (syntax == LabelSyntax::BoxCaption) for (std::size_t at = 0; at < input.size(); ++at) {
        if (!work.spend()) return {};
        if (input[at] != '&') continue;
        auto end = at + 1;
        while (end < input.size() && input[end] != ' ' && input[end] != '\t' && input[end] != '<') {
            if (!work.spend() || input[end] == ';') return {};
            ++end;
        }
    }
    SequenceSourceLabel result;
    result.wrapped.emplace();
    auto& paragraphs = result.wrapped->paragraphs;
    paragraphs.emplace_back();
    std::size_t total = 0;
    bool have_text = false;
    auto paragraph = [&](std::string_view raw) -> bool {
        // Raw BR ownership is resolved by the caller, before a single entity
        // decode. Decoded '<br>' never re-enters source syntax.
        std::string decoded;
        decoded.reserve(raw.size());
        for (std::size_t at = 0; at < raw.size();) {
            if (!work.spend()) return false;
            if (identity || raw[at] != '#') { decoded += raw[at++]; continue; }
            constexpr std::pair<std::string_view, std::string_view> atoms[]{
                {"#35;", "#"}, {"#59;", ";"}, {"#38;", "&"}, {"#9829;", "♥"},
                {"#lt;", "<"}, {"#gt;", ">"}, {"#infin;", "∞"}};
            bool found = false;
            for (const auto& [spelling, value] : atoms) {
                if (!text_syntax && spelling == "#38;") continue;
                if (!work.spend()) return false;
                if (std::string_view(raw).substr(at).starts_with(spelling)) {
                    decoded += value; at += spelling.size(); found = true; break;
                }
            }
            if (!found) return false;
        }
        std::string literal;
        bool base = false;
        auto emit = [&](std::string_view text, int cells, bool space, bool disclosure) {
            if (cells <= 0 || cells > static_cast<int>(kLabelLimit) ||
                total > kLabelLimit - static_cast<std::size_t>(cells) ||
                !work.spend(text.size() * 4 + sizeof(SequenceWrapAtom) * 2)) return false;
            total += static_cast<std::size_t>(cells);
            result.text += text;
            paragraphs.back().push_back({std::string(text), cells, space, disclosure});
            have_text |= !space;
            return true;
        };
        auto flush = [&] {
            if (literal.empty()) return true;
            if (!work.spend(literal.size() * 48 + 64)) return false;
            const auto glyphs = ftxui::Utf8ToGlyphs(literal);
            std::string restored;
            for (const auto& glyph : glyphs) restored += glyph;
            if (restored != literal) return false;
            for (std::size_t n = 0; n < glyphs.size();) {
                const int cells = ftxui::string_width(glyphs[n]);
                if (cells < 1 || cells > 2 || n + static_cast<std::size_t>(cells) > glyphs.size()) return false;
                for (int tail = 1; tail < cells; ++tail)
                    if (!glyphs[n + static_cast<std::size_t>(tail)].empty()) return false;
                // A single whitespace scalar is a separator; attached marks
                // keep the complete grapheme a literal indivisible atom.
                bool space = false;
                for (const std::uint32_t scalar : {0x20u, 0xa0u, 0x1680u, 0x2000u, 0x2001u, 0x2002u,
                     0x2003u, 0x2004u, 0x2005u, 0x2006u, 0x2007u, 0x2008u, 0x2009u, 0x200au,
                     0x202fu, 0x205fu, 0x3000u}) {
                    if (!work.spend()) return false;
                    // UTF8 encoding is bounded to three bytes for this set.
                    std::string encoded;
                    if (scalar < 0x80) encoded += static_cast<char>(scalar);
                    else if (scalar < 0x800) { encoded += static_cast<char>(0xc0 | (scalar >> 6)); encoded += static_cast<char>(0x80 | (scalar & 63)); }
                    else { encoded += static_cast<char>(0xe0 | (scalar >> 12)); encoded += static_cast<char>(0x80 | ((scalar >> 6) & 63)); encoded += static_cast<char>(0x80 | (scalar & 63)); }
                    space |= glyphs[n] == encoded;
                }
                if (!emit(glyphs[n], cells, space, false)) return false;
                n += static_cast<std::size_t>(cells);
            }
            literal.clear(); return true;
        };
        for (std::size_t at = 0; at < decoded.size();) {
            if (!work.spend()) return false;
            const auto lead = static_cast<unsigned char>(decoded[at]);
            std::size_t bytes = 1; std::uint32_t scalar = lead;
            if (lead >= 0xc2 && lead <= 0xdf) { bytes = 2; scalar &= 31; }
            else if (lead >= 0xe0 && lead <= 0xef) { bytes = 3; scalar &= 15; }
            else if (lead >= 0xf0 && lead <= 0xf4) { bytes = 4; scalar &= 7; }
            else if (lead >= 0x80) return false;
            if (bytes > decoded.size() - at) return false;
            for (std::size_t n = 1; n < bytes; ++n) {
                const auto ch = static_cast<unsigned char>(decoded[at + n]);
                if ((ch & 0xc0) != 0x80) return false;
                scalar = (scalar << 6) | (ch & 63);
            }
            if ((bytes == 2 && scalar < 0x80) || (bytes == 3 && scalar < 0x800) ||
                (bytes == 4 && scalar < 0x10000) || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) return false;
            if (!work.spend(bytes * 24 + 96)) return false;
            const std::string unit = decoded.substr(at, bytes); at += bytes;
            const bool unsafe = scalar < 0x20 || (scalar >= 0x7f && scalar <= 0x9f) || scalar == 0xfeff ||
                scalar == 0x200b || scalar == 0x061c || scalar == 0x200e || scalar == 0x200f ||
                scalar == 0x2028 || scalar == 0x2029 || (scalar >= 0x202a && scalar <= 0x202e) ||
                (scalar >= 0x2066 && scalar <= 0x2069);
            std::string isolated;
            if (!base && !unsafe) for (const auto& glyph : ftxui::Utf8ToGlyphs(unit)) isolated += glyph;
            if (unsafe || (!base && (isolated != unit || ftxui::string_width(unit) == 0))) {
                if (!flush()) return false;
                auto visible = std::format(R"(\u{{{:04X}}})", scalar);
                const int cells = static_cast<int>(visible.size());
                if (!emit(std::move(visible), cells, false, true)) return false;
                base = false;
            } else {
                if (scalar == '\\') literal += R"(\\)"; else literal += unit;
                if (ftxui::string_width(unit) > 0) base = true;
            }
        }
        return flush();
    };
    for (std::size_t at = 0; at <= input.size();) {
        auto end = identity ? std::string_view::npos : input.find('<', at);
        std::size_t raw_break = 0;
        if (text_syntax) while (end != std::string_view::npos) {
            const auto size = sequence_raw_break_size(input.substr(end), work);
            if (!size) return {};
            if (*size != 0) { raw_break = *size; break; }
            do {
                if (!work.spend()) return {};
                ++end;
            } while (end < input.size() && input[end] != '<');
            if (end == input.size()) end = std::string_view::npos;
        }
        if (!paragraph(input.substr(at, end == std::string_view::npos ? end : end - at))) return {};
        if (end == std::string_view::npos) break;
        if (text_syntax) {
            if (total == kLabelLimit) return {};
            ++total; result.text += '\n'; paragraphs.emplace_back(); at = end + raw_break;
            continue;
        }
        auto close = end + 3;
        if (!input.substr(end).starts_with("<br")) return {};
        while (close < input.size() && (input[close] == ' ' || input[close] == '\t')) ++close;
        if (close < input.size() && input[close] == '/') ++close;
        if (close == input.size() || input[close] != '>' || total == kLabelLimit) return {};
        ++total; result.text += '\n'; paragraphs.emplace_back(); at = close + 1;
    }
    if (!have_text) return {};
    return result;
}
std::optional<std::string> sequence_pack_label(const SequenceWrappedLabel& source, int host,
                                             std::string_view prefix, Work& work) {
    if (host <= 0 || host > kWidthLimit || !work.spend(prefix.size() * 112 + 32)) return {};
    std::vector<SequenceWrapAtom> number;
    for (const char ch : prefix) number.push_back({std::string(1, ch), 1, ch == ' ', false});
    std::string result, row;
    int used = 0; std::size_t rows = 0, cells = 0;
    auto finish = [&](bool empty) {
        if (row.empty() && !empty) return true;
        if (++rows > kLabelLimit + 1 || !work.spend(row.size() + 1)) return false;
        if (rows > 1) result += '\n';
        result += row; row.clear(); used = 0; return true;
    };
    for (std::size_t p = 0; p < source.paragraphs.size(); ++p) {
        const auto& paragraph = source.paragraphs[p];
        const auto extra = p == 0 ? number.size() : 0;
        const auto count = extra + paragraph.size();
        auto atom = [&](std::size_t n) -> const SequenceWrapAtom& { return n < extra ? number[n] : paragraph[n - extra]; };
        if (!count) { if (!finish(true)) return {}; continue; }
        for (std::size_t at = 0; at < count;) {
            if (!work.spend()) return {};
            if (atom(at).separator) { ++at; continue; }
            auto end = at; int word = 0;
            while (end < count && !atom(end).separator) { if (!work.spend()) return {}; word += atom(end++).cells; }
            if (word > host) {
                if (!finish(false)) return {};
                for (auto n = at; n < end; ++n) {
                    const auto& item = atom(n);
                    if (item.cells > host || !work.spend(item.text.size() * 4 + 16)) return {};
                    if (used + item.cells > host && !finish(false)) return {};
                    row += item.text; used += item.cells; cells += static_cast<std::size_t>(item.cells);
                }
                if (!finish(false)) return {};
            } else {
                if (used + (used ? 1 : 0) + word > host && !finish(false)) return {};
                if (used) { row += ' '; ++used; ++cells; }
                for (auto n = at; n < end; ++n) {
                    if (!work.spend(atom(n).text.size() * 4 + 16)) return {};
                    row += atom(n).text; used += atom(n).cells; cells += static_cast<std::size_t>(atom(n).cells);
                }
            }
            at = end;
        }
        if (!finish(false)) return {};
    }
    if (cells > kLabelLimit || cells + (rows ? rows - 1 : 0) > kLabelLimit) return {};
    return result;
}

struct ParticipantGroup {
    std::vector<std::size_t> actors;
    std::string title, color;
    std::optional<SequenceWrappedLabel> wrapped = std::nullopt;
};

std::optional<ParticipantGroup> participant_group(std::string_view declaration, Work& work, bool inherited = false) {
    declaration = trim(declaration);
    // CSS named-color facts from the pinned Mermaid-ascii 1.6.1 parser,
    // cf447b3e, lines 749-772 (MIT); color spelling stays source-owned.
    constexpr std::string_view names =
        "aliceblue antiquewhite aqua aquamarine azure beige bisque black blanchedalmond blue "
        "blueviolet brown burlywood cadetblue chartreuse chocolate coral cornflowerblue cornsilk crimson "
        "cyan darkblue darkcyan darkgoldenrod darkgray darkgreen darkgrey darkkhaki darkmagenta darkolivegreen "
        "darkorange darkorchid darkred darksalmon darkseagreen darkslateblue darkslategray darkslategrey darkturquoise darkviolet "
        "deeppink deepskyblue dimgray dimgrey dodgerblue firebrick floralwhite forestgreen fuchsia gainsboro "
        "ghostwhite gold goldenrod gray green greenyellow grey honeydew hotpink indianred "
        "indigo ivory khaki lavender lavenderblush lawngreen lemonchiffon lightblue lightcoral lightcyan "
        "lightgoldenrodyellow lightgray lightgreen lightgrey lightpink lightsalmon lightseagreen lightskyblue lightslategray lightslategrey "
        "lightsteelblue lightyellow lime limegreen linen magenta maroon mediumaquamarine mediumblue mediumorchid "
        "mediumpurple mediumseagreen mediumslateblue mediumspringgreen mediumturquoise mediumvioletred midnightblue mintcream mistyrose moccasin "
        "navajowhite navy oldlace olive olivedrab orange orangered orchid palegoldenrod palegreen "
        "paleturquoise palevioletred papayawhip peachpuff peru pink plum powderblue purple rebeccapurple "
        "red rosybrown royalblue saddlebrown salmon sandybrown seagreen seashell sienna silver "
        "skyblue slateblue slategray slategrey snow springgreen steelblue tan teal thistle "
        "tomato turquoise violet wheat white whitesmoke yellow yellowgreen transparent ";
    if (!work.spend(declaration.size() * 4 + names.size())) return {};
    auto lower = [](std::string_view value) {
        std::string result(value);
        for (auto& ch : result) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
        return result;
    };
    std::string_view raw_color;
    auto title = declaration;
    const auto token_end = declaration.find_first_of(" \t");
    const auto token = declaration.substr(0, token_end);
    const auto folded = lower(token);
    const std::string key = " " + folded + " ";
    const std::string palette = " " + std::string(names);
    if (!token.empty() && palette.find(key) != std::string::npos) {
        raw_color = token; title = trim(declaration.substr(token.size()));
    } else if (!declaration.empty() && declaration.front() == '#') {
        const auto count = token.size() - 1;
        if (count != 3 && count != 4 && count != 6 && count != 8) return {};
        for (const auto ch : token.substr(1))
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return {};
        raw_color = token; title = trim(declaration.substr(token.size()));
    } else {
        const auto opening = declaration.find('(');
        const auto function = lower(declaration.substr(0, opening));
        const bool legacy = function == "rgb" || function == "rgba" || function == "hsl" || function == "hsla";
        constexpr std::string_view unsupported[]{"lab", "lch", "oklab", "oklch", "color", "color-mix",
            "device-cmyk", "light-dark", "var", "calc"};
        constexpr std::string_view contextual[]{"currentcolor", "inherit", "initial", "unset", "revert", "revert-layer"};
        if (std::ranges::find(contextual, folded) != std::end(contextual) ||
            (opening != std::string_view::npos && std::ranges::find(unsupported, function) != std::end(unsupported))) return {};
        if (legacy && opening != std::string_view::npos) {
            const auto closing = declaration.find(')', opening + 1);
            if (closing == std::string_view::npos ||
                (closing + 1 < declaration.size() && declaration[closing + 1] != ' ' && declaration[closing + 1] != '\t')) return {};
            auto arguments = declaration.substr(opening + 1, closing - opening - 1);
            if (arguments.find_first_of("()") != std::string_view::npos) return {};
            const bool rgb = function.starts_with("rgb");
            const int count = function == "rgba" || function == "hsla" ? 4 : 3;
            bool rgb_percent = false;
            for (int n = 0; n < count; ++n) {
                const auto comma = arguments.find(',');
                if ((n == count - 1) != (comma == std::string_view::npos)) return {};
                auto value = trim(arguments.substr(0, comma));
                bool percent = value.ends_with('%');
                if (percent) value.remove_suffix(1);
                if (!rgb && n == 0) {
                    const auto hue = lower(value);
                    for (const auto suffix : {std::string_view{"deg"}, std::string_view{"grad"}, std::string_view{"rad"}, std::string_view{"turn"}})
                        if (hue.ends_with(suffix)) {value.remove_suffix(suffix.size());break;}
                    if (percent) return {};
                } else if (!rgb && (n == 1 || n == 2) && !percent) return {};
                if (rgb && n < 3) {
                    if (!n) rgb_percent = percent;
                    else if (percent != rgb_percent) return {};
                }
                if (value.empty()) return {};
                std::size_t at = 0;
                if (value[at] == '+' || value[at] == '-') ++at;
                const auto digits = at;
                while (at < value.size() && value[at] >= '0' && value[at] <= '9') ++at;
                bool mantissa = at > digits;
                if (at < value.size() && value[at] == '.') {
                    const auto start = ++at;
                    while (at < value.size() && value[at] >= '0' && value[at] <= '9') ++at;
                    if (at == start) return {};
                    mantissa = true;
                }
                if (!mantissa) return {};
                if (at < value.size() && (value[at] == 'e' || value[at] == 'E')) {
                    ++at;
                    if (at < value.size() && (value[at] == '+' || value[at] == '-')) ++at;
                    const auto start = at;
                    while (at < value.size() && value[at] >= '0' && value[at] <= '9') ++at;
                    if (at == start) return {};
                }
                if (at != value.size()) return {};
                if (value.starts_with('+')) value.remove_prefix(1);
                double number = 0;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || !std::isfinite(number)) return {};
                if (comma != std::string_view::npos) arguments.remove_prefix(comma + 1);
            }
            raw_color = declaration.substr(0, closing + 1); title = trim(declaration.substr(closing + 1));
        }
    }
    ParticipantGroup result;
    if (!raw_color.empty()) {
        auto safe = sequence_display_row(raw_color, work);
        if (!safe || !work.spend(safe->size() * 5 + 7) || !validated_plain_label("color: " + *safe)) return {};
        result.color = std::move(*safe);
    }
    if (!title.empty()) {
        auto label = sequence_source_label(title, inherited, work, LabelSyntax::BoxCaption);
        if (!label) return {};
        result.title = std::move(label->text);
        result.wrapped = std::move(label->wrapped);
    }
    return result;
}

enum class Role { Participant, Actor, Boundary, Control, Entity, Database, Collections, Queue };

struct Participant {
    std::string id, text;
    bool explicit_label = false;
    Role role = Role::Participant;
    bool explicit_role = false;
    bool created = false;
    bool alive = true;
    bool destroyed = false;
    std::optional<SequenceWrappedLabel> wrapped = std::nullopt;
};
struct Message {
    std::size_t from; std::size_t to; std::string text; bool dashed = false;
    Arrow kind = Arrow::Filled; bool central_from = false; bool central_to = false;
    std::string number;
    std::optional<SequenceWrappedLabel> wrapped = std::nullopt;
};
enum class NotePosition { Over, Left, Right };
struct Note {
    std::vector<std::size_t> actors; std::string text;
    NotePosition position = NotePosition::Over;
    std::optional<SequenceWrappedLabel> wrapped = std::nullopt;
};
struct Fragment {
    std::string kind, text;
    std::size_t depth;
    std::vector<std::string> dividers;
};
enum class EventKind { Message, Note, Open, Divide, Close, Activate, Deactivate, Create, Destroy };
struct Event { EventKind kind; std::size_t index; std::size_t part = 0; };
enum class NoteTextAlign { Left, Center, Right };
struct Sequence {
    std::string title;
    std::vector<Participant> participants;
    std::vector<Message> messages;
    std::vector<Note> notes;
    std::vector<Fragment> fragments;
    std::vector<Event> events;
    std::vector<std::size_t> open_fragments;
    double number_next = 1, number_step = 1;
    bool number_visible = false;
    // Presence also rejects ambiguous repeated raw configuration aliases.
    std::optional<bool> configured_number_visible;
    std::optional<NoteTextAlign> note_text_align;
    std::optional<bool> root_wrap, sequence_wrap;
    bool wrap_default() const { return root_wrap.value_or(sequence_wrap.value_or(false)); }
    bool has_wrapped_labels = false;
    std::vector<ParticipantGroup> groups;
    std::optional<std::size_t> open_group;
    std::size_t group_openings = 0;
    std::vector<std::size_t> activation_depth;
    std::optional<std::size_t> pending_create, pending_destroy;
    bool activation(std::size_t actor, bool opening) {
        if (!participants[actor].alive && (opening || !participants[actor].destroyed)) return false;
        auto& depth = activation_depth[actor];
        if (opening) {
            if (depth >= 16) return false;
            ++depth;
        } else {
            if (!depth) return false;
            --depth;
        }
        return event(opening ? EventKind::Activate : EventKind::Deactivate, actor);
    }
    bool event(EventKind kind, std::size_t index, std::size_t part = 0) {
        if (events.size() >= 512) return false;
        events.push_back({kind, index, part});
        return true;
    }
    std::optional<std::size_t> find_participant(std::string_view id) const {
        for (std::size_t n = 0; n < participants.size(); ++n)
            if (participants[n].id == id) return n;
        return {};
    }
    std::optional<std::size_t> participant(std::string id) {
        if (const auto known = find_participant(id)) return known;
        if (participants.size() >= 32) return {};
        participants.push_back({id, id, false});
        activation_depth.push_back(0);
        return participants.size() - 1;
    }
};

std::optional<double> number_parameter(std::string_view token, Work& work) {
    if (token.empty() || !work.spend(token.size())) return {};
    const auto dot = token.find('.');
    if (dot != std::string_view::npos &&
        (token.size() - dot - 1 < 1 || token.size() - dot - 1 > 2)) return {};
    for (std::size_t n = 0; n < token.size(); ++n)
        if (n != dot && (token[n] < '0' || token[n] > '9')) return {};
    double value = 0;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error != std::errc{} || end != token.data() + token.size() || !std::isfinite(value)) return {};
    return value;
}

std::optional<std::string> number_display(double value, Work& work) {
    if (!std::isfinite(value) || value <= 0) return {};
    std::array<char, 64> buffer{};
    const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                           std::chars_format::general);
    if (error != std::errc{} || !work.spend(static_cast<std::size_t>(end - buffer.data()))) return {};
    std::string text(buffer.data(), end);
    const auto marker = text.find('e');
    if (marker == std::string::npos) return text;
    auto exponent_text = std::string_view(text).substr(marker + 1);
    if (exponent_text.starts_with('+')) exponent_text.remove_prefix(1);
    int exponent = 0;
    const auto [last, status] = std::from_chars(exponent_text.data(), exponent_text.data() + exponent_text.size(), exponent);
    if (status != std::errc{} || last != exponent_text.data() + exponent_text.size()) return {};
    // JavaScript formats its shortest significant digits without exponent
    // notation in this range, rather than choosing a shortest fixed integer.
    if (value >= 1e-6 && value < 1e21) {
        auto digits = text.substr(0, marker);
        const auto dot = digits.find('.');
        const int place = static_cast<int>(dot == std::string::npos ? digits.size() : dot) + exponent;
        if (dot != std::string::npos) digits.erase(dot, 1);
        if (place <= 0) text = "0." + std::string(static_cast<std::size_t>(-place), '0') + digits;
        else if (place >= static_cast<int>(digits.size())) text = digits + std::string(static_cast<std::size_t>(place) - digits.size(), '0');
        else text = digits.substr(0, static_cast<std::size_t>(place)) + "." + digits.substr(static_cast<std::size_t>(place));
    } else text = text.substr(0, marker) + "e" + (exponent >= 0 ? "+" : "") + std::to_string(exponent);
    if (!work.spend(text.size())) return {};
    return text;
}

struct RoleDeclaration {
    std::string id;
    std::optional<std::string> alias;
    Role role;
    std::optional<SequenceWrappedLabel> wrapped = std::nullopt;
};
std::optional<std::vector<std::pair<std::string,std::string>>> role_fields(std::string_view object,Work& work) {
    // The shared block-YAML reader cannot enforce this flow grammar's complete
    // consumption and duplicate rules. Only this private role shape is read.
    // role_declaration already prepaid eight passes over the whole declaration
    // for scans, strict scalar parsing, storage and copies. Do not charge that
    // original JSON route twice; new raw-YAML validation owns its extra work.
    if (!work.spend(64)||object.size()<2||object.front()!='{'||object.back()!='}'||
        object.find_first_of("\n\r")!=std::string_view::npos) return {};
    auto input=trim(object.substr(1,object.size()-2));
    std::vector<std::pair<std::string,std::string>> fields;
    fields.reserve(2);
    auto raw_scalar_valid=[&](std::string_view scalar) {
        // Reuse the strict JSON string reader for UTF-8 validation without
        // applying display admission to an alias that AS may override.
        // YAML raw C0 bytes are illegal except TAB; encoded JSON escapes keep
        // their existing decoded-value contract in the double-quoted branch.
        if (!work.spend(scalar.size()*8+64)) return false;
        std::string encoded;
        encoded.reserve(scalar.size()*2+2);
        encoded+='"';
        for (const unsigned char ch:scalar) {
            if (ch<0x20&&ch!='\t') return false;
            if (ch=='\t') encoded+="\\t";
            else {
                if (ch=='"'||ch=='\\') encoded+='\\';
                encoded+=static_cast<char>(ch);
            }
        }
        encoded+='"';
        const auto value=loom::utils::json::parse(encoded);
        return value&&value->root().is_str();
    };
    auto quoted=[&]() -> std::optional<std::string> {
        if (input.front()=='"') {
            std::size_t at=1;bool escape=false;
            for (;at<input.size();++at) {
                const char ch=input[at];
                if (escape) {escape=false;continue;}
                if (ch=='\\') {escape=true;continue;}
                if (ch=='"') break;
            }
            if (at==input.size()) return {};
            const auto scalar=loom::utils::json::parse(input.substr(0,at+1));
            if (!scalar||!scalar->root().is_str()) return {};
            std::string value(scalar->root().as_str());input.remove_prefix(at+1);return value;
        }
        std::string value;
        for (std::size_t at=1;at<input.size();++at) {
            if (input[at]!='\'') {value+=input[at];continue;}
            if (at+1<input.size()&&input[at+1]=='\'') {value+='\'';++at;continue;}
            if (!raw_scalar_valid(value)) return {};
            input.remove_prefix(at+1);return value;
        }
        return {};
    };
    while (!input.empty()&&fields.size()<2) {
        const bool quoted_key=input.front()=='"'||input.front()=='\'';
        std::optional<std::string> key;
        if (quoted_key) key=quoted();
        else {
            std::size_t at=0;while (at<input.size()&&id_char(input[at])) ++at;
            if (!at) return {};
            key=std::string(input.substr(0,at));input.remove_prefix(at);
        }
        if (!key||(*key!="type"&&*key!="alias")||
            std::ranges::any_of(fields,[&](const auto& field){return field.first==*key;})) return {};
        input=trim(input);
        if (input.empty()||input.front()!=':') return {};
        input.remove_prefix(1);
        if (!quoted_key&&!input.empty()&&input.front()!=' '&&input.front()!='\t') return {};
        input=trim(input);if (input.empty()) return {};
        std::optional<std::string> value;
        if (input.front()=='"'||input.front()=='\'') value=quoted();
        else {
            const auto end=input.find(',');const auto scalar=trim(input.substr(0,end));
            if (scalar.empty()||scalar.find_first_of("\t:\"'{}[]&*!|>#\\")!=std::string_view::npos) return {};
            const auto first=static_cast<unsigned char>(scalar.front());
            if (!id_start(scalar.front())&&first<0x80) return {};
            if (!raw_scalar_valid(scalar)) return {};
            std::string lower(scalar);for (auto& ch:lower) if (ch>='A'&&ch<='Z') ch=static_cast<char>(ch+('a'-'A'));
            if (lower=="true"||lower=="false"||lower=="null") return {};
            value=std::string(scalar);input.remove_prefix(static_cast<std::size_t>(scalar.data()-input.data())+scalar.size());
        }
        if (!value) return {};
        fields.emplace_back(std::move(*key),std::move(*value));input=trim(input);
        if (input.empty()) return fields;
        if (input.front()!=',') return {};
        input=trim(input.substr(1));if (input.empty()) return {};
    }
    return {};
}

std::optional<RoleDeclaration> role_declaration(std::string_view declaration, Role keyword, Work& work, bool inherited) {
    // Only a failed ordinary declaration with an actual introducer reaches
    // this closed string-mapping grammar. Prepay scans, parser storage and copies.
    if (declaration.size() > kSourceLimit || !work.spend(declaration.size() * 8 + 32)) return {};
    const auto open = declaration.find("@{");
    if (open == std::string_view::npos) return {};
    const auto raw_id = trim(declaration.substr(0, open));
    if (raw_id.empty() || raw_id.front() == '"') return {};
    // Metadata ACTOR is one token: fixed ECMAScript whitespace, not a
    // second Unicode lexer. The original ID validator follows this byte pass.
    for (std::size_t at = 0; at < raw_id.size(); ++at) {
        const auto a = static_cast<unsigned char>(raw_id[at]);
        if (a < 0x80) {
            if ((a >= 0x09 && a <= 0x0d) || a == 0x20) return {};
            continue;
        }
        const auto b = at + 1 < raw_id.size() ? static_cast<unsigned char>(raw_id[at + 1]) : 0u;
        const auto c = at + 2 < raw_id.size() ? static_cast<unsigned char>(raw_id[at + 2]) : 0u;
        if ((a == 0xc2 && b == 0xa0) || (a == 0xe1 && b == 0x9a && c == 0x80) ||
            (a == 0xe2 && b == 0x80 && ((c >= 0x80 && c <= 0x8a) || c == 0xa8 || c == 0xa9 || c == 0xaf)) ||
            (a == 0xe2 && b == 0x81 && c == 0x9f) || (a == 0xe3 && b == 0x80 && c == 0x80) ||
            (a == 0xef && b == 0xbb && c == 0xbf)) return {};
    }
    auto id = participant_name(raw_id, IdentityContext::Declaration);
    if (!id) return {};
    // Primary CONFIG ends at the first literal close even inside a string.
    const auto close = declaration.find('}', open + 2);
    if (close == std::string_view::npos) return {};
    const auto object = declaration.substr(open + 1, close - open);
    const auto fields=role_fields(object,work);
    if (!fields) return {};
    std::string_view type, metadata_alias;
    bool have_type=false;
    for (const auto& [name,value]:*fields) {
        if (name=="type") {have_type=true;type=value;}
        else metadata_alias=value;
    }
    Role role = keyword;
    if (have_type) {
        if (type == "participant") role = Role::Participant;
        else if (type == "actor") role = Role::Actor;
        else if (type == "boundary") role = Role::Boundary;
        else if (type == "control") role = Role::Control;
        else if (type == "entity") role = Role::Entity;
        else if (type == "database") role = Role::Database;
        else if (type == "collections") role = Role::Collections;
        else if (type == "queue") role = Role::Queue;
        else return {};
    }
    const auto raw_tail = declaration.substr(close + 1);
    if (!raw_tail.empty() && raw_tail.front() != ' ' && raw_tail.front() != '\t') return {};
    auto tail = trim(raw_tail);
    std::optional<std::string_view> raw_as;
    std::optional<std::string> alias;
    if (!tail.empty()) {
        if (tail.size() < 3 || (tail[0] != 'a' && tail[0] != 'A') ||
            (tail[1] != 's' && tail[1] != 'S') || (tail[2] != ' ' && tail[2] != '\t')) return {};
        raw_as = trim(tail.substr(2));
        if (raw_as->empty()) return {};
    }
    // Resolve raw precedence before display admission: either overridden
    // alias may be valid identity data but unsupported selected TXT syntax.
    std::optional<SequenceWrappedLabel> wrapped = std::nullopt;
    std::optional<std::string_view> selector{raw_id};
    bool metadata_wrap = inherited;
    if (raw_as) {
        const auto selected = sequence_wrap_source(*raw_as, inherited);
        metadata_wrap = selected.wrap;
        if (selected.wrap && selected.prefix) {
            if (!work.spend(selected.prefix)) return {};
            selector = selected.text;
        } else selector = source_nowrap_text(*raw_as, work);
    }
    if (!selector) return {};
    std::optional<SequenceSourceLabel> label;
    if (!metadata_alias.empty() && *selector == raw_id) {
        if (metadata_alias != trim(metadata_alias)) return {};
        label = sequence_source_label(metadata_alias, metadata_wrap, work, LabelSyntax::Metadata);
    } else if (raw_as) label = sequence_source_label(*raw_as, inherited, work);
    if ((!metadata_alias.empty() && *selector == raw_id) || raw_as) {
        if (!label) return {};
        alias = std::move(label->text); wrapped = std::move(label->wrapped);
    }
    return RoleDeclaration{std::move(*id), std::move(alias), role, std::move(wrapped)};
}

// These readers consume nonvisual source without adding actors or events.
// They are deliberately local to the closed sequence terminal profile.
bool sequence_raw_scalar(std::string_view value, Work& work) {
    if (!work.spend(value.size() * 8 + 32)) return false;
    std::string quoted;
    quoted.reserve(value.size() * 2 + 2);
    quoted += '"';
    for (const unsigned char ch : value) {
        if (ch < 0x20) return false;
        if (ch == '"' || ch == '\\') quoted += '\\';
        quoted += static_cast<char>(ch);
    }
    quoted += '"';
    const auto doc = loom::utils::json::parse(quoted);
    return doc && doc->root().is_str() && unicode_text(doc->root().as_str());
}

bool sequence_json_tree(loom::utils::json::JsonVal root, Work& work) {
    using loom::utils::json::JsonVal;
    if (!work.spend(32)) return false;
    std::vector<JsonVal> pending{root};
    while (!pending.empty()) {
        const auto value = pending.back(); pending.pop_back();
        if (!work.spend(16 + value.size() * 32)) return false;
        if (value.is_obj()) {
            std::vector<std::string_view> keys;
            keys.reserve(value.size());
            bool valid = true;
            value.iter_obj([&](JsonVal key, JsonVal child) {
                if (!valid) return;
                const auto name = key.as_str();
                if (!work.spend(name.size() * (keys.size() + 1) + 1) || !unicode_text(name)) {
                    valid = false; return;
                }
                if (std::ranges::find(keys, name) != keys.end()) { valid = false; return; }
                keys.push_back(name); pending.push_back(child);
            });
            if (!valid) return false;
        } else if (value.is_arr()) {
            value.iter([&](JsonVal child) { pending.push_back(child); });
        } else if (value.is_str()) {
            if (!work.spend(value.as_str().size()) || !unicode_text(value.as_str())) return false;
        } else if (value.is_num() && !std::isfinite(value.as_double())) return false;
    }
    return true;
}

std::optional<loom::utils::json::JsonDoc> sequence_json(std::string_view text, Work& work) {
    if (!work.spend(text.size() * 8 + 32)) return {};
    auto doc = loom::utils::json::parse(text);
    if (!doc || !sequence_json_tree(doc->root(), work)) return {};
    return std::move(*doc);
}

// A finite fully consumed monochrome fill profile, not a CSS engine. Unknown
// rules retain the complete source; browser colour/cascade is not reproduced.
bool sequence_fill_css(std::string_view css, Work& work) {
    if (css.empty() || css.size() > kSourceLimit || !work.spend(css.size() * 64 + 64)) return false;
    std::size_t at = 0;
    auto space = [&] { while (at < css.size() && css[at] == ' ') ++at; };
    auto take = [&](std::string_view token) {
        if (!css.substr(at).starts_with(token)) return false;
        at += token.size(); return true;
    };
    auto letter = [](char ch) { return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'); };
    auto word = [&] {
        const auto begin = at;
        while (at < css.size() && letter(css[at])) ++at;
        return css.substr(begin, at - begin);
    };
    auto equal_keyword = [](std::string_view value, std::string_view expected) {
        if (value.size() != expected.size()) return false;
        for (std::size_t n = 0; n < value.size(); ++n) {
            char ch = value[n]; if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
            if (ch != expected[n]) return false;
        }
        return true;
    };
    bool rules = false;
    space();
    while (at < css.size()) {
        if (!take(".loopText") && !take(".labelBox")) return false;
        space(); if (!take("{")) return false;
        bool declaration = false;
        while (true) {
            space();
            if (take("}")) { if (!declaration) return false; break; }
            if (!equal_keyword(word(), "fill")) return false;
            space(); if (!take(":")) return false;
            space();
            if (take("#")) {
                const auto begin = at;
                while (at < css.size() && ((css[at] >= '0' && css[at] <= '9') ||
                       (css[at] >= 'A' && css[at] <= 'F') || (css[at] >= 'a' && css[at] <= 'f'))) ++at;
                if (at - begin != 3 && at - begin != 6) return false;
            } else {
                const auto color = word();
                bool known = false;
                for (const std::string_view name : {"aqua", "black", "blue", "fuchsia", "gray", "green", "lime", "maroon",
                                                    "navy", "olive", "purple", "red", "silver", "teal", "white", "yellow"})
                    known |= equal_keyword(color, name);
                if (!known) return false;
            }
            space();
            if (take("!")) { space(); if (!equal_keyword(word(), "important")) return false; space(); }
            declaration = true;
            if (take(";")) continue;
            if (at >= css.size() || css[at] != '}') return false;
        }
        rules = true; space();
    }
    return rules;
}

bool sequence_config_leaf(std::string_view path, loom::utils::json::JsonVal value,
                          bool directive, Sequence& graph, Work& work) {
    // Keep raw origin namespaces distinct. An init's legacy config maps under
    // sequence; it is not recursively flattened into frontmatter.config.
    if (!directive && path == "title") {
        if (!value.is_str() || !work.spend(value.as_str().size())) return false;
        const auto title = sequence_label(value.as_str(), work);
        if (!title) return false;
        graph.title = *title; return true;
    }
    bool sequence_options = false;
    if (!directive) {
        if (!path.starts_with("config.")) return false;
        path.remove_prefix(7);
        if (path.starts_with("sequence.")) { path.remove_prefix(9); sequence_options = true; }
    } else if (path.starts_with("config.")) {
        path.remove_prefix(7);
        sequence_options = true;
        // This raw legacy path is deliberately not recursively flattened.
        // It is a reviewed inert nested source option, not an effective flag.
        if (path.starts_with("sequence."))
            return path == "sequence.hideUnusedParticipants" && value.is_bool();
    } else if (path.starts_with("sequence.")) { path.remove_prefix(9); sequence_options = true; }
    auto numeric = [&] { return value.is_num() && std::isfinite(value.as_double()) && value.as_double() >= 0; };
    auto string_in = [&](std::initializer_list<std::string_view> choices) {
        return value.is_str() && std::ranges::find(choices, value.as_str()) != choices.end();
    };
    if (path == "wrap") {
        if (!value.is_bool()) return false;
        (sequence_options ? graph.sequence_wrap : graph.root_wrap) = value.as_bool();
        return true;
    }
    if (!sequence_options) {
        if (path == "theme") return string_in({"default", "base", "dark", "forest", "neutral",
                                               "redux", "redux-color", "redux-dark", "redux-dark-color"});
        if (path == "look") return string_in({"classic", "handDrawn", "neo"});
        if (path == "themeCSS") return value.is_str() && sequence_fill_css(value.as_str(), work);
        if (path == "logLevel") return numeric() && value.as_double() <= 5 && std::floor(value.as_double()) == value.as_double();
        if (path == "themeVariables.fontSize")
            return value.is_str() && !value.as_str().empty() && sequence_raw_scalar(value.as_str(), work);
        if (path != "fontSize") return false;
    }
    if (sequence_options && path == "showSequenceNumbers") {
        if (!value.is_bool() || graph.configured_number_visible.has_value()) return false;
        graph.configured_number_visible = value.as_bool(); return true;
    }
    if (sequence_options && path == "noteAlign") {
        if (!value.is_str() || graph.note_text_align.has_value()) return false;
        if (value.as_str() == "left") graph.note_text_align = NoteTextAlign::Left;
        else if (value.as_str() == "center") graph.note_text_align = NoteTextAlign::Center;
        else if (value.as_str() == "right") graph.note_text_align = NoteTextAlign::Right;
        else return false;
        return true;
    }
    if (sequence_options && path == "useMaxWidth") return value.is_bool();
    if (sequence_options && path == "actorMargin") return numeric();
    if (path == "fontSize" || path == "messageFontSize" ||
        (sequence_options && (path == "actorFontSize" || path == "noteFontSize")))
        return numeric() && value.as_double() > 0;
    if (path == "diagramMarginX" || path == "diagramMarginY") return numeric();
    if (path == "mirrorActors" || path == "forceMenus" || path == "hideUnusedParticipants") return value.is_bool();
    if (path == "messageAlign") return string_in({"left", "center", "right"});
    // Finite monochrome/cell-metric projection: these source-owned visual
    // leaves cannot alter terminal font, SVG width or menu/security machinery.
    // Unknown siblings/types still fail the complete source.
    if (sequence_options && path == "securityLevel")
        return string_in({"strict", "loose", "antiscript", "sandbox"});
    if (sequence_options && path == "width") return numeric() && value.as_double() > 0;
    if (sequence_options && (path == "fontWeight" || path == "messageFontWeight"))
        return (numeric() && value.as_double() > 0) || string_in({"normal", "bold", "bolder", "lighter"});
    if (path == "actorFontFamily" || path == "messageFontFamily" ||
        (sequence_options && (path == "fontFamily" || path == "noteFontFamily")))
        return value.is_str() && !value.as_str().empty() && sequence_raw_scalar(value.as_str(), work);
    return false;
}

bool sequence_config(loom::utils::json::JsonVal root, bool directive, Sequence& graph, Work& work) {
    using loom::utils::json::JsonVal;
    if (!root.is_obj() || !root.size() || !work.spend(root.size() * 64 + 32)) return false;
    std::vector<std::pair<std::string, JsonVal>> pending;
    bool valid = true;
    root.iter_obj([&](JsonVal key, JsonVal value) {
        if (!valid) return;
        if (!work.spend(key.as_str().size() + 32)) { valid = false; return; }
        if (key.as_str().find('.') != std::string_view::npos) { valid = false; return; }
        pending.emplace_back(key.as_str(), value);
    });
    if (!valid) return false;
    while (!pending.empty()) {
        auto [path, value] = std::move(pending.back()); pending.pop_back();
        if (!work.spend(path.size() + value.size() * (path.size() + 64) + 16)) return false;
        if (value.is_obj()) {
            const bool container = path == "config" || path == "sequence" || path == "config.sequence" ||
                path == "themeVariables" || path == "config.themeVariables";
            if (!container || !value.size()) return false;
            value.iter_obj([&](JsonVal key, JsonVal child) {
                if (!valid) return;
                if (!work.spend(path.size() + key.as_str().size() + 32)) { valid = false; return; }
                if (key.as_str().find('.') != std::string_view::npos) { valid = false; return; }
                pending.emplace_back(path + "." + std::string(key.as_str()), child);
            });
            if (!valid) return false;
        } else if (!sequence_config_leaf(path, value, directive, graph, work)) return false;
    }
    return true;
}

std::optional<std::string> sequence_directive_json(std::string_view text, Work& work) {
    if (!work.spend(text.size() * 12 + 64)) return {};
    std::string normalized;
    normalized.reserve(text.size() * 2);
    for (std::size_t at = 0; at < text.size();) {
        const char quote = text[at];
        if (quote != '\'' && quote != '"') { normalized += text[at++]; continue; }
        const auto begin = at++;
        bool escaped = false;
        for (; at < text.size(); ++at) {
            const char ch = text[at];
            if (quote == '\'' && ch == '\\') return {}; // Unreviewed single-quote escape grammar.
            if (escaped) { escaped = false; continue; }
            if (ch == '\\') { escaped = true; continue; }
            if (ch == quote) break;
            if (static_cast<unsigned char>(ch) < 0x20) return {};
        }
        if (at == text.size()) return {};
        if (quote == '"') normalized.append(text.substr(begin, at - begin + 1));
        else {
            const auto raw = text.substr(begin + 1, at - begin - 1);
            if (!sequence_raw_scalar(raw, work)) return {};
            normalized += '"';
            for (const char ch : raw) { if (ch == '"') normalized += '\\'; normalized += ch; }
            normalized += '"';
        }
        ++at;
    }
    return normalized;
}

bool sequence_directive(std::string_view& source, Sequence& graph, Work& work) {
    // Prepaid quote-aware wrapper scan; only a complete leading directive is consumed.
    if (!work.spend(source.size() + 32)) return false;
    std::size_t close = std::string_view::npos;
    char quote = 0; bool escaped = false;
    for (std::size_t at = 3; at + 2 < source.size(); ++at) {
        const char ch = source[at];
        if (escaped) { escaped = false; continue; }
        if (quote && ch == '\\') { escaped = true; continue; }
        if (quote) { if (ch == quote) quote = 0; continue; }
        if (ch == '"' || ch == '\'') { quote = ch; continue; }
        if (source.substr(at).starts_with("}%%")) { close = at; break; }
    }
    if (close == std::string_view::npos) return false;
    auto contents = source.substr(3, close - 3);
    while (!contents.empty() && (contents.front() == ' ' || contents.front() == '\t' || contents.front() == '\n' || contents.front() == '\r')) contents.remove_prefix(1);
    const auto colon = contents.find(':');
    if (colon == std::string_view::npos) {
        const auto command = trim(contents);
        if (command != "wrap" && command != "nowrap") return false;
        graph.root_wrap = command == "wrap";
    } else {
        const auto kind = trim(contents.substr(0, colon));
        if (kind != "init" && kind != "initialize") return false;
        const auto normalized = sequence_directive_json(contents.substr(colon + 1), work);
        if (!normalized) return false;
        const auto doc = sequence_json(*normalized, work);
        if (!doc || !sequence_config(doc->root(), true, graph, work)) return false;
    }
    auto tail = source.substr(close + 3);
    const auto end = tail.find('\n');
    auto trailing = tail.substr(0, end);
    if (trailing.ends_with('\r')) trailing.remove_suffix(1);
    if (!trim(trailing).empty()) return false;
    source = end == std::string_view::npos ? std::string_view{} : tail.substr(end + 1);
    return true;
}

bool sequence_description_scalar(std::string_view value, Work& work) {
    if (!work.spend(value.size() * 2 + 1)) return false;
    if (value.find("\xef\xbb\xbf") != std::string_view::npos ||
        value.find("\xe2\x80\x8b") != std::string_view::npos) return false;
    // Horizontal tabs inside opaque documentation are data, not indentation or
    // terminal output. Validate every other byte with the existing scalar rule.
    while (true) {
        const auto tab = value.find('\t');
        if (!sequence_raw_scalar(value.substr(0, tab), work)) return false;
        if (tab == std::string_view::npos) return true;
        value.remove_prefix(tab + 1);
    }
}
// Description is an opaque documentation string in the pinned frontmatter
// contract. Consume only this finite folded-scalar profile, never business text.
bool sequence_description_block(std::string_view& source, Work& work) {
    // Pay for all delimiter/physical-line lookahead before searching. Views keep
    // the original body and any in-body init directive completely untouched.
    if (!work.spend(source.size() + 32)) return false;
    std::optional<std::size_t> indentation;
    while (!source.empty()) {
        const auto end = source.find('\n');
        auto line = source.substr(0, end);
        if (line.ends_with('\r')) line.remove_suffix(1);
        if (!work.spend(line.size() + 16)) return false;
        if (trim(line).empty() && line.find('\t') != std::string_view::npos) return false;
        if (!trim(line).empty()) {
            const auto indent = line.find_first_not_of(' ');
            if (indent == 0) return true; // Reprocess this complete root line.
            if (indent == std::string_view::npos || line[indent] == '\t') return false;
            if (!indentation) indentation = indent;
            if (indent < *indentation || !sequence_description_scalar(line.substr(*indentation), work)) return false;
        }
        source = end == std::string_view::npos ? std::string_view{} : source.substr(end + 1);
    }
    // The original frontmatter loop still requires its actual closing fence.
    return true;
}

bool sequence_frontmatter(std::string_view frontmatter, Sequence& graph, Work& work) {
    auto source = frontmatter;
    // A finite block-map reader, not the permissive shared YAML parser.
    std::vector<std::pair<std::size_t, std::string>> parents;
    std::vector<std::pair<std::string, std::size_t>> child_indents;
    auto has_children = [&](std::string_view path) {
        return std::ranges::any_of(child_indents, [&](const auto& item) { return item.first == path; });
    };
    std::vector<std::string> seen;
    bool content = false;
    while (!source.empty()) {
        const auto end = source.find('\n');
        auto line = source.substr(0, end);
        source = end == std::string_view::npos ? std::string_view{} : source.substr(end + 1);
        if (line.ends_with('\r')) line.remove_suffix(1);
        if (!work.spend(line.size() * 12 + 64)) return false;
        if (trim(line).empty()) continue;
        const auto indent = line.find_first_not_of(' ');
        if (indent == std::string_view::npos || line[indent] == '\t') return false;
        auto field = line.substr(indent);
        const auto colon = field.find(':');
        if (colon == std::string_view::npos) return false;
        const auto key = field.substr(0, colon);
        if (key.empty() || !std::ranges::all_of(key, id_char)) return false;
        while (!parents.empty() && parents.back().first >= indent) {
            if (!has_children(parents.back().second)) return false;
            parents.pop_back();
        }
        if ((parents.empty() && indent != 0) || (!parents.empty() && indent <= parents.back().first)) return false;
        const std::string_view parent = parents.empty() ? std::string_view{} : parents.back().second;
        const auto sibling = std::ranges::find_if(child_indents, [&](const auto& item) { return item.first == parent; });
        if (sibling != child_indents.end()) { if (sibling->second != indent) return false; }
        else child_indents.emplace_back(parent, indent);
        std::string path = parents.empty() ? std::string(key) : parents.back().second + "." + std::string(key);
        if (!work.spend(path.size() * (seen.size() + 1) + 1) || std::ranges::find(seen, path) != seen.end()) return false;
        seen.push_back(path);
        const auto raw = field.substr(colon + 1);
        if (!raw.empty() && raw.front() != ' ' && raw.front() != '\t') return false;
        const auto value = trim(raw);
        if (path == "description" && (value == ">" || value == ">-")) {
            if (indent != 0 || !sequence_description_block(source, work)) return false;
            content = true; continue;
        }
        if (path == "description" && !value.empty() && value.front() != '\'' && value.front() != '"') {
            // A closed plain-string subset, not YAML scalar type inference.
            // Quoted strings below use the existing complete scalar decoder.
            const auto first = static_cast<unsigned char>(value.front());
            if (!id_start(value.front()) && first < 0x80) return false;
            if (!work.spend(value.size() + 1)) return false;
            std::string lower(value);
            for (auto& ch : lower) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
            if (lower == "null" || lower == "true" || lower == "false") return false;
        }
        if (value.empty()) {
            if (path != "config" && path != "config.sequence" && path != "config.themeVariables") return false;
            parents.emplace_back(indent, std::move(path)); continue;
        }
        // Preserve the existing plain frontmatter-title profile. Quoted title
        // text needs its own literal policy, not parseMessage/BR/nowrap rules.
        if (path == "title" && (value.front() == '"' || value.front() == '\'')) return false;
        std::string scalar;
        if (value.front() == '"') scalar = value;
        else if (value.front() == '\'') {
            if (value.size() < 2 || value.back() != '\'') return false;
            const auto normalized = sequence_directive_json(value, work);
            if (!normalized) return false;
            scalar = *normalized;
        } else if (value == "true" || value == "false" || value == "null" ||
                   value.front() == '-' || (value.front() >= '0' && value.front() <= '9')) scalar = value;
        else {
            // Only opaque documentation admits literal backticks; titles and
            // every other plain configuration scalar retain the original guard.
            const auto forbidden = path == "description" ? "\t:#\"'&*{}[],|!%@\\" : "\t:#\"'&*{}[],|!%@`\\";
            if (value.find_first_of(forbidden) != std::string_view::npos ||
                !sequence_raw_scalar(value, work)) return false;
            if (path == "title") {
                if (value.find("\xef\xbb\xbf") != std::string_view::npos ||
                    value.find("\xe2\x80\x8b") != std::string_view::npos) return false;
                const auto first = static_cast<unsigned char>(value.front());
                if (!id_start(value.front()) && first < 0x80) return false;
                std::string lower(value);
                for (auto& ch : lower) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
                if (lower == "null" || lower == "true" || lower == "false" || lower == "yes" ||
                    lower == "no" || lower == "on" || lower == "off") return false;
            }
            scalar = '"' + std::string(value) + '"';
        }
        const auto doc = sequence_json(scalar, work);
        if (!doc) return false;
        if (path == "description") {
            if (indent != 0 || !doc->root().is_str() || !sequence_description_scalar(doc->root().as_str(), work)) return false;
        } else if (!sequence_config_leaf(path, doc->root(), false, graph, work)) return false;
        content = true;
    }
    for (const auto& parent : parents) if (!has_children(parent.second)) return false;
    return content;
}

bool sequence_metadata_prefix(std::string_view text) {
    // One allocation-free scanner peek, within the existing prepaid two-pass
    // line allowance. The statement's original paid guard owns classification.
    for (const std::string_view word : kSequenceMetadataWords) {
        if (text.size() <= word.size() || (text[word.size()] != ' ' && text[word.size()] != '\t')) continue;
        bool equal = true;
        for (std::size_t at = 0; at < word.size(); ++at) {
            const char ch = text[at];
            if ((ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch) != word[at]) { equal = false; break; }
        }
        if (!equal) continue;
        auto operand = Scanner::trim_front(text.substr(word.size()));
        if (operand.empty()) return false;
        const auto first = static_cast<unsigned char>(operand.front());
        if (first >= 0x80) return false;
        return std::string_view("/\\+()<>:\n,;\r\f\v-").find(operand.front()) == std::string_view::npos;
    }
    return false;
}

bool sequence_metadata(std::string_view text, std::string_view kind, Work& work) {
    if (!work.spend(text.size() * 8 + 64)) return false;
    const auto space = text.find_first_of(" \t");
    const auto body = trim(text.substr(space));
    const auto colon = body.find(':');
    if (colon == std::string_view::npos) return false;
    const auto actor = trim(body.substr(0, colon));
    if (actor.empty() || actor.find_first_of(" \t\"<>:,;()\\/+#{}@") != std::string_view::npos || !sequence_raw_scalar(actor, work)) return false;
    const auto payload = trim(body.substr(colon + 1));
    if (payload.empty()) return false;
    if (kind == "links" || kind == "properties") {
        const auto doc = sequence_json(payload, work);
        if (!doc || !doc->root().is_obj()) return false;
        if (kind == "links") {
            bool valid = true;
            doc->root().iter_obj([&](auto key, auto value) {
                if (!valid) return;
                valid = !key.as_str().empty() && value.is_str() && !value.as_str().empty();
            });
            return valid;
        }
        return true;
    }
    if (kind == "link") {
        const auto separator = payload.find('@');
        if (separator == std::string_view::npos || trim(payload.substr(0, separator)).empty() || trim(payload.substr(separator + 1)).empty()) return false;
    }
    return sequence_raw_scalar(payload, work);
}

// 0 is an ordinary business line, 1 a fully consumed a11y setter, -1 failure.
int sequence_accessibility(std::string_view line, std::string_view& source, Work& work) {
    constexpr std::string_view names[]{"acctitle", "accdescr"};
    for (const auto name : names) {
        if (line.size() < name.size()) continue;
        bool equal = true;
        for (std::size_t n = 0; n < name.size(); ++n) {
            const char ch = line[n];
            if ((ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch) != name[n]) { equal = false; break; }
        }
        if (!equal) continue;
        auto value = trim(line.substr(name.size()));
        if (value.empty() || (value.front() != ':' && (name != "accdescr" || value.front() != '{'))) return 0;
        const bool block = value.front() == '{'; value.remove_prefix(1);
        if (!block) return sequence_raw_scalar(value, work) ? 1 : -1;
        for (;;) {
            const auto close = value.find('}');
            if (!sequence_raw_scalar(value.substr(0, close), work)) return -1;
            if (close != std::string_view::npos) return trim(value.substr(close + 1)).empty() ? 1 : -1;
            if (source.empty()) return -1;
            const auto end = source.find('\n');
            value = source.substr(0, end);
            source = end == std::string_view::npos ? std::string_view{} : source.substr(end + 1);
            if (value.ends_with('\r')) value.remove_suffix(1);
        }
    }
    return 0;
}

bool sequence_statement(std::string_view text, Sequence& graph, Work& work, bool number_terminated,
                        std::string_view raw_statement, bool raw_text_angle_cr) {
    std::string_view metadata;
    if (!sequence_initial_prefix_allowed(text, work, &metadata)) return false;
    if (!metadata.empty()) return !graph.open_group && sequence_metadata(text, metadata, work);
    auto folded = [](std::string value) {
        for (auto& ch : value) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch + ('a' - 'A'));
        return value;
    };
    Scanner note_scanner{text};
    const auto keyword = note_scanner.id();
    const bool standalone_keyword = keyword && (note_scanner.rest.empty() ||
        note_scanner.rest.front() == ' ' || note_scanner.rest.front() == '\t');
    const auto command = standalone_keyword ? folded(*keyword) : std::string{};
    bool command_message=false;
    if (command=="activate"||command=="deactivate"||command=="destroy") {
        auto operand=trim(note_scanner.rest);
        if (operand.starts_with("()")) operand=trim(operand.substr(2));
        if (!work.spend(64)) return false;
        command_message=std::ranges::any_of(kOperators,[&](const auto& op){return operand.starts_with(op.spelling);});
    }
    // Prepay the declaration/message discriminator before scanning or copying.
    // Existing declarations retain their allowance; database message IDs now
    // pay their actual extra INITIAL probe instead of acquiring a free scan.
    if (command == "database" && !work.spend(text.size() + 1)) return false;
    const auto database_operator = command == "database" ? message_operator(text, true) :
        std::pair<std::size_t, const Operator*>{std::string_view::npos, nullptr};
    const bool database_declaration = command == "database"&&!database_operator.second;
    if (graph.open_group && command != "participant" && command != "actor" && command != "end" && !database_declaration) return false;
    if (keyword && folded(*keyword) == "title" &&
        (standalone_keyword || note_scanner.rest.starts_with(":"))) {
        // The primary setter consumes one separator and preserves literal text.
        // Inline title data does not use YAML unquoting or message BR decoding.
        auto raw = Scanner::trim_front(raw_statement);
        std::size_t offset = 5;
        if (raw.size() > offset && raw[offset] == ':') ++offset;
        if (raw.size() <= offset || (raw[offset] != ' ' && raw[offset] != '\t')) return false;
        const auto value = raw.substr(offset + 1);
        if (!work.spend(value.size() * 6 + 32) ||
            value.find_first_of("#&`;\\\t") != std::string_view::npos ||
            value.find("\xef\xbb\xbf") != std::string_view::npos ||
            value.find("\xe2\x80\x8b") != std::string_view::npos) return false;
        for (auto at = value.find('<'); at != std::string_view::npos; at = value.find('<', at + 4))
            if (!value.substr(at).starts_with("<br>")) return false;
        const auto title = validated_plain_label(value);
        if (!title) return false;
        graph.title = *title;
        return true;
    }
    if (command == "box") {
        if (graph.open_group || !graph.open_fragments.empty() || graph.group_openings++ >= 64) return false;
        auto group = participant_group(note_scanner.rest, work, graph.wrap_default());
        if (!group) return false;
        graph.open_group = graph.groups.size();graph.has_wrapped_labels |= group->wrapped.has_value();
        graph.groups.push_back(std::move(*group));
        return true;
    }
    if (command == "end" && graph.open_group) {
        if (!trim(note_scanner.rest).empty()) return false;
        if (graph.groups.back().actors.empty()) graph.groups.pop_back();
        graph.open_group.reset();return true;
    }
    // The existing paid source ownership walk observes physical CR before
    // TXT/AS operands are trimmed or overridden. Whole-source preprocessing
    // would make it a statement LF; decoded Metadata CR is not a source byte.
    // Plain title/BoxCaption have already taken their unchanged branches.
    if (raw_text_angle_cr) return false;
    if (command == "destroy"&&!command_message) {
        const auto id = participant_name(note_scanner.rest, IdentityContext::Declaration);
        if (!id) return false;
        const auto actor = graph.find_participant(*id);
        if (!actor || !graph.participants[*actor].alive || graph.participants[*actor].destroyed) return false;
        graph.pending_destroy = *actor;
        return true;
    }
    if ((command == "activate" || command == "deactivate")&&!command_message) {
        const auto id = participant_name(note_scanner.rest, IdentityContext::Declaration);
        if (!id) return false;
        const auto actor = graph.participant(*id);
        return actor && graph.activation(*actor, command == "activate");
    }
    if (command == "loop" || command == "opt" || command == "alt" || command == "par" ||
        command == "critical" || command == "break" || command == "rect" || command == "par_over") {
        if (graph.fragments.size() >= 64 || graph.open_fragments.size() >= 16) return false;
        auto label = trim(note_scanner.rest).empty() ? std::optional<std::string>{""} : sequence_label(note_scanner.rest, work, LabelSyntax::Text);
        if (!label) return false;
        const auto index = graph.fragments.size();
        graph.fragments.push_back({command, std::move(*label), graph.open_fragments.size(), {}});
        graph.open_fragments.push_back(index);
        return graph.event(EventKind::Open, index);
    }
    if (command == "else" || command == "and" || command == "option") {
        if (graph.open_fragments.empty()) return false;
        const auto index = graph.open_fragments.back();
        auto& fragment = graph.fragments[index];
        if ((command == "else" && fragment.kind != "alt") || (command == "and" && fragment.kind != "par") ||
            (command == "option" && fragment.kind != "critical")) return false;
        auto label = trim(note_scanner.rest).empty() ? std::optional<std::string>{""} : sequence_label(note_scanner.rest, work, LabelSyntax::Text);
        if (!label || !graph.event(EventKind::Divide, index, fragment.dividers.size())) return false;
        fragment.dividers.push_back(std::move(*label));
        return true;
    }
    if (command == "end") {
        if (!trim(note_scanner.rest).empty() || graph.open_fragments.empty()) return false;
        const auto index = graph.open_fragments.back();
        graph.open_fragments.pop_back();
        return graph.event(EventKind::Close, index);
    }
    if (standalone_keyword && folded(*keyword) == "autonumber") {
        auto parameters = trim(note_scanner.rest);
        if (parameters.empty()) { graph.number_visible = true; return true; }
        if (folded(std::string(parameters)) == "off") { graph.number_visible = false; return true; }
        if (!number_terminated) return false;
        const auto space = parameters.find(' ');
        const auto first = number_parameter(parameters.substr(0, space), work);
        if (!first) return false;
        double step = 1;
        if (space != std::string_view::npos) {
            parameters = trim(parameters.substr(space + 1));
            const auto second = number_parameter(parameters, work);
            if (!second) return false;
            step = *second;
        }
        if (*first != 0) graph.number_next = *first;
        if (step != 0) graph.number_step = step;
        graph.number_visible = true;
        return true;
    }
    if (standalone_keyword && folded(*keyword) == "note") {
        const auto position = note_scanner.id();
        if (!position || graph.notes.size() >= 64) return false;
        Note note{{}, {}};
        const auto placement = folded(*position);
        if (placement == "left" || placement == "right") {
            const auto of = note_scanner.id();
            if (!of || folded(*of) != "of") return false;
            note.position = placement == "left" ? NotePosition::Left : NotePosition::Right;
        } else if (placement != "over") return false;
        while (true) {
            note_scanner.space();
            const auto comma = delimiter(note_scanner.rest, ','), colon = delimiter(note_scanner.rest, ':');
            if (colon == std::string_view::npos) return false;
            const auto end = std::min(comma, colon);
            const auto identity = note_scanner.rest.substr(0, end);
            if (!sequence_initial_prefix_allowed(identity, work)) return false;
            const auto id = participant_name(identity);
            if (!id) return false;
            const auto index = graph.participant(*id);
            // An already destroyed actor still owns its Note anchor. Only a
            // participant that has not yet been born lacks a usable identity.
            if (!index || (!graph.participants[*index].alive && !graph.participants[*index].destroyed) ||
                std::ranges::find(note.actors, *index) != note.actors.end()) return false;
            note.actors.push_back(*index); note_scanner.rest.remove_prefix(end);
            if (!note_scanner.take(",")) break;
            if (note.actors.size() >= (note.position == NotePosition::Over ? 2u : 1u)) return false;
        }
        if (!note_scanner.take(":")) return false;
        auto text_label = sequence_source_label(note_scanner.rest, graph.wrap_default(), work);
        if (!text_label) return false;
        note.text = std::move(text_label->text); note.wrapped = std::move(text_label->wrapped);
        graph.has_wrapped_labels |= note.wrapped.has_value();
        graph.notes.push_back(std::move(note));
        return graph.event(EventKind::Note, graph.notes.size() - 1);
    }
    const bool creating = command == "create";
    auto declaration_role = command;
    if (creating) {
        const auto role = note_scanner.id();
        if (!role) return false;
        declaration_role = folded(*role);
        if (declaration_role != "participant" && declaration_role != "actor") return false;
        if (!note_scanner.rest.empty() && note_scanner.rest.front() != ' ' && note_scanner.rest.front() != '\t') return false;
    }
    if (creating || command == "participant" || command == "actor" || database_declaration) {
        const auto [identity, alias_text] = declaration_parts(note_scanner.rest);
        auto id = participant_name(identity, IdentityContext::Declaration);
        auto role = database_declaration ? Role::Database : declaration_role == "actor" ? Role::Actor : Role::Participant;
        std::optional<RoleDeclaration> metadata;
        if (!id && !database_declaration && note_scanner.rest.find("@{") != std::string_view::npos) {
            metadata = role_declaration(note_scanner.rest, role, work, graph.wrap_default());
            if (!metadata) return false;
            id = std::move(metadata->id); role = metadata->role;
        }
        if (!id) return false;
        // participant() returns an existing index or appends one new last item.
        // This includes implicit identities without a second linear lookup.
        const auto prior_count = graph.participants.size();
        const auto index = graph.participant(std::move(*id));
        if (!index || (creating && *index < prior_count)) return false;
        const bool known = *index < prior_count;
        if (graph.open_group) {
            for (std::size_t n = 0; n < graph.groups.size(); ++n) {
                if (!work.spend(graph.groups[n].actors.size() + 1)) return false;
                if (n != *graph.open_group && std::ranges::find(graph.groups[n].actors, *index) != graph.groups[n].actors.end()) return false;
            }
            auto& actors = graph.groups[*graph.open_group].actors;
            if (std::ranges::find(actors, *index) == actors.end()) actors.push_back(*index);
        }
        auto& participant = graph.participants[*index];
        if (creating) {
            // The source setter owns one pending signal. A later declaration
            // replaces that signal; the superseded participant is ordinary.
            if (graph.pending_create) {
                auto& previous = graph.participants[*graph.pending_create];
                previous.created = false;
                previous.alive = true;
            }
            participant.created = true; participant.alive = false;
            graph.pending_create = *index;
        }
        const bool has_description = metadata ? metadata->alias.has_value() : alias_text.has_value();
        // JS actor metadata assigns/checks its Box before this null-description
        // return. The separately supported database keyword keeps its contract.
        if (!database_declaration && known && !has_description) return true;
        if (database_declaration && participant.explicit_role && participant.role != role) return false;
        participant.role = role;
        participant.explicit_role = true;
        if (!has_description) return true;
        auto label = metadata ? std::optional<SequenceSourceLabel>{SequenceSourceLabel{
            std::move(*metadata->alias), std::move(metadata->wrapped)}} :
            sequence_source_label(*alias_text, graph.wrap_default(), work);
        if (!label) return false;
        if (database_declaration && participant.explicit_label && (participant.text != label->text ||
            participant.wrapped.has_value() != label->wrapped.has_value())) return false;
        const bool removes_wrap = participant.wrapped.has_value() && !label->wrapped.has_value();
        if (removes_wrap && !work.spend(graph.participants.size() + graph.messages.size() +
                                       graph.notes.size() + graph.groups.size())) return false;
        participant.text = std::move(label->text); participant.wrapped = std::move(label->wrapped);
        if (removes_wrap) {
            // A superseded wrapped alias must not select wrapped layout after
            // every final actor/message/note/group is now unwrapped.
            const auto wrapped = [](const auto& item) { return item.wrapped.has_value(); };
            graph.has_wrapped_labels = std::ranges::any_of(graph.participants, wrapped) ||
                std::ranges::any_of(graph.messages, wrapped) || std::ranges::any_of(graph.notes, wrapped) ||
                std::ranges::any_of(graph.groups, wrapped);
        } else graph.has_wrapped_labels |= participant.wrapped.has_value();
        participant.explicit_label = true;
        return true;
    }
    const auto [arrow_at, selected] = command == "database" ? database_operator : message_operator(text);
    if (!selected) return false;
    auto left = trim(text.substr(0, arrow_at));
    const bool central_from = left.ends_with("()");
    if (central_from) left = trim(left.substr(0, left.size() - 2));
    auto from_id = participant_name(left, IdentityContext::Message);
    if (!from_id) return false;
    Scanner scanner{text.substr(arrow_at + selected->spelling.size())};
    scanner.space();
    const int activation = scanner.take("+") ? 1 : scanner.take("-") ? -1 : 0;
    scanner.space();
    const bool central_to = scanner.take("()");
    scanner.space();
    const auto colon = delimiter(scanner.rest, ':');
    if (colon == std::string_view::npos) return false;
    const auto identity = scanner.rest.substr(0, colon);
    if (!sequence_initial_prefix_allowed(identity, work)) return false;
    auto to_id = participant_name(identity, IdentityContext::Message);
    if (!to_id) return false;
    scanner.rest.remove_prefix(colon + 1);
    auto message = trim(scanner.rest).empty() ? std::optional<SequenceSourceLabel>{SequenceSourceLabel{}} :
        sequence_source_label(scanner.rest, graph.wrap_default(), work);
    if (!message || graph.messages.size() >= 128) return false;
    const auto from = graph.participant(std::move(*from_id));
    const auto to = graph.participant(std::move(*to_id));
    if (!from || !to) return false;
    const bool consumes_create=graph.pending_create.has_value();
    if (consumes_create) {
        const auto actor = *graph.pending_create;
        if (actor != *to || !graph.event(EventKind::Create, actor)) return false;
        graph.participants[actor].alive = true; graph.pending_create.reset();
    }
    if (!graph.participants[*from].alive || !graph.participants[*to].alive ||
        (!consumes_create&&graph.pending_destroy&&*graph.pending_destroy!=*from&&*graph.pending_destroy!=*to)) return false;
    std::string number;
    if (graph.number_visible || graph.configured_number_visible.value_or(false)) {
        auto display = number_display(graph.number_next, work);
        if (!display) return false;
        number = std::move(*display);
    }
    graph.has_wrapped_labels |= message->wrapped.has_value();
    graph.messages.push_back({*from, *to, std::move(message->text), selected->dashed, selected->kind, central_from, central_to, std::move(number), std::move(message->wrapped)});
    // The pinned renderer advances every signal, even while numbering is hidden.
    graph.number_next = std::round((graph.number_next + graph.number_step) * 100) / 100;
    if (!graph.event(EventKind::Message, graph.messages.size() - 1) ||
        (activation && !graph.activation(activation > 0 ? *to : *from, activation > 0))) return false;
    if (!consumes_create&&graph.pending_destroy) {
        const auto actor = *graph.pending_destroy;
        if (!graph.event(EventKind::Destroy, actor)) return false;
        graph.participants[actor].alive = false; graph.participants[actor].destroyed = true;
        // Destruction ends visibility; a checked trailing close still owns its logical depth.
        graph.pending_destroy.reset();
    }
    return true;
}

bool fragment_line(std::string_view statement, Work& work) {
    if (!work.spend(statement.size()*2+128)) return false;
    statement=trim(statement);
    std::size_t end=0;
    while (end<statement.size()&&end<9&&id_char(statement[end])) ++end;
    if (end<statement.size()&&statement[end]!=' '&&statement[end]!='\t') return false;
    constexpr std::string_view words[]{"loop","rect","opt","alt","else","par","and","critical","option","break","end"};
    for (const auto word:words) {
        if (end!=word.size()) continue;
        bool equal=true;
        for (std::size_t n=0;n<end;++n) {
            const char ch=statement[n];
            if ((ch>='A'&&ch<='Z'?ch+('a'-'A'):ch)!=word[n]) {equal=false;break;}
        }
        if (equal) return true;
    }
    return false;
}

enum class StatementSyntax { Identity, Declaration, Text };
StatementSyntax statement_syntax(std::string_view statement) {
    statement=Scanner::trim_front(statement);
    std::size_t end=0;
    while (end<statement.size()&&end<12&&id_char(statement[end])) ++end;
    if (end<statement.size()&&statement[end]!=' '&&statement[end]!='\t'&&statement[end]!=':')
        return StatementSyntax::Identity;
    auto is=[&](std::string_view word) {
        if (word.size()!=end) return false;
        for (std::size_t n=0;n<end;++n) {
            const char ch=statement[n];
            if ((ch>='A'&&ch<='Z'?ch+('a'-'A'):ch)!=word[n]) return false;
        }
        return true;
    };
    if (is("participant")||is("actor")||is("database")||is("create"))
        return StatementSyntax::Declaration;
    constexpr std::string_view captions[]{"box","loop","rect","opt","alt","else","par",
        "par_over","and","critical","option","break","end","title","acctitle","accdescr"};
    for (const auto word:captions) if (is(word)) return StatementSyntax::Text;
    return StatementSyntax::Identity;
}

std::optional<Sequence> parse(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    Sequence diagram;
    bool have_header = false;
    bool have_frontmatter = false;
    while (!source.empty()) {
        if (!have_header) {
            auto first = source.substr(0, source.find('\n'));
            if (first.ends_with('\r')) first.remove_suffix(1);
            first = trim(first);
            if (first.starts_with("%%{")) {
                const auto prefix = static_cast<std::size_t>(first.data() - source.data());
                if (!work.spend(prefix)) return {};
                source.remove_prefix(prefix);
                if (!sequence_directive(source, diagram, work)) return {};
                continue;
            }
        }
        const auto end = source.find('\n');
        auto line = source.substr(0, end);
        source = end == std::string_view::npos ? std::string_view{} : source.substr(end + 1);
        if (line.ends_with('\r')) line.remove_suffix(1);
        if (!have_header && line == "---") {
            if (have_frontmatter || !work.spend(line.size() + 1)) return {};
            // Reconstruct the full source only to let the shared envelope
            // reader locate the closing fence; the caller keeps its original
            // view for the body offset.
            std::string full;
            full.reserve(line.size() + 1u + source.size());
            full.append(line); full.push_back('\n'); full.append(source);
            const auto envelope = parse_envelope(full, work);
            if (!envelope || !envelope->has_frontmatter ||
                !sequence_frontmatter(envelope->raw_frontmatter, diagram, work)) return {};
            const auto body_offset = static_cast<std::size_t>(envelope->raw_body.data() - full.data());
            const auto opening_size = line.size() + 1u;
            if (body_offset < opening_size) return {};
            source = source.substr(body_offset - opening_size);
            have_frontmatter = true;
            continue;
        }
        const auto* raw_line_end = line.data() + line.size();
        const auto last_nonspace = line.find_last_not_of(" \t");
        // The primary whitespace rule can consume a command newline after
        // trailing blanks; keep that unresolved spelling as complete source.
        const bool line_number_terminated = last_nonspace == std::string_view::npos ||
            last_nonspace + 1 == line.size();
        line = trim(line);
        if (line.starts_with("%%{")) {
            // Source presentation setters may follow the header while the
            // diagram is still in its prelude. Reuse the complete directive
            // reader before any label or group has acquired source semantics.
            if (!have_header || !diagram.participants.empty() || !diagram.events.empty() ||
                !diagram.groups.empty()) return {};
            auto directive = line;
            if (!sequence_directive(directive, diagram, work) || !directive.empty()) return {};
            continue;
        }
        if (line.empty() || line.starts_with("%%")) continue;
        if (have_header) {
            const int accessibility = sequence_accessibility(line, source, work);
            if (accessibility < 0) return {};
            if (accessibility > 0) { if (diagram.open_group) return {}; continue; }
        }
        if (line.front() == '#') {
            const auto atom = sequence_atom_size(line, work);
            if (!atom) return {};
            // Complete atoms are data, including unsupported ones. Only an
            // unprotected standalone hash line is a comment in this profile.
            if (*atom == 0) {
                if (!work.spend(line.size())) return {};
                continue;
            }
        }
        // Statement ownership matters: quotes protect ID syntax, while
        // TXT/LINE quotes are literal data and cannot hide a semicolon.
        if (!work.spend(line.size()*2+128)) return {};
        bool quoted = false;
        bool escaped = false;
        bool config = false;
        std::size_t start = 0;
        auto syntax=statement_syntax(line);
        bool text=syntax==StatementSyntax::Text;
        bool metadata_text=sequence_metadata_prefix(line);
        bool raw_text_angle = false, raw_text_cr = false;
        for (std::size_t at = 0; at <= line.size(); ++at) {
            auto ch = at == line.size() ? '\0' : line[at];
            if (at == line.size() && (quoted || config || escaped)) return {};
            // Primary CONFIG ends at its first literal close, including when
            // that byte occurs inside a scalar; role_fields checks the body.
            if (config) {
                if (ch=='}') config=false;
                continue;
            }
            if (!text) {
                if (escaped) {escaped=false;continue;}
                if (quoted&&ch=='\\') {escaped=true;continue;}
                if (ch=='"') {quoted=!quoted;continue;}
                if (!quoted) {
                    if (syntax==StatementSyntax::Declaration&&ch=='@'&&
                        at+1<line.size()&&line[at+1]=='{') {config=true;++at;continue;}
                    if (syntax==StatementSyntax::Declaration&&at>start&&
                        (line[at-1]==' '||line[at-1]=='\t')&&at+2<line.size()&&
                        (ch=='a'||ch=='A')&&(line[at+1]=='s'||line[at+1]=='S')&&
                        (line[at+2]==' '||line[at+2]=='\t')) text=true;
                    else if (ch==':') text=true;
                }
            }
            // Reuse this already prepaid byte walk. CONFIG bytes continue
            // above; JSON-decoded metadata CR never enters these source bits.
            if (text) {
                raw_text_angle |= ch == '<';
                raw_text_cr |= ch == '\r';
            }
            // LINE control captions own their comment tail. Quote bytes in a
            // caption are data, so a quote in that tail cannot reopen a string.
            if (ch=='#'&&(metadata_text||fragment_line(line.substr(start,at-start),work))) {
                const auto atom=sequence_atom_size(line.substr(at),work);
                if (!atom) return {};
                if (*atom==0) {line=line.substr(0,at);ch='\0';}
            }
            if (!quoted && ch == '#') {
                const auto atom = sequence_atom_size(line.substr(at), work);
                if (!atom) return {};
                if (*atom != 0) { at += *atom - 1; continue; }
                // INITIAL may begin again after a semicolon. Only a blank
                // current statement prefix owns this standalone comment.
                if (!work.spend(at - start + 1)) return {};
                if (trim(line.substr(start, at - start)).empty()) {
                    const auto tail = line.substr(at);
                    if (!work.spend(tail.size())) return {};
                    // Primary preprocessing turns CR into a physical LF; do
                    // not swallow possible business after that unreviewed split.
                    if (tail.find('\r') != std::string_view::npos) return {};
                    line = line.substr(0, at); ch = '\0';
                }
            }
            if (at != line.size() && (ch != ';' || quoted)) continue;
            if (quoted || config || escaped) return {};
            const auto raw_statement = line.substr(start, at - start);
            const bool raw_text_angle_cr = raw_text_angle && raw_text_cr;
            raw_text_angle = false; raw_text_cr = false;
            const auto statement = trim(raw_statement);
            const auto last_token = raw_statement.find_last_not_of(" \t");
            const bool number_terminated = at == line.size() ? line_number_terminated :
                last_token != std::string_view::npos && last_token + 1 < raw_statement.size() &&
                raw_statement[last_token + 1] == ' ';
            start = at + 1;
            syntax=statement_syntax(line.substr(std::min(start,line.size())));
            text=syntax==StatementSyntax::Text;
            metadata_text=sequence_metadata_prefix(Scanner::trim_front(line.substr(std::min(start,line.size()))));
            if (statement.empty()) continue;
            if (!have_header) {
                if (statement != "sequenceDiagram") return {};
                have_header = true;
            } else {
                const auto complete_raw = at == line.size() ?
                    std::string_view(raw_statement.data(), static_cast<std::size_t>(raw_line_end - raw_statement.data())) : raw_statement;
                if (!sequence_statement(statement, diagram, work, number_terminated, complete_raw, raw_text_angle_cr)) return {};
            }
        }
    }
    if (!have_header || !diagram.open_fragments.empty() || diagram.open_group ||
        diagram.pending_create) return {};
    for (auto& group : diagram.groups) {
        if (!work.spend(group.actors.size() * 6)) return {};
        std::ranges::sort(group.actors);
        for (std::size_t n = 1; n < group.actors.size(); ++n)
            if (group.actors[n] != group.actors[n - 1] + 1) return {};
    }
    // Resolve default presentation after every declaration. A later explicit
    // alias replaces the default label, while the raw ID remains its identity.
    for (auto& participant : diagram.participants) if (!participant.explicit_label) {
        if (diagram.wrap_default()) {
            auto label = sequence_source_label(participant.id, true, work, LabelSyntax::Identity);
            if (!label) return {};
            participant.text = std::move(label->text); participant.wrapped = std::move(label->wrapped);
            diagram.has_wrapped_labels = true;
        } else {
            auto display = sequence_display_row(participant.id, work);
            if (!display) return {};
            participant.text = std::move(*display);
        }
    }
    return diagram;
}


struct Box { int x; int y; int width; int height = 5; };
bool node_box(Canvas& canvas, const Box& box, const Participant& node,
              std::optional<NoteTextAlign> note_align = {}) {
    for (int y = 0; y < box.height; ++y) for (int x = 0; x < box.width; ++x)
        if (!canvas.put({box.x + x, box.y + y}, " ")) return false;
    for (int x = 1; x < box.width - 1; ++x)
        if (!canvas.put({box.x + x, box.y}, "─") || !canvas.put({box.x + x, box.y + box.height - 1}, "─")) return false;
    for (int y = 1; y < box.height - 1; ++y)
        if (!canvas.put({box.x, box.y + y}, "│") || !canvas.put({box.x + box.width - 1, box.y + y}, "│")) return false;
    if (!canvas.put({box.x, box.y}, "┌") || !canvas.put({box.x + box.width - 1, box.y}, "┐") ||
        !canvas.put({box.x, box.y + box.height - 1}, "└") ||
        !canvas.put({box.x + box.width - 1, box.y + box.height - 1}, "┘")) return false;
    if (note_align && *note_align != NoteTextAlign::Center) {
        // Pay row storage, display-width strings and glyph conversion before
        // allocation. The absent/center path retains its original fees below.
        if (!canvas.work.spend((node.text.size() + 1) * 64)) return false;
        const auto lines = label_rows(node.text);
        if (box.height < 2 || lines.size() > static_cast<std::size_t>(box.height - 2)) return false;
        int y = box.y + (box.height - static_cast<int>(lines.size())) / 2;
        for (const auto line : lines) {
            const int width = ftxui::string_width(std::string(line));
            if (width < 0 || width > box.width - 4) return false;
            const int x = *note_align == NoteTextAlign::Left ? box.x + 2 : box.x + box.width - 2 - width;
            if (!canvas.text({x, y++}, line)) return false;
        }
        return true;
    }
    const auto rows = static_cast<int>(label_rows(node.text).size());
    return canvas.multiline({box.x, box.y + (box.height - rows) / 2}, node.text, box.width);
}

int role_height(Role role) {
    return role == Role::Participant ? 0 : role == Role::Actor || role == Role::Boundary || role == Role::Queue ? 3 : 4;
}
bool new_role_figure(Canvas& canvas, const Box& box, Role role) {
    const int center = box.x + box.width / 2, bottom = box.y;
    auto row = [&](int dx, int dy, std::string_view value) {
        // Constant text conversion/storage is prepaid; Canvas charges each
        // real glyph put, including required blank contour cells.
        return canvas.work.spend(value.size() * 2 + 1) && canvas.text({center + dx, bottom + dy}, value);
    };
    switch (role) {
        case Role::Boundary:
            return row(-3,-3,"│ ╭─╮") && row(-3,-2,"├─┤ │") && row(-3,-1,"│ ╰─╯");
        case Role::Control:
            return row(-1,-4,"◀┐ ") && row(-1,-3,"╭┴╮") && row(-1,-2,"│ │") && row(-1,-1,"╰─╯");
        case Role::Entity:
            return row(-1,-4,"╭─╮") && row(-1,-3,"│ │") && row(-1,-2,"╰─╯") && row(-2,-1,"─────");
        case Role::Database:
            return row(-2,-4,"╭───╮") && row(-2,-3,"╰───╯") && row(-2,-2,"│   │") && row(-2,-1,"╰───╯");
        case Role::Collections:
            return row(-1,-4,"┌───┐") && row(-2,-3,"┌───┐│") && row(-2,-2,"│   │┘") && row(-2,-1,"└───┘");
        case Role::Queue:
            return row(-3,-3,"╭───┬─╮") && row(-3,-2,"│   │ │") && row(-3,-1,"╰───┴─╯");
        default: return false;
    }
}

std::optional<std::vector<std::string>> layout(Sequence& diagram, Work& work) {
    if (!work.spend(diagram.events.size() + diagram.participants.size() + diagram.messages.size() +
                    diagram.notes.size() + diagram.fragments.size() + diagram.groups.size())) return {};
    if (diagram.participants.empty() && diagram.fragments.empty()) return std::vector<std::string>{""};
    if (diagram.has_wrapped_labels) for (auto& actor : diagram.participants) {
        if (!work.spend()) return {};
        if (!actor.wrapped) continue;
        auto packed = sequence_pack_label(*actor.wrapped, 12, {}, work);
        if (!packed) return {};
        actor.text = std::move(*packed);
    }
    std::vector<std::size_t> active_depth(diagram.participants.size()),max_depth(diagram.participants.size());
    bool has_activation = false;
    for (const auto& event : diagram.events) {
        if (!work.spend()) return {};
        if (event.kind == EventKind::Activate) {
            has_activation = true;
            max_depth[event.index] = std::max(max_depth[event.index], ++active_depth[event.index]);
        } else if (event.kind == EventKind::Deactivate) --active_depth[event.index];
        // Destruction ends physical strips without erasing logical activation depth.
    }
    const auto max_span = max_depth.empty() ? 0 : static_cast<int>(*std::ranges::max_element(max_depth)) * 2;
    std::vector<std::string> labels;
    for (std::size_t n = 0; n < diagram.messages.size(); ++n) {
        std::string label = diagram.messages[n].number.empty() ? "" : diagram.messages[n].number + ". ";
        label += diagram.messages[n].text;
        std::size_t glyphs = 0, cells = 0;
        const auto rows = label_rows(label);
        for (const auto row : rows) {
            if (!work.spend(row.size())) return {};
            glyphs += ftxui::Utf8ToGlyphs(std::string(row)).size();
            cells += static_cast<std::size_t>(ftxui::string_width(std::string(row)));
        }
        if (glyphs + rows.size() - 1 > kLabelLimit || cells > kLabelLimit) return {};
        labels.push_back(std::move(label));
    }
    std::vector<Box> boxes;
    std::vector<int> centers;
    int longest = 0;
    bool last_self = false;
    int figure_rows = std::ranges::any_of(diagram.participants, [](const Participant& actor) { return actor.role == Role::Actor; }) ? 3 : 0;
    bool has_new_role = false;
    int header_height = diagram.participants.empty() ? 0 : 3;
    for (const auto& participant : diagram.participants) {
        header_height = std::max(header_height, static_cast<int>(label_rows(participant.text).size()) + 2);
        if (participant.role != Role::Participant && participant.role != Role::Actor) {
            if (!work.spend()) return {};
            has_new_role = true; figure_rows = std::max(figure_rows, role_height(participant.role));
        }
    }
    header_height += figure_rows;
    for (std::size_t n = 0; n < diagram.messages.size(); ++n) {
        const auto& message = diagram.messages[n];
        const auto& label = labels[n];
        // Effective true labels cannot enlarge their own source host.
        if (!message.wrapped) longest = std::max(longest, label_width(label));
        last_self |= message.from == message.to && message.from + 1 == diagram.participants.size();
    }
    // The reviewed wrapped-group profile has one complete contiguous unit.
    // Complex notes/activation/groups need an independently specified host.
    if (diagram.has_wrapped_labels && !diagram.groups.empty()) {
        if (diagram.groups.size() != 1 || !diagram.notes.empty() || !diagram.fragments.empty() || has_activation ||
            diagram.groups.front().actors.size() != diagram.participants.size()) return {};
        const int source_gap = std::max(7, longest + 4);
        int content = 0;
        for (std::size_t n = 0; n < diagram.participants.size(); ++n) {
            if (!work.spend(diagram.participants[n].text.size() * 3 + 1) || diagram.groups.front().actors[n] != n) return {};
            content += std::max(7, label_width(diagram.participants[n].text) + 4);
            if (n + 1 < diagram.participants.size()) content += source_gap;
        }
        int left = 0;
        for (std::size_t n = 0; n < diagram.participants.size(); ++n) {
            const int node = std::max(7, label_width(diagram.participants[n].text) + 4);
            const int center = left + node / 2;
            for (std::size_t m = 0; m < diagram.messages.size(); ++m) {
                if (!work.spend()) return {};
                const auto& message = diagram.messages[m];
                if (!message.wrapped && message.from == n && message.to == n)
                    content = std::max(content, center + std::max(4, label_width(labels[m]) + 3) + 2);
            }
            left += node + source_gap;
        }
        auto& group = diagram.groups.front();
        if (group.wrapped) {
            auto packed = sequence_pack_label(*group.wrapped, std::max(12, content), {}, work);
            if (!packed) return {};
            group.title = std::move(*packed);
        }
    }
    struct GroupGeometry {Box frame;std::vector<std::string> captions;};
    std::vector<GroupGeometry> groups(diagram.groups.size());
    std::vector<std::optional<std::size_t>> actor_group(diagram.participants.size());
    int header_offset = 0;
    for (std::size_t n = 0; n < diagram.groups.size(); ++n) {
        const auto& group = diagram.groups[n];
        if (group.actors.empty() || !work.spend(group.actors.size() + 1)) return {};
        for (const auto actor : group.actors) {
            if (actor >= actor_group.size() || actor_group[actor]) return {};
            actor_group[actor] = n;
        }
        if (!group.color.empty()) groups[n].captions.push_back("color: " + group.color);
        groups[n].captions.push_back(group.title);
        int bottom = 0;
        for (const auto& caption : groups[n].captions) {
            if (!work.spend(caption.size() + 1)) return {};
            bottom += static_cast<int>(label_rows(caption).size()) + 1;
        }
        header_offset = std::max(header_offset, bottom + 2);
    }
    header_height += header_offset;
    const auto gap = std::max(7, longest + 4 + (has_activation ? max_span : 0));
    int width = 0;
    if (groups.empty()) {
        bool local_gaps = has_new_role;
        bool fragment_events=false,create_events=false;
        if (local_gaps) for (const auto& event : diagram.events) {
            if (!work.spend()) return {};
            if (event.kind==EventKind::Open||event.kind==EventKind::Divide||event.kind==EventKind::Close) fragment_events=true;
            else if (event.kind==EventKind::Create) create_events=true;
            else if (event.kind!=EventKind::Message&&event.kind!=EventKind::Note) local_gaps=false;
        }
        if (local_gaps&&(fragment_events||create_events)) {
            // Preserve every successful conservative placement. Creation uses
            // the same fixed actor columns; it adds no activation-strip width.
            // Reuse the existing local gaps only for a measured excess.
            int conservative=0,last_center=0;
            for (const auto& participant:diagram.participants) {
                if (!work.spend(participant.text.size()*3+1)) return {};
                const int node_width=std::max(7,label_width(participant.text)+4);
                last_center=conservative+node_width/2;
                conservative+=node_width+gap;
            }
            if (!diagram.participants.empty()) conservative-=gap;
            if (last_self) conservative=std::max(conservative,last_center+longest+6);
            std::size_t nesting=0;
            for (const auto& fragment:diagram.fragments) {
                if (!work.spend()) return {};
                nesting=std::max(nesting,fragment.depth+1);
            }
            local_gaps=conservative+static_cast<int>(nesting*4)>kWidthLimit;
        }
        if (local_gaps) {
            const auto count = diagram.participants.size();
            if (!work.spend(5 * count + (count ? count - 1 : 0))) return {};
            boxes.reserve(count); centers.reserve(count);
            std::vector<int> gaps(count ? count - 1 : 0, 7);
            for (const auto& participant : diagram.participants) {
                if (!work.spend(participant.text.size() * 3 + 1)) return {};
                boxes.push_back({0, figure_rows, std::max(7, label_width(participant.text) + 4),
                                 static_cast<int>(label_rows(participant.text).size()) + 2});
            }
            for (std::size_t n = 0; n < diagram.messages.size(); ++n) {
                if (diagram.messages[n].wrapped) continue;
                if (!work.spend(labels[n].size() * 3 + 1)) return {};
                const auto& message = diagram.messages[n];
                const auto first = std::min(message.from, message.to);
                if (first + 1 >= count) continue; // Existing last-self extent remains below.
                const int length = label_width(labels[n]); // Finalized number prefix included.
                const int needed = message.from == message.to ? std::max(4, length + 3) + 2 : length + 4;
                const int halves = boxes[first].width - boxes[first].width / 2 + boxes[first + 1].width / 2;
                gaps[first] = std::max(gaps[first], needed - halves);
            }
            for (std::size_t n = 0; n < count; ++n) {
                if (!work.spend() || boxes[n].width > kWidthLimit - width) return {};
                boxes[n].x = width; centers.push_back(width + boxes[n].width / 2); width += boxes[n].width;
                if (n + 1 < count) {
                    if (gaps[n] > kWidthLimit - width) return {};
                    width += gaps[n];
                }
            }
        } else {
            // Preserve the existing no-group geometry and Work path.
            for (const auto& participant : diagram.participants) {
                const auto node_width = std::max(7, label_width(participant.text) + 4);
                boxes.push_back({width, figure_rows, node_width, static_cast<int>(label_rows(participant.text).size()) + 2});
                centers.push_back(width + node_width / 2);
                width += node_width + gap;
            }
            width = boxes.empty() ? 8 : width - gap;
        }
        if (last_self) width = std::max(width, centers.back() + longest + 6 + (has_activation ? max_span : 0));
        if (has_activation) for (std::size_t n = 0; n < centers.size(); ++n) {
            if (!work.spend()) return {};
            width = std::max(width, centers[n] + static_cast<int>(max_depth[n] * 2) + 2);
        }
    } else {
        boxes.resize(diagram.participants.size());centers.resize(boxes.size());
        for (std::size_t first = 0; first < boxes.size();) {
            const auto group = actor_group[first];
            const auto last = group ? diagram.groups[*group].actors.back() : first;
            if (group && diagram.groups[*group].actors.front() != first) return {};
            int cursor = 0;
            for (std::size_t n = first; n <= last; ++n) {
                if (!work.spend()) return {};
                const auto& participant = diagram.participants[n];
                const int node_width = std::max(7, label_width(participant.text) + 4);
                boxes[n] = {cursor, header_offset + figure_rows, node_width, static_cast<int>(label_rows(participant.text).size()) + 2};
                centers[n] = cursor + node_width / 2;cursor += node_width + gap;
            }
            int left = 0, right = cursor - gap - 1;
            for (std::size_t n = first; n <= last; ++n) {
                if (!work.spend()) return {};
                if (max_depth[n]) right = std::max(right, centers[n] + static_cast<int>(max_depth[n] * 2) + 1);
            }
            for (std::size_t n = 0; n < diagram.messages.size(); ++n) {
                if (!work.spend()) return {};
                const auto& message = diagram.messages[n];
                if (message.wrapped || message.from != message.to || message.from < first || message.from > last) continue;
                const int span = max_depth[message.from] ? static_cast<int>((max_depth[message.from] - 1) * 2) : 0;
                right = std::max(right, centers[message.from] + span + std::max(4, label_width(labels[n]) + 3) + 1);
            }
            for (const auto& note : diagram.notes) {
                if (!work.spend(note.actors.size() + 1)) return {};
                if (!std::ranges::all_of(note.actors, [&](std::size_t actor) {return actor >= first && actor <= last;})) continue;
                const int a = centers[note.actors.front()], b = centers[note.actors.back()];
                const int low = std::min(a, b), high = std::max(a, b);
                const int note_width = std::max({7, label_width(note.text) + 4, note.position == NotePosition::Over ? high - low + 5 : 0});
                const int x = note.position == NotePosition::Left ? a - note_width - 3 :
                    note.position == NotePosition::Right ? a + 3 + static_cast<int>(max_depth[note.actors.front()] * 2) :
                    (low + high - note_width + 1) / 2;
                left = std::min(left, x);right = std::max(right, x + note_width - 1);
            }
            const int content_width = right - left + 1;
            int unit_width = content_width + (group ? 6 : 0);
            if (group) for (const auto& caption : groups[*group].captions) {
                if (!work.spend(caption.size() + 1)) return {};
                unit_width = std::max(unit_width, label_width(caption) + 4);
            }
            const int shift = width + (unit_width - content_width) / 2 - left;
            for (std::size_t n = first; n <= last; ++n) {
                boxes[n].x += shift;centers[n] += shift;
            }
            if (group) groups[*group].frame = {width, 0, unit_width, 0};
            width += unit_width + gap;first = last + 1;
        }
        width -= gap;
    }
    // Centers are now fixed by headers and finalized false messages. The
    // one adjacent interval owns forward/reverse/nonadjacent/self text alike.
    if (diagram.has_wrapped_labels) {
        bool independent_last_self = false;
        for (const auto& message : diagram.messages) {
            if (!work.spend()) return {};
            independent_last_self |= !message.wrapped && message.from == message.to && message.from + 1 == centers.size();
        }
        for (std::size_t n = 0; n < diagram.messages.size(); ++n) {
            const auto& message = diagram.messages[n];
            if (!message.wrapped) continue;
            if (!work.spend()) return {};
            const auto first = std::min(message.from, message.to);
            if (first + 1 == centers.size() && !independent_last_self) return {};
            const int start = centers[first] + static_cast<int>(max_depth[first] ? (max_depth[first] - 1) * 2 : 0) + 2;
            const int end = first + 1 < centers.size() ? centers[first + 1] - (max_depth[first + 1] ? 1 : 0) - 1 : width - 1;
            const auto prefix = message.number.empty() ? std::string{} : message.number + ". ";
            if (!work.spend(prefix.size() + 1)) return {};
            auto packed = sequence_pack_label(*message.wrapped, end - start, prefix, work);
            if (!packed) return {};
            labels[n] = std::move(*packed);
        }
        for (auto& note : diagram.notes) {
            if (!work.spend()) return {};
            if (!note.wrapped) continue;
            const int host = std::max(24, std::abs(centers[note.actors.front()] - centers[note.actors.back()]));
            auto packed = sequence_pack_label(*note.wrapped, host, {}, work);
            if (!packed) return {};
            note.text = std::move(*packed);
        }
    }
    int left_margin = 0;
    for (const auto& note : diagram.notes) if (note.position == NotePosition::Left) {
        const int note_width = std::max(7, label_width(note.text) + 4);
        left_margin = std::max(left_margin, note_width + 3 - centers[note.actors.front()]);
    }
    for (auto& box : boxes) box.x += left_margin;
    for (auto& center : centers) center += left_margin;
    width += left_margin;
    for (auto& group : groups) group.frame.x += left_margin;
    std::vector<Box> note_boxes;
    for (const auto& note : diagram.notes) {
        const int first = centers[note.actors.front()], last = centers[note.actors.back()];
        const int low = std::min(first, last), high = std::max(first, last);
        const int note_width = std::max({7, label_width(note.text) + 4,
            note.position == NotePosition::Over ? high - low + 5 : 0});
        const int left = note.position == NotePosition::Left ? first - note_width - 3 :
            note.position == NotePosition::Right ? first + 3 + (has_activation ? static_cast<int>(max_depth[note.actors.front()] * 2) : 0) : std::max(0, (low + high - note_width + 1) / 2);
        const int note_height = static_cast<int>(label_rows(note.text).size()) + 2;
        note_boxes.push_back({left, 0, note_width, note_height});
        width = std::max(width, left + note_width);
    }
    std::vector<std::vector<std::string>> fragment_headers;
    std::size_t depth = 0;
    for (const auto& fragment : diagram.fragments) {
        depth = std::max(depth, fragment.depth + 1);
        std::vector<std::string> captions;
        auto caption = [&](std::string_view kind, const std::string& text) {
            auto value = std::string(kind) + (text.empty() ? "" : " " + text);
            std::size_t glyphs = 0, cells = 0;
            const auto rows = label_rows(value);
            for (const auto row : rows) {
                if (!work.spend(row.size())) return false;
                glyphs += ftxui::Utf8ToGlyphs(std::string(row)).size();
                cells += static_cast<std::size_t>(ftxui::string_width(std::string(row)));
            }
            if (glyphs + rows.size() - 1 > kLabelLimit || cells > kLabelLimit) return false;
            width = std::max(width, label_width(value) + 4 + static_cast<int>(fragment.depth * 4));
            captions.push_back(std::move(value));
            return true;
        };
        if (!caption(fragment.kind, fragment.text)) return {};
        const std::string_view divider = fragment.kind == "alt" ? "else" : fragment.kind == "par" ? "and" : "option";
        for (const auto& text : fragment.dividers) if (!caption(divider, text)) return {};
        fragment_headers.push_back(std::move(captions));
    }
    const int margin = static_cast<int>(depth * 2);
    width += margin * 2;
    for (auto& box : boxes) box.x += margin;
    for (auto& center : centers) center += margin;
    for (auto& box : note_boxes) box.x += margin;
    for (auto& group : groups) group.frame.x += margin;
    std::vector<Box> fragment_boxes(diagram.fragments.size());
    std::vector<std::vector<int>> header_positions(diagram.fragments.size());
    std::vector<int> event_positions;
    enum class StripEnd { Tail, Cap, Destruction };
    struct ActiveStrip { std::size_t actor, depth; int start, finish; StripEnd end = StripEnd::Tail; };
    std::vector<ActiveStrip> active_strips;
    std::vector<std::vector<std::size_t>> open_strips(diagram.participants.size());
    std::vector<int> last_cap(diagram.participants.size(), -1);
    std::vector<int> life_birth(diagram.participants.size(), -1), life_death(diagram.participants.size(), -1);
    int height = header_height + 2;
    for (const auto& event : diagram.events) {
        if (!work.spend()) return {};
        if (event.kind == EventKind::Activate || event.kind == EventKind::Deactivate)
            height = std::max(height, last_cap[event.index] + 2);
        event_positions.push_back(height);
        if (event.kind == EventKind::Activate) {
            auto& stack = open_strips[event.index];
            stack.push_back(active_strips.size());
            active_strips.push_back({event.index, stack.size(), height, -1, StripEnd::Tail});
            last_cap[event.index] = height++;
        } else if (event.kind == EventKind::Deactivate) {
            // Earlier destruction already ended every visible strip. Keep the
            // source event and its position slot, without a cap or new row.
            if (life_death[event.index] >= 0) continue;
            auto& stack = open_strips[event.index];
            auto& strip = active_strips[stack.back()]; strip.finish = height; strip.end = StripEnd::Cap;
            stack.pop_back(); last_cap[event.index] = height++;
        } else if (event.kind == EventKind::Create) {
            life_birth[event.index] = height++;
        } else if (event.kind == EventKind::Destroy) {
            life_death[event.index] = height;
            for (const auto index : open_strips[event.index]) {
                if (!work.spend()) return {};
                active_strips[index].finish = height - 1;
                active_strips[index].end = StripEnd::Destruction;
            }
            open_strips[event.index].clear(); height += 2;
        } else if (event.kind == EventKind::Message) {
            const auto& message = diagram.messages[event.index];
            const auto& label = labels[event.index];
            height += (label.empty() ? 0 : static_cast<int>(label_rows(label).size())) + (message.from == message.to ? 5 : 3);
        } else if (event.kind == EventKind::Note) height += note_boxes[event.index].height + 2;
        else if (event.kind == EventKind::Close) {
            auto& box = fragment_boxes[event.index];
            box.height = height - box.y + 1;
            height += 3;
        } else {
            const auto& fragment = diagram.fragments[event.index];
            if (event.kind == EventKind::Open) {
                const int inset = static_cast<int>(fragment.depth * 2);
                fragment_boxes[event.index] = {inset, height, width - inset * 2, 0};
            }
            const auto part = event.kind == EventKind::Open ? 0 : event.part + 1;
            header_positions[event.index].push_back(height);
            height += static_cast<int>(label_rows(fragment_headers[event.index][part]).size()) + 3;
        }
        if (height > kHeightLimit) return {};
    }
    if (has_activation) height += 2;
    for (auto& strip : active_strips) if (strip.end == StripEnd::Tail) strip.finish = height - 1;
    const int timeline_height = height;
    if (!groups.empty()) ++height;
    for (auto& group : groups) group.frame.height = height;
    if (width <= 0 || width > kWidthLimit || height > kHeightLimit ||
        !work.spend(static_cast<std::size_t>(width * height))) return {};
    std::vector<int> label_positions;
    std::vector<bool> forbidden;
    if (!groups.empty()) {
        if (!work.spend(static_cast<std::size_t>(width))) return {};
        forbidden.resize(static_cast<std::size_t>(width));
        label_positions.resize(diagram.messages.size());
        for (std::size_t n = 0; n < centers.size(); ++n) {
            const int right = centers[n] + (max_depth[n] ? static_cast<int>((max_depth[n] - 1) * 2) + 1 : 0);
            const int left = centers[n] - (max_depth[n] ? 1 : 0);
            for (int x = left; x <= right; ++x) {
                if (x < 0 || x >= width || !work.spend()) return {};
                forbidden[static_cast<std::size_t>(x)] = true;
            }
        }
        for (const auto& group : groups) {
            if (!work.spend(2)) return {};
            forbidden[static_cast<std::size_t>(group.frame.x)] = true;
            forbidden[static_cast<std::size_t>(group.frame.x + group.frame.width - 1)] = true;
        }
    }
    if (!groups.empty()) for (std::size_t n = 0; n < diagram.messages.size(); ++n) {
        if (!work.spend()) return {};
        const auto& message = diagram.messages[n];
        const int from = centers[message.from], to = centers[message.to];
        const auto owner = from <= to ? message.from : message.to;
        const int span = max_depth[owner] ? static_cast<int>((max_depth[owner] - 1) * 2) : 0;
        label_positions[n] = std::min(from, to) + span + 2;
        const int count = label_width(labels[n]);
        if (!count) continue;
        const int end = from == to ?
            (message.wrapped ? (message.from + 1 < centers.size() ? centers[message.from + 1] -
                (max_depth[message.from + 1] ? 1 : 0) - 1 : width - static_cast<int>(depth * 2) - 1) :
                from + span + std::max(4, count + 3)) : std::max(from, to);
        int run = 0, chosen = -1;
        for (int x = label_positions[n]; x < end; ++x) {
            if (x < 0 || x >= width || !work.spend()) return {};
            run = forbidden[static_cast<std::size_t>(x)] ? 0 : run + 1;
            if (run >= count) {chosen = x - count + 1;break;}
        }
        if (chosen < 0) return {};
        label_positions[n] = chosen;
    }
    Canvas canvas(width, height, work);
    enum class Ink { Ordinary, Text, Opaque, Marker, SelfVertical };
    std::vector<Ink> ink;
    if (has_activation) {
        if (!work.spend(static_cast<std::size_t>(width * height))) return {};
        ink.resize(static_cast<std::size_t>(width * height), Ink::Ordinary);
    }
    auto mark = [&](int x, int y, Ink role) {
        if (!has_activation) return true;
        if (x < 0 || x >= width || y < 0 || y >= height || !work.spend()) return false;
        ink[static_cast<std::size_t>(y * width + x)] = role; return true;
    };
    for (const auto& group : groups) {
        const auto box = group.frame;
        const Point tl{box.x, 0}, tr{box.x + box.width - 1, 0};
        const Point bl{box.x, height - 1}, br{tr.x, height - 1};
        if (!canvas.segment(tl, tr) || !canvas.segment(tr, br) || !canvas.segment(br, bl) || !canvas.segment(bl, tl)) return {};
        int top = 0;
        for (const auto& caption : group.captions) {
            const int bottom = top + static_cast<int>(label_rows(caption).size()) + 1;
            if (!canvas.segment({box.x, bottom}, {tr.x, bottom})) return {};
            for (int y = top + 1; y < bottom; ++y) for (int x = box.x + 1; x < tr.x; ++x)
                if (!canvas.put({x, y}, " ")) return {};
            if (!canvas.multiline({box.x + 1, top + 1}, caption, box.width - 2)) return {};
            top = bottom;
        }
    }
    for (std::size_t n = 0; n < boxes.size(); ++n) {
        const int first = diagram.participants[n].created ? life_birth[n] : boxes[n].y + boxes[n].height;
        const int last = life_death[n] < 0 ? timeline_height - 1 : life_death[n] - 1;
        if (first < 0 || first > last || !node_box(canvas, boxes[n], diagram.participants[n]) ||
            !canvas.segment({centers[n], first}, {centers[n], last})) return {};
        if (diagram.participants[n].role == Role::Actor) {
            const int center = centers[n], actor_top = boxes[n].y - 3;
            if (!canvas.put({center, actor_top}, "○") || !canvas.put({center - 1, actor_top + 1}, "╱") ||
                !canvas.put({center, actor_top + 1}, "│") || !canvas.put({center + 1, actor_top + 1}, "╲") ||
                !canvas.put({center - 1, actor_top + 2}, "╱") || !canvas.put({center + 1, actor_top + 2}, "╲")) return {};
        } else if (diagram.participants[n].role != Role::Participant &&
                   !new_role_figure(canvas, boxes[n], diagram.participants[n].role)) return {};
    }
    for (std::size_t index = 0; index < fragment_boxes.size(); ++index) {
        const auto box = fragment_boxes[index];
        const Point tl{box.x, box.y}, tr{box.x + box.width - 1, box.y};
        const Point bl{box.x, box.y + box.height - 1}, br{tr.x, bl.y};
        if (!canvas.segment(tl, tr) || !canvas.segment(tr, br) ||
            !canvas.segment(br, bl) || !canvas.segment(bl, tl)) return {};
        for (std::size_t part = 0; part < fragment_headers[index].size(); ++part) {
            const int top = header_positions[index][part];
            const auto& caption = fragment_headers[index][part];
            const int bottom = top + static_cast<int>(label_rows(caption).size()) + 1;
            if (part && !canvas.segment({box.x, top}, {tr.x, top})) return {};
            if (!canvas.segment({box.x, bottom}, {tr.x, bottom})) return {};
            for (int y = top + 1; y < bottom; ++y) for (int x = box.x + 1; x < tr.x; ++x)
                if (!canvas.put({x, y}, " ") || !mark(x, y, Ink::Opaque)) return {};
            if (!canvas.multiline({box.x + 1, top + 1}, caption, box.width - 2)) return {};
        }
    }
    for (std::size_t event_index = 0; event_index < diagram.events.size(); ++event_index) {
        const auto& event = diagram.events[event_index];
        const int y = event_positions[event_index];
        if (event.kind == EventKind::Note) {
            auto box = note_boxes[event.index]; box.y = y;
            Participant node; node.text = diagram.notes[event.index].text;
            if (!node_box(canvas, box, node, diagram.note_text_align)) return {};
            if (has_activation) for (int row = box.y; row < box.y + box.height; ++row)
                for (int col = box.x; col < box.x + box.width; ++col)
                    if (!mark(col, row, Ink::Opaque)) return {};
            const auto& note = diagram.notes[event.index];
            if (note.position == NotePosition::Over) {
                for (const auto actor : note.actors) {
                    // The event row, not final participant.alive, owns the
                    // connection: earlier Notes keep their original junctions.
                    if (life_death[actor] >= 0 && box.y >= life_death[actor]) continue;
                    if (!canvas.put({centers[actor], box.y}, "┴") ||
                        !canvas.put({centers[actor], box.y + box.height - 1}, "┬")) return {};
                }
            } else {
                const bool left = note.position == NotePosition::Left;
                const Point port{left ? box.x + box.width - 1 : box.x, box.y + (box.height - 1) / 2};
                if (!canvas.segment(port, {centers[note.actors.front()], port.y}) ||
                    !canvas.put(port, left ? "├" : "┤")) return {};
            }
            continue;
        }
        if (event.kind != EventKind::Message) continue;
        const auto index = event.index;
        const auto& message = diagram.messages[index];
        const auto from = centers[message.from], to = centers[message.to];
        const auto left_actor = from <= to ? message.from : message.to;
        const auto span = has_activation && max_depth[left_actor] ? static_cast<int>((max_depth[left_actor] - 1) * 2) : 0;
        const auto label_x = groups.empty() ? std::min(from, to) + span + 2 : label_positions[index];
        const auto& label = labels[index];
        const auto rows = label.empty() ? 0 : static_cast<int>(label_rows(label).size());
        const auto label_cells = label_width(label);
        if (!label.empty() && !canvas.multiline({label_x, y}, label, label_cells)) return {};
        if (has_activation) for (int row = y; row < y + rows; ++row)
            for (int col = label_x; col < label_x + label_cells; ++col)
                if (!mark(col, row, Ink::Text)) return {};
        const auto shaft = y + rows;
        if (from == to) {
            const auto turn = message.wrapped ?
                (message.from + 1 < centers.size() ? centers[message.from + 1] -
                    (max_depth[message.from + 1] ? 1 : 0) - 1 : width - static_cast<int>(depth * 2) - 1) :
                from + span + std::max(4, label_cells + 3);
            if (turn <= from + span || (message.wrapped && label_x + label_cells > turn)) return {};
            if (turn >= width || !canvas.segment({from, shaft}, {turn, shaft}, message.dashed) ||
                !canvas.segment({turn, shaft}, {turn, shaft + 2}, message.dashed) ||
                !canvas.segment({turn, shaft + 2}, {from, shaft + 2}, message.dashed)) return {};
            if (has_activation) for (int row = shaft; row <= shaft + 2; ++row)
                if (!mark(turn, row, Ink::SelfVertical)) return {};
        } else {
            if (!canvas.segment({from, shaft}, {to, shaft}, message.dashed)) return {};
        }
        const bool self = from == to;
        const int departure = self || from < to ? 1 : -1;
        const int arrival = self ? -1 : departure;
        const int finish = shaft + (self ? 2 : 0);
        auto marker = [&](int x, int row, int direction, Arrow kind) {
            if (kind == Arrow::Unheaded) return true;
            // Half shape and authored endpoint ownership follow the pinned
            // Mermaid sequence signal grammar; physical direction is separate.
            std::string_view glyph;
            switch (kind) {
            case Arrow::FilledHalfTop: case Arrow::FilledHalfTopReverse:
                glyph = direction > 0 ? "◢" : "◣"; break;
            case Arrow::FilledHalfBottom: case Arrow::FilledHalfBottomReverse:
                glyph = direction > 0 ? "◥" : "◤"; break;
            case Arrow::OpenHalfTop: case Arrow::OpenHalfTopReverse:
                glyph = direction > 0 ? "╲" : "╱"; break;
            case Arrow::OpenHalfBottom: case Arrow::OpenHalfBottomReverse:
                glyph = direction > 0 ? "╱" : "╲"; break;
            default:
                glyph = kind == Arrow::Async ? (direction > 0 ? "›" : "‹") :
                    kind == Arrow::Cross ? "×" : (direction > 0 ? "▶" : "◀"); break;
            }
            return canvas.put({x, row}, glyph) && mark(x, row, Ink::Marker);
        };
        int source_marker = from, target_marker = to;
        if (message.central_from) {
            if (!canvas.put({from, shaft}, "○") || !mark(from, shaft, Ink::Marker)) return {};
            source_marker += departure;
        }
        if (message.central_to) {
            if (!canvas.put({to, finish}, "○") || !mark(to, finish, Ink::Marker)) return {};
            target_marker -= arrival;
        }
        const bool reverse = message.kind == Arrow::FilledHalfTopReverse ||
            message.kind == Arrow::FilledHalfBottomReverse ||
            message.kind == Arrow::OpenHalfTopReverse || message.kind == Arrow::OpenHalfBottomReverse;
        if (reverse) {
            if (!marker(source_marker, shaft, -departure, message.kind)) return {};
        } else {
            if (message.kind == Arrow::Both && !marker(source_marker, shaft, -departure, Arrow::Filled)) return {};
            if (!marker(target_marker, finish, arrival, message.kind)) return {};
        }
    }
    if (has_activation) {
        auto heavy = [](unsigned bits) -> std::string_view {
            if (bits == (kUp | kDown)) return "┃";
            if (bits == (kUp | kDown | kRight)) return "┣";
            if (bits == (kUp | kDown | kLeft)) return "┫";
            if (bits == (kUp | kDown | kLeft | kRight)) return "╋";
            return {};
        };
        for (const auto& strip : active_strips) {
            const int column = centers[strip.actor] + static_cast<int>((strip.depth - 1) * 2);
            for (int row = strip.start; row <= strip.finish; ++row) {
                if (!work.spend()) return {};
                const auto role = ink[static_cast<std::size_t>(row * width + column)];
                if (role == Ink::Text || role == Ink::SelfVertical) return {};
                if (role == Ink::Opaque || role == Ink::Marker) continue;
                auto& cell = canvas.at({column, row});
                const auto glyph = heavy(cell.bits | kUp | kDown);
                if (glyph.empty() || !canvas.put({column, row}, glyph)) return {};
            }
        }
        for (const auto& strip : active_strips) {
            const int center = centers[strip.actor], column = center + static_cast<int>((strip.depth - 1) * 2);
            auto cap = [&](int row, bool opening) {
                if (!canvas.put({center - 1, row}, "╺") || !canvas.put({column + 1, row}, "╸")) return false;
                for (int x = center; x <= column; ++x) {
                    const std::string_view glyph = (x - center) % 2 ? "━" :
                        x == column && strip.depth > 1 ? (opening ? "┳" : "┻") : "╋";
                    if (!canvas.put({x, row}, glyph)) return false;
                }
                return true;
            };
            if (!cap(strip.start, true) || (strip.end == StripEnd::Cap && !cap(strip.finish, false))) return {};
        }
    }
    for (std::size_t actor = 0; actor < life_death.size(); ++actor) {
        if (life_death[actor] >= 0 &&
            (!work.spend() || !canvas.put({centers[actor], life_death[actor]}, "×"))) return {};
    }
    auto lines = canvas.lines();
    if (lines.empty()) return {};
    return lines;
}

} // namespace

std::optional<std::vector<std::string>> sequence_layout(std::string_view source, Work& work) {
    if (!work.spend(source.size())) return {};
    auto sequence = parse(source, work);
    if (!sequence) return {};
    auto lines = layout(*sequence, work);
    if (!lines || sequence->title.empty()) return lines;
    const auto title_rows = label_rows(sequence->title);
    if (lines->size() + title_rows.size() + 1 > kHeightLimit ||
        label_width(sequence->title) > kWidthLimit ||
        !work.spend(sequence->title.size() + title_rows.size() + 1)) return {};
    std::vector<std::string> titled;
    titled.reserve(title_rows.size() + 1 + lines->size());
    for (const auto row : title_rows) titled.emplace_back(row);
    titled.emplace_back();
    for (auto& line : *lines) titled.push_back(std::move(line));
    return titled;
}

} // namespace loom::ui::mermaid::detail
