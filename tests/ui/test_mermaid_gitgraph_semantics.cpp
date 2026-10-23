// RFC 0030 GitGraph terminal assertion port. This test owns its source ledger,
// PixelAt capture, cell oracle, and report. It imports no producer semantic
// record, parser result, route, or native report.
module;

#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
module loom.ui.visual.mermaid;

import std;
import loom.ui.visual.markdown;
import loom.serdes.json;
import loom.crypto.crypto;

// Compile-time module-privacy probe: this test translation unit belongs to the
// existing Mermaid module but exports no declaration and contributes no credit.
static_assert(loom::ui::mermaid::detail::kWorkLimit == 1'000'000u);
static_assert(std::is_same_v<decltype(loom::ui::mermaid::detail::Work::remaining), std::size_t>);

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
using json::JsonVal;
const fs::path kRoot{LOOM_TESTS_DIR};
constexpr std::string_view kAsset = "fixtures/mermaid_native/gitgraph30_terminal_models.v1.json";
constexpr std::string_view kAssetSha256 = "6f59e325f4b64a75191452688128986bc49d845331c32923347a8cd565d8ed65";
constexpr std::string_view kCasesSha256 = "f828e1939c15c0ea44cec661119f2606a993e4538b531fee59e5b73757170094";
constexpr std::string_view kFollowing = "AFTER_GITGRAPH_NATIVE";
constexpr std::size_t kSceneLimit = 128u * 1024u * 1024u;
constexpr std::size_t kSetupLimit = 8u * 1024u * 1024u;
constexpr int kWide = 240, kNarrow = 40, kMaxHeight = 1024;
constexpr int U = 1, R = 2, D = 4, L = 8;

[[noreturn]] void fail(std::string_view message) { throw std::runtime_error(std::string(message)); }
void need(bool value, std::string_view message) { if (!value) fail(message); }
std::string digest(std::string_view bytes) { return loom::utils::crypto::sha256(bytes); }

struct Work {
    std::size_t remaining;
    void spend(std::size_t amount = 1) {
        if (amount > remaining) fail("GitGraph terminal oracle work exhausted");
        remaining -= amount;
    }
};
std::size_t product(std::size_t a, std::size_t b, std::string_view message) {
    if (a != 0 && b > std::numeric_limits<std::size_t>::max() / a) fail(message);
    return a * b;
}

struct Point { int x = 0, y = 0; auto operator==(const Point&) const -> bool = default; };
struct Matches { std::size_t count=0u; Point first{}; auto size() const -> std::size_t { return count; } auto empty() const -> bool { return count==0u; } auto front() const -> const Point& { if(empty()) fail("GitGraph empty text match");return first; } };
struct Cell { std::string text{" "}; auto operator==(const Cell&) const -> bool = default; };
struct Grid {
    int width = 0, height = 0;
    std::vector<Cell> cells;
    auto at(Point point) const -> const Cell& {
        if (point.x < 0 || point.y < 0 || point.x >= width || point.y >= height) fail("GitGraph cell point out of bounds");
        return cells.at(static_cast<std::size_t>(point.y * width + point.x));
    }
    auto edit(Point point) -> Cell& {
        if (point.x < 0 || point.y < 0 || point.x >= width || point.y >= height) fail("GitGraph cell point out of bounds");
        return cells.at(static_cast<std::size_t>(point.y * width + point.x));
    }
    auto operator==(const Grid&) const -> bool = default;
};

std::string read_exact(const fs::path& path, std::size_t cap, Work& work) {
    std::error_code error;
    const auto bytes = fs::file_size(path,error);
    if (error || bytes > cap) fail("GitGraph immutable asset size");
    work.spend(static_cast<std::size_t>(bytes) * 8u + 4096u);
    std::ifstream input(path,std::ios::binary);
    if (!input) fail("GitGraph immutable asset open");
    std::string out{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    if (out.size() != bytes) fail("GitGraph immutable asset read");
    return out;
}

std::string fenced(std::string_view source, std::string_view language) {
    std::size_t run = 0, longest = 0;
    for (const auto ch : source) { run = ch == '`' ? run + 1u : 0u; longest = std::max(longest,run); }
    const std::string fence(std::max(std::size_t{3},longest+1u),'`');
    return fence + std::string(language) + "\n" + std::string(source) + "\n" + fence + "\n\n" + std::string(kFollowing);
}

Grid capture_paid(ftxui::Element element, int width, int height, Work& work) {
    need(width>0&&width<=kWide&&height>0&&height<=kMaxHeight,"GitGraph capture bounds");
    const auto count=product(static_cast<std::size_t>(width),static_cast<std::size_t>(height),"GitGraph capture overflow");
    constexpr std::size_t kScreenFee=sizeof(ftxui::Screen)+sizeof(std::vector<ftxui::Pixel>)+256u;
    constexpr std::size_t kPixelAtCellFee=sizeof(ftxui::Pixel)+sizeof(Cell)+3u*sizeof(std::string)+128u;
    work.spend(kScreenFee+product(count,kPixelAtCellFee,"GitGraph PixelAt capture fee overflow")+4096u);
    auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(width),ftxui::Dimension::Fixed(height));
    ftxui::Render(screen,std::move(element));
    Grid grid{screen.dimx(),screen.dimy(),{}};grid.cells.reserve(count);
    for(int y=0;y<grid.height;++y) for(int x=0;x<grid.width;++x) grid.cells.push_back({screen.PixelAt(x,y).character});
    return grid;
}

Matches find_text(const Grid& grid, std::string_view text, Work& work) {
    work.spend(product(text.size()+1u,sizeof(std::string)+16u,"GitGraph glyph decode fee overflow"));
    const auto glyphs = ftxui::Utf8ToGlyphs(std::string(text));
    if (glyphs.empty() || glyphs.size() > static_cast<std::size_t>(grid.width)) return {};
    const auto cells=product(static_cast<std::size_t>(grid.width),static_cast<std::size_t>(grid.height),"GitGraph text scan cell overflow");
    // Cover candidate visits up front; charge every glyph comparison before
    // performing it. Short-circuited comparisons do not consume Work.
    work.spend(cells);
    Matches found;
    for (int y=0;y<grid.height;++y) for (int x=0;x+static_cast<int>(glyphs.size())<=grid.width;++x) {
        bool same = true;
        for (std::size_t n=0;n<glyphs.size();++n) {
            work.spend();
            if (grid.at({x+static_cast<int>(n),y}).text != glyphs[n]) { same=false; break; }
        }
        if (same) {if(found.empty())found.first={x,y};++found.count;}
    }
    return found;
}

int stroke(std::string_view glyph) {
    constexpr std::array<std::pair<std::string_view,int>,29> map{{
        {"─",L|R},{"│",U|D},{"┌",R|D},{"┐",L|D},{"└",U|R},{"┘",U|L},{"├",U|R|D},{"┤",U|L|D},{"┬",L|R|D},{"┴",U|L|R},{"┼",U|R|D|L},
        {"━",L|R},{"┃",U|D},{"┏",R|D},{"┓",L|D},{"┗",U|R},{"┛",U|L},{"┣",U|R|D},{"┫",U|L|D},{"┳",L|R|D},{"┻",U|L|R},{"╋",U|R|D|L},
        {"┄",L|R},{"┈",L|R},{"┆",U|D},{"┊",U|D},{"●",U|R|D|L},{"✖",U|R|D|L},{"■",U|R|D|L}}};
    for (const auto& [key,bits] : map) if (glyph == key) return bits;
    return 0;
}

std::optional<std::vector<Point>> route(const Grid& grid, Point from, Point to, Work& work,const std::set<int>& forbidden = {}) {
    const auto count = product(static_cast<std::size_t>(grid.width),static_cast<std::size_t>(grid.height),"GitGraph route overflow");
    work.spend(product(count,sizeof(int)+2u*sizeof(Point)+16u,"GitGraph route workspace fee overflow")+4096u);
    std::vector<int> parent(count,-1); std::vector<Point> queue; queue.reserve(count);
    const auto index=[&](Point p) { return p.y*grid.width+p.x; };
    const auto add=[&](Point p,int previous) { const auto at=index(p); if(parent.at(static_cast<std::size_t>(at))<0) {parent[static_cast<std::size_t>(at)]=previous;queue.push_back(p);} };
    add(from,index(from));
    constexpr std::array<std::pair<Point,int>,4> directions{{{{0,-1},U},{{1,0},R},{{0,1},D},{{-1,0},L}}};
    for (std::size_t read=0;read<queue.size();++read) {
        const auto here=queue[read]; if (here==to) break; const auto bits=stroke(grid.at(here).text);
        for (const auto& [delta,bit] : directions) {
            if (!(bits&bit)) continue; const Point next{here.x+delta.x,here.y+delta.y};
            if(next.x<0||next.y<0||next.x>=grid.width||next.y>=grid.height) continue;
            if(next!=to&&forbidden.contains(index(next))) continue;
            const int opposite = bit==U ? D : bit==D ? U : bit==L ? R : L;
            if (stroke(grid.at(next).text)&opposite) add(next,index(here));
        }
    }
    if (parent[static_cast<std::size_t>(index(to))]<0) return std::nullopt;
    std::vector<Point> result;result.reserve(count);
    for (int at=index(to);;) { result.push_back({at%grid.width,at/grid.width}); const auto previous=parent[static_cast<std::size_t>(at)]; if(previous==at) break; at=previous; }
    std::ranges::reverse(result); return result;
}

struct Commit { std::string id,branch,type,tag,source_operation; int sequence = 0; std::vector<std::string> parents; };
struct Branch { std::string name,start,head; int declaration_ordinal = 0; };
struct Model {
    std::string case_id,direction,main_branch,current_branch;
    std::vector<Commit> commits; std::vector<Branch> branches;
    bool empty_zero_ink_exception = false; bool reset = false;
};

Model decode_bound_model(JsonVal row, Work& work) {
    work.spend(32768u);
    need(row.get("case_id").is_str() && row.get("raw_source").is_str() && row.get("state_projection").is_obj(),"GitGraph Model schema");
    Model model; model.case_id=row.get_string("case_id"); const auto state=row.get("state_projection");
    model.direction=state.get_string("direction"); model.main_branch=state.get_string("main_branch"); model.current_branch=state.get_string("current_branch");
    model.empty_zero_ink_exception=state.get("empty_zero_ink_exception").as_bool(); model.reset=state.get("reset").is_obj();
    need(model.direction=="LR"||model.direction=="TB"||model.direction=="BT","GitGraph direction Model");
    const auto branch_rows=state.get("branches"),commit_rows=state.get("commits");
    need(branch_rows.is_arr()&&commit_rows.is_arr()&&branch_rows.size()<=64u&&commit_rows.size()<=512u,"GitGraph Model cardinality");
    work.spend(branch_rows.size()*(sizeof(Branch)+64u)+commit_rows.size()*(sizeof(Commit)+96u)+4096u);
    model.branches.reserve(branch_rows.size());model.commits.reserve(commit_rows.size());
    branch_rows.iter([&](JsonVal item) {
        Branch branch{item.get_string("name"),item.get_string("start"),item.get_string("head"),static_cast<int>(item.get_int("declaration_ordinal"))};
        if(branch.name.empty()||branch.start.empty()||branch.head.empty())fail("GitGraph branch Model");
        model.branches.push_back(std::move(branch));
    });
    commit_rows.iter([&](JsonVal item) {
        Commit commit;commit.id=item.get_string("id");commit.branch=item.get_string("branch");commit.type=item.get_string("type");commit.tag=item.get_string("tag");
        commit.sequence=static_cast<int>(item.get_int("sequence"));commit.source_operation=item.get_string("source_operation");
        const auto parents=item.get("parents");need(parents.is_arr()&&parents.size()<=8u,"GitGraph parent Model cardinality");
        work.spend(parents.size()*(sizeof(std::string)+32u)+128u);commit.parents.reserve(parents.size());
        parents.iter([&](JsonVal parent){commit.parents.emplace_back(parent.as_str());});
        if(commit.id.empty()||commit.branch.empty()||commit.source_operation.empty())fail("GitGraph commit Model");
        model.commits.push_back(std::move(commit));
    });
    const auto branch_exists=[&](std::string_view name) { return std::ranges::any_of(model.branches,[&](const Branch& branch){return branch.name==name;}); };
    need(!model.branches.empty()&&branch_exists(model.current_branch)&&branch_exists(model.main_branch),"GitGraph current/main branch Model");
    for(std::size_t n=0;n<model.branches.size();++n) need(model.branches[n].declaration_ordinal==static_cast<int>(n),"GitGraph branch declaration order");
    for(std::size_t n=0;n<model.commits.size();++n) {
        const auto& commit=model.commits[n];
        need(commit.sequence==static_cast<int>(n),"GitGraph commit sequence is not statement ordinal");
        need(branch_exists(commit.branch),"GitGraph commit branch Model");
        need(commit.source_operation=="commit"||commit.source_operation=="merge"||commit.source_operation=="cherry_pick"||commit.source_operation=="commit_after_reset","GitGraph source operation Model");
        for(const auto& parent:commit.parents) need(std::ranges::any_of(model.commits,[&](const Commit& candidate){return candidate.id==parent;}),"GitGraph parent Model reference");
    }
    if(model.empty_zero_ink_exception) need(model.commits.empty()&&model.case_id=="termaid/a0655f0ceb954df5ace6","GitGraph sole empty exception");
    else need(!model.commits.empty(),"GitGraph nonempty Model");
    return model;
}

std::string_view source_lines(std::string_view text,int first,int last) {
    need(first>0&&last>=first,"GitGraph source line range");
    std::size_t start=0u;
    for(int line=1;line<first;++line) {
        const auto at=text.find('\n',start);
        if(at==std::string_view::npos) fail("GitGraph source start line");
        start=at+1u;
    }
    std::size_t end=start;
    for(int line=first;line<=last;++line) {
        const auto at=text.find('\n',end);
        if(at==std::string_view::npos) {
            need(line==last,"GitGraph source end line");
            return text.substr(start);
        }
        end=at+1u;
    }
    return text.substr(start,end-start);
}

void bind_reset_function(JsonVal root,JsonVal row,Work& work) {
    const auto input=root.get("source_model_inputs").get("reset_test_gitgraph");
    need(input.is_obj()&&input.get("functions").is_arr(),"GitGraph reset source lock schema");
    const auto full=read_exact(kRoot/input.get_string("path"),1024u*1024u,work);
    need(digest(full)==input.get_string("sha256"),"GitGraph reset upstream complete-file lock");
    const auto id=row.get_string("case_id");std::size_t found=0u;
    input.get("functions").iter([&](JsonVal function) {
        if(function.get_string("case_id")!=id) return;
        ++found;
        const auto first=static_cast<int>(function.get_int("line_start")),last=static_cast<int>(function.get_int("line_end"));
        const auto exact=source_lines(full,first,last);
        need(exact==function.get_string("source_text")&&exact.size()==static_cast<std::size_t>(function.get_int("source_bytes"))&&digest(exact)==function.get_string("source_sha256"),"GitGraph reset original function binding");
    });
    need(found==1u,"GitGraph reset function selector");
}

void bind_after_capture(JsonVal root, JsonVal row, Work& work) {
    const auto source=row.get_string("raw_source"), id=row.get_string("case_id");
    need(static_cast<std::size_t>(row.get_int("raw_bytes"))==source.size()&&digest(source)==row.get_string("raw_sha256"),"GitGraph source byte/hash binding");
    if(row.get_string("origin")=="termaid_cases_json") {
        const auto cases_bytes=read_exact(kRoot/"fixtures/mermaid_terminal/termaid/cases.json",4282593u,work);
        need(digest(cases_bytes)==root.get("source_model_inputs").get_string("installed_cases_json_sha256")&&digest(cases_bytes)==kCasesSha256,"GitGraph public cases lock");
        const auto cases=json::parse(cases_bytes);if(!cases)fail("GitGraph public cases JSON");std::size_t found=0u,total=0u;
        cases->root().get("cases").iter([&](JsonVal candidate) {
            if(candidate.get_string("family")!="gitgraph")return;++total;
            if(candidate.get_string("id")==id) {++found;need(candidate.get_string("input")==source&&candidate.get_string("sha256")==row.get_string("raw_sha256"),"GitGraph public source divergence");}
        });
        need(total==28u&&found==1u,"GitGraph selected public source identity");
    } else {
        need(row.get_string("origin")=="termaid_reset_test"&&(id=="termaid_test_reset_to_branch"||id=="termaid_test_reset_ancestor"),"GitGraph reset selector");
        bind_reset_function(root,row,work);
    }
}

std::string_view marker_for(std::string_view type) {
    if(type=="NORMAL")return "●";if(type=="REVERSE")return "✖";if(type=="HIGHLIGHT")return "■";fail("GitGraph commit type");
}
std::string_view commit_caption(const Commit& commit) { return commit.id; }
std::string branch_body(const Branch& branch,const Model& model) {
    return std::string(branch.name==model.current_branch ? "*" : "")+
           (branch.name==model.main_branch ? "@" : "")+branch.name+" → "+branch.head;
}
int text_cells(std::string_view text) { return ftxui::string_width(std::string(text)); }
int parent_total(const Model& model) {
    int total=0;for(const auto& commit:model.commits) total+=static_cast<int>(commit.parents.size());return total;
}
int max_tags(const Model& model) {
    return std::ranges::any_of(model.commits,[](const Commit& commit){return !commit.tag.empty();}) ? 1 : 0;
}
int max_branch_ref_width(const Model& model) {
    int width=0;for(const auto& branch:model.branches)width=std::max(width,text_cells(branch_body(branch,model)));return width;
}

struct Claims {
    const Grid& grid;
    std::vector<bool> used;
    Claims(const Grid& input,Work& work) : grid(input) {
        work.spend(product(input.cells.size(),sizeof(bool)+2u*sizeof(std::size_t)+32u,"GitGraph claim workspace fee overflow")+4096u);
        used.assign(input.cells.size(),false);
    }
    auto index(Point point) const -> std::size_t { return static_cast<std::size_t>(point.y*grid.width+point.x); }
    void cell(Point point) {
        need(grid.at(point).text!=" ","GitGraph attempted to claim blank cell");
        used.at(index(point))=true;
    }
    void text(Point point,std::string_view text,Work& work) {
        work.spend(product(text.size()+1u,sizeof(std::string)+32u,"GitGraph claim text fee overflow"));
        const auto glyphs=ftxui::Utf8ToGlyphs(std::string(text));
        need(!glyphs.empty(),"GitGraph empty claimed text");
        for(std::size_t n=0;n<glyphs.size();++n) {
            const Point at{point.x+static_cast<int>(n),point.y};
            need(grid.at(at).text==glyphs[n],"GitGraph claimed text diverged");
            if(glyphs[n]!=" ") cell(at);
        }
    }
    void path(const std::vector<Point>& points) { for(const auto point:points) cell(point); }
    auto claimed(Point point) const -> bool { return used.at(index(point)); }
    auto all_ink(Work& work) const -> bool {
        for(int y=0;y<grid.height;++y) for(int x=0;x<grid.width;++x) {
            work.spend();const auto at=Point{x,y};
            if(grid.at(at).text!=" "&&!used.at(index(at))) return false;
        }
        return true;
    }
};

bool text_at(const Grid& grid,Point point,std::string_view text,Work& work) {
    work.spend(product(text.size()+1u,sizeof(std::string)+16u,"GitGraph exact text fee overflow"));
    const auto glyphs=ftxui::Utf8ToGlyphs(std::string(text));
    if(glyphs.empty()||point.x<0||point.y<0||point.y>=grid.height||point.x>grid.width-static_cast<int>(glyphs.size()))return false;
    for(std::size_t n=0;n<glyphs.size();++n) {work.spend();if(grid.at({point.x+static_cast<int>(n),point.y}).text!=glyphs[n])return false;}
    return true;
}
// Commit captions are role-local, but IDs may be prefixes of other IDs.  The
// cell after a matched ID must be blank or outside the captured row so `B`
// cannot also claim the marker caption `B-cherry`.
bool text_at_caption_boundary(const Grid& grid,Point point,std::string_view text,Work& work) {
    work.spend(product(text.size()+2u,sizeof(std::string)+16u,"GitGraph caption boundary fee overflow"));
    const auto glyphs=ftxui::Utf8ToGlyphs(std::string(text));
    if(glyphs.empty()||point.x<0||point.y<0||point.y>=grid.height||point.x>grid.width-static_cast<int>(glyphs.size()))return false;
    for(std::size_t n=0;n<glyphs.size();++n) {work.spend();if(grid.at({point.x+static_cast<int>(n),point.y}).text!=glyphs[n])return false;}
    const int after=point.x+static_cast<int>(glyphs.size());
    return after==grid.width||grid.at({after,point.y}).text==" ";
}
struct CommitInk { Point marker; Point caption; };
std::optional<CommitInk> unique_commit_ink(const Grid& grid,const Commit& commit,Work& work) {
    work.spend(product(grid.cells.size(),2u,"GitGraph marker-role scan overflow")+128u);
    std::optional<CommitInk> found;
    const auto marker=marker_for(commit.type);
    for(int y=0;y<grid.height;++y)for(int x=0;x<grid.width;++x) {
        const Point point{x,y};if(grid.at(point).text!=marker)continue;
        const Point caption{x+3,y};
        if(!text_at_caption_boundary(grid,caption,commit_caption(commit),work))continue;
        if(found)return {};found=CommitInk{point,caption};
    }
    return found;
}
struct BranchInk { Point guide; Point caption; };
std::optional<BranchInk> unique_branch_ink(const Grid& grid,const Branch& branch,const Model& model,Work& work) {
    work.spend(512u);const auto body=branch_body(branch,model);const auto text=find_text(grid,body,work);
    if(text.size()!=1u||text.front().x<3)return {};
    const Point caption=text.front(),guide{caption.x-3,caption.y};
    if(grid.at(guide).text!="│")return {};
    return BranchInk{guide,caption};
}

// A marker is an endpoint, never a transit cell. One preallocated BFS per
// marker therefore observes exactly the direct, undirected edge set while
// preserving the Model's directed parent list separately.
struct DirectMarkerWorkspace {
    const Grid& grid;
    std::vector<int> marker_at;
    std::vector<int> predecessor;
    std::vector<Point> queue;
    std::vector<unsigned char> expected;
    std::vector<unsigned char> observed;
    std::vector<unsigned char> selected_path;

    DirectMarkerWorkspace(const Grid& input,const std::vector<Point>& markers,Work& work) : grid(input) {
        const auto cells=product(input.cells.size(),2u*sizeof(int)+sizeof(Point)+sizeof(unsigned char)+64u,"GitGraph direct-marker workspace overflow");
        const auto state=product(markers.size(),2u*sizeof(unsigned char)+64u,"GitGraph direct-marker state overflow");
        work.spend(cells+state+4096u);
        marker_at.assign(input.cells.size(),-1);
        predecessor.assign(input.cells.size(),-1);
        queue.reserve(input.cells.size());
        expected.assign(markers.size(),0u);
        observed.assign(markers.size(),0u);
        selected_path.assign(input.cells.size(),0u);
        for(std::size_t n=0;n<markers.size();++n) {
            const auto point=markers[n];
            need(point.x>=0&&point.y>=0&&point.x<input.width&&point.y<input.height,"GitGraph marker workspace point");
            const auto at=index(point);
            need(marker_at.at(at)<0,"GitGraph duplicate marker workspace point");
            marker_at[at]=static_cast<int>(n);
        }
    }

    auto index(Point point) const -> std::size_t { return static_cast<std::size_t>(point.y*grid.width+point.x); }
    auto point(int at) const -> Point { return {at%grid.width,at/grid.width}; }

    template<class Error>
    void verify(std::size_t source,const Model& model,const std::vector<Point>& markers,Claims& claims,Work& work,Error&& error) {
        const auto cells=marker_at.size();
        work.spend(product(cells,8u,"GitGraph direct-marker scan overflow")+
                   product(markers.size(),32u,"GitGraph direct-marker comparison overflow")+1024u);
        std::fill(predecessor.begin(),predecessor.end(),-1);
        std::fill(expected.begin(),expected.end(),0u);
        std::fill(observed.begin(),observed.end(),0u);
        for(const auto& parent_id:model.commits[source].parents) {
            const auto parent=std::ranges::find_if(model.commits,[&](const Commit& commit){return commit.id==parent_id;});
            if(parent==model.commits.end()) { error("GitGraph parent endpoint"); continue; }
            const auto target=static_cast<std::size_t>(parent-model.commits.begin());
            if(target>=source) { error("GitGraph parent sequence order"); continue; }
            if(expected[target]) { error("GitGraph duplicate parent Model"); continue; }
            expected[target]=1u;
        }
        for(std::size_t child=source+1u;child<model.commits.size();++child)
            if(std::ranges::any_of(model.commits[child].parents,[&](const std::string& parent){return parent==model.commits[source].id;}))
                expected[child]=1u;

        const auto start=index(markers[source]);
        predecessor[start]=static_cast<int>(start);
        queue.clear();
        queue.push_back(markers[source]);
        constexpr std::array<std::pair<Point,int>,4> directions{{{{0,-1},U},{{1,0},R},{{0,1},D},{{-1,0},L}}};
        for(std::size_t read=0u;read<queue.size();++read) {
            const auto here=queue[read];
            const auto here_bits=stroke(grid.at(here).text);
            for(const auto& [delta,bit]:directions) {
                if(!(here_bits&bit)) continue;
                const Point next{here.x+delta.x,here.y+delta.y};
                if(next.x<0||next.y<0||next.x>=grid.width||next.y>=grid.height) continue;
                const int opposite=bit==U?D:bit==D?U:bit==L?R:L;
                if(!(stroke(grid.at(next).text)&opposite)) continue;
                const auto next_at=index(next);
                const auto target=marker_at[next_at];
                if(target>=0) {
                    if(static_cast<std::size_t>(target)!=source) {
                        observed[static_cast<std::size_t>(target)]=1u;
                        if(predecessor[next_at]<0) predecessor[next_at]=static_cast<int>(index(here));
                    }
                    continue;
                }
                if(claims.claimed(next)||predecessor[next_at]>=0) continue;
                predecessor[next_at]=static_cast<int>(index(here));
                queue.push_back(next);
            }
        }
        for(std::size_t target=0u;target<markers.size();++target)
            if(expected[target]!=observed[target]) error("GitGraph direct marker adjacency");
        for(std::size_t target=0u;target<markers.size();++target) if(expected[target]) {
            int at=static_cast<int>(index(markers[target]));
            if(predecessor[static_cast<std::size_t>(at)]<0) { error("GitGraph direct parent path"); continue; }
            while(true) {
                selected_path[static_cast<std::size_t>(at)]=1u;
                const auto previous=predecessor[static_cast<std::size_t>(at)];
                if(previous==at) break;
                at=previous;
            }
        }
    }

    void claim_selected_paths(Claims& claims,Work& work) const {
        work.spend(marker_at.size()+256u);
        for(std::size_t at=0u;at<selected_path.size();++at)
            if(selected_path[at]) claims.cell(point(static_cast<int>(at)));
    }
};

std::vector<std::string> geometry_errors(const Grid& grid, const Model& model, Work& work) {
    work.spend(64u*(sizeof(std::string)+64u)+8192u);
    std::vector<std::string> errors;errors.reserve(64u);
    const auto error=[&](std::string_view message){errors.emplace_back(message);};
    try {
        const auto sentinel=find_text(grid,kFollowing,work);
        if(sentinel.size()!=1u) error("GitGraph following token");
        if(model.empty_zero_ink_exception) {
            Claims claims(grid,work);
            if(sentinel.size()==1u)claims.text(sentinel.front(),kFollowing,work);
            if(!claims.all_ink(work))error("GitGraph empty source has diagram ink");
            return errors;
        }
        Claims claims(grid,work);
        if(sentinel.size()==1u)claims.text(sentinel.front(),kFollowing,work);
        std::map<std::string,CommitInk> commits;
        std::vector<Point> marker_by_commit;marker_by_commit.reserve(model.commits.size());
        for(const auto& commit:model.commits) {
            const auto ink=unique_commit_ink(grid,commit,work);
            if(!ink) {error("GitGraph role-filtered commit marker/caption");continue;}
            commits.emplace(commit.id,*ink);marker_by_commit.push_back(ink->marker);
            claims.cell(ink->marker);claims.text(ink->caption,commit_caption(commit),work);
            if(!commit.tag.empty()) {
                const Point tag{ink->caption.x,ink->caption.y+1};
                if(!text_at(grid,tag,"["+commit.tag+"]",work))error("GitGraph tag exact placement");
                else claims.text(tag,"["+commit.tag+"]",work);
            }
        }
        std::size_t visible_markers=0u;
        for(int y=0;y<grid.height;++y)for(int x=0;x<grid.width;++x) {
            work.spend();const auto value=grid.at({x,y}).text;
            if(value=="●"||value=="✖"||value=="■")++visible_markers;
        }
        if(commits.size()!=model.commits.size()||marker_by_commit.size()!=model.commits.size()||visible_markers!=model.commits.size())error("GitGraph marker inventory");

        std::map<std::string,BranchInk> branches;
        for(const auto& branch:model.branches) {
            const auto ink=unique_branch_ink(grid,branch,model,work);
            if(!ink){error("GitGraph compact branch ref/guide");continue;}
            branches.emplace(branch.name,*ink);claims.cell(ink->guide);claims.text(ink->caption,branch_body(branch,model),work);
        }
        if(branches.size()!=model.branches.size())error("GitGraph branch ref inventory");

        const int parents=parent_total(model), tags=max_tags(model), max_ref=max_branch_ref_width(model);
        const int lane_step=tags+3, sequence_step=std::max(tags+3,4);
        const auto main_ref=branches.find(model.main_branch);
        const auto main_model=std::ranges::find_if(model.branches,[&](const Branch& branch){return branch.name==model.main_branch;});
        if(main_ref==branches.end()||main_model==model.branches.end()) {
            error("GitGraph main branch ref geometry");
        } else if(model.direction=="LR") {
            // Canvas::lines removes leading blank rows from the diagram.  The
            // main reference is the only origin witness; every other branch
            // and marker must retain its source-derived lane offset.  Left
            // crop remains impossible here because the guide stays at x=0.
            const int main_lane_y=parents+2+main_model->declaration_ordinal*lane_step;
            const int origin_y=main_ref->second.guide.y-main_lane_y;
            const int first_x=max_ref+8;
            if(!marker_by_commit.empty()&&marker_by_commit.front().x!=first_x)error("GitGraph LR first marker origin");
            for(const auto& branch:model.branches) {
                const auto ref=branches.find(branch.name);if(ref==branches.end())continue;
                const int lane_y=origin_y+parents+2+branch.declaration_ordinal*lane_step;
                if(ref->second.guide!=Point{0,lane_y}||ref->second.caption!=Point{3,lane_y})error("GitGraph LR branch ref geometry");
                for(const auto& commit:model.commits)if(commit.branch==branch.name) {
                    const auto at=commits.find(commit.id);if(at!=commits.end()&&at->second.marker.y!=lane_y)error("GitGraph LR branch marker lane equality");
                }
            }
        } else {
            const int route_columns=parents+2, lane_width=std::max(6,max_ref+4);
            const int state_y=static_cast<int>(model.commits.size())*sequence_step+1;
            // Canvas::lines can also remove leading blank columns.  Derive
            // exactly one x origin from main; preserve TB's header and BT's
            // footer rows, then compare every branch and marker relatively.
            const int main_lane_x=route_columns+main_model->declaration_ordinal*lane_width;
            const int origin_x=main_ref->second.guide.x-main_lane_x;
            if(model.direction=="TB"&&!marker_by_commit.empty()&&
               marker_by_commit.front().y!=static_cast<int>(model.branches.size())+2)
                error("GitGraph TB first marker header clearance");
            for(const auto& branch:model.branches) {
                const auto ref=branches.find(branch.name);if(ref==branches.end())continue;
                const int lane_x=origin_x+route_columns+branch.declaration_ordinal*lane_width;
                // Pinned Termaid semantics require TB references above the first
                // marker. BT keeps the footer references below the last marker.
                const int ref_y=model.direction=="TB" ? branch.declaration_ordinal : state_y+branch.declaration_ordinal;
                if(ref->second.guide!=Point{lane_x,ref_y}||
                   ref->second.caption!=Point{lane_x+3,ref_y})error("GitGraph vertical branch ref geometry");
                for(const auto& commit:model.commits)if(commit.branch==branch.name) {
                    const auto at=commits.find(commit.id);if(at!=commits.end()&&at->second.marker.x!=lane_x)error("GitGraph vertical branch marker lane equality");
                }
            }
        }
        if(marker_by_commit.size()==model.commits.size()) {
            DirectMarkerWorkspace direct{grid,marker_by_commit,work};
            for(std::size_t source=0u;source<model.commits.size();++source)direct.verify(source,model,marker_by_commit,claims,work,error);
            direct.claim_selected_paths(claims,work);
        }
        for(std::size_t n=1u;n<model.commits.size();++n) {
            const auto prior=commits.find(model.commits[n-1u].id),current=commits.find(model.commits[n].id);
            if(prior==commits.end()||current==commits.end())continue;
            const bool forward=model.direction=="LR" ? current->second.marker.x>prior->second.marker.x :
                               model.direction=="TB" ? current->second.marker.y>prior->second.marker.y :
                               current->second.marker.y<prior->second.marker.y;
            if(!forward)error("GitGraph commit sequence axis rank");
        }
        if(!claims.all_ink(work))error("GitGraph contains unclaimed nonblank ink");
    } catch(const std::exception& caught) {
        if(std::string_view{caught.what()}=="GitGraph terminal oracle work exhausted")throw;
        error(caught.what());
    }
    return errors;
}

std::string geometry_detail(const std::vector<std::string>& wide,
                            const std::vector<std::string>& restored,
                            const std::vector<std::string>* narrow,Work& work) {
    constexpr std::size_t kPerView=3u,kPerError=128u;
    const std::array<std::pair<std::string_view,const std::vector<std::string>*>,3> views{{
        {"wide",&wide},{"restored",&restored},{"narrow",narrow}}};
    std::size_t bytes=32u;
    for(const auto& [name,errors]:views) if(errors) {
        const auto count=std::min(kPerView,errors->size());
        bytes+=name.size()+2u;
        for(std::size_t n=0;n<count;++n) bytes+=std::min(kPerError,errors->at(n).size())+2u;
        if(errors->size()>count) bytes+=24u;
    }
    work.spend(bytes*2u+512u);
    std::string out;out.reserve(bytes);
    for(const auto& [name,errors]:views) if(errors&&!errors->empty()) {
        if(!out.empty())out.append("; ");out.append(name).append(": ");
        const auto count=std::min(kPerView,errors->size());
        for(std::size_t n=0;n<count;++n) {
            if(n)out.append(" | ");
            out.append(errors->at(n).substr(0u,kPerError));
        }
        if(errors->size()>count)out.append(" | +").append(std::to_string(errors->size()-count)).append(" more");
    }
    return out.empty()?"GitGraph cell geometry":out;
}

void physical_rejected(const Grid& proven_good,const Model& model,Work& work,auto mutate,std::string_view name) {
    work.spend(proven_good.cells.size()*(2u*sizeof(Cell)+16u)+4096u);
    auto bad=proven_good;mutate(bad);need(!geometry_errors(bad,model,work).empty(),name);
}

void model_rejected(const Grid& proven_good,const Model& proven_model,Work& work,auto mutate,std::string_view name) {
    work.spend(proven_model.commits.size()*512u+proven_model.branches.size()*512u+4096u);
    auto bad=proven_model;mutate(bad);need(!geometry_errors(proven_good,bad,work).empty(),name);
}
void write_text(Grid& grid,Point point,std::string_view text,Work& work) {
    work.spend(product(text.size()+1u,sizeof(std::string)+16u,"GitGraph mutation glyph fee overflow"));
    const auto glyphs=ftxui::Utf8ToGlyphs(std::string(text));
    for(std::size_t n=0;n<glyphs.size();++n)grid.edit({point.x+static_cast<int>(n),point.y}).text=glyphs[n];
}
void write_raw(Grid& grid,Point point,std::string_view text,Work& work) {
    work.spend(product(text.size()+1u,sizeof(std::string)+16u,"GitGraph synthetic glyph fee overflow")+64u);
    const auto glyphs=ftxui::Utf8ToGlyphs(std::string(text));
    for(std::size_t n=0;n<glyphs.size();++n)grid.edit({point.x+static_cast<int>(n),point.y}).text=glyphs[n];
}
void clear_raw(Grid& grid,Point point,std::string_view text,Work& work) {
    work.spend(product(text.size()+1u,sizeof(std::string)+16u,"GitGraph synthetic clear fee overflow")+64u);
    const auto glyphs=ftxui::Utf8ToGlyphs(std::string(text));
    for(std::size_t n=0;n<glyphs.size();++n)grid.edit({point.x+static_cast<int>(n),point.y}).text=" ";
}
void replace_cell(Grid& grid,Point point,std::string_view glyph,Work& work) {
    work.spend(product(glyph.size()+1u,sizeof(std::string)+16u,"GitGraph synthetic cell fee overflow")+32u);
    grid.edit(point).text=std::string(glyph);
}
Point blank_run(const Grid& grid,std::size_t length,Work& work) {
    for(int y=0;y<grid.height;++y)for(int x=0;x+static_cast<int>(length)<=grid.width;++x){bool blank=true;for(std::size_t n=0;n<length;++n){work.spend();if(grid.at({x+static_cast<int>(n),y}).text!=" "){blank=false;break;}}if(blank)return {x,y};}
    fail("GitGraph physical blank run target");
}
std::string glyph_for_bits(int bits) {
    switch(bits) {
        case U|D:return "│";case L|R:return "─";case R|D:return "┌";case L|D:return "┐";
        case U|R:return "└";case U|L:return "┘";case U|R|D:return "├";case U|L|D:return "┤";
        case L|R|D:return "┬";case U|L|R:return "┴";case U|R|D|L:return "┼";
        default:fail("GitGraph synthetic route bits");
    }
}

// Pay before constructing a synthetic path container. `draw_path` avoids a
// second expanded point vector: it charges the complete walk before writing a
// single route glyph.
std::vector<std::vector<Point>> paid_paths(std::size_t count,Work& work) {
    work.spend(product(count,sizeof(std::vector<Point>)+64u,"GitGraph synthetic path vector fee overflow")+256u);
    std::vector<std::vector<Point>> out;out.reserve(count);return out;
}
void append_path(std::vector<std::vector<Point>>& paths,std::initializer_list<Point> corners,Work& work) {
    work.spend(product(corners.size(),sizeof(Point)+32u,"GitGraph synthetic path corner fee overflow")+128u);
    paths.emplace_back(corners);
}
void draw_path(Grid& grid,const std::vector<Point>& corners,Work& work) {
    need(corners.size()>=2u,"GitGraph synthetic route corners");std::size_t steps=0u;
    for(std::size_t n=1u;n<corners.size();++n) {
        const auto from=corners[n-1u],to=corners[n];
        need(from.x==to.x||from.y==to.y,"GitGraph synthetic route diagonal");
        const auto delta=from.x==to.x ? std::abs(to.y-from.y) : std::abs(to.x-from.x);
        need(delta>0,"GitGraph synthetic zero route segment");steps+=static_cast<std::size_t>(delta);
    }
    work.spend(product(steps+corners.size(),sizeof(Point)+64u,"GitGraph synthetic route walk fee overflow")+256u);
    const auto bit=[](Point from,Point to) {
        const auto d=Point{to.x-from.x,to.y-from.y};
        return d.x>0?R:d.x<0?L:d.y>0?D:U;
    };
    const auto paint=[&](Point point,int directions) {
        grid.edit(point).text=glyph_for_bits(stroke(grid.at(point).text)|directions);
    };
    for(std::size_t n=1u;n<corners.size();++n) {
        const auto from=corners[n-1u],end=corners[n];
        const Point step{end.x==from.x ? 0 : end.x>from.x ? 1 : -1,end.y==from.y ? 0 : end.y>from.y ? 1 : -1};
        // Endpoints are commit markers. Each non-endpoint corner is painted
        // once, with both the full incoming segment and the next outgoing step.
        if(n>1u) paint(from,bit(from,corners[n-2u])|bit(from,end));
        for(Point here{from.x+step.x,from.y+step.y};here!=end;here={here.x+step.x,here.y+step.y})
            paint(here,bit(here,{here.x-step.x,here.y-step.y})|bit(here,{here.x+step.x,here.y+step.y}));
    }
}
void draw_path(Grid& grid,std::initializer_list<Point> corners,Work& work) {
    // The braced physical-mutant calls must not materialize a temporary vector
    // before their fee is paid.
    work.spend(product(corners.size(),sizeof(Point)+32u,"GitGraph synthetic inline route fee overflow")+128u);
    std::vector<Point> owned{corners};draw_path(grid,owned,work);
}

std::vector<Point> lr_positions(const Model& model,Work& work) {
    work.spend(product(model.commits.size(),sizeof(Point)+128u,"GitGraph LR position fee overflow")+
               product(model.branches.size(),512u,"GitGraph LR branch width fee overflow")+1024u);
    const int start=max_branch_ref_width(model)+8,lane_step=max_tags(model)+3,parents=parent_total(model);
    std::vector<Point> out;out.reserve(model.commits.size());int x=start;
    for(const auto& commit:model.commits) {
        const auto branch=std::ranges::find_if(model.branches,[&](const Branch& candidate){return candidate.name==commit.branch;});
        need(branch!=model.branches.end(),"GitGraph synthetic branch lookup");out.push_back({x,parents+2+branch->declaration_ordinal*lane_step});x+=20;
    }
    return out;
}
std::vector<Point> vertical_positions(const Model& model,Work& work) {
    need(model.direction=="TB"||model.direction=="BT","GitGraph synthetic vertical direction");
    work.spend(product(model.commits.size(),sizeof(Point)+128u,"GitGraph vertical position fee overflow")+
               product(model.branches.size(),512u,"GitGraph vertical branch width fee overflow")+1024u);
    const int parents=parent_total(model),route_columns=parents+2,lane_width=std::max(6,max_branch_ref_width(model)+4);
    const int sequence_step=std::max(max_tags(model)+3,4),first_y=model.direction=="TB" ? static_cast<int>(model.branches.size())+2 : 0;
    std::vector<Point> out;out.reserve(model.commits.size());
    for(const auto& commit:model.commits) {
        const auto branch=std::ranges::find_if(model.branches,[&](const Branch& candidate){return candidate.name==commit.branch;});
        need(branch!=model.branches.end(),"GitGraph synthetic vertical branch lookup");
        const int y=model.direction=="TB" ? first_y+commit.sequence*sequence_step :
                    (static_cast<int>(model.commits.size())-1-commit.sequence)*sequence_step;
        out.push_back({route_columns+branch->declaration_ordinal*lane_width,y});
    }
    return out;
}
Grid make_lr_good(const Model& model,const std::vector<std::vector<Point>>& paths,Work& work) {
    const auto markers=lr_positions(model,work);int maximum_y=0,maximum_x=0;
    for(const auto point:markers){maximum_y=std::max(maximum_y,point.y);maximum_x=std::max(maximum_x,point.x+32);}
    for(const auto& branch:model.branches)maximum_y=std::max(maximum_y,parent_total(model)+2+branch.declaration_ordinal*(max_tags(model)+3));
    const int width=std::max(120,maximum_x+8),height=maximum_y+5;
    const auto cells=product(static_cast<std::size_t>(width),static_cast<std::size_t>(height),"GitGraph synthetic grid overflow");
    work.spend(product(cells,sizeof(Cell)+sizeof(std::string)+64u,"GitGraph synthetic grid fee overflow")+4096u);
    Grid grid{width,height,std::vector<Cell>(cells)};
    for(const auto& path:paths)draw_path(grid,path,work);
    for(std::size_t n=0;n<model.commits.size();++n) {
        grid.edit(markers[n]).text=marker_for(model.commits[n].type);write_raw(grid,{markers[n].x+3,markers[n].y},commit_caption(model.commits[n]),work);
        if(!model.commits[n].tag.empty()) {work.spend(512u);const auto tag="["+model.commits[n].tag+"]";write_raw(grid,{markers[n].x+3,markers[n].y+1},tag,work);}
    }
    for(const auto& branch:model.branches) {
        const Point guide{0,parent_total(model)+2+branch.declaration_ordinal*(max_tags(model)+3)};
        grid.edit(guide).text="│";work.spend(512u);const auto body=branch_body(branch,model);write_raw(grid,{3,guide.y},body,work);
    }
    write_raw(grid,{0,grid.height-1},kFollowing,work);return grid;
}
Grid make_vertical_good(const Model& model,const std::vector<std::vector<Point>>& paths,Work& work) {
    const auto markers=vertical_positions(model,work);const int parents=parent_total(model),route_columns=parents+2;
    work.spend(product(model.branches.size(),512u,"GitGraph synthetic vertical branch width fee overflow")+512u);
    const int lane_width=std::max(6,max_branch_ref_width(model)+4),sequence_step=std::max(max_tags(model)+3,4);
    const int state_y=static_cast<int>(model.commits.size())*sequence_step+1;
    int maximum_x=0,maximum_y=0;
    for(const auto point:markers){maximum_x=std::max(maximum_x,point.x+32);maximum_y=std::max(maximum_y,point.y+2);}
    for(const auto& branch:model.branches) {
        const int lane_x=route_columns+branch.declaration_ordinal*lane_width;
        const int ref_y=model.direction=="TB" ? branch.declaration_ordinal : state_y+branch.declaration_ordinal;
        maximum_x=std::max(maximum_x,lane_x+max_branch_ref_width(model)+8);maximum_y=std::max(maximum_y,ref_y);
    }
    const int width=std::max(120,maximum_x+8),height=maximum_y+5;
    const auto cells=product(static_cast<std::size_t>(width),static_cast<std::size_t>(height),"GitGraph synthetic vertical grid overflow");
    work.spend(product(cells,sizeof(Cell)+sizeof(std::string)+64u,"GitGraph synthetic vertical grid fee overflow")+4096u);
    Grid grid{width,height,std::vector<Cell>(cells)};
    for(const auto& path:paths)draw_path(grid,path,work);
    for(std::size_t n=0;n<model.commits.size();++n) {
        grid.edit(markers[n]).text=marker_for(model.commits[n].type);write_raw(grid,{markers[n].x+3,markers[n].y},commit_caption(model.commits[n]),work);
        if(!model.commits[n].tag.empty()) {work.spend(512u);const auto tag="["+model.commits[n].tag+"]";write_raw(grid,{markers[n].x+3,markers[n].y+1},tag,work);}
    }
    for(const auto& branch:model.branches) {
        const int lane_x=route_columns+branch.declaration_ordinal*lane_width;
        const int ref_y=model.direction=="TB" ? branch.declaration_ordinal : state_y+branch.declaration_ordinal;
        grid.edit({lane_x,ref_y}).text="│";work.spend(512u);const auto body=branch_body(branch,model);write_raw(grid,{lane_x+3,ref_y},body,work);
    }
    write_raw(grid,{0,grid.height-1},kFollowing,work);return grid;
}
Point owned_marker(const Grid& grid,const Commit& commit,Work& work) {
    const auto ink=unique_commit_ink(grid,commit,work);need(ink.has_value(),"GitGraph owned marker target");return ink->marker;
}
Point owned_parent_stroke(const Grid& grid,const Model& model,Work& work) {
    for(auto child=model.commits.rbegin();child!=model.commits.rend();++child)for(const auto& parent_id:child->parents) {
        const auto parent=std::ranges::find_if(model.commits,[&](const Commit& candidate){return candidate.id==parent_id;});need(parent!=model.commits.end(),"GitGraph parent Model target");
        const auto from=unique_commit_ink(grid,*child,work),to=unique_commit_ink(grid,*parent,work);need(from&&to,"GitGraph parent marker target");
        work.spend(product(grid.cells.size(),sizeof(int)+64u,"GitGraph parent stroke forbidden fee overflow")+1024u);
        std::set<int> forbidden;for(int y=0;y<grid.height;++y)for(int x=0;x<grid.width;++x){const auto glyph=grid.at({x,y}).text;if(glyph=="●"||glyph=="✖"||glyph=="■")forbidden.insert(y*grid.width+x);}
        const auto path=route(grid,from->marker,to->marker,work,forbidden);if(path&&path->size()>2u)return path->at(path->size()/2u);
    }
    fail("GitGraph no interior parent stroke");
}
void move_branch_ref(Grid& grid,const Branch& branch,const Model& model,Point to,Work& work) {
    const auto old=unique_branch_ink(grid,branch,model,work);need(old.has_value(),"GitGraph synthetic branch ref target");
    work.spend(512u);const auto body=branch_body(branch,model);clear_raw(grid,old->caption,body,work);replace_cell(grid,old->guide," ",work);
    replace_cell(grid,{to.x-3,to.y},"│",work);write_raw(grid,to,body,work);
}

Model vertical_lane_model(std::string_view direction);

// Model Canvas::lines clipping directly: translate diagram ink while keeping
// the Markdown following token in place.  No geometry predicate is relaxed;
// only the one main-branch axis origin may shift.
Grid translate_diagram_ink(const Grid& source,Point delta,Work& work) {
    const auto following=find_text(source,kFollowing,work);
    need(following.size()==1u,"GitGraph synthetic following token");
    const auto first=following.front();
    const auto in_following=[&](Point point) {
        return point.y==first.y&&point.x>=first.x&&point.x<first.x+static_cast<int>(kFollowing.size());
    };
    work.spend(product(source.cells.size(),sizeof(Cell)+sizeof(std::string)+32u,"GitGraph synthetic translation fee overflow")+4096u);
    Grid out{source.width,source.height,std::vector<Cell>(source.cells.size())};
    for(int y=0;y<source.height;++y)for(int x=0;x<source.width;++x) {
        const Point point{x,y};if(in_following(point))out.edit(point)=source.at(point);
    }
    for(int y=0;y<source.height;++y)for(int x=0;x<source.width;++x) {
        const Point point{x,y};if(in_following(point)||source.at(point).text==" ")continue;
        const Point shifted{point.x+delta.x,point.y+delta.y};
        need(shifted.x>=0&&shifted.y>=0&&shifted.x<out.width&&shifted.y<out.height,"GitGraph synthetic translation bounds");
        need(out.at(shifted).text==" ","GitGraph synthetic translation collision");
        out.edit(shifted)=source.at(point);
    }
    return out;
}

void commit_caption_boundary_controls() {
    Work work{kSetupLimit};work.spend(8192u);
    const Model model{"synthetic_caption_boundary","LR","main","main",{{"B","main","NORMAL","","commit",0,{}},{"B-cherry","main","NORMAL","","commit",1,{"B"}}},{{"main","∅","B-cherry",0}},false,false};
    const auto p=lr_positions(model,work);auto paths=paid_paths(1u,work);
    append_path(paths,{p[0],{p[0].x,p[0].y-2},{p[1].x,p[0].y-2},p[1]},work);
    const Grid good=make_lr_good(model,paths,work);
    need(geometry_errors(good,model,work).empty(),"GitGraph compact B and B-cherry boundary good geometry");
    physical_rejected(good,model,work,[&](Grid& bad) {
        const auto short_id=unique_commit_ink(bad,model.commits.front(),work);
        need(short_id.has_value(),"GitGraph synthetic B caption target");
        replace_cell(bad,{short_id->caption.x+1,short_id->caption.y},"-",work);
    },"GitGraph compact commit caption boundary mutation");
}

void cropped_translation_controls() {
    Work work{kSetupLimit};work.spend(24576u);
    const Model lr{"synthetic_cropped_lr","LR","main","main",{{"a","main","NORMAL","","commit",0,{}},{"b","side","NORMAL","","commit",1,{"a"}}},{{"main","∅","a",0},{"side","a","b",1}},false,false};
    const auto lr_points=lr_positions(lr,work);auto lr_paths=paid_paths(1u,work);
    append_path(lr_paths,{lr_points[0],{lr_points[0].x,lr_points[0].y+1},{lr_points[1].x,lr_points[0].y+1},lr_points[1]},work);
    const auto lr_good=make_lr_good(lr,lr_paths,work);
    const auto lr_cropped=translate_diagram_ink(lr_good,{0,-3},work);
    need(geometry_errors(lr_cropped,lr,work).empty(),"GitGraph cropped LR translation good geometry");
    physical_rejected(lr_cropped,lr,work,[&](Grid& bad) {
        const auto side=unique_branch_ink(bad,lr.branches[1],lr,work);
        need(side.has_value(),"GitGraph cropped LR side ref target");
        move_branch_ref(bad,lr.branches[1],lr,{side->caption.x,side->caption.y+1},work);
    },"GitGraph cropped LR single lane translation mutation");

    for(const std::string_view direction:{std::string_view{"TB"},std::string_view{"BT"}}) {
        const auto vertical=vertical_lane_model(direction);const auto p=vertical_positions(vertical,work);auto paths=paid_paths(2u,work);
        append_path(paths,{p[1],{p[0].x+1,p[1].y},{p[0].x+1,p[0].y},p[0]},work);
        append_path(paths,{p[2],p[0]},work);
        const auto good=make_vertical_good(vertical,paths,work);
        const int route_columns=parent_total(vertical)+2;
        const auto cropped=translate_diagram_ink(good,{-route_columns,0},work);
        need(geometry_errors(cropped,vertical,work).empty(),"GitGraph cropped vertical translation good geometry");
    }
}

void good_first_controls() {
    Work work{kSetupLimit};work.spend(8192u);
    const Model model{"synthetic","LR","main","main",{{"a","main","NORMAL","v1","commit",0,{}},{"b","main","NORMAL","","commit",1,{"a"}}},{{"main","∅","b",0}},false,false};
    const auto positions=lr_positions(model,work);auto paths=paid_paths(1u,work);append_path(paths,{positions[0],{positions[0].x,positions[0].y-2},{positions[1].x,positions[1].y-2},positions[1]},work);const Grid good=make_lr_good(model,paths,work);
    need(geometry_errors(good,model,work).empty(),"GitGraph compact good-first synthetic geometry");
    physical_rejected(good,model,work,[&](Grid& bad){replace_cell(bad,unique_commit_ink(bad,model.commits.front(),work)->caption,"?",work);},"GitGraph compact physical commit id");
    physical_rejected(good,model,work,[&](Grid& bad){replace_cell(bad,owned_marker(bad,model.commits.front(),work)," ",work);},"GitGraph compact physical marker");
    physical_rejected(good,model,work,[&](Grid& bad){const auto ink=unique_commit_ink(bad,model.commits.front(),work);replace_cell(bad,{ink->caption.x,ink->caption.y+1}," ",work);},"GitGraph compact physical tag");
    physical_rejected(good,model,work,[&](Grid& bad){const auto ref=unique_branch_ink(bad,model.branches.front(),model,work);replace_cell(bad,ref->caption," ",work);},"GitGraph compact physical current/main ref");
    physical_rejected(good,model,work,[&](Grid& bad){const auto ref=unique_branch_ink(bad,model.branches.front(),model,work);replace_cell(bad,ref->guide," ",work);},"GitGraph compact physical ref guide");
    physical_rejected(good,model,work,[&](Grid& bad){replace_cell(bad,owned_parent_stroke(bad,model,work)," ",work);},"GitGraph compact physical parent route");
    physical_rejected(good,model,work,[&](Grid& bad){write_text(bad,blank_run(bad,2u,work),"██",work);},"GitGraph compact physical unowned extra ink");
    physical_rejected(good,model,work,[&](Grid& bad){write_text(bad,blank_run(bad,2u,work),"──",work);},"GitGraph compact physical forged route");
}

void direct_adjacency_controls() {
    Work work{kSetupLimit};work.spend(8192u);
    const Model model{"synthetic_direct_linear","LR","main","main",{{"a","main","NORMAL","","commit",0,{}},{"b","main","NORMAL","","commit",1,{"a"}},{"c","main","NORMAL","","commit",2,{"b"}}},{{"main","∅","c",0}},false,false};
    const auto p=lr_positions(model,work);auto paths=paid_paths(2u,work);append_path(paths,{p[0],{p[0].x,p[0].y-2},{p[1].x,p[0].y-2},p[1]},work);append_path(paths,{p[1],{p[1].x,p[1].y+2},{p[2].x,p[1].y+2},p[2]},work);const Grid good=make_lr_good(model,paths,work);
    need(geometry_errors(good,model,work).empty(),"GitGraph compact direct-linear good geometry");
    model_rejected(good,model,work,[](Model& bad){bad.commits[2].parents={"a"};},"GitGraph compact wrong direct parent Model mutation");
    model_rejected(good,model,work,[](Model& bad){bad.commits[1].parents={"c"};},"GitGraph compact future parent Model mutation");
    physical_rejected(good,model,work,[&](Grid& bad){draw_path(bad,{p[0],{p[0].x,p[0].y-3},{p[2].x,p[0].y-3},p[2]},work);},"GitGraph compact extra direct shortcut mutation");
}

void branch_lane_controls() {
    Work work{kSetupLimit};work.spend(8192u);
    const Model model{"synthetic_branch_lanes","LR","main","main",{{"a","main","NORMAL","","commit",0,{}},{"b","side","NORMAL","","commit",1,{"a"}}},{{"main","∅","a",0},{"side","a","b",1}},false,false};
    const auto p=lr_positions(model,work);auto paths=paid_paths(1u,work);append_path(paths,{p[0],{p[0].x,p[0].y-2},{p[1].x,p[0].y-2},p[1]},work);const Grid good=make_lr_good(model,paths,work);
    need(geometry_errors(good,model,work).empty(),"GitGraph compact branch-lane good geometry");
    model_rejected(good,model,work,[](Model& bad){bad.commits[1].branch="main";},"GitGraph compact branch lane reassignment mutation");
}

void reset_skip_edge_controls() {
    Work work{kSetupLimit};work.spend(12288u);
    const Model model{"synthetic_reset_ancestor","LR","main","main",{{"c1","main","NORMAL","","commit",0,{}},{"c2","main","NORMAL","","commit",1,{"c1"}},{"c3","main","NORMAL","","commit",2,{"c2"}},{"c4","main","NORMAL","","commit_after_reset",3,{"c1"}}},{{"main","∅","c4",0}},false,true};
    const auto p=lr_positions(model,work);auto paths=paid_paths(3u,work);
    // Every edge leaves c1 on a distinct arm. c2/c3 cannot become directly
    // reachable through an unclaimed shared trunk, while c4 remains a valid
    // ancestor-reset edge.
    append_path(paths,{p[0],{p[0].x+1,p[0].y},{p[0].x+1,p[0].y-2},{p[1].x,p[0].y-2},p[1]},work);
    append_path(paths,{p[1],{p[1].x,p[1].y+2},{p[2].x,p[1].y+2},p[2]},work);
    append_path(paths,{p[0],{p[0].x-1,p[0].y},{p[0].x-1,p[0].y-4},{p[3].x,p[0].y-4},p[3]},work);
    const Grid good=make_lr_good(model,paths,work);
    need(geometry_errors(good,model,work).empty(),"GitGraph compact reset skip good geometry");
    model_rejected(good,model,work,[](Model& bad){bad.commits.back().parents={"c2"};},"GitGraph compact reset c4-to-c2 Model mutation");
    model_rejected(good,model,work,[](Model& bad){bad.commits.back().parents={"c3"};},"GitGraph compact reset c4-to-c3 Model mutation");
    physical_rejected(good,model,work,[&](Grid& bad){replace_cell(bad,{p[3].x,p[0].y-4}," ",work);},"GitGraph compact reset skip-edge physical mutation");
    physical_rejected(good,model,work,[&](Grid& bad){draw_path(bad,{p[1],{p[1].x+1,p[1].y},{p[1].x+1,p[1].y+4},{p[3].x-1,p[1].y+4},{p[3].x-1,p[3].y},p[3]},work);},"GitGraph compact reset forged shortcut mutation");
}

Model vertical_lane_model(std::string_view direction) {
    return {"synthetic_vertical_"+std::string(direction),std::string(direction),"main","main",{{"a","main","NORMAL","","commit",0,{}},{"b","side","NORMAL","","commit",1,{"a"}},{"c","main","NORMAL","","commit",2,{"a"}}},{{"main","∅","c",0},{"side","a","b",1}},false,false};
}
void vertical_lane_controls(std::string_view direction) {
    Work work{kSetupLimit};work.spend(16384u);const Model model=vertical_lane_model(direction);
    const auto p=vertical_positions(model,work);auto paths=paid_paths(2u,work);
    append_path(paths,{p[1],{p[0].x+1,p[1].y},{p[0].x+1,p[0].y},p[0]},work);
    append_path(paths,{p[2],p[0]},work);
    const Grid good=make_vertical_good(model,paths,work);
    need(geometry_errors(good,model,work).empty(),"GitGraph compact vertical good-first geometry");
    physical_rejected(good,model,work,[&](Grid& bad){replace_cell(bad,owned_marker(bad,model.commits[1],work)," ",work);},"GitGraph compact vertical marker mutation");
    physical_rejected(good,model,work,[&](Grid& bad){replace_cell(bad,{p[0].x+1,(p[0].y+p[1].y)/2}," ",work);},"GitGraph compact vertical route mutation");
    physical_rejected(good,model,work,[&](Grid& bad){const auto ref=unique_branch_ink(bad,model.branches[1],model,work);move_branch_ref(bad,model.branches[1],model,{ref->caption.x,ref->caption.y+1},work);},"GitGraph compact vertical branch ref mutation");
    physical_rejected(good,model,work,[&](Grid& bad){const auto b=unique_commit_ink(bad,model.commits[1],work);const auto c=unique_commit_ink(bad,model.commits[2],work);replace_cell(bad,b->caption,model.commits[2].id,work);replace_cell(bad,c->caption,model.commits[1].id,work);},"GitGraph compact vertical sequence rank mutation");
}
void tb_lane_controls() { vertical_lane_controls("TB"); }
void bt_lane_controls() { vertical_lane_controls("BT"); }

struct MutationCounts { std::size_t physical=0u; std::size_t model=0u; };
MutationCounts prove_nonempty_controls(const Grid& good,const Model& model,Work& work) {
    MutationCounts out;
    const auto physical=[&](auto mutate,std::string_view name){physical_rejected(good,model,work,mutate,name);++out.physical;};
    const auto semantic=[&](auto mutate,std::string_view name){model_rejected(good,model,work,mutate,name);++out.model;};
    physical([&](Grid& bad){replace_cell(bad,unique_commit_ink(bad,model.commits.front(),work)->caption,"?",work);},"GitGraph source compact id mutation");
    physical([&](Grid& bad){replace_cell(bad,owned_marker(bad,model.commits.front(),work)," ",work);},"GitGraph source compact marker mutation");
    if(!model.commits.front().tag.empty())physical([&](Grid& bad){const auto ink=unique_commit_ink(bad,model.commits.front(),work);replace_cell(bad,{ink->caption.x,ink->caption.y+1}," ",work);},"GitGraph source compact tag mutation");
    const auto current=std::ranges::find_if(model.branches,[&](const Branch& branch){return branch.name==model.current_branch;});need(current!=model.branches.end(),"GitGraph source current branch");
    physical([&](Grid& bad){const auto ref=unique_branch_ink(bad,*current,model,work);replace_cell(bad,ref->caption," ",work);},"GitGraph source compact current ref mutation");
    const auto main=std::ranges::find_if(model.branches,[&](const Branch& branch){return branch.name==model.main_branch;});need(main!=model.branches.end(),"GitGraph source main branch");
    physical([&](Grid& bad){const auto ref=unique_branch_ink(bad,*main,model,work);work.spend(512u);const auto body=branch_body(*main,model);const auto index=body.find('@');need(index!=std::string::npos,"GitGraph compact main marker");replace_cell(bad,{ref->caption.x+static_cast<int>(index),ref->caption.y}," ",work);},"GitGraph source compact main ref mutation");
    physical([&](Grid& bad){const auto ref=unique_branch_ink(bad,*main,model,work);replace_cell(bad,ref->guide," ",work);},"GitGraph source compact ref guide mutation");
    if(std::ranges::any_of(model.commits,[](const Commit& commit){return !commit.parents.empty();}))physical([&](Grid& bad){replace_cell(bad,owned_parent_stroke(bad,model,work)," ",work);},"GitGraph source compact parent route mutation");
    physical([&](Grid& bad){write_text(bad,blank_run(bad,2u,work),"██",work);},"GitGraph source compact extra ink mutation");
    physical([&](Grid& bad){write_text(bad,blank_run(bad,2u,work),"──",work);},"GitGraph source compact forged route mutation");
    // A reset successor is observable through its direct parent edge. Exercise
    // every alternate pre-reset or known commit target without source-ID
    // dispatch: reset-dev rejects the former main head, reset-main~2 rejects
    // both c2 and c3 instead of its required c1.
    if(model.reset&&model.commits.size()>=3u) {
        const auto final=model.commits.size()-1u;
        const auto reject_parent=[&](const std::string& alternate,std::string_view name) {
            if(model.commits[final].parents.size()==1u&&model.commits[final].parents.front()!=alternate)
                semantic([&](Model& bad){bad.commits.back().parents={alternate};},name);
        };
        reject_parent(model.commits.front().id,"GitGraph source reset former-head parent mutation");
        reject_parent(model.commits[model.commits.size()-2u].id,"GitGraph source reset immediate-head parent mutation");
        if(model.commits.size()>=4u)reject_parent(model.commits[1u].id,"GitGraph source reset known-parent mutation");
    }
    return out;
}

namespace native_git = loom::ui::mermaid::detail;

native_git::GitDirection native_direction(std::string_view direction) {
    if(direction=="LR")return native_git::GitDirection::LR;
    if(direction=="TB")return native_git::GitDirection::TB;
    if(direction=="BT")return native_git::GitDirection::BT;
    fail("GitGraph parser expected direction");
}
native_git::GitType native_type(std::string_view type) {
    if(type=="NORMAL")return native_git::GitType::Normal;
    if(type=="REVERSE")return native_git::GitType::Reverse;
    if(type=="HIGHLIGHT")return native_git::GitType::Highlight;
    fail("GitGraph parser expected type");
}
std::vector<std::string> parser_errors(const native_git::GitGraph& actual,const Model& expected,Work& work) {
    work.spend(32768u+expected.commits.size()*2048u+expected.branches.size()*2048u);
    std::vector<std::string> errors;errors.reserve(32u);
    const auto error=[&](std::string_view message){errors.emplace_back(message);};
    if(actual.direction!=native_direction(expected.direction))error("GitGraph parser direction");
    if(actual.main_branch!=expected.main_branch)error("GitGraph parser main branch");
    if(actual.current_branch!=expected.current_branch)error("GitGraph parser current branch");
    if(actual.commits.size()!=expected.commits.size())error("GitGraph parser commit count");
    if(actual.branches.size()!=expected.branches.size())error("GitGraph parser branch count");
    const auto commits=std::min(actual.commits.size(),expected.commits.size());
    for(std::size_t n=0u;n<commits;++n) {
        const auto& got=actual.commits[n];const auto& want=expected.commits[n];
        if(got.id!=want.id)error("GitGraph parser commit id");
        if(got.sequence!=static_cast<std::size_t>(want.sequence))error("GitGraph parser commit sequence");
        if(got.branch!=want.branch)error("GitGraph parser commit branch");
        if(got.type!=native_type(want.type))error("GitGraph parser commit type");
        const std::vector<std::string> tags=want.tag.empty()?std::vector<std::string>{}:std::vector<std::string>{want.tag};
        if(got.tags!=tags)error("GitGraph parser commit tags");
        if(got.parents!=want.parents)error("GitGraph parser commit parents");
        if(got.operation!=want.source_operation)error("GitGraph parser source operation");
    }
    const auto branches=std::min(actual.branches.size(),expected.branches.size());
    for(std::size_t n=0u;n<branches;++n) {
        const auto& got=actual.branches[n];const auto& want=expected.branches[n];
        if(got.name!=want.name)error("GitGraph parser branch name");
        if(got.declaration!=static_cast<std::size_t>(want.declaration_ordinal))error("GitGraph parser branch declaration");
        if(got.start!=want.start)error("GitGraph parser branch start");
        if(got.head!=want.head)error("GitGraph parser branch head");
    }
    return errors;
}
void parser_rejected(const native_git::GitGraph& actual,const Model& proven,Work& work,auto mutate,std::string_view name) {
    work.spend(proven.commits.size()*1024u+proven.branches.size()*1024u+2048u);
    auto forged=proven;mutate(forged);need(!parser_errors(actual,forged,work).empty(),name);
}
std::size_t prove_parser_controls(const native_git::GitGraph& actual,const Model& model,Work& work) {
    std::size_t rejected=0u;
    const auto check=[&](auto mutate,std::string_view name){parser_rejected(actual,model,work,mutate,name);++rejected;};
    check([](Model& bad){bad.direction=bad.direction=="LR"?"TB":"LR";},"GitGraph parser direction forgery");
    check([](Model& bad){bad.main_branch="__forged_main__";},"GitGraph parser main forgery");
    check([](Model& bad){bad.current_branch="__forged_current__";},"GitGraph parser current forgery");
    check([](Model& bad){bad.branches.front().name="__forged_branch__";},"GitGraph parser branch name forgery");
    check([](Model& bad){bad.branches.front().declaration_ordinal+=1;},"GitGraph parser branch declaration forgery");
    check([](Model& bad){bad.branches.front().start="__forged_start__";},"GitGraph parser branch start forgery");
    check([](Model& bad){bad.branches.front().head="__forged_head__";},"GitGraph parser branch head forgery");
    if(!model.commits.empty()) {
        check([](Model& bad){bad.commits.front().id="__forged_id__";},"GitGraph parser id forgery");
        check([](Model& bad){bad.commits.front().sequence+=1;},"GitGraph parser sequence forgery");
        check([](Model& bad){bad.commits.front().branch="__forged_branch__";},"GitGraph parser commit branch forgery");
        check([](Model& bad){bad.commits.front().type=bad.commits.front().type=="HIGHLIGHT" ? "NORMAL" : "HIGHLIGHT";},"GitGraph parser type forgery");
        check([](Model& bad){bad.commits.front().tag="__forged_tag__";},"GitGraph parser tag forgery");
        check([](Model& bad){bad.commits.front().parents={"__forged_parent__"};},"GitGraph parser parent forgery");
        check([](Model& bad){bad.commits.front().source_operation="merge";},"GitGraph parser operation forgery");
    }
    if(model.reset&&model.commits.size()>=3u) {
        const auto final=model.commits.size()-1u;
        const auto reject_parent=[&](const std::string& alternate,std::string_view name) {
            if(model.commits[final].parents.size()==1u&&model.commits[final].parents.front()!=alternate)
                check([&](Model& bad){bad.commits.back().parents={alternate};},name);
        };
        reject_parent(model.commits.front().id,"GitGraph parser reset former-head parent forgery");
        reject_parent(model.commits[model.commits.size()-2u].id,"GitGraph parser reset immediate-head parent forgery");
        if(model.commits.size()>=4u)reject_parent(model.commits[1u].id,"GitGraph parser reset known-parent forgery");
    }
    return rejected;
}

struct Frames { std::array<Grid,5> view; std::size_t published=0u; };
void publish_frame(json::JsonMutVal root,json::JsonMutVal row,std::string_view name,const Grid& grid,Work& work) {
    std::size_t payload=256u+name.size();
    for(const auto& cell:grid.cells) {
        const auto bytes=cell.text.size();
        if(bytes>std::numeric_limits<std::size_t>::max()-payload-96u) fail("GitGraph publication byte overflow");
        payload+=bytes+96u;
    }
    const auto cells=grid.cells.size();
    work.spend(product(cells,sizeof(Cell)+sizeof(std::string)*4u+128u,"GitGraph publication cell fee overflow")+product(payload,2u,"GitGraph publication JSON fee overflow")+8192u);
    auto frame=root.make_obj(),pixels=root.make_arr();frame.set("width",grid.width);frame.set("height",grid.height);
    for(const auto& cell:grid.cells) pixels.append(root.make_str(cell.text));
    frame.add("pixel_at_cells",pixels);row.add(name,frame);
}
void observe_and_publish(std::string_view source,json::JsonMutVal root,json::JsonMutVal row,Frames& frames,Work& work) {
    work.spend((source.size()+128u)*(32u*sizeof(std::string)+256u)+16384u);
    auto diagram=loom::ui::render_markdown(fenced(source,"mermaid"));auto ordinary=loom::ui::render_markdown(fenced(source,"text"));
    diagram->ComputeRequirement();ordinary->ComputeRequirement();
    const int height=std::max(32,std::max(diagram->requirement().min_y,ordinary->requirement().min_y)+12);
    need(height<=kMaxHeight,"GitGraph complete capture height");
    constexpr std::array<int,5> widths{{kWide,kNarrow,kWide,kWide,kNarrow}};
    constexpr std::array<std::string_view,5> names{{"wide_240","narrow_40","restored_240","source_control_240","source_control_40"}};
    for(std::size_t n=0;n<frames.view.size();++n) {
        frames.view[n]=capture_paid(n<3u?diagram:ordinary,widths[n],height,work);
        publish_frame(root,row,names[n],frames.view[n],work);
        ++frames.published;
    }
}
void lifecycle(const Frames& frames,Work& work) {
    need(frames.published==frames.view.size(),"GitGraph incomplete capture publication");
    for(const auto& grid:frames.view)need(find_text(grid,kFollowing,work).size()==1u,"GitGraph following token");
    need(frames.view[0]==frames.view[2],"GitGraph restored wide PixelAt grid");
    if(frames.view[0]==frames.view[3])need(frames.view[1]==frames.view[4],"GitGraph ordinary narrow lifecycle");
}
void write_report(const json::JsonMutDoc& document) {
#ifdef LOOM_MERMAID_REPORT_DIR
    std::ofstream out(fs::path(LOOM_MERMAID_REPORT_DIR)/"mermaid-gitgraph30-native-v2.json",std::ios::binary|std::ios::trunc);if(!out||!(out<<document.to_string()<<'\n'))fail("GitGraph report write");
#else
    static_cast<void>(document);
#endif
}

TEST(MermaidGitGraph30, TextScanPreservesCardinalityAndBoundsAdversarialPrefixes) {
    constexpr int width=240,height=8;
    constexpr std::size_t count=static_cast<std::size_t>(width*height);
    Work setup{kSetupLimit};
    setup.spend(8192u+count*(2u*sizeof(Cell)+128u)+4096u);
    Grid sparse{width,height,std::vector<Cell>(count)};
    setup.spend(8192u);
    const std::string needle="m"+std::string(78u,'a')+"z";
    const auto write=[&](int x,int y) {
        setup.spend(needle.size()*(sizeof(std::string)+32u)+256u);
        for(std::size_t n=0;n<needle.size();++n)
            sparse.edit({x+static_cast<int>(n),y}).text=std::string(1u,needle[n]);
    };
    write(120,4);
    Work one{20000u};
    const auto single=find_text(sparse,needle,one);
    ASSERT_EQ(single.size(),1u);
    EXPECT_EQ(single.front().x,120); EXPECT_EQ(single.front().y,4);
    EXPECT_GT(one.remaining,0u);
    write(0,5);
    Work two{20000u};
    const auto duplicate=find_text(sparse,needle,two);
    ASSERT_EQ(duplicate.size(),2u);
    EXPECT_EQ(duplicate.front().x,120); EXPECT_EQ(duplicate.front().y,4);
    EXPECT_GT(two.remaining,0u);

    setup.spend(count*(sizeof(Cell)+64u)+8192u);
    Grid repeated{width,height,std::vector<Cell>(count,Cell{"a"})};
    const std::string long_prefix=std::string(79u,'a')+"b";
    Work bounded{20000u};
    try {
        static_cast<void>(find_text(repeated,long_prefix,bounded));
        FAIL()<<"Adversarial prefix scan must stop at the unchanged Work bound";
    } catch(const std::runtime_error& caught) {
        EXPECT_STREQ(caught.what(),"GitGraph terminal oracle work exhausted");
        EXPECT_EQ(bounded.remaining,0u);
    }
}

TEST(MermaidGitGraph30, PrivateParserExactlyMatchesIndependentModels) {
    Work setup{kSetupLimit};const auto asset_bytes=read_exact(kRoot/kAsset,512u*1024u,setup);need(digest(asset_bytes)==kAssetSha256,"GitGraph immutable Model asset hash");
    const auto document=json::parse(asset_bytes);if(!document)fail("GitGraph immutable Model asset JSON");const auto root_value=document->root();
    need(root_value.get_string("schema")=="loom_gitgraph30_terminal_source_models/v1"&&root_value.get("cases").is_arr()&&root_value.get("cases").size()==30u,"GitGraph parser Model asset schema");
    std::size_t selected=0u,rejected=0u;
    root_value.get("cases").iter([&](JsonVal row) {
        ++selected;Work work{kSceneLimit};
        bind_after_capture(root_value,row,work);const auto expected=decode_bound_model(row,work);
        native_git::Work parser_work;const auto actual=native_git::parse_gitgraph(row.get_string("raw_source"),parser_work);
        need(actual.has_value(),"GitGraph private parser rejected bound complete source");
        const auto errors=parser_errors(*actual,expected,work);need(errors.empty(),errors.empty()?"GitGraph parser Model match":errors.front());
        rejected+=prove_parser_controls(*actual,expected,work);
    });
    EXPECT_EQ(selected,30u);EXPECT_EQ(rejected,416u);
}

TEST(MermaidGitGraph30, CellOracleHasGoodFirstStateAndRouteControls) {
    EXPECT_NO_THROW(good_first_controls());
    EXPECT_NO_THROW(commit_caption_boundary_controls());
    EXPECT_NO_THROW(cropped_translation_controls());
    EXPECT_NO_THROW(direct_adjacency_controls());
    EXPECT_NO_THROW(branch_lane_controls());
    EXPECT_NO_THROW(reset_skip_edge_controls());
    EXPECT_NO_THROW(tb_lane_controls());
    EXPECT_NO_THROW(bt_lane_controls());
}

TEST(MermaidGitGraph30, ThirtyCompleteSourcesAreActualPixelAtFirstRed) {
    Work setup{kSetupLimit};const auto asset_bytes=read_exact(kRoot/kAsset,512u*1024u,setup);need(digest(asset_bytes)==kAssetSha256,"GitGraph immutable Model asset hash");const auto document=json::parse(asset_bytes);if(!document)fail("GitGraph immutable Model asset JSON");const auto root_value=document->root();need(root_value.get_string("schema")=="loom_gitgraph30_terminal_source_models/v1"&&root_value.get("cases").is_arr()&&root_value.get("cases").size()==30u,"GitGraph immutable Model asset schema");
    json::JsonMutDoc report;auto out=report.object(),rows=out.make_arr();out.set("schema",2);out.set("kind","gitgraph30_private_terminal_compact_v2_first_red");out.set("qualification","Only actual complete non-fallback captures with source binding, decoded immutable Model, full compact cell geometry, and exact physical controls can earn native credit. PrivateParserExactlyMatchesIndependentModels separately compares the module-private parser to the independent Model; this test reads no Native report or producer semantic result.");out.set("asset_sha256",std::string(kAssetSha256));out.set("private_parser_semantic_test","MermaidGitGraph30.PrivateParserExactlyMatchesIndependentModels");
    std::size_t selected=0,admitted=0,empty_verified=0;
    root_value.get("cases").iter([&](JsonVal source_row) {
        ++selected;
        Work work{kSceneLimit};
        auto row=out.make_obj();
        row.set("scene_initial_budget",static_cast<std::int64_t>(kSceneLimit));
        bool admitted_row=false,empty_row=false,resource_error=false;
        MutationCounts controls{};Frames frames;std::string error,phase="capture_before_source_binding",case_id,source_sha;
        try {
            case_id=source_row.get_string("case_id");
            const auto source=source_row.get_string("raw_source");
            source_sha=source_row.get_string("raw_sha256");
            row.set("case_id",case_id);
            // Raw source drives all five PixelAt captures before any source
            // authority binding or expected-Model decode.
            observe_and_publish(source,out,row,frames,work);
            phase="lifecycle_after_capture";lifecycle(frames,work);
            phase="source_binding_after_capture";bind_after_capture(root_value,source_row,work);
            phase="model_decode_after_binding";const auto model=decode_bound_model(source_row,work);
            phase="wide_restored_narrow_geometry";
            const auto wide_errors=geometry_errors(frames.view[0],model,work);
            const auto restored_errors=geometry_errors(frames.view[2],model,work);
            const auto narrow_errors=frames.view[1]!=frames.view[4]
                ? geometry_errors(frames.view[1],model,work) : std::vector<std::string>{};
            const auto narrow_ptr=frames.view[1]!=frames.view[4] ? &narrow_errors : nullptr;
            if(!wide_errors.empty()||!restored_errors.empty()||!narrow_errors.empty())
                fail(geometry_detail(wide_errors,restored_errors,narrow_ptr,work));
            if(model.empty_zero_ink_exception) { empty_row=true;++empty_verified; }
            else {
                if(frames.view[0]==frames.view[3])fail("GitGraph source fallback is not drawing credit");
                phase="good_first_mutations";controls=prove_nonempty_controls(frames.view[0],model,work);admitted_row=true;++admitted;
            }
            phase="complete";
        } catch(const std::exception& caught) {
            error=caught.what();resource_error=std::string_view{caught.what()}=="GitGraph terminal oracle work exhausted";
        }
        row.set("source_sha256",source_sha);
        row.set("phase",phase);row.set("published_frames",static_cast<std::int64_t>(frames.published));
        row.set("resource_error",resource_error);row.set("empty_zero_ink_verified",empty_row);row.set("admitted",admitted_row);
        row.set("cell_physical_controls_rejected",static_cast<std::int64_t>(controls.physical));
        row.set("cell_model_controls_rejected",static_cast<std::int64_t>(controls.model));
        row.set("private_parser_semantic_controls_required",true);
        row.set("error",error);row.set("work_remaining",static_cast<std::int64_t>(work.remaining));rows.append(row);
    });
    out.add("rows",rows);out.set("selected_complete_sources",static_cast<std::int64_t>(selected));out.set("admitted_nonempty_sources",static_cast<std::int64_t>(admitted));out.set("empty_zero_ink_verified",static_cast<std::int64_t>(empty_verified));out.set("unselected_other_terminal_or_official_sources",true);report.set_root(out);write_report(report);
    EXPECT_EQ(selected,30u);EXPECT_EQ(empty_verified,1u);EXPECT_EQ(admitted,29u) << "A nonempty fallback, resource failure, schema failure, or cell-geometry failure is first-red; the single empty source proves only its zero-ink predicate and earns no drawing credit.";
}
// TEMPORARY E2a LENS B probe — do not keep.
namespace {
[[maybe_unused]] std::string render_text(const std::string& source, bool ascii = false) {
    auto element = ascii
        ? loom::ui::mermaid::render_mermaid(source, ftxui::text("ZZ_FALLBACK_ZZ"), true)
        : loom::ui::mermaid::render_mermaid(source, ftxui::text("ZZ_FALLBACK_ZZ"));
    if (!element) return std::string{"<nullopt>"};
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(200));
    ftxui::Render(screen, *element);
    return screen.ToString();
}
[[maybe_unused]] bool native_drawn(const std::string& text) {
    return text.find("ZZ_FALLBACK_ZZ") == std::string::npos &&
           text.find("gitGraph") == std::string::npos && text != "<nullopt>";
}
} // namespace

TEST(MermaidGitGraphEnvelope, DescriptionOnlyFrontmatterDraws) {
    const std::string source =
        "---\n"
        "description: >-\n"
        "  SecretDescriptionToken\n"
        "---\n"
        "gitGraph LR:\n"
        "  commit id:\"1\"\n"
        "  commit id:\"2\"\n"
        "  commit id:\"3\"\n";
    const auto text = render_text(source);
    EXPECT_TRUE(native_drawn(text)) << text;
}

TEST(MermaidGitGraphEnvelope, DescriptionIsNeverRendered) {
    const std::string source =
        "---\n"
        "description: >-\n"
        "  SecretDescriptionToken\n"
        "---\n"
        "gitGraph LR:\n"
        "  commit id:\"1\"\n"
        "  commit id:\"2\"\n";
    const auto text = render_text(source);
    ASSERT_TRUE(native_drawn(text)) << text;
    EXPECT_EQ(text.find("SecretDescriptionToken"), std::string::npos);
}

TEST(MermaidGitGraphEnvelope, TitleIsBlockedNotDropped) {
    const std::string source =
        "---\n"
        "description: >-\n"
        "  A titled gitGraph row has no title projection.\n"
        "title: simpleGitGraphTitle\n"
        "---\n"
        "gitGraph BT:\n"
        "  commit id:\"1\"\n";
    const auto text = render_text(source);
    // Whole source falls back rather than drawing with the title dropped.
    EXPECT_FALSE(native_drawn(text)) << text;
}

TEST(MermaidGitGraphEnvelope, ConfigFrontmatterStaysSource) {
    const std::string source =
        "---\n"
        "description: >-\n"
        "  d\n"
        "config:\n"
        "  gitGraph:\n"
        "    mainBranchName: trunk\n"
        "---\n"
        "gitGraph LR:\n"
        "  commit id:\"1\"\n";
    const auto text = render_text(source);
    EXPECT_FALSE(native_drawn(text)) << text;
}

TEST(MermaidGitGraphEnvelope, InitBearingFrontmatterStaysSource) {
    const std::string source =
        "---\n"
        "description: >-\n"
        "  d\n"
        "---\n"
        "%%{init: {\"gitGraph\": {\"mainBranchName\": \"trunk\"}}}%%\n"
        "gitGraph LR:\n"
        "  commit id:\"1\"\n";
    const auto text = render_text(source);
    EXPECT_FALSE(native_drawn(text)) << text;
}

TEST(MermaidGitGraphEnvelope, MultilineQuotedBranchLabelStaysSource) {
    const std::string source =
        "---\n"
        "description: >-\n"
        "  d\n"
        "---\n"
        "gitGraph LR:\n"
        "  commit id:\"1\"\n"
        "  branch \"Feature A\n"
        "  (ongoing)\"\n"
        "  commit id:\"2\"\n";
    const auto text = render_text(source);
    EXPECT_FALSE(native_drawn(text)) << text;
}

TEST(MermaidGitGraphEnvelope, AsciiProfileKeepsRawSemantics) {
    const std::string source =
        "---\n"
        "description: >-\n"
        "  d\n"
        "---\n"
        "gitGraph LR:\n"
        "  commit id:\"1\"\n"
        "  commit id:\"2\"\n";
    auto element = loom::ui::mermaid::render_mermaid(source, ftxui::text("fb"), true);
    EXPECT_FALSE(element);
}

} // namespace
