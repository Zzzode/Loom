// Source-only two-case gate to the separate restricted physical strand oracle.
#pragma once
#include "mermaid_flow_strand_oracle.hpp"

namespace loom::test::mermaid_flow {
inline constexpr std::string_view kFlowP0StrandAuthority = "immutable_Flow400_P0Strands2";

// Debit complete measured immutable discovery to each new scene as well as
// retaining the separately bounded setup ledgers. No setup work is borrowed.
inline std::size_t flow_p0_strand_discovery_charge(const Flow400Documents& primary,
                                                   const FlowP0Documents& documents,
                                                   OracleWork& work) {
    constexpr std::size_t limit = 8000000u;
    flow400_require(primary.manifest_work.remaining <= limit &&
                        primary.compact_work.remaining <= limit &&
                        documents.asset_work.remaining <= limit,
                    "Flow strand setup ledger bounds");
    const auto charged = (limit - primary.manifest_work.remaining) +
                         (limit - primary.compact_work.remaining) +
                         (limit - documents.asset_work.remaining);
    work.spend(charged);
    return charged;
}

inline std::optional<std::size_t> flow_p0_strand_index(std::size_t ordinal) {
    if (ordinal == kFlowP0Pins[4u].ordinal) return 4u;
    if (ordinal == kFlowP0Pins[9u].ordinal) return 9u;
    return {};
}

inline JsonVal flow_p0_strand_select(const FlowP0Documents& documents, JsonVal base,
                                     JsonVal fixture, std::size_t ordinal, std::size_t global,
                                     std::string_view source, OracleWork& work) {
    work.spend(8192u);
    const auto index = flow_p0_strand_index(ordinal);
    if (!index) return JsonVal{};
    const auto& pin = kFlowP0Pins[*index];
    flow400_require(global == pin.global && flow400_text(fixture.get("path")) == pin.fixture &&
                        base.get("model_json").is_null(),
                    "Flow strand disjoint complete source tuple");
    flow400_bind(base, fixture, ordinal, global, source, work);
    const auto record = documents.asset.root().get("rows").at(*index);
    work.product(source.size(), 8u);
    flow400_require(source == flow400_text(record.get("source_utf8")) &&
                        digest(source) == pin.source_sha,
                    "Flow strand actual complete original source");
    return record;
}

inline Model flow_p0_strand_model(JsonVal record, OracleWork& work) {
    const auto index = flow_p0_strand_index(flow400_number(record.get("flow_ordinal")));
    flow400_require(index.has_value(), "Flow strand closed source index");
    const auto& pin = kFlowP0Pins[*index];
    const auto encoded = group_endpoint15_canonical(record.get("model"), work);
    work.product(encoded.size(), 64u);
    work.spend(32768u);
    flow400_require(encoded.size() == pin.model_bytes && digest(encoded) == pin.model_sha,
                    "Flow strand immutable complete Model");
    const auto value = record.get("model");
    group_endpoint15_keys(value,
                          {"base_model", "tagged_group_endpoint_edges", "source_edge_order",
                           "source_semantic_counts"},
                          "strand Model");
    const auto base = value.get("base_model");
    flow400_keys(base, {"direction", "nodes", "edges", "subgraphs"});
    const auto tagged = value.get("tagged_group_endpoint_edges"),
               order = value.get("source_edge_order");
    flow400_require(tagged.is_arr() && tagged.size() == 0u && order.is_arr() &&
                        order.size() == base.get("edges").size(),
                    "Flow strand no omitted endpoints/order");
    for (std::size_t at = 0u; at < order.size(); ++at) {
        work.spend(256u);
        group_endpoint15_keys(order.at(at), {"kind", "index"}, "strand source order");
        flow400_require(flow400_text(order.at(at).get("kind")) == "vertex" &&
                            flow400_number(order.at(at).get("index")) == at,
                        "Flow strand exact source edge order");
    }
    auto model = parse_model(base);
    const auto counts = value.get("source_semantic_counts");
    group_endpoint15_keys(
        counts,
        {"nodes", "groups", "ordinary_vertex_edges", "group_endpoint_edges", "source_edges"},
        "strand counts");
    flow400_require(model.nodes.size() == (*index == 4u ? 11u : 4u) && model.groups.size() == 2u &&
                        model.edges.size() == (*index == 4u ? 14u : 3u) &&
                        flow400_number(counts.get("nodes")) == model.nodes.size() &&
                        flow400_number(counts.get("groups")) == model.groups.size() &&
                        flow400_number(counts.get("ordinary_vertex_edges")) == model.edges.size() &&
                        flow400_number(counts.get("group_endpoint_edges")) == 0u &&
                        flow400_number(counts.get("source_edges")) == model.edges.size(),
                    "Flow strand complete semantic counts");
    return model;
}
}  // namespace loom::test::mermaid_flow
