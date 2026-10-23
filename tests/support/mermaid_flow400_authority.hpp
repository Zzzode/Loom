// Test-private immutable Flow400 source authority and strict existing codec.
// Definitions only; callers own document lifetime and all setup/scene ledgers.
#pragma once
#include "mermaid_flow_oracle.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
namespace loom::test::mermaid_flow {
inline constexpr std::size_t kFlow400SceneLimit = 128000000u;
inline constexpr std::string_view kFlow400AssetSha = "f03178a8c6b3872abf1a3d2693fc1841b31492961f7a7a632424860d6a8d884c";
inline constexpr std::string_view kFlow400ManifestSha = "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b";

inline std::string_view flow400_text(JsonVal value) {
    flow400_require(value.is_str(),"Flow400 protocol expected string"); return value.as_str();
}
inline std::size_t flow400_number(JsonVal value) {
    flow400_require(value.is_int() && value.as_int() >= 0,"Flow400 protocol expected nonnegative integer");
    const auto n=static_cast<std::size_t>(value.as_int());
    flow400_require(static_cast<int64_t>(n)==value.as_int(),"Flow400 protocol integer range"); return n;
}
inline void flow400_keys(JsonVal value,std::initializer_list<std::string_view> keys) {
    flow400_require(value.is_obj() && value.size()==keys.size() && keys.size()<=16,"Flow400 protocol object keys");
    std::array<bool,16> seen{};
    value.iter_obj([&](JsonVal key,JsonVal) {
        const auto name=flow400_text(key); const auto at=std::ranges::find(keys,name);
        flow400_require(at!=keys.end(),"Flow400 protocol unknown key");
        const auto index=static_cast<std::size_t>(at-keys.begin());
        flow400_require(!seen[index],"Flow400 protocol duplicate key"); seen[index]=true;
    });
}
inline std::string flow400_read(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    flow400_require(static_cast<bool>(input),"Flow400 protocol input unavailable");
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
struct Flow400Documents {
    OracleWork manifest_work,compact_work;
    json::JsonDoc manifest,compact;
};
inline Flow400Documents flow400_documents() {
    Flow400Documents docs;
    const auto fixed=4u*sizeof(json::JsonDoc)+4u*sizeof(OracleWork)+8192u;
    docs.manifest_work.spend(fixed); docs.compact_work.spend(fixed);
    const auto root=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures";
    const auto mp=root/"mermaid_official/manifest.json";
    const auto ap=root/"mermaid_native/flow400_native_whole_source.v1.json";
    const auto ms=std::filesystem::file_size(mp),as=std::filesystem::file_size(ap);
    flow400_require(ms==1272942u && as==188865u,"Flow400 protocol immutable input bytes");
    docs.manifest_work.product(static_cast<std::size_t>(ms),3u);
    auto mb=flow400_read(mp); flow400_require(digest(mb)==kFlow400ManifestSha,"Flow400 protocol manifest SHA");
    auto manifest=json::parse(mb); flow400_require(manifest.has_value(),"Flow400 protocol manifest JSON"); docs.manifest=std::move(*manifest);
    docs.compact_work.product(static_cast<std::size_t>(as),3u);
    auto ab=flow400_read(ap); flow400_require(digest(ab)==kFlow400AssetSha,"Flow400 protocol authority SHA");
    auto compact=json::parse(ab); flow400_require(compact.has_value(),"Flow400 protocol authority JSON"); docs.compact=std::move(*compact);
    const auto top=docs.compact.root();
    flow400_keys(top,{"schema","native_review","source_mapping_gate","dagre5_source_gate","counts","cases"});
    flow400_require(flow400_text(top.get("schema"))=="flow400_native_compact_authority_candidate/v1" &&
        flow400_text(top.get("native_review"))=="pending","Flow400 protocol authority schema");
    const auto counts=top.get("counts"); flow400_keys(counts,{"all_sources","complete_models","pending"});
    flow400_require(flow400_number(counts.get("all_sources"))==400u && flow400_number(counts.get("complete_models"))==8u &&
        flow400_number(counts.get("pending"))==392u,"Flow400 protocol authority declared counts");
    const auto cases=top.get("cases"),fixtures=docs.manifest.root().get("fixtures");
    flow400_require(cases.is_arr() && cases.size()==400u && fixtures.is_arr() && fixtures.size()==2145u,"Flow400 protocol inventory size");
    docs.compact_work.product(400u,2048u); docs.compact_work.product(32809u,4u); docs.compact_work.spend(800u);
    std::array<bool,400> seen{}; std::size_t next=0;
    std::size_t global_ordinal=0;
    fixtures.iter([&](JsonVal fixture) {
        docs.manifest_work.spend(128u);const auto global=global_ordinal++;
        const auto path=flow400_text(fixture.get("path"));docs.manifest_work.spend(path.size());
        if (!path.starts_with("flowchart/")) return;
        flow400_require(next<400u,"Flow400 protocol extra original");
        const auto row=cases.at(next);
        flow400_keys(row,{"ordinal","manifest_global_ordinal","fixture","source_bytes","source_sha256","source_git_blob","oracle_record_sha256","model_json","model_sha256","adaptation"});
        const auto index=flow400_number(row.get("ordinal"));
        flow400_require(index==next && !seen[index] && flow400_number(row.get("manifest_global_ordinal"))==global,"Flow400 protocol ordinal order/duplicate");
        seen[index]=true;
        flow400_require(flow400_text(row.get("fixture"))==path && path.ends_with(".mmd") && path.find("..") == std::string_view::npos,
            "Flow400 protocol original path");
        flow400_require(flow400_number(row.get("source_bytes"))==flow400_number(fixture.get("bytes")) &&
            flow400_text(row.get("source_sha256"))==flow400_text(fixture.get("sha256")) &&
            flow400_text(row.get("source_git_blob"))==flow400_text(fixture.get("git_blob")),"Flow400 protocol original tuple");
        flow400_require(flow400_text(row.get("oracle_record_sha256")).size()==64u,"Flow400 protocol DB provenance");
        ++next;
    });
    flow400_require(global_ordinal==2145u,"Flow400 protocol incomplete manifest visit");
    flow400_require(next==400u && std::ranges::all_of(seen,[](bool v){return v;}),"Flow400 protocol incomplete originals");
    return docs;
}
inline void flow400_bind(JsonVal row,JsonVal fixture,std::size_t ordinal,std::size_t global,std::string_view source,OracleWork& work) {
    const auto path=flow400_text(fixture.get("path")); work.spend(2048u+4u*path.size());
    flow400_keys(row,{"ordinal","manifest_global_ordinal","fixture","source_bytes","source_sha256","source_git_blob","oracle_record_sha256","model_json","model_sha256","adaptation"});
    flow400_require(flow400_number(row.get("ordinal"))==ordinal && flow400_number(row.get("manifest_global_ordinal"))==global,
        "Flow400 protocol source ordinal");
    flow400_require(flow400_text(row.get("fixture"))==path && flow400_number(row.get("source_bytes"))==source.size() &&
        flow400_number(fixture.get("bytes"))==source.size(),"Flow400 protocol complete source bytes");
    work.product(source.size(),4u); work.spend(256u);
    flow400_require(digest(source)==flow400_text(row.get("source_sha256")) &&
        flow400_text(row.get("source_sha256"))==flow400_text(fixture.get("sha256")) &&
        flow400_text(row.get("source_git_blob"))==flow400_text(fixture.get("git_blob")),"Flow400 protocol complete source identity");
}
inline void flow400_shape(JsonVal value,std::size_t& groups,std::size_t& members,bool typed_shapes=false,bool classic_shapes=false) {
    flow400_keys(value,{"direction","nodes","edges","subgraphs"});
    const auto direction=flow400_text(value.get("direction"));
    flow400_require(direction=="TB" || direction=="TD" || direction=="BT" || direction=="LR" || direction=="RL","Flow400 protocol Model direction");
    const auto nodes=value.get("nodes"),edges=value.get("edges"),subgraphs=value.get("subgraphs");
    flow400_require(nodes.is_arr() && nodes.size()>0u && nodes.size()<=64u && edges.is_arr() && edges.size()<=128u && subgraphs.is_arr(),
        "Flow400 protocol Model arrays");
    nodes.iter([&](JsonVal pair) {
        flow400_require(pair.is_arr() && pair.size()==2u,"Flow400 protocol node tuple"); const auto node=pair.at(1);
        flow400_keys(node,{"id","label","shape"});
        const auto shape=flow400_text(node.get("shape"));
        const bool typed_closed=shape=="rectangle" || shape=="rounded" || shape=="diamond";
        const bool classic_closed=shape=="stadium" || shape=="circle" || shape=="subroutine" ||
            shape=="double_circle" || shape=="hexagon" || shape=="cylinder" || shape=="asymmetric" ||
            shape=="trapezoid" || shape=="trapezoid_alt" || shape=="parallelogram" || shape=="parallelogram_alt";
        const bool allowed=classic_shapes ? (typed_closed || classic_closed) : (typed_shapes ? typed_closed : shape=="rectangle");
        flow400_require(flow400_text(pair.at(0))==flow400_text(node.get("id")) && !flow400_text(node.get("id")).empty() &&
            !flow400_text(node.get("label")).empty() && allowed,
            classic_shapes?"Flow classic Model closed shape":(typed_shapes?"Flow typed Model closed shape":"Flow400 protocol literal rectangle"));
    });
    edges.iter([&](JsonVal edge) {
        flow400_keys(edge,{"source","target","label","style","hasArrowStart","hasArrowEnd"});
        static_cast<void>(flow400_text(edge.get("source"))); static_cast<void>(flow400_text(edge.get("target")));
        static_cast<void>(flow400_text(edge.get("label"))); const auto style=flow400_text(edge.get("style"));
        flow400_require(style=="solid" || style=="dotted" || style=="thick","Flow400 protocol edge style");
        flow400_require(edge.get("hasArrowStart").is_bool() && !edge.get("hasArrowStart").as_bool() &&
            edge.get("hasArrowEnd").is_bool() && edge.get("hasArrowEnd").as_bool(),"Flow400 protocol unsupported marker profile");
    });
    auto walk=[&](auto&& self,JsonVal array,std::size_t depth)->void {
        flow400_require(array.is_arr() && depth<=64u,"Flow400 protocol group depth");
        array.iter([&](JsonVal group) {
            flow400_require(++groups<=64u,"Flow400 protocol group count");
            flow400_keys(group,{"id","label","direction","nodeIds","children"});
            static_cast<void>(flow400_text(group.get("id"))); static_cast<void>(flow400_text(group.get("label")));
            const auto dir=flow400_text(group.get("direction"));
            flow400_require(dir.empty() || dir=="TB" || dir=="TD" || dir=="BT" || dir=="LR" || dir=="RL","Flow400 protocol group direction");
            const auto ids=group.get("nodeIds");flow400_require(ids.is_arr(),"Flow400 protocol group membership array");
            ids.iter([&](JsonVal id){static_cast<void>(flow400_text(id));++members;});self(self,group.get("children"),depth+1u);
        });
    }; walk(walk,subgraphs,0u);
}
inline std::optional<Model> flow400_model(JsonVal row,OracleWork& work) {
    const auto text=row.get("model_json"),hash=row.get("model_sha256"),adapt=row.get("adaptation");
    work.spend(1024u);
    if (text.is_null()) {flow400_require(hash.is_null() && adapt.is_null(),"Flow400 protocol null expectation");return {};}
    const auto bytes=flow400_text(text),expected=flow400_text(hash),kind=flow400_text(adapt);
    flow400_require(bytes.size()<=1435u && expected.size()==64u && (kind=="plain_literal_flow" || kind=="root_layout_dagre_native_orthogonal"),
        "Flow400 protocol finite Model identity");
    work.product(bytes.size(),3u);work.spend(4u*sizeof(json::JsonDoc)+8192u);
    flow400_require(digest(bytes)==expected,"Flow400 protocol Model text SHA");
    auto document=json::parse(bytes);flow400_require(document.has_value(),"Flow400 protocol Model JSON");
    work.product(bytes.size(),32u);
    const auto value=document->root();std::size_t groups=0,members=0;flow400_shape(value,groups,members);
    const auto n=value.get("nodes").size(),e=value.get("edges").size();
    // Exact source text bounds every contained payload; endpoint scans are paid separately.
    const auto M=bytes.size();work.spend(4u*sizeof(Model));work.product(M,32u);
    work.product(8u*n+8u,sizeof(Node)+8u);work.product(8u*e+8u,sizeof(Edge)+8u);work.product(8u*groups+8u,sizeof(Group)+8u);
    work.product((2u*e+members)*n,2u*M+8u);
    return parse_model(value);
}

// The typed-shape control asset binds complete Models directly.  It deliberately
// shares Flow400's strict object/edge/group codec and only widens the three
// approved node wire tokens for this synthetic, zero-credit test family.
inline Model flow_typed_shape_model(JsonVal value,OracleWork& work) {
    work.spend(4096u+8u*sizeof(Model)+16u*sizeof(Node)+16u*sizeof(Edge)+16u*sizeof(Group));
    std::size_t groups=0,members=0; flow400_shape(value,groups,members,true);
    const auto n=value.get("nodes").size(),e=value.get("edges").size();
    work.product(8u*n+8u,sizeof(Node)+16u);
    work.product(8u*e+8u,sizeof(Edge)+16u);
    work.product(8u*groups+8u,sizeof(Group)+16u);
    work.product((2u*e+members)*n,16u+4u*sizeof(std::size_t));
    return parse_model(value);
}

// This closed test-private classic family reuses the same strict Flow400
// object, edge, and recursive-group codec. It widens only its listed node
// wire values; legacy Flow400 and the three-shape controls retain their paths.
inline Model flow_classic_shape_model(JsonVal value,OracleWork& work) {
    work.spend(8192u+12u*sizeof(Model)+24u*sizeof(Node)+24u*sizeof(Edge)+24u*sizeof(Group));
    std::size_t groups=0,members=0; flow400_shape(value,groups,members,true,true);
    const auto n=value.get("nodes").size(),e=value.get("edges").size();
    work.product(12u*n+12u,sizeof(Node)+24u);
    work.product(12u*e+12u,sizeof(Edge)+24u);
    work.product(12u*groups+12u,sizeof(Group)+24u);
    work.product((2u*e+members)*n,24u+6u*sizeof(std::size_t));
    return parse_model(value);
}

// Test-private inline helpers shared by Native typed-shape controls.
// Native callers retain their original following token. Other owned consumers
// pass their own token through both the good baseline and mutated geometry.
inline void flow_typed_expect_rejected(const Grid& good,const Model& model,OracleWork& work,auto&& mutate,std::string_view name,std::string_view token=kSentinel) {
    const auto baseline=flow400_geometry(good,model,work,token);
    flow400_require(baseline.empty(),"Flow typed physical negative requires a complete good baseline");
    flow400_copy_grid(good,work); auto bad=good; work.spend(2048u); mutate(bad);
    const auto errors=flow400_geometry(bad,model,work,token);
    flow400_require(!errors.empty(),"Flow typed physical negative admitted");
    for(const auto& error:errors) flow400_require(!flow400_resource(error),"Flow typed physical negative became resource credit");
    static_cast<void>(name);
}

inline void flow_typed_expect_model_rejected(const Grid& good,const Model& model,OracleWork& work,auto&& mutate,std::string_view name,std::string_view token=kSentinel) {
    const auto baseline=flow400_geometry(good,model,work,token);
    flow400_require(baseline.empty(),"Flow typed semantic negative requires a complete good baseline");
    flow400_copy_model(model,work); auto bad=model; work.spend(2048u); mutate(bad);
    const auto errors=flow400_geometry(good,bad,work,token);
    flow400_require(!errors.empty(),"Flow typed semantic negative admitted");
    for(const auto& error:errors) flow400_require(!flow400_resource(error),"Flow typed semantic negative became resource credit");
    static_cast<void>(name);
}

inline std::vector<Label> flow_typed_caption_blocks(const Grid& grid,std::string_view text,OracleWork& work) {
    work.spend(64u*sizeof(Label)+16u*sizeof(std::string)+4096u); work.product(text.size(),8u);
    return label_blocks(grid,text,{-1,-1,grid.width,grid.height},work);
}

inline void flow_typed_erase_label(Grid& grid,const Label& block,std::string_view text,OracleWork& work) {
    work.spend(16u*sizeof(std::string)+256u); work.product(text.size(),8u);
    const auto lines=split_lines(text);
    flow400_require(lines.size()==block.starts.size(),"Flow typed caption block line count");
    for(std::size_t row=0;row<lines.size();++row) {
        work.spend(8u*sizeof(std::string)+128u); work.product(lines[row].size(),4u);
        const auto glyphs=ftxui::Utf8ToGlyphs(lines[row]);
        for(std::size_t column=0;column<glyphs.size();++column) {
            const Point point{block.starts[row].x+static_cast<int>(column),block.starts[row].y};
            flow400_require(grid.has(point),"Flow typed caption erase bounds");
            work.spend(16u); grid.put(point," ");
        }
    }
}

inline void flow_typed_write_label(Grid& grid,const Label& block,std::string_view original,std::string_view replacement,OracleWork& work) {
    work.spend(32u*sizeof(std::string)+512u); work.product(original.size(),8u); work.product(replacement.size(),8u);
    const auto old_lines=split_lines(original),new_lines=split_lines(replacement);
    flow400_require(old_lines.size()==new_lines.size()&&new_lines.size()==block.starts.size(),"Flow typed equal-line caption replacement");
    for(std::size_t row=0;row<new_lines.size();++row) {
        work.spend(16u*sizeof(std::string)+256u); work.product(old_lines[row].size(),4u); work.product(new_lines[row].size(),4u);
        const auto old_glyphs=ftxui::Utf8ToGlyphs(old_lines[row]),new_glyphs=ftxui::Utf8ToGlyphs(new_lines[row]);
        flow400_require(old_glyphs.size()==new_glyphs.size(),"Flow typed equal-width physical caption replacement");
        for(std::size_t column=0;column<new_glyphs.size();++column) {
            const Point point{block.starts[row].x+static_cast<int>(column),block.starts[row].y};
            flow400_require(grid.has(point),"Flow typed caption replacement bounds");
            work.spend(16u); grid.put(point,new_glyphs[column]);
        }
    }
}

inline std::optional<Point> flow_typed_orphan_label_start(const Grid& grid,std::string_view text,const std::vector<ShapeFrame>& frames,OracleWork& work) {
    work.spend(16u*sizeof(std::string)+256u); work.product(text.size(),8u);
    const auto lines=split_lines(text);
    if(lines.empty()) return std::nullopt;
    int width=0;
    for(const auto& line:lines) width=std::max(width,ftxui::string_width(line));
    if(width<=0) return std::nullopt;
    for(int y=1;y+static_cast<int>(lines.size())<grid.height-1;++y) for(int x=1;x+width<grid.width-1;++x) {
        bool blank=true;
        for(int yy=y-1;blank&&yy<=y+static_cast<int>(lines.size());++yy) for(int xx=x-1;blank&&xx<=x+width;++xx) {
            const Point point{xx,yy}; work.spend();
            if(!grid.has(point)||grid.at(point)!=" "||std::ranges::any_of(frames,[&](const ShapeFrame& frame){return frame.contains(point);})) blank=false;
        }
        if(blank) return Point{x,y};
    }
    return std::nullopt;
}

inline void flow_typed_write_at(Grid& grid,Point start,std::string_view text,OracleWork& work) {
    work.spend(16u*sizeof(std::string)+256u); work.product(text.size(),8u);
    const auto lines=split_lines(text);
    for(std::size_t row=0;row<lines.size();++row) {
        work.spend(8u*sizeof(std::string)+128u); work.product(lines[row].size(),4u);
        const auto glyphs=ftxui::Utf8ToGlyphs(lines[row]);
        for(std::size_t column=0;column<glyphs.size();++column) {
            const Point point{start.x+static_cast<int>(column),start.y+static_cast<int>(row)};
            flow400_require(grid.has(point),"Flow typed orphan caption write bounds");
            work.spend(16u); grid.put(point,glyphs[column]);
        }
    }
}

inline std::optional<ShapeFrame> flow_typed_frame_for_node(const Grid& grid,const std::vector<ShapeFrame>& frames,const Node& node,OracleWork& work) {
    work.spend(8u*sizeof(int)+512u);
    auto candidates=frame_candidates(grid,frames,node.label,node.shape,work);
    if(candidates.size()!=1u) return std::nullopt;
    return frames[static_cast<std::size_t>(candidates.front())];
}

} // namespace loom::test::mermaid_flow
