// Test-private complete-source authority for the reviewed ClassDefined6 D1b cohort.
// Proposed Models stay immutable; only actual whole-scene proof earns admission.
#pragma once
#include "mermaid_relations_oracle.hpp"
import loom.crypto.crypto;

namespace loom::test::mermaid_class_defined6 {
namespace relations = loom::test::mermaid_relations;
using relations::JsonVal;
using relations::Work;
inline constexpr std::size_t kAssetBytes = 22362u;
inline constexpr std::string_view kAssetSha =
    "71684e48846f0f634e6d02e336d5f2ef08a1a0f2d4a7eb393119f88414322202";

struct Pin {
    std::size_t ordinal, global, source_bytes, model_bytes;
    std::string_view fixture, source_sha, model_sha, git_blob;
};

inline constexpr std::array<Pin, 6> kPins{{
    {99u, 247u, 447u, 1305u,
     "class-diagram/nightly-check/dagre/"
     "dagre-should-render-a-simple-class-diagram-with-classdefs-being-applied-without-htmllabels."
     "mmd",
     "38b3e89e6bd63076bc9768d4438013d8d6d131f97fed297c30ab4b2d4218873d",
     "387695b72c1e7bd038667bbb59c9fe780830437cf22a8f9659bd5b31c60f1622",
     "2e03ce344189db2f5fd6c3fd8946acae4cdcdd67"},
    {100u, 248u, 409u, 1304u,
     "class-diagram/nightly-check/dagre/"
     "dagre-should-render-a-simple-class-diagram-with-classdefs-being-applied.mmd",
     "638c72491c62933bb57ee110a8b9379e2a887d811ed0e962ffa0c6d990a238c2",
     "3aafb9b89522dd1a44658592c99de5a447281901edf741de9c0ae193d2289dac",
     "0ff39b40a7811483964b20650bdde845ffe21b38"},
    {161u, 309u, 548u, 1307u,
     "class-diagram/nightly-check/handdrawn/"
     "hd-should-render-a-simple-class-diagram-with-classdefs-being-applied-without-htmllabels.mmd",
     "441b3c121009bb37942556fe9afc6c9629b5fa88f8d5fe062dbd5c4439966523",
     "68bc2ad784cab7632ac1510e2809ac966dad0a0ba799a8c8be510ae6865a71f1",
     "cdaff6a53480276dd670a909fa2d5776d8e98523"},
    {162u, 310u, 526u, 1306u,
     "class-diagram/nightly-check/handdrawn/"
     "hd-should-render-a-simple-class-diagram-with-classdefs-being-applied.mmd",
     "6667a821245c241716e7574ff9fb6863aa565632014ad6c7c295616178bbb28e",
     "bf31c39c4a19b35d1a16da2b6d240251d4d16f72806d4f0b2e84629cf907c7f4",
     "c51f6cd369b1733061926423c04e9f8ffa199e76"},
    {207u, 355u, 448u, 1288u,
     "class-diagram/nightly-check/"
     "should-render-a-simple-class-diagram-with-classdefs-being-applied-without-htmllabels.mmd",
     "d83931282212080a11ebb4a31addb8abeab2453998592fd34dbdc849fcd64b0d",
     "a4e5d3fa5e614dfeb94a7f981a6b144eeab34986b8cf11a29afabd5c32afc841",
     "4cde54be986ed2021b19462fd3cf9e99affd9407"},
    {208u, 356u, 426u, 1287u,
     "class-diagram/nightly-check/"
     "should-render-a-simple-class-diagram-with-classdefs-being-applied.mmd",
     "5b52ab02817baa019b7b883ca4637ca8db73709e273935aad37ad870c16cb365",
     "6c6fd45439a5b47e5c9d4a371525c8b778bd28627182926b8c442b4a201f8277",
     "540bd95e52f706e8edd3fddd17058d9258ed4939"},
}};

inline std::optional<std::size_t> index(std::string_view fixture) {
    for (std::size_t i = 0; i < kPins.size(); ++i)
        if (kPins[i].fixture == fixture) return i;
    return {};
}

struct Documents {
    std::string raw;
    loom::utils::json::JsonDoc document;
};

struct Expected {
    std::size_t index = 0;
    relations::Model graph;
    std::array<std::string, 4> raw_records;
};

inline std::string_view trim(std::string_view value) {
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '\t' || value.front() == '\r'))
        value.remove_prefix(1u);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
        value.remove_suffix(1u);
    return value;
}

inline void require(bool condition, std::string_view why) {
    relations::class481_require(condition, why);
}

inline std::string hash(std::string_view value) {
    return loom::utils::crypto::sha256(value);
}

inline Documents load(Work& work) {
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) /
                      "fixtures/mermaid_native/class_defined_style6_source_models.proposal.v1.json";
    require(std::filesystem::file_size(path) == kAssetBytes, "ClassDefined6 immutable asset size");
    work.spend(kAssetBytes * 24u + 65536u);
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "ClassDefined6 source asset read");
    std::string raw{std::istreambuf_iterator<char>(input), {}};
    require(raw.size() == kAssetBytes && hash(raw) == kAssetSha,
            "ClassDefined6 immutable raw asset hash");
    auto parsed = loom::utils::json::parse(raw);
    require(parsed.has_value(), "ClassDefined6 source asset JSON");
    const auto root = parsed->root();
    relations::class481_keys(root, {"schema", "status", "authority", "count", "cases"}, work);
    require(root.get_string("schema") == "class_defined_style6_source_models.proposal/v1" &&
                root.get_string("status") ==
                    "proposal_requires_independent_source_review_no_admission" &&
                root.get_string("authority") ==
                    "independently_authored_complete_sources_not_renderer_output" &&
                root.get("count").is_int() && root.get_int("count") == 6,
            "ClassDefined6 closed proposal identity");
    const auto cases = root.get("cases");
    require(cases.is_arr() && cases.size() == 6u, "ClassDefined6 exact cases");
    return {std::move(raw), std::move(*parsed)};
}

inline void validate_config(JsonVal value, std::size_t selected, Work& work) {
    require(selected < kPins.size(), "ClassDefined6 configuration domain");
    work.spend(4096u);
    const bool dagre = selected < 2u, handdrawn = selected >= 2u && selected < 4u;
    if (dagre)
        relations::class481_keys(value, {"logLevel", "htmlLabels", "layout"}, work);
    else if (handdrawn)
        relations::class481_keys(value, {"logLevel", "htmlLabels", "look"}, work);
    else
        relations::class481_keys(value, {"logLevel", "htmlLabels"}, work);
    require(value.get("logLevel").is_int() && value.get_int("logLevel") == 1 &&
                value.get("htmlLabels").is_bool() &&
                value.get("htmlLabels").as_bool() == (selected % 2u == 1u),
            "ClassDefined6 complete configuration scalar types/values");
    if (dagre)
        require(value.get("layout").is_str() && value.get_string("layout") == "dagre",
                "ClassDefined6 exact layout");
    if (handdrawn)
        require(value.get("look").is_str() && value.get_string("look") == "handDrawn",
                "ClassDefined6 exact look");
}

inline Expected bind(const Documents& documents, std::size_t selected, std::size_t ordinal,
                     std::size_t global, std::string_view source, JsonVal manifest, Work& work) {
    require(selected < kPins.size(), "ClassDefined6 selected index");
    const auto& pin = kPins[selected];
    work.spend(source.size() * 16u + 8192u);
    require(pin.ordinal == ordinal && pin.global == global && source.size() == pin.source_bytes &&
                hash(source) == pin.source_sha,
            "ClassDefined6 exact complete source tuple");
    require(manifest.get_string("path") == pin.fixture &&
                manifest.get_string("sha256") == pin.source_sha && manifest.get("bytes").is_int() &&
                manifest.get_int("bytes") == static_cast<std::int64_t>(pin.source_bytes) &&
                manifest.get_string("git_blob") == pin.git_blob,
            "ClassDefined6 immutable manifest tuple");
    const auto row = documents.document.root().get("cases").at(selected);
    relations::class481_keys(row,
                             {"fixture", "source_utf8_bytes", "source_sha256", "body_utf8_bytes",
                              "body_sha256", "frontmatter_verbatim", "frontmatter_utf8_bytes",
                              "frontmatter_sha256", "Model", "Model_canonical_utf8_bytes",
                              "Model_canonical_sha256", "foreign_assertions_implemented",
                              "native_admission"},
                             work);
    require(row.get_string("fixture") == pin.fixture &&
                row.get_string("source_sha256") == pin.source_sha &&
                row.get_int("source_utf8_bytes") == static_cast<std::int64_t>(pin.source_bytes) &&
                row.get_int("Model_canonical_utf8_bytes") ==
                    static_cast<std::int64_t>(pin.model_bytes) &&
                row.get_string("Model_canonical_sha256") == pin.model_sha &&
                row.get("native_admission").is_bool() && !row.get("native_admission").as_bool() &&
                row.get("foreign_assertions_implemented").is_int() &&
                row.get_int("foreign_assertions_implemented") == 0,
            "ClassDefined6 exact source authority row");
    const auto front = relations::class481_text(row.get("frontmatter_verbatim"), work);
    const auto body_bytes = row.get("body_utf8_bytes");
    require(body_bytes.is_int() && body_bytes.as_int() >= 0 && front.size() <= source.size(),
            "ClassDefined6 body extent types");
    const auto body = source.substr(front.size());
    require(source.substr(0, front.size()) == front &&
                row.get_int("frontmatter_utf8_bytes") == static_cast<std::int64_t>(front.size()) &&
                row.get_string("frontmatter_sha256") == hash(front) &&
                body_bytes.as_int() == static_cast<std::int64_t>(body.size()) &&
                row.get_string("body_sha256") == hash(body),
            "ClassDefined6 complete physical body/frontmatter binding");
    const auto model = row.get("Model");
    const auto canonical = relations::class481_canonical(model, work);
    require(canonical.size() == pin.model_bytes && hash(canonical) == pin.model_sha,
            "ClassDefined6 complete canonical Model identity");
    relations::class481_keys(model,
                             {"family", "header", "direction", "title", "accessibility_title",
                              "accessibility_description", "hide_empty_members_box", "entities",
                              "relationships", "namespaces", "notes", "frontmatter_config",
                              "style_definitions", "style_assignments", "effective_styles"},
                             work);
    require(model.get_string("family") == "class" && model.get_string("header") == "classDiagram" &&
                model.get_string("direction") == "TB" && model.get("title").is_null() &&
                model.get("accessibility_title").is_null() &&
                model.get("accessibility_description").is_null() &&
                model.get("hide_empty_members_box").is_bool() &&
                !model.get("hide_empty_members_box").as_bool() &&
                model.get("frontmatter_config").is_obj(),
            "ClassDefined6 complete header/default metadata");
    validate_config(model.get("frontmatter_config"), selected, work);
    for (const auto key : {"relationships", "namespaces", "notes"})
        require(model.get(key).is_arr() && model.get(key).size() == 0u,
                "ClassDefined6 complete absence of graph facts");
    const auto entities = model.get("entities");
    require(entities.is_arr() && entities.size() == 1u, "ClassDefined6 exact entity count");
    const auto entity = entities.at(0u);
    relations::class481_keys(
        entity, {"id", "label", "annotation", "owner", "shape", "declared", "members"}, work);
    require(entity.get_string("id") == "Class10" && entity.get_string("label") == "Class10" &&
                entity.get_string("annotation").empty() && entity.get("owner").is_int() &&
                entity.get_int("owner") == -1 && entity.get_string("shape") == "Box" &&
                entity.get("declared").is_bool() && entity.get("declared").as_bool() &&
                entity.get("members").is_arr() && entity.get("members").size() == 0u,
            "ClassDefined6 whole entity semantics");
    const auto definitions = model.get("style_definitions"),
               assignments = model.get("style_assignments"),
               effective = model.get("effective_styles");
    require(definitions.is_arr() && definitions.size() == 2u && assignments.is_arr() &&
                assignments.size() == 2u && effective.is_arr() && effective.size() == 1u,
            "ClassDefined6 complete styles");
    constexpr std::array<std::string_view, 4> logical{
        {"class Class10:::pink", "cssClass \"Class10\" bold", "classDef pink fill:#f9f",
         "classDef bold stroke:#333,stroke-width:6px,color:#fff"}};
    constexpr std::array<std::string_view, 2> names{{"pink", "bold"}};
    for (std::size_t n = 0; n < 2u; ++n) {
        const auto definition = definitions.at(n);
        relations::class481_keys(definition, {"name", "verbatim", "properties"}, work);
        require(definition.get_string("name") == names[n] &&
                    definition.get_string("verbatim") == logical[n + 2u],
                "ClassDefined6 exact definitions");
        const auto assignment = assignments.at(n);
        relations::class481_keys(
            assignment, {"kind", "targets", "class_name", "verbatim", "assignment_order"}, work);
        require(assignment.get_string("kind") ==
                        std::string_view(n == 0u ? "inline_class_declaration" : "cssClass") &&
                    assignment.get_string("class_name") == names[n] &&
                    assignment.get_string("verbatim") == logical[n] &&
                    assignment.get("assignment_order").is_int() &&
                    assignment.get_int("assignment_order") == static_cast<std::int64_t>(n),
                "ClassDefined6 ordered exact assignments");
        const auto targets = assignment.get("targets");
        require(targets.is_arr() && targets.size() == 1u && targets.at(0u).is_str() &&
                    targets.at(0u).as_str() == "Class10",
                "ClassDefined6 exact ID targets");
        const auto properties = definition.get("properties");
        require(properties.is_arr() && properties.size() == (n == 0u ? 1u : 3u),
                "ClassDefined6 complete property list");
        constexpr std::array<std::pair<std::string_view, std::string_view>, 4> values{
            {{"fill", "#f9f"}, {"stroke", "#333"}, {"stroke-width", "6px"}, {"color", "#fff"}}};
        for (std::size_t k = 0; k < properties.size(); ++k) {
            const auto v = properties.at(k);
            const auto& wanted = values[n == 0u ? 0u : k + 1u];
            relations::class481_keys(v, {"name", "value"}, work);
            require(v.get_string("name") == wanted.first && v.get_string("value") == wanted.second,
                    "ClassDefined6 all source properties");
        }
    }
    const auto tuple = effective.at(0u);
    relations::class481_keys(
        tuple, {"target", "fill", "stroke", "stroke-width", "color", "projected"}, work);
    require(tuple.get_string("target") == "Class10" && tuple.get_string("fill") == "#f9f" &&
                tuple.get_string("stroke") == "#333" && tuple.get_string("stroke-width") == "6px" &&
                tuple.get_string("color") == "#fff",
            "ClassDefined6 complete effective source tuple");
    const auto projected = tuple.get("projected");
    relations::class481_keys(projected,
                             {"fill", "stroke", "border_bold", "label_foreground", "source_color",
                              "source_color_projected", "font_bold", "font_underline"},
                             work);
    require(
        projected.get_string("fill") == "Purple" && projected.get_string("stroke") == "Neutral" &&
            projected.get("border_bold").is_bool() && projected.get("border_bold").as_bool() &&
            projected.get_string("label_foreground") == "palette.text" &&
            projected.get_string("source_color") == "Neutral" &&
            projected.get("source_color_projected").is_bool() &&
            !projected.get("source_color_projected").as_bool() &&
            projected.get("font_bold").is_bool() && !projected.get("font_bold").as_bool() &&
            projected.get("font_underline").is_bool() && !projected.get("font_underline").as_bool(),
        "ClassDefined6 exact independent palette tuple");
    Expected result;
    result.index = selected;
    result.graph = {"class", "TB", {{"Class10", "Class10", "box", "", {{"Class10"}}, {}}},
                    {},      {},   {}};
    std::size_t found = 0u;
    for (std::size_t at = 0; at < body.size();) {
        const auto end = body.find('\n', at);
        auto physical = body.substr(at, (end == std::string_view::npos ? body.size() : end) - at);
        if (physical.ends_with('\r')) physical.remove_suffix(1u);
        work.spend(physical.size() * 8u + 64u);
        if (!trim(physical).empty() && trim(physical) != "classDiagram") {
            require(found < logical.size() && trim(physical) == logical[found],
                    "ClassDefined6 complete physical statement order");
            result.raw_records[found++] = physical;
        }
        if (end == std::string_view::npos) break;
        at = end + 1u;
    }
    require(found == 4u, "ClassDefined6 all physical source records");
    return result;
}
}  // namespace loom::test::mermaid_class_defined6
