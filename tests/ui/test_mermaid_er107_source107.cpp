// Native ER107 complete-original consumer. Own public captures precede every
// current-case semantic bind. No ER producer implementation or foreign/Official
// captures/reports participate in source expectations. No admission updater.
#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
import std;
import loom.ui.visual.markdown;
import loom.ui.visual.mermaid.testing;
import loom.ui.foundation.design_tokens;
import loom.ui.foundation.theme_provider;
import loom.serdes.json;
import loom.crypto.crypto;
#include "mermaid_er107_authority.hpp"

namespace {
namespace er=loom::test::mermaid_er107;
namespace auth=er::authority;
namespace json=loom::utils::json;
namespace theme=loom::ui::design::theme;
namespace fs=std::filesystem;
constexpr std::string_view kSentinel="AFTER_ER107_NATIVE";
const fs::path kFixtures=fs::path(LOOM_TESTS_DIR)/"fixtures";
using Frames=std::array<auth::Frame,5>;
std::string hash(std::string_view v){return loom::utils::crypto::sha256(v);}
void need(bool yes,std::string_view why){auth::require(yes,why);}
std::string read(const fs::path& path,std::size_t cap=er::kMaxFactBytes) {
    need(!fs::is_symlink(fs::symlink_status(path))&&fs::is_regular_file(path)&&fs::file_size(path)<=cap,"resource: ER107 Native bounded file");
    std::ifstream stream(path,std::ios::binary);need(stream.good(),"protocol: ER107 Native source open");return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
json::JsonDoc parse(std::string_view bytes){auto d=json::parse(bytes);need(d.has_value(),"protocol: ER107 immutable JSON parse");return std::move(*d);}
struct Documents {
    json::JsonDoc source,terminal,presentation,manifest,prior;
    Documents(std::string_view s,std::string_view t,std::string_view e,std::string_view m,std::string_view p):source(parse(s)),terminal(parse(t)),presentation(parse(e)),manifest(parse(m)),prior(parse(p)){}
};
struct Inputs {
    std::string source_bytes=read(kFixtures/std::string(er::kFactsPath));
    std::string terminal_bytes=read(kFixtures/std::string(auth::kProjectionPath));
    std::string presentation_bytes=read(kFixtures/std::string(auth::kPresentationPath));
    std::string manifest_bytes=read(kFixtures/"mermaid_official/manifest.json");
    std::string prior_bytes=read(kFixtures/std::string(er::kPriorPath));
    std::optional<Documents> docs;
    Inputs(){er::OracleWork identities;auth::verify_asset_bytes(source_bytes,terminal_bytes,presentation_bytes,manifest_bytes,prior_bytes,identities);}
    // No parse/typed-root constructor before owned frames. DOM parsing is lazy;
    // later iterations still defer the selected case's bind until its15 frames.
    Documents& after_owned_captures(std::size_t count){need(count==15,"protocol: ER107 requires15 captures before binding");if(!docs)docs.emplace(source_bytes,terminal_bytes,presentation_bytes,manifest_bytes,prior_bytes);return *docs;}
    auth::BoundCase bind(std::size_t i,std::string_view raw,std::size_t captures,er::OracleWork& work){auto& d=after_owned_captures(captures);return auth::bind_case(d.source.root(),d.terminal.root(),d.manifest.root(),d.prior.root(),auth::kPins[i].path,raw,work);}
    void finish(std::size_t all_captured){need(all_captured==107&&docs.has_value(),"protocol: ER107 aggregate roots only after all own captures");er::OracleWork source_roots,projection_components;auth::validate_roots(docs->source.root(),docs->terminal.root(),docs->presentation.root(),docs->manifest.root(),docs->prior.root(),source_roots,projection_components);}
};
struct ThemeGuard {theme::Theme original=theme::current_theme();~ThemeGuard(){theme::set_theme(original);}};
auth::Surfaces surfaces(std::uint16_t dim=0) {
    const auto& p=*theme::current_theme().palette;const std::array<ftxui::Color,7> fg{p.text,p.text,p.danger,p.warning,p.success,p.info,p.accent_purple};
    const std::array<ftxui::Color,7> bg{ftxui::Color::Default,p.surface_tint_2,p.danger_tint_bg,p.warning_tint_bg,p.success_tint_bg,p.info_tint_bg,p.surface_tint_4};
    auth::Surfaces out;for(std::size_t i=0;i<7;++i){out.foreground[i]=fg[i].Print(false);out.background[i]=bg[i].Print(true);}out.inherited_background=ftxui::Color(ftxui::Color::Default).Print(true);out.inherited_dim=dim;out.block_exterior_dim=dim;return out;
}
ftxui::Element ordinary(std::string_view raw){return loom::ui::render_markdown("```mermaid-source\n"+std::string(raw)+"\n```\n");}
ftxui::Element footer(ftxui::Element body,bool dim){if(dim)body=body|ftxui::dim;return ftxui::vbox({std::move(body),ftxui::text(std::string(kSentinel))})|ftxui::color(theme::current_theme().palette->text)|ftxui::bgcolor(ftxui::Color::Default);}
auth::Frame capture(ftxui::Element node,int width,int height,er::OracleWork& work) {
    need(width>0&&width<=240&&height>0&&height<=1040,"resource: ER107 public capture bounds");const auto area=static_cast<std::size_t>(width*height);er::detail::pay(work,area,sizeof(auth::ObservedCell)+48,1024);
    auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(width),ftxui::Dimension::Fixed(height));ftxui::Render(screen,node);auth::Frame f;f.width=width;f.height=height;f.cells.reserve(area);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){const auto& p=screen.PixelAt(x,y);const auto fg=p.foreground_color.Print(false),bg=p.background_color.Print(true);
        need(p.character.size()<=16&&fg.size()<=128&&bg.size()<=128,"resource: ER107 actual captured glyph/color");er::detail::pay(work,p.character.size()+fg.size()+bg.size(),2,8);
        const auto other=static_cast<std::uint16_t>((p.blink?1:0)|(p.inverted?4:0)|(p.underlined?8:0)|(p.underlined_double?16:0)|(p.strikethrough?32:0));
        f.cells.push_back({p.character,fg,bg,p.bold,static_cast<std::uint16_t>(p.dim?2:0),other,p.hyperlink});
    }
    return f;
}
struct Observation {Frames frames;bool accepted=false;ftxui::Element retained;};
Observation observe(std::string_view raw,er::OracleWork& work,loom::ui::mermaid::LayoutRowsCache* cache=nullptr,bool dim=false) {
    auto fallback=ordinary(raw);auto rendered=cache?cache->render(raw,false,fallback):loom::ui::mermaid::render_mermaid(raw,fallback,false);Observation out;out.accepted=rendered.has_value();out.retained=rendered?*rendered:fallback;
    auto node=footer(out.retained,dim),plain=footer(ordinary(raw),dim);node->ComputeRequirement();plain->ComputeRequirement();
    const int height=std::min(1040,std::max(40,std::max(node->requirement().min_y,plain->requirement().min_y)+8));
    constexpr std::array<int,5> widths{240,40,240,240,40};for(std::size_t i=0;i<widths.size();++i)out.frames[i]=capture(i<3?node:plain,widths[i],height,work);return out;
}
std::string frame_bytes(const auth::Frame& f,er::OracleWork& w) {
    er::detail::pay(w,f.cells.size(),96,128);std::string bytes=std::to_string(f.width)+","+std::to_string(f.height)+";";
    for(const auto& c:f.cells){for(const auto* v:{&c.glyph,&c.foreground,&c.background}){er::detail::pay(w,v->size(),2,8);bytes+=std::to_string(v->size());bytes+=':';bytes+=*v;}bytes+=c.bold?'1':'0';bytes+=':';bytes+=std::to_string(c.dim)+","+std::to_string(c.other_attributes)+","+std::to_string(c.hyperlink)+";";}return bytes;
}
void publish(const Frames& frames,json::JsonMutVal record,er::OracleWork& work) {
    auto values=record.make_arr();for(const auto& frame:frames){auto value=record.make_obj();value.set("width",frame.width);value.set("height",frame.height);value.set("cell_sha256",hash(frame_bytes(frame,work)));auto runs=value.make_arr();
        for(std::size_t begin=0;begin<frame.cells.size();){std::size_t end=begin+1;while(end<frame.cells.size()&&frame.cells[end]==frame.cells[begin]){work.spend(8);++end;}work.spend(512);const auto& c=frame.cells[begin];auto run=value.make_arr();run.append(value.make_num(static_cast<std::int64_t>(end-begin)));for(const auto* s:{&c.glyph,&c.foreground,&c.background})run.append(value.make_str(*s));run.append(value.make_bool(c.bold));run.append(value.make_num(c.dim));run.append(value.make_num(c.other_attributes));run.append(value.make_num(c.hyperlink));runs.append(run);begin=end;}
        value.add("complete_cell_runs",runs);values.append(value);
    }record.add("five_frames",values);
}
struct Checked {int width=0,body_height=0,total_rows=0;};
Checked validate_observation(const Observation& actual,const auth::BoundCase& bound,const auth::Surfaces& colors,er::OracleWork& work) {
    need(actual.accepted,"physical: ER107 complete original rejected by public factory");need(actual.frames[0]==actual.frames[2],"physical: ER107 same-node width restoration");need(actual.frames[0]!=actual.frames[3],"physical: ER107 ordinary fallback cannot qualify as drawing");
    const auto wide=auth::check_complete_frame(actual.frames[0],bound,colors,kSentinel,work);
    static_cast<void>(auth::check_complete_frame(actual.frames[2],bound,colors,kSentinel,work));
    if(wide->intrinsic_width>40)need(actual.frames[1]==actual.frames[4],"physical: ER107 exact full ordinary narrow fallback including attributes");
    else{const auto narrow=auth::check_complete_frame(actual.frames[1],bound,colors,kSentinel,work);need(narrow->body_width==wide->body_width&&narrow->body_height==wide->body_height&&narrow->total_rows==wide->total_rows,"physical: ER107 narrow-fit complete source geometry extent");}
    return {wide->intrinsic_width,wide->body_height,wide->total_rows};
}
void report_identity(json::JsonMutVal r) {
    r.set("schema","er107_native_complete_source/v1");r.set("consumer","NativeER107");r.set("source_facts_sha256",er::kFactsSha);r.set("terminal_facts_sha256",auth::kProjectionSha);r.set("presentation_sha256",auth::kPresentationSha);r.set("contract_snapshot_sha256",auth::kContractSnapshotSha);r.set("source_lock_sha256",er::kSourceLock);r.set("manifest_sha256",er::kManifestSha);
    r.set("drawing_credit",0);r.set("formal_admission",false);r.set("native_report_input",false);r.set("foreign_assertions_certified",0);r.set("production_work_measured",false);r.set("authority_adaptation",auth::kAdaptation);r.set("authority_review_status",auth::kB3ReviewStatus);r.set("limitations",auth::kB3Limitations);
}
void write_report(std::string_view name,json::JsonMutDoc& doc) {
    const auto bytes=doc.to_string();need(bytes.size()<=512u*1024u*1024u,"resource: ER107 local lossless report512MiB");fs::create_directories(fs::path(LOOM_MERMAID_REPORT_DIR));const auto path=fs::path(LOOM_MERMAID_REPORT_DIR)/std::string(name);std::ofstream stream(path,std::ios::binary);need(stream.good(),"protocol: ER107 local report open");stream<<bytes;need(stream.good(),"protocol: ER107 local report write");
}
void record_modes(const Observation& u,const Observation& m,const Observation& h,json::JsonMutVal r,er::OracleWork& w){auto uncached=r.make_obj(),miss=r.make_obj(),hit=r.make_obj();publish(u.frames,uncached,w);publish(m.frames,miss,w);publish(h.frames,hit,w);r.add("uncached",uncached);r.add("cache_miss",miss);r.add("cache_hit",hit);}
} // namespace

TEST(MermaidEr107Source107, AllCompleteOriginalsOwnFifteenFramesBeforeBindingAndKeepZeroAdmission) {
    ThemeGuard guard;theme::set_theme(theme::ThemeVariant::Dark);Inputs input;json::JsonMutDoc report,compact;auto root=report.object(),verdict=compact.object(),records=root.make_arr(),summary=verdict.make_arr();std::size_t good=0,errors=0,all_captured=0,semantic=0,physical=0;
    for(std::size_t i=0;i<er::kPins.size();++i){const auto& pin=er::kPins[i];SCOPED_TRACE(pin.path);auto item=records.make_obj(),short_item=summary.make_obj();for(auto r:{item,short_item}){report_identity(r);r.set("path",pin.path);r.set("source_sha256",pin.raw_sha);r.set("manifest_global_ordinal",pin.global);r.set("local_ready",false);r.set("captures_before_binding",false);r.set("captures",0);}
        try{
            const auto raw=read(kFixtures/"mermaid_official/cases"/std::string(pin.path),er::kMaxRawBytes);er::OracleWork captures;loom::ui::mermaid::LayoutRowsCache cache(4,4*1024*1024);
            const auto uncached=observe(raw,captures),miss=observe(raw,captures,&cache),hit=observe(raw,captures,&cache);++all_captured;
            er::OracleWork publication;record_modes(uncached,miss,hit,item,publication);
            for(auto r:{item,short_item}){r.set("captures",15);r.set("captures_before_binding",true);r.set("factory_uncached_accepted",uncached.accepted);r.set("factory_cache_miss_accepted",miss.accepted);r.set("factory_cache_hit_accepted",hit.accepted);r.set("cache_hits",static_cast<std::int64_t>(cache.hits()));}
            er::OracleWork binding;const auto bound=input.bind(i,raw,15,binding);const auto colors=surfaces();std::array<er::OracleWork,3> checks;
            const auto status=validate_observation(uncached,bound,colors,checks[0]);static_cast<void>(validate_observation(miss,bound,colors,checks[1]));static_cast<void>(validate_observation(hit,bound,colors,checks[2]));
            need(cache.hits()>0,"protocol: ER107 genuine public cache hit");need(uncached.retained.get()!=miss.retained.get()&&miss.retained.get()!=hit.retained.get(),"physical: ER107 fresh public cache elements");need(uncached.frames==miss.frames&&miss.frames==hit.frames,"physical: ER107 complete fifteen-plane uncached/miss/hit equality");
            er::OracleWork mutations;const auto controls=auth::exercise(uncached.frames[0],bound,colors,kSentinel,mutations);semantic+=controls.semantic;physical+=controls.physical;++good;
            for(auto r:{item,short_item}){r.set("local_ready",true);r.set("width",status.width);r.set("body_height",status.body_height);r.set("total_rows",status.total_rows);r.set("complete_model_sha256",auth::kPins[i].complete_sha);r.set("terminal_model_sha256",auth::kPins[i].model_sha);r.set("source_model_sha256",pin.model_sha);r.set("source_witnesses_sha256",pin.witness_sha);r.set("manifest_record_sha256",pin.manifest_sha);r.set("source_binding_sha256",auth::kPins[i].binding_sha);r.set("all_fifteen_frames_equal_across_modes",true);r.set("semantic_mutants_rejected",static_cast<std::int64_t>(controls.semantic));r.set("physical_mutants_rejected",static_cast<std::int64_t>(controls.physical));auto skipped=r.make_arr();for(const auto& v:controls.skipped)skipped.append(r.make_str(v));r.add("mutants_not_applicable",skipped);}
        }catch(const std::exception& e){++errors;for(auto r:{item,short_item}){r.set("error",e.what());r.set("resource_failure",auth::resource(e.what()));}ADD_FAILURE()<<e.what();}
        records.append(item);summary.append(short_item);
    }
    bool aggregate=false;try{input.finish(all_captured);aggregate=true;}catch(const std::exception& e){ADD_FAILURE()<<e.what();root.set("aggregate_error",e.what());verdict.set("aggregate_error",e.what());}
    for(auto r:{root,verdict}){report_identity(r);r.set("originals",107);r.set("all_source_captures_completed",static_cast<std::int64_t>(all_captured));r.set("aggregate_binding_after_all_captures",aggregate);r.set("local_ready",static_cast<std::int64_t>(aggregate&&errors==0&&good==107?good:0));r.set("scene_positives_before_aggregate",static_cast<std::int64_t>(good));r.set("errors",static_cast<std::int64_t>(errors));r.set("semantic_mutants_rejected",static_cast<std::int64_t>(semantic));r.set("physical_mutants_rejected",static_cast<std::int64_t>(physical));r.set("independent_stage_oracle_cap",static_cast<std::int64_t>(auth::kRootPhaseWorkCap));}
    root.add("cases",records);verdict.add("cases",summary);report.set_root(root);compact.set_root(verdict);write_report("mermaid-er107-native.json",report);write_report("mermaid-er107-native-verdict.json",compact);EXPECT_EQ(all_captured,107u);EXPECT_TRUE(aggregate);EXPECT_EQ(good,107u);EXPECT_EQ(errors,0u);
}

TEST(MermaidEr107Source107, OwnedPositivePrecedesRawAndRecomputedModelGuards) {
    ThemeGuard guard;theme::set_theme(theme::ThemeVariant::Dark);Inputs input;const auto raw=read(kFixtures/"mermaid_official/cases"/std::string(er::kPins[0].path),er::kMaxRawBytes);er::OracleWork captures;loom::ui::mermaid::LayoutRowsCache cache(4,4*1024*1024);
    const auto uncached=observe(raw,captures),miss=observe(raw,captures,&cache),hit=observe(raw,captures,&cache);er::OracleWork binding;const auto good=input.bind(0,raw,15,binding);std::array<er::OracleWork,3> checks;validate_observation(uncached,good,surfaces(),checks[0]);validate_observation(miss,good,surfaces(),checks[1]);validate_observation(hit,good,surfaces(),checks[2]);
    auto rejects=[&](auto&& run){bool rejected=false;try{run();}catch(const std::exception& e){EXPECT_FALSE(auth::resource(e.what()))<<e.what();rejected=true;}EXPECT_TRUE(rejected);};
    rejects([&]{er::OracleWork w;input.bind(0,raw+" ",15,w);});
    auto& d=*input.docs;json::JsonMutDoc changed;changed.set_root(changed.copy_val(d.terminal.root()));auto c=changed.root_mut().get("cases").at(0);c.get("terminal_model").get("whole_scene_model").get("entities").at(0).set("label","FORGED");auto intermediate=parse(changed.to_string());er::OracleWork digest;c.set("terminal_model_sha256",hash(auth::canonical(intermediate.root().get("cases").at(0).get("terminal_model"),digest)));auto mutant=parse(changed.to_string());
    rejects([&]{er::OracleWork w;auth::bind_case(d.source.root(),mutant.root(),d.manifest.root(),d.prior.root(),auth::kPins[0].path,raw,w);});
    er::OracleWork exhausted;exhausted.left=0;bool resource=false;try{input.bind(0,raw,15,exhausted);}catch(const std::exception& e){resource=auth::resource(e.what());}EXPECT_TRUE(resource); // never a semantic mutant success.
}

TEST(MermaidEr107Source107, FreshPublicThemeSnapshotsAndInheritedDimPreserveCompleteSourceRoles) {
    ThemeGuard guard;Inputs input;const auto raw=read(kFixtures/"mermaid_official/cases"/std::string(er::kPins[0].path),er::kMaxRawBytes);loom::ui::mermaid::LayoutRowsCache cache(4,4*1024*1024);
    constexpr std::array<theme::ThemeVariant,7> variants{theme::ThemeVariant::Dark,theme::ThemeVariant::Light,theme::ThemeVariant::DarkDaltonized,theme::ThemeVariant::LightDaltonized,theme::ThemeVariant::Monochrome,theme::ThemeVariant::LightAnsi,theme::ThemeVariant::DarkAnsi};
    for(const auto variant:variants){theme::set_theme(variant);for(const bool dim:{false,true}){er::OracleWork captures;const auto u=observe(raw,captures,nullptr,dim),m=observe(raw,captures,&cache,dim),h=observe(raw,captures,&cache,dim);er::OracleWork binding;const auto b=input.bind(0,raw,15,binding);std::array<er::OracleWork,3> checking;validate_observation(u,b,surfaces(dim?2:0),checking[0]);validate_observation(m,b,surfaces(dim?2:0),checking[1]);validate_observation(h,b,surfaces(dim?2:0),checking[2]);EXPECT_EQ(u.frames,m.frames);EXPECT_EQ(m.frames,h.frames);EXPECT_NE(m.retained.get(),h.retained.get());}}
}

TEST(MermaidEr107Source107, Description30HtmlLabelsControlUsesApprovedWholeSourceProfile) {
    // Authored regression for immutable description30 row13. This is not a
    // 108th ER107 original and cannot change either frozen inventory or credit.
    constexpr std::string_view raw="---\ndescription: >-\n  Text\nconfig:\n  htmlLabels: false\n---\nerDiagram\nA ||--o{ B : owns\n";
    constexpr std::string_view raw_sha="36415e62c2e2f84c5e349b9e23900681c6f5841460e8b142c7c2e5fda1d89a2a";
    ASSERT_EQ(raw.size(),87u);ASSERT_EQ(hash(raw),raw_sha);
    ThemeGuard guard;theme::set_theme(theme::ThemeVariant::Dark);er::OracleWork captures;loom::ui::mermaid::LayoutRowsCache cache(4,4*1024*1024);
    const auto uncached=observe(raw,captures),miss=observe(raw,captures,&cache),hit=observe(raw,captures,&cache);
    // All15 actual public frames now exist. Only now construct the literal
    // Model from raw source and the approved ER107 presentation contract.
    auth::BoundCase bound;bound.source.path="authored/description30-row13-htmlLabels-profile";
    auto& p=bound.projection;p.theme="redux-color";p.look="neo";p.categorical=true;p.scene.family="er";p.scene.direction="TB";
    for(const auto* id:{"A","B"}){auth::relation::Entity e;e.id=id;e.label=id;e.shape="box";e.headers={{id}};p.scene.entities.push_back(std::move(e));}
    p.scene.relations.push_back({"A","B","owns","card:one","card:zero-many","solid",0});
    p.roles={{"A","",6,6,0,0,"Purple","Purple"},{"B","",5,5,1,1,"Cyan","Cyan"}};
    // Description Text is validated documentation-only metadata, not a notice.
    // These complete notice strings and every32-cell row/joiner are hand-bound
    // to source config plus fresh-owner redux-color/neo defaults. No frame
    // content, producer coordinate or producer theme code supplies them.
    p.notices={
        "ER native projection v1",
        "config.htmlLabels=false",
        "effective theme=redux-color; look=neo",
        "look=neo -> native orthogonal; roughness/relief not reproduced",
        "htmlLabels=false -> native Markdown subset",
        "theme RGB/font/CSS -> terminal roles; foreign assertions pending",
        "entity A slot=0 border=Purple fill=Purple",
        "entity B slot=1 border=Cyan fill=Cyan"};
    p.notice_rows={
        {{"ER native projection v1",""}},
        {{"config.htmlLabels=false",""}},
        {{"effective theme=redux-color;"," "},{"look=neo",""}},
        {{"look=neo -> native orthogonal;"," "},{"roughness/relief not reproduced",""}},
        {{"htmlLabels=false -> native"," "},{"Markdown subset",""}},
        {{"theme RGB/font/CSS -> terminal"," "},{"roles; foreign assertions"," "},{"pending",""}},
        {{"entity A slot=0 border=Purple"," "},{"fill=Purple",""}},
        {{"entity B slot=1 border=Cyan"," "},{"fill=Cyan",""}}};
    ASSERT_EQ(p.notices.size(),8u);ASSERT_EQ(p.notice_rows.size(),p.notices.size());
    for(std::size_t i=0;i<p.notices.size();++i){std::string joined;for(const auto& row:p.notice_rows[i]){joined+=row.text+row.joiner;EXPECT_LE(row.text.size(),32u);}EXPECT_EQ(joined,p.notices[i]);er::OracleWork wrap;EXPECT_EQ(auth::wrap(p.notices[i],wrap),p.notice_rows[i]);}
    std::array<er::OracleWork,3> checks;const auto status=validate_observation(uncached,bound,surfaces(),checks[0]);validate_observation(miss,bound,surfaces(),checks[1]);validate_observation(hit,bound,surfaces(),checks[2]);
    ASSERT_GT(cache.hits(),0u);EXPECT_EQ(uncached.frames,miss.frames);EXPECT_EQ(miss.frames,hit.frames);EXPECT_NE(uncached.retained.get(),miss.retained.get());EXPECT_NE(miss.retained.get(),hit.retained.get());
    er::OracleWork controls_work;const auto controls=auth::exercise(uncached.frames[0],bound,surfaces(),kSentinel,controls_work);EXPECT_GT(controls.semantic,0u);EXPECT_GT(controls.physical,0u);
    // Keep these non-scoring captures separate from the strict107 report roots.
    json::JsonMutDoc report;auto root=report.object();report_identity(root);root.set("schema","er107_native_authored_description30_regression/v1");root.set("originals",0);root.set("drawing_credit",0);root.set("source_sha256",raw_sha);root.set("full_source",raw);root.set("captures",15);root.set("captures_before_literal_model",true);root.set("description_value","Text");root.set("description_visible",false);root.set("width",status.width);root.set("semantic_mutants_rejected",static_cast<std::int64_t>(controls.semantic));root.set("physical_mutants_rejected",static_cast<std::int64_t>(controls.physical));
    er::OracleWork publication;record_modes(uncached,miss,hit,root,publication);report.set_root(root);write_report("mermaid-er107-native-description30-control.json",report);
    RecordProperty("source_sha256",std::string(raw_sha));RecordProperty("captures_before_model",15);RecordProperty("drawing_credit",0);
}

TEST(MermaidEr107Source107, QuotedRoleWhitespaceUsesTrimmedDisplayAndExactBlankDisclosure) {
    // Two authored controls, not additional originals. Raw quoted roles retain
    // their bytes for disclosure; display strips only leading/trailing SP/HT/CR.
    struct Control {std::string_view raw,sha,raw_role,display;std::size_t bytes;};
    constexpr std::array<Control,2> controls{{
        {"---\nconfig:\n  look: neo\n---\nerDiagram\nA ||--o{ B : \"  owns  \"\n",
         "e2de8d1bbafdbd970f7650e260e673d5ce69cb5021e9bf3904d98acc01e52355","  owns  ","owns",62},
        {"---\nconfig:\n  look: neo\n---\nerDiagram\nA ||--o{ B : \"   \"\n",
         "f571ea90d81cdefae7c929650945e3d631e23ca64df69b5dcd16e8abc683fa4e","   ","",57}
    }};
    ThemeGuard guard;theme::set_theme(theme::ThemeVariant::Dark);
    for(const auto& control:controls){
        SCOPED_TRACE(control.sha);ASSERT_EQ(control.raw.size(),control.bytes);ASSERT_EQ(hash(control.raw),control.sha);
        er::OracleWork captures;loom::ui::mermaid::LayoutRowsCache cache(4,4*1024*1024);
        const auto uncached=observe(control.raw,captures),miss=observe(control.raw,captures,&cache),hit=observe(control.raw,captures,&cache);
        // Construct typed expectations only after this source's own15 frames.
        auth::BoundCase bound;bound.source.path="authored/quoted-role-whitespace/"+std::string(control.sha);
        auto& p=bound.projection;p.theme="redux-color";p.look="neo";p.categorical=true;p.scene.family="er";p.scene.direction="TB";
        for(const auto* id:{"A","B"}){auth::relation::Entity e;e.id=id;e.label=id;e.shape="box";e.headers={{id}};p.scene.entities.push_back(std::move(e));}
        p.scene.relations.push_back({"A","B",std::string(control.display),"card:one","card:zero-many","solid",0});
        p.roles={{"A","",6,6,0,0,"Purple","Purple"},{"B","",5,5,1,1,"Cyan","Cyan"}};
        // Handwritten source/contract expectations, including exact32-cell
        // last-fitting-space rows. No producer or captured route supplies art.
        p.notices={
            "ER native projection v1",
            "config.look=\"neo\"",
            "effective theme=redux-color; look=neo",
            "look=neo -> native orthogonal; roughness/relief not reproduced",
            "theme RGB/font/CSS -> terminal roles; foreign assertions pending",
            "entity A slot=0 border=Purple fill=Purple",
            "entity B slot=1 border=Cyan fill=Cyan"};
        p.notice_rows={
            {{"ER native projection v1",""}},
            {{"config.look=\"neo\"",""}},
            {{"effective theme=redux-color;"," "},{"look=neo",""}},
            {{"look=neo -> native orthogonal;"," "},{"roughness/relief not reproduced",""}},
            {{"theme RGB/font/CSS -> terminal"," "},{"roles; foreign assertions"," "},{"pending",""}},
            {{"entity A slot=0 border=Purple"," "},{"fill=Purple",""}},
            {{"entity B slot=1 border=Cyan"," "},{"fill=Cyan",""}}};
        if(control.display.empty()){
            p.notices.push_back("edge 0 raw-role=\"   \" -> no visible words");
            p.notice_rows.push_back({{"edge 0 raw-role=\"   \" -> no"," "},{"visible words",""}});
        }
        ASSERT_EQ(p.notices.size(),control.display.empty()?8u:7u);ASSERT_EQ(p.notice_rows.size(),p.notices.size());
        for(std::size_t i=0;i<p.notices.size();++i){std::string joined;for(const auto& row:p.notice_rows[i]){joined+=row.text+row.joiner;EXPECT_LE(row.text.size(),32u);}EXPECT_EQ(joined,p.notices[i]);er::OracleWork wrapping;EXPECT_EQ(auth::wrap(p.notices[i],wrapping),p.notice_rows[i]);}
        std::array<er::OracleWork,3> checking;const auto status=validate_observation(uncached,bound,surfaces(),checking[0]);validate_observation(miss,bound,surfaces(),checking[1]);validate_observation(hit,bound,surfaces(),checking[2]);
        ASSERT_GT(cache.hits(),0u);EXPECT_EQ(uncached.frames,miss.frames);EXPECT_EQ(miss.frames,hit.frames);EXPECT_NE(uncached.retained.get(),miss.retained.get());EXPECT_NE(miss.retained.get(),hit.retained.get());
        er::OracleWork mutation_work;const auto mutants=auth::exercise(uncached.frames[0],bound,surfaces(),kSentinel,mutation_work);EXPECT_GT(mutants.semantic,0u);EXPECT_GT(mutants.physical,0u);
        // Additionally challenge exactly the regression: padded visible text
        // must not replace trimmed words, and a blank role must not invent words.
        auto wrong_display=bound;wrong_display.projection.scene.relations[0].label=control.display.empty()?"owns":std::string(control.raw_role);
        er::OracleWork wrong_work;bool rejected=false;
        try{static_cast<void>(auth::check_complete_frame(uncached.frames[0],wrong_display,surfaces(),kSentinel,wrong_work));}
        catch(const std::exception& error){if(std::string_view(error.what()).starts_with("resource:"))throw;rejected=true;}
        EXPECT_TRUE(rejected);
        // Separate non-scoring, lossless reports; never alter the107 root.
        json::JsonMutDoc report;auto root=report.object();report_identity(root);root.set("schema","er107_native_authored_quoted_role_regression/v1");root.set("originals",0);root.set("source_sha256",control.sha);root.set("full_source",control.raw);root.set("raw_role",control.raw_role);root.set("display_role",control.display);root.set("captures",15);root.set("captures_before_literal_model",true);root.set("width",status.width);root.set("semantic_mutants_rejected",static_cast<std::int64_t>(mutants.semantic));root.set("physical_mutants_rejected",static_cast<std::int64_t>(mutants.physical));root.set("wrong_display_rejected",rejected);
        er::OracleWork publication;record_modes(uncached,miss,hit,root,publication);report.set_root(root);write_report(control.display.empty()?"mermaid-er107-native-quoted-blank-control.json":"mermaid-er107-native-quoted-trim-control.json",report);
    }
    RecordProperty("authored_source_controls",2);RecordProperty("captures_before_each_model",15);RecordProperty("drawing_credit",0);
}
