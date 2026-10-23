// Test-private Flow cell oracle and acquisition primitive.
// Mechanical extraction only: no TESTs, fixtures, report, or source parsing.
#pragma once
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
import std;
import loom.serdes.json;
import loom.crypto.crypto;
namespace loom::test::mermaid_flow {
namespace json = loom::utils::json;
using json::JsonVal;
inline constexpr int U = 1, R = 2, D = 4, L = 8;
inline constexpr std::string_view kSentinel = "AFTER_FLOW_SEMANTICS";
struct Point { int x = 0, y = 0; bool operator==(const Point&) const = default; };
inline constexpr std::array<Point, 4> kSteps{{{0,-1},{1,0},{0,1},{-1,0}}};
inline constexpr std::array<int, 4> kBits{{U,R,D,L}};
struct Glyph { int bits = 0, stroke = 0, arrow = -1; };
inline Glyph glyph(std::string_view text) {
    constexpr std::array<std::string_view, 4> heads{{"▲","▶","▼","◀"}};
    constexpr std::array<std::string_view, 4> other_heads{{"▲","►","▼","◄"}};
    for (int n = 0; n < 4; ++n)
        if (text == heads[static_cast<std::size_t>(n)] || text == other_heads[static_cast<std::size_t>(n)])
            return {kBits[static_cast<std::size_t>((n + 2) % 4)], 0, n};
    constexpr std::array<std::pair<std::string_view, int>, 15> light{{
        {"─",L|R},{"│",U|D},{"┌",R|D},{"┐",L|D},{"└",U|R},{"┘",U|L},
        {"├",U|R|D},{"┤",U|L|D},{"┬",L|R|D},{"┴",U|L|R},{"┼",U|R|D|L},
        {"╭",R|D},{"╮",L|D},{"╰",U|R},{"╯",U|L}}};
    for (const auto& [s,b] : light) if (text == s) return {b,0,-1};
    if (text == "┄" || text == "┈" || text == "┅" || text == "┉") return {L|R,1,-1};
    if (text == "┆" || text == "┊" || text == "┇" || text == "┋") return {U|D,1,-1};
    constexpr std::array<std::pair<std::string_view, int>, 11> heavy{{
        {"━",L|R},{"┃",U|D},{"┏",R|D},{"┓",L|D},{"┗",U|R},{"┛",U|L},
        {"┣",U|R|D},{"┫",U|L|D},{"┳",L|R|D},{"┻",U|L|R},{"╋",U|R|D|L}}};
    for (const auto& [s,b] : heavy) if (text == s) return {b,2,-1};
    return {};
}
struct Grid {
    int width = 0, height = 0;
    std::vector<std::string> cells;
    bool has(Point p) const { return p.x >= 0 && p.y >= 0 && p.x < width && p.y < height; }
    int index(Point p) const { return p.y * width + p.x; }
    Point point(int i) const { return {i % width,i / width}; }
    std::string_view at(Point p) const { return has(p) ? cells[static_cast<std::size_t>(index(p))] : std::string_view{}; }
    void put(Point p, std::string value) {
        if (!has(p)) throw std::runtime_error("Flow400 protocol cell write outside grid");
        cells.at(static_cast<std::size_t>(index(p))) = std::move(value);
    }
    std::string row(int y) const {
        std::string result;
        for (int x = 0; x < width; ++x) result += at({x,y});
        return result;
    }
    bool operator==(const Grid&) const = default;
};
struct OracleWork {
    std::size_t remaining = 8000000;
    void spend() {
        if (!remaining) throw std::runtime_error("Bounded cell-oracle work exhausted");
        --remaining;
    }
    void spend(std::size_t count) {
        if (count > remaining) { remaining = 0; throw std::runtime_error("Bounded cell-oracle work exhausted"); }
        remaining -= count;
    }
    void product(std::size_t count,std::size_t unit) {
        if (unit && count > remaining/unit) { remaining = 0; throw std::runtime_error("Bounded cell-oracle work exhausted"); }
        spend(count*unit);
    }
};
inline Grid capture(ftxui::Element element, int width, int height, OracleWork* paid_work = nullptr) {
    if (paid_work) {
        paid_work->spend(sizeof(ftxui::Screen)+16u*sizeof(std::size_t)+8u*sizeof(void*)+
            alignof(ftxui::Pixel)+alignof(std::vector<ftxui::Pixel>)+1024u);
        if (width<=0 || width>240 || height<=0 || height>1040) throw std::runtime_error("Flow400 protocol capture dimensions");
        const auto W=static_cast<std::size_t>(width),H=static_cast<std::size_t>(height),N=W*H;
        constexpr auto P=sizeof(ftxui::Pixel),R=sizeof(std::vector<ftxui::Pixel>);
        paid_work->product(H+1u,R);paid_work->product((H+1u)*W,P);paid_work->product(W,P);
        paid_work->product(N,2u*P);paid_work->product(H,2u*R);paid_work->product((H+1u)*W,2u);
        paid_work->product(W,4u);paid_work->product(N,4u);paid_work->product((H+1u)*W,16u);
    }
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, element);
    if (paid_work) {
        paid_work->spend(4u*sizeof(Grid)+8u*sizeof(std::size_t)+256u);
        paid_work->product(static_cast<std::size_t>(width*height),3u*sizeof(std::string)+8u);
        paid_work->product(static_cast<std::size_t>(height),4u);
    }
    Grid result{width,height,{}};
    result.cells.reserve(static_cast<std::size_t>(width * height));
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        if (paid_work) { paid_work->product(screen.PixelAt(x,y).character.size(),4u);paid_work->spend(8u); }
        result.cells.push_back(screen.PixelAt(x,y).character);
    }
    return result;
}

struct Rect {
    int left,top,right,bottom;
    bool operator==(const Rect&) const = default;
    bool interior(Point p) const { return p.x > left && p.x < right && p.y > top && p.y < bottom; }
    bool inside(const Rect& other) const { return left > other.left && top > other.top && right < other.right && bottom < other.bottom; }
    bool boundary(Point p) const {
        return ((p.x == left || p.x == right) && p.y >= top && p.y <= bottom) ||
               ((p.y == top || p.y == bottom) && p.x >= left && p.x <= right);
    }
    int area() const { return (right-left+1)*(bottom-top+1); }
};

// Closed test-private Model vocabulary. Its wire spelling is deliberately
// handled by parse_shape below, so Shape::Decision is never a JSON alias.
enum class Shape : std::uint8_t {
    Rectangle, Rounded, Decision,
    Stadium, Circle, Subroutine, DoubleCircle, Hexagon, Cylinder, Asymmetric,
    Trapezoid, TrapezoidAlt, Parallelogram, ParallelogramAlt,
};
struct ShapeFrame;
inline bool classic_shape(Shape shape);
inline bool classic_boundary(const ShapeFrame& frame,Point point);
inline bool classic_interior(const ShapeFrame& frame,Point point);
inline bool classic_decoration(const ShapeFrame& frame,Point point);
// Only the exact inner ring of a DoubleCircle is decoration. This does not
// relax ordinary nested entity ownership.
inline bool classic_decorative_frame(const ShapeFrame& child,const ShapeFrame& enclosing);
inline bool classic_legal_port(const ShapeFrame& frame,Point point,int outward);
inline bool classic_accepts_port(const Grid& grid,const ShapeFrame& frame,Point point,int outward);
inline int classic_perimeter_bits(const ShapeFrame& frame,Point point);
inline std::vector<ShapeFrame> classic_closed_frames(const Grid& grid,OracleWork& work);

struct ShapeFrame : Rect {
    Shape shape = Shape::Rectangle;
    bool operator==(const ShapeFrame&) const = default;
    int radius() const { return (bottom-top)/2; }
    bool boundary(Point p) const {
        if (classic_shape(shape)) return classic_boundary(*this,p);
        if (shape != Shape::Decision) return Rect::boundary(p);
        const int q=radius();
        if (q < 2 || p.y < top || p.y > bottom) return false;
        const int inset=std::abs(q-(p.y-top)),a=left+inset,b=right-inset;
        if (p.y==top || p.y==bottom) return p.x>=a && p.x<=b;
        return p.x==a || p.x==b;
    }
    bool interior(Point p) const {
        if (classic_shape(shape)) return classic_interior(*this,p);
        if (shape != Shape::Decision) return Rect::interior(p);
        const int q=radius();
        if (p.y<=top || p.y>=bottom || q<2) return false;
        const int inset=std::abs(q-(p.y-top));
        return p.x>left+inset && p.x<right-inset;
    }
    bool contains(Point p) const { return boundary(p) || interior(p); }
    bool legal_port(Point p,int outward) const {
        if (classic_shape(shape)) return classic_legal_port(*this,p,outward);
        if (!boundary(p)) return false;
        if (shape != Shape::Decision) {
            if (p.y==top || p.y==bottom) return p.x>left && p.x<right && (outward==(p.y==top?U:D));
            if (p.x==left || p.x==right) return p.y>top && p.y<bottom && (outward==(p.x==left?L:R));
            return false;
        }
        const int q=radius(),middle=top+q;
        if (p.y==middle && p.x==left) return outward==L;
        if (p.y==middle && p.x==right) return outward==R;
        if (p.y==top || p.y==bottom) return p.x>left+q && p.x<right-q && outward==(p.y==top?U:D);
        return false;
    }
    bool accepts_port(const Grid& grid,Point p,int outward) const {
        if (classic_shape(shape)) return classic_accepts_port(grid,*this,p,outward);
        if (!legal_port(p,outward)) return false;
        // Decision side terminals are physical shape points, not line arms.
        if (shape==Shape::Decision && p.y==top+radius() && (p.x==left || p.x==right)) return true;
        return (glyph(grid.at(p)).bits & outward)!=0;
    }
    int perimeter_bits(Point p) const {
        if (classic_shape(shape)) return classic_perimeter_bits(*this,p);
        if (!boundary(p)) return 0;
        if (shape==Shape::Decision) {
            if ((p.y==top || p.y==bottom) && p.x>left+radius() && p.x<right-radius()) return L|R;
            return 0;
        }
        int result=0;
        if (p.y==top || p.y==bottom) { if (p.x>left) result|=L; if (p.x<right) result|=R; }
        if (p.x==left || p.x==right) { if (p.y>top) result|=U; if (p.y<bottom) result|=D; }
        return result;
    }
};

inline std::vector<Rect> closed_rectangles(const Grid& grid,OracleWork& work) {
    std::vector<Rect> frames;
    auto has = [&](int x,int y,int bits) { return (glyph(grid.at({x,y})).bits & bits) == bits; };
    for (int top = 0; top + 2 < grid.height; ++top) for (int left = 0; left + 2 < grid.width; ++left) {
        work.spend();
        if (!has(left,top,R|D)) continue;
        for (int right = left+1; right < grid.width; ++right) {
            work.spend();
            if (!has(right,top,L)) break;
            if (right > left+1 && has(right,top,L|D)) {
                for (int bottom = top+1; bottom < grid.height; ++bottom) {
                    work.spend();
                    if (!has(left,bottom,U)) break;
                    if (bottom > top+1 && has(left,bottom,U|R) && has(right,bottom,U|L)) {
                        bool valid = true;
                        for (int x = left+1; valid && x < right; ++x) { work.spend(); valid = has(x,bottom,L|R); }
                        for (int y = top+1; valid && y < bottom; ++y) { work.spend(); valid = has(left,y,U|D) && has(right,y,U|D); }
                        if (valid) {
                            if (frames.size() >= 128) throw std::runtime_error("Too many candidate closed frames");
                            frames.push_back({left,top,right,bottom});
                        }
                    }
                    if (!has(left,bottom,D)) break;
                }
            }
            if (!has(right,top,R)) break;
        }
    }
    std::ranges::sort(frames, [](const Rect& a,const Rect& b) { return a.area() < b.area(); });
    return frames;
}
inline bool exact_box(const Grid& grid,const ShapeFrame& frame,OracleWork& work) {
    const auto exact=[&](Point p,std::string_view expected) { work.spend(); return grid.at(p)==expected; };
    if (frame.right-frame.left<2 || frame.bottom-frame.top<2) return false;
    const bool rounded=frame.shape==Shape::Rounded;
    if (frame.shape==Shape::Decision) return false;
    if (!exact({frame.left,frame.top},rounded?"╭":"┌") || !exact({frame.right,frame.top},rounded?"╮":"┐") ||
        !exact({frame.left,frame.bottom},rounded?"╰":"└") || !exact({frame.right,frame.bottom},rounded?"╯":"┘")) return false;
    for (int x=frame.left+1;x<frame.right;++x) {
        work.spend();
        if ((glyph(grid.at({x,frame.top})).bits&(L|R))!=(L|R) || (glyph(grid.at({x,frame.bottom})).bits&(L|R))!=(L|R)) return false;
    }
    for (int y=frame.top+1;y<frame.bottom;++y) {
        work.spend();
        if ((glyph(grid.at({frame.left,y})).bits&(U|D))!=(U|D) || (glyph(grid.at({frame.right,y})).bits&(U|D))!=(U|D)) return false;
    }
    return true;
}

inline bool exact_decision(const Grid& grid,const ShapeFrame& frame,OracleWork& work) {
    const int h=frame.bottom-frame.top+1,q=(h-1)/2;
    if (frame.shape!=Shape::Decision || h<5 || (h%2)==0 || frame.right-frame.left+1<2*q+5) return false;
    for (int local=0;local<=2*q;++local) {
        work.spend(); const int inset=std::abs(q-local),y=frame.top+local;
        const Point a{frame.left+inset,y},b{frame.right-inset,y};
        const auto pair=local<q?std::pair<std::string_view,std::string_view>{"╱","╲"}:
            local==q?std::pair<std::string_view,std::string_view>{"◇","◇"}:
            std::pair<std::string_view,std::string_view>{"╲","╱"};
        if (grid.at(a)!=pair.first || grid.at(b)!=pair.second) return false;
        if (local==0 || local==2*q) for (int x=a.x+1;x<b.x;++x) {
            work.spend(); if ((glyph(grid.at({x,y})).bits&(L|R))!=(L|R)) return false;
        }
    }
    return true;
}

inline std::vector<ShapeFrame> closed_frames(const Grid& grid,OracleWork& work) {
    // Reserve every new candidate payload and all discovery scans before any
    // candidate vector/loop is entered; the existing 128M per-scene cap stays.
    work.spend(8u*sizeof(ShapeFrame)+4u*sizeof(std::vector<ShapeFrame>)+512u);
    work.product(grid.cells.size(),12u);
    std::vector<ShapeFrame> result;
    auto append=[&](ShapeFrame frame) {
        if (result.size()>=128u) throw std::runtime_error("Too many typed Flow contour candidates");
        if (std::ranges::none_of(result,[&](const ShapeFrame& other) {
            return other.left==frame.left && other.top==frame.top && other.right==frame.right && other.bottom==frame.bottom && other.shape==frame.shape;
        })) result.push_back(frame);
    };
    for (const auto& rect:closed_rectangles(grid,work)) {
        ShapeFrame sharp{{rect.left,rect.top,rect.right,rect.bottom},Shape::Rectangle};
        if (exact_box(grid,sharp,work)) append(sharp);
    }
    // Rounded candidates share the bounded corner/run scan but reject mixed corners.
    for (int top=0;top+2<grid.height;++top) for (int left=0;left+2<grid.width;++left) {
        work.spend(); if (grid.at({left,top})!="╭") continue;
        for (int right=left+2;right<grid.width;++right) {
            work.spend(); if (!(glyph(grid.at({right,top})).bits&L)) break;
            if (grid.at({right,top})!="╮") continue;
            for (int bottom=top+2;bottom<grid.height;++bottom) {
                work.spend(); if (!(glyph(grid.at({left,bottom})).bits&U)) break;
                ShapeFrame rounded{{left,top,right,bottom},Shape::Rounded};
                if (grid.at({left,bottom})=="╰" && grid.at({right,bottom})=="╯" && exact_box(grid,rounded,work)) append(rounded);
            }
        }
    }
    // A decision is anchored only by its two middle-row ◇ terminals.  The
    // bounded q scan validates every contour cell and never derives ownership
    // from a producer Box or route.
    for (int middle=2;middle+2<grid.height;++middle) for (int left=0;left<grid.width;++left) {
        work.spend(); if (grid.at({left,middle})!="◇") continue;
        for (int right=left+5;right<grid.width;++right) {
            work.spend(); if (grid.at({right,middle})!="◇") continue;
            const int max_q=std::min({middle,grid.height-1-middle,(right-left-4)/2});
            for (int q=2;q<=max_q;++q) {
                work.spend(); ShapeFrame decision{{left,middle-q,right,middle+q},Shape::Decision};
                if (exact_decision(grid,decision,work)) append(decision);
            }
        }
    }
    std::ranges::sort(result,[](const ShapeFrame& a,const ShapeFrame& b) { return a.area()<b.area(); });
    return result;
}

inline std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    while (true) {
        const auto end = text.find('\n'); lines.emplace_back(text.substr(0,end));
        if (end == std::string_view::npos) return lines;
        text.remove_prefix(end+1);
    }
}
inline std::vector<Point> occurrences(const Grid& grid,std::string_view text,const Rect& bounds,OracleWork& work) {
    std::vector<Point> positions;
    const auto glyphs = ftxui::Utf8ToGlyphs(std::string(text));
    const int size = ftxui::string_width(std::string(text));
    if (text.empty()) return positions;
    for (int y = bounds.top+1; y < bounds.bottom; ++y) for (int x = bounds.left+1; x+size <= bounds.right; ++x) {
        work.spend();
        int column = x; bool equal = true;
        for (const auto& value : glyphs) {
            if (grid.at({column,y}) != value) { equal = false; break; }
            ++column;
        }
        if (!equal) continue;
        // An authored short ID must not match a substring of another label.
        auto word = [](std::string_view value) {
            return value.size() == 1 && ((value[0] >= 'a' && value[0] <= 'z') ||
                (value[0] >= 'A' && value[0] <= 'Z') || (value[0] >= '0' && value[0] <= '9') || value[0] == '_');
        };
        if (word(std::string_view(text).substr(0,1)) && word(grid.at({x-1,y}))) continue;
        if (word(std::string_view(text).substr(text.size()-1)) && word(grid.at({column,y}))) continue;
        positions.push_back({x,y});
    }
    return positions;
}
struct Label { std::vector<Point> starts; Rect bounds; };
inline std::vector<Label> label_blocks(const Grid& grid,std::string_view label,const Rect& bounds,OracleWork& work) {
    const auto lines = split_lines(label);
    if (lines.empty() || lines.front().empty()) return {};
    std::vector<Label> matches;
    for (const auto first : occurrences(grid,lines.front(),bounds,work)) {
        std::vector<Point> chosen{first};
        bool valid = true; int previous = first.y;
        for (std::size_t n = 1; n < lines.size(); ++n) {
            if (lines[n].empty()) { chosen.push_back({first.x,++previous}); continue; }
            const auto options = occurrences(grid,lines[n],bounds,work);
            const auto found = std::ranges::find_if(options,[&](Point p) { return p.y == previous+1; });
            if (found == options.end()) { valid = false; break; }
            chosen.push_back(*found); previous = found->y;
        }
        if (!valid) continue;
        int left = first.x,right = first.x;
        for (std::size_t n = 0; n < lines.size(); ++n) {
            left = std::min(left,chosen[n].x);
            right = std::max(right,chosen[n].x+ftxui::string_width(lines[n])-1);
            if (lines[n].empty()) {
                for (int x = left; x <= right; ++x)
                    if (grid.at({x,chosen[n].y}) != " ") valid = false;
            }
        }
        if (valid) matches.push_back({chosen,{left-1,first.y-1,right+1,previous+1}});
    }
    return matches;
}
struct Node { std::string id,label; Shape shape = Shape::Rectangle; };
struct Edge { int from,to; std::string label,style; bool start = false,end = true; };
inline Shape parse_shape(std::string_view wire) {
    if (wire=="rectangle") return Shape::Rectangle;
    if (wire=="rounded") return Shape::Rounded;
    if (wire=="diamond") return Shape::Decision;
    if (wire=="stadium") return Shape::Stadium;
    if (wire=="circle") return Shape::Circle;
    if (wire=="subroutine") return Shape::Subroutine;
    if (wire=="double_circle") return Shape::DoubleCircle;
    if (wire=="hexagon") return Shape::Hexagon;
    if (wire=="cylinder") return Shape::Cylinder;
    if (wire=="asymmetric") return Shape::Asymmetric;
    if (wire=="trapezoid") return Shape::Trapezoid;
    if (wire=="trapezoid_alt") return Shape::TrapezoidAlt;
    if (wire=="parallelogram") return Shape::Parallelogram;
    if (wire=="parallelogram_alt") return Shape::ParallelogramAlt;
    throw std::runtime_error("Flow typed Model unknown shape wire token");
}
struct Group { std::string id,label,direction; int parent = -1; std::vector<int> nodes; };
struct Model { std::string direction; std::vector<Node> nodes; std::vector<Edge> edges; std::vector<Group> groups; };
// LinkProfile is a closed test-private extension for complete Flow400
// sources whose syntax carries endpoint markers and an edge minimum length.
// The legacy Model and flow400_geometry path remain profile-free.
enum class LinkMarker : std::uint8_t { None, Point };
struct LinkProfileEdge { LinkMarker source = LinkMarker::None,target = LinkMarker::Point; int minimum_length = 1; };
struct LinkProfile { std::vector<LinkProfileEdge> edges; std::vector<int> node_ranks; };
inline bool linkprofile3_profile_shape(const Model& model,const LinkProfile& profile) {
    if(model.direction!="TD" || !model.groups.empty() || profile.edges.size()!=model.edges.size() ||
        profile.node_ranks.size()!=model.nodes.size()) return false;
    for(const auto& node:model.nodes)
        if(node.shape!=Shape::Rectangle || node.label.empty() || node.label.find('\n')!=std::string::npos) return false;
    // This profile is closed to finite captured grids. Validate every rank before
    // deriving max_rank+1, including isolated Model nodes.
    for(const int rank:profile.node_ranks) if(rank<0 || rank>1040) return false;
    for(std::size_t at=0;at<model.edges.size();++at){
        const auto& edge=model.edges[at];const auto& declared=profile.edges[at];
        if(edge.from<0 || edge.to<0 || static_cast<std::size_t>(edge.from)>=model.nodes.size() || static_cast<std::size_t>(edge.to)>=model.nodes.size() ||
            declared.minimum_length<1 || declared.minimum_length>5) return false;
        const bool source=declared.source==LinkMarker::Point,target=declared.target==LinkMarker::Point;
        if((source&&!target) || edge.start!=source || edge.end!=target) return false;
        if(profile.node_ranks[static_cast<std::size_t>(edge.to)]-profile.node_ranks[static_cast<std::size_t>(edge.from)]<declared.minimum_length) return false;
    }
    return true;
}
inline Model parse_model(JsonVal value) {
    Model result; result.direction = value.get_string("direction");
    value.get("nodes").iter([&](JsonVal pair) {
        const auto node = pair.at(1);
        result.nodes.push_back({node.get_string("id"),node.get_string("label"),parse_shape(node.get_string("shape"))});
    });
    auto index = [&](std::string_view id) {
        const auto found = std::ranges::find_if(result.nodes,[&](const Node& n) { return n.id == id; });
        if (found == result.nodes.end()) throw std::runtime_error("Upstream edge/group references absent node");
        return static_cast<int>(found-result.nodes.begin());
    };
    value.get("edges").iter([&](JsonVal e) {
        result.edges.push_back({index(e.get_string("source")),index(e.get_string("target")),
            e.get_string("label"),e.get_string("style"),e.get("hasArrowStart").as_bool(),e.get("hasArrowEnd").as_bool()});
    });
    auto add_groups = [&](auto&& self,JsonVal groups,int parent) -> void {
        groups.iter([&](JsonVal g) {
            const int n = static_cast<int>(result.groups.size());
            Group group{g.get_string("id"),g.get_string("label"),g.get_string("direction"),parent,{}};
            g.get("nodeIds").iter([&](JsonVal id) { group.nodes.push_back(index(id.as_str())); });
            result.groups.push_back(std::move(group)); self(self,g.get("children"),n);
        });
    };
    add_groups(add_groups,value.get("subgraphs"),-1);
    return result;
}
inline std::vector<int> frame_candidates(const Grid& grid,const std::vector<ShapeFrame>& frames,std::string_view label,Shape shape,OracleWork& work) {
    work.spend(4u*sizeof(int)+256u);
    const bool excludes_decorative_children=classic_shape(shape);
    if (excludes_decorative_children) work.product(frames.size(),frames.size());
    std::vector<int> result;
    for (std::size_t n = 0; n < frames.size(); ++n) {
        if (frames[n].shape!=shape) continue;
        if (excludes_decorative_children) {
            bool decorative_child=false;
            for (const auto& enclosing:frames) {
                if (classic_decorative_frame(frames[n],enclosing)) { decorative_child=true; break; }
            }
            if (decorative_child) continue;
        }
        for (const auto& match : label_blocks(grid,label,frames[n],work)) {
            bool direct = true;
            for (std::size_t other = 0; other < frames.size(); ++other) {
                if (classic_decorative_frame(frames[other],frames[n])) continue;
                if (!frames[other].inside(frames[n])) continue;
                for (const auto start : match.starts) if (frames[other].interior(start)) direct = false;
            }
            if (direct) { result.push_back(static_cast<int>(n)); break; }
        }
    }
    return result;
}
inline void claim_label(const Grid& grid,std::string_view text,const Label& block,std::vector<bool>& claimed) {
    const auto lines = split_lines(text);
    for (std::size_t n = 0; n < lines.size(); ++n) {
        int x = block.starts[n].x;
        const auto cells = ftxui::Utf8ToGlyphs(lines[n]);
        for (std::size_t column = 0; column < cells.size(); ++column) {
            const Point p{x+static_cast<int>(column),block.starts[n].y};
            if (grid.has(p)) claimed[static_cast<std::size_t>(grid.index(p))] = true;
        }
    }
}
inline bool complete_node_content(const Grid& grid,const ShapeFrame& frame,std::string_view label,OracleWork& work) {
    // The label finder uses the enclosing bbox; the completeness pass below
    // claims only cells in the approved physical interior.
    for (const auto& block : label_blocks(grid,label,frame,work)) {
        std::vector<bool> claimed(grid.cells.size());
        claim_label(grid,label,block,claimed);
        bool complete = true;
        const auto lines=split_lines(label);
        for (std::size_t row=0;complete&&row<lines.size();++row) {
            const auto cells=ftxui::Utf8ToGlyphs(lines[row]);
            for (std::size_t column=0;column<cells.size();++column) {
                work.spend();
                if (!frame.interior({block.starts[row].x+static_cast<int>(column),block.starts[row].y})) { complete=false; break; }
            }
        }
        for (int y = frame.top+1; complete && y < frame.bottom; ++y) {
            for (int x = frame.left+1; x < frame.right; ++x) {
                work.spend(); const Point p{x,y}; if (!frame.interior(p)) continue;
                if (grid.at(p) != " " && !grid.at(p).empty() && !claimed[static_cast<std::size_t>(grid.index(p))]) {
                    complete = false; break;
                }
            }
        }
        if (complete) return true;
    }
    return false;
}
struct Trace {
    Point head;
    int target = -1;
    // labels are indexes into the distinct physical LabelRegion inventory.
    struct Source { int node; std::array<int,3> straight_strokes{}; std::vector<int> labels; bool valid = true; std::vector<int> path{}; Point border{}; int outward = 0; };
    std::vector<Source> sources;
};
struct LabelRegion { Label block; std::string text; };

// Optional test-private group endpoint profile. The default Flow Model remains
// node-only; a profile borrows string_views from a caller-owned decoded Model
// or live Json document and therefore is rebuilt after every semantic copy.
enum class GroupEndpointKind : std::uint8_t { Vertex, Group };
struct GroupEndpointRef { GroupEndpointKind kind = GroupEndpointKind::Vertex; std::size_t index = 0u; };
struct GroupEndpointEdge { GroupEndpointRef source,target; std::string_view label,style; bool arrow_start = false,arrow_end = true; };
enum class GroupEndpointOrderKind : std::uint8_t { Ordinary, Tagged };
struct GroupEndpointOrder { GroupEndpointOrderKind kind = GroupEndpointOrderKind::Ordinary; std::size_t index = 0u; };
struct GroupEndpointProfile {
    std::vector<GroupEndpointEdge> tagged;
    // This is the source statement order, not a convenient ordinary-then-tagged order.
    std::vector<GroupEndpointOrder> source_order;
};

inline std::vector<std::string> check_group_endpoint_assignment(const Grid&,const Model&,const GroupEndpointProfile&,const std::vector<ShapeFrame>&,const std::vector<int>&,const std::vector<LabelRegion>&,OracleWork&,const int*);

// The marker matcher is intentionally entered only for a non-null closed
// LinkProfile.  The legacy target-arrow trace below remains its own branch.
struct LinkProfileEndpoint { Point border,outside; int outward = 0; bool point = false; };
struct LinkProfileRoute { std::vector<int> cells,labels; LinkProfileEndpoint source,target; };
struct LinkProfileSearch { std::vector<int> parents,bridges,queue,used; };
inline int linkprofile3_direction(Point from,Point to) {
    if(from.x==to.x&&from.y!=to.y)return to.y<from.y?0:2;
    if(from.y==to.y&&from.x!=to.x)return to.x<from.x?3:1;
    throw std::runtime_error("LinkProfile3 route has nonorthogonal cells");
}
inline std::vector<LinkProfileEndpoint> linkprofile3_endpoints(const Grid& grid,const ShapeFrame& frame,bool source,bool point,
                                                               OracleWork& work) {
    // Flow direction constrains rank bands, not which rectangle face may
    // carry a link. Observe every legal perimeter port, preserving its normal.
    const int horizontal=frame.right-frame.left-1,vertical=frame.bottom-frame.top-1;
    if(horizontal<1||vertical<1)return {};
    const auto span=static_cast<std::size_t>(2*(horizontal+vertical));
    work.spend(4u*sizeof(std::vector<LinkProfileEndpoint>)+256u);
    work.product(span,3u*sizeof(LinkProfileEndpoint)+16u);
    std::vector<LinkProfileEndpoint> result;result.reserve(span);
    const std::array<int,4> directions=source?std::array<int,4>{2,1,3,0}:std::array<int,4>{0,3,1,2};
    for(const int direction:directions){
        const bool across=direction==0||direction==2;
        const int first=across?frame.left+1:frame.top+1,last=across?frame.right-1:frame.bottom-1;
        for(int coordinate=first;coordinate<=last;++coordinate){
            work.spend();const Point border=across?Point{coordinate,direction==0?frame.top:frame.bottom}:
                Point{direction==3?frame.left:frame.right,coordinate};
            const Point outside{border.x+kSteps[static_cast<std::size_t>(direction)].x,border.y+kSteps[static_cast<std::size_t>(direction)].y};
            if(!grid.has(outside)||!frame.legal_port(border,kBits[static_cast<std::size_t>(direction)]))continue;
            const auto value=glyph(grid.at(outside));
            if(point) {
                if(value.arrow!=(direction+2)%4 || !(value.bits&kBits[static_cast<std::size_t>(direction)]))continue;
            } else if(value.arrow>=0||!frame.accepts_port(grid,border,kBits[static_cast<std::size_t>(direction)])||
                !(value.bits&kBits[static_cast<std::size_t>((direction+2)%4)])) continue;
            result.push_back({border,outside,kBits[static_cast<std::size_t>(direction)],point});
        }
    }
    return result;
}
inline int linkprofile3_bits(const Grid& grid,Point point,const std::vector<ShapeFrame>& nodes) {
    if(!grid.has(point))return 0;
    for(const auto& frame:nodes)if(frame.contains(point))return 0;
    return glyph(grid.at(point)).bits;
}
inline std::optional<LinkProfileRoute> linkprofile3_route(const Grid& grid,const std::vector<ShapeFrame>& nodes,
    const std::vector<LabelRegion>& labels,const LinkProfileEndpoint& source,const LinkProfileEndpoint& target,
    OracleWork& work,LinkProfileSearch& search) {
    work.spend(8u*sizeof(int)+4u*sizeof(LinkProfileRoute)+1024u);
    // Each finite port-pair attempt reuses the one paid scene workspace.
    // Resets are still charged before touching every shared array cell.
    work.product(grid.cells.size(),3u);
    auto& parents=search.parents;auto& bridges=search.bridges;auto& queue=search.queue;auto& used=search.used;
    std::ranges::fill(parents,-2);std::ranges::fill(bridges,-1);std::ranges::fill(used,0);queue.clear();
    const int start=grid.index(source.outside),goal=grid.index(target.outside);
    parents[static_cast<std::size_t>(start)]=-1;queue.push_back(start);
    for(std::size_t cursor=0u;cursor<queue.size();++cursor){
        work.spend();const int current=queue[cursor];const Point here=grid.point(current);
        if(current==goal)break;
        const int bits=linkprofile3_bits(grid,here,nodes);
        for(int direction=0;direction<4;++direction){
            work.spend();if(!(bits&kBits[static_cast<std::size_t>(direction)]))continue;
            const Point next{here.x+kSteps[static_cast<std::size_t>(direction)].x,here.y+kSteps[static_cast<std::size_t>(direction)].y};
            if(!grid.has(next)||next==source.outside)continue;
            if(next!=target.outside&&glyph(grid.at(next)).arrow>=0)continue;
            if(!(linkprofile3_bits(grid,next,nodes)&kBits[static_cast<std::size_t>((direction+2)%4)]))continue;
            const int index=grid.index(next);if(parents[static_cast<std::size_t>(index)]!=-2)continue;
            parents[static_cast<std::size_t>(index)]=current;queue.push_back(index);
        }
        for(std::size_t at=0u;at<labels.size();++at){
            const auto& box=labels[at].block.bounds;if(here.y<=box.top||here.y>=box.bottom)continue;
            const bool left=here.x==box.left,right=here.x==box.right;if((!left&&!right)||!(bits&(left?R:L)))continue;
            const Point opposite{left?box.right:box.left,here.y};
            if(!(linkprofile3_bits(grid,opposite,nodes)&(left?L:R)))continue;
            const int index=grid.index(opposite);if(parents[static_cast<std::size_t>(index)]!=-2)continue;
            parents[static_cast<std::size_t>(index)]=current;bridges[static_cast<std::size_t>(index)]=static_cast<int>(at);queue.push_back(index);
        }
    }
    if(parents[static_cast<std::size_t>(goal)]==-2)return std::nullopt;
    // Successful paths and caption indices outlive this search attempt.
    // Count them first, then prepay their exact owned capacity before reserve.
    std::size_t path_size=0u,label_count=0u;
    for(int at=goal;at>=0;at=parents[static_cast<std::size_t>(at)]){
        work.spend();++path_size;if(bridges[static_cast<std::size_t>(at)]>=0)++label_count;
    }
    work.product(path_size,3u*sizeof(int)+16u);work.product(label_count,3u*sizeof(int)+16u);
    LinkProfileRoute result;result.source=source;result.target=target;result.labels.reserve(label_count);
    std::vector<int> path;path.reserve(path_size);
    for(int at=goal;at>=0;at=parents[static_cast<std::size_t>(at)]){
        work.spend();path.push_back(at);if(bridges[static_cast<std::size_t>(at)]>=0)result.labels.push_back(bridges[static_cast<std::size_t>(at)]);
    }
    std::ranges::sort(result.labels);result.labels.erase(std::unique(result.labels.begin(),result.labels.end()),result.labels.end());
    for(std::size_t at=1u;at<path.size();++at){
        const Point child=grid.point(path[at-1u]),parent=grid.point(path[at]);
        if(child.y==parent.y&&(child.x==parent.x+1||parent.x==child.x+1)){
            const int direction=child.x<parent.x?1:3;used[static_cast<std::size_t>(path[at-1u])]|=kBits[static_cast<std::size_t>(direction)];used[static_cast<std::size_t>(path[at])]|=kBits[static_cast<std::size_t>((direction+2)%4)];
        }else if(child.x==parent.x&&(child.y==parent.y+1||parent.y==child.y+1)){
            const int direction=child.y<parent.y?2:0;used[static_cast<std::size_t>(path[at-1u])]|=kBits[static_cast<std::size_t>(direction)];used[static_cast<std::size_t>(path[at])]|=kBits[static_cast<std::size_t>((direction+2)%4)];
        }else if(child.y==parent.y){
            const int direction=child.x<parent.x?1:3;used[static_cast<std::size_t>(path[at-1u])]|=kBits[static_cast<std::size_t>(direction)];used[static_cast<std::size_t>(path[at])]|=kBits[static_cast<std::size_t>((direction+2)%4)];
        }else return std::nullopt;
    }
    const auto inward=[](int outward){return outward==U?D:outward==R?L:outward==D?U:R;};
    if(!source.point)used[static_cast<std::size_t>(start)]|=inward(source.outward);
    if(!target.point)used[static_cast<std::size_t>(goal)]|=inward(target.outward);
    std::array<int,3> straight{};
    for(const int index:path){
        const int actual=linkprofile3_bits(grid,grid.point(index),nodes),expected=used[static_cast<std::size_t>(index)];
        if(actual!=expected)return std::nullopt;
        const auto value=glyph(grid.at(grid.point(index)));
        if((expected==(L|R)||expected==(U|D))&&value.arrow<0)++straight[static_cast<std::size_t>(value.stroke)];
    }
    const auto style=[&](int wanted){return wanted==0?straight[1]==0&&straight[2]==0:wanted==1?straight[1]>0&&straight[0]==0&&straight[2]==0:straight[2]>0&&straight[0]==0&&straight[1]==0;};
    // A style is selected by the caller after the route's endpoint and caption facts.
    if(path.empty() || (!style(0)&&!style(1)&&!style(2)))return std::nullopt;
    result.cells=std::move(path);return result;
}
inline bool linkprofile3_style_matches(const Grid& grid,const LinkProfileRoute& route,const std::vector<ShapeFrame>& nodes,std::string_view style) {
    std::array<int,3> straight{};
    for(std::size_t at=0u;at<route.cells.size();++at){
        const auto point=grid.point(route.cells[at]);const auto value=glyph(grid.at(point));if(value.arrow>=0)continue;
        const int bits=linkprofile3_bits(grid,point,nodes);if(bits==(L|R)||bits==(U|D))++straight[static_cast<std::size_t>(value.stroke)];
    }
    return style=="solid"?straight[1]==0&&straight[2]==0:style=="dotted"?straight[1]>0&&straight[0]==0&&straight[2]==0:style=="thick"?straight[2]>0&&straight[0]==0&&straight[1]==0:false;
}
inline std::vector<std::string> check_linkprofile3_assignment(const Grid& grid,const Model& model,const LinkProfile& profile,
    const std::vector<ShapeFrame>& frames,const std::vector<int>& assignment,const std::vector<LabelRegion>& labels,OracleWork& work,const int* following_row) {
    work.spend(4u*sizeof(std::vector<std::string>)+512u);std::vector<std::string> errors;
    if(!linkprofile3_profile_shape(model,profile))return {"LinkProfile3 closed Model/profile contract differs"};
    work.spend(4u*sizeof(std::vector<ShapeFrame>)+512u);work.product(model.nodes.size(),2u*sizeof(ShapeFrame)+sizeof(int)+16u);
    std::vector<ShapeFrame> nodes;nodes.reserve(model.nodes.size());for(std::size_t at=0u;at<model.nodes.size();++at)nodes.push_back(frames.at(static_cast<std::size_t>(assignment.at(at))));
    int origin=-1,max_rank=0;for(std::size_t at=0u;at<nodes.size();++at){
        const int rank=profile.node_ranks[at];if(nodes[at].bottom-nodes[at].top!=4)return {"LinkProfile3 node height differs from rank contract"};
        max_rank=std::max(max_rank,rank);if(rank==0){if(origin<0)origin=nodes[at].top;else if(nodes[at].top!=origin)return {"LinkProfile3 rank-zero band differs"};}
    }
    if(origin<0)return {"LinkProfile3 lacks rank-zero source band"};
    work.spend(4u*sizeof(std::vector<int>)+256u);work.product(static_cast<std::size_t>(max_rank+1),sizeof(int)+8u);std::vector<int> band_tops(static_cast<std::size_t>(max_rank+1),-1);
    for(std::size_t at=0u;at<nodes.size();++at){const int rank=profile.node_ranks[at],top=nodes[at].top;auto& band=band_tops[static_cast<std::size_t>(rank)];
        if(band<0)band=top;else if(band!=top)return {"LinkProfile3 same-rank band differs"};}
    int next=origin;
    for(int rank=0;rank<=max_rank;++rank){const auto& observed=band_tops[static_cast<std::size_t>(rank)];if(observed>=0&&observed!=next)return {"LinkProfile3 source-derived rank band differs"};
        // Occupied ranks advance by the fixed five-cell node height plus the
        // existing eight-cell major gap. Empty source ranks advance by the gap.
        next+=((observed>=0)?5:0)+8;}
    for(std::size_t at=0u;at<model.edges.size();++at)if(profile.node_ranks[static_cast<std::size_t>(model.edges[at].to)]-profile.node_ranks[static_cast<std::size_t>(model.edges[at].from)]<profile.edges[at].minimum_length)return {"LinkProfile3 minimum rank difference differs"};
    std::size_t arrows=0u,expected_arrows=0u;for(const auto& edge:profile.edges){expected_arrows+=(edge.source==LinkMarker::Point)+(edge.target==LinkMarker::Point);}for(const auto& cell:grid.cells)if(glyph(cell).arrow>=0)++arrows;
    if(arrows!=expected_arrows)return {"LinkProfile3 endpoint marker multiplicity differs"};
    // This closed test profile tries only the finite, observed port pairs. The
    // cap bounds route construction and backtracking without prescribing which
    // producer port a source Model edge must use.
    constexpr std::size_t kRouteAttemptLimit=1024u,kSelectionStepLimit=65536u;
    work.spend(8u*sizeof(std::vector<LinkProfileRoute>)+4u*sizeof(std::vector<bool>)+4u*sizeof(std::vector<int>)+4096u);
    work.product(model.edges.size(),3u*sizeof(std::vector<LinkProfileRoute>)+2u*sizeof(int)+16u);
    work.product(grid.cells.size(),3u*sizeof(bool)+sizeof(int)+16u);work.product(labels.size(),sizeof(bool)+8u);
    // Ports are tried serially; only their successful owned routes survive.
    // Four BFS arrays need one allocation per geometry decision, not one per
    // candidate. This preserves the fixed scene cap and all charged resets.
    work.spend(4u*sizeof(std::vector<int>)+1024u);work.product(grid.cells.size(),6u*sizeof(int)+16u);
    LinkProfileSearch search;search.parents.resize(grid.cells.size());search.bridges.resize(grid.cells.size());
    search.used.resize(grid.cells.size());search.queue.reserve(grid.cells.size());
    std::vector<std::vector<LinkProfileRoute>> candidates(model.edges.size());std::size_t attempts=0u;
    for(std::size_t at=0u;at<model.edges.size();++at){
        const auto& edge=model.edges[at];const auto sources=linkprofile3_endpoints(grid,nodes[static_cast<std::size_t>(edge.from)],true,profile.edges[at].source==LinkMarker::Point,work);
        const auto targets=linkprofile3_endpoints(grid,nodes[static_cast<std::size_t>(edge.to)],false,profile.edges[at].target==LinkMarker::Point,work);
        if(sources.empty()||targets.empty())return {"LinkProfile3 endpoint port/marker differs"};
        if(sources.size()>kRouteAttemptLimit/targets.size())return {"LinkProfile3 endpoint candidate bound differs"};
        const auto pairs=sources.size()*targets.size();if(pairs>kRouteAttemptLimit-attempts)return {"LinkProfile3 route candidate bound differs"};
        work.product(pairs,sizeof(LinkProfileRoute)+4u*sizeof(int)+16u);candidates[at].reserve(pairs);attempts+=pairs;
        for(const auto& source:sources)for(const auto& target:targets){
            auto route=linkprofile3_route(grid,nodes,labels,source,target,work,search);if(!route||!linkprofile3_style_matches(grid,*route,nodes,edge.style))continue;
            if(edge.label.empty()? !route->labels.empty() : route->labels.size()!=1u||labels[static_cast<std::size_t>(route->labels.front())].text!=edge.label)continue;
            candidates[at].push_back(std::move(*route));
        }
        if(candidates[at].empty())return {"LinkProfile3 complete route differs"};
    }
    std::vector<bool> cells(grid.cells.size()),captions(labels.size());std::vector<int> selected(model.edges.size(),-1);std::size_t selection_steps=0u;
    const auto select=[&](auto&& self,std::size_t edge)->bool{if(++selection_steps>kSelectionStepLimit)throw std::runtime_error("LinkProfile3 exclusive assignment bound exhausted");work.spend();if(edge==model.edges.size())return std::ranges::all_of(captions,[](bool used){return used;});for(std::size_t at=0u;at<candidates[edge].size();++at){const auto& route=candidates[edge][at];bool free=true;for(const int cell:route.cells)if(cells[static_cast<std::size_t>(cell)])free=false;for(const int label:route.labels)if(captions[static_cast<std::size_t>(label)])free=false;if(!free)continue;for(const int cell:route.cells)cells[static_cast<std::size_t>(cell)]=true;for(const int label:route.labels)captions[static_cast<std::size_t>(label)]=true;selected[edge]=static_cast<int>(at);if(self(self,edge+1u))return true;for(const int cell:route.cells)cells[static_cast<std::size_t>(cell)]=false;for(const int label:route.labels)captions[static_cast<std::size_t>(label)]=false;}return false;};
    if(!select(select,0u))return {"LinkProfile3 edge-to-route/caption ownership differs"};
    std::vector<bool> claimed(grid.cells.size());std::vector<int> port_normals(grid.cells.size());
    for(std::size_t at=0u;at<model.edges.size();++at){const auto& route=candidates[at][static_cast<std::size_t>(selected[at])];for(const int cell:route.cells)claimed[static_cast<std::size_t>(cell)]=true;if(!route.source.point)port_normals[static_cast<std::size_t>(grid.index(route.source.border))]|=route.source.outward;if(!route.target.point)port_normals[static_cast<std::size_t>(grid.index(route.target.border))]|=route.target.outward;}
    const auto claim_frame=[&](const ShapeFrame& frame){for(int y=frame.top;y<=frame.bottom;++y)for(int x=frame.left;x<=frame.right;++x){work.spend();const Point point{x,y};if(!frame.boundary(point))continue;const int perimeter=frame.perimeter_bits(point),allowed=port_normals[static_cast<std::size_t>(grid.index(point))];if(glyph(grid.at(point)).bits&~perimeter&~allowed)errors.emplace_back("Unclaimed arm on a LinkProfile3 node perimeter");claimed[static_cast<std::size_t>(grid.index(point))]=true;}};
    for(const auto& frame:nodes)claim_frame(frame);for(std::size_t at=0u;at<model.nodes.size();++at){const auto matches=label_blocks(grid,model.nodes[at].label,nodes[at],work);if(matches.size()!=1u)return {"LinkProfile3 entity direct label differs"};claim_label(grid,model.nodes[at].label,matches.front(),claimed);}for(std::size_t at=0u;at<labels.size();++at)claim_label(grid,labels[at].text,labels[at].block,claimed);
    if(!errors.empty())return errors;for(int y=0;y<grid.height;++y){if(following_row?y==*following_row:grid.row(y).find(kSentinel)!=std::string::npos)continue;for(int x=0;x<grid.width;++x){work.spend();const Point point{x,y};const auto value=grid.at(point);if(!claimed[static_cast<std::size_t>(grid.index(point))]&&value!=" "&&!value.empty())return {"Unclaimed text/route glyph reveals an extra/misowned/duplicated fact"};}}
    return {};
}

inline std::vector<std::string> check_assignment(const Grid& grid,const Model& model,const std::vector<ShapeFrame>& frames,
                                        const std::vector<int>& assignment,OracleWork& work,const int* following_row = nullptr,const LinkProfile* link_profile = nullptr,const GroupEndpointProfile* group_endpoint_profile = nullptr) {
    std::vector<std::string> errors;
    const auto node_count = model.nodes.size();
    auto node = [&](int n) -> const ShapeFrame& { return frames.at(static_cast<std::size_t>(assignment.at(static_cast<std::size_t>(n)))); };
    // Group candidates are selected only from Shape::Rectangle below.
    auto group = [&](int n) -> const ShapeFrame& { return frames.at(static_cast<std::size_t>(assignment.at(node_count+static_cast<std::size_t>(n)))); };
    std::vector<int> owners(node_count,-1);
    for (std::size_t g = 0; g < model.groups.size(); ++g)
        for (const int n : model.groups[g].nodes) if (owners[static_cast<std::size_t>(n)] < 0) owners[static_cast<std::size_t>(n)] = static_cast<int>(g);
    for (std::size_t g = 0; g < model.groups.size(); ++g) {
        const int parent = model.groups[g].parent;
        if (parent >= 0 && !group(static_cast<int>(g)).inside(group(parent))) errors.emplace_back("Nested group frame is outside its parent");
        for (std::size_t other = 0; other < g; ++other) {
            auto ancestor = [&](int child,int candidate) {
                for (int at = model.groups[static_cast<std::size_t>(child)].parent; at >= 0; at = model.groups[static_cast<std::size_t>(at)].parent)
                    if (at == candidate) return true;
                return false;
            };
            const auto& a = group(static_cast<int>(g)); const auto& b = group(static_cast<int>(other));
            if (!ancestor(static_cast<int>(g),static_cast<int>(other)) && !ancestor(static_cast<int>(other),static_cast<int>(g)) &&
                !(a.right < b.left || b.right < a.left || a.bottom < b.top || b.bottom < a.top))
                errors.emplace_back("Sibling/disconnected groups overlap");
        }
    }
    for (std::size_t n = 0; n < node_count; ++n) {
        for (std::size_t g = 0; g < model.groups.size(); ++g) {
            bool expected = false;
            for (int at = owners[n]; at >= 0; at = model.groups[static_cast<std::size_t>(at)].parent)
                if (at == static_cast<int>(g)) expected = true;
            if (node(static_cast<int>(n)).inside(group(static_cast<int>(g))) != expected)
                errors.emplace_back("Node/group ownership differs: " + model.nodes[n].id + " / " + model.groups[g].id);
        }
    }
    if (!errors.empty()) return errors;
    const Rect whole{-1,-1,grid.width,grid.height};
    // Physical captions never deduplicate by text: every complete block gets
    // a stable LabelRegion index and semantic multiplicity is exact.
    work.spend(8u*sizeof(LabelRegion)+8u*sizeof(std::size_t)+512u);
    std::map<std::string,std::size_t> caption_count;
    for (const auto& edge:model.edges) if (!edge.label.empty()) ++caption_count[edge.label];
    std::vector<LabelRegion> labels;
    for (const auto& [text,required]:caption_count) {
        auto matches=label_blocks(grid,text,whole,work);
        std::erase_if(matches,[&](const Label& block) {
            for (std::size_t n=0;n<node_count;++n)
                for (const auto point:block.starts) if (node(static_cast<int>(n)).interior(point)) return true;
            return false;
        });
        if (matches.size()!=required) {
            errors.emplace_back("Edge caption physical multiplicity differs: "+text);
            continue;
        }
        for (auto& block:matches) labels.push_back({std::move(block),text});
    }
    if (!errors.empty()) return errors;
    if(group_endpoint_profile) return check_group_endpoint_assignment(grid,model,*group_endpoint_profile,frames,assignment,labels,work,following_row);
    if(link_profile) return check_linkprofile3_assignment(grid,model,*link_profile,frames,assignment,labels,work,following_row);
    auto bits = [&](Point p) {
        if (!grid.has(p)) return 0;
        for (std::size_t n = 0; n < node_count; ++n) {
            const auto& box = node(static_cast<int>(n));
            if (box.contains(p)) return 0;
        }
        int value = glyph(grid.at(p)).bits;
        for (std::size_t g = 0; g < model.groups.size(); ++g) {
            const auto& box = group(static_cast<int>(g));
            if (!box.boundary(p)) continue;
            // A crossing can go THROUGH a group perimeter, never along it.
            if (p.y == box.top || p.y == box.bottom) value &= ~(L|R);
            if (p.x == box.left || p.x == box.right) value &= ~(U|D);
        }
        return value;
    };
    std::vector<Trace> traces;
    for (int y = 0; y < grid.height; ++y) for (int x = 0; x < grid.width; ++x) {
        const Point head{x,y}; const auto arrow = glyph(grid.at(head));
        if (arrow.arrow < 0) continue;
        const auto forward = kSteps[static_cast<std::size_t>(arrow.arrow)];
        const Point target{head.x+forward.x,head.y+forward.y};
        int target_node = -1;
        for (std::size_t n = 0; n < node_count; ++n)
            if (node(static_cast<int>(n)).legal_port(target,kBits[static_cast<std::size_t>((arrow.arrow+2)%4)])) target_node = static_cast<int>(n);
        if (target_node < 0) { errors.emplace_back("Arrow does not enter an actual node frame"); continue; }
        const Point behind{head.x-forward.x,head.y-forward.y};
        if (!(bits(behind) & kBits[static_cast<std::size_t>(arrow.arrow)])) {
            errors.emplace_back("Arrow shaft is disconnected"); continue;
        }
        Trace trace{head,target_node,{}};
        std::vector<int> parents(grid.cells.size(),-2),bridges(grid.cells.size(),-1);
        std::vector<int> queue{grid.index(behind)};
        parents[static_cast<std::size_t>(queue.front())] = -1;
        std::set<int> found_sources;
        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
            work.spend();
            const int current = queue[cursor]; const auto p = grid.point(current);
            for (int d = 0; d < 4; ++d) {
                work.spend();
                const auto delta = kSteps[static_cast<std::size_t>(d)];
                const Point next{p.x+delta.x,p.y+delta.y};
                if (!(bits(p) & kBits[static_cast<std::size_t>(d)])) continue;
                for (std::size_t n = 0; n < node_count; ++n) {
                    if (!node(static_cast<int>(n)).accepts_port(grid,next,kBits[static_cast<std::size_t>((d+2)%4)])) continue;
                    if (!found_sources.insert(static_cast<int>(n)).second) continue;
                    Trace::Source source{static_cast<int>(n),{}, {}};
                    std::vector<int> path;
                    for (int at = current; at >= 0; at = parents[static_cast<std::size_t>(at)]) {
                        work.spend(); path.push_back(at);
                        if (bridges[static_cast<std::size_t>(at)] >= 0) source.labels.push_back(bridges[static_cast<std::size_t>(at)]);
                    }
                    std::ranges::sort(source.labels);
                    source.labels.erase(std::unique(source.labels.begin(),source.labels.end()),source.labels.end());
                    source.path = path; source.border = next;
                    source.outward = kBits[static_cast<std::size_t>((d+2)%4)];
                    auto direction = [](Point from,Point to) {
                        if (from.x == to.x && from.y != to.y) return to.y < from.y ? 0 : 2;
                        if (from.y == to.y && from.x != to.x) return to.x < from.x ? 3 : 1;
                        throw std::runtime_error("Nonorthogonal cell route witness");
                    };
                    for (std::size_t step = 0; step < path.size(); ++step) {
                        work.spend(); const auto point = grid.point(path[step]);
                        const auto before = step ? grid.point(path[step-1]) : next;
                        const auto after = step+1 < path.size() ? grid.point(path[step+1]) : head;
                        const int used = kBits[static_cast<std::size_t>(direction(point,before))] |
                                         kBits[static_cast<std::size_t>(direction(point,after))];
                        const int actual = bits(point);
                        // A junction cannot disguise an ordinary straight
                        // shaft: unused arms are not authored graph facts.
                        if ((actual & used) != used || (actual & ~used)) { source.valid = false; break; }
                        bool perimeter = false;
                        for (std::size_t g = 0; g < model.groups.size(); ++g)
                            if (group(static_cast<int>(g)).boundary(point)) perimeter = true;
                        // Only true turns and known group perimeter crossings
                        // may use a light structural glyph on a styled route.
                        if (!perimeter && (used == (L|R) || used == (U|D)))
                            ++source.straight_strokes[static_cast<std::size_t>(glyph(grid.at(point)).stroke)];
                    }
                    trace.sources.push_back(std::move(source));
                }
                if (!grid.has(next) || next == head || glyph(grid.at(next)).arrow >= 0) continue;
                if (!(bits(next) & kBits[static_cast<std::size_t>((d+2)%4)])) continue;
                const int next_index = grid.index(next);
                if (parents[static_cast<std::size_t>(next_index)] != -2) continue;
                parents[static_cast<std::size_t>(next_index)] = current; queue.push_back(next_index);
            }
            for (std::size_t label = 0; label < labels.size(); ++label) {
                const auto& box = labels[label].block.bounds;
                if (p.y <= box.top || p.y >= box.bottom) continue;
                const bool left = p.x == box.left, right = p.x == box.right;
                if ((!left && !right) || !(bits(p) & (left ? R : L))) continue;
                const Point opposite{left ? box.right : box.left,p.y};
                if (!(bits(opposite) & (left ? L : R))) continue;
                const int next_index = grid.index(opposite);
                if (parents[static_cast<std::size_t>(next_index)] != -2) continue;
                parents[static_cast<std::size_t>(next_index)] = current;
                bridges[static_cast<std::size_t>(next_index)] = static_cast<int>(label); queue.push_back(next_index);
            }
        }
        if (trace.sources.empty()) errors.emplace_back("Arrow has no connected authored source port");
        traces.push_back(std::move(trace));
    }
    if (traces.size() != model.edges.size()) errors.emplace_back("Arrow/semantic edge multiplicity differs");
    if (!errors.empty()) return errors;
    auto matches_edge = [&](const Edge& edge,const Trace& trace,int* witness,int* caption) {
        if (edge.start || !edge.end || edge.to != trace.target) return false;
        for (std::size_t source_index = 0; source_index < trace.sources.size(); ++source_index) {
            const auto& source = trace.sources[source_index];
            if (!source.valid || source.node != edge.from) continue;
            const auto& strokes = source.straight_strokes;
            if ((edge.style == "solid" && (strokes[1] || strokes[2])) ||
                (edge.style == "dotted" && (!strokes[1] || strokes[0] || strokes[2])) ||
                (edge.style == "thick" && (!strokes[2] || strokes[0] || strokes[1]))) continue;
            if (edge.label.empty() && !source.labels.empty()) continue;
            if (!edge.label.empty() && (source.labels.size()!=1u || labels[static_cast<std::size_t>(source.labels.front())].text!=edge.label)) continue;
            if (witness) *witness=static_cast<int>(source_index);
            if (caption) *caption=edge.label.empty()?-1:source.labels.front();
            return true;
        }
        return false;
    };
    work.spend(8u*sizeof(int)+4u*sizeof(bool)+512u);
    std::vector<bool> used(traces.size()),caption_used(labels.size());
    std::vector<int> selected_trace(model.edges.size(),-1),selected_source(model.edges.size(),-1),selected_caption(model.edges.size(),-1);
    auto match_all = [&](auto&& self,std::size_t edge) -> bool {
        work.spend();
        if (edge==model.edges.size()) return std::ranges::all_of(caption_used,[](bool used_caption){return used_caption;});
        for (std::size_t n=0;n<traces.size();++n) {
            work.spend(); int witness=-1,caption=-1;
            if (!used[n] && matches_edge(model.edges[edge],traces[n],&witness,&caption) &&
                (caption<0 || !caption_used[static_cast<std::size_t>(caption)])) {
                selected_trace[edge]=static_cast<int>(n); selected_source[edge]=witness; selected_caption[edge]=caption;
                used[n]=true; if(caption>=0)caption_used[static_cast<std::size_t>(caption)]=true;
                if (self(self,edge+1)) return true;
                if(caption>=0)caption_used[static_cast<std::size_t>(caption)]=false; used[n]=false;
            }
        }
        return false;
    };
    if (!match_all(match_all,0)) return {"Glyph connectivity/endpoint/stroke/per-edge-caption facts do not match every edge"};
    for (const auto& trace : traces) for (const auto& source : trace.sources) {
        if (std::ranges::none_of(model.edges,[&](const Edge& e) { return e.from == source.node && e.to == trace.target; }))
            errors.emplace_back("A route junction introduces an un-authored source/target connection");
    }
    // Direction is a rank property, not an assumption that every cycle arrow
    // points forward. Group-local directions apply only inside that group.
    std::vector<std::vector<bool>> reachable(node_count,std::vector<bool>(node_count));
    for (const auto& e : model.edges) reachable[static_cast<std::size_t>(e.from)][static_cast<std::size_t>(e.to)] = true;
    for (std::size_t k = 0; k < node_count; ++k) for (std::size_t i = 0; i < node_count; ++i) for (std::size_t j = 0; j < node_count; ++j)
        reachable[i][j] = reachable[i][j] || (reachable[i][k] && reachable[k][j]);
    for (const auto& edge : model.edges) {
        if (reachable[static_cast<std::size_t>(edge.to)][static_cast<std::size_t>(edge.from)]) continue;
        auto from = node(edge.from),to = node(edge.to); auto direction = model.direction;
        const int own_from = owners[static_cast<std::size_t>(edge.from)],own_to = owners[static_cast<std::size_t>(edge.to)];
        std::set<int> target_ancestors;
        for (int at = own_to; at >= 0; at = model.groups[static_cast<std::size_t>(at)].parent)
            target_ancestors.insert(at);
        int common = -1;
        for (int at = own_from; at >= 0; at = model.groups[static_cast<std::size_t>(at)].parent)
            if (target_ancestors.contains(at)) { common = at; break; }
        for (int at = common; at >= 0; at = model.groups[static_cast<std::size_t>(at)].parent) {
            if (!model.groups[static_cast<std::size_t>(at)].direction.empty()) {
                direction = model.groups[static_cast<std::size_t>(at)].direction; break;
            }
        }
        auto frontier = [&](int owner) {
            if (owner == common) return -1;
            while (owner >= 0 && model.groups[static_cast<std::size_t>(owner)].parent != common)
                owner = model.groups[static_cast<std::size_t>(owner)].parent;
            return owner;
        };
        const int a = frontier(own_from),b = frontier(own_to);
        if (a >= 0) from = group(a);
        if (b >= 0) to = group(b);
        if (from == to) continue;
        const int dx = to.left+to.right-from.left-from.right,dy = to.top+to.bottom-from.top-from.bottom;
        if ((direction == "LR" && dx <= 0) || (direction == "RL" && dx >= 0) ||
            ((direction == "TD" || direction == "TB") && dy <= 0) || (direction == "BT" && dy >= 0))
            errors.emplace_back("Acyclic rank ordering contradicts the declared direction");
    }
    // Every authored ordinary text cell must belong to exactly one declared
    // entity/edge label block. Incidental closed rectangles formed by routes
    // are harmless; an extra labelled leaf node or header is not.
    std::vector<bool> claimed(grid.cells.size());
    std::vector<int> port_normals(grid.cells.size());
    for (std::size_t edge = 0; edge < model.edges.size(); ++edge) {
        const auto& trace = traces[static_cast<std::size_t>(selected_trace[edge])];
        const auto& source = trace.sources[static_cast<std::size_t>(selected_source[edge])];
        for (const int cell : source.path) { work.spend(); claimed[static_cast<std::size_t>(cell)] = true; }
        claimed[static_cast<std::size_t>(grid.index(trace.head))] = true;
        port_normals[static_cast<std::size_t>(grid.index(source.border))] |= source.outward;
        const auto arrow = glyph(grid.at(trace.head)); const auto delta = kSteps[static_cast<std::size_t>(arrow.arrow)];
        const Point target{trace.head.x+delta.x,trace.head.y+delta.y};
        port_normals[static_cast<std::size_t>(grid.index(target))] |= kBits[static_cast<std::size_t>((arrow.arrow+2)%4)];
    }
    auto claim_frame = [&](const ShapeFrame& frame,bool is_group) {
        for (int y=frame.top;y<=frame.bottom;++y) for (int x=frame.left;x<=frame.right;++x) {
            work.spend(); const Point point{x,y};
            if (!frame.boundary(point) && !classic_decoration(frame,point)) continue;
            const int perimeter=frame.perimeter_bits(point);
            int allowed=port_normals[static_cast<std::size_t>(grid.index(point))];
            if (is_group && claimed[static_cast<std::size_t>(grid.index(point))]) allowed|=glyph(grid.at(point)).bits&~perimeter;
            if (glyph(grid.at(point)).bits&~perimeter&~allowed) errors.emplace_back("Unclaimed arm on a node/group perimeter");
            claimed[static_cast<std::size_t>(grid.index(point))]=true;
        }
    };
    for (std::size_t n = 0; n < node_count; ++n) claim_frame(node(static_cast<int>(n)),false);
    for (std::size_t g = 0; g < model.groups.size(); ++g) claim_frame(group(static_cast<int>(g)),true);
    auto claim_entity = [&](std::string_view text,const ShapeFrame& frame) {
        const auto matches = label_blocks(grid,text,frame,work);
        const auto direct = std::ranges::find_if(matches,[&](const Label& block) {
            const auto lines=split_lines(text);
            for (std::size_t row=0;row<lines.size();++row) {
                const auto cells=ftxui::Utf8ToGlyphs(lines[row]);
                for(std::size_t column=0;column<cells.size();++column) {
                    work.spend();
                    if(!frame.interior({block.starts[row].x+static_cast<int>(column),block.starts[row].y})) return false;
                }
            }
            for (const auto& other : frames) {
                if (classic_decorative_frame(other,frame)) continue;
                if (!other.inside(frame)) continue;
                for (const auto p : block.starts) if (other.interior(p)) return false;
            }
            return true;
        });
        if (direct == matches.end()) errors.emplace_back("Entity lacks its direct label block");
        else claim_label(grid,text,*direct,claimed);
    };
    for (std::size_t n = 0; n < node_count; ++n) claim_entity(model.nodes[n].label,node(static_cast<int>(n)));
    for (std::size_t g = 0; g < model.groups.size(); ++g) claim_entity(model.groups[g].label,group(static_cast<int>(g)));
    for (const auto selected:selected_caption) if (selected>=0)
        claim_label(grid,labels[static_cast<std::size_t>(selected)].text,labels[static_cast<std::size_t>(selected)].block,claimed);
    for (int y = 0; y < grid.height; ++y) {
        if (following_row) { if (y == *following_row) continue; }
        else if (grid.row(y).find(kSentinel) != std::string::npos) continue;
        for (int x = 0; x < grid.width; ++x) {
            work.spend(); const Point p{x,y}; const auto value = grid.at(p);
            if (!claimed[static_cast<std::size_t>(grid.index(p))] && value != " " && !value.empty()) {
                errors.emplace_back("Unclaimed text/route glyph reveals an extra/misowned/duplicated fact");
                return errors;
            }
        }
    }
    return errors;
}
inline std::vector<std::string> validate_impl(const Grid& grid,const Model& model,OracleWork& work,
                                       std::vector<std::string>* assignment_diagnostics,const int* following_row = nullptr,const LinkProfile* link_profile = nullptr,const GroupEndpointProfile* group_endpoint_profile = nullptr) {
    if (grid.width <= 0 || grid.width > 240 || grid.height <= 0 || grid.height > 1040 ||
        grid.cells.size() != static_cast<std::size_t>(grid.width*grid.height) || model.nodes.size() > 64 ||
        model.edges.size() > 128 || model.groups.size() > 64) return {"Cell oracle bounds exceeded"};
    for (const auto& edge : model.edges)
        if (edge.from < 0 || edge.to < 0 || static_cast<std::size_t>(edge.from) >= model.nodes.size() ||
            static_cast<std::size_t>(edge.to) >= model.nodes.size()) return {"Invalid semantic endpoint"};
    for (std::size_t n = 0; n < model.groups.size(); ++n) {
        std::set<int> seen;
        for (int at = static_cast<int>(n); at >= 0; at = model.groups[static_cast<std::size_t>(at)].parent) {
            if (static_cast<std::size_t>(at) >= model.groups.size() || !seen.insert(at).second)
                return {"Invalid or cyclic semantic group parent"};
        }
        for (const int member : model.groups[n].nodes)
            if (member < 0 || static_cast<std::size_t>(member) >= model.nodes.size()) return {"Invalid semantic group member"};
    }
    // Preserve the legacy arrowhead scan and its one-sided diagnostic byte-for-byte.
    // Closed profiles alone declare a different exact arrowhead cardinality.
    if (group_endpoint_profile) {
        if (link_profile || model.edges.size()+group_endpoint_profile->tagged.size()>128u)
            return {"GroupEndpoint profile cardinality bound"};
        std::size_t heads = 0u;
        const auto expected = model.edges.size()+group_endpoint_profile->tagged.size();
        for (const auto& cell : grid.cells) if (glyph(cell).arrow >= 0 && ++heads > expected)
            return {"Extra arrowheads exceed the semantic edge count"};
        if(heads!=expected)return {"GroupEndpoint endpoint marker multiplicity differs"};
    } else if (!link_profile) {
        std::size_t heads = 0;
        for (const auto& cell : grid.cells) if (glyph(cell).arrow >= 0 && ++heads > model.edges.size())
            return {"Extra arrowheads exceed the semantic edge count"};
    } else {
        if(!linkprofile3_profile_shape(model,*link_profile)) return {"LinkProfile3 closed Model/profile contract differs"};
        std::size_t heads = 0u,expected_heads = 0u;
        for(const auto& edge:link_profile->edges)
            expected_heads+=(edge.source==LinkMarker::Point)+(edge.target==LinkMarker::Point);
        for (const auto& cell : grid.cells) if (glyph(cell).arrow >= 0 && ++heads > expected_heads)
            return {"Extra arrowheads exceed the semantic edge count"};
        if(heads!=expected_heads)return {"LinkProfile3 endpoint marker multiplicity differs"};
    }
    if (model.nodes.empty() && model.groups.empty()) return {"Empty semantic model"};
    auto frames = closed_frames(grid,work);
    // The existing detector and its paid work remain byte-for-byte on Models
    // without a classic token. Only the closed test-private vocabulary pays
    // the separate bounded profile search.
    if (std::ranges::any_of(model.nodes,[](const Node& node) { return classic_shape(node.shape); })) {
        auto classic=classic_closed_frames(grid,work);
        work.spend(4u*sizeof(ShapeFrame)+256u);
        if (frames.size()+classic.size()>128u) throw std::runtime_error("Too many typed Flow contour candidates");
        frames.insert(frames.end(),classic.begin(),classic.end());
        std::ranges::sort(frames,[](const ShapeFrame& a,const ShapeFrame& b) { return a.area()<b.area(); });
    }
    work.spend(8u*sizeof(std::vector<int>)+8u*sizeof(int)+512u);
    std::vector<std::vector<int>> candidates;
    for (const auto& node : model.nodes) {
        auto options=frame_candidates(grid,frames,node.label,node.shape,work);
        std::erase_if(options,[&](int index) { return !complete_node_content(grid,frames[static_cast<std::size_t>(index)],node.label,work); });
        candidates.push_back(std::move(options));
    }
    for (const auto& group:model.groups) candidates.push_back(frame_candidates(grid,frames,group.label,Shape::Rectangle,work));
    for (std::size_t n=0;n<candidates.size();++n) if(candidates[n].empty())
        return {"Missing exact typed contour with complete direct multiline label for entity "+std::to_string(n)};
    std::map<std::pair<Shape,std::string>,std::size_t> multiplicity;
    for (const auto& node:model.nodes) ++multiplicity[{node.shape,node.label}];
    for (std::size_t n=0;n<model.nodes.size();++n)
        if(candidates[n].size()!=multiplicity[{model.nodes[n].shape,model.nodes[n].label}]) return {"Duplicate-(shape,label) physical node multiplicity differs"};
    std::vector<int> assignment(candidates.size(),-1); std::vector<bool> used(frames.size());
    std::vector<std::string> last{"No one-to-one identity/topology assignment"};
    std::size_t attempts = 0;
    auto search = [&](auto&& self,std::size_t at) -> bool {
        if (++attempts > 10000) { last = {"Bounded identity matching exhausted"}; return false; }
        if (at == candidates.size()) {
            auto errors = check_assignment(grid,model,frames,assignment,work,following_row,link_profile,group_endpoint_profile);
            if (assignment_diagnostics && assignment_diagnostics->size() < 32) {
                std::string detail;
                for (std::size_t n = 0; n < assignment.size(); ++n) {
                    const auto& f = frames[static_cast<std::size_t>(assignment[n])];
                    const auto& id = n < model.nodes.size() ? model.nodes[n].id : model.groups[n-model.nodes.size()].id;
                    detail += id + "=(" + std::to_string(f.left) + "," + std::to_string(f.top) + "," +
                              std::to_string(f.right) + "," + std::to_string(f.bottom) + ") ";
                }
                detail += "errors:";
                for (const auto& error : errors) detail += " " + error + ";";
                assignment_diagnostics->push_back(std::move(detail));
            }
            if (errors.empty()) { last.clear(); return true; }
            if (last.front() == "No one-to-one identity/topology assignment" || errors.size() < last.size()) last = std::move(errors);
            return false;
        }
        for (const int candidate : candidates[at]) if (!used[static_cast<std::size_t>(candidate)]) {
            assignment[at] = candidate; used[static_cast<std::size_t>(candidate)] = true;
            if (self(self,at+1)) return true;
            used[static_cast<std::size_t>(candidate)] = false;
        }
        return false;
    };
    if (search(search,0)) return {};
    return last;
}
inline std::vector<std::string> validate(const Grid& grid,const Model& model,
                                  std::vector<std::string>* assignment_diagnostics = nullptr) {
    try { OracleWork work; return validate_impl(grid,model,work,assignment_diagnostics); }
    catch (const std::exception& error) { return {error.what()}; }
}
inline std::string errors_text(const std::vector<std::string>& errors) {
    std::string result; for (const auto& error : errors) result += error + "; "; return result;
}


inline std::string digest(std::string_view text) { return loom::utils::crypto::sha256(text); }

inline void flow400_require(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}

inline bool flow400_same(const Grid& a,const Grid& b,OracleWork& work) {
    work.spend(4u*sizeof(Grid));
    if (a.width!=b.width || a.height!=b.height || a.cells.size()!=b.cells.size()) return false;
    work.product(a.cells.size(),2u*sizeof(std::string)+8u);
    bool same=true;
    for (std::size_t i=0;i<a.cells.size();++i) {
        work.product(a.cells[i].size(),2u);work.product(b.cells[i].size(),2u);
        if (a.cells[i]!=b.cells[i])same=false;
    } return same;
}
inline int flow400_following(const Grid& grid,OracleWork& work,std::string_view token=kSentinel) {
    work.spend(16u*sizeof(std::size_t)+256u);
    // Existing fixed metadata tariff covers bounded view/guard scalars.
    // Default Native calls retain identical charges and token identity.
    flow400_require(!token.empty() && token.size()<=240u,"Flow400 protocol following token bound");
    if(token.data()!=kSentinel.data() || token.size()!=kSentinel.size()){
        work.product(token.size(),4u);
        for(const unsigned char byte:token)
            flow400_require(byte>=0x21u && byte<=0x7eu,"Flow400 protocol following token ASCII");
    }
    flow400_require(grid.width>0 && grid.width<=240 && grid.height>0 && grid.height<=1040 &&
        grid.cells.size()==static_cast<std::size_t>(grid.width*grid.height),"Flow400 protocol Grid dimensions");
    work.product(grid.cells.size(),2u*sizeof(std::string)+12u);
    int row=-1,last_body=-1;std::size_t found=0;
    for (int y=0;y<grid.height;++y) {
        bool exact=static_cast<std::size_t>(grid.width)>=token.size(),ink=false;
        for (int x=0;x<grid.width;++x) {
            const auto cell=grid.at({x,y});work.product(cell.size(),2u);
            if (!cell.empty() && cell!=" ")ink=true;
            const auto expected=static_cast<std::size_t>(x)<token.size()?token.substr(static_cast<std::size_t>(x),1u):std::string_view{" "};
            if (cell!=expected)exact=false;
        }
        if (exact) {row=y;++found;} else if(ink)last_body=y;
    }
    flow400_require(found==1u && row>last_body,"Flow400 protocol exact independent following row");return row;
}
inline std::vector<std::string> flow400_geometry(const Grid& grid,const Model& model,OracleWork& work,std::string_view token=kSentinel) {
    const int following=flow400_following(grid,work,token);return validate_impl(grid,model,work,nullptr,&following);
}
inline std::vector<std::string> flow_linkprofile3_geometry(const Grid& grid,const Model& model,const LinkProfile& profile,OracleWork& work,std::string_view token=kSentinel) {
    const int following=flow400_following(grid,work,token);return validate_impl(grid,model,work,nullptr,&following,&profile);
}
inline std::vector<std::string> flow_group_endpoint_geometry(const Grid& grid,const Model& model,const GroupEndpointProfile& profile,OracleWork& work,std::string_view token=kSentinel) {
    const int following=flow400_following(grid,work,token);return validate_impl(grid,model,work,nullptr,&following,nullptr,&profile);
}
inline bool flow400_resource(std::string_view message) {
    return message.find("work exhausted")!=std::string_view::npos || message.find("resource")!=std::string_view::npos ||
        message.find("bounds exceeded")!=std::string_view::npos || message.find("Too many candidate")!=std::string_view::npos ||
        message.find("matching exhausted")!=std::string_view::npos || message.find("bad_alloc")!=std::string_view::npos;
}
inline void flow400_copy_model(const Model& model,OracleWork& work) {
    work.spend(4u*sizeof(Model)+256u);
    work.product(model.nodes.size(),3u*sizeof(Node)+8u);work.product(model.edges.size(),3u*sizeof(Edge)+8u);work.product(model.groups.size(),3u*sizeof(Group)+8u);
    auto text=[&](const std::string& s){work.spend(8u);work.product(s.size(),4u);};text(model.direction);
    for(const auto& n:model.nodes){text(n.id);text(n.label);}
    for(const auto& e:model.edges){text(e.label);text(e.style);}
    for(const auto& g:model.groups){text(g.id);text(g.label);text(g.direction);work.product(g.nodes.size(),3u*sizeof(int)+8u);}
}
inline void flow400_copy_grid(const Grid& grid,OracleWork& work) {
    work.spend(4u*sizeof(Grid)+256u);work.product(grid.cells.size(),3u*sizeof(std::string)+8u);
    for(const auto& cell:grid.cells) {work.spend(8u);work.product(cell.size(),4u);}
}
inline std::size_t flow400_mutations(const Grid& good,const Model& model,OracleWork& work,std::string_view token=kSentinel) {
    const auto baseline=flow400_geometry(good,model,work,token);flow400_require(baseline.empty(),"Flow400 mutation requires complete good baseline");
    std::size_t rejected=0;
    auto reject=[&](const Model& wrong) {
        const auto errors=flow400_geometry(good,wrong,work,token);
        flow400_require(!errors.empty(),"Flow400 semantic Model mutant accepted");
        for(const auto& error:errors)flow400_require(!flow400_resource(error),"Flow400 resource is not semantic-negative credit");++rejected;
    };
    flow400_copy_model(model,work);auto word=model;work.spend(1024u);word.nodes.front().label+="__absent_flow400";reject(word);
    if(!model.edges.empty() && model.edges.front().from!=model.edges.front().to){
        flow400_copy_model(model,work);auto target=model;target.edges.front().to=target.edges.front().from;reject(target);
    }
    const auto owner=std::ranges::find_if(model.groups,[](const Group& g){return !g.nodes.empty();});
    if(owner!=model.groups.end()){
        flow400_copy_model(model,work);auto ownership=model;const auto at=static_cast<std::size_t>(owner-model.groups.begin());
        work.product(ownership.groups[at].nodes.size(),2u*sizeof(int)+4u);
        ownership.groups[at].nodes.erase(ownership.groups[at].nodes.begin());reject(ownership);
    }
    flow400_copy_grid(good,work);auto ink=good;const auto footer=flow400_following(good,work,token);bool placed=false;
    for(int y=0;!placed && y<footer;++y)for(int x=0;!placed && x<good.width;++x){
        work.spend();if(good.at({x,y})==" "){work.spend(32u);ink.put({x,y},"§");placed=true;}
    }
    flow400_require(placed,"Flow400 no physical extra-ink witness");
    const auto errors=flow400_geometry(ink,model,work,token);flow400_require(!errors.empty(),"Flow400 extra physical ink accepted");
    for(const auto& error:errors)flow400_require(!flow400_resource(error),"Flow400 extra-ink resource is not negative credit");return rejected+1u;
}

} // namespace loom::test::mermaid_flow

// The classic profile is an included extension of this existing typed oracle.
#include "mermaid_flow_classic_shape_profile.hpp"

// Optional group-endpoint profile. It is deliberately below the existing profile
// extension so its private route checker can reuse the ordinary frame assignment,
// direct-label discovery, group containment, and final following-row protocol.
namespace loom::test::mermaid_flow {
inline int group_endpoint_direction(Point from,Point to) {
    if(from.x==to.x && from.y!=to.y) return to.y<from.y?0:2;
    if(from.y==to.y && from.x!=to.x) return to.x<from.x?3:1;
    throw std::runtime_error("GroupEndpoint nonorthogonal route");
}
inline std::vector<std::string> check_group_endpoint_assignment(
    const Grid& grid,const Model& model,const GroupEndpointProfile& profile,
    const std::vector<ShapeFrame>& frames,const std::vector<int>& assignment,
    const std::vector<LabelRegion>& labels,OracleWork& work,const int* following_row) {
    try {
        const auto node_count=model.nodes.size(),group_count=model.groups.size(),entity_count=node_count+group_count;
        if(node_count>64u || entity_count>128u || model.edges.size()+profile.tagged.size()>128u)
            return {"GroupEndpoint profile cardinality bound"};
        if(!labels.empty()) return {"GroupEndpoint closed profile has unexpected captions"};
        work.spend(24u*sizeof(std::vector<int>)+8192u);
        work.product(grid.cells.size(),8u*sizeof(int)+4u*sizeof(bool)+32u);
        work.product(entity_count,4u*sizeof(ShapeFrame)+4u*sizeof(int)+32u);
        std::vector<ShapeFrame> entities;entities.reserve(entity_count);
        for(const int at:assignment) {work.spend();entities.push_back(frames.at(static_cast<std::size_t>(at)));}
        auto entity_index=[&](GroupEndpointRef ref)->std::size_t {
            const auto limit=ref.kind==GroupEndpointKind::Vertex?node_count:group_count;
            if(ref.index>=limit) throw std::runtime_error("GroupEndpoint endpoint index bound");
            return ref.kind==GroupEndpointKind::Vertex?ref.index:node_count+ref.index;
        };
        auto entity=[&](GroupEndpointRef ref)->const ShapeFrame& {return entities.at(entity_index(ref));};

        // Existing Model parsing checked group references. Retain the selected-frame
        // ownership relation before interpreting any tagged endpoint route.
        work.product(node_count,sizeof(int)+16u);
        std::vector<int> owners(node_count,-1);
        for(std::size_t group=0u;group<group_count;++group) for(const int member:model.groups[group].nodes) {
            work.spend();if(member<0 || static_cast<std::size_t>(member)>=node_count) return {"GroupEndpoint group membership differs"};
            if(owners[static_cast<std::size_t>(member)]<0) owners[static_cast<std::size_t>(member)]=static_cast<int>(group);
        }
        for(std::size_t group=0u;group<group_count;++group) for(std::size_t node=0u;node<node_count;++node) {
            bool expected=false;
            for(int at=owners[node];at>=0;at=model.groups[static_cast<std::size_t>(at)].parent) {
                work.spend();
                if(at==static_cast<int>(group)) expected=true;
            }
            if(entities[node].inside(entities[node_count+group])!=expected)
                return {"GroupEndpoint node/group ownership differs"};
        }

        struct Resolved { GroupEndpointRef source,target; std::string_view label,style; bool start=false,end=true; };
        const auto total=model.edges.size()+profile.tagged.size();
        if(profile.source_order.size()!=total) return {"GroupEndpoint authored source edge order differs"};
        work.product(total,2u*sizeof(Resolved)+sizeof(bool)+32u);
        std::vector<bool> ordinary_seen(model.edges.size()),tagged_seen(profile.tagged.size());
        std::vector<Resolved> edges;edges.reserve(total);
        for(const auto entry:profile.source_order) {
            work.spend();if(entry.kind==GroupEndpointOrderKind::Ordinary) {
                if(entry.index>=model.edges.size() || ordinary_seen[entry.index]) return {"GroupEndpoint authored source edge order differs"};
                ordinary_seen[entry.index]=true;const auto& edge=model.edges[entry.index];
                edges.push_back({{GroupEndpointKind::Vertex,static_cast<std::size_t>(edge.from)},
                    {GroupEndpointKind::Vertex,static_cast<std::size_t>(edge.to)},edge.label,edge.style,edge.start,edge.end});
            } else {
                if(entry.index>=profile.tagged.size() || tagged_seen[entry.index]) return {"GroupEndpoint authored source edge order differs"};
                tagged_seen[entry.index]=true;const auto& edge=profile.tagged[entry.index];
                static_cast<void>(entity_index(edge.source));static_cast<void>(entity_index(edge.target));
                edges.push_back({edge.source,edge.target,edge.label,edge.style,edge.arrow_start,edge.arrow_end});
            }
        }
        if(!std::ranges::all_of(ordinary_seen,[](bool value){return value;}) ||
           !std::ranges::all_of(tagged_seen,[](bool value){return value;})) return {"GroupEndpoint authored source edge order differs"};
        work.product(total,4u*sizeof(std::size_t)+64u);
        std::set<std::size_t> targets;
        for(const auto& edge:edges) {
            work.spend();
            if(!edge.label.empty() || edge.style!="solid" || edge.start || !edge.end)
                return {"GroupEndpoint closed edge semantics differs"};
            if(!targets.insert(entity_index(edge.target)).second)
                return {"GroupEndpoint closed target multiplicity differs"};
        }

        auto frame_perimeter=[&](Point point) {
            work.product(entity_count,1u);int result=0;
            for(const auto& frame:entities) if(frame.boundary(point)) result|=frame.perimeter_bits(point);
            return result;
        };
        auto route_bits=[&](Point point) {
            if(!grid.has(point)) return 0;
            work.product(entity_count,1u);
            for(std::size_t node=0u;node<node_count;++node) if(entities[node].contains(point)) return 0;
            int result=glyph(grid.at(point)).bits;
            for(std::size_t group=0u;group<group_count;++group) {
                const auto& frame=entities[node_count+group]; if(!frame.boundary(point)) continue;
                if(point.y==frame.top || point.y==frame.bottom) result&=~(L|R);
                if(point.x==frame.left || point.x==frame.right) result&=~(U|D);
            }
            return result;
        };
        struct Route { std::vector<int> cells,arms; int head=-1,source=-1,target=-1; int source_outward=0; };
        constexpr std::size_t kCandidatesPerEdge=16u,kCandidateTotal=512u,kSelectionSteps=65536u,kRouteCells=2048u;
        work.product(total,2u*sizeof(std::vector<Route>)+2u*sizeof(int)+sizeof(Point)+32u);
        work.product(total*kCandidatesPerEdge,sizeof(Route)+32u);
        work.product(grid.cells.size(),4u*sizeof(int)+32u);
        std::vector<Point> heads;heads.reserve(total);
        for(int y=0;y<grid.height;++y) for(int x=0;x<grid.width;++x) {
            work.spend(); if(glyph(grid.at({x,y})).arrow>=0) heads.push_back({x,y});
        }
        if(heads.size()!=total) return {"GroupEndpoint endpoint marker multiplicity differs"};
        std::vector<int> parents(grid.cells.size(),-2),queue;queue.reserve(grid.cells.size());
        std::vector<std::vector<Route>> candidates(total);for(auto& list:candidates) list.reserve(kCandidatesPerEdge);
        std::size_t candidate_total=0u;
        auto route_from=[&](const ShapeFrame& source,const ShapeFrame& target,Point head,std::vector<Route>& result)->std::size_t {
            const auto arrow=glyph(grid.at(head));const int forward=kBits[static_cast<std::size_t>(arrow.arrow)];
            const int backward=kBits[static_cast<std::size_t>((arrow.arrow+2)%4)];
            const auto target_delta=kSteps[static_cast<std::size_t>(arrow.arrow)];
            const Point target_port{head.x+target_delta.x,head.y+target_delta.y};
            if(!grid.has(target_port) || !target.legal_port(target_port,backward) ||
               glyph(grid.at(target_port)).bits!=target.perimeter_bits(target_port)) return 0u;
            const auto begin_delta=kSteps[static_cast<std::size_t>((arrow.arrow+2)%4)];
            const Point begin{head.x+begin_delta.x,head.y+begin_delta.y};
            if(!grid.has(begin) || !(route_bits(begin)&forward)) return 0u;
            const auto prior=result.size();work.product(grid.cells.size(),3u);std::ranges::fill(parents,-2);queue.clear();
            const int root=grid.index(begin);parents[static_cast<std::size_t>(root)]=-1;queue.push_back(root);
            for(std::size_t cursor=0u;cursor<queue.size();++cursor) {
                work.spend();const int current=queue[cursor];const auto point=grid.point(current);const int current_bits=route_bits(point);
                for(std::size_t direction=0u;direction<4u;++direction) {
                    work.spend();const int arm=kBits[direction];if(!(current_bits&arm)) continue;
                    const auto next_delta=kSteps[direction];const Point next{point.x+next_delta.x,point.y+next_delta.y};if(!grid.has(next)) continue;
                    const int outward=kBits[(direction+2u)%4u];
                    if(source.accepts_port(grid,next,outward)) {
                        std::size_t path_size=0u;for(int at=current;at>=0;at=parents[static_cast<std::size_t>(at)]) {work.spend();++path_size;}
                        if(path_size+3u>kRouteCells || result.size()==kCandidatesPerEdge) continue;
                        work.product(path_size+3u,3u*sizeof(int)+32u);
                        Route route;route.cells.reserve(path_size+3u);route.arms.reserve(path_size+3u);
                        route.source=grid.index(next);route.head=grid.index(head);route.target=grid.index(target_port);route.source_outward=outward;
                        route.cells.push_back(route.source);
                        for(int at=current;at>=0;at=parents[static_cast<std::size_t>(at)]) route.cells.push_back(at);
                        route.cells.push_back(route.head);route.cells.push_back(route.target);
                        bool unique=true;for(std::size_t a=0u;a<route.cells.size();++a) for(std::size_t b=0u;b<a;++b) if(route.cells[a]==route.cells[b]) unique=false;
                        if(!unique) continue;
                        route.arms.assign(route.cells.size(),0);
                        for(std::size_t step=0u;step+1u<route.cells.size();++step) {
                            const int direction_to_next=group_endpoint_direction(grid.point(route.cells[step]),grid.point(route.cells[step+1u]));
                            route.arms[step]|=kBits[static_cast<std::size_t>(direction_to_next)];
                            route.arms[step+1u]|=kBits[static_cast<std::size_t>((direction_to_next+2)%4)];
                        }
                        bool exact=true;
                        for(std::size_t at=0u;at<route.cells.size()&&exact;++at) {
                            const Point cell=grid.point(route.cells[at]);const int actual=glyph(grid.at(cell)).bits;
                            if(route.cells[at]==route.head) {
                                exact=glyph(grid.at(cell)).arrow==arrow.arrow && actual==backward;
                            } else if(route.cells[at]==route.source) {
                                exact=(actual&~frame_perimeter(cell))==route.source_outward;
                            } else if(route.cells[at]==route.target) {
                                exact=actual==frame_perimeter(cell);
                            } else if(frame_perimeter(cell)) {
                                exact=(actual&~frame_perimeter(cell))==(route.arms[at]&~frame_perimeter(cell));
                            } else exact=actual==route.arms[at];
                        }
                        if(exact) result.push_back(std::move(route));
                        continue;
                    }
                    if(next==head || glyph(grid.at(next)).arrow>=0 || !(route_bits(next)&outward)) continue;
                    const auto at=static_cast<std::size_t>(grid.index(next));if(parents[at]!=-2) continue;
                    parents[at]=current;queue.push_back(static_cast<int>(at));
                }
            }
            return result.size()-prior;
        };
        for(std::size_t at=0u;at<edges.size();++at) {
            const auto& source=entity(edges[at].source);const auto& target=entity(edges[at].target);
            for(const auto head:heads) {
                work.spend();const auto found=route_from(source,target,head,candidates[at]);
                if(candidate_total+found>kCandidateTotal) return {"GroupEndpoint route candidate bound"};
                candidate_total+=found;
            }
            if(candidates[at].empty()) return {"GroupEndpoint complete route differs"};
        }
        work.product(grid.cells.size(),3u*sizeof(bool)+sizeof(int)+32u);work.product(total,sizeof(int)+16u);
        std::vector<bool> occupied(grid.cells.size()),claimed(grid.cells.size());std::vector<int> route_arms(grid.cells.size()),chosen(total,-1);
        std::size_t selection_steps=0u;
        const auto choose=[&](auto&& self,std::size_t edge)->bool {
            if(++selection_steps>kSelectionSteps) throw std::runtime_error("GroupEndpoint exclusive route ownership exhausted");
            work.spend(); if(edge==total) return true;
            for(std::size_t at=0u;at<candidates[edge].size();++at) {
                const auto& route=candidates[edge][at];bool free=true;
                work.product(route.cells.size(),3u);
                for(const int cell:route.cells) if(occupied[static_cast<std::size_t>(cell)]) free=false;
                if(!free) continue;
                for(const int cell:route.cells) occupied[static_cast<std::size_t>(cell)]=true;
                chosen[edge]=static_cast<int>(at);if(self(self,edge+1u)) return true;
                for(const int cell:route.cells) occupied[static_cast<std::size_t>(cell)]=false;
            }
            return false;
        };
        if(!choose(choose,0u)) return {"GroupEndpoint exclusive route ownership differs"};
        for(std::size_t edge=0u;edge<total;++edge) {
            const auto& route=candidates[edge][static_cast<std::size_t>(chosen[edge])];
            for(std::size_t at=0u;at<route.cells.size();++at) {
                const auto cell=static_cast<std::size_t>(route.cells[at]);claimed[cell]=true;route_arms[cell]|=route.arms[at];
            }
        }

        // Direction uses the existing direct vertex owner and Group.parent as
        // the tagged group's owner. It deliberately does not add child-to-group
        // edges to semantic reachability. Acyclic semantic edges may still form
        // a cycle after those endpoints are projected to their real frontiers;
        // rank order is undefined only for that profile-local quotient cycle.
        struct FrontierRankEdge { std::size_t from=0u,to=0u; int common_owner=-1; std::string_view direction; };
        work.product(entity_count,sizeof(std::vector<bool>)+entity_count*sizeof(bool)+16u);
        work.product(group_count+1u,sizeof(std::vector<std::vector<bool>>)+16u);
        work.product((group_count+1u)*entity_count,sizeof(std::vector<bool>)+entity_count*sizeof(bool)+16u);
        work.product((group_count+1u)*entity_count*entity_count,sizeof(bool)+8u);
        work.product(total,2u*sizeof(FrontierRankEdge)+32u);
        work.product(total*group_count,4u*sizeof(int)+64u);
        std::vector<std::vector<bool>> semantic_reachable(entity_count,std::vector<bool>(entity_count));
        for(const auto& edge:edges) {work.spend();semantic_reachable[entity_index(edge.source)][entity_index(edge.target)]=true;}
        for(std::size_t k=0u;k<entity_count;++k) for(std::size_t i=0u;i<entity_count;++i) for(std::size_t j=0u;j<entity_count;++j) {
            work.spend();semantic_reachable[i][j]=semantic_reachable[i][j] || (semantic_reachable[i][k]&&semantic_reachable[k][j]);
        }
        const auto owner_of=[&](GroupEndpointRef endpoint) {return endpoint.kind==GroupEndpointKind::Vertex?owners[endpoint.index]:model.groups[endpoint.index].parent;};
        const auto common_owner=[&](int source_owner,int target_owner) {
            std::set<int> target_ancestors;
            for(int at=target_owner;at>=0;at=model.groups[static_cast<std::size_t>(at)].parent) {work.spend();target_ancestors.insert(at);}
            for(int at=source_owner;at>=0;at=model.groups[static_cast<std::size_t>(at)].parent) {work.spend();if(target_ancestors.contains(at)) return at;}
            return -1;
        };
        const auto frontier_index=[&](GroupEndpointRef endpoint,int owner,int common) {
            if(endpoint.kind==GroupEndpointKind::Group && model.groups[endpoint.index].parent==common) return entity_index(endpoint);
            if(owner==common) return entity_index(endpoint);
            while(owner>=0 && model.groups[static_cast<std::size_t>(owner)].parent!=common) {work.spend();owner=model.groups[static_cast<std::size_t>(owner)].parent;}
            return owner>=0?node_count+static_cast<std::size_t>(owner):entity_index(endpoint);
        };
        std::vector<FrontierRankEdge> rank_edges;rank_edges.reserve(total);
        for(const auto& edge:edges) {
            work.spend();const auto source_index=entity_index(edge.source),target_index=entity_index(edge.target);
            if(semantic_reachable[target_index][source_index]) continue;
            const int source_owner=owner_of(edge.source),target_owner=owner_of(edge.target),common=common_owner(source_owner,target_owner);
            std::string_view direction=model.direction;
            for(int at=common;at>=0;at=model.groups[static_cast<std::size_t>(at)].parent) {work.spend();if(!model.groups[static_cast<std::size_t>(at)].direction.empty()) {direction=model.groups[static_cast<std::size_t>(at)].direction;break;}}
            rank_edges.push_back({frontier_index(edge.source,source_owner,common),frontier_index(edge.target,target_owner,common),common,direction});
        }
        std::vector<std::vector<std::vector<bool>>> frontier_reachable(group_count+1u,std::vector<std::vector<bool>>(entity_count,std::vector<bool>(entity_count)));
        const auto owner_slot=[&](int common_owner) {return common_owner<0?group_count:static_cast<std::size_t>(common_owner);};
        for(const auto& edge:rank_edges) {work.spend();frontier_reachable[owner_slot(edge.common_owner)][edge.from][edge.to]=true;}
        for(std::size_t owner=0u;owner<frontier_reachable.size();++owner) for(std::size_t k=0u;k<entity_count;++k) for(std::size_t i=0u;i<entity_count;++i) for(std::size_t j=0u;j<entity_count;++j) {
            work.spend();frontier_reachable[owner][i][j]=frontier_reachable[owner][i][j] || (frontier_reachable[owner][i][k]&&frontier_reachable[owner][k][j]);
        }
        for(const auto& edge:rank_edges) {
            work.spend();if(edge.from==edge.to || frontier_reachable[owner_slot(edge.common_owner)][edge.to][edge.from]) continue;
            const auto& from=entities.at(edge.from);const auto& to=entities.at(edge.to);
            const int dx=to.left+to.right-from.left-from.right,dy=to.top+to.bottom-from.top-from.bottom;
            if((edge.direction=="LR"&&dx<=0)||(edge.direction=="RL"&&dx>=0)||((edge.direction=="TD"||edge.direction=="TB")&&dy<=0)||(edge.direction=="BT"&&dy>=0))
                return {"GroupEndpoint acyclic rank ordering contradicts declared direction"};
        }

        const auto claim_entity=[&](std::string_view text,const ShapeFrame& frame) {
            const auto matches=label_blocks(grid,text,frame,work);
            const auto direct=std::ranges::find_if(matches,[&](const Label& block) {
                for(const auto& other:entities) {
                    if(classic_decorative_frame(other,frame)) continue;
                    if(!other.inside(frame)) continue;
                    for(const auto start:block.starts) if(other.interior(start)) return false;
                }
                return true;
            });
            if(direct==matches.end()) throw std::runtime_error("GroupEndpoint entity lacks direct label block");
            claim_label(grid,text,*direct,claimed);
        };
        for(std::size_t node=0u;node<node_count;++node) claim_entity(model.nodes[node].label,entities[node]);
        for(std::size_t group=0u;group<group_count;++group) claim_entity(model.groups[group].label,entities[node_count+group]);
        for(const auto& frame:entities) for(int y=frame.top;y<=frame.bottom;++y) for(int x=frame.left;x<=frame.right;++x) {
            work.spend();const Point point{x,y};if(!frame.boundary(point)) continue;
            const auto at=static_cast<std::size_t>(grid.index(point));const int extra=glyph(grid.at(point)).bits&~frame.perimeter_bits(point);
            if(extra&~route_arms[at]) return {"GroupEndpoint unclaimed arm on a node/group perimeter"};
            claimed[at]=true;
        }
        for(int y=0;y<grid.height;++y) {
            if(following_row && y==*following_row) continue;
            for(int x=0;x<grid.width;++x) {work.spend();const auto at=static_cast<std::size_t>(grid.index({x,y}));
                if(!claimed[at] && grid.at({x,y})!=" " && !grid.at({x,y}).empty()) return {"Unclaimed text/route glyph reveals an extra/misowned/duplicated fact"};}
        }
        return {};
    } catch(const std::exception& error) { return {error.what()}; }
}
} // namespace loom::test::mermaid_flow
