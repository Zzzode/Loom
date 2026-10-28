// Consumer-local controls for the source-pinned Flow400 Extra7 cohort.
#pragma once
#include <fstream>
#include "mermaid_flow_group_endpoint15_oracle.hpp"
#include "mermaid_flow400_group_endpoint_extra7_authority.hpp"

namespace loom::test::mermaid_flow {
using json::JsonMutVal;
struct GroupEndpointExtra7ControlRun {
    std::string controls_json;
    std::string route_witness_sha256;
    std::string mutation_preimages_sha256;
    std::size_t semantic_rejections = 0u;
    std::size_t physical_rejections = 0u;
    std::size_t decoder_successes = 0u;
};
struct GroupEndpointExtra7RouteBinding {
    std::size_t ordinal=0u,tangential_source_order=0u,target_head_source_order=0u;
    std::size_t overlap_source_order=0u,overlap_target_order=0u;
};
inline constexpr std::array<GroupEndpointExtra7RouteBinding,7> kGroupEndpointExtra7RouteBindings{{
    {15u,3u,2u,2u,3u},{26u,0u,0u,0u,1u},{28u,4u,4u,4u,5u},
    {269u,3u,2u,2u,3u},{303u,1u,1u,1u,0u},{306u,0u,0u,0u,1u},{308u,4u,4u,4u,5u}
}};

inline std::string group_endpoint_extra7_frame_sha256(const Grid& grid,OracleWork* work=nullptr) {
    if(work) {
        std::size_t bytes=0u;for(const auto& cell:grid.cells)bytes+=cell.size()+1u;
        work->product(grid.cells.size(),sizeof(std::string)+16u);work->product(bytes,2u);work->spend(128u);
    }
    std::string bytes=std::to_string(grid.width)+"x"+std::to_string(grid.height)+"\n";
    for(int y=0;y<grid.height;++y) {bytes+=grid.row(y);bytes+='\n';}
    return digest(bytes);
}
inline std::string group_endpoint_extra7_consumer_source_set_sha256() {
    constexpr std::array<std::string_view,22> inputs{{
        "tests/ui/CMakeLists.txt","tests/ui/test_mermaid_flow_semantics.cpp",
        "tests/ui/test_mermaid_flow_group_endpoints.cpp","tests/ui/test_mermaid_official.cpp",
        "tests/ui/test_mermaid_flow_ascii.cpp",
        "tests/support/mermaid_flow_oracle.hpp","tests/support/mermaid_flow_group_endpoint15_oracle.hpp",
        "tests/support/mermaid_flow400_group_endpoint15_authority.hpp",
        "tests/support/mermaid_flow400_group_endpoint_extra7_authority.hpp","tests/support/mermaid_flow_extra7_controls.hpp",
        "tests/fixtures/mermaid_native/flow400_group_endpoint_extra7_source_models.v1.json",
        "tests/fixtures/mermaid_native/flow400_group_endpoint_extra7_controls.v1.json",
        "tests/fixtures/mermaid_native/flow400_group_endpoint_extra7_hand_mutations.v1.json",
        "tests/fixtures/mermaid_native/flow400_group_endpoint_extra7_body_facts.v1.json",
        "tests/fixtures/mermaid_official/manifest.json",
        "tests/fixtures/mermaid_official/cases/flowchart/nightly-check/dagre/2050-dagre-handling-of-different-rendering-direction-in-subgraphs.mmd",
        "tests/fixtures/mermaid_official/cases/flowchart/nightly-check/dagre/55-dagre-handle-nested-subgraphs-with-outgoing-links-2.mmd",
        "tests/fixtures/mermaid_official/cases/flowchart/nightly-check/dagre/57-dagre-handle-nested-subgraphs-with-outgoing-links-2.mmd",
        "tests/fixtures/mermaid_official/cases/flowchart/visual-check/2050-handling-of-different-rendering-direction-in-subgraphs.mmd",
        "tests/fixtures/mermaid_official/cases/flowchart/visual-check/5064-should-render-when-subgraph-child-has-links-to-outside-node-and-subgraph.mmd",
        "tests/fixtures/mermaid_official/cases/flowchart/visual-check/55-handle-nested-subgraphs-with-outgoing-links-2.mmd",
        "tests/fixtures/mermaid_official/cases/flowchart/visual-check/57-handle-nested-subgraphs-with-outgoing-links-2.mmd"
    }};
    const auto root=std::filesystem::path(LOOM_TESTS_DIR).parent_path();std::string bytes;
    for(const auto input:inputs) {
        const auto content=flow400_read(root/std::filesystem::path(input));
        bytes.append(input);bytes.push_back('\0');bytes+=digest(content);bytes.push_back('\n');
    }
    return digest(bytes);
}

inline std::string group_endpoint_extra7_ref_kind(GroupEndpointRef ref) {
    return ref.kind==GroupEndpointKind::Group?"group":"vertex";
}

inline std::string group_endpoint_extra7_witness_sha256(const GroupEndpointGeometryWitness& witness,OracleWork& work) {
    work.spend(1024u+4u*sizeof(json::JsonMutDoc));
    json::JsonMutDoc document;auto root=document.object(),routes=root.make_arr(),frames=root.make_arr(),labels=root.make_arr();
    root.set("following_row",witness.following_row);
    for(const auto& route:witness.routes) {
        work.product(route.route_cells.size(),sizeof(Point)+16u);
        auto row=root.make_obj();row.set("source_order",static_cast<int64_t>(route.source_order));
        row.set("source_kind",group_endpoint_extra7_ref_kind(route.source));row.set("source_id",route.source_id);
        row.set("target_kind",group_endpoint_extra7_ref_kind(route.target));row.set("target_id",route.target_id);
        auto point=[&](json::JsonMutVal owner,std::string_view key,Point value) {auto pair=owner.make_arr();pair.append(owner.make_num(value.x));pair.append(owner.make_num(value.y));owner.add(key,pair);};
        point(row,"source_port",route.source_port);point(row,"target_port",route.target_port);point(row,"arrow_head",route.arrow_head);
        row.set("source_outward",route.source_outward);row.set("target_outward",route.target_outward);
        auto cells=row.make_arr();for(const auto cell:route.route_cells) {auto pair=cells.make_arr();pair.append(cells.make_num(cell.x));pair.append(cells.make_num(cell.y));cells.append(pair);}
        row.add("route_cells",cells);routes.append(row);
    }
    for(const auto& [id,frame]:witness.frames) {auto row=root.make_obj();row.set("id",id);row.set("left",frame.left);row.set("top",frame.top);row.set("right",frame.right);row.set("bottom",frame.bottom);frames.append(row);}
    for(const auto& label:witness.labels) {auto row=root.make_obj();row.set("left",label.left);row.set("top",label.top);row.set("right",label.right);row.set("bottom",label.bottom);labels.append(row);}
    root.add("routes",routes);root.add("frames",frames);root.add("labels",labels);document.set_root(root);
    return digest(document.to_string());
}
inline void group_endpoint_extra7_append_case(json::JsonMutDoc& report,json::JsonMutVal cases,JsonVal source,
    const std::array<std::string,5>& frame_hashes,std::string_view narrow_disposition,
    const GroupEndpointExtra7ControlRun& control_run) {
    auto row=cases.make_obj(),tuple=row.make_obj();
    tuple.set("ordinal",static_cast<int64_t>(flow400_number(source.get("ordinal"))));
    tuple.set("manifest_global_ordinal",static_cast<int64_t>(flow400_number(source.get("manifest_global_ordinal"))));
    tuple.set("fixture",flow400_text(source.get("fixture")));
    tuple.set("source_bytes",static_cast<int64_t>(flow400_number(source.get("source_bytes"))));
    tuple.set("source_sha256",flow400_text(source.get("source_sha256")));
    tuple.set("source_git_blob",flow400_text(source.get("source_git_blob")));
    tuple.set("body_offset_bytes",static_cast<int64_t>(flow400_number(source.get("body_offset_bytes"))));
    tuple.set("body_bytes",static_cast<int64_t>(flow400_number(source.get("body_bytes"))));
    tuple.set("body_sha256",flow400_text(source.get("body_sha256")));
    tuple.set("canonical_model_sha256",flow400_text(source.get("canonical_model_sha256")));
    tuple.set("canonical_wire_model_sha256",flow400_text(source.get("canonical_wire_model_sha256")));
    row.add("tuple",tuple);
    constexpr std::array<std::string_view,5> names{{"wide_frame","narrow_frame","restored_frame","ordinary_wide_frame","ordinary_narrow_frame"}};
    auto captures=row.make_arr();for(std::size_t index=0u;index<names.size();++index) {
        auto capture=captures.make_obj();capture.set("name",names[index]);capture.set("frame_sha256",frame_hashes[index]);capture.set("published",true);captures.append(capture);
    }
    row.add("captures",captures);row.set("captures_before_binding",true);row.set("bound_after_captures",true);row.set("decoded",true);
    row.set("canonical_model_sha256",flow400_text(source.get("canonical_model_sha256")));
    row.set("canonical_wire_model_sha256",flow400_text(source.get("canonical_wire_model_sha256")));
    row.set("following_token_exact",true);row.set("wide_geometry_good",true);row.set("restored_geometry_good",true);row.set("narrow_disposition",narrow_disposition);
    const auto parsed=json::parse(control_run.controls_json);flow400_require(parsed.has_value(),"Extra7 compact control ledger JSON");
    const auto control_records=parsed->root().get("records");auto controls=row.make_arr();
    control_records.iter([&](JsonVal control) {controls.append(report.copy_val(control));});row.add("controls",controls);
    row.set("route_witness_sha256",control_run.route_witness_sha256);row.set("mutation_preimages_sha256",control_run.mutation_preimages_sha256);
    row.set("resource_failures",0);row.set("fallback_credit",0);cases.append(row);
}
inline void group_endpoint_extra7_write_atomic(const std::filesystem::path& path,std::string_view contents) {
    auto temporary=path;temporary+=".tmp";
    {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output||!output.write(contents.data(),static_cast<std::streamsize>(contents.size())))throw std::runtime_error("Extra7 verdict temporary write");output.flush();if(!output)throw std::runtime_error("Extra7 verdict temporary flush");}
    std::error_code error;std::filesystem::rename(temporary,path,error);
    if(error) {std::filesystem::remove(temporary);throw std::runtime_error("Extra7 verdict atomic rename: "+error.message());}
}
inline void group_endpoint_extra7_finish_verdict(json::JsonMutDoc& document,json::JsonMutVal cases,
    std::string_view schema,std::string_view consumer_id,std::string_view producer_target,
    std::string_view binary_name,std::size_t ready,std::size_t captures,std::size_t published,
    std::size_t semantic_rejections,std::size_t physical_rejections,std::size_t resource_failures,
    std::size_t fallback_credit) {
    const std::filesystem::path report_dir=LOOM_MERMAID_REPORT_DIR;
    const auto preset=report_dir.parent_path().parent_path().filename().string();
    flow400_require(preset=="debug"||preset=="release","Extra7 report preset identity");
    const auto binary=flow400_read(report_dir/std::string(binary_name));
    const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto root=document.object();root.set("schema",schema);root.set("consumer_id",consumer_id);root.set("preset",preset);
    root.set("source_facts_sha256","886e44af39c518d68db636e924362e1ab3d4d2240f2a40654835458927970efd");
    root.set("source_asset_sha256",kFlow400GroupEndpointExtra7AssetSha);root.set("controls_asset_sha256",kFlow400GroupEndpointExtra7ControlsSha);
    root.set("hand_mutations_asset_sha256",kFlow400GroupEndpointExtra7HandSha);root.set("body_facts_asset_sha256",kFlow400GroupEndpointExtra7BodySha);
    root.set("official_manifest_sha256",kFlow400ManifestSha);root.set("consumer_source_set_sha256",group_endpoint_extra7_consumer_source_set_sha256());
    root.set("producer_target",producer_target);root.set("producer_binary_sha256",digest(binary));root.set("finished_at_unix_ns",static_cast<int64_t>(now));
    root.set("local_ready",static_cast<int64_t>(ready));root.set("case_count",static_cast<int64_t>(cases.size()));
    root.set("captures",static_cast<int64_t>(captures));root.set("published",static_cast<int64_t>(published));
    root.set("semantic_mutations_rejected",static_cast<int64_t>(semantic_rejections));root.set("physical_mutations_rejected",static_cast<int64_t>(physical_rejections));
    root.set("resource_failures",static_cast<int64_t>(resource_failures));root.set("fallback_credit",static_cast<int64_t>(fallback_credit));
    root.set("foreign_assertions_certified",0);root.set("native_report_input",false);root.add("cases",cases);document.set_root(root);
}

inline std::string group_endpoint_extra7_glyph_for_bits(int bits) {
    constexpr std::array<std::pair<int,std::string_view>,15> values{{
        {L|R,"─"},{U|D,"│"},{R|D,"┌"},{L|D,"┐"},{U|R,"└"},{U|L,"┘"},
        {U|R|D,"├"},{U|L|D,"┤"},{L|R|D,"┬"},{U|L|R,"┴"},{U|R|D|L,"┼"},
        {R|D,"╭"},{L|D,"╮"},{U|R,"╰"},{U|L,"╯"}}};
    for(const auto& [mask,glyph_text]:values)if(mask==bits)return std::string(glyph_text);
    throw std::runtime_error("Extra7 physical control has no exact replacement glyph");
}
inline int group_endpoint_extra7_clockwise(int outward) {
    if(outward==U)return R;if(outward==R)return D;if(outward==D)return L;if(outward==L)return U;
    throw std::runtime_error("Extra7 physical control has invalid outward vector");
}

inline void group_endpoint_extra7_mutate_wire(JsonMutVal model,const JsonVal control) {
    const auto mutation=control.get("mutation");const auto id=flow400_text(control.get("id"));
    if(id.starts_with("semantic_wrong_tagged_")) {
        const auto index=flow400_number(mutation.get("tagged_edge_index"));const auto endpoint=flow400_text(mutation.get("endpoint"));
        flow400_require(index<model.get("tagged_group_endpoint_edges").size()&&(endpoint=="source"||endpoint=="target"),"Extra7 tagged mutation selector");
        auto current=model.get("tagged_group_endpoint_edges").at(index).get(endpoint);const auto from=mutation.get("from"),to=mutation.get("to");
        flow400_require(current.get("kind").as_str()==flow400_text(from.get("kind"))&&current.get("id").as_str()==flow400_text(from.get("id")),"Extra7 tagged mutation preimage");
        flow400_require(current.remove("kind")&&current.remove("id"),"Extra7 mutable tagged ref");
        current.add("kind",current.make_str(flow400_text(to.get("kind"))));current.add("id",current.make_str(flow400_text(to.get("id"))));return;
    }
    if(id=="semantic_wrong_ordinary_target_identity") {
        const auto index=flow400_number(mutation.get("ordinary_edge_index"));auto edge=model.get("base_model").get("edges").at(index);auto target=edge.get("target");
        flow400_require(target.as_str()==flow400_text(mutation.get("from")),"Extra7 ordinary target mutation preimage");
        edge.add("target",edge.make_str(flow400_text(mutation.get("to"))));return;
    }
    if(id=="semantic_wrong_direction") {
        auto base=model.get("base_model");auto direction=base.get("direction");flow400_require(direction.as_str()==flow400_text(mutation.get("from")),"Extra7 direction mutation preimage");
        base.add("direction",base.make_str(flow400_text(mutation.get("to"))));return;
    }
    if(id=="semantic_wrong_group_label") {
        const auto group_id=flow400_text(mutation.get("group_id"));auto groups=model.get("base_model").get("subgraphs");std::size_t matches=0u;
        const auto visit=[&](auto&& self,JsonMutVal values)->void {
            values.iter([&](JsonMutVal group,std::size_t) {
                if(group.get("id").as_str()==group_id) {
                    auto label=group.get("label");flow400_require(label.as_str()==flow400_text(mutation.get("from")),"Extra7 label mutation preimage");
                    group.add("label",group.make_str(flow400_text(mutation.get("to"))));++matches;
                }
                const auto children=group.get("children");if(children.is_arr())self(self,children);
            });
        };
        visit(visit,groups);flow400_require(matches==1u,"Extra7 exact nested group label selector");return;
    }
    throw std::runtime_error("Extra7 unknown semantic control id");
}

inline bool group_endpoint_extra7_outside(Rect rect,Point point) {
    return point.x<rect.left||point.x>rect.right||point.y<rect.top||point.y>rect.bottom;
}
inline bool group_endpoint_extra7_internal_route_cell(const GroupEndpointRouteWitness& route,Point point,
    const GroupEndpointGeometryWitness& witness,const std::vector<const GroupEndpointRouteWitness*>& other_routes) {
    if(point==route.source_port||point==route.target_port||point==route.arrow_head)return false;
    for(const auto& [id,frame]:witness.frames) {static_cast<void>(id);if(frame.boundary(point))return false;}
    for(const auto& label:witness.labels) if(!group_endpoint_extra7_outside(label,point))return false;
    for(const auto* other:other_routes) if(std::ranges::find(other->route_cells,point)!=other->route_cells.end())return false;
    return true;
}

inline GroupEndpointExtra7ControlRun group_endpoint_extra7_run_controls(
    JsonVal source_record,JsonVal control_record,const Grid& good,const GroupEndpoint15Model& model,
    const GroupEndpointProfile& profile,const GroupEndpointGeometryWitness& witness,OracleWork& work,
    std::string_view consumer_id,std::string_view token=kSentinel) {
    static_cast<void>(consumer_id);
    const auto tuple=group_endpoint_extra7_tuple(source_record);
    const auto binding_it=std::ranges::find_if(kGroupEndpointExtra7RouteBindings,[&](const auto& binding){return binding.ordinal==tuple.ordinal;});
    flow400_require(binding_it!=kGroupEndpointExtra7RouteBindings.end(),"Extra7 exact physical route binding");
    const auto& route_binding=*binding_it;
    flow400_require(witness.routes.size()==model.source_order.size()&&witness.frames.size()==model.base.nodes.size()+model.base.groups.size(),"Extra7 witness/model cardinality");
    const auto controls=control_record.get("controls"),expected_keys=control_record.get("expected_control_keys");
    flow400_require(controls.size()==9u&&expected_keys.size()==9u,"Extra7 source controls shape");
    json::JsonMutDoc control_doc;auto control_root=control_doc.object(),control_rows=control_root.make_arr();
    std::string preimage_chain;std::size_t semantic=0u,physical=0u,decoded_count=0u;
    for(std::size_t index=0u;index<controls.size();++index) {
        const auto control=controls.at(index);const auto id=flow400_text(control.get("id"));const auto control_class=flow400_text(control.get("class"));
        const auto execution_key=flow400_text(control.get("execution_key"));
        const auto expected_key=std::string(flow400_text(source_record.get("case_id")))+"/"+std::to_string(flow400_number(source_record.get("ordinal")))+"/"+std::string(id);
        flow400_require(execution_key==expected_key&&flow400_text(expected_keys.at(index))==expected_key,"Extra7 control ID order/uniqueness");
        std::string preimage;bool decoded=false;bool rejected=false;
        if(control_class=="semantic_model") {
            flow400_require(flow400_text(control.get("expected"))=="decoded_then_grid_rejects","Extra7 semantic control expected outcome");
            group_endpoint15_copy_model(model,work);const auto wire=source_record.get("model");
            work.product(kFlow400GroupEndpointExtra7AssetBytes,2u);json::JsonMutDoc mutated;auto root=mutated.copy_val(wire);group_endpoint_extra7_mutate_wire(root,control);mutated.set_root(root);
            preimage=control.get("mutation").to_string();const auto parsed=json::parse(mutated.to_string());flow400_require(parsed.has_value(),"Extra7 semantic control JSON serialization");
            const auto changed=group_endpoint_extra7_model(parsed->root(),work);const auto changed_profile=group_endpoint_extra7_profile(changed,work);decoded=true;++decoded_count;
            const auto errors=flow_group_endpoint_geometry(good,changed.base,changed_profile,work,token);rejected=group_endpoint15_nonresource(errors);
            if(!rejected)throw std::runtime_error("Extra7 semantic negative accepted or resource failed: ordinal="+std::to_string(tuple.ordinal)+" control="+std::string(id)+" geometry="+(errors.empty()?"accepted":errors.front()));++semantic;
        } else if(control_class=="physical_grid") {
            flow400_require(flow400_text(control.get("expected"))=="prepaid_original_grid_copy_rejected","Extra7 physical control expected outcome");
            flow400_copy_grid(good,work);auto changed=good;const auto before_sha=group_endpoint_extra7_frame_sha256(good,&work);
            const auto route_at=[&](std::size_t order)->const GroupEndpointRouteWitness& {const auto found=std::ranges::find_if(witness.routes,[&](const auto& route){return route.source_order==order;});flow400_require(found!=witness.routes.end(),"Extra7 physical route witness index");return *found;};
            if(id=="physical_tangential_group_source") {
                const auto found=std::ranges::find_if(witness.routes,[&](const auto& route){return route.source_order==route_binding.tangential_source_order;});
                flow400_require(found!=witness.routes.end()&&found->source.kind==GroupEndpointKind::Group,"Extra7 group source route binding");const auto point=found->source_port;const auto original=std::string(good.at(point));const auto parsed=glyph(original);
                flow400_require((parsed.bits&found->source_outward)!=0,"Extra7 group source outward preimage");const int replacement_bits=(parsed.bits&~found->source_outward)|group_endpoint_extra7_clockwise(found->source_outward);
                changed.put(point,group_endpoint_extra7_glyph_for_bits(replacement_bits));preimage=before_sha+"|"+std::to_string(found->source_order)+"|"+std::to_string(point.x)+","+std::to_string(point.y)+"|"+original+"|"+std::to_string(found->source_outward)+"|"+std::string(changed.at(point));
            } else if(id=="physical_target_head_port_collision") {
                const auto found=std::ranges::find_if(witness.routes,[&](const auto& route){return route.source_order==route_binding.target_head_source_order;});
                flow400_require(found!=witness.routes.end()&&found->arrow_head!=found->target_port,"Extra7 target port/head route");const auto head=std::string(good.at(found->arrow_head)),port=std::string(good.at(found->target_port));
                changed.put(found->arrow_head," ");changed.put(found->target_port,head);
                preimage=before_sha+"|"+std::to_string(found->source_order)+"|"+std::to_string(found->arrow_head.x)+","+std::to_string(found->arrow_head.y)+"|"+head+"|"+std::to_string(found->target_port.x)+","+std::to_string(found->target_port.y)+"|"+port;
            } else if(id=="physical_route_overlap") {
                const GroupEndpointRouteWitness* from=nullptr;const GroupEndpointRouteWitness* to=nullptr;Point from_point{},to_point{};
                const auto& candidate_from=route_at(route_binding.overlap_source_order);const auto& candidate_to=route_at(route_binding.overlap_target_order);
                {
                    std::vector<const GroupEndpointRouteWitness*> none;
                    for(const auto point:candidate_from.route_cells)if(group_endpoint_extra7_internal_route_cell(candidate_from,point,witness,none)) {
                        const std::vector<const GroupEndpointRouteWitness*> occupied{&candidate_from};
                        for(const auto other_point:candidate_to.route_cells)if(group_endpoint_extra7_internal_route_cell(candidate_to,other_point,witness,occupied)) {from=&candidate_from;to=&candidate_to;from_point=point;to_point=other_point;break;}
                        if(from)break;
                    }
                }
                flow400_require(from&&to&&from_point!=to_point,"Extra7 exclusive route overlap preimage selector");
                const auto from_glyph=std::string(good.at(from_point)),to_glyph=std::string(good.at(to_point));
                changed.put(from_point," ");changed.put(to_point,from_glyph);
                preimage=before_sha+"|"+std::to_string(from->source_order)+"|"+std::to_string(from_point.x)+","+std::to_string(from_point.y)+"|"+from_glyph+"|"+std::to_string(to->source_order)+"|"+std::to_string(to_point.x)+","+std::to_string(to_point.y)+"|"+to_glyph;
            } else if(id=="physical_unclaimed_ink") {
                const auto footer=witness.following_row;flow400_require(footer>=0,"Extra7 following-row witness");bool placed=false;Point point{};
                for(int y=0;y<footer&&!placed;++y)for(int x=0;x<good.width&&!placed;++x) {
                    point={x,y};if(good.at(point)!=" ")continue;
                    if(std::ranges::any_of(witness.frames,[&](const auto& frame){return !group_endpoint_extra7_outside(frame.second,point);}))continue;
                    if(std::ranges::any_of(witness.labels,[&](const Rect& rect){return !group_endpoint_extra7_outside(rect,point);}))continue;
                    if(std::ranges::any_of(witness.routes,[&](const auto& route){return std::ranges::find(route.route_cells,point)!=route.route_cells.end();}))continue;
                    placed=true;
                }
                flow400_require(placed,"Extra7 unclaimed-ink owner-map blank cell");changed.put(point,"┼");
                preimage=before_sha+"|"+std::to_string(point.x)+","+std::to_string(point.y)+"| |┼|"+std::to_string(footer);
            } else throw std::runtime_error("Extra7 unknown physical control id");
            const auto errors=flow_group_endpoint_geometry(changed,model.base,profile,work,token);rejected=group_endpoint15_nonresource(errors);
            flow400_require(rejected,"Extra7 physical negative accepted or resource failed");++physical;
        } else throw std::runtime_error("Extra7 control class unknown");
        const auto preimage_sha=digest(preimage);preimage_chain+=id;preimage_chain+='\0';preimage_chain+=preimage_sha;preimage_chain+='\n';
        auto item=control_root.make_obj();item.set("id",id);item.set("class",control_class);item.set("execution_key",execution_key);
        item.set("decoded_before_geometry",decoded);item.set("rejected",rejected);item.set("resource_failure",false);item.set("preimage_sha256",preimage_sha);control_rows.append(item);
    }
    flow400_require(semantic==5u&&physical==4u&&decoded_count==5u,"Extra7 exact 5 semantic/4 physical control execution");
    control_root.add("records",control_rows);control_doc.set_root(control_root);
    GroupEndpointExtra7ControlRun result;result.controls_json=control_doc.to_string();result.semantic_rejections=semantic;
    result.physical_rejections=physical;result.decoder_successes=decoded_count;
    result.route_witness_sha256=group_endpoint_extra7_witness_sha256(witness,work);result.mutation_preimages_sha256=digest(preimage_chain);return result;
}
} // namespace loom::test::mermaid_flow
