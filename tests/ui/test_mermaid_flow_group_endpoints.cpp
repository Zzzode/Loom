// RFC 0030 test-first group endpoint cohort. Installed only after independent review.
#include <ftxui/dom/elements.hpp>
#include <gtest/gtest.h>

#include <filesystem>
import std;
import loom.ui.visual.markdown;
import loom.ui.visual.mermaid.testing;
import loom.serdes.json;
#include "mermaid_flow400_group_endpoint_extra7_authority.hpp"
#include "mermaid_flow_extra7_controls.hpp"
#include "mermaid_flow_group_endpoint15_oracle.hpp"
#include "mermaid_flow_p0_authority.hpp"
#include "mermaid_flow_strand_mutations.hpp"

namespace {
namespace json = loom::utils::json;
using json::JsonVal;
using namespace loom::test::mermaid_flow;

std::string group_endpoint15_fenced(std::string_view text, std::string_view kind) {
    return "```" + std::string(kind) + "\n" + std::string(text) + "\n```";
}

JsonVal group_endpoint15_fixture(JsonVal fixtures, std::string_view path) {
    JsonVal found;
    std::size_t count = 0u;
    fixtures.iter([&](JsonVal fixture) {
        if (flow400_text(fixture.get("path")) == path) {
            found = fixture;
            ++count;
        }
    });
    flow400_require(count == 1u, "GroupEndpoint15 immutable fixture selection");
    return found;
}

void group_endpoint15_publish_frame(json::JsonMutVal row, std::string_view key, const Grid& grid,
                                    OracleWork& work) {
    work.product(grid.cells.size(), 8u);
    std::size_t bytes = 0u;
    for (const auto& cell : grid.cells) {
        flow400_require(cell.size() <= std::numeric_limits<std::size_t>::max() - bytes,
                        "GroupEndpoint15 frame payload overflow");
        bytes += cell.size();
    }
    work.product(bytes, 48u);
    work.product(static_cast<std::size_t>(3 * grid.height + 32), 8u);
    work.product(static_cast<std::size_t>(grid.height),
                 3u * sizeof(std::string) + 4u * sizeof(json::JsonMutVal) + 64u);
    work.spend(4u * sizeof(json::JsonDoc) + 512u);
    auto lines = row.make_arr();
    for (int y = 0; y < grid.height; ++y)
        lines.append(row.make_str(grid.row(y)));
    row.add(key, lines);
}

Grid group_endpoint_extra7_capture(std::string_view source, std::string_view kind = "mermaid") {
    OracleWork work{kFlow400SceneLimit};
    auto rendered = loom::ui::render_markdown(group_endpoint15_fenced(source, kind));
    auto element = ftxui::vbox({rendered, ftxui::text(std::string(kSentinel))});
    element->ComputeRequirement();
    constexpr int height = 64;
    flow400_require(element->requirement().min_y <= height, "Extra7 control source height");
    auto grid = capture(element, 240, height, &work);
    static_cast<void>(flow400_following(grid, work));
    return grid;
}

std::string group_endpoint_extra7_grid_digest(const Grid& grid) {
    std::string bytes = std::to_string(grid.width) + "x" + std::to_string(grid.height) + "\n";
    for (int y = 0; y < grid.height; ++y) {
        bytes += grid.row(y);
        bytes += '\n';
    }
    return digest(bytes);
}

GroupEndpoint15Model group_endpoint_extra7_manual_model(std::string direction,
                                                        std::vector<Group> groups,
                                                        std::vector<std::string> target_ids) {
    GroupEndpoint15Model model;
    model.base.direction = std::move(direction);
    for (std::size_t index = 0u; index < target_ids.size(); ++index)
        model.base.nodes.push_back({target_ids[index], target_ids[index]});
    model.base.groups = std::move(groups);
    for (std::size_t index = 0u; index < target_ids.size(); ++index) {
        model.tagged.push_back({{GroupEndpointKind::Group, "G"},
                                {GroupEndpointKind::Vertex, target_ids[index]},
                                "",
                                "solid",
                                false,
                                true,
                                index});
        model.source_order.push_back({GroupEndpointOrderKind::Tagged, index});
    }
    return model;
}

TEST(MermaidFlowGroupEndpoint15, ImmutableAuthorityRejectsForgedExactTupleAndWire) {
    auto owned = group_endpoint15_documents();
    const auto record = owned.asset.root().get("records").at(0u);
    OracleWork work{kFlow400SceneLimit};
    group_endpoint15_record(record, work);
    json::JsonMutDoc forged;
    auto root = forged.raw_json(record.to_string());
    forged.set_root(root);
    root.set("case_id", "flow400_group_endpoint_48");
    const auto parsed = json::parse(forged.to_string());
    ASSERT_TRUE(parsed.has_value());
    EXPECT_THROW(group_endpoint15_record(parsed->root(), work), std::runtime_error);
    json::JsonMutDoc wire;
    auto model = wire.raw_json(record.get("model").to_string());
    wire.set_root(model);
    model.get("tagged_group_endpoint_edges").at(0u).get("source").set("kind", "vertex");
    const auto parsed_wire = json::parse(wire.to_string());
    ASSERT_TRUE(parsed_wire.has_value());
    EXPECT_THROW(group_endpoint15_model(parsed_wire->root(), work), std::runtime_error);
}

TEST(MermaidFlowGroupEndpoint15, CompleteOriginalSourcesRequireOwnedFiveFrameGeometry) {
    auto flow400 = flow400_documents();
    auto owned = group_endpoint15_documents();
    const auto records = owned.asset.root().get("records");
    const auto fixtures = flow400.manifest.root().get("fixtures"),
               cases = flow400.compact.root().get("cases");
    std::size_t complete = 0u, fallbacks = 0u, resources = 0u, published = 0u, rejected = 0u;
    constexpr std::array<std::string_view, 5> names{{"wide_frame", "narrow_frame", "restored_frame",
                                                     "ordinary_wide_frame",
                                                     "ordinary_narrow_frame"}};
    records.iter([&](JsonVal record) {
        OracleWork work{kFlow400SceneLimit};
        work.spend(65536u + 32u * sizeof(Grid) + 16u * sizeof(GroupEndpoint15Model) +
                   8u * sizeof(json::JsonMutVal) + 4096u);
        bool resource = false;
        try {
            const auto ordinal = flow400_number(record.get("ordinal"));
            const auto global = flow400_number(record.get("manifest_global_ordinal"));
            const auto path = flow400_text(record.get("fixture"));
            const auto fixture = group_endpoint15_fixture(fixtures, path);
            const auto source_path = std::filesystem::path(LOOM_TESTS_DIR) /
                                     "fixtures/mermaid_official/cases" / std::string(path);
            work.product(flow400_number(fixture.get("bytes")), 32u);
            work.product(path.size(), 8u);
            const auto source = flow400_read(source_path);
            flow400_require(source == flow400_text(record.get("raw_source_utf8")),
                            "GroupEndpoint15 immutable full source differs");
            flow400_require(source.size() == flow400_number(fixture.get("bytes")) &&
                                digest(source) == flow400_text(fixture.get("sha256")),
                            "GroupEndpoint15 actual fixture SHA");
            auto mermaid = loom::ui::render_markdown(group_endpoint15_fenced(source, "mermaid"));
            auto ordinary = loom::ui::render_markdown(group_endpoint15_fenced(source, "text"));
            auto element = ftxui::vbox({mermaid, ftxui::text(std::string(kSentinel))});
            auto plain = ftxui::vbox({ordinary, ftxui::text(std::string(kSentinel))});
            element->ComputeRequirement();
            plain->ComputeRequirement();
            const int height =
                std::max({32, element->requirement().min_y + 8, plain->requirement().min_y + 8});
            flow400_require(height <= 1040, "GroupEndpoint15 bounded source height");
            json::JsonMutDoc publication;
            auto row = publication.object();
            std::array<std::optional<Grid>, 5> frames;
            constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
            for (std::size_t index = 0u; index < frames.size(); ++index) {
                frames[index] = capture(index < 3u ? element : plain, widths[index], height, &work);
                group_endpoint15_publish_frame(row, names[index], *frames[index], work);
                ++published;
            }
            flow400_require(published % 5u == 0u && row.size() == 5u,
                            "GroupEndpoint15 five full frames attached before bind");
            for (const auto& frame : frames)
                static_cast<void>(flow400_following(*frame, work));
            const bool source_wide = flow400_same(*frames[0], *frames[3], work),
                       source_narrow = flow400_same(*frames[1], *frames[4], work),
                       restored = flow400_same(*frames[0], *frames[2], work);
            flow400_require(restored, "GroupEndpoint15 restored wide cells differ");
            group_endpoint15_bind(cases.at(ordinal), fixture, records, record, ordinal, global,
                                  source, work);
            const auto model = group_endpoint15_model(record.get("model"), work);
            if (source_wide) {
                ++fallbacks;
                throw std::runtime_error(
                    "GroupEndpoint15 mandatory original returned ordinary source");
            }
            const auto wide = group_endpoint15_geometry(*frames[0], model, work, kSentinel);
            flow400_require(wide.empty(), wide.empty() ? "" : wide.front().c_str());
            if (!source_narrow) {
                const auto narrow = group_endpoint15_geometry(*frames[1], model, work, kSentinel);
                flow400_require(narrow.empty(), narrow.empty() ? "" : narrow.front().c_str());
            }
            const auto restored_errors =
                group_endpoint15_geometry(*frames[2], model, work, kSentinel);
            flow400_require(restored_errors.empty(),
                            restored_errors.empty() ? "" : restored_errors.front().c_str());
            const auto local = group_endpoint15_mutations(*frames[0], model, work, kSentinel);
            flow400_require(local == 7u, "GroupEndpoint15 exact meaningful source mutations");
            rejected += local;
            ++complete;
        } catch (const std::exception& error) {
            resource = work.remaining == 0u || flow400_resource(error.what());
        }
        resources += resource;
        EXPECT_FALSE(resource);
        EXPECT_NE(work.remaining, 0u);
    });
    EXPECT_EQ(published, 75u);
    EXPECT_EQ(complete, 15u);
    EXPECT_EQ(fallbacks, 0u);
    EXPECT_EQ(resources, 0u);
    EXPECT_EQ(rejected, 105u);
}

struct GroupEndpoint15HandControlSpec {
    std::string_view id, basis, mutation, must_reject, scene_id;
};

constexpr std::array<GroupEndpoint15HandControlSpec, 8> kGroupEndpoint15HandControlSpecs{{
    {"semantic_group_to_wrong_child", "cluster_to_cluster_then_cluster_to_child_td_hand_good",
     "Change the tagged group:two target from vertex:c2 to vertex:b1 while retaining a well-formed "
     "decoded Model and the Grid.",
     "The grid owns the tagged target at child c2, not sibling child b1.",
     "cluster_to_cluster_then_cluster_to_child_td_hand_good"},
    {"semantic_wrong_sibling_group", "cluster_to_cluster_plus_child_crossing_td_hand_good",
     "Change the tagged S1→S2 source to sibling group S2 while retaining a well-formed decoded "
     "Model and the Grid.",
     "Named group endpoint identity is exact.",
     "cluster_to_cluster_plus_child_crossing_td_hand_good"},
    {"semantic_group_to_child_source", "cluster_to_cluster_then_cluster_to_child_td_hand_good",
     "Change the group:two source tag of group_to_child to vertex:b1 in a well-formed decoded "
     "Model without changing the Grid.",
     "The grid owns group two perimeter, not child b1.",
     "cluster_to_cluster_then_cluster_to_child_td_hand_good"},
    {"physical_tangential_group_contact", "cluster_to_cluster_plus_child_crossing_td_hand_good",
     "Replace the downward S1 group-source junction on its bottom straight border with a "
     "right-facing junction.",
     "A tangential contact is not a legal outward group endpoint.",
     "cluster_to_cluster_plus_child_crossing_td_hand_good"},
    {"physical_unclaimed_group_shortcut", "cluster_to_cluster_plus_child_crossing_td_hand_good",
     "Insert an unconsumed container-border junction.",
     "Perimeter arms require exclusive route ownership.",
     "cluster_to_cluster_plus_child_crossing_td_hand_good"},
    {"semantic_crossing_boundary_as_group_target",
     "cluster_to_cluster_plus_child_crossing_td_hand_good",
     "Retag child-crossing ordinary edge sub1→sub4 as tagged sub1→group:S1 in a rebuilt "
     "well-formed Model while retaining the Grid.",
     "The S1 boundary crossing is not the tagged endpoint; the Grid still ends at child sub4.",
     "cluster_to_cluster_plus_child_crossing_td_hand_good"},
    {"model_missing_direct_membership", "cluster_to_cluster_plus_child_crossing_td_hand_good",
     "Remove sub4 from S2 direct membership in a rebuilt well-formed Model.",
     "Existing group membership obligations remain active.",
     "cluster_to_cluster_plus_child_crossing_td_hand_good"},
    {"model_duplicate_source_order", "cluster_to_cluster_plus_child_crossing_td_hand_good",
     "Reuse source_edge_order index 1 and omit index 2.",
     "Ordinary plus tagged source order is an exact partition.",
     "cluster_to_cluster_plus_child_crossing_td_hand_good"},
}};

struct GroupEndpoint15HandBinding {
    std::string_view scene_id, source_case_id, topology;
};

constexpr std::array<GroupEndpoint15HandBinding, 8> kGroupEndpoint15HandBindings{{
    {"cluster_to_cluster_then_cluster_to_child_td_hand_good", "flow400_group_endpoint_46",
     "cluster_to_cluster_then_cluster_to_child"},
    {"cluster_to_cluster_then_cluster_to_child_bt_hand_good", "flow400_group_endpoint_46",
     "cluster_to_cluster_then_cluster_to_child"},
    {"cluster_to_cluster_then_cluster_to_child_lr_hand_good", "flow400_group_endpoint_46",
     "cluster_to_cluster_then_cluster_to_child"},
    {"cluster_to_cluster_then_cluster_to_child_rl_hand_good", "flow400_group_endpoint_46",
     "cluster_to_cluster_then_cluster_to_child"},
    {"cluster_to_cluster_plus_child_crossing_td_hand_good", "flow400_group_endpoint_51",
     "cluster_to_cluster_plus_child_crossing"},
    {"cluster_to_cluster_plus_child_crossing_bt_hand_good", "flow400_group_endpoint_51",
     "cluster_to_cluster_plus_child_crossing"},
    {"cluster_to_cluster_plus_child_crossing_lr_hand_good", "flow400_group_endpoint_51",
     "cluster_to_cluster_plus_child_crossing"},
    {"cluster_to_cluster_plus_child_crossing_rl_hand_good", "flow400_group_endpoint_51",
     "cluster_to_cluster_plus_child_crossing"},
}};

JsonVal group_endpoint15_hand_scene(JsonVal root, std::string_view id) {
    JsonVal found;
    std::size_t count = 0u;
    const auto scenes = root.get("good_scenes");
    scenes.iter([&](JsonVal scene) {
        if (flow400_text(scene.get("id")) == id) {
            found = scene;
            ++count;
        }
    });
    flow400_require(count == 1u, "GroupEndpoint15 declared control hand scene");
    return found;
}

JsonVal group_endpoint15_hand_route(JsonVal scene, std::string_view role) {
    JsonVal found;
    std::size_t count = 0u;
    scene.get("routes").iter([&](JsonVal route) {
        if (flow400_text(route.get("edge_role")) == role) {
            found = route;
            ++count;
        }
    });
    flow400_require(count == 1u, "GroupEndpoint15 declared control route");
    return found;
}

JsonVal group_endpoint15_hand_frame(JsonVal scene, std::string_view id) {
    JsonVal found;
    std::size_t count = 0u;
    scene.get("frames").iter([&](JsonVal frame) {
        if (flow400_text(frame.get("id")) == id) {
            found = frame;
            ++count;
        }
    });
    flow400_require(count == 1u, "GroupEndpoint15 declared control frame");
    return found;
}

Point group_endpoint15_hand_point(JsonVal point) {
    flow400_require(point.is_arr() && point.size() == 2u, "GroupEndpoint15 hand point");
    return {static_cast<int>(flow400_number(point.at(0u))),
            static_cast<int>(flow400_number(point.at(1u)))};
}

JsonVal group_endpoint15_hand_record(JsonVal records, std::string_view case_id) {
    JsonVal found;
    std::size_t count = 0u;
    records.iter([&](JsonVal record) {
        if (flow400_text(record.get("case_id")) == case_id) {
            found = record;
            ++count;
        }
    });
    flow400_require(count == 1u, "GroupEndpoint15 hand source record");
    return found;
}

std::string group_endpoint15_directionless_canonical(JsonVal wire, OracleWork& work) {
    work.product(kFlow400GroupEndpoint15ModelWireBytes, 32u);
    work.spend(4u * sizeof(json::JsonMutDoc) + 1024u);
    json::JsonMutDoc document;
    auto root = document.copy_val(wire);
    auto base = root.get("base_model");
    flow400_require(base.remove("direction"), "GroupEndpoint15 hand Model direction");
    base.add("direction", base.make_str("__directionless__"));
    document.set_root(root);
    const auto encoded = document.to_string();
    work.product(encoded.size(), 8u);
    const auto parsed = json::parse(encoded);
    flow400_require(parsed.has_value(), "GroupEndpoint15 directionless Model JSON");
    return group_endpoint15_canonical(parsed->root(), work);
}

struct GroupEndpoint15HandModel {
    JsonVal record;
    GroupEndpoint15Model model;
};

GroupEndpoint15HandModel group_endpoint15_hand_model(JsonVal hand_root, JsonVal records,
                                                     JsonVal scene, OracleWork& work) {
    const auto scene_id = flow400_text(scene.get("id"));
    const auto topology = flow400_text(scene.get("source_derived_obligation"));
    const auto direction = flow400_text(scene.get("direction"));
    const auto binding = std::ranges::find_if(kGroupEndpoint15HandBindings, [&](const auto& value) {
        return value.scene_id == scene_id;
    });
    flow400_require(binding != kGroupEndpoint15HandBindings.end() && binding->topology == topology,
                    "GroupEndpoint15 hand binding map");
    JsonVal asset_binding;
    std::size_t binding_count = 0u;
    hand_root.get("hand_model_bindings").iter([&](JsonVal value) {
        group_endpoint15_keys(value, {"scene_id", "source_case_id", "topology"},
                              "hand Model binding");
        if (flow400_text(value.get("scene_id")) == scene_id) {
            asset_binding = value;
            ++binding_count;
        }
    });
    flow400_require(binding_count == 1u &&
                        flow400_text(asset_binding.get("source_case_id")) ==
                            binding->source_case_id &&
                        flow400_text(asset_binding.get("topology")) == binding->topology,
                    "GroupEndpoint15 immutable hand binding");
    const auto record = group_endpoint15_hand_record(records, binding->source_case_id);
    flow400_require(flow400_text(record.get("topology")) == topology,
                    "GroupEndpoint15 hand selected topology");
    const auto canonical = group_endpoint15_directionless_canonical(record.get("model"), work);
    std::size_t equivalent = 0u;
    records.iter([&](JsonVal value) {
        if (flow400_text(value.get("topology")) == topology) {
            ++equivalent;
            flow400_require(group_endpoint15_directionless_canonical(value.get("model"), work) ==
                                canonical,
                            "GroupEndpoint15 hand source Model differs beyond direction");
        }
    });
    const std::size_t expected = topology == "cluster_to_cluster_then_cluster_to_child" ? 8u : 7u;
    flow400_require(equivalent == expected, "GroupEndpoint15 hand topology source set");
    auto model = group_endpoint15_model(record.get("model"), work);
    model.base.direction = std::string(direction);
    return {record, std::move(model)};
}

Grid group_endpoint15_hand_grid(JsonVal scene, OracleWork& work) {
    const int width = static_cast<int>(flow400_number(scene.get("grid_width"))),
              height = static_cast<int>(flow400_number(scene.get("grid_height")));
    flow400_require(width > 0 && width <= 240 && height > 0 && height <= 1040,
                    "GroupEndpoint15 hand dimensions");
    const auto cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    work.spend(4u * sizeof(Grid) + 8u * sizeof(std::size_t) + 256u);
    work.product(cells, 3u * sizeof(std::string) + 16u);
    Grid grid{width, height, {}};
    grid.cells.assign(cells, " ");
    const auto put = [&](int x, int y, std::string_view cell) {
        flow400_require(grid.has({x, y}), "GroupEndpoint15 hand coordinate bound");
        grid.put({x, y}, std::string(cell));
    };
    scene.get("frames").iter([&](JsonVal value) {
        const auto rect = value.get("rect");
        const int left = static_cast<int>(flow400_number(rect.at(0u))),
                  top = static_cast<int>(flow400_number(rect.at(1u))),
                  right = static_cast<int>(flow400_number(rect.at(2u))),
                  bottom = static_cast<int>(flow400_number(rect.at(3u)));
        for (int x = left + 1; x < right; ++x) {
            put(x, top, "─");
            put(x, bottom, "─");
        }
        for (int y = top + 1; y < bottom; ++y) {
            put(left, y, "│");
            put(right, y, "│");
        }
        put(left, top, "┌");
        put(right, top, "┐");
        put(left, bottom, "└");
        put(right, bottom, "┘");
        const auto start = value.get("label_start");
        const auto id = flow400_text(value.get("id"));
        for (std::size_t index = 0u; index < id.size(); ++index)
            put(static_cast<int>(flow400_number(start.at(0u)) + index),
                static_cast<int>(flow400_number(start.at(1u))), id.substr(index, 1u));
    });
    scene.get("routes").iter([&](JsonVal route) {
        const auto source = route.get("source_port"), target = route.get("target_port"),
                   head = route.get("arrow_head");
        const auto source_point = source.get("point"), target_point = target.get("point"),
                   head_point = head.get("point");
        put(static_cast<int>(flow400_number(source_point.at(0u))),
            static_cast<int>(flow400_number(source_point.at(1u))),
            flow400_text(source.get("glyph")));
        put(static_cast<int>(flow400_number(target_point.at(0u))),
            static_cast<int>(flow400_number(target_point.at(1u))),
            flow400_text(target.get("glyph")));
        put(static_cast<int>(flow400_number(head_point.at(0u))),
            static_cast<int>(flow400_number(head_point.at(1u))), flow400_text(head.get("glyph")));
        route.get("route_glyphs").iter([&](JsonVal cell) {
            put(static_cast<int>(flow400_number(cell.at(0u))),
                static_cast<int>(flow400_number(cell.at(1u))), flow400_text(cell.at(2u)));
        });
    });
    const int sentinel = static_cast<int>(flow400_number(scene.get("sentinel_row")));
    const auto token = flow400_text(scene.get("exact_following_token"));
    for (std::size_t index = 0u; index < token.size(); ++index)
        put(static_cast<int>(index), sentinel, token.substr(index, 1u));
    return grid;
}

void group_endpoint15_wire_model_and_reject(JsonVal record, const Grid& grid, OracleWork& work,
                                            std::string_view why, std::string_view token,
                                            auto&& mutate) {
    work.product(kFlow400GroupEndpoint15ModelWireBytes, 48u);
    work.spend(8u * sizeof(json::JsonMutDoc) + 2048u);
    json::JsonMutDoc document;
    auto root = document.copy_val(record.get("model"));
    mutate(document, root);
    document.set_root(root);
    const auto encoded = document.to_string();
    work.product(encoded.size(), 8u);
    const auto parsed = json::parse(encoded);
    flow400_require(parsed.has_value(), "GroupEndpoint15 wrong Model JSON");
    const auto wrong = group_endpoint15_model(parsed->root(), work);
    group_endpoint15_require_rejected(grid, wrong, work, why, token);
}

void group_endpoint15_wire_decode_reject(JsonVal record, OracleWork& work, std::string_view why,
                                         auto&& mutate) {
    work.product(kFlow400GroupEndpoint15ModelWireBytes, 48u);
    work.spend(8u * sizeof(json::JsonMutDoc) + 2048u);
    json::JsonMutDoc document;
    auto root = document.copy_val(record.get("model"));
    mutate(document, root);
    document.set_root(root);
    const auto encoded = document.to_string();
    work.product(encoded.size(), 8u);
    const auto parsed = json::parse(encoded);
    flow400_require(parsed.has_value(), "GroupEndpoint15 adversarial Model JSON");
    bool rejected = false, resource = false;
    try {
        static_cast<void>(group_endpoint15_model(parsed->root(), work));
    } catch (const std::exception& error) {
        resource = work.remaining == 0u || flow400_resource(error.what());
        rejected = !resource;
    }
    flow400_require(rejected && !resource, std::string(why).c_str());
}

void group_endpoint15_wire_replace_ref(json::JsonMutVal ref, std::string_view kind,
                                       std::string_view id) {
    flow400_require(ref.remove("kind") && ref.remove("id"), "GroupEndpoint15 mutable endpoint ref");
    ref.add("kind", ref.make_str(kind));
    ref.add("id", ref.make_str(id));
}

void group_endpoint15_wire_crossing_as_group_target(json::JsonMutDoc&, json::JsonMutVal root) {
    auto base = root.get("base_model");
    auto old_edges = base.get("edges"), old_tagged = root.get("tagged_group_endpoint_edges");
    flow400_require(old_edges.is_arr() && old_edges.size() == 2u && old_tagged.is_arr() &&
                        old_tagged.size() == 1u,
                    "GroupEndpoint15 crossing wire witness");
    auto first_edge = old_edges.at(0u);
    auto retained = root.make_obj();
    retained.set("source", first_edge.get("source").as_str());
    retained.set("target", first_edge.get("target").as_str());
    retained.set("label", first_edge.get("label").as_str());
    retained.set("style", first_edge.get("style").as_str());
    retained.set("hasArrowStart", first_edge.get("hasArrowStart").as_bool());
    retained.set("hasArrowEnd", first_edge.get("hasArrowEnd").as_bool());
    auto ordinary = root.make_arr();
    ordinary.append(retained);
    flow400_require(base.remove("edges"), "GroupEndpoint15 crossing ordinary replace");
    base.add("edges", ordinary);
    auto first_tagged = old_tagged.at(0u);
    auto old_source = first_tagged.get("source"), old_target = first_tagged.get("target");
    auto tagged = root.make_arr(), kept = root.make_obj(), kept_source = kept.make_obj(),
         kept_target = kept.make_obj();
    kept_source.set("kind", old_source.get("kind").as_str());
    kept_source.set("id", old_source.get("id").as_str());
    kept_target.set("kind", old_target.get("kind").as_str());
    kept_target.set("id", old_target.get("id").as_str());
    kept.add("source", kept_source);
    kept.add("target", kept_target);
    kept.set("label", first_tagged.get("label").as_str());
    kept.set("style", first_tagged.get("style").as_str());
    kept.set("hasArrowStart", first_tagged.get("hasArrowStart").as_bool());
    kept.set("hasArrowEnd", first_tagged.get("hasArrowEnd").as_bool());
    kept.set("source_order", first_tagged.get("source_order").as_int());
    tagged.append(kept);
    auto edge = root.make_obj(), source = edge.make_obj(), target = edge.make_obj();
    source.set("kind", "vertex");
    source.set("id", "sub1");
    target.set("kind", "group");
    target.set("id", "S1");
    edge.add("source", source);
    edge.add("target", target);
    edge.set("label", "");
    edge.set("style", "solid");
    edge.set("hasArrowStart", false);
    edge.set("hasArrowEnd", true);
    edge.set("source_order", 2);
    tagged.append(edge);
    flow400_require(root.remove("tagged_group_endpoint_edges"),
                    "GroupEndpoint15 crossing tagged replace");
    root.add("tagged_group_endpoint_edges", tagged);
    auto order = root.make_arr(), vertex = root.make_obj(), tag0 = root.make_obj(),
         tag1 = root.make_obj();
    vertex.set("kind", "vertex");
    vertex.set("index", 0);
    tag0.set("kind", "group_endpoint");
    tag0.set("index", 0);
    tag1.set("kind", "group_endpoint");
    tag1.set("index", 1);
    order.append(vertex);
    order.append(tag0);
    order.append(tag1);
    flow400_require(root.remove("source_edge_order"), "GroupEndpoint15 crossing order replace");
    root.add("source_edge_order", order);
    auto counts = root.make_obj();
    counts.set("nodes", 3);
    counts.set("groups", 2);
    counts.set("ordinary_vertex_edges", 1);
    counts.set("group_endpoint_edges", 2);
    counts.set("source_edges", 3);
    flow400_require(root.remove("source_semantic_counts"),
                    "GroupEndpoint15 crossing count replace");
    root.add("source_semantic_counts", counts);
}

std::size_t group_endpoint15_hand_controls(JsonVal scene, const GroupEndpoint15Model& model,
                                           Grid good, OracleWork& work) {
    const auto token = flow400_text(scene.get("exact_following_token"));
    const auto good_errors = group_endpoint15_geometry(
        good, model, work, flow400_text(scene.get("exact_following_token")));
    flow400_require(good_errors.empty(), good_errors.empty() ? "" : good_errors.front().c_str());
    std::size_t rejected = group_endpoint15_semantic_mutations(good, model, work, token);
    flow400_require(rejected == 4u, "GroupEndpoint15 hand semantic negative count");
    const auto routes = scene.get("routes");
    flow400_require(routes.size() >= 2u, "GroupEndpoint15 hand route witnesses");
    const auto mutate_cell = [&](Grid& bad, JsonVal route, std::string_view field,
                                 std::string_view replacement) {
        const auto point = route.get(field).get("point");
        bad.put({static_cast<int>(flow400_number(point.at(0u))),
                 static_cast<int>(flow400_number(point.at(1u)))},
                std::string(replacement));
    };
    group_endpoint15_copy_grid(good, work);
    auto broken_reciprocal = good;
    const auto first_route = routes.at(0u);
    const auto source = first_route.get("source_port");
    const auto outward = flow400_text(source.get("outward"));
    mutate_cell(broken_reciprocal, first_route, "source_port",
                outward == "U"   ? "┬"
                : outward == "D" ? "┴"
                : outward == "L" ? "├"
                                 : "┤");
    group_endpoint15_require_rejected(broken_reciprocal, model, work,
                                      "GroupEndpoint15 broken reciprocal source arm accepted",
                                      token);
    ++rejected;
    group_endpoint15_copy_grid(good, work);
    auto head_on_perimeter = good;
    mutate_cell(head_on_perimeter, first_route, "target_port",
                flow400_text(first_route.get("arrow_head").get("glyph")));
    group_endpoint15_require_rejected(head_on_perimeter, model, work,
                                      "GroupEndpoint15 arrowhead on target perimeter accepted",
                                      token);
    ++rejected;
    group_endpoint15_copy_grid(good, work);
    auto extra_branch = good;
    const auto cells = first_route.get("route_glyphs");
    flow400_require(cells.size() > 0u, "GroupEndpoint15 hand path witness");
    const auto cell = cells.at(0u);
    extra_branch.put({static_cast<int>(flow400_number(cell.at(0u))),
                      static_cast<int>(flow400_number(cell.at(1u)))},
                     "┼");
    group_endpoint15_require_rejected(extra_branch, model, work,
                                      "GroupEndpoint15 unconsumed route branch accepted", token);
    ++rejected;
    group_endpoint15_copy_grid(good, work);
    auto crossing = good;
    const auto second_route = routes.at(1u);
    const auto second_cells = second_route.get("route_glyphs");
    flow400_require(second_cells.size() > 0u, "GroupEndpoint15 crossing witness");
    const auto cross = second_cells.at(0u);
    crossing.put({static_cast<int>(flow400_number(cross.at(0u))),
                  static_cast<int>(flow400_number(cross.at(1u)))},
                 "┼");
    group_endpoint15_require_rejected(crossing, model, work,
                                      "GroupEndpoint15 nonconsumed group crossing accepted", token);
    ++rejected;
    return rejected;
}

std::size_t group_endpoint15_declared_hand_control(JsonVal hand_root, JsonVal records,
                                                   JsonVal control, OracleWork& work) {
    group_endpoint15_keys(control, {"id", "basis", "mutation", "must_reject"},
                          "GroupEndpoint15 declared hand control");
    const auto id = flow400_text(control.get("id"));
    const auto found = std::ranges::find_if(kGroupEndpoint15HandControlSpecs,
                                            [&](const auto& spec) { return spec.id == id; });
    flow400_require(found != kGroupEndpoint15HandControlSpecs.end(),
                    "GroupEndpoint15 declared hand control id");
    flow400_require(flow400_text(control.get("basis")) == found->basis &&
                        flow400_text(control.get("mutation")) == found->mutation &&
                        flow400_text(control.get("must_reject")) == found->must_reject,
                    "GroupEndpoint15 declared hand control text");
    const auto scene = group_endpoint15_hand_scene(hand_root, found->scene_id);
    const auto hand = group_endpoint15_hand_model(hand_root, records, scene, work);
    auto good = group_endpoint15_hand_grid(scene, work);
    const auto token = flow400_text(scene.get("exact_following_token"));
    const auto baseline = group_endpoint15_geometry(good, hand.model, work, token);
    flow400_require(baseline.empty(),
                    "GroupEndpoint15 declared control requires complete good scene");
    const auto reject_reason = flow400_text(control.get("must_reject"));
    if (id == "semantic_group_to_wrong_child") {
        group_endpoint15_wire_model_and_reject(
            hand.record, good, work, reject_reason, token,
            [](json::JsonMutDoc&, json::JsonMutVal root) {
                group_endpoint15_wire_replace_ref(
                    root.get("tagged_group_endpoint_edges").at(1u).get("target"), "vertex", "b1");
            });
    } else if (id == "semantic_wrong_sibling_group") {
        group_endpoint15_wire_model_and_reject(
            hand.record, good, work, reject_reason, token,
            [](json::JsonMutDoc&, json::JsonMutVal root) {
                group_endpoint15_wire_replace_ref(
                    root.get("tagged_group_endpoint_edges").at(0u).get("source"), "group", "S2");
            });
    } else if (id == "semantic_group_to_child_source") {
        group_endpoint15_wire_model_and_reject(
            hand.record, good, work, reject_reason, token,
            [](json::JsonMutDoc&, json::JsonMutVal root) {
                group_endpoint15_wire_replace_ref(
                    root.get("tagged_group_endpoint_edges").at(1u).get("source"), "vertex", "b1");
            });
    } else if (id == "physical_tangential_group_contact") {
        group_endpoint15_copy_grid(good, work);
        auto bad = good;
        const auto route = group_endpoint15_hand_route(scene, "group_to_group");
        const auto port = group_endpoint15_hand_point(route.get("source_port").get("point"));
        flow400_require(bad.at(port) == "┬", "GroupEndpoint15 tangential source witness");
        bad.put(port, "├");
        group_endpoint15_require_rejected(bad, hand.model, work, reject_reason, token);
    } else if (id == "physical_unclaimed_group_shortcut") {
        group_endpoint15_copy_grid(good, work);
        auto bad = good;
        const auto rect = group_endpoint15_hand_frame(scene, "S1").get("rect");
        const Point shortcut{
            static_cast<int>((flow400_number(rect.at(0u)) + flow400_number(rect.at(2u))) / 2u),
            static_cast<int>(flow400_number(rect.at(3u)))};
        flow400_require(bad.at(shortcut) == "─",
                        "GroupEndpoint15 unclaimed container border witness");
        bad.put(shortcut, "┼");
        group_endpoint15_require_rejected(bad, hand.model, work, reject_reason, token);
    } else if (id == "semantic_crossing_boundary_as_group_target") {
        group_endpoint15_wire_model_and_reject(hand.record, good, work, reject_reason, token,
                                               group_endpoint15_wire_crossing_as_group_target);
    } else if (id == "model_missing_direct_membership") {
        group_endpoint15_wire_model_and_reject(
            hand.record, good, work, reject_reason, token,
            [](json::JsonMutDoc&, json::JsonMutVal root) {
                auto groups = root.get("base_model").get("subgraphs");
                json::JsonMutVal group;
                groups.iter([&](json::JsonMutVal value, std::size_t) {
                    if (value.get("id").as_str() == "S2") group = value;
                });
                flow400_require(group.valid(), "GroupEndpoint15 membership wire group");
                auto members = group.get("nodeIds"), replacement = group.make_arr();
                members.iter([&](json::JsonMutVal value, std::size_t) {
                    if (value.as_str() != "sub4")
                        replacement.append(replacement.make_str(value.as_str()));
                });
                flow400_require(group.remove("nodeIds"), "GroupEndpoint15 membership wire replace");
                group.add("nodeIds", replacement);
            });
    } else if (id == "model_duplicate_source_order") {
        group_endpoint15_wire_decode_reject(
            hand.record, work, reject_reason, [](json::JsonMutDoc&, json::JsonMutVal root) {
                auto item = root.get("source_edge_order").at(2u);
                flow400_require(item.remove("kind"), "GroupEndpoint15 order kind mutation");
                item.set("kind", "group_endpoint");
                flow400_require(item.remove("index"), "GroupEndpoint15 order mutation");
                item.set("index", 0);
            });
    } else
        throw std::runtime_error("GroupEndpoint15 declared hand control dispatch");
    flow400_require(work.remaining != 0u, "GroupEndpoint15 declared hand control resource");
    return 1u;
}

TEST(MermaidFlowGroupEndpoint15, GeometryRequiresCallerOwnedFollowingToken) {
    auto documents = group_endpoint15_documents();
    auto hands = group_endpoint15_hand_documents();
    const auto records = documents.asset.root().get("records");
    const auto root = hands.asset.root();
    const auto scene =
        group_endpoint15_hand_scene(root, "cluster_to_cluster_then_cluster_to_child_td_hand_good");
    OracleWork work{kFlow400SceneLimit};
    const auto hand = group_endpoint15_hand_model(root, records, scene, work);
    const auto good = group_endpoint15_hand_grid(scene, work);
    const auto token = flow400_text(scene.get("exact_following_token"));
    ASSERT_TRUE(group_endpoint15_geometry(good, hand.model, work, token).empty());
    bool rejected = false;
    try {
        static_cast<void>(
            group_endpoint15_geometry(good, hand.model, work, "WRONG_GROUP_ENDPOINT_TOKEN"));
    } catch (const std::runtime_error& error) {
        rejected =
            std::string_view(error.what()) == "Flow400 protocol exact independent following row";
    }
    EXPECT_TRUE(rejected);
    EXPECT_NE(work.remaining, 0u);
}

TEST(MermaidFlowGroupEndpoint15, FrontierQuotientCycleSuppressesOnlyOwnerLocalCycleRankChecks) {
    auto documents = group_endpoint15_documents();
    auto hands = group_endpoint15_hand_documents();
    const auto records = documents.asset.root().get("records"), root = hands.asset.root();
    const auto cycle_scene =
        group_endpoint15_hand_scene(root, "cluster_to_cluster_then_cluster_to_child_td_hand_good");
    OracleWork cycle_work{kFlow400SceneLimit};
    const auto cycle = group_endpoint15_hand_model(root, records, cycle_scene, cycle_work);
    const auto cycle_grid = group_endpoint15_hand_grid(cycle_scene, cycle_work);
    const auto cycle_errors =
        group_endpoint15_geometry(cycle_grid, cycle.model, cycle_work,
                                  flow400_text(cycle_scene.get("exact_following_token")));
    EXPECT_TRUE(cycle_errors.empty());
    EXPECT_NE(cycle_work.remaining, 0u);
    const auto crossing_scene =
        group_endpoint15_hand_scene(root, "cluster_to_cluster_plus_child_crossing_td_hand_good");
    OracleWork crossing_work{kFlow400SceneLimit};
    const auto crossing = group_endpoint15_hand_model(root, records, crossing_scene, crossing_work);
    const auto crossing_grid = group_endpoint15_hand_grid(crossing_scene, crossing_work);
    const auto crossing_errors =
        group_endpoint15_geometry(crossing_grid, crossing.model, crossing_work,
                                  flow400_text(crossing_scene.get("exact_following_token")));
    EXPECT_TRUE(crossing_errors.empty());
    group_endpoint15_copy_model(crossing.model, crossing_work);
    auto wrong = crossing.model;
    wrong.base.direction = "BT";
    group_endpoint15_require_rejected(crossing_grid, wrong, crossing_work,
                                      "GroupEndpoint15 acyclic frontier rank reversal accepted",
                                      flow400_text(crossing_scene.get("exact_following_token")));
    EXPECT_NE(crossing_work.remaining, 0u);
}

TEST(MermaidFlowGroupEndpoint15, PublicDeclaredGroupToVertexOwnsCompleteGeometryAndClosedGrammar) {
    constexpr std::string_view source = "flowchart TD\nsubgraph G\nA\nend\nG --> B\n";
    constexpr std::string_view source_sha =
        "64039709c31f9e5a7f0fa860a10fce6f294188b06afff420d60fe622cd09aded";
    const GroupEndpoint15Model model{
        Model{"TD", {{"A", "A"}, {"B", "B"}}, {}, {{"G", "G", "", -1, {0}}}},
        {{{GroupEndpointKind::Group, "G"},
          {GroupEndpointKind::Vertex, "B"},
          "",
          "solid",
          false,
          true,
          0u}},
        {{GroupEndpointOrderKind::Tagged, 0u}}};
    OracleWork work{kFlow400SceneLimit};
    work.spend(65536u + 8u * sizeof(Grid) + 8u * sizeof(GroupEndpoint15Model) +
               8u * sizeof(json::JsonMutVal) + 4096u);
    work.product(source.size(), 32u);
    ASSERT_EQ(digest(source), source_sha);
    auto mermaid = loom::ui::render_markdown(group_endpoint15_fenced(source, "mermaid"));
    auto ordinary = loom::ui::render_markdown(group_endpoint15_fenced(source, "text"));
    auto element = ftxui::vbox({mermaid, ftxui::text(std::string(kSentinel))});
    auto plain = ftxui::vbox({ordinary, ftxui::text(std::string(kSentinel))});
    element->ComputeRequirement();
    plain->ComputeRequirement();
    const int height =
        std::max({32, element->requirement().min_y + 8, plain->requirement().min_y + 8});
    ASSERT_LE(height, 1040);
    json::JsonMutDoc publication;
    auto row = publication.object();
    std::array<std::optional<Grid>, 5> frames;
    constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
    constexpr std::array<std::string_view, 5> names{
        {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
    for (std::size_t index = 0u; index < frames.size(); ++index) {
        frames[index] = capture(index < 3u ? element : plain, widths[index], height, &work);
        group_endpoint15_publish_frame(row, names[index], *frames[index], work);
    }
    ASSERT_EQ(row.size(), 5u);
    for (const auto& frame : frames)
        static_cast<void>(flow400_following(*frame, work, kSentinel));
    const bool source_wide = flow400_same(*frames[0], *frames[3], work);
    const bool source_narrow = flow400_same(*frames[1], *frames[4], work);
    ASSERT_FALSE(source_wide);
    ASSERT_TRUE(flow400_same(*frames[0], *frames[2], work));
    const auto wide = group_endpoint15_geometry(*frames[0], model, work, kSentinel);
    ASSERT_TRUE(wide.empty()) << wide.front();
    if (!source_narrow) {
        const auto narrow = group_endpoint15_geometry(*frames[1], model, work, kSentinel);
        ASSERT_TRUE(narrow.empty()) << narrow.front();
    }
    const auto restored = group_endpoint15_geometry(*frames[2], model, work, kSentinel);
    ASSERT_TRUE(restored.empty()) << restored.front();

    group_endpoint15_copy_model(model, work);
    auto child_source = model;
    child_source.tagged.front().source = {GroupEndpointKind::Vertex, "A"};
    group_endpoint15_require_rejected(*frames[0], child_source, work,
                                      "GroupEndpoint15 public group-to-child substitution accepted",
                                      kSentinel);
    group_endpoint15_copy_grid(*frames[0], work);
    auto missing_marker = *frames[0];
    bool removed = false;
    for (int y = 0; y < missing_marker.height && !removed; ++y)
        for (int x = 0; x < missing_marker.width && !removed; ++x) {
            if (glyph(missing_marker.at({x, y})).arrow >= 0) {
                missing_marker.put({x, y}, " ");
                removed = true;
            }
        }
    ASSERT_TRUE(removed);
    group_endpoint15_require_rejected(missing_marker, model, work,
                                      "GroupEndpoint15 public missing endpoint marker accepted",
                                      kSentinel);
    group_endpoint15_copy_grid(*frames[0], work);
    auto extra_ink = *frames[0];
    const auto footer = flow400_following(*frames[0], work, kSentinel);
    bool placed = false;
    for (int y = 0; y < footer && !placed; ++y)
        for (int x = 0; x < extra_ink.width && !placed; ++x) {
            if (extra_ink.at({x, y}) == " ") {
                extra_ink.put({x, y}, "§");
                placed = true;
            }
        }
    ASSERT_TRUE(placed);
    group_endpoint15_require_rejected(extra_ink, model, work,
                                      "GroupEndpoint15 public extra ink accepted", kSentinel);
    EXPECT_NE(work.remaining, 0u);
}

TEST(MermaidFlowGroupEndpoint15, HandControlsCoverTagKindsMembershipAndRouteArmOwnership) {
    auto documents = group_endpoint15_documents();
    auto hands = group_endpoint15_hand_documents();
    const auto records = documents.asset.root().get("records");
    const auto hand_root = hands.asset.root();
    const auto scenes = hand_root.get("good_scenes"), controls = hand_root.get("negative_controls");
    ASSERT_EQ(scenes.size(), 8u);
    ASSERT_EQ(controls.size(), kGroupEndpoint15HandControlSpecs.size());
    const auto expected =
        flow400_number(hand_root.get("expected_nonresource_rejections_per_good_scene"));
    std::size_t good = 0u, generic_rejected = 0u, declared_rejected = 0u;
    scenes.iter([&](JsonVal scene) {
        OracleWork work{kFlow400SceneLimit};
        auto hand = group_endpoint15_hand_model(hand_root, records, scene, work);
        auto grid = group_endpoint15_hand_grid(scene, work);
        const auto local = group_endpoint15_hand_controls(scene, hand.model, std::move(grid), work);
        EXPECT_EQ(local, expected);
        generic_rejected += local;
        EXPECT_NE(work.remaining, 0u);
        ++good;
    });
    std::array<bool, kGroupEndpoint15HandControlSpecs.size()> seen{};
    controls.iter([&](JsonVal control) {
        const auto id = flow400_text(control.get("id"));
        const auto index = static_cast<std::size_t>(
            std::ranges::find_if(kGroupEndpoint15HandControlSpecs,
                                 [&](const auto& spec) { return spec.id == id; }) -
            kGroupEndpoint15HandControlSpecs.begin());
        ASSERT_LT(index, seen.size());
        ASSERT_FALSE(seen[index]);
        seen[index] = true;
        OracleWork work{kFlow400SceneLimit};
        declared_rejected +=
            group_endpoint15_declared_hand_control(hand_root, records, control, work);
        EXPECT_NE(work.remaining, 0u);
    });
    EXPECT_TRUE(std::ranges::all_of(seen, [](bool value) { return value; }));
    EXPECT_EQ(good, 8u);
    EXPECT_EQ(generic_rejected, 64u);
    EXPECT_EQ(declared_rejected, 8u);
}

TEST(MermaidFlowGroupEndpointExtra7, SevenPinnedSourceModelsDecodeWithBoundedSourcePositions) {
    auto documents = group_endpoint_extra7_documents();
    const auto records = documents.asset.root().get("records");
    std::size_t visited = 0u, tagged = 0u, ordinary = 0u;
    records.iter([&](JsonVal record) {
        OracleWork work{kFlow400SceneLimit};
        ASSERT_NO_THROW(group_endpoint_extra7_record(record, work));
        const auto model = group_endpoint_extra7_model(record.get("model"), work);
        const auto profile = group_endpoint_extra7_profile(model, work);
        EXPECT_LE(model.tagged.size(), 3u);
        EXPECT_EQ(profile.max_target_incoming, 2u);
        tagged += model.tagged.size();
        ordinary += model.base.edges.size();
        ++visited;
        EXPECT_NE(work.remaining, 0u);
    });
    EXPECT_EQ(visited, 7u);
    EXPECT_EQ(tagged, 15u);
    EXPECT_EQ(ordinary, 15u);
}

TEST(MermaidFlowGroupEndpointExtra7, NativeSourcesNeedFivePublishedFramesAndCompleteGeometry) {
    auto documents = group_endpoint_extra7_documents();
    auto control_documents = group_endpoint_extra7_control_documents();
    auto upstream = flow400_documents();
    const auto records = documents.asset.root().get("records"),
               fixtures = upstream.manifest.root().get("fixtures");
    const auto source_controls = control_documents.controls.root();
    json::JsonMutDoc local_verdict;
    auto local_cases = local_verdict.array();
    constexpr std::array<std::string_view, 5> names{{"wide_frame", "narrow_frame", "restored_frame",
                                                     "ordinary_wide_frame",
                                                     "ordinary_narrow_frame"}};
    std::size_t visited = 0u, wide_drawn = 0u, restored_drawn = 0u, narrow_drawn = 0u,
                narrow_source_fallback = 0u;
    std::size_t wide_errors = 0u, restored_errors = 0u, narrow_errors = 0u,
                semantic_rejections = 0u, physical_rejections = 0u, decoder_successes = 0u,
                control_resources = 0u;
    records.iter([&](JsonVal record) {
        OracleWork work{kFlow400SceneLimit};
        const auto tuple = group_endpoint_extra7_tuple(record);
        const auto fixture = group_endpoint15_fixture(fixtures, tuple.fixture);
        const auto source_path = std::filesystem::path(LOOM_TESTS_DIR) /
                                 "fixtures/mermaid_official/cases" / std::string(tuple.fixture);
        const auto source = flow400_read(source_path);
        ASSERT_EQ(source, flow400_text(record.get("raw_source_utf8")));
        ASSERT_EQ(source.size(), flow400_number(fixture.get("bytes")));
        ASSERT_EQ(digest(source), flow400_text(fixture.get("sha256")));
        const auto mermaid = loom::ui::render_markdown(group_endpoint15_fenced(source, "mermaid"));
        const auto ordinary = loom::ui::render_markdown(group_endpoint15_fenced(source, "text"));
        auto element = ftxui::vbox({mermaid, ftxui::text(std::string(kSentinel))});
        auto plain = ftxui::vbox({ordinary, ftxui::text(std::string(kSentinel))});
        element->ComputeRequirement();
        plain->ComputeRequirement();
        const int height =
            std::max({32, element->requirement().min_y + 8, plain->requirement().min_y + 8});
        ASSERT_LE(height, 1040);
        json::JsonMutDoc publication;
        auto published = publication.object();
        std::array<std::optional<Grid>, 5> frames;
        std::array<std::string, 5> frame_hashes{};
        constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
        for (std::size_t index = 0u; index < frames.size(); ++index) {
            frames[index] = capture(index < 3u ? element : plain, widths[index], height, &work);
            group_endpoint15_publish_frame(published, names[index], *frames[index], work);
            frame_hashes[index] = group_endpoint_extra7_frame_sha256(*frames[index], &work);
        }
        ASSERT_EQ(published.size(), 5u) << tuple.fixture;
        for (const auto& frame : frames)
            static_cast<void>(flow400_following(*frame, work));
        ASSERT_TRUE(flow400_same(*frames[0], *frames[2], work)) << tuple.fixture;
        const bool wide_source = flow400_same(*frames[0], *frames[3], work);
        const bool narrow_is_source = flow400_same(*frames[1], *frames[4], work);
        ASSERT_FALSE(wide_source) << "Source fallback is not a drawing: " << tuple.fixture;

        group_endpoint_extra7_record(record, work);
        const auto model = group_endpoint_extra7_model(record.get("model"), work);
        const auto profile = group_endpoint_extra7_profile(model, work);
        GroupEndpointGeometryWitness witness;
        const auto wide = flow_group_endpoint_geometry_with_witness(*frames[0], model.base, profile,
                                                                    work, kSentinel, witness);
        if (!wide.empty())
            ++wide_errors;
        else
            ++wide_drawn;
        EXPECT_TRUE(wide.empty()) << tuple.fixture << ": " << (wide.empty() ? "" : wide.front());
        const auto restored =
            flow_group_endpoint_geometry(*frames[2], model.base, profile, work, kSentinel);
        if (!restored.empty())
            ++restored_errors;
        else
            ++restored_drawn;
        EXPECT_TRUE(restored.empty())
            << tuple.fixture << ": " << (restored.empty() ? "" : restored.front());
        if (!narrow_is_source) {
            const auto narrow =
                flow_group_endpoint_geometry(*frames[1], model.base, profile, work, kSentinel);
            if (!narrow.empty())
                ++narrow_errors;
            else
                ++narrow_drawn;
            EXPECT_TRUE(narrow.empty())
                << tuple.fixture << ": " << (narrow.empty() ? "" : narrow.front());
        } else
            ++narrow_source_fallback;
        const auto control_row = group_endpoint_extra7_control_row(source_controls, tuple);
        const auto controls = group_endpoint_extra7_run_controls(
            record, control_row, *frames[0], model, profile, witness, work, "native_raw");
        semantic_rejections += controls.semantic_rejections;
        physical_rejections += controls.physical_rejections;
        decoder_successes += controls.decoder_successes;
        group_endpoint_extra7_append_case(local_verdict, local_cases, record, frame_hashes,
                                          narrow_is_source ? "source_fallback" : "drawn", controls);
        EXPECT_NE(work.remaining, 0u) << tuple.fixture;
        ++visited;
    });
    RecordProperty("wide_complete_drawn_sources", static_cast<int>(wide_drawn));
    RecordProperty("restored_complete_drawn_sources", static_cast<int>(restored_drawn));
    RecordProperty("narrow_complete_drawn_sources", static_cast<int>(narrow_drawn));
    RecordProperty("narrow_exact_source_fallbacks", static_cast<int>(narrow_source_fallback));
    RecordProperty("wide_geometry_errors", static_cast<int>(wide_errors));
    RecordProperty("restored_geometry_errors", static_cast<int>(restored_errors));
    RecordProperty("narrow_geometry_errors", static_cast<int>(narrow_errors));
    RecordProperty("semantic_mutations_rejected", static_cast<int>(semantic_rejections));
    RecordProperty("physical_mutations_rejected", static_cast<int>(physical_rejections));
    RecordProperty("control_resource_failures", static_cast<int>(control_resources));
    EXPECT_EQ(visited, 7u);
    EXPECT_EQ(wide_drawn, 7u);
    EXPECT_EQ(restored_drawn, 7u);
    EXPECT_EQ(narrow_drawn + narrow_source_fallback, 7u);
    EXPECT_EQ(narrow_drawn, 3u);
    EXPECT_EQ(narrow_source_fallback, 4u);
    EXPECT_EQ(wide_errors, 0u);
    EXPECT_EQ(restored_errors, 0u);
    EXPECT_EQ(narrow_errors, 0u);
    EXPECT_EQ(semantic_rejections, 35u);
    EXPECT_EQ(physical_rejections, 28u);
    EXPECT_EQ(decoder_successes, 35u);
    EXPECT_EQ(control_resources, 0u);
    group_endpoint_extra7_finish_verdict(
        local_verdict, local_cases, "flow400_extra7_native_local_verdict/v1", "native_raw",
        "test_mermaid_flow_semantics", "test_mermaid_flow_semantics", visited, visited * 5u,
        visited * 5u, semantic_rejections, physical_rejections, control_resources, 0u);
    group_endpoint_extra7_write_atomic(std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                                           "mermaid-flow400-extra7-native-verdict.json",
                                       local_verdict.to_string() + "\n");
}

TEST(MermaidFlowGroupEndpointExtra7, NonDescendantRouteContractsAndStableDigests) {
    // Captured before the group-to-descendant route change from implementation
    // source SHA-256 3ae0b50c629954b8b6ba176cb617044eeea718ba2563d375caa268225cef4cc1
    // using: ctest --preset debug -R
    // MermaidFlowGroupEndpointExtra7.NonDescendantRouteContractsAndStableDigests.
    constexpr std::array<std::pair<std::string_view, std::string_view>, 3> controls{{
        {"group_to_external", "flowchart TD\nsubgraph G\nA\nend\nG --> B\n"},
        {"vertex_to_group", "flowchart TD\nA\nsubgraph G\nB\nend\nA --> G\n"},
        {"ordinary_edge", "flowchart TD\nA --> B\n"},
    }};
    constexpr std::array<std::string_view, 3> expected{{
        "077c3d6c18f8da7f1bf9e22ba304bf779b7900497ea008c745ab5900b2c67e1d",
        "05ca123e3e918c18559506a46419678086743d2f621f3319da8793de41829f66",
        "0ecb74155ea54002dd5fb189b7cbcb63a487ee35a68692d5a949356043fcf4f2",
    }};
    std::size_t index = 0u;
    for (const auto& [name, source] : controls) {
        const auto grid = group_endpoint_extra7_capture(source);
        EXPECT_EQ(group_endpoint_extra7_grid_digest(grid), expected[index]) << name;
        ++index;
        const auto ordinary = group_endpoint_extra7_capture(source, "text");
        OracleWork work{kFlow400SceneLimit};
        EXPECT_FALSE(flow400_same(grid, ordinary, work))
            << name << " unexpectedly remained source fallback";
    }

    auto external = group_endpoint_extra7_manual_model("TD", {{"G", "G", "", -1, {0}}}, {"B"});
    external.base.nodes.insert(external.base.nodes.begin(), {"A", "A"});
    external.tagged.front().target.id = "B";
    const auto external_grid = group_endpoint_extra7_capture(controls[0].second);
    OracleWork external_work{kFlow400SceneLimit};
    const auto external_errors =
        group_endpoint15_geometry(external_grid, external, external_work, kSentinel);
    EXPECT_TRUE(external_errors.empty())
        << (external_errors.empty() ? "" : external_errors.front());

    GroupEndpoint15Model incoming;
    incoming.base.direction = "TD";
    incoming.base.nodes = {{"A", "A"}, {"B", "B"}};
    incoming.base.groups = {{"G", "G", "", -1, {1}}};
    incoming.tagged.push_back({{GroupEndpointKind::Vertex, "A"},
                               {GroupEndpointKind::Group, "G"},
                               "",
                               "solid",
                               false,
                               true,
                               0u});
    incoming.source_order.push_back({GroupEndpointOrderKind::Tagged, 0u});
    const auto incoming_grid = group_endpoint_extra7_capture(controls[1].second);
    OracleWork incoming_work{kFlow400SceneLimit};
    const auto incoming_errors =
        group_endpoint15_geometry(incoming_grid, incoming, incoming_work, kSentinel);
    EXPECT_TRUE(incoming_errors.empty())
        << (incoming_errors.empty() ? "" : incoming_errors.front());

    Model ordinary_model{"TD", {{"A", "A"}, {"B", "B"}}, {{0, 1, "", "solid", false, true}}, {}};
    const auto ordinary_grid = group_endpoint_extra7_capture(controls[2].second);
    OracleWork ordinary_work{kFlow400SceneLimit};
    const auto ordinary_errors =
        flow400_geometry(ordinary_grid, ordinary_model, ordinary_work, kSentinel);
    EXPECT_TRUE(ordinary_errors.empty())
        << (ordinary_errors.empty() ? "" : ordinary_errors.front());
}

TEST(MermaidFlowGroupEndpointExtra7, GroupToDescendantRoutesEnterFramesInAllDirections) {
    constexpr std::array<std::string_view, 4> directions{{"TD", "BT", "LR", "RL"}};
    for (const auto direction : directions) {
        const auto source =
            "flowchart " + std::string(direction) + "\nsubgraph G\nA\nend\nG --> A\n";
        auto model = group_endpoint_extra7_manual_model(std::string(direction),
                                                        {{"G", "G", "", -1, {0}}}, {"A"});
        const auto grid = group_endpoint_extra7_capture(source);
        OracleWork work{kFlow400SceneLimit};
        const auto errors = group_endpoint15_geometry(grid, model, work, kSentinel);
        EXPECT_TRUE(errors.empty()) << direction << ": " << (errors.empty() ? "" : errors.front());
        EXPECT_NE(work.remaining, 0u) << direction;
    }
}

TEST(MermaidFlowGroupEndpointExtra7, GroupToNestedDescendantCrossesIntermediateFrames) {
    constexpr std::string_view source =
        "flowchart TD\nsubgraph G\nsubgraph H\nA\nend\nend\nG --> A\n";
    auto model = group_endpoint_extra7_manual_model(
        "TD", {{"G", "G", "", -1, {}}, {"H", "H", "", 0, {0}}}, {"A"});
    const auto grid = group_endpoint_extra7_capture(source);
    OracleWork work{kFlow400SceneLimit};
    const auto errors = group_endpoint15_geometry(grid, model, work, kSentinel);
    EXPECT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
    EXPECT_NE(work.remaining, 0u);
}

TEST(MermaidFlowGroupEndpointExtra7, CompetingGroupSourcesUseDistinctPorts) {
    constexpr std::string_view source = "flowchart TD\nsubgraph G\nA\nB\nend\nG --> A\nG --> B\n";
    auto model = group_endpoint_extra7_manual_model("TD", {{"G", "G", "", -1, {0, 1}}}, {"A", "B"});
    const auto grid = group_endpoint_extra7_capture(source);
    OracleWork work{kFlow400SceneLimit};
    const auto errors = group_endpoint15_geometry(grid, model, work, kSentinel);
    EXPECT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
    EXPECT_NE(work.remaining, 0u);
}

}  // namespace

namespace {
// Independent geometric papers: boxes and routes are declared separately from
// their semantic Models. No renderer, captured pixels or geometry output authors
// either side. Rotations re-place horizontal labels in the rotated rectangles.
struct FlowP0Paper {
    Grid grid;
    Model model;
    GroupEndpointProfile profile;
};

FlowP0Paper flow_p0_paper(int kind, int turn, OracleWork& setup) {
    setup.spend(80u * 72u * (2u * sizeof(std::string) + 16u) + 4u * 64u * 64u * sizeof(int) +
                16384u);
    FlowP0Paper out{Grid{80, 72, std::vector<std::string>(80u * 72u, " ")}, {}, {}};
    std::array<int, 64u * 64u> bits{}, arrows{};
    arrows.fill(-1);
    const auto index = [](Point p) { return static_cast<std::size_t>(p.y * 64 + p.x); };
    const auto add = [&](Point p, int mask) { bits[index(p)] |= mask; };

    struct Box {
        int l, t, r, b;
        std::string_view label;
    };

    const std::vector<Box> boxes =
        kind == 3
            ? std::vector<Box>{{0, 0, 16, 16, "S"},
                               {30, 0, 50, 18, "T"},
                               {4, 2, 12, 4, "A"},
                               {4, 12, 12, 14, "A"},
                               {35, 8, 44, 12, "C"}}
            : std::vector<Box>{{0, 0, 16, 6, "S1"}, {0, 10, 16, 16, "S2"}, {30, 0, 50, 18, "T"},
                               {4, 2, 12, 4, "A"},  {4, 12, 12, 14, "B"},  {35, 8, 44, 12, "C"}};
    for (const auto& b : boxes) {
        for (int x = b.l + 1; x < b.r; ++x) {
            add({x, b.t}, L | R);
            add({x, b.b}, L | R);
        }
        for (int y = b.t + 1; y < b.b; ++y) {
            add({b.l, y}, U | D);
            add({b.r, y}, U | D);
        }
        add({b.l, b.t}, R | D);
        add({b.r, b.t}, L | D);
        add({b.l, b.b}, U | R);
        add({b.r, b.b}, U | L);
    }
    const auto line = [&](std::initializer_list<Point> waypoints, bool head = true) {
        auto from = *waypoints.begin();
        for (auto next = waypoints.begin() + 1; next != waypoints.end(); ++next) {
            const int direction = group_endpoint_direction(from, *next);
            const auto delta = kSteps[static_cast<std::size_t>(direction)];
            while (from != *next) {
                const Point to{from.x + delta.x, from.y + delta.y};
                add(from, kBits[static_cast<std::size_t>(direction)]);
                add(to, kBits[static_cast<std::size_t>((direction + 2) % 4)]);
                from = to;
            }
        }
        if (head) arrows[index(from)] = 1;
    };
    if (kind == 4) {
        line({{12, 3}, {20, 3}, {20, 10}}, false);
        line({{12, 13}, {20, 13}, {20, 10}}, false);
        line({{20, 10}, {31, 10}}, false);
        line({{31, 10}, {31, 9}, {34, 9}});
        line({{31, 10}, {31, 11}, {34, 11}});
    } else if (kind == 5) {
        line({{12, 3}, {20, 3}}, false);
        line({{20, 3}, {31, 3}, {31, 9}, {34, 9}});
        line({{20, 3}, {20, 11}, {34, 11}});
    } else if (kind == 6) {
        line({{12, 3}, {31, 3}, {31, 9}, {34, 9}});
        line({{12, 13}, {32, 13}, {32, 9}, {34, 9}});
    } else if (kind == 3) {
        line({{12, 13}, {32, 13}, {32, 11}, {34, 11}});
    } else if (kind == 0) {
        line({{12, 3}, {31, 3}, {31, 9}, {34, 9}});
        line({{12, 13}, {32, 13}, {32, 11}, {34, 11}});
    } else if (kind == 1) {
        line({{16, 3}, {29, 3}});
        line({{16, 13}, {29, 13}});
    } else {
        line({{12, 3}, {31, 3}, {31, 9}, {34, 9}});
        line({{30, 13}, {28, 13}, {28, 11}, {34, 11}});
    }
    const auto rotate = [&](Point p) {
        for (int n = 0; n < turn; ++n)
            p = {63 - p.y, p.x};
        return p;
    };
    const auto ink = [](int mask) -> std::string_view {
        switch (mask) {
        case U | D:
            return "│";
        case L | R:
            return "─";
        case R | D:
            return "┌";
        case L | D:
            return "┐";
        case U | R:
            return "└";
        case U | L:
            return "┘";
        case U | D | R:
            return "├";
        case U | D | L:
            return "┤";
        case L | R | D:
            return "┬";
        case L | R | U:
            return "┴";
        case U | D | L | R:
            return "┼";
        default:
            throw std::runtime_error("Flow P0 independent paper arm");
        }
    };
    constexpr std::array<std::string_view, 4> heads{"▲", "▶", "▼", "◀"};
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const Point point{x, y};
            const auto at = index(point);
            if (!bits[at]) continue;
            const auto target = rotate(point);
            if (arrows[at] >= 0)
                out.grid.put(target,
                             std::string(heads[static_cast<std::size_t>((arrows[at] + turn) % 4)]));
            else {
                int mask = 0;
                for (int n = 0; n < 4; ++n)
                    if (bits[at] & kBits[static_cast<std::size_t>(n)])
                        mask |= kBits[static_cast<std::size_t>((n + turn) % 4)];
                out.grid.put(target, std::string(ink(mask)));
            }
        }
    for (const auto& b : boxes) {
        const auto a = rotate({b.l, b.t}), z = rotate({b.r, b.b});
        const int l = std::min(a.x, z.x), t = std::min(a.y, z.y);
        for (std::size_t n = 0; n < b.label.size(); ++n)
            out.grid.put({l + 1 + static_cast<int>(n), t + 1}, std::string(1, b.label[n]));
    }
    for (std::size_t n = 0; n < kSentinel.size(); ++n)
        out.grid.put({static_cast<int>(n), 70}, std::string(1, kSentinel[n]));
    constexpr std::array<std::string_view, 4> directions{"LR", "TB", "RL", "BT"};
    out.model.direction = directions[static_cast<std::size_t>(turn)];
    out.model.nodes = {
        {"A", "A", Shape::Rectangle}, {"B", "B", Shape::Rectangle}, {"C", "C", Shape::Rectangle}};
    out.model.groups = {
        {"S1", "S1", "", -1, {0}}, {"S2", "S2", "", -1, {1}}, {"T", "T", "", -1, {2}}};
    if (kind == 3) {
        out.model.nodes[1].label = "A";
        out.model.groups = {{"S", "S", "", -1, {0, 1}}, {"T", "T", "", -1, {2}}};
        out.model.edges = {{0, 2, "", "solid", false, true}};
        out.profile.source_order = {{GroupEndpointOrderKind::Ordinary, 0}};
    } else if (kind == 0 || kind >= 4) {
        out.model.edges = {{0, 2, "", "solid", false, true}, {1, 2, "", "solid", false, true}};
        out.profile.source_order = {{GroupEndpointOrderKind::Ordinary, 0},
                                    {GroupEndpointOrderKind::Ordinary, 1}};
    } else if (kind == 1) {
        out.profile.tagged = {{{GroupEndpointKind::Group, 0},
                               {GroupEndpointKind::Group, 2},
                               "",
                               "solid",
                               false,
                               true},
                              {{GroupEndpointKind::Group, 1},
                               {GroupEndpointKind::Group, 2},
                               "",
                               "solid",
                               false,
                               true}};
        out.profile.source_order = {{GroupEndpointOrderKind::Tagged, 0},
                                    {GroupEndpointOrderKind::Tagged, 1}};
    } else {
        out.model.edges = {{0, 2, "", "solid", false, true}};
        out.profile.tagged = {{{GroupEndpointKind::Group, 2},
                               {GroupEndpointKind::Vertex, 2},
                               "",
                               "solid",
                               false,
                               true}};
        out.profile.source_order = {{GroupEndpointOrderKind::Ordinary, 0},
                                    {GroupEndpointOrderKind::Tagged, 0}};
    }
    return out;
}

TEST(MermaidFlowP0, IndependentRepeatedTargetPapersKeepDistinctRoutesInFourDirections) {
    for (int kind = 0; kind < 3; ++kind)
        for (int turn = 0; turn < 4; ++turn) {
            SCOPED_TRACE(std::to_string(kind) + "/" + std::to_string(turn));
            OracleWork work{kFlow400SceneLimit};
            auto paper = flow_p0_paper(kind, turn, work);
            const auto errors =
                flow_group_endpoint_p0_geometry(paper.grid, paper.model, paper.profile, work);
            ASSERT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
            OracleWork legacy{kFlow400SceneLimit};
            const auto old =
                flow_group_endpoint_geometry(paper.grid, paper.model, paper.profile, legacy);
            EXPECT_FALSE(old.empty());
            EXPECT_FALSE(std::ranges::any_of(
                old, [](const auto& error) { return flow400_resource(error); }));
        }
}

TEST(MermaidFlowP0, RepeatedTargetPapersRejectOwnershipInkAndResourceForgeries) {
    OracleWork setup{kFlow400SceneLimit};
    const auto paper = flow_p0_paper(0, 0, setup);
    OracleWork baseline{kFlow400SceneLimit};
    ASSERT_TRUE(
        flow_group_endpoint_p0_geometry(paper.grid, paper.model, paper.profile, baseline).empty());
    const auto reject = [&](auto mutate) {
        OracleWork work{kFlow400SceneLimit};
        flow400_copy_grid(paper.grid, work);
        flow400_copy_model(paper.model, work);
        work.product(paper.profile.source_order.size(), 4u * sizeof(GroupEndpointOrder) + 64u);
        work.spend(4096u);
        auto wrong = paper;
        mutate(wrong);
        bool rejected = false, resource = false;
        try {
            const auto errors =
                flow_group_endpoint_p0_geometry(wrong.grid, wrong.model, wrong.profile, work);
            rejected = !errors.empty();
            resource = std::ranges::any_of(
                errors, [](const auto& error) { return flow400_resource(error); });
        } catch (const std::exception& error) {
            rejected = true;
            resource = flow400_resource(error.what()) || work.remaining == 0u;
        }
        EXPECT_TRUE(rejected);
        EXPECT_FALSE(resource);
    };
    reject([](FlowP0Paper& bad) { bad.grid.put({34, 9}, "─"); });
    reject([](FlowP0Paper& bad) { bad.grid.put({60, 60}, "▶"); });
    reject([](FlowP0Paper& bad) { bad.grid.put({20, 3}, "┬"); });
    reject([](FlowP0Paper& bad) { bad.grid.put({20, 3}, "┼"); });
    reject([](FlowP0Paper& bad) { bad.grid.put({60, 60}, "─"); });
    reject([](FlowP0Paper& bad) { bad.model.edges[1].from = 0; });  // No head/port reuse.
    reject([](FlowP0Paper& bad) { bad.model.edges[0].to = 1; });
    reject([](FlowP0Paper& bad) { bad.model.groups[2].nodes.clear(); });
    reject([](FlowP0Paper& bad) { bad.profile.source_order[1] = bad.profile.source_order[0]; });
    reject([](FlowP0Paper& bad) { bad.model.direction = "RL"; });
    reject([](FlowP0Paper& bad) { bad.grid.put({0, 70}, "X"); });
    for (const int kind : {1, 2}) {
        OracleWork preparation{kFlow400SceneLimit};
        const auto other = flow_p0_paper(kind, 0, preparation);
        ASSERT_TRUE(
            flow_group_endpoint_p0_geometry(other.grid, other.model, other.profile, preparation)
                .empty());
        for (const int control : {0, 1}) {
            OracleWork work{kFlow400SceneLimit};
            flow400_copy_grid(other.grid, work);
            flow400_copy_model(other.model, work);
            work.spend(8192u);
            auto bad = other;
            if (control == 0)
                bad.profile.tagged[0].target.kind =
                    bad.profile.tagged[0].target.kind == GroupEndpointKind::Vertex
                        ? GroupEndpointKind::Group
                        : GroupEndpointKind::Vertex;
            else if (kind == 1)
                bad.profile.tagged[0].source = bad.profile.tagged[1].source;
            else
                bad.profile.tagged[0].source = {GroupEndpointKind::Vertex, 0u};
            const auto errors =
                flow_group_endpoint_p0_geometry(bad.grid, bad.model, bad.profile, work);
            EXPECT_FALSE(errors.empty());
            EXPECT_FALSE(std::ranges::any_of(
                errors, [](const auto& error) { return flow400_resource(error); }));
        }
    }
    for (int mode = 0; mode < 4; ++mode) {
        GroupEndpointP0Limits limits;
        switch (mode) {
        case 0:
            limits.route_cells = 0;
            break;
        case 1:
            limits.per_edge_candidates = 0;
            break;
        case 2:
            limits.total_candidates = 0;
            break;
        default:
            limits.selection_steps = 0;
            break;
        }
        OracleWork work{kFlow400SceneLimit};
        const auto errors = flow_group_endpoint_p0_geometry(paper.grid, paper.model, paper.profile,
                                                            work, kSentinel, limits);
        EXPECT_FALSE(errors.empty());
        EXPECT_TRUE(std::ranges::any_of(errors, [](const auto& error) {
            return flow400_resource(error);
        })) << (errors.empty() ? "" : errors.front());
    }
    for (int mode = 0; mode < 4; ++mode) {
        GroupEndpointP0Limits limits;
        switch (mode) {
        case 0:
            limits.route_cells = 30u;
            break;
        case 1:
            limits.per_edge_candidates = 1u;
            break;
        case 2:
            limits.total_candidates = 2u;
            break;
        default:
            limits.selection_steps = 3u;
            break;
        }
        OracleWork exact{kFlow400SceneLimit};
        const auto accepted = flow_group_endpoint_p0_geometry(
            paper.grid, paper.model, paper.profile, exact, kSentinel, limits);
        EXPECT_TRUE(accepted.empty()) << (accepted.empty() ? "" : accepted.front());
        switch (mode) {
        case 0:
            --limits.route_cells;
            break;
        case 1:
            --limits.per_edge_candidates;
            break;
        case 2:
            --limits.total_candidates;
            break;
        default:
            --limits.selection_steps;
            break;
        }
        OracleWork shortfall{kFlow400SceneLimit};
        const auto rejected = flow_group_endpoint_p0_geometry(
            paper.grid, paper.model, paper.profile, shortfall, kSentinel, limits);
        EXPECT_TRUE(std::ranges::any_of(rejected, [](const auto& error) {
            return flow400_resource(error);
        })) << (rejected.empty() ? "" : rejected.front());
    }
    {
        GroupEndpointP0Limits raised;
        raised.route_cells = 2049u;
        OracleWork work{kFlow400SceneLimit};
        const auto errors = flow_group_endpoint_p0_geometry(paper.grid, paper.model, paper.profile,
                                                            work, kSentinel, raised);
        EXPECT_TRUE(
            std::ranges::any_of(errors, [](const auto& error) { return flow400_resource(error); }));
    }
    OracleWork zero{0u};
    EXPECT_THROW(static_cast<void>(
                     flow_group_endpoint_p0_geometry(paper.grid, paper.model, paper.profile, zero)),
                 std::runtime_error);
    EXPECT_TRUE(flow400_resource("GroupEndpoint route candidate bound"));
    EXPECT_TRUE(flow400_resource("GroupEndpoint exclusive route ownership exhausted"));
}
}  // namespace

namespace {
void flow_p0_source_paper_case(const Flow400Documents& primary, const FlowP0Documents& proposal,
                               std::size_t i) {
    const auto& pin = kFlowP0Pins[i];
    SCOPED_TRACE(pin.fixture);
    OracleWork work{kFlow400SceneLimit};
    work.spend(pin.bytes * 192u + 4000000u);
    const auto path =
        std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases" / pin.fixture;
    ASSERT_EQ(std::filesystem::file_size(path), pin.bytes);
    const auto source = flow400_read(path);
    auto mermaid = loom::ui::render_markdown(group_endpoint15_fenced(source, "mermaid"));
    auto ordinary = loom::ui::render_markdown(group_endpoint15_fenced(source, "text"));
    auto node = ftxui::vbox({mermaid, ftxui::text(std::string(kSentinel))});
    auto plain = ftxui::vbox({ordinary, ftxui::text(std::string(kSentinel))});
    node->ComputeRequirement();
    plain->ComputeRequirement();
    const int height =
        std::max({32, node->requirement().min_y + 8, plain->requirement().min_y + 8});
    ASSERT_LE(height, 1040);
    std::array<std::optional<Grid>, 5> frames;
    constexpr std::array<int, 5> widths{240, 40, 240, 240, 40};
    for (std::size_t n = 0; n < 5u; ++n)
        frames[n] = capture(n < 3u ? node : plain, widths[n], height, &work);
    // Reserve final evidence serialization from actual bounded glyph bytes
    // before any frame is published or semantic Model decoded.
    for (const auto& grid : frames) {
        work.product(grid->cells.size(), 16u);
        for (const auto& cell : grid->cells)
            work.product(cell.size(), 64u);
        work.spend(static_cast<std::size_t>(grid->height) * 128u + 4096u);
    }
    json::JsonMutDoc evidence;
    auto item = evidence.object();
    constexpr std::array<std::string_view, 5> names{"wide_240", "narrow_40", "restored_240",
                                                    "source_240", "source_40"};
    for (std::size_t n = 0; n < 5u; ++n)
        group_endpoint15_publish_frame(item, names[n], *frames[n], work);
    item.set("phase", "actual_captures_before_binding");
    evidence.set_root(item);
#ifdef LOOM_MERMAID_REPORT_DIR
    {
        const auto output =
            std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
            ("mermaid-flow-p0-" + std::to_string(pin.ordinal) + "-whole-source.json");
        std::ofstream file(output);
        ASSERT_TRUE(file.good());
        file << evidence.to_pretty_string() << '\n';
    }
#endif
    const auto fixture = primary.manifest.root().get("fixtures").at(pin.global),
               base = primary.compact.root().get("cases").at(pin.ordinal);
    const auto record =
        flow_p0_group_select(proposal, base, fixture, pin.ordinal, pin.global, source, work);
    ASSERT_TRUE(record.valid());
    const auto model = flow_p0_group_model(record, work);
    ASSERT_FALSE(flow400_same(*frames[0], *frames[3], work));
    ASSERT_TRUE(flow400_same(*frames[0], *frames[2], work));
    const auto wide = flow_p0_group_geometry(*frames[0], model, work);
    ASSERT_TRUE(wide.empty()) << (wide.empty() ? "" : wide.front());
    const auto restored = flow_p0_group_geometry(*frames[2], model, work);
    ASSERT_TRUE(restored.empty()) << (restored.empty() ? "" : restored.front());
    const bool narrow_source = flow400_same(*frames[1], *frames[4], work);
    if (!narrow_source) {
        const auto narrow = flow_p0_group_geometry(*frames[1], model, work);
        ASSERT_TRUE(narrow.empty()) << (narrow.empty() ? "" : narrow.front());
    }
    for (const auto& frame : frames)
        static_cast<void>(flow400_following(*frame, work));
    ASSERT_EQ(flow_p0_group_mutations(*frames[0], model, work), 6u);
    item.set("phase", "complete_source_geometry_and_mutations");
    item.set("source_sha256", pin.source_sha);
    item.set("Model_sha256", pin.model_sha);
    item.set("source_asset_sha256", kFlowP0AssetSha);
    item.set("source_bound", true);
    item.set("five_captures_before_binding", true);
    item.set("narrow_source", narrow_source);
    item.set("mutations", 6);
    item.set("formal_matrix_admission", false);
    evidence.set_root(item);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto output = std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                        ("mermaid-flow-p0-" + std::to_string(pin.ordinal) + "-whole-source.json");
    std::ofstream file(output);
    ASSERT_TRUE(file.good());
    file << evidence.to_pretty_string() << '\n';
#endif
}

TEST(MermaidFlowP0, SevenGroupedOriginalsRequireCompleteSourceGeometryAndMutations) {
    const auto primary = flow400_documents();
    const auto proposal = flow_p0_documents(primary);
    int visited = 0;
    for (std::size_t i = 0; i < kFlowP0Pins.size(); ++i)
        if (kFlowP0Pins[i].grouped) {
            ++visited;
            flow_p0_source_paper_case(primary, proposal, i);
        }
    EXPECT_EQ(visited, 7);
}
}  // namespace

namespace {
TEST(MermaidFlowP0, LaterIdentityResourceFailureCannotBecomeSemanticNegativeCredit) {
    OracleWork setup{kFlow400SceneLimit};
    const auto paper = flow_p0_paper(3, 0, setup);
    OracleWork good{kFlow400SceneLimit};
    std::vector<std::string> diagnostics;
    const int following = flow400_following(paper.grid, good);
    const auto baseline =
        validate_impl(paper.grid, paper.model, good, &diagnostics, &following, nullptr,
                      &paper.profile, GroupEndpointDomain::SourceReviewedP0);
    ASSERT_TRUE(baseline.empty()) << (baseline.empty() ? "" : baseline.front());
    ASSERT_GE(diagnostics.size(), 2u);
    EXPECT_TRUE(diagnostics.front().find("GroupEndpoint complete route differs") !=
                std::string::npos);
    GroupEndpointP0Limits limits;
    limits.per_edge_candidates = 0u;
    OracleWork work{kFlow400SceneLimit};
    std::vector<std::string> failed;
    const auto errors =
        validate_impl(paper.grid, paper.model, work, &failed, &following, nullptr, &paper.profile,
                      GroupEndpointDomain::SourceReviewedP0, &limits);
    ASSERT_GE(failed.size(), 2u);
    EXPECT_TRUE(failed.front().find("GroupEndpoint complete route differs") != std::string::npos);
    ASSERT_FALSE(errors.empty());
    EXPECT_TRUE(std::ranges::any_of(errors, [](const auto& error) {
        return flow400_resource(error);
    })) << errors.front();
}
}  // namespace

namespace {
TEST(MermaidFlowP0, IndependentlyConstructedSharedPortsAndRouteStemsRemainFailures) {
    OracleWork preparation{kFlow400SceneLimit};
    const auto good = flow_p0_paper(0, 0, preparation);
    ASSERT_TRUE(
        flow_group_endpoint_p0_geometry(good.grid, good.model, good.profile, preparation).empty());
    for (const int kind : {4, 5, 6}) {
        SCOPED_TRACE(kind);
        OracleWork work{kFlow400SceneLimit};
        const auto shared = flow_p0_paper(kind, 0, work);
        const auto errors =
            flow_group_endpoint_p0_geometry(shared.grid, shared.model, shared.profile, work);
        ASSERT_FALSE(errors.empty());
        EXPECT_FALSE(std::ranges::any_of(errors, [](const auto& error) {
            return flow400_resource(error);
        })) << errors.front();
    }
}

TEST(MermaidFlowP0, LegacyHandScenesStillFitTheirEightMillionComparisonBudget) {
    const auto documents = group_endpoint15_documents();
    const auto hands = group_endpoint15_hand_documents();
    const auto records = documents.asset.root().get("records"), root = hands.asset.root();
    int seen = 0;
    root.get("good_scenes").iter([&](JsonVal scene) {
        ++seen;
        OracleWork work{8000000u};
        const auto hand = group_endpoint15_hand_model(root, records, scene, work);
        const auto grid = group_endpoint15_hand_grid(scene, work);
        const auto errors = group_endpoint15_geometry(
            grid, hand.model, work, flow400_text(scene.get("exact_following_token")));
        EXPECT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
        EXPECT_GT(work.remaining, 0u);
    });
    EXPECT_EQ(seen, 8);
}

TEST(MermaidFlowP0, ParentGroupEndpointsCannotReenterTheirOwnPortInFourDirections) {
    for (const auto direction : {"TB", "BT", "LR", "RL"}) {
        SCOPED_TRACE(direction);
        OracleWork work{kFlow400SceneLimit};
        work.spend(4000000u);
        const auto source = "flowchart " + std::string(direction) +
                            "\nOut --> In\nsubgraph Sub\nIn\nend\nSub --> In\n";
        auto drawing = loom::ui::render_markdown(group_endpoint15_fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(group_endpoint15_fenced(source, "text"));
        auto node = ftxui::vbox({drawing, ftxui::text(std::string(kSentinel))});
        auto plain = ftxui::vbox({ordinary, ftxui::text(std::string(kSentinel))});
        node->ComputeRequirement();
        plain->ComputeRequirement();
        const int height =
            std::max({32, node->requirement().min_y + 8, plain->requirement().min_y + 8});
        ASSERT_LE(height, 1040);
        const auto wide = capture(node, 240, height, &work),
                   narrow = capture(node, 40, height, &work),
                   restored = capture(node, 240, height, &work),
                   source240 = capture(plain, 240, height, &work),
                   source40 = capture(plain, 40, height, &work);
        GroupEndpoint15Model model;
        model.base.direction = direction;
        model.base.nodes = {{"Out", "Out", Shape::Rectangle}, {"In", "In", Shape::Rectangle}};
        model.base.groups = {{"Sub", "Sub", "", -1, {1}}};
        model.base.edges = {{0, 1, "", "solid", false, true}};
        model.tagged = {{{GroupEndpointKind::Group, "Sub"},
                         {GroupEndpointKind::Vertex, "In"},
                         "",
                         "solid",
                         false,
                         true,
                         1u}};
        model.source_order = {{GroupEndpointOrderKind::Ordinary, 0},
                              {GroupEndpointOrderKind::Tagged, 0}};
        ASSERT_FALSE(flow400_same(wide, source240, work));
        ASSERT_TRUE(flow400_same(wide, restored, work));
        const auto errors = flow_p0_group_geometry(wide, model, work);
        ASSERT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
        ASSERT_TRUE(flow_p0_group_geometry(restored, model, work).empty());
        if (!flow400_same(narrow, source40, work))
            ASSERT_TRUE(flow_p0_group_geometry(narrow, model, work).empty());
        for (const auto* grid : {&wide, &narrow, &restored, &source240, &source40})
            static_cast<void>(flow400_following(*grid, work));
    }
}
}  // namespace

namespace {
TEST(MermaidFlowP0, HardReservationOverridesRelaxedCrossingAndRestoresBothStates) {
    namespace seam = loom::ui::mermaid::testing;
    for (const bool blocked : {false, true})
        for (const bool crossed : {false, true}) {
            seam::Work work;
            const auto proof = seam::flow_group_border_probe(blocked, crossed, work);
            ASSERT_TRUE(proof);
            EXPECT_EQ((*proof)[0], !blocked);
            EXPECT_EQ((*proof)[1], !blocked || crossed);
            EXPECT_FALSE((*proof)[2]);
            EXPECT_TRUE((*proof)[3]);
            EXPECT_FALSE((*proof)[4]);
        }
    for (const std::size_t allowance : {0u, 16u, 16385u, 16400u}) {
        seam::Work work;
        work.remaining = allowance;
        EXPECT_FALSE(seam::flow_group_border_probe(true, true, work));
        EXPECT_EQ(work.remaining, 0u);
    }
}
}  // namespace

namespace {
TEST(MermaidFlowP0, LinearRouteUniquenessPrepaysItsExactBudgetBeforeMutation) {
    std::vector<int> path(2048u);
    std::iota(path.begin(), path.end(), 0);
    std::vector<std::uint32_t> seen(2048u, 0u);
    std::uint32_t epoch = 0u;
    const auto fee = group_endpoint_unique_fee(path.size());
    OracleWork shortfall{fee - 1u};
    EXPECT_THROW(static_cast<void>(group_endpoint_route_unique(path, seen, epoch, shortfall)),
                 std::runtime_error);
    EXPECT_EQ(shortfall.remaining, 0u);
    EXPECT_EQ(epoch, 0u);
    EXPECT_TRUE(std::ranges::all_of(seen, [](auto stamp) { return stamp == 0u; }));
    OracleWork exact{fee};
    EXPECT_TRUE(group_endpoint_route_unique(path, seen, epoch, exact));
    EXPECT_EQ(exact.remaining, 0u);
    EXPECT_EQ(epoch, 1u);
    path.back() = 0;
    OracleWork duplicate{fee};
    EXPECT_FALSE(group_endpoint_route_unique(path, seen, epoch, duplicate));
    EXPECT_EQ(duplicate.remaining, 0u);
    OracleWork overflow{kFlow400SceneLimit};
    epoch = std::numeric_limits<std::uint32_t>::max();
    bool resource = false;
    try {
        static_cast<void>(group_endpoint_route_unique(path, seen, epoch, overflow));
    } catch (const std::runtime_error& error) {
        resource = flow400_resource(error.what());
    }
    EXPECT_TRUE(resource);
}

struct StrandPaper {
    Grid grid;
    Model model;
};

StrandPaper strand_parallel_paper(int turn, bool mixed = false, int edge_count = 5,
                                  bool grouped = true) {
    StrandPaper out{{72, 72, std::vector<std::string>(72u * 72u, " ")}, {}};
    std::array<int, 4096> masks{};
    std::array<int, 4096> arrows{};
    arrows.fill(-1);
    const auto index = [](Point p) { return static_cast<std::size_t>(p.y * 64 + p.x); };
    const auto add = [&](Point p, int bits) { masks[index(p)] |= bits; };

    struct Box {
        int l, t, r, b;
    };

    const std::array<Box, 4> boxes{
        {{0, 0, 14, 40}, {24, 0, 40, 40}, {3, 5, 11, 32}, {28, 5, 36, 32}}};
    for (std::size_t number = 0u; number < boxes.size(); ++number) {
        if (!grouped && number < 2u) continue;
        const auto& b = boxes[number];
        for (int x = b.l + 1; x < b.r; ++x) {
            add({x, b.t}, L | R);
            add({x, b.b}, L | R);
        }
        for (int y = b.t + 1; y < b.b; ++y) {
            add({b.l, y}, U | D);
            add({b.r, y}, U | D);
        }
        add({b.l, b.t}, R | D);
        add({b.r, b.t}, L | D);
        add({b.l, b.b}, U | R);
        add({b.r, b.b}, U | L);
    }
    for (int row = 10; row < 10 + edge_count; ++row) {
        const bool head = mixed && row < 12;
        for (int x = 11; x <= (head ? 27 : 28); ++x) {
            if (x > 11) add({x, row}, L);
            if (x < (head ? 27 : 28)) add({x, row}, R);
        }
        if (head) arrows[index({27, row})] = 1;
    }
    const auto rotate = [&](Point point) {
        for (int n = 0; n < turn; ++n)
            point = {63 - point.y, point.x};
        return point;
    };
    const auto ink = [](int mask) -> std::string_view {
        switch (mask) {
        case U | D:
            return "│";
        case L | R:
            return "─";
        case R | D:
            return "┌";
        case L | D:
            return "┐";
        case U | R:
            return "└";
        case U | L:
            return "┘";
        case U | D | R:
            return "├";
        case U | D | L:
            return "┤";
        case L | R | D:
            return "┬";
        case L | R | U:
            return "┴";
        case U | D | L | R:
            return "┼";
        default:
            throw std::runtime_error("Independent strand paper mask");
        }
    };
    constexpr std::array<std::string_view, 4> heads{"▲", "▶", "▼", "◀"};
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const auto at = index({x, y});
            if (!masks[at]) continue;
            const auto point = rotate({x, y});
            int mask = 0;
            for (int d = 0; d < 4; ++d)
                if (masks[at] & kBits[static_cast<std::size_t>(d)])
                    mask |= kBits[static_cast<std::size_t>((d + turn) % 4)];
            out.grid.put(
                point, std::string(arrows[at] < 0
                                       ? ink(mask)
                                       : heads[static_cast<std::size_t>((arrows[at] + turn) % 4)]));
        }
    const std::array<std::pair<Point, std::string_view>, 4> labels{
        {{{2, 2}, "G"}, {{26, 2}, "H"}, {{5, 7}, "A"}, {{30, 7}, "B"}}};
    for (std::size_t at = 0u; at < labels.size(); ++at)
        if (grouped || at >= 2u)
            out.grid.put(rotate(labels[at].first), std::string(labels[at].second));
    for (std::size_t at = 0; at < kSentinel.size(); ++at)
        out.grid.put({static_cast<int>(at), 70}, std::string(1, kSentinel[at]));
    out.model.direction =
        std::array<std::string_view, 4>{"LR", "TB", "RL", "BT"}[static_cast<std::size_t>(turn)];
    out.model.nodes = {{"A", "A", Shape::Rectangle}, {"B", "B", Shape::Rectangle}};
    if (grouped) out.model.groups = {{"G", "G", "", -1, {0}}, {"H", "H", "", -1, {1}}};
    for (int edge = 0; edge < edge_count; ++edge)
        out.model.edges.push_back({0, 1, "", "solid", false, mixed && edge < 2});
    return out;
}

TEST(MermaidFlowStrands, IndependentFiveParallelAndMixedGroupRoutesInAllDirections) {
    for (int turn = 0; turn < 4; ++turn)
        for (bool mixed : {false, true}) {
            SCOPED_TRACE(std::to_string(turn) + "/" + std::to_string(mixed));
            const auto paper = strand_parallel_paper(turn, mixed);
            OracleWork work{kFlow400SceneLimit};
            const auto result = strands::geometry(paper.grid, paper.model, work);
            EXPECT_EQ(result.outcome, strands::Outcome::Match) << static_cast<int>(result.reason);
            EXPECT_EQ(result.physical_edges, 5u);
            EXPECT_FALSE(result.resource);
        }
}

TEST(MermaidFlowStrands, DecisivePhysicalNegativesAndStructuralResourceOutcomes) {
    const auto paper = strand_parallel_paper(0);
    OracleWork baseline{kFlow400SceneLimit};
    ASSERT_EQ(strands::geometry(paper.grid, paper.model, baseline).outcome,
              strands::Outcome::Match);
    const auto negative = [&](auto change) {
        auto bad = paper;
        change(bad);
        OracleWork work{kFlow400SceneLimit};
        const auto result = strands::geometry(bad.grid, bad.model, work);
        EXPECT_EQ(result.outcome, strands::Outcome::Mismatch) << static_cast<int>(result.reason);
        EXPECT_FALSE(result.resource);
    };
    negative([](auto& bad) { bad.grid.put({0, 70}, "?"); });
    negative([](auto& bad) { bad.grid.put({20, 10}, "§"); });
    negative([](auto& bad) { bad.grid.put({20, 10}, "┬"); });
    negative([](auto& bad) { bad.grid.put({20, 10}, " "); });
    negative([](auto& bad) { bad.grid.put({20, 10}, "━"); });
    negative([](auto& bad) { bad.model.edges.pop_back(); });
    negative([](auto& bad) { bad.model.edges[0].end = true; });
    negative([](auto& bad) { bad.model.nodes[0].label = "Missing"; });
    negative([](auto& bad) { bad.model.groups[0].nodes.clear(); });
    negative([](auto& bad) { bad.model.direction = "RL"; });
    for (const auto limits :
         std::array<strands::Limits, 4>{{{0u, 14u}, {2048u, 0u}, {2049u, 14u}, {2048u, 15u}}}) {
        OracleWork work{kFlow400SceneLimit};
        const auto result = strands::geometry(paper.grid, paper.model, work, kSentinel, limits);
        EXPECT_EQ(result.outcome, strands::Outcome::Inconclusive);
        EXPECT_TRUE(result.resource);
    }
    OracleWork short_path{kFlow400SceneLimit};
    EXPECT_EQ(strands::geometry(paper.grid, paper.model, short_path, kSentinel, {17u, 5u}).outcome,
              strands::Outcome::Inconclusive);
    OracleWork exact_path{kFlow400SceneLimit};
    EXPECT_EQ(strands::geometry(paper.grid, paper.model, exact_path, kSentinel, {18u, 5u}).outcome,
              strands::Outcome::Match);
    const auto exact_fee = kFlow400SceneLimit - baseline.remaining;
    OracleWork exact_work{exact_fee};
    EXPECT_EQ(strands::geometry(paper.grid, paper.model, exact_work).outcome,
              strands::Outcome::Match);
    EXPECT_EQ(exact_work.remaining, 0u);
    OracleWork short_work{exact_fee - 1u};
    const auto unpaid = strands::geometry(paper.grid, paper.model, short_work);
    EXPECT_EQ(unpaid.outcome, strands::Outcome::Inconclusive);
    EXPECT_TRUE(unpaid.resource);
    auto bad_parent = paper.model;
    bad_parent.groups[0].parent = 1;
    bad_parent.groups[1].parent = 999;
    OracleWork invalid_parent{kFlow400SceneLimit};
    EXPECT_EQ(strands::geometry(paper.grid, bad_parent, invalid_parent).outcome,
              strands::Outcome::Inconclusive);
    negative([](auto& bad) {
        for (std::size_t at = 0u; at < kSentinel.size(); ++at)
            bad.grid.put({static_cast<int>(at), 69}, std::string(1, kSentinel[at]));
    });
    negative([](auto& bad) { bad.grid.put({50, 71}, "§"); });
    OracleWork zero{0u};
    const auto result = strands::geometry(paper.grid, paper.model, zero);
    EXPECT_EQ(result.outcome, strands::Outcome::Inconclusive);
    EXPECT_TRUE(result.resource);
}

TEST(MermaidFlowStrands, StraightOrthogonalCrossingHasTwoStrandsAndNeverTurns) {
    Grid grid{64, 64, std::vector<std::string>(4096u, " ")};
    std::array<int, 4096> masks{};
    const auto at = [](Point p) { return static_cast<std::size_t>(p.y * 64 + p.x); };
    const auto add = [&](Point p, int bits) { masks[at(p)] |= bits; };

    struct Box {
        int l, t, r, b;
    };

    const std::array<Box, 4> boxes{
        {{2, 18, 10, 22}, {40, 18, 48, 22}, {14, 2, 22, 6}, {28, 34, 36, 38}}};
    for (const auto& b : boxes) {
        for (int x = b.l + 1; x < b.r; ++x) {
            add({x, b.t}, L | R);
            add({x, b.b}, L | R);
        }
        for (int y = b.t + 1; y < b.b; ++y) {
            add({b.l, y}, U | D);
            add({b.r, y}, U | D);
        }
        add({b.l, b.t}, R | D);
        add({b.r, b.t}, L | D);
        add({b.l, b.b}, U | R);
        add({b.r, b.b}, U | L);
    }
    for (int x = 10; x <= 40; ++x) {
        if (x > 10) add({x, 20}, L);
        if (x < 40) add({x, 20}, R);
    }
    for (int y = 6; y <= 36; ++y) {
        if (y > 6) add({18, y}, U);
        if (y < 36) add({18, y}, D);
    }
    for (int x = 18; x <= 28; ++x) {
        if (x > 18) add({x, 36}, L);
        if (x < 28) add({x, 36}, R);
    }
    const auto ink = [](int mask) -> std::string_view {
        switch (mask) {
        case U | D:
            return "│";
        case L | R:
            return "─";
        case R | D:
            return "┌";
        case L | D:
            return "┐";
        case U | R:
            return "└";
        case U | L:
            return "┘";
        case U | D | R:
            return "├";
        case U | D | L:
            return "┤";
        case L | R | D:
            return "┬";
        case L | R | U:
            return "┴";
        case U | D | L | R:
            return "┼";
        default:
            throw std::runtime_error("Independent crossing paper mask");
        }
    };
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            if (masks[at({x, y})]) grid.put({x, y}, std::string(ink(masks[at({x, y})])));
    grid.put({4, 20}, "A");
    grid.put({42, 20}, "B");
    grid.put({16, 4}, "C");
    grid.put({30, 36}, "D");
    for (std::size_t n = 0; n < kSentinel.size(); ++n)
        grid.put({static_cast<int>(n), 60}, std::string(1, kSentinel[n]));
    Model model{"LR",
                {{"A", "A", Shape::Rectangle},
                 {"B", "B", Shape::Rectangle},
                 {"C", "C", Shape::Rectangle},
                 {"D", "D", Shape::Rectangle}},
                {{0, 1, "", "solid", false, false}, {2, 3, "", "solid", false, false}},
                {}};
    OracleWork work{kFlow400SceneLimit};
    const auto positive = strands::geometry(grid, model, work);
    ASSERT_EQ(positive.outcome, strands::Outcome::Match) << static_cast<int>(positive.reason);
    EXPECT_EQ(positive.physical_edges, 2u);
    model.edges[0].to = 3;
    model.edges[1].to = 1;
    OracleWork wrong_turn{kFlow400SceneLimit};
    const auto wrong = strands::geometry(grid, model, wrong_turn);
    EXPECT_EQ(wrong.outcome, strands::Outcome::Mismatch);
    EXPECT_EQ(wrong.reason, strands::Reason::Multiplicity);
}

TEST(MermaidFlowStrands, TwoActualCompleteSourcesRequireIndependentPhysicalStrands) {
    const auto primary = flow400_documents();
    const auto documents = flow_p0_documents(primary);
    for (const auto index : std::array<std::size_t, 2>{4u, 9u}) {
        const auto& pin = kFlowP0Pins[index];
        SCOPED_TRACE(pin.fixture);
        OracleWork work{kFlow400SceneLimit};
        work.spend(4000000u + pin.bytes * 192u);
        const auto setup_charge = flow_p0_strand_discovery_charge(primary, documents, work);
        const auto source = flow400_read(std::filesystem::path(LOOM_TESTS_DIR) /
                                         "fixtures/mermaid_official/cases" / pin.fixture);
        auto mermaid = loom::ui::render_markdown(group_endpoint15_fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(group_endpoint15_fenced(source, "text"));
        auto node = ftxui::vbox({mermaid, ftxui::text(std::string(kSentinel))});
        auto plain = ftxui::vbox({ordinary, ftxui::text(std::string(kSentinel))});
        node->ComputeRequirement();
        plain->ComputeRequirement();
        const int height = std::max({1, node->requirement().min_y, plain->requirement().min_y}) + 1;
        ASSERT_LE(height, 1040);
        std::array<std::optional<Grid>, 5> frames;
        const std::array<int, 5> widths{240, 40, 240, 240, 40};
        json::JsonMutDoc document;
        auto actual = document.object();
        actual.set("scope", "Prototype physical strand proof; no formal matrix admission");
        actual.set("formal_matrix_admission", false);
        actual.set("fixture", pin.fixture);
        actual.set("complete_discovery_debited", static_cast<int64_t>(setup_charge));
        for (std::size_t n = 0; n < frames.size(); ++n)
            frames[n] = capture(n < 3u ? node : plain, widths[n], height, &work);
        for (const auto& frame : frames) {
            work.product(frame->cells.size(), 16u);
            for (const auto& cell : frame->cells)
                work.product(cell.size(), 64u);
            work.spend(static_cast<std::size_t>(frame->height) * 128u + 4096u);
        }
        for (std::size_t n = 0; n < frames.size(); ++n)
            group_endpoint15_publish_frame(
                actual,
                std::array<std::string_view, 5>{"wide", "narrow", "restored", "ordinary_wide",
                                                "ordinary_narrow"}[n],
                *frames[n], work);
        actual.set("phase", "actual_captures_before_binding");
        document.set_root(actual);
#ifdef LOOM_MERMAID_REPORT_DIR
        {
            std::ofstream initial(
                std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                ("mermaid-flow-strand-prototype-" + std::to_string(pin.ordinal) + ".json"));
            initial << document.to_string();
            flow400_require(static_cast<bool>(initial),
                            "Flow strand prototype captures publication");
        }
#endif
        const auto selected =
            flow_p0_strand_select(documents, primary.compact.root().get("cases").at(pin.ordinal),
                                  primary.manifest.root().get("fixtures").at(pin.global),
                                  pin.ordinal, pin.global, source, work);
        const auto model = flow_p0_strand_model(selected, work);
        EXPECT_TRUE(flow400_same(*frames[0], *frames[2], work));
        EXPECT_FALSE(flow400_same(*frames[0], *frames[3], work));
        for (std::size_t n = 0u; n < 3u; ++n) {
            if (n == 1u && flow400_same(*frames[n], *frames[4], work)) continue;
            const auto result = strands::geometry(*frames[n], model, work);
            actual.set(std::string("result_") + std::to_string(n),
                       static_cast<int>(result.outcome));
            actual.set(std::string("reason_") + std::to_string(n), static_cast<int>(result.reason));
            EXPECT_EQ(result.outcome, strands::Outcome::Match)
                << "width=" << widths[n] << " reason=" << static_cast<int>(result.reason);
            EXPECT_FALSE(result.resource);
        }
        const auto negatives = strands::mutations(*frames[0], model, work);
        actual.set("decisive_mutations_rejected", static_cast<int64_t>(negatives.rejected));
        actual.set("mutations_complete", negatives.complete);
        EXPECT_TRUE(negatives.complete) << negatives.rejected;
        EXPECT_EQ(negatives.rejected, strands::kRequiredMutations);
        EXPECT_FALSE(negatives.resource);
        EXPECT_FALSE(negatives.inconclusive);
        actual.set("phase", "prototype_geometry_complete");
        actual.set("remaining", static_cast<int64_t>(work.remaining));
        document.set_root(actual);
#ifdef LOOM_MERMAID_REPORT_DIR
        const auto encoded = document.to_string();
        std::ofstream output(
            std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
            ("mermaid-flow-strand-prototype-" + std::to_string(pin.ordinal) + ".json"));
        output << encoded;
        ASSERT_TRUE(static_cast<bool>(output));
#endif
    }
}

TEST(MermaidFlowStrands, InclusiveMaximumPathStrandsAndGridBounds) {
    const auto fourteen = strand_parallel_paper(0, false, 14, false);
    OracleWork exact_strands{kFlow400SceneLimit};
    const auto full = strands::geometry(fourteen.grid, fourteen.model, exact_strands);
    ASSERT_EQ(full.outcome, strands::Outcome::Match) << static_cast<int>(full.reason);
    EXPECT_EQ(full.physical_edges, 14u);
    OracleWork strand_short{kFlow400SceneLimit};
    const auto partial =
        strands::geometry(fourteen.grid, fourteen.model, strand_short, kSentinel, {2048u, 13u});
    EXPECT_EQ(partial.outcome, strands::Outcome::Inconclusive);
    EXPECT_TRUE(partial.resource);
    const auto contour_cap = strand_parallel_paper(0, false, 14);
    OracleWork contour_work{kFlow400SceneLimit};
    const auto contour_result =
        strands::geometry(contour_cap.grid, contour_cap.model, contour_work);
    EXPECT_EQ(contour_result.outcome, strands::Outcome::Inconclusive);
    EXPECT_EQ(contour_result.reason, strands::Reason::Limit);
    EXPECT_TRUE(contour_result.resource);
    Grid long_path{240, 330, std::vector<std::string>(240u * 330u, " ")};
    std::vector<int> bits(long_path.cells.size());
    const auto add = [&](Point p, int mask) {
        bits[static_cast<std::size_t>(long_path.index(p))] |= mask;
    };
    for (const auto box : std::array<Rect, 2>{{{2, 2, 10, 6}, {228, 311, 236, 315}}}) {
        for (int x = box.left + 1; x < box.right; ++x) {
            add({x, box.top}, L | R);
            add({x, box.bottom}, L | R);
        }
        for (int y = box.top + 1; y < box.bottom; ++y) {
            add({box.left, y}, U | D);
            add({box.right, y}, U | D);
        }
        add({box.left, box.top}, R | D);
        add({box.right, box.top}, L | D);
        add({box.left, box.bottom}, U | R);
        add({box.right, box.bottom}, U | L);
    }
    std::vector<Point> points{{10, 4}, {210, 4}};
    int x = 210, y = 4;
    for (int row = 0; row < 8; ++row) {
        y += 34;
        points.push_back({x, y});
        x = x == 210 ? 20 : 210;
        points.push_back({x, y});
    }
    points.push_back({210, 313});
    points.push_back({228, 313});
    std::size_t moves = 0u;
    for (std::size_t n = 1; n < points.size(); ++n) {
        auto from = points[n - 1u];
        const auto to = points[n];
        const int bit = from.x < to.x ? R : from.x > to.x ? L : from.y < to.y ? D : U;
        while (from != to) {
            const auto next = strands::step(from, bit);
            add(from, bit);
            add(next, strands::reverse(bit));
            from = next;
            ++moves;
        }
    }
    ASSERT_EQ(moves + 1u, 2048u);
    const auto ink = [](int mask) -> std::string_view {
        switch (mask) {
        case U | D:
            return "│";
        case L | R:
            return "─";
        case R | D:
            return "┌";
        case L | D:
            return "┐";
        case U | R:
            return "└";
        case U | L:
            return "┘";
        case U | D | R:
            return "├";
        case U | D | L:
            return "┤";
        default:
            throw std::runtime_error("Independent2048-cell path mask");
        }
    };
    for (std::size_t n = 0; n < bits.size(); ++n)
        if (bits[n]) long_path.put(long_path.point(static_cast<int>(n)), std::string(ink(bits[n])));
    long_path.put({4, 4}, "A");
    long_path.put({230, 313}, "B");
    for (std::size_t n = 0; n < kSentinel.size(); ++n)
        long_path.put({static_cast<int>(n), 325}, std::string(1, kSentinel[n]));
    Model model{"LR",
                {{"A", "A", Shape::Rectangle}, {"B", "B", Shape::Rectangle}},
                {{0, 1, "", "solid", false, false}},
                {}};
    OracleWork exact_path{kFlow400SceneLimit};
    const auto path = strands::geometry(long_path, model, exact_path);
    EXPECT_EQ(path.outcome, strands::Outcome::Match) << static_cast<int>(path.reason);
    OracleWork short_path{kFlow400SceneLimit};
    const auto cutoff = strands::geometry(long_path, model, short_path, kSentinel, {2047u, 14u});
    EXPECT_EQ(cutoff.outcome, strands::Outcome::Inconclusive);
    EXPECT_TRUE(cutoff.resource);
    Grid maximum{240, 1040, std::vector<std::string>(240u * 1040u, " ")};
    for (int row = 0; row < fourteen.grid.height; ++row)
        for (int column = 0; column < fourteen.grid.width; ++column)
            maximum.put({column, row}, std::string(fourteen.grid.at({column, row})));
    OracleWork exact_grid{kFlow400SceneLimit};
    EXPECT_EQ(strands::geometry(maximum, fourteen.model, exact_grid).outcome,
              strands::Outcome::Match);
    ++maximum.width;
    OracleWork wide{kFlow400SceneLimit};
    const auto too_wide = strands::geometry(maximum, fourteen.model, wide);
    EXPECT_EQ(too_wide.outcome, strands::Outcome::Inconclusive);
    EXPECT_TRUE(too_wide.resource);
    maximum.width = 240;
    maximum.height = 1041;
    OracleWork tall{kFlow400SceneLimit};
    EXPECT_EQ(strands::geometry(maximum, fourteen.model, tall).outcome,
              strands::Outcome::Inconclusive);
}

TEST(MermaidFlowStrands, PublicLabeledDuplicatesAndOccupiedGroupCrossingsKeepTheirOwnedRoutes) {
    constexpr std::array<std::string_view, 4> directions{"LR", "TB", "RL", "BT"};
    for (const auto direction : directions)
        for (bool grouped : {false, true}) {
            SCOPED_TRACE(std::string(direction) + "/" + std::to_string(grouped));
            const std::string source =
                grouped ? "flowchart " + std::string(direction) +
                              "\nsubgraph S\nA[Alpha]\nend\nsubgraph T\nB[Beta]\nend\nC[Gamma]\nA "
                              "--> B\nA --> B\nS --> C\n"
                        : "flowchart " + std::string(direction) +
                              "\nA[Alpha] -->|first| B[Beta]\nA -->|second| B\n";
            Model model;
            model.direction = direction;
            model.nodes = {{"A", "Alpha", Shape::Rectangle}, {"B", "Beta", Shape::Rectangle}};
            GroupEndpointProfile profile;
            if (grouped) {
                model.nodes.push_back({"C", "Gamma", Shape::Rectangle});
                model.groups = {{"S", "S", "", -1, {0}}, {"T", "T", "", -1, {1}}};
                model.edges = {{0, 1, "", "solid", false, true}, {0, 1, "", "solid", false, true}};
                profile.tagged = {{{GroupEndpointKind::Group, 0u},
                                   {GroupEndpointKind::Vertex, 2u},
                                   "",
                                   "solid",
                                   false,
                                   true}};
                profile.source_order = {{GroupEndpointOrderKind::Ordinary, 0u},
                                        {GroupEndpointOrderKind::Ordinary, 1u},
                                        {GroupEndpointOrderKind::Tagged, 0u}};
            } else
                model.edges = {{0, 1, "first", "solid", false, true},
                               {0, 1, "second", "solid", false, true}};
            auto native = loom::ui::render_markdown(group_endpoint15_fenced(source, "mermaid")),
                 ordinary = loom::ui::render_markdown(group_endpoint15_fenced(source, "text"));
            auto node = ftxui::vbox({native, ftxui::text(std::string(kSentinel))}),
                 plain = ftxui::vbox({ordinary, ftxui::text(std::string(kSentinel))});
            node->ComputeRequirement();
            plain->ComputeRequirement();
            const int height =
                std::max({32, node->requirement().min_y + 8, plain->requirement().min_y + 8});
            OracleWork work{kFlow400SceneLimit};
            std::array<std::optional<Grid>, 5> frames;
            constexpr std::array<int, 5> widths{240, 40, 240, 240, 40};
            for (std::size_t n = 0; n < 5; ++n)
                frames[n] = capture(n < 3u ? node : plain, widths[n], height, &work);
            const bool wide_source = flow400_same(*frames[0], *frames[3], work);
            EXPECT_FALSE(wide_source);
            if (wide_source) continue;
            EXPECT_TRUE(flow400_same(*frames[0], *frames[2], work));
            for (std::size_t n = 0; n < 3u; ++n) {
                if (n == 1u && flow400_same(*frames[n], *frames[4], work)) continue;
                const auto errors =
                    grouped ? flow_group_endpoint_p0_geometry(*frames[n], model, profile, work)
                            : flow400_geometry(*frames[n], model, work);
                EXPECT_TRUE(errors.empty()) << errors_text(errors);
            }
            if (grouped) {
                const auto contours = closed_frames(*frames[0], work);
                const auto candidates =
                    frame_candidates(*frames[0], contours, "S", Shape::Rectangle, work);
                ASSERT_EQ(candidates.size(), 1u);
                const auto frame = contours[static_cast<std::size_t>(candidates[0])];
                int crossings = 0, source_ports = 0;
                for (int y = frame.top; y <= frame.bottom; ++y)
                    for (int x = frame.left; x <= frame.right; ++x)
                        if (frame.boundary({x, y})) {
                            const int bits = glyph(frames[0]->at({x, y})).bits;
                            crossings += bits == 15;
                            source_ports += std::popcount(static_cast<unsigned>(bits)) == 3;
                        }
                EXPECT_GE(crossings, 2);
                EXPECT_GE(source_ports, 1);
            }
        }
}

TEST(MermaidFlowStrands, UnrelatedNodeAndGroupCannotShareAWall) {
    Grid grid{64, 32, std::vector<std::string>(2048u, " ")};
    std::array<int, 2048> bits{};
    const auto add = [&](Point p, int mask) {
        bits[static_cast<std::size_t>(grid.index(p))] |= mask;
    };
    for (const auto box : std::array<Rect, 3>{{{10, 2, 30, 22}, {2, 8, 10, 12}, {14, 8, 22, 12}}}) {
        for (int x = box.left + 1; x < box.right; ++x) {
            add({x, box.top}, L | R);
            add({x, box.bottom}, L | R);
        }
        for (int y = box.top + 1; y < box.bottom; ++y) {
            add({box.left, y}, U | D);
            add({box.right, y}, U | D);
        }
        add({box.left, box.top}, R | D);
        add({box.right, box.top}, L | D);
        add({box.left, box.bottom}, U | R);
        add({box.right, box.bottom}, U | L);
    }
    const auto ink = [](int mask) -> std::string_view {
        switch (mask) {
        case U | D:
            return "│";
        case L | R:
            return "─";
        case R | D:
            return "┌";
        case L | D:
            return "┐";
        case U | R:
            return "└";
        case U | L:
            return "┘";
        case U | D | R:
            return "├";
        case U | D | L:
            return "┤";
        default:
            throw std::runtime_error("Independent shared wall mask");
        }
    };
    for (std::size_t n = 0; n < bits.size(); ++n)
        if (bits[n]) grid.put(grid.point(static_cast<int>(n)), std::string(ink(bits[n])));
    grid.put({12, 4}, "G");
    grid.put({4, 10}, "A");
    grid.put({16, 10}, "B");
    for (std::size_t n = 0; n < kSentinel.size(); ++n)
        grid.put({static_cast<int>(n), 28}, std::string(1, kSentinel[n]));
    Model model{"LR",
                {{"A", "A", Shape::Rectangle}, {"B", "B", Shape::Rectangle}},
                {},
                {{"G", "G", "", -1, {1}}}};
    OracleWork work{kFlow400SceneLimit};
    const auto actual = strands::geometry(grid, model, work);
    EXPECT_EQ(actual.outcome, strands::Outcome::Mismatch);
    EXPECT_TRUE(actual.reason == strands::Reason::Ownership ||
                actual.reason == strands::Reason::Identity);
    EXPECT_FALSE(actual.resource);
}

TEST(MermaidFlowStrands, SourceOccurrenceOrderIsRejectedAtBindingRatherThanInferredFromPixels) {
    const auto primary = flow400_documents();
    const auto documents = flow_p0_documents(primary);
    for (const auto index : std::array<std::size_t, 2>{4u, 9u}) {
        const auto& pin = kFlowP0Pins[index];
        SCOPED_TRACE(pin.fixture);
        OracleWork work{kFlow400SceneLimit};
        const auto debit = flow_p0_strand_discovery_charge(primary, documents, work);
        EXPECT_GT(debit, 0u);
        const auto source = flow400_read(std::filesystem::path(LOOM_TESTS_DIR) /
                                         "fixtures/mermaid_official/cases" / pin.fixture);
        const auto fixture = primary.manifest.root().get("fixtures").at(pin.global),
                   base = primary.compact.root().get("cases").at(pin.ordinal);
        const auto selected =
            flow_p0_strand_select(documents, base, fixture, pin.ordinal, pin.global, source, work);
        const auto good = flow_p0_strand_model(selected, work);
        EXPECT_EQ(good.edges.size(), index == 4u ? 14u : 3u);
        const auto encoded = group_endpoint15_canonical(selected, work);
        work.product(encoded.size(), 32u);
        json::JsonMutDoc changed;
        auto bad = changed.raw_json(encoded);
        changed.set_root(bad);
        auto order = bad.get("model").get("source_edge_order");
        const auto last = order.size() - 1u;
        order.at(last).set("index", static_cast<int64_t>(last - 1u));
        order.at(last - 1u).set("index", static_cast<int64_t>(last));
        const auto corrupted = json::parse(changed.to_string());
        ASSERT_TRUE(corrupted.has_value());
        bool rejected = false;
        try {
            static_cast<void>(flow_p0_strand_model(corrupted->root(), work));
        } catch (const std::runtime_error& error) {
            rejected = true;
            EXPECT_NE(std::string_view(error.what()).find("immutable complete Model"),
                      std::string_view::npos);
        }
        EXPECT_TRUE(rejected);
        EXPECT_GT(work.remaining, 0u);
        OracleWork zero{0u};
        EXPECT_THROW(static_cast<void>(flow_p0_strand_model(selected, zero)), std::runtime_error);
        OracleWork wrong_source{kFlow400SceneLimit};
        EXPECT_THROW(
            static_cast<void>(flow_p0_strand_select(documents, base, fixture, pin.ordinal,
                                                    pin.global, source + " ", wrong_source)),
            std::runtime_error);
        EXPECT_GT(wrong_source.remaining, 0u);
    }
}

TEST(MermaidFlowStrands, EveryProbeBudgetPrefixPreservesStateAndNeverReturnsPartialProof) {
    for (bool blocked : {false, true})
        for (bool crossable : {false, true}) {
            loom::ui::mermaid::testing::Work full{1000000u};
            const auto valid =
                loom::ui::mermaid::testing::flow_group_border_probe(blocked, crossable, full);
            ASSERT_TRUE(valid.has_value());
            const auto spent = 1000000u - full.remaining;
            for (std::size_t budget = 16384u; budget < spent; ++budget) {
                loom::ui::mermaid::testing::Work limited{budget};
                EXPECT_NO_THROW({
                    const auto partial = loom::ui::mermaid::testing::flow_group_border_probe(
                        blocked, crossable, limited);
                    EXPECT_FALSE(partial.has_value());
                });
            }
        }
}

TEST(MermaidFlowStrands, UnrelatedLongCaptionDoesNotWidenEveryBandOfAParallelChain) {
    for (bool labeled_pair : {false, true}) {
        SCOPED_TRACE(labeled_pair);
        std::string source = "flowchart LR\n";
        Model model;
        model.direction = "LR";
        for (int at = 0; at < 12; ++at) {
            const std::string id(1, static_cast<char>('A' + at));
            source += id + "[" + id + "]\n";
            model.nodes.push_back({id, id, Shape::Rectangle});
        }
        source += "A --> B\nA -->" + std::string(labeled_pair ? "|second| " : " ") + "B\n";
        model.edges = {{0, 1, "", "solid", false, true},
                       {0, 1, labeled_pair ? "second" : "", "solid", false, true}};
        for (int at = 1; at < 11; ++at) {
            const std::string label = at == 10 ? "unrelated label" : "";
            source += std::string(1, static_cast<char>('A' + at)) + " -->" +
                      (label.empty() ? std::string(" ") : "|" + label + "| ") +
                      std::string(1, static_cast<char>('B' + at)) + "\n";
            model.edges.push_back({at, at + 1, label, "solid", false, true});
        }
        auto native = loom::ui::render_markdown(group_endpoint15_fenced(source, "mermaid")),
             ordinary = loom::ui::render_markdown(group_endpoint15_fenced(source, "text"));
        auto node = ftxui::vbox({native, ftxui::text(std::string(kSentinel))}),
             plain = ftxui::vbox({ordinary, ftxui::text(std::string(kSentinel))});
        node->ComputeRequirement();
        plain->ComputeRequirement();
        const int height =
            std::max({32, node->requirement().min_y + 8, plain->requirement().min_y + 8});
        OracleWork work{kFlow400SceneLimit};
        const auto good = capture(node, 240, height, &work),
                   fallback = capture(plain, 240, height, &work);
        EXPECT_FALSE(flow400_same(good, fallback, work));
        const auto errors = flow400_geometry(good, model, work);
        EXPECT_TRUE(errors.empty()) << errors_text(errors);
    }
}

TEST(MermaidFlowStrands, CompoundLabeledPairsPreserveAuthoredIdentityWhenRankingRegions) {
    for (const auto direction : std::array<std::string_view, 4>{"LR", "RL", "TB", "BT"}) {
        SCOPED_TRACE(direction);
        const std::string source = "flowchart " + std::string(direction) +
                                   "\nsubgraph S\nA[Alpha]\nend\nsubgraph T\nB[Beta]\nend\nA "
                                   "-->|first| B\nA -->|second| B\n";
        Model model{std::string(direction),
                    {{"A", "Alpha", Shape::Rectangle}, {"B", "Beta", Shape::Rectangle}},
                    {{0, 1, "first", "solid", false, true}, {0, 1, "second", "solid", false, true}},
                    {{"S", "S", "", -1, {0}}, {"T", "T", "", -1, {1}}}};
        auto native = loom::ui::render_markdown(group_endpoint15_fenced(source, "mermaid")),
             ordinary = loom::ui::render_markdown(group_endpoint15_fenced(source, "text"));
        auto node = ftxui::vbox({native, ftxui::text(std::string(kSentinel))}),
             plain = ftxui::vbox({ordinary, ftxui::text(std::string(kSentinel))});
        node->ComputeRequirement();
        plain->ComputeRequirement();
        const int height =
            std::max({32, node->requirement().min_y + 8, plain->requirement().min_y + 8});
        OracleWork work{kFlow400SceneLimit};
        const auto grid = capture(node, 240, height, &work),
                   fallback = capture(plain, 240, height, &work);
        EXPECT_FALSE(flow400_same(grid, fallback, work));
        const auto errors = flow400_geometry(grid, model, work);
        EXPECT_TRUE(errors.empty()) << errors_text(errors);
        if (direction == "LR" || direction == "RL") {
            const auto frames = closed_frames(grid, work);
            const auto a = frame_candidates(grid, frames, "S", Shape::Rectangle, work),
                       b = frame_candidates(grid, frames, "T", Shape::Rectangle, work);
            ASSERT_EQ(a.size(), 1u);
            ASSERT_EQ(b.size(), 1u);
            const auto& left = frames[static_cast<std::size_t>(direction == "LR" ? a[0] : b[0])];
            const auto& right = frames[static_cast<std::size_t>(direction == "LR" ? b[0] : a[0])];
            EXPECT_GE(right.left - left.right - 1, 14);
        }
    }
}

TEST(MermaidFlowP0, NegativeHelpersDistinguishAllocationAndAcceptedAdversaryFailures) {
    struct OpaqueAllocation final : std::bad_alloc {
        const char* what() const noexcept override { return "allocation denied"; }
    };

    OracleWork work{kFlow400SceneLimit};
    EXPECT_THROW(flow400_require_decisive_negative(
                     work, "opaque allocation",
                     []() -> std::vector<std::string> { throw OpaqueAllocation{}; }),
                 FlowOracleResourceFailure);
    EXPECT_THROW(flow400_require_decisive_negative(work, "resource word in an accepted-case label",
                                                   []() { return std::vector<std::string>{}; }),
                 FlowOracleAcceptedAdversary);
    EXPECT_THROW(
        flow400_require_decisive_negative(
            work, "candidate truncation",
            []() { return std::vector<std::string>{"GroupEndpoint route candidate bound"}; }),
        FlowOracleResourceFailure);
    for (const auto message : {"LinkProfile3 endpoint candidate bound differs",
                               "LinkProfile3 route candidate bound differs"}) {
        EXPECT_THROW(
            flow400_require_decisive_negative(work, "bounded legacy profile",
                                              [&]() { return std::vector<std::string>{message}; }),
            FlowOracleResourceFailure);
    }
    const FlowOracleAcceptedAdversary accepted{"resource word is not exhaustion"};
    EXPECT_FALSE(flow400_resource_exception(accepted, work.remaining));
    const OpaqueAllocation allocation;
    EXPECT_TRUE(flow400_resource_exception(allocation, work.remaining));
    OracleWork setup{kFlow400SceneLimit};
    const auto paper = flow_p0_paper(0, 0, setup);
    GroupEndpoint15Model model;
    model.base = paper.model;
    model.source_order = paper.profile.source_order;
    EXPECT_THROW(flow400_require_decisive_negative(work, "unknown exception",
                                                   []() -> std::vector<std::string> {
                                                       throw std::runtime_error("internal failure");
                                                   }),
                 FlowOracleInconclusiveFailure);
    GroupEndpointP0Limits injected;
    injected.assignment_failure_probe = +[]() { throw OpaqueAllocation{}; };
    EXPECT_THROW(flow400_require_decisive_negative(work, "inner opaque allocation",
                                                   [&]() {
                                                       return flow_group_endpoint_p0_geometry(
                                                           paper.grid, paper.model, paper.profile,
                                                           work, kSentinel, injected);
                                                   }),
                 FlowOracleResourceFailure);
    injected.assignment_failure_probe = +[]() { throw std::runtime_error("internal failure"); };
    EXPECT_THROW(flow400_require_decisive_negative(work, "inner unknown failure",
                                                   [&]() {
                                                       return flow_group_endpoint_p0_geometry(
                                                           paper.grid, paper.model, paper.profile,
                                                           work, kSentinel, injected);
                                                   }),
                 FlowOracleInconclusiveFailure);
    const auto cap_paper = flow_p0_paper(1, 0, setup);
    setup.spend(129u * (sizeof(GroupEndpointEdge) * 3u + 64u));
    auto oversized_profile = cap_paper.profile;
    ASSERT_FALSE(oversized_profile.tagged.empty());
    oversized_profile.tagged.resize(129u, oversized_profile.tagged.front());
    const auto bounded =
        flow_group_endpoint_p0_geometry(cap_paper.grid, cap_paper.model, oversized_profile, work);
    ASSERT_EQ(bounded.size(), 1u);
    ASSERT_EQ(bounded.front(), "GroupEndpoint profile cardinality bound");
    EXPECT_THROW(flow400_require_decisive_negative(work, "actual profile cardinality cap",
                                                   [&]() {
                                                       return flow_group_endpoint_p0_geometry(
                                                           cap_paper.grid, cap_paper.model,
                                                           oversized_profile, work);
                                                   }),
                 FlowOracleResourceFailure);
    OracleWork positive{kFlow400SceneLimit};
    EXPECT_THROW(flow_p0_group_rejected(paper.grid, model, positive, kSentinel),
                 FlowOracleAcceptedAdversary);
    OracleWork exhausted_p0{0u};
    EXPECT_THROW(flow_p0_group_rejected(paper.grid, model, exhausted_p0, kSentinel),
                 FlowOracleResourceFailure);
    OracleWork exhausted_legacy{0u};
    EXPECT_THROW(group_endpoint15_require_rejected(paper.grid, model, exhausted_legacy,
                                                   "legacy resource control", kSentinel),
                 FlowOracleResourceFailure);
}

}  // namespace
