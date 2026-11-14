module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;

import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// An agentflow-beta diagram is an indentation-structured graph of connector
// declarations, `flow`/`global` containers, metadata-bearing nodes and four
// edge vocabularies. The terminal has no agentflow geometry engine or color
// contract, so the projection is a bounded native outline: every statement is
// shape-validated against a closed grammar, every edge endpoint and metadata
// owner resolves to a declared element, and the validated structure is
// transcribed as a hierarchy with canonical edge markers. No geometry, no
// color; unknown statements reject the whole source rather than being drawn
// as an approximate flowchart.

constexpr int kAfMaxDepth = 8;             // corpus max indent is 8 spaces
constexpr std::size_t kAfMaxElements = 256;
constexpr std::size_t kAfMaxEdges = 256;
constexpr std::size_t kAfMaxRows = 1024;
constexpr std::size_t kAfMaxMeta = 16;

enum class AfKind { Node, Connector, Flow, Global };
enum class AfEdge { Solid, Dotted, Failure };

struct AfMeta {
    std::string key, value;  // value stored decoded (quotes removed)
};

struct AfElement {
    AfKind kind = AfKind::Node;
    std::string id;
    std::string label;             // decoded display label, may be empty
    std::string shape;             // node shape, empty when unset
    bool collapsed = false;        // flow view: collapsed
    std::vector<AfMeta> metadata;  // validated, ordered key/value rows
    unsigned meta_seen = 0;        // persistent key bitmask across @{ } blocks
    int depth = 0;                 // owning indent depth (root == 0)
    std::size_t parent = std::string::npos;
    std::vector<std::size_t> children;
};

struct AfEdgeRecord {
    std::size_t from, to;
    AfEdge marker = AfEdge::Solid;
    std::string label;  // canonical decoded label, empty when unlabeled
    int depth = 0;
};

struct AfDiagram {
    std::string direction = "TB";
    std::vector<AfElement> elements;
    std::vector<AfEdgeRecord> edges;
    std::vector<std::size_t> roots;
};

std::string_view af_trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return value;
}

bool af_ident(std::string_view token) {
    if (token.empty() || token.size() > kLabelLimit) return false;
    for (std::size_t i = 0; i < token.size(); ++i) {
        const char c = token[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
              (i > 0 && c >= '0' && c <= '9')))
            return false;
    }
    return true;
}

// A quoted scalar: opening and closing double quote, printable ASCII contents
// (the corpus bodies are pure ASCII); the decoded value excludes the quotes.
std::optional<std::string> af_quoted(std::string_view token) {
    if (token.size() < 2 || token.front() != '"' || token.back() != '"') return std::nullopt;
    auto inner = token.substr(1, token.size() - 2);
    for (const unsigned char c : inner)
        if (c < 0x20 || c == 0x7f) return std::nullopt;
    return std::string(inner);
}

// Key-based visual gate over a leading frontmatter block. Mirrors the other
// greenfield renderers: config/theme/look (and any non-description key) keep
// the whole source on fallback because the text surface has no such contract.
bool af_frontmatter_gated(std::string_view source, std::string_view& body) {
    if (!source.starts_with("---\n")) {
        body = source;
        return false;
    }
    std::size_t search = 4;
    std::optional<std::size_t> close_nl;
    std::size_t close_line_start = 0;
    while (true) {
        const auto nl = source.find('\n', search);
        const auto line = source.substr(
            search, nl == std::string_view::npos ? std::string_view::npos : nl - search);
        if (line == "---") {
            close_nl = nl;
            close_line_start = search;
            break;
        }
        if (nl == std::string_view::npos) return true;  // unclosed fence
        search = nl + 1;
    }
    if (!close_nl) return true;
    // A closing fence with no trailing newline leaves an empty body.
    if (*close_nl == std::string_view::npos) {
        body = std::string_view{};
        return false;
    }
    // Exclude the closing fence line itself from the scanned frontmatter.
    const auto frontmatter = source.substr(4, close_line_start - 4);
    for (std::size_t pos = 0;;) {
        const auto eol = frontmatter.find('\n', pos);
        const auto line = frontmatter.substr(
            pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
        pos = eol == std::string_view::npos ? frontmatter.size() : eol + 1;
        const auto key_start = line.find_first_not_of(' ');
        if (key_start == std::string_view::npos) {            // blank line
            if (eol == std::string_view::npos) break;
            continue;
        }
        if (key_start != 0) {                                  // nested/continuing line
            if (eol == std::string_view::npos) break;
            continue;
        }
        const auto column0 = line.substr(key_start);
        if (column0.starts_with('#')) {                       // YAML comment
            if (eol == std::string_view::npos) break;
            continue;
        }
        // Mirror parse_envelope: a column-0 non-comment line needs a colon.
        const auto colon = column0.find(':');
        if (colon == std::string_view::npos) return true;
        // Trim whitespace around the key so `description : x` is recognized.
        const auto key = af_trim(column0.substr(0, colon));
        if (key != "description") return true;
        if (eol == std::string_view::npos) break;
    }
    body = source.substr(*close_nl + 1);
    return false;
}

const std::array<std::string_view, 6> kAfShapes{
    {"input", "task", "tool", "refdoc", "decision", "action"}};

bool af_shape_name(std::string_view value) {
    for (const auto shape : kAfShapes)
        if (value == shape) return true;
    return false;
}

// Validate one metadata component against the closed key vocabulary. String
// keys require a quoted scalar; shape is a bare closed-set token; view is the
// quoted collapsed marker; token_required is a boolean. Unknown keys, values
// or duplicate keys reject.
bool af_meta_pair(std::string_view key, std::string_view raw_value, AfElement& element,
                  unsigned& seen) {
    const auto value = af_trim(raw_value);
    const auto mark = [&](unsigned bit) -> bool {
        if (seen & bit) return false;
        seen |= bit;
        return true;
    };
    if (key == "shape") {
        // Only nodes carry a rendered shape; a shape on a flow/global/connector
        // would be stored but never emitted, so reject it rather than ignore it.
        if (element.kind != AfKind::Node) return false;
        if (!mark(1u) || !af_shape_name(value) || value.find('"') != std::string_view::npos)
            return false;
        element.shape = std::string(value);
        return true;
    }
    if (key == "view") {
        if (!mark(2u)) return false;
        const auto decoded = af_quoted(value);
        if (!decoded || *decoded != "collapsed") return false;
        if (element.kind != AfKind::Flow) return false;
        element.collapsed = true;
        return true;
    }
    if (key == "token_required") {
        if (!mark(4u) || (value != "true" && value != "false")) return false;
        element.metadata.push_back({std::string(key), std::string(value)});
        return true;
    }
    static constexpr std::array<std::pair<std::string_view, unsigned>, 9> string_keys{{
        {"value", 8u},     {"description", 16u}, {"params", 32u},
        {"returns", 64u},  {"protocol", 128u},    {"endpoint", 256u},
        {"connectorRef", 512u}, {"instruction", 1024u}, {"model", 2048u},
    }};
    for (const auto [name, bit] : string_keys) {
        if (key != name) continue;
        if (!mark(bit)) return false;
        const auto decoded = af_quoted(value);
        if (!decoded) return false;
        element.metadata.push_back({std::string(key), std::move(*decoded)});
        return true;
    }
    return false;
}

// Parse the inside of a `@{ ... }` block into metadata. The persistent `seen`
// bitmask lives on the element so a duplicate key supplied in a later block is
// rejected exactly like one inside the same block. Commas separate pairs but
// a comma inside a quoted value does not, so scanning is quote-aware.
bool af_meta_block(std::string_view text, AfElement& element, unsigned& seen) {
    std::size_t start = 0;
    bool in_quote = false;
    auto parse_component = [&](std::string_view component) -> bool {
        component = af_trim(component);
        if (component.empty()) return true;  // tolerate a trailing comma
        const auto colon = component.find(':');
        if (colon == std::string_view::npos) return false;
        const auto key = af_trim(component.substr(0, colon));
        const auto value = af_trim(component.substr(colon + 1));
        return af_ident(key) && af_meta_pair(key, value, element, seen);
    };
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i < text.size() && text[i] == '"') {
            in_quote = !in_quote;
            continue;
        }
        if (i == text.size() || (!in_quote && text[i] == ',')) {
            if (!parse_component(text.substr(start, i - start))) return false;
            start = i + 1;
        }
    }
    if (in_quote) return false;  // unterminated quoted value
    return true;
}

// A declaration target: `id["label"]`, `id("label")` or bare `id`.
struct AfDeclTarget {
    std::string id;
    std::string label;
    bool has_label = false;
};

std::optional<AfDeclTarget> af_decl_target(std::string_view text) {
    const auto open_sq = text.find('['), open_paren = text.find('(');
    const auto open = open_sq == std::string_view::npos
                          ? open_paren
                          : (open_paren == std::string_view::npos
                                 ? open_sq
                                 : std::min(open_sq, open_paren));
    if (open == std::string_view::npos) {
        const auto id = af_trim(text);
        if (!af_ident(id)) return std::nullopt;
        return AfDeclTarget{std::string(id), {}, false};
    }
    const char close_char = text[open] == '[' ? ']' : ')';
    if (text.back() != close_char) return std::nullopt;
    const auto id = af_trim(text.substr(0, open));
    if (!af_ident(id)) return std::nullopt;
    const auto label = af_quoted(text.substr(open + 1, text.size() - open - 2));
    if (!label) return std::nullopt;
    return AfDeclTarget{std::string(id), std::move(*label), true};
}

// One parsed chain hop before id resolution. A target may be a bare id or an
// inline declaration (`id["label"]` / `id("label")`).
struct AfHop {
    AfEdge marker;
    std::string label;       // edge label, empty when unlabeled
    std::string target;      // target id
    std::string target_label;
    bool target_declared_inline = false;
};

// Parse one endpoint at `text` (no leading spaces). Returns the target and
// the consumed length, or nullopt when the shape is malformed.
std::optional<std::tuple<std::string, std::string, bool, std::size_t>> af_endpoint(
    std::string_view text) {
    const auto id_end = text.find_first_of(" [](");
    if (id_end == std::string_view::npos) {
        const auto id = af_trim(text);
        if (!af_ident(id)) return std::nullopt;
        return std::tuple{std::string(id), std::string{}, false, id.size()};
    }
    const auto id = af_trim(text.substr(0, id_end));
    if (!af_ident(id) || id_end >= text.size()) return std::nullopt;
    // A space terminates a bare id; a bracket opens an inline declaration.
    if (text[id_end] == ' ')
        return std::tuple{std::string(id), std::string{}, false, id_end};
    const char open = text[id_end];
    const char close = open == '[' ? ']' : ')';
    const auto close_pos = text.find(close, id_end + 1);
    if (close_pos == std::string_view::npos || close_pos <= id_end + 1)
        return std::nullopt;
    const auto decoded =
        af_quoted(text.substr(id_end + 1, close_pos - id_end - 1));
    if (!decoded) return std::nullopt;
    return std::tuple{std::string(id), std::move(*decoded), true, close_pos + 1};
}

// Match an arrow token at the start of `text`; returns marker, edge label and
// consumed length.
std::optional<std::tuple<AfEdge, std::string, std::size_t>> af_arrow_token(
    std::string_view text) {
    if (text.starts_with("-->")) {
        const std::size_t arrow_len = 3;
        const auto rest = af_trim(text.substr(arrow_len));
        if (rest.starts_with("|\"")) {
            // Offset of `rest` relative to `text`; already includes the arrow.
            const std::size_t rest_offset =
                static_cast<std::size_t>(rest.data() - text.data());
            const auto close = rest.find('|', 2);
            if (close == std::string_view::npos) return std::nullopt;
            const auto decoded = af_quoted(rest.substr(1, close - 1));
            if (!decoded) return std::nullopt;
            return std::tuple{AfEdge::Solid, std::move(*decoded),
                              rest_offset + (close + 1)};
        }
        return std::tuple{AfEdge::Solid, std::string{}, arrow_len};
    }
    if (text.starts_with("-.-"))
        return std::tuple{AfEdge::Dotted, std::string{}, std::size_t{3}};
    if (text.starts_with("--x"))
        return std::tuple{AfEdge::Failure, std::string{}, std::size_t{3}};
    if (text.starts_with("--")) {
        const auto tail = text.find("-->");
        if (tail == std::string_view::npos) return std::nullopt;
        const auto label = af_trim(text.substr(2, tail - 2));
        if (!af_ident(label)) return std::nullopt;
        return std::tuple{AfEdge::Solid, std::string(label), tail + 3};
    }
    return std::nullopt;
}

// Tokenize an edge line into its source endpoint and one or more hops. The
// grammar is `endpoint arrow endpoint (arrow endpoint)*`, where an endpoint
// is a bare id or an inline `id["label"]` declaration. Any leftover token
// rejects the whole line.
std::optional<std::pair<AfDeclTarget, std::vector<AfHop>>> af_edge_line(
    std::string_view line) {
    std::size_t pos = 0;
    const auto first = af_endpoint(af_trim(line));
    if (!first) return std::nullopt;
    const auto [src_id, src_label, src_inline, src_len] = *first;
    pos += (line.size() - af_trim(line).size()) + src_len;
    AfDeclTarget source{src_id, src_label, src_inline};
    std::vector<AfHop> hops;
    while (pos < line.size()) {
        auto text = af_trim(line.substr(pos));
        pos += line.substr(pos).size() - text.size();
        const auto arrow = af_arrow_token(text);
        if (!arrow) return std::nullopt;
        const auto [marker, edge_label, arrow_len] = *arrow;
        pos += arrow_len;
        text = af_trim(line.substr(pos));
        pos += line.substr(pos).size() - text.size();
        const auto target = af_endpoint(text);
        if (!target) return std::nullopt;
        const auto [tid, tlabel, tinline, tlen] = *target;
        pos += tlen;
        hops.push_back(AfHop{marker, edge_label, tid, tlabel, tinline});
    }
    if (hops.empty()) return std::nullopt;
    return std::pair{std::move(source), std::move(hops)};
}

struct AfParser {
    std::string_view source;
    Work& work;
    AfDiagram diagram;
    std::map<std::string, std::size_t> index;
    std::vector<std::size_t> scope;  // element indices, back = current container

    explicit AfParser(std::string_view src, Work& w) : source(src), work(w) {}

    bool charge(std::size_t amount) { return work.spend(amount); }

    std::size_t current_parent() const {
        return scope.empty() ? std::string::npos : scope.back();
    }

    AfElement* find(std::string_view id) {
        const auto it = index.find(std::string(id));
        return it == index.end() ? nullptr : &diagram.elements[it->second];
    }

    bool register_element(AfElement element) {
        if (diagram.elements.size() >= kAfMaxElements) return false;
        if (index.contains(element.id)) return false;
        const auto idx = diagram.elements.size();
        index.emplace(element.id, idx);
        const auto parent = current_parent();
        element.parent = parent;
        if (parent == std::string::npos)
            diagram.roots.push_back(idx);
        else
            diagram.elements[parent].children.push_back(idx);
        diagram.elements.push_back(std::move(element));
        return true;
    }

    // Attach a metadata block to an existing owner by id. The owner's
    // persistent key mask makes duplicate keys across blocks fail closed.
    bool attach_metadata(std::string_view id, std::string_view text) {
        auto* owner = find(id);
        if (!owner) return false;
        if (af_trim(text).empty()) return true;  // whitespace-only block is a no-op
        if (owner->metadata.size() >= kAfMaxMeta) return false;
        const auto before = owner->metadata.size();
        const auto collapsed_before = owner->collapsed;
        const auto shape_before = owner->shape;
        if (!af_meta_block(text, *owner, owner->meta_seen)) return false;
        if (owner->metadata.size() == before && owner->collapsed == collapsed_before &&
            owner->shape == shape_before)
            return false;  // a non-empty block parsed no attribute
        return true;
    }

    // Resolve an endpoint, registering an inline declaration when the id is
    // new. A redeclaration must match the existing element's label. Edges may
    // terminate at a node or a flow container (the corpus chains flows), but
    // never at a connector: connectors are bound via connectorRef metadata,
    // not graph edges.
    std::optional<std::size_t> resolve_endpoint(const AfDeclTarget& endpoint, int depth) {
        if (auto* existing = find(endpoint.id)) {
            if (existing->kind == AfKind::Connector) return std::nullopt;
            if (endpoint.has_label && existing->label != endpoint.label) return std::nullopt;
            return static_cast<std::size_t>(existing - diagram.elements.data());
        }
        if (!endpoint.has_label) return std::nullopt;  // bare id must pre-exist
        AfElement element;
        element.kind = AfKind::Node;
        element.id = endpoint.id;
        element.label = endpoint.label;
        element.depth = depth;
        if (!register_element(std::move(element))) return std::nullopt;
        return diagram.elements.size() - 1;
    }

    bool parse_edge(std::string_view line, int depth) {
        const auto tokenized = af_edge_line(line);
        if (!tokenized) return false;
        const auto from_opt = resolve_endpoint(tokenized->first, depth);
        if (!from_opt) return false;
        std::size_t from = *from_opt;
        for (const auto& hop : tokenized->second) {
            const AfDeclTarget target_ep{hop.target, hop.target_label,
                                         hop.target_declared_inline};
            const auto to_opt = resolve_endpoint(target_ep, depth);
            if (!to_opt) return false;
            if (diagram.edges.size() >= kAfMaxEdges) return false;
            diagram.edges.push_back(AfEdgeRecord{from, *to_opt, hop.marker, hop.label, depth});
            from = *to_opt;
        }
        return true;
    }

    std::optional<AfDiagram> run() {
        if (source.empty() || source.size() > kSourceLimit) return std::nullopt;
        if (!charge(source.size() * 2 + 256)) return std::nullopt;
        for (const unsigned char c : source)
            if (c == '\t' || c == '\r' || c == 0x7f || (c < 0x20 && c != '\n'))
                return std::nullopt;

        std::string_view body;
        if (af_frontmatter_gated(source, body)) return std::nullopt;

        std::size_t at = 0;
        bool header_seen = false;
        std::optional<std::pair<int, std::string>> pending_meta;  // depth, owner
        std::string accumulated_meta;
        while (at <= body.size()) {
            const auto nl = body.find('\n', at);
            auto raw = body.substr(at, nl == std::string_view::npos
                                           ? std::string_view::npos : nl - at);
            at = nl == std::string_view::npos ? body.size() + 1 : nl + 1;
            if (raw.empty()) continue;
            const auto content_pos = raw.find_first_not_of(' ');
            if (content_pos == std::string_view::npos) continue;
            if (content_pos % 2 != 0 || content_pos > kAfMaxDepth * 2) return std::nullopt;
            const int depth = static_cast<int>(content_pos / 2);
            const auto content = raw.substr(content_pos);
            // Plain comments are ignored, but an engine `%%{init ...}%%`
            // directive is a visual contract the text surface cannot honor:
            // reject it explicitly rather than silently dropping it.
            if (content.starts_with("%%")) {
                if (content.starts_with("%%{")) return std::nullopt;
                continue;
            }

            // Continuation of a multi-line metadata block: accumulate inner
            // lines until the closing `}`, then validate the whole block so
            // an inner key can never be silently ignored.
            if (pending_meta) {
                if (content == "}") {
                    // The corpus places the closing brace on its own column-0
                    // line regardless of the opener's indent, so only the
                    // token itself is required.
                    const auto owner = pending_meta->second;
                    const auto text = af_trim(accumulated_meta);
                    if (!text.empty() && !attach_metadata(owner, text))
                        return std::nullopt;
                    pending_meta.reset();
                    accumulated_meta.clear();
                } else {
                    if (!accumulated_meta.empty()) accumulated_meta += ' ';
                    accumulated_meta += std::string(content);
                }
                continue;
            }

            if (!header_seen) {
                const auto space = content.find(' ');
                const auto keyword =
                    space == std::string_view::npos ? content : content.substr(0, space);
                if (keyword != "agentflow-beta") return std::nullopt;
                if (space != std::string_view::npos) {
                    const auto direction = af_trim(content.substr(space + 1));
                    if (direction != "TB" && direction != "LR" && direction != "BT" &&
                        direction != "RL")
                        return std::nullopt;
                    diagram.direction = std::string(direction);
                }
                header_seen = true;
                continue;
            }

            // An `end` closes the current container and must sit at that
            // container's own indent depth. Handle it before the generic
            // dedent pop so a same-depth end is not mistaken for a new scope.
            if (content == "end") {
                if (scope.empty() || depth != diagram.elements[scope.back()].depth)
                    return std::nullopt;
                scope.pop_back();
                continue;
            }

            // A non-`end` statement may not silently close an open container:
            // the grammar requires an explicit same-depth `end`, so any dedent
            // across an open scope is a malformed source.
            if (!scope.empty() && depth <= diagram.elements[scope.back()].depth)
                return std::nullopt;
            if (content == "global") {
                AfElement element;
                element.kind = AfKind::Global;
                element.id = "";
                element.depth = depth;
                if (!register_element(std::move(element))) return std::nullopt;
                scope.push_back(diagram.elements.size() - 1);
                continue;
            }
            if (content.starts_with("connector ")) {
                // Connectors are root-anchored: the corpus indents them two
                // spaces but never nests one inside a flow/global block.
                const auto target = af_decl_target(af_trim(content.substr(10)));
                if (!target || !scope.empty() || !target->has_label ||
                    target->label.empty())
                    return std::nullopt;
                AfElement element;
                element.kind = AfKind::Connector;
                element.id = target->id;
                element.label = std::move(target->label);
                element.depth = depth;
                if (!register_element(std::move(element))) return std::nullopt;
                continue;
            }
            if (content.starts_with("flow ")) {
                const auto target = af_decl_target(af_trim(content.substr(5)));
                if (!target || !target->has_label || target->label.empty())
                    return std::nullopt;
                AfElement element;
                element.kind = AfKind::Flow;
                element.id = target->id;
                element.label = std::move(target->label);
                element.depth = depth;
                if (!register_element(std::move(element))) return std::nullopt;
                scope.push_back(diagram.elements.size() - 1);
                continue;
            }

            // Metadata block. The owner prefix is either a bare id or a full
            // node declaration (`id["label"]@{ ... }`) that is registered first.
            const auto meta_at = content.find("@{");
            if (meta_at != std::string_view::npos) {
                const auto prefix = af_trim(content.substr(0, meta_at));
                const auto decl = af_decl_target(prefix);
                if (!decl) return std::nullopt;
                if (decl->has_label) {
                    AfElement element;
                    element.kind = AfKind::Node;
                    element.id = decl->id;
                    element.label = std::move(decl->label);
                    element.depth = depth;
                    if (!register_element(std::move(element))) return std::nullopt;
                }
                const std::string owner = decl->id;
                const auto close = content.rfind('}');
                if (close == std::string_view::npos || close < meta_at) {
                    pending_meta = {depth, owner};
                    continue;
                }
                const auto text =
                    af_trim(content.substr(meta_at + 2, close - meta_at - 2));
                if (!text.empty() && !attach_metadata(owner, text)) return std::nullopt;
                continue;
            }

            // Edge statement (may chain several arrows).
            if (content.find("-->") != std::string_view::npos ||
                content.find("-.-") != std::string_view::npos ||
                content.find("--x") != std::string_view::npos) {
                if (!parse_edge(content, depth)) return std::nullopt;
                continue;
            }

            // Plain node declaration.
            {
                const auto target = af_decl_target(content);
                if (!target) return std::nullopt;
                AfElement element;
                element.kind = AfKind::Node;
                element.id = target->id;
                element.label = std::move(target->label);
                element.depth = depth;
                if (!register_element(std::move(element))) return std::nullopt;
            }
        }
        if (!header_seen || pending_meta) return std::nullopt;
        if (!scope.empty()) return std::nullopt;  // unclosed container
        if (diagram.elements.empty()) return std::nullopt;
        return diagram;
    }
};

std::string af_edge_marker(AfEdge marker) {
    switch (marker) {
    case AfEdge::Dotted:
        return "-.-";
    case AfEdge::Failure:
        return "--x";
    default:
        return "-->";
    }
}

bool af_emit_element(std::vector<std::string>& rows, const AfDiagram& diagram, std::size_t idx,
                     Work& work) {
    const auto& element = diagram.elements[idx];
    auto push = [&](const std::string& row) -> bool {
        if (rows.size() >= kAfMaxRows || ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return false;
        rows.push_back(row);
        return true;
    };
    const std::string indent(static_cast<std::size_t>(element.depth) * 2, ' ');

    if (element.kind == AfKind::Global) {
        if (!push(indent + "global")) return false;
    } else if (element.kind == AfKind::Connector) {
        if (!push(indent + "connector " + element.id + ": " + element.label)) return false;
    } else if (element.kind == AfKind::Flow) {
        std::string row = indent + "flow " + element.id + ": " + element.label;
        if (element.collapsed) row += " [collapsed]";
        if (!push(row)) return false;
    } else {
        std::string row = indent + element.id;
        if (!element.shape.empty()) row += " [" + element.shape + "]";
        if (!element.label.empty()) row += ": " + element.label;
        if (!push(row)) return false;
    }
    for (const auto& meta : element.metadata) {
        if (!push(indent + "  " + meta.key + ": " + meta.value)) return false;
    }
    for (const auto child : element.children)
        if (!af_emit_element(rows, diagram, child, work)) return false;
    return true;
}

}  // namespace

std::optional<std::vector<std::string>> agentflow_layout(std::string_view source, Work& work) {
    AfParser parser(source, work);
    auto parsed = parser.run();
    if (!parsed) return std::nullopt;
    std::vector<std::string> rows;
    if (!work.spend(32)) return std::nullopt;
    rows.push_back("agentflow-beta " + parsed->direction);
    for (const auto root : parsed->roots)
        if (!af_emit_element(rows, *parsed, root, work)) return std::nullopt;
    // Edges form a separate ordered pass, each emitted once at its own source
    // indent depth with canonical markers and decoded labels.
    for (const auto& edge : parsed->edges) {
        const std::string indent(static_cast<std::size_t>(edge.depth) * 2, ' ');
        std::string row = indent + parsed->elements[edge.from].id + " ";
        if (!edge.label.empty()) row += "-- " + edge.label + " ";
        row += af_edge_marker(edge.marker) + " " + parsed->elements[edge.to].id;
        if (rows.size() >= kAfMaxRows || ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return std::nullopt;
        rows.push_back(std::move(row));
    }
    return rows;
}

}  // namespace loom::ui::mermaid::detail
