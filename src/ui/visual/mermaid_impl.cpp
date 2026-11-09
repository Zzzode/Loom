module;

#include <cassert>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_tokens;
import loom.serdes.json;

namespace loom::ui::mermaid {
namespace {

using namespace detail;
// RFC 0031 FS-3a: classDef/class/direct-style projection graduated to
// production, so no style gate remains. The handDrawn look metadata profile
// (FS-8) is still unadmitted and available only through the reviewed test
// facade.
thread_local bool flow_handdrawn_preview = false;

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return value;
}

bool id_start(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_';
}

bool id_char(char value) {
    return id_start(value) || (value >= '0' && value <= '9');
}

// This is the bounded ASCII projection needed by the pinned Flow lexer.  Keep
// complete-link precedence in flow_statement: hyphen never absorbs --> or
// -.->, while dotted and hyphenated source IDs remain whole identifiers.
bool flow_id_start(char value) {
    return id_char(value);
}

bool flow_id_continue(std::string_view value, std::size_t at) {
    const auto ch = value[at];
    if (flow_id_start(ch) || ch == '.') return true;
    return ch == '-' && at + 1u < value.size() && value[at + 1u] != '>' && value[at + 1u] != '-' &&
           value[at + 1u] != '.';
}

// Syntax admission remains local to the flow/sequence grammar. After UTF-8
// validation, ASCII marker bytes cannot occur inside a multibyte scalar.
bool plain_unicode(std::string_view text) {
    return detail::unicode_text(text) && text.find_first_of("<>&#`") == std::string_view::npos;
}

// A decoded flow label plus its Markdown span runs. Runs is empty for a
// plain label; for a backtick Markdown string it carries bold/underline
// spans and its concatenated text equals `text`.
struct RichLabel {
    std::string text;
    std::vector<detail::LabelRun> runs;
};

// FS-1: per-diagram Markdown state kept entirely OUTSIDE the hot parse
// structs so Diagram/Vertex/Edge/Group stay byte-identical to HEAD and the
// sizeof-driven gas profile (and dense-graph budget boundary) is unchanged.
// The tables are sparse: the bounded worst-case slot fee is paid once, only
// on the first label that actually carries spans, and rows are filled lazily;
// an absent index paints as a plain label.
struct RichLabels {
    std::vector<std::vector<detail::LabelRun>> vertex;
    std::vector<std::vector<detail::LabelRun>> edge;
    std::vector<std::vector<detail::LabelRun>> group;
    // Disclosure footer lines for accepted non-terminal keys (frontmatter
    // htmlLabels / body init flowchart.htmlLabels), appended below the art.
    std::vector<std::string> notices;
    bool paid = false;

    bool activate(Work& work) {
        if (paid) return true;
        constexpr std::size_t kFee =
            (64u + 128u + kGroupLimit) * sizeof(std::vector<detail::LabelRun>) + 256u;
        if (!work.spend(kFee)) return false;
        paid = true;
        return true;
    }
    bool bind_vertex(std::size_t index, std::vector<detail::LabelRun> runs, Work& work) {
        if (runs.empty()) return true;
        if (!activate(work)) return false;
        vertex.resize(std::max(vertex.size(), index + 1u));
        vertex[index] = std::move(runs);
        return true;
    }
    bool bind_edge(std::size_t index, const std::vector<detail::LabelRun>& runs, Work& work) {
        if (runs.empty()) return true;
        if (!activate(work)) return false;
        edge.resize(std::max(edge.size(), index + 1u));
        edge[index] = runs;
        return true;
    }
    bool bind_group(std::size_t index, const std::vector<detail::LabelRun>& runs, Work& work) {
        if (runs.empty()) return true;
        if (!activate(work)) return false;
        group.resize(std::max(group.size(), index + 1u));
        group[index] = runs;
        return true;
    }
};

// Closed Markdown-string subset (FS-1): whole content wrapped in backticks,
// `**bold**`, `*underline*` (no nesting), physical newlines and <br> as row
// breaks. Anything else (unbalanced markers, other tags/entities, inner
// backticks) rejects so the source keeps its ordinary fallback.

std::optional<RichLabel> markdown_label(std::string_view input) {
    if (input.size() < 2u || input.front() != '`' || input.back() != '`') return {};
    const auto raw = input.substr(1u, input.size() - 2u);
    // Physical newlines are visual row breaks; mermaid createText trims the
    // continuation indentation. Trim each physical line of the WHOLE content
    // once: trimming per parsed run would swallow spaces adjacent to markup on
    // the same line ("in **the** hog" would become "inthehog").
    std::string content;
    {
        std::size_t at = 0;
        bool first_line = true;
        while (true) {
            const auto nl = raw.find('\n', at);
            auto line = raw.substr(at, nl == std::string_view::npos ? raw.size() - at : nl - at);
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
                line.remove_prefix(1);
            while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
                line.remove_suffix(1);
            if (!first_line) content += '\n';
            content += line;
            first_line = false;
            if (nl == std::string_view::npos) break;
            at = nl + 1u;
        }
    }
    RichLabel result;
    // A run segment may span visual row breaks ('\n'); each physical line of
    // it must be ordinary printable unicode with no tag/entity/backtick chars.
    auto valid_segment = [](std::string_view segment) {
        std::size_t at = 0;
        while (true) {
            const auto nl = segment.find('\n', at);
            const auto line =
                segment.substr(at, nl == std::string_view::npos ? segment.size() - at : nl - at);
            if (!line.empty() &&
                (!plain_unicode(line) || ftxui::Utf8ToGlyphs(std::string{line}).empty()))
                return false;
            if (nl == std::string_view::npos) return true;
            at = nl + 1u;
        }
    };
    auto append_plain = [&](std::string_view segment) -> bool {
        if (segment.empty()) return true;
        if (!valid_segment(segment)) return false;
        result.text += segment;
        if (!result.runs.empty() && !result.runs.back().bold &&
            !result.runs.back().underline)
            result.runs.back().text += segment;
        else
            result.runs.emplace_back(std::string{segment}, false, false);
        return true;
    };
    auto append_styled = [&](std::string_view segment, bool bold) -> bool {
        if (segment.empty() || !valid_segment(segment)) return false;
        result.text += segment;
        // Merge with an adjacent identical-style run.
        if (!result.runs.empty() && result.runs.back().bold == bold &&
            result.runs.back().underline == !bold)
            result.runs.back().text += segment;
        else
            result.runs.emplace_back(std::string{segment}, bold, !bold);
        return true;
    };
    std::size_t at = 0;
    while (at < content.size()) {
        if (content.compare(at, 4, "<br>") == 0 ||
            content.compare(at, 5, "<br/>") == 0 ||
            content.compare(at, 6, "<br />") == 0) {
            if (!append_plain("\n")) return {};
            at = content.find('>', at) + 1u;
            continue;
        }
        const char ch = content[at];
        if (ch == '<') return {};  // no tags other than <br> forms
        if (ch == '*') {
            const bool bold = content.compare(at, 2, "**") == 0;
            const std::string_view marker = bold ? "**" : "*";
            const std::size_t open = at + marker.size();
            std::size_t close = std::string_view::npos;
            if (bold) {
                const auto candidate = content.find("**", open);
                if (candidate != std::string_view::npos) close = candidate;
            } else {
                // A single-* span may not contain another '*' (no nesting).
                for (std::size_t scan = open; scan < content.size(); ++scan) {
                    if (content[scan] == '*') {
                        close = scan;
                        break;
                    }
                }
            }
            if (close == std::string_view::npos) return {};
            if (!append_styled(content.substr(open, close - open), bold)) return {};
            at = close + marker.size();
            continue;
        }
        const auto next = content.find_first_of("*<", at);
        const auto end = next == std::string_view::npos ? content.size() : next;
        if (!append_plain(content.substr(at, end - at))) return {};
        at = end;
    }
    if (trim(result.text).empty()) return {};
    return result;
}

std::optional<RichLabel> label(std::string_view input, bool delimited = false) {
    input = trim(input);
    if (input.empty()) return {};
    if (input.front() == '"') {
        if (input.size() < 2 || input.back() != '"') return {};
        input = input.substr(1, input.size() - 2);
        // Flowchart quoted strings do not use C-style backslash decoding.
        // Preserve source for escapes instead of inventing a different label.
        if (input.find_first_of("\"\\") != std::string_view::npos) return {};
    } else {
        if (input.find('"') != std::string_view::npos) return {};
        if (delimited && input.find_first_of("[]{}()|") != std::string_view::npos) return {};
    }
    if (input.size() >= 2u && input.front() == '`' && input.back() == '`')
        return markdown_label(input);
    RichLabel result;
    // RFC 0031 FS-6: the pinned Mermaid createText renderer expands Font
    // Awesome markers in plain (and quoted) labels. The terminal has no icon
    // font, so a recognized marker projects to one fixed marker cell and the
    // human label text is retained; a marker whose icon name is outside the
    // closed vocabulary fails closed (whole-source fallback) instead of
    // drawing the marker literally and pretending the diagram was understood.
    // The optional style prefix (fas:/fab:/fal:/far:/fak:) is consumed.
    constexpr std::string_view kIconNames[]{"car", "code"};
    constexpr std::string_view kIconMarker = "\xE2\x96\xA0";  // U+25A0 BLACK SQUARE
    std::string projected;
    projected.reserve(input.size());
    for (std::size_t at = 0; at <= input.size();) {
        const auto next = input.find("fa", at);
        if (next == std::string_view::npos) {
            projected += input.substr(at);
            break;
        }
        projected += input.substr(at, next - at);
        std::size_t p = next + 2u;
        if (p < input.size() &&
            std::string_view{"bklrs"}.find(input[p]) != std::string_view::npos)
            ++p;
        if (input.substr(p).starts_with(":fa-")) {
            p += 4u;
            std::size_t q = p;
            while (q < input.size() && (id_char(input[q]) || input[q] == '-')) ++q;
            const std::string_view icon_name = input.substr(p, q - p);
            if (!icon_name.empty() &&
                std::ranges::find(kIconNames, icon_name) != std::end(kIconNames)) {
                projected += kIconMarker;
                at = q;
                continue;
            }
            // A well-formed marker naming an unknown icon cannot be projected.
            if (q > p) return {};
        }
        projected += "fa";
        at = next + 2u;
    }
    input = projected;
    // Flowchart line breaks are presentation syntax, not arbitrary HTML. Decode
    // only the explicit break forms, then validate every retained plain row.
    std::string decoded;
    std::size_t glyphs = 0, cells = 0;
    while (!input.empty()) {
        if (input.starts_with("<br>") || input.starts_with("<br/>") ||
            input.starts_with("<br />")) {
            const auto end = input.find('>');
            decoded += '\n';
            input.remove_prefix(end + 1);
            ++glyphs;
        } else if (input.starts_with("<strong>") || input.starts_with("<b>")) {
            // Inline emphasis wrapper: drop the tag, retain the inner text.
            input.remove_prefix(input.starts_with("<strong>") ? 8u : 3u);
        } else if (input.starts_with("</strong>") || input.starts_with("</b>")) {
            input.remove_prefix(input.starts_with("</strong>") ? 9u : 4u);
        } else if (input.starts_with("<img") && input.size() > 4u &&
                   (input[4] == '>' || input[4] == '/' || input[4] == ' ' ||
                    input[4] == '\t' || input[4] == '\n' || input[4] == '\r')) {
            // Void image element with optional attributes. The tag-name
            // boundary is required so "<imgrand>" cannot masquerade as an
            // image. Find the closing '>' while skipping single/double
            // quoted attribute spans so a '>' inside a value cannot
            // truncate the tag and leak attribute fragments as text; a
            // missing close, an embedded newline or a nested '<' is not a
            // label tag.
            std::size_t close = std::string_view::npos;
            char quote = '\0';
            for (std::size_t at = 4u; at < input.size(); ++at) {
                const char ch = input[at];
                if (quote != '\0') {
                    if (ch == quote) quote = '\0';
                    continue;
                }
                if (ch == '\n') return {};
                if (ch == '"' || ch == '\'') {
                    quote = ch;
                } else if (ch == '>') {
                    close = at;
                    break;
                } else if (ch == '\n' || ch == '<') {
                    return {};
                }
            }
            if (close == std::string_view::npos || quote != '\0') return {};
            input.remove_prefix(close + 1);
        } else if (input.starts_with('<')) {
            return {};  // unknown inline tag fails closed
        } else {
            const auto end = input.find('<');
            const auto row = input.substr(0, end);
            if (!plain_unicode(row)) return {};
            const auto parts = ftxui::Utf8ToGlyphs(std::string(row));
            std::string round_trip;
            for (const auto& part : parts)
                round_trip += part;
            if (round_trip != row) return {};
            glyphs += parts.size();
            cells += static_cast<std::size_t>(ftxui::string_width(std::string(row)));
            decoded += row;
            input.remove_prefix(row.size());
        }
        if (glyphs > kLabelLimit || cells > kLabelLimit) return {};
    }
    if (trim(decoded).empty()) return {};
    result.text = std::move(decoded);
    return result;
}

struct Scanner {
    std::string_view rest;

    void space() { rest = trim_front(rest); }

    static std::string_view trim_front(std::string_view value) {
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
            value.remove_prefix(1);
        return value;
    }

    bool take(std::string_view text) {
        if (!rest.starts_with(text)) return false;
        rest.remove_prefix(text.size());
        return true;
    }

    std::optional<std::string> id() {
        space();
        if (rest.empty() || !flow_id_start(rest.front())) return {};
        std::size_t length = 1;
        while (length < rest.size() && flow_id_continue(rest, length))
            ++length;
        auto result = std::string(rest.substr(0, length));
        rest.remove_prefix(length);
        return result;
    }

    std::optional<std::string_view> enclosed_view(char close) {
        bool quoted = false;
        bool escaped = false;
        for (std::size_t at = 0; at < rest.size(); ++at) {
            const auto ch = rest[at];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (quoted && ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                quoted = !quoted;
                continue;
            }
            if (!quoted && ch == close) {
                const auto result = rest.substr(0, at);
                rest.remove_prefix(at + 1);
                return result;
            }
        }
        return {};
    }

    std::optional<RichLabel> enclosed(char close) {
        const auto raw = enclosed_view(close);
        return raw ? label(*raw, true) : std::nullopt;
    }
};

enum class Direction { Down, Up, Right, Left };
enum class Shape {
    Rectangle,
    Rounded,
    Decision,
    Stadium,
    Circle,
    Subroutine,
    DoubleCircle,
    Hexagon,
    Cylinder,
    Asymmetric,
    Trapezoid,
    TrapezoidAlt,
    Parallelogram,
    ParallelogramAlt,
    Triangle,
    Delay,
    SmallCircle,
    ForkJoin,
    DividedRectangle,
    FlippedTriangle,
    SlopedRectangle,
    WindowPane,
    Card,
    Folder,
    HorizontalCylinder,
    LinedCylinder,
    Datastore,
    NotchedPentagon,
    Hourglass,
    Bolt,
    BowTie,
    Brace,
    StackedRectangle,
    TaggedRectangle,
    Document,
    StackedDocument,
    LinedDocument,
    TaggedDocument,
    FilledCircle,
    FramedCircle,
    CrossedCircle,
    ShadedProcess,
    CurvedTrapezoid,
    PaperTape,
};
enum class Stroke { Solid, Dotted, Thick };

bool classic_shape(Shape shape) {
    switch (shape) {
    case Shape::Stadium:
    case Shape::Circle:
    case Shape::Subroutine:
    case Shape::DoubleCircle:
    case Shape::Hexagon:
    case Shape::Cylinder:
    case Shape::Asymmetric:
    case Shape::Trapezoid:
    case Shape::TrapezoidAlt:
    case Shape::Parallelogram:
    case Shape::ParallelogramAlt:
    case Shape::Triangle:
    case Shape::Delay:
    case Shape::SmallCircle:
    case Shape::ForkJoin:
    case Shape::DividedRectangle:
    case Shape::FlippedTriangle:
    case Shape::SlopedRectangle:
    case Shape::WindowPane:
    case Shape::Card:
    case Shape::Folder:
    case Shape::HorizontalCylinder:
    case Shape::LinedCylinder:
    case Shape::Datastore:
    case Shape::NotchedPentagon:
    case Shape::Hourglass:
    case Shape::Bolt:
    case Shape::BowTie:
    case Shape::Brace:
    case Shape::StackedRectangle:
    case Shape::TaggedRectangle:
    case Shape::Document:
    case Shape::StackedDocument:
    case Shape::LinedDocument:
    case Shape::TaggedDocument:
    case Shape::FilledCircle:
    case Shape::FramedCircle:
    case Shape::CrossedCircle:
    case Shape::ShadedProcess:
    case Shape::CurvedTrapezoid:
    case Shape::PaperTape:
        return true;
    case Shape::Rectangle:
    case Shape::Rounded:
    case Shape::Decision:
        return false;
    }
    return false;
}

int classic_height(Shape shape) {
    if (shape == Shape::Circle || shape == Shape::SmallCircle) return 9;
    if (shape == Shape::DoubleCircle) return 13;
    if (shape == Shape::Cylinder) return 7;
    if (shape == Shape::DividedRectangle) return 7;
    if (shape == Shape::SlopedRectangle) return 7;
    if (shape == Shape::LinedCylinder) return 7;
    if (shape == Shape::ForkJoin) return 5;
    return 5;
}

int classic_min_width(Shape shape) {
    if (shape == Shape::DoubleCircle) return 13;
    if (shape == Shape::Cylinder || shape == Shape::Parallelogram ||
        shape == Shape::ParallelogramAlt || shape == Shape::SlopedRectangle ||
        shape == Shape::LinedCylinder)
        return 11;
    if (shape == Shape::ForkJoin) return 17;
    return 9;
}

bool classic_requires_odd_width(Shape shape) {
    switch (shape) {
    case Shape::Circle:
    case Shape::SmallCircle:
    case Shape::DoubleCircle:
    case Shape::Hexagon:
    case Shape::Cylinder:
    case Shape::Trapezoid:
    case Shape::TrapezoidAlt:
    case Shape::Parallelogram:
    case Shape::ParallelogramAlt:
    case Shape::Triangle:
    case Shape::FlippedTriangle:
    case Shape::WindowPane:
    case Shape::HorizontalCylinder:
    case Shape::LinedCylinder:
    case Shape::Hourglass:
    case Shape::Bolt:
    case Shape::BowTie:
    case Shape::FilledCircle:
    case Shape::FramedCircle:
    case Shape::CrossedCircle:
        return true;
    case Shape::Stadium:
    case Shape::Subroutine:
    case Shape::Asymmetric:
    case Shape::Delay:
    case Shape::ForkJoin:
    case Shape::DividedRectangle:
    case Shape::SlopedRectangle:
    case Shape::Card:
    case Shape::Folder:
    case Shape::Datastore:
    case Shape::NotchedPentagon:
    case Shape::Brace:
    case Shape::StackedRectangle:
    case Shape::TaggedRectangle:
    case Shape::Document:
    case Shape::StackedDocument:
    case Shape::LinedDocument:
    case Shape::TaggedDocument:
    case Shape::ShadedProcess:
    case Shape::CurvedTrapezoid:
    case Shape::PaperTape:
    case Shape::Rectangle:
    case Shape::Rounded:
    case Shape::Decision:
        return false;
    }
    return false;
}

struct Vertex {
    std::string id;
    std::string text;
    Shape shape = Shape::Rectangle;
    bool explicit_label = false;
    // FS-aws: an admitted closed-vocabulary icon marker has already been
    // prepended to text, so a repeated icon statement stays idempotent.
    bool icon = false;
    int group = -1;
    std::size_t order = 0;
};
enum class EndpointKind : unsigned char { Vertex, Group };

struct Endpoint {
    EndpointKind kind = EndpointKind::Vertex;
    std::size_t index = 0u;
    auto operator<=>(const Endpoint&) const = default;
};

struct Edge {
    Endpoint from{};
    Endpoint to{};
    std::optional<std::string> text;
    Stroke stroke = Stroke::Solid;
    bool arrow_start = false;
    bool arrow_end = true;
    int minimum_length = 1;
};
enum class LinkScan { NoMatch, Complete, Unsupported };

struct LinkLexeme {
    LinkScan state = LinkScan::NoMatch;
    Stroke stroke = Stroke::Solid;
    bool arrow_start = false;
    bool arrow_end = true;
    int minimum_length = 1;
    std::size_t consumed = 0;
};

bool link_space(char value) {
    return value == ' ' || value == '\t';
}

std::size_t link_space_after(std::string_view value, std::size_t at) {
    while (at < value.size() && link_space(value[at]))
        ++at;
    return at;
}

// This finite projection retains the primary complete-LINK-before-START_LINK
// priority. It recognizes every normal, dotted, and thick spelling needed by
// the closed LinkProfile3 sources, then refuses markers outside point/none.
LinkLexeme complete_link(std::string_view value) {
    LinkLexeme result;
    if (value.empty()) return result;
    std::size_t at = 0;
    bool invalid_start = false;
    if (value[at] == '<') {
        result.arrow_start = true;
        ++at;
    } else if (value[at] == 'x' || value[at] == 'o') {
        invalid_start = true;
        ++at;
    }
    if (at >= value.size()) return {};

    const auto finish = [&](Stroke stroke, bool arrow_end, int minimum_length, std::size_t end,
                            bool invalid_end) {
        LinkLexeme link;
        link.stroke = stroke;
        link.arrow_start = result.arrow_start;
        link.arrow_end = arrow_end;
        link.state = (invalid_start || invalid_end || (link.arrow_start && !arrow_end))
                         ? LinkScan::Unsupported
                         : LinkScan::Complete;
        link.minimum_length = minimum_length;
        link.consumed = link_space_after(value, end);
        return link;
    };

    if (value[at] == '-') {
        std::size_t end = at;
        while (end < value.size() && value[end] == '-')
            ++end;
        const auto count = end - at;
        if (count < 2) return {};
        if (end < value.size() && (value[end] == '>' || value[end] == 'x' || value[end] == 'o'))
            return finish(Stroke::Solid, value[end] == '>', static_cast<int>(count - 1u), end + 1u,
                          value[end] != '>');
        if (count >= 3u)
            return finish(Stroke::Solid, false, static_cast<int>(count - 2u), end, false);
        return {};
    }
    if (value[at] == '=') {
        std::size_t end = at;
        while (end < value.size() && value[end] == '=')
            ++end;
        const auto count = end - at;
        if (count < 2) return {};
        if (end < value.size() && (value[end] == '>' || value[end] == 'x' || value[end] == 'o'))
            return finish(Stroke::Thick, value[end] == '>', static_cast<int>(count - 1u), end + 1u,
                          value[end] != '>');
        if (count >= 3u)
            return finish(Stroke::Thick, false, static_cast<int>(count - 2u), end, false);
        return {};
    }
    if (value[at] == '-' || value[at] == '.') return {};
    return {};
}

LinkLexeme complete_dotted_link(std::string_view value) {
    LinkLexeme result;
    if (value.empty()) return result;
    std::size_t at = 0;
    bool invalid_start = false;
    if (value[at] == '<') {
        result.arrow_start = true;
        ++at;
    } else if (value[at] == 'x' || value[at] == 'o') {
        invalid_start = true;
        ++at;
    }
    if (at < value.size() && value[at] == '-') ++at;
    const auto dots = at;
    while (at < value.size() && value[at] == '.')
        ++at;
    if (at == dots || at >= value.size() || value[at] != '-') return {};
    ++at;
    bool arrow_end = false, invalid_end = false;
    if (at < value.size() && (value[at] == '>' || value[at] == 'x' || value[at] == 'o')) {
        arrow_end = value[at] == '>';
        invalid_end = !arrow_end;
        ++at;
    }
    result.state = (invalid_start || invalid_end || (result.arrow_start && !arrow_end))
                       ? LinkScan::Unsupported
                       : LinkScan::Complete;
    result.stroke = Stroke::Dotted;
    result.arrow_end = arrow_end;
    // The primary FlowDB measures dotted rank length by dots, not hyphens.
    result.minimum_length =
        static_cast<int>(std::ranges::count(value.substr(dots, at - dots), '.'));
    result.consumed = link_space_after(value, at);
    return result;
}

LinkLexeme complete_flow_link(std::string_view value) {
    const auto dotted = complete_dotted_link(value);
    if (dotted.state != LinkScan::NoMatch) return dotted;
    return complete_link(value);
}

struct StartLink {
    LinkScan state = LinkScan::NoMatch;
    Stroke stroke = Stroke::Solid;
    bool arrow_start = false;
    std::size_t consumed = 0;
};

StartLink start_link(std::string_view value) {
    StartLink result;
    if (value.empty()) return result;
    std::size_t at = 0;
    bool invalid_start = false;
    if (value[at] == '<') {
        result.arrow_start = true;
        ++at;
    } else if (value[at] == 'x' || value[at] == 'o') {
        invalid_start = true;
        ++at;
    }
    if (at + 1u >= value.size()) return {};
    if (value.substr(at, 2u) == "--")
        result.stroke = Stroke::Solid;
    else if (value.substr(at, 2u) == "==")
        result.stroke = Stroke::Thick;
    else if (value.substr(at, 2u) == "-.")
        result.stroke = Stroke::Dotted;
    else
        return {};
    result.state = invalid_start ? LinkScan::Unsupported : LinkScan::Complete;
    result.consumed = link_space_after(value, at + 2u);
    return result;
}

std::optional<std::pair<std::string_view, LinkLexeme>>
edge_text_until_link(std::string_view value, const StartLink& start) {
    bool quoted = false;
    bool escaped = false;
    for (std::size_t at = 0; at < value.size(); ++at) {
        const auto ch = value[at];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (quoted && ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            quoted = !quoted;
            continue;
        }
        if (quoted) continue;
        const auto link = complete_flow_link(value.substr(at));
        if (link.state == LinkScan::Unsupported) return {};
        if (link.state == LinkScan::Complete) {
            if (link.stroke != start.stroke || link.arrow_start) return {};
            return std::pair{value.substr(0, at), link};
        }
        // Jison edgeText may contain a single '-' or '=', but a possible
        // complete-link prefix cannot be swallowed as prose after it fails.
        const auto tail = value.substr(at);
        if (tail.starts_with("--") || tail.starts_with("==") || tail.starts_with("-.") ||
            ((ch == '<' || ch == 'x' || ch == 'o') &&
             (tail.substr(1).starts_with("--") || tail.substr(1).starts_with("==") ||
              tail.substr(1).starts_with("-."))))
            return {};
        // In dottedEdgeText, a dot run that did not produce LINK has no
        // internal dotted-LINK start: the primary rule needs its terminal '-'.
        // Skip it once so an unclosed run cannot trigger suffix rescans.
        if (ch == '.') {
            std::size_t end = at;
            while (end < value.size() && value[end] == '.')
                ++end;
            at = end - 1u;
        }
    }
    return {};
}

struct Group {
    std::string id, text;
    int parent = -1;
    std::optional<Direction> direction;
    std::size_t order = 0;
    bool implicit_id = false;
    // FS-7a: a shell pre-registered by the forward-reference pre-scan. It
    // holds only the authored id so edges can target the group before its
    // `subgraph` statement activates it; activation fills the remaining
    // fields. A successful parse never leaves a shell deferred.
    bool deferred = false;
    // Set when an edge endpoint resolves to the group while it is still a
    // deferred shell. Such a group may be declared exactly once: composing
    // pre-declaration edges with the reopen identity-merge path is a separate
    // reviewed interaction and stays whole-source rejected (its positive
    // Model supplement is a standalone milestone).
    bool forward_referenced = false;
    // FS-7b: a trailing `id@{ view: collapsed }` directive collapses the
    // group to a single titled node. Appended as the LAST data member: the
    // positional aggregate initializers below rely on member order, and a
    // same-typed bool inserted earlier would silently rebind their trailing
    // arguments.
    bool collapsed = false;
};
// FS-7b: the new trailing bool must stay inside the existing struct padding
// so the sizeof-driven prepay and exact-budget tests cannot move.
static_assert(sizeof(Group) == 80, "Group layout is part of the Work-budget contract");
enum class FlowStyleKind { Definition, Assignment, Direct };

struct FlowStyleRecord {
    FlowStyleKind kind = FlowStyleKind::Definition;
    std::string name, physical;
    std::vector<std::string> targets;
    ClassStyleProperties properties;
    bool used = false;
};

struct FlowStyles {
    std::vector<FlowStyleRecord> records;
    std::vector<ClassStyleProperties> effective;
    std::vector<std::string> legend;
    std::size_t definitions = 0u, assignments = 0u, direct = 0u, applications = 0u;
};

struct Diagram {
    Direction direction = Direction::Down;
    std::vector<Vertex> vertices;
    std::vector<Edge> edges;
    std::vector<Group> groups;
    std::vector<std::size_t> group_stack;
    std::size_t next_order = 0;
    std::size_t group_counter = 0;
    bool has_group_endpoint = false;
    std::optional<std::size_t> vertex(std::string id, Work& work,
                                      bool& cross_owner_membership_request) {
        (void)work;  // RFC 0031 E5: per-vertex bookkeeping folded into the
                     // whole-body flow_parse_prepay (both profiles).
        if (std::ranges::any_of(groups, [&](const Group& group) { return group.id == id; }))
            return {};
        for (std::size_t n = 0; n < vertices.size(); ++n)
            if (vertices[n].id == id) {
                if (!group_stack.empty() && vertices[n].group >= 0 &&
                    vertices[n].group != static_cast<int>(group_stack.back()))
                    cross_owner_membership_request = true;
                if (!group_stack.empty() && vertices[n].group < 0)
                    vertices[n].group = static_cast<int>(group_stack.back());
                return n;
            }
        if (vertices.size() >= 64u) return {};
        vertices.push_back({id, id, Shape::Rectangle, false, false,
                            group_stack.empty() ? -1 : static_cast<int>(group_stack.back()),
                            next_order++});
        return vertices.size() - 1;
    }
};

bool flow_envelope_parse_prepay(std::string_view body, Work& work);

bool flow_style_identifier(std::string_view value) {
    if (value.empty() || value.size() > 64u || !id_start(value.front())) return false;
    return std::ranges::all_of(value, [](char c) { return id_char(c); });
}

// Constant root-keyword dispatch is covered by the existing coarse parse prepay.
bool flow_style_keyword(std::string_view statement) {
    for (const auto word :
         {std::string_view("classDef"), std::string_view("class"), std::string_view("style")})
        if (statement.size() > word.size() && statement.starts_with(word) &&
            (statement[word.size()] == ' ' || statement[word.size()] == '\t'))
            return true;
    return false;
}

bool flow_style_collect(std::string_view statement, std::string_view physical,
                        std::unique_ptr<FlowStyles>& pending, Work& work) {
    if (statement.size() > 1024u || physical.size() > 1024u ||
        !work.spend(physical.size() * 32u + 4u * sizeof(FlowStyleRecord) +
                    4u * sizeof(std::string) + 256u))
        return false;
    for (const auto c : physical)
        if ((static_cast<unsigned char>(c) < 0x20u && c != '\t') ||
            static_cast<unsigned char>(c) == 0x7fu)
            return false;
    auto logical = trim(physical);
    if (logical.ends_with(';')) logical = trim(logical.substr(0u, logical.size() - 1u));
    if (logical != statement) return false;
    if (!pending) {
        if (!work.spend(4u * sizeof(FlowStyles) + 4u * 4u * sizeof(FlowStyleRecord) + 256u))
            return false;
        pending = std::make_unique<FlowStyles>();
        pending->records.reserve(4u);
    }
    FlowStyleRecord record;
    record.physical = physical;
    auto rest = statement;
    if (rest.starts_with("classDef")) {
        if (pending->definitions >= 64u) return false;
        record.kind = FlowStyleKind::Definition;
        rest = trim(rest.substr(8u));
        const auto split = rest.find_first_of(" \t");
        if (split == std::string_view::npos || !flow_style_identifier(rest.substr(0u, split)))
            return false;
        record.name = rest.substr(0u, split);
        if (record.name == "default") return false;
        for (const auto& previous : pending->records) {
            if (!work.spend(previous.name.size() + record.name.size() + 32u)) return false;
            if (previous.kind == FlowStyleKind::Definition && previous.name == record.name)
                return false;
        }
        if (!source_style_properties(trim(rest.substr(split)), record.properties, work))
            return false;
        ++pending->definitions;
    } else {
        const bool direct = rest.starts_with("style");
        record.kind = direct ? FlowStyleKind::Direct : FlowStyleKind::Assignment;
        if ((direct && pending->direct >= 64u) || (!direct && pending->assignments >= 64u))
            return false;
        rest = trim(rest.substr(5u));
        const auto split = rest.find_first_of(" \t");
        if (split == std::string_view::npos) return false;
        auto list = rest.substr(0u, split);
        auto tail = trim(rest.substr(split));
        const auto target_count =
            static_cast<std::size_t>(std::count(list.begin(), list.end(), ',')) + 1u;
        if (target_count > 64u ||
            !work.spend(target_count * 4u * sizeof(std::string) + list.size() * 8u + 1024u))
            return false;
        record.targets.reserve(target_count);
        while (!list.empty()) {
            const auto comma = list.find(',');
            const auto target = list.substr(0u, comma);
            if (!flow_style_identifier(target) || record.targets.size() >= 64u ||
                (direct && comma != std::string_view::npos))
                return false;
            for (const auto& previous : record.targets) {
                if (!work.spend(previous.size() + target.size() + 32u) || previous == target)
                    return false;
            }
            record.targets.emplace_back(target);
            if (comma == std::string_view::npos) break;
            list.remove_prefix(comma + 1u);
            if (list.empty()) return false;
        }
        if (record.targets.empty() || record.targets.size() > 128u - pending->applications)
            return false;
        if (direct) {
            if (!source_style_properties(tail, record.properties, work)) return false;
            ++pending->direct;
        } else {
            if (!flow_style_identifier(tail)) return false;
            record.name = tail;
            ++pending->assignments;
        }
        pending->applications += record.targets.size();
    }
    if (pending->records.size() == pending->records.capacity()) {
        const auto capacity = std::min<std::size_t>(192u, pending->records.capacity() * 2u);
        if (capacity <= pending->records.size() ||
            !work.spend(capacity * 4u * sizeof(FlowStyleRecord) +
                        pending->records.size() * sizeof(FlowStyleRecord) + 256u))
            return false;
        pending->records.reserve(capacity);
    }
    pending->records.push_back(std::move(record));
    return true;
}

bool resolve_flow_styles(FlowStyles& styles, const Diagram& diagram, Work& work) {
    if (!work.spend(diagram.vertices.size() * 4u * sizeof(ClassStyleProperties) + 256u))
        return false;
    styles.effective.resize(diagram.vertices.size());
    const auto find = [&](std::string_view id) -> std::optional<std::size_t> {
        for (std::size_t n = 0; n < diagram.vertices.size(); ++n) {
            if (!work.spend(id.size() + diagram.vertices[n].id.size() + 32u)) return {};
            if (diagram.vertices[n].id == id) {
                if (diagram.vertices[n].group >= 0 || diagram.vertices[n].shape != Shape::Rectangle)
                    return {};
                return n;
            }
        }
        return {};
    };
    for (const auto kind : {FlowStyleKind::Assignment, FlowStyleKind::Direct})
        for (auto& record : styles.records) {
            if (!work.spend(32u)) return false;
            if (record.kind != kind) continue;
            if (kind == FlowStyleKind::Assignment) {
                std::optional<std::size_t> definition;
                for (std::size_t n = 0; n < styles.records.size(); ++n) {
                    const auto& candidate = styles.records[n];
                    if (!work.spend(candidate.name.size() + record.name.size() + 32u)) return false;
                    if (candidate.kind == FlowStyleKind::Definition &&
                        candidate.name == record.name) {
                        definition = n;
                        break;
                    }
                }
                if (!definition) return false;
                styles.records[*definition].used = true;
                record.properties = styles.records[*definition].properties;
            }
            for (const auto& id : record.targets) {
                const auto selected = find(id);
                if (!selected) return false;
                auto& effective = styles.effective[*selected];
                const auto& properties = record.properties;
                if (properties.fill) effective.fill = properties.fill;
                if (properties.stroke) effective.stroke = properties.stroke;
                if (properties.color) effective.color = properties.color;
                if (properties.width) effective.width = properties.width;
            }
        }
    if (styles.applications == 0u) {
        styles.effective.clear();
        return true;
    }
    constexpr std::string_view projection_key =
        "key: hue roles use palette; width>=4=bold border; color on fill not projected; assignment "
        "order; direct last; normalized consecutive repeats keep first spelling; source TAB=\\t";
    if (!work.spend(8u * 4u * sizeof(std::string) + projection_key.size() * 8u +
                    4u * sizeof(std::vector<std::string>) + 256u))
        return false;
    styles.legend.reserve(8u);
    for (std::size_t n = 0; n < styles.records.size(); ++n) {
        const auto& record = styles.records[n];
        if (!work.spend(64u)) return false;
        if (record.kind == FlowStyleKind::Definition && !record.used) continue;
        std::size_t count = 1u;
        if (record.kind != FlowStyleKind::Definition) {
            while (n + count < styles.records.size()) {
                const auto& next = styles.records[n + count];
                if (!work.spend((record.name.size() + record.physical.size()) * 4u + 512u))
                    return false;
                if (next.kind != record.kind || next.name != record.name ||
                    next.targets != record.targets || next.properties != record.properties)
                    break;
                ++count;
            }
        }
        if (styles.legend.size() >= 7u || !work.spend(record.physical.size() * 12u + 4096u))
            return false;
        std::string text;
        text.reserve(record.physical.size() * 2u + 256u);
        for (const auto c : record.physical) {
            if (c == '\t')
                text += "\\t";
            else
                text += c;
        }
        text += " ->";
        const auto append = [&](std::string_view key, std::string_view value) {
            text += ' ';
            text += key;
            text += ':';
            text += value;
        };
        if (record.kind == FlowStyleKind::Definition) {
            append("definition", record.name);
            text += " (applied)";
        } else {
            if (record.kind == FlowStyleKind::Assignment) append("class", record.name);
            const auto& properties = record.properties;
            if (properties.fill)
                append("fill", properties.fill->transparent
                                   ? std::string_view("none")
                                   : source_style_role_name(properties.fill->role));
            if (properties.stroke)
                append("stroke", source_style_role_name(properties.stroke->role));
            if (properties.width) append("width", *properties.width >= 4.0 ? "bold" : "normal");
            if (properties.color) {
                append("fg", source_style_role_name(properties.color->role));
                text += "(fill targets use palette.text)";
            }
            if (count > 1u) {
                text += " occurrences:";
                text += std::to_string(count);
            }
        }
        if (ftxui::string_width(text) > kWidthLimit) return false;
        styles.legend.push_back(std::move(text));
        n += count - 1u;
    }
    styles.legend.emplace_back(projection_key);
    return true;
}

bool flow_parse_prepay(std::string_view body, Work& work, bool full_profile,
                       bool upgrade_to_full = false);

struct GroupHeader {
    std::optional<std::string> id;
    std::string title;
    std::vector<detail::LabelRun> rich;
};

std::optional<GroupHeader> flow_group_header(Scanner& group) {
    group.space();
    const auto source = group.rest;
    if (source.empty()) return {};
    std::optional<std::string> id;
    std::optional<RichLabel> title;
    if (source.front() != '"') {
        id = group.id();
        if (id) {
            group.space();
            if (group.take("[")) {
                title = group.enclosed(']');
                group.space();
                if (!title || !group.rest.empty()) return {};
            }
        }
    }
    if (!title) {
        // The primary Flow DB receives the same id/title object for a plain
        // whitespace title, then assigns a subGraphN identity at completion.
        title = label(source);
        if (!title) return {};
        group.rest = {};
        if (title->text.find_first_of(" \t") != std::string::npos)
            id.reset();
        else
            id = title->text;
    }
    return GroupHeader{std::move(id), std::move(title->text), std::move(title->runs)};
}

// FS-7a: collect the authored ids of every `subgraph` statement before the
// main parse, so an edge may target a group whose declaration appears later
// in the source (mermaid/dagre resolve cluster edges after the full parse).
// The scan reuses flow_group_header unchanged, accepting exactly the headers
// the real parser accepts and registering only fixed-id forms; quoted or
// multi-word titles keep their generated subGraphN identity and can never be
// forward-referenced.
//
// Each physical line is first split into top-level statements with the same
// quote/bracket-aware ';' boundary parse() uses, so the text handed to
// flow_group_header is byte-identical to what flow_statement later receives.
// That matters for trailing-semicolon headers (`subgraph G;`,
// `subgraph G [T];`): feeding the whole physical line would register a
// mismatched shell (or hard-fail) and regress a source the main parser
// admits. Bounded temporary work (at most kGroupLimit headers and one
// GroupHeader per line) is covered by the whole-body flow_parse_prepay
// charged before the pre-scan runs; the scan allocates no per-edge state.
std::optional<std::vector<std::string>> flow_prescan_group_ids(std::string_view source) {
    std::vector<std::string> ids;
    auto scan_statement = [&](std::string_view statement) -> bool {
        statement = trim(statement);
        if (!statement.starts_with("subgraph ") &&
            !statement.starts_with("subgraph\t"))
            return true;
        Scanner scanner{statement};
        scanner.take("subgraph");
        auto header = flow_group_header(scanner);
        if (!header) return false;
        if (header->id && std::ranges::find(ids, *header->id) == ids.end()) {
            if (ids.size() >= kGroupLimit) return false;
            ids.push_back(*header->id);
        }
        return true;
    };
    while (!source.empty()) {
        const auto nl = source.find('\n');
        auto line = source.substr(0, nl);
        source = nl == std::string_view::npos ? std::string_view{} : source.substr(nl + 1);
        if (line.ends_with('\r')) line.remove_suffix(1u);
        line = trim(line);
        if (line.empty() || line.starts_with("%%")) continue;
        bool quoted = false, escaped = false;
        char close = 0;
        std::size_t start = 0;
        for (std::size_t at = 0; at <= line.size(); ++at) {
            const char ch = at == line.size() ? '\0' : line[at];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (quoted && ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                quoted = !quoted;
                continue;
            }
            if (!quoted) {
                if (close && ch == close)
                    close = 0;
                else if (!close &&
                         (ch == '[' || ch == '(' || ch == '{' || ch == '|' ||
                          (ch == '>' &&
                           (at == 0u || (line[at - 1u] != '-' && line[at - 1u] != '=')))))
                    close = ch == '[' || ch == '>' ? ']'
                          : ch == '('             ? ')'
                          : ch == '{'             ? '}'
                                                  : '|';
            }
            if (ch != '\0' && (ch != ';' || close || quoted)) continue;
            // An unterminated quote/bracket at the boundary rejects the
            // pre-scan, matching the main parser's whole-source failure.
            if (quoted || close || escaped) return {};
            if (!scan_statement(line.substr(start, at - start))) return {};
            start = at + 1;
        }
    }
    return ids;
}

// FS-aws: fixed marker cell (U+25A0 BLACK SQUARE, the same glyph FS-6 uses
// for FontAwesome) plus the separator before the human text.
constexpr std::string_view kAwsIconMarker = "\xE2\x96\xA0 ";

struct FlowShapeData {
    Shape shape = Shape::Rectangle;
    std::optional<std::string> label;
    // True only when the block explicitly carried `shape:`. A label-only
    // `@{ label: }` block leaves the node's existing bracket shape intact.
    bool shape_specified = false;
    // FS-aws: a closed-vocabulary `icon: "aws:..."` marker. The terminal has
    // no icon font, so the node keeps its rectangle and the icon projects to
    // one fixed marker cell prepended to the human text; an unknown icon or
    // non-aws namespace fails closed.
    bool icon = false;
};

// Reads a shape-data label value, which mermaid delimits with either a
// single or double quote. Returns the decoded label and advances past the
// closing quote, or nullopt on a missing/unterminated/escaped value. Both
// quote styles share one strict rule: no backslash escapes and no embedded
// double quote, so the plain-label validator never sees an unbalanced
// quote. Decoding the unquoted inner text via label() also trims
// surrounding whitespace; no pinned corpus label carries such padding, so
// accepted output matches the prior double-quote path on every fixture.
// (The AWS icon *name* is not routed through here; that fixed vocabulary
// stays double-quoted.)
std::optional<std::string> read_shape_data_label(Scanner& scanner) {
    if (scanner.rest.empty()) return {};
    const char quote = scanner.rest.front();
    if (quote != '"' && quote != '\'') return {};
    const auto close = scanner.rest.find(quote, 1u);
    if (close == std::string_view::npos) return {};
    const auto inner = scanner.rest.substr(1u, close - 1u);
    if (inner.find('\\') != std::string_view::npos ||
        inner.find('"') != std::string_view::npos)
        return {};
    const auto decoded = label(inner);
    if (!decoded) return {};
    scanner.rest.remove_prefix(close + 1u);
    return decoded->text;
}

std::optional<FlowShapeData> flow_shape_data(Scanner& scanner, Work& work) {
    // RFC 0031 E5: this closed fixed-token production is covered by the
    // whole-body flow_parse_prepay fee.
    (void)work;
    if (!scanner.take("@{")) return {};
    scanner.space();
    // FS-5a: label-only metadata `@{ label: "text" }` with no shape key is a
    // rectangular labeled node. Accept an optional space before the colon and
    // whitespace around the quoted value; anything else (icon, animation, a
    // second key, a non-rectangle shape) falls through to the shape
    // production below and fails closed.
    // `take("label")` is a raw prefix match; the immediately following
    // space()+required `:` below is what distinguishes it from keys like
    // "labelled"/"labelX" — do not insert any acceptance between them.
    if (scanner.take("label")) {
        scanner.space();
        if (!scanner.take(":")) return {};
        scanner.space();
        auto shape_label = read_shape_data_label(scanner);
        if (!shape_label) return {};
        scanner.space();
        if (!scanner.take("}")) return {};
        return FlowShapeData{Shape::Rectangle, std::move(shape_label), false};
    }
    // FS-aws: `@{ icon: "aws:<name>" [, label: "text"] }`. Only the pinned
    // AWS architecture vocabulary is admitted; the icon projects to a fixed
    // marker cell (the terminal has no icon font) while the human label or,
    // absent one, the node id is retained. Any other namespace, an unknown
    // icon, or any extra key (shape/animation/second icon) fails closed.
    if (scanner.take("icon")) {
        constexpr std::string_view kAwsIconNames[] = {
            "arch-amazon-cloudwatch", "arch-amazon-eks-cloud", "arch-amazon-route-53"};
        scanner.space();
        if (!scanner.take(":")) return {};
        scanner.space();
        if (scanner.rest.empty() || scanner.rest.front() != '"') return {};
        const auto quote_end = scanner.rest.find('"', 1u);
        if (quote_end == std::string_view::npos) return {};
        const auto quoted = scanner.rest.substr(1u, quote_end - 1u);
        constexpr std::string_view kAwsPrefix = "aws:";
        if (!quoted.starts_with(kAwsPrefix) ||
            quoted.find_first_of("\\`") != std::string_view::npos ||
            std::ranges::find(kAwsIconNames, quoted.substr(kAwsPrefix.size())) ==
                std::end(kAwsIconNames))
            return {};
        scanner.rest.remove_prefix(quote_end + 1u);
        scanner.space();
        std::optional<std::string> icon_label;
        if (scanner.take(",")) {
            scanner.space();
            if (!scanner.take("label")) return {};
            scanner.space();
            if (!scanner.take(":")) return {};
            scanner.space();
            icon_label = read_shape_data_label(scanner);
            if (!icon_label) return {};
            scanner.space();
        }
        if (!scanner.take("}")) return {};
        return FlowShapeData{Shape::Rectangle, std::move(icon_label), false, true};
    }
    if (!scanner.take("shape")) return {};
    scanner.space();
    if (!scanner.take(":")) return {};
    scanner.space();
    const auto token = scanner.id();
    if (!token) return {};
    scanner.space();
    Shape shape;
    if (*token == "rect" || *token == "process" || *token == "proc" || *token == "rectangle")
        shape = Shape::Rectangle;
    else if (*token == "rounded" || *token == "event")
        shape = Shape::Rounded;
    else if (*token == "sm-circ" || *token == "small-circle" || *token == "start")
        shape = Shape::SmallCircle;
    else if (*token == "diam" || *token == "decision" || *token == "diamond")
        shape = Shape::Decision;
    else if (*token == "stadium" || *token == "pill" || *token == "terminal")
        shape = Shape::Stadium;
    else if (*token == "db" || *token == "database" || *token == "cylinder" || *token == "cyl")
        shape = Shape::Cylinder;
    else if (*token == "fr-rect" || *token == "subproc" || *token == "subprocess" ||
             *token == "framed-rectangle" || *token == "subroutine")
        shape = Shape::Subroutine;
    else if (*token == "hex" || *token == "hexagon" || *token == "prepare")
        shape = Shape::Hexagon;
    else if (*token == "lean-l" || *token == "lean-left" || *token == "out-in")
        shape = Shape::Parallelogram;
    else if (*token == "lean-r" || *token == "lean-right" || *token == "in-out")
        shape = Shape::ParallelogramAlt;
    else if (*token == "trap-b" || *token == "trapezoid-bottom" || *token == "priority")
        shape = Shape::Trapezoid;
    else if (*token == "trap-t" || *token == "trapezoid-top" || *token == "manual")
        shape = Shape::TrapezoidAlt;
    else if (*token == "dbl-circ" || *token == "double-circle")
        shape = Shape::DoubleCircle;
    else if (*token == "extract" || *token == "tri" || *token == "triangle")
        shape = Shape::Triangle;
    else if (*token == "flip-tri" || *token == "manual-file" ||
             *token == "flipped-triangle")
        shape = Shape::FlippedTriangle;
    else if (*token == "sl-rect" || *token == "manual-input" ||
             *token == "sloped-rectangle")
        shape = Shape::SlopedRectangle;
    else if (*token == "win-pane" || *token == "internal-storage" ||
             *token == "window-pane")
        shape = Shape::WindowPane;
    else if (*token == "card" || *token == "notch-rect" ||
             *token == "notched-rectangle")
        shape = Shape::Card;
    else if (*token == "folder" || *token == "directory")
        shape = Shape::Folder;
    else if (*token == "h-cyl" || *token == "das" ||
             *token == "horizontal-cylinder")
        shape = Shape::HorizontalCylinder;
    else if (*token == "lin-cyl" || *token == "disk" ||
             *token == "lined-cylinder")
        shape = Shape::LinedCylinder;
    else if (*token == "datastore" || *token == "data-store")
        shape = Shape::Datastore;
    else if (*token == "notch-pent" || *token == "loop-limit" ||
             *token == "notched-pentagon")
        shape = Shape::NotchedPentagon;
    else if (*token == "collate" || *token == "hourglass")
        shape = Shape::Hourglass;
    else if (*token == "bolt" || *token == "com-link" ||
             *token == "lightning-bolt")
        shape = Shape::Bolt;
    else if (*token == "bow-rect" || *token == "stored-data" ||
             *token == "bow-tie-rectangle")
        shape = Shape::BowTie;
    else if (*token == "comment" || *token == "brace-l")
        shape = Shape::Brace;
    else if (*token == "procs" || *token == "processes" ||
             *token == "st-rect" || *token == "stacked-rectangle")
        shape = Shape::StackedRectangle;
    else if (*token == "tag-rect" || *token == "tag-proc" ||
             *token == "tagged-rectangle" || *token == "tagged-process")
        shape = Shape::TaggedRectangle;
    else if (*token == "doc" || *token == "document")
        shape = Shape::Document;
    else if (*token == "docs" || *token == "documents" ||
             *token == "st-doc" || *token == "stacked-document")
        shape = Shape::StackedDocument;
    else if (*token == "lin-doc" || *token == "lined-document")
        shape = Shape::LinedDocument;
    else if (*token == "tag-doc" || *token == "tagged-document")
        shape = Shape::TaggedDocument;
    else if (*token == "f-circ" || *token == "junction" ||
             *token == "filled-circle")
        shape = Shape::FilledCircle;
    else if (*token == "fr-circ" || *token == "framed-circle" ||
             *token == "stop")
        shape = Shape::FramedCircle;
    else if (*token == "cross-circ" || *token == "summary" ||
             *token == "crossed-circle")
        shape = Shape::CrossedCircle;
    else if (*token == "lin-rect" || *token == "lined-rectangle" ||
             *token == "lin-proc" || *token == "lined-process" ||
             *token == "shaded-process")
        shape = Shape::ShadedProcess;
    else if (*token == "curv-trap" || *token == "curved-trapezoid" ||
             *token == "display")
        shape = Shape::CurvedTrapezoid;
    else if (*token == "flag" || *token == "paper-tape")
        shape = Shape::PaperTape;
    else if (*token == "delay" || *token == "half-rounded-rectangle")
        shape = Shape::Delay;
    else if (*token == "fork" || *token == "join")
        shape = Shape::ForkJoin;
    else if (*token == "div-rect" || *token == "div-proc" ||
             *token == "divided-rectangle" || *token == "divided-process")
        shape = Shape::DividedRectangle;
    else
        return {};
    std::optional<std::string> shape_label;
    if (scanner.take(",")) {
        scanner.space();
        if (!scanner.take("label")) return {};
        scanner.space();
        if (!scanner.take(":")) return {};
        scanner.space();
        shape_label = read_shape_data_label(scanner);
        if (!shape_label) return {};
        scanner.space();
    }
    if (!scanner.take("}")) return {};
    return FlowShapeData{shape, std::move(shape_label), true};
}

bool classic_opener(std::string_view text) {
    return text.starts_with("((") || text.starts_with("([") || text.starts_with("[[") ||
           text.starts_with("[(") || text.starts_with("{{") || text.starts_with(">") ||
           text.starts_with("[/") || text.starts_with("[\\");
}

struct ClassicDeclaration {
    Shape shape;
    RichLabel label;
};

std::optional<ClassicDeclaration> classic_declaration(Scanner& scanner, Work& work) {
    // RFC 0031 E5: the eleven bracket forms are a closed Flow production; the
    // allocation-free close-delimiter scan is input-bounded and folded into
    // the whole-body flow_parse_prepay fee.
    (void)work;

    struct Form {
        std::string_view open, close;
        Shape shape;
    };

    // Prefix forms are ordered longest-first. The bracket/slash alternatives
    // stay explicit: this is not an alias table or a generic metadata parser.
    constexpr std::array<Form, 11> forms{{
        {"(((", ")))", Shape::DoubleCircle},
        {"((", "))", Shape::Circle},
        {"([", "])", Shape::Stadium},
        {"[(", ")]", Shape::Cylinder},
        {"[[", "]]", Shape::Subroutine},
        {"{{", "}}", Shape::Hexagon},
        {">", "]", Shape::Asymmetric},
        {"[/", "\\]", Shape::Trapezoid},
        {"[\\", "/]", Shape::TrapezoidAlt},
        {"[/", "/]", Shape::Parallelogram},
        {"[\\", "\\]", Shape::ParallelogramAlt},
    }};
    const auto selected = std::ranges::find_if(
        forms, [&](const Form& form) { return scanner.rest.starts_with(form.open); });
    if (selected == forms.end()) return {};
    const auto body = scanner.rest.substr(selected->open.size());
    bool quoted = false, escaped = false;
    for (std::size_t at = 0; at < body.size(); ++at) {
        const auto ch = body[at];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (quoted && ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            quoted = !quoted;
            continue;
        }
        if (quoted) continue;
        // Shared openers own the first complete unquoted close, regardless of
        // which alternative appears first in the grammar table. A later node's
        // close cannot steal this declaration's earlier valid boundary.
        for (const auto& form : forms) {
            if (form.open != selected->open || !body.substr(at).starts_with(form.close)) continue;
            auto text = label(body.substr(0, at), true);
            if (!text) return {};
            scanner.rest = body.substr(at + form.close.size());
            return ClassicDeclaration{form.shape, std::move(*text)};
        }
    }
    return {};
}

std::optional<std::size_t> flow_node_from_id(std::string id, Scanner& scanner, Diagram& graph,
                                             Work& work, bool& cross_owner_membership_request,
                                             RichLabels* rich) {
    // Unsupported structural keywords must not turn into ordinary nodes.
    constexpr std::string_view reserved[]{"end",       "subgraph", "direction", "style",
                                          "linkStyle", "classDef", "class",     "click"};
    if (std::ranges::find(reserved, id) != std::end(reserved)) return {};
    auto index = graph.vertex(std::move(id), work, cross_owner_membership_request);
    if (!index) return {};
    scanner.space();
    if (scanner.rest.empty()) return index;
    if (scanner.rest.starts_with("@{")) {
        const auto metadata = flow_shape_data(scanner, work);
        if (!metadata) return {};
        auto& node = graph.vertices[*index];
        // Explicit `shape:` metadata is last-wins; a label-only block leaves
        // the shape the node (possibly declared earlier) already has.
        if (metadata->shape_specified) node.shape = metadata->shape;
        if (metadata->label) {
            // Compare against the logical label: an earlier icon block has
            // already prepended the marker cell to node.text.
            const auto logical =
                node.icon && node.text.starts_with(kAwsIconMarker)
                    ? std::string_view{node.text}.substr(kAwsIconMarker.size())
                    : std::string_view{node.text};
            if (node.explicit_label && logical != *metadata->label) return {};
            node.text = std::move(*metadata->label);
            node.explicit_label = true;
        }
        // FS-aws: surface the admitted icon as one fixed marker cell in front
        // of the human text (the given label, otherwise the node id). The
        // Vertex.icon flag keeps repeated icon statements idempotent; a label
        // reassignment on an already-marked node re-applies the marker.
        if (metadata->icon && (!node.icon || static_cast<bool>(metadata->label))) {
            node.icon = true;
            node.text = std::string{kAwsIconMarker} + node.text;
        }
        return index;
    }
    if (classic_opener(scanner.rest)) {
        const auto declaration = classic_declaration(scanner, work);
        if (!declaration) return {};
        auto& node = graph.vertices[*index];
        if (node.explicit_label &&
            (node.text != declaration->label.text || node.shape != declaration->shape))
            return {};
        node.text = declaration->label.text;
        if (rich && !rich->bind_vertex(*index, std::move(declaration->label.runs), work)) return {};
        node.shape = declaration->shape;
        node.explicit_label = true;
        scanner.space();
        // Shape metadata has its independently closed three-token grammar. A
        // classic bracket declaration never flattens to its old vocabulary.
        if (scanner.rest.starts_with("@{")) return {};
        return index;
    }
    const auto open = scanner.rest.front();
    if (open != '[' && open != '(' && open != '{') return index;
    scanner.rest.remove_prefix(1);
    // These are distinct official shape openers, not punctuation inside the
    // supported rectangular/rounded labels. Quoted literal labels still work.
    if (!scanner.rest.empty() &&
        ((open == '[' && (scanner.rest.front() == '/' || scanner.rest.front() == '\\')) ||
         (open == '(' && scanner.rest.front() == '-')))
        return {};
    const auto shape = open == '['   ? Shape::Rectangle
                       : open == '(' ? Shape::Rounded
                                     : Shape::Decision;
    auto labeled = scanner.enclosed(open == '[' ? ']' : open == '(' ? ')' : '}');
    if (!labeled) return {};
    auto& node = graph.vertices[*index];
    if (node.explicit_label && (node.text != labeled->text || node.shape != shape)) return {};
    node.text = std::move(labeled->text);
    if (rich && !rich->bind_vertex(*index, std::move(labeled->runs), work)) return {};
    node.shape = shape;
    node.explicit_label = true;
    scanner.space();
    if (scanner.rest.starts_with("@{")) {
        const auto metadata = flow_shape_data(scanner, work);
        // FS-aws: icon markers are projected only through the bare-id shape
        // data path; a bracket-declared node carrying icon metadata has no
        // marker injection site here, so reject it instead of silently
        // dropping the icon.
        if (!metadata || metadata->icon ||
            (metadata->label && *metadata->label != node.text))
            return {};
        // Explicit `shape:` metadata wins; a label-only block must not
        // flatten the bracket shape this vertex already declared.
        if (metadata->shape_specified) node.shape = metadata->shape;
    }
    return index;
}

std::optional<Endpoint> flow_endpoint(Scanner& scanner, Diagram& graph, Work& work,
                                      bool& cross_owner_membership_request, RichLabels* rich) {
    auto id = scanner.id();
    if (!id) return {};
    // Only an exact, already-declared Group.id has group endpoint meaning.
    // The comparison charge precedes each scan; titles and implicit ids never
    // participate because the parser keeps no title lookup table.
    for (std::size_t index = 0u; index < graph.groups.size(); ++index) {
        auto& group = graph.groups[index];
        if (group.id != *id) continue;
        scanner.space();
        // A group name is an endpoint only. It cannot acquire a vertex shape,
        // metadata, or compound endpoint role; a later link may make it a
        // terminal endpoint, while flow_statement rejects a bare group line.
        if (scanner.rest.starts_with("&") || scanner.rest.starts_with("@{") ||
            classic_opener(scanner.rest) || scanner.rest.starts_with("[") ||
            scanner.rest.starts_with("(") || scanner.rest.starts_with("{"))
            return {};
        // Record a pre-declaration edge so the reopen path can keep the
        // forward-reference + identity-merge combination fail-closed.
        if (group.deferred) group.forward_referenced = true;
        return Endpoint{EndpointKind::Group, index};
    }
    const auto vertex =
        flow_node_from_id(std::move(*id), scanner, graph, work, cross_owner_membership_request,
                          rich);
    return vertex ? std::optional<Endpoint>{Endpoint{EndpointKind::Vertex, *vertex}} : std::nullopt;
}

bool flow_statement(std::string_view text, Diagram& graph, Work& work, std::string_view original,
                    bool already_prepaid, bool& saw_reopen, bool& cross_owner_membership_request,
                    RichLabels* rich = nullptr) {
    // RFC 0031 E5: the former bounded statement-local fee is covered by the
    // whole-body flow_parse_prepay (bare or configured profile) charged at
    // parse() entry; only the reopen profile activation below still charges.
    if (text == "end") {
        if (graph.group_stack.empty()) return false;
        const auto completed_index = graph.group_stack.back();
        auto& completed = graph.groups[completed_index];
        if (completed.implicit_id) {
            auto generated = "subGraph" + std::to_string(graph.group_counter);
            if (std::ranges::any_of(graph.vertices,
                                    [&](const Vertex& vertex) { return vertex.id == generated; }) ||
                std::ranges::any_of(graph.groups, [&](const Group& candidate) {
                    return &candidate != &completed && candidate.id == generated;
                }))
                return false;
            completed.id = std::move(generated);
        }
        ++graph.group_counter;
        graph.group_stack.pop_back();
        return true;
    }
    if (text.starts_with("subgraph ") || text.starts_with("subgraph\t")) {
        Scanner group{text};
        group.take("subgraph");
        if (graph.group_stack.size() >= kGroupDepthLimit) return false;
        auto header = flow_group_header(group);
        if (!header || (header->id && std::ranges::any_of(graph.vertices, [&](const Vertex& v) {
                            return v.id == *header->id;
                        })))
            return false;
        std::optional<std::size_t> existing;
        if (header->id)
            for (std::size_t n = 0; n < graph.groups.size(); ++n) {
                if (graph.groups[n].id == *header->id) {
                    existing = n;
                    break;
                }
            }
        if (existing) {
            auto& existing_group = graph.groups[*existing];
            if (existing_group.deferred) {
                // First activation of a forward-reference shell created by
                // the pre-scan. Fill the fields the edge parser could not know
                // and enter the normal group stack; no reopen fee is owed
                // because the shell has never been an active container.
                existing_group.deferred = false;
                existing_group.text = std::move(header->title);
                existing_group.parent =
                    graph.group_stack.empty() ? -1 : static_cast<int>(graph.group_stack.back());
                existing_group.order = graph.next_order++;
                existing_group.implicit_id = false;
                if (rich && !rich->bind_group(*existing, header->rich, work)) return false;
                graph.group_stack.push_back(*existing);
                return true;
            }
            const int parent =
                graph.group_stack.empty() ? -1 : static_cast<int>(graph.group_stack.back());
            // A forward-referenced group is admitted only for a single
            // declaration; reopening it would compose pre-declaration edge
            // identity with the reopen merge, which has no reviewed positive
            // Model, so the whole source stays on ordinary fallback.
            if (existing_group.forward_referenced || existing_group.implicit_id ||
                existing_group.parent != parent ||
                std::ranges::find(graph.group_stack, *existing) != graph.group_stack.end())
                return false;
            // Only a newly activated bare repeat pays the delta to the
            // full-source profile, instead of the whole full bound on top of
            // the already-paid bare profile. Earlier live elements are
            // included in its future relocation bound.
            if (!saw_reopen && !already_prepaid) {
                if (!flow_parse_prepay(original, work, true, true)) return false;
            }
        } else if (graph.groups.size() >= kGroupLimit)
            return false;
        if (existing) {
            if (graph.groups[*existing].text != header->title) return false;
            if (rich) {
                const auto& bound = *existing < rich->group.size() ? rich->group[*existing]
                                                                    : std::vector<detail::LabelRun>{};
                if (bound.empty() != header->rich.empty()) return false;
            }
            saw_reopen = true;
            graph.group_stack.push_back(*existing);
            return true;
        }
        const bool implicit_id = !header->id;
        graph.groups.push_back(
            {header->id ? std::move(*header->id) : std::string{},
             std::move(header->title),
             graph.group_stack.empty() ? -1 : static_cast<int>(graph.group_stack.back()),
             {},
             graph.next_order++,
             implicit_id});
        if (rich && !rich->bind_group(graph.groups.size() - 1u, header->rich, work))
            return false;
        graph.group_stack.push_back(graph.groups.size() - 1);
        return true;
    }
    if (text.starts_with("direction ") || text.starts_with("direction\t")) {
        if (graph.group_stack.empty()) return false;
        Scanner direction{text};
        direction.take("direction");
        direction.space();
        auto& value = graph.groups[graph.group_stack.back()].direction;
        if (direction.rest == "LR")
            value = Direction::Right;
        else if (direction.rest == "RL")
            value = Direction::Left;
        else if (direction.rest == "TD" || direction.rest == "TB")
            value = Direction::Down;
        else if (direction.rest == "BT")
            value = Direction::Up;
        else
            return false;
        return true;
    }
    // FS-7b: a trailing `<id>@{ view: collapsed }` directive collapses an
    // already-declared group to a single titled node. It is recognized only
    // when the id names a group (a vertex shape-data statement falls through
    // to the ordinary node path), only outside any open group, and only for a
    // non-deferred group. The sole key is `view` with a bare or
    // double-quoted `collapsed` value; body statements bypass the init
    // directive quote normalizer, so single quotes are not accepted. This is
    // a closed fixed-token production and spends no Work.
    {
        Scanner probe{text};
        const auto candidate = probe.id();
        probe.space();
        if (candidate && probe.rest.starts_with("@{")) {
            std::optional<std::size_t> group_index;
            for (std::size_t n = 0; n < graph.groups.size(); ++n)
                if (graph.groups[n].id == *candidate) {
                    group_index = n;
                    break;
                }
            if (group_index) {
                if (!graph.group_stack.empty()) return false;
                Scanner sc{text};
                if (sc.id() != *candidate) return false;
                sc.space();
                if (!sc.take("@{")) return false;
                sc.space();
                if (!sc.take("view")) return false;
                sc.space();
                if (!sc.take(":")) return false;
                sc.space();
                if (sc.rest.starts_with('"')) {
                    if (!sc.take("\"collapsed\"")) return false;
                } else if (!sc.take("collapsed")) {
                    return false;
                }
                sc.space();
                if (!sc.take("}")) return false;
                sc.space();
                if (!sc.rest.empty()) return false;
                auto& target = graph.groups[*group_index];
                if (target.deferred) return false;
                target.collapsed = true;  // idempotent on a well-formed repeat
                return true;
            }
        }
    }
    // FS-2: a `click` binding is a non-visual interaction statement; the
    // terminal has no interaction layer, so a well-formed binding is consumed
    // as a no-op (no node, edge, or canvas row). The bound id is not required
    // to name a declared vertex: an inert binding on an unknown id can create
    // no geometry, so it is intentionally still a no-op. Accepted shapes mirror
    // the Mermaid flow grammar:
    //   click <id> "<url>" ["<tooltip>"] [<target>]
    //   click <id> href "<url>" [<target>]
    //   click <id> <callback> [","? "<arg>"]...
    // Anything else (no binding, unterminated quote, trailing garbage, a bare
    // link target outside mermaid's set) fails closed and keeps the whole
    // source on ordinary fallback.
    if (text.starts_with("click ") || text.starts_with("click\t")) {
        Scanner click{text};
        click.take("click");
        click.space();
        const auto node = click.id();
        if (!node || node->empty()) return false;
        click.space();
        if (click.rest.empty()) return false;
        bool explicit_href = false;
        if (click.rest.starts_with("href") && click.rest.size() > 4u &&
            (click.rest[4] == ' ' || click.rest[4] == '\t')) {
            click.take("href");
            click.space();
            explicit_href = true;
        }
        auto quoted = [&]() -> bool {
            if (!click.rest.starts_with('"')) return false;
            // A standalone quoted token: the next unescaped quote terminates it
            // (enclosed_view treats quotes as paired escapes and is meant for
            // pipe-delimited edge labels, not ordinary strings).
            bool escaped = false;
            std::size_t at = 1u;
            for (; at < click.rest.size(); ++at) {
                const char ch = click.rest[at];
                if (escaped) {
                    escaped = false;
                    continue;
                }
                if (ch == '\\') {
                    escaped = true;
                    continue;
                }
                if (ch == '"') break;
            }
            if (at >= click.rest.size()) return false;  // unterminated quote
            click.rest.remove_prefix(at + 1u);
            click.space();
            return true;
        };
        if (click.rest.starts_with('"')) {
            // Hyperlink form: quoted URL, then at most one quoted tooltip, then
            // an optional bare link target restricted to mermaid's set.
            if (!quoted()) return false;
            if (click.rest.starts_with('"') && !quoted()) return false;
            if (!click.rest.empty()) {
                constexpr std::string_view kTargets[]{"_blank", "_self", "_parent", "_top"};
                const auto target = click.id();
                if (!target ||
                    std::ranges::find(kTargets, std::string_view{*target}) == std::end(kTargets))
                    return false;
                click.space();
            }
            return click.rest.empty();
        }
        if (explicit_href) return false;  // href requires a quoted URL
        // Callback form: bare callback name followed by zero or more quoted
        // arguments separated by an optional comma (both historical spellings
        // appear in the pinned corpus).
        const auto callback = click.id();
        if (!callback) return false;
        click.space();
        while (!click.rest.empty()) {
            static_cast<void>(click.take(","));
            click.space();
            if (!quoted()) return false;
        }
        return true;
    }
    Scanner scanner{text};
    bool uses_group_endpoint = false;
    auto nodes = [&]() -> std::optional<std::vector<Endpoint>> {
        constexpr std::size_t kEndpointSlots = 64u;
        std::vector<Endpoint> result;
        while (true) {
            const auto endpoint =
                flow_endpoint(scanner, graph, work, cross_owner_membership_request, rich);
            if (!endpoint || result.size() >= kEndpointSlots) return {};
            // A group id has a closed scalar grammar. Reject both `G & A`
            // and `A & G` before reserving or storing the mixed compound.
            if (endpoint->kind == EndpointKind::Group && !result.empty()) return {};
            if (result.size() == result.capacity()) {
                const auto capacity = result.capacity();
                const auto next_capacity =
                    capacity == 0u ? 1u : std::min(kEndpointSlots, capacity * 2u);
                if (next_capacity <= capacity) return {};
                result.reserve(next_capacity);
            }
            uses_group_endpoint = uses_group_endpoint || endpoint->kind == EndpointKind::Group;
            result.push_back(*endpoint);
            scanner.space();
            if (!scanner.take("&")) return result;
            if (endpoint->kind == EndpointKind::Group) return {};
        }
    };
    auto from = nodes();
    if (!from) return false;
    bool emitted_link = false;
    scanner.space();
    // RFC 0031 E5: the former bounded per-statement LINK/EDGE_TEXT scan and
    // label copies are covered by the whole-body flow_parse_prepay fee.
    while (!scanner.rest.empty()) {
        LinkLexeme link = complete_flow_link(scanner.rest);
        std::optional<RichLabel> edge_label;
        if (link.state == LinkScan::Unsupported) return false;
        if (link.state == LinkScan::Complete) {
            scanner.rest.remove_prefix(link.consumed);
            // Preserve pipe labels for every fully recognized stroke form.
            if (scanner.take("|")) {
                const auto raw = scanner.enclosed_view('|');
                if (!raw) return false;
                edge_label = label(*raw, true);
                if (!edge_label) return false;
            }
        } else {
            const auto start = start_link(scanner.rest);
            if (start.state != LinkScan::Complete) return false;
            scanner.rest.remove_prefix(start.consumed);
            const auto text_and_link = edge_text_until_link(scanner.rest, start);
            if (!text_and_link) return false;
            const auto raw = trim(text_and_link->first);
            if (raw.empty()) return false;
            edge_label = label(raw, true);
            if (!edge_label) return false;
            link = text_and_link->second;
            link.arrow_start = start.arrow_start;
            if (link.arrow_start && !link.arrow_end) return false;
            scanner.rest.remove_prefix(text_and_link->first.size() + link.consumed);
        }
        auto to = nodes();
        if (!to || from->size() > (128u - graph.edges.size()) / to->size()) return false;
        const auto carries_group =
            std::ranges::any_of(
                *from,
                [](const Endpoint& endpoint) { return endpoint.kind == EndpointKind::Group; }) ||
            std::ranges::any_of(
                *to, [](const Endpoint& endpoint) { return endpoint.kind == EndpointKind::Group; });
        // GroupEndpoint15 is a closed scalar spelling: its existing source
        // links are unlabelled, solid, one-way arrows with rank length one.
        // Other group-adjacent forms remain whole-source unsupported instead
        // of borrowing broader vertex link syntax.
        if (carries_group &&
            (from->size() != 1u || to->size() != 1u || edge_label || link.stroke != Stroke::Solid ||
             link.arrow_start || !link.arrow_end || link.minimum_length != 1))
            return false;
        graph.has_group_endpoint = graph.has_group_endpoint || carries_group;
        for (const auto a : *from)
            for (const auto b : *to) {
                graph.edges.push_back(
                    {a, b,
                     edge_label ? std::optional<std::string>{edge_label->text} : std::nullopt,
                     link.stroke, link.arrow_start, link.arrow_end, link.minimum_length});
                if (edge_label && rich &&
                    !rich->bind_edge(graph.edges.size() - 1u, edge_label->runs, work))
                    return false;
            }
        emitted_link = true;
        from = std::move(to);
        scanner.space();
    }
    return !uses_group_endpoint || emitted_link;
}

// Rewrites mermaid's single-quoted directive strings into strict JSON.
// Double-quoted strings pass through verbatim; a single-quoted string is
// rejected on any backslash (JSON5 escapes are not synthesized), validated
// as unicode_text, and re-emitted with double quotes, escaping any embedded
// double quote. Returns false on an unterminated quote or a raw control
// byte. This mirrors the class family's class_init_meta normalizer.
bool normalize_directive_quotes(std::string_view argument, std::string& normalized) {
    normalized.clear();
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
        if (q == '"') {
            normalized.append(argument.substr(begin, at - begin + 1));
        } else {
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
    return true;
}

// FS-1/FS-init: the closed body-directive whitelist. Two one-key shapes
// are accepted, both non-terminal and disclosed below the art:
//   {"flowchart":{"htmlLabels":bool}} changes only SVG label anchoring
//       upstream; the terminal renders plain text labels either way.
//   {"logLevel": <0..5|trace|debug|info|warn|error|fatal>} selects logger
//       verbosity only and has no rendering effect (the class and state
//       families whitelist the same value grammar).
// Legacy mermaid spells the payload with single quotes, which the strict
// JSON parser rejects, so the quote normalizer runs first. Any other key,
// extra key, value outside the grammar, or malformed spelling keeps the
// whole source on ordinary fallback.
bool flow_init_directive(std::string_view line, Work& work,
                         std::vector<std::string>& notices) {
    constexpr std::string_view kPrefix = "%%{init:";
    constexpr std::string_view kSuffix = "}%%";
    if (!line.starts_with(kPrefix) || !line.ends_with(kSuffix) ||
        line.size() <= kPrefix.size() + kSuffix.size())
        return false;
    const auto payload =
        trim(line.substr(kPrefix.size(), line.size() - kPrefix.size() - kSuffix.size()));
    if (!work.spend(payload.size() * 4u + 256u)) return false;
    std::string normalized;
    if (!normalize_directive_quotes(payload, normalized) ||
        !work.spend(normalized.size() * 8u + 64u))
        return false;
    const auto document = loom::utils::json::parse(normalized);
    if (!document) return false;
    using loom::utils::json::JsonVal;
    const auto root = document->root();
    if (!root.is_obj()) return false;
    constexpr std::array<std::string_view, 6> kLogLevelNames{
        "trace", "debug", "info", "warn", "error", "fatal"};
    std::size_t top_count = 0;
    std::string notice;
    bool ok = true;
    root.iter_obj([&](JsonVal key, JsonVal value) {
        if (!ok) return;
        ++top_count;
        if (key.as_str() == "flowchart" && value.is_obj()) {
            std::size_t nested = 0;
            bool html_labels = false;
            value.iter_obj([&](JsonVal nkey, JsonVal nvalue) {
                if (!ok) return;
                ++nested;
                if (nkey.as_str() != "htmlLabels" || !nvalue.is_bool()) {
                    ok = false;
                    return;
                }
                html_labels = nvalue.as_bool();
            });
            if (nested != 1u) {
                ok = false;
                return;
            }
            notice = std::string("config.flowchart.htmlLabels=") +
                     (html_labels ? "true" : "false") +
                     " -> native text labels; not applied";
        } else if (key.as_str() == "logLevel") {
            if (value.is_int()) {
                const auto level = value.as_int();
                if (level < 0 || level > 5) {
                    ok = false;
                    return;
                }
                notice = "config.logLevel=" + std::to_string(level) +
                         " -> logger only; not applied";
            } else if (value.is_str() &&
                       std::ranges::find(kLogLevelNames, value.as_str()) !=
                           std::end(kLogLevelNames)) {
                notice = "config.logLevel=" + std::string(value.as_str()) +
                         " -> logger only; not applied";
            } else {
                ok = false;
            }
        } else {
            ok = false;
        }
    });
    if (!ok || top_count != 1u) return false;
    notices.push_back(std::move(notice));
    return true;
}

// FS-init: accessibility statements (`accTitle:` / `accDescr:`) carry
// screen-reader metadata that upstream never paints, so the terminal
// consumes them as validated no-ops, mirroring the class family's scalar
// contract. Returns 1 for a title, 2 for a description, or 0 when the line
// is not an accessibility statement. The multi-line `accDescr { ... }`
// block form is not part of this whitelist and stays source.
int flow_accessibility_kind(std::string_view line) {
    int kind = 0;
    if (line.starts_with("accTitle"))
        kind = 1;
    else if (line.starts_with("accDescr"))
        kind = 2;
    else
        return 0;
    if (line.size() == 8u || (line[8] != ':' && line[8] != ' ' && line[8] != '\t'))
        return 0;
    return kind;
}

// Validates one accessibility scalar: one optional colon, a non-empty
// space-trimmed value of at most 256 bytes, valid unicode, and none of the
// markup/backtick characters the terminal cannot represent. Duplicate
// titles/descriptions reject the whole source.
bool flow_consume_accessibility(int kind, std::string_view line, bool& title_seen,
                                 bool& description_seen, Work& work) {
    if ((kind == 1 && title_seen) || (kind == 2 && description_seen)) return false;
    auto rest = trim(line.substr(8u));
    if (rest.empty() || rest.front() != ':') return false;
    rest = trim(rest.substr(1u));
    if (rest.empty() || rest.size() > 256u ||
        rest.find_first_of("<>&\\`") != std::string_view::npos || !unicode_text(rest))
        return false;
    if (!work.spend(rest.size() * 8u + 128u)) return false;
    if (kind == 1)
        title_seen = true;
    else
        description_seen = true;
    return true;
}

std::optional<Diagram> parse(std::string_view source, Work& work, bool already_prepaid = false,
                             std::unique_ptr<FlowStyles>* style_result = nullptr,
                             RichLabels* rich = nullptr) {
    if (style_result) style_result->reset();
    // A failed first attempt (e.g. primary parse of a frontmatter source) must
    // not leave stale runs/notices for the envelope retry that reuses this.
    if (rich) {
        rich->vertex.clear();
        rich->edge.clear();
        rich->group.clear();
        rich->notices.clear();
        rich->paid = false;
    }
    std::vector<std::string> discarded_notices;
    std::unique_ptr<FlowStyles> pending_styles;
    if (source.empty() || source.size() > kSourceLimit) return {};
    // RFC 0031 E5: the bare whole-body profile is charged as soon as a valid
    // graph/flowchart header is recognized (the configured/envelope path has
    // already paid the full profile). Charging follows the allocation-free
    // header peek rather than function entry, so non-Flow sources that share
    // this first-attempt parser pay nothing before classification, exactly as
    // the former fixed entry charge behaved.
    bool prepaid = already_prepaid;
    const auto original = source;
    bool saw_reopen = false, cross_owner_membership_request = false;
    bool acc_title_seen = false, acc_descr_seen = false;
    for (const auto byte : source) {
        const auto value = static_cast<unsigned char>(byte);
        if (value < 0x20 && value != '\n' && value != '\r' && value != '\t') return {};
    }
    Diagram diagram;
    bool have_header = false;
    bool have_statement = false;
    while (!source.empty()) {
        const auto end = source.find('\n');
        auto line = source.substr(0, end);
        auto physical = line;
        if (physical.ends_with('\r')) physical.remove_suffix(1u);
        source = end == std::string_view::npos ? std::string_view{} : source.substr(end + 1);
        if (line.ends_with('\r')) line.remove_suffix(1);
        line = trim(line);
        if (line.starts_with("%%{")) {
            // FS-1: the closed non-terminal init whitelist; every other
            // directive keeps the whole source on ordinary fallback.
            if (!flow_init_directive(line, work,
                                     rich ? rich->notices : discarded_notices))
                return {};
            continue;
        }
        if (line.empty() || line.starts_with("%%")) continue;
        bool quoted = false;
        bool escaped = false;
        char close = 0;
        std::size_t start = 0;
        for (std::size_t at = 0; at <= line.size(); ++at) {
            const auto ch = at == line.size() ? '\0' : line[at];
            if (at == line.size() && (quoted || close || escaped)) return {};
            if (escaped) {
                escaped = false;
                continue;
            }
            if (quoted && ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                quoted = !quoted;
                continue;
            }
            if (!quoted && have_header) {
                if (close && ch == close)
                    close = 0;
                else if (!close && (ch == '[' || ch == '(' || ch == '{' || ch == '|' ||
                                    (ch == '>' &&
                                     (at == 0u || (line[at - 1u] != '-' && line[at - 1u] != '=')))))
                    close = ch == '[' || ch == '>' ? ']' : ch == '(' ? ')' : ch == '{' ? '}' : '|';
            }
            if (ch != '\0' && (ch != ';' || close || quoted)) continue;
            if (quoted || close || escaped) return {};
            const auto statement = trim(line.substr(start, at - start));
            start = at + 1;
            if (statement.empty()) continue;
            if (!have_header) {
                Scanner header{statement};
                auto kind = header.id();
                if (!kind || (*kind != "graph" && *kind != "flowchart")) return {};
                if (header.rest.empty()) {
                    diagram.direction = Direction::Down;
                } else {
                    if (header.rest.front() != ' ' && header.rest.front() != '\t') return {};
                    header.space();
                    if (header.rest == "TD" || header.rest == "TB")
                        diagram.direction = Direction::Down;
                    else if (header.rest == "BT")
                        diagram.direction = Direction::Up;
                    else if (header.rest == "LR")
                        diagram.direction = Direction::Right;
                    else if (header.rest == "RL")
                        diagram.direction = Direction::Left;
                    else
                        return {};
                }
                have_header = true;
                if (!prepaid) {
                    if (!flow_parse_prepay(original, work, false)) return {};
                    prepaid = true;
                }
                // FS-7a: pre-register every fixed-id subgraph as a deferred
                // shell so edges appearing before its declaration resolve to
                // EndpointKind::Group through the ordinary lookup path. This
                // runs on the first (header) statement, so no vertex exists
                // yet; a later vertex/group id clash is rejected by the
                // subgraph branch and Diagram::vertex group-name guard. Any
                // shell not reached by its declaration fails the deferred-
                // residue check at the end of the parse.
                const auto future_groups = flow_prescan_group_ids(original);
                if (!future_groups) return {};
                for (const auto& group_id : *future_groups)
                    diagram.groups.push_back(
                        {group_id, std::string{}, -1, std::nullopt, 0, false, true});
            } else {
                if (const auto accessibility = flow_accessibility_kind(statement)) {
                    // Non-visual screen-reader metadata; consumed and validated
                    // but never counted as graph content. accTitle/accDescr are
                    // diagram-level declarations, so one inside a subgraph is
                    // outside this whitelist and keeps the whole source.
                    if (!diagram.group_stack.empty() ||
                        !flow_consume_accessibility(accessibility, statement,
                                                    acc_title_seen, acc_descr_seen,
                                                    work))
                        return {};
                } else if (flow_style_keyword(statement)) {
                    if (!style_result || !diagram.group_stack.empty() ||
                        !flow_style_collect(statement, physical, pending_styles, work))
                        return {};
                    have_statement = true;
                } else {
                    if (!flow_statement(statement, diagram, work, original, already_prepaid,
                                        saw_reopen, cross_owner_membership_request, rich))
                        return {};
                    have_statement = true;
                }
            }
        }
    }
    // FS-7a: a direction-less header defaults to TB (Direction::Down), the
    // same layout input as an explicit `flowchart TD`, so group containers
    // no longer reject that header profile. A deferred shell would mean a
    // pre-registered group never reached its activation statement.
    if (!have_header || !have_statement || !diagram.group_stack.empty() ||
        std::ranges::any_of(diagram.groups, [](const Group& group) { return group.deferred; }) ||
        (diagram.vertices.empty() && diagram.groups.empty()))
        return {};
    if (saw_reopen) {
        // RFC 0031 E5: the bounded reopen validation walks are covered by the
        // full profile (activated on first reopen) / bare linear term.
        if (cross_owner_membership_request) return {};
        for (const auto& group : diagram.groups) {
            if (group.direction.has_value()) return {};
        }
    }
    if (pending_styles) {
        if (!resolve_flow_styles(*pending_styles, diagram, work)) return {};
        if (pending_styles->applications != 0u) *style_result = std::move(pending_styles);
    }
    return diagram;
}

// Returns the total coarse parse bound for the selected profile.
std::size_t flow_parse_prepay_total(std::string_view body, bool full_profile) {
    if (body.empty() || body.size() > kSourceLimit) return std::numeric_limits<std::size_t>::max();
    std::size_t legacy_identifiers = 0, numeric_identifiers = 0, group_words = 0;
    for (std::size_t at = 0; at < body.size();) {
        if (!id_start(body[at])) {
            ++at;
            continue;
        }
        const auto first = at++;
        while (at < body.size() && id_char(body[at]))
            ++at;
        ++legacy_identifiers;
        if (body.substr(first, at - first) == "subgraph") ++group_words;
    }
    for (std::size_t at = 0; at < body.size();) {
        if (!flow_id_start(body[at])) {
            ++at;
            continue;
        }
        const bool digit_start = !id_start(body[at]);
        ++at;
        while (at < body.size() && flow_id_continue(body, at))
            ++at;
        if (digit_start) ++numeric_identifiers;
    }
    const auto identifiers = legacy_identifiers + numeric_identifiers;
    const auto add = [](std::size_t total, std::size_t count, std::size_t unit) {
        if (!unit || count > (std::numeric_limits<std::size_t>::max() - total) / unit)
            return std::numeric_limits<std::size_t>::max();
        return total + count * unit;
    };
    std::size_t total =
        16u * (sizeof(Diagram) + sizeof(std::optional<Diagram>)) + 16u * sizeof(std::size_t) + 256u;
    total = add(total, body.size(), 4u);
    if (!full_profile) {
        constexpr std::size_t kIdentifierUnit =
            8u * sizeof(std::vector<std::size_t>) + 8u * sizeof(std::size_t) + 64u +
            (64u + kGroupLimit + 8u) * (2u * sizeof(std::size_t) + 8u);
        constexpr std::size_t kBarePerByte = 60u;
        constexpr std::size_t kBareLinearUnit = 34u;
        constexpr std::size_t kBareLinearVertices = 64u;
        constexpr std::size_t kBareLinearEdges = 128u;
        constexpr std::size_t kBareFixed = 4096u;
        total = add(total, body.size(), kBarePerByte);
        total = add(total, identifiers + 1u, kIdentifierUnit);
        total = add(total, kBareLinearVertices * kBareLinearEdges, kBareLinearUnit);
        if (total > std::numeric_limits<std::size_t>::max() - kBareFixed)
            return std::numeric_limits<std::size_t>::max();
        return total + kBareFixed;
    }
    const auto vertices = std::min<std::size_t>(64u, identifiers);
    const auto groups = std::min(kGroupLimit, group_words);
    const auto edges =
        identifiers >= 12u ? 128u : std::min<std::size_t>(128u, identifiers * identifiers);
    const auto depth = std::min(kGroupDepthLimit, groups);
    const auto bytes = body.size() + 1u;
    total = add(total, bytes, 4u * sizeof(std::string_view) + 4u * sizeof(std::size_t) + 32u);
    total = add(total, bytes, 32u * sizeof(std::string) + 512u);
    total = add(total, vertices, 8u * sizeof(Vertex));
    total = add(total, groups, 8u * sizeof(Group));
    total = add(total, edges, 8u * sizeof(Edge));
    total = add(total, depth, 8u * sizeof(std::size_t));
    total = add(total, identifiers + 1u,
                8u * sizeof(std::vector<std::size_t>) + 8u * sizeof(std::size_t) + 64u +
                    (64u + kGroupLimit + 8u) * (2u * sizeof(std::size_t) + 8u));
    total = add(total, edges, 8u * (body.size() + 1u));
    return total;
}

// RFC 0031 E5: whole-body coarse parse prepayment with two deliberately
// distinct profiles. upgrade_to_full is set only when a bare parse activates
// the full profile on its first group reopen: it charges the delta so the
// source pays once at the full-profile total, never both profiles.
bool flow_parse_prepay(std::string_view body, Work& work, bool full_profile, bool upgrade_to_full) {
    if (body.empty() || body.size() > kSourceLimit) return false;
    if (upgrade_to_full) {
        const auto bare = flow_parse_prepay_total(body, false);
        const auto full = flow_parse_prepay_total(body, true);
        if (full < bare) {
            // The full profile's analytic total is below what the bare profile
            // already charged; the bare fee dominates.
            return work.spend(0);
        }
        const auto delta = full - bare;
        if (delta > work.remaining) {
            static_cast<void>(work.spend(std::numeric_limits<std::size_t>::max()));
            return false;
        }
        return work.spend(delta);
    }
    const auto charge = [&](std::size_t count, std::size_t unit) {
        if (unit && count > work.remaining / unit) {
            static_cast<void>(work.spend(std::numeric_limits<std::size_t>::max()));
            return false;
        }
        return work.spend(count * unit);
    };
    if (!work.spend(16u * (sizeof(Diagram) + sizeof(std::optional<Diagram>)) +
                    16u * sizeof(std::size_t) + 256u) ||
        !charge(body.size(), 4u))
        return false;
    // Overcounts words in quotes/labels deliberately: these are bounds on
    // existing calls/allocations, not a second grammar or an admission decision.
    // Preserve the legacy word split exactly: it is the prior conservative
    // prepayment bound, rather than a grammar decision. The second pass only
    // adds digit-start Flow IDs, so dots and hyphens never lower that bound.
    std::size_t legacy_identifiers = 0, numeric_identifiers = 0, group_words = 0;
    for (std::size_t at = 0; at < body.size();) {
        if (!id_start(body[at])) {
            ++at;
            continue;
        }
        const auto first = at++;
        while (at < body.size() && id_char(body[at]))
            ++at;
        ++legacy_identifiers;
        if (body.substr(first, at - first) == "subgraph") ++group_words;
    }
    for (std::size_t at = 0; at < body.size();) {
        if (!flow_id_start(body[at])) {
            ++at;
            continue;
        }
        const bool digit_start = !id_start(body[at]);
        ++at;
        while (at < body.size() && flow_id_continue(body, at))
            ++at;
        if (digit_start) ++numeric_identifiers;
    }
    const auto identifiers = legacy_identifiers + numeric_identifiers;
    if (!full_profile) {
        // Bare profile: 60/byte dominates the deleted statement fees
        // (4/byte plus label scans at <=56/byte); the per-identifier unit is
        // the same vertex/index-table bound as the full profile below;
        // 64*128*34 dominates the linear vertex/group/edge scans (endpoint
        // group comparisons, reopen validation); a fixed term covers the
        // per-statement constants under the declaration caps.
        constexpr std::size_t kBarePerByte = 60u;
        constexpr std::size_t kBareLinearUnit = 34u;
        constexpr std::size_t kBareLinearVertices = 64u;
        constexpr std::size_t kBareLinearEdges = 128u;
        constexpr std::size_t kBareFixed = 4096u;
        constexpr std::size_t kIdentifierUnit =
            8u * sizeof(std::vector<std::size_t>) + 8u * sizeof(std::size_t) + 64u +
            (64u + kGroupLimit + 8u) * (2u * sizeof(std::size_t) + 8u);
        if (body.size() > work.remaining / kBarePerByte) return false;
        if (!work.spend(kBarePerByte * body.size()) || !charge(identifiers + 1u, kIdentifierUnit) ||
            !charge(kBareLinearVertices * kBareLinearEdges, kBareLinearUnit) ||
            !work.spend(kBareFixed))
            return false;
        return true;
    }
    const auto vertices = std::min<std::size_t>(64u, identifiers);
    const auto groups = std::min(kGroupLimit, group_words);
    const auto edges =
        identifiers >= 12u ? 128u : std::min<std::size_t>(128u, identifiers * identifiers);
    const auto depth = std::min(kGroupDepthLimit, groups);
    const auto bytes = body.size() + 1u;
    // New Flow lexer/group-header views and generated-ID materialization are
    // paid before configured/reopened parsing can allocate or scan them.
    if (!charge(bytes, 4u * sizeof(std::string_view) + 4u * sizeof(std::size_t) + 32u))
        return false;
    // Text input scans, Unicode/glyph tables, temporary strings, glyph vector
    // storage/copies, round-trip and decoded growth. Fan-out payloads follow.
    if (!charge(bytes, 32u * sizeof(std::string) + 512u) ||
        !charge(vertices, 8u * sizeof(Vertex)) || !charge(groups, 8u * sizeof(Group)) ||
        !charge(edges, 8u * sizeof(Edge)) || !charge(depth, 8u * sizeof(std::size_t)) ||
        !charge(identifiers + 1u, 8u * sizeof(std::vector<std::size_t>) + 8u * sizeof(std::size_t) +
                                      64u +
                                      (64u + kGroupLimit + 8u) * (2u * sizeof(std::size_t) + 8u)) ||
        !charge(edges, 8u * (body.size() + 1u)))
        return false;
    return true;
}

bool flow_envelope_parse_prepay(std::string_view body, Work& work) {
    return flow_parse_prepay(body, work, true);
}

struct FlowEnvelope {
    std::string_view body;
    std::optional<std::string> title;
    // B6: the frontmatter explicitly selected dagre. Only these configured
    // whole sources are eligible for the untrusted-compound fallback gate.
    bool dagre_layout = false;
    // FS-1: top-level config.htmlLabels was present (non-terminal: it only
    // selects SVG label anchoring upstream; the terminal draws text either
    // way). Disclosed below the art via Diagram::config_notices.
    std::optional<bool> html_labels;
};

std::optional<FlowEnvelope> flow_envelope_body(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    // The whole-envelope profile accepts CRLF, never a hidden bare CR.
    for (std::size_t n = 0; n < source.size(); ++n)
        if (source[n] == '\r' && (n + 1u == source.size() || source[n + 1u] != '\n')) return {};
    const auto envelope = detail::parse_envelope(source, work);
    if (!envelope || !envelope->has_frontmatter) return {};
    if (envelope->raw_frontmatter.find('\t') != std::string_view::npos) return {};

    enum ConfigBit : unsigned {
        Layout = 1u,
        Font = 2u,
        RootHtml = 4u,
        Flowchart = 8u,
        NestedHtml = 16u,
        Security = 32u,
        Log = 64u,
        Look = 1024u,
        MaxWidth = 128u,
        TitleTopMargin = 256u,
        DiagramPadding = 512u,
        NodeSpacing = 2048u,
        RankSpacing = 4096u,
        // FS-elk: an `elk.rectpacking` layout-engine value. The terminal runs
        // its own fixed layout, so the upstream engine selection is
        // non-terminal. The bare `elk` and `elk.box` values stay whole-source:
        // every corpus fixture using them either crosses the envelope Work
        // ceiling (FS-1b) or carries a subgraph/body debt that the open
        // envelope would surface as unreviewed art, so only the single
        // verified rectpacking profile is admitted.
        ElkEngine = 8192u
    };

    bool description = false, description_text = false, config = false, title = false;
    bool root_html_value = false, nested_html_value = false, log_one = false;
    bool title_top_margin_zero = false, diagram_padding_zero = false;
    std::optional<bool> use_max_width;
    unsigned fields = 0;
    const auto mark = [&](unsigned bit) {
        if (fields & bit) return false;
        fields |= bit;
        return true;
    };
    const auto boolean = [&](std::string_view scalar, unsigned bit, bool& value) {
        if (!mark(bit)) return false;
        if (scalar == "true") {
            value = true;
            return true;
        }
        if (scalar == "false") {
            value = false;
            return true;
        }
        return false;
    };
    const auto description_block = [&](const detail::EnvelopeScalar& scalar) -> bool {
        if (!((scalar.form == detail::EnvelopeScalarForm::Plain && scalar.header.empty()) ||
              (scalar.form == detail::EnvelopeScalarForm::Folded && scalar.header == ">-")))
            return false;
        std::size_t at = 0;
        while (at < scalar.text.size()) {
            const auto newline = scalar.text.find('\n', at);
            const auto line = scalar.text.substr(
                at, newline == std::string_view::npos ? std::string_view::npos : newline - at);
            at = newline == std::string_view::npos ? scalar.text.size() : newline + 1u;
            if (trim(line).empty()) continue;
            if (!line.starts_with("  ") || line.size() == 2u || line[2] == ' ') return false;
            const auto payload = line.substr(2);
            if (!unicode_text(payload)) return false;
            if (scalar.form == detail::EnvelopeScalarForm::Plain && !description_text) {
                const auto first = static_cast<unsigned char>(payload.front());
                constexpr std::array<unsigned char, 16> forbidden{
                    {44, 91, 93, 123, 125, 35, 38, 42, 33, 124, 62, 39, 34, 37, 64, 96}};
                if (std::ranges::find(forbidden, first) != forbidden.end()) return false;
                if ((first == '?' || first == '-') && (payload.size() == 1u || payload[1] == ' '))
                    return false;
            }
            if (scalar.form == detail::EnvelopeScalarForm::Plain) {
                for (std::size_t pos = 0; pos < payload.size(); ++pos) {
                    if (payload[pos] == ':' &&
                        (pos + 1u == payload.size() || payload[pos + 1u] == ' '))
                        return false;
                    if (payload[pos] == '#' && (pos == 0u || payload[pos - 1u] == ' '))
                        return false;
                }
            }
            description_text = true;
        }
        return true;
    };
    for (const auto& event : envelope->events) {
        if (event.blank || event.comment) continue;
        if (event.top_level) {
            if (event.path == "description") {
                if (description || (config && fields != Layout)) return {};
                if (!envelope->description || !description_block(*envelope->description)) return {};
                description = true;
                continue;
            }
            if (event.path == "title") {
                if (title || !envelope->title ||
                    envelope->title->form != detail::EnvelopeScalarForm::Plain ||
                    envelope->title->text.empty() || envelope->title->text.size() > 256u ||
                    envelope->title->text.find_first_of("\r\n") != std::string::npos ||
                    !unicode_text(envelope->title->text))
                    return {};
                title = true;
                continue;
            }
            if (event.path == "config") {
                if (config || (description && !description_text)) return {};
                config = true;
                continue;
            }
            return {};
        }
        if (event.namespace_entry) {
            if (event.path != "flowchart" || event.indent != 2) return {};
            if (!mark(Flowchart)) return {};
            continue;
        }
        if (!config) return {};
        if (event.indent == 2) {
            if (event.path.find('.') != std::string::npos) return {};
            if (event.path == "layout") {
                const auto scalar = trim(event.value);
                if (!mark(Layout)) return {};
                if (scalar == "dagre" || scalar == "\"dagre\"") {
                    // dagre engine selection stays on the existing profiles.
                } else if (scalar == "elk.rectpacking") {
                    // FS-elk: layout-engine selection is non-terminal. The
                    // terminal runs its own fixed layout, so which upstream
                    // engine positions nodes is ignored. Only the single bare
                    // elk.rectpacking value is admitted; bare elk / elk.box,
                    // quoted and unknown engine values keep the whole source.
                    if (!mark(ElkEngine)) return {};
                } else {
                    return {};
                }
            } else if (event.path == "look") {
                if (!flow_handdrawn_preview || !mark(Look) ||
                    trim(event.value) != "handDrawn")
                    return {};
            } else if (event.path == "fontFamily") {
                const auto scalar = trim(event.value);
                if (!mark(Font) || (scalar != "courier" && scalar != "\"courier\"")) return {};
            } else if (event.path == "securityLevel") {
                const auto scalar = trim(event.value);
                if (!mark(Security) || (scalar != "loose" && scalar != "\"loose\"")) return {};
            } else if (event.path == "logLevel") {
                const auto scalar = trim(event.value);
                if (!mark(Log) || scalar != "1") return {};
                log_one = true;
            } else if (event.path == "htmlLabels") {
                if (!boolean(trim(event.value), RootHtml, root_html_value)) return {};
            } else {
                return {};
            }
            continue;
        }
        if (event.indent == 4) {
            if (!(fields & Flowchart)) return {};
            if (event.path == "flowchart.htmlLabels") {
                if (!boolean(trim(event.value), NestedHtml, nested_html_value)) return {};
            } else if (event.path == "flowchart.useMaxWidth") {
                // This option controls responsive SVG sizing. Terminal rows
                // use the same intrinsic cell geometry at either value.
                if (!mark(MaxWidth)) return {};
                const auto value = trim(event.value);
                if (value == "true")
                    use_max_width = true;
                else if (value == "false")
                    use_max_width = false;
                else
                    return {};
            } else if (event.path == "flowchart.titleTopMargin") {
                // A zero margin has no terminal effect when there is no
                // diagram title, but it must not block the rest of the graph.
                // Non-zero values remain unsupported because they affect SVG
                // title placement and have no terminal equivalent.
                if (!mark(TitleTopMargin) || trim(event.value) != "0") return {};
                title_top_margin_zero = true;
            } else if (event.path == "flowchart.diagramPadding") {
                // SVG viewport padding has no terminal equivalent. Zero is
                // the exact inert profile and matches the terminal canvas.
                if (!mark(DiagramPadding) || trim(event.value) != "0") return {};
                diagram_padding_zero = true;
            } else if (event.path == "flowchart.nodeSpacing" ||
                       event.path == "flowchart.rankSpacing") {
                // FS-spacing: dagre rank spacing only positions nodes in the
                // upstream SVG layout; the terminal uses its own fixed layout,
                // so a non-negative pixel value (bare or quoted integer) is a
                // non-terminal leaf and never changes the rendered geometry.
                auto scalar = trim(event.value);
                // YAML allows the pixel integer bare or wrapped in either
                // quote style; accept both but reject a mismatched pair.
                if (scalar.size() >= 2u &&
                    ((scalar.front() == '"' && scalar.back() == '"') ||
                     (scalar.front() == '\'' && scalar.back() == '\'')))
                    scalar = trim(scalar.substr(1u, scalar.size() - 2u));
                if (scalar.empty()) return {};
                for (const char ch : scalar)
                    if (ch < '0' || ch > '9') return {};
                if (!mark(event.path == "flowchart.nodeSpacing" ? NodeSpacing : RankSpacing))
                    return {};
            } else {
                return {};
            }
            continue;
        }
        return {};
    }
    if ((!description && !config) || (description && !description_text)) return {};
    if (!config) {
        if (title) return {};
        return FlowEnvelope{envelope->raw_body, {}, false, std::nullopt};
    }
    const bool legacy_dagre = fields == Layout && !title;
    const bool dagre_diagram_title = fields == Layout && title;
    // FS-elk: a single elk.rectpacking engine value. Layout stays marked, so
    // the FlowEnvelope.dagre_layout flag below is set and the
    // untrusted-compound gate still rejects a rectpacking graph that carries
    // a subgraph.
    const bool elk_rectpacking_profile = fields == (Layout | ElkEngine);
    const bool terminal_max_width = fields == (Flowchart | MaxWidth) && use_max_width.has_value();
    const bool terminal_zero_diagram_padding = (fields == (Flowchart | DiagramPadding) ||
                                                fields == (Layout | Flowchart | DiagramPadding)) &&
                                               diagram_padding_zero;
    const bool title_top_margin_zero_without_layout =
        fields == (Flowchart | TitleTopMargin) && title_top_margin_zero && !title;
    const bool dagre_title_top_margin_zero =
        fields == (Layout | Flowchart | TitleTopMargin) && title_top_margin_zero && !title;
    const bool courier_only = fields == Font;
    const bool courier_nested_false =
        fields == (Font | Flowchart | NestedHtml) && !nested_html_value;
    const bool logging_false = fields == (Log | RootHtml) && log_one && !root_html_value;
    // FS-1: a lone top-level htmlLabels (either value) only switches SVG label
    // anchoring upstream; terminal text geometry is identical. Non-terminal,
    // disclosed per diagram.
    const bool root_html_only = fields == RootHtml;
    const bool both_false =
        fields == (RootHtml | Flowchart | NestedHtml) && !root_html_value && !nested_html_value;
    const unsigned strict_bits = RootHtml | Flowchart | NestedHtml | Security;
    const bool strict = (fields == strict_bits || fields == (strict_bits | Layout) ||
                         fields == (strict_bits | Look)) &&
                        root_html_value && nested_html_value;
    // FS-spacing: a validated nodeSpacing/rankSpacing leaf is non-terminal,
    // so it may accompany one of two inert cores: a bare flowchart namespace
    // or a courier-font flowchart. Anything richer keeps its existing exact
    // profile below; this never admits a new visual key on its own.
    const unsigned spacing_mask = NodeSpacing | RankSpacing;
    const bool spacing_profile =
        (fields & spacing_mask) && (fields & Flowchart) &&
        ((fields & ~spacing_mask) == Flowchart ||
         (fields & ~spacing_mask) == (Font | Flowchart));
    if (legacy_dagre || dagre_diagram_title || elk_rectpacking_profile ||
        terminal_max_width || terminal_zero_diagram_padding ||
        title_top_margin_zero_without_layout || dagre_title_top_margin_zero ||
        root_html_only || spacing_profile ||
        (description &&
         (courier_only || courier_nested_false || logging_false || both_false || strict))) {
        FlowEnvelope result{envelope->raw_body, {}, (fields & Layout) != 0u,
                            root_html_only ? std::optional<bool>{root_html_value} : std::nullopt};
        if (title) {
            const auto& value = envelope->title->text;
            if (!work.spend(value.size() * 8u + sizeof(std::string) + 128u)) return {};
            result.title.emplace(value);
        }
        return result;
    }
    return {};
}

bool prepend_flow_title(std::vector<std::string>& lines, std::string_view title, Work& work) {
    if (title.empty() || title.size() > 256u ||
        title.find_first_of("\r\n") != std::string_view::npos || !unicode_text(title) ||
        lines.empty())
        return false;
    const int title_width = ftxui::string_width(std::string(title));
    if (title_width <= 0 || title_width > kWidthLimit) return false;
    int graph_width = 0;
    std::size_t bytes = title.size();
    for (const auto& line : lines) {
        const int width = ftxui::string_width(line);
        if (width < 0 || width > kWidthLimit) return false;
        graph_width = std::max(graph_width, width);
        if (line.size() > std::numeric_limits<std::size_t>::max() - bytes) return false;
        bytes += line.size();
    }
    const int width = std::max(graph_width, title_width);
    if (width > kWidthLimit || lines.size() > static_cast<std::size_t>(kHeightLimit - 2))
        return false;
    if (bytes > (std::numeric_limits<std::size_t>::max() - 4096u) / 8u ||
        !work.spend(bytes * 8u + (lines.size() + 2u) * (sizeof(std::string) + 64u) + 4096u))
        return false;
    std::vector<std::string> titled;
    titled.reserve(lines.size() + 2u);
    const int title_left = (width - title_width) / 2;
    titled.emplace_back(static_cast<std::size_t>(title_left), ' ');
    titled.back().append(title);
    titled.back().append(static_cast<std::size_t>(width - title_left - title_width), ' ');
    titled.emplace_back(static_cast<std::size_t>(width), ' ');
    for (auto& line : lines) {
        const int line_width = ftxui::string_width(line);
        const int left = (width - line_width) / 2;
        std::string centered(static_cast<std::size_t>(left), ' ');
        centered.append(line);
        centered.append(static_cast<std::size_t>(width - left - line_width), ' ');
        titled.push_back(std::move(centered));
    }
    lines = std::move(titled);
    return true;
}

struct Box {
    int x;
    int y;
    int width;
    int height = 5;
};

std::optional<Box> node_size(const Vertex& node, Work& work) {
    // RFC 0031 E5: the classic/decision label-row measurement is linear in
    // validated <=kLabelLimit labels; its bookkeeping is folded into the parse
    // prepay (bare profile) and the collector literal_bytes term (full
    // profile). The parameter is retained for the shared call chain.
    (void)work;
    const int rows = static_cast<int>(label_rows(node.text).size());
    if (classic_shape(node.shape)) {
        if (rows != 1) {
            // Explicit multi-line labels (authored <br/>) are admitted only
            // for box contours whose glyph scales cleanly with height; the
            // stadium end caps are drawn relative to t/b so a taller box keeps
            // the same silhouette. Fixed-silhouette classics (rings,
            // triangles, waves) keep single-row-only sizing and fail closed.
            if (node.shape != Shape::Stadium) return {};
            const int width = std::max(classic_min_width(node.shape),
                                       label_width(node.text) + 4);
            const int height = rows + 4;  // top/bottom border + one-cell padding
            if (width > kWidthLimit || height > kHeightLimit) return {};
            return Box{0, 0, width, height};
        }
        if (node.shape == Shape::ForkJoin) {
            const int width = std::max(classic_min_width(node.shape), label_width(node.text) + 8);
            if (width > kWidthLimit) return {};
            return Box{0, 0, width, classic_height(node.shape)};
        }
        if (node.shape == Shape::SmallCircle) {
            int diameter = std::max(classic_min_width(node.shape), label_width(node.text) + 4);
            if ((diameter % 2) == 0) ++diameter;
            if (diameter > kWidthLimit || diameter > kHeightLimit) return {};
            return Box{0, 0, diameter, diameter};
        }
        if (node.shape == Shape::FilledCircle || node.shape == Shape::CrossedCircle) {
            // Upstream filledCircle (radius 7) and crossedCircle
            // (radius max(30,width)) are fixed-size label-less glyphs with
            // node.label cleared, so the node size is independent of text.
            const int diameter = classic_min_width(node.shape);
            return Box{0, 0, diameter, diameter};
        }
        if (node.shape == Shape::FramedCircle) {
            // The framed circle (stateEnd) keeps its label and sizes to it.
            int diameter = std::max(classic_min_width(node.shape), label_width(node.text) + 4);
            if ((diameter % 2) == 0) ++diameter;
            if (diameter > kWidthLimit || diameter > kHeightLimit) return {};
            return Box{0, 0, diameter, diameter};
        }
        if (node.shape == Shape::ShadedProcess) {
            // Main chamber plus a one-column outer frame on the left
            // (upstream shadedProcess FRAME_WIDTH); the label is centred in
            // the chamber, which starts one column in.
            int width = std::max(classic_min_width(node.shape), label_width(node.text) + 4) + 1;
            if (width > kWidthLimit) return {};
            return Box{0, 0, width, 5};
        }
        if (node.shape == Shape::CurvedTrapezoid) {
            // Two-row chevron shoulders on the left and a half-round cap on
            // the right (upstream curvedTrapezoid); reserve clearance for
            // both around the label. The geometry needs at least eleven
            // columns (chevron + body + cap), independent of the label.
            int width = std::max(11, label_width(node.text) + 6);
            if (width > kWidthLimit) return {};
            return Box{0, 0, width, 7};
        }
        if (node.shape == Shape::PaperTape) {
            // Five-row chamber plus a top crest row and a bottom trough row
            // (upstream waveRectangle waves on both edges).
            int width = std::max(classic_min_width(node.shape), label_width(node.text) + 4);
            if (width > kWidthLimit) return {};
            return Box{0, 0, width, 7};
        }
        if (node.shape == Shape::Triangle || node.shape == Shape::FlippedTriangle) {
            int height = std::max(5, label_width(node.text) + 2);
            if ((height % 2) == 0) ++height;
            const int width = 2 * height - 1;
            if (width > kWidthLimit || height > kHeightLimit) return {};
            return Box{0, 0, width, height};
        }
        if (node.shape == Shape::StackedRectangle) {
            // Front 5-row chamber plus one back sheet peeking at the top and
            // one column peeking at the right (upstream multiRect rectOffset).
            int width = std::max(classic_min_width(node.shape), label_width(node.text) + 4) + 1;
            if (width > kWidthLimit) return {};
            return Box{0, 0, width, 6};
        }
        if (node.shape == Shape::TaggedRectangle) {
            // Front rectangle plus one column for the folded corner tag tip.
            int width = std::max(classic_min_width(node.shape), label_width(node.text) + 4) + 1;
            if (width > kWidthLimit) return {};
            return Box{0, 0, width, 5};
        }
        if (node.shape == Shape::Document || node.shape == Shape::LinedDocument ||
            node.shape == Shape::TaggedDocument) {
            // Five-row chamber plus one bottom row for the wave trough floor
            // (upstream waveEdgedRectangle sine edge projects one cell deep).
            int width = std::max(classic_min_width(node.shape), label_width(node.text) + 4);
            if (node.shape == Shape::TaggedDocument) width += 1;  // side tag tip column
            if (width > kWidthLimit) return {};
            return Box{0, 0, width, 6};
        }
        if (node.shape == Shape::StackedDocument) {
            // Back sheet peeking one row at the top and one column at the
            // right, six-row front chamber plus the trough floor row.
            int width = std::max(classic_min_width(node.shape), label_width(node.text) + 4) + 1;
            if (width > kWidthLimit) return {};
            return Box{0, 0, width, 7};
        }
        const int padding =
            node.shape == Shape::Subroutine || node.shape == Shape::DoubleCircle ? 8 : 4;
        int width = std::max(classic_min_width(node.shape), label_width(node.text) + padding);
        if (classic_requires_odd_width(node.shape) && (width % 2) == 0) ++width;
        const int height = classic_height(node.shape);
        if (width > kWidthLimit || height > kHeightLimit) return {};
        return Box{0, 0, width, height};
    }
    int height = std::max(5, rows + 2);
    if (node.shape != Shape::Decision)
        return Box{0, 0, std::max(9, label_width(node.text) + 4), height};
    if (height % 2 == 0) ++height;
    const int half = height / 2;
    const int width = std::max(2 * half + 7, label_width(node.text) + 2 * half);
    if (width > kWidthLimit || height > kHeightLimit) return {};
    return Box{0, 0, width, height};
}

bool classic_ring(Canvas& canvas, int left, int top, int right, int bottom) {
    if (right - left + 1 < 9 || bottom - top + 1 < 9 || ((right - left + 1) % 2) == 0 ||
        ((bottom - top + 1) % 2) == 0)
        return false;
    const auto put = [&](int x, int y, std::string_view glyph) {
        return canvas.put({x, y}, glyph);
    };
    if (!put(left + 2, top, "╭") || !put(right - 2, top, "╮") || !put(left + 1, top + 1, "╭") ||
        !put(right - 1, top + 1, "╮") || !put(left + 1, bottom - 1, "╰") ||
        !put(right - 1, bottom - 1, "╯") || !put(left + 2, bottom, "╰") ||
        !put(right - 2, bottom, "╯"))
        return false;
    for (int x = left + 3; x < right - 2; ++x)
        if (!put(x, top, "─") || !put(x, bottom, "─")) return false;
    for (int x = left + 2; x < right - 1; ++x)
        if (!put(x, top + 1, "─") || !put(x, bottom - 1, "─")) return false;
    for (int y = top + 2; y <= bottom - 2; ++y)
        if (!put(left, y, "│") || !put(right, y, "│")) return false;
    return true;
}

// Fill the straight-walled interior of a classic_ring outline with solid
// blocks, yielding a filled disc (rows top+2..bottom-2, cols left+1..right-1).
// The ring outline itself is preserved.
bool classic_ring_fill(Canvas& canvas, int left, int top, int right, int bottom) {
    if (right - left + 1 < 9 || bottom - top + 1 < 9) return false;
    const auto put = [&](int x, int y, std::string_view glyph) {
        return canvas.put({x, y}, glyph);
    };
    for (int y = top + 2; y <= bottom - 2; ++y)
        for (int x = left + 1; x <= right - 1; ++x)
            if (!put(x, y, "█")) return false;
    return true;
}

// Shared sine-wave bottom edge of the four document shapes (upstream
// generateFullSineWavePoints, 0.8 of a cycle across the width). At cell
// resolution the wave projects as a single centred one-cell trough: the
// baseline rule descends and rises around a one-cell gap, with the trough
// floor one row below. The caller owns the (l,base)/(r,base) corners.
// Returns false only for a chamber too narrow to fit the trough.
bool classic_wave_bottom(Canvas& canvas, int l, int r, int base_y, int floor_y) {
    if (r - l + 1 < 7) return false;
    const int mid = l + (r - l) / 2;
    const auto put = [&](int x, int y, std::string_view glyph) {
        return canvas.put({x, y}, glyph);
    };
    for (int x = l + 1; x < mid - 1; ++x)
        if (!put(x, base_y, "─")) return false;
    for (int x = mid + 2; x < r; ++x)
        if (!put(x, base_y, "─")) return false;
    return put(mid - 1, base_y, "╲") && put(mid + 1, base_y, "╱") &&
           put(mid, floor_y, "─");
}

// Mirror of classic_wave_bottom for a wave CREST rising one row above the
// top edge: the baseline carries an ascent and a descent around a gap, with
// the crest cap one row above. The caller owns the (l,base)/(r,base) corners.
bool classic_wave_top(Canvas& canvas, int l, int r, int base_y, int cap_y) {
    if (r - l + 1 < 7) return false;
    const int mid = l + (r - l) / 2;
    const auto put = [&](int x, int y, std::string_view glyph) {
        return canvas.put({x, y}, glyph);
    };
    for (int x = l + 1; x < mid - 1; ++x)
        if (!put(x, base_y, "─")) return false;
    for (int x = mid + 2; x < r; ++x)
        if (!put(x, base_y, "─")) return false;
    return put(mid - 1, base_y, "╱") && put(mid + 1, base_y, "╲") &&
           put(mid, cap_y, "─");
}

bool classic_box(Canvas& canvas, const Box& box, Shape shape) {
    const int l = box.x, r = box.x + box.width - 1, t = box.y, b = box.y + box.height - 1;
    const auto put = [&](int x, int y, std::string_view glyph) {
        return canvas.put({x, y}, glyph);
    };
    const auto sharp = [&]() {
        for (int x = l + 1; x < r; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int y = t + 1; y < b; ++y)
            if (!put(l, y, "│") || !put(r, y, "│")) return false;
        return put(l, t, "┌") && put(r, t, "┐") && put(l, b, "└") && put(r, b, "┘");
    };
    switch (shape) {
    case Shape::ForkJoin:
        for (int y = t; y <= b; ++y)
            for (int x = l; x <= r; ++x)
                if (!put(x, y, "█")) return false;
        return put(l, t, "▛") && put(r, t, "▜") && put(l, b, "▙") && put(r, b, "▟");
    case Shape::Triangle: {
        if (box.height < 5 || (box.height % 2) == 0 || box.width != 2 * box.height - 1)
            return false;
        const int center = box.x + box.width / 2;
        if (!put(center, t, "△")) return false;
        for (int y = 1; y < box.height - 1; ++y) {
            const int inset = box.height - 1 - y;
            if (!put(l + inset, t + y, "╱") || !put(r - inset, t + y, "╲")) return false;
        }
        if (!put(l, b, "╱") || !put(r, b, "╲")) return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, b, "─")) return false;
        return true;
    }
    case Shape::FlippedTriangle: {
        // Manual-file shape: the vertical mirror of Triangle — the flat edge
        // is on top and the apex sits at the bottom centre.
        if (box.height < 5 || (box.height % 2) == 0 || box.width != 2 * box.height - 1)
            return false;
        const int center = box.x + box.width / 2;
        if (!put(center, b, "▽")) return false;
        if (!put(l, t, "╲") || !put(r, t, "╱")) return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, t, "─")) return false;
        for (int y = 1; y < box.height - 1; ++y)
            if (!put(l + y, t + y, "╲") || !put(r - y, t + y, "╱")) return false;
        return true;
    }
    case Shape::SlopedRectangle: {
        // Manual-input shape: a rectangle whose top edge slopes up to the
        // right, so the right wall extends two rows above the left wall
        // (upstream shapes/slopedRect.ts polygon).
        if (b - t + 1 < 7) return false;
        if (!put(l, t + 2, "┌") || !put(r, t, "┐") || !put(l + 1, t + 1, "╱") ||
            !put(r, t + 1, "│") || !put(l, b, "└") || !put(r, b, "┘"))
            return false;
        for (int x = l + 2; x < r; ++x)
            if (!put(x, t, "─")) return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, t + 2, "─")) return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, b, "─")) return false;
        for (int y = t + 3; y < b; ++y)
            if (!put(l, y, "│")) return false;
        // The right wall runs from the raised top (t, t+1 explicit above)
        // straight down; fill from t+2 so no cell breaks the contour.
        for (int y = t + 2; y < b; ++y)
            if (!put(r, y, "│")) return false;
        return true;
    }
    case Shape::WindowPane: {
        // Internal-storage shape: a rectangle split into four panes by one
        // horizontal and one vertical divider through the centre (upstream
        // shapes/windowPane.ts). The centred label is painted afterwards and
        // overwrites the cross at its centre, as in upstream.
        if (!sharp() || box.height < 5 || (box.height % 2) == 0 ||
            (box.width % 2) == 0)
            return false;
        const int mid_y = t + (b - t) / 2;
        const int mid_x = l + (r - l) / 2;
        if (!put(l, mid_y, "├") || !put(r, mid_y, "┤") ||
            !put(mid_x, t, "┬") || !put(mid_x, b, "┴"))
            return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, mid_y, x == mid_x ? "┼" : "─")) return false;
        for (int y = t + 1; y < b; ++y)
            if (y != mid_y && !put(mid_x, y, "│")) return false;
        return true;
    }
    case Shape::Card: {
        // Notched-rectangle card: a rectangle whose top-left corner is cut
        // by a one-cell diagonal (upstream shapes/card.ts polygon).
        if (b - t + 1 < 5) return false;
        if (!put(l + 1, t, "┌") || !put(r, t, "┐") || !put(l, t + 1, "╱") ||
            !put(l, b, "└") || !put(r, b, "┘"))
            return false;
        for (int x = l + 2; x < r; ++x)
            if (!put(x, t, "─")) return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, b, "─")) return false;
        for (int y = t + 2; y < b; ++y)
            if (!put(l, y, "│")) return false;
        for (int y = t + 1; y < b; ++y)
            if (!put(r, y, "│")) return false;
        return true;
    }
    case Shape::Folder: {
        // Folder/directory: a body rectangle with a tab on the top-left that
        // is one row tall (upstream shapes/folder.ts polygon); the body's top
        // edge sits one row below the tab.
        if (b - t + 1 < 5) return false;
        int tab = (r - l + 1) * 38 / 100;  // ~0.38 of the width, upstream ratio
        if (tab < 2) tab = 2;
        if (tab > r - l - 2) tab = r - l - 2;  // keep at least two edge cells
        if (!put(l, t, "┌") || !put(l + tab, t, "┐") || !put(l, t + 1, "├") ||
            !put(l + tab, t + 1, "┴") || !put(r, t + 1, "┐") ||
            !put(l, b, "└") || !put(r, b, "┘"))
            return false;
        for (int x = l + 1; x < l + tab; ++x)
            if (!put(x, t, "─")) return false;
        for (int x = l + 1; x < r; ++x)
            if (x != l + tab && !put(x, t + 1, "─")) return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, b, "─")) return false;
        for (int y = t + 2; y < b; ++y)
            if (!put(l, y, "│") || !put(r, y, "│")) return false;
        return true;
    }
    case Shape::Delay:
        if (!put(l, t, "┌") || !put(r - 2, t, "─") || !put(l, t + 1, "│") ||
            !put(r - 1, t + 1, "╮") || !put(l, t + 2, "│") || !put(r, t + 2, ")") ||
            !put(l, t + 3, "│") || !put(r - 1, t + 3, "╯") || !put(l, b, "└") ||
            !put(r - 2, b, "─"))
            return false;
        for (int x = l + 1; x < r - 2; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        return true;
    case Shape::Stadium:
        if (!put(l + 1, t, "╭") || !put(r - 1, t, "╮") || !put(l + 1, b, "╰") ||
            !put(r - 1, b, "╯"))
            return false;
        for (int x = l + 2; x < r - 1; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int y = t + 1; y < b; ++y)
            if (!put(l, y, "(") || !put(r, y, ")")) return false;
        return true;
    case Shape::Circle:
    case Shape::SmallCircle:
        return classic_ring(canvas, l, t, r, b);
    case Shape::DoubleCircle:
        return classic_ring(canvas, l, t, r, b) && classic_ring(canvas, l + 2, t + 2, r - 2, b - 2);
    case Shape::FilledCircle:
        // A solid disc: circular ring silhouette with a blocked interior
        // (upstream filledCircle, fixed radius with the label cleared).
        return classic_ring(canvas, l, t, r, b) && classic_ring_fill(canvas, l, t, r, b);
    case Shape::FramedCircle: {
        // UML final state: an outer ring with a filled inner core whose
        // radius is 5/14 of the diameter (upstream stateEnd). The odd core
        // diameter is at least five cells, so it extends beyond the single
        // label row and stays visible around the centred text.
        if (!classic_ring(canvas, l, t, r, b)) return false;
        int side = (box.width * 10) / 14;  // inner diameter ~10/14 of outer
        if ((side % 2) == 0) --side;
        if (side < 5) side = 5;
        const int inset = (box.width - side) / 2;
        const int il = l + inset, ir = r - inset, it = t + inset, ib = b - inset;
        for (int y = it; y <= ib; ++y)
            for (int x = il; x <= ir; ++x)
                if (!put(x, y, "█")) return false;
        return true;
    }
    case Shape::CrossedCircle: {
        // Summary: a ring with two crossing diameters (upstream
        // crossedCircle createLine). Inscribe an X in the straight interior.
        // The stroke endpoints meet the first/last straight wall cells rather
        // than the ring's 45-degree points: the terminal's 2:1 cell aspect
        // needs that extra step for the diagonals to read (same convention as
        // Triangle's width = 2h-1). The bottom half mirrors the top so the X
        // stays symmetric for every odd diameter (round-half-up on a linear
        // interpolation would otherwise bias D == 5 mod 8).
        if (!classic_ring(canvas, l, t, r, b)) return false;
        const int top_y = t + 2, bottom_y = b - 2;
        const int span = bottom_y - top_y + 1;
        const int mid_row = span / 2;
        // Descending-stroke column for interior row yy. The top half follows
        // a left->right linear interpolation; the bottom half is derived by
        // mirroring the top half across the centre rather than interpolated,
        // so round-half-up ties (D == 5 mod 8) cannot skew one half outward.
        const auto descent_col = [&](int yy) {
            const auto linear = [&](int q) {
                return l + (2 * q * (r - l) + (span - 1)) / (2 * (span - 1));
            };
            return yy <= mid_row ? linear(yy) : l + (r - l) - (linear(2 * mid_row - yy) - l);
        };
        for (int y = top_y; y <= bottom_y; ++y) {
            const int yy = y - top_y;
            const int xd = descent_col(yy);
            const int xr = r - (xd - l);
            if (yy == mid_row) {
                if (!put(xd, y, "╳")) return false;
            } else if (!put(xd, y, "╲") || !put(xr, y, "╱")) {
                return false;
            }
        }
        return true;
    }
    case Shape::Subroutine:
        if (!sharp()) return false;
        for (int y = t + 1; y < b; ++y)
            if (!put(l + 2, y, "║") || !put(r - 2, y, "║")) return false;
        return true;
    case Shape::DividedRectangle: {
        // A process box split into an empty title band (top two rows) and a
        // body band that carries the label.
        if (!sharp() || b - t + 1 < 7) return false;
        const int divider = t + 2;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, divider, "─")) return false;
        return put(l, divider, "├") && put(r, divider, "┤");
    }
    case Shape::Hexagon:
        if (!put(l + 2, t, "╱") || !put(r - 2, t, "╲") || !put(l + 1, t + 1, "╱") ||
            !put(r - 1, t + 1, "╲") || !put(l, t + 2, "│") || !put(r, t + 2, "│") ||
            !put(l + 1, b - 1, "╲") || !put(r - 1, b - 1, "╱") || !put(l + 2, b, "╲") ||
            !put(r - 2, b, "╱"))
            return false;
        for (int x = l + 3; x < r - 2; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        return true;
    case Shape::Cylinder:
        if (!put(l + 2, t, "╭") || !put(r - 2, t, "╮") || !put(l + 1, t + 1, "╭") ||
            !put(r - 1, t + 1, "╮") || !put(l + 1, b - 1, "╰") || !put(r - 1, b - 1, "╯") ||
            !put(l + 2, b, "╰") || !put(r - 2, b, "╯"))
            return false;
        for (int x = l + 3; x < r - 2; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int x = l + 2; x < r - 1; ++x)
            if (!put(x, t + 1, "─") || !put(x, b - 1, "─")) return false;
        for (int y = t + 2; y <= b - 2; ++y)
            if (!put(l, y, "│") || !put(r, y, "│")) return false;
        return true;
    case Shape::LinedCylinder: {
        // Disk storage: the vertical database cylinder plus one extra ellipse
        // line just under the top face marking a stacked disk
        // (upstream shapes/linedCylinder.ts inner line, offset 0.1*h).
        if (!put(l + 2, t, "╭") || !put(r - 2, t, "╮") || !put(l + 1, t + 1, "╭") ||
            !put(r - 1, t + 1, "╮") || !put(l + 1, b - 1, "╰") || !put(r - 1, b - 1, "╯") ||
            !put(l + 2, b, "╰") || !put(r - 2, b, "╯"))
            return false;
        for (int x = l + 3; x < r - 2; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int x = l + 2; x < r - 1; ++x)
            if (!put(x, t + 1, "─") || !put(x, b - 1, "─")) return false;
        for (int y = t + 2; y <= b - 2; ++y)
            if (!put(l, y, "│") || !put(r, y, "│")) return false;
        // The stacked-disk line on the first body row, meeting the walls.
        if (!put(l, t + 2, "├") || !put(r, t + 2, "┤")) return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, t + 2, "─")) return false;
        return true;
    }
    case Shape::HorizontalCylinder: {
        // Direct-access storage: a cylinder lying on its side - straight top
        // and bottom edges, shallow elliptical end caps one cell deep on the
        // middle row, and a centre vertical line for the far rim (upstream
        // shapes/tiltedCylinder.ts, rx << ry).
        if (box.height < 5 || (box.height % 2) == 0) return false;
        const int mid = t + (b - t) / 2;
        if (!put(l, t, "┌") || !put(r, t, "┐") || !put(l, b, "└") ||
            !put(r, b, "┘") || !put(mid, t, "┬") || !put(mid, b, "┴"))
            return false;
        for (int x = l + 1; x < r; ++x)
            if (x != mid && (!put(x, t, "─") || !put(x, b, "─"))) return false;
        for (int y = t + 1; y < b; ++y) {
            if (y == mid) {
                if (!put(l, y, "(") || !put(r, y, ")")) return false;
            } else if (!put(l, y, "│") || !put(r, y, "│")) {
                return false;
            }
            if (y != mid && !put(mid, y, "│")) return false;
        }
        return true;
    }
    case Shape::Datastore: {
        // Data-flow data store: an open rectangle with only the top and
        // bottom rules (upstream shapes/datastore.ts draws two lines and
        // suppresses the side walls with stroke-dasharray).
        if (box.height < 3) return false;
        for (int x = l; x <= r; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        return true;
    }
    case Shape::NotchedPentagon: {
        // Loop-limit trapezoidal pentagon: the top edge is inset on both
        // sides with a one-row shoulder dropping to the straight walls
        // (upstream shapes/trapezoidalPentagon.ts, top edge 80% of width).
        if (b - t + 1 < 5) return false;
        int inset = (r - l + 1) / 10;
        if (inset < 1) inset = 1;
        const int lt = l + inset, rt = r - inset;
        if (lt >= rt) return false;
        if (!put(lt, t, "┌") || !put(rt, t, "┐") || !put(l, t + 1, "╱") ||
            !put(r, t + 1, "╲") || !put(l, b, "└") || !put(r, b, "┘"))
            return false;
        for (int x = lt + 1; x < rt; ++x)
            if (!put(x, t, "─")) return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, b, "─")) return false;
        for (int y = t + 2; y < b; ++y)
            if (!put(l, y, "│") || !put(r, y, "│")) return false;
        return true;
    }
    case Shape::Hourglass:
    case Shape::BowTie: {
        // Collate hourglass / stored-data bow tie: a full top and bottom
        // edge whose sides sweep one cell inward toward the label row and
        // back out (upstream hourglass.ts / bowTieRect.ts converge at the
        // centre; at cell resolution the single-row waist opens to carry the
        // centred label, the bow tie's concave arcs collapsing onto the same
        // diagonals).
        if (box.height < 5 || (box.height % 2) == 0) return false;
        if (!put(l, t, "┌") || !put(r, t, "┐") || !put(l, b, "└") ||
            !put(r, b, "┘") || !put(l + 1, t + 1, "╲") ||
            !put(r - 1, t + 1, "╱") || !put(l + 1, b - 1, "╱") ||
            !put(r - 1, b - 1, "╲"))
            return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        return true;
    }
    case Shape::Bolt: {
        // Communication-link lightning bolt: a right-aligned head, a
        // down-left stroke to the waist, a right jog, then a down-left stroke
        // to a left-aligned tail (upstream shapes/lightningBolt.ts zig-zag).
        if (b - t + 1 < 5) return false;
        const int midx = l + (r - l) / 2;
        if (!put(r, t, "┐") || !put(midx, t, "┌") || !put(l, t + 1, "╱") ||
            !put(midx, t + 1, "╲") || !put(l, t + 2, "└") ||
            !put(r, t + 2, "┐") || !put(l, b, "└") || !put(r, b, "┘") ||
            !put(midx, b - 1, "╱") || !put(r, b - 1, "╲"))
            return false;
        for (int x = midx + 1; x < r; ++x)
            if (!put(x, t, "─")) return false;
        // The waist jog is a near-full-width edge (upstream E2): paint across
        // the whole interior; the centred label overwrites its middle.
        for (int x = l + 1; x < r; ++x)
            if (!put(x, t + 2, "─")) return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, b, "─")) return false;
        return true;
    }
    case Shape::Brace: {
        // Comment curly brace on the LEFT opening rightward: straight top,
        // right and bottom rules, a left wall, and a brace tip on the middle
        // row meeting the interior rule at ┤ (upstream
        // shapes/curlyBraceLeft.ts; the rounded curls are below cell
        // resolution).
        if (b - t + 1 < 5) return false;
        const int midy = t + (b - t) / 2;
        if (!put(l, t, "┌") || !put(r, t, "┐") || !put(l, midy, "◁") ||
            !put(r, midy, "┤") || !put(l, b, "└") || !put(r, b, "┘"))
            return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, t, "─") || !put(x, b, "─") || !put(x, midy, "─"))
                return false;
        for (int y = t + 1; y < midy; ++y)
            if (!put(l, y, "│")) return false;
        for (int y = midy + 1; y < b; ++y)
            if (!put(l, y, "│")) return false;
        // Skip midy: the interior rule already put ┤ there.
        for (int y = t + 1; y < midy; ++y)
            if (!put(r, y, "│")) return false;
        for (int y = midy + 1; y < b; ++y)
            if (!put(r, y, "│")) return false;
        return true;
    }
    case Shape::StackedRectangle: {
        // Multi-process: a front rectangle with a second sheet peeking out
        // along the top-right - a short back edge on the top row and a
        // one-column back wall on the right (upstream shapes/multiRect.ts
        // diagonal rectOffset). Box is 6 rows x (w+1); the front chamber is
        // rows t+1..b, columns l..r-1.
        if (b - t + 1 < 6) return false;
        // Back sheet: a two-cell top edge ending at the top-right corner and
        // a right wall one column beyond the front for the full height.
        if (!put(r - 1, t, "─") || !put(r, t, "┐")) return false;
        // Front rectangle (r-1 is the front top-right corner; the back wall
        // continues one column beyond it down to the bottom).
        if (!put(l, t + 1, "┌") || !put(r - 1, t + 1, "┐") ||
            !put(l, b, "└") || !put(r - 1, b, "┘") || !put(r, b, "┘"))
            return false;
        for (int x = l + 1; x < r - 1; ++x)
            if (!put(x, t + 1, "─") || !put(x, b, "─")) return false;
        for (int y = t + 2; y < b; ++y)
            if (!put(l, y, "│") || !put(r - 1, y, "│") || !put(r, y, "│"))
                return false;
        if (!put(r, t + 1, "│")) return false;
        return true;
    }
    case Shape::TaggedRectangle: {
        // Tagged process: a rectangle whose bottom-right corner is folded
        // into a one-cell tag that extends one column right (upstream
        // shapes/taggedRect.ts, tagWidth=tagHeight=0.2*h). Front chamber is
        // columns l..r-1; column r carries only the folded tip.
        if (b - t + 1 < 5) return false;
        if (!put(l, t, "┌") || !put(r - 1, t, "┐") || !put(l, b, "└"))
            return false;
        for (int x = l + 1; x < r - 1; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int y = t + 1; y < b; ++y)
            if (!put(l, y, "│") || !put(r - 1, y, "│")) return false;
        // Folded tag: diagonal fold at the front bottom-right corner, a tip
        // one column right, and the tag bottom aligned with the box bottom.
        // The tip joins a left arm (to the fold diagonal) and an up arm (to
        // the tag hypotenuse), i.e. a box up-and-left corner.
        if (!put(r - 1, b, "╱") || !put(r, b - 1, "╱") || !put(r, b, "┘"))
            return false;
        return true;
    }
    case Shape::Document: {
        // A rectangle whose bottom edge is a single centred one-cell wave
        // trough (upstream waveEdgedRectangle); the last box row carries the
        // trough floor.
        if (b - t + 1 < 6) return false;
        const int base = b - 1;
        if (!put(l, t, "┌") || !put(r, t, "┐") ||
            !put(l, base, "└") || !put(r, base, "┘"))
            return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, t, "─")) return false;
        for (int y = t + 1; y < base; ++y)
            if (!put(l, y, "│") || !put(r, y, "│")) return false;
        return classic_wave_bottom(canvas, l, r, base, b);
    }
    case Shape::LinedDocument: {
        // Wave document with a book binding: a second vertical rule one
        // column inside the left edge, junctioned into the top edge and the
        // wave baseline (upstream linedWaveEdgedRect extra left rule).
        if (b - t + 1 < 6) return false;
        const int base = b - 1;
        if (!put(l, t, "┌") || !put(r, t, "┐") ||
            !put(l, base, "└") || !put(r, base, "┘"))
            return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, t, "─")) return false;
        for (int y = t + 1; y < base; ++y)
            if (!put(l, y, "│") || !put(r, y, "│")) return false;
        if (!classic_wave_bottom(canvas, l, r, base, b)) return false;
        if (!put(l + 1, t, "├") || !put(l + 1, base, "┴")) return false;
        for (int y = t + 1; y < base; ++y)
            if (!put(l + 1, y, "│")) return false;
        return true;
    }
    case Shape::TaggedDocument: {
        // Wave document with a small side tag pointing right at the middle
        // row (upstream taggedWaveEdgedRectangle triangle). The box gains one
        // column for the tip; the chamber is columns l..r-1. The tag slants
        // replace the right wall one row above and below the tip.
        if (b - t + 1 < 6) return false;
        const int base = b - 1;
        // The tag spans the three interior rows, so its centre is the mean
        // of top-edge and baseline rows; the slants meet the top-right
        // corner and the wave corner directly (chamber is five rows tall).
        const int midy = (t + base) / 2;
        if (!put(l, t, "┌") || !put(r - 1, t, "┐") ||
            !put(l, base, "└") || !put(r - 1, base, "┘"))
            return false;
        for (int x = l + 1; x < r - 1; ++x)
            if (!put(x, t, "─")) return false;
        for (int y = t + 1; y < base; ++y) {
            if (!put(l, y, "│")) return false;
            if (y < midy - 1 || y > midy + 1) {
                if (!put(r - 1, y, "│")) return false;
            }
        }
        if (!put(r - 1, midy - 1, "╲") || !put(r, midy, "▷") ||
            !put(r - 1, midy + 1, "╱"))
            return false;
        return classic_wave_bottom(canvas, l, r - 1, base, b);
    }
    case Shape::StackedDocument: {
        // Multi-document: the stacked-rectangle back sheet (one row peeking
        // at the top, one column at the right) combined with the wave bottom
        // on the front chamber (upstream multiWaveEdgedRectangle).
        if (b - t + 1 < 7) return false;
        const int base = b - 1;
        if (!put(r - 1, t, "─") || !put(r, t, "┐")) return false;
        if (!put(r, t + 1, "│")) return false;
        for (int y = t + 2; y <= base; ++y)
            if (!put(r, y, "│")) return false;
        if (!put(l, t + 1, "┌") || !put(r - 1, t + 1, "┐") ||
            !put(l, base, "└") || !put(r - 1, base, "┘") || !put(r, b, "┘"))
            return false;
        for (int x = l + 1; x < r - 1; ++x)
            if (!put(x, t + 1, "─")) return false;
        for (int y = t + 2; y < base; ++y)
            if (!put(l, y, "│") || !put(r - 1, y, "│")) return false;
        return classic_wave_bottom(canvas, l, r - 1, base, b);
    }
    case Shape::ShadedProcess: {
        // Lined/shaded process (upstream shadedProcess): a main chamber with
        // a thin outer frame strip one column to its left. The frame is
        // column l; the chamber is l+1..r.
        if (b - t + 1 < 5) return false;
        if (!put(l, t, "┌") || !put(l + 1, t, "┬") || !put(r, t, "┐") ||
            !put(l, b, "└") || !put(l + 1, b, "┴") || !put(r, b, "┘"))
            return false;
        for (int x = l + 2; x < r; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int y = t + 1; y < b; ++y)
            if (!put(l, y, "│") || !put(l + 1, y, "│") || !put(r, y, "│"))
                return false;
        return true;
    }
    case Shape::CurvedTrapezoid: {
        // Display (upstream curvedTrapezoid): a two-row chevron point on the
        // left feeding a straight body whose right end is a half-round cap.
        // h=7: tip at the middle row, two stacked diagonal cells climbing to
        // the shoulder corners; the cap spans three rows (radius ~h/2).
        if (b - t + 1 < 7 || r - l + 1 < 11) return false;
        const int midy = t + 3;
        if (!put(l, midy, "◁") || !put(l + 2, t, "┌") || !put(l + 2, b, "└") ||
            !put(r - 2, t, "╮") || !put(r - 2, b, "╯"))
            return false;
        if (!put(l + 1, t + 1, "╱") || !put(l + 1, t + 2, "╱") ||
            !put(l + 1, b - 2, "╲") || !put(l + 1, b - 1, "╲"))
            return false;
        for (int x = l + 3; x < r - 2; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int y = t + 1; y < b; ++y)
            if (!put(l + 2, y, "│")) return false;
        if (!put(r - 1, t + 1, ")") || !put(r - 1, b - 1, ")"))
            return false;
        for (int y = t + 2; y < b - 1; ++y)
            if (!put(r, y, ")")) return false;
        return true;
    }
    case Shape::PaperTape: {
        // Flag / paper tape (upstream waveRectangle): a rectangle whose top
        // and bottom edges are both sine waves. The five-row chamber sits
        // between a crest row (t) and a trough row (b).
        if (b - t + 1 < 7) return false;
        const int top_base = t + 1, base = b - 1;
        if (!put(l, top_base, "┌") || !put(r, top_base, "┐") ||
            !put(l, base, "└") || !put(r, base, "┘"))
            return false;
        for (int y = top_base + 1; y < base; ++y)
            if (!put(l, y, "│") || !put(r, y, "│")) return false;
        return classic_wave_top(canvas, l, r, top_base, t) &&
               classic_wave_bottom(canvas, l, r, base, b);
    }
    case Shape::Asymmetric:
        if (!put(l + 2, t, "╱") || !put(r, t, "┐") || !put(l + 1, t + 1, "╱") ||
            !put(l, t + 2, "◁") || !put(l + 1, b - 1, "╲") || !put(l + 2, b, "╲") ||
            !put(r, b, "┘"))
            return false;
        for (int x = l + 3; x < r; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int y = t + 1; y < b; ++y)
            if (!put(r, y, "│")) return false;
        return true;
    case Shape::Trapezoid:
        if (!put(l + 2, t, "┌") || !put(r - 2, t, "┐") || !put(l + 1, t + 1, "╱") ||
            !put(r - 1, t + 1, "╲") || !put(l + 1, t + 2, "│") || !put(r - 1, t + 2, "│") ||
            !put(l, b - 1, "│") || !put(r, b - 1, "│") || !put(l, b, "└") || !put(r, b, "┘"))
            return false;
        for (int x = l + 3; x < r - 2; ++x)
            if (!put(x, t, "─")) return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, b, "─")) return false;
        return true;
    case Shape::TrapezoidAlt:
        if (!put(l, t, "┌") || !put(r, t, "┐") || !put(l, t + 1, "│") || !put(r, t + 1, "│") ||
            !put(l + 1, t + 2, "│") || !put(r - 1, t + 2, "│") || !put(l + 1, b - 1, "│") ||
            !put(r - 1, b - 1, "│") || !put(l + 2, b, "└") || !put(r - 2, b, "┘"))
            return false;
        for (int x = l + 1; x < r; ++x)
            if (!put(x, t, "─")) return false;
        for (int x = l + 3; x < r - 2; ++x)
            if (!put(x, b, "─")) return false;
        return true;
    case Shape::Parallelogram:
        if (!put(l + 2, t, "┌") || !put(r, t, "┐") || !put(l + 1, t + 1, "╱") ||
            !put(r - 1, t + 1, "╱") || !put(l + 1, t + 2, "│") || !put(r - 1, t + 2, "│") ||
            !put(l, b - 1, "│") || !put(r - 2, b - 1, "│") || !put(l, b, "└") ||
            !put(r - 2, b, "┘"))
            return false;
        for (int x = l + 3; x < r; ++x)
            if (!put(x, t, "─")) return false;
        for (int x = l + 1; x < r - 2; ++x)
            if (!put(x, b, "─")) return false;
        return true;
    case Shape::ParallelogramAlt:
        if (!put(l, t, "┌") || !put(r - 2, t, "┐") || !put(l, t + 1, "╲") ||
            !put(r - 2, t + 1, "╲") || !put(l + 1, t + 2, "│") || !put(r - 1, t + 2, "│") ||
            !put(l + 1, b - 1, "│") || !put(r - 1, b - 1, "│") || !put(l + 2, b, "└") ||
            !put(r, b, "┘"))
            return false;
        for (int x = l + 1; x < r - 2; ++x)
            if (!put(x, t, "─")) return false;
        for (int x = l + 3; x < r; ++x)
            if (!put(x, b, "─")) return false;
        return true;
    case Shape::Rectangle:
    case Shape::Rounded:
    case Shape::Decision:
        return false;
    }
    return false;
}

bool classic_port_corridors(Canvas& canvas, const Box& box, Shape shape) {
    const int middle_y = box.y + box.height / 2;
    const bool shifted_side = shape == Shape::Trapezoid || shape == Shape::TrapezoidAlt ||
                              shape == Shape::Parallelogram || shape == Shape::ParallelogramAlt;
    if (shape == Shape::Folder) {
        // A top edge lands on the full-width body edge (row t+1), so its
        // outside cell is the blank one row above at the centre column —
        // inside the prefilled bbox but honest exterior (right of the short
        // tab). Unblock just that cell for the incoming shaft.
        const Point above{box.x + box.width / 2, box.y};
        if (canvas.at(above).glyph != " ") return false;
        if (!canvas.put(above, " ", false)) return false;
        return true;
    }
    if (shape == Shape::StackedRectangle || shape == Shape::StackedDocument) {
        // The front chamber starts one row below the back-sheet peek; a top
        // edge onto the front needs the centre cell on the back row unblocked
        // (it is blank unless the box is so narrow the centre sits on the
        // back corner, in which case there is nothing to unblock). The
        // stacked document's front chamber is one column narrower (its extra
        // column is the back wall), so its centre is one cell left.
        const int cx = box.x + box.width / 2 -
                       (shape == Shape::StackedDocument ? 1 : 0);
        const Point above{cx, box.y};
        if (canvas.at(above).glyph == " " && !canvas.put(above, " ", false))
            return false;
        return true;
    }
    if (shape == Shape::TaggedRectangle) {
        // The folded tag occupies only the bottom of the extra right column;
        // a side edge onto the front wall needs the mid-row peek cell open.
        const Point side{box.x + box.width - 1, middle_y};
        if (canvas.at(side).glyph != " ") return false;
        if (!canvas.put(side, " ", false)) return false;
        return true;
    }
    if (!shifted_side) return true;
    const Point left{box.x, middle_y}, right{box.x + box.width - 1, middle_y};
    // These are the two blank envelope cells immediately outside the actual
    // l+1/r-1 side terminals. Keep the contour and interior blocked.
    if (canvas.at(left).glyph != " " || canvas.at(right).glyph != " ") return false;
    return canvas.put(left, " ", false) && canvas.put(right, " ", false);
}

// The explicit ASCII Flow profile needs the actual native canvas rows before
// FTXUI adds Screen padding.  It records only literal text cells; geometry is
// projected after the same router has completed the ordinary canvas.
struct FlowTextCollector {
    explicit FlowTextCollector(Work& work)
        : work_(work) {}

    bool prepare(int width, int height, std::size_t literal_cell_bound, std::size_t literal_bytes) {
        if (prepared_ || width <= 0 || height <= 0) return false;
        const auto area = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if (area / static_cast<std::size_t>(width) != static_cast<std::size_t>(height))
            return false;
        // The collector is the only new owner of this index and retained literal
        // copies. Row output is charged later from the real cropped geometry and
        // literal byte totals, before either output vector is allocated.
        // RFC 0031 E5: the classic/decision node_size label measurement
        // (former 352/literal-byte fee) is folded into this term.
        constexpr std::size_t kClassicMeasurePerByte =
            8u * sizeof(std::string_view) + 4u * sizeof(std::string) + 128u;
        constexpr std::size_t kClassicMeasureFixed = 64u * 512u;
        if (!charge({area * sizeof(int), literal_cell_bound * sizeof(std::string),
                     literal_bytes * (1u + kClassicMeasurePerByte) + kClassicMeasureFixed,
                     2u * area + 4096u}))
            return false;
        owners_.assign(area, 0);
        glyphs_.reserve(literal_cell_bound);
        width_ = width;
        height_ = height;
        prepared_ = true;
        return true;
    }

    // Re-arm after a discarded strict routing pass so the single bounded
    // relaxed retry repaints onto a fresh canvas. Same dimensions and bounds
    // as prepare(), charged anew; the strict pass's partial owner/glyph
    // records are discarded and never reach finish().
    bool restart(int width, int height, std::size_t literal_cell_bound, std::size_t literal_bytes) {
        prepared_ = false;
        owners_.clear();
        glyphs_.clear();
        ascii_rows_.reset();
        return prepare(width, height, literal_cell_bound, literal_bytes);
    }

    std::optional<int> row_count(std::string_view value) {
        if (!prepared_ || !charge({value.size() * 2u + 32u})) return {};
        int count = 1;
        for (const char value_byte : value)
            if (value_byte == '\n') ++count;
        return count;
    }

    bool multiline(Canvas& canvas, Point point, std::string_view value, int width) {
        if (!prepared_ || canvas.width != width_ || canvas.height != height_) return false;
        // Do not call label_rows here. Its vector allocation would otherwise
        // precede the collector payment. This single pass has the same LF row
        // boundaries and keeps the existing null-collector path untouched.
        for (std::size_t begin = 0u;;) {
            const auto newline = value.find('\n', begin);
            const auto end = newline == std::string_view::npos ? value.size() : newline;
            const auto row = value.substr(begin, end - begin);
            // FTXUI's Utf8ToGlyphs first copies `row` into a std::string, then
            // reserves one string slot per input byte. Its temporary `append`
            // strings and output payloads together cannot exceed two row-byte
            // copies. The retained glyph copy was reserved in prepare().
            if (!charge({sizeof(std::string) + row.size(), sizeof(std::vector<std::string>),
                         row.size() * sizeof(std::string), row.size() * 2u, row.size() * 4u + 32u}))
                return false;
            const std::string input(row);
            const auto cells = ftxui::Utf8ToGlyphs(input);
            // Utf8ToGlyphs includes exactly one empty cell for a full-width
            // glyph, so its cell count is the actual centered terminal width.
            Point cell{point.x + (width - static_cast<int>(cells.size())) / 2, point.y};
            for (const auto& glyph : cells) {
                if (!canvas.inside(cell)) return false;
                const auto index = static_cast<std::size_t>(canvas.index(cell));
                if (owners_[index] != 0 || !canvas.put(cell, glyph)) return false;
                glyphs_.emplace_back(glyph);
                owners_[index] = static_cast<int>(glyphs_.size());
                ++cell.x;
            }
            ++point.y;
            if (newline == std::string_view::npos) return true;
            begin = newline + 1u;
        }
    }

    std::optional<std::vector<std::string>> finish(const Canvas& canvas) {
        if (!prepared_ || canvas.width != width_ || canvas.height != height_) return {};
        const auto area = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
        // Pay the full native-canvas scan and its per-row literal accounting
        // before allocating the accounting vector or inspecting a canvas cell.
        if (!charge({area * 4u, static_cast<std::size_t>(height_) * sizeof(std::size_t) + 256u}))
            return {};
        std::vector<std::size_t> literal_bytes(static_cast<std::size_t>(height_), 0u);
        int x_min = width_, y_min = height_, x_max = -1, y_max = -1;
        for (int y = 0; y < height_; ++y)
            for (int x = 0; x < width_; ++x) {
                const auto index = static_cast<std::size_t>(canvas.index({x, y}));
                const auto& glyph = canvas.at({x, y}).glyph;
                const auto owner = owners_[index];
                if (owner != 0) {
                    // A later route or frame write must not be hidden by restoring
                    // a stale literal glyph into the ASCII projection.
                    if (owner < 1) return {};
                    const auto offset = static_cast<std::size_t>(owner - 1);
                    if (offset >= glyphs_.size() || glyph != glyphs_[offset]) return {};
                    if (glyph.size() > std::numeric_limits<std::size_t>::max() -
                                           literal_bytes[static_cast<std::size_t>(y)])
                        return {};
                    literal_bytes[static_cast<std::size_t>(y)] += glyph.size();
                }
                if (glyph != " ") {
                    x_min = std::min(x_min, x);
                    x_max = std::max(x_max, x);
                    y_min = std::min(y_min, y);
                    y_max = std::max(y_max, y);
                }
            }
        if (x_max < x_min || y_max < y_min) return {};
        const auto rows = static_cast<std::size_t>(y_max - y_min + 1);
        const auto columns = static_cast<std::size_t>(x_max - x_min + 1);
        constexpr std::size_t kNativeGeometryBytes = 3u;
        std::size_t output_capacity = 0u;
        for (int y = y_min; y <= y_max; ++y) {
            const auto literal = literal_bytes[static_cast<std::size_t>(y)];
            if (columns >
                (std::numeric_limits<std::size_t>::max() - literal) / (kNativeGeometryBytes + 1u))
                return {};
            const auto row_capacity = literal * 2u + columns * (kNativeGeometryBytes + 1u);
            if (row_capacity > std::numeric_limits<std::size_t>::max() - output_capacity) return {};
            output_capacity += row_capacity;
        }
        // Reserve both row vectors and every per-row native/ASCII capacity from
        // the measured geometry span plus the actual literal UTF-8 bytes.
        if (!charge({rows * 2u * sizeof(std::string), output_capacity, rows * columns * 2u + 256u}))
            return {};
        std::vector<std::string> native, ascii;
        native.reserve(rows);
        ascii.reserve(rows);
        for (int y = y_min; y <= y_max; ++y) {
            const auto literal = literal_bytes[static_cast<std::size_t>(y)];
            std::string native_row, ascii_row;
            native_row.reserve(columns * kNativeGeometryBytes + literal);
            ascii_row.reserve(columns + literal);
            for (int x = x_min; x <= x_max; ++x) {
                const auto index = static_cast<std::size_t>(canvas.index({x, y}));
                const auto& glyph = canvas.at({x, y}).glyph;
                native_row += glyph;
                if (owners_[index] != 0) {
                    ascii_row += glyphs_[static_cast<std::size_t>(owners_[index] - 1)];
                    continue;
                }
                const auto projected = geometry_glyph(glyph);
                if (!projected) return {};
                ascii_row += *projected;
            }
            native.emplace_back(std::move(native_row));
            ascii.emplace_back(std::move(ascii_row));
        }
        ascii_rows_ = std::move(ascii);
        return native;
    }

    std::optional<std::vector<std::string>> take_ascii_rows() {
        return std::exchange(ascii_rows_, std::nullopt);
    }

private:
    bool charge(std::initializer_list<std::size_t> costs) {
        std::size_t total = 0u;
        for (const auto cost : costs) {
            if (cost > std::numeric_limits<std::size_t>::max() - total) return false;
            total += cost;
        }
        return work_.spend(total);
    }

    static std::optional<std::string_view> geometry_glyph(std::string_view glyph) {
        if (glyph == " ") return " ";
        if (glyph == "─") return "-";
        if (glyph == "│") return "|";
        if (glyph == "╱") return "/";
        if (glyph == "╲") return "\\";
        if (glyph == "△") return "^";
        if (glyph == "┌" || glyph == "┐" || glyph == "└" || glyph == "┘" || glyph == "┬" ||
            glyph == "┤" || glyph == "┴" || glyph == "├" || glyph == "┼")
            return "+";
        if (glyph == "▶") return ">";
        if (glyph == "◀") return "<";
        if (glyph == "▼") return "v";
        if (glyph == "▽") return "v";
        if (glyph == "▲") return "^";
        if (glyph == "█" || glyph == "▛" || glyph == "▜" || glyph == "▙" || glyph == "▟")
            return "#";
        return {};
    }

    Work& work_;
    int width_ = 0, height_ = 0;
    bool prepared_ = false;
    std::vector<int> owners_;
    std::vector<std::string> glyphs_;
    std::optional<std::vector<std::string>> ascii_rows_;
};

bool paint_multiline(Canvas& canvas, FlowTextCollector* collector, Point point,
                     std::string_view value, int width) {
    return collector ? collector->multiline(canvas, point, value, width)
                     : canvas.multiline(point, value, width);
}

bool node_box(Canvas& canvas, const Box& box, const Vertex& node,
              FlowTextCollector* collector = nullptr,
              const std::vector<detail::LabelRun>* rich = nullptr) {
    for (int y = 0; y < box.height; ++y)
        for (int x = 0; x < box.width; ++x)
            if (!canvas.put({box.x + x, box.y + y}, " ")) return false;
    if (node.shape == Shape::Decision) {
        if (!canvas.work.spend(8u * sizeof(int) + 32u) || box.height < 5 || box.height % 2 == 0)
            return false;
        const int half = box.height / 2;
        if (box.width < 2 * half + 3) return false;
        for (int y = 0; y < box.height; ++y) {
            const int inset = std::abs(half - y);
            const auto left = y < half ? "╱" : y > half ? "╲" : "◇";
            const auto right = y < half ? "╲" : y > half ? "╱" : "◇";
            if (!canvas.put({box.x + inset, box.y + y}, left) ||
                !canvas.put({box.x + box.width - inset - 1, box.y + y}, right))
                return false;
        }
        for (int x = half + 1; x < box.width - half - 1; ++x)
            if (!canvas.put({box.x + x, box.y}, "─") ||
                !canvas.put({box.x + x, box.y + box.height - 1}, "─"))
                return false;
    } else if (classic_shape(node.shape)) {
        if (!classic_box(canvas, box, node.shape) ||
            !classic_port_corridors(canvas, box, node.shape))
            return false;
    } else {
        const auto top_left = node.shape == Shape::Rounded ? "╭" : "┌";
        const auto top_right = node.shape == Shape::Rounded ? "╮" : "┐";
        const auto bottom_left = node.shape == Shape::Rounded ? "╰" : "└";
        const auto bottom_right = node.shape == Shape::Rounded ? "╯" : "┘";
        for (int x = 1; x < box.width - 1; ++x)
            if (!canvas.put({box.x + x, box.y}, "─") ||
                !canvas.put({box.x + x, box.y + box.height - 1}, "─"))
                return false;
        for (int y = 1; y < box.height - 1; ++y)
            if (!canvas.put({box.x, box.y + y}, "│") ||
                !canvas.put({box.x + box.width - 1, box.y + y}, "│"))
                return false;
        if (!canvas.put({box.x, box.y}, top_left) ||
            !canvas.put({box.x + box.width - 1, box.y}, top_right) ||
            !canvas.put({box.x, box.y + box.height - 1}, bottom_left) ||
            !canvas.put({box.x + box.width - 1, box.y + box.height - 1}, bottom_right))
            return false;
    }
    const auto rows = collector
                          ? collector->row_count(node.text)
                          : std::optional<int>{static_cast<int>(label_rows(node.text).size())};
    if (!rows) return false;
    // Filled and crossed circles are label-less glyph discs upstream (their
    // node.label is cleared); painting text over them would be unreadable.
    if (node.shape == Shape::FilledCircle || node.shape == Shape::CrossedCircle)
        return true;
    // A divided rectangle reserves its top two rows (t, t+1) plus the divider
    // row as the title band; the label is centred across the body rows
    // t+3..b-1, i.e. offset 3 plus half of the (height-4-rows) remainder.
    int label_top = box.y + (box.height - *rows) / 2;
    if (node.shape == Shape::DividedRectangle && box.height >= 7)
        label_top = box.y + 3 + (box.height - 4 - *rows) / 2;
    // The stacked rectangle/document reserves its top row for the back-sheet
    // peek; the label is centred in the front chamber (rows t+1..b-1 for the
    // rectangle, t+1..b-1 for the document whose last row is the trough floor).
    if (node.shape == Shape::StackedRectangle ||
        node.shape == Shape::StackedDocument)
        label_top = box.y + 1 + (box.height - 1 - *rows) / 2;
    // The shaded process label is centred in the main chamber one column
    // right of the outer frame strip.
    const int label_left =
        node.shape == Shape::ShadedProcess ? box.x + 1 : box.x;
    const int label_width_box =
        node.shape == Shape::ShadedProcess ? box.width - 1 : box.width;
    const Point at{label_left, label_top};
    if (!collector && rich && !rich->empty())
        return canvas.multiline_rich(at, *rich, label_width_box);
    return paint_multiline(canvas, collector, at, node.text, label_width_box);
}

// Ranking ignores DFS back edges only in its temporary DAG. All original
// edges, including back edges and self loops, still go through the router.
struct RankedEdge {
    std::size_t target;
    int minimum_length;
};

std::optional<std::vector<int>> ranks(const Diagram& graph, Work& work) {
    const auto count = graph.vertices.size();
    if (count == 0) return std::vector<int>{};
    // Pay every rank-owned adjacency, stack, and layer predecessor before any
    // new vector allocation. The fixed graph limits bound this calculation.
    const auto rank_charge =
        count * (2u * sizeof(std::vector<RankedEdge>) + sizeof(std::vector<std::size_t>) +
                 3u * sizeof(std::vector<int>) + 3u * sizeof(std::size_t) + 48u) +
        graph.edges.size() * (8u * sizeof(RankedEdge) + 4u * sizeof(std::size_t) + 32u);
    if (!work.spend(rank_charge)) return {};
    std::vector<std::vector<RankedEdge>> children(count), dag(count);
    // ranks() accepts only the local vertex-only quotient built by
    // place_region(). Authored group endpoints must never become accidental
    // vector indexes here.
    const auto local_vertex = [&](const Endpoint& endpoint) -> std::optional<std::size_t> {
        if (endpoint.kind != EndpointKind::Vertex || endpoint.index >= count) return {};
        return endpoint.index;
    };
    for (const auto& edge : graph.edges) {
        const auto from = local_vertex(edge.from), to = local_vertex(edge.to);
        if (!from || !to || edge.minimum_length < 1 || edge.minimum_length > kHeightLimit)
            return {};
        if (*from != *to) children[*from].push_back({*to, edge.minimum_length});
    }
    // Prefer graph entries, then the first edge's source in cyclic components.
    // Standalone declaration order must not arbitrarily turn the main chain
    // into a long back edge which encloses the component's remaining ports.
    std::vector<std::size_t> roots;
    std::vector<int> incoming(count);
    for (const auto& edge : graph.edges)
        if (edge.from != edge.to) ++incoming[edge.to.index];
    for (std::size_t n = 0; n < count; ++n)
        if (!incoming[n]) roots.push_back(n);
    for (const auto& edge : graph.edges)
        roots.push_back(edge.from.index);
    for (std::size_t n = 0; n < count; ++n)
        roots.push_back(n);
    std::vector<int> color(count), order;
    for (const auto root : roots) {
        if (color[root]) continue;
        std::vector<std::pair<std::size_t, std::size_t>> stack{{root, 0}};
        color[root] = 1;
        while (!stack.empty()) {
            auto& [current, next] = stack.back();
            if (next == children[current].size()) {
                color[current] = 2;
                order.push_back(static_cast<int>(current));
                stack.pop_back();
                continue;
            }
            const auto edge = children[current][next++];
            // Preserve the existing DFS back-edge suppression: only retained
            // temporary-DAG edges may advance the weighted rank recurrence.
            if (color[edge.target] == 1) continue;
            dag[current].push_back(edge);
            if (color[edge.target] == 0) {
                color[edge.target] = 1;
                stack.emplace_back(edge.target, 0);
            }
        }
    }
    std::vector<int> result(count);
    for (auto at = order.rbegin(); at != order.rend(); ++at)
        for (const auto& edge : dag[static_cast<std::size_t>(*at)]) {
            const auto source = result[static_cast<std::size_t>(*at)];
            if (source > kHeightLimit - edge.minimum_length) return {};
            result[edge.target] = std::max(result[edge.target], source + edge.minimum_length);
        }
    return result;
}

// Opposite directed links are distinct relations, not parallel occurrences.
// Reversed headless links have the same physical endpoint identity.
bool repeated_strand_endpoints(const Edge& a, const Edge& b) {
    const auto same = [](const Endpoint& x, const Endpoint& y) {
        return x.kind == y.kind && x.index == y.index;
    };
    return (same(a.from, b.from) && same(a.to, b.to)) ||
           (!a.arrow_start && !a.arrow_end && !b.arrow_start && !b.arrow_end &&
            same(a.from, b.to) && same(a.to, b.from));
}

// Ranking projections discard caption strings; authored pair checks create
// these bounded band hints before projection can collapse endpoint identities.
struct CaptionBandHint {
    std::size_t from, to;
    int width;
};

std::optional<std::vector<Box>>
place_sized_nodes(const Diagram& graph, std::vector<Box> boxes, bool compact_major_gap, Work& work,
                  const std::vector<CaptionBandHint>* authored_hints = nullptr,
                  bool parallel_endpoints = false) {
    if (boxes.empty()) return boxes;
    const auto rank = ranks(graph, work);
    if (!rank || rank->empty()) return {};
    const auto last = *std::max_element(rank->begin(), rank->end());
    // The rank may contain intentional empty bands. Reject the maximum before
    // allocating layers, preserving the 240 x 1024 canvas cap.
    if (last < 0 || last >= kHeightLimit ||
        !work.spend((static_cast<std::size_t>(last) + 1u) *
                    (sizeof(std::vector<std::size_t>) + 4u * sizeof(std::size_t) + 32u)))
        return {};
    std::vector<std::vector<std::size_t>> layers(static_cast<std::size_t>(last + 1));
    for (std::size_t n = 0; n < rank->size(); ++n)
        layers[static_cast<std::size_t>((*rank)[n])].push_back(n);
    const bool horizontal =
        graph.direction == Direction::Right || graph.direction == Direction::Left;
    int cross_extent = 0;
    for (const auto& layer : layers) {
        int extent = -6;
        for (const auto n : layer) {
            extent += (horizontal ? boxes[n].height : boxes[n].width) + 6;
        }
        cross_extent = std::max(cross_extent, extent);
    }
    // The caller establishes this while it already measures each direct node.
    // The closed Classic11 LR chain needs a compact major-axis gap inside the
    // existing 240-cell canvas; old-shape placement retains its eight-cell gap.
    const int major_gap = compact_major_gap ? 2 : 8;
    std::optional<std::vector<int>> caption_gaps;
    // Only a labeled repeated endpoint pair widens its own intervening band.
    // Unrelated labels and other ranks keep their existing spacing.
    const auto widen = [&](std::size_t from_index, std::size_t to_index, int width) {
        if (from_index >= rank->size() || to_index >= rank->size()) return false;
        const int from = (*rank)[from_index], to = (*rank)[to_index];
        if (from == to) return true;
        if (!caption_gaps) {
            if (!work.spend(layers.size() * (sizeof(int) + 4u) + sizeof(std::vector<int>) + 64u))
                return false;
            caption_gaps.emplace(layers.size(), major_gap);
        }
        const auto band = static_cast<std::size_t>(std::min(from, to));
        (*caption_gaps)[band] = std::max((*caption_gaps)[band], width + 8);
        return true;
    };
    if (horizontal && !compact_major_gap && parallel_endpoints) {
        if (authored_hints) {
            for (const auto hint : *authored_hints) {
                if (!work.spend(64u) || !widen(hint.from, hint.to, hint.width)) return {};
            }
        } else if (graph.edges.size() > 1u) {
            for (std::size_t a = 0u; a < graph.edges.size(); ++a) {
                const auto& edge = graph.edges[a];
                if (!work.spend()) return {};
                if (!edge.text) continue;
                bool parallel = false;
                for (std::size_t b = 0u; b < graph.edges.size(); ++b) {
                    if (a == b) continue;
                    if (!work.spend(32u)) return {};
                    const auto& other = graph.edges[b];
                    if (repeated_strand_endpoints(edge, other)) {
                        parallel = true;
                        break;
                    }
                }
                if (!parallel) continue;
                if (edge.from.kind != EndpointKind::Vertex || edge.to.kind != EndpointKind::Vertex)
                    return {};
                if (!work.spend(edge.text->size() * 8u + 64u) ||
                    !widen(edge.from.index, edge.to.index, label_width(*edge.text)))
                    return {};
            }
        }
    }
    int major = 4;
    for (std::size_t band = 0u; band < layers.size(); ++band) {
        const auto& layer = layers[band];
        int extent = -6;
        int major_size = 0;
        for (const auto n : layer) {
            extent += (horizontal ? boxes[n].height : boxes[n].width) + 6;
            major_size = std::max(major_size, horizontal ? boxes[n].width : boxes[n].height);
        }
        int cross = 4 + (cross_extent - extent) / 2;
        for (const auto n : layer) {
            boxes[n].x = horizontal ? major : cross;
            boxes[n].y = horizontal ? cross : major;
            cross += (horizontal ? boxes[n].height : boxes[n].width) + 6;
        }
        major += major_size + (caption_gaps ? (*caption_gaps)[band] : major_gap);
    }
    int width = 0, height = 0;
    for (const auto& box : boxes) {
        width = std::max(width, box.x + box.width);
        height = std::max(height, box.y + box.height);
    }
    if (width + 4 > kWidthLimit || height + 4 > kHeightLimit) return {};
    for (auto& box : boxes) {
        if (graph.direction == Direction::Left) box.x = width + 4 - box.x - box.width;
        if (graph.direction == Direction::Up) box.y = height + 4 - box.y - box.height;
    }
    return boxes;
}

std::optional<std::vector<Box>> place_nodes(const Diagram& graph, Work& work,
                                            bool parallel_endpoints) {
    std::vector<Box> boxes;
    bool compact_major_gap = false;
    for (const auto& node : graph.vertices) {
        const auto size = node_size(node, work);
        if (!size) return {};
        compact_major_gap = compact_major_gap || classic_shape(node.shape);
        boxes.push_back(*size);
    }
    return place_sized_nodes(graph, std::move(boxes), compact_major_gap, work, nullptr,
                             parallel_endpoints);
}

struct Region {
    int width = 0, height = 0;
    std::vector<std::pair<std::size_t, Box>> nodes, groups;
};

// Compound regions are placed bottom-up. A parent ranks complete child frames,
// never individual descendants which could interleave with a sibling region.
std::optional<Region> place_region(const Diagram& graph, int owner, Direction inherited, Work& work,
                                   bool parallel_endpoints) {
    const auto direction =
        owner < 0 ? inherited
                  : graph.groups[static_cast<std::size_t>(owner)].direction.value_or(inherited);

    // FS-7b: a collapsed group short-circuits to a single titled rectangle.
    // It never enumerates its vertices or recurses into child groups, so all
    // descendants are absent from every ancestor Region. The leaf box is the
    // exact ordinary-rectangle node_size result (no title-band shift); the
    // group stays in Region.groups, never Region.nodes (group indices share
    // the vertex-slot namespace there).
    if (owner >= 0 && graph.groups[static_cast<std::size_t>(owner)].collapsed) {
        const auto& collapsed_group = graph.groups[static_cast<std::size_t>(owner)];
        const Vertex leaf{collapsed_group.id, collapsed_group.text, Shape::Rectangle,
                          false, false, -1, 0};
        const auto leaf_box = node_size(leaf, work);
        if (!leaf_box || leaf_box->width <= 0 || leaf_box->height <= 0 ||
            leaf_box->width > kWidthLimit || leaf_box->height > kHeightLimit)
            return {};
        Region leaf_region;
        leaf_region.width = leaf_box->width;
        leaf_region.height = leaf_box->height;
        leaf_region.groups.push_back({static_cast<std::size_t>(owner), *leaf_box});
        return leaf_region;
    }

    struct Item {
        std::size_t index, order;
        bool group;
        Box size;
        std::optional<Region> child;
    };

    std::size_t local_item_count = 0u;
    for (const auto& vertex : graph.vertices)
        if (vertex.group == owner) ++local_item_count;
    for (const auto& group : graph.groups)
        if (group.parent == owner) ++local_item_count;
    // The region owns its temporary item vector.  Prepay before reserve and
    // keep each child frame as one item rather than substituting its members.
    if (!work.spend(local_item_count *
                    (sizeof(Item) + sizeof(Box) + 3u * sizeof(std::size_t) + 32u)))
        return {};
    std::vector<Item> items;
    items.reserve(local_item_count);
    bool compact_major_gap = false;
    for (std::size_t n = 0; n < graph.vertices.size(); ++n) {
        const auto& node = graph.vertices[n];
        if (node.group != owner) continue;
        const auto size = node_size(node, work);
        if (!size) return {};
        compact_major_gap = compact_major_gap || classic_shape(node.shape);
        items.push_back({n, node.order, false, *size, {}});
    }
    for (std::size_t g = 0; g < graph.groups.size(); ++g) {
        if (graph.groups[g].parent != owner) continue;
        auto child = place_region(graph, static_cast<int>(g), direction, work, parallel_endpoints);
        if (!child) return {};
        const Box size{0, 0, child->width, child->height};
        items.push_back({g, graph.groups[g].order, true, size, std::move(child)});
    }
    std::ranges::sort(items, {}, &Item::order);

    // Project only this region's real direct vertices and child group frames
    // into the temporary ranking graph.  Authored endpoints remain tagged in
    // graph.edges; owner-frontier projection is only a layout implementation
    // detail, so it cannot invent a child vertex for a group endpoint.
    const auto projection_charge =
        (graph.vertices.size() + graph.groups.size()) *
            (sizeof(int) + 2u * sizeof(std::size_t) + 16u) +
        items.size() * (sizeof(Vertex) + sizeof(Box) + 16u) +
        graph.edges.size() * (sizeof(Endpoint) * 4u + sizeof(Edge) * 2u + 32u);
    if (!work.spend(projection_charge)) return {};
    std::vector<int> direct_vertex(graph.vertices.size(), -1);
    std::vector<int> direct_group(graph.groups.size(), -1);
    for (std::size_t n = 0; n < items.size(); ++n) {
        if (items[n].group)
            direct_group[items[n].index] = static_cast<int>(n);
        else
            direct_vertex[items[n].index] = static_cast<int>(n);
    }
    const auto frontier_group = [&](int group) -> std::optional<std::size_t> {
        std::size_t hops = 0u;
        while (group >= 0) {
            const auto index = static_cast<std::size_t>(group);
            if (index >= graph.groups.size()) return {};
            if (graph.groups[index].parent == owner) {
                const auto item = direct_group[index];
                return item >= 0 ? std::optional<std::size_t>{static_cast<std::size_t>(item)}
                                 : std::nullopt;
            }
            group = graph.groups[index].parent;
            if (++hops > graph.groups.size()) return {};
        }
        return {};
    };
    const auto item_for = [&](const Endpoint& endpoint) -> std::optional<std::size_t> {
        if (endpoint.kind == EndpointKind::Vertex) {
            if (endpoint.index >= graph.vertices.size()) return {};
            const auto group = graph.vertices[endpoint.index].group;
            if (group == owner) {
                const auto item = direct_vertex[endpoint.index];
                return item >= 0 ? std::optional<std::size_t>{static_cast<std::size_t>(item)}
                                 : std::nullopt;
            }
            return frontier_group(group);
        }
        if (endpoint.index >= graph.groups.size()) return {};
        const auto group = static_cast<int>(endpoint.index);
        if (group == owner) return {};
        return frontier_group(group);
    };

    Diagram quotient;
    quotient.direction = direction;
    quotient.vertices.resize(items.size());
    quotient.edges.reserve(graph.edges.size());
    std::vector<CaptionBandHint> authored_hints;
    const bool horizontal = direction == Direction::Left || direction == Direction::Right;
    for (const auto& edge : graph.edges) {
        const auto from = item_for(edge.from), to = item_for(edge.to);
        if (from && to && from != to) {
            // Preserve the legacy per-projected-edge copy tariff before the
            // quotient insertion. projection_charge separately prepays the
            // new typed projection tables and reserved quotient capacity.
            if (!work.spend(4u * sizeof(Edge) + 32u)) return {};
            quotient.edges.push_back({{EndpointKind::Vertex, *from},
                                      {EndpointKind::Vertex, *to},
                                      {},
                                      edge.stroke,
                                      edge.arrow_start,
                                      edge.arrow_end,
                                      edge.minimum_length});
            if (horizontal && !compact_major_gap && parallel_endpoints && edge.text) {
                bool parallel = false;
                for (const auto& other : graph.edges) {
                    if (&other == &edge) continue;
                    if (!work.spend(32u)) return {};
                    if (repeated_strand_endpoints(edge, other)) {
                        parallel = true;
                        break;
                    }
                }
                if (parallel) {
                    if (authored_hints.empty()) {
                        if (!work.spend(graph.edges.size() * (sizeof(CaptionBandHint) + 8u) +
                                        sizeof(std::vector<CaptionBandHint>) + 64u))
                            return {};
                        authored_hints.reserve(graph.edges.size());
                    }
                    if (!work.spend(edge.text->size() * 8u + 64u)) return {};
                    authored_hints.push_back({*from, *to, label_width(*edge.text)});
                }
            }
        }
    }
    std::vector<Box> sizes;
    sizes.reserve(items.size());
    for (const auto& item : items)
        sizes.push_back(item.size);
    auto placed = place_sized_nodes(quotient, std::move(sizes), compact_major_gap, work,
                                    &authored_hints, parallel_endpoints);
    if (!placed) return {};
    Region result;
    for (const auto& box : *placed) {
        result.width = std::max(result.width, box.x + box.width + 4);
        result.height = std::max(result.height, box.y + box.height + 4);
    }
    int shift_x = 0, shift_y = 0;
    if (owner >= 0) {
        const auto& title = graph.groups[static_cast<std::size_t>(owner)].text;
        const auto width = std::max(result.width, label_width(title) + 4);
        shift_x = (width - result.width) / 2;
        shift_y = static_cast<int>(label_rows(title).size()) + 2;
        result.width = width;
        result.height = std::max(2, result.height) + shift_y;
        result.groups.push_back(
            {static_cast<std::size_t>(owner), {0, 0, result.width, result.height}});
    }
    if (result.width <= 0 || result.height <= 0 || result.width > kWidthLimit ||
        result.height > kHeightLimit)
        return {};
    for (std::size_t n = 0; n < items.size(); ++n) {
        auto box = (*placed)[n];
        box.x += shift_x;
        box.y += shift_y;
        if (!items[n].group)
            result.nodes.push_back({items[n].index, box});
        else {
            auto append = [&](const auto& source, auto& target) {
                for (const auto& [index, relative] : source) {
                    auto absolute = relative;
                    absolute.x += box.x;
                    absolute.y += box.y;
                    target.push_back({index, absolute});
                }
            };
            append(items[n].child->nodes, result.nodes);
            append(items[n].child->groups, result.groups);
        }
    }
    return result;
}

bool group_frame(Canvas& canvas, const Box& box, std::string_view title,
                 const std::vector<detail::LabelRun>* rich = nullptr) {
    auto put = [&](Point point, unsigned bits, unsigned allowed, bool corner) {
        if (!canvas.put(point, line_glyph(bits), corner)) return false;
        canvas.at(point).bits = bits;
        canvas.at(point).allowed = allowed;
        return true;
    };
    for (int x = 1; x + 1 < box.width; ++x)
        if (!put({box.x + x, box.y}, kLeft | kRight, kUp | kDown, false) ||
            !put({box.x + x, box.y + box.height - 1}, kLeft | kRight, kUp | kDown, false))
            return false;
    for (int y = 1; y + 1 < box.height; ++y)
        if (!put({box.x, box.y + y}, kUp | kDown, kLeft | kRight, false) ||
            !put({box.x + box.width - 1, box.y + y}, kUp | kDown, kLeft | kRight, false))
            return false;
    if (!put({box.x, box.y}, kRight | kDown, 0, true) ||
        !put({box.x + box.width - 1, box.y}, kLeft | kDown, 0, true) ||
        !put({box.x, box.y + box.height - 1}, kRight | kUp, 0, true) ||
        !put({box.x + box.width - 1, box.y + box.height - 1}, kLeft | kUp, 0, true))
        return false;
    const Point title_at{box.x, box.y + 1};
    return rich ? canvas.multiline_rich(title_at, *rich, box.width)
                : canvas.multiline(title_at, title, box.width);
}

struct Port {
    Point border;
    Point outside;
    Point normal;
};

int distance(Point a, Point b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

std::vector<Port> ports(const Box& box, Shape shape, Point target) {
    std::vector<Port> result;
    const auto add = [&](Point border, Point normal) {
        result.push_back({border, border + normal, normal});
    };
    if (classic_shape(shape)) {
        const int middle_x = box.x + box.width / 2;
        const int middle_y = box.y + box.height / 2;
        if (shape == Shape::Triangle || shape == Shape::FlippedTriangle) {
            add({middle_x, box.y}, {0, -1});
            add({middle_x, box.y + box.height - 1}, {0, 1});
        } else if (shape == Shape::Datastore || shape == Shape::Hourglass ||
                   shape == Shape::BowTie) {
            // Open two-rule (datastore) and centre-waisted (hourglass/bow
            // tie) contours have no straight side wall at the middle row:
            // edges attach at the continuous top/bottom edges only.
            add({middle_x, box.y}, {0, -1});
            add({middle_x, box.y + box.height - 1}, {0, 1});
        } else if (shape == Shape::Folder) {
            // Only the left tab occupies the top row; a top edge attaches to
            // the full-width body edge one row below. The body walls are
            // straight, so expose the normal four cardinal ports.
            add({middle_x, box.y + 1}, {0, -1});
            add({middle_x, box.y + box.height - 1}, {0, 1});
            add({box.x, middle_y}, {-1, 0});
            add({box.x + box.width - 1, middle_y}, {1, 0});
        } else if (shape == Shape::StackedRectangle) {
            // Top edge attaches to the front chamber (row t+1, below the
            // back-sheet peek); other ports sit on the front/back walls and
            // bottom edge (corridor unblocks the centre back cell).
            add({middle_x, box.y + 1}, {0, -1});
            add({middle_x, box.y + box.height - 1}, {0, 1});
            add({box.x, middle_y}, {-1, 0});
            add({box.x + box.width - 1, middle_y}, {1, 0});
        } else if (shape == Shape::TaggedRectangle) {
            // The right side port lands on the FRONT wall one column inside;
            // the folded tip column is opened by the corridor. Top/bottom and
            // the left port sit on the front rectangle.
            add({middle_x, box.y}, {0, -1});
            add({middle_x, box.y + box.height - 1}, {0, 1});
            add({box.x, middle_y}, {-1, 0});
            add({box.x + box.width - 2, middle_y}, {1, 0});
        } else if (shape == Shape::Document || shape == Shape::LinedDocument) {
            // The wave trough is centred on l + (width-1)/2 (the baseline's
            // descent/aspect centre), which differs from width/2 by one column
            // for even widths; top and bottom ports must share that axis so an
            // incoming TB arrow meets the trough floor cell. Side walls stay
            // straight at the middle row.
            const int wave_x = box.x + (box.width - 1) / 2;
            add({wave_x, box.y}, {0, -1});
            add({wave_x, box.y + box.height - 1}, {0, 1});
            add({box.x, middle_y}, {-1, 0});
            add({box.x + box.width - 1, middle_y}, {1, 0});
        } else if (shape == Shape::StackedDocument) {
            // Front chamber is one column narrower than the box (back sheet
            // peeks right); top attaches to the front edge and bottom to the
            // trough floor, both on the chamber centre column.
            const int cx = middle_x - 1;
            add({cx, box.y + 1}, {0, -1});
            add({cx, box.y + box.height - 1}, {0, 1});
            add({box.x, middle_y}, {-1, 0});
            add({box.x + box.width - 1, middle_y}, {1, 0});
        } else if (shape == Shape::TaggedDocument) {
            // Chamber centre carries top and trough-floor ports; side ports
            // meet the tag centre row, the right one landing on the tip.
            const int cx = middle_x - 1;
            const int tag_y = box.y + (box.height - 2) / 2;
            add({cx, box.y}, {0, -1});
            add({cx, box.y + box.height - 1}, {0, 1});
            add({box.x, tag_y}, {-1, 0});
            add({box.x + box.width - 1, tag_y}, {1, 0});
        } else if (shape == Shape::PaperTape) {
            // Top attaches to the crest cap and bottom to the trough floor;
            // both lie on the wave centre column (even widths included).
            const int wave_x = box.x + (box.width - 1) / 2;
            add({wave_x, box.y}, {0, -1});
            add({wave_x, box.y + box.height - 1}, {0, 1});
            add({box.x, middle_y}, {-1, 0});
            add({box.x + box.width - 1, middle_y}, {1, 0});
        } else if (shape == Shape::Asymmetric) {
            add({box.x, middle_y}, {-1, 0});
            add({box.x + box.width - 1, middle_y}, {1, 0});
        } else {
            add({middle_x, box.y}, {0, -1});
            add({middle_x, box.y + box.height - 1}, {0, 1});
            const bool shifted_side = shape == Shape::Trapezoid || shape == Shape::TrapezoidAlt ||
                                      shape == Shape::Parallelogram ||
                                      shape == Shape::ParallelogramAlt;
            const int left = box.x + (shifted_side ? 1 : 0);
            const int right = box.x + box.width - 1 - (shifted_side ? 1 : 0);
            add({left, middle_y}, {-1, 0});
            add({right, middle_y}, {1, 0});
        }
    } else {
        const int inset = shape == Shape::Decision ? box.height / 2 + 1 : 1;
        for (int x = inset; x < box.width - inset; ++x) {
            add({box.x + x, box.y}, {0, -1});
            add({box.x + x, box.y + box.height - 1}, {0, 1});
        }
        for (int y = 1; y < box.height - 1; ++y) {
            if (shape == Shape::Decision && y != box.height / 2) continue;
            add({box.x, box.y + y}, {-1, 0});
            add({box.x + box.width - 1, box.y + y}, {1, 0});
        }
    }
    const Point center{box.x + box.width / 2, box.y + box.height / 2};
    std::stable_sort(result.begin(), result.end(), [&](const Port& a, const Port& b) {
        const auto score = [&](const Port& port) {
            const auto middle = port.normal.x ? std::abs(port.border.y - center.y)
                                              : std::abs(port.border.x - center.x);
            return distance(port.outside, target) * 4 + middle;
        };
        return score(a) < score(b);
    });
    return result;
}

struct Connection {
    std::vector<Point> path;
    std::size_t source, target;
};

class Router {
    Canvas& canvas_;
    std::vector<int> distance_, parent_;
    std::vector<unsigned> seen_;
    unsigned search_ = 0;
    // Strict-first pass 2 only: cells painted by earlier connector interiors.
    // When non-null the A* expansion may enter such a blocked cell (route
    // crossing); node/label cells, frame corners, port heads and temporary
    // reservations are never marked, so terminals and painted art stay sealed.
    // Nullptr selects the exact strict search pass 1 always used.
    const std::vector<unsigned char>* crossable_ = nullptr;
    // Relaxed mode shares one terminal/origin scratch across all edges, paid
    // once per pass instead of per connection() call (the strict pass keeps
    // its per-call allocation and fee untouched).
    std::vector<int> terminal_relaxed_, origin_relaxed_;
    bool relaxed_scratch_paid_ = false;

    // Mirrors Canvas::free (one Work unit per probe, inside bounds) but treats
    // marked earlier-connector cells as enterable in the relaxed pass.
    bool enterable(Point point) const {
        if (!canvas_.work.spend() || !canvas_.inside(point)) return false;
        if (!canvas_.at(point).blocked) return true;
        return crossable_ != nullptr &&
               (*crossable_)[static_cast<std::size_t>(canvas_.index(point))] != 0u;
    }

public:
    explicit Router(Canvas& canvas, const std::vector<unsigned char>* crossable = nullptr)
        : canvas_(canvas)
        , distance_(canvas.cells.size())
        , parent_(canvas.cells.size())
        , seen_(canvas.cells.size())
        , crossable_(crossable) {
        if (crossable != nullptr) {
            terminal_relaxed_.assign(canvas.cells.size(), -1);
            origin_relaxed_.assign(canvas.cells.size(), -1);
        }
    }

    // Landing cells are terminals, entered only toward their node border.
    // A multi-source search chooses ports using actual route length, avoiding
    // long detours around the source which can enclose its remaining ports.
    std::optional<Connection> connection(const std::vector<Port>& sources,
                                         const std::vector<Port>& targets,
                                         const std::unordered_set<int>& used) {
        const bool relaxed = crossable_ != nullptr;
        if (relaxed) {
            // One workspace fee for the whole relaxed pass, charged on the
            // first multi-source search.
            if (!relaxed_scratch_paid_) {
                if (!canvas_.work.spend(canvas_.cells.size() * 2)) return {};
                relaxed_scratch_paid_ = true;
            }
            std::fill(terminal_relaxed_.begin(), terminal_relaxed_.end(), -1);
            std::fill(origin_relaxed_.begin(), origin_relaxed_.end(), -1);
        } else if (!canvas_.work.spend(canvas_.cells.size() * 2))
            return {};
        std::vector<int> terminal_strict, origin_strict;
        std::vector<int>* terminal = &terminal_relaxed_;
        std::vector<int>* origin = &origin_relaxed_;
        if (!relaxed) {
            terminal_strict.assign(canvas_.cells.size(), -1);
            origin_strict.assign(canvas_.cells.size(), -1);
            terminal = &terminal_strict;
            origin = &origin_strict;
        }
        Point low{canvas_.width, canvas_.height}, high{-1, -1};
        for (std::size_t n = 0; n < targets.size(); ++n) {
            if (!canvas_.work.spend()) return {};
            const auto& port = targets[n];
            const auto before = port.outside + port.normal;
            if (used.contains(canvas_.index(port.border)) || !canvas_.free(port.outside) ||
                !canvas_.free(before) ||
                !(canvas_.at(port.outside).allowed & direction_bits(port.normal)) ||
                !(canvas_.at(before).allowed & direction_bits(port.normal)))
                continue;
            (*terminal)[static_cast<std::size_t>(canvas_.index(port.outside))] =
                static_cast<int>(n);
            low.x = std::min(low.x, port.outside.x);
            low.y = std::min(low.y, port.outside.y);
            high.x = std::max(high.x, port.outside.x);
            high.y = std::max(high.y, port.outside.y);
        }
        if (high.x < 0) return {};
        const auto heuristic = [&](Point point) {
            return std::max({low.x - point.x, point.x - high.x, 0}) +
                   std::max({low.y - point.y, point.y - high.y, 0});
        };
        ++search_;
        using Entry = std::tuple<int, std::size_t, int>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
        std::size_t serial = 0;
        for (std::size_t n = 0; n < sources.size(); ++n) {
            if (!canvas_.work.spend()) return {};
            const auto& port = sources[n];
            if (used.contains(canvas_.index(port.border)) || !canvas_.free(port.outside) ||
                !(canvas_.at(port.outside).allowed & direction_bits(port.normal)))
                continue;
            const auto index = static_cast<std::size_t>(canvas_.index(port.outside));
            if (seen_[index] == search_ || (*terminal)[index] >= 0) continue;
            seen_[index] = search_;
            distance_[index] = 0;
            parent_[index] = -1;
            (*origin)[index] = static_cast<int>(n);
            queue.emplace(heuristic(port.outside), serial++, static_cast<int>(index));
        }
        while (!queue.empty()) {
            if (!canvas_.work.spend()) return {};
            const auto [priority, order, current_index] = queue.top();
            queue.pop();
            (void)order;
            const auto index = static_cast<std::size_t>(current_index);
            const Point current{current_index % canvas_.width, current_index / canvas_.width};
            if (priority != distance_[index] + heuristic(current)) continue;
            if ((*terminal)[index] >= 0) {
                Connection result{{},
                                  static_cast<std::size_t>((*origin)[index]),
                                  static_cast<std::size_t>((*terminal)[index])};
                for (int at = current_index; at >= 0; at = parent_[static_cast<std::size_t>(at)]) {
                    if (!canvas_.work.spend()) return {};
                    result.path.push_back({at % canvas_.width, at / canvas_.width});
                }
                std::reverse(result.path.begin(), result.path.end());
                return result;
            }
            for (const auto step : kSteps) {
                if (!canvas_.work.spend()) return {};
                const auto next = current + step;
                if (!enterable(next) || !(canvas_.at(current).allowed & direction_bits(step)) ||
                    !(canvas_.at(next).allowed & direction_bits(Point{} - step)))
                    continue;
                const auto next_index = static_cast<std::size_t>(canvas_.index(next));
                const auto target = (*terminal)[next_index];
                if (target >= 0 &&
                    step != Point{} - targets[static_cast<std::size_t>(target)].normal)
                    continue;
                const auto cost = distance_[index] + 1;
                if (seen_[next_index] == search_ && cost >= distance_[next_index]) continue;
                seen_[next_index] = search_;
                distance_[next_index] = cost;
                parent_[next_index] = current_index;
                (*origin)[next_index] = (*origin)[index];
                queue.emplace(cost + heuristic(next), serial++, static_cast<int>(next_index));
            }
        }
        return {};
    }

    std::optional<std::vector<Point>> path(Point start, Point goal) {
        if (!canvas_.free(start) || !canvas_.free(goal)) return {};
        ++search_;
        using QueueItem = std::pair<int, int>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
        const auto first = canvas_.index(start), last = canvas_.index(goal);
        seen_[static_cast<std::size_t>(first)] = search_;
        distance_[static_cast<std::size_t>(first)] = 0;
        parent_[static_cast<std::size_t>(first)] = -1;
        queue.emplace(distance(start, goal), first);
        while (!queue.empty()) {
            if (!canvas_.work.spend()) return {};
            const auto [priority, index] = queue.top();
            queue.pop();
            const Point current{index % canvas_.width, index / canvas_.width};
            if (priority != distance_[static_cast<std::size_t>(index)] + distance(current, goal))
                continue;
            if (index == last) {
                std::vector<Point> result;
                for (int at = last; at >= 0; at = parent_[static_cast<std::size_t>(at)]) {
                    if (!canvas_.work.spend()) return {};
                    result.push_back({at % canvas_.width, at / canvas_.width});
                }
                std::reverse(result.begin(), result.end());
                return result;
            }
            for (const auto step : kSteps) {
                if (!canvas_.work.spend()) return {};
                const auto next = current + step;
                if (!enterable(next)) continue;
                if (!(canvas_.at(current).allowed & direction_bits(step)) ||
                    !(canvas_.at(next).allowed & direction_bits(Point{} - step)))
                    continue;
                const auto next_index = static_cast<std::size_t>(canvas_.index(next));
                const auto cost = distance_[static_cast<std::size_t>(index)] + 1;
                if (seen_[next_index] == search_ && cost >= distance_[next_index]) continue;
                seen_[next_index] = search_;
                distance_[next_index] = cost;
                parent_[next_index] = index;
                queue.emplace(cost + distance(next, goal), static_cast<int>(next_index));
            }
        }
        return {};
    }
};

bool draw_path(Canvas& canvas, const std::vector<Point>& path, Point before, Point after,
               bool arrow_start, bool arrow_end, Stroke stroke = Stroke::Solid,
               std::optional<Point> prefixed_head = {}, std::optional<Point> suffixed_head = {},
               std::vector<unsigned char>* routed = nullptr, bool orthogonal_only = false) {
    if (path.empty() || (arrow_start && arrow_end &&
                         path.size() + prefixed_head.has_value() + suffixed_head.has_value() == 1u))
        return false;
    const auto prefix = prefixed_head ? 1u : 0u;
    const auto total = path.size() + prefix + (suffixed_head ? 1u : 0u);
    const auto point_at = [&](std::size_t index) {
        if (prefixed_head && index == 0) return *prefixed_head;
        if (suffixed_head && index + 1u == total) return *suffixed_head;
        return path[index - prefix];
    };
    for (std::size_t n = 0; n < total; ++n) {
        if (orthogonal_only && routed && !canvas.work.spend(16u)) return false;
        const auto current = point_at(n);
        const auto prior = n ? point_at(n - 1u) : before;
        const auto next = n + 1u < total ? point_at(n + 1u) : after;
        const auto bits = direction_bits(prior - current) | direction_bits(next - current) |
                          canvas.at(current).bits;
        auto glyph = line_glyph(bits);
        if (bits == (kLeft | kRight) && stroke != Stroke::Solid)
            glyph = stroke == Stroke::Dotted ? "┄" : "━";
        if (bits == (kUp | kDown) && stroke != Stroke::Solid)
            glyph = stroke == Stroke::Dotted ? "┆" : "┃";
        const bool head = (arrow_start && n == 0) || (arrow_end && n + 1u == total);
        const auto painted = arrow_start && n == 0          ? arrow(before - current)
                             : arrow_end && n + 1u == total ? arrow(after - current)
                                                            : glyph;
        if (!canvas.put(current, painted)) return false;
        // Relaxed pass bookkeeping: arrow heads stay sealed (never marked),
        // line interiors become crossable and their union bits persist so a
        // later connector crossing here renders the correct junction glyph.
        if (routed != nullptr && !head) {
            (*routed)[static_cast<std::size_t>(canvas.index(current))] = 1u;
            canvas.at(current).bits = bits;
            if (orthogonal_only) {
                const bool horizontal = bits == (kLeft | kRight), vertical = bits == (kUp | kDown);
                (*routed)[static_cast<std::size_t>(canvas.index(current))] =
                    (horizontal || vertical) ? 1u : 0u;
                canvas.at(current).allowed = horizontal ? (kUp | kDown)
                                             : vertical ? (kLeft | kRight)
                                                        : 0u;
            }
        } else if (routed && orthogonal_only) {
            (*routed)[static_cast<std::size_t>(canvas.index(current))] = 0u;
            canvas.at(current).allowed = 0u;
        }
    }
    return true;
}

bool source_port(Canvas& canvas, const Port& port) {
    auto bits = port.normal.x ? kUp | kDown : kLeft | kRight;
    bits |= direction_bits(port.normal);
    if (canvas.at(port.border).glyph == "◇") return true;
    return canvas.put(port.border, line_glyph(bits));
}

struct ScopedBlockedCell {
    Canvas& canvas;
    Point point;
    bool active = false, previous = false;

    ScopedBlockedCell(Canvas& canvas_, Point point_, bool reserve)
        : canvas(canvas_)
        , point(point_)
        , active(reserve) {
        if (active) {
            previous = canvas.at(point).blocked;
            canvas.at(point).blocked = true;
        }
    }

    ScopedBlockedCell(const ScopedBlockedCell&) = delete;
    ScopedBlockedCell& operator=(const ScopedBlockedCell&) = delete;

    ~ScopedBlockedCell() { release(); }

    void release() {
        if (!active) return;
        canvas.at(point).blocked = previous;
        active = false;
    }
};

// Group endpoints may border old relaxed-pass crossings. A hard temporary
// reservation must suppress both ordinary occupancy and crossing eligibility.
struct ScopedGroupBorder {
    ScopedBlockedCell cell;
    std::vector<unsigned char>* routed = nullptr;
    unsigned char previous = 0;
    bool active = false;

    ScopedGroupBorder(Canvas& canvas, Point point, bool reserve,
                      std::vector<unsigned char>* crossing)
        : cell(canvas, point, reserve)
        , routed(crossing)
        , active(reserve) {
        if (active && routed) {
            const auto index = static_cast<std::size_t>(canvas.index(point));
            previous = (*routed)[index];
            (*routed)[index] = 0u;
        }
    }

    ScopedGroupBorder(const ScopedGroupBorder&) = delete;
    ScopedGroupBorder& operator=(const ScopedGroupBorder&) = delete;

    ~ScopedGroupBorder() { release(); }

    void release() {
        if (!active) return;
        if (routed) (*routed)[static_cast<std::size_t>(cell.canvas.index(cell.point))] = previous;
        cell.release();
        active = false;
    }
};

// A first label half is a hard temporary reservation, even over old routes.
// Storage and restoration are paid before changing any cell; reverse restore
// also handles a repeated cell safely without weakening its original state.
struct ScopedPathReservation {
    struct Saved {
        bool blocked;
        unsigned char crossable;
    };

    Canvas& canvas;
    const std::vector<Point>& path;
    std::vector<unsigned char>* routed;
    std::vector<Saved> saved;
    bool ready = false, active = false;

    ScopedPathReservation(Canvas& target, const std::vector<Point>& points,
                          std::vector<unsigned char>* crossing)
        : canvas(target)
        , path(points)
        , routed(crossing) {
        // A strict Router enters only free cells and cannot cross blocked art.
        // With no crossing bitmap every prior state is therefore false; retain
        // its existing two-write tariff and avoid storing redundant states.
        if (!routed) {
            if (!canvas.work.spend(path.size() * 2u)) return;
            for (const auto point : path)
                canvas.at(point).blocked = true;
            ready = true;
            active = true;
            return;
        }
        // Pay saved bytes plus the four state writes (reserve and restore).
        if (!canvas.work.spend(path.size() * (sizeof(Saved) + 4u) + 4u * sizeof(void*) + 128u))
            return;
        saved.reserve(path.size());
        for (const auto point : path) {
            const auto index = static_cast<std::size_t>(canvas.index(point));
            saved.push_back({canvas.at(point).blocked, (*routed)[index]});
            canvas.at(point).blocked = true;
            (*routed)[index] = 0u;
        }
        ready = true;
        active = true;
    }

    ScopedPathReservation(const ScopedPathReservation&) = delete;
    ScopedPathReservation& operator=(const ScopedPathReservation&) = delete;

    void release() {
        if (!active) return;
        for (std::size_t i = path.size(); i > 0u; --i) {
            const auto point = path[i - 1u];
            canvas.at(point).blocked = routed ? saved[i - 1u].blocked : false;
            if (routed)
                (*routed)[static_cast<std::size_t>(canvas.index(point))] = saved[i - 1u].crossable;
        }
        active = false;
    }

    ~ScopedPathReservation() { release(); }
};

void seal_group_port(Canvas& canvas, Point border, std::vector<unsigned char>* routed) {
    canvas.at(border).blocked = true;
    if (routed) (*routed)[static_cast<std::size_t>(canvas.index(border))] = 0u;
}

bool connect(Canvas& canvas, Router& router, const Port& from, const Port& to,
             const std::optional<std::string>& label_text, bool arrow_start, bool arrow_end,
             Stroke stroke = Stroke::Solid, FlowTextCollector* collector = nullptr,
             std::vector<unsigned char>* routed = nullptr, bool protect_from = false,
             bool protect_to = false, bool orthogonal_only = false,
             const std::vector<detail::LabelRun>* rich_label = nullptr) {
    if ((protect_from || protect_to) && !canvas.work.spend(8u * sizeof(ScopedGroupBorder) + 128u))
        return false;
    const auto start = arrow_start ? from.outside + from.normal : from.outside;
    const auto goal = to.outside + to.normal;
    if (from.border == to.border || !canvas.free(from.outside) || !canvas.free(start) ||
        !canvas.free(to.outside) || !canvas.free(goal))
        return false;
    if (!(canvas.at(from.outside).allowed & direction_bits(from.normal)) ||
        !(canvas.at(to.outside).allowed & direction_bits(to.normal)) ||
        !(canvas.at(goal).allowed & direction_bits(to.normal)))
        return false;
    // A point source must leave its frame by the straight outward shaft. Keep
    // its arrow cell unavailable to the search, release it before paint, and
    // logically prepend it to the first painted path without a new vector.
    // A group border is permeable to other crossings, but an endpoint's own
    // port cannot also be an interior route cell. Release before committing
    // paint so the permanent port remains sealed on success.
    ScopedGroupBorder source_border{canvas, from.border, protect_from, routed};
    ScopedGroupBorder target_border{canvas, to.border, protect_to, routed};
    ScopedBlockedCell target_hold{canvas, to.outside, true};
    ScopedBlockedCell source_head{canvas, from.outside, arrow_start};
    const auto prefix = arrow_start ? std::optional<Point>{from.outside} : std::nullopt;
    const auto suffix = std::optional<Point>{to.outside};
    if (!label_text) {
        auto path = router.path(start, goal);
        target_hold.release();
        source_head.release();
        source_border.release();
        target_border.release();
        if (!path) return false;
        if (!draw_path(canvas, *path, from.border, to.border, arrow_start, arrow_end, stroke,
                       prefix, suffix, routed, orthogonal_only))
            return false;
        if (!arrow_start && !source_port(canvas, from)) return false;
        if (!arrow_end && !source_port(canvas, to)) return false;
        if (protect_from) seal_group_port(canvas, from.border, routed);
        if (protect_to) seal_group_port(canvas, to.border, routed);
        return true;
    }
    const auto length = label_width(*label_text);
    const auto row_count =
        collector ? collector->row_count(*label_text)
                  : std::optional<int>{static_cast<int>(label_rows(*label_text).size())};
    if (!row_count) return false;
    const int rows = *row_count;
    const Point ideal{(start.x + goal.x - length) / 2, (start.y + goal.y - rows + 1) / 2};
    // Try nearby label positions without an unbounded search over the plane.
    for (int radius = 0; radius < 32 && canvas.work.remaining; ++radius) {
        const Point offsets[]{{0, radius}, {0, -radius}, {radius, 0}, {-radius, 0}};
        for (const auto offset : offsets) {
            const auto position = ideal + offset;
            const Point left{position.x - 1, position.y + rows / 2},
                right{position.x + length, position.y + rows / 2};
            bool free = canvas.free(left) && canvas.free(right) && left != start &&
                        right != start && left != goal && right != goal;
            for (int y = 0; free && y < rows; ++y)
                for (int x = 0; free && x < length; ++x) {
                    if (!canvas.work.spend()) {
                        free = false;
                        break;
                    }
                    const Point cell{position.x + x, position.y + y};
                    free = canvas.free(cell) && canvas.at(cell).bits == 0 && cell != start &&
                           cell != goal;
                }
            if (!free) continue;
            if (!canvas.work.spend(static_cast<std::size_t>(length * rows) * 2u)) break;
            auto reserve = [&](bool blocked) {
                for (int y = 0; y < rows; ++y)
                    for (int x = 0; x < length; ++x)
                        canvas.at({position.x + x, position.y + y}).blocked = blocked;
            };
            reserve(true);
            const bool reverse_label = orthogonal_only && start.x > goal.x;
            const auto entry = reverse_label ? right : left, exit = reverse_label ? left : right;
            auto first = router.path(start, entry);
            std::optional<std::vector<Point>> second;
            if (first) {
                ScopedPathReservation held(canvas, *first, routed);
                if (!held.ready) {
                    reserve(false);
                    return false;
                }
                second = router.path(exit, goal);
            }
            reserve(false);
            if (!first || !second) continue;
            target_hold.release();
            source_head.release();
            source_border.release();
            target_border.release();
            if (!draw_path(canvas, *first, from.border,
                           {entry.x + (reverse_label ? -1 : 1), entry.y}, arrow_start, false,
                           stroke, prefix, {}, routed, orthogonal_only) ||
                !draw_path(canvas, *second, {exit.x + (reverse_label ? 1 : -1), exit.y}, to.border,
                           false, arrow_end, stroke, {}, suffix, routed, orthogonal_only))
                return false;
            reserve(true);
            const bool label_painted =
                (!collector && rich_label && !rich_label->empty())
                    ? canvas.multiline_rich(position, *rich_label, length)
                    : paint_multiline(canvas, collector, position, *label_text, length);
            if (!label_painted) {
                // The ordinary branch returns as before. The collector may fail
                // after recording a private cell, so release its temporary text
                // reservation before the discarded ASCII layout falls back.
                if (collector) reserve(false);
                return false;
            }
            if (!arrow_start && !source_port(canvas, from)) return false;
            if (!arrow_end && !source_port(canvas, to)) return false;
            if (protect_from) seal_group_port(canvas, from.border, routed);
            if (protect_to) seal_group_port(canvas, to.border, routed);
            return true;
        }
    }
    return false;
}

bool paint_flow_styles(Canvas& canvas, const Diagram& graph, const FlowStyles& styles,
                       const std::vector<Box>& boxes, Work& work) {
    if (styles.effective.size() != graph.vertices.size() || boxes.size() != graph.vertices.size())
        return false;
    const auto count = canvas.cells.size();
    if (!work.spend(count * (sizeof(SemanticCellStyle) + 8u) +
                    4u * sizeof(std::vector<SemanticCellStyle>) + 256u))
        return false;
    // Markdown span emphasis may already own the style plane; reuse it and
    // merge bold/underline rather than wiping label emphasis.
    if (!canvas.styles)
        canvas.styles = std::make_unique<std::vector<SemanticCellStyle>>(count);
    else if (canvas.styles->size() != count)
        return false;
    for (std::size_t n = 0; n < boxes.size(); ++n) {
        const auto& properties = styles.effective[n];
        if (!properties.fill && !properties.stroke && !properties.color && !properties.width)
            continue;
        if (graph.vertices[n].shape != Shape::Rectangle || graph.vertices[n].group >= 0)
            return false;
        const auto& box = boxes[n];
        const bool filled = properties.fill && !properties.fill->transparent;
        for (int y = box.y; y < box.y + box.height; ++y)
            for (int x = box.x; x < box.x + box.width; ++x) {
                if (!work.spend(16u)) return false;
                const Point at{x, y};
                if (!canvas.inside(at)) return false;
                const bool border = x == box.x || y == box.y || x == box.x + box.width - 1 ||
                                    y == box.y + box.height - 1;
                const bool stroke = border || canvas.at(at).bits != 0u;
                SemanticCellStyle value =
                    (*canvas.styles)[static_cast<std::size_t>(canvas.index(at))];
                if (!border && filled) value.fill = properties.fill->role;
                if (stroke) {
                    if (properties.stroke) value.foreground = properties.stroke->role;
                    value.bold = value.bold ||
                                 (properties.width && *properties.width >= 4.0);
                } else if (filled)
                    value.foreground = SemanticColorRole::Neutral;
                else if (properties.color)
                    value.foreground = properties.color->role;
                (*canvas.styles)[static_cast<std::size_t>(canvas.index(at))] = value;
            }
    }
    return true;
}

// B6 dagre Flow admission gate. The terminal layout is not dagre: it ranks
// child group frames as atomic quotient items but routes vertex connectors
// to the real descendant boxes, so a connector between two disjoint nested
// subtrees overpaints frame walls/title bands that no rank reserved. This
// is a POSITIVE, structural gate for the two compound profiles known to be
// unfaithful, evaluated only for a whole source whose frontmatter selected
// dagre. It never keys on `layout: dagre` alone (many dagre-flagged flows
// paint faithfully), and it runs only on the configured envelope path, so
// frontmatter-stripped body diagnostics and bare/ASCII graphs are exempt.
// Returns true when the configured whole source must stay whole-null
// (ordinary fallback). A Work exhaustion is a safe whole-source reject.
bool flow_untrusted_dagre_compound(const Diagram& graph, Work& work) {
    const std::size_t vertices = graph.vertices.size(), groups = graph.groups.size(),
                      edges = graph.edges.size();
    if (!work.spend(vertices * 16u + edges * 16u + groups * 32u + 512u)) return true;
    if (groups == 0u) return false;

    auto is_ancestor = [&](int ancestor, int descendant) {
        for (int node = descendant, hops = 0; node >= 0 && hops <= static_cast<int>(groups);
             ++hops, node = graph.groups[static_cast<std::size_t>(node)].parent)
            if (node == ancestor) return true;
        return false;
    };
    auto owner_group = [&](const Endpoint& endpoint) -> int {
        if (endpoint.kind != EndpointKind::Vertex || endpoint.index >= vertices) return -1;
        return graph.vertices[endpoint.index].group;
    };

    // Clause D: a vertex edge between two disjoint owner groups, both nested
    // below root (the route crosses each leaf frame and an unreserved
    // enclosing interior).
    for (const auto& edge : graph.edges) {
        const int a = owner_group(edge.from), b = owner_group(edge.to);
        if (a >= 0 && b >= 0 && a != b && graph.groups[static_cast<std::size_t>(a)].parent >= 0 &&
            graph.groups[static_cast<std::size_t>(b)].parent >= 0 && !is_ancestor(a, b) &&
            !is_ancestor(b, a))
            return true;
    }

    // Clause T: external u->v plus a shortcut u->m->v through one member m
    // of a group neither u nor v belongs to (the group frame is forced into
    // an intermediate rank while connectors reach the real in-frame box).
    auto subtree_contains_vertex = [&](int group, std::size_t vertex) {
        for (int node = graph.vertices[vertex].group, hops = 0;
             node >= 0 && hops <= static_cast<int>(groups);
             ++hops, node = graph.groups[static_cast<std::size_t>(node)].parent)
            if (node == group) return true;
        return false;
    };
    // A vertex edge matches a given endpoint.
    auto edge_between = [&](std::size_t from, std::size_t to) {
        const Endpoint a{EndpointKind::Vertex, from}, b{EndpointKind::Vertex, to};
        return std::ranges::any_of(graph.edges,
                                   [&](const Edge& edge) { return edge.from == a && edge.to == b; });
    };
    for (std::size_t m = 0; m < vertices; ++m) {
        const int member_group = graph.vertices[m].group;
        if (member_group < 0) continue;
        for (std::size_t u = 0; u < vertices; ++u) {
            if (u == m || !edge_between(u, m) || subtree_contains_vertex(member_group, u))
                continue;
            for (std::size_t v = 0; v < vertices; ++v) {
                if (v == m || v == u || !edge_between(m, v) ||
                    subtree_contains_vertex(member_group, v))
                    continue;
                if (edge_between(u, v)) return true;
            }
        }
    }
    return false;
}

std::optional<std::vector<std::string>>
flow_layout(const Diagram& graph, Work& work, FlowTextCollector* collector = nullptr,
            const FlowStyles* source_styles = nullptr,
            std::optional<StyledRows>* styled_output = nullptr,
            const RichLabels* rich = nullptr) {
    if (source_styles && (!styled_output || collector)) return {};
    // Classify once for placement and both routing passes. Packed scalar keys
    // avoid a quadratic scan on the legacy dense-graph budget boundary.
    if (!work.spend(graph.edges.size() * 2u * (sizeof(std::uint64_t) + 4u) + 128u)) return {};
    std::vector<std::uint64_t> pair_keys;
    pair_keys.reserve(graph.edges.size() * 2u);
    const auto endpoint_key = [](const Endpoint& endpoint) {
        return static_cast<std::uint64_t>(endpoint.index) +
               (endpoint.kind == EndpointKind::Group ? 32768u : 0u);
    };
    for (const auto& edge : graph.edges) {
        if (!work.spend(8u)) return {};
        const auto from = endpoint_key(edge.from), to = endpoint_key(edge.to);
        if (from >= 65536u || to >= 65536u) return {};
        pair_keys.push_back((from << 16u) | to);
        if (!edge.arrow_start && !edge.arrow_end)
            pair_keys.push_back((1ull << 32u) | (std::min(from, to) << 16u) | std::max(from, to));
    }

    struct PairWorkExhausted {};

    try {
        std::sort(pair_keys.begin(), pair_keys.end(), [&](auto a, auto b) {
            if (!work.spend()) throw PairWorkExhausted{};
            return a < b;
        });
    } catch (const PairWorkExhausted&) {
        return {};
    }
    bool independent_strands = false;
    for (std::size_t n = 1u; n < pair_keys.size(); ++n) {
        if (!work.spend()) return {};
        if (pair_keys[n] == pair_keys[n - 1u]) {
            independent_strands = true;
            break;
        }
    }
    std::optional<Region> region;
    std::optional<std::vector<Box>> placed;
    if (graph.groups.empty())
        placed = place_nodes(graph, work, independent_strands);
    else {
        region = place_region(graph, -1, graph.direction, work, independent_strands);
        if (!region) return {};
        if (!work.spend(graph.vertices.size() * (sizeof(Box) + 16u))) return {};
        placed.emplace(graph.vertices.size(), Box{0, 0, 0, 0});
        for (const auto& [index, box] : region->nodes) {
            if (index >= placed->size() || box.width <= 0 || box.height <= 0) return {};
            (*placed)[index] = box;
        }
    }
    if (!placed) return {};

    // FS-7b: when no group is collapsed, every structure below is left empty
    // and all consumers use the raw graph unchanged, so the byte-frozen
    // non-collapsed paths take no new branch. When a group is collapsed, build
    // a visibility map and a projected edge list that drops edges internal to
    // a collapsed leaf; hidden vertices/groups are skipped by every consumer.
    const bool any_group_collapsed =
        std::ranges::any_of(graph.groups, [](const Group& group) { return group.collapsed; });
    std::vector<char> hidden_vertex;
    std::vector<Edge> projected_edges;
    std::vector<std::size_t> projected_edge_index;
    bool projected_has_group_endpoint = false;
    if (any_group_collapsed) {
        // Deepest collapsed ancestor of a group index, or -1. A collapsed
        // group resolves to itself.
        const auto collapsed_ancestor = [&](int group) -> int {
            int current = group;
            while (current >= 0) {
                if (static_cast<std::size_t>(current) >= graph.groups.size()) return -1;
                if (graph.groups[static_cast<std::size_t>(current)].collapsed) return current;
                current = graph.groups[static_cast<std::size_t>(current)].parent;
            }
            return -1;
        };
        hidden_vertex.assign(graph.vertices.size(), 0);
        for (std::size_t n = 0; n < graph.vertices.size(); ++n)
            if (graph.vertices[n].group >= 0 &&
                collapsed_ancestor(graph.vertices[n].group) >= 0)
                hidden_vertex[n] = 1;
        // Collapsed ancestor that HIDES this endpoint, or -1 when the endpoint
        // stays visible. A vertex inside a collapsed group is hidden; a group
        // endpoint is hidden only when it is a non-collapsed nested group
        // swallowed by a collapsed ancestor — the collapsed leaf group itself
        // stays visible (it is the rendered node).
        const auto hidden_leaf = [&](const Endpoint& endpoint) -> int {
            if (endpoint.kind == EndpointKind::Vertex) {
                if (endpoint.index >= graph.vertices.size() ||
                    graph.vertices[endpoint.index].group < 0)
                    return -1;
                return collapsed_ancestor(graph.vertices[endpoint.index].group);
            }
            if (endpoint.index >= graph.groups.size()) return -1;
            const auto& group = graph.groups[endpoint.index];
            if (group.collapsed) return -1;  // the collapsed leaf is visible
            return collapsed_ancestor(static_cast<int>(endpoint.index));
        };
        // Project a hidden endpoint onto its collapsed ancestor as a Group
        // endpoint; a visible endpoint passes through unchanged (leaf < 0).
        const auto project_endpoint = [&](const Endpoint& endpoint, int leaf) -> Endpoint {
            if (leaf < 0) return endpoint;
            return Endpoint{EndpointKind::Group, static_cast<std::size_t>(leaf)};
        };
        projected_edges.reserve(graph.edges.size());
        projected_edge_index.reserve(graph.edges.size());
        for (std::size_t index = 0; index < graph.edges.size(); ++index) {
            const auto& edge = graph.edges[index];
            const int from_leaf = hidden_leaf(edge.from);
            const int to_leaf = hidden_leaf(edge.to);
            const bool from_hidden = from_leaf >= 0;
            const bool to_hidden = to_leaf >= 0;
            if (from_hidden && to_hidden) {
                // Same collapsed leaf: a genuine internal edge, drop it.
                // Different leaves: remapping would join two collapsed nodes;
                // that conservative case stays fail-closed (no Slice C fixture
                // needs it) rather than risking a fan-in port conflict.
                if (from_leaf != to_leaf) return {};
                continue;
            }
            // FS-7b Slice C: a single hidden endpoint (a vertex inside, or a
            // nested group swallowed by, a collapsed ancestor) is remapped onto
            // the collapsed group's own endpoint. The synthetic Edge copies
            // every other field so labels, arrows and stroke survive; only the
            // hidden endpoint's kind/index change.
            Edge projected = edge;
            projected.from = project_endpoint(edge.from, from_leaf);
            projected.to = project_endpoint(edge.to, to_leaf);
            // A remap that collapses both endpoints onto the same node (an
            // edge from the collapsed group into one of its own hidden
            // vertices) is an internal/self connection with no external
            // meaning: drop it rather than paint an unreviewed self-loop.
            // Guard on an actual remap so an authored self-loop on a fully
            // visible vertex (an existing supported feature) is never dropped.
            if ((from_hidden || to_hidden) && projected.from == projected.to) continue;
            if (projected.from.kind == EndpointKind::Group ||
                projected.to.kind == EndpointKind::Group)
                projected_has_group_endpoint = true;
            projected_edges.push_back(std::move(projected));
            projected_edge_index.push_back(index);
        }
    }
    const bool route_has_group_endpoint =
        any_group_collapsed ? projected_has_group_endpoint : graph.has_group_endpoint;
    const std::vector<Edge>& route_edges =
        any_group_collapsed ? projected_edges : graph.edges;

    const bool has_group_endpoint = route_has_group_endpoint;

    struct EndpointBox {
        Box box;
        Shape shape = Shape::Rectangle;
    };

    struct RoutedEdge {
        std::size_t edge = 0u;       // original graph.edges index (rich-label binding)
        Edge routed;                 // edge actually routed (synthetic after FS-7b remap)
        EndpointBox from, to;
        int distance = 0;
    };

    std::vector<std::optional<Box>> group_boxes;
    std::vector<RoutedEdge> tagged_routing_order;
    if (has_group_endpoint) {
        // Route from the real box belonging to each authored endpoint. A group
        // is its painted frame, never a representative child vertex. These
        // tables and the stable routing order are prepaid before allocation.
        // FS-7c: RoutedEdge now carries a synthetic Edge (so remapped Group
        // kinds reach route_one); this larger per-edge charge applies to every
        // group-endpoint graph, so the GroupEndpoint15/FS-7a exact-budget gates
        // are re-verified in both presets as part of each collapse cohort.
        const auto route_table_charge =
            graph.groups.size() * (sizeof(std::optional<Box>) + sizeof(Box) + 16u) +
            route_edges.size() * (2u * sizeof(RoutedEdge) + sizeof(std::size_t) + 48u);
        if (!work.spend(route_table_charge)) return {};
        group_boxes.assign(graph.groups.size(), std::nullopt);
        if (region)
            for (const auto& [index, box] : region->groups) {
                if (index >= group_boxes.size() || group_boxes[index] || box.width <= 0 ||
                    box.height <= 0)
                    return {};
                group_boxes[index] = box;
            }
        // Without collapse every group must have a box. With collapse, a group
        // hidden inside a collapsed ancestor has no Region box and no kept edge
        // references it, so only drawable groups are required.
        if (!any_group_collapsed)
            for (const auto& group : group_boxes)
                if (!group) return {};
        const auto box_for = [&](const Endpoint& endpoint) -> std::optional<EndpointBox> {
            if (endpoint.kind == EndpointKind::Vertex) {
                if (endpoint.index >= placed->size()) return {};
                if (any_group_collapsed && hidden_vertex[endpoint.index]) return {};
                const auto box = (*placed)[endpoint.index];
                if (box.width <= 0 || box.height <= 0) return {};
                return EndpointBox{box, graph.vertices[endpoint.index].shape};
            }
            if (endpoint.index >= group_boxes.size() || !group_boxes[endpoint.index]) return {};
            return EndpointBox{*group_boxes[endpoint.index], Shape::Rectangle};
        };
        tagged_routing_order.reserve(route_edges.size());
        for (std::size_t position = 0; position < route_edges.size(); ++position) {
            const auto original_index =
                any_group_collapsed ? projected_edge_index[position] : position;
            const auto from = box_for(route_edges[position].from);
            const auto to = box_for(route_edges[position].to);
            if (!from || !to) return {};
            tagged_routing_order.push_back(
                {original_index, route_edges[position], *from, *to,
                 distance({from->box.x + from->box.width / 2, from->box.y + from->box.height / 2},
                          {to->box.x + to->box.width / 2, to->box.y + to->box.height / 2})});
        }
    }

    int width = 0, height = 0, longest = 0;
    for (std::size_t slot = 0; slot < placed->size(); ++slot) {
        // FS-7b: a vertex hidden inside a collapsed group keeps a zero box and
        // contributes no canvas extent or validation.
        if (any_group_collapsed && hidden_vertex[slot]) continue;
        const auto& box = (*placed)[slot];
        if (box.width <= 0 || box.height <= 0) return {};
        width = std::max(width, box.x + box.width);
        height = std::max(height, box.y + box.height);
    }
    if (region) {
        width = std::max(width, region->width);
        height = std::max(height, region->height);
    }
    for (const auto& edge : route_edges)
        if (edge.text) longest = std::max(longest, label_width(*edge.text));
    if (longest + 4 > kWidthLimit) return {};
    width =
        std::min(kWidthLimit, width + 12 + 2 * longest + static_cast<int>(graph.edges.size()) * 2);
    height = std::min(kHeightLimit, height + 12 + static_cast<int>(graph.edges.size()) * 3);
    if (!work.spend(static_cast<std::size_t>(width * height) * 4)) return {};
    std::size_t literal_bytes = 0u;
    if (collector) {
        for (const auto& vertex : graph.vertices)
            literal_bytes += vertex.text.size();
        for (const auto& edge : graph.edges)
            if (edge.text) literal_bytes += edge.text->size();
        if (!collector->prepare(width, height,
                                (graph.vertices.size() + graph.edges.size()) *
                                    static_cast<std::size_t>(kLabelLimit),
                                literal_bytes))
            return {};
    }
    const auto canvas_area = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    // One whole-plan paint-and-route attempt over a fresh canvas. Pass 1
    // (strict) reproduces the historical routing byte-for-byte: connector
    // cells are sealed once laid, so edges never cross. Pass 2 (relaxed) runs
    // only after a full strict failure; it may cross earlier connector
    // interiors while node/label cells, frame corners and port heads stay
    // sealed. Both passes are deterministic and metered by the same Work.
    const auto attempt = [&](bool strict) -> std::optional<std::vector<std::string>> {
        std::vector<unsigned char> routed;
        FlowTextCollector* pass_collector = collector;
        if (!strict) {
            // The discarded strict pass may have recorded label owners. Re-arm the
            // collector with its identical bounds before repainting.
            if (collector && !collector->restart(width, height,
                                                 (graph.vertices.size() + graph.edges.size()) *
                                                     static_cast<std::size_t>(kLabelLimit),
                                                 literal_bytes)) {
                return {};
            }
            // Rebuild fee for the second workspace: the crossing byte grid plus a
            // fixed slack. The fresh canvas and the Router's BFS scratch are
            // allocated once per pass; the multi-source search charges its own
            // shared workspace fee on the first edge (instead of per edge as in
            // the strict pass). A fresh Work per layout means a failed strict pass
            // already spent its budget; an unaffordable retry is the same
            // whole-source result as today.
            if (canvas_area > std::numeric_limits<std::size_t>::max() - 4096u) return {};
            if (!work.spend(canvas_area + 4096u)) return {};
            routed.assign(canvas_area, 0u);
        }
        Canvas canvas(width, height, work);
        const auto runs_nonempty =
            [](const std::vector<std::vector<detail::LabelRun>>& table) {
                return std::ranges::any_of(table,
                                           [](const auto& runs) { return !runs.empty(); });
            };
        const bool rich_present =
            rich && (runs_nonempty(rich->vertex) || runs_nonempty(rich->edge) ||
                     runs_nonempty(rich->group));
        if (rich_present && !canvas.ensure_styles()) return {};
        if (region)
            for (const auto& [index, box] : region->groups) {
                if (index >= graph.groups.size()) return {};
                const std::vector<detail::LabelRun>* group_runs = nullptr;
                if (rich && index < rich->group.size() && !rich->group[index].empty())
                    group_runs = &rich->group[index];
                if (graph.groups[index].collapsed) {
                    // FS-7b: a collapsed group is a single titled rectangle,
                    // not a frame. Reuse node_box with a synthetic vertex and
                    // the group's rich runs so styled titles survive.
                    const Vertex collapsed_node{graph.groups[index].id,
                                                graph.groups[index].text, Shape::Rectangle,
                                                false, false, -1, 0};
                    if (!node_box(canvas, box, collapsed_node, pass_collector, group_runs))
                        return {};
                } else if (!group_frame(canvas, box, graph.groups[index].text, group_runs)) {
                    return {};
                }
            }
        for (std::size_t n = 0; n < placed->size(); ++n) {
            if (any_group_collapsed && hidden_vertex[n]) continue;  // FS-7b
            const std::vector<detail::LabelRun>* runs = nullptr;
            if (rich && n < rich->vertex.size() && !rich->vertex[n].empty())
                runs = &rich->vertex[n];
            if (!node_box(canvas, (*placed)[n], graph.vertices[n], pass_collector, runs))
                return {};
        }

        Router router(canvas, strict ? nullptr : &routed);
        std::unordered_set<int> used_ports;
        const auto route_one = [&](std::size_t edge_index, const Edge& edge,
                                   const EndpointBox& source_box,
                                   const EndpointBox& target_box) {
            const auto& from = source_box.box;
            const auto& to = target_box.box;
            auto sources =
                ports(from, source_box.shape, {to.x + to.width / 2, to.y + to.height / 2});
            auto targets =
                ports(to, target_box.shape, {from.x + from.width / 2, from.y + from.height / 2});
            if (!strict &&
                (edge.from.kind == EndpointKind::Group || edge.to.kind == EndpointKind::Group)) {
                // An existing route crossing cannot simultaneously own a new
                // endpoint port. Ordinary node borders are already sealed.
                if (!canvas.work.spend(sources.size() + targets.size() + 64u)) return false;
                const auto crossed = [&](const Port& port) {
                    return routed[static_cast<std::size_t>(canvas.index(port.border))] != 0u ||
                           canvas.at(port.border).bits == (kUp | kRight | kDown | kLeft);
                };
                const bool source_crossed =
                    edge.from.kind == EndpointKind::Group && std::ranges::any_of(sources, crossed);
                const bool target_crossed =
                    edge.to.kind == EndpointKind::Group && std::ranges::any_of(targets, crossed);
                if (source_crossed || target_crossed) {
                    if (!canvas.work.spend((sources.size() + targets.size()) *
                                           (2u * sizeof(Port) + 16u)))
                        return false;
                    if (source_crossed) std::erase_if(sources, crossed);
                    if (target_crossed) std::erase_if(targets, crossed);
                }
            }
            bool group_source_targets_descendant = false;
            if (edge.from.kind == EndpointKind::Group && edge.to.kind == EndpointKind::Vertex) {
                if (edge.from.index >= graph.groups.size() ||
                    edge.to.index >= graph.vertices.size())
                    return false;
                int owner = graph.vertices[edge.to.index].group;
                for (std::size_t depth = 0; owner >= 0 && depth < kGroupDepthLimit; ++depth) {
                    if (!work.spend() || static_cast<std::size_t>(owner) >= graph.groups.size())
                        return false;
                    if (static_cast<std::size_t>(owner) == edge.from.index) {
                        group_source_targets_descendant = true;
                        break;
                    }
                    owner = graph.groups[static_cast<std::size_t>(owner)].parent;
                }
                if (owner < -1 || (owner >= 0 && !group_source_targets_descendant)) return false;
            }
            const bool horizontal =
                graph.direction == Direction::Right || graph.direction == Direction::Left;
            const bool reverse =
                graph.direction == Direction::Left || graph.direction == Direction::Up;
            const auto advance = horizontal ? to.x - from.x : to.y - from.y;
            if ((reverse ? -advance : advance) <= 0) {
                // Prefer outside-facing ports for back edges and self loops, reducing
                // paths that enclose their own node or intervening nodes along the spine.
                const auto outside_first = [&](const Port& a, const Port& b) {
                    return (horizontal ? a.normal.y : a.normal.x) >
                           (horizontal ? b.normal.y : b.normal.x);
                };
                std::stable_sort(sources.begin(), sources.end(), outside_first);
                std::stable_sort(targets.begin(), targets.end(), outside_first);
            }
            if (group_source_targets_descendant)
                for (auto& source : sources) {
                    source.normal = {-source.normal.x, -source.normal.y};
                    source.outside = source.border + source.normal;
                }
            if (!edge.text && edge.from != edge.to && !edge.arrow_start && edge.arrow_end) {
                auto connection = router.connection(sources, targets, used_ports);
                if (!connection) return false;
                const auto& source = sources[connection->source];
                const auto& target = targets[connection->target];
                const bool group_endpoint =
                    edge.from.kind == EndpointKind::Group || edge.to.kind == EndpointKind::Group;
                bool repeats_port = false;
                if (group_endpoint) {
                    if (!canvas.work.spend(connection->path.size() * 2u + 32u)) return false;
                    repeats_port = std::ranges::any_of(connection->path, [&](Point point) {
                        return point == source.border || point == target.border;
                    });
                }
                if (!repeats_port) {
                    if (!draw_path(canvas, connection->path, source.border, target.border, false,
                                   true, edge.stroke, {}, {}, strict ? nullptr : &routed,
                                   independent_strands) ||
                        !source_port(canvas, source))
                        return false;
                    if (edge.from.kind == EndpointKind::Group)
                        seal_group_port(canvas, source.border, strict ? nullptr : &routed);
                    if (edge.to.kind == EndpointKind::Group)
                        seal_group_port(canvas, target.border, strict ? nullptr : &routed);
                    used_ports.insert(canvas.index(source.border));
                    used_ports.insert(canvas.index(target.border));
                    return true;
                }
                // Multi-source state is shared by cell, so an origin-specific port
                // exclusion cannot safely be added to that search. Retry its bad
                // group candidate through the existing single-pair search instead.
                if (!group_endpoint) return false;
            }
            for (const auto& source : sources) {
                if (used_ports.contains(canvas.index(source.border))) continue;
                for (const auto& target : targets) {
                    if (!work.remaining) return false;
                    if (used_ports.contains(canvas.index(target.border))) continue;
                    const std::vector<detail::LabelRun>* edge_rich = nullptr;
                    if (rich && edge_index < rich->edge.size() &&
                        !rich->edge[edge_index].empty())
                        edge_rich = &rich->edge[edge_index];
                    if (!connect(canvas, router, source, target, edge.text, edge.arrow_start,
                                 edge.arrow_end, edge.stroke, pass_collector,
                                 strict ? nullptr : &routed, edge.from.kind == EndpointKind::Group,
                                 edge.to.kind == EndpointKind::Group, independent_strands,
                                 edge_rich))
                        continue;
                    used_ports.insert(canvas.index(source.border));
                    used_ports.insert(canvas.index(target.border));
                    return true;
                }
            }
            return false;
        };
        if (has_group_endpoint) {
            // The source order is retained through `edge`; only route order is
            // stably distance-sorted, exactly as for the legacy branch below.
            std::stable_sort(
                tagged_routing_order.begin(), tagged_routing_order.end(),
                [](const RoutedEdge& a, const RoutedEdge& b) { return a.distance < b.distance; });
            for (const auto& route : tagged_routing_order)
                // FS-7b: route the (possibly remapped) synthetic Edge so the
                // Group-kind sealing in route_one sees the projected endpoint;
                // route.edge still carries the original index for rich-label
                // binding and disclosure (design section 3.3.4).
                if (!route_one(route.edge, route.routed, route.from, route.to)) {
                    return std::optional<std::vector<std::string>>{};
                }
        } else {
            // The private parser only creates vertex endpoints on this branch, but
            // validate that invariant before the unchanged index-based route sort.
            if (!work.spend(route_edges.size() * (2u * sizeof(Endpoint) + 16u))) return {};
            for (const auto& edge : route_edges)
                if (edge.from.kind != EndpointKind::Vertex ||
                    edge.to.kind != EndpointKind::Vertex || edge.from.index >= placed->size() ||
                    edge.to.index >= placed->size())
                    return {};
            std::vector<std::size_t> routing_order(route_edges.size());
            std::iota(routing_order.begin(), routing_order.end(), 0);
            const auto edge_distance = [&](std::size_t position) {
                const auto& edge = route_edges[position];
                const auto& a = (*placed)[edge.from.index];
                const auto& b = (*placed)[edge.to.index];
                return distance({a.x + a.width / 2, a.y + a.height / 2},
                                {b.x + b.width / 2, b.y + b.height / 2});
            };
            std::stable_sort(
                routing_order.begin(), routing_order.end(),
                [&](std::size_t a, std::size_t b) { return edge_distance(a) < edge_distance(b); });
            for (const auto position : routing_order) {
                const auto original_index =
                    any_group_collapsed ? projected_edge_index[position] : position;
                const auto& edge = route_edges[position];
                if (!route_one(original_index, edge,
                               {(*placed)[edge.from.index], graph.vertices[edge.from.index].shape},
                               {(*placed)[edge.to.index], graph.vertices[edge.to.index].shape}))
                    return std::optional<std::vector<std::string>>{};
            }
        }

        if (source_styles && !paint_flow_styles(canvas, graph, *source_styles, *placed, work))
            return {};
        auto lines = pass_collector ? pass_collector->finish(canvas)
                                    : std::optional<std::vector<std::string>>{canvas.lines()};
        if (!lines || lines->empty()) return {};
        const auto art_rows = lines->size();
        if (source_styles) {
            if (art_rows + source_styles->legend.size() >
                    static_cast<std::size_t>(kHeightLimit) ||
                !work.spend((art_rows + source_styles->legend.size()) * 4u *
                                sizeof(std::string) +
                            4u * sizeof(std::vector<std::string>) + 256u))
                return {};
            lines->reserve(art_rows + source_styles->legend.size());
            for (const auto& record : source_styles->legend) {
                if (!work.spend(record.size() * 8u + 256u)) return {};
                lines->push_back(record);
            }
        }
        // FS-1: disclosure footer for accepted non-terminal body init
        // directives, appended BEFORE style extraction so the styled-row plane
        // covers the footer as plain runs (drawing_rows stays the canvas art).
        // The collector (ASCII) path carries no notices.
        static const std::vector<std::string> kNoNotices;
        const std::vector<std::string>& notices = rich ? rich->notices : kNoNotices;
        if (!pass_collector && !notices.empty()) {
            constexpr std::size_t kFlowNoticeLineLimit = 96u;
            std::size_t bytes = 0;
            for (const auto& notice : notices) {
                if (notice.size() > kFlowNoticeLineLimit) return {};
                bytes += notice.size();
            }
            if (lines->size() + 1u + notices.size() >
                    static_cast<std::size_t>(kHeightLimit) ||
                !work.spend((1u + notices.size()) *
                                    (4u * sizeof(std::string) + 256u) +
                                bytes * 2u + 256u))
                return {};
            lines->emplace_back();
            for (const auto& notice : notices)
                lines->push_back(notice);
        }
        // Style extraction spans only the canvas art rows (drawing_rows);
        // legend/footer rows map to plain runs.
        if (source_styles || (rich_present && styled_output)) {
            auto styled = extract_styled_rows(canvas, *lines, 0u, art_rows, work);
            if (!styled) return {};
            *styled_output = std::move(*styled);
        }
        return lines;
    };

    // Pass 1: the exact strict plan. Every input that routes today keeps this
    // output; the relaxed retry exists only for full-plan failure, and its
    // result is consumed only when pass 1 failed.
    if (auto lines = attempt(true)) return lines;
    // Pass 2: one deterministic bounded whole-plan retry permitting crossings.
    return attempt(false);
}

bool ascii_flow_header(std::string_view source) {
    const auto end = source.find('\n');
    const auto header = trim(source.substr(0, end));
    const auto valid = [](std::string_view line, std::string_view name) {
        if (!line.starts_with(name) || line.size() == name.size()) return false;
        const auto separator = line[name.size()];
        if (separator != ' ' && separator != '\t') return false;
        line.remove_prefix(name.size());
        line = trim(line);
        return line == "LR" || line == "TD" || line == "BT";
    };
    return valid(header, "graph") || valid(header, "flowchart");
}

bool ascii_flow_profile(const Diagram& graph, Work& work) {
    if (!graph.groups.empty()) return false;
    // A complete three-node DAG is a chain plus one skip edge. Inspect its
    // topology independently of vertex/edge declaration order; projection uses
    // the same already-bounded native layout and literal ownership as LR.
    if ((graph.direction == Direction::Down || graph.direction == Direction::Up) &&
        graph.vertices.size() == 3u && graph.edges.size() == 3u) {
        // Prepay the fixed three-vertex/three-edge topology check. The existing
        // LR profile keeps its original budget accounting exactly.
        if (!work.spend(512u)) return false;
        std::array<bool, 9> seen{};
        std::array<int, 3> incoming{};
        for (const auto& vertex : graph.vertices)
            if (vertex.shape != Shape::Rectangle || vertex.text.empty() ||
                vertex.text.find('\n') != std::string::npos)
                return false;
        for (const auto& edge : graph.edges) {
            if (edge.from.kind != EndpointKind::Vertex || edge.to.kind != EndpointKind::Vertex ||
                edge.from.index >= 3u || edge.to.index >= 3u || edge.from == edge.to ||
                edge.text.has_value() || edge.stroke != Stroke::Solid || edge.arrow_start ||
                !edge.arrow_end || edge.minimum_length != 1)
                return false;
            const auto pair = edge.from.index * 3u + edge.to.index;
            // Reject either orientation of a repeated unordered pair.
            if (seen[pair] || seen[edge.to.index * 3u + edge.from.index]) return false;
            seen[pair] = true;
            ++incoming[edge.to.index];
        }
        std::ranges::sort(incoming);
        return incoming == std::array<int, 3>{{0, 1, 2}};
    }
    if (graph.direction != Direction::Right || graph.vertices.size() != 2u ||
        graph.edges.size() != 1u)
        return false;
    for (const auto& vertex : graph.vertices)
        if (vertex.shape != Shape::Rectangle || !vertex.explicit_label || vertex.text.empty())
            return false;
    const auto& edge = graph.edges.front();
    return edge.from.kind == EndpointKind::Vertex && edge.from.index == 0u &&
           edge.to.kind == EndpointKind::Vertex && edge.to.index == 1u && edge.text.has_value() &&
           !edge.text->empty() && edge.stroke == Stroke::Solid && !edge.arrow_start &&
           edge.arrow_end && edge.minimum_length == 1;
}

std::optional<detail::FlowAsciiRows> ascii_flow_rows(std::string_view source, Work& work) {
    if (!ascii_flow_header(source)) return {};
    const auto graph = parse(source, work);
    if (!graph || !ascii_flow_profile(*graph, work)) return {};
    FlowTextCollector collector{work};
    auto native = flow_layout(*graph, work, &collector);
    auto ascii = collector.take_ascii_rows();
    if (!native || !ascii || native->empty() || ascii->empty() || native->size() != ascii->size())
        return {};
    return detail::FlowAsciiRows{std::move(*native), std::move(*ascii)};
}

// A routing-only probe. The complete untouched source still goes to its owner.
// It never grants config syntax to another family or parses config values.
enum class PrefixOwner : unsigned char { Other, Sequence, Class };

bool class_prefix_header(std::string_view line) {
    return line == "classDiagram" || line == "classDiagram-v2";
}

bool requirement_prefix_header(std::string_view line) {
    // The Requirement lexer is case-insensitive; compare two folded spellings,
    // not a folded input against the mixed-case source keyword.
    constexpr std::string_view header = "requirementdiagram";
    if (line.size() != header.size()) return false;
    for (std::size_t at = 0; at < header.size(); ++at) {
        const auto byte = line[at];
        const auto folded = byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a') : byte;
        if (folded != header[at]) return false;
    }
    return true;
}

std::optional<PrefixOwner> sequence_prefix_route(std::string_view source, Work& work) {
    // RFC 0031 E5: this linear source walk is dominated by the single
    // classify_mermaid probe fee.
    (void)work;
    bool frontmatter = false, seen_frontmatter = false, directive = false;
    while (!source.empty()) {
        const auto end = source.find('\n');
        auto line = source.substr(0, end);
        source = end == std::string_view::npos ? std::string_view{} : source.substr(end + 1);
        if (line.ends_with('\r')) line.remove_suffix(1);
        line = trim(line);
        if (directive) {
            // A close inside quoted data with a nonblank tail is not a prefix
            // boundary. Complete string/object validation belongs to the owner.
            if (line.ends_with("}%%")) directive = false;
            continue;
        }
        if (frontmatter) {
            if (line == "---") frontmatter = false;
            continue;
        }
        if (line.empty()) continue;
        if (line == "---" && !seen_frontmatter) {
            frontmatter = true;
            seen_frontmatter = true;
            continue;
        }
        if (line.starts_with("%%{")) {
            directive = !line.ends_with("}%%");
            continue;
        }
        if (line.starts_with("%%")) continue;
        while (line.starts_with(';'))
            line = trim(line.substr(1));
        if (line.starts_with("sequenceDiagram")) return PrefixOwner::Sequence;
        if (line.starts_with("classDiagram")) {
            return class_prefix_header(line) ? PrefixOwner::Class : PrefixOwner::Other;
        }
        return PrefixOwner::Other;
    }
    return PrefixOwner::Other;
}

class MermaidNode final : public ftxui::Node {
    int diagram_width_;
    bool diagram_ = true;
    bool changed_ = false;

public:
    MermaidNode(ftxui::Element art, ftxui::Element fallback, int width)
        : Node({std::move(art), std::move(fallback)})
        , diagram_width_(width) {}

    void ComputeRequirement() override {
        for (const auto& child : children_)
            child->ComputeRequirement();
        requirement_ = children_[diagram_ ? 0 : 1]->requirement();
        requirement_.flex_shrink_x = 1;
    }

    void SetBox(ftxui::Box box) override {
        box_ = box;
        const auto desired = box.x_max - box.x_min + 1 >= diagram_width_;
        if (desired != diagram_) {
            diagram_ = desired;
            changed_ = true;
        }
        children_[diagram_ ? 0 : 1]->SetBox(box);
    }

    void Check(Status* status) override {
        children_[diagram_ ? 0 : 1]->Check(status);
        status->need_iteration |= changed_ || status->iteration == 0;
        changed_ = false;
    }

    void Render(ftxui::Screen& screen) override { children_[diagram_ ? 0 : 1]->Render(screen); }
};

std::optional<ftxui::Element> finish_legacy_rows(std::vector<std::string> lines,
                                                 ftxui::Element fallback) {
    if (lines.empty()) return {};
    int width = 0;
    ftxui::Elements rows;
    for (auto& line : lines) {
        width = std::max(width, ftxui::string_width(line));
        rows.push_back(ftxui::text(std::move(line)));
    }
    if (width > kWidthLimit || rows.size() > kHeightLimit) return {};
    return std::make_shared<MermaidNode>(ftxui::vbox(std::move(rows)), std::move(fallback), width);
}

}  // namespace

namespace {
ftxui::Color style_role_color(SemanticColorRole role, bool background,
                              const loom::ui::design::tokens::Palette& palette) {
    switch (role) {
    case SemanticColorRole::Red:
        return background ? palette.danger_tint_bg : palette.danger;
    case SemanticColorRole::Orange:
    case SemanticColorRole::Yellow:
        return background ? palette.warning_tint_bg : palette.warning;
    case SemanticColorRole::Green:
        return background ? palette.success_tint_bg : palette.success;
    case SemanticColorRole::Cyan:
    case SemanticColorRole::Blue:
        return background ? palette.info_tint_bg : palette.info;
    case SemanticColorRole::Purple:
        return background ? palette.surface_tint_4 : palette.accent_purple;
    default:
        return background ? palette.surface_tint_2 : palette.text;
    }
}
}  // namespace

bool detail::set_flow_handdrawn_preview_for_testing(bool enabled) {
    const bool previous = flow_handdrawn_preview;
    flow_handdrawn_preview = enabled;
    return previous;
}

bool detail::flow_handdrawn_preview_active() {
    return flow_handdrawn_preview;
}

std::optional<ftxui::Element> detail::finish_native_layout(std::optional<NativeLayout> layout,
                                                           ftxui::Element fallback, Work& work,
                                                           FinalEmissionTrace* trace) {
    if (!layout) return {};
    if (!layout->packed_styles.empty()) {
        if (layout->styles) return {};
        return finish_mermaid_payload(
            LayoutPayload{std::move(layout->rows), std::move(layout->packed_styles)},
            std::move(fallback), work, trace, layout->packed_meter,
            layout->packed_meter == PackedMeter::Er ? &layout->payload_prepaid : nullptr,
            layout->packed_meter == PackedMeter::Block ? &layout->payload_work : nullptr);
    }
    if (!layout->styles) return finish_legacy_rows(std::move(layout->rows), std::move(fallback));
    if (trace) {
        trace->styled_entered = true;
        trace->entry_remaining = work.remaining;
    }
    const auto emission_spend = [&](std::size_t amount) {
        if (trace) {
            trace->last_debit = amount;
            ++trace->preflight_debits;
        }
        const bool paid = work.spend(amount);
        if (trace) {
            if (paid) {
                trace->emission_debit += amount;
                ++trace->successful_debits;
            } else
                trace->failed_debit = amount;
        }
        return paid;
    };
    const auto& styled = layout->styles->rows;
    if (styled.size() != layout->rows.size() || styled.empty() ||
        styled.size() > static_cast<std::size_t>(kHeightLimit) ||
        !emission_spend(styled.size() * (8u * sizeof(ftxui::Element) + 64u) + 512u))
        return {};
    for (const auto& row : styled) {
        if (row.size() > static_cast<std::size_t>(kWidthLimit) + 1u ||
            !emission_spend(row.size() * 32u + 128u))
            return {};
        for (const auto& run : row)
            if (!emission_spend(run.text.size() * 8u + 8u * sizeof(StyledRun) +
                                8u * sizeof(ftxui::Element) + 256u))
                return {};
    }
    if (trace) trace->preflight_complete = true;
    const auto theme = loom::ui::design::theme::current_theme();
    const auto& palette = *theme.palette;
    int diagram_width = 0;
    ftxui::Elements rows;
    if (trace) trace->allocation_started = true;
    rows.reserve(styled.size());
    for (auto& row : layout->styles->rows) {
        ftxui::Elements elements;
        elements.reserve(row.size());
        int width = 0;
        for (auto& run : row) {
            width += ftxui::string_width(run.text);
            auto element = ftxui::text(std::move(run.text));
            if (run.style.foreground != SemanticColorRole::Inherit)
                element =
                    element | ftxui::color(style_role_color(run.style.foreground, false, palette));
            if (run.style.fill != SemanticColorRole::Inherit)
                element = element | ftxui::bgcolor(style_role_color(run.style.fill, true, palette));
            if (run.style.bold) element = element | ftxui::bold;
            if (run.style.underline) element = element | ftxui::underlined;
            elements.push_back(std::move(element));
        }
        if (width > kWidthLimit) return {};
        diagram_width = std::max(diagram_width, width);
        rows.push_back(ftxui::hbox(std::move(elements)) |
                       ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width));
    }
    auto result = std::make_shared<MermaidNode>(ftxui::vbox(std::move(rows)), std::move(fallback),
                                                diagram_width);
    if (trace) trace->returned_native = true;
    return result;
}

std::optional<std::array<bool, 5>> testing::flow_group_border_probe(bool blocked, bool crossable,
                                                                    testing::Work& work) {
    // Fixed storage, queue/path growth and scalar metadata are bounded before
    // construction. Real router steps continue spending the same caller Work.
    if (!work.spend(16384u)) return {};
    Canvas canvas(7, 3, work);
    std::vector<unsigned char> routed(21u, 0u);
    for (auto& cell : canvas.cells)
        cell.blocked = true;
    for (int x = 1; x <= 5; ++x)
        canvas.at({x, 1}).blocked = false;
    const Point middle{3, 1};
    canvas.at(middle).blocked = blocked;
    routed[static_cast<std::size_t>(canvas.index(middle))] = crossable ? 1u : 0u;
    Router strict(canvas);
    const bool strict_before = strict.path({1, 1}, {5, 1}).has_value();
    if (!work.remaining) return {};
    Router relaxed(canvas, &routed);
    const bool before = relaxed.path({1, 1}, {5, 1}).has_value();
    if (!work.remaining) return {};
    bool reserved = false;
    {
        ScopedGroupBorder guard(canvas, middle, true, &routed);
        reserved = relaxed.path({1, 1}, {5, 1}).has_value();
        if (!work.remaining) return {};
    }
    const bool after = relaxed.path({1, 1}, {5, 1}).has_value();
    if (!work.remaining) return {};
    bool restored =
        canvas.at(middle).blocked == blocked &&
        routed[static_cast<std::size_t>(canvas.index(middle))] == (crossable ? 1u : 0u) &&
        after == before;
    if (!work.spend(1024u)) return {};
    const std::vector<Point> path{middle};
    bool path_exhausted = false;
    {
        ScopedPathReservation guard(canvas, path, &routed);
        if (!guard.ready) return {};
        reserved = reserved || relaxed.path({1, 1}, {5, 1}).has_value();
        path_exhausted = !work.remaining;
    }
    // The initial fixed reserve pays this check even after search exhaustion.
    if (canvas.at(middle).blocked != blocked ||
        routed[static_cast<std::size_t>(canvas.index(middle))] != (crossable ? 1u : 0u))
        throw std::logic_error("Flow path reservation restoration");
    if (path_exhausted) return {};
    if (!work.spend(128u)) return {};

    struct ProbeUnwind {};

    try {
        ScopedPathReservation guard(canvas, path, &routed);
        if (!guard.ready) return {};
        throw ProbeUnwind{};
    } catch (const ProbeUnwind&) {
    }
    if (canvas.at(middle).blocked != blocked ||
        routed[static_cast<std::size_t>(canvas.index(middle))] != (crossable ? 1u : 0u))
        throw std::logic_error("Flow path reservation unwind restoration");
    const bool path_after = relaxed.path({1, 1}, {5, 1}).has_value();
    if (!work.remaining) return {};
    restored = restored && canvas.at(middle).blocked == blocked &&
               routed[static_cast<std::size_t>(canvas.index(middle))] == (crossable ? 1u : 0u) &&
               path_after == before;
    if (!blocked) {
        {
            ScopedPathReservation guard(canvas, path, nullptr);
            if (!guard.ready) return {};
            reserved = reserved || strict.path({1, 1}, {5, 1}).has_value();
            if (!work.remaining) return {};
        }
        const bool strict_after = strict.path({1, 1}, {5, 1}).has_value();
        if (!work.remaining) return {};
        restored =
            restored && !canvas.at(middle).blocked &&
            routed[static_cast<std::size_t>(canvas.index(middle))] == (crossable ? 1u : 0u) &&
            strict_after == strict_before;
    }
    seal_group_port(canvas, middle, &routed);
    const bool sealed = relaxed.path({1, 1}, {5, 1}).has_value();
    if (!work.remaining) return {};
    return std::array<bool, 5>{strict_before, before, reserved, restored, sealed};
}

std::optional<detail::FlowAsciiRows> detail::flow_ascii_rows(std::string_view source,
                                                             detail::Work& work) {
    return ascii_flow_rows(source, work);
}

std::optional<detail::MermaidProbeDecision> detail::classify_mermaid(std::string_view source,
                                                                     Work& work) {
    using detail::MermaidProbeDecision;
    using detail::MermaidProbeFamily;

    // RFC 0031 E5: the probe-table boundary fee replaces the former flag
    // bookkeeping. The fixed term equals the historic fixed flag fees. A
    // frontmatter peek adds 1/peek-byte (the legacy prefix_work accumulation)
    // plus a small constant dominating the deleted body-line charges.
    constexpr std::size_t kProbePerPeekByte = 1u;
    constexpr std::size_t kProbePeekFixed = 600u;
    constexpr std::size_t kProbeFixed = 192u;
    std::size_t peek_bytes = 0;
    bool has_frontmatter_peek = false;

    // ---- textual scan (byte-identical to the legacy header/peek loops) ----
    auto remaining = source;
    std::string_view header;
    bool multiline_directive = false;
    bool class_envelope_seen = false, class_peek = false;
    bool flow = false, requirement = false;
    while (!remaining.empty()) {
        const auto end = remaining.find('\n');
        header = remaining.substr(0, end);
        if (header.ends_with('\r')) header.remove_suffix(1);
        header = trim(header);
        while (header.starts_with(';'))
            header = trim(header.substr(1));
        if (header.starts_with("%%{") && !header.ends_with("}%%")) multiline_directive = true;
        if (header.starts_with("%%{") && header.ends_with("}%%")) class_envelope_seen = true;
        if (!header.empty() && !header.starts_with("%%")) break;
        if (end == std::string_view::npos) break;
        remaining.remove_prefix(end + 1);
    }
    bool sequence = false, er = false, gitgraph = false, state = false;
    bool mindmap = header == "mindmap";
    bool block_input = header == "block" || header == "block-beta";
    if (header == "---") {
        has_frontmatter_peek = true;
        auto peek = remaining;
        bool opening = true;
        bool closed = false;
        while (!peek.empty()) {
            const auto end = peek.find('\n');
            auto line = peek.substr(0, end);
            peek_bytes += line.size() + 1;
            peek = end == std::string_view::npos ? std::string_view{} : peek.substr(end + 1);
            if (line.ends_with('\r')) line.remove_suffix(1);
            if (opening) {
                opening = false;
                continue;
            }
            if (!closed) {
                if (line == "---") closed = true;
                continue;
            }
            line = trim(line);
            while (line.starts_with(';'))
                line = trim(line.substr(1));
            if (line.starts_with("%%{") && !line.ends_with("}%%")) multiline_directive = true;
            if (line.empty() || line.starts_with("%%")) continue;
            flow = (line.starts_with("flowchart") &&
                    (line.size() == 9u || line[9] == ' ' || line[9] == '\t')) ||
                   (line.starts_with("graph") &&
                    (line.size() == 5u || line[5] == ' ' || line[5] == '\t'));
            sequence = line.starts_with("sequenceDiagram");
            er = line.starts_with("erDiagram") &&
                 (line.size() == 9 || line[9] == ' ' || line[9] == '\t');
            requirement = requirement_prefix_header(line);
            gitgraph = line.starts_with("gitGraph") &&
                       (line.size() == 8u || line[8] == ' ' || line[8] == '\t' || line[8] == ':');
            state = line == "stateDiagram-v2" || line == "stateDiagram";
            mindmap = line == "mindmap";
            block_input = line == "block" || line == "block-beta";
            if (line.starts_with("classDiagram")) {
                class_peek = class_prefix_header(line);
            }
            break;
        }
    }
    // Legacy semantics: when the multiline-directive route runs it OVERWRITES
    // (never ORs) the peeked class/sequence flags.
    bool class_flag = class_peek;
    if (multiline_directive && !sequence && !header.starts_with("sequenceDiagram")) {
        // A directive header without a frontmatter fence still walks the full
        // source once; count that span in the bounded probe term.
        if (!has_frontmatter_peek) peek_bytes = std::max(peek_bytes, source.size());
        const auto probe = sequence_prefix_route(source, work);
        if (!probe) return std::nullopt;  // hard reject
        sequence = *probe == PrefixOwner::Sequence;
        class_flag = *probe == PrefixOwner::Class;
    }

    // Charge after the peek span is known; the multiline route above walks the
    // source one extra time and is bounded by the same term. spend() drains the
    // remaining balance on an unaffordable probe so the caller sees genuine
    // exhaustion; an early pre-charge return would leave a positive balance and
    // be misread as a semantic rejection by the layout cache.
    const std::size_t probe_total =
        kProbeFixed + (has_frontmatter_peek || multiline_directive
                           ? kProbePeekFixed + kProbePerPeekByte * peek_bytes
                           : 0u);
    if (!work.spend(probe_total)) return std::nullopt;

    // ---- ordered probes, first match wins; Charts is the terminal fallback ----
    // RFC 0031 E5: the old per-family prepay accumulator is dominated by the
    // probe fee above; decision.prepay stays zero for the switch.
    MermaidProbeDecision decision;

    if (flow) {
        decision.family = MermaidProbeFamily::Flow;
        return decision;
    }
    if (header.starts_with("sequenceDiagram") || sequence) {
        decision.family = MermaidProbeFamily::Sequence;
        return decision;
    }
    if (header.starts_with("gitGraph") || gitgraph) {
        decision.family = MermaidProbeFamily::GitGraph;
        return decision;
    }

    // Relations family: the execution switch calls one relations_layout for
    // all four; the subfamily label documents the probe order only.
    const bool header_requirement = requirement_prefix_header(header);
    const bool relations_owner = header.starts_with("classDiagram") ||
                                 header.starts_with("erDiagram") ||
                                 header.starts_with("stateDiagram") || header_requirement || er ||
                                 class_flag || requirement || state;
    if (!relations_owner) {
        decision.family = block_input ? MermaidProbeFamily::Block
                          : mindmap   ? MermaidProbeFamily::Mindmap
                                      : MermaidProbeFamily::Charts;
        return decision;
    }
    if (header_requirement || requirement)
        decision.family = MermaidProbeFamily::Requirement;
    else if (class_flag || header.starts_with("classDiagram"))
        decision.family = MermaidProbeFamily::Class;
    else if (state || header.starts_with("stateDiagram"))
        decision.family = MermaidProbeFamily::State;
    else
        decision.family = MermaidProbeFamily::Er;
    decision.class_envelope = class_flag || (class_envelope_seen && class_prefix_header(header));
    decision.requirement_input = header_requirement || requirement;
    return decision;
}

bool detail::valid_packed_rows(const std::vector<std::string>& rows,
                               const std::vector<std::uint16_t>& styles, Work& work,
                               std::size_t* prepaid, Work* prep_sink) {
    const auto deduct = [&](std::size_t charge) {
        // The sole call site routes one family at a time: ER carries the
        // prepaid credit, Block carries the preparation sink; never both
        // (credit would otherwise be consumed but ignored).
        assert(!(prepaid != nullptr && prep_sink != nullptr));
        std::size_t credit = 0u;
        if (prepaid) {
            credit = std::min(charge, *prepaid);
            *prepaid -= credit;
        }
        // D-035 batch 2: Block routes gate charges to its own preparation
        // meter; every other family keeps them on layout Work.
        return (prep_sink ? prep_sink->spend(charge) : work.spend(charge - credit));
    };
    if (rows.size() > kHeightLimit || !deduct(rows.size() * 32u + 128u)) return false;
    std::size_t bytes_to_scan = 0;
    for (const auto& row : rows) {
        if (row.size() > kSourceLimit) return false;
        bytes_to_scan += row.size();
    }
    if (rows.size() > kHeightLimit ||
        styles.size() > static_cast<std::size_t>(kWidthLimit * kHeightLimit) ||
        !deduct(bytes_to_scan * 4u + styles.size() * 2u))
        return false;
    if (styles.empty()) return true;
    if (rows.empty() || rows.size() > kHeightLimit) return false;
    std::size_t slot = 0, bytes = 0;
    for (const auto& line : rows) {
        if (line.size() > kSourceLimit || !unicode_text(line)) return false;
        bytes += line.size();
        const int width = ftxui::string_width(line);
        if (width < 0 || width > kWidthLimit) return false;
        // Validate without allocating a byte-capacity glyph vector. FTXUI's
        // scalar width is the same predicate its Utf8ToGlyphs uses; zero-width
        // scalars extend the previous complete glyph, including a wide tail.
        // unicode_text above has already checked every scalar's encoding.
        bool base = false;
        for (std::size_t byte = 0; byte < line.size();) {
            const auto lead = static_cast<unsigned char>(line[byte]);
            const std::size_t length = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
            const int cells = ftxui::string_width(line.substr(byte, length));
            byte += length;
            if (cells == 0) {
                if (!base) return false;
                continue;
            }
            if ((cells != 1 && cells != 2) || slot > styles.size() ||
                static_cast<std::size_t>(cells) > styles.size() - slot)
                return false;
            const auto style = styles[slot];
            if ((style & 0xf800u) != 0 || (style & 31u) > 6 || ((style >> 5) & 31u) > 6 ||
                (cells == 2 && styles[slot + 1] != style))
                return false;
            slot += static_cast<std::size_t>(cells);
            base = true;
        }
    }
    return slot == styles.size() && slot <= bytes;
}

std::optional<ftxui::Element> detail::finish_mermaid_payload(std::optional<LayoutPayload> payload,
                                                             ftxui::Element fallback, Work& work,
                                                             FinalEmissionTrace* trace,
                                                             PackedMeter meter,
                                                             std::size_t* prepaid,
                                                             Work* external_meter) {
    if (!fallback || !payload || payload->rows.empty()) return {};
    if (payload->styles.empty())
        return finish_legacy_rows(std::move(payload->rows), std::move(fallback));
    const bool metered = meter != PackedMeter::Off;
    // Metered packed ftxui Element emission (ADR 0032 ER; D-035 batch 1 Block).
    // Worst case from the enforced canvas contract: R<=1024 and U<=240*R.
    Work owned_element_work;
    Work* element_meter = nullptr;
    if (metered) {
        // Per-run real-node fee decomposition (D-018 proof): 128 vector slack
        // for run children + 128 text node + 128 for the foreground/background
        // decorator pair + 32 slack = 416U (ER, bold never set). Block edge/
        // box cells add one bold node (style bit 10): +128 -> 544U.
        constexpr std::size_t kRunElementEr = 416u, kRunElementBlock = 544u;
        const std::size_t limit = meter == PackedMeter::Er ? kErElementWorkLimit
                                                            : kBlockPayloadWorkLimit;
        static_assert(kErElementWorkLimit ==
                          320u * static_cast<std::size_t>(kHeightLimit) + 512u +
                              kRunElementEr * static_cast<std::size_t>(kWidthLimit) *
                                  static_cast<std::size_t>(kHeightLimit),
                      "ADR 0032 ElementWork cap derivation");
        static_assert(kBlockElementWorkLimit ==
                          320u * static_cast<std::size_t>(kHeightLimit) + 512u +
                              kRunElementBlock * static_cast<std::size_t>(kWidthLimit) *
                                  static_cast<std::size_t>(kHeightLimit),
                      "D-035 Block ElementWork cap derivation");
        if (trace && trace->element_work)
            element_meter = trace->element_work;
        else if (external_meter)
            // D-035 batch 2 Block: the preparation meter created before the
            // gates (or reconstructed on a cache hit) continues here.
            element_meter = external_meter;
        else {
            owned_element_work.remaining = limit;
            element_meter = &owned_element_work;
        }
        // Style-code range guard, before any fee or allocation. ErPaint emits
        // foreground/background roles only (bits 0-9); Block edge/box cells may
        // additionally set bold (bit 10). Both ranges cover the exact decorator
        // counts the per-run fee funds; anything outside fails closed.
        const std::uint16_t allowed_mask =
            meter == PackedMeter::Er ? static_cast<std::uint16_t>(0x03ffu)
                                     : static_cast<std::uint16_t>(0x07ffu);
        for (const std::uint16_t code : payload->styles)
            if ((code & ~allowed_mask) != 0u) return {};
        if (trace) {
            trace->element_active = true;
            trace->element_cap = element_meter->remaining;
        }
    }
    if (trace) {
        trace->styled_entered = true;
        trace->entry_remaining = work.remaining;
    }
    // Legacy layout-Work debit, exact one-atomic-charge-per-fee sequence for
    // every non-packed family (and for Block's uncredited bookkeeping below).
    const auto pay = [&](std::size_t amount) {
        if (trace) {
            trace->last_debit = amount;
            ++trace->preflight_debits;
        }
        const bool paid = work.spend(amount);
        if (trace) {
            if (paid) {
                trace->emission_debit += amount;
                ++trace->successful_debits;
            } else
                trace->failed_debit = amount;
        }
        return paid;
    };
    // Metered real Element fees. Set above on every metered path (owned capped
    // meter or the F4 injected one); never called for non-packed families.
    const auto pay_element = [&](std::size_t amount) {
        assert(element_meter != nullptr);
        const bool paid = element_meter->spend(amount);
        if (trace) {
            if (paid)
                trace->element_debit += amount;
            else
                trace->element_failed = amount;
        }
        return paid;
    };
    // ER-only: duplicated 8B/256U bookkeeping stays on layout Work, deducted
    // from the ErPaint prepaid credit. A zero residual is already covered and
    // does not spend. D-035 batch 2 leaves Block out of this lambda: Block's
    // bookkeeping rides the payload meter via pay_element (pay_bookkeeping
    // below selects the route), so Block finish never touches layout Work.
    const auto pay_credited = [&](std::size_t amount) {
        const std::size_t credit = prepaid ? std::min(amount, *prepaid) : 0u;
        if (prepaid) *prepaid -= credit;
        const std::size_t residual = amount - credit;
        if (residual == 0u) return true;
        return pay(residual);
    };
    const std::size_t run_bookkeeping = 8u * sizeof(StyledRun);
    const std::size_t run_element_base = 8u * sizeof(ftxui::Element) + 288u;
    // Uniform worst-case per-run rate by family: ER runs never set bold, so
    // the basis 416U (D-018: 128 vector slack + 128 text + 128 fg/bg pair +
    // 32 slack) is exact; Block CAN carry one bold node per run, so every
    // Block run is charged 544U (+128) regardless of whether that particular
    // run is bold — an unconditional worst-case rate, deliberately
    // conservative on non-bold runs (never an under-count) so the closed-form
    // cap with U<=240*R stays valid.
    constexpr std::size_t kBlockBoldNodeFee = 128u;
    const std::size_t run_element =
        run_element_base + (meter == PackedMeter::Block ? kBlockBoldNodeFee : 0u);
    if (metered) {
        if (!pay_element(payload->rows.size() * (8u * sizeof(ftxui::Element) + 64u) + 512u))
            return {};
    } else if (!pay(payload->rows.size() * (8u * sizeof(ftxui::Element) + 64u) + 512u)) {
        return {};
    }
    std::size_t preflight_slot = 0;
    for (const auto& line : payload->rows) {
        std::size_t runs = 0, cells = 0;
        std::uint16_t previous = 0;
        for (std::size_t byte = 0; byte < line.size();) {
            const auto lead = static_cast<unsigned char>(line[byte]);
            const std::size_t length = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
            const auto occupied = ftxui::string_width(line.substr(byte, length));
            byte += length;
            if (occupied == 0) continue;
            const auto code = payload->styles[preflight_slot + cells];
            if (cells == 0 || code != previous) ++runs;
            previous = code;
            cells += static_cast<std::size_t>(occupied);
        }
        preflight_slot += cells;
        if (metered) {
            // Element first: a meter refusal leaves the layout Work untouched.
            if (!pay_element(128u)) return {};
            // ER's bookkeeping is duplicated ErPaint work on layout Work
            // (credited). Block's whole preparation is on its own meter
            // (D-035 batch 2), including the byte rescan and run enumeration.
            const auto pay_bookkeeping = [&](std::size_t amount) {
                return meter == PackedMeter::Er ? pay_credited(amount)
                                                : pay_element(amount);
            };
            if (!pay_bookkeeping(line.size() * 8u)) return {};
            if (!pay_element(runs * run_element) ||
                !pay_bookkeeping(runs * run_bookkeeping))
                return {};
        } else {
            if (!pay(line.size() * 8u + 128u)) return {};
            if (!pay(runs * (run_bookkeeping + run_element_base))) return {};
        }
    }
    if (trace) trace->preflight_complete = true;
    // This private finish only receives admitted payloads from the single
    // layout kernel or immutable exact-key cache. Do not revalidate/relayout.
    // Resolve semantic roles exactly once per fresh Element; immutable cache
    // entries contain neither this palette nor an Element/view into its data.
    const auto active = loom::ui::design::theme::current_theme();
    const auto& p = *active.palette;
    // Role 0 inherits below; slot 0 is an unused palette-only placeholder.
    const std::array<ftxui::Color, 7> fg{p.text,    p.text, p.danger,       p.warning,
                                         p.success, p.info, p.accent_purple};
    const std::array<ftxui::Color, 7> bg{p.background,      p.surface_tint_2,  p.danger_tint_bg,
                                         p.warning_tint_bg, p.success_tint_bg, p.info_tint_bg,
                                         p.surface_tint_4};
    int width = 0;
    std::size_t slot = 0;
    ftxui::Elements rows;
    if (trace) trace->allocation_started = true;
    rows.reserve(payload->rows.size());
    for (const auto& line : payload->rows) {
        width = std::max(width, ftxui::string_width(line));
        ftxui::Elements runs;
        std::string run;
        std::uint16_t code = 0;
        const auto flush = [&] {
            if (run.empty()) return;
            auto node = ftxui::text(std::move(run));
            if (const auto role = code & 31u; role != 0) node = node | ftxui::color(fg[role]);
            if (const auto role = (code >> 5) & 31u; role != 0)
                node = node | ftxui::bgcolor(bg[role]);
            if (code & 1024u) node = node | ftxui::bold;
            runs.push_back(std::move(node));
            run.clear();
        };
        std::size_t cells = 0;
        for (std::size_t byte = 0; byte < line.size();) {
            const auto begin = byte;
            const auto scalar_length = [&](std::size_t at) -> std::size_t {
                const auto lead = static_cast<unsigned char>(line[at]);
                return lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
            };
            const auto length = scalar_length(byte);
            const int occupied = ftxui::string_width(line.substr(byte, length));
            byte += length;
            // Group every zero-width continuation with its base. This retains
            // wide+combining tails as exact original bytes, with no temporary
            // byte-capacity Utf8ToGlyphs vector during fresh-node assembly.
            while (byte < line.size()) {
                const auto tail = scalar_length(byte);
                if (ftxui::string_width(line.substr(byte, tail)) != 0) break;
                byte += tail;
            }
            const auto next = payload->styles[slot + cells];
            if (!run.empty() && code != next) flush();
            code = next;
            run.append(line, begin, byte - begin);
            cells += static_cast<std::size_t>(occupied);
        }
        flush();
        slot += cells;
        // An empty annotation separator still owns one physical row.
        rows.push_back(runs.empty() ? ftxui::text("") : ftxui::hbox(std::move(runs)));
    }
    auto result =
        std::make_shared<MermaidNode>(ftxui::vbox(std::move(rows)), std::move(fallback), width);
    if (trace) trace->returned_native = true;
    return result;
}

// RFC 0031 E6b: layout-only seam. This is the exact pre-E6b dispatch body
// of the two-argument render_mermaid, plus the explicit-ASCII routing of
// the three-argument entry point; it owns a fresh Work budget per call and
// returns immutable rows (nullopt = whole-source rejection).
std::optional<detail::NativeLayout> detail::layout_mermaid_content(std::string_view source,
                                                                   bool use_ascii, Work& work) {
    const auto starting_work = work.remaining;
    std::vector<std::uint16_t> packed_styles;
    // ER sets Er with an ErPaint-prepaid payload; Block sets Block with no
    // prepayment. Both route their real Element fees onto ElementWork
    // (ADR 0032; D-035 batch 1). Every other family leaves this Off, so its
    // charges and rejections are unchanged.
    std::size_t payload_prepaid = 0u;
    PackedMeter packed_meter = PackedMeter::Off;
    // D-035 batch 2: Block-only preparation meter, funding the two packed
    // gates and finish bookkeeping/Element fees. Stays default (and unused)
    // for every other family.
    Work payload_work;
    bool er_packed = false;
    if (source.empty() || source.size() > kSourceLimit) return {};
    if (use_ascii) {
        // An explicit bare Flow header is owned by the closed ASCII Flow
        // profile. Rejected Flow never falls through to GitGraph as a
        // label-like foreign input.
        if (ascii_flow_header(source)) {
            auto rows = detail::flow_ascii_rows(source, work);
            if (!rows) return {};
            return NativeLayout{std::move(rows->ascii), {}, 0u};
        }
        auto rows = detail::gitgraph_layout(source, work, true);
        if (!rows) return {};
        return NativeLayout{std::move(*rows), {}, 0u};
    }
    std::optional<std::vector<std::string>> lines;
    std::optional<StyledRows> styles;
    std::unique_ptr<FlowStyles> flow_styles;
    RichLabels rich;
    if (const auto diagram = parse(source, work, false, &flow_styles, &rich)) {
        lines = flow_layout(*diagram, work, nullptr, flow_styles.get(), &styles, &rich);
    } else {
        // RFC 0031 E4: ordered family probes replace the mutable flag cluster.
        // Classification is one textual scan; execution spends exactly the
        // charges the legacy flags did. nullopt means the legacy route bailed
        // on Work exhaustion before selecting an owner (whole-source result).
        const auto decision = detail::classify_mermaid(source, work);
        if (!decision) return {};
        switch (decision->family) {
        case detail::MermaidProbeFamily::Flow: {
            const auto envelope = flow_envelope_body(source, work);
            if (!envelope || !flow_envelope_parse_prepay(envelope->body, work)) return {};
            auto diagram = parse(envelope->body, work, true, &flow_styles, &rich);
            if (!diagram) return {};
            // FS-1: disclose the accepted non-terminal frontmatter key through
            // the same footer channel as body init directives.
            if (envelope->html_labels)
                rich.notices.push_back(
                    std::string("config.htmlLabels=") + (*envelope->html_labels ? "true" : "false") +
                    " -> native text labels; not applied");
            if (envelope->dagre_layout && flow_untrusted_dagre_compound(*diagram, work))
                return {};
            lines = flow_layout(*diagram, work, nullptr, flow_styles.get(), &styles, &rich);
            if (lines && envelope->title && !prepend_flow_title(*lines, *envelope->title, work))
                return {};
            break;
        }
        case detail::MermaidProbeFamily::Sequence:
            lines = detail::sequence_layout(source, work);
            break;
        case detail::MermaidProbeFamily::GitGraph:
            lines = detail::gitgraph_layout(source, work);
            break;
        case detail::MermaidProbeFamily::Class:
        case detail::MermaidProbeFamily::Er:
        case detail::MermaidProbeFamily::State:
        case detail::MermaidProbeFamily::Requirement:
            er_packed = decision->family == detail::MermaidProbeFamily::Er;
            lines = detail::relations_layout(
                source, work, decision->class_envelope, decision->requirement_input, &styles,
                &packed_styles, er_packed ? &payload_prepaid : nullptr);
            break;
        case detail::MermaidProbeFamily::Block: {
            auto payload = detail::block_layout(source, work);
            if (!payload) return {};
            lines = std::move(payload->rows);
            packed_styles = std::move(payload->styles);
            packed_meter = PackedMeter::Block;
            // D-035 batch 2: every post-parser packed cost (gates + finish
            // preparation) runs on its own bounded meter, keeping dense
            // themed composites inside the fixed layout Work pool.
            payload_work.remaining = kBlockPayloadWorkLimit;
            break;
        }
        case detail::MermaidProbeFamily::Mindmap:
            lines = detail::mindmap_layout(source, work);
            break;
        case detail::MermaidProbeFamily::Charts:
            lines = detail::charts_layout(source, work);
            break;
        }
    }
    if (!lines)
        return {};
    // A bare plain-ER diagram comes back as legacy rows with no packed styles.
    er_packed = er_packed && !packed_styles.empty();
    if (er_packed) packed_meter = PackedMeter::Er;
    if (packed_styles.empty()) packed_meter = PackedMeter::Off;
    NativeLayout result{std::move(*lines), std::move(styles), 0u, std::move(packed_styles),
                        packed_meter, payload_prepaid, std::move(payload_work), 0u};
    Work* const block_prep =
        result.packed_meter == PackedMeter::Block ? &result.payload_work : nullptr;
    if (!result.packed_styles.empty() &&
        (result.styles ||
         !valid_packed_rows(result.rows, result.packed_styles, work,
                            er_packed ? &result.payload_prepaid : nullptr, block_prep)))
        return {};
    if (!prepay_styled_payload(result, source.size(), work,
                               er_packed ? &result.payload_prepaid : nullptr, block_prep))
        return {};
    if (result.styles || !result.packed_styles.empty())
        result.measured_layout_work = starting_work - work.remaining;
    if (result.packed_meter == PackedMeter::Block)
        result.measured_payload_work = kBlockPayloadWorkLimit - result.payload_work.remaining;
    return result;
}

std::optional<std::vector<std::string>> detail::layout_mermaid_rows(std::string_view source,
                                                                    bool use_ascii) {
    Work work;
    auto layout = detail::layout_mermaid_content(source, use_ascii, work);
    if (!layout) return {};
    return std::move(layout->rows);
}

std::optional<ftxui::Element> detail::render_mermaid_with_budget(std::string_view source,
                                                                 ftxui::Element fallback,
                                                                 bool use_ascii, Work& work,
                                                                 FinalEmissionTrace* trace) {
    // Preserve a caller-injected ElementWork meter (ADR 0032 F4) across the
    // per-call observation reset.
    Work* injected_element_work = trace ? trace->element_work : nullptr;
    if (trace) *trace = {};
    if (trace) trace->element_work = injected_element_work;
    if (!fallback) return {};
    try {
        return finish_native_layout(layout_mermaid_content(source, use_ascii, work),
                                    std::move(fallback), work, trace);
    } catch (const std::bad_alloc&) {
        return {};
    } catch (const std::length_error&) {
        return {};
    }
}

std::optional<ftxui::Element> render_mermaid(std::string_view source, ftxui::Element fallback) {
    Work work;
    return detail::render_mermaid_with_budget(source, std::move(fallback), false, work);
}

std::optional<ftxui::Element> render_mermaid(std::string_view source, ftxui::Element fallback,
                                             bool use_ascii) {
    Work work;
    return detail::render_mermaid_with_budget(source, std::move(fallback), use_ascii, work);
}

}  // namespace loom::ui::mermaid
