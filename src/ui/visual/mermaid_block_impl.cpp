module;

#include <ftxui/screen/string.hpp>
#include <ftxui/dom/node.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace block {
// RFC 0030: private closed Block grammar and terminal lattice. No source
// filename, fixture JSON, active palette, shared Canvas or exported model.
struct Rejected {};
void require(bool value,std::string_view = {}) {if(!value)throw Rejected{};}
struct Ledger {
    Work& work;
    std::size_t used=0;
    void pay(std::size_t count,std::string_view = {}) {if(!work.spend(count))throw Rejected{};used+=count;}
    void product(std::size_t n,std::size_t unit,std::string_view stage={}) {
        require(!unit||n<=std::numeric_limits<std::size_t>::max()/unit);pay(n*unit,stage);
    }
};
std::size_t scalar_length(std::string_view s,std::size_t at) {
    const auto c=static_cast<unsigned char>(s[at]);return c<0x80?1:c<0xe0?2:c<0xf0?3:4;
}
struct Point { int x=0,y=0; bool operator==(const Point&) const = default; };
struct Box {
    int x=0,y=0,w=0,h=0;
    bool contains(Point p) const { return p.x>=x&&p.y>=y&&p.x<x+w&&p.y<y+h; }
};
inline constexpr std::array<Point,4> kStep{{{1,0},{0,1},{-1,0},{0,-1}}};
struct Placement { int child=0,col=0,row=0,span=1; };
struct Entity {
    std::string id,shape;
    std::vector<std::string> rows;
    std::vector<Placement> children;
    std::vector<int> columns,row_heights;
    int parent=-1,span=1,min_w=0,min_h=0;
    unsigned heads=0;
    bool group=false,space=false;
    std::uint16_t fill=1,stroke=1;
    bool bold=false;
    Box band,body,caption,core;
    std::array<Point,4> ports{};
};
struct Edge {
    int source=0,target=0;
    std::string label,start,end;
    bool bold=false,dotted=false;
};
// The proposed compact raster uses this exact ABI; it retains no owning string.
struct Cell {
    std::string_view glyph=" ";
    std::uint16_t style=0;
    std::uint8_t owner=0,flags=0;
};
static_assert(sizeof(Cell)<=24);
struct Gate { int group=0; Point at; };
struct Route {
    int source=0,target=0;
    std::vector<Point> points;
    std::vector<Gate> gates;
    Point start_marker,end_marker;
    int start_direction=-1,end_direction=-1;
    std::string start_kind,end_kind,label;
    Box label_box;
};
struct Metrics {
    std::size_t area=0;
    int grid_width=0,grid_height=0,height=0;
};
struct Plan {
    std::vector<Entity> entities;
    std::vector<Edge> edges;
    std::vector<Cell> cells;
    std::vector<Route> routes;
    std::vector<std::string> notes;
    Metrics metrics;
};

inline void natural(Entity& e) {
    int w=0;for(const auto& row:e.rows)w=std::max(w,ftxui::string_width(row));
    const int h=static_cast<int>(e.rows.size());
    if(e.space){e.min_w=1;e.min_h=5;return;}
    e.min_w=std::max(9,w+4);e.min_h=std::max(5,h+2);
    if(e.shape=="circle"||e.shape=="diamond") {
        const int side=std::max(10,w+2*h+6);e.min_w=e.min_h=side+(side%2);
    } else if(e.shape=="hexagon"||e.shape=="lean_right"||e.shape=="trapezoid"||e.shape=="rect_left_inv_arrow") {
        e.min_w=std::max(11,w+8);e.min_h=std::max(7,h+4);
    } else if(e.shape=="stadium") {
        e.min_h=std::max(7,h+4);e.min_w=std::max(e.min_h+4,w+8);
    } else if(e.shape=="subroutine")e.min_w=std::max(9,w+8);
    else if(e.shape=="block_arrow") {
        require(e.heads==1||e.heads==2||e.heads==4||e.heads==8||e.heads==5||e.heads==10||e.heads==15,"Block unreviewed block-arrow direction set");
        const bool horizontal=(e.heads&5)!=0,vertical=(e.heads&10)!=0;
        const int l=std::max((e.heads&4)?3:0,vertical?1:0),r=std::max((e.heads&1)?3:0,vertical?1:0);
        const int t=std::max((e.heads&8)?3:0,horizontal?1:0),b=std::max((e.heads&2)?3:0,horizontal?1:0);
        e.min_w=std::max(7,w+4)+l+r;e.min_h=std::max(5,h+2)+t+b;
    } else require(e.shape=="rectangle"||e.shape=="rounded_rectangle","Block unknown shape");
}
inline int measure(Plan& p,int id,Ledger& ledger,int depth=0) {
    require(depth<=16,"Block depth");auto& e=p.entities.at(static_cast<std::size_t>(id));
    if(!e.group){natural(e);return e.min_w;}
    int k=0,row_count=0,unit=1;
    for(const auto& c:e.children) {
        ledger.pay(32,"grid measure");measure(p,c.child,ledger,depth+1);
        const auto& child=p.entities.at(static_cast<std::size_t>(c.child));
        k=std::max(k,c.col+c.span);row_count=std::max(row_count,c.row+1);
        unit=std::max(unit,std::max(0,child.min_w-3*(c.span-1)+c.span-1)/c.span);
    }
    require(k>0&&k<=64&&row_count>0&&row_count<=64,"Block empty or excessive grid");
    ledger.product(static_cast<std::size_t>(k+row_count),2*sizeof(int),"grid arrays");
    e.columns.assign(static_cast<std::size_t>(k),unit);e.row_heights.assign(static_cast<std::size_t>(row_count),0);
    for(const auto& c:e.children)e.row_heights[static_cast<std::size_t>(c.row)]=
        std::max(e.row_heights[static_cast<std::size_t>(c.row)],p.entities[static_cast<std::size_t>(c.child)].min_h);
    const int pad=id==0?2:4;
    e.min_w=k*unit+3*(k-1)+pad;e.min_h=std::accumulate(e.row_heights.begin(),e.row_heights.end(),0)+3*(row_count-1)+pad;
    require(e.min_w<=240&&e.min_h<=1024,"Block intrinsic canvas bound");return e.min_w;
}
inline void place(Plan& p,int id,Box band,Ledger& ledger) {
    auto& e=p.entities[static_cast<std::size_t>(id)];e.band=band;e.body=band;
    if(!e.group) {
        if(e.shape=="circle"||e.shape=="diamond") {
            e.body.x+=(band.w-e.min_w)/2;e.body.w=e.min_w;
        }
        e.core=e.body;
        if(e.shape=="block_arrow") {
            const bool hz=(e.heads&5)!=0,vt=(e.heads&10)!=0;
            const int l=std::max((e.heads&4)?3:0,vt?1:0),r=std::max((e.heads&1)?3:0,vt?1:0);
            const int t=std::max((e.heads&8)?3:0,hz?1:0),b=std::max((e.heads&2)?3:0,hz?1:0);
            e.core={e.body.x+l,e.body.y+t,e.body.w-l-r,e.body.h-t-b};
        }
        int cw=0;for(const auto& row:e.rows)cw=std::max(cw,ftxui::string_width(row));
        const int ch=static_cast<int>(e.rows.size());
        e.caption={e.core.x+(e.core.w-cw)/2,e.core.y+(e.core.h-ch)/2,cw,ch};return;
    }
    const int pad=id==0?1:2,k=static_cast<int>(e.columns.size());
    const int available=band.w-2*pad-3*(k-1);require(available>=k,"Block column grant");
    for(int x=0;x<k;++x){ledger.pay(16,"column grant");e.columns[static_cast<std::size_t>(x)]=available/k+(x<available%k?1:0);}
    for(const auto& c:e.children) {
        ledger.pay(32,"grid placement");int x=band.x+pad,y=band.y+pad,w=0;
        for(int col=0;col<c.col;++col){ledger.pay(16,"column offset");x+=e.columns[static_cast<std::size_t>(col)]+3;}
        for(int row=0;row<c.row;++row){ledger.pay(16,"row offset");y+=e.row_heights[static_cast<std::size_t>(row)]+3;}
        for(int col=c.col;col<c.col+c.span;++col){ledger.pay(16,"span width");w+=e.columns[static_cast<std::size_t>(col)];}w+=3*(c.span-1);
        const auto& child=p.entities[static_cast<std::size_t>(c.child)];
        const int height=child.min_h;y+=(e.row_heights[static_cast<std::size_t>(c.row)]-height)/2;
        place(p,c.child,{x,y,w,height},ledger);
    }
}
inline bool triangle(Point p,Point a,Point b,Point c) {
    const auto cross=[](Point u,Point v,Point q)->std::int64_t{return static_cast<std::int64_t>(v.x-u.x)*(q.y-u.y)-static_cast<std::int64_t>(v.y-u.y)*(q.x-u.x);};
    const auto x=cross(a,b,p),y=cross(b,c,p),z=cross(c,a,p);
    return (x>=0&&y>=0&&z>=0)||(x<=0&&y<=0&&z<=0);
}
inline bool polygon(Point p,std::span<const Point> vertices) {
    bool in=false;
    for(std::size_t i=0,j=vertices.size()-1;i<vertices.size();j=i++) {
        const auto a=vertices[j],b=vertices[i];
        const std::int64_t cross=static_cast<std::int64_t>(b.x-a.x)*(p.y-a.y)-static_cast<std::int64_t>(b.y-a.y)*(p.x-a.x);
        if(cross==0&&p.x>=std::min(a.x,b.x)&&p.x<=std::max(a.x,b.x)&&p.y>=std::min(a.y,b.y)&&p.y<=std::max(a.y,b.y))return true;
        if((a.y>p.y)!=(b.y>p.y)) {
            // Sign-aware integer comparison avoids floating point edge ties.
            const auto lhs=static_cast<std::int64_t>(p.x-a.x)*(b.y-a.y),rhs=static_cast<std::int64_t>(b.x-a.x)*(p.y-a.y);
            if((b.y>a.y&&lhs<rhs)||(b.y<a.y&&lhs>rhs))in=!in;
        }
    }return in;
}
inline bool mask(const Entity& e,Point world) {
    if(!e.body.contains(world))return false;
    const int x=world.x-e.body.x,y=world.y-e.body.y,w=e.body.w,h=e.body.h;
    if(e.group||e.shape=="rectangle"||e.shape=="rounded_rectangle"||e.shape=="subroutine")return true;
    if(e.shape=="circle") {
        const auto dx=2*x-(w-1),dy=2*y-(h-1);return dx*dx+dy*dy<=w*w;
    }
    if(e.shape=="diamond")return std::abs(2*x-(w-1))+std::abs(2*y-(h-1))<=w;
    if(e.shape=="stadium") {
        const int r=(h-1)/2;
        if(x>=r&&x<=w-1-r)return true;
        const int cx=x<r?r:w-1-r;return (x-cx)*(x-cx)+(y-r)*(y-r)<=r*r;
    }
    if(e.shape=="block_arrow") {
        const auto& c=e.core;const Point q{world.x,world.y};
        if(c.contains(q))return true;
        const int l=c.x,r=c.x+c.w-1,t=c.y,b=c.y+c.h-1,mx=l+(c.w-1)/2,my=t+(c.h-1)/2;
        return ((e.heads&1)&&triangle(q,{r-1,t-1},{r+3,my},{r-1,b+1}))||
            ((e.heads&4)&&triangle(q,{l+1,t-1},{l-3,my},{l+1,b+1}))||
            ((e.heads&8)&&triangle(q,{l-1,t+1},{mx,t-3},{r+1,t+1}))||
            ((e.heads&2)&&triangle(q,{l-1,b-1},{mx,b+3},{r+1,b-1}));
    }
    if(e.shape=="hexagon") {const std::array<Point,6> v{{{2,0},{w-3,0},{w-1,h/2},{w-3,h-1},{2,h-1},{0,h/2}}};return polygon({x,y},v);}
    if(e.shape=="lean_right") {const std::array<Point,4> v{{{2,0},{w-1,0},{w-3,h-1},{0,h-1}}};return polygon({x,y},v);}
    if(e.shape=="trapezoid") {const std::array<Point,4> v{{{2,0},{w-3,0},{w-1,h-1},{0,h-1}}};return polygon({x,y},v);}
    if(e.shape=="rect_left_inv_arrow") {const std::array<Point,5> v{{{0,0},{2,h/2},{0,h-1},{w-1,h-1},{w-1,0}}};return polygon({x,y},v);}
    return false;
}
inline std::size_t offset(const Plan& p,Point q) {
    require(q.x>=0&&q.y>=0&&q.x<p.metrics.grid_width&&q.y<p.metrics.grid_height,"Block raster bounds");
    return static_cast<std::size_t>(q.y)*static_cast<std::size_t>(p.metrics.grid_width)+static_cast<std::size_t>(q.x);
}
inline bool inside(const Plan& p,Point q) {return q.x>=0&&q.y>=0&&q.x<p.metrics.grid_width&&q.y<p.metrics.grid_height;}
inline bool ancestor(const Plan& p,int group,int node,Ledger& ledger) {
    for(int n=p.entities[static_cast<std::size_t>(node)].parent;n>=0;n=p.entities[static_cast<std::size_t>(n)].parent){ledger.pay(16,"ancestor walk");if(n==group)return true;}
    return false;
}
inline void paint(Plan& p,Ledger& ledger) {
    for(const bool composites:{true,false})for(std::size_t i=1;i<p.entities.size();++i) {
        auto& e=p.entities[i];if(e.space||e.group!=composites)continue;
        ledger.product(static_cast<std::size_t>(e.body.w*e.body.h),8,"mask visits");
        for(int y=e.body.y;y<e.body.y+e.body.h;++y)for(int x=e.body.x;x<e.body.x+e.body.w;++x) {
            if(!mask(e,{x,y}))continue;
            ledger.pay(8,"mask write");
            auto& cell=p.cells[offset(p,{x,y})];cell.owner=static_cast<std::uint8_t>(i);cell.style=static_cast<std::uint16_t>(1|(e.fill<<5));
        }
        ledger.product(static_cast<std::size_t>(e.body.w*e.body.h),8,"outline visits");
        for(int y=e.body.y;y<e.body.y+e.body.h;++y)for(int x=e.body.x;x<e.body.x+e.body.w;++x) {

            auto& cell=p.cells[offset(p,{x,y})];if(cell.owner!=i)continue;
            bool border=false;
            for(const auto d:kStep) {ledger.pay(8,"outline neighbour");Point q{x+d.x,y+d.y};if(!inside(p,q)||p.cells[offset(p,q)].owner!=i)border=true;}
            const bool rail=e.shape=="subroutine"&&(x==e.body.x+2||x==e.body.x+e.body.w-3)&&y>e.body.y&&y<e.body.y+e.body.h-1;
            if(border||rail) {
                ledger.pay(8,"outline write");
                cell.flags|=1;cell.glyph="│";
                // Final contour glyphs replace this static non-owning placeholder
                // after all masks, captions, routes and styles are resolved.
                const auto bg=rail?e.fill:(e.parent>0?p.entities[static_cast<std::size_t>(e.parent)].fill:0);
                cell.style=static_cast<std::uint16_t>(e.stroke|(bg<<5)|(e.bold?1024:0));
            }
        }
        if(e.group)continue;
        for(std::size_t row=0;row<e.rows.size();++row) {
            int column=0;const auto& text=e.rows[row];
            for(std::size_t byte=0;byte<text.size();) {
                const auto begin=byte;auto length=scalar_length(text,byte);
                const int width=ftxui::string_width(text.substr(byte,length));require(width==1||width==2,"caption base glyph");byte+=length;
                while(byte<text.size()) {length=scalar_length(text,byte);if(ftxui::string_width(text.substr(byte,length))!=0)break;byte+=length;}
                for(int cell=0;cell<width;++cell) {
                    const Point q{e.caption.x+column+cell,e.caption.y+static_cast<int>(row)};
                    require(e.body.contains(q),"caption allocation");ledger.pay(8,"caption write");auto& target=p.cells[offset(p,q)];
                    require(target.owner==i&&(target.flags&1)==0,"caption interior");target.flags|=2;
                    target.glyph=cell==0?std::string_view(text).substr(begin,byte-begin):std::string_view{};
                }
                column+=width;
            }
        }
        const Point anchor{e.core.x+(e.core.w-1)/2,e.core.y+(e.core.h-1)/2};
        for(int d=0;d<4;++d) {
            Point q=anchor;require(mask(e,q),"Block port anchor");
            while(true) {Point next{q.x+kStep[static_cast<std::size_t>(d)].x,q.y+kStep[static_cast<std::size_t>(d)].y};
                ledger.pay(8,"port ray");if(!mask(e,next))break;q=next;}
            e.ports[static_cast<std::size_t>(d)]=q;
        }
    }
    for(auto& e:p.entities)if(e.group) {
        const auto& b=e.body;e.ports={Point{b.x+b.w-1,b.y+b.h/2},Point{b.x+b.w/2,b.y+b.h-1},Point{b.x,b.y+b.h/2},Point{b.x+b.w/2,b.y}};
    }
}
inline Point outside(const Entity& e,int direction,Ledger& ledger) {
    auto q=e.ports[static_cast<std::size_t>(direction)];const auto d=kStep[static_cast<std::size_t>(direction)];
    do {ledger.pay(8,"endpoint stem");q.x+=d.x;q.y+=d.y;}while(e.band.contains(q));return q;
}
inline std::vector<Point> expand(std::span<const Point> bends) {
    std::vector<Point> points;
    std::size_t count=1;for(std::size_t i=1;i<bends.size();++i)count+=static_cast<std::size_t>(std::abs(bends[i].x-bends[i-1].x)+std::abs(bends[i].y-bends[i-1].y));points.reserve(count);
    for(std::size_t i=1;i<bends.size();++i) {
        auto q=bends[i-1],end=bends[i];require(q.x==end.x||q.y==end.y,"Block orthogonal route");
        const Point step{(end.x>q.x)-(end.x<q.x),(end.y>q.y)-(end.y<q.y)};
        if(points.empty())points.push_back(q);
        while(q!=end){q.x+=step.x;q.y+=step.y;points.push_back(q);}
    }return points;
}
inline bool simple_polyline(std::span<const Point> bends) {
    // At most eight bends: pairwise segment tests need no second area buffer.
    for(std::size_t i=2;i<bends.size();++i) {
        const Point a{bends[i-1].x-bends[i-2].x,bends[i-1].y-bends[i-2].y};
        const Point b{bends[i].x-bends[i-1].x,bends[i].y-bends[i-1].y};
        if(a.x*b.y==a.y*b.x&&a.x*b.x+a.y*b.y<0)return false;
    }
    for(std::size_t i=1;i<bends.size();++i)for(std::size_t j=i+2;j<bends.size();++j) {
        const auto a=bends[i-1],b=bends[i],c=bends[j-1],d=bends[j];
        const int lx=std::max(std::min(a.x,b.x),std::min(c.x,d.x)),rx=std::min(std::max(a.x,b.x),std::max(c.x,d.x));
        const int ly=std::max(std::min(a.y,b.y),std::min(c.y,d.y)),ry=std::min(std::max(a.y,b.y),std::max(c.y,d.y));
        if(lx<=rx&&ly<=ry)return false;
    }
    return true;
}
inline bool on_stem(Point q,Point port,Point out) {
    return (port.x==out.x&&q.x==port.x&&q.y>=std::min(port.y,out.y)&&q.y<=std::max(port.y,out.y))||
        (port.y==out.y&&q.y==port.y&&q.x>=std::min(port.x,out.x)&&q.x<=std::max(port.x,out.x));
}
inline bool valid_route(Plan& p,const Edge& e,std::span<const Point> points,int sd,int td,Route& out,Ledger& ledger) {
    const auto& source=p.entities[static_cast<std::size_t>(e.source)];const auto& target=p.entities[static_cast<std::size_t>(e.target)];
    const auto sp=source.ports[static_cast<std::size_t>(sd)],tp=target.ports[static_cast<std::size_t>(td)];
    const auto so=outside(source,sd,ledger),to=outside(target,td,ledger);std::vector<int> required;required.reserve(32);
    for(int g=source.parent;g>0;g=p.entities[static_cast<std::size_t>(g)].parent)if(!ancestor(p,g,e.target,ledger))required.push_back(g);
    std::vector<int> entering;entering.reserve(16);for(int g=target.parent;g>0;g=p.entities[static_cast<std::size_t>(g)].parent)if(!ancestor(p,g,e.source,ledger))entering.push_back(g);
    std::ranges::reverse(entering);required.insert(required.end(),entering.begin(),entering.end());
    std::vector<Gate> gates;gates.reserve(32);
    for(std::size_t at=0;at<points.size();++at) {
        ledger.pay(8,"route cell");const auto q=points[at];if(!inside(p,q))return false;
        const auto& cell=p.cells[offset(p,q)];if((cell.flags&6)!=0)return false;
        for(std::size_t n=1;n<p.entities.size();++n) {
            ledger.pay(8,"route allocation inspection");
            const auto& box=p.entities[n];if(box.space||!box.band.contains(q))continue;
            const bool endpoint=static_cast<int>(n)==e.source||static_cast<int>(n)==e.target;
            if(endpoint) {if(!on_stem(q,sp,so)&&!on_stem(q,tp,to))return false;continue;}
            if(!box.group)return false;
            if(!ancestor(p,static_cast<int>(n),e.source,ledger)&&!ancestor(p,static_cast<int>(n),e.target,ledger))return false;
            const auto& b=box.body;const bool vertical=q.x==b.x||q.x==b.x+b.w-1,horizontal=q.y==b.y||q.y==b.y+b.h-1;
            if(vertical||horizontal) {
                if(vertical&&horizontal)return false;
                if(std::ranges::find(required,static_cast<int>(n))==required.end()||at==0||at+1==points.size())return false;
                const auto before=points[at-1],after=points[at+1];
                if((vertical&&(before.y!=q.y||after.y!=q.y))||(horizontal&&(before.x!=q.x||after.x!=q.x)))return false;
                if(gates.size()>=required.size()||required[gates.size()]!=static_cast<int>(n))return false;
                gates.push_back({static_cast<int>(n),q});
            }
        }
    }
    if(gates.size()!=required.size())return false;
    for(std::size_t i=0;i<required.size();++i)if(gates[i].group!=required[i])return false;
    Box label{};
    if(!e.label.empty()) {
        bool found=false;
        for(std::size_t i=1;i<points.size()&&!found;++i) {
            if(points[i].y!=points[i-1].y)continue;
            std::size_t last=i;while(last+1<points.size()&&points[last+1].y==points[i].y)++last;
            const int lo=std::min(points[i-1].x,points[last].x),hi=std::max(points[i-1].x,points[last].x);
            if(hi-lo>=ftxui::string_width(e.label)+4)for(const int dy:{-1,1}) {
                Box candidate{lo+(hi-lo+1-ftxui::string_width(e.label))/2,points[i].y+dy,ftxui::string_width(e.label),1};bool clear=true;
                for(int x=candidate.x-1;x<=candidate.x+candidate.w;++x) {
                    ledger.pay(8,"route label");Point q{x,candidate.y};
                    ledger.product(points.size(),8,"candidate label versus polyline");
                    if(std::ranges::find(points,q)!=points.end()){clear=false;break;}
                    if(!inside(p,q)||(p.cells[offset(p,q)].flags&7)!=0){clear=false;break;}
                    for(std::size_t n=1;n<p.entities.size();++n)if(!p.entities[n].group&&!p.entities[n].space&&p.entities[n].band.contains(q))clear=false;
                }
                if(clear){label=candidate;found=true;break;}
            }
            i=last;
        }if(!found)return false;
    }
    ledger.product(e.label.size()+1,2,"selected route label");
    out.source=e.source;out.target=e.target;out.points.assign(points.begin(),points.end());out.gates=std::move(gates);out.label=e.label;out.label_box=label;
    out.start_kind=e.start;out.end_kind=e.end;
    out.start_marker={sp.x+kStep[static_cast<std::size_t>(sd)].x,sp.y+kStep[static_cast<std::size_t>(sd)].y};
    out.end_marker={tp.x+kStep[static_cast<std::size_t>(td)].x,tp.y+kStep[static_cast<std::size_t>(td)].y};
    out.start_direction=(sd+2)%4;out.end_direction=(td+2)%4;return true;
}
inline void route_all(Plan& p,Ledger& ledger) {
    ledger.product(2*(8*p.entities.size()+2),sizeof(int),"route lane storage");
    std::vector<int> xs,ys;xs.reserve(8*p.entities.size()+2);ys.reserve(8*p.entities.size()+2);
    xs.push_back(0);xs.push_back(p.metrics.grid_width-1);ys.push_back(0);ys.push_back(p.metrics.grid_height-1);
    for(const auto& n:p.entities) {
        if(n.group){xs.push_back(n.body.x+1);xs.push_back(n.body.x+n.body.w-2);ys.push_back(n.body.y+1);ys.push_back(n.body.y+n.body.h-2);}
        if(n.space){xs.push_back(n.band.x+n.band.w/2);ys.push_back(n.band.y+n.band.h/2);}
        for(const auto q:n.ports){xs.push_back(q.x);ys.push_back(q.y);}
    }
    std::ranges::sort(xs);xs.erase(std::unique(xs.begin(),xs.end()),xs.end());std::ranges::sort(ys);ys.erase(std::unique(ys.begin(),ys.end()),ys.end());

    for(const auto& edge:p.edges) {
        struct Pair {int sd,td,distance,order;};
        const auto& s=p.entities[static_cast<std::size_t>(edge.source)];const auto& t=p.entities[static_cast<std::size_t>(edge.target)];
        ledger.pay(2*16*sizeof(Pair)+512,"port pair storage and ordering");
        std::vector<Pair> pairs;pairs.reserve(16);
        constexpr std::array<int,4> target_order{2,3,0,1};
        for(int sd=0;sd<4;++sd)for(int rank=0;rank<4;++rank) {
            const int td=target_order[static_cast<std::size_t>(rank)];const auto a=s.ports[static_cast<std::size_t>(sd)],b=t.ports[static_cast<std::size_t>(td)];
            pairs.push_back({sd,td,std::abs(a.x-b.x)+std::abs(a.y-b.y),sd*4+rank});
        }
        std::ranges::sort(pairs,[](const Pair& a,const Pair& b){return std::tie(a.distance,a.order)<std::tie(b.distance,b.order);});
        bool done=false;Route selected;
        for(const auto pair:pairs) {
            ledger.pay(16,"port pair");
            if((p.cells[offset(p,s.ports[static_cast<std::size_t>(pair.sd)])].flags&4)!=0||
               (p.cells[offset(p,t.ports[static_cast<std::size_t>(pair.td)])].flags&4)!=0)continue;
            const auto a=outside(s,pair.sd,ledger),b=outside(t,pair.td,ledger);
            const auto attempt=[&](std::initializer_list<Point> mid) {
                if(done)return;
                ledger.pay(256+2*17*(sizeof(int)+sizeof(Gate)),"candidate bends/gates");
                std::vector<Point> bends{s.ports[static_cast<std::size_t>(pair.sd)]};bends.reserve(8);bends.insert(bends.end(),mid.begin(),mid.end());bends.push_back(t.ports[static_cast<std::size_t>(pair.td)]);
                bends.erase(std::unique(bends.begin(),bends.end()),bends.end());
                if(!simple_polyline(bends))return;
                std::size_t length=1;for(std::size_t i=1;i<bends.size();++i)length+=static_cast<std::size_t>(std::abs(bends[i].x-bends[i-1].x)+std::abs(bends[i].y-bends[i-1].y));
                ledger.product(length,4*sizeof(Point),"route candidate and retained storage");
                auto points=expand(bends);
                if(valid_route(p,edge,points,pair.sd,pair.td,selected,ledger))done=true;
            };
            attempt({a,{b.x,a.y},b});attempt({a,{a.x,b.y},b});
            for(int x:xs){if(done)break;attempt({a,{x,a.y},{x,b.y},b});}
            for(int y:ys){if(done)break;attempt({a,{a.x,y},{b.x,y},b});}
            if(done)break;
        }
        require(done,"route unavailable");
        for(const auto q:selected.points) {
            ledger.pay(8,"route raster write");
            auto& c=p.cells[offset(p,q)];c.flags|=4;c.glyph="│";
            const bool pixel_port=(c.flags&1)&&c.owner>0&&!p.entities[c.owner].group&&p.entities[c.owner].shape!="rectangle"&&p.entities[c.owner].shape!="rounded_rectangle"&&p.entities[c.owner].shape!="subroutine";
            if(!pixel_port)c.style=static_cast<std::uint16_t>(1|(c.style&992)|(edge.bold?1024:0));
        }
        for(int x=selected.label_box.x-1;selected.label_box.w>0&&x<=selected.label_box.x+selected.label_box.w;++x) {
            ledger.pay(8,"route label write");
            auto& c=p.cells[offset(p,{x,selected.label_box.y})];c.flags|=2;if(x>=selected.label_box.x&&x<selected.label_box.x+selected.label_box.w)c.style=static_cast<std::uint16_t>(1|(c.style&992));c.glyph=" ";
        }
        p.routes.push_back(std::move(selected));
    }
}

std::string join(Ledger& l,std::initializer_list<std::string_view> parts) {
    std::size_t n=0;for(auto part:parts){require(part.size()<=4096-n);n+=part.size();}
    l.product(n+1,2,"composed source notice");std::string out;out.reserve(n);for(auto part:parts)out.append(part);return out;
}
std::string_view trim(std::string_view s) {
    while(!s.empty()&&(s.front()==' '||s.front()=='\t'||s.front()=='\r'))s.remove_prefix(1);
    while(!s.empty()&&(s.back()==' '||s.back()=='\t'||s.back()=='\r'))s.remove_suffix(1);
    return s;
}
template<class T> void reserve(std::vector<T>& v,std::size_t n,Ledger& l) {
    if(n<=v.capacity())return;
    l.product(n,2*sizeof(T),"bounded vector capacity");v.reserve(n);require(v.capacity()<=2*n);
}
template<class T> void append(std::vector<T>& v,T value,std::size_t limit,Ledger& l) {
    require(v.size()<limit);if(v.size()==v.capacity())reserve(v,std::min(limit,std::max<std::size_t>(1,2*v.size())),l);v.push_back(std::move(value));
}
std::string own(std::string_view s,Ledger& l) {l.product(s.size()+1,2,"owned string");return std::string(s);}
int width(std::string_view text) {
    int cells=0;for(std::size_t at=0;at<text.size();){const auto n=scalar_length(text,at);cells+=ftxui::string_width(std::string(text.substr(at,n)));at+=n;}return cells;
}
std::vector<std::string> wrap(std::string_view s,Ledger& l) {
    require(s.size()<=4096&&unicode_text(s));
    l.product(s.size()+1,32,"wrapping scans and string growth");
    std::vector<std::string> out;const int total=width(s);require(total<=4096);
    reserve(out,static_cast<std::size_t>((total+31)/32+1),l);
    if(s.empty())return out;
    if(s==" "){out.emplace_back(" ");return out;}
    std::string line;int cells=0;std::size_t at=0;bool separator=false;
    const auto flush=[&]{require(!line.empty());append(out,std::move(line),1024,l);line.clear();cells=0;};
    while(at<=s.size()) {
        const auto next=s.find(' ',at);auto word=s.substr(at,next==std::string_view::npos?s.size()-at:next-at);
        int width=block::width(word);
        if(separator&&cells+1+width>32&&!line.empty())flush();
        else if(separator){line+=' ';++cells;}
        if(width>32&&!line.empty())flush();
        while(width>32) {
            std::size_t end=0;int count=0;
            while(end<word.size()) {
                const auto length=scalar_length(word,end);const int w=ftxui::string_width(std::string(word.substr(end,length)));
                require(w>=0&&w<=2);if(w>0&&count+w>32)break;count+=w;end+=length;
            }
            require(end>0);line.append(word.substr(0,end));cells=count;flush();word.remove_prefix(end);width-=count;
        }
        line.append(word);cells+=width;
        if(next==std::string_view::npos)break;
        at=next+1;separator=true;
    }
    if(!line.empty())append(out,std::move(line),1024,l);return out;
}
std::vector<std::string> caption(std::string_view raw,Ledger& l) {
    require(raw.size()<=1024&&unicode_text(raw));l.product(raw.size()+1,4,"caption decode");std::string decoded;decoded.reserve(raw.size());
    for(std::size_t at=0;at<raw.size();) {
        if(raw.substr(at).starts_with("&lt;")){decoded+='<';at+=4;}
        else if(raw.substr(at).starts_with("&gt;")){decoded+='>';at+=4;}
        else {require(raw[at]!='&'&&raw[at]!='<'&&raw[at]!='>'&&raw[at]!='`');decoded+=raw[at++];}
    }
    require(ftxui::string_width(decoded)<=256);return wrap(decoded,l);
}
struct Property {std::string_view literal;unsigned hue=0;};
struct Style {
    std::array<Property,4> values{};
    std::array<unsigned,4> order{};
    unsigned count=0;
    void set(unsigned key,Property value) {
        if(values[key].literal.empty())order[count++]=key;
        values[key]=value;
    }
};
constexpr std::array<std::string_view,8> kHues{"Neutral","Red","Orange","Yellow","Green","Cyan","Blue","Purple"};
constexpr std::array<unsigned,8> kRoles{1,2,3,3,4,5,5,6};
std::string_view hue_name(unsigned h,bool shared=true) {
    constexpr std::array<std::string_view,8> display{"Neutral","Red","Orange (shared warning)","Yellow (shared warning)","Green","Cyan (shared info)","Blue (shared info)","Purple"};
    return shared?display.at(h):kHues.at(h);
}
unsigned color_hue(std::string_view value) {
    require((value.size()==4||value.size()==7)&&value.front()=='#');
    const auto nibble=[](char c)->int {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;throw Rejected{};};
    std::array<int,3> rgb{};
    for(int i=0;i<3;++i)rgb[static_cast<std::size_t>(i)]=value.size()==4?17*nibble(value[static_cast<std::size_t>(i+1)]):16*nibble(value[static_cast<std::size_t>(1+2*i)])+nibble(value[static_cast<std::size_t>(2+2*i)]);
    const int hi=*std::ranges::max_element(rgb),lo=*std::ranges::min_element(rgb),d=hi-lo,sum=hi+lo,den=255-std::abs(sum-255);
    if(sum<51||d==0||20*d<3*den)return 0;
    int h=hi==rgb[0]?60*(rgb[1]-rgb[2]):hi==rgb[1]?60*(rgb[2]-rgb[0])+120*d:60*(rgb[0]-rgb[1])+240*d;
    if(h<0)h+=360*d;
    return h<20*d||h>=345*d?1:h<45*d?2:h<70*d?3:h<165*d?4:h<195*d?5:h<255*d?6:7;
}
struct Request {unsigned kind=0;std::string_view ids,value,literal;};
struct Class {std::string_view name;Style style;};
struct Scratch {
    std::array<int,65> columns{};
    std::array<Style,65> styles{};
    std::array<Class,64> classes{};
    std::array<Request,256> requests{};
    std::size_t class_count=0,request_count=0,events=0,properties=0;
};
struct Parser {
    std::string_view raw,body;std::size_t at=0;
    Plan& plan;Scratch& scratch;Ledger& ledger;
    std::string_view theme;
    std::vector<std::string> metadata,slots,declarations,losses;
    void event(){require(++scratch.events<=1024);ledger.pay(16,"source event");}
    void skip(){while(at<body.size()&&(body[at]==' '||body[at]=='\t'||body[at]=='\r'||body[at]=='\n'||body[at]==';'))++at;}
    static bool id_char(char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='.';}
    std::string_view id(){const auto begin=at;while(at<body.size()&&id_char(body[at]))++at;require(at>begin&&at-begin<=128);return body.substr(begin,at-begin);}
    int number(){const auto v=id();int result=0;const auto [end,error]=std::from_chars(v.data(),v.data()+v.size(),result);require(error==std::errc{}&&end==v.data()+v.size()&&result>0&&result<=64);return result;}
    int find(std::string_view name){for(std::size_t i=1;i<plan.entities.size();++i){const auto& id=plan.entities[i].id;ledger.pay(1+std::min(id.size(),name.size()+3),"identity compare");if(id.starts_with("id:")&&std::string_view(id).substr(3)==name)return static_cast<int>(i);}return -1;}
    void notice(std::vector<std::string>& list,std::string text){ledger.product(text.size()+1,2,"notice string");append(list,std::move(text),512,ledger);}
    int add(Entity e,int parent){require(plan.entities.size()<=64);e.parent=parent;const int n=static_cast<int>(plan.entities.size());append(plan.entities,std::move(e),65,ledger);append(plan.entities[static_cast<std::size_t>(parent)].children,Placement{n,0,0,plan.entities.back().span},64,ledger);scratch.columns[static_cast<std::size_t>(n)]=-1;return n;}
    std::string_view quoted(){require(at<body.size()&&body[at]=='"');const auto first=++at;while(at<body.size()&&body[at]!='"'){require(body[at]!='\n'&&body[at]!='\r');++at;}require(at<body.size());auto result=body.substr(first,at-first);++at;return result;}
    int node(int parent,bool group=false){
        skip();const auto name=id();std::string_view kind="rectangle",raw_caption=name;bool explicit_type=false,explicit_caption=false;unsigned heads=0;int span=1;
        skip_horizontal();
        if(at<body.size()&&(body[at]=='['||body[at]=='('||body[at]=='{'||body[at]=='>'||body.substr(at).starts_with("<["))) {
            const auto open_start=at;
            for(const auto prefix:{std::string_view("<["),std::string_view("[["),std::string_view("(("),std::string_view("{{"),std::string_view("(["),std::string_view("[/"),std::string_view("["),std::string_view("("),std::string_view("{"),std::string_view(">")})if(body.substr(at).starts_with(prefix)){at+=prefix.size();break;}
            const auto opening=body.substr(open_start,at-open_start);raw_caption=quoted();explicit_caption=true;
            std::string_view close;
            for(const auto suffix:{std::string_view("]]"),std::string_view("))"),std::string_view("}}"),std::string_view("])"),std::string_view("/]"),std::string_view("\\]"),std::string_view("]>"),std::string_view("]"),std::string_view(")"),std::string_view("}")})if(body.substr(at).starts_with(suffix)){close=suffix;at+=suffix.size();break;}
            require(!close.empty());ledger.pay(opening.size()+close.size()+16,"shape resolution");const auto syntax=std::string(opening)+std::string(close);
            if(syntax=="[]")kind="rectangle";
            else if(syntax=="()")kind="rounded_rectangle";
            else if(syntax=="(())")kind="circle";
            else if(syntax=="{}")kind="diamond";
            else if(syntax=="{{}}")kind="hexagon";
            else if(syntax=="([])")kind="stadium";
            else if(syntax=="[[]]")kind="subroutine";
            else if(syntax=="[//]")kind="lean_right";
            else if(syntax=="[/\\]")kind="trapezoid";
            else if(syntax==">")throw Rejected{};
            else if(syntax==">]")kind="rect_left_inv_arrow";
            else if(syntax=="[/]")kind="rectangle";
            else if(syntax=="<[]>"){
                kind="block_arrow";skip_horizontal();require(at<body.size()&&body[at++]=='(');
                do{skip_horizontal();if(at<body.size()&&body[at]==','){++at;skip_horizontal();}const auto d=id();heads|=d=="right"?1u:d=="down"?2u:d=="left"?4u:d=="up"?8u:d=="x"?5u:d=="y"?10u:0u;require(d=="right"||d=="down"||d=="left"||d=="up"||d=="x"||d=="y");skip_horizontal();}while(at<body.size()&&body[at]!=')');
                require(at<body.size()&&body[at++]==')');
            }else throw Rejected{};
            explicit_type=syntax!="[/]";
        }
        skip_horizontal();if(at<body.size()&&body[at]==':'){++at;span=number();}
        const int found=find(name);
        if(found>=0){auto& e=plan.entities[static_cast<std::size_t>(found)];require(!group);if(e.group){require(!explicit_type&&!explicit_caption&&span==1);return found;}if(explicit_type)e.shape=own(kind,ledger);if(explicit_caption&&raw_caption!=name)e.rows=caption(raw_caption,ledger);return found;}
        if(kind=="block_arrow")require(heads==1||heads==2||heads==4||heads==8||heads==5||heads==10||heads==15);
        Entity e;e.id=join(ledger,{"id:",name});e.shape=own(group?"group":kind,ledger);e.group=group;e.fill=group?6:1;e.heads=heads;e.span=span;
        if(group)require(!explicit_caption);else e.rows=caption(raw_caption,ledger);
        return add(std::move(e),parent);
    }
    void skip_horizontal(){while(at<body.size()&&(body[at]==' '||body[at]=='\t'))++at;}
    Style properties(std::string_view value){
        Style result;value=trim(value);if(value.ends_with(';'))value.remove_suffix(1);
        while(!value.empty()){
            const auto comma=value.find(',');const auto atom=trim(value.substr(0,comma));const auto colon=atom.find(':');require(colon!=std::string_view::npos);const auto key=trim(atom.substr(0,colon)),literal=trim(atom.substr(colon+1));
            require(++scratch.properties<=256);ledger.pay(16,"style atom");unsigned n=key=="fill"?0:key=="stroke"?1:key=="color"?2:key=="stroke-width"?3:4;require(n<4&&!literal.empty());
            unsigned hue=0;if(n==3)require(literal=="2px"||literal=="4px");else hue=color_hue(literal);result.set(n,{literal,hue});
            if(comma==std::string_view::npos)break;value.remove_prefix(comma+1);require(!trim(value).empty());
        }
        require(result.count>0);return result;
    }
    template<class F> void each_id(std::string_view list,F fn){while(!list.empty()){auto comma=list.find(',');auto name=trim(list.substr(0,comma));require(!name.empty()&&name.size()<=128&&std::ranges::all_of(name,[](char c){return id_char(c)&&c!='.';}));fn(name);if(comma==std::string_view::npos)break;list.remove_prefix(comma+1);require(!trim(list).empty());}}
    void style_line(unsigned kind,std::size_t begin){
        skip_horizontal();const auto targets_begin=at;while(at<body.size()&&body[at]!=' '&&body[at]!='\t'&&body[at]!='\n'&&body[at]!='\r')++at;const auto ids=body.substr(targets_begin,at-targets_begin);skip_horizontal();const auto value_begin=at;while(at<body.size()&&body[at]!='\n'&&body[at]!='\r')++at;const auto value=trim(body.substr(value_begin,at-value_begin));require(!ids.empty()&&!value.empty());
        const auto literal=trim(body.substr(begin,at-begin));notice(declarations,join(ledger,{"Block declaration: ",literal}));
        if(kind==0){require(ids!="default"&&ids.find(',')==std::string_view::npos);auto st=properties(value);each_id(ids,[&](std::string_view name){std::size_t k=0;while(k<scratch.class_count&&scratch.classes[k].name!=name){ledger.pay(1+name.size());++k;}if(k==scratch.class_count){require(k<64);scratch.classes[k].name=name;++scratch.class_count;}for(unsigned x=0;x<st.count;++x){const unsigned key=st.order[x];scratch.classes[k].style.set(key,st.values[key]);}});}
        else {require(scratch.request_count<256);scratch.requests[scratch.request_count++]={kind,ids,value,literal};}
    }
    void edge(int from,int parent){
        skip_horizontal();const auto begin=at;while(at<body.size()&&std::string_view("<>=-.xo").find(body[at])!=std::string_view::npos)++at;auto op=body.substr(begin,at-begin);std::string_view label;
        skip_horizontal();if(op=="--"&&at<body.size()&&body[at]=='"'){label=quoted();skip_horizontal();const auto b=at;while(at<body.size()&&std::string_view("<>=-.xo").find(body[at])!=std::string_view::npos)++at;op=body.substr(b,at-b);}
        require(op=="---"||op=="-->"||op=="--x"||op=="--o"||op=="<-->"||op=="==="||op=="==>"||op=="<==>"||op=="-.-"||op=="-.->"||op=="<-.->");
        const int to=node(parent);require(to!=from);Edge e;e.source=from;e.target=to;e.start=op.front()=='<'?"point":"none";e.end=op.back()=='>'?"point":op.back()=='x'?"cross":op.back()=='o'?"circle":"none";e.bold=op.find('=')!=std::string_view::npos;e.dotted=op.find('.')!=std::string_view::npos;
        const auto rows=caption(label,ledger);require(rows.size()<=1);if(!rows.empty())e.label=own(rows.front(),ledger);append(plan.edges,std::move(e),64,ledger);
    }
    void scope(int parent,int depth,bool top=false){
        require(depth<=16);while(true){skip();if(at==body.size()){require(top);return;}const auto begin=at;
            if(body.substr(at).starts_with("%%")){while(at<body.size()&&body[at]!='\n')++at;continue;}
            auto keyword=id();event();
            if(keyword=="end"){require(!top);return;}
            if(keyword=="columns"){skip_horizontal();scratch.columns[static_cast<std::size_t>(parent)]=number();continue;}
            if(keyword=="classDef"||keyword=="class"||keyword=="style"){style_line(keyword=="classDef"?0:keyword=="class"?1:2,begin);continue;}
            if(keyword=="space"){
                int count=1;if(at<body.size()&&body[at]==':'){++at;count=number();}
                for(int i=0;i<count;++i){Entity e;e.space=true;e.shape="space";e.id=join(ledger,{"@space:",std::to_string(static_cast<std::size_t>(body.data()-raw.data())+begin),":",std::to_string(i)});add(std::move(e),parent);}continue;
            }
            if(keyword=="block"){
                int child;
                if(at<body.size()&&body[at]==':'){++at;child=node(parent,true);}
                else {Entity e;e.group=true;e.shape="group";e.fill=6;e.id=join(ledger,{"@group:",std::to_string(static_cast<std::size_t>(body.data()-raw.data())+begin)});child=add(std::move(e),parent);}
                scope(child,depth+1);continue;
            }
            at=begin;const int from=node(parent);skip_horizontal();
            if(at<body.size()&&(body[at]=='-'||body[at]=='='||body[at]=='<'))edge(from,parent);
        }
    }
    void resolve(){
        constexpr std::array<std::string_view,12> border{"#E879F9","#2DD4BF","#FB923C","#22D3EE","#4ADE80","#A78BFA","#F87171","#FACC15","#818CF8","#A3E635","#38BDF8","#FB7185"};
        constexpr std::array<std::string_view,12> fill{"#FDF4FF","#F0FDFA","#FFF7ED","#ECFEFF","#F0FDF4","#F5F3FF","#FEF2F2","#FEFCE8","#EEF2FF","#F7FEE7","#F0F9FF","#FFF1F2"};
        unsigned composite=0;
        for(std::size_t n=1;n<plan.entities.size();++n)if(plan.entities[n].group){
            const auto index=composite++,slot=index%12;
            if(theme!="redux-color"&&theme!="redux-dark-color")continue;
            auto& style=scratch.styles[n];const auto stroke=border[slot];style.set(1,{stroke,color_hue(stroke)});
            if(theme=="redux-color")style.set(0,{fill[slot],color_hue(fill[slot])});
            notice(slots,join(ledger,{"Block container ",plan.entities[n].id,": source index ",std::to_string(index),", slot ",std::to_string(slot),"/12; border ",hue_name(color_hue(stroke)),"; fill ",theme=="redux-color"?hue_name(color_hue(fill[slot])):std::string_view("no categorical override"),"."}));
        }
        for(unsigned phase:{1u,2u})for(std::size_t r=0;r<scratch.request_count;++r){
            const auto request=scratch.requests[r];if(request.kind!=phase)continue;
            if(phase==1){
                auto names=trim(request.value);if(names.ends_with(';'))names.remove_suffix(1);
                each_id(request.ids,[&](std::string_view target){const int n=find(target);require(n>=0);each_id(names,[&](std::string_view name){std::size_t c=0;while(c<scratch.class_count&&scratch.classes[c].name!=name){ledger.pay(1+name.size());++c;}require(c<scratch.class_count);const auto& st=scratch.classes[c].style;for(unsigned k=0;k<st.count;++k){ledger.pay(16,"class property");const auto key=st.order[k];scratch.styles[static_cast<std::size_t>(n)].set(key,st.values[key]);}});});
            }else{
                const auto st=properties(request.value);each_id(request.ids,[&](std::string_view target){const int n=find(target);require(n>=0);for(unsigned k=0;k<st.count;++k){ledger.pay(16,"direct property");const auto key=st.order[k];scratch.styles[static_cast<std::size_t>(n)].set(key,st.values[key]);}});
            }
        }
        constexpr std::array<std::string_view,4> keys{"fill","stroke","color","stroke-width"};
        for(std::size_t n=1;n<plan.entities.size();++n){
            auto& e=plan.entities[n];const auto& st=scratch.styles[n];if(!st.values[0].literal.empty())e.fill=static_cast<std::uint16_t>(kRoles[st.values[0].hue]);if(!st.values[1].literal.empty())e.stroke=static_cast<std::uint16_t>(kRoles[st.values[1].hue]);e.bold=st.values[3].literal=="4px";
            for(unsigned k=0;k<st.count;++k){const auto key=st.order[k];const auto value=st.values[key];
                const auto prefix=join(ledger,{"Block style ",e.id,": "});
                if(key==3)notice(losses,join(ledger,{prefix,"stroke-width ",value.literal," -> ",e.bold?"bold":"normal","; exact CSS width not reproduced"}));
                else if(key==2)notice(losses,join(ledger,{prefix,"source color ",value.literal," (",hue_name(value.hue,false),") not projected on fill; label uses default readable text."}));
                else notice(losses,join(ledger,{prefix,keys[key]," ",value.literal," -> ",hue_name(value.hue),"; exact RGB not reproduced"}));
            }
            if(!st.values[2].literal.empty()&&!st.values[0].literal.empty())notice(losses,join(ledger,{"Block style ",e.id,": source color not projected on fill; label uses default readable text"}));
        }
        for(std::size_t n=0;n<plan.entities.size();++n)if(plan.entities[n].group){
            auto& group=plan.entities[n];const int columns=scratch.columns[n];int position=0;
            for(auto& child:group.children){ledger.pay(32,"source placement");child.span=plan.entities[static_cast<std::size_t>(child.child)].span;child.col=columns<0?position:position%columns;child.row=columns<0?0:position/columns;position+=columns<0?child.span:std::min(child.span,columns-position%columns);}
        }
        for(const auto* records:{&metadata,&slots,&declarations,&losses})for(const auto& text:*records){append(plan.notes,std::string{},1024,ledger);auto rows=wrap(text,ledger);for(auto& row:rows)append(plan.notes,std::move(row),1024,ledger);}
    }
};
std::string_view outline(const Entity& e,Point q) {
    const auto& b=e.body;const int x=q.x-b.x,y=q.y-b.y;
    if(e.group||e.shape=="rectangle"||e.shape=="rounded_rectangle"||e.shape=="subroutine"){
        const bool left=x==0,right=x==b.w-1,top=y==0,bottom=y==b.h-1;
        if(e.shape=="subroutine"&&(x==2||x==b.w-3))return top?"┬":bottom?"┴":"│";
        const bool round=e.shape=="rounded_rectangle";
        if(top&&left)return round?"╭":"┌";if(top&&right)return round?"╮":"┐";
        if(bottom&&left)return round?"╰":"└";if(bottom&&right)return round?"╯":"┘";
        return top||bottom?"─":"│";
    }
    return "█";
}
unsigned direction(Point a,Point b){return b.x>a.x?1u:b.y>a.y?2u:b.x<a.x?4u:b.y<a.y?8u:0u;}
void glyphs(Plan& p,Ledger& l){
    for(int y=0;y<p.metrics.grid_height;++y)for(int x=0;x<p.metrics.grid_width;++x){
        l.pay(8,"contour glyph");auto& c=p.cells[offset(p,{x,y})];if(c.flags&1)c.glyph=outline(p.entities[c.owner],{x,y});
    }
    constexpr std::array<std::string_view,4> arrows{"▶","▼","◀","▲"};
    for(std::size_t n=0;n<p.routes.size();++n){const auto& r=p.routes[n];const auto& edge=p.edges[n];
        for(std::size_t i=0;i<r.points.size();++i){
            l.pay(8,"route glyph");const auto q=r.points[i];auto& cell=p.cells[offset(p,q)];
            if((cell.flags&1)&&cell.owner>0){const auto& e=p.entities[cell.owner];if(!e.group&&e.shape!="rectangle"&&e.shape!="rounded_rectangle"&&e.shape!="subroutine"){cell.glyph="█";continue;}}
            unsigned bits=0;if(i)bits|=direction(q,r.points[i-1]);if(i+1<r.points.size())bits|=direction(q,r.points[i+1]);
            if(cell.flags&1)for(std::size_t d=0;d<4;++d){l.pay(8,"reciprocal port");const auto step=kStep[d];const Point a{q.x+step.x,q.y+step.y};if(inside(p,a)){const auto& c=p.cells[offset(p,a)];if(c.owner==cell.owner&&(c.flags&1))bits|=1u<<d;}}
            cell.glyph=detail::line_glyph(bits);
            if(edge.dotted&&(cell.flags&1)==0){if(bits==5||bits==1||bits==4)cell.glyph="┄";else if(bits==10||bits==2||bits==8)cell.glyph="┆";}
        }
        for(int end=0;end<2;++end){const auto at=end?r.end_marker:r.start_marker;const auto kind=end?r.end_kind:r.start_kind;const int d=end?r.end_direction:r.start_direction;l.pay(8,"endpoint marker");
            if(kind=="none"||kind.empty())continue;require(inside(p,at));auto& c=p.cells[offset(p,at)];if(kind=="point"){require(d>=0&&d<4);c.glyph=arrows[static_cast<std::size_t>(d)];}else if(kind=="circle")c.glyph="○";else if(kind=="cross")c.glyph="×";else throw Rejected{};
        }
        int x=r.label_box.x;
        for(std::size_t byte=0;byte<r.label.size();){const auto first=byte;const auto length=scalar_length(r.label,byte);const int width=ftxui::string_width(r.label.substr(byte,length));require(width==1||width==2);byte+=length;
            while(byte<r.label.size()){const auto len=scalar_length(r.label,byte);if(ftxui::string_width(r.label.substr(byte,len))!=0)break;byte+=len;}
            for(int slot=0;slot<width;++slot){l.pay(8,"edge caption glyph");p.cells[offset(p,{x++,r.label_box.y})].glyph=slot?std::string_view{}:std::string_view(r.label).substr(first,byte-first);}
        }
    }
}
LayoutPayload serialize(Plan& p,Ledger& l){
    glyphs(p,l);std::size_t bytes=0,cells=p.cells.size(),runs=0;
    for(int y=0;y<p.metrics.grid_height;++y)for(int x=0;x<p.metrics.grid_width;++x){l.pay(8,"glyph precount");const auto& c=p.cells[offset(p,{x,y})];bytes+=c.glyph.size();if(x==0||c.style!=p.cells[offset(p,{x-1,y})].style)++runs;}
    for(const auto& note:p.notes){const int width=ftxui::string_width(note);require(width<=32&&width>=0);bytes+=note.size();cells+=static_cast<std::size_t>(width);if(!note.empty())++runs;}
    // Includes owned output/cache copies, repeated byte scans, FTXUI's byte-
    // capacity glyph vector and geometric string growth for combining tails.
    l.product(bytes,48,"glyph bytes and fresh render scratch");
    l.product(runs,512+8*sizeof(std::string),"fresh styled runs");
    l.product(static_cast<std::size_t>(p.metrics.height),256+4*sizeof(ftxui::Element),"fresh row elements");
    LayoutPayload out;reserve(out.rows,static_cast<std::size_t>(p.metrics.height),l);
    l.product(cells,2*sizeof(std::uint16_t),"retained and returned semantic planes");out.styles.resize(cells);
    std::size_t slot=0;
    for(int y=0;y<p.metrics.grid_height;++y){std::size_t size=0;for(int x=0;x<p.metrics.grid_width;++x)size+=p.cells[offset(p,{x,y})].glyph.size();std::string row;row.reserve(size);
        for(int x=0;x<p.metrics.grid_width;++x){const auto& c=p.cells[offset(p,{x,y})];row.append(c.glyph);out.styles[slot++]=c.style;}
        out.rows.push_back(std::move(row));
    }
    for(const auto& note:p.notes){out.rows.push_back(note);for(int n=0;n<ftxui::string_width(note);++n)out.styles[slot++]=1;}
    require(slot==cells&&out.rows.size()==static_cast<std::size_t>(p.metrics.height));return out;
}
} // namespace block

std::optional<LayoutPayload> block_layout(std::string_view source,Work& work){
    using namespace block;
    try{
        require(!source.empty()&&source.size()<=kSourceLimit);Ledger ledger{work};
        ledger.product(source.size(),12,"Block lexical source scan");ledger.pay(sizeof(Scratch)+sizeof(Plan)+512,"Block fixed scratch");
        Scratch scratch;scratch.columns.fill(-1);Plan plan;reserve(plan.entities,65,ledger);reserve(plan.edges,64,ledger);
        const auto envelope=detail::parse_envelope(source,work);require(envelope.has_value()&&!envelope->title&&envelope->init.empty());
        Parser parser{source,envelope->raw_body,0,plan,scratch,ledger,{}, {},{},{},{}};
        for(const auto& event:envelope->events){ledger.pay(16,"metadata event");if(event.blank)continue;require(!event.comment);
            require((event.top_level&&((event.path=="description"&&!event.namespace_entry)||(event.path=="config"&&event.namespace_entry)))||
                (!event.top_level&&!event.namespace_entry&&(event.path=="theme"||event.path=="look")));
        }
        bool seen_theme=false,seen_look=false;
        for(const auto& event:envelope->config){auto value=trim(event.value);if(value.size()>=2&&((value.front()=='\''&&value.back()=='\'')||(value.front()=='"'&&value.back()=='"')))value=value.substr(1,value.size()-2);
            if(event.path=="theme"){require(!seen_theme);seen_theme=true;require(value=="neo"||value=="neo-dark"||value=="redux"||value=="redux-dark"||value=="redux-color"||value=="redux-dark-color");parser.theme=value;}
            else if(event.path=="look"){require(!seen_look&&value=="neo");seen_look=true;}else throw Rejected{};
            parser.notice(parser.metadata,join(ledger,{"Block metadata: config.",event.path,"=",value,event.path=="theme"?"; browser RGB/brightness not reproduced.":"; SVG gradients/relief not applied."}));
        }
        Entity root;root.id="@root";root.group=true;root.shape="group";plan.entities.push_back(std::move(root));parser.skip();
        if(parser.body.substr(parser.at).starts_with("block-beta"))parser.at+=10;
        else if(parser.body.substr(parser.at).starts_with("block"))parser.at+=5;
        else throw Rejected{};
        require(parser.at==parser.body.size()||std::string_view(" \t\r\n;").find(parser.body[parser.at])!=std::string_view::npos);
        parser.scope(0,0,true);parser.resolve();require(plan.entities.size()>1);
        measure(plan,0,ledger);place(plan,0,{0,0,plan.entities[0].min_w,plan.entities[0].min_h},ledger);
        plan.metrics.grid_width=plan.entities[0].min_w;plan.metrics.grid_height=plan.entities[0].min_h;
        plan.metrics.height=plan.metrics.grid_height+static_cast<int>(plan.notes.size());require(plan.metrics.height<=kHeightLimit);
        plan.metrics.area=static_cast<std::size_t>(plan.metrics.grid_width)*static_cast<std::size_t>(plan.metrics.grid_height);
        ledger.product(plan.metrics.area,sizeof(block::Cell),"Block raster");plan.cells.resize(plan.metrics.area);paint(plan,ledger);
        reserve(plan.routes,plan.edges.size(),ledger);route_all(plan,ledger);return serialize(plan,ledger);
    }catch(const Rejected&){return {};}
}
} // namespace loom::ui::mermaid::detail
