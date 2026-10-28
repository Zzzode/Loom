// Test-private adapter for the source-pinned group-endpoint rows. Geometry itself
// stays in the existing Flow oracle optional-profile branch. Every caller must
// provide its own independently attached following-row token.
#pragma once
#include "mermaid_flow400_group_endpoint15_authority.hpp"

namespace loom::test::mermaid_flow {
inline std::vector<std::string> group_endpoint15_geometry(const Grid& grid,
                                                          const GroupEndpoint15Model& model,
                                                          OracleWork& work,
                                                          std::string_view token) {
    const auto profile = group_endpoint15_profile(model, work);
    return flow_group_endpoint_geometry(grid, model.base, profile, work, token);
}

inline bool group_endpoint15_nonresource(const std::vector<std::string>& errors) {
    return !errors.empty() && std::ranges::none_of(errors, [](const std::string& error) {
        return flow400_resource(error);
    });
}

inline void group_endpoint15_copy_grid(const Grid& grid, OracleWork& work) {
    flow400_copy_grid(grid, work);
}

inline void group_endpoint15_require_rejected(const Grid& grid, const GroupEndpoint15Model& model,
                                              OracleWork& work, std::string_view why,
                                              std::string_view token) {
    bool rejected = false, resource = false;
    try {
        const auto errors = group_endpoint15_geometry(grid, model, work, token);
        rejected = group_endpoint15_nonresource(errors);
    } catch (const std::exception& error) {
        resource = work.remaining == 0u || flow400_resource(error.what());
        rejected = !resource;
    }
    if (!rejected || resource) throw std::runtime_error(std::string(why));
}

inline void group_endpoint15_copy_and_reject(const Grid& grid, const GroupEndpoint15Model& model,
                                             OracleWork& work, std::string_view why,
                                             std::string_view token, auto&& mutate) {
    group_endpoint15_copy_model(model, work);
    auto wrong = model;
    mutate(wrong);
    group_endpoint15_require_rejected(grid, wrong, work, why, token);
}

inline std::size_t group_endpoint15_semantic_mutations(const Grid& good,
                                                       const GroupEndpoint15Model& model,
                                                       OracleWork& work, std::string_view token) {
    flow400_require(!model.tagged.empty() && !model.base.nodes.empty() &&
                        !model.base.groups.empty(),
                    "GroupEndpoint15 semantic mutation witness");
    std::size_t rejected = 0u;
    group_endpoint15_copy_and_reject(
        good, model, work, "GroupEndpoint15 valid wrong endpoint kind accepted", token,
        [&](GroupEndpoint15Model& wrong) {
            wrong.tagged.front().source = {GroupEndpointKind::Vertex, wrong.base.nodes.front().id};
        });
    ++rejected;
    group_endpoint15_copy_and_reject(
        good, model, work, "GroupEndpoint15 valid wrong group accepted", token,
        [&](GroupEndpoint15Model& wrong) {
            const auto current = wrong.tagged.front().source.id;
            const auto other = std::ranges::find_if(
                wrong.base.groups, [&](const Group& group) { return group.id != current; });
            flow400_require(other != wrong.base.groups.end(),
                            "GroupEndpoint15 wrong-group witness");
            wrong.tagged.front().source.id = other->id;
        });
    ++rejected;
    group_endpoint15_copy_and_reject(
        good, model, work, "GroupEndpoint15 valid wrong direct membership accepted", token,
        [&](GroupEndpoint15Model& wrong) {
            const auto group = std::ranges::find_if(
                wrong.base.groups, [](const Group& value) { return !value.nodes.empty(); });
            flow400_require(group != wrong.base.groups.end(), "GroupEndpoint15 membership witness");
            group->nodes.erase(group->nodes.begin());
        });
    ++rejected;
    group_endpoint15_copy_and_reject(good, model, work,
                                     "GroupEndpoint15 duplicate authored edge order accepted",
                                     token, [&](GroupEndpoint15Model& wrong) {
                                         flow400_require(wrong.source_order.size() >= 2u,
                                                         "GroupEndpoint15 source order witness");
                                         wrong.source_order.back() = wrong.source_order.front();
                                     });
    ++rejected;
    return rejected;
}

inline std::size_t group_endpoint15_physical_mutations(const Grid& good,
                                                       const GroupEndpoint15Model& model,
                                                       OracleWork& work, std::string_view token) {
    std::size_t rejected = 0u;
    group_endpoint15_copy_grid(good, work);
    auto missing_head = good;
    bool changed = false;
    for (int y = 0; y < missing_head.height && !changed; ++y)
        for (int x = 0; x < missing_head.width && !changed; ++x) {
            const Point point{x, y};
            if (glyph(missing_head.at(point)).arrow >= 0) {
                missing_head.put(point, " ");
                changed = true;
            }
        }
    flow400_require(changed, "GroupEndpoint15 head mutation witness");
    group_endpoint15_require_rejected(missing_head, model, work,
                                      "GroupEndpoint15 missing endpoint head accepted", token);
    ++rejected;
    group_endpoint15_copy_grid(good, work);
    auto branch = good;
    changed = false;
    const auto footer = flow400_following(good, work, token);
    for (int y = 0; y < footer && !changed; ++y)
        for (int x = 0; x < branch.width && !changed; ++x) {
            const auto cell = branch.at({x, y});
            if (cell == "─") {
                branch.put({x, y}, "┼");
                changed = true;
            } else if (cell == "│") {
                branch.put({x, y}, "┼");
                changed = true;
            }
        }
    flow400_require(changed, "GroupEndpoint15 route branch witness");
    group_endpoint15_require_rejected(branch, model, work,
                                      "GroupEndpoint15 nonconsumed route branch accepted", token);
    ++rejected;
    group_endpoint15_copy_grid(good, work);
    auto ink = good;
    changed = false;
    for (int y = 0; y < footer && !changed; ++y)
        for (int x = 0; x < ink.width && !changed; ++x)
            if (ink.at({x, y}) == " ") {
                ink.put({x, y}, "§");
                changed = true;
            }
    flow400_require(changed, "GroupEndpoint15 extra ink witness");
    group_endpoint15_require_rejected(ink, model, work,
                                      "GroupEndpoint15 extra physical ink accepted", token);
    ++rejected;
    return rejected;
}

inline std::size_t group_endpoint15_mutations(const Grid& good, const GroupEndpoint15Model& model,
                                              OracleWork& work, std::string_view token) {
    const auto baseline = group_endpoint15_geometry(good, model, work, token);
    flow400_require(baseline.empty(), "GroupEndpoint15 mutation requires complete good baseline");
    return group_endpoint15_semantic_mutations(good, model, work, token) +
           group_endpoint15_physical_mutations(good, model, work, token);
}
}  // namespace loom::test::mermaid_flow

namespace loom::test::mermaid_flow {
// This additive selector is intentionally separate from the frozen Base8,
// Config21, Source4, Classic, and LinkProfile selectors. It returns no record
// for every unselected Flow400 tuple and binds all selected bytes before decode.
inline std::optional<BoundGroupEndpoint15>
group_endpoint15_select(JsonVal base, JsonVal fixture, JsonVal records, std::size_t ordinal,
                        std::size_t global, std::string_view source, OracleWork& work) {
    work.spend(8192u + 8u * sizeof(JsonVal) + 4u * sizeof(BoundGroupEndpoint15));
    JsonVal selected;
    std::size_t matches = 0u;
    records.iter([&](JsonVal record) {
        work.spend(512u);
        if (flow400_number(record.get("ordinal")) == ordinal &&
            flow400_number(record.get("manifest_global_ordinal")) == global) {
            selected = record;
            ++matches;
        }
    });
    flow400_require(matches <= 1u, "GroupEndpoint15 duplicate selected tuple");
    if (matches == 0u) return std::nullopt;
    return group_endpoint15_bind(base, fixture, records, selected, ordinal, global, source, work);
}

inline std::size_t group_endpoint15_expected_mutations(JsonVal record) {
    static_cast<void>(group_endpoint15_tuple(record));
    return 7u;
}
}  // namespace loom::test::mermaid_flow

namespace loom::test::mermaid_flow {
inline bool group_endpoint15_candidate_ordinal(std::size_t ordinal) {
    return std::ranges::any_of(
        kFlow400GroupEndpoint15Tuples,
        [&](const GroupEndpoint15Tuple& tuple) { return tuple.ordinal == ordinal; });
}
}  // namespace loom::test::mermaid_flow

namespace loom::test::mermaid_flow {
inline std::optional<std::size_t> group_endpoint15_tuple_index(std::size_t ordinal,
                                                               std::size_t global) {
    const auto found =
        std::ranges::find_if(kFlow400GroupEndpoint15Tuples, [&](const GroupEndpoint15Tuple& tuple) {
            return tuple.ordinal == ordinal && tuple.manifest_global_ordinal == global;
        });
    if (found == kFlow400GroupEndpoint15Tuples.end()) return std::nullopt;
    return static_cast<std::size_t>(found - kFlow400GroupEndpoint15Tuples.begin());
}
}  // namespace loom::test::mermaid_flow
