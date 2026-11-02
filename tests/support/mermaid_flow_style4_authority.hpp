// Test-private complete original/body source authority. No producer AST is used.
#pragma once
#include "mermaid_flow_group_endpoint15_oracle.hpp"

namespace loom::test::mermaid_flow::style4 {
using Work = OracleWork;
inline constexpr std::size_t kAssetBytes = 31764u;
inline constexpr std::string_view kAssetSha =
    "37da945aa0de4196a73ae1b3d46af9d5a71a181abf9295585464c5224fb559f5";
enum class Domain { Original, BodyDiagnostic };

inline std::string_view contract(Domain domain) {
    return domain == Domain::Original ? "FlowStyle4OriginalWholeScreen/v1"
                                      : "FlowStyle4BodyDiagnosticWholeScreen/v1";
}

struct Pin {
    std::size_t ordinal, global, source_bytes, front_bytes, body_bytes, model_bytes,
        body_model_bytes;
    std::string_view fixture, source_sha, front_sha, body_sha, model_sha, body_model_sha, git_blob;
};

inline constexpr std::array<Pin, 4> kPins{{
    {45u, 706u, 440u, 308u, 132u, 1663u, 1573u,
     "flowchart/nightly-check/dagre/65-dagre-text-color-from-classes.mmd",
     "c2997bcd6ff431525895864dbf41109d008b8fd423ebb58ce0c2cd2a282762a1",
     "73d463ac5afcb08bb1363315a1e4416a9f4239e6ba56ec6223b2253a1da8f0a1",
     "1d42be3a7ee0d6fff605178bfb7462eb3bbeaeb83a1ffeef847cd1d534c6c668",
     "517bb0b2aca29ba33d35485920ac327da5733695cae93318ad270f532f6ed17e",
     "2992866cba63a6cc8ec9653d0196552b18d9a84cb460cf8ae2f50a687edae82d",
     "0a9b81a24e163b937df625fc4face60109a8bb69"},
    {126u, 787u, 435u, 303u, 132u, 1665u, 1573u,
     "flowchart/nightly-check/handdrawn/text-color-from-classes.mmd",
     "55e3d5fa087ae1584c2ad696372a9c02d56a4b1fb74d72a7f1167bf6b669c7d8",
     "73b2de40aff422fa3a3a6f83684e64638e30cf7bd512427a2edf2da7de926fb0",
     "1d42be3a7ee0d6fff605178bfb7462eb3bbeaeb83a1ffeef847cd1d534c6c668",
     "79840c42a75b42d230127b6e7cbb07617396718853da6ea07df11adedfbc55d6",
     "2992866cba63a6cc8ec9653d0196552b18d9a84cb460cf8ae2f50a687edae82d",
     "b39b743e9bae1a94ccc43c9d0d1b310b320d5ae7"},
    {246u, 907u, 424u, 237u, 187u, 1671u, 1671u, "flowchart/pr-check/4-classdef-style.mmd",
     "1c946035a650b161f4f2025cdd1398dd3b3e313a8712c2db1f87867e0f5f097b",
     "a8cb2dd4a4f122333f25296a2fdea5cc6152017907e196bdd5a4e7288b17cca0",
     "89aacce4adf6069ebba25162e24dbe9e050777140e4f9b8f4a22e29684789542",
     "bb7ca82d840453073f775a7cfc835446654c89120f487e68d46876b0b3072e42",
     "bb7ca82d840453073f775a7cfc835446654c89120f487e68d46876b0b3072e42",
     "5a7a69dfe229930e66eeea02de0c1f4cc710a776"},
    {325u, 986u, 455u, 323u, 132u, 1646u, 1573u,
     "flowchart/visual-check/65-1-text-color-from-classes.mmd",
     "51ac937335dd4d17d2fa20c8f12dfd295e6890aef74f6ac913cdc451db3c35de",
     "138390f00aed8c5c271995f513cb6edba21122d0374f22da1f003913e04f74d7",
     "1d42be3a7ee0d6fff605178bfb7462eb3bbeaeb83a1ffeef847cd1d534c6c668",
     "adf13c05a33a6d5e53829aa2feed8aae138113722db1dfe410efdf5bb71fc963",
     "2992866cba63a6cc8ec9653d0196552b18d9a84cb460cf8ae2f50a687edae82d",
     "a8903894273c3ef6f8c087ad0744b060b9dd3fe5"},
}};

inline void require(bool good, std::string_view why) {
    if (!good) throw std::runtime_error(std::string(why));
}

inline std::string hash(std::string_view value) {
    return loom::utils::crypto::sha256(value);
}

inline std::string_view trim_record(std::string_view value) {
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '\t' || value.front() == '\r'))
        value.remove_prefix(1u);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
        value.remove_suffix(1u);
    return value;
}

inline void keys(JsonVal value, std::initializer_list<std::string_view> names, Work& work) {
    require(value.is_obj() && value.size() <= 24u, "FlowStyle4 bounded object");
    work.spend(value.size() * 512u + 1024u);
    flow400_keys(value, names);
}

inline std::string_view text(JsonVal value, Work& work, std::size_t cap = 1024u) {
    require(value.is_str() && value.as_str().size() <= cap, "FlowStyle4 bounded text");
    work.spend(value.as_str().size() * 8u + 64u);
    return value.as_str();
}

struct Documents {
    std::string raw;
    json::JsonDoc document;
};

inline Documents load(Work& work) {
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) /
                      "fixtures/mermaid_native/flow_style4_source_models.proposal.v1.json";
    require(std::filesystem::file_size(path) == kAssetBytes, "FlowStyle4 immutable asset size");
    work.spend(kAssetBytes * 24u + 65536u);
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "FlowStyle4 asset read");
    std::string raw(kAssetBytes, '\0');
    file.read(raw.data(), static_cast<std::streamsize>(raw.size()));
    require(file.gcount() == static_cast<std::streamsize>(kAssetBytes) &&
                file.peek() == std::char_traits<char>::eof() && hash(raw) == kAssetSha,
            "FlowStyle4 exact bounded asset bytes/hash");
    auto parsed = json::parse(raw);
    require(parsed.has_value(), "FlowStyle4 JSON");
    const auto root = parsed->root();
    keys(root, {"schema", "status", "authority", "count", "cases"}, work);
    require(root.get_string("schema") == "flow_style4_source_models.proposal/v1" &&
                root.get_string("status") ==
                    "proposal_requires_independent_source_review_no_admission" &&
                root.get_string("authority") ==
                    "independently_authored_complete_sources_and_separate_complete_body_"
                    "diagnostics_not_renderer_output" &&
                root.get("count").is_int() && root.get_int("count") == 4 &&
                root.get("cases").is_arr() && root.get("cases").size() == 4u,
            "FlowStyle4 closed source authority");
    return {std::move(raw), std::move(*parsed)};
}
enum class Fill { None, Neutral, Orange, Green };

struct NodeStyle {
    Fill fill = Fill::None;
    bool stroke = false, bold = false;
};

struct Expected {
    std::size_t selected = 0u;
    Domain domain = Domain::Original;
    Model graph;
    std::array<NodeStyle, 3> styles;
    std::vector<std::string> raw_records;
};

inline void decode_config(JsonVal value, std::size_t selected, Domain domain, Work& work) {
    require(selected < 4u, "FlowStyle4 configuration domain");
    if (domain == Domain::BodyDiagnostic || selected == 2u) {
        keys(value, {}, work);
        return;
    }
    if (selected == 0u)
        keys(value, {"htmlLabels", "flowchart", "securityLevel", "layout"}, work);
    else if (selected == 1u)
        keys(value, {"htmlLabels", "flowchart", "securityLevel", "look"}, work);
    else
        keys(value, {"htmlLabels", "flowchart", "securityLevel"}, work);
    require(value.get("htmlLabels").is_bool() && value.get("htmlLabels").as_bool() &&
                value.get("securityLevel").is_str() && value.get_string("securityLevel") == "loose",
            "FlowStyle4 exact root HTML/security");
    const auto nested = value.get("flowchart");
    keys(nested, {"htmlLabels"}, work);
    require(nested.get("htmlLabels").is_bool() && nested.get("htmlLabels").as_bool(),
            "FlowStyle4 exact nested HTML");
    if (selected == 0u)
        require(value.get("layout").is_str() && value.get_string("layout") == "dagre",
                "FlowStyle4 exact layout");
    if (selected == 1u)
        require(value.get("look").is_str() && value.get_string("look") == "handDrawn",
                "FlowStyle4 exact look");
}

inline void property_list(JsonVal value, std::string_view fill, std::string_view stroke,
                          std::string_view width, bool color, Work& work) {
    require(value.is_arr() && value.size() == (color ? 4u : 3u), "FlowStyle4 complete properties");
    const std::array<std::pair<std::string_view, std::string_view>, 4> wanted{
        {{"fill", fill}, {"stroke", stroke}, {"stroke-width", width}, {"color", "#fff"}}};
    for (std::size_t n = 0; n < value.size(); ++n) {
        const auto item = value.at(n);
        keys(item, {"name", "value"}, work);
        require(text(item.get("name"), work) == wanted[n].first &&
                    text(item.get("value"), work) == wanted[n].second,
                "FlowStyle4 exact source property");
    }
}

inline Expected decode_model(JsonVal model, std::size_t selected, Domain domain, Work& work) {
    require(selected < 4u && (domain == Domain::Original || domain == Domain::BodyDiagnostic),
            "FlowStyle4 closed source domain");
    const bool small = selected == 2u;
    keys(model,
         {"family", "header", "direction", "title", "accessibility_title",
          "accessibility_description", "nodes", "edges", "groups", "frontmatter_config",
          "style_definitions", "style_assignments", "direct_styles", "effective_styles"},
         work);
    require(text(model.get("family"), work) == "flow" &&
                text(model.get("header"), work) == "flowchart" &&
                text(model.get("direction"), work) == (small ? "TD" : "LR") &&
                model.get("title").is_null() && model.get("accessibility_title").is_null() &&
                model.get("accessibility_description").is_null(),
            "FlowStyle4 complete header/defaults");
    decode_config(model.get("frontmatter_config"), selected, domain, work);
    Expected result;
    result.selected = selected;
    result.domain = domain;
    result.graph.direction = small ? "TD" : "LR";
    const std::array<std::string_view, 3> ids =
        small ? std::array<std::string_view, 3>{{"A", "B", "C"}}
              : std::array<std::string_view, 3>{{"Lorem", "Ipsum", "Dolor"}};
    const std::array<std::string_view, 3> labels =
        small ? std::array<std::string_view, 3>{{"Start", "Process", "End"}} : ids;
    const auto nodes = model.get("nodes"), edges = model.get("edges");
    require(nodes.is_arr() && nodes.size() == 3u && edges.is_arr() && edges.size() == 2u &&
                model.get("groups").is_arr() && model.get("groups").size() == 0u,
            "FlowStyle4 complete three-node/two-edge graph");
    work.spend(32768u);
    result.graph.nodes.reserve(3u);
    result.graph.edges.reserve(2u);
    for (std::size_t n = 0; n < 3u; ++n) {
        const auto node = nodes.at(n);
        keys(node, {"id", "label", "shape", "owner"}, work);
        require(text(node.get("id"), work) == ids[n] &&
                    text(node.get("label"), work) == labels[n] &&
                    text(node.get("shape"), work) == "rectangle" && node.get("owner").is_null(),
                "FlowStyle4 all root node semantics");
        result.graph.nodes.push_back(
            {std::string(ids[n]), std::string(labels[n]), Shape::Rectangle});
    }
    for (std::size_t n = 0; n < 2u; ++n) {
        const auto edge = edges.at(n);
        keys(edge, {"from", "to", "label", "style", "start", "end", "min_length"}, work);
        require(
            text(edge.get("from"), work) == ids[n] && text(edge.get("to"), work) == ids[n + 1u] &&
                text(edge.get("label"), work).empty() && text(edge.get("style"), work) == "solid" &&
                edge.get("start").is_bool() && !edge.get("start").as_bool() &&
                edge.get("end").is_bool() && edge.get("end").as_bool() &&
                edge.get("min_length").is_int() && edge.get_int("min_length") == 1,
            "FlowStyle4 every directed occurrence");
        result.graph.edges.push_back(
            {static_cast<int>(n), static_cast<int>(n + 1u), "", "solid", false, true});
    }
    const auto definitions = model.get("style_definitions"),
               assignments = model.get("style_assignments"), direct = model.get("direct_styles"),
               effective = model.get("effective_styles");
    require(definitions.is_arr() && definitions.size() == 1u && assignments.is_arr() &&
                assignments.size() == 1u && direct.is_arr() && direct.size() == (small ? 1u : 0u) &&
                effective.is_arr() && effective.size() == 2u,
            "FlowStyle4 complete style cardinalities");
    const auto definition = definitions.at(0u), assignment = assignments.at(0u);
    keys(definition, {"name", "verbatim", "properties"}, work);
    keys(assignment, {"targets", "class_name", "verbatim", "assignment_order"}, work);
    const auto name = small ? std::string_view("highlight") : std::string_view("dark");
    require(text(definition.get("name"), work) == name &&
                text(assignment.get("class_name"), work) == name &&
                assignment.get("assignment_order").is_int() &&
                assignment.get_int("assignment_order") == 0,
            "FlowStyle4 class identity/order");
    require(text(definition.get("verbatim"), work) ==
                    (small ? "classDef highlight fill:#f96,stroke:#333,stroke-width:2px;"
                           : "classDef dark fill:#000,stroke:#000,stroke-width:4px,color:#fff") &&
                text(assignment.get("verbatim"), work) ==
                    (small ? "class B highlight;" : "class Lorem,Dolor dark"),
            "FlowStyle4 complete logical declarations");
    property_list(definition.get("properties"), small ? "#f96" : "#000", small ? "#333" : "#000",
                  small ? "2px" : "4px", !small, work);
    const auto targets = assignment.get("targets");
    require(targets.is_arr() && targets.size() == (small ? 1u : 2u), "FlowStyle4 target list");
    for (std::size_t n = 0; n < targets.size(); ++n)
        require(text(targets.at(n), work) == (small ? ids[1u] : ids[n * 2u]),
                "FlowStyle4 exact ordered ID targets");
    if (small) {
        const auto statement = direct.at(0u);
        keys(statement, {"target", "verbatim", "properties"}, work);
        require(text(statement.get("target"), work) == "C" &&
                    text(statement.get("verbatim"), work) ==
                        "style C fill:#9f9,stroke:#333,stroke-width:2px;",
                "FlowStyle4 exact direct declaration");
        property_list(statement.get("properties"), "#9f9", "#333", "2px", false, work);
    }
    for (std::size_t n = 0; n < 2u; ++n) {
        const auto tuple = effective.at(n);
        keys(tuple, {"target", "fill", "stroke", "stroke-width", "color", "projected"}, work);
        const auto node = small ? n + 1u : n * 2u;
        const auto fill = small ? (n == 0u ? "#f96" : "#9f9") : "#000";
        const auto role = small ? (n == 0u ? "Orange" : "Green") : "Neutral";
        require(
            text(tuple.get("target"), work) == ids[node] && text(tuple.get("fill"), work) == fill &&
                text(tuple.get("stroke"), work) == (small ? "#333" : "#000") &&
                text(tuple.get("stroke-width"), work) == (small ? "2px" : "4px") &&
                (small ? tuple.get("color").is_null() : text(tuple.get("color"), work) == "#fff"),
            "FlowStyle4 all effective source properties");
        const auto projection = tuple.get("projected");
        keys(projection,
             {"fill", "stroke", "border_bold", "label_foreground", "source_color",
              "source_color_projected", "font_bold", "font_underline"},
             work);
        require(text(projection.get("fill"), work) == role &&
                    text(projection.get("stroke"), work) == "Neutral" &&
                    projection.get("border_bold").is_bool() &&
                    projection.get("border_bold").as_bool() == !small &&
                    text(projection.get("label_foreground"), work) == "palette.text" &&
                    (small ? projection.get("source_color").is_null()
                           : text(projection.get("source_color"), work) == "Neutral") &&
                    projection.get("source_color_projected").is_bool() &&
                    !projection.get("source_color_projected").as_bool() &&
                    projection.get("font_bold").is_bool() &&
                    !projection.get("font_bold").as_bool() &&
                    projection.get("font_underline").is_bool() &&
                    !projection.get("font_underline").as_bool(),
                "FlowStyle4 exact terminal property projection");
        result.styles[node] = {small ? (n == 0u ? Fill::Orange : Fill::Green) : Fill::Neutral, true,
                               !small};
    }
    return result;
}

inline Expected bind(const Documents& docs, std::size_t selected, Domain domain,
                     std::string_view input, std::string_view original, JsonVal manifest,
                     Work& work) {
    require(selected < kPins.size() &&
                (domain == Domain::Original || domain == Domain::BodyDiagnostic),
            "FlowStyle4 trusted domain");
    const auto& pin = kPins[selected];
    work.spend((input.size() + original.size()) * 16u + 32768u);
    require(original.size() == pin.source_bytes && hash(original) == pin.source_sha &&
                manifest.get_string("path") == pin.fixture && manifest.get("bytes").is_int() &&
                manifest.get_int("bytes") == static_cast<std::int64_t>(pin.source_bytes) &&
                manifest.get_string("sha256") == pin.source_sha &&
                manifest.get_string("git_blob") == pin.git_blob,
            "FlowStyle4 complete immutable original identity");
    require(pin.front_bytes <= original.size() &&
                pin.body_bytes == original.size() - pin.front_bytes,
            "FlowStyle4 immutable source extents");
    const auto front = original.substr(0u, pin.front_bytes),
               body = original.substr(pin.front_bytes);
    require(hash(front) == pin.front_sha && hash(body) == pin.body_sha &&
                input == (domain == Domain::Original ? original : body),
            "FlowStyle4 actual full/body input identity independent of Model hash");
    const auto row = docs.document.root().get("cases").at(selected);
    keys(row,
         {"fixture", "source_utf8_bytes", "source_sha256", "frontmatter_verbatim",
          "frontmatter_utf8_bytes", "frontmatter_sha256", "body_utf8_bytes", "body_sha256", "Model",
          "Model_canonical_utf8_bytes", "Model_canonical_sha256", "Body_Model",
          "Body_Model_canonical_utf8_bytes", "Body_Model_canonical_sha256",
          "foreign_assertions_implemented", "native_admission"},
         work);
    require(text(row.get("fixture"), work) == pin.fixture &&
                row.get_int("source_utf8_bytes") == static_cast<std::int64_t>(pin.source_bytes) &&
                text(row.get("source_sha256"), work) == pin.source_sha &&
                text(row.get("frontmatter_verbatim"), work, 65536u) == front &&
                row.get_int("frontmatter_utf8_bytes") ==
                    static_cast<std::int64_t>(pin.front_bytes) &&
                text(row.get("frontmatter_sha256"), work) == pin.front_sha &&
                row.get_int("body_utf8_bytes") == static_cast<std::int64_t>(pin.body_bytes) &&
                text(row.get("body_sha256"), work) == pin.body_sha &&
                row.get("native_admission").is_bool() && !row.get("native_admission").as_bool() &&
                row.get("foreign_assertions_implemented").is_int() &&
                row.get_int("foreign_assertions_implemented") == 0,
            "FlowStyle4 complete source record and no foreign credit");
    const bool whole = domain == Domain::Original;
    const auto model = row.get(whole ? "Model" : "Body_Model");
    const auto canonical = group_endpoint15_canonical(model, work);
    const auto bytes = whole ? pin.model_bytes : pin.body_model_bytes;
    const auto digest = whole ? pin.model_sha : pin.body_model_sha;
    require(
        canonical.size() == bytes && hash(canonical) == digest &&
            row.get_int(whole ? "Model_canonical_utf8_bytes" : "Body_Model_canonical_utf8_bytes") ==
                static_cast<std::int64_t>(bytes) &&
            row.get_string(whole ? "Model_canonical_sha256" : "Body_Model_canonical_sha256") ==
                digest,
        "FlowStyle4 domain-specific complete Model identity");
    auto result = decode_model(model, selected, domain, work);
    const auto definitions = model.get("style_definitions"),
               assignments = model.get("style_assignments"), direct = model.get("direct_styles");
    const std::array<std::string_view, 3> logical{
        {text(definitions.at(0u).get("verbatim"), work),
         text(assignments.at(0u).get("verbatim"), work),
         selected == 2u ? text(direct.at(0u).get("verbatim"), work) : std::string_view{}}};
    const std::size_t expected = selected == 2u ? 3u : 2u;
    work.spend(expected * 4u * sizeof(std::string) + 8192u);
    result.raw_records.reserve(expected);
    for (std::size_t at = 0u; at < body.size();) {
        const auto end = body.find('\n', at);
        auto physical = body.substr(at, (end == std::string_view::npos ? body.size() : end) - at);
        if (physical.ends_with('\r')) physical.remove_suffix(1u);
        const auto value = trim_record(physical);
        work.spend(physical.size() * 8u + 64u);
        if (value.starts_with("classDef ") || value.starts_with("class ") ||
            value.starts_with("style ")) {
            require(result.raw_records.size() < expected &&
                        value == logical[result.raw_records.size()],
                    "FlowStyle4 physical root record/order");
            result.raw_records.emplace_back(physical);
        }
        if (end == std::string_view::npos) break;
        at = end + 1u;
    }
    require(result.raw_records.size() == expected, "FlowStyle4 all active source records");
    return result;
}
}  // namespace loom::test::mermaid_flow::style4
