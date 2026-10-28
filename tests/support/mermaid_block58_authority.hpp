// Independent complete-original Block58 test authority; no production includes,
// file IO, captured pixels, or reports participate in expectation construction.
// Reuses reviewed preflight semantic math and contour serialization, then adds
// explicit checks below. The worksheet's false approval flags remain unchanged.
#pragma once
#include <ftxui/screen/string.hpp>
#include "mermaid_block58_boundary_authority.hpp"

namespace loom::test::mermaid_block58::authority {
using Json = JsonVal;
using projection::Point;
using projection::Box;
using projection::require;
inline constexpr std::string_view kReviewedDesignSha = "a0f2976fd5408ede363dd2f8307a282559a887d8a9df4734f91a731d532cc54d";
// Includes the B2 review note that followed the reviewed substantive snapshot.
inline constexpr std::string_view kDesignFileSha = "4df4b2242f959ae1379b965addbc2b599d51654b6276cadf227e3c641360304c";
inline constexpr std::string_view kAuthorityReviewStatus = "B3_REVIEW_APPROVED";
inline constexpr std::string_view kProjectionHelperSha = "bbcb680aa8cf29652a1bfdc7275b4c16e76da52c4ca2a2eed2acb9b6ca4d8d4d";
inline constexpr std::string_view kContourHelperSha = "1acab0050d8942520721ea91efbd42760ba930c646328fbfaa6cfe5d569e14e6";
inline constexpr std::string_view kAdaptation = "block58_complete_original_authority/v1";
inline constexpr std::string_view kLimitations =
    "Frozen ASCII captions only; general Unicode/wrapping not exercised. "
    "Finite route candidate ordering/label search and contour serialization reuse reviewed test-private helpers; search minimality is not independently proved. "
    "geometry, masks, scheduling, captions, style atoms and full planes are additionally checked. "
    "No production prepayment proof, collision runtime fixture, cache copy/assignment/eviction plane proof, Official admission or foreign RGB equivalence.";
inline constexpr int kDrawingCredit = 0;
struct Pixel {
    std::string glyph = " ";
    std::uint16_t style = 0;
    bool operator==(const Pixel&) const = default;
};
struct Expected {
    projection::Plan geometry;
    std::vector<std::vector<Pixel>> rows;
    std::string path;
    int global = 0;
};
// Captures own concrete colors, never role guesses inferred from observations.
struct ObservedPixel {
    std::string glyph, foreground, background;
    bool bold = false;
    std::uint16_t other_attributes = 0;
    std::uint8_t hyperlink = 0;
    bool operator==(const ObservedPixel&) const = default;
};
struct Surface {
    int width=0,height=0;
    std::vector<ObservedPixel> cells;
    bool operator==(const Surface&) const = default;
};
struct Palette {
    std::array<std::string,7> foreground,background;
};
inline void verify_design_bytes(std::string_view bytes) {
    require(bytes.size()<=65536 && loom::utils::crypto::sha256(bytes)==kDesignFileSha,
            "source: Block58 approved design file pin");
}
inline bool same_box(Box a,Box b) {return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;}
inline int num(Json j) {return projection::number(j);}
inline std::string str(Json j) {return projection::text(j);}
inline std::vector<std::string> caption_rows(Json caption,OracleWork& work) {
    const auto raw=str(caption.get("raw_caption"));
    require(raw.size()<=1024,"source: caption raw bound");work.spend(raw.size()*16+256);
    std::string decoded;
    for(std::size_t i=0;i<raw.size();) {
        if(raw[i]=='&') {
            if(raw.substr(i,4)=="&lt;"){decoded+='<';i+=4;}
            else if(raw.substr(i,4)=="&gt;"){decoded+='>';i+=4;}
            else throw std::runtime_error("source: unsupported caption entity");
        } else decoded+=raw[i++];
    }
    require(decoded==caption.get_string("display_caption"),"source: one-pass caption decode");
    require(decoded.size()<=32&&std::ranges::all_of(decoded,[](unsigned char c){return c>=32&&c<=126;}),
            "source: frozen ASCII caption domain, not production allowlist");
    const auto rows=caption.get("caption_rows"),joiners=caption.get("row_joiners");
    require(rows.is_arr()&&joiners.is_arr()&&rows.size()==(decoded.empty()?0u:1u)&&joiners.size()==0,
            "source: frozen complete caption row/joiner domain");
    if(decoded.empty())return {};
    require(rows.at(0).as_str()==decoded&&num(caption.get("display_cells"))==static_cast<int>(decoded.size())&&
        num(caption.get("display_utf8_bytes"))==static_cast<int>(decoded.size()),"source: caption bytes/cells/spaces");
    return {decoded};
}
namespace independent {
// Winding-number test, distinct from the worksheet's even/odd ray test.
inline bool polygon(Point q,std::span<const Point> vertices) {
    int winding=0;
    for(std::size_t i=0;i<vertices.size();++i) {
        const auto a=vertices[i],b=vertices[(i+1)%vertices.size()];
        const std::int64_t side=static_cast<std::int64_t>(b.x-a.x)*(q.y-a.y)-static_cast<std::int64_t>(q.x-a.x)*(b.y-a.y);
        if(side==0&&q.x>=std::min(a.x,b.x)&&q.x<=std::max(a.x,b.x)&&q.y>=std::min(a.y,b.y)&&q.y<=std::max(a.y,b.y))return true;
        if(a.y<=q.y&&b.y>q.y&&side>0)++winding;
        if(a.y>q.y&&b.y<=q.y&&side<0)--winding;
    }
    return winding!=0;
}
inline bool mask(const projection::Entity& e,Point world) {
    if(!e.body.contains(world))return false;
    const int x=world.x-e.body.x,y=world.y-e.body.y,w=e.body.w,h=e.body.h;
    if(e.group||e.shape=="rectangle"||e.shape=="rounded_rectangle"||e.shape=="subroutine")return true;
    if(e.shape=="circle")return static_cast<std::int64_t>(2*x-w+1)*(2*x-w+1)+static_cast<std::int64_t>(2*y-h+1)*(2*y-h+1)<=static_cast<std::int64_t>(w)*w;
    if(e.shape=="diamond")return std::abs(2*x-w+1)+std::abs(2*y-h+1)<=w;
    if(e.shape=="stadium") {
        const int r=(h-1)/2;
        if(x>=r&&x<w-r)return true;
        const int dx=x-(x<r?r:w-1-r),dy=y-r;
        return dx*dx+dy*dy<=r*r;
    }
    if(e.shape=="block_arrow") {
        if(e.core.contains(world))return true;
        const auto c=e.core;const int l=c.x,r=c.x+c.w-1,t=c.y,b=c.y+c.h-1,mx=l+(c.w-1)/2,my=t+(c.h-1)/2;
        const std::array<std::array<Point,3>,4> triangles{
            std::array<Point,3>{Point{r-1,t-1},Point{r+3,my},Point{r-1,b+1}},
            std::array<Point,3>{Point{l-1,b-1},Point{mx,b+3},Point{r+1,b-1}},
            std::array<Point,3>{Point{l+1,t-1},Point{l-3,my},Point{l+1,b+1}},
            std::array<Point,3>{Point{l-1,t+1},Point{mx,t-3},Point{r+1,t+1}}};
        for(std::size_t i=0;i<4;++i)if((e.heads&(1u<<i))&&independent::polygon(world,triangles[i]))return true;
        return false;
    }
    const Point q{x,y};
    if(e.shape=="hexagon"){const std::array<Point,6> v{{{2,0},{w-3,0},{w-1,h/2},{w-3,h-1},{2,h-1},{0,h/2}}};return independent::polygon(q,v);}
    if(e.shape=="lean_right"){const std::array<Point,4> v{{{2,0},{w-1,0},{w-3,h-1},{0,h-1}}};return independent::polygon(q,v);}
    if(e.shape=="trapezoid"){const std::array<Point,4> v{{{2,0},{w-3,0},{w-1,h-1},{0,h-1}}};return independent::polygon(q,v);}
    require(e.shape=="rect_left_inv_arrow","source: independent shape domain");
    const std::array<Point,5> v{{{0,0},{2,h/2},{0,h-1},{w-1,h-1},{w-1,0}}};return independent::polygon(q,v);
}
inline bool border(const projection::Entity& e,Point q) {
    if(!independent::mask(e,q))return false;
    for(const auto d:projection::kStep)if(!independent::mask(e,{q.x+d.x,q.y+d.y}))return true;
    return e.shape=="subroutine"&&(q.x==e.body.x+2||q.x==e.body.x+e.body.w-3);
}
struct Size {int w,h;};
inline Size natural(const projection::Entity& e) {
    int l=0;for(const auto& row:e.rows)l=std::max(l,static_cast<int>(row.size()));
    const int c=static_cast<int>(e.rows.size());
    if(e.space)return {1,5};
    if(e.shape=="circle"||e.shape=="diamond"){const int s=std::max(10,l+2*c+6);return {s+s%2,s+s%2};}
    if(e.shape=="hexagon"||e.shape=="lean_right"||e.shape=="trapezoid"||e.shape=="rect_left_inv_arrow")return {std::max(11,l+8),std::max(7,c+4)};
    if(e.shape=="stadium"){const int h=std::max(7,c+4);return {std::max(h+4,l+8),h};}
    if(e.shape=="block_arrow") {
        const bool hz=(e.heads&5)!=0,vt=(e.heads&10)!=0;
        return {std::max(7,l+4)+std::max((e.heads&4)?3:0,vt?1:0)+std::max((e.heads&1)?3:0,vt?1:0),
            std::max(5,c+2)+std::max((e.heads&8)?3:0,hz?1:0)+std::max((e.heads&2)?3:0,hz?1:0)};
    }
    return {std::max(9,l+(e.shape=="subroutine"?8:4)),std::max(5,c+2)};
}
inline Size measurement(const projection::Plan& p,int id,OracleWork& work,int depth=0) {
    require(depth<=16,"source: independent depth");work.spend(512);
    const auto& e=p.entities.at(static_cast<std::size_t>(id));
    Size size=natural(e);
    if(e.group) {
        int cols=0,rows=0,unit=1;std::array<int,64> heights{};
        for(const auto& child:e.children) {
            require(child.span>0&&child.col>=0&&child.row>=0&&child.col+child.span<=64&&child.row<64,"source: independent grid bounds");
            const auto s=measurement(p,child.child,work,depth+1);
            unit=std::max(unit,(std::max(0,s.w-3*(child.span-1))+child.span-1)/child.span);
            heights[static_cast<std::size_t>(child.row)]=std::max(heights[static_cast<std::size_t>(child.row)],s.h);
            cols=std::max(cols,child.col+child.span);rows=std::max(rows,child.row+1);
        }
        require(cols>0&&rows>0,"source: independent nonempty scope");
        require(e.row_heights.size()==static_cast<std::size_t>(rows),"source: independent row count");
        for(int row=0;row<rows;++row)require(e.row_heights[static_cast<std::size_t>(row)]==heights[static_cast<std::size_t>(row)],"source: independent row maximum");
        const int inset=id?4:2;
        size={cols*unit+3*(cols-1)+inset,std::accumulate(heights.begin(),heights.begin()+rows,0)+3*(rows-1)+inset};
    }
    require(e.min_w==size.w&&e.min_h==size.h,"source: independent intrinsic measurement");return size;
}
inline void placement(const projection::Plan& p,int id,Box expected,OracleWork& work) {
    work.spend(512);const auto& e=p.entities.at(static_cast<std::size_t>(id));
    require(same_box(e.band,expected),"source: independent allocation band");
    Box body=expected;
    if(e.shape=="circle"||e.shape=="diamond"){body.x+=(body.w-e.min_w)/2;body.w=e.min_w;}
    require(same_box(e.body,body),"source: independent fixed/stretched body");
    if(e.group) {
        const int inset=id?2:1;int k=0;
        for(const auto c:e.children)k=std::max(k,c.col+c.span);
        const int available=expected.w-2*inset-3*(k-1);require(available>=k,"source: independent column grant");
        require(e.columns.size()==static_cast<std::size_t>(k),"source: independent column extent");
        for(int col=0;col<k;++col)require(e.columns[static_cast<std::size_t>(col)]==available/k+(col<available%k?1:0),"source: independent quotient/remainder columns");
        const auto start=[&](int col){return expected.x+inset+col*(available/k+3)+std::min(col,available%k);};
        for(const auto c:e.children) {
            int y=expected.y+inset;
            for(int r=0;r<c.row;++r)y+=e.row_heights.at(static_cast<std::size_t>(r))+3;
            const auto& n=p.entities.at(static_cast<std::size_t>(c.child));y+=(e.row_heights.at(static_cast<std::size_t>(c.row))-n.min_h)/2;
            placement(p,c.child,{start(c.col),y,start(c.col+c.span)-start(c.col)-3,n.min_h},work);
        }
    } else if(!e.space) {
        Box core=body;
        if(e.shape=="block_arrow") {
            const bool hz=(e.heads&5)!=0,vt=(e.heads&10)!=0;
            const int l=std::max((e.heads&4)?3:0,vt?1:0),r=std::max((e.heads&1)?3:0,vt?1:0),
                t=std::max((e.heads&8)?3:0,hz?1:0),b=std::max((e.heads&2)?3:0,hz?1:0);
            core={body.x+l,body.y+t,body.w-l-r,body.h-t-b};
        }
        require(same_box(e.core,core),"source: independent arrow core");
        int l=0;for(const auto& row:e.rows)l=std::max(l,static_cast<int>(row.size()));const int c=static_cast<int>(e.rows.size());
        require(same_box(e.caption,{core.x+(core.w-l)/2,core.y+(core.h-c)/2,l,c}),"source: independent caption centering");
    }
}
} // namespace independent

inline void check_semantics(const projection::Plan& p,Json facts,OracleWork& work) {
    const auto groups=facts.get("root_and_groups"),nodes=facts.get("nodes");
    for(const auto key:{"root_and_groups","nodes"}) {
        const auto items=facts.get(key);
        for(std::size_t i=0;i<items.size();++i) {
            const auto item=items.at(i);const auto& e=p.entities.at(static_cast<std::size_t>(projection::index(p,item.get_string("handle"))));
            require(e.rows==caption_rows(item.get("caption"),work),"source: full caption identity");
            if(!e.group) {
                require(e.shape==item.get_string("resolved_display_shape"),"source: full shape identity");
                unsigned bits=0;const auto dirs=item.get("resolved_direction_heads");
                for(std::size_t d=0;d<dirs.size();++d) {
                    const auto s=dirs.at(d).as_str();bits|=s=="right"?1u:s=="down"?2u:s=="left"?4u:s=="up"?8u:0u;
                }
                require(e.heads==bits,"source: retained direction identity");
            }
        }
    }
    require(groups.size()+nodes.size()<=p.entities.size(),"source: complete entity population");
    for(std::size_t i=0;i<groups.size();++i) {
        const auto grid=groups.at(i).get("logical_source_grid"),ps=grid.get("placements");const int columns=num(grid.get("columns"));int pos=0;
        require(ps.size()==p.entities[i].children.size(),"source: complete scope placement count");
        for(std::size_t j=0;j<ps.size();++j) {
            const auto q=ps.at(j);const auto c=p.entities[i].children[j];const auto& e=p.entities.at(static_cast<std::size_t>(c.child));
            require(e.id==q.get_string("handle")&&e.parent==static_cast<int>(i)&&c.span==num(q.get("retained_span"))&&e.span==c.span&&
                c.col==(columns<0?pos:pos%columns)&&c.row==(columns<0?0:pos/columns),"source: independent first-owner source scheduling");
            pos+=columns<0?c.span:std::min(c.span,columns-pos%columns);
        }
    }
    // Reconstruct each layer's final property from the authored ordered atoms.
    const auto styles=facts.get("styles");
    std::set<std::string> styled;
    for(std::size_t i=0;i<styles.size();++i) {
        const auto handle=styles.at(i).get_string("handle");
        require(styled.insert(handle).second&&projection::index(p,handle)>0,"source: unique declared style target");
    }
    for(std::size_t i=1;i<p.entities.size();++i) {
        const auto& e=p.entities[i];if(e.space)continue;
        Json style=Json{};
        for(std::size_t j=0;j<styles.size();++j)if(styles.at(j).get_string("handle")==e.id)style=styles.at(j);
        if(!style.valid()) {
            require(e.group&&e.id.starts_with("@group:")&&e.fill==6&&e.stroke==1&&!e.bold,
                "source: anonymous group base properties");continue;
        }
        int fill=e.group?6:1,stroke=1;bool bold=false;int previous=-1;
        const auto atoms=style.get("ordered_property_atoms");
        for(std::size_t j=0;j<atoms.size();++j) {
            work.spend(256);const auto a=atoms.at(j);const auto layer=a.get_string("layer");
            const int rank=layer=="categorical_palette"?0:layer=="class"?1:layer=="direct"?2:-1;
            require(rank>=previous&&rank>=0,"source: style layer order");previous=rank;
            const auto prop=a.get_string("property");const auto value=a.get("value");
            if(prop=="fill")fill=projection::role(value.get_string("hue_role"));
            else if(prop=="stroke")stroke=projection::role(value.get_string("hue_role"));
            else if(prop=="stroke-width") {
                const auto literal=value.get_string("source_literal");require(literal=="2px"||literal=="4px","source: closed authored width");bold=literal=="4px";
            } else require(prop=="color","source: unknown property, no silent loss");
        }
        require(e.fill==fill&&e.stroke==stroke&&e.bold==bold,"source: per-property palette/class/direct style resolution");
    }
    const auto edges=facts.get("edges");
    require(edges.size()==p.edges.size(),"source: complete edge population");
    for(std::size_t i=0;i<edges.size();++i) {
        const auto f=edges.at(i);const auto& e=p.edges[i];const auto rows=caption_rows(f.get("caption"),work);
        require(p.entities.at(static_cast<std::size_t>(e.source)).id==f.get_string("source")&&
            p.entities.at(static_cast<std::size_t>(e.target)).id==f.get_string("target")&&
            e.start==f.get_string("resolved_marker_start")&&e.end==f.get_string("resolved_marker_end")&&
            e.label==(rows.empty()?std::string{}:rows.front())&&e.bold==(f.get_string("stroke_emphasis")=="bold")&&
            e.dotted==(f.get_string("pattern")=="dotted"),"source: complete edge identity/caption/markers/style");
    }
    std::vector<std::string> notes;
    for(const auto key:{"required_metadata_notices","required_slot_legend","required_source_style_declaration_legend","required_style_loss_notices"}) {
        const auto records=facts.get(key);
        for(std::size_t i=0;i<records.size();++i) {
            notes.emplace_back();const auto rows=records.at(i).get("rows");
            for(std::size_t r=0;r<rows.size();++r)notes.push_back(str(rows.at(r)));
        }
    }
    require(notes==p.notes,"source: complete ordered notices including empty separators");
    const auto size=independent::measurement(p,0,work);
    independent::placement(p,0,{0,0,size.w,size.h},work);
    require(size.w==p.metrics.grid_width&&size.h==p.metrics.grid_height,"source: independent root dimensions");
}

inline void check_geometry(const projection::Plan& p,OracleWork& work) {
    projection::validate(p);
    for(std::size_t i=1;i<p.entities.size();++i) {
        const auto& e=p.entities[i];if(e.space)continue;
        for(int y=e.body.y;y<e.body.y+e.body.h;++y)for(int x=e.body.x;x<e.body.x+e.body.w;++x) {
            work.spend(64);const Point q{x,y};require(independent::mask(e,q)==projection::mask(e,q),"source: independent full analytic mask");
        }
        if(!e.group) {
            for(std::size_t row=0;row<e.rows.size();++row)for(std::size_t col=0;col<e.rows[row].size();++col) {
                const Point q{e.caption.x+static_cast<int>(col),e.caption.y+static_cast<int>(row)};
                require(independent::mask(e,q)&&!independent::border(e,q),"source: full caption interior, including blanks");
            }
            for(std::size_t d=0;d<4;++d) {
                Point q{e.core.x+(e.core.w-1)/2,e.core.y+(e.core.h-1)/2};
                for(int n=0;;++n) {
                    require(n<=1024&&independent::mask(e,q),"source: finite independent port ray");work.spend(64);
                    const Point next{q.x+projection::kStep[d].x,q.y+projection::kStep[d].y};if(!independent::mask(e,next))break;q=next;
                }
                require(q==e.ports[d],"source: independent exact mask port");
            }
        }
    }
    // Independently check simple cell paths, prior-route disjointness, unrelated
    // allocations, exact endpoints and the ancestor crossing reconstruction.
    std::set<std::pair<int,int>> prior;
    for(std::size_t i=0;i<p.routes.size();++i) {
        const auto& route=p.routes[i];std::set<std::pair<int,int>> own;
        for(std::size_t j=0;j<route.points.size();++j) {
            work.spend(256);const auto q=route.points[j];
            require(own.emplace(q.x,q.y).second&&!prior.contains({q.x,q.y}),"source: route self/prior intersection");
            if(j)require(std::abs(q.x-route.points[j-1].x)+std::abs(q.y-route.points[j-1].y)==1,"source: route cardinal continuity");
            for(std::size_t n=1;n<p.entities.size();++n) {
                const auto& e=p.entities[n];if(e.space||!e.band.contains(q))continue;
                if(static_cast<int>(n)==route.source||static_cast<int>(n)==route.target)continue;
                require(e.group&&(projection::ancestor(p,static_cast<int>(n),route.source)||projection::ancestor(p,static_cast<int>(n),route.target)),"source: unrelated allocation traversal");
            }
        }
        prior.insert(own.begin(),own.end());
        static_cast<void>(projection::required_crossings(p,route));
        if(!route.label.empty()) {
            require(route.label_box.w==static_cast<int>(route.label.size())&&route.label_box.h==1,"source: complete horizontal edge caption");
            for(int x=route.label_box.x-1;x<=route.label_box.x+route.label_box.w;++x) {
                const Point q{x,route.label_box.y};require(projection::inside(p,q),"source: edge label side clearance bounds");
                for(const auto& other:p.routes)require(std::ranges::find(other.points,q)==other.points.end(),"source: edge label/route collision");
                for(std::size_t n=1;n<p.entities.size();++n) {
                    const auto& e=p.entities[n];if(e.space)continue;
                    require(e.group?!independent::border(e,q):!e.band.contains(q),"source: edge label/contour/allocation collision");
                }
            }
        }
    }
}

inline Expected derive(const BoundFacts& facts,std::size_t source_bytes,OracleWork& work) {
    Expected out;out.path=facts.path;out.global=facts.global;
    out.geometry=projection::build(facts.terminal_model,source_bytes,work,facts.source_model);
    check_semantics(out.geometry,facts.terminal_model,work);check_geometry(out.geometry,work);
    const auto raster=contours::raster(out.geometry,work);const auto& p=out.geometry;
    work.spend((p.cells.size()+p.metrics.note_cells)*sizeof(Pixel)+raster.bytes*2+65536);
    for(const auto& line:raster.rows) {
        auto glyphs=ftxui::Utf8ToGlyphs(line);std::vector<Pixel> row;row.reserve(glyphs.size());
        for(auto& glyph:glyphs){require(ftxui::string_width(glyph)==1,"protocol: frozen whole one-cell glyph");row.push_back({std::move(glyph),0});}
        out.rows.push_back(std::move(row));
    }
    // Repaint roles from analytic masks, independently of worksheet cell styles.
    for(std::size_t i=1;i<p.entities.size();++i) {
        const auto& e=p.entities[i];if(e.space)continue;
        for(int y=e.body.y;y<e.body.y+e.body.h;++y)for(int x=e.body.x;x<e.body.x+e.body.w;++x) {
            work.spend(64);const Point q{x,y};if(!independent::mask(e,q))continue;
            const bool border=independent::border(e,q);
            const bool rail=e.shape=="subroutine"&&(x==e.body.x+2||x==e.body.x+e.body.w-3)&&y>e.body.y&&y<e.body.y+e.body.h-1;
            const int parent=rail?e.fill:e.parent>0?p.entities.at(static_cast<std::size_t>(e.parent)).fill:0;
            out.rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)].style=static_cast<std::uint16_t>(border?
                e.stroke|(parent<<5)|(e.bold?1024:0):1|(e.fill<<5));
        }
    }
    for(std::size_t i=0;i<p.routes.size();++i) {
        const auto& route=p.routes[i];const auto& edge=p.edges[i];
        const auto style_at=[&](Point q){return static_cast<std::uint16_t>(1|(projection::route_background(p,q)<<5)|(edge.bold?1024:0));};
        for(const auto q:route.points) {
            auto& pixel=out.rows.at(static_cast<std::size_t>(q.y)).at(static_cast<std::size_t>(q.x));
            const auto& cell=p.cells[projection::offset(p,q)];
            bool preserve=false;
            if((cell.flags&1)&&cell.owner>0) {
                const auto& e=p.entities[cell.owner];preserve=!e.group&&e.shape!="rectangle"&&e.shape!="rounded_rectangle"&&e.shape!="subroutine";
            }
            if(!preserve)pixel.style=style_at(q);
        }
        for(int end=0;end<2;++end) {
            const auto kind=end?route.end_kind:route.start_kind;
            if(kind=="point"||kind=="circle"||kind=="cross") {
                const auto q=end?route.end_marker:route.start_marker;
                out.rows.at(static_cast<std::size_t>(q.y)).at(static_cast<std::size_t>(q.x)).style=style_at(q);
            }
        }
        for(std::size_t c=0;c<route.label.size();++c) {
            const Point q{route.label_box.x+static_cast<int>(c),route.label_box.y};
            out.rows.at(static_cast<std::size_t>(q.y)).at(static_cast<std::size_t>(q.x)).style=static_cast<std::uint16_t>(1|(projection::route_background(p,q)<<5));
        }
    }
    for(std::size_t y=static_cast<std::size_t>(p.metrics.grid_height);y<out.rows.size();++y)for(auto& pixel:out.rows[y])pixel.style=1;
    require(out.rows.size()==static_cast<std::size_t>(p.metrics.height),"protocol: complete output rows");
    return out;
}

// Positive literals are always checked before any mutation can qualify.
inline std::size_t boundary_positives(Json root,OracleWork& work) {
    boundary::verify_root(root);const auto fixtures=root.get("fixtures");
    std::set<std::string> ids,types;std::set<unsigned> heads;
    for(std::size_t i=0;i<fixtures.size();++i) {
        const auto f=fixtures.at(i);require(ids.insert(f.get_string("fixture_id")).second,"source: unique boundary positive");
        const auto refs=f.get("source_refs");require(refs.is_arr()&&refs.size()>0,"source: boundary source witnesses");
        for(std::size_t r=0;r<refs.size();++r) {
            const auto ref=refs.at(r);const auto found=std::ranges::find_if(kPins,[&](const Pin& pin){return pin.path==ref.get_string("path");});
            require(found!=kPins.end()&&found->raw_sha==ref.get_string("source_sha256")&&found->source_model_sha==ref.get_string("complete_model_sha256"),"source: boundary frozen source joins");
            types.insert(ref.get_string("source_shape_type"));
        }
        const auto p=boundary::primitive(f,work);const auto raster=contours::raster(p,work);boundary::check(f,p,raster);
        const auto& e=p.entities.at(1);if(e.shape=="block_arrow")heads.insert(e.heads);
        for(int y=0;y<e.body.h;++y)for(int x=0;x<e.body.w;++x){work.spend(64);require(independent::mask(e,{x,y})==projection::mask(e,{x,y}),"source: boundary independent winding/mask");}
    }
    require(types.size()==12&&heads==std::set<unsigned>{1,2,4,5,8,10,15},"source: complete primitive and direction positive domain");return fixtures.size();
}

inline std::vector<std::string> validate(const Surface& actual,const Expected& expected,const Palette& palette,
                                       int origin_x,int origin_y,OracleWork& work) {
    require(actual.width>0&&actual.width<=240&&actual.height>0&&actual.height<=1040&&actual.cells.size()==static_cast<std::size_t>(actual.width*actual.height),"protocol: bounded captured surface");
    std::vector<std::string> errors;
    if(origin_x<0||origin_y<0||origin_y+static_cast<int>(expected.rows.size())>actual.height||origin_x+expected.geometry.metrics.width>actual.width)return {"physical: complete expected plane outside capture"};
    std::size_t differences=0;
    for(std::size_t y=0;y<expected.rows.size();++y)for(std::size_t x=0;x<expected.rows[y].size();++x) {
        work.spend(64);const auto& want=expected.rows[y][x];const auto& got=actual.cells[static_cast<std::size_t>((origin_y+static_cast<int>(y))*actual.width+origin_x)+x];
        const auto fg=static_cast<std::size_t>(want.style&31),bg=static_cast<std::size_t>((want.style>>5)&31);
        require(fg<7&&bg<7&&(want.style&0xf800)==0,"protocol: expected packed roles");
        if(got.glyph!=want.glyph||got.foreground!=palette.foreground[fg]||got.background!=palette.background[bg]||got.bold!=((want.style&1024)!=0)||got.other_attributes!=0||got.hyperlink!=0) {
            if(errors.size()<8)errors.push_back("physical: glyph/foreground/background/bold at "+std::to_string(x)+","+std::to_string(y));++differences;
        }
    }
    if(differences)errors.push_back("physical: total mismatched cells "+std::to_string(differences));
    return errors;
}
struct MutationResult {std::size_t semantic=0,physical=0;};
inline bool resource_error(std::string_view message) {return message.starts_with("resource:")||message.starts_with("planned-work/");}
inline MutationResult exercise(const Surface& good,const Expected& expected,const Palette& palette,int ox,int oy,Json facts,OracleWork& work) {
    require(validate(good,expected,palette,ox,oy,work).empty(),"protocol: good-first complete plane failed");
    MutationResult counts;
    check_semantics(expected.geometry,facts,work);check_geometry(expected.geometry,work);
    const auto semantic=[&](auto mutate) {
        work.spend(expected.geometry.cells.size()*sizeof(projection::Cell)+65536);
        auto changed=expected.geometry;mutate(changed);bool rejected=false;
        try {check_semantics(changed,facts,work);check_geometry(changed,work);}
        catch(const std::exception& error){if(resource_error(error.what()))throw;rejected=true;}
        require(rejected,"protocol: meaningful full-model semantic mutant survived");++counts.semantic;
    };
    const auto leaf=std::ranges::find_if(expected.geometry.entities,[](const auto& e){return !e.group&&!e.space&&!e.rows.empty();});
    require(leaf!=expected.geometry.entities.end(),"protocol: semantic leaf support");
    const auto leaf_index=static_cast<std::size_t>(leaf-expected.geometry.entities.begin());
    semantic([&](auto& p){p.entities[leaf_index].rows[0]+="?";});
    semantic([&](auto& p){p.entities[leaf_index].shape=p.entities[leaf_index].shape=="circle"?"diamond":"circle";});
    semantic([&](auto& p){++p.entities[leaf_index].span;});
    semantic([&](auto& p){p.entities[leaf_index].fill=p.entities[leaf_index].fill==6?1:6;});
    semantic([&](auto& p){p.entities[leaf_index].bold=!p.entities[leaf_index].bold;});
    semantic([&](auto& p){++p.entities[leaf_index].band.x;});
    for(std::size_t i=0;i<expected.geometry.entities.size();++i)if(expected.geometry.entities[i].shape=="block_arrow") {
        semantic([&](auto& p){p.entities[i].heads=p.entities[i].heads==1?4:1;});break;
    }
    if(!expected.geometry.notes.empty())semantic([](auto& p){p.notes.back()+="?";});
    if(!expected.geometry.edges.empty())semantic([](auto& p){p.edges[0].end=p.edges[0].end=="cross"?"circle":"cross";});
    const auto physical=[&](std::size_t x,std::size_t y,auto mutate) {
        work.spend(good.cells.size()*(sizeof(ObservedPixel)+32));auto bad=good;auto& pixel=bad.cells.at(static_cast<std::size_t>((oy+static_cast<int>(y))*good.width+ox)+x);
        const auto before=pixel;mutate(pixel);require(pixel!=before,"protocol: vacuous physical mutant");
        require(!validate(bad,expected,palette,ox,oy,work).empty(),"protocol: physical mutant survived");++counts.physical;
    };
    std::optional<std::pair<std::size_t,std::size_t>> outline,blank,caption,note;
    for(std::size_t y=0;y<expected.rows.size();++y)for(std::size_t x=0;x<expected.rows[y].size();++x) {
        const auto& pixel=expected.rows[y][x];
        if(!outline&&pixel.glyph!=" "&&y<static_cast<std::size_t>(expected.geometry.metrics.grid_height))outline={{x,y}};
        if(!blank&&pixel.glyph==" "&&((pixel.style>>5)&31)!=0)blank={{x,y}};
        if(!note&&y>=static_cast<std::size_t>(expected.geometry.metrics.grid_height)&&pixel.glyph!=" ")note={{x,y}};
    }
    for(const auto& e:expected.geometry.entities)if(!e.group&&!e.space&&!e.rows.empty()&&!caption)caption={{static_cast<std::size_t>(e.caption.x),static_cast<std::size_t>(e.caption.y)}};
    require(outline&&blank&&caption,"protocol: full-source mutation support");
    for(const auto at:{*outline,*blank,*caption}) {
        const auto [x,y]=at;
        physical(x,y,[](ObservedPixel& p){p.glyph=p.glyph=="?"?"!":"?";});
    }
    const auto [bx,by]=*blank;
    physical(bx,by,[](ObservedPixel& p){p.bold=!p.bold;});
    physical(bx,by,[&](ObservedPixel& p){p.background=p.background==palette.background[2]?palette.background[4]:palette.background[2];});
    physical(bx,by,[&](ObservedPixel& p){p.foreground=p.foreground==palette.foreground[2]?palette.foreground[6]:palette.foreground[2];});
    physical(bx,by,[](ObservedPixel& p){p.other_attributes^=1;});
    if(note){const auto [x,y]=*note;physical(x,y,[](ObservedPixel& p){p.glyph="?";});}
    // Ports and route cells are additional geometric positives, not substitutes
    // for the exact full-row comparison above.
    if(!expected.geometry.routes.empty()) {
        const auto q=expected.geometry.routes.front().points[expected.geometry.routes.front().points.size()/2];
        physical(static_cast<std::size_t>(q.x),static_cast<std::size_t>(q.y),[](ObservedPixel& p){p.glyph="?";});
    }
    return counts;
}
} // namespace loom::test::mermaid_block58::authority
