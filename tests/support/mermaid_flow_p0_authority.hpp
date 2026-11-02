// Test-private source-pinned P0 Flow graph proposal. Only the independently
// approved seven grouped records enter SourceReviewedP0; the other three keep
// their explicit source obligations and earn no admission from this loader.
#pragma once
#include "mermaid_flow_group_endpoint15_oracle.hpp"

namespace loom::test::mermaid_flow {
inline constexpr std::size_t kFlowP0AssetBytes = 64152u;
inline constexpr std::string_view kFlowP0AssetSha =
    "46e19f214ffdf8b4dd5ba25bfcf41db64a6d8f7ba88fe50fe7d2fe65170b7283";

struct FlowP0Pin {
    std::size_t ordinal, global, bytes, model_bytes;
    std::string_view fixture, source_sha, model_sha;
    bool grouped;
};

inline constexpr std::array<FlowP0Pin, 10> kFlowP0Pins{{
    {15u, 676u, 481u, 1608u,
     "flowchart/nightly-check/dagre/"
     "2050-dagre-handling-of-different-rendering-direction-in-subgraphs.mmd",
     "72366985bda8c7b42cf9018299c9c2946d22ba27aacff41d57aa6a7e615e8fa2",
     "ccc3b72afb5adcc10b162fd4f57d610f36da7a88f687db69ec269d418d6d90ff", true},
    {19u, 680u, 456u, 2079u, "flowchart/nightly-check/dagre/4-dagre-length-of-edges.mmd",
     "1d4ebceac88cec1c55f8ec1e0af730e7f87ae1beecd70108ce3b51f453c3beb9",
     "73ee7d9dd53a33c9b44dec19c1d1bdafcccda36ebe063426ced831346e0803fd", false},
    {26u, 687u, 587u, 1064u,
     "flowchart/nightly-check/dagre/55-dagre-handle-nested-subgraphs-with-outgoing-links-2.mmd",
     "84e286bea79da3522f89f78d3acb252c34d89c963afb45e97b4aeff9ec0efb57",
     "8ede6930357b2493c0617f45348d92338642813e235517521777b4c8a89faa40", true},
    {28u, 689u, 457u, 1884u,
     "flowchart/nightly-check/dagre/57-dagre-handle-nested-subgraphs-with-outgoing-links-2.mmd",
     "0da0f803ec8e2ccf162bd3ac7013a3d87778763f86788deb336faabbe258187c",
     "71e8730c3ea074cebe04ea81a8cd42de869cf3935845d18d35b2e2250133df3d", true},
    {54u, 715u, 565u, 2824u,
     "flowchart/nightly-check/dagre/"
     "7213-should-render-dagre-edges-with-right-angles-not-curves.mmd",
     "a4889b253941d911e7d1b3dae02117f72473a12d10d91bc7febf8288b8c92f53",
     "2ccbff0e6e3091db21f7b87b89521cd49c2ae80e8b714e693799706fa38f1f80", false},
    {269u, 930u, 573u, 1608u,
     "flowchart/visual-check/2050-handling-of-different-rendering-direction-in-subgraphs.mmd",
     "2c3b7a57f3edaede358e102d9b8b9e5d84e981b914739ddd1d7184a38f7ec593",
     "ccc3b72afb5adcc10b162fd4f57d610f36da7a88f687db69ec269d418d6d90ff", true},
    {303u, 964u, 296u, 741u,
     "flowchart/visual-check/"
     "5064-should-render-when-subgraph-child-has-links-to-outside-node-and-subgraph.mmd",
     "a0ab1c8f8393cb9a771cd6f40f35ec5d0b9ace5c25a6272c3b42a335da38579b",
     "8dc68569a217d9b804ba5248dac52a68b854ae0b55894904dcfb2130460fa64a", true},
    {306u, 967u, 681u, 1064u,
     "flowchart/visual-check/55-handle-nested-subgraphs-with-outgoing-links-2.mmd",
     "dd7af0ae31d1bc5ed20388d3c5f32783a39e0f0b1fb7175f4676334be607174c",
     "8ede6930357b2493c0617f45348d92338642813e235517521777b4c8a89faa40", true},
    {308u, 969u, 557u, 1884u,
     "flowchart/visual-check/57-handle-nested-subgraphs-with-outgoing-links-2.mmd",
     "4aff65938cfea8a17a085cb20ae697b5a3e613c5505c622e6cecfed2ad026483",
     "71e8730c3ea074cebe04ea81a8cd42de869cf3935845d18d35b2e2250133df3d", true},
    {357u, 1018u, 529u, 990u,
     "flowchart/visual-check/issue-4648-nested-subgraph-with-external-connection.mmd",
     "403c54cf5f29cc7de0886f4ac363fd8c3229bc3d0d4539023f1a9a15a9a560a8",
     "6099cc34612fe0780b7466ffdcceac69b7a8b71b3382e514c38e278c7cbcd121", false},
}};

struct FlowP0Documents {
    OracleWork asset_work;
    json::JsonDoc asset;
};

inline FlowP0Documents flow_p0_documents(const Flow400Documents& primary) {
    FlowP0Documents documents;
    documents.asset_work.product(kFlowP0AssetBytes, 24u);
    documents.asset_work.spend(131072u);
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) /
                      "fixtures/mermaid_native/flow_p0_10_source_models.proposal.v1.json";
    flow400_require(std::filesystem::file_size(path) == kFlowP0AssetBytes, "Flow P0 asset stat");
    const auto raw = flow400_read(path);
    flow400_require(digest(raw) == kFlowP0AssetSha, "Flow P0 asset SHA");
    auto parsed = json::parse(raw);
    flow400_require(parsed.has_value(), "Flow P0 asset JSON");
    const auto root = parsed->root();
    group_endpoint15_keys(root, {"schema", "qualification", "rows"}, "P0 source root");
    flow400_require(flow400_text(root.get("schema")) == "flow_p0_10_source_models/proposal-v1" &&
                        root.get("qualification").is_str() && root.get("rows").is_arr() &&
                        root.get("rows").size() == 10u,
                    "Flow P0 complete proposal denominator");
    for (std::size_t i = 0; i < 10u; ++i) {
        auto& work = documents.asset_work;
        work.spend(8192u + i);
        const auto row = root.get("rows").at(i);
        const auto& pin = kFlowP0Pins[i];
        group_endpoint15_keys(row,
                              {"fixture", "flow_ordinal", "manifest_global_ordinal",
                               "complete_manifest_record", "source_utf8", "source_bytes",
                               "source_sha256", "model", "canonical_model_bytes",
                               "canonical_model_sha256", "source_obligations"},
                              "P0 complete source row");
        const auto source = flow400_text(row.get("source_utf8"));
        work.product(source.size(), 16u);
        flow400_require(flow400_number(row.get("flow_ordinal")) == pin.ordinal &&
                            flow400_number(row.get("manifest_global_ordinal")) == pin.global &&
                            flow400_text(row.get("fixture")) == pin.fixture &&
                            flow400_number(row.get("source_bytes")) == pin.bytes &&
                            source.size() == pin.bytes &&
                            flow400_text(row.get("source_sha256")) == pin.source_sha &&
                            digest(source) == pin.source_sha,
                        "Flow P0 exact source tuple");
        const auto manifest = primary.manifest.root().get("fixtures").at(pin.global);
        work.spend(pin.global + 1u);
        flow400_require(group_endpoint15_canonical(manifest, work) ==
                            group_endpoint15_canonical(row.get("complete_manifest_record"), work),
                        "Flow P0 complete immutable manifest record");
        const auto canonical = group_endpoint15_canonical(row.get("model"), work);
        work.product(canonical.size(), 8u);
        flow400_require(canonical.size() == pin.model_bytes &&
                            flow400_number(row.get("canonical_model_bytes")) == pin.model_bytes &&
                            digest(canonical) == pin.model_sha &&
                            flow400_text(row.get("canonical_model_sha256")) == pin.model_sha,
                        "Flow P0 complete source Model SHA");
        if (pin.grouped) flow400_require(pin.model_bytes <= 1884u, "Flow P0 grouped wire bound");
        flow400_require(row.get("source_obligations").is_obj(),
                        "Flow P0 foreign/configuration obligations retained");
    }
    documents.asset = std::move(*parsed);
    return documents;
}

inline std::optional<std::size_t> flow_p0_group_index(std::size_t ordinal) {
    for (std::size_t i = 0; i < kFlowP0Pins.size(); ++i)
        if (kFlowP0Pins[i].grouped && kFlowP0Pins[i].ordinal == ordinal) return i;
    return {};
}

// Source-owned selection; geometry domains remain separately dispatched.
inline std::optional<std::size_t> flow_p0_admitted_index(std::size_t ordinal) {
    if (ordinal == kFlowP0Pins[1u].ordinal) return 1u;
    if (ordinal == kFlowP0Pins[4u].ordinal) return 4u;
    if (ordinal == kFlowP0Pins[9u].ordinal) return 9u;
    return flow_p0_group_index(ordinal);
}

inline JsonVal flow_p0_group_select(const FlowP0Documents& documents, JsonVal base, JsonVal fixture,
                                    std::size_t ordinal, std::size_t global,
                                    std::string_view source, OracleWork& work) {
    work.spend(4096u + 4u * sizeof(std::size_t));
    const auto selected = flow_p0_group_index(ordinal);
    if (!selected) return JsonVal{};
    const auto& pin = kFlowP0Pins[*selected];
    flow400_require(pin.global == global && pin.fixture == flow400_text(fixture.get("path")) &&
                        base.get("model_json").is_null(),
                    "Flow P0 disjoint immutable whole-null source");
    flow400_bind(base, fixture, ordinal, global, source, work);
    const auto record = documents.asset.root().get("rows").at(*selected);
    work.product(source.size(), 8u);
    flow400_require(source == flow400_text(record.get("source_utf8")) &&
                        digest(source) == pin.source_sha,
                    "Flow P0 actual complete source");
    return record;
}

inline GroupEndpoint15Model flow_p0_group_model(JsonVal record, OracleWork& work) {
    const auto ordinal = flow400_number(record.get("flow_ordinal"));
    const auto index = flow_p0_group_index(ordinal);
    flow400_require(index.has_value(), "Flow P0 grouped domain selection");
    const auto& pin = kFlowP0Pins[*index];
    const auto encoded = group_endpoint15_canonical(record.get("model"), work);
    flow400_require(encoded.size() == pin.model_bytes && digest(encoded) == pin.model_sha,
                    "Flow P0 selected exact Model");
    return group_endpoint15_model(record.get("model"), work, GroupEndpointDomain::SourceReviewedP0);
}

inline std::vector<std::string> flow_p0_group_geometry(const Grid& grid,
                                                       const GroupEndpoint15Model& model,
                                                       OracleWork& work,
                                                       std::string_view token = kSentinel) {
    const auto profile = group_endpoint15_profile(model, work);
    return flow_group_endpoint_p0_geometry(grid, model.base, profile, work, token);
}

inline void flow_p0_group_rejected(const Grid& grid, const GroupEndpoint15Model& model,
                                   OracleWork& work, std::string_view token) {
    flow400_require_decisive_negative(work, "Flow P0 group adversary", [&]() {
        return flow_p0_group_geometry(grid, model, work, token);
    });
}

inline std::size_t flow_p0_group_mutations(const Grid& grid, const GroupEndpoint15Model& model,
                                           OracleWork& work, std::string_view token = kSentinel) {
    flow400_require(flow_p0_group_geometry(grid, model, work, token).empty(),
                    "Flow P0 good-first mutation baseline");
    std::size_t count = 0;
    const auto reject = [&](auto change) {
        group_endpoint15_copy_model(model, work);
        work.spend(8192u);
        auto wrong = model;
        change(wrong);
        flow_p0_group_rejected(grid, wrong, work, token);
        ++count;
    };
    reject([](GroupEndpoint15Model& bad) { bad.base.nodes.front().label = "WRONG"; });
    reject([](GroupEndpoint15Model& bad) {
        const auto removed = bad.source_order.front();
        bad.source_order.erase(bad.source_order.begin());
        if (removed.kind == GroupEndpointOrderKind::Ordinary)
            bad.base.edges.erase(bad.base.edges.begin() +
                                 static_cast<std::ptrdiff_t>(removed.index));
        else
            bad.tagged.erase(bad.tagged.begin() + static_cast<std::ptrdiff_t>(removed.index));
        for (auto& entry : bad.source_order)
            if (entry.kind == removed.kind && entry.index > removed.index) --entry.index;
        for (std::size_t n = 0; n < bad.source_order.size(); ++n)
            if (bad.source_order[n].kind == GroupEndpointOrderKind::Tagged)
                bad.tagged[bad.source_order[n].index].source_order = n;
    });
    reject([](GroupEndpoint15Model& bad) {
        auto& target = bad.tagged.front().target;
        target = target.kind == GroupEndpointKind::Group
                     ? GroupEndpoint15WireRef{GroupEndpointKind::Vertex, bad.base.nodes.front().id}
                     : GroupEndpoint15WireRef{GroupEndpointKind::Group, bad.base.groups.front().id};
    });
    reject([](GroupEndpoint15Model& bad) {
        const auto selected = std::ranges::find_if(
            bad.base.groups, [](const Group& group) { return !group.nodes.empty(); });
        flow400_require(selected != bad.base.groups.end(), "Flow P0 owned membership witness");
        selected->nodes.erase(selected->nodes.begin());
    });
    reject([](GroupEndpoint15Model& bad) {
        bad.base.direction = bad.base.direction == "LR"   ? "RL"
                             : bad.base.direction == "RL" ? "LR"
                             : bad.base.direction == "BT" ? "TB"
                                                          : "BT";
    });
    flow400_copy_grid(grid, work);
    auto orphan = grid;
    flow400_require(orphan.at({orphan.width - 1, 0}) == " ", "Flow P0 independent orphan location");
    orphan.put({orphan.width - 1, 0}, "X");
    flow_p0_group_rejected(orphan, model, work, token);
    ++count;
    flow400_require(count == 6u, "Flow P0 exact useful adversary count");
    return count;
}
}  // namespace loom::test::mermaid_flow
