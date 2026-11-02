// Test-private complete-source authority for the reviewed Class13 D1a cohort.
// Proposed Models stay immutable; only actual whole-scene proof earns admission.
#pragma once
#include "mermaid_relations_oracle.hpp"
import loom.crypto.crypto;

namespace loom::test::mermaid_class_style13 {
namespace relations = loom::test::mermaid_relations;
using relations::JsonVal;
using relations::Work;
inline constexpr std::size_t kAssetBytes = 35243u;
inline constexpr std::string_view kAssetSha =
    "59e4506fe68605d3f91229a28c7dad809147769d7b431de743c6a591953c3fc0";

struct Pin {
    std::size_t ordinal, global, source_bytes, model_bytes;
    std::string_view fixture, source_sha, model_sha, git_blob;
};

inline constexpr std::array<Pin, 13> kPins{{
    {107u, 255u, 345u, 820u,
     "class-diagram/nightly-check/dagre/"
     "dagre-should-render-a-simple-class-diagram-with-style-definition-without-htmllabels.mmd",
     "813c0221b108744414426864be848f651a84a38af30dfaf419f8f86a35c8a933",
     "efc165c75feae259e2c3586ff5e62d38b21deee9a0a7b9220121487a7dba167f",
     "9961ff9d33035a8543546993aecabd5765487740"},
    {108u, 256u, 305u, 819u,
     "class-diagram/nightly-check/dagre/"
     "dagre-should-render-a-simple-class-diagram-with-style-definition.mmd",
     "099a2db1e5a8ce2bf106e15e35da1de0adb933828a10412116308b650a09beb5",
     "8ed3c64d0de186b64f90d1ff0dd2b82375de1160400b3d1bcf11de045e4aa747",
     "c0ec021e4b9d84f3651fe8d701e49eab0ff54ded"},
    {118u, 266u, 523u, 889u,
     "class-diagram/nightly-check/dagre/handdrawn/"
     "dagre-should-render-a-simple-class-diagram-with-styles-and-the-handdrawn-look-without-"
     "htmllabels.mmd",
     "5532944156a8f4666445a8d1bd77e2a70cc7c1f86cb797a6ca2050b98d006046",
     "bb9999b97f2e6db988453ad87e8cf85902792e2072c42a22524132e902f46b7b",
     "eee0f8cdf8daa7907e1e0b0b9c0a9d22ef9b7919"},
    {119u, 267u, 441u, 888u,
     "class-diagram/nightly-check/dagre/handdrawn/"
     "dagre-should-render-a-simple-class-diagram-with-styles-and-the-handdrawn-look.mmd",
     "1fb58a2add14a748ed9369ec193bbeec8d82de2d4082d474479586c2c10ae7a3",
     "45e419997217fcc7a57decc28616a609c0607e52c5e924142ff4b5e450df9c3a",
     "e0c18c66f9cc73db1d14434491f0f425b638a532"},
    {169u, 317u, 318u, 822u,
     "class-diagram/nightly-check/handdrawn/"
     "hd-should-render-a-simple-class-diagram-with-style-definition-without-htmllabels.mmd",
     "b32faa6dcb41c6e35dc48cd5348b4c575330a88a2efef66d420f2a518574f39a",
     "c59581d0a6f7bb5cfef06c688b3443f92b458844e1726fc74390e1db0a8cb4fd",
     "4e06586de10fd358326cfeace5c734875bd31226"},
    {170u, 318u, 314u, 821u,
     "class-diagram/nightly-check/handdrawn/"
     "hd-should-render-a-simple-class-diagram-with-style-definition.mmd",
     "1844eaacd2cf318d7f3d82c143e5826dcd291b8eb1a325c4cc633a7f743967df",
     "229fcb67762f3a35c02d368b53524fda59bc0fea953bd6692d0fc739da7fde7b",
     "ba6d505278e6761becda6ad905663450f7dcec80"},
    {171u, 319u, 396u, 872u,
     "class-diagram/nightly-check/handdrawn/"
     "hd-should-render-a-simple-class-diagram-with-styles-and-the-handdrawn-look-without-"
     "htmllabels.mmd",
     "a2d6e5ac23dd2254a8504d920100e0b886b0fafa4b3659f5ee93d9204426c3bb",
     "e03618578340748c3467706167d9eee210d96dbbd9cb361eda5bc5e2fdd97fd5",
     "fda97d46b7f9c5ec496ba6a6d0af7499d735852d"},
    {172u, 320u, 336u, 871u,
     "class-diagram/nightly-check/handdrawn/"
     "hd-should-render-a-simple-class-diagram-with-styles-and-the-handdrawn-look.mmd",
     "1bafc4bf9f0b119c41abb86ecef62b42d4747225fbf468b09517baf872ff752f",
     "3f187c1ea1facd69ed75c670421fde3bd70e3ee5f36871d570bfff527a03f701",
     "b6b2f1632187a7e5632faa3062b91e147fd7c909"},
    {183u, 331u, 326u, 872u,
     "class-diagram/nightly-check/handdrawn/"
     "should-render-a-simple-class-diagram-with-styles-and-the-handdrawn-look-without-htmllabels."
     "mmd",
     "4ce323919c264ed96c6cf6fb264b05450a5218abf81e21dff6d90e18639f9d27",
     "e03618578340748c3467706167d9eee210d96dbbd9cb361eda5bc5e2fdd97fd5",
     "26290fccd8e209b9bdd3a83ef99bcf915698d511"},
    {184u, 332u, 336u, 871u,
     "class-diagram/nightly-check/handdrawn/"
     "should-render-a-simple-class-diagram-with-styles-and-the-handdrawn-look.mmd",
     "1bafc4bf9f0b119c41abb86ecef62b42d4747225fbf468b09517baf872ff752f",
     "3f187c1ea1facd69ed75c670421fde3bd70e3ee5f36871d570bfff527a03f701",
     "b6b2f1632187a7e5632faa3062b91e147fd7c909"},
    {216u, 364u, 308u, 802u,
     "class-diagram/nightly-check/should-render-a-simple-class-diagram-with-style-definition-2.mmd",
     "9507c71c12cb8a1248ed1cb98c6de9ffa0fdf3b17c759cb59be678840adb586c",
     "6add7d30e5cd03c7148ddd09cffbfcf64d46be9dee6eb72ca75ed75b063bc7c4",
     "2525236bcd25358f6e1bceee3e03c7a346fe0354"},
    {217u, 365u, 311u, 803u,
     "class-diagram/nightly-check/"
     "should-render-a-simple-class-diagram-with-style-definition-without-htmllabels.mmd",
     "6cd94b8b6eb002faa1aa926d151c9c1b48d7879acc7121baa078e02331a5eda4",
     "e6da442e06a410a4db81e7f72f8a9e479bc9cc96bc7363eff4c04070608b7dd6",
     "4d2ad8e452d1c52295b1c23535bbc57e0985e66a"},
    {218u, 366u, 366u, 820u,
     "class-diagram/nightly-check/should-render-a-simple-class-diagram-with-style-definition.mmd",
     "7f5b2a36dd27eaa5a5b024170b143f01b84e29bfbf62de43072d4471cd0db7c2",
     "6a645864559058af86164030bce8937cf059383c073f61bca972906496daf7a5",
     "12338526edfbfe6e543aaaa29710b965c6e988b6"},
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
    std::string raw_declaration;
    bool source_color = false;
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
                      "fixtures/mermaid_native/class_direct_style13_source_models.proposal.v1.json";
    require(std::filesystem::file_size(path) == kAssetBytes, "Class13 immutable asset size");
    work.spend(kAssetBytes * 24u + 65536u);
    std::ifstream input(path, std::ios::binary);
    require(static_cast<bool>(input), "Class13 source asset read");
    std::string raw{std::istreambuf_iterator<char>(input), {}};
    require(raw.size() == kAssetBytes && hash(raw) == kAssetSha,
            "Class13 immutable raw asset hash");
    auto parsed = loom::utils::json::parse(raw);
    require(parsed.has_value(), "Class13 source asset JSON");
    const auto root = parsed->root();
    relations::class481_keys(root, {"schema", "status", "authority", "count", "cases"}, work);
    require(root.get_string("schema") == "class_direct_style13_source_models.proposal/v1" &&
                root.get_string("status") ==
                    "proposal_requires_independent_source_review_no_admission" &&
                root.get_string("authority") ==
                    "independently_authored_complete_sources_not_renderer_output" &&
                root.get("count").is_int() && root.get_int("count") == 13,
            "Class13 closed proposal identity");
    const auto cases = root.get("cases");
    require(cases.is_arr() && cases.size() == 13u, "Class13 exact cases");
    return {std::move(raw), std::move(*parsed)};
}

inline Expected bind(const Documents& documents, std::size_t selected, std::size_t ordinal,
                     std::size_t global, std::string_view source, JsonVal manifest, Work& work) {
    require(selected < kPins.size(), "Class13 selected index");
    const auto& pin = kPins[selected];
    work.spend(source.size() * 16u + 8192u);
    require(pin.ordinal == ordinal && pin.global == global && source.size() == pin.source_bytes &&
                hash(source) == pin.source_sha,
            "Class13 exact complete source tuple");
    require(manifest.get_string("path") == pin.fixture &&
                manifest.get_string("sha256") == pin.source_sha && manifest.get("bytes").is_int() &&
                manifest.get_int("bytes") == static_cast<std::int64_t>(pin.source_bytes) &&
                manifest.get_string("git_blob") == pin.git_blob,
            "Class13 immutable manifest tuple");
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
            "Class13 exact source authority row");
    const auto front = relations::class481_text(row.get("frontmatter_verbatim"), work);
    const auto body_bytes = row.get("body_utf8_bytes");
    require(body_bytes.is_int() && body_bytes.as_int() >= 0 && front.size() <= source.size(),
            "Class13 body extent types");
    const auto body = source.substr(front.size());
    require(source.substr(0, front.size()) == front &&
                row.get_int("frontmatter_utf8_bytes") == static_cast<std::int64_t>(front.size()) &&
                row.get_string("frontmatter_sha256") == hash(front) &&
                body_bytes.as_int() == static_cast<std::int64_t>(body.size()) &&
                row.get_string("body_sha256") == hash(body),
            "Class13 complete physical body/frontmatter binding");
    const auto model = row.get("Model");
    const auto canonical = relations::class481_canonical(model, work);
    require(canonical.size() == pin.model_bytes && hash(canonical) == pin.model_sha,
            "Class13 complete canonical Model identity");
    relations::class481_keys(model,
                             {"family", "header", "direction", "title", "accessibility_title",
                              "accessibility_description", "hide_empty_members_box", "entities",
                              "relationships", "namespaces", "notes", "frontmatter_config",
                              "style_declarations"},
                             work);
    require(model.get_string("family") == "class" &&
                (model.get_string("header") == "classDiagram" ||
                 model.get_string("header") == "classDiagram-v2") &&
                model.get_string("direction") == "TB" && model.get("title").is_null() &&
                model.get("accessibility_title").is_null() &&
                model.get("accessibility_description").is_null() &&
                model.get("hide_empty_members_box").is_bool() &&
                !model.get("hide_empty_members_box").as_bool() &&
                model.get("frontmatter_config").is_obj(),
            "Class13 complete header/default metadata");
    for (const auto key : {"relationships", "namespaces", "notes"})
        require(model.get(key).is_arr() && model.get(key).size() == 0u,
                "Class13 complete absence of graph facts");
    const auto entities = model.get("entities");
    require(entities.is_arr() && entities.size() == 1u, "Class13 exact entity count");
    const auto entity = entities.at(0u);
    relations::class481_keys(
        entity, {"id", "label", "annotation", "owner", "shape", "declared", "members"}, work);
    require(entity.get_string("id") == "Class10" && entity.get_string("label") == "Class10" &&
                entity.get_string("annotation").empty() && entity.get("owner").is_int() &&
                entity.get_int("owner") == -1 && entity.get_string("shape") == "Box" &&
                entity.get("declared").is_bool() && entity.get("declared").as_bool() &&
                entity.get("members").is_arr() && entity.get("members").size() == 0u,
            "Class13 whole entity semantics");
    const auto declarations = model.get("style_declarations");
    require(declarations.is_arr() && declarations.size() == 1u, "Class13 exact declaration count");
    const auto declaration = declarations.at(0u);
    relations::class481_keys(declaration, {"target", "verbatim", "properties", "projected"}, work);
    require(declaration.get_string("target") == "Class10", "Class13 exact style target");
    const auto logical = relations::class481_text(declaration.get("verbatim"), work, 1024u);
    const auto properties = declaration.get("properties");
    require(properties.is_arr() && (properties.size() == 3u || properties.size() == 4u),
            "Class13 complete property count");
    constexpr std::array<std::pair<std::string_view, std::string_view>, 4> expected{
        {{"fill", "#f9f"}, {"stroke", "#333"}, {"stroke-width", "4px"}, {"color", "white"}}};
    for (std::size_t i = 0; i < properties.size(); ++i) {
        const auto property = properties.at(i);
        relations::class481_keys(property, {"name", "value"}, work);
        require(property.get_string("name") == expected[i].first &&
                    property.get_string("value") == expected[i].second,
                "Class13 all exact source property values");
    }
    const bool color = properties.size() == 4u;
    const auto projected = declaration.get("projected");
    relations::class481_keys(projected,
                             {"fill", "stroke", "border_bold", "label_foreground", "source_color",
                              "source_color_projected", "font_bold", "font_underline"},
                             work);
    require(projected.get_string("fill") == "Purple" &&
                projected.get_string("stroke") == "Neutral" &&
                projected.get("border_bold").is_bool() && projected.get("border_bold").as_bool() &&
                projected.get_string("label_foreground") == "palette.text" &&
                projected.get("source_color_projected").is_bool() &&
                !projected.get("source_color_projected").as_bool() &&
                projected.get("font_bold").is_bool() && !projected.get("font_bold").as_bool() &&
                projected.get("font_underline").is_bool() &&
                !projected.get("font_underline").as_bool() &&
                (color ? projected.get_string("source_color") == "Neutral"
                       : projected.get("source_color").is_null()),
            "Class13 exact independent projected tuple");
    Expected result;
    result.index = selected;
    result.source_color = color;
    result.graph = {"class", "TB", {{"Class10", "Class10", "box", "", {{"Class10"}}, {}}},
                    {},      {},   {}};
    std::size_t found = 0u;
    for (std::size_t at = 0; at < body.size();) {
        const auto end = body.find('\n', at);
        auto physical = body.substr(at, (end == std::string_view::npos ? body.size() : end) - at);
        if (physical.ends_with('\r')) physical.remove_suffix(1u);
        work.spend(physical.size() * 8u + 64u);
        if (trim(physical).starts_with("style ")) {
            require(trim(physical) == logical,
                    "Class13 exact logical/physical declaration binding");
            result.raw_declaration = physical;
            ++found;
        }
        if (end == std::string_view::npos) break;
        at = end + 1u;
    }
    require(found == 1u, "Class13 complete physical declaration count");
    return result;
}
}  // namespace loom::test::mermaid_class_style13
