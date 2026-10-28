// Test-private source-bound Classic Flow400 overlay.
// It leaves Base8, Config21, and TypedSource4 assets untouched and supplies
// only complete original rows whose Classic Models are locked below.
#pragma once
#include "mermaid_flow400_typed_source4_authority.hpp"

namespace loom::test::mermaid_flow {

inline constexpr std::size_t kFlow400ClassicAssetBytes = 19764u;
inline constexpr std::string_view kFlow400ClassicAssetSha =
    "9b8580bda97015356b3bd442c4a698a3cda1e6d686aec731d3d1820ec281f79d";

struct Flow400ClassicDocuments { OracleWork asset_work; json::JsonDoc supplement; };

inline void flow400_classic_keys(JsonVal value,std::initializer_list<std::string_view> keys,
                                 std::string_view scope) {
    if(!value.is_obj()) throw std::runtime_error("Flow400 Classic "+std::string(scope)+" wrong kind");
    std::array<bool,16> seen{};
    if(keys.size()>seen.size()) throw std::runtime_error("Flow400 Classic internal key bound");
    value.iter_obj([&](JsonVal key,JsonVal) {
        const auto name=flow400_text(key);const auto at=std::ranges::find(keys,name);
        if(at==keys.end()) throw std::runtime_error("Flow400 Classic "+std::string(scope)+" unknown key");
        const auto index=static_cast<std::size_t>(at-keys.begin());
        if(seen[index]) throw std::runtime_error("Flow400 Classic "+std::string(scope)+" duplicate key");
        seen[index]=true;
    });
    if(value.size()!=keys.size()) throw std::runtime_error("Flow400 Classic "+std::string(scope)+" missing key");
    for(std::size_t index=0;index<keys.size();++index)
        if(!seen[index]) throw std::runtime_error("Flow400 Classic "+std::string(scope)+" missing key");
}

inline std::size_t flow400_classic_expected(JsonVal row) {
    const auto ordinal=flow400_number(row.get("base_authority").get("ordinal"));
    const auto expected=flow400_number(row.get("native_requirement").get("exact_rejections"));
    const std::size_t fixed=ordinal==244u?5u:ordinal==41u?5u:(ordinal==13u||ordinal==40u||ordinal==295u||ordinal==376u)?4u:0u;
    flow400_require(fixed>0u&&expected==fixed,"Flow400 Classic exact mutation count");return fixed;
}

inline void flow400_classic_envelope(JsonVal envelope,JsonVal row) {
    flow400_classic_keys(envelope,{"profile","bytes","sha256","text","description_form","root_keys","config","body_header","body_sha256"},"envelope");
    const auto source=flow400_text(row.get("source")),text=flow400_text(envelope.get("text"));
    flow400_require(flow400_number(envelope.get("bytes"))==text.size()&&digest(text)==flow400_text(envelope.get("sha256"))&&source.starts_with(text),
        "Flow400 Classic envelope bytes/SHA");
    flow400_require(flow400_text(envelope.get("description_form"))=="plain_multiline","Flow400 Classic description form");
    const auto roots=envelope.get("root_keys");flow400_require(roots.is_arr(),"Flow400 Classic envelope root keys");
    const auto body=source.substr(text.size());
    flow400_require(digest(body)==flow400_text(envelope.get("body_sha256"))&&body.starts_with(flow400_text(envelope.get("body_header"))),
        "Flow400 Classic body SHA/header");
    const auto profile=flow400_text(envelope.get("profile"));
    const auto ordinal=flow400_number(row.get("base_authority").get("ordinal"));
    if(profile=="plain_description_only") {
        flow400_require(ordinal==376u&&roots.size()==1u&&flow400_text(roots.at(0u))=="description"&&envelope.get("config").is_null()&&
            flow400_text(envelope.get("body_header"))=="flowchart TB","Flow400 Classic plain envelope");
    } else {
        flow400_require(roots.size()==2u&&flow400_text(roots.at(0u))=="description"&&flow400_text(roots.at(1u))=="config"&&
            envelope.get("config").is_obj(),"Flow400 Classic config envelope roots");
        const auto config=envelope.get("config");
        if(profile=="description_config_dagre") {
            flow400_require(ordinal==244u&&flow400_text(envelope.get("body_header"))=="flowchart TD","Flow400 Classic dagre body header");
            flow400_classic_keys(config,{"layout"},"dagre config");flow400_require(flow400_text(config.get("layout"))=="dagre","Flow400 Classic dagre config");
        } else if(profile=="description_config_dagre_title_top_margin_zero") {
            flow400_require((ordinal==13u&&flow400_text(envelope.get("body_header"))=="flowchart TD")||
                ((ordinal==40u||ordinal==41u)&&flow400_text(envelope.get("body_header"))=="flowchart LR"),"Flow400 Classic titleTopMargin body header");
            flow400_classic_keys(config,{"layout","flowchart"},"dagre titleTopMargin config");
            flow400_require(flow400_text(config.get("layout"))=="dagre","Flow400 Classic titleTopMargin dagre config");
            const auto flowchart=config.get("flowchart");flow400_classic_keys(flowchart,{"titleTopMargin"},"titleTopMargin namespace");
            flow400_require(flow400_number(flowchart.get("titleTopMargin"))==0u,"Flow400 Classic titleTopMargin value");
        } else {
            flow400_require(profile=="description_config_font_courier"&&ordinal==295u&&flow400_text(envelope.get("body_header"))=="graph TD",
                "Flow400 Classic courier body header");
            flow400_classic_keys(config,{"fontFamily"},"courier config");flow400_require(flow400_text(config.get("fontFamily"))=="courier","Flow400 Classic courier config");
        }
    }
}

inline void flow400_classic_model_shape(const Model& model,std::size_t ordinal) {
    const auto node=[&](std::string_view id,Shape shape,std::string_view label) {
        const auto found=std::ranges::find_if(model.nodes,[&](const Node& candidate){return candidate.id==id;});
        flow400_require(found!=model.nodes.end()&&found->shape==shape&&found->label==label,"Flow400 Classic Model node identity");
    };
    const auto index=[&](std::string_view id){
        const auto found=std::ranges::find_if(model.nodes,[&](const Node& candidate){return candidate.id==id;});
        flow400_require(found!=model.nodes.end(),"Flow400 Classic Model endpoint node");return static_cast<int>(found-model.nodes.begin());
    };
    const auto edge=[&](std::size_t at,std::string_view from,std::string_view to,std::string_view label){
        flow400_require(at<model.edges.size(),"Flow400 Classic Model edge count");const auto& value=model.edges[at];
        flow400_require(value.from==index(from)&&value.to==index(to)&&value.label==label&&value.style=="solid"&&!value.start&&value.end,
            "Flow400 Classic Model edge identity");
    };
    if(ordinal==13u) {
        flow400_require(model.direction=="TB"&&model.nodes.size()==2u&&model.edges.size()==1u&&model.groups.empty(),
            "Flow400 Classic titleTopMargin Model cardinality");
        node("A",Shape::Rectangle,"A");node("B",Shape::Rectangle,"B");edge(0u,"A","B","");
    } else if(ordinal==244u) {
        flow400_require(model.direction=="TB"&&model.nodes.size()==6u&&model.edges.size()==5u&&model.groups.size()==3u,"Flow400 Classic dagre Model cardinality");
        node("A",Shape::Rectangle,"Client");node("B",Shape::Rectangle,"Cache");node("C",Shape::Rectangle,"API");
        node("D",Shape::Cylinder,"Database");node("E",Shape::Rectangle,"Auth Service");node("F",Shape::Cylinder,"Sessions");
        edge(0u,"A","B","");edge(1u,"C","D","");edge(2u,"E","F","");edge(3u,"B","C","");edge(4u,"B","E","");
        constexpr std::array<std::string_view,3> ids{{"Frontend","Backend","Auth"}};
        constexpr std::array<std::string_view,6> members{{"A","B","C","D","E","F"}};
        for(std::size_t at=0u;at<ids.size();++at){const auto& group=model.groups[at];
            flow400_require(group.id==ids[at]&&group.label==ids[at]&&group.direction.empty()&&group.parent==-1&&group.nodes.size()==2u&&
                group.nodes[0]==index(members[2u*at])&&group.nodes[1]==index(members[2u*at+1u]),"Flow400 Classic Model group identity");}
    } else if(ordinal==295u) {
        flow400_require(model.direction=="TB"&&model.nodes.size()==6u&&model.edges.size()==5u&&model.groups.empty(),"Flow400 Classic trapezoid Model cardinality");
        node("A",Shape::Trapezoid,"Christmas");node("B",Shape::TrapezoidAlt,"Go shopping");
        node("C",Shape::Decision,"Let me thinksssss\nssssssssssssssssssssss\nsssssssssssssssssssssssssss");
        node("D",Shape::Parallelogram,"Laptop");node("E",Shape::ParallelogramAlt,"iPhone");node("F",Shape::Rectangle,"Car");
        edge(0u,"A","B","Get money");edge(1u,"B","C","");edge(2u,"C","D","One");edge(3u,"C","E","Two");edge(4u,"C","F","Three");
    } else if(ordinal==40u) {
        flow400_require(model.direction=="LR"&&model.nodes.size()==3u&&model.edges.size()==2u&&model.groups.empty(),
            "Flow400 Classic diamond fanout Model cardinality");
        node("A",Shape::Decision,"A");node("B",Shape::Rectangle,"B");node("C",Shape::Rectangle,"C");
        edge(0u,"A","B","");edge(1u,"A","C","");
    } else if(ordinal==41u) {
        flow400_require(model.direction=="LR"&&model.nodes.size()==3u&&model.edges.size()==2u&&model.groups.size()==1u,
            "Flow400 Classic diamond subgraph Model cardinality");
        node("A",Shape::Decision,"A");node("B",Shape::Rectangle,"B");node("C",Shape::Rectangle,"C");
        edge(0u,"A","B","");edge(1u,"A","C","");
        const auto& group=model.groups.front();
        flow400_require(group.id=="subbe"&&group.label=="subbe"&&group.direction.empty()&&group.parent==-1&&
            group.nodes.size()==1u&&group.nodes.front()==index("A"),"Flow400 Classic diamond subgraph membership");
    } else {
        flow400_require(ordinal==376u&&model.direction=="TB"&&model.nodes.size()==3u&&model.edges.size()==2u&&model.groups.empty(),
            "Flow400 Classic stadium Model cardinality");
        node("A",Shape::Stadium,"Start");node("n1",Shape::Rectangle,"Untitled Node");node("n2",Shape::Rectangle,"Untitled Node");
        edge(0u,"A","n1","");edge(1u,"A","n2","");
    }
}

inline void flow400_classic_case_shape(JsonVal row,OracleWork& work) {
    work.spend(8192u);
    flow400_classic_keys(row,{"case_id","base_authority","source","source_sha256","classic_model_json","classic_model_sha256","envelope","native_requirement","formal_status"},"case");
    const auto id=flow400_text(row.get("case_id"));
    flow400_require(id=="original_13_dagre_title_top_margin_zero"||id=="original_40_diamond_fanout"||id=="original_41_diamond_fanout_in_subgraph"||
        id=="original_244_dagre_subgraphs"||id=="original_295_classic_trapezoids"||id=="original_376_stadium","Flow400 Classic case identity");
    const auto authority=row.get("base_authority");
    flow400_keys(authority,{"adaptation","fixture","manifest_global_ordinal","model_json","model_sha256","oracle_record_sha256","ordinal","source_bytes","source_git_blob","source_sha256"});
    flow400_require(authority.get("model_json").is_null()&&authority.get("model_sha256").is_null()&&authority.get("adaptation").is_null(),
        "Flow400 Classic base must remain null");
    const auto source=flow400_text(row.get("source"));
    flow400_require(source.size()==flow400_number(authority.get("source_bytes"))&&digest(source)==flow400_text(row.get("source_sha256"))&&
        flow400_text(row.get("source_sha256"))==flow400_text(authority.get("source_sha256")),"Flow400 Classic source lock");
    const auto model=flow400_text(row.get("classic_model_json"));
    flow400_require(model.size()<=4096u&&digest(model)==flow400_text(row.get("classic_model_sha256")),"Flow400 Classic Model lock");
    work.product(source.size()+model.size(),32u);work.spend(4u*sizeof(json::JsonDoc)+8192u);
    auto model_document=json::parse(model);flow400_require(model_document.has_value(),"Flow400 Classic Model JSON");
    const auto parsed=flow_classic_shape_model(model_document->root(),work);const auto ordinal=flow400_number(authority.get("ordinal"));
    flow400_classic_model_shape(parsed,ordinal);flow400_classic_envelope(row.get("envelope"),row);
    const auto native=row.get("native_requirement");flow400_classic_keys(native,{"exact_rejections","mutations"},"native requirement");
    const auto expected=flow400_classic_expected(row);
    const auto mutations=native.get("mutations");
    flow400_require(mutations.is_arr()&&mutations.size()==expected,"Flow400 Classic native mutation plan");
    constexpr std::array<std::string_view,4> title_top_margin{{"A_rectangle_to_circle","A_label_to_absent","first_edge_endpoint_to_self","extra_ink"}};
    constexpr std::array<std::string_view,4> diamond_fanout{{"diamond_to_rectangle","A_label_to_absent","fanout_target_to_self","extra_ink"}};
    constexpr std::array<std::string_view,5> diamond_fanout_group{{"diamond_to_rectangle","A_label_to_absent","fanout_target_to_self","subbe_remove_A","extra_ink"}};
    constexpr std::array<std::string_view,5> dagre{{"cylinder_to_circle","Client_label_to_absent","first_edge_endpoint_to_self","Frontend_remove_first_member","extra_ink"}};
    constexpr std::array<std::string_view,4> trapezoids{{"trapezoid_to_trapezoid_alt","Christmas_label_to_absent","first_edge_endpoint_to_self","extra_ink"}};
    constexpr std::array<std::string_view,4> stadium{{"stadium_to_circle","Start_label_to_absent","first_edge_endpoint_to_self","extra_ink"}};
    for(std::size_t at=0u;at<expected;++at){const auto actual=flow400_text(mutations.at(at));
        const auto wanted=ordinal==13u?title_top_margin.at(at):ordinal==40u?diamond_fanout.at(at):ordinal==41u?diamond_fanout_group.at(at):ordinal==244u?dagre.at(at):ordinal==295u?trapezoids.at(at):stadium.at(at);
        flow400_require(actual==wanted,"Flow400 Classic native mutation identity");}
    const bool exact_tuple=(id=="original_13_dagre_title_top_margin_zero"&&ordinal==13u&&expected==4u)||
        (id=="original_40_diamond_fanout"&&ordinal==40u&&expected==4u)||
        (id=="original_41_diamond_fanout_in_subgraph"&&ordinal==41u&&expected==5u)||
        (id=="original_244_dagre_subgraphs"&&ordinal==244u&&expected==5u)||
        (id=="original_295_classic_trapezoids"&&ordinal==295u&&expected==4u)||
        (id=="original_376_stadium"&&ordinal==376u&&expected==4u);
    flow400_require(exact_tuple,"Flow400 Classic case-id/ordinal/mutation tuple");
    flow400_require(flow400_text(row.get("formal_status"))=="unadmitted_design_only","Flow400 Classic unadmitted source state");
}

inline void flow400_classic_root(JsonVal root,OracleWork& work) {
    flow400_classic_keys(root,{"schema","qualification","base_authority","prior_authority","formal_counts","cases"},"root");
    flow400_require(flow400_text(root.get("schema"))=="flow400_classic_native_supplement/v1"&&
        flow400_text(root.get("qualification"))=="Source-pinned complete Classic Models require independent Native and Official actual runs before admission.",
        "Flow400 Classic schema/qualification");
    const auto base=root.get("base_authority");flow400_classic_keys(base,{"asset_sha256","manifest_sha256","all_sources","complete_models","pending"},"base authority");
    flow400_require(flow400_text(base.get("asset_sha256"))==kFlow400AssetSha&&flow400_text(base.get("manifest_sha256"))==kFlow400ManifestSha&&
        flow400_number(base.get("all_sources"))==400u&&flow400_number(base.get("complete_models"))==8u&&flow400_number(base.get("pending"))==392u,
        "Flow400 Classic base authority");
    const auto prior=root.get("prior_authority");flow400_classic_keys(prior,{"config21_asset_sha256","typed_source4_asset_sha256","aggregate_complete_before_classic","aggregate_pending_before_classic"},"prior authority");
    flow400_require(flow400_text(prior.get("config21_asset_sha256"))==kFlow400Config21AssetSha&&flow400_text(prior.get("typed_source4_asset_sha256"))==kFlow400TypedSource4AssetSha&&
        flow400_number(prior.get("aggregate_complete_before_classic"))==33u&&flow400_number(prior.get("aggregate_pending_before_classic"))==367u,
        "Flow400 Classic prior authority");
    const auto counts=root.get("formal_counts");flow400_classic_keys(counts,{"cases","aggregate_complete_after_all_official_runs","aggregate_pending_after_all_official_runs","ultimate_complete_required","ultimate_pending_required","official_credit_before_execution"},"formal counts");
    flow400_require(flow400_number(counts.get("cases"))==6u&&flow400_number(counts.get("aggregate_complete_after_all_official_runs"))==39u&&
        flow400_number(counts.get("aggregate_pending_after_all_official_runs"))==361u&&flow400_number(counts.get("ultimate_complete_required"))==400u&&
        flow400_number(counts.get("ultimate_pending_required"))==0u&&flow400_number(counts.get("official_credit_before_execution"))==0u,
        "Flow400 Classic formal counts");
    const auto cases=root.get("cases");flow400_require(cases.is_arr()&&cases.size()==6u,"Flow400 Classic case count");
    std::array<bool,400> ordinals{};std::array<bool,6> identities{};
    cases.iter([&](JsonVal row) {
        flow400_classic_case_shape(row,work);const auto ordinal=flow400_number(row.get("base_authority").get("ordinal"));
        flow400_require(!ordinals[ordinal],"Flow400 Classic duplicate source ordinal");ordinals[ordinal]=true;
        const auto id=flow400_text(row.get("case_id"));const auto index=id=="original_13_dagre_title_top_margin_zero"?0u:
            id=="original_40_diamond_fanout"?1u:id=="original_244_dagre_subgraphs"?2u:id=="original_295_classic_trapezoids"?3u:
            id=="original_376_stadium"?4u:5u;
        flow400_require(!identities[index],"Flow400 Classic duplicate case identity");identities[index]=true;
    });
    flow400_require(ordinals[13]&&ordinals[40]&&ordinals[41]&&ordinals[244]&&ordinals[295]&&ordinals[376]&&std::ranges::all_of(identities,[](bool present){return present;}),
        "Flow400 Classic selected ordinal closure");
}

inline Flow400ClassicDocuments flow400_classic_documents() {
    Flow400ClassicDocuments documents;documents.asset_work.spend(4u*sizeof(json::JsonDoc)+4u*sizeof(OracleWork)+16384u);
    const auto path=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_native/flow400_classic_native_supplement.v1.json";
    const auto size=std::filesystem::file_size(path);flow400_require(size==kFlow400ClassicAssetBytes,"Flow400 Classic immutable asset bytes");
    documents.asset_work.product(static_cast<std::size_t>(size),3u);const auto bytes=flow400_read(path);
    flow400_require(digest(bytes)==kFlow400ClassicAssetSha,"Flow400 Classic immutable asset SHA");
    auto parsed=json::parse(bytes);flow400_require(parsed.has_value(),"Flow400 Classic asset JSON");documents.supplement=std::move(*parsed);
    flow400_classic_root(documents.supplement.root(),documents.asset_work);return documents;
}

inline JsonVal flow400_classic_select(const BoundFlow400TypedSource4Authority& prior,JsonVal classic,JsonVal fixture,
                                      std::size_t ordinal,std::size_t global,std::string_view source,OracleWork& work) {
    work.spend(8192u+8u*sizeof(std::size_t)+4u*sizeof(json::JsonVal));
    const auto cases=classic.get("cases");flow400_require(cases.is_arr()&&cases.size()==6u,"Flow400 Classic selection denominator");
    JsonVal selected;std::size_t matches=0u;
    cases.iter([&](JsonVal candidate) {
        work.spend(512u);if(flow400_number(candidate.get("base_authority").get("ordinal"))==ordinal){selected=candidate;++matches;}
    });
    flow400_require(matches<=1u,"Flow400 Classic duplicate source selection");if(matches==0u)return JsonVal{};
    flow400_require(prior.kind==Flow400TypedSource4Kind::Base&&prior.authority.get("model_json").is_null(),"Flow400 Classic overlaps prior authority");
    flow400_classic_case_shape(selected,work);flow400_bind(selected.get("base_authority"),fixture,ordinal,global,source,work);return selected;
}

inline Model flow400_classic_model(JsonVal selected,OracleWork& work) {
    const auto bytes=flow400_text(selected.get("classic_model_json"));
    flow400_require(digest(bytes)==flow400_text(selected.get("classic_model_sha256")),"Flow400 Classic selected Model SHA");
    work.product(bytes.size(),32u);work.spend(4u*sizeof(json::JsonDoc)+8192u);
    auto document=json::parse(bytes);flow400_require(document.has_value(),"Flow400 Classic selected Model JSON");
    auto model=flow_classic_shape_model(document->root(),work);
    flow400_classic_model_shape(model,flow400_number(selected.get("base_authority").get("ordinal")));return model;
}

} // namespace loom::test::mermaid_flow
