// Independent literal-fixture checks for the B2 test-only Block contract.
// Expected masks/rows are authored separately; this file never generates them.
#pragma once
#include <ftxui/screen/string.hpp>
#include "mermaid_block58_contours.hpp"

namespace loom::test::mermaid_block58::boundary {
using Json=loom::utils::json::JsonVal;
inline constexpr std::string_view kAssetSha="d5c3ce61332082a75737183533d92553cefe4abb5c571afa05c092442244cb07";
inline void verify_bytes(std::string_view bytes) {
    projection::require(bytes.size()<=256*1024&&loom::utils::crypto::sha256(bytes)==kAssetSha,"boundary independently authored asset bytes");
}
inline void verify_root(Json root) {
    const auto authority=root.get("source_authority");
    projection::require(root.get_string("schema")=="loom_block58_boundary_contract/v1"&&root.get_string("authoring_status")=="COMPLETE_POSITIVE_PROPOSAL"&&
        root.get("drawing_credit").is_int()&&root.get("drawing_credit").as_int()==0,"boundary independent proposal schema/credit");
    projection::require(authority.get_string("source_sha256")==kSourceFactsSha&&authority.get_string("terminal_sha256")==kTerminalFactsSha&&
        authority.get_string("source_lock_sha256")==kSourceLock,"boundary frozen authority pins");
    projection::require(root.get("fixtures").is_arr()&&root.get("fixtures").size()==36,"boundary complete fixture population");
}
inline projection::Box box(Json value,bool empty=false) {
    projection::require(value.is_obj(),"boundary box object");
    projection::Box b{projection::number(value.get("x")),projection::number(value.get("y")),
        projection::number(value.get("w")),projection::number(value.get("h"))};
    projection::require(b.x>=0&&b.y>=0&&b.w>=(empty?0:1)&&b.h>=(empty?0:1)&&b.x+b.w<=240&&b.y+b.h<=1024,"boundary box limits");
    return b;
}
inline projection::Plan primitive(Json fixture,OracleWork& oracle) {
    const auto g=fixture.get("geometry");const auto b=box(g.get("body"));
    projection::require(b.x==0&&b.y==0,"boundary normalized origin");
    oracle.spend(4096+static_cast<std::size_t>(b.w*b.h)*64);
    projection::Plan p;p.entities.reserve(2);projection::Entity root;root.id="@root";root.group=true;root.body=root.band=b;p.entities.push_back(std::move(root));
    projection::Entity e;e.id="boundary-node";e.shape=projection::text(g.get("shape"));e.parent=0;e.body=e.band=b;e.core=box(g.get("core"));
    e.caption=box(g.get("caption"),true);e.heads=static_cast<unsigned>(projection::number(g.get("heads")));
    const auto rows=g.get("caption_rows");projection::require(rows.is_arr()&&rows.size()<=32,"boundary caption rows");
    for(std::size_t i=0;i<rows.size();++i)e.rows.push_back(projection::text(rows.at(i)));
    projection::require(e.core.x+e.core.w<=b.w&&e.core.y+e.core.h<=b.h,"boundary core containment");
    p.entities.push_back(std::move(e));p.metrics.width=p.metrics.grid_width=b.w;p.metrics.height=p.metrics.grid_height=b.h;
    p.metrics.area=static_cast<std::size_t>(b.w*b.h);p.metrics.serialized_bytes_upper=3*p.metrics.area;p.cells.resize(p.metrics.area);
    projection::Ledger ledger;projection::paint(p,ledger);return p;
}
inline void check(Json fixture,const projection::Plan& p,const contours::Raster& actual) {
    const auto expected=fixture.get("expected"),rows=expected.get("glyph_rows"),mask=expected.get("mask_rows"),interior=expected.get("inside_rows"),ports=expected.get("ports");
    projection::require(rows.is_arr()&&mask.is_arr()&&rows.size()==static_cast<std::size_t>(p.metrics.grid_height)&&mask.size()==rows.size(),"boundary complete expected height");
    projection::require(actual.rows.size()==rows.size()&&interior.is_arr()&&interior.size()==rows.size(),"boundary actual/interior row count");
    for(std::size_t y=0;y<rows.size();++y) {
        const auto line=rows.at(y).as_str(),bits=mask.at(y).as_str(),inside=interior.at(y).as_str();
        projection::require(rows.at(y).is_str()&&mask.at(y).is_str()&&ftxui::string_width(std::string(line))==p.metrics.grid_width&&bits.size()==static_cast<std::size_t>(p.metrics.grid_width),"boundary expected width");
        projection::require(interior.at(y).is_str()&&inside.size()==bits.size(),"boundary complete interior width");
        projection::require(actual.rows[y]==line,"boundary complete literal glyph mismatch at row "+std::to_string(y)+": actual ["+actual.rows[y]+"] expected ["+std::string(line)+"]");
        for(std::size_t x=0;x<bits.size();++x) {
            projection::require(bits[x]=='0'||bits[x]=='1',"boundary expected binary mask");
            const auto& cell=p.cells[projection::offset(p,{static_cast<int>(x),static_cast<int>(y)})];
            projection::require((cell.owner==1)==(bits[x]=='1'),"boundary complete literal mask mismatch");
            projection::require(inside[x]=='0'||inside[x]=='1',"boundary binary interior");
            projection::require((cell.owner==1&&(cell.flags&1)==0)==(inside[x]=='1'),"boundary complete literal interior mismatch");
        }
    }
    projection::require(ports.is_arr()&&ports.size()>0&&ports.size()<=4,"boundary expected ports");
    unsigned seen=0;
    for(std::size_t i=0;i<ports.size();++i) {
        const auto port=ports.at(i);const int d=projection::number(port.get("direction"));
        projection::require(d>=0&&d<4&&(seen&(1u<<d))==0,"boundary unique port direction");seen|=1u<<d;
        const projection::Point at{projection::number(port.get("x")),projection::number(port.get("y"))};
        projection::require(p.entities.at(1).ports[static_cast<std::size_t>(d)]==at,"boundary source port position");
    }
}
inline std::string replace_glyph(std::string_view line,int column,std::string_view replacement) {
    auto glyphs=ftxui::Utf8ToGlyphs(std::string(line));projection::require(column>=0&&static_cast<std::size_t>(column)<glyphs.size(),"boundary mutation column");
    glyphs[static_cast<std::size_t>(column)]=replacement;std::string out;for(const auto& glyph:glyphs)out+=glyph;return out;
}
} // namespace loom::test::mermaid_block58::boundary
