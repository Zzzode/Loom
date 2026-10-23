// Test-private, source-bound first-red controls for the closed classic-shape slice.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iterator>

import std;
import loom.ui.visual.markdown;
import loom.serdes.json;
import loom.crypto.crypto;

#include "mermaid_flow400_authority.hpp"

namespace {
namespace json = loom::utils::json;
using json::JsonVal;
namespace flow_oracle = loom::test::mermaid_flow;
using namespace flow_oracle;

constexpr std::string_view kClassicAssetSha="163ec12327a10744bb0f593964248eadea85685f3bebfa59a6e852700c142922";
constexpr std::size_t kClassicAssetBytes=7178u;

std::string fenced(std::string_view source,std::string_view language) {
    return "```"+std::string(language)+"\n"+std::string(source)+"\n```\n"+std::string(kSentinel);
}
std::size_t publication_payload(const Grid& grid,OracleWork& work) {
    work.spend(2048u); work.product(grid.cells.size(),8u); std::size_t bytes=0u;
    for (const auto& cell:grid.cells) {
        flow400_require(cell.size()<=std::numeric_limits<std::size_t>::max()-bytes,"Classic profile payload overflow");
        bytes+=cell.size();
    }
    return bytes;
}
void publication(const Grid& grid,OracleWork& work) {
    const auto bytes=publication_payload(grid,work),rows=static_cast<std::size_t>(grid.height);
    work.product(bytes,48u); work.product(3u*rows+32u,8u);
    work.product(rows,3u*sizeof(std::string)+4u*sizeof(json::JsonMutVal)+64u);
    work.spend(4u*sizeof(json::JsonDoc)+512u);
}
void append_frame(json::JsonMutVal row,std::string_view name,const Grid& grid) {
    auto lines=row.make_arr();
    for (int y=0;y<grid.height;++y) lines.append(row.make_str(grid.row(y)));
    row.add(name,lines);
}
std::vector<ShapeFrame> classic_contours(const Grid& grid,OracleWork& work) {
    auto frames=closed_frames(grid,work); auto extra=classic_closed_frames(grid,work);
    if (frames.size()+extra.size()>128u) throw std::runtime_error("Too many classic control contours");
    frames.insert(frames.end(),extra.begin(),extra.end());
    std::ranges::sort(frames,[](const ShapeFrame& a,const ShapeFrame& b) { return a.area()<b.area(); });
    return frames;
}
std::optional<std::tuple<Point,int,Point,Point>> blank_envelope_arrow(const Grid& grid,const ShapeFrame& frame) {
    for (int y=frame.top;y<=frame.bottom;++y) for (int x=frame.left;x<=frame.right;++x) {
        const Point target{x,y};
        if (frame.contains(target) || classic_decoration(frame,target) || grid.at(target)!=" ") continue;
        for (int direction=0;direction<4;++direction) {
            const auto delta=kSteps[static_cast<std::size_t>(direction)];
            const Point head{target.x-delta.x,target.y-delta.y};
            const Point shaft{target.x-2*delta.x,target.y-2*delta.y};
            if (grid.has(head)&&grid.has(shaft)&&grid.at(head)==" "&&grid.at(shaft)==" ")
                return std::tuple{target,direction,head,shaft};
        }
    }
    return {};
}
std::string_view rotated_port_glyph(int outward) {
    if (outward==U) return "┬";
    if (outward==D) return "┴";
    if (outward==L) return "├";
    if (outward==R) return "┤";
    throw std::runtime_error("Classic profile invalid cardinal normal");
}
std::string_view port_glyph(int outward) {
    if (outward==U) return "┴";
    if (outward==D) return "┬";
    if (outward==L) return "┤";
    if (outward==R) return "├";
    throw std::runtime_error("Classic profile invalid cardinal normal");
}

struct TangentPortScene {
    Grid grid;
    Model model;
    Point port;
    std::string_view restored_border;
};

TangentPortScene tangent_port_scene(Shape shape,int outward) {
    constexpr int kWidth=48,kHeight=8,kTop=1,kBottom=5,kTargetWidth=9;
    Grid grid{kWidth,kHeight,std::vector<std::string>(static_cast<std::size_t>(kWidth*kHeight)," ")};
    const auto profile=classic_profile_frame(shape);
    const int source_width=profile.right-profile.left+1;
    const bool rightward=outward==R;
    const int source_left=rightward?2:kWidth-source_width-2;
    ShapeFrame source{{source_left,kTop,source_left+source_width-1,kBottom},shape};
    flow400_require(source.left>=0 && source.right<kWidth,"Tangent hand source outside grid");
    const int target_left=rightward?28:2;
    ShapeFrame target{{target_left,kTop,target_left+kTargetWidth-1,kBottom},Shape::Rectangle};
    for (int y=source.top;y<=source.bottom;++y) for (int x=source.left;x<=source.right;++x)
        if (const auto cell=classic_expected_cell(source,{x,y})) grid.put({x,y},std::string(*cell));
    const Point source_label{(source.left+source.right)/2,(source.top+source.bottom)/2};
    grid.put(source_label,"X");
    for (int x=target.left+1;x<target.right;++x) { grid.put({x,target.top},"─"); grid.put({x,target.bottom},"─"); }
    for (int y=target.top+1;y<target.bottom;++y) { grid.put({target.left,y},"│"); grid.put({target.right,y},"│"); }
    grid.put({target.left,target.top},"┌"); grid.put({target.right,target.top},"┐");
    grid.put({target.left,target.bottom},"└"); grid.put({target.right,target.bottom},"┘");
    grid.put({(target.left+target.right)/2,(target.top+target.bottom)/2},"Y");
    const int middle=(source.top+source.bottom)/2;
    const Point port{rightward?source.right:source.left,middle};
    const std::string_view restored_border=shape==Shape::Asymmetric?"◁":(rightward?")":"(");
    grid.put(port,std::string(port_glyph(outward)));
    if (rightward) {
        for (int x=source.right+1;x<target.left-1;++x) grid.put({x,middle},"─");
        grid.put({target.left-1,middle},"▶");
    } else {
        for (int x=target.right+2;x<source.left;++x) grid.put({x,middle},"─");
        grid.put({target.right+1,middle},"◀");
    }
    for (std::size_t n=0;n<kSentinel.size();++n) grid.put({static_cast<int>(n),kHeight-1},std::string(1,kSentinel[n]));
    return {std::move(grid),{rightward?"LR":"RL",{{"source","X",shape},{"target","Y",Shape::Rectangle}},{{0,1,"","solid",false,true}},{}},port,restored_border};
}

OracleWork tangent_port_work() {
    OracleWork work{kFlow400SceneLimit};
    work.spend(65536u+16u*sizeof(Grid)+16u*sizeof(Model)+64u*sizeof(ShapeFrame)+512u*sizeof(std::string));
    return work;
}

void expect_non_resource_rejection(const std::vector<std::string>& errors,std::string_view expected) {
    ASSERT_EQ(errors.size(),1u) << errors_text(errors);
    EXPECT_EQ(errors.front(),expected);
    EXPECT_FALSE(flow400_resource(errors.front()));
}

TEST(MermaidFlowClassicShapeProfiles, GridWritesRejectCoordinatesThatWouldWrapIntoAnotherRow) {
    Grid grid{2,2,std::vector<std::string>(4," ")};
    EXPECT_THROW(grid.put({2,0},"X"),std::runtime_error);
    EXPECT_THROW(grid.put({-1,1},"X"),std::runtime_error);
    EXPECT_THROW(grid.put({0,2},"X"),std::runtime_error);
    for (const auto& cell:grid.cells) EXPECT_EQ(cell," ");
    grid.put({1,1},"Y");
    EXPECT_EQ(grid.at({1,1}),"Y");
}

TEST(MermaidFlowClassicShapeProfiles, EvenWidthSubroutineUsesTheProductionVerticalMidpoint) {
    constexpr ShapeFrame frame{{2,1,19,5},Shape::Subroutine};
    constexpr int kWidth=24,kHeight=8;
    Grid grid{kWidth,kHeight,std::vector<std::string>(static_cast<std::size_t>(kWidth*kHeight)," ")};
    for (int y=frame.top;y<=frame.bottom;++y) for (int x=frame.left;x<=frame.right;++x)
        if (const auto cell=classic_expected_cell(frame,{x,y})) grid.put({x,y},std::string(*cell));
    constexpr std::string_view label="Subroutine";
    for (std::size_t n=0;n<label.size();++n) grid.put({6+static_cast<int>(n),3},std::string(1,label[n]));
    // Production source_port writes L|R|D at the right-middle bottom cell of
    // this 18-column Box. It must remain an exact profile junction.
    const Point top_port{11,1};
    const Point bottom_port{11,5};
    grid.put(bottom_port,"┬");
    for (std::size_t n=0;n<kSentinel.size();++n) grid.put({static_cast<int>(n),kHeight-1},std::string(1,kSentinel[n]));

    const auto ports=classic_declared_ports(frame);
    ASSERT_EQ(ports.size,4u);
    EXPECT_EQ(ports.values[0].point,top_port); EXPECT_EQ(ports.values[0].outward,U);
    EXPECT_EQ(ports.values[1].point,bottom_port); EXPECT_EQ(ports.values[1].outward,D);
    EXPECT_TRUE(frame.legal_port(bottom_port,D)); EXPECT_FALSE(frame.legal_port({10,5},D));
    EXPECT_TRUE(frame.accepts_port(grid,bottom_port,D)); EXPECT_TRUE(classic_exact_frame(grid,frame));

    OracleWork work{kFlow400SceneLimit};
    work.spend(65536u+16u*sizeof(Grid)+32u*sizeof(ShapeFrame));
    const auto frames=classic_closed_frames(grid,work);
    EXPECT_TRUE(std::ranges::find(frames,frame)!=frames.end());
}

TEST(MermaidFlowClassicShapeProfiles, StadiumAndAsymmetricTangentPortsRequireOneActualSourceArm) {
    struct Case { Shape shape; int outward; std::string_view restored_border; };
    constexpr std::array<Case,3> cases{{
        {Shape::Stadium,L,"("}, {Shape::Stadium,R,")"}, {Shape::Asymmetric,L,"◁"},
    }};
    for (const auto item:cases) {
        auto scene=tangent_port_scene(item.shape,item.outward);
        std::string diagnostic;
        for (int y=0;y<scene.grid.height;++y) diagnostic+=scene.grid.row(y)+"\n";
        SCOPED_TRACE(diagnostic);
        ASSERT_EQ(scene.restored_border,item.restored_border);
        auto good_work=tangent_port_work();
        const auto good=flow400_geometry(scene.grid,scene.model,good_work);
        EXPECT_TRUE(good.empty()) << "shape=" << static_cast<int>(item.shape) << " outward=" << item.outward << " " << errors_text(good);

        auto missing_work=tangent_port_work();
        flow400_copy_grid(scene.grid,missing_work); auto missing=scene.grid;
        missing.put(scene.port,std::string(scene.restored_border));
        expect_non_resource_rejection(flow400_geometry(missing,scene.model,missing_work),"Arrow has no connected authored source port");

        auto extra_work=tangent_port_work();
        flow400_copy_grid(scene.grid,extra_work); auto extra=scene.grid;
        extra.put(scene.port,"┼");
        expect_non_resource_rejection(flow400_geometry(extra,scene.model,extra_work),"Unclaimed arm on a node/group perimeter");
    }
}

TEST(MermaidFlowClassicShapeProfiles, HandFramesUseOnlyTheirDeclaredPortsAndDecorations) {
    constexpr std::array<Shape,11> shapes{{Shape::Stadium,Shape::Circle,Shape::Subroutine,Shape::DoubleCircle,Shape::Hexagon,
        Shape::Cylinder,Shape::Asymmetric,Shape::Trapezoid,Shape::TrapezoidAlt,Shape::Parallelogram,Shape::ParallelogramAlt}};
    for (const auto shape:shapes) {
        OracleWork work{kFlow400SceneLimit};
        work.spend(65536u+32u*sizeof(Grid)+32u*sizeof(ShapeFrame)+256u*sizeof(json::JsonMutVal));
        const auto grid=classic_profile_good_grid(shape);
        const auto frame=classic_profile_frame(shape);
        ASSERT_TRUE(classic_exact_frame(grid,frame));
        const auto contours=classic_contours(grid,work);
        ASSERT_TRUE(std::ranges::find(contours,frame)!=contours.end());
        const Model model{"TB",{{"n","X",shape}},{},{}};
        const auto geometry=flow400_geometry(grid,model,work);
        EXPECT_TRUE(geometry.empty()) << "shape=" << static_cast<int>(shape) << " " << errors_text(geometry);
        const auto ports=classic_declared_ports(frame);
        EXPECT_EQ(ports.size,shape==Shape::Asymmetric?2u:4u);
        for (std::size_t n=0;n<ports.size;++n) {
            EXPECT_TRUE(frame.legal_port(ports.values[n].point,ports.values[n].outward));
            const auto& port=ports.values[n];
            flow400_copy_grid(grid,work); auto armed=grid;
            armed.put(port.point,std::string(port_glyph(port.outward)));
            EXPECT_TRUE(frame.accepts_port(armed,port.point,port.outward));
            flow400_copy_grid(grid,work); auto rotated=grid;
            rotated.put(port.point,std::string(rotated_port_glyph(port.outward)));
            EXPECT_FALSE(frame.accepts_port(rotated,port.point,port.outward));
        }
        const auto expect_cell=[&](Point point,std::string_view expected) {
            const auto cell=classic_outer_cell(frame,point);
            ASSERT_TRUE(cell.has_value()); EXPECT_EQ(*cell,expected);
        };
        const auto expect_not_cell=[&](Point point) {
            EXPECT_FALSE(classic_outer_cell(frame,point).has_value());
        };
        const int middle=(frame.top+frame.bottom)/2;
        if (shape==Shape::Circle || shape==Shape::DoubleCircle) {
            expect_cell({frame.left+2,frame.bottom},"╰"); expect_cell({frame.right-2,frame.bottom},"╯");
            expect_cell({frame.left+1,frame.bottom-1},"╰"); expect_cell({frame.right-1,frame.bottom-1},"╯");
            expect_cell({frame.left,frame.bottom-2},"│"); expect_cell({frame.right,frame.bottom-2},"│");
        }
        if (shape==Shape::Trapezoid) {
            expect_cell({frame.left+1,middle},"│"); expect_cell({frame.right-1,middle},"│");
            expect_cell({frame.left,frame.bottom-1},"│"); expect_cell({frame.right,frame.bottom-1},"│");
            expect_not_cell({frame.left+1,frame.bottom-1}); expect_not_cell({frame.right-1,frame.bottom-1});
        }
        if (shape==Shape::TrapezoidAlt) {
            expect_cell({frame.left+1,middle},"│"); expect_cell({frame.right-1,middle},"│");
            expect_cell({frame.left,frame.top+1},"│"); expect_cell({frame.right,frame.top+1},"│");
            expect_not_cell({frame.left,frame.bottom-1}); expect_not_cell({frame.right,frame.bottom-1});
        }
        if (shape==Shape::Parallelogram || shape==Shape::ParallelogramAlt) {
            expect_cell({frame.left+1,middle},"│"); expect_cell({frame.right-1,middle},"│");
            EXPECT_EQ(ports.values[2].point.x,frame.left+1); EXPECT_EQ(ports.values[2].point.y,middle);
            EXPECT_EQ(ports.values[3].point.x,frame.right-1); EXPECT_EQ(ports.values[3].point.y,middle);
            if (shape==Shape::Parallelogram) {
                expect_cell({frame.left+1,frame.top+1},"╱"); expect_cell({frame.right-1,frame.top+1},"╱");
                EXPECT_FALSE(frame.legal_port({frame.right,middle},R));
            } else {
                expect_cell({frame.left,frame.top+1},"╲"); expect_cell({frame.right-2,frame.top+1},"╲");
                EXPECT_FALSE(frame.legal_port({frame.left,middle},L));
            }
        }
        if (shape==Shape::DoubleCircle || shape==Shape::Subroutine) {
            bool decoration=false;
            for (int y=frame.top;y<=frame.bottom;++y) for (int x=frame.left;x<=frame.right;++x) {
                const Point point{x,y}; if (!classic_decoration(frame,point)) continue;
                decoration=true;
                EXPECT_FALSE(frame.legal_port(point,U)); EXPECT_FALSE(frame.legal_port(point,R));
                EXPECT_FALSE(frame.legal_port(point,D)); EXPECT_FALSE(frame.legal_port(point,L));
                if (shape==Shape::DoubleCircle) {
                    EXPECT_EQ(frame.perimeter_bits(point),glyph(grid.at(point)).bits);
                    EXPECT_NE(frame.perimeter_bits(point),0);
                }
            }
            EXPECT_TRUE(decoration);
        }
        if (shape==Shape::DoubleCircle) {
            // The exact inner ring is decorative only: it must not become a
            // second authored Circle merely because it encloses the same label.
            OracleWork forged_work{kFlow400SceneLimit};
            forged_work.spend(65536u+32u*sizeof(Grid)+32u*sizeof(ShapeFrame)+256u*sizeof(json::JsonMutVal));
            const Model forged{"TB",{{"outer","X",Shape::DoubleCircle},{"inner","X",Shape::Circle}},{},{}};
            const auto forged_errors=flow400_geometry(grid,forged,forged_work);
            ASSERT_EQ(forged_errors.size(),1u);
            EXPECT_EQ(forged_errors.front(),"Missing exact typed contour with complete direct multiline label for entity 1");
            EXPECT_FALSE(flow400_resource(forged_errors.front()));
        }
    }
}

TEST(MermaidFlowClassicShapes, CompleteTerminalSourcesRequireFullGeometryAndExactGoodFirstMutants) {
    OracleWork setup{kFlow400SceneLimit};
    setup.spend(65536u+16u*sizeof(json::JsonDoc)+2048u*sizeof(json::JsonMutVal));
    const auto asset_path=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_native/flow_classic_shapes_terminal_sources.v2.json";
    flow400_require(std::filesystem::file_size(asset_path)==kClassicAssetBytes,"Classic source asset byte bound before read");
    const auto raw=flow400_read(asset_path);
    flow400_require(raw.size()==kClassicAssetBytes&&digest(raw)==kClassicAssetSha,"Classic source asset SHA");
    auto asset=json::parse(raw); ASSERT_TRUE(asset.has_value()); const auto root=asset->root();
    flow400_keys(root,{"schema","qualification","source_design_r1_payload_sha256","source_design_review_raw_sha256","cases"});
    flow400_require(flow400_text(root.get("schema"))=="flow_classic_shapes_terminal_native_asset/v2","Classic source asset schema");
    flow400_require(flow400_text(root.get("source_design_r1_payload_sha256"))=="86e64b9c1e53693009647f548bb845f90911eb2e22a2008b3ae49b3b419ff437","Classic source design payload pin");
    flow400_require(flow400_text(root.get("source_design_review_raw_sha256"))=="7c4ba7c73f26ac2d22b895c286f74119cca6a45b9cdd4a0bd6bffd576917a6eb","Classic source design review pin");
    flow400_require(root.get("cases").is_arr()&&root.get("cases").size()==3u,"Classic source exact cohort cardinality");

    json::JsonMutDoc report; auto report_root=report.object(),rows=report_root.make_arr();
    std::size_t visited=0u,complete=0u,resources=0u,semantic_total=0u,physical_total=0u;
    root.get("cases").iter([&](JsonVal item) {
        OracleWork work{kFlow400SceneLimit};
        // Every captured grid, publication payload, profile scan, model copy,
        // mutation copy and bounded report field is charged before capture.
        work.spend(524288u+64u*sizeof(Grid)+64u*sizeof(Model)+256u*sizeof(ShapeFrame)+256u*sizeof(Label)+
                   2048u*sizeof(json::JsonMutVal)+2048u*sizeof(std::string));
        flow400_keys(item,{"case_id","vendor","source_path","source","source_bytes","source_sha256","snapshot_path",
            "upstream_model_canonical_sha256","proposed_test_private_model_json","proposed_test_private_model_sha256",
            "semantic_mutations","physical_mutations","total_mutations","new_shapes"});
        const auto id=flow400_text(item.get("case_id")),source=flow400_text(item.get("source"));
        const auto expected_semantic=flow400_number(item.get("semantic_mutations"));
        const auto expected_physical=flow400_number(item.get("physical_mutations"));
        const auto expected_total=flow400_number(item.get("total_mutations"));
        auto row=report_root.make_obj(); row.set("case_id",id); row.set("source_sha256",flow400_text(item.get("source_sha256")));
        row.set("model_sha256",flow400_text(item.get("proposed_test_private_model_sha256"))); row.set("expected_semantic_mutations",static_cast<int64_t>(expected_semantic));
        row.set("expected_physical_mutations",static_cast<int64_t>(expected_physical));
        std::array<std::optional<Grid>,5> frames; std::array<char,513> error{};
        constexpr std::array<int,5> widths{{240,40,240,240,40}};
        constexpr std::array<std::string_view,5> names{{"wide","narrow","restored","ordinary_wide","ordinary_narrow"}};
        bool source_wide=true,source_narrow=true,restored=false,decoded=false,admitted=false,resource=false;
        std::size_t published=0u,semantic_rejected=0u,physical_rejected=0u; const char* phase="capture";
        try {
            const auto source_path=std::filesystem::path(LOOM_TESTS_DIR).parent_path()/std::string(flow400_text(item.get("source_path")));
            flow400_require(std::filesystem::file_size(source_path)==flow400_number(item.get("source_bytes")),"Classic source byte bound before read");
            const auto original=flow400_read(source_path);
            flow400_require(original==source&&original.size()==flow400_number(item.get("source_bytes"))&&digest(original)==flow400_text(item.get("source_sha256")),"Classic complete raw source identity");
            work.product(source.size(),32u); auto element=loom::ui::render_markdown(fenced(source,"mermaid"));
            auto ordinary=loom::ui::render_markdown(fenced(source,"text")); element->ComputeRequirement(); ordinary->ComputeRequirement();
            const int height=std::max({32,element->requirement().min_y+8,ordinary->requirement().min_y+8});
            flow400_require(height<=1040,"Classic source capture height");
            for (std::size_t n=0;n<frames.size();++n) {
                frames[n]=capture(n<3u?element:ordinary,widths[n],height,&work);
                publication(*frames[n],work); append_frame(row,names[n],*frames[n]); ++published;
            }
            flow400_require(published==5u,"Classic five public frames attached before source binding");
            phase="bind"; work.product(source.size(),4u);
            flow400_require(element->requirement().min_x<=240,"Classic source exceeds the fixed 240-cell scene bound");
            for (const auto& frame:frames) static_cast<void>(flow400_following(*frame,work));
            source_wide=flow400_same(*frames[0],*frames[3],work); source_narrow=flow400_same(*frames[1],*frames[4],work);
            restored=flow400_same(*frames[0],*frames[2],work); flow400_require(restored,"Classic restored wide frame differs");
            flow400_require(!source_wide,"Classic source returned ordinary text at wide width");
            phase="model"; const auto model_bytes=flow400_text(item.get("proposed_test_private_model_json"));
            work.product(model_bytes.size(),32u); work.spend(8192u+4u*sizeof(json::JsonDoc));
            flow400_require(digest(model_bytes)==flow400_text(item.get("proposed_test_private_model_sha256")),"Classic source Model SHA");
            auto document=json::parse(model_bytes); flow400_require(document.has_value(),"Classic source Model JSON");
            const auto model=flow_classic_shape_model(document->root(),work); decoded=true;
            const auto classic_nodes=std::count_if(model.nodes.begin(),model.nodes.end(),[](const Node& node) { return classic_shape(node.shape); });
            flow400_require(static_cast<std::size_t>(classic_nodes)==flow400_number(item.get("new_shapes")),"Classic source closed-shape Model cardinality");
            flow400_require(expected_semantic==static_cast<std::size_t>(classic_nodes)+3u&&expected_physical==static_cast<std::size_t>(classic_nodes)+3u&&
                expected_total==expected_semantic+expected_physical,"Classic exact mutation catalogue");
            phase="geometry"; const auto wide_errors=flow400_geometry(*frames[0],model,work); flow400_require(wide_errors.empty(),wide_errors.empty()?"":wide_errors.front().c_str());
            if (!source_narrow) { const auto narrow_errors=flow400_geometry(*frames[1],model,work); flow400_require(narrow_errors.empty(),narrow_errors.empty()?"":narrow_errors.front().c_str()); }
            const auto restored_errors=flow400_geometry(*frames[2],model,work); flow400_require(restored_errors.empty(),restored_errors.empty()?"":restored_errors.front().c_str());
            phase="good_first_mutations"; const auto contours=classic_contours(*frames[0],work);
            std::optional<ShapeFrame> first_classic;
            for (std::size_t n=0;n<model.nodes.size();++n) if (classic_shape(model.nodes[n].shape)) {
                const auto frame=flow_typed_frame_for_node(*frames[0],contours,model.nodes[n],work);
                flow400_require(frame.has_value(),"Classic Model node lacks one exact physical profile");
                if (!first_classic) first_classic=*frame;
                flow_typed_expect_model_rejected(*frames[0],model,work,[&](Model& bad) { bad.nodes[n].shape=classic_next_shape(bad.nodes[n].shape); },"classic model wire shape swap");
                ++semantic_rejected;
                const auto marker=classic_marker_point(*frame); flow400_require(marker.has_value(),"Classic profile lacks a removable marker");
                flow_typed_expect_rejected(*frames[0],model,work,[&](Grid& bad) { bad.put(*marker," "); },"classic physical marker erase");
                ++physical_rejected;
            }
            flow400_require(!model.edges.empty(),"Classic full source lacks routed edges");
            flow_typed_expect_model_rejected(*frames[0],model,work,[&](Model& bad) { bad.edges.front().to=bad.edges.front().from; },"classic wrong endpoint"); ++semantic_rejected;
            flow_typed_expect_model_rejected(*frames[0],model,work,[&](Model& bad) { bad.edges.front().style=bad.edges.front().style=="solid"?"dotted":"solid"; },"classic wrong edge style"); ++semantic_rejected;
            flow_typed_expect_model_rejected(*frames[0],model,work,[&](Model& bad) { bad.direction=bad.direction=="LR"?"RL":(bad.direction=="TB"?"BT":"TB"); },"classic wrong rank direction"); ++semantic_rejected;
            flow400_require(first_classic.has_value(),"Classic source lacks selected profile frame");
            const auto blank=blank_envelope_arrow(*frames[0],*first_classic); flow400_require(blank.has_value(),"Classic source lacks a blank non-port envelope witness");
            flow_typed_expect_rejected(*frames[0],model,work,[&](Grid& bad) {
                const auto [target,direction,head,shaft]=*blank; static_cast<void>(target);
                constexpr std::array<std::string_view,4> arrows{{"▲","▶","▼","◀"}};
                constexpr std::array<std::string_view,4> shafts{{"│","─","│","─"}};
                bad.put(shaft,std::string(shafts[static_cast<std::size_t>(direction)])); bad.put(head,std::string(arrows[static_cast<std::size_t>(direction)]));
            },"classic blank bounding-envelope port"); ++physical_rejected;
            const auto ports=classic_declared_ports(*first_classic); flow400_require(ports.size>0u,"Classic profile lacks declared port");
            flow_typed_expect_rejected(*frames[0],model,work,[&](Grid& bad) { bad.put(ports.values[0].point,std::string(rotated_port_glyph(ports.values[0].outward))); },"classic rotated port normal"); ++physical_rejected;
            flow_typed_expect_rejected(*frames[0],model,work,[&](Grid& bad) { bad.put(ports.values[0].point,"┼"); },"classic extra port arm"); ++physical_rejected;
            flow400_require(semantic_rejected==expected_semantic&&physical_rejected==expected_physical,"Classic exact non-resource mutation count");
            admitted=true; phase="complete";
        } catch (const std::exception& failure) {
            const char* message=failure.what(); std::size_t n=0u;
            for (;message&&message[n]&&n+1u<error.size();++n) error[n]=message[n];
            resource=work.remaining==0u||flow400_resource(std::string_view(error.data(),n));
        }
        row.set("phase",phase); row.set("published_frames",static_cast<int64_t>(published)); row.set("source_wide",source_wide); row.set("source_narrow",source_narrow);
        row.set("restored_identically",restored); row.set("Model_decoded",decoded); row.set("semantic_mutations_rejected",static_cast<int64_t>(semantic_rejected));
        row.set("physical_mutations_rejected",static_cast<int64_t>(physical_rejected)); row.set("whole_source_admitted",admitted); row.set("resource_error",resource);
        row.set("error",std::string_view(error.data())); row.set("remaining",static_cast<int64_t>(work.remaining)); rows.append(row);
        ++visited; complete+=admitted; resources+=resource; semantic_total+=semantic_rejected; physical_total+=physical_rejected;
        EXPECT_EQ(published,5u)<<id; EXPECT_FALSE(resource)<<id<<": "<<error.data(); EXPECT_TRUE(admitted)<<id<<": "<<error.data();
    });
    report_root.set("schema","flow_classic_shapes_terminal_results/v1");
    report_root.set("qualification","Three full source-bound terminal inputs and a closed eleven-shape local profile only. A source fallback, a parser failure, a geometry error, an over-width scene, or exhausted work grants zero drawing, mutation, or formal-admission credit.");
    report_root.set("complete_sources",static_cast<int64_t>(complete)); report_root.set("formal_admissions",0); report_root.set("resource_errors",static_cast<int64_t>(resources));
    report_root.set("semantic_mutations_rejected",static_cast<int64_t>(semantic_total)); report_root.set("physical_mutations_rejected",static_cast<int64_t>(physical_total)); report_root.add("cases",rows); report.set_root(report_root);
#ifdef LOOM_MERMAID_REPORT_DIR
    std::ofstream output(std::filesystem::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-flow-classic-shapes-terminal.json",std::ios::binary);
    ASSERT_TRUE(output&&(output<<report.to_string()<<'\n'))<<"Cannot publish classic-shape result";
#endif
    EXPECT_EQ(visited,3u); EXPECT_EQ(complete,3u); EXPECT_EQ(resources,0u); EXPECT_EQ(semantic_total,24u); EXPECT_EQ(physical_total,24u);
}
} // namespace
