// Test-private source authority for the reviewed Flow400 Extra7 cohort.
#pragma once
#include "mermaid_flow400_group_endpoint15_authority.hpp"

namespace loom::test::mermaid_flow {
inline constexpr std::size_t kFlow400GroupEndpointExtra7AssetBytes = 73763u;
inline constexpr std::string_view kFlow400GroupEndpointExtra7AssetSha =
    "6ef9d782ad92b35e4a8d8c4d45580747a8637ff38823309d5caf195d1ef510d2";
inline constexpr std::size_t kFlow400GroupEndpointExtra7BodyBytes = 9549u;
inline constexpr std::string_view kFlow400GroupEndpointExtra7BodySha =
    "99bfd3f1385e3e96df6a88f8f7942bc389731b1c2403f8d43f0232ad47b7a46b";
inline constexpr std::size_t kFlow400GroupEndpointExtra7ControlsBytes = 67746u;
inline constexpr std::string_view kFlow400GroupEndpointExtra7ControlsSha =
    "2f632fcb8c3d9ec3c6c6f0ae6f1382e0edc9bdd12f34e48831b50cfb9c9ad150";
inline constexpr std::size_t kFlow400GroupEndpointExtra7HandBytes = 26828u;
inline constexpr std::string_view kFlow400GroupEndpointExtra7HandSha =
    "5a96b731bad8b53bd9e63fdc6700d41c9ccbfe23e018213e33820de43508c5b9";
inline constexpr std::size_t kFlow400GroupEndpointExtra7HandMutationsBytes = 26828u;
inline constexpr std::string_view kFlow400GroupEndpointExtra7HandMutationsSha =
    "5a96b731bad8b53bd9e63fdc6700d41c9ccbfe23e018213e33820de43508c5b9";

struct GroupEndpointExtra7Tuple {
    std::string_view case_id,fixture,source_sha256;
    std::size_t ordinal=0u,manifest_global_ordinal=0u;
};
inline constexpr std::array<GroupEndpointExtra7Tuple,7> kFlow400GroupEndpointExtra7Tuples{{
    {"flow400_group_endpoint_extra_15","flowchart/nightly-check/dagre/2050-dagre-handling-of-different-rendering-direction-in-subgraphs.mmd","72366985bda8c7b42cf9018299c9c2946d22ba27aacff41d57aa6a7e615e8fa2",15u,676u},
    {"flow400_group_endpoint_extra_26","flowchart/nightly-check/dagre/55-dagre-handle-nested-subgraphs-with-outgoing-links-2.mmd","84e286bea79da3522f89f78d3acb252c34d89c963afb45e97b4aeff9ec0efb57",26u,687u},
    {"flow400_group_endpoint_extra_28","flowchart/nightly-check/dagre/57-dagre-handle-nested-subgraphs-with-outgoing-links-2.mmd","0da0f803ec8e2ccf162bd3ac7013a3d87778763f86788deb336faabbe258187c",28u,689u},
    {"flow400_group_endpoint_extra_269","flowchart/visual-check/2050-handling-of-different-rendering-direction-in-subgraphs.mmd","2c3b7a57f3edaede358e102d9b8b9e5d84e981b914739ddd1d7184a38f7ec593",269u,930u},
    {"flow400_group_endpoint_extra_303","flowchart/visual-check/5064-should-render-when-subgraph-child-has-links-to-outside-node-and-subgraph.mmd","a0ab1c8f8393cb9a771cd6f40f35ec5d0b9ace5c25a6272c3b42a335da38579b",303u,964u},
    {"flow400_group_endpoint_extra_306","flowchart/visual-check/55-handle-nested-subgraphs-with-outgoing-links-2.mmd","dd7af0ae31d1bc5ed20388d3c5f32783a39e0f0b1fb7175f4676334be607174c",306u,967u},
    {"flow400_group_endpoint_extra_308","flowchart/visual-check/57-handle-nested-subgraphs-with-outgoing-links-2.mmd","4aff65938cfea8a17a085cb20ae697b5a3e613c5505c622e6cecfed2ad026483",308u,969u},
}};

struct GroupEndpointExtra7Documents { OracleWork asset_work; json::JsonDoc asset; };
struct GroupEndpointExtra7ControlDocuments { OracleWork asset_work; json::JsonDoc controls; json::JsonDoc hand; json::JsonDoc body; };

inline json::JsonDoc group_endpoint_extra7_asset(std::string_view path,std::size_t expected_bytes,
                                                 std::string_view expected_sha,std::string_view schema) {
    const auto file=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_native"/std::string(path);
    const auto size=std::filesystem::file_size(file);
    flow400_require(size==expected_bytes,"GroupEndpoint Extra7 immutable asset bytes");
    const auto bytes=flow400_read(file);
    flow400_require(digest(bytes)==expected_sha,"GroupEndpoint Extra7 immutable asset SHA");
    auto parsed=json::parse(bytes);flow400_require(parsed.has_value(),"GroupEndpoint Extra7 immutable asset JSON");
    flow400_require(flow400_text(parsed->root().get("schema"))==schema,"GroupEndpoint Extra7 immutable asset schema");
    return std::move(*parsed);
}

inline GroupEndpointExtra7Documents group_endpoint_extra7_documents() {
    GroupEndpointExtra7Documents result;
    result.asset_work.spend(16384u+4u*sizeof(json::JsonDoc));
    result.asset=group_endpoint_extra7_asset("flow400_group_endpoint_extra7_source_models.v1.json",
        kFlow400GroupEndpointExtra7AssetBytes,kFlow400GroupEndpointExtra7AssetSha,
        "flow400_group_endpoint_extra7_source_models/v1");
    const auto root=result.asset.root();
    group_endpoint15_keys(root,{"cohort","formal_credit_before_native_and_official_owned_execution","qualification","records","schema","source_facts_sha256","wire_contract","source_control_contract"},"Extra7 source root");
    flow400_require(flow400_number(root.get("formal_credit_before_native_and_official_owned_execution"))==0u&&
                    root.get("records").is_arr()&&root.get("records").size()==kFlow400GroupEndpointExtra7Tuples.size(),
                    "GroupEndpoint Extra7 source root bounds");
    return result;
}

inline GroupEndpointExtra7ControlDocuments group_endpoint_extra7_control_documents() {
    GroupEndpointExtra7ControlDocuments result;
    result.asset_work.spend(16384u+8u*sizeof(json::JsonDoc));
    result.controls=group_endpoint_extra7_asset("flow400_group_endpoint_extra7_controls.v1.json",
        kFlow400GroupEndpointExtra7ControlsBytes,kFlow400GroupEndpointExtra7ControlsSha,
        "flow400_group_endpoint_extra7_controls/v1");
    result.hand=group_endpoint_extra7_asset("flow400_group_endpoint_extra7_hand_mutations.v1.json",
        kFlow400GroupEndpointExtra7HandBytes,kFlow400GroupEndpointExtra7HandSha,
        "flow400_group_endpoint_extra7_hand_mutations/v1");
    result.body=group_endpoint_extra7_asset("flow400_group_endpoint_extra7_body_facts.v1.json",
        kFlow400GroupEndpointExtra7BodyBytes,kFlow400GroupEndpointExtra7BodySha,
        "flow400_group_endpoint_extra7_body_facts/v1");
    const auto controls=result.controls.root();
    group_endpoint15_keys(controls,{"global_decoder_controls","hand_controls","qualification","schema","source_control_execution_gate","source_control_totals","source_rows"},"Extra7 controls root");
    flow400_require(controls.get("source_rows").is_arr()&&controls.get("source_rows").size()==kFlow400GroupEndpointExtra7Tuples.size(),"Extra7 controls source rows");
    const auto hand=result.hand.root();
    group_endpoint15_keys(hand,{"execution_contract","limits","qualification","scenes","schema"},"Extra7 hand root");
    flow400_require(hand.get("scenes").is_arr()&&hand.get("scenes").size()==4u,"Extra7 hand scenes");
    const auto body=result.body.root();
    group_endpoint15_keys(body,{"coverage_contract","formal_credit_before_owned_execution","official_track","qualification","records","schema","source_facts_sha256"},"Extra7 body facts root");
    flow400_require(body.get("records").is_arr()&&body.get("records").size()==kFlow400GroupEndpointExtra7Tuples.size(),"Extra7 body facts count");
    return result;
}

inline GroupEndpointExtra7Tuple group_endpoint_extra7_tuple(JsonVal row) {
    const auto case_id=flow400_text(row.get("case_id"));
    const auto ordinal=flow400_number(row.get("ordinal"));
    const auto global=flow400_number(row.get("manifest_global_ordinal"));
    const auto found=std::ranges::find_if(kFlow400GroupEndpointExtra7Tuples,[&](const auto& item) {
        return item.case_id==case_id&&item.ordinal==ordinal&&item.manifest_global_ordinal==global;
    });
    flow400_require(found!=kFlow400GroupEndpointExtra7Tuples.end()&&flow400_text(row.get("fixture"))==found->fixture&&
                    flow400_text(row.get("source_sha256"))==found->source_sha256,
                    "GroupEndpoint Extra7 exact source tuple");
    return *found;
}

inline JsonVal group_endpoint_extra7_control_row(JsonVal root,const GroupEndpointExtra7Tuple& tuple) {
    JsonVal selected;std::size_t matches=0u;
    root.get("source_rows").iter([&](JsonVal row) {
        if(flow400_number(row.get("ordinal"))==tuple.ordinal&&
           flow400_number(row.get("manifest_global_ordinal"))==tuple.manifest_global_ordinal&&
           flow400_text(row.get("case_id"))==tuple.case_id&&flow400_text(row.get("fixture"))==tuple.fixture) {
            selected=row;++matches;
        }
    });
    flow400_require(matches==1u,"Extra7 exact source control row");
    group_endpoint15_keys(selected,{"case_id","controls","expected_control_keys","expected_decoder_success_before_geometry","expected_nonresource_rejections","fixture","manifest_global_ordinal","ordinal"},"Extra7 control row");
    flow400_require(flow400_number(selected.get("expected_nonresource_rejections"))==9u&&
        flow400_number(selected.get("expected_decoder_success_before_geometry"))==5u&&
        selected.get("controls").is_arr()&&selected.get("controls").size()==9u&&
        selected.get("expected_control_keys").is_arr()&&selected.get("expected_control_keys").size()==9u,
        "Extra7 exact per-source control count");
    return selected;
}

inline void group_endpoint_extra7_position(JsonVal value,std::string_view raw,std::string_view token,
                                            std::string_view scope,OracleWork& work) {
    group_endpoint15_keys(value,{"byte_offset","byte_length","line","column_bytes"},scope);
    const auto offset=flow400_number(value.get("byte_offset"));
    const auto length=flow400_number(value.get("byte_length"));
    flow400_require(length==token.size()&&offset<=raw.size()&&length<=raw.size()-offset&&raw.substr(offset,length)==token,
                    "GroupEndpoint Extra7 source token position");
    std::size_t line=1u,column=1u;
    for(std::size_t at=0u;at<offset;++at) {work.spend();if(raw[at]=='\n'){++line;column=1u;}else ++column;}
    flow400_require(flow400_number(value.get("line"))==line&&flow400_number(value.get("column_bytes"))==column,
                    "GroupEndpoint Extra7 source line and byte column");
}

inline void group_endpoint_extra7_record(JsonVal row,OracleWork& work) {
    group_endpoint15_keys(row,{"base_model_is_immutable_null","body_bytes","body_offset_bytes","body_sha256","body_utf8",
        "canonical_model_sha256","case_id","fixture","manifest_global_ordinal","model","negative_control_ids",
        "negative_control_keys","ordinal","raw_source_utf8","source_bytes","source_git_blob","source_position_facts",
        "source_semantic_counts","source_sha256","target_incoming_counts","expected_nonresource_rejections",
        "expected_decoder_success_before_geometry","selection_constraints","canonical_wire_model_sha256"},"Extra7 source record");
    const auto tuple=group_endpoint_extra7_tuple(row);const auto raw=flow400_text(row.get("raw_source_utf8"));
    flow400_require(row.get("base_model_is_immutable_null").as_bool()&&raw.size()==flow400_number(row.get("source_bytes"))&&
                    digest(raw)==tuple.source_sha256&&digest(flow400_text(row.get("body_utf8")))==flow400_text(row.get("body_sha256")),
                    "GroupEndpoint Extra7 source and body identity");
    const auto body_offset=flow400_number(row.get("body_offset_bytes"));
    const auto body=flow400_text(row.get("body_utf8"));
    flow400_require(body_offset<=raw.size()&&body.size()<=raw.size()-body_offset&&raw.substr(body_offset,body.size())==body&&
                    body.size()==flow400_number(row.get("body_bytes")),"GroupEndpoint Extra7 exact body offset");

    const auto model_value=row.get("model");const auto model=group_endpoint_extra7_model(model_value,work);
    const auto canonical_wire=group_endpoint15_canonical(model_value,work);
    flow400_require(digest(canonical_wire)==flow400_text(row.get("canonical_wire_model_sha256")),
                    "GroupEndpoint Extra7 canonical wire Model SHA");
    json::JsonMutDoc semantic_document;auto semantic=semantic_document.copy_val(model_value);
    flow400_require(semantic.remove("source_positions"),"GroupEndpoint Extra7 semantic Model source positions");
    semantic_document.set_root(semantic);
    const auto semantic_text=semantic_document.to_string();const auto semantic_parsed=json::parse(semantic_text);
    flow400_require(semantic_parsed.has_value()&&digest(group_endpoint15_canonical(semantic_parsed->root(),work))==flow400_text(row.get("canonical_model_sha256")),
                    "GroupEndpoint Extra7 canonical semantic Model SHA");
    flow400_require(model.tagged.size()<=3u&&flow400_number(row.get("expected_nonresource_rejections"))==9u&&
                    flow400_number(row.get("expected_decoder_success_before_geometry"))==5u,
                    "GroupEndpoint Extra7 source control bounds");

    const auto positions=model_value.get("source_positions");
    flow400_require(group_endpoint15_canonical(row.get("source_position_facts"),work)==
                    group_endpoint15_canonical(positions,work),"GroupEndpoint Extra7 outer/model source positions differ");
    const auto declarations=positions.get("group_declarations");
    declarations.iter([&](JsonVal declaration) {
        const auto id=flow400_text(declaration.get("id"));const auto position=declaration.get("position");
        group_endpoint_extra7_position(position,raw,id,"Extra7 group declaration position",work);
        const auto offset=flow400_number(position.get("byte_offset"));
        const auto line_begin=raw.rfind('\n',offset==0u?0u:offset-1u);
        const auto prefix=line_begin==std::string_view::npos?raw.substr(0u,offset):raw.substr(line_begin+1u,offset-line_begin-1u);
        flow400_require(prefix.find("subgraph ")!=std::string_view::npos,"GroupEndpoint Extra7 group declaration keyword");
    });
    const auto facts=positions.get("ordered_wire_facts");
    std::size_t fact_index=0u;
    facts.iter([&](JsonVal fact) {
        const auto at=fact_index++;
        const auto entry=model.source_order.at(at);
        const auto statement=fact.get("statement");
        const auto offset=flow400_number(statement.get("byte_offset")),length=flow400_number(statement.get("byte_length"));
        flow400_require(offset<=raw.size()&&length<=raw.size()-offset,"GroupEndpoint Extra7 statement span bounds");
        const auto statement_text=raw.substr(offset,length);
        flow400_require(statement_text.find("-->")!=std::string_view::npos,"GroupEndpoint Extra7 source edge operator span");
        for(const auto endpoint_name:{std::string_view("source"),std::string_view("target")}) {
            const auto endpoint=fact.get(endpoint_name);const auto id=flow400_text(endpoint.get("id"));const auto position=endpoint.get("position");
            group_endpoint_extra7_position(position,raw,id,"Extra7 endpoint token position",work);
            const auto endpoint_offset=flow400_number(position.get("byte_offset"));
            flow400_require(endpoint_offset>=offset&&endpoint_offset<offset+length&&
                            flow400_number(position.get("line"))==flow400_number(statement.get("line")),
                            "GroupEndpoint Extra7 endpoint within statement");
        }
        const auto expected_kind=entry.kind==GroupEndpointOrderKind::Ordinary?"vertex":"group_endpoint";
        flow400_require(flow400_text(fact.get("wire_kind"))==expected_kind,"GroupEndpoint Extra7 ordered source wire kind");
    });
}

inline GroupEndpointProfile group_endpoint_extra7_profile(const GroupEndpoint15Model& model,OracleWork& work) {
    auto result=group_endpoint15_profile(model,work,2u);
    flow400_require(model.tagged.size()<=3u,"GroupEndpoint Extra7 tagged profile bound");
    return result;
}
} // namespace loom::test::mermaid_flow
