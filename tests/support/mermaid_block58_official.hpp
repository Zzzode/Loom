// Official-owned public Markdown observations. Expectations come exclusively
// from the frozen source binder and independent geometry authority, never from
// Native reports, producer internals or the captured output itself.
#pragma once
#include "mermaid_block58_authority.hpp"

namespace loom::test::mermaid_block58::official {
namespace auth=authority;
namespace json=loom::utils::json;
namespace theme=loom::ui::design::theme;
namespace fs=std::filesystem;
inline constexpr std::string_view kContract="Block58/complete-original/v1";
inline constexpr std::string_view kSentinel="AFTER_OFFICIAL_BLOCK58";
inline constexpr std::string_view kEvidence="mermaid-official-block58.jsonl";
using Frames=std::array<auth::Surface,5>;
inline void need(bool good,std::string_view message){projection::require(good,message);}
inline std::string hash(std::string_view bytes){return loom::utils::crypto::sha256(bytes);}
inline std::string read(const fs::path& path,std::size_t cap=kMaxFactBytes) {
    need(!fs::is_symlink(fs::symlink_status(path))&&fs::is_regular_file(path)&&fs::file_size(path)<=cap,
         "source: bounded Official Block58 file");
    std::ifstream input(path,std::ios::binary);need(input.good(),"source: Official Block58 file open");
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
struct Documents {
    json::JsonDoc source,terminal,manifest,boundary;
    Documents() {
        const fs::path root=fs::path(LOOM_TESTS_DIR)/"fixtures";
        const auto s=read(root/"mermaid_native/block58_complete_source_proposal.v1.json");
        const auto t=read(root/"mermaid_native/block58_terminal_projection_proposal.v1.json");
        const auto m=read(root/"mermaid_official/manifest.json");
        const auto b=read(root/"mermaid_native/block58_boundary_contract.v1.json",256*1024);
        OracleWork work;verify_asset_bytes(s,t,work);need(hash(m)==kManifestSha,"source: Official Block58 manifest SHA");
        boundary::verify_bytes(b);
        auth::verify_design_bytes(read(fs::path(LOOM_TESTS_DIR).parent_path()/"docs/design/mermaid-block58-terminal-contract.md",65536));
        auto sd=json::parse(s),td=json::parse(t),md=json::parse(m),bd=json::parse(b);
        need(sd&&td&&md&&bd,"source: Official Block58 documents JSON");
        source=std::move(*sd);terminal=std::move(*td);manifest=std::move(*md);boundary=std::move(*bd);
        OracleWork positives;need(auth::boundary_positives(boundary.root(),positives)==36,"source: independent literal boundary positives");
    }
    BoundFacts bind(std::string_view path,std::string_view raw,OracleWork& work) const {
        return mermaid_block58::bind(source.root(),terminal.root(),path,raw,manifest.root(),work);
    }
};
inline std::optional<std::size_t> select(std::size_t global,std::string_view path,std::string_view raw_sha) {
    for(std::size_t i=0;i<kPins.size();++i)if(kPins[i].path==path) {
        need(kPins[i].global==static_cast<int>(global)&&kPins[i].raw_sha==raw_sha,"source: Official Block58 selection ordinal/raw SHA");return i;
    }
    return std::nullopt;
}
inline auth::Palette palette() {
    const auto snapshot=theme::current_theme();const auto& p=*snapshot.palette;
    const std::array<ftxui::Color,7> fg{p.text,p.text,p.danger,p.warning,p.success,p.info,p.accent_purple};
    const std::array<ftxui::Color,7> bg{ftxui::Color::Default,p.surface_tint_2,p.danger_tint_bg,p.warning_tint_bg,p.success_tint_bg,p.info_tint_bg,p.surface_tint_4};
    auth::Palette out;for(std::size_t i=0;i<7;++i){out.foreground[i]=fg[i].Print(false);out.background[i]=bg[i].Print(true);}return out;
}
inline auth::Surface capture(ftxui::Element node,int width,int height,OracleWork& work) {
    need(width>0&&width<=240&&height>0&&height<=1040,"resource: Official Block58 bounded public frame");
    const auto count=static_cast<std::size_t>(width*height);work.spend(count*(sizeof(auth::ObservedPixel)+32)+512);
    auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(width),ftxui::Dimension::Fixed(height));ftxui::Render(screen,node);
    auth::Surface out;out.width=width;out.height=height;out.cells.reserve(count);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        const auto& p=screen.PixelAt(x,y);const auto other=static_cast<std::uint16_t>((p.blink?1:0)|(p.dim?2:0)|(p.inverted?4:0)|
            (p.underlined?8:0)|(p.underlined_double?16:0)|(p.strikethrough?32:0));
        auto fg=p.foreground_color.Print(false),bg=p.background_color.Print(true);
        need(p.character.size()<=4096&&fg.size()<=128&&bg.size()<=128,"resource: Official captured cell bytes");work.spend(p.character.size()+fg.size()+bg.size());
        out.cells.push_back({p.character,std::move(fg),std::move(bg),p.bold,other,p.hyperlink});
    }
    return out;
}
inline std::string fence(std::string_view raw,std::string_view language) {
    std::size_t run=0,longest=0;for(const auto c:raw){run=c=='`'?run+1:0;longest=std::max(longest,run);}
    const std::string ticks(std::max(std::size_t{3},longest+1),'`');return ticks+std::string(language)+"\n"+std::string(raw)+"\n"+ticks+"\n";
}
inline Frames observe(std::string_view raw,OracleWork& work,loom::ui::mermaid::LayoutRowsCache* cache=nullptr,bool dim=true) {
    loom::ui::MarkdownOptions options;options.dim_color=dim;options.mermaid_rows_cache=cache;
    auto drawing=loom::ui::render_markdown(fence(raw,"mermaid"),options);
    options.mermaid_rows_cache=nullptr;auto source=loom::ui::render_markdown(fence(raw,"mermaid-source"),options);
    const auto colors=theme::current_theme();
    const auto surround=[&](ftxui::Element body){return ftxui::vbox({std::move(body),ftxui::text(std::string(kSentinel))})|
        ftxui::color(colors.palette->text)|ftxui::bgcolor(ftxui::Color::Default);};
    auto node=surround(drawing),ordinary=surround(source);node->ComputeRequirement();ordinary->ComputeRequirement();
    // Capture dimensions use public requirements only, before any case binding.
    const int height=std::min(1040,std::max(40,std::max(node->requirement().min_y,ordinary->requirement().min_y)+8));
    constexpr std::array<int,5> widths{240,40,240,240,40};Frames frames;
    for(std::size_t i=0;i<5;++i)frames[i]=capture(i<3?node:ordinary,widths[i],height,work);
    return frames;
}
inline std::string cell_bytes(const auth::Surface& surface) {
    std::string out;for(const auto& p:surface.cells) {
        for(const auto* s:{&p.glyph,&p.foreground,&p.background}){out+=std::to_string(s->size());out+=':';out+=*s;}
        out+=p.bold?'1':'0';out+=':';out+=std::to_string(p.other_attributes);out+=':';out+=std::to_string(p.hyperlink);out+=';';
    }
    return out;
}
inline void publish(const Frames& frames,json::JsonMutVal record) {
    auto array=record.make_arr();constexpr std::array<std::string_view,5> names{"wide_240","narrow_40","restored_240","source_240","source_40"};
    for(std::size_t n=0;n<frames.size();++n) {
        const auto& frame=frames[n];auto value=record.make_obj();value.set("state",names[n]);value.set("width",frame.width);value.set("height",frame.height);
        value.set("cell_sha256",hash(cell_bytes(frame)));auto runs=value.make_arr();
        for(std::size_t begin=0;begin<frame.cells.size();) {
            std::size_t end=begin+1;while(end<frame.cells.size()&&frame.cells[end]==frame.cells[begin])++end;
            const auto& p=frame.cells[begin];auto run=value.make_arr();run.append(value.make_num(static_cast<std::int64_t>(end-begin)));
            for(const auto* s:{&p.glyph,&p.foreground,&p.background})run.append(value.make_str(*s));
            run.append(value.make_bool(p.bold));run.append(value.make_num(p.other_attributes));run.append(value.make_num(p.hyperlink));runs.append(run);begin=end;
        }
        value.add("complete_cell_runs",runs);array.append(value);
    }record.add("five_frames",array);
}
// Markdown dim decorates the entire diagram allocation, including blank cells.
// Assert the exact expected bit first; only then remove that known inherited bit
// for the shared non-Markdown plane checker. No observed attribute is ignored.
inline auth::Surface normalize_dim(const auth::Surface& input,const auth::Expected& expected,bool dim,OracleWork& work) {
    work.spend(input.cells.size()*(sizeof(auth::ObservedPixel)+32));auto out=input;
    for(int y=0;y<input.height;++y)for(int x=0;x<input.width;++x) {
        auto& p=out.cells[static_cast<std::size_t>(y*input.width+x)];
        const auto want=static_cast<std::uint16_t>(dim&&y<expected.geometry.metrics.height?2:0);
        need(p.other_attributes==want,"physical: exact Markdown inherited dim and absence of other attributes");p.other_attributes=0;
    }
    return out;
}
inline auth::Surface validate(const Frames& frames,const auth::Expected& expected,const auth::Palette& colors,bool dim,OracleWork& work) {
    need(frames[0]==frames[2],"physical: Official Block58 width restoration of full attributes");
    need(frames[0]!=frames[3],"physical: Official Block58 original still source fallback");
    auth::Surface first;
    for(const auto n:{0u,1u,2u}) {
        if(n==1&&expected.geometry.metrics.width>40){need(frames[1]==frames[4],"physical: Official exact narrow ordinary source");continue;}
        auto normalized=normalize_dim(frames[n],expected,dim,work);const auto errors=auth::validate(normalized,expected,colors,0,0,work);
        need(errors.empty(),errors.empty()?"physical: Official full plane":errors.front());
        const int stop=expected.geometry.metrics.height;need(stop<normalized.height,"physical: following Official sentinel clipped");
        for(int y=0;y<normalized.height;++y)for(int x=0;x<normalized.width;++x) {
            if(y<stop&&static_cast<std::size_t>(x)<expected.rows[static_cast<std::size_t>(y)].size())continue;
            const auto& got=normalized.cells[static_cast<std::size_t>(y*normalized.width+x)];
            const auto glyph=y==stop&&static_cast<std::size_t>(x)<kSentinel.size()?std::string(kSentinel.substr(static_cast<std::size_t>(x),1)):" ";
            need(got.glyph==glyph&&got.foreground==colors.foreground[0]&&got.background==colors.background[0]&&!got.bold&&got.other_attributes==0&&got.hyperlink==0,
                 "physical: Official complete exterior blank/sentinel plane");
        }
        if(n==0)first=std::move(normalized);
    }return first;
}
inline void pin_metadata(json::JsonMutVal value,const Pin& pin) {
    value.set("contract",kContract);value.set("path",pin.path);value.set("manifest_global_ordinal",pin.global);value.set("source_sha256",pin.raw_sha);
    value.set("source_facts_sha256",kSourceFactsSha);value.set("terminal_facts_sha256",kTerminalFactsSha);value.set("official_manifest_sha256",kManifestSha);
    value.set("source_lock_sha256",kSourceLock);value.set("complete_model_sha256",pin.source_model_sha);value.set("terminal_model_sha256",pin.terminal_model_sha);
    value.set("source_witnesses_sha256",pin.witness_sha);value.set("manifest_record_sha256",pin.manifest_record_sha);value.set("terminal_source_binding_sha256",pin.terminal_source_binding_sha);
    value.set("boundary_sha256",boundary::kAssetSha);value.set("reviewed_design_sha256",auth::kReviewedDesignSha);
    value.set("authority_adaptation",auth::kAdaptation);value.set("authority_review_status",auth::kAuthorityReviewStatus);value.set("limitations",auth::kLimitations);
    value.set("foreign_assertions_certified",0);value.set("native_report_input",false);value.set("drawing_credit",0);value.set("formal_admission",false);value.set("join_required",true);
}
inline void check_pins(JsonVal proof,const Pin& pin) {
    json::JsonMutDoc expected;auto root=expected.object();pin_metadata(root,pin);expected.set_root(root);
    auto parsed=json::parse(expected.to_string());need(parsed.has_value(),"protocol: Official pin metadata JSON");
    parsed->root().iter_obj([&](JsonVal key,JsonVal value){
        const auto got=proof.get(key.as_str());
        need(value.is_str()?got.is_str()&&got.as_str()==value.as_str():value.is_bool()?got.is_bool()&&got.as_bool()==value.as_bool():got.is_int()&&got.as_int()==value.as_int(),
             "source: Official Block58 closed proof metadata pin");
    });
}
} // namespace loom::test::mermaid_block58::official
