// Closed test-private profiles for the eleven bracket-shape Flow forms.
// Included after mermaid_flow_oracle.hpp declares the shared typed oracle.
#pragma once
namespace loom::test::mermaid_flow {

struct ClassicPort { Point point; int outward = 0; };
struct ClassicPortSet { std::array<ClassicPort,4> values{}; std::size_t size = 0; };
struct ClassicAnchor { Shape shape; int dx = 0; int dy = 0; std::string_view glyph; };

inline bool classic_shape(Shape shape) {
    switch (shape) {
    case Shape::Stadium: case Shape::Circle: case Shape::Subroutine: case Shape::DoubleCircle:
    case Shape::Hexagon: case Shape::Cylinder: case Shape::Asymmetric: case Shape::Trapezoid:
    case Shape::TrapezoidAlt: case Shape::Parallelogram: case Shape::ParallelogramAlt: return true;
    case Shape::Rectangle: case Shape::Rounded: case Shape::Decision: return false;
    }
    return false;
}

inline int classic_min_width(Shape shape) {
    switch (shape) {
    case Shape::DoubleCircle: return 13;
    case Shape::Cylinder: case Shape::Parallelogram: case Shape::ParallelogramAlt: return 11;
    case Shape::Stadium: case Shape::Circle: case Shape::Subroutine: case Shape::Hexagon:
    case Shape::Asymmetric: case Shape::Trapezoid: case Shape::TrapezoidAlt: return 9;
    case Shape::Rectangle: case Shape::Rounded: case Shape::Decision: return 0;
    }
    return 0;
}
inline int classic_height(Shape shape) {
    switch (shape) {
    case Shape::Circle: return 9;
    case Shape::DoubleCircle: return 13;
    case Shape::Cylinder: return 7;
    case Shape::Stadium: case Shape::Subroutine: case Shape::Hexagon: case Shape::Asymmetric:
    case Shape::Trapezoid: case Shape::TrapezoidAlt: case Shape::Parallelogram: case Shape::ParallelogramAlt: return 5;
    case Shape::Rectangle: case Shape::Rounded: case Shape::Decision: return 0;
    }
    return 0;
}
inline bool classic_requires_odd_width(Shape shape) {
    switch (shape) {
    case Shape::Circle: case Shape::DoubleCircle: case Shape::Hexagon: case Shape::Cylinder:
    case Shape::Trapezoid: case Shape::TrapezoidAlt: case Shape::Parallelogram: case Shape::ParallelogramAlt: return true;
    case Shape::Stadium: case Shape::Subroutine: case Shape::Asymmetric:
    case Shape::Rectangle: case Shape::Rounded: case Shape::Decision: return false;
    }
    return false;
}
inline bool classic_dimensions(const ShapeFrame& frame) {
    if (!classic_shape(frame.shape) || frame.left < 0 || frame.top < 0 || frame.right < frame.left || frame.bottom < frame.top) return false;
    const int width=frame.right-frame.left+1;
    return width>=classic_min_width(frame.shape) && frame.bottom-frame.top+1==classic_height(frame.shape) &&
        (!classic_requires_odd_width(frame.shape) || (width%2)==1);
}

inline std::optional<std::string_view> classic_ring_cell(const Rect& frame,Point p) {
    if (p.x<frame.left || p.x>frame.right || p.y<frame.top || p.y>frame.bottom ||
        frame.right-frame.left+1<9 || frame.bottom-frame.top+1<9 ||
        ((frame.right-frame.left+1)%2)==0 || ((frame.bottom-frame.top+1)%2)==0) return {};
    const int local=p.y-frame.top,h=frame.bottom-frame.top;
    if (local==0) {
        if (p.x==frame.left+2) return "╭";
        if (p.x==frame.right-2) return "╮";
        if (p.x>frame.left+2 && p.x<frame.right-2) return "─";
    } else if (local==1) {
        if (p.x==frame.left+1) return "╭";
        if (p.x==frame.right-1) return "╮";
        if (p.x>frame.left+1 && p.x<frame.right-1) return "─";
    } else if (local==h) {
        if (p.x==frame.left+2) return "╰";
        if (p.x==frame.right-2) return "╯";
        if (p.x>frame.left+2 && p.x<frame.right-2) return "─";
    } else if (local==h-1) {
        if (p.x==frame.left+1) return "╰";
        if (p.x==frame.right-1) return "╯";
        if (p.x>frame.left+1 && p.x<frame.right-1) return "─";
    } else {
        if (p.x==frame.left || p.x==frame.right) return "│";
    }
    return {};
}
inline std::optional<std::string_view> classic_box_cell(const ShapeFrame& frame,Point p) {
    if (p.x<frame.left || p.x>frame.right || p.y<frame.top || p.y>frame.bottom) return {};
    if (p.y==frame.top) {
        if (p.x==frame.left) return "┌";
        if (p.x==frame.right) return "┐";
        if (p.x>frame.left && p.x<frame.right) return "─";
    }
    if (p.y==frame.bottom) {
        if (p.x==frame.left) return "└";
        if (p.x==frame.right) return "┘";
        if (p.x>frame.left && p.x<frame.right) return "─";
    }
    if (p.y>frame.top && p.y<frame.bottom && (p.x==frame.left || p.x==frame.right)) return "│";
    return {};
}
inline std::optional<std::string_view> classic_outer_cell(const ShapeFrame& frame,Point p) {
    const int l=frame.left,r=frame.right,t=frame.top,b=frame.bottom,y=p.y,x=p.x;
    if (!classic_dimensions(frame)) return {};
    switch (frame.shape) {
    case Shape::Stadium:
        if (y==t) { if (x==l+1) return "╭"; if (x==r-1) return "╮"; if (x>l+1 && x<r-1) return "─"; }
        if (y==b) { if (x==l+1) return "╰"; if (x==r-1) return "╯"; if (x>l+1 && x<r-1) return "─"; }
        if (y>t && y<b && x==l) return "(";
        if (y>t && y<b && x==r) return ")";
        return {};
    case Shape::Circle: return classic_ring_cell(frame,p);
    case Shape::DoubleCircle: return classic_ring_cell(frame,p);
    case Shape::Subroutine: return classic_box_cell(frame,p);
    case Shape::Hexagon:
        if (y==t) { if (x==l+2) return "╱"; if (x==r-2) return "╲"; if (x>l+2 && x<r-2) return "─"; }
        if (y==t+1) { if (x==l+1) return "╱"; if (x==r-1) return "╲"; }
        if (y==t+2 && (x==l || x==r)) return "│";
        if (y==b-1) { if (x==l+1) return "╲"; if (x==r-1) return "╱"; }
        if (y==b) { if (x==l+2) return "╲"; if (x==r-2) return "╱"; if (x>l+2 && x<r-2) return "─"; }
        return {};
    case Shape::Cylinder:
        if (y==t) { if (x==l+2) return "╭"; if (x==r-2) return "╮"; if (x>l+2 && x<r-2) return "─"; }
        if (y==t+1) { if (x==l+1) return "╭"; if (x==r-1) return "╮"; if (x>l+1 && x<r-1) return "─"; }
        if (y>t+1 && y<b-1 && (x==l || x==r)) return "│";
        if (y==b-1) { if (x==l+1) return "╰"; if (x==r-1) return "╯"; if (x>l+1 && x<r-1) return "─"; }
        if (y==b) { if (x==l+2) return "╰"; if (x==r-2) return "╯"; if (x>l+2 && x<r-2) return "─"; }
        return {};
    case Shape::Asymmetric:
        if (y==t) { if (x==l+2) return "╱"; if (x==r) return "┐"; if (x>l+2 && x<r) return "─"; }
        if (y==t+1 && x==l+1) return "╱";
        if (y==t+2 && x==l) return "◁";
        if (y==b-1 && x==l+1) return "╲";
        if (y==b) { if (x==l+2) return "╲"; if (x==r) return "┘"; if (x>l+2 && x<r) return "─"; }
        if (y>t && y<b && x==r) return "│";
        return {};
    case Shape::Trapezoid:
        if (y==t) { if (x==l+2) return "┌"; if (x==r-2) return "┐"; if (x>l+2 && x<r-2) return "─"; }
        if (y==t+1) { if (x==l+1) return "╱"; if (x==r-1) return "╲"; }
        if (y==t+2 && (x==l+1 || x==r-1)) return "│";
        if (y==b-1 && (x==l || x==r)) return "│";
        if (y==b) { if (x==l) return "└"; if (x==r) return "┘"; if (x>l && x<r) return "─"; }
        return {};
    case Shape::TrapezoidAlt:
        if (y==t) { if (x==l) return "┌"; if (x==r) return "┐"; if (x>l && x<r) return "─"; }
        if (y==t+1 && (x==l || x==r)) return "│";
        if (y==t+2 && (x==l+1 || x==r-1)) return "│";
        if (y==b-1 && (x==l+1 || x==r-1)) return "│";
        if (y==b) { if (x==l+2) return "└"; if (x==r-2) return "┘"; if (x>l+2 && x<r-2) return "─"; }
        return {};
    case Shape::Parallelogram:
        if (y==t) { if (x==l+2) return "┌"; if (x==r) return "┐"; if (x>l+2 && x<r) return "─"; }
        if (y==t+1) { if (x==l+1 || x==r-1) return "╱"; }
        if (y==t+2) { if (x==l+1 || x==r-1) return "│"; }
        if (y==b-1) { if (x==l) return "│"; if (x==r-2) return "│"; }
        if (y==b) { if (x==l) return "└"; if (x==r-2) return "┘"; if (x>l && x<r-2) return "─"; }
        return {};
    case Shape::ParallelogramAlt:
        if (y==t) { if (x==l) return "┌"; if (x==r-2) return "┐"; if (x>l && x<r-2) return "─"; }
        if (y==t+1) { if (x==l || x==r-2) return "╲"; }
        if (y==t+2) { if (x==l+1 || x==r-1) return "│"; }
        if (y==b-1) { if (x==l+1 || x==r-1) return "│"; }
        if (y==b) { if (x==l+2) return "└"; if (x==r) return "┘"; if (x>l+2 && x<r) return "─"; }
        return {};
    case Shape::Rectangle: case Shape::Rounded: case Shape::Decision: return {};
    }
    return {};
}
inline std::optional<std::string_view> classic_decoration_cell(const ShapeFrame& frame,Point p) {
    if (frame.shape==Shape::Subroutine && p.y>frame.top && p.y<frame.bottom &&
        (p.x==frame.left+2 || p.x==frame.right-2)) return "║";
    if (frame.shape==Shape::DoubleCircle) {
        const Rect inner{frame.left+2,frame.top+2,frame.right-2,frame.bottom-2};
        return classic_ring_cell(inner,p);
    }
    return {};
}
inline std::optional<std::string_view> classic_expected_cell(const ShapeFrame& frame,Point p) {
    if (const auto outer=classic_outer_cell(frame,p)) return outer;
    return classic_decoration_cell(frame,p);
}
inline ClassicPortSet classic_declared_ports(const ShapeFrame& frame) {
    ClassicPortSet result;
    if (!classic_dimensions(frame)) return result;
    // Box uses a half-open extent: for an even width, its visual midpoint is
    // the right-middle cell, exactly as production ports() computes it.
    const int x=frame.left+(frame.right-frame.left+1)/2;
    const int y=frame.top+(frame.bottom-frame.top+1)/2;
    if (frame.shape==Shape::Asymmetric) {
        result.values[0]={{frame.left,y},L}; result.values[1]={{frame.right,y},R}; result.size=2; return result;
    }
    if (frame.shape==Shape::Trapezoid || frame.shape==Shape::TrapezoidAlt ||
        frame.shape==Shape::Parallelogram || frame.shape==Shape::ParallelogramAlt) {
        result.values[0]={{x,frame.top},U}; result.values[1]={{x,frame.bottom},D};
        result.values[2]={{frame.left+1,y},L}; result.values[3]={{frame.right-1,y},R}; result.size=4; return result;
    }
    result.values[0]={{x,frame.top},U}; result.values[1]={{x,frame.bottom},D};
    result.values[2]={{frame.left,y},L}; result.values[3]={{frame.right,y},R}; result.size=4;
    return result;
}
inline Shape classic_next_shape(Shape shape) {
    switch (shape) {
    case Shape::Stadium: return Shape::Circle;
    case Shape::Circle: return Shape::Stadium;
    case Shape::Subroutine: return Shape::DoubleCircle;
    case Shape::DoubleCircle: return Shape::Subroutine;
    case Shape::Hexagon: return Shape::Cylinder;
    case Shape::Cylinder: return Shape::Hexagon;
    case Shape::Asymmetric: return Shape::Trapezoid;
    case Shape::Trapezoid: return Shape::TrapezoidAlt;
    case Shape::TrapezoidAlt: return Shape::Parallelogram;
    case Shape::Parallelogram: return Shape::ParallelogramAlt;
    case Shape::ParallelogramAlt: return Shape::Asymmetric;
    case Shape::Rectangle: case Shape::Rounded: case Shape::Decision: return Shape::Stadium;
    }
    return Shape::Stadium;
}
inline std::optional<Point> classic_marker_point(const ShapeFrame& frame) {
    const auto ports=classic_declared_ports(frame);
    for (int y=frame.top;y<=frame.bottom;++y) for (int x=frame.left;x<=frame.right;++x) {
        const Point point{x,y};
        if (!classic_outer_cell(frame,point)) continue;
        bool port=false;
        for (std::size_t n=0;n<ports.size;++n) if (ports.values[n].point==point) port=true;
        if (!port) return point;
    }
    return {};
}
inline bool classic_boundary(const ShapeFrame& frame,Point p) { return classic_outer_cell(frame,p).has_value(); }
inline bool classic_decoration(const ShapeFrame& frame,Point p) { return classic_decoration_cell(frame,p).has_value(); }
inline bool classic_decorative_frame(const ShapeFrame& child,const ShapeFrame& enclosing) {
    return enclosing.shape==Shape::DoubleCircle && child.shape==Shape::Circle &&
        child.left==enclosing.left+2 && child.top==enclosing.top+2 &&
        child.right==enclosing.right-2 && child.bottom==enclosing.bottom-2;
}
inline bool classic_legal_port(const ShapeFrame& frame,Point p,int outward) {
    const auto ports=classic_declared_ports(frame);
    for (std::size_t n=0;n<ports.size;++n) if (ports.values[n].point==p && ports.values[n].outward==outward) return true;
    return false;
}
inline int classic_perimeter_bits(const ShapeFrame& frame,Point p) {
    const int middle=(frame.top+frame.bottom)/2;
    // source_port replaces only these declared horizontal terminals with a
    // light T junction. Their U/D arms are the physical tangent, while the
    // selected trace must still claim the one outward horizontal arm.
    if ((frame.shape==Shape::Stadium && p.y==middle && (p.x==frame.left || p.x==frame.right)) ||
        (frame.shape==Shape::Asymmetric && p==Point{frame.left,middle})) return U|D;
    if (const auto outer=classic_outer_cell(frame,p)) return glyph(*outer).bits;
    if (const auto decoration=classic_decoration_cell(frame,p)) return glyph(*decoration).bits;
    return 0;
}
inline bool classic_accepts_port(const Grid& grid,const ShapeFrame& frame,Point p,int outward) {
    return classic_legal_port(frame,p,outward) && (glyph(grid.at(p)).bits&outward)!=0;
}
inline bool classic_ring_interior(const Rect& frame,Point p) {
    if (p.y<=frame.top || p.y>=frame.bottom) return false;
    const int local=p.y-frame.top,h=frame.bottom-frame.top;
    const int left=(local==1 || local==h-1)?frame.left+1:frame.left;
    const int right=(local==1 || local==h-1)?frame.right-1:frame.right;
    return p.x>left && p.x<right;
}
inline bool classic_interior(const ShapeFrame& frame,Point p) {
    if (!classic_dimensions(frame) || classic_boundary(frame,p) || classic_decoration(frame,p)) return false;
    const int l=frame.left,r=frame.right,t=frame.top,b=frame.bottom,x=p.x,y=p.y;
    if (x<=l || x>=r || y<=t || y>=b) return false;
    switch (frame.shape) {
    case Shape::Stadium: case Shape::Subroutine: return true;
    case Shape::Circle: return classic_ring_interior(frame,p);
    case Shape::DoubleCircle: return classic_ring_interior({l+2,t+2,r-2,b-2},p);
    case Shape::Hexagon: return (y==t+1 || y==b-1) ? x>l+1 && x<r-1 : true;
    case Shape::Trapezoid:
        return (y==t+1 || y==t+2) ? x>l+1 && x<r-1 : x>l && x<r;
    case Shape::TrapezoidAlt:
        return y==t+1 ? x>l && x<r : x>l+1 && x<r-1;
    case Shape::Cylinder: return (y==t+1 || y==b-1) ? x>l+1 && x<r-1 : true;
    case Shape::Asymmetric: return (y==t+1 || y==b-1) ? x>l+1 && x<r : true;
    case Shape::Parallelogram: return (y==t+1 || y==t+2) ? x>l+1 && x<r-1 : x>l && x<r-2;
    case Shape::ParallelogramAlt: return (y==t+1) ? x>l && x<r-2 : x>l+1 && x<r-1;
    case Shape::Rectangle: case Shape::Rounded: case Shape::Decision: return false;
    }
    return false;
}

inline bool classic_exact_frame(const Grid& grid,const ShapeFrame& frame) {
    if (!classic_dimensions(frame) || !grid.has({frame.left,frame.top}) || !grid.has({frame.right,frame.bottom})) return false;
    for (int y=frame.top;y<=frame.bottom;++y) for (int x=frame.left;x<=frame.right;++x) {
        const Point point{x,y}; const auto expected=classic_expected_cell(frame,point);
        if (!expected) continue;
        if (grid.at(point)==*expected) continue;
        const auto ports=classic_declared_ports(frame);
        bool route_junction=false;
        for (std::size_t n=0;n<ports.size;++n)
            if (ports.values[n].point==point && (glyph(grid.at(point)).bits&ports.values[n].outward)!=0) route_junction=true;
        if (!route_junction) return false;
    }
    return true;
}

inline std::vector<ShapeFrame> classic_closed_frames(const Grid& grid,OracleWork& work) {
    constexpr std::array<ClassicAnchor,11> anchors{{
        {Shape::Stadium,1,0,"╭"}, {Shape::Circle,2,0,"╭"}, {Shape::Subroutine,0,0,"┌"},
        {Shape::DoubleCircle,2,0,"╭"}, {Shape::Hexagon,2,0,"╱"}, {Shape::Cylinder,2,0,"╭"},
        {Shape::Asymmetric,2,0,"╱"}, {Shape::Trapezoid,2,0,"┌"}, {Shape::TrapezoidAlt,0,0,"┌"},
        {Shape::Parallelogram,2,0,"┌"}, {Shape::ParallelogramAlt,0,0,"┌"},
    }};
    constexpr std::size_t kAnchorLimit=32u,kMaxWidth=240u,kMaxHeight=13u;
    // Prepay the fixed anchor scan, all bounded width attempts, exact profile
    // checks, and the maximum candidate payload before discovering a frame.
    work.spend(128u*sizeof(ShapeFrame)+8u*sizeof(std::vector<ShapeFrame>)+1024u);
    work.product(grid.cells.size(),anchors.size());
    work.spend(anchors.size()*kAnchorLimit*(kMaxWidth*(kMaxHeight+1u)+kMaxWidth+32u));
    std::array<std::size_t,anchors.size()> found{};
    std::vector<ShapeFrame> result; result.reserve(128u);
    auto append=[&](ShapeFrame frame) {
        if (result.size()>=128u) throw std::runtime_error("Too many candidate classic contours");
        if (std::ranges::none_of(result,[&](const ShapeFrame& other) { return other==frame; })) result.push_back(frame);
    };
    for (int y=0;y<grid.height;++y) for (int x=0;x<grid.width;++x) {
        for (std::size_t a=0;a<anchors.size();++a) {
            const auto& anchor=anchors[a];
            if (grid.at({x,y})!=anchor.glyph) continue;
            if (++found[a]>kAnchorLimit) throw std::runtime_error("Too many candidate classic anchors");
            const int left=x-anchor.dx,top=y-anchor.dy,height=classic_height(anchor.shape);
            if (left<0 || top<0 || top+height>grid.height) continue;
            const int min_width=classic_min_width(anchor.shape);
            for (int width=min_width;left+width<=grid.width;++width) {
                ShapeFrame frame{{left,top,left+width-1,top+height-1},anchor.shape};
                if (classic_dimensions(frame) && classic_exact_frame(grid,frame)) append(frame);
            }
        }
    }
    std::ranges::sort(result,[](const ShapeFrame& a,const ShapeFrame& b) { return a.area()<b.area(); });
    return result;
}

inline int classic_profile_grid_width(Shape shape) {
    int width=std::max({classic_min_width(shape)+4,17,static_cast<int>(kSentinel.size())});
    if (classic_requires_odd_width(shape) && (width%2)==0) ++width;
    return width;
}
inline Grid classic_profile_good_grid(Shape shape) {
    const int width=classic_profile_grid_width(shape),height=classic_height(shape)+4;
    Grid result{width,height,std::vector<std::string>(static_cast<std::size_t>(width*height)," ")};
    ShapeFrame frame{{2,1,width-3,1+classic_height(shape)-1},shape};
    for (int y=frame.top;y<=frame.bottom;++y) for (int x=frame.left;x<=frame.right;++x)
        if (const auto expected=classic_expected_cell(frame,{x,y})) result.put({x,y},std::string(*expected));
    const int label_x=(frame.left+frame.right)/2;
    const int label_y=(frame.top+frame.bottom)/2;
    if (!classic_interior(frame,{label_x,label_y})) throw std::runtime_error("Classic profile lacks a central interior cell");
    result.put({label_x,label_y},"X");
    for (std::size_t n=0;n<kSentinel.size();++n) result.put({static_cast<int>(n),height-1},std::string(1,kSentinel[n]));
    return result;
}
inline ShapeFrame classic_profile_frame(Shape shape) {
    const int width=classic_profile_grid_width(shape);
    return {{2,1,width-3,1+classic_height(shape)-1},shape};
}

} // namespace loom::test::mermaid_flow
