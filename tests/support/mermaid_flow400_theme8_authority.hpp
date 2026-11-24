// Test-private source-bound theme-group Flow400 overlay (D-470).
// TL round 804 authorizes a per-group Flow400 re-baseline: the eight
// redux-color-subgraph originals become complete typed Models. Base8,
// Config21, TypedSource4 and Classic assets are untouched.
#pragma once
#include "mermaid_flow400_classic_authority.hpp"

namespace loom::test::mermaid_flow {

inline constexpr std::size_t kFlow400Theme8AssetBytes = 33333u;
inline constexpr std::string_view kFlow400Theme8AssetSha =
    "7f321f74af35745c0b88a28938a4016f93000d3d1b5607290f494ab4a37982ba";

struct Flow400Theme8Documents {
    OracleWork asset_work;
    json::JsonDoc supplement;
};

inline void flow400_theme8_keys(JsonVal value, std::initializer_list<std::string_view> keys,
                                std::string_view scope) {
    if (!value.is_obj())
        throw std::runtime_error("Flow400 Theme8 " + std::string(scope) + " wrong kind");
    std::array<bool, 16> seen{};
    if (keys.size() > seen.size()) throw std::runtime_error("Flow400 Theme8 internal key bound");
    value.iter_obj([&](JsonVal key, JsonVal) {
        const auto name = flow400_text(key);
        const auto at = std::ranges::find(keys, name);
        if (at == keys.end())
            throw std::runtime_error("Flow400 Theme8 " + std::string(scope) + " unknown key");
        const auto index = static_cast<std::size_t>(at - keys.begin());
        if (seen[index])
            throw std::runtime_error("Flow400 Theme8 " + std::string(scope) + " duplicate key");
        seen[index] = true;
    });
    if (value.size() != keys.size())
        throw std::runtime_error("Flow400 Theme8 " + std::string(scope) + " missing key");
    for (std::size_t index = 0; index < keys.size(); ++index)
        if (!seen[index])
            throw std::runtime_error("Flow400 Theme8 " + std::string(scope) + " missing key");
}

inline std::size_t flow400_theme8_expected(JsonVal row) {
    const auto expected = flow400_number(row.get("native_requirement").get("exact_rejections"));
    flow400_require(expected == 4u, "Flow400 Theme8 exact mutation count");
    return expected;
}

inline void flow400_theme8_envelope(JsonVal envelope, JsonVal row) {
    flow400_theme8_keys(envelope,
                        {"profile", "bytes", "sha256", "text", "description_form", "root_keys",
                         "config", "body_header", "body_sha256"},
                        "envelope");
    const auto source = flow400_text(row.get("source")), text = flow400_text(envelope.get("text"));
    flow400_require(flow400_number(envelope.get("bytes")) == text.size() &&
                        digest(text) == flow400_text(envelope.get("sha256")) &&
                        source.starts_with(text),
                    "Flow400 Theme8 envelope bytes/SHA");
    const auto form = flow400_text(envelope.get("description_form"));
    flow400_require(form == "plain_multiline" || form == "folded >-",
                    "Flow400 Theme8 description form");
    const auto roots = envelope.get("root_keys");
    flow400_require(roots.is_arr() && roots.size() == 2u &&
                        flow400_text(roots.at(0u)) == "description" &&
                        flow400_text(roots.at(1u)) == "config",
                    "Flow400 Theme8 config envelope roots");
    const auto body = std::string_view{source}.substr(text.size());
    auto head = body;
    while (!head.empty() && (head.front() == '\n' || head.front() == ' ' ||
                             head.front() == '\r'))
        head.remove_prefix(1u);
    flow400_require(digest(body) == flow400_text(envelope.get("body_sha256")) &&
                        head.starts_with(flow400_text(envelope.get("body_header"))) &&
                        (head.size() == flow400_text(envelope.get("body_header")).size() ||
                         head[flow400_text(envelope.get("body_header")).size()] == '\n'),
                    "Flow400 Theme8 body SHA/header");
    const auto config = envelope.get("config");
    flow400_theme8_keys(config, {"theme"}, "config");
    const auto theme = flow400_text(config.get("theme"));
    flow400_require(theme == "\"redux\"" || theme == "\"redux-color\"" ||
                        theme == "\"redux-dark\"" || theme == "\"redux-dark-color\"",
                    "Flow400 Theme8 palette value");
    const auto ordinal = flow400_number(row.get("base_authority").get("ordinal"));
    const bool siblings = ordinal == 137u || ordinal == 141u || ordinal == 145u ||
                          ordinal == 149u;
    flow400_require(flow400_text(envelope.get("body_header")) ==
                        (siblings ? "flowchart TB" : "flowchart LR"),
                    "Flow400 Theme8 body header");
}

inline void flow400_theme8_case_shape(JsonVal row, OracleWork& work) {
    flow400_theme8_keys(row,
                        {"case_id", "base_authority", "source", "source_sha256",
                         "typed_model_json", "typed_model_sha256", "envelope",
                         "native_requirement", "formal_status"},
                        "case");
    const auto base = row.get("base_authority");
    flow400_theme8_keys(base,
                        {"adaptation", "fixture", "manifest_global_ordinal", "model_json",
                         "model_sha256", "oracle_record_sha256", "ordinal", "source_bytes",
                         "source_git_blob", "source_sha256"},
                        "base authority");
    flow400_require(base.get("model_json").is_null() && base.get("model_sha256").is_null() &&
                        base.get("adaptation").is_null(),
                    "Flow400 Theme8 base pending authority");
    const auto source = flow400_text(row.get("source"));
    flow400_require(static_cast<std::size_t>(flow400_number(base.get("source_bytes"))) ==
                            source.size() &&
                        digest(source) == flow400_text(base.get("source_sha256")) &&
                        flow400_text(row.get("source_sha256")) ==
                            flow400_text(base.get("source_sha256")),
                    "Flow400 Theme8 source binding");
    const auto model = flow400_text(row.get("typed_model_json"));
    flow400_require(model.size() <= 2048u &&
                        digest(model) == flow400_text(row.get("typed_model_sha256")),
                    "Flow400 Theme8 Model text SHA");
    work.product(model.size(), 32u);
    work.spend(4u * sizeof(json::JsonDoc) + 8192u);
    auto document = json::parse(model);
    flow400_require(document.has_value(), "Flow400 Theme8 Model JSON");
    static_cast<void>(flow_typed_shape_model(document->root(), work));
    flow400_theme8_envelope(row.get("envelope"), row);
    const auto requirement = row.get("native_requirement");
    flow400_theme8_keys(requirement, {"exact_rejections", "mutations"}, "native requirement");
    const auto mutations = requirement.get("mutations");
    flow400_require(flow400_number(requirement.get("exact_rejections")) == 4u &&
                        mutations.is_arr() && mutations.size() == 4u,
                    "Flow400 Theme8 mutation ledger");
    flow400_require(flow400_text(row.get("formal_status")) == "unadmitted_design_only",
                    "Flow400 Theme8 formal status");
    static_cast<void>(flow400_text(row.get("case_id")));
}

inline void flow400_theme8_root(JsonVal root, OracleWork& work) {
    flow400_theme8_keys(root,
                        {"schema", "qualification", "base_authority", "prior_authority",
                         "formal_counts", "cases"},
                        "root");
    flow400_require(flow400_text(root.get("schema")) == "flow400_theme8_native_supplement/v1",
                    "Flow400 Theme8 schema");
    flow400_require(!flow400_text(root.get("qualification")).empty(),
                    "Flow400 Theme8 qualification");
    const auto base = root.get("base_authority");
    flow400_theme8_keys(base,
                        {"asset_sha256", "manifest_sha256", "all_sources", "complete_models",
                         "pending"},
                        "base authority");
    flow400_require(flow400_text(base.get("asset_sha256")) == kFlow400AssetSha &&
                        flow400_text(base.get("manifest_sha256")) == kFlow400ManifestSha &&
                        flow400_number(base.get("all_sources")) == 400u &&
                        flow400_number(base.get("complete_models")) == 8u &&
                        flow400_number(base.get("pending")) == 392u,
                    "Flow400 Theme8 base authority lock");
    const auto prior = root.get("prior_authority");
    flow400_theme8_keys(prior,
                        {"config21_asset_sha256", "typed_source4_asset_sha256",
                         "classic_asset_sha256", "aggregate_complete_before_theme8",
                         "aggregate_pending_before_theme8"},
                        "prior authority");
    flow400_require(flow400_text(prior.get("config21_asset_sha256")) ==
                            kFlow400Config21AssetSha &&
                        flow400_text(prior.get("typed_source4_asset_sha256")) ==
                            kFlow400TypedSource4AssetSha &&
                        flow400_text(prior.get("classic_asset_sha256")) ==
                            kFlow400ClassicAssetSha &&
                        flow400_number(prior.get("aggregate_complete_before_theme8")) == 39u &&
                        flow400_number(prior.get("aggregate_pending_before_theme8")) == 361u,
                    "Flow400 Theme8 prior authority lock");
    const auto counts = root.get("formal_counts");
    flow400_theme8_keys(counts,
                        {"cases", "aggregate_complete_after_all_official_runs",
                         "aggregate_pending_after_all_official_runs",
                         "ultimate_complete_required", "ultimate_pending_required",
                         "official_credit_before_execution"},
                        "formal counts");
    flow400_require(flow400_number(counts.get("cases")) == 8u &&
                        flow400_number(counts.get("aggregate_complete_after_all_official_runs")) ==
                            47u &&
                        flow400_number(
                            counts.get("aggregate_pending_after_all_official_runs")) == 353u &&
                        flow400_number(counts.get("ultimate_complete_required")) == 400u &&
                        flow400_number(counts.get("ultimate_pending_required")) == 0u &&
                        flow400_number(counts.get("official_credit_before_execution")) == 0u,
                    "Flow400 Theme8 formal counts");
    const auto cases = root.get("cases");
    flow400_require(cases.is_arr() && cases.size() == 8u,
                    "Flow400 Theme8 selection denominator");
    work.product(8u, 4096u);
    cases.iter([&](JsonVal row) { flow400_theme8_case_shape(row, work); });
}

inline Flow400Theme8Documents flow400_theme8_documents() {
    Flow400Theme8Documents documents;
    documents.asset_work.spend(16384u);
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) /
                      "fixtures/mermaid_native/flow400_theme8_native_supplement.v1.json";
    const auto size = std::filesystem::file_size(path);
    flow400_require(size == kFlow400Theme8AssetBytes,
                    "Flow400 Theme8 immutable asset bytes");
    documents.asset_work.product(static_cast<std::size_t>(size), 3u);
    const auto bytes = flow400_read(path);
    flow400_require(digest(bytes) == kFlow400Theme8AssetSha,
                    "Flow400 Theme8 immutable asset SHA");
    auto parsed = json::parse(bytes);
    flow400_require(parsed.has_value(), "Flow400 Theme8 asset JSON");
    documents.supplement = std::move(*parsed);
    flow400_theme8_root(documents.supplement.root(), documents.asset_work);
    return documents;
}

inline void flow400_theme8_model_shape(const Model& model, std::size_t ordinal) {
    const auto node = [&](std::string_view id, Shape shape, std::string_view label) {
        const auto found = std::ranges::find_if(
            model.nodes, [&](const Node& candidate) { return candidate.id == id; });
        flow400_require(found != model.nodes.end() && found->shape == shape &&
                            found->label == label,
                        "Flow400 Theme8 Model node identity");
    };
    const auto index = [&](std::string_view id) {
        const auto found = std::ranges::find_if(
            model.nodes, [&](const Node& candidate) { return candidate.id == id; });
        flow400_require(found != model.nodes.end(), "Flow400 Theme8 Model endpoint node");
        return static_cast<int>(found - model.nodes.begin());
    };
    const auto edge = [&](std::size_t at, std::string_view from, std::string_view to) {
        flow400_require(at < model.edges.size(), "Flow400 Theme8 Model edge count");
        const auto& value = model.edges[at];
        flow400_require(value.from == index(from) && value.to == index(to) &&
                            value.label.empty() && value.style == "solid" && !value.start &&
                            value.end,
                        "Flow400 Theme8 Model edge identity");
    };
    const bool siblings = ordinal == 137u || ordinal == 141u || ordinal == 145u ||
                          ordinal == 149u;
    if (siblings) {
        flow400_require(model.direction == "TB" && model.nodes.size() == 8u &&
                            model.edges.size() == 7u && model.groups.size() == 5u,
                        "Flow400 Theme8 siblings Model cardinality");
        node("A", Shape::Rectangle, "Fetch");
        node("B", Shape::Rectangle, "Validate");
        node("C", Shape::Rectangle, "Normalise");
        node("D", Shape::Rectangle, "Enrich");
        node("E", Shape::Cylinder, "Warehouse");
        node("F", Shape::Rectangle, "API");
        node("G", Shape::Rectangle, "Cache");
        node("H", Shape::Rectangle, "Metrics");
        edge(0u, "A", "B");
        edge(1u, "C", "D");
        edge(2u, "F", "G");
        edge(3u, "B", "C");
        edge(4u, "D", "E");
        edge(5u, "E", "F");
        edge(6u, "F", "H");
        constexpr std::array<std::string_view, 5> ids{
            {"Ingest", "Transform", "Store", "Serve", "Observe"}};
        const std::array<std::vector<std::string_view>, 5> members{{
            {"A", "B"}, {"C", "D"}, {"E"}, {"F", "G"}, {"H"}}};
        for (std::size_t at = 0u; at < ids.size(); ++at) {
            const auto& group = model.groups[at];
            flow400_require(group.id == ids[at] && group.label == ids[at] &&
                                group.direction.empty() && group.parent == -1 &&
                                group.nodes.size() == members[at].size(),
                            "Flow400 Theme8 Model group identity");
            for (std::size_t n = 0u; n < members[at].size(); ++n)
                flow400_require(group.nodes[n] == index(members[at][n]),
                                "Flow400 Theme8 Model group member");
        }
    } else {
        flow400_require(model.direction == "LR" && model.nodes.size() == 4u &&
                            model.edges.size() == 3u && model.groups.size() == 4u,
                        "Flow400 Theme8 nested Model cardinality");
        node("A", Shape::Rectangle, "one");
        node("B", Shape::Rectangle, "two");
        node("C", Shape::Rectangle, "three");
        node("D", Shape::Rectangle, "four");
        edge(0u, "A", "B");
        edge(1u, "B", "C");
        edge(2u, "C", "D");
        const auto& outer = model.groups[0];
        flow400_require(outer.id == "Outer" && outer.label == "Outer" &&
                            outer.direction.empty() && outer.parent == -1 &&
                            outer.nodes.empty(),
                        "Flow400 Theme8 nested outer group");
        const auto& inner_one = model.groups[1];
        const auto& inner_two = model.groups[2];
        const auto& sibling = model.groups[3];
        flow400_require(inner_one.id == "InnerOne" && inner_one.label == "InnerOne" &&
                            inner_one.parent == 0 &&
                            inner_one.nodes.size() == 2u &&
                            inner_one.nodes[0] == index("A") && inner_one.nodes[1] == index("B"),
                        "Flow400 Theme8 nested inner one");
        flow400_require(inner_two.id == "InnerTwo" && inner_two.label == "InnerTwo" &&
                            inner_two.parent == 0 && inner_two.nodes.size() == 1u &&
                            inner_two.nodes[0] == index("C"),
                        "Flow400 Theme8 nested inner two");
        flow400_require(sibling.id == "Sibling" && sibling.label == "Sibling" &&
                            sibling.parent == -1 && sibling.nodes.size() == 1u &&
                            sibling.nodes[0] == index("D"),
                        "Flow400 Theme8 nested sibling");
    }
}

inline JsonVal flow400_theme8_select(const BoundFlow400TypedSource4Authority& prior,
                                     JsonVal theme8, JsonVal fixture, std::size_t ordinal,
                                     std::size_t global, std::string_view source,
                                     OracleWork& work) {
    work.spend(8192u + 8u * sizeof(std::size_t) + 4u * sizeof(json::JsonVal));
    const auto cases = theme8.get("cases");
    flow400_require(cases.is_arr() && cases.size() == 8u,
                    "Flow400 Theme8 selection denominator");
    JsonVal selected;
    std::size_t matches = 0u;
    cases.iter([&](JsonVal candidate) {
        work.spend(512u);
        if (flow400_number(candidate.get("base_authority").get("ordinal")) == ordinal) {
            selected = candidate;
            ++matches;
        }
    });
    flow400_require(matches <= 1u, "Flow400 Theme8 duplicate source selection");
    if (matches == 0u) return JsonVal{};
    flow400_require(prior.kind == Flow400TypedSource4Kind::Base &&
                        prior.authority.get("model_json").is_null(),
                    "Flow400 Theme8 overlaps prior authority");
    flow400_theme8_case_shape(selected, work);
    flow400_bind(selected.get("base_authority"), fixture, ordinal, global, source, work);
    return selected;
}

inline Model flow400_theme8_model(JsonVal selected, OracleWork& work) {
    const auto bytes = flow400_text(selected.get("typed_model_json"));
    flow400_require(digest(bytes) == flow400_text(selected.get("typed_model_sha256")),
                    "Flow400 Theme8 selected Model SHA");
    work.product(bytes.size(), 32u);
    work.spend(4u * sizeof(json::JsonDoc) + 8192u);
    auto document = json::parse(bytes);
    flow400_require(document.has_value(), "Flow400 Theme8 selected Model JSON");
    auto model = flow_typed_shape_model(document->root(), work);
    flow400_theme8_model_shape(model,
                               flow400_number(selected.get("base_authority").get("ordinal")));
    return model;
}

}  // namespace loom::test::mermaid_flow
