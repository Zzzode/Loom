// Test-private source-bound LinkProfile3 Flow400 overlay.
// It records the exact Base8, Config21, TypedSource4, and Classic authority chain.
#pragma once
#include "mermaid_flow400_classic_authority.hpp"

namespace loom::test::mermaid_flow {

inline constexpr std::size_t kFlow400LinkProfile3AssetBytes = 19617u;
inline constexpr std::string_view kFlow400LinkProfile3AssetSha =
    "dea7fa501e066b7b9f178661779daf8bd5f2c6a9bca4c454a0ec8bdee204b7bf";

struct Flow400LinkProfile3Documents {
    OracleWork asset_work;
    json::JsonDoc supplement;
};

struct Flow400LinkProfile3Decoded {
    Model model;
    LinkProfile profile;
    std::size_t exact_rejections = 0u;
};

inline void flow400_linkprofile3_keys(JsonVal value, std::initializer_list<std::string_view> keys,
                                      std::string_view scope) {
    if (!value.is_obj())
        throw std::runtime_error("Flow400 LinkProfile3 " + std::string(scope) + " wrong kind");
    std::array<bool, 16> seen{};
    if (keys.size() > seen.size())
        throw std::runtime_error("Flow400 LinkProfile3 internal key bound");
    value.iter_obj([&](JsonVal key, JsonVal) {
        const auto name = flow400_text(key);
        const auto at = std::ranges::find(keys, name);
        if (at == keys.end())
            throw std::runtime_error("Flow400 LinkProfile3 " + std::string(scope) + " unknown key");
        const auto index = static_cast<std::size_t>(at - keys.begin());
        if (seen[index])
            throw std::runtime_error("Flow400 LinkProfile3 " + std::string(scope) +
                                     " duplicate key");
        seen[index] = true;
    });
    if (value.size() != keys.size())
        throw std::runtime_error("Flow400 LinkProfile3 " + std::string(scope) + " missing key");
    for (std::size_t index = 0; index < keys.size(); ++index)
        if (!seen[index])
            throw std::runtime_error("Flow400 LinkProfile3 " + std::string(scope) + " missing key");
}

inline LinkMarker flow400_linkprofile3_marker(JsonVal value) {
    const auto marker = flow400_text(value);
    if (marker == "none") return LinkMarker::None;
    if (marker == "point") return LinkMarker::Point;
    throw std::runtime_error("Flow400 LinkProfile3 closed endpoint marker");
}

inline void flow400_linkprofile3_model_shape(JsonVal value) {
    flow400_linkprofile3_keys(value, {"direction", "nodes", "edges", "subgraphs"}, "Model");
    flow400_require(flow400_text(value.get("direction")) == "TD",
                    "Flow400 LinkProfile3 Model direction");
    const auto nodes = value.get("nodes"), edges = value.get("edges"),
               groups = value.get("subgraphs");
    flow400_require(nodes.is_arr() && nodes.size() > 0u && nodes.size() <= 64u && edges.is_arr() &&
                        edges.size() > 0u && edges.size() <= 128u && groups.is_arr() &&
                        groups.size() == 0u,
                    "Flow400 LinkProfile3 Model arrays");
    nodes.iter([](JsonVal pair) {
        flow400_require(pair.is_arr() && pair.size() == 2u, "Flow400 LinkProfile3 node tuple");
        const auto node = pair.at(1u);
        flow400_linkprofile3_keys(node, {"id", "label", "shape"}, "Model node");
        const auto id = flow400_text(node.get("id")), label = flow400_text(node.get("label"));
        flow400_require(flow400_text(pair.at(0u)) == id && !id.empty() && !label.empty() &&
                            label.find('\n') == std::string_view::npos &&
                            flow400_text(node.get("shape")) == "rectangle",
                        "Flow400 LinkProfile3 closed node");
    });
    edges.iter([](JsonVal edge) {
        flow400_linkprofile3_keys(
            edge, {"source", "target", "label", "style", "hasArrowStart", "hasArrowEnd"},
            "Model edge");
        static_cast<void>(flow400_text(edge.get("source")));
        static_cast<void>(flow400_text(edge.get("target")));
        static_cast<void>(flow400_text(edge.get("label")));
        const auto style = flow400_text(edge.get("style"));
        flow400_require(style == "solid" || style == "dotted" || style == "thick",
                        "Flow400 LinkProfile3 edge style");
        flow400_require(
            edge.get("hasArrowStart").is_bool() && edge.get("hasArrowEnd").is_bool() &&
                (!edge.get("hasArrowStart").as_bool() || edge.get("hasArrowEnd").as_bool()),
            "Flow400 LinkProfile3 endpoint profile");
    });
}

inline LinkProfile flow400_linkprofile3_profile(JsonVal value, const Model& model,
                                                OracleWork& work) {
    flow400_linkprofile3_keys(value, {"schema", "edgeProfiles", "nodeRanks", "rankWitness"},
                              "profile");
    flow400_require(flow400_text(value.get("schema")) == "loom.flow400.link_profile/v1",
                    "Flow400 LinkProfile3 profile schema");
    const auto edge_profiles = value.get("edgeProfiles"), node_ranks = value.get("nodeRanks"),
               witness = value.get("rankWitness");
    flow400_require(edge_profiles.is_arr() && edge_profiles.size() == model.edges.size() &&
                        node_ranks.is_arr() && node_ranks.size() == model.nodes.size(),
                    "Flow400 LinkProfile3 profile cardinality");
    flow400_linkprofile3_keys(witness,
                              {"majorGap", "nodeHeight", "profile", "rankFormula", "temporaryDag"},
                              "rank witness");
    flow400_require(
        flow400_number(witness.get("majorGap")) == 8u &&
            flow400_number(witness.get("nodeHeight")) == 5u &&
            flow400_text(witness.get("profile")) == "ungrouped_td_single_line_rectangle/v1" &&
            flow400_text(witness.get("rankFormula")) ==
                "rank(target) >= rank(source) + minimumLength" &&
            flow400_text(witness.get("temporaryDag")) ==
                "preserve current DFS back-edge suppression; propagate only retained DAG edges",
        "Flow400 LinkProfile3 rank witness");
    work.spend(4096u + 4u * sizeof(LinkProfile) + 8u * sizeof(LinkProfileEdge) + 8u * sizeof(int));
    LinkProfile result;
    result.edges.reserve(model.edges.size());
    result.node_ranks.reserve(model.nodes.size());
    edge_profiles.iter([&](JsonVal item) {
        flow400_linkprofile3_keys(item, {"minimumLength", "sourceMarker", "targetMarker"},
                                  "profile edge");
        const auto minimum = flow400_number(item.get("minimumLength"));
        flow400_require(minimum >= 1u && minimum <= 5u, "Flow400 LinkProfile3 minimum length");
        result.edges.push_back({flow400_linkprofile3_marker(item.get("sourceMarker")),
                                flow400_linkprofile3_marker(item.get("targetMarker")),
                                static_cast<int>(minimum)});
    });
    for (std::size_t at = 0u; at < model.nodes.size(); ++at) {
        const auto pair = node_ranks.at(at);
        flow400_require(pair.is_arr() && pair.size() == 2u &&
                            flow400_text(pair.at(0u)) == model.nodes[at].id,
                        "Flow400 LinkProfile3 ordered node rank");
        const auto rank = flow400_number(pair.at(1u));
        flow400_require(rank <= 63u, "Flow400 LinkProfile3 rank bound");
        result.node_ranks.push_back(static_cast<int>(rank));
    }
    flow400_require(linkprofile3_profile_shape(model, result),
                    "Flow400 LinkProfile3 Model/profile compatibility");
    return result;
}

inline std::size_t flow400_linkprofile3_expected(JsonVal row) {
    const auto ordinal = flow400_number(row.get("base_authority").get("ordinal"));
    const auto expected = flow400_number(row.get("native_requirement").get("exact_rejections"));
    flow400_require((ordinal == 19u || ordinal == 291u || ordinal == 294u || ordinal == 298u) &&
                        expected == 6u,
                    "Flow400 LinkProfile3 exact mutation count");
    return expected;
}

inline void flow400_linkprofile3_case_shape(JsonVal row, OracleWork& work) {
    work.spend(8192u);
    flow400_linkprofile3_keys(row,
                              {"case_id", "base_authority", "source", "source_sha256",
                               "link_model_json", "link_model_sha256", "link_profile_json",
                               "link_profile_sha256", "native_requirement", "formal_status"},
                              "case");
    const auto id = flow400_text(row.get("case_id"));
    const auto authority = row.get("base_authority");
    const auto ordinal = flow400_number(authority.get("ordinal"));
    flow400_keys(authority, {"adaptation", "fixture", "manifest_global_ordinal", "model_json",
                             "model_sha256", "oracle_record_sha256", "ordinal", "source_bytes",
                             "source_git_blob", "source_sha256"});
    flow400_require(authority.get("model_json").is_null() &&
                        authority.get("model_sha256").is_null() &&
                        authority.get("adaptation").is_null(),
                    "Flow400 LinkProfile3 base must remain null");
    const auto source = flow400_text(row.get("source"));
    flow400_require(source.size() == flow400_number(authority.get("source_bytes")) &&
                        digest(source) == flow400_text(row.get("source_sha256")) &&
                        flow400_text(row.get("source_sha256")) ==
                            flow400_text(authority.get("source_sha256")),
                    "Flow400 LinkProfile3 source lock");
    const auto model = flow400_text(row.get("link_model_json")),
               profile = flow400_text(row.get("link_profile_json"));
    flow400_require(model.size() <= 8192u && profile.size() <= 8192u &&
                        digest(model) == flow400_text(row.get("link_model_sha256")) &&
                        digest(profile) == flow400_text(row.get("link_profile_sha256")),
                    "Flow400 LinkProfile3 Model/Profile lock");
    work.product(source.size() + model.size() + profile.size(), 32u);
    work.spend(8u * sizeof(json::JsonDoc) + 16384u);
    auto model_document = json::parse(model), profile_document = json::parse(profile);
    flow400_require(model_document.has_value() && profile_document.has_value(),
                    "Flow400 LinkProfile3 JSON");
    flow400_linkprofile3_model_shape(model_document->root());
    const auto parsed = parse_model(model_document->root());
    static_cast<void>(flow400_linkprofile3_profile(profile_document->root(), parsed, work));
    const auto native = row.get("native_requirement");
    flow400_linkprofile3_keys(native, {"exact_rejections", "mutations"}, "native requirement");
    const auto expected = flow400_linkprofile3_expected(row);
    const auto mutations = native.get("mutations");
    flow400_require(mutations.is_arr() && mutations.size() == expected,
                    "Flow400 LinkProfile3 mutation plan");
    constexpr std::array<std::string_view, 6> first{
        {"edge_profile_marker_semantic", "minimum_length_increase", "Label_1_model_text",
         "point_marker_physical", "Label_2_caption_ownership", "extra_ink"}};
    constexpr std::array<std::string_view, 6> last{
        {"C_to_D_invented_label", "minimum_length_increase", "open_endpoint_marker_physical",
         "open_endpoint_contour_arms", "spare_caption", "extra_ink"}};
    for (std::size_t at = 0u; at < expected; ++at)
        flow400_require(flow400_text(mutations.at(at)) ==
                            (ordinal == 298u ? last.at(at) : first.at(at)),
                        "Flow400 LinkProfile3 mutation identity");
    const bool tuple = (id == "link_profile_019" && ordinal == 19u) ||
                       (id == "link_profile_291" && ordinal == 291u) ||
                       (id == "link_profile_294" && ordinal == 294u) ||
                       (id == "link_profile_298" && ordinal == 298u);
    flow400_require(tuple, "Flow400 LinkProfile3 case-id/ordinal tuple");
    flow400_require(flow400_text(row.get("formal_status")) == "unadmitted_design_only",
                    "Flow400 LinkProfile3 unadmitted source state");
}

inline void flow400_linkprofile3_root(JsonVal root, OracleWork& work) {
    flow400_linkprofile3_keys(
        root,
        {"schema", "qualification", "base_authority", "prior_authority", "formal_counts", "cases"},
        "root");
    flow400_require(flow400_text(root.get("schema")) == "flow400_linkprofile3_supplement/v1" &&
                        flow400_text(root.get("qualification")) ==
                            "Four source-pinned LinkProfile Models require independent Native and "
                            "Official runs before admission.",
                    "Flow400 LinkProfile3 schema/qualification");
    const auto base = root.get("base_authority");
    flow400_linkprofile3_keys(
        base, {"asset_sha256", "manifest_sha256", "all_sources", "complete_models", "pending"},
        "base authority");
    flow400_require(flow400_text(base.get("asset_sha256")) == kFlow400AssetSha &&
                        flow400_text(base.get("manifest_sha256")) == kFlow400ManifestSha &&
                        flow400_number(base.get("all_sources")) == 400u &&
                        flow400_number(base.get("complete_models")) == 8u &&
                        flow400_number(base.get("pending")) == 392u,
                    "Flow400 LinkProfile3 base authority");
    const auto prior = root.get("prior_authority");
    flow400_linkprofile3_keys(prior,
                              {"config21_asset_sha256", "typed_source4_asset_sha256",
                               "classic_asset_sha256", "aggregate_complete_before_linkprofile3",
                               "aggregate_pending_before_linkprofile3"},
                              "prior authority");
    flow400_require(
        flow400_text(prior.get("config21_asset_sha256")) == kFlow400Config21AssetSha &&
            flow400_text(prior.get("typed_source4_asset_sha256")) == kFlow400TypedSource4AssetSha &&
            flow400_text(prior.get("classic_asset_sha256")) == kFlow400ClassicAssetSha &&
            flow400_number(prior.get("aggregate_complete_before_linkprofile3")) == 39u &&
            flow400_number(prior.get("aggregate_pending_before_linkprofile3")) == 361u,
        "Flow400 LinkProfile3 prior authority");
    const auto counts = root.get("formal_counts");
    flow400_linkprofile3_keys(counts,
                              {"cases", "aggregate_complete_after_all_official_runs",
                               "aggregate_pending_after_all_official_runs",
                               "ultimate_complete_required", "ultimate_pending_required",
                               "official_credit_before_execution"},
                              "formal counts");
    flow400_require(
        flow400_number(counts.get("cases")) == 4u &&
            flow400_number(counts.get("aggregate_complete_after_all_official_runs")) == 43u &&
            flow400_number(counts.get("aggregate_pending_after_all_official_runs")) == 357u &&
            flow400_number(counts.get("ultimate_complete_required")) == 400u &&
            flow400_number(counts.get("ultimate_pending_required")) == 0u &&
            flow400_number(counts.get("official_credit_before_execution")) == 0u,
        "Flow400 LinkProfile3 formal counts");
    const auto cases = root.get("cases");
    flow400_require(cases.is_arr() && cases.size() == 4u, "Flow400 LinkProfile3 case count");
    std::array<bool, 400> ordinals{};
    std::array<bool, 4> identities{};
    cases.iter([&](JsonVal row) {
        flow400_linkprofile3_case_shape(row, work);
        const auto ordinal = flow400_number(row.get("base_authority").get("ordinal"));
        flow400_require(!ordinals[ordinal], "Flow400 LinkProfile3 duplicate source ordinal");
        ordinals[ordinal] = true;
        const auto id = flow400_text(row.get("case_id"));
        const auto index = id == "link_profile_019"   ? 0u
                           : id == "link_profile_291" ? 1u
                           : id == "link_profile_294" ? 2u
                                                      : 3u;
        flow400_require(!identities[index], "Flow400 LinkProfile3 duplicate case identity");
        identities[index] = true;
    });
    flow400_require(ordinals[19] && ordinals[291] && ordinals[294] && ordinals[298] &&
                        std::ranges::all_of(identities, [](bool present) { return present; }),
                    "Flow400 LinkProfile3 selected ordinal closure");
}

inline Flow400LinkProfile3Documents flow400_linkprofile3_documents() {
    Flow400LinkProfile3Documents documents;
    documents.asset_work.spend(4u * sizeof(json::JsonDoc) + 4u * sizeof(OracleWork) + 16384u);
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) /
                      "fixtures/mermaid_native/flow400_linkprofile3_supplement.v1.json";
    const auto size = std::filesystem::file_size(path);
    flow400_require(size == kFlow400LinkProfile3AssetBytes,
                    "Flow400 LinkProfile3 immutable asset bytes");
    documents.asset_work.product(static_cast<std::size_t>(size), 3u);
    const auto bytes = flow400_read(path);
    flow400_require(digest(bytes) == kFlow400LinkProfile3AssetSha,
                    "Flow400 LinkProfile3 immutable asset SHA");
    auto parsed = json::parse(bytes);
    flow400_require(parsed.has_value(), "Flow400 LinkProfile3 asset JSON");
    documents.supplement = std::move(*parsed);
    flow400_linkprofile3_root(documents.supplement.root(), documents.asset_work);
    return documents;
}

inline JsonVal flow400_linkprofile3_select(const BoundFlow400TypedSource4Authority& prior,
                                           JsonVal classic, JsonVal supplement, JsonVal fixture,
                                           std::size_t ordinal, std::size_t global,
                                           std::string_view source, OracleWork& work) {
    work.spend(8192u + 8u * sizeof(std::size_t) + 4u * sizeof(json::JsonVal));
    const auto cases = supplement.get("cases");
    flow400_require(cases.is_arr() && cases.size() == 4u,
                    "Flow400 LinkProfile3 selection denominator");
    JsonVal selected;
    std::size_t matches = 0u;
    cases.iter([&](JsonVal candidate) {
        work.spend(512u);
        if (flow400_number(candidate.get("base_authority").get("ordinal")) == ordinal) {
            selected = candidate;
            ++matches;
        }
    });
    flow400_require(matches <= 1u, "Flow400 LinkProfile3 duplicate source selection");
    if (matches == 0u) return JsonVal{};
    flow400_require(!classic.is_obj() && prior.kind == Flow400TypedSource4Kind::Base &&
                        prior.authority.get("model_json").is_null(),
                    "Flow400 LinkProfile3 overlaps prior authority");
    flow400_linkprofile3_case_shape(selected, work);
    flow400_bind(selected.get("base_authority"), fixture, ordinal, global, source, work);
    return selected;
}

inline Flow400LinkProfile3Decoded flow400_linkprofile3_model(JsonVal selected, OracleWork& work) {
    const auto model_bytes = flow400_text(selected.get("link_model_json")),
               profile_bytes = flow400_text(selected.get("link_profile_json"));
    flow400_require(digest(model_bytes) == flow400_text(selected.get("link_model_sha256")) &&
                        digest(profile_bytes) == flow400_text(selected.get("link_profile_sha256")),
                    "Flow400 LinkProfile3 selected Model/Profile SHA");
    work.product(model_bytes.size() + profile_bytes.size(), 32u);
    work.spend(8u * sizeof(json::JsonDoc) + 16384u);
    auto model_document = json::parse(model_bytes), profile_document = json::parse(profile_bytes);
    flow400_require(model_document.has_value() && profile_document.has_value(),
                    "Flow400 LinkProfile3 selected Model/Profile JSON");
    flow400_linkprofile3_model_shape(model_document->root());
    auto model = parse_model(model_document->root());
    auto profile = flow400_linkprofile3_profile(profile_document->root(), model, work);
    return {std::move(model), std::move(profile), flow400_linkprofile3_expected(selected)};
}

inline void flow400_linkprofile3_copy(const LinkProfile& profile, OracleWork& work) {
    work.spend(4u * sizeof(LinkProfile) + 256u);
    work.product(profile.edges.size(), 3u * sizeof(LinkProfileEdge) + 8u);
    work.product(profile.node_ranks.size(), 3u * sizeof(int) + 8u);
}

// One immutable captured scene can exercise several independent mutations.
// Validate its complete geometry once before copying any negative candidate.
class LinkProfileMutationScene {
public:
    LinkProfileMutationScene(const Grid& good, const Model& model, const LinkProfile& profile,
                             OracleWork& work, std::string_view token = kSentinel)
        : good_(good)
        , model_(model)
        , profile_(profile)
        , work_(work)
        , token_(token) {
        work_.spend(sizeof(LinkProfileMutationScene));
        const auto baseline = flow_linkprofile3_geometry(good_, model_, profile_, work_, token_);
        flow400_require(baseline.empty(), "Flow400 LinkProfile3 mutation scene baseline");
    }

    void physical(auto&& mutate, std::string_view name) const {
        flow400_copy_grid(good_, work_);
        auto bad = good_;
        work_.spend(2048u);
        mutate(bad);
        rejected(flow_linkprofile3_geometry(bad, model_, profile_, work_, token_), name);
    }

    void model(auto&& mutate, std::string_view name) const {
        flow400_copy_model(model_, work_);
        auto bad = model_;
        work_.spend(2048u);
        mutate(bad);
        rejected(flow_linkprofile3_geometry(good_, bad, profile_, work_, token_), name);
    }

    void profile(auto&& mutate, std::string_view name) const {
        flow400_linkprofile3_copy(profile_, work_);
        auto bad = profile_;
        work_.spend(2048u);
        mutate(bad);
        rejected(flow_linkprofile3_geometry(good_, model_, bad, work_, token_), name);
    }

private:
    static void rejected(const std::vector<std::string>& errors, std::string_view name) {
        flow400_require(!errors.empty(), "Flow400 LinkProfile3 negative admitted");
        for (const auto& error : errors)
            flow400_require(!flow400_resource(error),
                            "Flow400 LinkProfile3 negative became resource credit");
        static_cast<void>(name);
    }

    const Grid& good_;
    const Model& model_;
    const LinkProfile& profile_;
    OracleWork& work_;
    std::string_view token_;
};

inline void flow400_linkprofile3_expect_physical_rejected(const Grid& good, const Model& model,
                                                          const LinkProfile& profile,
                                                          OracleWork& work, auto&& mutate,
                                                          std::string_view name,
                                                          std::string_view token = kSentinel) {
    LinkProfileMutationScene(good, model, profile, work, token).physical(mutate, name);
}

inline void flow400_linkprofile3_expect_model_rejected(const Grid& good, const Model& model,
                                                       const LinkProfile& profile, OracleWork& work,
                                                       auto&& mutate, std::string_view name,
                                                       std::string_view token = kSentinel) {
    LinkProfileMutationScene(good, model, profile, work, token).model(mutate, name);
}

inline void flow400_linkprofile3_expect_profile_rejected(const Grid& good, const Model& model,
                                                         const LinkProfile& profile,
                                                         OracleWork& work, auto&& mutate,
                                                         std::string_view name,
                                                         std::string_view token = kSentinel) {
    LinkProfileMutationScene(good, model, profile, work, token).profile(mutate, name);
}

}  // namespace loom::test::mermaid_flow
