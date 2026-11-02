// Exact source alias to the separately authenticated LinkProfile3 rank contract.
// This helper grants no capture, geometry, mutation, or matrix admission by itself.
#pragma once
#include "mermaid_flow400_linkprofile3_authority.hpp"
#include "mermaid_flow_p0_authority.hpp"

namespace loom::test::mermaid_flow {
inline constexpr std::size_t kFlowP0LengthIndex = 1u;
inline constexpr std::string_view kFlowP0LengthSourceSha =
    "db9f0f4423695f567763cfaf86fcb4c33f74ac60dd80fcfe6d92e77b8cab171b";
inline constexpr std::string_view kFlowP0LengthBaseSha =
    "d38efbfd6c7b56fed9ecc62a02377c825302e455f3f48af19dc37f826df66fe0";
inline constexpr std::string_view kFlowP0LengthProfileSha =
    "10040552ca9808a311a4b7176e448bd562b666d6725291049d84ec4c69975270";
inline constexpr std::string_view kFlowP0LengthAuthority =
    "immutable_Flow400_P0Length1_LinkProfile3";

inline bool flow_p0_length_candidate(std::size_t ordinal) {
    return ordinal == kFlowP0Pins[kFlowP0LengthIndex].ordinal;
}

inline JsonVal flow_p0_length_select(const FlowP0Documents& documents, JsonVal base,
                                     JsonVal fixture, std::size_t ordinal, std::size_t global,
                                     std::string_view source, OracleWork& work) {
    work.spend(4096u + 4u * sizeof(std::size_t));
    if (!flow_p0_length_candidate(ordinal)) return JsonVal{};
    const auto& pin = kFlowP0Pins[kFlowP0LengthIndex];
    flow400_require(pin.global == global && pin.fixture == flow400_text(fixture.get("path")) &&
                        base.get("model_json").is_null(),
                    "Flow P0 length alias disjoint source");
    flow400_bind(base, fixture, ordinal, global, source, work);
    const auto record = documents.asset.root().get("rows").at(kFlowP0LengthIndex);
    work.product(source.size(), 8u);
    flow400_require(source == flow400_text(record.get("source_utf8")) &&
                        digest(source) == pin.source_sha,
                    "Flow P0 length alias actual complete source");
    return record;
}

inline JsonVal flow_p0_length_reviewed(JsonVal supplement, OracleWork& work) {
    work.product(kFlow400LinkProfile3AssetBytes, 32u);
    work.spend(8192u);
    const auto cases = supplement.get("cases");
    flow400_require(cases.is_arr() && cases.size() == 3u,
                    "Flow P0 length reviewed case denominator");
    const auto selected = cases.at(1u);
    flow400_linkprofile3_case_shape(selected, work);
    flow400_require(
        flow400_text(selected.get("case_id")) == "link_profile_294" &&
            flow400_number(selected.get("base_authority").get("ordinal")) == 294u &&
            flow400_number(selected.get("base_authority").get("manifest_global_ordinal")) == 955u,
        "Flow P0 length reviewed exact case identity");
    flow400_require(flow400_text(selected.get("source_sha256")) == kFlowP0LengthSourceSha &&
                        flow400_text(selected.get("link_model_sha256")) == kFlowP0LengthBaseSha &&
                        flow400_text(selected.get("link_profile_sha256")) ==
                            kFlowP0LengthProfileSha,
                    "Flow P0 length reviewed fixed source/Model/Profile identities");
    return selected;
}

inline Flow400LinkProfile3Decoded flow_p0_length_model(JsonVal record, JsonVal supplement,
                                                       OracleWork& work) {
    const auto& pin = kFlowP0Pins[kFlowP0LengthIndex];
    const auto wire = group_endpoint15_canonical(record.get("model"), work);
    work.product(wire.size(), 8u);
    flow400_require(flow400_number(record.get("flow_ordinal")) == pin.ordinal &&
                        wire.size() == pin.model_bytes && digest(wire) == pin.model_sha,
                    "Flow P0 length exact complete Model");
    const auto reviewed = flow_p0_length_reviewed(supplement, work);
    const auto model_wire = flow400_text(reviewed.get("link_model_json"));
    work.product(model_wire.size(), 32u);
    work.spend(4u * sizeof(json::JsonDoc) + 8192u);
    const auto parsed = json::parse(model_wire);
    flow400_require(parsed.has_value(), "Flow P0 length reviewed base JSON");
    flow400_require(group_endpoint15_canonical(record.get("model").get("base_model"), work) ==
                        group_endpoint15_canonical(parsed->root(), work),
                    "Flow P0 length complete base Model equals reviewed Model");
    const auto tagged = record.get("model").get("tagged_group_endpoint_edges"),
               order = record.get("model").get("source_edge_order");
    flow400_require(tagged.is_arr() && tagged.size() == 0u && order.is_arr() && order.size() == 10u,
                    "Flow P0 length no omitted tagged edges");
    for (std::size_t i = 0u; i < 10u; ++i) {
        work.spend(256u);
        const auto edge = order.at(i);
        group_endpoint15_keys(edge, {"kind", "index"}, "P0 length edge order");
        flow400_require(flow400_text(edge.get("kind")) == "vertex" &&
                            flow400_number(edge.get("index")) == i,
                        "Flow P0 length exact source edge order");
    }
    const auto counts = record.get("model").get("source_semantic_counts");
    group_endpoint15_keys(
        counts,
        {"nodes", "groups", "ordinary_vertex_edges", "group_endpoint_edges", "source_edges"},
        "P0 length counts");
    flow400_require(flow400_number(counts.get("nodes")) == 11u &&
                        flow400_number(counts.get("groups")) == 0u &&
                        flow400_number(counts.get("ordinary_vertex_edges")) == 10u &&
                        flow400_number(counts.get("group_endpoint_edges")) == 0u &&
                        flow400_number(counts.get("source_edges")) == 10u,
                    "Flow P0 length complete source counts");
    auto decoded = flow400_linkprofile3_model(reviewed, work);
    const auto obligations = record.get("source_obligations"),
               minimum = obligations.get("edge_length_proposal"),
               ranks = obligations.get("proposed_node_ranks");
    flow400_require(minimum.is_arr() && minimum.size() == decoded.profile.edges.size() &&
                        ranks.is_obj() && ranks.size() == decoded.model.nodes.size(),
                    "Flow P0 length independent source profile shape");
    for (std::size_t i = 0u; i < decoded.profile.edges.size(); ++i) {
        work.spend(256u);
        flow400_require(flow400_number(minimum.at(i)) ==
                            static_cast<std::size_t>(decoded.profile.edges[i].minimum_length),
                        "Flow P0 length independent minima agree");
    }
    for (std::size_t i = 0u; i < decoded.model.nodes.size(); ++i) {
        work.product(decoded.model.nodes[i].id.size(), 8u);
        work.spend(256u);
        flow400_require(flow400_number(ranks.get(decoded.model.nodes[i].id)) ==
                            static_cast<std::size_t>(decoded.profile.node_ranks[i]),
                        "Flow P0 length independent ranks agree");
    }
    return decoded;
}
}  // namespace loom::test::mermaid_flow
