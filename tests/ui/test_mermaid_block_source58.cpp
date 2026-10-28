// Native complete-original consumer; the required testing facade exposes the
// public render/cache API. Source-owned expectations bind after own captures.
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
#include "mermaid_block58_authority.hpp"

namespace {
namespace block=loom::test::mermaid_block58;
namespace auth=block::authority;
namespace json=loom::utils::json;
namespace theme=loom::ui::design::theme;
namespace fs=std::filesystem;
const fs::path kFixtures=fs::path(LOOM_TESTS_DIR)/"fixtures";
constexpr std::string_view kSentinel="AFTER_BLOCK58";
using Frames=std::array<auth::Surface,5>;
void need(bool yes,std::string_view why){auth::require(yes,why);}
std::string read(const fs::path& path,std::size_t cap=block::kMaxFactBytes) {
    need(!fs::is_symlink(fs::symlink_status(path))&&fs::is_regular_file(path)&&fs::file_size(path)<=cap,"resource: Block58 bounded input");
    std::ifstream stream(path,std::ios::binary);need(stream.good(),"protocol: Block58 input open");
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
std::string hash(std::string_view text){return loom::utils::crypto::sha256(text);}
struct Inputs {
    // Hash checks are byte checks only, never an early typed case binding.
    std::string source_bytes,terminal_bytes,manifest_bytes,boundary_bytes;
    json::JsonDoc source,terminal,manifest,boundary;
    Inputs():source_bytes(read(kFixtures/"mermaid_native/block58_complete_source_proposal.v1.json")),
        terminal_bytes(read(kFixtures/"mermaid_native/block58_terminal_projection_proposal.v1.json")),
        manifest_bytes(read(kFixtures/"mermaid_official/manifest.json")),
        boundary_bytes(read(kFixtures/"mermaid_native/block58_boundary_contract.v1.json",256*1024)) {
        block::OracleWork work;block::verify_asset_bytes(source_bytes,terminal_bytes,work);
        need(hash(manifest_bytes)==block::kManifestSha,"source: exact official manifest bytes");
        block::boundary::verify_bytes(boundary_bytes);
        auth::verify_design_bytes(read(fs::path(LOOM_TESTS_DIR).parent_path()/"docs/design/mermaid-block58-terminal-contract.md",65536));
        auto s=json::parse(source_bytes),t=json::parse(terminal_bytes),m=json::parse(manifest_bytes),b=json::parse(boundary_bytes);
        need(s&&t&&m&&b,"protocol: immutable authority JSON");
        source=std::move(*s);terminal=std::move(*t);manifest=std::move(*m);boundary=std::move(*b);
    }
    block::BoundFacts bind(std::string_view path,std::string_view raw,block::OracleWork& work) const {
        return block::bind(source.root(),terminal.root(),path,raw,manifest.root(),work);
    }
};
struct ThemeGuard {
    theme::Theme original=theme::current_theme();
    ~ThemeGuard(){theme::set_theme(original);}
};
auth::Palette palette() {
    const auto& p=*theme::current_theme().palette;
    const std::array<ftxui::Color,7> fg{p.text,p.text,p.danger,p.warning,p.success,p.info,p.accent_purple};
    const std::array<ftxui::Color,7> bg{ftxui::Color::Default,p.surface_tint_2,p.danger_tint_bg,p.warning_tint_bg,p.success_tint_bg,p.info_tint_bg,p.surface_tint_4};
    auth::Palette out;for(std::size_t i=0;i<7;++i){out.foreground[i]=fg[i].Print(false);out.background[i]=bg[i].Print(true);}return out;
}
ftxui::Element ordinary(std::string_view source) {
    // The existing m-prefix highlighter makes this the foreground-equivalent
    // source fallback. A plain text fence would erase source syntax attributes.
    return loom::ui::render_markdown("```mermaid-source\n"+std::string(source)+"\n```\n");
}
ftxui::Element with_sentinel(ftxui::Element body) {
    return ftxui::vbox({std::move(body),ftxui::text(std::string(kSentinel))})|
        ftxui::color(theme::current_theme().palette->text)|ftxui::bgcolor(ftxui::Color::Default);
}
auth::Surface capture(ftxui::Element node,int width,int height,block::OracleWork& work) {
    need(width>0&&width<=240&&height>0&&height<=1040,"resource: Block58 capture dimensions");
    const auto area=static_cast<std::size_t>(width*height);work.spend(area*(sizeof(auth::ObservedPixel)+32)+512);
    auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(width),ftxui::Dimension::Fixed(height));
    ftxui::Render(screen,node);auth::Surface out;out.width=width;out.height=height;out.cells.reserve(area);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        const auto& pixel=screen.PixelAt(x,y);
        const auto other=static_cast<std::uint16_t>((pixel.blink?1:0)|(pixel.dim?2:0)|(pixel.inverted?4:0)|
            (pixel.underlined?8:0)|(pixel.underlined_double?16:0)|(pixel.strikethrough?32:0));
        const auto fg=pixel.foreground_color.Print(false),bg=pixel.background_color.Print(true);
        need(pixel.character.size()<=4096&&fg.size()<=128&&bg.size()<=128,"resource: captured glyph/color bytes");
        work.spend(pixel.character.size()+fg.size()+bg.size());
        out.cells.push_back({pixel.character,fg,bg,pixel.bold,other,pixel.hyperlink});
    }
    return out;
}
struct Observation {Frames frames;bool factory_accepted=false;};
Observation observe(std::string_view raw,block::OracleWork& work,loom::ui::mermaid::LayoutRowsCache* cache=nullptr) {
    auto fallback=ordinary(raw);
    auto result=cache?cache->render(raw,false,fallback):loom::ui::mermaid::render_mermaid(raw,fallback,false);
    Observation out;out.factory_accepted=result.has_value();
    auto node=with_sentinel(result?*result:fallback);auto plain=with_sentinel(ordinary(raw));
    node->ComputeRequirement();plain->ComputeRequirement();
    // Only public Element requirements influence capture height; no Model yet.
    const int height=std::min(1040,std::max(40,std::max(node->requirement().min_y,plain->requirement().min_y)+8));
    constexpr std::array<int,5> widths{240,40,240,240,40};
    for(std::size_t i=0;i<5;++i)out.frames[i]=capture(i<3?node:plain,widths[i],height,work);
    return out;
}
std::string cell_bytes(const auth::Surface& frame) {
    std::string bytes;
    for(const auto& p:frame.cells) {
        for(const auto* s:{&p.glyph,&p.foreground,&p.background}){bytes+=std::to_string(s->size());bytes+=':';bytes+=*s;}
        bytes+=p.bold?'1':'0';bytes+=':';bytes+=std::to_string(p.other_attributes);bytes+=':';bytes+=std::to_string(p.hyperlink);bytes+=';';
    }
    return bytes;
}
void publish(const Frames& frames,json::JsonMutVal record) {
    auto values=record.make_arr();
    for(const auto& frame:frames) {
        auto value=record.make_obj();value.set("width",frame.width);value.set("height",frame.height);value.set("cell_sha256",hash(cell_bytes(frame)));
        // Lossless local evidence, including spaces, concrete colors and bold.
        auto runs=value.make_arr();
        for(std::size_t begin=0;begin<frame.cells.size();) {
            std::size_t end=begin+1;while(end<frame.cells.size()&&frame.cells[end]==frame.cells[begin])++end;
            const auto& p=frame.cells[begin];auto run=value.make_arr();run.append(value.make_num(static_cast<std::int64_t>(end-begin)));
            for(const auto* s:{&p.glyph,&p.foreground,&p.background})run.append(value.make_str(*s));
            run.append(value.make_bool(p.bold));run.append(value.make_num(p.other_attributes));run.append(value.make_num(p.hyperlink));runs.append(run);begin=end;
        }
        value.add("complete_cell_runs",runs);values.append(value);
    }
    record.add("five_frames",values);
}
void require_sentinel_and_exterior(const auth::Surface& surface,const auth::Expected& expected,const auth::Palette& colors) {
    const int stop=static_cast<int>(expected.rows.size());
    need(stop<surface.height,"physical: following sentinel clipped");
    for(int y=0;y<surface.height;++y)for(int x=0;x<surface.width;++x) {
        if(y<stop&&static_cast<std::size_t>(x)<expected.rows[static_cast<std::size_t>(y)].size())continue;
        const auto& got=surface.cells[static_cast<std::size_t>(y*surface.width+x)];
        const auto glyph=y==stop&&static_cast<std::size_t>(x)<kSentinel.size()?std::string(kSentinel.substr(static_cast<std::size_t>(x),1)):" ";
        need(got.glyph==glyph&&got.foreground==colors.foreground[0]&&got.background==colors.background[0]&&!got.bold&&got.other_attributes==0&&got.hyperlink==0,
             "physical: exact following sentinel/exterior blank attributes");
    }
}
void validate_observation(const Observation& observation,const auth::Expected& expected,const auth::Palette& colors,block::OracleWork& work) {
    need(observation.factory_accepted,"physical: complete original rejected by public factory");
    const auto& f=observation.frames;need(f[0]==f[2],"physical: same-node width restoration");
    need(f[0]!=f[3],"physical: original is ordinary source fallback");
    for(const auto i:{0u,1u,2u}) {
        if(i==1&&expected.geometry.metrics.width>40){need(f[1]==f[4],"physical: narrow complete ordinary fallback attributes");continue;}
        need(auth::validate(f[i],expected,colors,0,0,work).empty(),"physical: exact complete glyph/role/bold plane");
        require_sentinel_and_exterior(f[i],expected,colors);
    }
}
void write_report(std::string_view name,json::JsonMutDoc& report) {
    const auto path=fs::path(LOOM_MERMAID_REPORT_DIR)/std::string(name);
    std::ofstream stream(path,std::ios::binary);need(stream.good(),"protocol: local report open");stream<<report.to_string();need(stream.good(),"protocol: local report write");
}
} // namespace

TEST(MermaidBlockSource58, CompleteOriginalsOwnFiveFramesAndFullPlanesWithoutAdmission) {
    ThemeGuard guard;theme::set_theme(theme::ThemeVariant::Dark);Inputs input;
    block::OracleWork positives;ASSERT_EQ(auth::boundary_positives(input.boundary.root(),positives),36u);
    json::JsonMutDoc report,compact;auto root=report.object(),verdict=compact.object(),records=root.make_arr(),summary=verdict.make_arr();
    std::size_t good=0,errors=0,semantic=0,physical=0;
    for(const auto& pin:block::kPins) {
        SCOPED_TRACE(pin.path);auto item=records.make_obj();auto short_item=summary.make_obj();
        item.set("path",pin.path);short_item.set("path",pin.path);item.set("source_sha256",pin.raw_sha);
        try {
            const auto raw=read(kFixtures/"mermaid_official/cases"/std::string(pin.path),block::kMaxRawBytes);
            block::OracleWork captures;captures.left=768'000'000;
            loom::ui::mermaid::LayoutRowsCache cache(4,4*1024*1024);
            // All public observations, including a genuine cache hit, precede
            // the first per-source typed Model binding and expectation build.
            const auto uncached=observe(raw,captures);const auto cached=observe(raw,captures,&cache);const auto hit=observe(raw,captures,&cache);
            publish(uncached.frames,item);auto cached_record=item.make_obj();publish(cached.frames,cached_record);item.add("cached",cached_record);auto hit_record=item.make_obj();publish(hit.frames,hit_record);item.add("cache_hit",hit_record);
            need(cache.hits()>0,"protocol: public cached hit not exercised");
            block::OracleWork binding;const auto facts=input.bind(pin.path,raw,binding);
            block::OracleWork deriving;const auto expected=auth::derive(facts,raw.size(),deriving);const auto colors=palette();
            block::OracleWork checking;checking.left=768'000'000;
            validate_observation(uncached,expected,colors,checking);validate_observation(cached,expected,colors,checking);validate_observation(hit,expected,colors,checking);
            need(uncached.frames==cached.frames&&cached.frames==hit.frames,"physical: uncached/cache miss/hit all five planes");
            const auto mutations=auth::exercise(uncached.frames[0],expected,colors,0,0,facts.terminal_model,checking);
            semantic+=mutations.semantic;physical+=mutations.physical;++good;
            for(auto record:{item,short_item}) {
                record.set("local_ready",true);record.set("captures_before_binding",true);record.set("captures",15);
                record.set("source_sha256",pin.raw_sha);record.set("complete_model_sha256",pin.source_model_sha);record.set("terminal_model_sha256",pin.terminal_model_sha);
                record.set("manifest_global_ordinal",pin.global);record.set("source_witnesses_sha256",pin.witness_sha);record.set("manifest_record_sha256",pin.manifest_record_sha);
                record.set("terminal_source_binding_sha256",pin.terminal_source_binding_sha);
                record.set("narrow_exact_ordinary",uncached.frames[1]==uncached.frames[4]);record.set("narrow_geometry_good",expected.geometry.metrics.width<=40);
                record.set("uncached_cached_five_planes_equal",true);record.set("cache_hit_observed",true);
                record.set("semantic_mutations_rejected",static_cast<std::int64_t>(mutations.semantic));record.set("physical_mutations_rejected",static_cast<std::int64_t>(mutations.physical));
                record.set("width",expected.geometry.metrics.width);record.set("height",expected.geometry.metrics.height);
            }
        } catch(const std::exception& error) {
            ++errors;item.set("error",error.what());short_item.set("error",error.what());short_item.set("local_ready",false);ADD_FAILURE()<<error.what();
        }
        records.append(item);summary.append(short_item);
    }
    for(auto record:{root,verdict}) {
        record.set("schema","block58_native_complete_original/v1");record.set("source_facts_sha256",block::kSourceFactsSha);record.set("terminal_facts_sha256",block::kTerminalFactsSha);
        record.set("source_lock_sha256",block::kSourceLock);record.set("reviewed_design_sha256",auth::kReviewedDesignSha);record.set("boundary_sha256",block::boundary::kAssetSha);
        record.set("originals",58);record.set("local_ready",static_cast<std::int64_t>(good));record.set("errors",static_cast<std::int64_t>(errors));
        record.set("semantic_mutations_rejected",static_cast<std::int64_t>(semantic));record.set("physical_mutations_rejected",static_cast<std::int64_t>(physical));
        record.set("drawing_credit",0);record.set("formal_admission",false);record.set("native_report_input",false);record.set("foreign_assertions_certified",0);record.set("authority_adaptation",auth::kAdaptation);record.set("authority_review_status",auth::kAuthorityReviewStatus);record.set("limitations",auth::kLimitations);
    }
    root.add("cases",records);report.set_root(root);verdict.add("cases",summary);compact.set_root(verdict);
    write_report("mermaid-block58-native.json",report);write_report("mermaid-block58-native-verdict.json",compact);
    EXPECT_EQ(good,58u);EXPECT_EQ(errors,0u);
}

TEST(MermaidBlockSource58, SourceAndRecomputedModelMutantsCannotBypassLiteralPins) {
    Inputs input;block::OracleWork positives;ASSERT_EQ(auth::boundary_positives(input.boundary.root(),positives),36u);
    const auto pin=block::kPins.front();const auto raw=read(kFixtures/"mermaid_official/cases"/std::string(pin.path),block::kMaxRawBytes);
    // Good public complete original first, then immutable source/binder controls.
    block::OracleWork captures;captures.left=768'000'000;const auto observed=observe(raw,captures);
    block::OracleWork binding;const auto facts=input.bind(pin.path,raw,binding);
    block::OracleWork deriving;const auto expected=auth::derive(facts,raw.size(),deriving);block::OracleWork checking;
    validate_observation(observed,expected,palette(),checking);
    const auto rejects=[&](auto action) {
        bool rejected=false;try{action();}catch(const std::exception& e){EXPECT_FALSE(auth::resource_error(e.what()))<<e.what();rejected=true;}EXPECT_TRUE(rejected);
    };
    rejects([&]{block::OracleWork w;static_cast<void>(input.bind(pin.path,raw+" ",w));});
    for(const auto key:{"manifest_global_ordinal","source_sha256","source_git_blob","source_bytes","complete_model_sha256","source_witnesses_sha256"}) {
        json::JsonMutDoc altered;auto root=altered.copy_val(input.source.root());altered.set_root(root);auto c=root.get("cases").at(0);
        if(std::string_view(key)=="manifest_global_ordinal"||std::string_view(key)=="source_bytes")c.set(key,c.get(key).as_int()+1);else c.set(key,"wrong");
        auto parsed=json::parse(altered.to_string());ASSERT_TRUE(parsed);
        rejects([&]{block::OracleWork w;static_cast<void>(block::bind(parsed->root(),input.terminal.root(),pin.path,raw,input.manifest.root(),w));});
    }
    json::JsonMutDoc altered;auto root=altered.copy_val(input.terminal.root());altered.set_root(root);
    auto c=root.get("cases").at(0),model=c.get("proposed_terminal_model");model.get("nodes").at(0).set("retained_span",1);
    auto interim=json::parse(altered.to_string());ASSERT_TRUE(interim);const auto parsed_model=interim->root().get("cases").at(0).get("proposed_terminal_model");block::OracleWork canonical_work;
    c.set("proposed_terminal_model_sha256",hash(block::detail::canonical(parsed_model,canonical_work,block::detail::RealScope::terminal_model)));
    auto parsed=json::parse(altered.to_string());ASSERT_TRUE(parsed);
    rejects([&]{block::OracleWork w;static_cast<void>(block::bind(input.source.root(),parsed->root(),pin.path,raw,input.manifest.root(),w));});
    block::OracleWork empty;empty.left=0;bool exhausted=false;
    try{static_cast<void>(input.bind(pin.path,raw,empty));}catch(const std::exception& e){exhausted=auth::resource_error(e.what());}EXPECT_TRUE(exhausted);
}

TEST(MermaidBlockSource58, ThemeChangesUseFreshOwnedNodesWithTheSameSourceGeometry) {
    ThemeGuard guard;Inputs input;block::OracleWork positives;ASSERT_EQ(auth::boundary_positives(input.boundary.root(),positives),36u);
    constexpr std::array<theme::ThemeVariant,7> variants{theme::ThemeVariant::Dark,theme::ThemeVariant::Light,
        theme::ThemeVariant::DarkDaltonized,theme::ThemeVariant::LightDaltonized,theme::ThemeVariant::Monochrome,theme::ThemeVariant::LightAnsi,theme::ThemeVariant::DarkAnsi};
    // Every original participates; no theme/source-caption census is a producer
    // allowlist. Each theme/flag group owns its fresh complete public frames.
    for(const auto& pin:block::kPins) {
        SCOPED_TRACE(pin.path);const auto raw=read(kFixtures/"mermaid_official/cases"/std::string(pin.path),block::kMaxRawBytes);
        loom::ui::mermaid::LayoutRowsCache cache(4,4*1024*1024);
        block::OracleWork initial_capture;initial_capture.left=768'000'000;const auto first=observe(raw,initial_capture,&cache);
        block::OracleWork binding;const auto facts=input.bind(pin.path,raw,binding);block::OracleWork deriving;const auto expected=auth::derive(facts,raw.size(),deriving);
        block::OracleWork initial_check;validate_observation(first,expected,palette(),initial_check);
        for(const auto variant:variants)for(int flags=0;flags<4;++flags) {
            theme::Accessibility a;a.high_contrast=(flags&1)!=0;a.force_monochrome=(flags&2)!=0;theme::set_theme(variant,a);
            block::OracleWork captures;captures.left=768'000'000;const auto observed=observe(raw,captures,&cache);
            block::OracleWork checking;validate_observation(observed,expected,palette(),checking);
        }
    }
}

TEST(MermaidBlockSource58Controls, PriorEdgeCaptionAndItsSideCellsSurviveLaterRoutes) {
    // Authored generic input, outside the 58-original census: zero admission
    // credit. No producer source or captured route supplies these expectations.
    // Equal nine-cell columns, three-cell gaps and the root apron give a 35x15
    // grid. A/B occupy (1,1)/(13,1); C/D/E occupy (1,9)/(13,9)/(25,9).
    //
    // Independent finite-route worksheet derivation, in source edge order:
    // E->A: (25,11),(24,11),(24,7),(10,7),(10,3),(9,3).
    // Its horizontal segment x=10..24 centres "label" at x=15..19,y=6;
    // the reserved side cells are (14,6) and (20,6).
    // The old flags4-only route test then accepted B->D through
    // (17,5),(17,6),(34,6),(34,14),(17,14),(17,13), crossing three
    // caption cells AND (20,6). This is an actual old-rule counterexample,
    // independent of whether a later paint order restores the caption ink.
    // With caption/side reservation, B's bottom exit is blocked; the next
    // viable port pair routes (17,1),(17,0),(34,0),(34,14),(17,14),(17,13).
    // Its horizontal top segment centres "next" at x=24..27,y=1, with
    // clear side cells x=23,28. The own-candidate-label collision class is
    // separate; this example specifically challenges a PRIOR edge caption.
    constexpr std::string_view source=
        "block\n"
        "  columns 3\n"
        "  A B space\n"
        "  C D E\n"
        "  E -- \"label\" --> A\n"
        "  B -- \"next\" --> D\n";
    ThemeGuard guard;theme::set_theme(theme::ThemeVariant::Dark);
    Inputs input;block::OracleWork boundary_work;
    ASSERT_EQ(auth::boundary_positives(input.boundary.root(),boundary_work),36u);
    block::OracleWork capture_work;capture_work.left=768'000'000;
    loom::ui::mermaid::LayoutRowsCache cache(2,1024*1024);
    const auto uncached=observe(source,capture_work);
    const auto cached=observe(source,capture_work,&cache);
    const auto hit=observe(source,capture_work,&cache);
    // All public captures precede checking the independent literal geometry.
    const auto colors=palette();
    const auto matches=[&](const auth::Surface& surface) {
        if(surface.width<35||surface.height<=15||surface.cells.size()!=static_cast<std::size_t>(surface.width*surface.height))return false;
        const auto cell=[&](int x,int y,std::string_view glyph,int background=0) {
            const auto& p=surface.cells.at(static_cast<std::size_t>(y*surface.width+x));
            return p.glyph==glyph&&p.foreground==colors.foreground[1]&&
                p.background==colors.background[static_cast<std::size_t>(background)]&&
                !p.bold&&p.other_attributes==0&&p.hyperlink==0;
        };
        struct Label {int x,y;std::string_view text;};
        for(const auto& label:std::array<Label,2>{{{15,6,"label"},{24,1,"next"}}}) {
            for(std::size_t x=0;x<label.text.size();++x)
                if(!cell(label.x+static_cast<int>(x),label.y,label.text.substr(x,1)))return false;
            // Check both complete one-cell side clearances, including colors
            // and blank attributes. Restoring label bytes over a crossing line
            // cannot hide the old route's ink in its right-side clearance.
            if(!cell(label.x-1,label.y," ")||!cell(label.x+static_cast<int>(label.text.size()),label.y," "))return false;
        }
        struct Node {int x,y;std::string_view caption;};
        for(const auto& node:std::array<Node,5>{{{1,1,"A"},{13,1,"B"},{1,9,"C"},{13,9,"D"},{25,9,"E"}}}) {
            if(!cell(node.x+4,node.y+2,node.caption,1)||!cell(node.x,node.y,"┌")||
                !cell(node.x+8,node.y,"┐")||!cell(node.x,node.y+4,"└")||!cell(node.x+8,node.y+4,"┘"))return false;
        }
        // Independently derived route anchors establish that labels accompany
        // their two routed edges, rather than appearing as detached notices.
        if(!cell(15,7,"─")||!cell(24,0,"─")||!cell(34,6,"│"))return false;
        for(int x=0;x<surface.width;++x) {
            const auto glyph=static_cast<std::size_t>(x)<kSentinel.size()?kSentinel.substr(static_cast<std::size_t>(x),1):std::string_view(" ");
            if(!cell(x,15,glyph))return false;
        }
        return true;
    };
    ASSERT_TRUE(uncached.factory_accepted);ASSERT_TRUE(cached.factory_accepted);ASSERT_TRUE(hit.factory_accepted);
    ASSERT_GT(cache.hits(),0u);
    for(const auto* observed:{&uncached,&cached,&hit}) {
        ASSERT_EQ(observed->frames[0],observed->frames[2]);
        ASSERT_NE(observed->frames[0],observed->frames[3]);
        // A 35-cell canvas fits both public widths. No fallback qualifies.
        for(const auto index:{0u,1u,2u})ASSERT_TRUE(matches(observed->frames[index]))<<"frame "<<index;
    }
    ASSERT_EQ(uncached.frames,cached.frames);ASSERT_EQ(cached.frames,hit.frames);
    // Good-first physical mutants keep the complete source and other cells.
    // These are comparator regressions, not additional original coverage.
    const auto rejects=[&](int x,int y,std::string_view glyph) {
        auto changed=uncached.frames[0];
        auto& pixel=changed.cells.at(static_cast<std::size_t>(y*changed.width+x));
        ASSERT_NE(pixel.glyph,glyph);pixel.glyph=glyph;
        EXPECT_FALSE(matches(changed));
    };
    rejects(15,6," "); // Missing first caption byte.
    rejects(17,6,"│"); // Old route overwrites a prior caption cell.
    rejects(20,6,"─"); // Actual old route crosses the right-side clearance.
    rejects(14,6,"│"); // Left-side clearance must also remain blank.
    rejects(24,1," "); // The second complete caption is independently required.
    rejects(28,1,"─"); // The second caption's side reservation is observable.
}
