// Test-private authority for the reviewed Flow400 group-endpoint cohort.
// It never changes the immutable base or Config21 authority assets.
#pragma once
#include "mermaid_flow400_authority.hpp"

namespace loom::test::mermaid_flow {
inline constexpr std::size_t kFlow400GroupEndpoint15AssetBytes = 64659u;
inline constexpr std::string_view kFlow400GroupEndpoint15AssetSha =
    "620ec808ffbad535391000b79bebf5d088b87f895a40a399ab91b250e9e23a1f";
// The frozen asset has only the 888- and 1,019-byte compact Model payloads.
// This is a per-Model bound, not an asset-sized charge repeated for every row.
inline constexpr std::size_t kFlow400GroupEndpoint15ModelWireBytes = 1019u;
inline constexpr std::size_t kFlow400GroupEndpoint15HandAssetBytes = 54421u;
inline constexpr std::string_view kFlow400GroupEndpoint15HandAssetSha =
    "8bf1edb4039e04fa60c8c17503dee0222ff72bf580bd315fba86c733567c4ff3";

struct GroupEndpoint15WireRef { GroupEndpointKind kind; std::string id; };
struct GroupEndpoint15WireEdge {
    GroupEndpoint15WireRef source,target;
    std::string label,style;
    bool start=false,end=true;
    std::size_t source_order=0u;
};
struct GroupEndpoint15Model {
    Model base;
    std::vector<GroupEndpoint15WireEdge> tagged;
    std::vector<GroupEndpointOrder> source_order;
};
struct GroupEndpoint15Documents { OracleWork asset_work; json::JsonDoc asset; };
struct GroupEndpoint15HandDocuments { OracleWork asset_work; json::JsonDoc asset; };
struct GroupEndpoint15Tuple {
    std::string_view case_id,fixture,source_sha256;
    std::size_t ordinal=0u,manifest_global_ordinal=0u;
};
inline constexpr std::array<GroupEndpoint15Tuple,15> kFlow400GroupEndpoint15Tuples{{
    {"flow400_group_endpoint_46","flowchart/nightly-check/dagre/66-dagre-more-nested-subgraph-cases-tb.mmd","4a950aa7c9c78e06ceb6e0f27b7f830b169fcfe9f30c58ff5b0a2811c58c1bdb",46u,707u},
    {"flow400_group_endpoint_48","flowchart/nightly-check/dagre/67-dagre-more-nested-subgraph-cases-rl.mmd","7c76370e0e7abbcf5accefa43ebe7347ce0f6d9dca17b85ac0db3ae8a094406a",48u,709u},
    {"flow400_group_endpoint_49","flowchart/nightly-check/dagre/68-dagre-more-nested-subgraph-cases-bt.mmd","6c1b8e69ef6ff81dc9fec44a178902606d821d6a8c02b5d303585fad93430985",49u,710u},
    {"flow400_group_endpoint_50","flowchart/nightly-check/dagre/69-dagre-more-nested-subgraph-cases-lr.mmd","152078a8abc87d36a815f03a8350ec8cdbf9ea83bdf4ee180462d3af1594727d",50u,711u},
    {"flow400_group_endpoint_51","flowchart/nightly-check/dagre/70-dagre-handle-nested-subgraph-cases-tb-link-out-and-link-between-subgraphs.mmd","ad8cccaaff50ae5e3cbbbfcf45bf67b740bb03ba1dcef0a29f2bab439c2d568d",51u,712u},
    {"flow400_group_endpoint_52","flowchart/nightly-check/dagre/71-dagre-handle-nested-subgraph-cases-rl-link-out-and-link-between-subgraphs.mmd","0097824ef99719abe027b6c72a8dd3af82d42ae7e2743e9d1c22b49754e9823a",52u,713u},
    {"flow400_group_endpoint_53","flowchart/nightly-check/dagre/72-dagre-handle-nested-subgraph-cases-bt-link-out-and-link-between-subgraphs.mmd","7bcbd6376a538b18d80a893f78fb95336663fd8d45d72f78f42168cf91cf590b",53u,714u},
    {"flow400_group_endpoint_56","flowchart/nightly-check/dagre/74-dagre-handle-nested-subgraph-cases-rl-link-out-and-link-between-subgraphs.mmd","294576e02acdb10b8432a886ce70b402ee5cd46391c3b4cde9c1ea954a2b6b86",56u,717u},
    {"flow400_group_endpoint_329","flowchart/visual-check/66-more-nested-subgraph-cases-tb.mmd","4f8673f25e71d8e16db8f2d1442363475ac006415d8e3deb3e43571e7593f3e2",329u,990u},
    {"flow400_group_endpoint_331","flowchart/visual-check/67-more-nested-subgraph-cases-rl.mmd","8d1eb583b7f85940c51969ed8b9f73e8257b19a64dab652583d664b7101deff4",331u,992u},
    {"flow400_group_endpoint_333","flowchart/visual-check/68-more-nested-subgraph-cases-bt.mmd","16008964dbead8130d94ea1ce29241be41b853abc501798181bab1e082d735ab",333u,994u},
    {"flow400_group_endpoint_335","flowchart/visual-check/69-more-nested-subgraph-cases-lr.mmd","03f48a035d877bb984d721661e374943bb561bdfee7ff31b85d0b6e69351b5e6",335u,996u},
    {"flow400_group_endpoint_340","flowchart/visual-check/70-handle-nested-subgraph-cases-tb-link-out-and-link-between-subgraphs.mmd","31cfba9707996355e2702000a2162b3b913bdca209c99ba1790abe4111590e99",340u,1001u},
    {"flow400_group_endpoint_343","flowchart/visual-check/71-handle-nested-subgraph-cases-rl-link-out-and-link-between-subgraphs.mmd","811d29c78d2a51cb35f6e239d6ffedfd5d3886d8f06aed13001938d05db17294",343u,1004u},
    {"flow400_group_endpoint_344","flowchart/visual-check/72-handle-nested-subgraph-cases-bt-link-out-and-link-between-subgraphs.mmd","7b9ccfdcdf87a068f7573ee3c73e6963df992477ce82deb007e1b174b41ee3a1",344u,1005u},
}};

inline void group_endpoint15_keys(JsonVal value,std::initializer_list<std::string_view> keys,std::string_view scope) {
    if(!value.is_obj()) throw std::runtime_error("GroupEndpoint15 "+std::string(scope)+" wrong kind");
    std::array<bool,24> seen{};if(keys.size()>seen.size()) throw std::runtime_error("GroupEndpoint15 internal key bound");
    value.iter_obj([&](JsonVal key,JsonVal) {const auto text=flow400_text(key);const auto at=std::ranges::find(keys,text);
        if(at==keys.end()) throw std::runtime_error("GroupEndpoint15 "+std::string(scope)+" unknown key");
        const auto index=static_cast<std::size_t>(at-keys.begin());if(seen[index]) throw std::runtime_error("GroupEndpoint15 "+std::string(scope)+" duplicate key");seen[index]=true;});
    if(value.size()!=keys.size()) throw std::runtime_error("GroupEndpoint15 "+std::string(scope)+" missing key");
    for(std::size_t index=0u;index<keys.size();++index) if(!seen[index]) throw std::runtime_error("GroupEndpoint15 "+std::string(scope)+" missing key");
}
inline std::string group_endpoint15_canonical(JsonVal value,OracleWork& work,int depth=0) {
    work.spend();flow400_require(depth<=24,"GroupEndpoint15 canonical JSON depth");
    if(value.is_obj()) {
        work.spend(4u*sizeof(std::vector<std::pair<std::string,JsonVal>>)+256u);
        std::vector<std::pair<std::string,JsonVal>> entries;entries.reserve(value.size());
        value.iter_obj([&](JsonVal key,JsonVal child) {work.product(key.as_str().size(),8u);work.spend(sizeof(std::pair<std::string,JsonVal>));entries.emplace_back(std::string(key.as_str()),child);});
        work.spend(entries.size()*entries.size()+1u);std::ranges::sort(entries,{},&std::pair<std::string,JsonVal>::first);
        std::string output="{";std::string previous;bool first=true;
        for(const auto& [key,child]:entries) {flow400_require(first||key!=previous,"GroupEndpoint15 duplicate canonical key");if(!first) output+=',';first=false;previous=key;json::JsonMutDoc atom;auto encoded=atom.string(key);atom.set_root(encoded);output+=atom.to_string();output+=':';output+=group_endpoint15_canonical(child,work,depth+1);}output+='}';
        return output;
    }
    if(value.is_arr()) {work.spend(4u*sizeof(std::string)+64u);std::string output="[";for(std::size_t index=0u;index<value.size();++index){if(index) output+=',';output+=group_endpoint15_canonical(value.at(index),work,depth+1);}output+=']';return output;}
    if(value.is_str()) work.product(value.as_str().size(),8u); else work.spend(64u);
    flow400_require(value.valid(),"GroupEndpoint15 canonical missing value");return value.to_string();
}
inline GroupEndpoint15WireRef group_endpoint15_ref(JsonVal value) {
    group_endpoint15_keys(value,{"kind","id"},"endpoint reference");const auto kind=flow400_text(value.get("kind")),id=flow400_text(value.get("id"));
    flow400_require(!id.empty()&&(kind=="vertex"||kind=="group"),"GroupEndpoint15 endpoint reference");
    return {kind=="vertex"?GroupEndpointKind::Vertex:GroupEndpointKind::Group,std::string(id)};
}
inline std::size_t group_endpoint15_node(const Model& model,std::string_view id) {
    const auto at=std::ranges::find_if(model.nodes,[&](const Node& value){return value.id==id;});
    if(at==model.nodes.end()) throw std::runtime_error("GroupEndpoint15 absent vertex endpoint");return static_cast<std::size_t>(at-model.nodes.begin());
}
inline std::size_t group_endpoint15_group(const Model& model,std::string_view id) {
    const auto at=std::ranges::find_if(model.groups,[&](const Group& value){return value.id==id;});
    if(at==model.groups.end()) throw std::runtime_error("GroupEndpoint15 absent group endpoint");return static_cast<std::size_t>(at-model.groups.begin());
}
inline GroupEndpoint15Model group_endpoint15_model(JsonVal value,OracleWork& work,
                                                   std::size_t max_tagged_edges=2u,
                                                   std::size_t max_target_incoming=1u) {
    // Prepay each bounded owned Model before parse/copy output, not the 64 KiB
    // source asset for every row.
    work.product(kFlow400GroupEndpoint15ModelWireBytes,24u);
    flow400_require(max_tagged_edges>=1u&&max_tagged_edges<=3u&&max_target_incoming>=1u&&max_target_incoming<=2u,
                    "GroupEndpoint15 decoder profile bound");
    if(max_tagged_edges==3u)
        group_endpoint15_keys(value,{"base_model","tagged_group_endpoint_edges","source_edge_order","source_semantic_counts","source_positions"},"Model");
    else
        group_endpoint15_keys(value,{"base_model","tagged_group_endpoint_edges","source_edge_order","source_semantic_counts"},"Model");
    std::size_t groups=0u,members=0u;flow400_shape(value.get("base_model"),groups,members);
    GroupEndpoint15Model result{parse_model(value.get("base_model")),{}, {}};
    const auto tagged=value.get("tagged_group_endpoint_edges");flow400_require(tagged.is_arr()&&tagged.size()>=1u&&tagged.size()<=max_tagged_edges,"GroupEndpoint15 tagged edge bound");
    work.product(tagged.size(),2u*sizeof(GroupEndpoint15WireEdge)+32u);result.tagged.reserve(tagged.size());
    tagged.iter([&](JsonVal edge) {
        group_endpoint15_keys(edge,{"source","target","label","style","hasArrowStart","hasArrowEnd","source_order"},"tagged edge");
        GroupEndpoint15WireEdge decoded{group_endpoint15_ref(edge.get("source")),group_endpoint15_ref(edge.get("target")),std::string(flow400_text(edge.get("label"))),std::string(flow400_text(edge.get("style"))),edge.get("hasArrowStart").as_bool(),edge.get("hasArrowEnd").as_bool(),flow400_number(edge.get("source_order"))};
        flow400_require(decoded.label.empty()&&decoded.style=="solid"&&!decoded.start&&decoded.end,"GroupEndpoint15 closed tagged edge wire");
        static_cast<void>(decoded.source.kind==GroupEndpointKind::Vertex?group_endpoint15_node(result.base,decoded.source.id):group_endpoint15_group(result.base,decoded.source.id));
        static_cast<void>(decoded.target.kind==GroupEndpointKind::Vertex?group_endpoint15_node(result.base,decoded.target.id):group_endpoint15_group(result.base,decoded.target.id));
        result.tagged.push_back(std::move(decoded));
    });
    const auto order=value.get("source_edge_order");const auto total=result.base.edges.size()+result.tagged.size();
    flow400_require(order.is_arr()&&order.size()==total,"GroupEndpoint15 source order cardinality");
    work.product(total,2u*sizeof(GroupEndpointOrder)+sizeof(bool)+16u);result.source_order.reserve(total);
    std::vector<bool> ordinary(result.base.edges.size()),tagged_seen(result.tagged.size());
    for(std::size_t position=0u;position<order.size();++position) {
        const auto item=order.at(position);group_endpoint15_keys(item,{"kind","index"},"source edge order");const auto kind=flow400_text(item.get("kind"));const auto index=flow400_number(item.get("index"));
        if(kind=="vertex") {flow400_require(index<ordinary.size()&&!ordinary[index],"GroupEndpoint15 duplicate ordinary source order");ordinary[index]=true;result.source_order.push_back({GroupEndpointOrderKind::Ordinary,index});}
        else if(kind=="group_endpoint") {flow400_require(index<tagged_seen.size()&&!tagged_seen[index]&&result.tagged[index].source_order==position,"GroupEndpoint15 tagged source order differs");tagged_seen[index]=true;result.source_order.push_back({GroupEndpointOrderKind::Tagged,index});}
        else throw std::runtime_error("GroupEndpoint15 source order kind");
    }
    flow400_require(std::ranges::all_of(ordinary,[](bool value){return value;})&&std::ranges::all_of(tagged_seen,[](bool value){return value;}),"GroupEndpoint15 source order partition");
    struct TypedTargetCount { GroupEndpointKind kind; std::string id; std::size_t count=0u; };
    std::vector<TypedTargetCount> targets;targets.reserve(total);
    const auto add_target=[&](GroupEndpointKind kind,std::string_view id,std::string_view error) {
        auto found=std::ranges::find_if(targets,[&](const auto& value){return value.kind==kind&&value.id==id;});
        if(found==targets.end()) {targets.push_back({kind,std::string(id),1u});return;}
        flow400_require(++found->count<=max_target_incoming,error.data());
    };
    for(const auto& edge:result.base.edges) {
        const auto id=result.base.nodes.at(static_cast<std::size_t>(edge.to)).id;
        add_target(GroupEndpointKind::Vertex,id,"GroupEndpoint15 duplicate ordinary target");
    }
    for(const auto& edge:result.tagged) {
        add_target(edge.target.kind,edge.target.id,"GroupEndpoint15 duplicate tagged target");
    }
    const auto counts=value.get("source_semantic_counts");group_endpoint15_keys(counts,{"nodes","groups","ordinary_vertex_edges","group_endpoint_edges","source_edges"},"semantic counts");
    flow400_require(flow400_number(counts.get("nodes"))==result.base.nodes.size()&&flow400_number(counts.get("groups"))==result.base.groups.size()&&flow400_number(counts.get("ordinary_vertex_edges"))==result.base.edges.size()&&flow400_number(counts.get("group_endpoint_edges"))==result.tagged.size()&&flow400_number(counts.get("source_edges"))==total,"GroupEndpoint15 semantic counts");
    if(max_tagged_edges==3u) {
        const auto positions=value.get("source_positions");
        group_endpoint15_keys(positions,{"position_unit","group_declarations","ordered_wire_facts","tagged_group_references","source_order_edge_count","all_group_declarations_precede_tagged_group_references","ordinary_edge_order_is_not_constrained_by_later_vertex_declarations","explicit_vertex_declarations"},"source positions");
        flow400_require(flow400_number(positions.get("source_order_edge_count"))==total,"GroupEndpoint15 source position cardinality");
        const auto facts=positions.get("ordered_wire_facts");flow400_require(facts.is_arr()&&facts.size()==total,"GroupEndpoint15 ordered source position facts");
        std::size_t fact_index=0u;
        facts.iter([&](JsonVal fact) {
            const auto at=fact_index++;
            flow400_require(flow400_number(fact.get("source_order"))==at,"GroupEndpoint15 source position order");
            const auto entry=result.source_order.at(at);const auto wire_kind=flow400_text(fact.get("wire_kind"));
            flow400_require((entry.kind==GroupEndpointOrderKind::Ordinary&&wire_kind=="vertex")||
                            (entry.kind==GroupEndpointOrderKind::Tagged&&wire_kind=="group_endpoint"),
                            "GroupEndpoint15 source position edge kind");
            const auto check_group=[&](JsonVal endpoint,std::string_view endpoint_name) {
                if(flow400_text(endpoint.get("kind"))!="group") return;
                const auto id=flow400_text(endpoint.get("id"));JsonVal declaration;std::size_t declarations=0u;
                positions.get("group_declarations").iter([&](JsonVal candidate) {if(flow400_text(candidate.get("id"))==id){declaration=candidate;++declarations;}});
                flow400_require(declarations==1u,"GroupEndpoint15 group declaration position identity");
                const auto reference=fact.get(endpoint_name).get("position").get("byte_offset");
                const auto declared=declaration.get("position").get("byte_offset");
                flow400_require(flow400_number(declared)<flow400_number(reference),"GroupEndpoint15 forward group endpoint reference");
            };
            if(entry.kind==GroupEndpointOrderKind::Tagged) {check_group(fact.get("source"),"source");check_group(fact.get("target"),"target");}
        });
    }
    return result;
}
inline GroupEndpoint15Model group_endpoint_extra7_model(JsonVal value,OracleWork& work) {
    return group_endpoint15_model(value,work,3u,2u);
}
inline GroupEndpointProfile group_endpoint15_profile(const GroupEndpoint15Model& model,OracleWork& work,std::size_t max_target_incoming=1u) {
    work.spend(4u*sizeof(GroupEndpointProfile)+256u);work.product(model.tagged.size(),2u*sizeof(GroupEndpointEdge)+32u);work.product(model.source_order.size(),2u*sizeof(GroupEndpointOrder)+16u);
    GroupEndpointProfile result;result.tagged.reserve(model.tagged.size());result.source_order=model.source_order;
    result.max_target_incoming=max_target_incoming;
    for(const auto& edge:model.tagged) {
        const auto ref=[&](const GroupEndpoint15WireRef& value) {
            return GroupEndpointRef{value.kind,value.kind==GroupEndpointKind::Vertex?group_endpoint15_node(model.base,value.id):group_endpoint15_group(model.base,value.id)};
        };
        result.tagged.push_back({ref(edge.source),ref(edge.target),edge.label,edge.style,edge.start,edge.end});
    }
    return result;
}
inline void group_endpoint15_copy_model(const GroupEndpoint15Model& model,OracleWork& work) {
    flow400_copy_model(model.base,work);work.product(model.tagged.size(),3u*sizeof(GroupEndpoint15WireEdge)+32u);work.product(model.source_order.size(),3u*sizeof(GroupEndpointOrder)+16u);
    for(const auto& edge:model.tagged) {for(const auto text:{std::string_view(edge.source.id),std::string_view(edge.target.id),std::string_view(edge.label),std::string_view(edge.style)}) work.product(text.size(),4u);}
}
inline const GroupEndpoint15Tuple& group_endpoint15_tuple(JsonVal row) {
    const auto case_id=flow400_text(row.get("case_id"));const auto ordinal=flow400_number(row.get("ordinal"));const auto global=flow400_number(row.get("manifest_global_ordinal"));
    const auto found=std::ranges::find_if(kFlow400GroupEndpoint15Tuples,[&](const GroupEndpoint15Tuple& tuple) {return tuple.case_id==case_id&&tuple.ordinal==ordinal&&tuple.manifest_global_ordinal==global;});
    if(found==kFlow400GroupEndpoint15Tuples.end()) throw std::runtime_error("GroupEndpoint15 exact tuple differs");
    flow400_require(flow400_text(row.get("fixture"))==found->fixture&&flow400_text(row.get("source_sha256"))==found->source_sha256,"GroupEndpoint15 exact tuple differs");return *found;
}
inline void group_endpoint15_record(JsonVal row,OracleWork& work) {
    group_endpoint15_keys(row,{"case_id","ordinal","manifest_global_ordinal","fixture","source_git_blob","raw_source_utf8","source_bytes","source_sha256","body_utf8","body_sha256","frontmatter","source_direction","topology","model","canonical_model_sha256","selection_constraints"},"record");
    static_cast<void>(group_endpoint15_tuple(row));const auto source=flow400_text(row.get("raw_source_utf8"));
    flow400_require(source.size()==flow400_number(row.get("source_bytes"))&&digest(source)==flow400_text(row.get("source_sha256")),"GroupEndpoint15 source SHA");
    flow400_require(digest(flow400_text(row.get("body_utf8")))==flow400_text(row.get("body_sha256")),"GroupEndpoint15 body SHA");
    const auto model=group_endpoint15_model(row.get("model"),work);static_cast<void>(model);
    const auto canonical=group_endpoint15_canonical(row.get("model"),work);
    flow400_require(digest(canonical)==flow400_text(row.get("canonical_model_sha256")),"GroupEndpoint15 canonical Model SHA");
}
inline GroupEndpoint15HandDocuments group_endpoint15_hand_documents() {
    GroupEndpoint15HandDocuments documents;documents.asset_work.spend(16384u+4u*sizeof(json::JsonDoc));
    const auto path=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_native/flow400_group_endpoint15_hand_controls.v4.json";
    const auto size=std::filesystem::file_size(path);flow400_require(size==kFlow400GroupEndpoint15HandAssetBytes,"GroupEndpoint15 immutable hand asset bytes");
    documents.asset_work.product(static_cast<std::size_t>(size),3u);const auto bytes=flow400_read(path);flow400_require(digest(bytes)==kFlow400GroupEndpoint15HandAssetSha,"GroupEndpoint15 immutable hand asset SHA");
    auto parsed=json::parse(bytes);flow400_require(parsed.has_value(),"GroupEndpoint15 hand asset JSON");documents.asset=std::move(*parsed);
    const auto root=documents.asset.root();group_endpoint15_keys(root,{"schema","qualification","revision_from_r1","point_target_contract","construction_contract","good_scenes","hand_model_bindings","negative_controls","expected_nonresource_rejections_per_good_scene","formal_credit_before_native_and_official_owned_execution"},"hand root");
    flow400_require(flow400_text(root.get("schema"))=="flow400_group_endpoint15_hand_geometry/v3"&&flow400_number(root.get("formal_credit_before_native_and_official_owned_execution"))==0u,"GroupEndpoint15 hand root");
    const auto scenes=root.get("good_scenes"),bindings=root.get("hand_model_bindings"),controls=root.get("negative_controls");
    flow400_require(scenes.is_arr()&&scenes.size()==8u&&bindings.is_arr()&&bindings.size()==8u&&controls.is_arr()&&controls.size()==8u&&flow400_number(root.get("expected_nonresource_rejections_per_good_scene"))==8u,"GroupEndpoint15 hand control counts");
    return documents;
}

inline GroupEndpoint15Documents group_endpoint15_documents() {
    GroupEndpoint15Documents documents;documents.asset_work.spend(16384u+4u*sizeof(json::JsonDoc));
    const auto path=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_native/flow400_group_endpoint15_source_models.v1.json";
    const auto size=std::filesystem::file_size(path);flow400_require(size==kFlow400GroupEndpoint15AssetBytes,"GroupEndpoint15 immutable asset bytes");
    documents.asset_work.product(static_cast<std::size_t>(size),3u);const auto bytes=flow400_read(path);flow400_require(digest(bytes)==kFlow400GroupEndpoint15AssetSha,"GroupEndpoint15 immutable asset SHA");
    auto parsed=json::parse(bytes);flow400_require(parsed.has_value(),"GroupEndpoint15 asset JSON");documents.asset=std::move(*parsed);
    const auto root=documents.asset.root();group_endpoint15_keys(root,{"schema","qualification","cohort","endpoint_wire_contract","records","formal_credit_before_owned_execution"},"root");
    flow400_require(flow400_text(root.get("schema"))=="flow400_group_endpoint15_source_models/v1"&&flow400_number(root.get("formal_credit_before_owned_execution"))==0u,"GroupEndpoint15 root");
    const auto records=root.get("records");flow400_require(records.is_arr()&&records.size()==kFlow400GroupEndpoint15Tuples.size(),"GroupEndpoint15 record count");
    std::array<bool,400> seen{};records.iter([&](JsonVal row) {group_endpoint15_record(row,documents.asset_work);const auto ordinal=flow400_number(row.get("ordinal"));flow400_require(!seen[ordinal],"GroupEndpoint15 duplicate ordinal");seen[ordinal]=true;});return documents;
}
struct BoundGroupEndpoint15 { JsonVal record,authority; };
inline BoundGroupEndpoint15 group_endpoint15_bind(JsonVal base,JsonVal fixture,JsonVal records,JsonVal selected,std::size_t ordinal,std::size_t global,std::string_view source,OracleWork& work) {
    work.spend(8192u+4u*sizeof(BoundGroupEndpoint15));const auto& tuple=group_endpoint15_tuple(selected);
    flow400_require(tuple.ordinal==ordinal&&tuple.manifest_global_ordinal==global,"GroupEndpoint15 selected tuple differs");
    std::size_t matches=0u;records.iter([&](JsonVal row) {work.spend(512u);if(flow400_text(row.get("case_id"))==tuple.case_id&&flow400_number(row.get("ordinal"))==ordinal&&flow400_number(row.get("manifest_global_ordinal"))==global) ++matches;});
    flow400_require(matches==1u,"GroupEndpoint15 strict selected source");flow400_bind(base,fixture,ordinal,global,source,work);
    flow400_require(flow400_text(selected.get("raw_source_utf8"))==source,"GroupEndpoint15 whole source binding");return {selected,base};
}
} // namespace loom::test::mermaid_flow
