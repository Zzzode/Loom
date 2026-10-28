// Requirement11 test-private oracle.  Include at EOF of
// test_mermaid_relations_semantics.cpp, after its established Grid/Work/report
// helpers.  It deliberately has no production or public-test API.
#pragma once

namespace requirement11_native {

inline constexpr std::size_t kSceneLimit = 128000000u;
inline constexpr std::size_t kSetupLimit = 8000000u;
inline constexpr std::string_view kFollowing = "AFTER_REQUIREMENT";
inline constexpr std::string_view kAssetRoot = "tests/fixtures/mermaid_native/requirement11";

enum class Role : unsigned char { Requirement, Element };
enum class EdgeKind : unsigned char {
    Contains,
    Copies,
    Derives,
    Satisfies,
    Verifies,
    Refines,
    Traces
};
// Requirement-private source-field mappings; neither aliases a production
// relation record nor changes its sizeof/default Work tariff.
enum class Risk : unsigned char { Low, Medium, High };
enum class Verification : unsigned char { Analysis, Demonstration, Inspection, Test };

inline std::optional<Risk> risk_display(std::string_view value) {
    if (value == "Low") return Risk::Low;
    if (value == "Medium") return Risk::Medium;
    if (value == "High") return Risk::High;
    return std::nullopt;
}

inline std::optional<Risk> risk_source(std::string_view value) {
    if (value == "low") return Risk::Low;
    if (value == "medium") return Risk::Medium;
    if (value == "high") return Risk::High;
    return std::nullopt;
}

inline std::string_view risk_text(Risk value) {
    switch (value) {
    case Risk::Low:
        return "Low";
    case Risk::Medium:
        return "Medium";
    case Risk::High:
        return "High";
    }
    throw std::runtime_error("Requirement11 Risk enum");
}

inline std::optional<Verification> verification_display(std::string_view value) {
    if (value == "Analysis") return Verification::Analysis;
    if (value == "Demonstration") return Verification::Demonstration;
    if (value == "Inspection") return Verification::Inspection;
    if (value == "Test") return Verification::Test;
    return std::nullopt;
}

inline std::optional<Verification> verification_source(std::string_view value) {
    if (value == "analysis") return Verification::Analysis;
    if (value == "demonstration") return Verification::Demonstration;
    if (value == "inspection") return Verification::Inspection;
    if (value == "test") return Verification::Test;
    return std::nullopt;
}

inline std::string_view verification_text(Verification value) {
    switch (value) {
    case Verification::Analysis:
        return "Analysis";
    case Verification::Demonstration:
        return "Demonstration";
    case Verification::Inspection:
        return "Inspection";
    case Verification::Test:
        return "Test";
    }
    throw std::runtime_error("Requirement11 Verification enum");
}

inline bool requirement_stereotype(std::string_view value) {
    return value == "Requirement" || value == "Functional Requirement" ||
           value == "Interface Requirement" || value == "Performance Requirement" ||
           value == "Physical Requirement" || value == "Design Constraint";
}

struct NodeRef {
    Role role{};
    std::string name;
    bool operator==(const NodeRef&) const = default;
};

struct Metadata {
    std::optional<std::string> id, text;
    std::optional<Risk> risk;
    std::optional<Verification> verification;
    std::optional<std::string> type, doc_ref;
    bool operator==(const Metadata&) const = default;
};

struct Node {
    NodeRef ref;
    std::string stereotype;
    Metadata metadata;
    bool operator==(const Node&) const = default;
};

struct Edge {
    NodeRef source, target;
    EdgeKind kind{};
    int order = 0;
    std::string syntax;
    bool operator==(const Edge&) const = default;
};

struct Scene {
    int ordinal = 0;
    std::string path, source_sha, direction;
    std::vector<Node> nodes;
    std::vector<Edge> edges;
};

struct Frames {
    Grid source240, source40, wide, narrow, restored;
};

struct Endpoint {
    std::size_t node = 0;
    Point border, marker, route;
    int exterior = 0;
    bool circle = false;
};

struct EdgeWitness {
    Endpoint marker;
    Point source_contact{}, target_contact{};
    Label label;
    std::vector<int> path;
};

struct Geometry {
    std::vector<Rect> boxes;
    std::vector<Endpoint> circles, arrows;
    std::vector<Label> labels;
    std::vector<bool> claimed;
    std::vector<std::vector<int>> edge_paths;
    std::vector<EdgeWitness> edges;
};

inline void need(bool value, std::string_view what) {
    class481_require(value, what);
}

inline std::string load(const std::filesystem::path& path, Work& work) {
    const auto bytes = std::filesystem::file_size(path);
    work.spend(static_cast<std::size_t>(bytes) * 4u + 4096u);
    std::ifstream input(path, std::ios::binary);
    need(input.good(), "Requirement11 immutable asset open");
    std::string out{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    need(out.size() == bytes, "Requirement11 immutable asset exact byte read");
    return out;
}

inline std::string text(JsonVal value, Work& work, std::size_t max = 4096u) {
    need(value.is_str(), "Requirement11 expected JSON string");
    const auto out = std::string(value.as_str());
    work.spend(out.size() * 4u + 64u);
    need(out.size() <= max, "Requirement11 JSON string bound");
    return out;
}

inline std::optional<std::string> nullable_text(JsonVal value, Work& work) {
    if (value.is_null()) return std::nullopt;
    return text(value, work);
}

inline Role role(std::string_view value) {
    if (value == "requirement") return Role::Requirement;
    if (value == "element") return Role::Element;
    throw std::runtime_error("Requirement11 unknown node role");
}

inline EdgeKind edge_kind(std::string_view value) {
    if (value == "contains") return EdgeKind::Contains;
    if (value == "copies") return EdgeKind::Copies;
    if (value == "derives") return EdgeKind::Derives;
    if (value == "satisfies") return EdgeKind::Satisfies;
    if (value == "verifies") return EdgeKind::Verifies;
    if (value == "refines") return EdgeKind::Refines;
    if (value == "traces") return EdgeKind::Traces;
    throw std::runtime_error("Requirement11 unknown relationship kind");
}

inline std::string_view edge_name(EdgeKind kind) {
    switch (kind) {
    case EdgeKind::Contains:
        return "contains";
    case EdgeKind::Copies:
        return "copies";
    case EdgeKind::Derives:
        return "derives";
    case EdgeKind::Satisfies:
        return "satisfies";
    case EdgeKind::Verifies:
        return "verifies";
    case EdgeKind::Refines:
        return "refines";
    case EdgeKind::Traces:
        return "traces";
    }
    throw std::runtime_error("Requirement11 edge enum");
}

inline NodeRef node_ref(std::string_view value) {
    const auto colon = value.find(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 1 >= value.size())
        throw std::runtime_error("Requirement11 NodeRef syntax");
    return {role(value.substr(0, colon)), std::string(value.substr(colon + 1))};
}

inline bool contains_node(const Scene& scene, const NodeRef& value) {
    return std::ranges::any_of(scene.nodes, [&](const Node& n) { return n.ref == value; });
}

inline std::string edge_label(EdgeKind kind) {
    return "<<" + std::string(edge_name(kind)) + ">>";
}

inline std::vector<std::string> visible_rows(const Node& node) {
    std::vector<std::string> rows;
    rows.reserve(6u);
    rows.push_back("<<" + node.stereotype + ">>");
    rows.push_back(node.ref.name);
    if (node.ref.role == Role::Requirement) {
        if (node.metadata.id) rows.push_back("ID: " + *node.metadata.id);
        if (node.metadata.text) rows.push_back("Text: " + *node.metadata.text);
        if (node.metadata.risk)
            rows.push_back("Risk: " + std::string(risk_text(*node.metadata.risk)));
        if (node.metadata.verification)
            rows.push_back("Verification: " +
                           std::string(verification_text(*node.metadata.verification)));
    } else {
        if (node.metadata.type) rows.push_back("Type: " + *node.metadata.type);
        if (node.metadata.doc_ref) rows.push_back("Doc Ref: " + *node.metadata.doc_ref);
    }
    return rows;
}

inline void semantic_well_formed(const Scene& scene, Work& work) {
    need(scene.direction == "TB" || scene.direction == "BT" || scene.direction == "LR" ||
             scene.direction == "RL",
         "Requirement11 source direction");
    for (std::size_t at = 0; at < scene.nodes.size(); ++at) {
        const auto& node = scene.nodes[at];
        work.spend(node.ref.name.size() * 8u + 512u);
        need(!node.ref.name.empty(), "Requirement11 nonempty logical name");
        for (std::size_t old = 0; old < at; ++old) {
            work.spend(16u);
            need(scene.nodes[old].ref != node.ref, "Requirement11 role-disjoint first insertion");
        }
        const auto expected = node.ref.role == Role::Requirement
                                  ? requirement_stereotype(node.stereotype)
                                  : node.stereotype == "Element";
        need(expected, "Requirement11 node stereotype role");
        if (node.ref.role == Role::Requirement)
            need(!node.metadata.type && !node.metadata.doc_ref,
                 "Requirement11 requirement metadata domain");
        else
            need(!node.metadata.id && !node.metadata.text && !node.metadata.risk &&
                     !node.metadata.verification,
                 "Requirement11 element metadata domain");
    }
    for (std::size_t n = 0; n < scene.edges.size(); ++n) {
        const auto& edge = scene.edges[n];
        work.spend(edge.syntax.size() * 8u + 256u);
        need(edge.order == static_cast<int>(n) && contains_node(scene, edge.source) &&
                 contains_node(scene, edge.target),
             "Requirement11 ordered resolved endpoint");
    }
}

inline Scene decode_scene(JsonVal value, Work& work) {
    class481_keys(
        value,
        {"ordinal", "path", "source_bytes", "source_sha256", "direction", "nodes", "relations"},
        work);
    Scene out;
    need(value.get("ordinal").is_int() && value.get_int("ordinal") >= 0,
         "Requirement11 model ordinal");
    out.ordinal = static_cast<int>(value.get_int("ordinal"));
    out.path = text(value.get("path"), work);
    out.source_sha = text(value.get("source_sha256"), work, 64u);
    out.direction = text(value.get("direction"), work, 2u);
    const auto nodes = value.get("nodes"), edges = value.get("relations");
    need(nodes.is_arr() && edges.is_arr(), "Requirement11 model arrays");
    nodes.iter([&](JsonVal row) {
        class481_keys(row, {"role", "logical_name", "stereotype", "metadata"}, work);
        const auto meta = row.get("metadata");
        class481_keys(meta, {"id", "text", "risk", "verification", "type", "doc_ref"}, work);
        Node node;
        node.ref = {role(text(row.get("role"), work, 16u)),
                    text(row.get("logical_name"), work, 512u)};
        node.stereotype = text(row.get("stereotype"), work, 64u);
        const auto risk = nullable_text(meta.get("risk"), work),
                   verification = nullable_text(meta.get("verification"), work);
        std::optional<Risk> decoded_risk;
        std::optional<Verification> decoded_verification;
        if (risk) {
            decoded_risk = risk_display(*risk);
            need(decoded_risk.has_value(), "Requirement11 finite Model Risk display");
        }
        if (verification) {
            decoded_verification = verification_display(*verification);
            need(decoded_verification.has_value(),
                 "Requirement11 finite Model Verification display");
        }
        node.metadata = {nullable_text(meta.get("id"), work),
                         nullable_text(meta.get("text"), work),
                         decoded_risk,
                         decoded_verification,
                         nullable_text(meta.get("type"), work),
                         nullable_text(meta.get("doc_ref"), work)};
        out.nodes.push_back(std::move(node));
    });
    edges.iter([&](JsonVal row) {
        class481_keys(row, {"order", "source", "target", "kind", "source_syntax"}, work);
        need(row.get("order").is_int(), "Requirement11 edge order type");
        out.edges.push_back({node_ref(text(row.get("source"), work, 1024u)),
                             node_ref(text(row.get("target"), work, 1024u)),
                             edge_kind(text(row.get("kind"), work, 32u)),
                             static_cast<int>(row.get_int("order")),
                             text(row.get("source_syntax"), work, 1024u)});
    });
    semantic_well_formed(out, work);
    return out;
}

// Binding consumes the complete physical envelope before Model admission.  It
// only accepts the two R2 forms and never searches a successful body for a
// convenient prefix.
inline std::string bind_envelope(JsonVal value, const std::string& source, int ordinal,
                                 Work& work) {
    class481_keys(value,
                  {"ordinal", "path", "source_bytes", "source_sha256", "prefix", "body",
                   "description", "authored_root_keys", "authored_config", "authored_title",
                   "authored_style", "body_graph_model_unchanged"},
                  work);
    need(value.get("ordinal").is_int() && value.get_int("ordinal") == ordinal &&
             value.get("source_bytes").is_int() &&
             static_cast<std::size_t>(value.get_int("source_bytes")) == source.size() &&
             digest(source) == text(value.get("source_sha256"), work, 64u),
         "Requirement11 complete source binding");
    const auto prefix = value.get("prefix"), body = value.get("body"),
               description = value.get("description");
    class481_keys(prefix, {"offset_utf8_bytes", "length_utf8_bytes", "raw_utf8", "sha256"}, work);
    class481_keys(body, {"offset_utf8_bytes", "length_utf8_bytes", "sha256"}, work);
    class481_keys(description,
                  {"form", "physical_offset_utf8_bytes", "physical_length_utf8_bytes",
                   "raw_field_utf8", "raw_field_sha256", "plain_terminal_metadata_value",
                   "rendered"},
                  work);
    need(prefix.get_int("offset_utf8_bytes") == 0 && prefix.get("length_utf8_bytes").is_int() &&
             body.get("offset_utf8_bytes").is_int() && body.get("length_utf8_bytes").is_int(),
         "Requirement11 envelope offsets");
    const auto raw = text(prefix.get("raw_utf8"), work, 4096u);
    const auto at = static_cast<std::size_t>(prefix.get_int("length_utf8_bytes"));
    const auto body_at = static_cast<std::size_t>(body.get_int("offset_utf8_bytes"));
    const auto body_size = static_cast<std::size_t>(body.get_int("length_utf8_bytes"));
    need(at == raw.size() && body_at == at && body_size == source.size() - at &&
             source.substr(0, at) == raw && digest(raw) == text(prefix.get("sha256"), work, 64u) &&
             digest(source.substr(at)) == text(body.get("sha256"), work, 64u),
         "Requirement11 immutable prefix/body partition");
    const auto field = text(description.get("raw_field_utf8"), work, 4096u);
    need(digest(field) == text(description.get("raw_field_sha256"), work, 64u) &&
             source.substr(
                 static_cast<std::size_t>(description.get_int("physical_offset_utf8_bytes")),
                 field.size()) == field,
         "Requirement11 immutable description field");
    const auto form = text(description.get("form"), work, 32u),
               declared = text(description.get("plain_terminal_metadata_value"), work, 4096u);
    std::string projected;
    if (form == "folded-strip") {
        need(field.starts_with("description: >-\n") && field.ends_with('\n'),
             "Requirement11 folded description physical form");
        auto rows = loom::test::mermaid_relations::lines(std::string_view(field).substr(16u));
        if (!rows.empty() && rows.back().empty()) rows.pop_back();
        need(!rows.empty(), "Requirement11 folded description payload");
        for (std::size_t n = 0; n < rows.size(); ++n) {
            need(rows[n].starts_with("  ") && !rows[n].starts_with("   "),
                 "Requirement11 folded description indentation");
            if (n) projected += ' ';
            projected += rows[n].substr(2u);
        }
    } else if (form == "inline-plain") {
        need(field.starts_with("description: ") && field.ends_with('\n') &&
                 field.find('\n') == field.size() - 1u,
             "Requirement11 inline description physical form");
        projected = field.substr(13u, field.size() - 14u);
        need(!projected.empty(), "Requirement11 inline description payload");
    } else
        throw std::runtime_error("Requirement11 description form");
    need(projected == declared && value.get("authored_root_keys").is_arr() &&
             value.get("authored_root_keys").size() == 1 &&
             value.get("authored_root_keys").at(0).as_str() == "description" &&
             value.get("authored_config").is_null() && value.get("authored_title").is_null() &&
             value.get("authored_style").is_null() &&
             value.get("body_graph_model_unchanged").as_bool() &&
             !description.get("rendered").as_bool(),
         "Requirement11 exact inert envelope authority");
    return source.substr(at);
}

inline std::optional<std::string> envelope_body(std::string_view source) {
    if (source.find('\r') != std::string_view::npos || !source.starts_with("---\n"))
        return std::nullopt;
    const auto end = source.find("---\n", 4u);
    if (end == std::string_view::npos) return std::nullopt;
    const auto prefix = source.substr(4u, end - 4u);
    const auto body = source.substr(end + 4u);
    if (body.empty() || !body.starts_with("requirementDiagram\n")) return std::nullopt;
    auto lines = loom::test::mermaid_relations::lines(prefix);
    // The delimiter begins after the prefix's terminating LF; that LF yields
    // one non-content terminal row, which is not a second YAML field.
    if (!lines.empty() && lines.back().empty()) lines.pop_back();
    if (lines.empty()) return std::nullopt;
    if (lines[0].starts_with("description: >-")) {
        if (lines[0] != "description: >-") return std::nullopt;
        for (std::size_t n = 1; n < lines.size(); ++n)
            if (!lines[n].starts_with("  ") || lines[n].starts_with("   ")) return std::nullopt;
    } else if (!lines[0].starts_with("description: ") || lines[0].size() == 13u ||
               lines.size() != 1u)
        return std::nullopt;
    return std::string(body);
}

inline std::optional<Rect> box_for(const Grid& grid, const std::vector<Rect>& boxes,
                                   const Node& node, Work& work) {
    std::optional<Rect> match;
    int count = 0;
    const auto rows = visible_rows(node);
    for (const auto& box : boxes) {
        if (box.shape != "box") continue;
        const auto body = content(grid, box, work);
        if (body.text != rows) continue;
        ++count;
        match = box;
    }
    if (count != 1) return std::nullopt;
    return match;
}

inline bool is_route(const Grid& grid, Point p, int back) {
    return grid.has(p) && (glyph(grid.at(p)).bits & kBits[static_cast<std::size_t>(back)]);
}

// exterior is a Point/move direction index.  The open glyph faces back toward
// the target box, hence it is the opposite of the exterior normal.
inline std::string arrow_for_exterior(int exterior) {
    switch (exterior) {
    case 3:
        return "▷";
    case 1:
        return "◁";
    case 0:
        return "▽";
    case 2:
        return "△";
    }
    throw std::runtime_error("Requirement11 exterior");
}

inline std::vector<Endpoint> endpoints(const Grid& grid, const std::vector<Rect>& boxes,
                                       bool circle, Work& work) {
    std::vector<Endpoint> out;
    for (std::size_t n = 0; n < boxes.size(); ++n) {
        const auto& box = boxes[n];
        for (int y = box.t; y <= box.b; ++y)
            for (int x = box.l; x <= box.r; ++x) {
                const Point border{x, y};
                if (!box.boundary(border)) continue;
                for (int direction = 0; direction < 4; ++direction) {
                    const auto marker = move(border, direction);
                    if (!grid.has(marker)) continue;
                    const auto want = circle ? std::string("⊕") : arrow_for_exterior(direction);
                    if (grid.at(marker) != want) continue;
                    const auto marker_parts = ftxui::Utf8ToGlyphs(std::string(grid.at(marker)));
                    if (marker_parts.size() != 1u ||
                        ftxui::string_width(std::string(grid.at(marker))) != 1)
                        continue;
                    const auto route = move(marker, direction);
                    if (!is_route(grid, route, (direction + 2) % 4)) continue;
                    bool only = true;
                    for (int side = 0; side < 4; ++side) {
                        const auto neighbor = move(marker, side);
                        if (side != direction && grid.has(neighbor) &&
                            (glyph(grid.at(neighbor)).bits &
                             kBits[static_cast<std::size_t>((side + 2) % 4)]))
                            only = false;
                    }
                    if (!only) continue;
                    out.push_back({n, border, marker, route, direction, circle});
                    work.spend(256u);
                }
            }
    }
    return out;
}

inline int route_bits(const Grid& grid, Point p, const std::vector<Rect>& boxes) {
    for (const auto& box : boxes)
        if (box.boundary(p) || box.interior(p)) return 0;
    return glyph(grid.at(p)).bits;
}

inline bool in_rect(Point point, const Rect& rect) {
    return point.x >= rect.l && point.x <= rect.r && point.y >= rect.t && point.y <= rect.b;
}

inline bool contains_index(const std::vector<int>& values, int value) {
    return std::ranges::find(values, value) != values.end();
}

inline std::vector<Point> box_contacts(const Grid& grid, const Rect& box,
                                       const std::vector<Rect>& boxes, Work& work) {
    std::vector<Point> out;
    for (int y = box.t; y <= box.b; ++y)
        for (int x = box.l; x <= box.r; ++x) {
            const Point border{x, y};
            if (!box.boundary(border)) continue;
            for (int direction = 0; direction < 4; ++direction) {
                const auto route = move(border, direction);
                work.spend();
                if (!grid.has(route) || (route_bits(grid, route, boxes) &
                                         kBits[static_cast<std::size_t>((direction + 2) % 4)]) == 0)
                    continue;
                if (std::ranges::find(out, route) == out.end()) out.push_back(route);
            }
        }
    return out;
}

inline bool label_has_route_ink(const Grid& grid, const Label& label) {
    return std::ranges::any_of(label.cells,
                               [&](Point point) { return glyph(grid.at(point)).bits != 0; });
}

inline bool path_touches_label(const Grid& grid, const std::vector<int>& path, const Label& label) {
    for (const auto index : path) {
        const auto point = grid.point(index);
        for (int direction = 0; direction < 4; ++direction) {
            const auto next = move(point, direction);
            if ((glyph(grid.at(point)).bits & kBits[static_cast<std::size_t>(direction)]) != 0 &&
                in_rect(next, label.box))
                return true;
        }
    }
    return false;
}

inline bool point_touches_label(const Grid& grid, Point point, const Label& label) {
    for (int direction = 0; direction < 4; ++direction)
        if ((glyph(grid.at(point)).bits & kBits[static_cast<std::size_t>(direction)]) != 0 &&
            in_rect(move(point, direction), label.box))
            return true;
    return false;
}

inline int path_degree(const Grid& grid, Point point, const std::vector<Rect>& boxes,
                       const Label& label, bool bridge_enabled) {
    int degree = 0;
    for (int direction = 0; direction < 4; ++direction) {
        const auto next = move(point, direction);
        if (grid.has(next) &&
            (route_bits(grid, point, boxes) & kBits[static_cast<std::size_t>(direction)]) &&
            (route_bits(grid, next, boxes) & kBits[static_cast<std::size_t>((direction + 2) % 4)]))
            ++degree;
    }
    if (bridge_enabled && point.y > label.box.t && point.y < label.box.b &&
        (point.x == label.box.l || point.x == label.box.r)) {
        const bool left = point.x == label.box.l;
        const auto other = Point{left ? label.box.r : label.box.l, point.y};
        const int outward = left ? R : L;
        if (grid.has(other) && (route_bits(grid, point, boxes) & outward) &&
            (route_bits(grid, other, boxes) & (left ? L : R)))
            ++degree;
    }
    return degree;
}

struct OwnedPath {
    std::vector<int> cells;
    Point goal{};
    bool bridged = false;
};

// This is a test-private, neutral cell traversal. It recognizes the existing
// label clearance bridge in a captured Grid; it receives no renderer path,
// Canvas ownership, Class, ER or State record.
inline std::size_t owned_path_workspace_fee(std::size_t cells) {
    if (cells > std::numeric_limits<std::size_t>::max() / 4u)
        throw std::runtime_error("Relation oracle work exhausted");
    const auto states = cells * 4u;
    constexpr auto fixed = 2u * sizeof(std::vector<int>) + 4096u, per_state = 2u * sizeof(int);
    if (states > (std::numeric_limits<std::size_t>::max() - fixed) / per_state)
        throw std::runtime_error("Relation oracle work exhausted");
    return states * per_state + fixed;
}

inline std::optional<OwnedPath> owned_path(const Grid& grid, Point start,
                                           const std::vector<Point>& goals,
                                           const std::vector<Rect>& boxes, const Label& label,
                                           Work& work) {
    if (!grid.has(start) || goals.empty() || label_has_route_ink(grid, label)) return std::nullopt;
    const auto cells = grid.cells.size();
    // parent and the full BFS queue both own four state slots per actual Grid
    // cell. Charge their capacities before either vector is constructed.
    work.spend(owned_path_workspace_fee(cells));
    const auto states = cells * 4u;
    // Bit 0 records the one permitted clearance bridge. Bit 1 records that
    // the walk has physically reached this relation's label clearance; a
    // label may lie in the middle of a route, rather than at its endpoint.
    std::vector<int> parent(states, -2), queue;
    queue.reserve(states);
    const auto slot = [&](Point point, int flags) {
        return grid.index(point) + flags * static_cast<int>(cells);
    };
    const int start_flags = point_touches_label(grid, start, label) ? 2 : 0;
    parent[static_cast<std::size_t>(slot(start, start_flags))] = -1;
    queue.push_back(slot(start, start_flags));
    int goal_state = -1;
    for (std::size_t cursor = 0; cursor < queue.size() && goal_state < 0; ++cursor) {
        const int state = queue[cursor], flags = state / static_cast<int>(cells);
        const bool bridged = (flags & 1) != 0;
        const auto point = grid.point(state % static_cast<int>(cells));
        if (std::ranges::find(goals, point) != goals.end() && (flags & 2) != 0) {
            goal_state = state;
            break;
        }
        for (int direction = 0; direction < 4; ++direction) {
            const auto next = move(point, direction);
            work.spend();
            if (!grid.has(next) ||
                (route_bits(grid, point, boxes) & kBits[static_cast<std::size_t>(direction)]) ==
                    0 ||
                (route_bits(grid, next, boxes) &
                 kBits[static_cast<std::size_t>((direction + 2) % 4)]) == 0)
                continue;
            const auto next_flags = flags | (point_touches_label(grid, next, label) ? 2 : 0);
            const auto next_state = slot(next, next_flags);
            if (parent[static_cast<std::size_t>(next_state)] == -2) {
                parent[static_cast<std::size_t>(next_state)] = state;
                queue.push_back(next_state);
            }
        }
        if (!bridged && point.y > label.box.t && point.y < label.box.b &&
            (point.x == label.box.l || point.x == label.box.r)) {
            const bool left = point.x == label.box.l;
            const auto next = Point{left ? label.box.r : label.box.l, point.y};
            const int outward = left ? R : L;
            const auto next_state = slot(next, flags | 3);
            if (grid.has(next) && (route_bits(grid, point, boxes) & outward) &&
                (route_bits(grid, next, boxes) & (left ? L : R)) &&
                parent[static_cast<std::size_t>(next_state)] == -2) {
                parent[static_cast<std::size_t>(next_state)] = state;
                queue.push_back(next_state);
            }
        }
    }
    if (goal_state < 0) return std::nullopt;
    std::size_t result_count = 0u;
    for (int state = goal_state; state >= 0; state = parent[static_cast<std::size_t>(state)]) {
        work.spend();
        if (++result_count > states)
            throw std::runtime_error("Requirement11 owned path parent cycle");
    }
    constexpr auto result_fixed = sizeof(OwnedPath) + sizeof(std::vector<int>) + 256u;
    if (result_count > (std::numeric_limits<std::size_t>::max() - result_fixed) / sizeof(int))
        throw std::runtime_error("Relation oracle work exhausted");
    // The returned witness has no growth allocation: count first, charge its
    // exact payload, then reserve before pushing the parent chain.
    work.spend(result_count * sizeof(int) + result_fixed);
    OwnedPath result;
    result.cells.reserve(result_count);
    result.goal = grid.point(goal_state % static_cast<int>(cells));
    result.bridged = (goal_state / static_cast<int>(cells) & 1) != 0;
    for (int state = goal_state; state >= 0; state = parent[static_cast<std::size_t>(state)])
        result.cells.push_back(state % static_cast<int>(cells));
    std::reverse(result.cells.begin(), result.cells.end());
    if (!result.bridged && !path_touches_label(grid, result.cells, label)) return std::nullopt;
    for (std::size_t at = 0; at < result.cells.size(); ++at) {
        const auto point = grid.point(result.cells[at]);
        const int wanted = (at == 0u || at + 1u == result.cells.size()) ? 1 : 2;
        if (path_degree(grid, point, boxes, label, result.bridged) != wanted) return std::nullopt;
    }
    return result;
}

// A relation's stroke style is witnessed only on an interior physical segment
// whose two consumed neighbors are opposite.  Corners, joins and label jumps
// have the renderer's light glyphs, but they do not certify a solid route.
inline bool expected_straight_stroke(const Grid& grid, const std::vector<int>& path, int expected,
                                     Work& work) {
    bool witnessed = false;
    const auto direction = [](Point from, Point to) {
        if (from.x == to.x && from.y != to.y) return to.y < from.y ? 0 : 2;
        if (from.y == to.y && from.x != to.x) return to.x < from.x ? 3 : 1;
        return -1;
    };
    for (std::size_t at = 0u; at < path.size(); ++at) {
        work.spend();
        const auto point = grid.point(path[at]);
        const auto visible = glyph(grid.at(point));
        // Marker-adjacent/source-adjacent endpoints still own their visible
        // straight style even when they cannot provide an interior witness.
        if ((visible.bits == (L | R) || visible.bits == (U | D)) && visible.stroke != expected)
            return false;
        if (at == 0u || at + 1u == path.size()) continue;
        const auto previous = grid.point(path[at - 1u]), next = grid.point(path[at + 1u]);
        const int backward = direction(point, previous), forward = direction(point, next);
        if (backward < 0 || forward < 0) continue;
        const int used =
            kBits[static_cast<std::size_t>(backward)] | kBits[static_cast<std::size_t>(forward)];
        if (used != (L | R) && used != (U | D)) continue;
        const auto actual = glyph(grid.at(point));
        if (actual.bits != used || actual.stroke != expected) return false;
        witnessed = true;
    }
    return witnessed;
}

inline bool path_has_only_declared_contact(const Grid& grid, const std::vector<int>& path,
                                           const std::vector<Rect>& boxes, int declared,
                                           Work& work) {
    for (const auto index : path) {
        const auto point = grid.point(index);
        for (std::size_t n = 0; n < boxes.size(); ++n)
            for (int direction = 0; direction < 4; ++direction) {
                work.spend();
                const auto border = move(point, direction);
                if (!boxes[n].boundary(border) ||
                    (glyph(grid.at(point)).bits & kBits[static_cast<std::size_t>(direction)]) == 0)
                    continue;
                if (static_cast<int>(n) != declared) return false;
            }
    }
    return true;
}

inline void claim(std::vector<bool>& used, const Grid& grid, Point p) {
    if (grid.has(p)) used[static_cast<std::size_t>(grid.index(p))] = true;
}

inline std::vector<std::string> geometry(const Grid& grid, const Scene& scene, Work& work,
                                         Geometry* evidence = nullptr) {
    std::vector<std::string> errors;
    auto fail = [&](std::string_view text) {
        if (errors.size() < 16u) errors.emplace_back(text);
    };
    semantic_well_formed(scene, work);
    if (grid.width <= 0 || grid.width > 240 || grid.height <= 0 || grid.height > 1040 ||
        grid.cells.size() != static_cast<std::size_t>(grid.width * grid.height)) {
        fail("Requirement11 actual Grid bounds");
        return errors;
    }
    const auto all = outlines(grid, work);
    std::vector<Rect> boxes;
    boxes.reserve(scene.nodes.size());
    for (const auto& node : scene.nodes) {
        const auto found = box_for(grid, all, node, work);
        if (!found) {
            fail("Requirement11 exact role/name/metadata box");
            continue;
        }
        boxes.push_back(*found);
    }
    if (boxes.size() != scene.nodes.size()) return errors;
    auto circles = endpoints(grid, boxes, true, work), arrows = endpoints(grid, boxes, false, work);
    std::size_t contains = 0;
    for (const auto& edge : scene.edges)
        contains += edge.kind == EdgeKind::Contains;
    if (circles.size() != contains || arrows.size() != scene.edges.size() - contains)
        fail("Requirement11 exact marker cardinality");
    std::vector<Label> labels;
    for (const auto kind :
         {EdgeKind::Contains, EdgeKind::Copies, EdgeKind::Derives, EdgeKind::Satisfies,
          EdgeKind::Verifies, EdgeKind::Refines, EdgeKind::Traces}) {
        const auto found = label_blocks(grid, edge_label(kind), work);
        labels.insert(labels.end(), found.begin(), found.end());
    }
    std::vector<bool> claimed(grid.cells.size());
    for (const auto& box : boxes)
        for (int y = box.t; y <= box.b; ++y)
            for (int x = box.l; x <= box.r; ++x)
                claim(claimed, grid, {x, y});
    std::vector<bool> used_circle(circles.size()), used_arrow(arrows.size()),
        used_label(labels.size()), used_path(grid.cells.size());
    std::vector<std::vector<int>> paths;
    std::vector<EdgeWitness> witnesses;
    auto node_index = [&](const NodeRef& ref) {
        for (std::size_t n = 0; n < scene.nodes.size(); ++n)
            if (scene.nodes[n].ref == ref) return static_cast<int>(n);
        return -1;
    };
    for (const auto& edge : scene.edges) {
        const auto source = node_index(edge.source), target = node_index(edge.target);
        if (source < 0 || target < 0) {
            fail("Requirement11 resolved node lookup");
            continue;
        }
        const bool contains_edge = edge.kind == EdgeKind::Contains;
        auto& endpoints_ = contains_edge ? circles : arrows;
        auto& used = contains_edge ? used_circle : used_arrow;
        const int marker_node = contains_edge ? source : target;
        const int opposite = contains_edge ? target : source;
        int found = -1, label_index = -1;
        bool candidate_marker = false;
        std::optional<OwnedPath> selected;
        const auto goals =
            box_contacts(grid, boxes[static_cast<std::size_t>(opposite)], boxes, work);
        // A node may own several incoming markers of different relation kinds.
        // Choose the marker only after its complete source/path/label witness.
        for (std::size_t m = 0; m < endpoints_.size() && !selected; ++m) {
            work.spend();
            if (used[m] || static_cast<int>(endpoints_[m].node) != marker_node) continue;
            candidate_marker = true;
            const auto& marker = endpoints_[m];
            for (std::size_t n = 0; n < labels.size(); ++n) {
                work.spend();
                if (used_label[n] || labels[n].text != edge_label(edge.kind)) continue;
                auto candidate = owned_path(grid, marker.route, goals, boxes, labels[n], work);
                if (!candidate) continue;
                bool exclusive = true;
                for (const auto cell : candidate->cells)
                    if (used_path[static_cast<std::size_t>(cell)]) exclusive = false;
                if (!exclusive) continue;
                for (const auto& other : circles)
                    if (other.marker != marker.marker &&
                        contains_index(candidate->cells, grid.index(other.route)))
                        exclusive = false;
                for (const auto& other : arrows)
                    if (other.marker != marker.marker &&
                        contains_index(candidate->cells, grid.index(other.route)))
                        exclusive = false;
                for (std::size_t other = 0; other < labels.size(); ++other)
                    if (other != n && path_touches_label(grid, candidate->cells, labels[other]))
                        exclusive = false;
                if (!exclusive ||
                    !path_has_only_declared_contact(grid, candidate->cells, boxes, opposite, work))
                    continue;
                found = static_cast<int>(m);
                label_index = static_cast<int>(n);
                selected = std::move(candidate);
                break;
            }
        }
        if (!candidate_marker) {
            fail("Requirement11 source circle or target arrow role");
            continue;
        }
        if (!selected) {
            fail("Requirement11 per-edge label/route/port ownership");
            continue;
        }
        const auto& marker = endpoints_[static_cast<std::size_t>(found)];
        used[static_cast<std::size_t>(found)] = true;
        used_label[static_cast<std::size_t>(label_index)] = true;
        for (const auto cell : selected->cells) {
            const auto point = grid.point(cell);
            used_path[static_cast<std::size_t>(cell)] = true;
            claim(claimed, grid, point);
        }
        const bool stroke =
            expected_straight_stroke(grid, selected->cells, contains_edge ? 0 : 1, work);
        for (const auto point : labels[static_cast<std::size_t>(label_index)].cells)
            claim(claimed, grid, point);
        claim(claimed, grid, marker.marker);
        claim(claimed, grid, marker.route);
        if (!stroke) {
            fail("Requirement11 solid contains or dashed noncontains route");
            continue;
        }
        const Point source_contact = contains_edge ? marker.route : selected->goal,
                    target_contact = contains_edge ? selected->goal : marker.route;
        witnesses.push_back({marker, source_contact, target_contact,
                             labels[static_cast<std::size_t>(label_index)], selected->cells});
        paths.push_back(selected->cells);
    }
    if (std::ranges::any_of(used_circle, [](bool used) { return !used; }) ||
        std::ranges::any_of(used_arrow, [](bool used) { return !used; }) ||
        std::ranges::any_of(used_label, [](bool used) { return !used; }))
        fail("Requirement11 no unowned marker or relation label");
    const auto following = loom::test::mermaid_relations::following_row(grid, work, kFollowing);
    if (!following)
        fail("Requirement11 independent physical following token");
    else
        for (int x = 0; x < grid.width; ++x)
            claim(claimed, grid, {x, *following});
    for (std::size_t n = 0; n < grid.cells.size(); ++n)
        if (grid.cells[n] != " " && !claimed[n]) {
            fail("Requirement11 unclaimed terminal ink");
            break;
        }
    if (evidence) *evidence = {boxes, circles, arrows, labels, claimed, paths, witnesses};
    return errors;
}

inline bool first_blank(const Grid& grid, const Geometry& evidence, Point& out) {
    for (std::size_t n = 0; n < grid.cells.size(); ++n)
        if (grid.cells[n] == " " && !evidence.claimed[n]) {
            out = grid.point(static_cast<int>(n));
            return true;
        }
    return false;
}

inline bool relocate_label_off_route(Grid& grid, const Geometry& evidence, const Label& label,
                                     Work& work) {
    const int width = label.box.r - label.box.l + 1, height = label.box.b - label.box.t + 1;
    for (int y = 0; y + height <= grid.height; ++y) {
        for (int x = 0; x + width <= grid.width; ++x) {
            bool blank = true;
            for (int dy = 0; dy < height && blank; ++dy) {
                for (int dx = 0; dx < width; ++dx) {
                    const Point point{x + dx, y + dy};
                    work.spend();
                    if (grid.at(point) != " " ||
                        evidence.claimed[static_cast<std::size_t>(grid.index(point))]) {
                        blank = false;
                        break;
                    }
                }
            }
            if (!blank) continue;
            for (const auto point : label.cells) {
                const Point target{x + point.x - label.box.l, y + point.y - label.box.t};
                grid.put(target, std::string(grid.at(point)));
                grid.put(point, " ");
            }
            return true;
        }
    }
    return false;
}

inline int scene_node(const Scene& scene, const NodeRef& reference) {
    for (std::size_t n = 0; n < scene.nodes.size(); ++n)
        if (scene.nodes[n].ref == reference) return static_cast<int>(n);
    return -1;
}

inline int target_exterior(std::string_view direction) {
    if (direction == "LR") return 3;
    if (direction == "RL") return 1;
    if (direction == "TB") return 0;
    if (direction == "BT") return 2;
    throw std::runtime_error("Requirement11 target direction");
}

inline void directional_rank(const Grid& grid, const Scene& scene, const Geometry& evidence,
                             Work& work) {
    need(scene.edges.size() == 1u && evidence.edges.size() == 1u,
         "Requirement11 one-edge directional control");
    const auto& edge = scene.edges.front();
    const auto source = scene_node(scene, edge.source), target = scene_node(scene, edge.target);
    need(source >= 0 && target >= 0, "Requirement11 directional source target");
    const auto& a = evidence.boxes[static_cast<std::size_t>(source)];
    const auto& b = evidence.boxes[static_cast<std::size_t>(target)];
    work.spend(128u);
    if (scene.direction == "LR")
        need(a.r < b.l, "Requirement11 LR source target rank");
    else if (scene.direction == "RL")
        need(a.l > b.r, "Requirement11 RL source target rank");
    else if (scene.direction == "TB")
        need(a.b < b.t, "Requirement11 TB source target rank");
    else if (scene.direction == "BT")
        need(a.t > b.b, "Requirement11 BT source target rank");
    else
        need(false, "Requirement11 directional control domain");
    const auto& witness = evidence.edges.front();
    need(!witness.marker.circle && static_cast<int>(witness.marker.node) == target &&
             witness.marker.exterior == target_exterior(scene.direction),
         "Requirement11 target exterior normal");
    need(!witness.path.empty() && grid.point(witness.path.front()) == witness.marker.route &&
             witness.target_contact == witness.marker.route,
         "Requirement11 target final route port");
    if (witness.path.size() > 1u)
        need(grid.point(witness.path[1]) == move(witness.marker.route, witness.marker.exterior),
             "Requirement11 target final route normal");
}

inline void require_rejected(const Grid& good, const Scene& scene, auto mutate, Work& work,
                             std::string_view name) {
    Geometry baseline;
    const auto before = geometry(good, scene, work, &baseline);
    need(before.empty(), "Requirement11 good-first baseline");
    work.spend(good.cells.size() * (2u * sizeof(std::string) + 8u) + 4096u);
    auto bad = good;
    mutate(bad, baseline);
    const auto rejected = geometry(bad, scene, work);
    need(!rejected.empty(), name);
}

inline void require_model_rejected(const Grid& good, const Scene& scene, auto mutate, Work& work,
                                   std::string_view name) {
    Geometry baseline;
    const auto before = geometry(good, scene, work, &baseline);
    need(before.empty(), "Requirement11 good-first Model baseline");
    work.spend(scene.nodes.size() * (2u * sizeof(Node) + 8u * sizeof(std::string)) +
               scene.edges.size() * (2u * sizeof(Edge) + 8u * sizeof(std::string)) + 4096u);
    auto bad = scene;
    mutate(bad);
    const auto rejected = geometry(good, bad, work);
    need(!rejected.empty(), name);
}

inline void require_semantic_rejected(const Scene& good, auto mutate, Work& work,
                                      std::string_view name) {
    auto bad = good;
    mutate(bad);
    bool rejected = false;
    try {
        semantic_well_formed(bad, work);
    } catch (const std::exception& error) {
        if (class481_resource(error.what())) throw;
        rejected = true;
    }
    need(rejected, name);
}

inline std::string fenced(std::string_view source, std::string_view language) {
    return "```" + std::string(language) + '\n' + std::string(source) +
           (source.ends_with('\n') ? "" : "\n") + "```\n\n" + std::string(kFollowing);
}

inline constexpr std::size_t kObserveMaxCells = 3u * 240u * 1024u + 2u * 40u * 1024u;

inline std::size_t observe_prepay_fee(std::size_t source_bytes) {
    // Two fenced Markdown strings/elements are built before a height exists.
    // The fixed cell term is five-capture request/Pixel traversal staging at
    // the public 240x1024/40x1024 maxima. note13_capture still pays each
    // actual Screen, Grid and UTF-8-cell allocation on this same ledger.
    constexpr auto source_unit = 32u * sizeof(std::string) + 256u;
    constexpr auto capture_cell_unit = sizeof(std::size_t) + sizeof(bool) + 8u;
    constexpr auto fixed = kObserveMaxCells * capture_cell_unit + 2u * sizeof(ftxui::Element) +
                           sizeof(Frames) + 5u * (4u * sizeof(std::size_t) + 64u) + 8192u;
    if (source_bytes > std::numeric_limits<std::size_t>::max() - 128u)
        throw std::runtime_error("Relation oracle work exhausted");
    const auto fenced_bytes = source_bytes + 128u;
    if (fenced_bytes > (std::numeric_limits<std::size_t>::max() - fixed) / (2u * source_unit))
        throw std::runtime_error("Relation oracle work exhausted");
    return fixed + 2u * fenced_bytes * source_unit;
}

inline Frames observe(std::string_view source, Work& work) {
    work.spend(observe_prepay_fee(source.size()));
    const auto drawing_fence = fenced(source, "mermaid"), ordinary_fence = fenced(source, "text");
    auto drawing = loom::ui::render_markdown(drawing_fence);
    auto ordinary = loom::ui::render_markdown(ordinary_fence);
    drawing->ComputeRequirement();
    ordinary->ComputeRequirement();
    const int height = std::min(
        1024,
        std::max(32, std::max(drawing->requirement().min_y, ordinary->requirement().min_y) + 12));
    work.spend(static_cast<std::size_t>(height) * (3u * 240u + 2u * 40u) * 4u + 65536u);
    return {loom::test::mermaid_relations::note13_capture(ordinary, 240, height, work),
            loom::test::mermaid_relations::note13_capture(ordinary, 40, height, work),
            loom::test::mermaid_relations::note13_capture(drawing, 240, height, work),
            loom::test::mermaid_relations::note13_capture(drawing, 40, height, work),
            loom::test::mermaid_relations::note13_capture(drawing, 240, height, work)};
}

inline void publish(const Frames& frames, json::JsonMutVal root, json::JsonMutVal item,
                    Work& work) {
    official285_publish_frame(frames.source240, "source_control_240", root, item, work);
    official285_publish_frame(frames.source40, "source_control_40", root, item, work);
    official285_publish_frame(frames.wide, "wide_240", root, item, work);
    official285_publish_frame(frames.narrow, "narrow_40", root, item, work);
    official285_publish_frame(frames.restored, "restored_240", root, item, work);
}

inline void lifecycle(const Frames& frames, Work& work) {
    need(loom::test::mermaid_relations::following_row(frames.source240, work, kFollowing)
                 .has_value() &&
             loom::test::mermaid_relations::following_row(frames.source40, work, kFollowing)
                 .has_value() &&
             loom::test::mermaid_relations::following_row(frames.wide, work, kFollowing)
                 .has_value() &&
             loom::test::mermaid_relations::following_row(frames.narrow, work, kFollowing)
                 .has_value() &&
             loom::test::mermaid_relations::following_row(frames.restored, work, kFollowing)
                 .has_value(),
         "Requirement11 five physical following captures");
    need(frames.wide == frames.restored, "Requirement11 same Markdown Node 240/40/240 restore");
    if (frames.wide == frames.source240)
        need(frames.narrow == frames.source40 && frames.restored == frames.source240,
             "Requirement11 source fallback preserves ordinary lifecycle");
}

inline void require_lifecycle_rejected(const Frames& good, auto mutate, Work& work,
                                       std::string_view name) {
    auto bad = good;
    mutate(bad);
    bool rejected = false;
    try {
        lifecycle(bad, work);
    } catch (const std::exception& error) {
        if (class481_resource(error.what())) throw;
        rejected = true;
    }
    need(rejected, name);
}

}  // namespace requirement11_native
