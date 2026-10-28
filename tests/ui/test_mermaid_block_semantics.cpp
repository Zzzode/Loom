// RFC 0030 B2 test-only feasibility trial. No drawing admission is published.
#include <gtest/gtest.h>
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
import std;
import loom.serdes.json;
import loom.crypto.crypto;
import loom.ui.visual.mermaid.testing;
import loom.ui.foundation.theme_provider;
#include "mermaid_block58_projection.hpp"
#include "mermaid_block58_contours.hpp"
#include "mermaid_block58_boundary_authority.hpp"

namespace {
namespace block=loom::test::mermaid_block58;
namespace plan=block::projection;
namespace contours=block::contours;
namespace json=loom::utils::json;
namespace fs=std::filesystem;
const fs::path kFixtures=fs::path(LOOM_TESTS_DIR)/"fixtures";
std::string read(const fs::path& path,std::size_t cap=block::kMaxFactBytes) {
    plan::require(fs::is_regular_file(path)&&fs::file_size(path)<=cap,"Block58 bounded fixture file");
    std::ifstream stream(path,std::ios::binary);plan::require(stream.good(),"Block58 fixture open");
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
struct Inputs {
    std::string source_bytes,terminal_bytes,manifest_bytes;
    json::JsonDoc source,terminal,manifest;
    Inputs():source_bytes(read(kFixtures/"mermaid_native/block58_complete_source_proposal.v1.json")),
        terminal_bytes(read(kFixtures/"mermaid_native/block58_terminal_projection_proposal.v1.json")),
        manifest_bytes(read(kFixtures/"mermaid_official/manifest.json")) {
        block::OracleWork work;block::verify_asset_bytes(source_bytes,terminal_bytes,work);
        plan::require(loom::utils::crypto::sha256(manifest_bytes)==block::kManifestSha,"Block58 manifest bytes");
        auto s=json::parse(source_bytes),t=json::parse(terminal_bytes),m=json::parse(manifest_bytes);
        plan::require(s&&t&&m,"Block58 fixture JSON parse");source=std::move(*s);terminal=std::move(*t);manifest=std::move(*m);
        plan::require(source&&terminal&&manifest,"Block58 fixture JSON");
    }
    block::BoundFacts bind(std::string_view path,block::OracleWork& work) {
        plan::require(std::ranges::find_if(block::kPins,[&](const auto& pin){return pin.path==path;})!=block::kPins.end(),"Block58 compiled source path before file read");
        const auto raw=read(kFixtures/"mermaid_official/cases"/std::string(path),block::kMaxRawBytes);
        return block::bind(source.root(),terminal.root(),path,raw,manifest.root(),work);
    }
    plan::Plan build(std::string_view path,block::OracleWork& work) {
        const auto facts=bind(path,work);
        return plan::build(facts.terminal_model,static_cast<std::size_t>(facts.source_case.get("source_bytes").as_int()),work,facts.source_model);
    }
};
template<class F> void rejects_without_resource(F&& fn) {
    bool rejected=false;
    try{fn();}catch(const std::exception& e){rejected=true;EXPECT_FALSE(std::string_view(e.what()).starts_with("resource:"));EXPECT_FALSE(std::string_view(e.what()).starts_with("planned-work/"));}
    EXPECT_TRUE(rejected);
}
json::JsonMutDoc copied(json::JsonVal value) {
    json::JsonMutDoc out;out.set_root(out.copy_val(value));return out;
}
void verify_current_contract(json::JsonVal terminal,std::string_view rfc,block::OracleWork& work) {
    const auto groups=terminal.get("current_rfc_contract_bindings");plan::require(groups.is_arr()&&groups.size()==11,"Block58 current contract groups");
    std::size_t segments=0;
    for(std::size_t g=0;g<groups.size();++g) {
        const auto parts=groups.at(g).get("segments");
        for(std::size_t s=0;s<parts.size();++s) {
            const auto part=parts.at(s);const auto fragment=part.get("excerpt_utf8").as_str();
            work.spend(rfc.size()+fragment.size()*8+512);
            plan::require(fragment.size()==static_cast<std::size_t>(part.get("excerpt_utf8_bytes").as_int())&&loom::utils::crypto::sha256(fragment)==part.get_string("excerpt_sha256"),"Block58 contract excerpt byte/hash");
            const auto at=rfc.find(fragment);plan::require(at!=std::string_view::npos&&rfc.find(fragment,at+1)==std::string_view::npos,"Block58 substantive current contract missing/ambiguous");
            std::vector<std::pair<int,std::string>> stack;
            auto before=rfc.substr(0,at);
            while(!before.empty()) {
                const auto end=before.find('\n');const auto line=before.substr(0,end);std::size_t hashes=0;while(hashes<line.size()&&line[hashes]=='#')++hashes;
                if(hashes>0&&hashes<=6&&hashes<line.size()&&line[hashes]==' ') {
                    while(!stack.empty()&&stack.back().first>=static_cast<int>(hashes))stack.pop_back();stack.emplace_back(static_cast<int>(hashes),line);
                }
                if(end==std::string_view::npos)break;before.remove_prefix(end+1);
            }
            const auto expected=part.get("section_anchors");plan::require(expected.size()==stack.size(),"Block58 current contract heading depth");
            for(std::size_t i=0;i<stack.size();++i)plan::require(expected.at(i).get("level").as_int()==stack[i].first&&expected.at(i).get_string("exact_heading_line")==stack[i].second,"Block58 current contract semantic heading");
            ++segments;
        }
    }
    plan::require(segments==13,"Block58 complete current contract segments");
}
void report_metrics(json::JsonMutVal record,const plan::Plan& p) {
    const auto& m=p.metrics;
    record.set("grid_width",m.grid_width);record.set("outer_width",m.width);record.set("grid_height",m.grid_height);record.set("total_height",m.height);
    record.set("area",static_cast<int64_t>(m.area));record.set("note_records",static_cast<int64_t>(m.note_records));record.set("note_rows",static_cast<int64_t>(m.note_rows));
    record.set("note_cells",static_cast<int64_t>(m.note_cells));record.set("runs",static_cast<int64_t>(m.runs));record.set("contour_visits",static_cast<int64_t>(m.contour_visits));
    record.set("route_visits",static_cast<int64_t>(m.route_visits));record.set("route_attempts",static_cast<int64_t>(m.route_attempts));
    record.set("serialized_bytes_upper",static_cast<int64_t>(m.serialized_bytes_upper));record.set("proposed_production_work_upper",static_cast<int64_t>(m.production_upper));
}

TEST(MermaidBlock58Preflight, AllCompleteOriginalsKeepSourceScheduleAndFiniteCosts) {
    Inputs input;json::JsonMutDoc report;auto root=report.object(),records=root.make_arr();
    std::size_t good=0,errors=0,total_nodes=0,total_groups=0,total_spaces=0,total_edges=0,note_records=0,note_rows=0,note_cells=0;
    for(const auto& pin:block::kPins) {
        auto item=records.make_obj();item.set("path",pin.path);item.set("source_sha256",pin.raw_sha);item.set("drawing_credit",0);
        item.set("captures_performed",0);item.set("glyph_oracle_approved",false);item.set("production_prepayment_verified",false);
        try {
            block::OracleWork work;const auto p=input.build(pin.path,work);plan::validate(p);report_metrics(item,p);
            for(std::size_t i=1;i<p.entities.size();++i) {
                const auto& n=p.entities[i];if(n.group)++total_groups;else if(n.space)++total_spaces;else ++total_nodes;
            }
            total_edges+=p.edges.size();note_records+=p.metrics.note_records;note_rows+=p.metrics.note_rows;note_cells+=p.metrics.note_cells;
            EXPECT_LE(p.metrics.production_upper,1'000'000u)<<pin.path;EXPECT_FALSE(p.glyph_oracle_approved);EXPECT_FALSE(p.production_prepayment_verified);EXPECT_EQ(p.drawing_credit,0);
            item.set("candidate_plan_valid",true);++good;
        }catch(const std::exception& e){item.set("candidate_plan_valid",false);item.set("error",e.what());++errors;ADD_FAILURE()<<pin.path<<": "<<e.what();}
        records.append(item);
    }
    root.set("schema","block58_B2_candidate_preflight/v1");root.set("qualification","Test-only source/mask/route worksheet; not observed terminal glyphs, production Work execution, native drawing or admission.");
    root.set("source_facts_sha256",block::kSourceFactsSha);root.set("terminal_facts_sha256",block::kTerminalFactsSha);
    root.set("originals",58);root.set("candidate_plans_valid",static_cast<int64_t>(good));root.set("errors",static_cast<int64_t>(errors));root.set("drawing_credit_added",0);
    root.set("root_apron_candidate",1);root.set("sizeof_cell",static_cast<int64_t>(sizeof(plan::Cell)));root.set("sizeof_entity",static_cast<int64_t>(sizeof(plan::Entity)));
    root.set("sizeof_parser_span",static_cast<int64_t>(sizeof(plan::ParserSpan)));root.set("sizeof_style_pair",static_cast<int64_t>(sizeof(plan::StylePair)));root.add("cases",records);report.set_root(root);
    const fs::path path=fs::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-block58-preflight.json";std::ofstream stream(path,std::ios::binary);ASSERT_TRUE(stream.good());stream<<report.to_string();stream.close();ASSERT_TRUE(stream.good());
    EXPECT_EQ(good,58u);EXPECT_EQ(errors,0u);EXPECT_EQ(total_nodes,279u);EXPECT_EQ(total_groups,60u);EXPECT_EQ(total_spaces,42u);EXPECT_EQ(total_edges,18u);
    EXPECT_EQ(note_records,122u);EXPECT_EQ(note_rows,386u);EXPECT_EQ(note_cells,9179u);
}

TEST(MermaidBlock58Preflight, ThreeWayFanoutUsesDistinctPortsAndAnActualRootApron) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/nightly-check/should-handle-different-types-of-edges.mmd",work);plan::validate(p);
    ASSERT_EQ(p.routes.size(),3u);EXPECT_EQ(p.metrics.grid_width,35);EXPECT_EQ(p.metrics.grid_height,23);
    const auto& a=p.entities[static_cast<std::size_t>(plan::index(p,"id:A"))];
    EXPECT_EQ(a.band.x,1);EXPECT_EQ(a.band.y,1);EXPECT_EQ(a.band.w,9);EXPECT_EQ(a.band.h,5);
    const auto& b=p.entities[static_cast<std::size_t>(plan::index(p,"id:B"))];const auto& c=p.entities[static_cast<std::size_t>(plan::index(p,"id:C"))];const auto& d=p.entities[static_cast<std::size_t>(plan::index(p,"id:D"))];
    EXPECT_EQ(b.band.x,1);EXPECT_EQ(c.band.x,13);EXPECT_EQ(d.band.x,25);EXPECT_EQ(b.band.y,17);EXPECT_EQ(c.band.y,17);EXPECT_EQ(d.band.y,17);
    std::set<std::pair<int,int>> starts;bool apron=false;
    for(const auto& r:p.routes){starts.emplace(r.points.front().x,r.points.front().y);for(auto q:r.points)apron=apron||q.x==0||q.y==0||q.x==34||q.y==22;}
    EXPECT_EQ(starts.size(),3u);EXPECT_TRUE(apron);EXPECT_EQ(p.routes[0].end_kind,"circle");EXPECT_EQ(p.routes[1].end_kind,"point");EXPECT_EQ(p.routes[2].end_kind,"cross");
    // Good first; a missing route cell is not an output generated expected value.
    auto missing=p;const auto q=missing.routes.back().points.at(missing.routes.back().points.size()/2);missing.cells[plan::offset(missing,q)].flags&=static_cast<std::uint8_t>(~4u);
    rejects_without_resource([&]{plan::validate(missing);});
}

TEST(MermaidBlock58Preflight, NineColumnSourceHasThreeRowsAndAllNineIndependentLinks) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/nightly-check/should-render-all-arrow-types-in-9-columns-grid.mmd",work);plan::validate(p);
    ASSERT_EQ(p.routes.size(),9u);ASSERT_EQ(p.entities[0].row_heights.size(),3u);EXPECT_EQ(p.metrics.grid_width,107);EXPECT_EQ(p.metrics.grid_height,23);
    std::size_t starts=0,ends=0;for(const auto& r:p.routes){if(r.start_kind=="point")++starts;if(r.end_kind=="point")++ends;}
    EXPECT_EQ(starts,3u);EXPECT_EQ(ends,6u);
    auto reversed=p;auto it=std::ranges::find_if(reversed.routes,[](const auto& r){return r.start_kind=="point";});ASSERT_NE(it,reversed.routes.end());it->start_direction=(it->start_direction+2)%4;
    rejects_without_resource([&]{plan::validate(reversed);});
}

TEST(MermaidBlock58Preflight, DescendantEdgesCrossActualContainerGatesWithoutRetargeting) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/pr-check/should-handle-edges-from-blocks-in-sub-blocks-to-other-blocks.mmd",work);plan::validate(p);
    ASSERT_EQ(p.routes.size(),1u);ASSERT_EQ(p.routes[0].gates.size(),1u);
    EXPECT_FALSE(p.entities[static_cast<std::size_t>(p.routes[0].source)].group);
    EXPECT_NE(p.routes[0].source,p.routes[0].gates[0].group);
    auto missing=p;missing.cells[plan::offset(missing,missing.routes[0].gates[0].at)].flags&=static_cast<std::uint8_t>(~1u);
    rejects_without_resource([&]{plan::validate(missing);});
    auto erased=p;erased.routes[0].gates.clear();rejects_without_resource([&]{plan::validate(erased);});
}

TEST(MermaidBlock58Preflight, ShapeMasksKeepConcaveNotchAndAllAuthoredArrowHeads) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/nightly-check/block-types-2-odd-diamond-and-hexagon.mmd",work);plan::validate(p);
    const auto& odd=p.entities[static_cast<std::size_t>(plan::index(p,"id:A"))];const auto& box=odd.body;
    EXPECT_TRUE(plan::mask(odd,{box.x,box.y}));EXPECT_FALSE(plan::mask(odd,{box.x,box.y+box.h/2}));EXPECT_TRUE(plan::mask(odd,{box.x+2,box.y+box.h/2}));
    const auto& port=odd.ports[2];EXPECT_EQ(port.x,box.x+2);EXPECT_EQ(port.y,box.y+box.h/2);
    // A bounding rectangle would incorrectly fill the inward notch.
    auto wrong=odd;wrong.shape="rectangle";EXPECT_TRUE(plan::mask(wrong,{box.x,box.y+box.h/2}));
    block::OracleWork arrows_work;const auto arrows=input.build("block/nightly-check/should-handle-block-arrows-in-different-directions.mmd",arrows_work);plan::validate(arrows);
    bool four=false;for(const auto& e:arrows.entities)if(e.shape=="block_arrow"&&e.heads==15){four=true;for(const auto q:e.ports)EXPECT_TRUE(plan::mask(e,q));}
    EXPECT_TRUE(four);
}

TEST(MermaidBlock58Preflight, NotesIncludeEveryCollectionAndDoNotAcquireGridPadding) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/visual-check/nested-redux-color-theme.mmd",work);plan::validate(p);
    EXPECT_EQ(p.metrics.note_records,14u);EXPECT_EQ(p.metrics.note_rows,48u);EXPECT_EQ(p.metrics.note_cells,1176u);EXPECT_EQ(p.notes.size(),62u);EXPECT_EQ(p.metrics.height-p.metrics.grid_height,62);
    EXPECT_EQ(std::ranges::count(p.notes,std::string{}),14);
    for(const auto& n:p.notes)EXPECT_LE(n.size(),32u);
}

TEST(MermaidBlock58Preflight, InvalidColumnsAndDuplicatePlacementsRejectBeforeArithmeticOrRaster) {
    Inputs input;block::OracleWork source_work;const auto facts=input.bind(block::kPins.front().path,source_work);
    {block::OracleWork good;const auto p=plan::build(facts.terminal_model,353,good,facts.source_model);plan::validate(p);}
    auto changed=copied(facts.terminal_model);changed.root_mut().get("root_and_groups").at(0).get("logical_source_grid").set("columns",0);
    auto immutable=json::parse(changed.to_string());ASSERT_TRUE(immutable);block::OracleWork bad;
    rejects_without_resource([&]{static_cast<void>(plan::build(immutable->root(),353,bad,facts.source_model));});
    auto duplicated=copied(facts.terminal_model);auto placements=duplicated.root_mut().get("root_and_groups").at(0).get("logical_source_grid").get("placements");
    placements.append(duplicated.copy_val(facts.terminal_model.get("root_and_groups").at(0).get("logical_source_grid").get("placements").at(0)));
    auto immutable_duplicate=json::parse(duplicated.to_string());ASSERT_TRUE(immutable_duplicate);block::OracleWork duplicate_work;
    rejects_without_resource([&]{static_cast<void>(plan::build(immutable_duplicate->root(),353,duplicate_work,facts.source_model));});
}

TEST(MermaidBlock58Preflight, ResourceFailuresCannotBeCountedAsGeometryNegativesOrAdmission) {
    Inputs input;block::OracleWork source_work;const auto facts=input.bind(block::kPins.front().path,source_work);
    block::OracleWork exhausted;exhausted.left=0;EXPECT_THROW(static_cast<void>(plan::build(facts.terminal_model,353,exhausted,facts.source_model)),std::exception);
    EXPECT_EQ(plan::Plan::drawing_credit,0);EXPECT_FALSE(plan::Plan::glyph_oracle_approved);EXPECT_FALSE(plan::Plan::production_prepayment_verified);
}

TEST(MermaidBlock58Preflight, AssetBytesAndRecomputedModelDigestsCannotBorrowSourceAuthority) {
    Inputs input;block::OracleWork good;const auto valid=input.bind(block::kPins.front().path,good);EXPECT_EQ(valid.global,58);
    {block::OracleWork bad;rejects_without_resource([&]{block::verify_asset_bytes(input.source_bytes+" ",input.terminal_bytes,bad);});}
    const auto raw=read(kFixtures/"mermaid_official/cases"/std::string(block::kPins.front().path));
    {block::OracleWork bad;rejects_without_resource([&]{static_cast<void>(block::bind(input.source.root(),input.terminal.root(),block::kPins.front().path,raw+"\n",input.manifest.root(),bad));});}
    auto forged=copied(input.terminal.root());auto item=forged.root_mut().get("cases").at(0);item.get("proposed_terminal_model").get("nodes").at(0).get("caption").set("display_caption","forged");
    auto interim=json::parse(forged.to_string());ASSERT_TRUE(interim);block::OracleWork encoding;
    const auto digest=loom::utils::crypto::sha256(block::detail::canonical(interim->root().get("cases").at(0).get("proposed_terminal_model"),encoding,block::detail::RealScope::terminal_model));
    item.set("proposed_terminal_model_sha256",digest);auto immutable=json::parse(forged.to_string());ASSERT_TRUE(immutable);block::OracleWork forged_work;
    rejects_without_resource([&]{static_cast<void>(block::bind(input.source.root(),immutable->root(),block::kPins.front().path,raw,input.manifest.root(),forged_work));});
}

TEST(MermaidBlock58Preflight, CurrentContractBindsSubstantiveTextWithoutWholeFileHashCycles) {
    Inputs input;const auto rfc=read(kFixtures.parent_path().parent_path()/"docs/rfcs/0030-terminal-mermaid-diagrams.md");
    {block::OracleWork work;verify_current_contract(input.terminal.root(),rfc,work);}
    {block::OracleWork work;verify_current_contract(input.terminal.root(),"Noncontract status line.\n"+rfc+"\nUnrelated review history.\n",work);}
    auto changed=rfc;const auto at=changed.find("Atomic style parsing is mandatory:");ASSERT_NE(at,std::string::npos);changed.replace(at,std::string_view("Atomic style parsing is mandatory:").size(),"Atomic style parsing is OPTIONAL:");
    block::OracleWork bad;rejects_without_resource([&]{verify_current_contract(input.terminal.root(),changed,bad);});
}

TEST(MermaidBlock58Preflight, SourceStyleRolesAndStyledBlankCellsRemainObservableInPlan) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/nightly-check/should-style-statements-and-class-statements.mmd",work);plan::validate(p);
    const int a=plan::index(p,"id:A"),b=plan::index(p,"id:B");EXPECT_EQ(p.entities[static_cast<std::size_t>(a)].fill,5);EXPECT_EQ(p.entities[static_cast<std::size_t>(a)].stroke,1);
    EXPECT_EQ(p.entities[static_cast<std::size_t>(b)].fill,6);EXPECT_TRUE(p.entities[static_cast<std::size_t>(b)].bold);
    auto changed=p;auto cell=std::ranges::find_if(changed.cells,[&](const auto& c){return c.owner==a&&(c.flags&7)==0;});ASSERT_NE(cell,changed.cells.end());
    // This is an interior blank with source fill, not an ink-only color test.
    cell->style=static_cast<std::uint16_t>(1|(6<<5));rejects_without_resource([&]{plan::validate(changed);});
    auto reserved=p;reserved.cells.front().style|=0x8000;rejects_without_resource([&]{plan::validate(reserved);});
    block::OracleWork link_work;const auto links=input.build("block/nightly-check/should-render-all-arrow-types-in-9-columns-grid.mmd",link_work);plan::validate(links);
    auto thin=links;const auto bold_point=thin.routes.at(3).points.at(thin.routes.at(3).points.size()/2);
    ASSERT_NE(thin.cells[plan::offset(thin,bold_point)].style&1024,0);thin.cells[plan::offset(thin,bold_point)].style&=static_cast<std::uint16_t>(~1024u);
    rejects_without_resource([&]{plan::validate(thin);});
    auto wrong_role=links;auto& route_cell=wrong_role.cells[plan::offset(wrong_role,wrong_role.routes.front().points.at(2))];route_cell.style=static_cast<std::uint16_t>((route_cell.style&~31u)|2u);
    rejects_without_resource([&]{plan::validate(wrong_role);});
}

TEST(MermaidBlock58Preflight, EmptyArrowDirectionsCannotSilentlyBecomeARectangle) {
    Inputs input;block::OracleWork good;const auto facts=input.bind(block::kPins.front().path,good);
    {block::OracleWork work;const auto p=plan::build(facts.terminal_model,353,work,facts.source_model);plan::validate(p);}
    auto changed=copied(facts.terminal_model);changed.root_mut().get("nodes").at(0).add("resolved_direction_heads",changed.array());
    auto immutable=json::parse(changed.to_string());ASSERT_TRUE(immutable);block::OracleWork bad;
    rejects_without_resource([&]{static_cast<void>(plan::build(immutable->root(),353,bad,facts.source_model));});
}

std::string glyph_at(const contours::Raster& r,plan::Point q) {
    plan::require(q.y>=0&&static_cast<std::size_t>(q.y)<r.rows.size(),"glyph fixture row");const auto glyphs=ftxui::Utf8ToGlyphs(r.rows[static_cast<std::size_t>(q.y)]);
    plan::require(q.x>=0&&static_cast<std::size_t>(q.x)<glyphs.size(),"glyph fixture column");return glyphs[static_cast<std::size_t>(q.x)];
}

TEST(MermaidBlock58Preflight, AllCompleteOriginalsHaveBoundedOwnedGlyphRowsWithoutAdmission) {
    Inputs input;json::JsonMutDoc report;auto root=report.object(),records=root.make_arr();std::size_t good=0,total_bytes=0,maximum_work=0;
    for(const auto& pin:block::kPins) {
        auto item=records.make_obj();item.set("path",pin.path);item.set("source_sha256",pin.raw_sha);item.set("drawing_credit",0);item.set("production_capture_count",0);
        try {
            block::OracleWork work;const auto facts=input.bind(pin.path,work);
            const auto p=plan::build(facts.terminal_model,static_cast<std::size_t>(facts.source_case.get("source_bytes").as_int()),work,facts.source_model);
            plan::validate(p);const auto raster=contours::raster(p,work);
            ASSERT_EQ(raster.rows.size(),static_cast<std::size_t>(p.metrics.height));std::string bytes;
            for(std::size_t y=0;y<raster.rows.size();++y) {
                if(y<static_cast<std::size_t>(p.metrics.grid_height))EXPECT_EQ(ftxui::string_width(raster.rows[y]),p.metrics.grid_width)<<pin.path;
                bytes+=std::to_string(raster.rows[y].size());bytes+=':';bytes+=raster.rows[y];
            }
            // Read immutable source facts again instead of deriving expected
            // caption/note content from the generated worksheet's own strings.
            const auto authored_nodes=facts.terminal_model.get("nodes");
            for(std::size_t n=0;n<authored_nodes.size();++n) {
                const auto node=authored_nodes.at(n);const auto& placed=p.entities[static_cast<std::size_t>(plan::index(p,node.get_string("handle")))];
                const auto rows=node.get("caption").get("caption_rows");ASSERT_EQ(placed.rows.size(),rows.size());
                for(std::size_t y=0;y<rows.size();++y) {
                    const auto expected=rows.at(y).as_str();EXPECT_EQ(placed.rows[y],expected);
                    for(std::size_t x=0;x<expected.size();++x)EXPECT_EQ(glyph_at(raster,{placed.caption.x+static_cast<int>(x),placed.caption.y+static_cast<int>(y)}),std::string(1,expected[x]));
                }
            }
            const auto authored_edges=facts.terminal_model.get("edges");ASSERT_EQ(p.routes.size(),authored_edges.size());
            for(std::size_t n=0;n<authored_edges.size();++n) {
                const auto rows=authored_edges.at(n).get("caption").get("caption_rows");
                if(rows.size()) {
                    const auto expected=rows.at(0).as_str();const auto& label=p.routes[n].label_box;
                    for(std::size_t x=0;x<expected.size();++x)EXPECT_EQ(glyph_at(raster,{label.x+static_cast<int>(x),label.y}),std::string(1,expected[x]));
                }
            }
            std::size_t note=static_cast<std::size_t>(p.metrics.grid_height);
            for(const auto key:{"required_metadata_notices","required_slot_legend","required_source_style_declaration_legend","required_style_loss_notices"}) {
                const auto collection=facts.terminal_model.get(key);for(std::size_t n=0;n<collection.size();++n) {
                    ASSERT_LT(note,raster.rows.size());EXPECT_EQ(raster.rows[note++],"");const auto lines=collection.at(n).get("rows");
                    for(std::size_t row=0;row<lines.size();++row){ASSERT_LT(note,raster.rows.size());EXPECT_EQ(raster.rows[note++],lines.at(row).as_str());}
                }
            }
            EXPECT_EQ(note,raster.rows.size());
            EXPECT_LE(raster.bytes,p.metrics.serialized_bytes_upper);const auto upper=p.metrics.production_upper+8*raster.visits+4096+
                raster.bytes*(sizeof(std::string)+2)+16*(p.metrics.area+p.metrics.note_cells);
            EXPECT_LE(upper,1'000'000u)<<pin.path;maximum_work=std::max(maximum_work,upper);total_bytes+=raster.bytes;
            item.set("glyph_mapping_visits",static_cast<int64_t>(raster.visits));item.set("glyph_bytes",static_cast<int64_t>(raster.bytes));item.set("candidate_work_with_glyph_upper",static_cast<int64_t>(upper));
            item.set("rows_sha256",loom::utils::crypto::sha256(bytes));auto lines=item.make_arr();for(const auto& row:raster.rows)lines.append(lines.make_str(row));item.add("candidate_rows",lines);
            item.set("candidate_rows_checked",true);++good;
        }catch(const std::exception& e){item.set("candidate_rows_checked",false);item.set("error",e.what());ADD_FAILURE()<<pin.path<<": "<<e.what();}
        records.append(item);
    }
    root.set("schema","block58_B2_candidate_glyph_rows/v1");root.set("qualification","Generated test-only proposal rows; independent glyph approval, observed production pixels, production prepayment and admission remain pending.");
    root.set("source_facts_sha256",block::kSourceFactsSha);root.set("terminal_facts_sha256",block::kTerminalFactsSha);root.set("originals",58);root.set("candidate_rows_checked",static_cast<int64_t>(good));root.set("drawing_credit_added",0);
    root.set("total_glyph_bytes",static_cast<int64_t>(total_bytes));root.set("maximum_candidate_work_with_glyph_upper",static_cast<int64_t>(maximum_work));root.add("cases",records);report.set_root(root);
    std::ofstream stream(fs::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-block58-glyph-preflight.json",std::ios::binary);ASSERT_TRUE(stream.good());stream<<report.to_string();stream.close();ASSERT_TRUE(stream.good());EXPECT_EQ(good,58u);
}

TEST(MermaidBlock58Preflight, LiteralRectangleGoldenKeepsLeadingAndTrailingSourceSpaceSlots) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/nightly-check/block-in-the-middle-with-space-on-each-side.mmd",work);const auto r=contours::raster(p,work);
    EXPECT_EQ(p.metrics.grid_width,59);EXPECT_EQ(p.metrics.grid_height,7);
    const std::array<std::string_view,5> expected{"┌───────────────┐","│               │","│ In the middle │","│               │","└───────────────┘"};
    for(int y=0;y<5;++y) {
        const std::string row=std::string(21,' ')+std::string(expected[static_cast<std::size_t>(y)])+std::string(21,' ');
        EXPECT_EQ(r.rows.at(static_cast<std::size_t>(y+1)),row);
    }
    EXPECT_EQ(r.rows.front(),std::string(59,' '));EXPECT_EQ(r.rows.back(),std::string(59,' '));
}

TEST(MermaidBlock58Preflight, LiteralBidirectionalMarkerCoordinatesFaceTheirOwnEndpoints) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/nightly-check/should-render-all-arrow-types-in-9-columns-grid.mmd",work);const auto r=contours::raster(p,work);
    // E/F are the third authored pair in the first logical row, not row three.
    EXPECT_EQ(glyph_at(r,{82,3}),"◀");EXPECT_EQ(glyph_at(r,{96,3}),"▶");
    EXPECT_EQ(glyph_at(r,{81,3}),"├");EXPECT_EQ(glyph_at(r,{97,3}),"┤");
    // Normal, thick and dotted links preserve separate glyph/attribute duties.
    EXPECT_EQ(glyph_at(r,{17,3}),"─");EXPECT_EQ(glyph_at(r,{17,11}),"─");EXPECT_EQ(glyph_at(r,{17,19}),"┄");
    EXPECT_NE(p.cells[plan::offset(p,{17,11})].style&1024,0);EXPECT_EQ(p.cells[plan::offset(p,{17,3})].style&1024,0);
}

TEST(MermaidBlock58Preflight, LiteralDescendantGateHasAllFourReciprocalArms) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/pr-check/should-handle-edges-from-blocks-in-sub-blocks-to-other-blocks.mmd",work);const auto r=contours::raster(p,work);
    ASSERT_EQ(p.routes.size(),1u);ASSERT_EQ(p.routes[0].gates.size(),1u);EXPECT_EQ(glyph_at(r,p.routes[0].gates[0].at),"┼");
    // Authored order is B, space, anonymous group(D): D reaches B leftwards.
    EXPECT_EQ(p.routes[0].gates[0].at,(plan::Point{33,5}));EXPECT_EQ(glyph_at(r,{33,5}),"┼");
    EXPECT_EQ(glyph_at(r,{35,5}),"┤");EXPECT_EQ(glyph_at(r,{13,5}),"├");EXPECT_EQ(glyph_at(r,{14,5}),"◀");
}

TEST(MermaidBlock58Preflight, AuthoredSingleSpaceCaptionNeverBecomesPlaceholderOrIdentifier) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/pr-check/look-neo-with-the-neo-theme.mmd",work);const auto r=contours::raster(p,work);
    const auto& node=p.entities[static_cast<std::size_t>(plan::index(p,"id:down"))];ASSERT_EQ(node.rows,std::vector<std::string>{" "});EXPECT_EQ(node.caption.w,1);
    EXPECT_EQ(glyph_at(r,{node.caption.x,node.caption.y})," ");
}

TEST(MermaidBlock58Preflight, IrregularApexGlyphsRetainBothIncidentContourArms) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/nightly-check/block-types-2-odd-diamond-and-hexagon.mmd",work);const auto r=contours::raster(p,work);
    // Raw source A>"rect_left_inv_arrow"] has an INWARD notch. C{{...}}
    // has two outward hexagon tips. These literals are not renderer output.
    EXPECT_EQ(glyph_at(r,{3,8}),"█");EXPECT_EQ(glyph_at(r,{61,8}),"█");EXPECT_EQ(glyph_at(r,{87,8}),"█");
    block::OracleWork arrows_work;const auto a=input.build("block/nightly-check/should-handle-block-arrows-in-different-directions.mmd",arrows_work);const auto rows=contours::raster(a,arrows_work);
    constexpr std::array<std::string_view,4> expected{"█","█","█","█"};
    for(const auto& n:a.entities)if(n.shape=="block_arrow")for(std::size_t d=0;d<4;++d)if(n.heads&(1u<<d))EXPECT_EQ(glyph_at(rows,n.ports[d]),expected[d]);
}

std::string cell_slice(const contours::Raster& r,int row,int first,int count) {
    const auto glyphs=ftxui::Utf8ToGlyphs(r.rows.at(static_cast<std::size_t>(row)));std::string out;
    plan::require(first>=0&&count>=0&&static_cast<std::size_t>(first+count)<=glyphs.size(),"literal boundary slice");
    for(int x=first;x<first+count;++x)out+=glyphs[static_cast<std::size_t>(x)];return out;
}

TEST(MermaidBlock58Preflight, CompleteStadiumGoldenKeepsBothCurvedPoleBranches) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/nightly-check/block-types-3-stadium.mmd",work);const auto r=contours::raster(p,work);
    // Radius-three disk caps expose the short horizontal shoulders at local
    // rows1/5 as well as the two curved poles; none may be erased.
    const std::array<std::string_view,9> expected{"                 ","    █████████    ","  ██         ██  ","  █           █  "," █   stadium   █ ","  █           █  ","  ██         ██  ","    █████████    ","                 "};
    ASSERT_EQ(r.rows.size(),expected.size());for(std::size_t y=0;y<expected.size();++y)EXPECT_EQ(r.rows[y],expected[y]);
}

TEST(MermaidBlock58Preflight, CompleteInwardNotchGoldenKeepsRailsAndBothIncidentSlants) {
    Inputs input;block::OracleWork work;const auto p=input.build("block/nightly-check/block-types-2-odd-diamond-and-hexagon.mmd",work);const auto r=contours::raster(p,work);
    const std::array<std::string,7> expected{
        "███████████████████████████",
        " █"+std::string(24,' ')+"█",
        "  █"+std::string(23,' ')+"█",
        "  █ rect_left_inv_arrow   █",
        "  █"+std::string(23,' ')+"█",
        " █"+std::string(24,' ')+"█",
        "███████████████████████████"};
    for(int y=0;y<7;++y)EXPECT_EQ(cell_slice(r,y+5,1,27),expected[static_cast<std::size_t>(y)]);
}

void bind_boundary_refs(Inputs& input,json::JsonVal fixture) {
    const auto refs=fixture.get("source_refs");plan::require(refs.is_arr()&&refs.size()>0&&refs.size()<=8,"boundary source references");
    for(std::size_t i=0;i<refs.size();++i) {
        const auto ref=refs.at(i);block::OracleWork work;const auto facts=input.bind(ref.get_string("path"),work);
        plan::require(ref.get_string("source_sha256")==facts.source_case.get_string("source_sha256")&&
            ref.get_string("complete_model_sha256")==facts.source_case.get_string("complete_model_sha256"),"boundary original source/Model identity");
        const auto nodes=facts.terminal_model.get("nodes");bool found=false;
        for(std::size_t n=0;n<nodes.size();++n)if(nodes.at(n).get_string("handle")==ref.get_string("handle")) {
            const auto node=nodes.at(n);plan::require(node.get_string("source_shape_type")==ref.get_string("source_shape_type")&&
                node.get_string("resolved_display_shape")==fixture.get("geometry").get_string("shape"),"boundary witnessed source shape identity");
            unsigned heads=0;const auto dirs=node.get("resolved_direction_heads");
            for(std::size_t d=0;d<dirs.size();++d){const auto name=dirs.at(d).as_str();heads|=name=="right"?1u:name=="down"?2u:name=="left"?4u:name=="up"?8u:0u;}
            plan::require(heads==static_cast<unsigned>(fixture.get("geometry").get("heads").as_int()),"boundary first-owned head set");found=true;
        }
        plan::require(found,"boundary source leaf witness found");
    }
}

TEST(MermaidBlock58Preflight, IndependentFullBoundaryFixturesOwnAllSourceTypesAndHeadsets) {
    Inputs input;const auto raw=read(kFixtures/"mermaid_native/block58_boundary_contract.v1.json",256*1024);block::boundary::verify_bytes(raw);auto doc=json::parse(raw);ASSERT_TRUE(doc);block::boundary::verify_root(doc->root());
    const auto root=doc->root(),fixtures=root.get("fixtures");ASSERT_TRUE(fixtures.is_arr());ASSERT_GE(fixtures.size(),18u);ASSERT_LE(fixtures.size(),64u);ASSERT_EQ(root.get("drawing_credit").as_int(),0);
    std::set<std::string> source_types,ids;std::set<int> headsets;
    for(std::size_t n=0;n<fixtures.size();++n) {
        const auto fixture=fixtures.at(n);SCOPED_TRACE(fixture.get_string("fixture_id"));ASSERT_TRUE(ids.insert(fixture.get_string("fixture_id")).second);
        bind_boundary_refs(input,fixture);const auto refs=fixture.get("source_refs");for(std::size_t i=0;i<refs.size();++i)source_types.insert(refs.at(i).get_string("source_shape_type"));
        const auto g=fixture.get("geometry");if(g.get_string("shape")=="block_arrow")headsets.insert(static_cast<int>(g.get("heads").as_int()));
        block::OracleWork work;const auto p=block::boundary::primitive(fixture,work);const auto r=contours::raster(p,work);EXPECT_NO_THROW(block::boundary::check(fixture,p,r));
    }
    EXPECT_EQ(source_types,(std::set<std::string>{"na","square","round","circle","diamond","hexagon","stadium","lean_right","subroutine","trapezoid","rect_left_inv_arrow","block_arrow"}));
    EXPECT_EQ(headsets,(std::set<int>{1,2,4,8,5,10,15}));
}

TEST(MermaidBlock58Preflight, BoundaryNegativesRequireCompleteLiteralGoodBeforeMutation) {
    Inputs input;const auto raw=read(kFixtures/"mermaid_native/block58_boundary_contract.v1.json",256*1024);block::boundary::verify_bytes(raw);auto doc=json::parse(raw);ASSERT_TRUE(doc);block::boundary::verify_root(doc->root());const auto fixtures=doc->root().get("fixtures");
    rejects_without_resource([&]{block::boundary::verify_bytes(raw+" ");});
    for(std::size_t n=0;n<fixtures.size();++n) {
        const auto fixture=fixtures.at(n);SCOPED_TRACE(fixture.get_string("fixture_id"));bind_boundary_refs(input,fixture);
        block::OracleWork work;const auto p=block::boundary::primitive(fixture,work);const auto good=contours::raster(p,work);
        ASSERT_NO_THROW(block::boundary::check(fixture,p,good));
        bool changed=false;auto missing=good;
        for(std::size_t y=0;y<missing.rows.size()&&!changed;++y) {
            const auto glyphs=ftxui::Utf8ToGlyphs(missing.rows[y]);for(std::size_t x=0;x<glyphs.size();++x)if(glyphs[x]!=" "){
                missing.rows[y]=block::boundary::replace_glyph(missing.rows[y],static_cast<int>(x)," ");changed=true;break;
            }
        }
        ASSERT_TRUE(changed);rejects_without_resource([&]{block::boundary::check(fixture,p,missing);});
        auto mask=p;mask.cells.front().owner=mask.cells.front().owner==1?0:1;rejects_without_resource([&]{block::boundary::check(fixture,mask,good);});
        auto port=p;const int direction=static_cast<int>(fixture.get("expected").get("ports").at(0).get("direction").as_int());++port.entities[1].ports[static_cast<std::size_t>(direction)].x;
        rejects_without_resource([&]{block::boundary::check(fixture,port,good);});
    }
}
TEST(MermaidBlock58Preflight, ConnectedPrimitivePortsKeepCompleteIndependentBoundaries) {
    Inputs input;const auto raw=read(kFixtures/"mermaid_native/block58_boundary_contract.v1.json",256*1024);
    block::boundary::verify_bytes(raw);auto doc=json::parse(raw);ASSERT_TRUE(doc);block::boundary::verify_root(doc->root());
    const auto fixtures=doc->root().get("fixtures");
    for(std::size_t n=0;n<fixtures.size();++n) {
        const auto fixture=fixtures.at(n);SCOPED_TRACE(fixture.get_string("fixture_id"));bind_boundary_refs(input,fixture);
        block::OracleWork work;const auto primitive=block::boundary::primitive(fixture,work);const auto positive=contours::raster(primitive,work);
        ASSERT_NO_THROW(block::boundary::check(fixture,primitive,positive));
        for(std::size_t d=0;d<4;++d) {
            auto p=primitive;auto& e=p.entities[1];const auto shift=[](plan::Box& b){++b.x;++b.y;};
            shift(e.band);shift(e.body);shift(e.core);shift(e.caption);
            p.metrics.grid_width+=2;p.metrics.grid_height+=2;p.metrics.width=p.metrics.grid_width;p.metrics.height=p.metrics.grid_height;
            p.metrics.area=static_cast<std::size_t>(p.metrics.grid_width*p.metrics.grid_height);p.metrics.serialized_bytes_upper=3*p.metrics.area;
            p.entities[0].body=p.entities[0].band={0,0,p.metrics.grid_width,p.metrics.grid_height};p.cells.assign(p.metrics.area,{});
            plan::Ledger ledger;plan::paint(p,ledger);
            const auto port=e.ports[d],step=plan::kStep[d];const plan::Point outward{port.x+step.x,port.y+step.y};
            plan::Route route;route.points={port,outward};p.routes.push_back(route);p.edges.emplace_back();
            for(const auto at:route.points)p.cells[plan::offset(p,at)].flags|=4;
            const bool rectangular=e.shape=="rectangle"||e.shape=="rounded_rectangle"||e.shape=="subroutine";
            constexpr std::array<std::string_view,4> junctions{"├","┬","┤","┴"},stems{"─","│","─","│"};
            const auto expected_port=rectangular?junctions[d]:std::string_view("█");
            // Expected full rows come only from the independent literal fixture
            // plus the four explicitly specified outward stem/junction glyphs.
            std::vector<std::string> expected(static_cast<std::size_t>(p.metrics.grid_height),std::string(static_cast<std::size_t>(p.metrics.grid_width),' '));
            const auto rows=fixture.get("expected").get("glyph_rows");
            for(std::size_t y=0;y<rows.size();++y)expected[y+1]=" "+std::string(rows.at(y).as_str())+" ";
            expected[static_cast<std::size_t>(port.y)]=block::boundary::replace_glyph(expected[static_cast<std::size_t>(port.y)],port.x,expected_port);
            expected[static_cast<std::size_t>(outward.y)]=block::boundary::replace_glyph(expected[static_cast<std::size_t>(outward.y)],outward.x,stems[d]);
            const auto actual=contours::raster(p,work);ASSERT_EQ(actual.rows,expected);
            auto lost=actual;lost.rows[static_cast<std::size_t>(port.y)]=block::boundary::replace_glyph(lost.rows[static_cast<std::size_t>(port.y)],port.x," ");EXPECT_NE(lost.rows,expected);
            if(!rectangular){auto overwritten=actual;overwritten.rows[static_cast<std::size_t>(port.y)]=block::boundary::replace_glyph(overwritten.rows[static_cast<std::size_t>(port.y)],port.x,junctions[d]);EXPECT_NE(overwritten.rows,expected);}
        }
    }
}
TEST(MermaidBlock58Preflight, ProductionUsesTheSameFiniteRemainingBudgetForAllOriginals) {
    Inputs input;std::size_t maximum=0;json::JsonMutDoc report;auto root=report.object(),records=root.make_arr();
    for(const auto& pin:block::kPins) {
        SCOPED_TRACE(pin.path);const auto raw=read(kFixtures/"mermaid_official/cases"/std::string(pin.path),block::kMaxRawBytes);
        block::OracleWork binding;static_cast<void>(input.bind(pin.path,binding));
        loom::ui::mermaid::testing::Work budget;
        const auto decision=loom::ui::mermaid::testing::classify_mermaid(raw,budget);ASSERT_TRUE(decision);
        EXPECT_EQ(decision->family,loom::ui::mermaid::testing::MermaidProbeFamily::Block);
        const auto prefix=1'000'000-budget.remaining;
        const bool success=loom::ui::mermaid::testing::block_layout_succeeds(raw,budget);
        auto record=records.make_obj();record.set("path",pin.path);record.set("source_sha256",pin.raw_sha);record.set("complete_model_sha256",pin.source_model_sha);
        record.set("layout_succeeded",success);record.set("prefix_work",static_cast<int64_t>(prefix));record.set("total_live_work",static_cast<int64_t>(1'000'000-budget.remaining));record.set("remaining",static_cast<int64_t>(budget.remaining));records.append(record);
        if(!success){loom::ui::mermaid::testing::Work diagnostic;diagnostic.remaining=2'000'000;const bool larger=loom::ui::mermaid::testing::block_layout_succeeds(raw,diagnostic);ADD_FAILURE()<<"1M remaining="<<budget.remaining<<", non-scoring 2M diagnostic="<<larger<<", diagnostic spent="<<2'000'000-diagnostic.remaining;}
        maximum=std::max(maximum,1'000'000-budget.remaining);
        loom::ui::mermaid::testing::Work empty;empty.remaining=0;EXPECT_FALSE(loom::ui::mermaid::testing::block_layout_succeeds(raw,empty));EXPECT_EQ(empty.remaining,0u);
    }
    root.set("schema","block58_live_production_budget/v1");root.set("source_facts_sha256",block::kSourceFactsSha);root.set("originals",58);root.set("maximum_live_work",static_cast<int64_t>(maximum));
    root.set("work_limit",1'000'000);root.set("drawing_credit",0);root.set("allocation_prepayment_review","PENDING");root.add("cases",records);report.set_root(root);
    std::ofstream output(fs::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-block58-production-budget.json",std::ios::binary);ASSERT_TRUE(output.good());output<<report.to_string();output.close();ASSERT_TRUE(output.good());
    RecordProperty("maximum_block_source_work",std::to_string(maximum));
    EXPECT_LT(maximum,1'000'000u);
}
TEST(MermaidBlock58Preflight, RedeclarationsKeepFirstDirectionsAndSpanWithoutValidatingIgnoredHeadsets) {
    using loom::ui::mermaid::LayoutRowsCache;
    const std::string first="block-beta\ncolumns 3\nA<[\"Label\"]>(y) space B\n";
    LayoutRowsCache cache(4,65536);const auto baseline=cache.get_or_layout(first,false);ASSERT_TRUE(baseline&&!baseline->empty());
    const auto repeated=cache.get_or_layout(first+"A<[\"Label\"]>(x, down):2\n",false);ASSERT_TRUE(repeated&&!repeated->empty());EXPECT_EQ(*baseline,*repeated);
    loom::ui::mermaid::testing::Work valid;EXPECT_TRUE(loom::ui::mermaid::testing::block_layout_succeeds(first,valid));
    loom::ui::mermaid::testing::Work unsupported;EXPECT_FALSE(loom::ui::mermaid::testing::block_layout_succeeds("block-beta\nA<[\"Label\"]>(x, down)\n",unsupported));
}
namespace block_theme=loom::ui::design::theme;
struct KeepBlockTheme {block_theme::Theme value=block_theme::current_theme();~KeepBlockTheme(){block_theme::set_theme(value);}};
ftxui::Screen runtime_screen(std::string_view source,loom::ui::mermaid::LayoutRowsCache* cache=nullptr,int width=80) {
    auto fallback=ftxui::text("RAW SOURCE");auto art=cache?cache->render(source,false,fallback):loom::ui::mermaid::render_mermaid(source,fallback);
    plan::require(art.has_value(),"public Block runtime control must draw");
    auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(width),ftxui::Dimension::Fixed(40));
    ftxui::Render(screen,*art|ftxui::color(block_theme::current_theme().palette->text));return screen;
}
void same_runtime_screen(const ftxui::Screen& left,const ftxui::Screen& right) {
    ASSERT_EQ(left.dimx(),right.dimx());ASSERT_EQ(left.dimy(),right.dimy());
    for(int y=0;y<left.dimy();++y)for(int x=0;x<left.dimx();++x){const auto& a=left.PixelAt(x,y);const auto& b=right.PixelAt(x,y);
        EXPECT_EQ(a.character,b.character);EXPECT_EQ(a.foreground_color,b.foreground_color);EXPECT_EQ(a.background_color,b.background_color);EXPECT_EQ(a.bold,b.bold);
    }
}
TEST(MermaidBlock58Preflight, ConnectedPixelPortKeepsItsAuthoredStrokeAndBoldAcrossCacheLifetimes) {
    KeepBlockTheme restore;block_theme::set_theme(block_theme::ThemeVariant::Dark);
    const std::string raw="block-beta\nA((\"X\")) space B\nstyle A stroke:#f00,stroke-width:4px\nA --> B\n";
    const auto baseline=runtime_screen(raw);const auto& p=*block_theme::current_theme().palette;
    EXPECT_EQ(baseline.PixelAt(10,5).character,"█");EXPECT_EQ(baseline.PixelAt(10,5).foreground_color,p.danger);EXPECT_TRUE(baseline.PixelAt(10,5).bold);
    EXPECT_EQ(baseline.PixelAt(11,5).character,"─");EXPECT_EQ(baseline.PixelAt(11,5).foreground_color,p.text);EXPECT_FALSE(baseline.PixelAt(11,5).bold);
    EXPECT_EQ(baseline.PixelAt(5,5).character,"X");EXPECT_EQ(baseline.PixelAt(5,5).background_color,p.surface_tint_2);
    loom::ui::mermaid::LayoutRowsCache cache(1,1u<<20);ASSERT_FALSE(cache.get_or_layout(raw,false)->empty());same_runtime_screen(baseline,runtime_screen(raw,&cache));
    auto copied=cache;loom::ui::mermaid::LayoutRowsCache assigned(2,8);assigned=cache;cache.clear();
    same_runtime_screen(baseline,runtime_screen(raw,&copied));same_runtime_screen(baseline,runtime_screen(raw,&assigned));
    const auto misses=copied.misses();block_theme::set_theme(block_theme::ThemeVariant::Light);const auto light=runtime_screen(raw,&copied);
    EXPECT_EQ(copied.misses(),misses);EXPECT_EQ(light.PixelAt(10,5).foreground_color,block_theme::current_theme().palette->danger);EXPECT_TRUE(light.PixelAt(10,5).bold);
    block_theme::set_theme(block_theme::ThemeVariant::Dark);
    ASSERT_FALSE(copied.get_or_layout("block\nZ[\"Other\"]\n",false)->empty());same_runtime_screen(baseline,runtime_screen(raw,&copied));EXPECT_EQ(copied.misses(),misses+2);
    loom::ui::mermaid::LayoutRowsCache small(1,1),disabled(0,1u<<20);same_runtime_screen(baseline,runtime_screen(raw,&small));same_runtime_screen(baseline,runtime_screen(raw,&disabled));EXPECT_EQ(small.size(),0u);EXPECT_EQ(disabled.size(),0u);
    EXPECT_TRUE(copied.get_or_layout(raw,true)->empty());same_runtime_screen(baseline,runtime_screen(raw,&copied));
    const auto invalid=raw+"style A stroke-width:0\n";EXPECT_TRUE(copied.get_or_layout(invalid,false)->empty());EXPECT_FALSE(copied.render(invalid,false,ftxui::text("RAW SOURCE")));
}
TEST(MermaidBlock58Preflight, UnicodeEdgeCaptionRetainsWideCombiningBytesAndNoPlaceholderCells) {
    KeepBlockTheme restore;block_theme::set_theme(block_theme::ThemeVariant::Dark);
    const std::string raw="block-beta\nA space B\nA -- \"界́\" --> B\n";
    loom::ui::mermaid::LayoutRowsCache cache(4,65536);const auto rows=cache.get_or_layout(raw,false);ASSERT_TRUE(rows&&!rows->empty());
    std::string joined;for(const auto& row:*rows)joined+=row;EXPECT_NE(joined.find("界́"),std::string::npos);EXPECT_EQ(joined.find('a'),std::string::npos);
    const auto pixels=runtime_screen(raw,&cache);bool found=false;
    for(int y=0;y<pixels.dimy();++y)for(int x=0;x+1<pixels.dimx();++x)if(pixels.PixelAt(x,y).character=="界") {
        found=true;EXPECT_EQ(pixels.PixelAt(x+1,y).character,"́");EXPECT_EQ(pixels.PixelAt(x,y).foreground_color,pixels.PixelAt(x+1,y).foreground_color);EXPECT_EQ(pixels.PixelAt(x,y).background_color,pixels.PixelAt(x+1,y).background_color);
    }
    EXPECT_TRUE(found);
}
TEST(MermaidBlock58Preflight, NamedCompositeEndpointsAndLongWrappedCaptionsRespectActualBudgetBoundary) {
    loom::ui::mermaid::LayoutRowsCache cache(4,65536);
    const auto group=cache.get_or_layout("block-beta\ncolumns 3\nblock:g\nA\nend\nspace B\ng --> B\n",false);ASSERT_TRUE(group&&!group->empty());
    std::string text;for(int n=0;n<10;++n){if(n)text+=' ';text+="abcdefghijklmnopq";}
    const std::string raw="block-beta\nA[\""+text+"\"]\n";const auto rows=cache.get_or_layout(raw,false);ASSERT_TRUE(rows&&!rows->empty());
    std::size_t labels=0;for(const auto& row:*rows)if(row.find("abcdefghijklmnopq")!=std::string::npos)++labels;EXPECT_EQ(labels,10u);
    using loom::ui::mermaid::testing::Work;Work normal;ASSERT_TRUE(loom::ui::mermaid::testing::block_layout_succeeds(raw,normal));const auto spent=1'000'000-normal.remaining;ASSERT_GT(spent,0u);
    Work exact;exact.remaining=spent;EXPECT_TRUE(loom::ui::mermaid::testing::block_layout_succeeds(raw,exact));EXPECT_EQ(exact.remaining,0u);
    Work short_by_one;short_by_one.remaining=spent-1;EXPECT_FALSE(loom::ui::mermaid::testing::block_layout_succeeds(raw,short_by_one));EXPECT_LT(short_by_one.remaining,spent-1);
}
TEST(MermaidBlock58Preflight, EmptyConfigNamespacesAndDefaultClassListsRejectTheWholeSource) {
    loom::ui::mermaid::LayoutRowsCache cache(8,65536);const std::string good="block-beta\nA[\"Text\"]\nclassDef x fill:#f00\nclass A x\n";
    const auto positive=cache.get_or_layout(good,false);ASSERT_TRUE(positive&&!positive->empty());
    for(const auto& bad:std::array<std::string,4>{"---\nconfig:\n  theme:\n---\nblock-beta\nA\n","---\nconfig:\n  look:\n---\nblock-beta\nA\n","block-beta\nA\nclassDef x,default fill:#f00\nclass A x\n","block-beta\nA\nclassDef default,x fill:#f00\nclass A x\n"}) {
        const auto rejected=cache.get_or_layout(bad,false);ASSERT_TRUE(rejected);EXPECT_TRUE(rejected->empty());EXPECT_FALSE(cache.render(bad,false,ftxui::text("RAW SOURCE")));
        EXPECT_EQ(cache.get_or_layout(good,false),positive);
    }
}
} // namespace
