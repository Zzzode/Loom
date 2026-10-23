// RFC 0030 test-first group endpoint cohort. Installed only after independent review.
#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <filesystem>
import std;
import loom.ui.visual.markdown;
import loom.serdes.json;
#include "mermaid_flow_group_endpoint15_oracle.hpp"

namespace {
namespace json=loom::utils::json;
using json::JsonVal;
using namespace loom::test::mermaid_flow;

std::string group_endpoint15_fenced(std::string_view text,std::string_view kind) {
    return "```"+std::string(kind)+"\n"+std::string(text)+"\n```";
}
JsonVal group_endpoint15_fixture(JsonVal fixtures,std::string_view path) {
    JsonVal found;std::size_t count=0u;
    fixtures.iter([&](JsonVal fixture) { if(flow400_text(fixture.get("path"))==path) {found=fixture;++count;} });
    flow400_require(count==1u,"GroupEndpoint15 immutable fixture selection");return found;
}
void group_endpoint15_publish_frame(json::JsonMutVal row,std::string_view key,const Grid& grid,OracleWork& work) {
    work.product(grid.cells.size(),8u);std::size_t bytes=0u;
    for(const auto& cell:grid.cells) {flow400_require(cell.size()<=std::numeric_limits<std::size_t>::max()-bytes,"GroupEndpoint15 frame payload overflow");bytes+=cell.size();}
    work.product(bytes,48u);work.product(static_cast<std::size_t>(3*grid.height+32),8u);
    work.product(static_cast<std::size_t>(grid.height),3u*sizeof(std::string)+4u*sizeof(json::JsonMutVal)+64u);work.spend(4u*sizeof(json::JsonDoc)+512u);
    auto lines=row.make_arr();for(int y=0;y<grid.height;++y) lines.append(row.make_str(grid.row(y)));row.add(key,lines);
}
TEST(MermaidFlowGroupEndpoint15, ImmutableAuthorityRejectsForgedExactTupleAndWire) {
    auto owned=group_endpoint15_documents();const auto record=owned.asset.root().get("records").at(0u);OracleWork work{kFlow400SceneLimit};group_endpoint15_record(record,work);
    json::JsonMutDoc forged;auto root=forged.raw_json(record.to_string());forged.set_root(root);root.set("case_id","flow400_group_endpoint_48");
    const auto parsed=json::parse(forged.to_string());ASSERT_TRUE(parsed.has_value());EXPECT_THROW(group_endpoint15_record(parsed->root(),work),std::runtime_error);
    json::JsonMutDoc wire;auto model=wire.raw_json(record.get("model").to_string());wire.set_root(model);model.get("tagged_group_endpoint_edges").at(0u).get("source").set("kind","vertex");
    const auto parsed_wire=json::parse(wire.to_string());ASSERT_TRUE(parsed_wire.has_value());EXPECT_THROW(group_endpoint15_model(parsed_wire->root(),work),std::runtime_error);
}

TEST(MermaidFlowGroupEndpoint15, CompleteOriginalSourcesRequireOwnedFiveFrameGeometry) {
    auto flow400=flow400_documents();auto owned=group_endpoint15_documents();const auto records=owned.asset.root().get("records");
    const auto fixtures=flow400.manifest.root().get("fixtures"),cases=flow400.compact.root().get("cases");
    std::size_t complete=0u,fallbacks=0u,resources=0u,published=0u,rejected=0u;
    constexpr std::array<std::string_view,5> names{{"wide_frame","narrow_frame","restored_frame","ordinary_wide_frame","ordinary_narrow_frame"}};
    records.iter([&](JsonVal record) {
        OracleWork work{kFlow400SceneLimit};work.spend(65536u+32u*sizeof(Grid)+16u*sizeof(GroupEndpoint15Model)+8u*sizeof(json::JsonMutVal)+4096u);
        bool resource=false;
        try {
            const auto ordinal=flow400_number(record.get("ordinal"));
            const auto global=flow400_number(record.get("manifest_global_ordinal"));
            const auto path=flow400_text(record.get("fixture"));
            const auto fixture=group_endpoint15_fixture(fixtures,path);const auto source_path=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_official/cases"/std::string(path);
            work.product(flow400_number(fixture.get("bytes")),32u);work.product(path.size(),8u);const auto source=flow400_read(source_path);
            flow400_require(source==flow400_text(record.get("raw_source_utf8")),"GroupEndpoint15 immutable full source differs");
            flow400_require(source.size()==flow400_number(fixture.get("bytes"))&&digest(source)==flow400_text(fixture.get("sha256")),"GroupEndpoint15 actual fixture SHA");
            auto mermaid=loom::ui::render_markdown(group_endpoint15_fenced(source,"mermaid"));auto ordinary=loom::ui::render_markdown(group_endpoint15_fenced(source,"text"));
            auto element=ftxui::vbox({mermaid,ftxui::text(std::string(kSentinel))});auto plain=ftxui::vbox({ordinary,ftxui::text(std::string(kSentinel))});element->ComputeRequirement();plain->ComputeRequirement();
            const int height=std::max({32,element->requirement().min_y+8,plain->requirement().min_y+8});flow400_require(height<=1040,"GroupEndpoint15 bounded source height");
            json::JsonMutDoc publication;auto row=publication.object();std::array<std::optional<Grid>,5> frames;constexpr std::array<int,5> widths{{240,40,240,240,40}};
            for(std::size_t index=0u;index<frames.size();++index) {frames[index]=capture(index<3u?element:plain,widths[index],height,&work);group_endpoint15_publish_frame(row,names[index],*frames[index],work);++published;}
            flow400_require(published%5u==0u&&row.size()==5u,"GroupEndpoint15 five full frames attached before bind");
            for(const auto& frame:frames) static_cast<void>(flow400_following(*frame,work));
            const bool source_wide=flow400_same(*frames[0],*frames[3],work),source_narrow=flow400_same(*frames[1],*frames[4],work),restored=flow400_same(*frames[0],*frames[2],work);
            flow400_require(restored,"GroupEndpoint15 restored wide cells differ");
            group_endpoint15_bind(cases.at(ordinal),fixture,records,record,ordinal,global,source,work);
            const auto model=group_endpoint15_model(record.get("model"),work);
            if(source_wide) {++fallbacks;throw std::runtime_error("GroupEndpoint15 mandatory original returned ordinary source");}
            const auto wide=group_endpoint15_geometry(*frames[0],model,work,kSentinel);flow400_require(wide.empty(),wide.empty()?"":wide.front().c_str());
            if(!source_narrow) {const auto narrow=group_endpoint15_geometry(*frames[1],model,work,kSentinel);flow400_require(narrow.empty(),narrow.empty()?"":narrow.front().c_str());}
            const auto restored_errors=group_endpoint15_geometry(*frames[2],model,work,kSentinel);flow400_require(restored_errors.empty(),restored_errors.empty()?"":restored_errors.front().c_str());
            const auto local=group_endpoint15_mutations(*frames[0],model,work,kSentinel);flow400_require(local==7u,"GroupEndpoint15 exact meaningful source mutations");rejected+=local;++complete;
        } catch(const std::exception& error) {resource=work.remaining==0u||flow400_resource(error.what());}
        resources+=resource;EXPECT_FALSE(resource);EXPECT_NE(work.remaining,0u);
    });
    EXPECT_EQ(published,75u);EXPECT_EQ(complete,15u);EXPECT_EQ(fallbacks,0u);EXPECT_EQ(resources,0u);EXPECT_EQ(rejected,105u);
}

struct GroupEndpoint15HandControlSpec {
    std::string_view id,basis,mutation,must_reject,scene_id;
};
constexpr std::array<GroupEndpoint15HandControlSpec,8> kGroupEndpoint15HandControlSpecs{{
    {"semantic_group_to_wrong_child","cluster_to_cluster_then_cluster_to_child_td_hand_good","Change the tagged group:two target from vertex:c2 to vertex:b1 while retaining a well-formed decoded Model and the Grid.","The grid owns the tagged target at child c2, not sibling child b1.","cluster_to_cluster_then_cluster_to_child_td_hand_good"},
    {"semantic_wrong_sibling_group","cluster_to_cluster_plus_child_crossing_td_hand_good","Change the tagged S1→S2 source to sibling group S2 while retaining a well-formed decoded Model and the Grid.","Named group endpoint identity is exact.","cluster_to_cluster_plus_child_crossing_td_hand_good"},
    {"semantic_group_to_child_source","cluster_to_cluster_then_cluster_to_child_td_hand_good","Change the group:two source tag of group_to_child to vertex:b1 in a well-formed decoded Model without changing the Grid.","The grid owns group two perimeter, not child b1.","cluster_to_cluster_then_cluster_to_child_td_hand_good"},
    {"physical_tangential_group_contact","cluster_to_cluster_plus_child_crossing_td_hand_good","Replace the downward S1 group-source junction on its bottom straight border with a right-facing junction.","A tangential contact is not a legal outward group endpoint.","cluster_to_cluster_plus_child_crossing_td_hand_good"},
    {"physical_unclaimed_group_shortcut","cluster_to_cluster_plus_child_crossing_td_hand_good","Insert an unconsumed container-border junction.","Perimeter arms require exclusive route ownership.","cluster_to_cluster_plus_child_crossing_td_hand_good"},
    {"semantic_crossing_boundary_as_group_target","cluster_to_cluster_plus_child_crossing_td_hand_good","Retag child-crossing ordinary edge sub1→sub4 as tagged sub1→group:S1 in a rebuilt well-formed Model while retaining the Grid.","The S1 boundary crossing is not the tagged endpoint; the Grid still ends at child sub4.","cluster_to_cluster_plus_child_crossing_td_hand_good"},
    {"model_missing_direct_membership","cluster_to_cluster_plus_child_crossing_td_hand_good","Remove sub4 from S2 direct membership in a rebuilt well-formed Model.","Existing group membership obligations remain active.","cluster_to_cluster_plus_child_crossing_td_hand_good"},
    {"model_duplicate_source_order","cluster_to_cluster_plus_child_crossing_td_hand_good","Reuse source_edge_order index 1 and omit index 2.","Ordinary plus tagged source order is an exact partition.","cluster_to_cluster_plus_child_crossing_td_hand_good"},
}};
struct GroupEndpoint15HandBinding { std::string_view scene_id,source_case_id,topology; };
constexpr std::array<GroupEndpoint15HandBinding,8> kGroupEndpoint15HandBindings{{
    {"cluster_to_cluster_then_cluster_to_child_td_hand_good","flow400_group_endpoint_46","cluster_to_cluster_then_cluster_to_child"},
    {"cluster_to_cluster_then_cluster_to_child_bt_hand_good","flow400_group_endpoint_46","cluster_to_cluster_then_cluster_to_child"},
    {"cluster_to_cluster_then_cluster_to_child_lr_hand_good","flow400_group_endpoint_46","cluster_to_cluster_then_cluster_to_child"},
    {"cluster_to_cluster_then_cluster_to_child_rl_hand_good","flow400_group_endpoint_46","cluster_to_cluster_then_cluster_to_child"},
    {"cluster_to_cluster_plus_child_crossing_td_hand_good","flow400_group_endpoint_51","cluster_to_cluster_plus_child_crossing"},
    {"cluster_to_cluster_plus_child_crossing_bt_hand_good","flow400_group_endpoint_51","cluster_to_cluster_plus_child_crossing"},
    {"cluster_to_cluster_plus_child_crossing_lr_hand_good","flow400_group_endpoint_51","cluster_to_cluster_plus_child_crossing"},
    {"cluster_to_cluster_plus_child_crossing_rl_hand_good","flow400_group_endpoint_51","cluster_to_cluster_plus_child_crossing"},
}};

JsonVal group_endpoint15_hand_scene(JsonVal root,std::string_view id) {
    JsonVal found;std::size_t count=0u;const auto scenes=root.get("good_scenes");
    scenes.iter([&](JsonVal scene) {if(flow400_text(scene.get("id"))==id) {found=scene;++count;}});
    flow400_require(count==1u,"GroupEndpoint15 declared control hand scene");return found;
}
JsonVal group_endpoint15_hand_route(JsonVal scene,std::string_view role) {
    JsonVal found;std::size_t count=0u;scene.get("routes").iter([&](JsonVal route) {if(flow400_text(route.get("edge_role"))==role) {found=route;++count;}});
    flow400_require(count==1u,"GroupEndpoint15 declared control route");return found;
}
JsonVal group_endpoint15_hand_frame(JsonVal scene,std::string_view id) {
    JsonVal found;std::size_t count=0u;scene.get("frames").iter([&](JsonVal frame) {if(flow400_text(frame.get("id"))==id) {found=frame;++count;}});
    flow400_require(count==1u,"GroupEndpoint15 declared control frame");return found;
}
Point group_endpoint15_hand_point(JsonVal point) {
    flow400_require(point.is_arr()&&point.size()==2u,"GroupEndpoint15 hand point");
    return {static_cast<int>(flow400_number(point.at(0u))),static_cast<int>(flow400_number(point.at(1u)))};
}
JsonVal group_endpoint15_hand_record(JsonVal records,std::string_view case_id) {
    JsonVal found;std::size_t count=0u;
    records.iter([&](JsonVal record) {if(flow400_text(record.get("case_id"))==case_id) {found=record;++count;}});
    flow400_require(count==1u,"GroupEndpoint15 hand source record");return found;
}
std::string group_endpoint15_directionless_canonical(JsonVal wire,OracleWork& work) {
    work.product(kFlow400GroupEndpoint15ModelWireBytes,32u);work.spend(4u*sizeof(json::JsonMutDoc)+1024u);
    json::JsonMutDoc document;auto root=document.copy_val(wire);auto base=root.get("base_model");
    flow400_require(base.remove("direction"),"GroupEndpoint15 hand Model direction");base.add("direction",base.make_str("__directionless__"));document.set_root(root);
    const auto encoded=document.to_string();work.product(encoded.size(),8u);const auto parsed=json::parse(encoded);
    flow400_require(parsed.has_value(),"GroupEndpoint15 directionless Model JSON");return group_endpoint15_canonical(parsed->root(),work);
}
struct GroupEndpoint15HandModel { JsonVal record; GroupEndpoint15Model model; };
GroupEndpoint15HandModel group_endpoint15_hand_model(JsonVal hand_root,JsonVal records,JsonVal scene,OracleWork& work) {
    const auto scene_id=flow400_text(scene.get("id"));const auto topology=flow400_text(scene.get("source_derived_obligation"));const auto direction=flow400_text(scene.get("direction"));
    const auto binding=std::ranges::find_if(kGroupEndpoint15HandBindings,[&](const auto& value) {return value.scene_id==scene_id;});
    flow400_require(binding!=kGroupEndpoint15HandBindings.end()&&binding->topology==topology,"GroupEndpoint15 hand binding map");
    JsonVal asset_binding;std::size_t binding_count=0u;hand_root.get("hand_model_bindings").iter([&](JsonVal value) {
        group_endpoint15_keys(value,{"scene_id","source_case_id","topology"},"hand Model binding");
        if(flow400_text(value.get("scene_id"))==scene_id) {asset_binding=value;++binding_count;}
    });
    flow400_require(binding_count==1u&&flow400_text(asset_binding.get("source_case_id"))==binding->source_case_id&&flow400_text(asset_binding.get("topology"))==binding->topology,"GroupEndpoint15 immutable hand binding");
    const auto record=group_endpoint15_hand_record(records,binding->source_case_id);flow400_require(flow400_text(record.get("topology"))==topology,"GroupEndpoint15 hand selected topology");
    const auto canonical=group_endpoint15_directionless_canonical(record.get("model"),work);std::size_t equivalent=0u;
    records.iter([&](JsonVal value) {if(flow400_text(value.get("topology"))==topology) {++equivalent;flow400_require(group_endpoint15_directionless_canonical(value.get("model"),work)==canonical,"GroupEndpoint15 hand source Model differs beyond direction");}});
    const std::size_t expected=topology=="cluster_to_cluster_then_cluster_to_child"?8u:7u;
    flow400_require(equivalent==expected,"GroupEndpoint15 hand topology source set");auto model=group_endpoint15_model(record.get("model"),work);model.base.direction=std::string(direction);return {record,std::move(model)};
}
Grid group_endpoint15_hand_grid(JsonVal scene,OracleWork& work) {
    const int width=static_cast<int>(flow400_number(scene.get("grid_width"))),height=static_cast<int>(flow400_number(scene.get("grid_height")));
    flow400_require(width>0&&width<=240&&height>0&&height<=1040,"GroupEndpoint15 hand dimensions");
    const auto cells=static_cast<std::size_t>(width)*static_cast<std::size_t>(height);
    work.spend(4u*sizeof(Grid)+8u*sizeof(std::size_t)+256u);work.product(cells,3u*sizeof(std::string)+16u);
    Grid grid{width,height,{}};grid.cells.assign(cells," ");
    const auto put=[&](int x,int y,std::string_view cell) {flow400_require(grid.has({x,y}),"GroupEndpoint15 hand coordinate bound");grid.put({x,y},std::string(cell));};
    scene.get("frames").iter([&](JsonVal value) {const auto rect=value.get("rect");const int left=static_cast<int>(flow400_number(rect.at(0u))),top=static_cast<int>(flow400_number(rect.at(1u))),right=static_cast<int>(flow400_number(rect.at(2u))),bottom=static_cast<int>(flow400_number(rect.at(3u)));
        for(int x=left+1;x<right;++x) {put(x,top,"─");put(x,bottom,"─");}for(int y=top+1;y<bottom;++y) {put(left,y,"│");put(right,y,"│");}put(left,top,"┌");put(right,top,"┐");put(left,bottom,"└");put(right,bottom,"┘");
        const auto start=value.get("label_start");const auto id=flow400_text(value.get("id"));for(std::size_t index=0u;index<id.size();++index) put(static_cast<int>(flow400_number(start.at(0u))+index),static_cast<int>(flow400_number(start.at(1u))),id.substr(index,1u));});
    scene.get("routes").iter([&](JsonVal route) {const auto source=route.get("source_port"),target=route.get("target_port"),head=route.get("arrow_head");const auto source_point=source.get("point"),target_point=target.get("point"),head_point=head.get("point");
        put(static_cast<int>(flow400_number(source_point.at(0u))),static_cast<int>(flow400_number(source_point.at(1u))),flow400_text(source.get("glyph")));put(static_cast<int>(flow400_number(target_point.at(0u))),static_cast<int>(flow400_number(target_point.at(1u))),flow400_text(target.get("glyph")));put(static_cast<int>(flow400_number(head_point.at(0u))),static_cast<int>(flow400_number(head_point.at(1u))),flow400_text(head.get("glyph")));
        route.get("route_glyphs").iter([&](JsonVal cell) {put(static_cast<int>(flow400_number(cell.at(0u))),static_cast<int>(flow400_number(cell.at(1u))),flow400_text(cell.at(2u)));});});
    const int sentinel=static_cast<int>(flow400_number(scene.get("sentinel_row")));const auto token=flow400_text(scene.get("exact_following_token"));for(std::size_t index=0u;index<token.size();++index) put(static_cast<int>(index),sentinel,token.substr(index,1u));return grid;
}
void group_endpoint15_wire_model_and_reject(JsonVal record,const Grid& grid,OracleWork& work,std::string_view why,std::string_view token,auto&& mutate) {
    work.product(kFlow400GroupEndpoint15ModelWireBytes,48u);work.spend(8u*sizeof(json::JsonMutDoc)+2048u);json::JsonMutDoc document;auto root=document.copy_val(record.get("model"));mutate(document,root);document.set_root(root);
    const auto encoded=document.to_string();work.product(encoded.size(),8u);const auto parsed=json::parse(encoded);flow400_require(parsed.has_value(),"GroupEndpoint15 wrong Model JSON");
    const auto wrong=group_endpoint15_model(parsed->root(),work);group_endpoint15_require_rejected(grid,wrong,work,why,token);
}
void group_endpoint15_wire_decode_reject(JsonVal record,OracleWork& work,std::string_view why,auto&& mutate) {
    work.product(kFlow400GroupEndpoint15ModelWireBytes,48u);work.spend(8u*sizeof(json::JsonMutDoc)+2048u);json::JsonMutDoc document;auto root=document.copy_val(record.get("model"));mutate(document,root);document.set_root(root);
    const auto encoded=document.to_string();work.product(encoded.size(),8u);const auto parsed=json::parse(encoded);flow400_require(parsed.has_value(),"GroupEndpoint15 adversarial Model JSON");
    bool rejected=false,resource=false;try {static_cast<void>(group_endpoint15_model(parsed->root(),work));}catch(const std::exception& error) {resource=work.remaining==0u||flow400_resource(error.what());rejected=!resource;}
    flow400_require(rejected&&!resource,std::string(why).c_str());
}
void group_endpoint15_wire_replace_ref(json::JsonMutVal ref,std::string_view kind,std::string_view id) {
    flow400_require(ref.remove("kind")&&ref.remove("id"),"GroupEndpoint15 mutable endpoint ref");ref.add("kind",ref.make_str(kind));ref.add("id",ref.make_str(id));
}
void group_endpoint15_wire_crossing_as_group_target(json::JsonMutDoc&,json::JsonMutVal root) {
    auto base=root.get("base_model");auto old_edges=base.get("edges"),old_tagged=root.get("tagged_group_endpoint_edges");
    flow400_require(old_edges.is_arr()&&old_edges.size()==2u&&old_tagged.is_arr()&&old_tagged.size()==1u,"GroupEndpoint15 crossing wire witness");
    auto first_edge=old_edges.at(0u);auto retained=root.make_obj();retained.set("source",first_edge.get("source").as_str());retained.set("target",first_edge.get("target").as_str());retained.set("label",first_edge.get("label").as_str());retained.set("style",first_edge.get("style").as_str());retained.set("hasArrowStart",first_edge.get("hasArrowStart").as_bool());retained.set("hasArrowEnd",first_edge.get("hasArrowEnd").as_bool());
    auto ordinary=root.make_arr();ordinary.append(retained);flow400_require(base.remove("edges"),"GroupEndpoint15 crossing ordinary replace");base.add("edges",ordinary);
    auto first_tagged=old_tagged.at(0u);auto old_source=first_tagged.get("source"),old_target=first_tagged.get("target");
    auto tagged=root.make_arr(),kept=root.make_obj(),kept_source=kept.make_obj(),kept_target=kept.make_obj();
    kept_source.set("kind",old_source.get("kind").as_str());kept_source.set("id",old_source.get("id").as_str());kept_target.set("kind",old_target.get("kind").as_str());kept_target.set("id",old_target.get("id").as_str());kept.add("source",kept_source);kept.add("target",kept_target);kept.set("label",first_tagged.get("label").as_str());kept.set("style",first_tagged.get("style").as_str());kept.set("hasArrowStart",first_tagged.get("hasArrowStart").as_bool());kept.set("hasArrowEnd",first_tagged.get("hasArrowEnd").as_bool());kept.set("source_order",first_tagged.get("source_order").as_int());tagged.append(kept);
    auto edge=root.make_obj(),source=edge.make_obj(),target=edge.make_obj();source.set("kind","vertex");source.set("id","sub1");target.set("kind","group");target.set("id","S1");edge.add("source",source);edge.add("target",target);edge.set("label","");edge.set("style","solid");edge.set("hasArrowStart",false);edge.set("hasArrowEnd",true);edge.set("source_order",2);tagged.append(edge);flow400_require(root.remove("tagged_group_endpoint_edges"),"GroupEndpoint15 crossing tagged replace");root.add("tagged_group_endpoint_edges",tagged);
    auto order=root.make_arr(),vertex=root.make_obj(),tag0=root.make_obj(),tag1=root.make_obj();vertex.set("kind","vertex");vertex.set("index",0);tag0.set("kind","group_endpoint");tag0.set("index",0);tag1.set("kind","group_endpoint");tag1.set("index",1);order.append(vertex);order.append(tag0);order.append(tag1);flow400_require(root.remove("source_edge_order"),"GroupEndpoint15 crossing order replace");root.add("source_edge_order",order);
    auto counts=root.make_obj();counts.set("nodes",3);counts.set("groups",2);counts.set("ordinary_vertex_edges",1);counts.set("group_endpoint_edges",2);counts.set("source_edges",3);flow400_require(root.remove("source_semantic_counts"),"GroupEndpoint15 crossing count replace");root.add("source_semantic_counts",counts);
}
std::size_t group_endpoint15_hand_controls(JsonVal scene,const GroupEndpoint15Model& model,Grid good,OracleWork& work) {
    const auto token=flow400_text(scene.get("exact_following_token"));
    const auto good_errors=group_endpoint15_geometry(good,model,work,flow400_text(scene.get("exact_following_token")));flow400_require(good_errors.empty(),good_errors.empty()?"":good_errors.front().c_str());
    std::size_t rejected=group_endpoint15_semantic_mutations(good,model,work,token);flow400_require(rejected==4u,"GroupEndpoint15 hand semantic negative count");
    const auto routes=scene.get("routes");flow400_require(routes.size()>=2u,"GroupEndpoint15 hand route witnesses");
    const auto mutate_cell=[&](Grid& bad,JsonVal route,std::string_view field,std::string_view replacement) {const auto point=route.get(field).get("point");bad.put({static_cast<int>(flow400_number(point.at(0u))),static_cast<int>(flow400_number(point.at(1u)))},std::string(replacement));};
    group_endpoint15_copy_grid(good,work);auto broken_reciprocal=good;const auto first_route=routes.at(0u);const auto source=first_route.get("source_port");const auto outward=flow400_text(source.get("outward"));
    mutate_cell(broken_reciprocal,first_route,"source_port",outward=="U"?"┬":outward=="D"?"┴":outward=="L"?"├":"┤");group_endpoint15_require_rejected(broken_reciprocal,model,work,"GroupEndpoint15 broken reciprocal source arm accepted",token);++rejected;
    group_endpoint15_copy_grid(good,work);auto head_on_perimeter=good;mutate_cell(head_on_perimeter,first_route,"target_port",flow400_text(first_route.get("arrow_head").get("glyph")));group_endpoint15_require_rejected(head_on_perimeter,model,work,"GroupEndpoint15 arrowhead on target perimeter accepted",token);++rejected;
    group_endpoint15_copy_grid(good,work);auto extra_branch=good;const auto cells=first_route.get("route_glyphs");flow400_require(cells.size()>0u,"GroupEndpoint15 hand path witness");const auto cell=cells.at(0u);extra_branch.put({static_cast<int>(flow400_number(cell.at(0u))),static_cast<int>(flow400_number(cell.at(1u)))},"┼");group_endpoint15_require_rejected(extra_branch,model,work,"GroupEndpoint15 unconsumed route branch accepted",token);++rejected;
    group_endpoint15_copy_grid(good,work);auto crossing=good;const auto second_route=routes.at(1u);const auto second_cells=second_route.get("route_glyphs");flow400_require(second_cells.size()>0u,"GroupEndpoint15 crossing witness");const auto cross=second_cells.at(0u);crossing.put({static_cast<int>(flow400_number(cross.at(0u))),static_cast<int>(flow400_number(cross.at(1u)))},"┼");group_endpoint15_require_rejected(crossing,model,work,"GroupEndpoint15 nonconsumed group crossing accepted",token);++rejected;
    return rejected;
}
std::size_t group_endpoint15_declared_hand_control(JsonVal hand_root,JsonVal records,JsonVal control,OracleWork& work) {
    group_endpoint15_keys(control,{"id","basis","mutation","must_reject"},"GroupEndpoint15 declared hand control");
    const auto id=flow400_text(control.get("id"));const auto found=std::ranges::find_if(kGroupEndpoint15HandControlSpecs,[&](const auto& spec) {return spec.id==id;});
    flow400_require(found!=kGroupEndpoint15HandControlSpecs.end(),"GroupEndpoint15 declared hand control id");
    flow400_require(flow400_text(control.get("basis"))==found->basis&&flow400_text(control.get("mutation"))==found->mutation&&flow400_text(control.get("must_reject"))==found->must_reject,"GroupEndpoint15 declared hand control text");
    const auto scene=group_endpoint15_hand_scene(hand_root,found->scene_id);const auto hand=group_endpoint15_hand_model(hand_root,records,scene,work);auto good=group_endpoint15_hand_grid(scene,work);
    const auto token=flow400_text(scene.get("exact_following_token"));const auto baseline=group_endpoint15_geometry(good,hand.model,work,token);flow400_require(baseline.empty(),"GroupEndpoint15 declared control requires complete good scene");
    const auto reject_reason=flow400_text(control.get("must_reject"));
    if(id=="semantic_group_to_wrong_child") {
        group_endpoint15_wire_model_and_reject(hand.record,good,work,reject_reason,token,[](json::JsonMutDoc&,json::JsonMutVal root) {group_endpoint15_wire_replace_ref(root.get("tagged_group_endpoint_edges").at(1u).get("target"),"vertex","b1");});
    } else if(id=="semantic_wrong_sibling_group") {
        group_endpoint15_wire_model_and_reject(hand.record,good,work,reject_reason,token,[](json::JsonMutDoc&,json::JsonMutVal root) {group_endpoint15_wire_replace_ref(root.get("tagged_group_endpoint_edges").at(0u).get("source"),"group","S2");});
    } else if(id=="semantic_group_to_child_source") {
        group_endpoint15_wire_model_and_reject(hand.record,good,work,reject_reason,token,[](json::JsonMutDoc&,json::JsonMutVal root) {group_endpoint15_wire_replace_ref(root.get("tagged_group_endpoint_edges").at(1u).get("source"),"vertex","b1");});
    } else if(id=="physical_tangential_group_contact") {
        group_endpoint15_copy_grid(good,work);auto bad=good;const auto route=group_endpoint15_hand_route(scene,"group_to_group");const auto port=group_endpoint15_hand_point(route.get("source_port").get("point"));flow400_require(bad.at(port)=="┬","GroupEndpoint15 tangential source witness");bad.put(port,"├");group_endpoint15_require_rejected(bad,hand.model,work,reject_reason,token);
    } else if(id=="physical_unclaimed_group_shortcut") {
        group_endpoint15_copy_grid(good,work);auto bad=good;const auto rect=group_endpoint15_hand_frame(scene,"S1").get("rect");const Point shortcut{static_cast<int>((flow400_number(rect.at(0u))+flow400_number(rect.at(2u)))/2u),static_cast<int>(flow400_number(rect.at(3u)))};flow400_require(bad.at(shortcut)=="─","GroupEndpoint15 unclaimed container border witness");bad.put(shortcut,"┼");group_endpoint15_require_rejected(bad,hand.model,work,reject_reason,token);
    } else if(id=="semantic_crossing_boundary_as_group_target") {
        group_endpoint15_wire_model_and_reject(hand.record,good,work,reject_reason,token,group_endpoint15_wire_crossing_as_group_target);
    } else if(id=="model_missing_direct_membership") {
        group_endpoint15_wire_model_and_reject(hand.record,good,work,reject_reason,token,[](json::JsonMutDoc&,json::JsonMutVal root) {auto groups=root.get("base_model").get("subgraphs");json::JsonMutVal group;groups.iter([&](json::JsonMutVal value,std::size_t) {if(value.get("id").as_str()=="S2") group=value;});flow400_require(group.valid(),"GroupEndpoint15 membership wire group");auto members=group.get("nodeIds"),replacement=group.make_arr();members.iter([&](json::JsonMutVal value,std::size_t) {if(value.as_str()!="sub4") replacement.append(replacement.make_str(value.as_str()));});flow400_require(group.remove("nodeIds"),"GroupEndpoint15 membership wire replace");group.add("nodeIds",replacement);});
    } else if(id=="model_duplicate_source_order") {
        group_endpoint15_wire_decode_reject(hand.record,work,reject_reason,[](json::JsonMutDoc&,json::JsonMutVal root) {auto item=root.get("source_edge_order").at(2u);flow400_require(item.remove("kind"),"GroupEndpoint15 order kind mutation");item.set("kind","group_endpoint");flow400_require(item.remove("index"),"GroupEndpoint15 order mutation");item.set("index",0);});
    } else throw std::runtime_error("GroupEndpoint15 declared hand control dispatch");
    flow400_require(work.remaining!=0u,"GroupEndpoint15 declared hand control resource");return 1u;
}
TEST(MermaidFlowGroupEndpoint15, GeometryRequiresCallerOwnedFollowingToken) {
    auto documents=group_endpoint15_documents();auto hands=group_endpoint15_hand_documents();const auto records=documents.asset.root().get("records");const auto root=hands.asset.root();
    const auto scene=group_endpoint15_hand_scene(root,"cluster_to_cluster_then_cluster_to_child_td_hand_good");OracleWork work{kFlow400SceneLimit};
    const auto hand=group_endpoint15_hand_model(root,records,scene,work);const auto good=group_endpoint15_hand_grid(scene,work);
    const auto token=flow400_text(scene.get("exact_following_token"));ASSERT_TRUE(group_endpoint15_geometry(good,hand.model,work,token).empty());
    bool rejected=false;try {static_cast<void>(group_endpoint15_geometry(good,hand.model,work,"WRONG_GROUP_ENDPOINT_TOKEN"));}
    catch(const std::runtime_error& error) {rejected=std::string_view(error.what())=="Flow400 protocol exact independent following row";}
    EXPECT_TRUE(rejected);EXPECT_NE(work.remaining,0u);
}

TEST(MermaidFlowGroupEndpoint15, FrontierQuotientCycleSuppressesOnlyOwnerLocalCycleRankChecks) {
    auto documents=group_endpoint15_documents();auto hands=group_endpoint15_hand_documents();const auto records=documents.asset.root().get("records"),root=hands.asset.root();
    const auto cycle_scene=group_endpoint15_hand_scene(root,"cluster_to_cluster_then_cluster_to_child_td_hand_good");OracleWork cycle_work{kFlow400SceneLimit};const auto cycle=group_endpoint15_hand_model(root,records,cycle_scene,cycle_work);const auto cycle_grid=group_endpoint15_hand_grid(cycle_scene,cycle_work);const auto cycle_errors=group_endpoint15_geometry(cycle_grid,cycle.model,cycle_work,flow400_text(cycle_scene.get("exact_following_token")));EXPECT_TRUE(cycle_errors.empty());EXPECT_NE(cycle_work.remaining,0u);
    const auto crossing_scene=group_endpoint15_hand_scene(root,"cluster_to_cluster_plus_child_crossing_td_hand_good");OracleWork crossing_work{kFlow400SceneLimit};const auto crossing=group_endpoint15_hand_model(root,records,crossing_scene,crossing_work);const auto crossing_grid=group_endpoint15_hand_grid(crossing_scene,crossing_work);const auto crossing_errors=group_endpoint15_geometry(crossing_grid,crossing.model,crossing_work,flow400_text(crossing_scene.get("exact_following_token")));EXPECT_TRUE(crossing_errors.empty());group_endpoint15_copy_model(crossing.model,crossing_work);auto wrong=crossing.model;wrong.base.direction="BT";group_endpoint15_require_rejected(crossing_grid,wrong,crossing_work,"GroupEndpoint15 acyclic frontier rank reversal accepted",flow400_text(crossing_scene.get("exact_following_token")));EXPECT_NE(crossing_work.remaining,0u);
}

TEST(MermaidFlowGroupEndpoint15, PublicDeclaredGroupToVertexOwnsCompleteGeometryAndClosedGrammar) {
    constexpr std::string_view source="flowchart TD\nsubgraph G\nA\nend\nG --> B\n";
    constexpr std::string_view source_sha="64039709c31f9e5a7f0fa860a10fce6f294188b06afff420d60fe622cd09aded";
    const GroupEndpoint15Model model{
        Model{"TD",{{"A","A"},{"B","B"}},{},{{"G","G","",-1,{0}}}},
        {{{GroupEndpointKind::Group,"G"},{GroupEndpointKind::Vertex,"B"},"","solid",false,true,0u}},
        {{GroupEndpointOrderKind::Tagged,0u}}};
    OracleWork work{kFlow400SceneLimit};
    work.spend(65536u+8u*sizeof(Grid)+8u*sizeof(GroupEndpoint15Model)+8u*sizeof(json::JsonMutVal)+4096u);
    work.product(source.size(),32u);ASSERT_EQ(digest(source),source_sha);
    auto mermaid=loom::ui::render_markdown(group_endpoint15_fenced(source,"mermaid"));
    auto ordinary=loom::ui::render_markdown(group_endpoint15_fenced(source,"text"));
    auto element=ftxui::vbox({mermaid,ftxui::text(std::string(kSentinel))});
    auto plain=ftxui::vbox({ordinary,ftxui::text(std::string(kSentinel))});
    element->ComputeRequirement();plain->ComputeRequirement();
    const int height=std::max({32,element->requirement().min_y+8,plain->requirement().min_y+8});
    ASSERT_LE(height,1040);
    json::JsonMutDoc publication;auto row=publication.object();std::array<std::optional<Grid>,5> frames;
    constexpr std::array<int,5> widths{{240,40,240,240,40}};
    constexpr std::array<std::string_view,5> names{{"wide","narrow","restored","ordinary_wide","ordinary_narrow"}};
    for(std::size_t index=0u;index<frames.size();++index) {
        frames[index]=capture(index<3u?element:plain,widths[index],height,&work);
        group_endpoint15_publish_frame(row,names[index],*frames[index],work);
    }
    ASSERT_EQ(row.size(),5u);
    for(const auto& frame:frames) static_cast<void>(flow400_following(*frame,work,kSentinel));
    const bool source_wide=flow400_same(*frames[0],*frames[3],work);
    const bool source_narrow=flow400_same(*frames[1],*frames[4],work);
    ASSERT_FALSE(source_wide);
    ASSERT_TRUE(flow400_same(*frames[0],*frames[2],work));
    const auto wide=group_endpoint15_geometry(*frames[0],model,work,kSentinel);
    ASSERT_TRUE(wide.empty())<<wide.front();
    if(!source_narrow) {
        const auto narrow=group_endpoint15_geometry(*frames[1],model,work,kSentinel);
        ASSERT_TRUE(narrow.empty())<<narrow.front();
    }
    const auto restored=group_endpoint15_geometry(*frames[2],model,work,kSentinel);
    ASSERT_TRUE(restored.empty())<<restored.front();

    group_endpoint15_copy_model(model,work);auto child_source=model;
    child_source.tagged.front().source={GroupEndpointKind::Vertex,"A"};
    group_endpoint15_require_rejected(*frames[0],child_source,work,"GroupEndpoint15 public group-to-child substitution accepted",kSentinel);
    group_endpoint15_copy_grid(*frames[0],work);auto missing_marker=*frames[0];bool removed=false;
    for(int y=0;y<missing_marker.height&&!removed;++y) for(int x=0;x<missing_marker.width&&!removed;++x) {
        if(glyph(missing_marker.at({x,y})).arrow>=0) {missing_marker.put({x,y}," ");removed=true;}
    }
    ASSERT_TRUE(removed);
    group_endpoint15_require_rejected(missing_marker,model,work,"GroupEndpoint15 public missing endpoint marker accepted",kSentinel);
    group_endpoint15_copy_grid(*frames[0],work);auto extra_ink=*frames[0];const auto footer=flow400_following(*frames[0],work,kSentinel);bool placed=false;
    for(int y=0;y<footer&&!placed;++y) for(int x=0;x<extra_ink.width&&!placed;++x) {
        if(extra_ink.at({x,y})==" ") {extra_ink.put({x,y},"§");placed=true;}
    }
    ASSERT_TRUE(placed);
    group_endpoint15_require_rejected(extra_ink,model,work,"GroupEndpoint15 public extra ink accepted",kSentinel);
    EXPECT_NE(work.remaining,0u);
}

TEST(MermaidFlowGroupEndpoint15, HandControlsCoverTagKindsMembershipAndRouteArmOwnership) {
    auto documents=group_endpoint15_documents();auto hands=group_endpoint15_hand_documents();const auto records=documents.asset.root().get("records");const auto hand_root=hands.asset.root();const auto scenes=hand_root.get("good_scenes"),controls=hand_root.get("negative_controls");ASSERT_EQ(scenes.size(),8u);ASSERT_EQ(controls.size(),kGroupEndpoint15HandControlSpecs.size());const auto expected=flow400_number(hand_root.get("expected_nonresource_rejections_per_good_scene"));
    std::size_t good=0u,generic_rejected=0u,declared_rejected=0u;scenes.iter([&](JsonVal scene) {OracleWork work{kFlow400SceneLimit};auto hand=group_endpoint15_hand_model(hand_root,records,scene,work);auto grid=group_endpoint15_hand_grid(scene,work);const auto local=group_endpoint15_hand_controls(scene,hand.model,std::move(grid),work);EXPECT_EQ(local,expected);generic_rejected+=local;EXPECT_NE(work.remaining,0u);++good;});
    std::array<bool,kGroupEndpoint15HandControlSpecs.size()> seen{};controls.iter([&](JsonVal control) {const auto id=flow400_text(control.get("id"));const auto index=static_cast<std::size_t>(std::ranges::find_if(kGroupEndpoint15HandControlSpecs,[&](const auto& spec) {return spec.id==id;})-kGroupEndpoint15HandControlSpecs.begin());ASSERT_LT(index,seen.size());ASSERT_FALSE(seen[index]);seen[index]=true;OracleWork work{kFlow400SceneLimit};declared_rejected+=group_endpoint15_declared_hand_control(hand_root,records,control,work);EXPECT_NE(work.remaining,0u);});
    EXPECT_TRUE(std::ranges::all_of(seen,[](bool value) {return value;}));EXPECT_EQ(good,8u);EXPECT_EQ(generic_rejected,64u);EXPECT_EQ(declared_rejected,8u);
}

} // namespace
