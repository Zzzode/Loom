// Test-only Block58 glyph candidate, derived before any Block producer.
// Coordinate/glyph approval and production prepayment remain independent gates.
#pragma once
#include "mermaid_block58_projection.hpp"

namespace loom::test::mermaid_block58::contours {
struct Raster {
    std::vector<std::string> rows;
    std::size_t visits=0,bytes=0;
};
namespace detail {
using projection::Point;
using projection::Entity;
inline constexpr std::array<std::string_view,16> kLine{
    " ","─","│","┌","─","─","┐","┬",
    "│","└","│","├","┘","┴","┤","┼"};
inline constexpr std::array<std::string_view,4> kArrow{"▶","▼","◀","▲"};
inline unsigned direction(Point a,Point b) {
    if(b.x>a.x)return 1;
    if(b.y>a.y)return 2;
    if(b.x<a.x)return 4;
    if(b.y<a.y)return 8;
    return 0;
}
struct Counter {
    Raster& out; OracleWork& work;
    void visit() { work.spend(1);++out.visits; }
};
inline std::string_view outline(const Entity& e,Point q,Counter& count) {
    count.visit();
    const auto& b=e.body;const int x=q.x-b.x,y=q.y-b.y;
    if(e.group||e.shape=="rectangle"||e.shape=="rounded_rectangle"||e.shape=="subroutine") {
        const bool left=x==0,right=x==b.w-1,top=y==0,bottom=y==b.h-1;
        if(e.shape=="subroutine"&&(x==2||x==b.w-3)) {
            if(top)return "┬";
            if(bottom)return "┴";
            return "│";
        }
        const bool round=e.shape=="rounded_rectangle";
        if(top&&left)return round?"╭":"┌";
        if(top&&right)return round?"╮":"┐";
        if(bottom&&left)return round?"╰":"└";
        if(bottom&&right)return round?"╯":"┘";
        return top||bottom?"─":"│";
    }
    // Independently authored full-boundary fixtures use filled terminal cells.
    // Every exposed analytic mask cell survives, including diagonal contacts,
    // both incident tip arms and union shoulders. No nearest-segment guess or
    // endpoint-marker shortcut participates in a nonrectangular contour.
    return "█";
}
inline std::string_view marker(std::string_view kind,int direction) {
    if(kind=="point") {projection::require(direction>=0&&direction<4,"contours marker direction");return kArrow[static_cast<std::size_t>(direction)];}
    if(kind=="circle")return "○";
    if(kind=="cross")return "×";
    projection::require(kind.empty()||kind=="arrow_open"||kind=="open"||kind=="none","contours unknown marker");
    return {};
}
} // namespace detail

inline Raster raster(const projection::Plan& p,OracleWork& work) {
    Raster out;detail::Counter count{out,work};
    projection::require(p.metrics.grid_width>0&&p.metrics.grid_width<=240&&p.metrics.grid_height>0&&p.metrics.grid_height<=1024,"contours grid bounds");
    projection::require(p.cells.size()==static_cast<std::size_t>(p.metrics.grid_width)*static_cast<std::size_t>(p.metrics.grid_height)&&p.routes.size()==p.edges.size()&&p.routes.size()<=9,"contours plan bounds");
    projection::require(p.entities.size()<=65&&p.notes.size()<=1024&&p.metrics.grid_height+static_cast<int>(p.notes.size())==p.metrics.height&&p.metrics.height<=1024,"contours output bounds");
    for(const auto& route:p.routes){count.visit();projection::require(route.points.size()>=2&&route.points.size()<=p.cells.size()&&route.label.size()<=32,"contours finite route");}
    // Pay the full fixed 240-cell row scratch and maximum segment frame, not
    // merely the active width. They are reused without an area-sized buffer.
    work.spend(static_cast<std::size_t>(p.metrics.height)*sizeof(std::string)+4096);
    out.rows.reserve(static_cast<std::size_t>(p.metrics.height));
    std::array<std::string_view,240> row{};
    for(int y=0;y<p.metrics.grid_height;++y) {
        for(int x=0;x<p.metrics.grid_width;++x) {
            count.visit();const auto& c=p.cells[projection::offset(p,{x,y})];auto& glyph=row[static_cast<std::size_t>(x)];glyph=" ";
            if(c.flags&1){projection::require(c.owner>0&&c.owner<p.entities.size(),"contours owner");glyph=detail::outline(p.entities[c.owner],{x,y},count);}
            if(c.flags&2) {
                if(c.owner>0&&c.owner<p.entities.size()) {
                    const auto& e=p.entities[c.owner];count.visit();
                    const int r=y-e.caption.y,col=x-e.caption.x;
                    if(r>=0&&static_cast<std::size_t>(r)<e.rows.size()&&col>=0&&static_cast<std::size_t>(col)<e.rows[static_cast<std::size_t>(r)].size()) {
                        glyph=std::string_view(e.rows[static_cast<std::size_t>(r)]).substr(static_cast<std::size_t>(col),1);
                        projection::require(glyph[0]>=32&&glyph[0]<=126,"contours frozen caption ASCII");
                    }
                }
            }
        }
        for(std::size_t n=0;n<p.routes.size();++n) {
            count.visit();const auto& route=p.routes[n];const auto& edge=p.edges[n];
            for(std::size_t i=0;i<route.points.size();++i) {
                count.visit();const auto q=route.points[i];if(q.y!=y)continue;
                projection::require(q.x>=0&&q.x<p.metrics.grid_width,"contours route bounds");
                unsigned bits=0;if(i)bits|=detail::direction(q,route.points[i-1]);if(i+1<route.points.size())bits|=detail::direction(q,route.points[i+1]);
                const auto& cell=p.cells[projection::offset(p,q)];
                if(cell.flags&1) {
                    for(std::size_t d=0;d<4;++d) {count.visit();const auto step=projection::kStep[d];const projection::Point adjacent{q.x+step.x,q.y+step.y};
                        if(projection::inside(p,adjacent)){const auto& c=p.cells[projection::offset(p,adjacent)];if(c.owner==cell.owner&&(c.flags&1))bits|=1u<<d;}}
                }
                // Connected nonrectangular ports retain their complete pixel
                // boundary; only the adjacent outward stem carries route ink.
                if((cell.flags&1)&&cell.owner>0) {
                    const auto& endpoint=p.entities[cell.owner];
                    const bool rect=endpoint.group||endpoint.shape=="rectangle"||endpoint.shape=="rounded_rectangle"||endpoint.shape=="subroutine";
                    if(!rect) {row[static_cast<std::size_t>(q.x)]="█";continue;}
                }
                auto glyph=detail::kLine[bits];
                if(edge.dotted&&(cell.flags&1)==0){if(bits==5||bits==1||bits==4)glyph="┄";else if(bits==10||bits==2||bits==8)glyph="┆";}
                row[static_cast<std::size_t>(q.x)]=glyph;
            }
            for(int end=0;end<2;++end) {
                count.visit();const auto at=end?route.end_marker:route.start_marker;
                const auto glyph=detail::marker(end?route.end_kind:route.start_kind,end?route.end_direction:route.start_direction);
                if(at.y==y&&!glyph.empty()){projection::require(at.x>=0&&at.x<p.metrics.grid_width,"contours marker bounds");row[static_cast<std::size_t>(at.x)]=glyph;}
            }
            if(route.label_box.y==y)for(std::size_t x=0;x<route.label.size();++x){count.visit();const int col=route.label_box.x+static_cast<int>(x);projection::require(col>=0&&col<p.metrics.grid_width,"contours label bounds");row[static_cast<std::size_t>(col)]=std::string_view(route.label).substr(x,1);}
        }
        std::size_t bytes=0;for(int x=0;x<p.metrics.grid_width;++x){count.visit();const auto g=row[static_cast<std::size_t>(x)];projection::require(!g.empty()&&g.size()<=3,"contours one-cell glyph bytes");bytes+=g.size();}
        work.spend(bytes);std::string line;line.reserve(bytes);for(int x=0;x<p.metrics.grid_width;++x)line.append(row[static_cast<std::size_t>(x)]);
        out.bytes+=line.size();out.rows.push_back(std::move(line));
    }
    std::size_t note_cells=0;
    for(const auto& note:p.notes){projection::require(note.size()<=32,"contours frozen note width");for(const unsigned char c:note){count.visit();projection::require(c>=32&&c<=126,"contours frozen note ASCII");}work.spend(note.size());note_cells+=note.size();out.bytes+=note.size();out.rows.push_back(note);}
    projection::require(out.bytes<=3*p.cells.size()+note_cells&&out.bytes<=p.metrics.serialized_bytes_upper,"contours serialized upper bound");
    return out;
}
} // namespace loom::test::mermaid_block58::contours
