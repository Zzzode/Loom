// Test-private whole-scene relation oracle and strict Class codec.
// Mechanical extraction from test_mermaid_relations_semantics.cpp; no renderer
// API, fixture catalog, TEST registration or observation/report state belongs here.
#pragma once

#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>

import std;
import loom.serdes.json;

namespace loom::test::mermaid_relations {
namespace json = loom::utils::json;
using json::JsonVal;

inline constexpr int U=1,R=2,D=4,L=8;
inline constexpr std::array<int,4> kBits{U,R,D,L};
struct Point { int x=0,y=0; bool operator==(const Point&) const = default; };
inline constexpr std::array<Point,4> kStep{{{0,-1},{1,0},{0,1},{-1,0}}};
inline Point move(Point p,int d,int amount=1) { return {p.x+kStep[static_cast<std::size_t>(d)].x*amount,p.y+kStep[static_cast<std::size_t>(d)].y*amount}; }
struct Work { std::size_t left=8000000; void spend(std::size_t count=1) { if (count>left) {left=0;throw std::runtime_error("Relation oracle work exhausted");}left-=count; } };
struct Glyph { int bits=0,stroke=0; };
inline Glyph glyph(std::string_view s) {
    constexpr std::array<std::pair<std::string_view,int>,15> light{{{"─",L|R},{"│",U|D},{"┌",R|D},{"┐",L|D},{"└",U|R},{"┘",U|L},{"├",U|R|D},{"┤",U|L|D},{"┬",L|R|D},{"┴",L|R|U},{"┼",L|R|U|D},{"╭",R|D},{"╮",L|D},{"╰",U|R},{"╯",U|L}}};
    for (const auto& [text,bits]:light) if(s==text)return {bits,0};
    if(s=="┄"||s=="┈"||s=="┅"||s=="┉")return {L|R,1};
    if(s=="┆"||s=="┊"||s=="┇"||s=="┋")return {U|D,1};
    if(s=="━")return {L|R,2}; if(s=="┃")return {U|D,2};
    return {};
}
struct Grid {
    int width=0,height=0; std::vector<std::string> cells;
    bool has(Point p)const{return p.x>=0&&p.y>=0&&p.x<width&&p.y<height;}
    int index(Point p)const{return p.y*width+p.x;}
    Point point(int n)const{return {n%width,n/width};}
    std::string_view at(Point p)const{return has(p)?cells[static_cast<std::size_t>(index(p))]:std::string_view{};}
    void put(Point p,std::string value){cells.at(static_cast<std::size_t>(index(p)))=std::move(value);}
    std::string row(int y)const{std::string result;for(int x=0;x<width;++x)result+=at({x,y});return result;}
    bool operator==(const Grid&)const=default;
};

inline constexpr std::string_view kRelationsFollowingToken="AFTER_RELATIONS_SEMANTICS";
// This locator proves physical spelling only. The complete chosen assignment
// below decides whether these cells have a source role or a following role.
inline std::optional<int> following_row(const Grid& grid,Work& work,std::string_view token=kRelationsFollowingToken){
    work.spend(8u*sizeof(std::size_t)+4u*sizeof(std::string_view)+4u*sizeof(std::optional<int>)+64u);
    if(grid.width<=0||grid.width>240||grid.height<=0||grid.height>1040||
       grid.cells.size()!=static_cast<std::size_t>(grid.width)*static_cast<std::size_t>(grid.height)||
       token.empty()||token.size()>240u)
        throw std::runtime_error("Following-row physical protocol bounds");
    for(const unsigned char value:token){work.spend(4u);if(value<0x21||value>0x7e)throw std::runtime_error("Following-row token must be printable non-SPACE ASCII");}
    if(token.size()>static_cast<std::size_t>(grid.width))return std::nullopt;
    int last=-1;
    for(int y=grid.height-1;y>=0&&last<0;--y){
        for(int x=0;x<grid.width;++x){work.spend(4u);if(grid.at({x,y})!=" "){last=y;break;}}
    }
    if(last<0)return std::nullopt;
    int first=0;
    while(first<grid.width){work.spend(4u);if(grid.at({first,last})!=" ")break;++first;}
    if(token.size()>static_cast<std::size_t>(grid.width-first))return std::nullopt;
    for(std::size_t n=0;n<token.size();++n){
        work.spend(4u);const auto cell=grid.at({first+static_cast<int>(n),last});
        if(cell.size()!=1u||cell.front()!=token[n])return std::nullopt;
    }
    for(int x=first+static_cast<int>(token.size());x<grid.width;++x){work.spend(4u);if(grid.at({x,last})!=" ")return std::nullopt;}
    return last;
}

using RelationTextStarts=std::array<std::vector<Point>,256>;
struct Rect {
    int l=0,t=0,r=0,b=0; std::string shape="box";
    bool inside(const Rect& p)const{return l>p.l&&r<p.r&&t>p.t&&b<p.b;}
    bool interior(Point p)const{return p.x>l&&p.x<r&&p.y>t&&p.y<b;}
    bool boundary(Point p)const{return ((p.x==l||p.x==r)&&p.y>=t&&p.y<=b)||((p.y==t||p.y==b)&&p.x>=l&&p.x<=r);}
};
inline std::vector<std::string> lines(std::string_view text){std::vector<std::string> out;while(true){const auto at=text.find('\n');out.emplace_back(text.substr(0,at));if(at==std::string_view::npos)return out;text.remove_prefix(at+1);}}
inline std::string trim(std::string value){while(!value.empty()&&value.back()==' ')value.pop_back();const auto at=value.find_first_not_of(' ');return at==std::string::npos?"":value.substr(at);}
inline std::vector<std::string> member_tokens(std::string_view text){
    std::vector<std::string> out;std::string word;
    auto flush=[&]{if(!word.empty()){out.push_back(std::move(word));word.clear();}};
    for(const unsigned char c:text){
        if(c==' '||c=='\t'){flush();continue;}
        const bool identifier=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c>=0x80;
        if(identifier)word+=static_cast<char>(c);else{flush();out.emplace_back(1,static_cast<char>(c));}
    }flush();return out;
}
struct ErTableAttribute {
    std::string type,name;
    std::vector<std::string> keys;
    std::string comment;
};
struct ErTableSpec {
    std::vector<std::string> header;
    std::vector<ErTableAttribute> attributes;
};
struct ErTableEvidence {
    std::vector<int> columns,row_dividers;
};
inline std::vector<std::string> er_table_errors(const Grid&,const Rect&,const ErTableSpec&,Work&,ErTableEvidence* evidence=nullptr);
struct Row {std::string role;std::vector<std::string> alternatives;};
struct Entity {std::string id,label,shape="box",owner;std::vector<std::vector<std::string>> headers;std::vector<Row> rows;std::optional<ErTableSpec> typed_er=std::nullopt;};
struct Relation {std::string from,to,label,start="none",end="arrow",stroke="solid";int order=0;std::optional<std::string> source_text=std::nullopt,target_text=std::nullopt;};
struct Group {std::string id,label,parent,direction;std::vector<std::string> members;};
struct Note {std::string text,target,position;std::string owner{};};
struct Model {std::string family,direction="TB";std::vector<Entity> entities;std::vector<Relation> relations;std::vector<Group> groups;std::vector<Note> notes;};
// Optional source-bound Class presentation; absent pointer keeps the old oracle.
struct ClassPresentation {std::optional<std::string_view> title;std::optional<bool> hide_empty_members_box;};

// A successful whole-scene assignment supplies only this iteration's physical
// mutation target. It never certifies a changed Grid or Model.
struct ClassCardWitness {
    const Grid* grid=nullptr;const Model* model=nullptr;const ClassPresentation* presentation=nullptr;
    std::optional<Rect> box;std::optional<int> separator;
};
// The new Class role is selected by source-bound strict Note mode, never by
// guessed target placement. Legacy side Notes retain their old matcher below.
inline bool assertion_utf8(std::string_view value);
inline bool class_note_text_domain(std::string_view text,Work& work){
    if(text.size()>256u){work.spend();return false;}
    work.spend(text.size()*8u+64u);
    if(!assertion_utf8(text))return false;
    for(std::size_t at=0;at<text.size();){
        const auto lead=static_cast<unsigned char>(text[at++]);std::uint32_t cp=lead;int extra=0;
        if(lead>=0xc2&&lead<=0xdf){cp=lead&0x1fu;extra=1;}
        else if(lead>=0xe0&&lead<=0xef){cp=lead&0x0fu;extra=2;}
        else if(lead>=0xf0){cp=lead&7u;extra=3;}
        for(int n=0;n<extra;++n)cp=(cp<<6u)|(static_cast<unsigned char>(text[at++])&0x3fu);
        if((cp<32u&&cp!=10u)||(cp>=0x7fu&&cp<=0x9fu)||cp==0x061cu||
           (cp>=0x200bu&&cp<=0x200fu)||(cp>=0x202au&&cp<=0x202eu)||
           (cp>=0x2066u&&cp<=0x2069u)||cp==0xfeffu)return false;
    }return true;
}
inline bool class_note_content(const Grid& grid,const Rect& box,const Note& note,Work& work){
    // Utf8ToGlyphs already includes empty wide-character continuation cells.
    // One cell of padding precedes the fixed text origin left+2.
    work.spend(note.text.size()*(8u*sizeof(std::string)+32u)+512u);
    const auto rows=lines(note.text);
    if(box.shape!="box"||box.b-box.t!=static_cast<int>(rows.size())+2||box.r-box.l<7)return false;
    auto physical=[&](int y,std::string_view text){
        work.spend(text.size()*(4u*sizeof(std::string)+16u)+128u);
        const auto parts=ftxui::Utf8ToGlyphs(std::string(text));
        if(parts.size()>static_cast<std::size_t>(box.r-box.l-3))return false;
        for(int x=box.l+1;x<box.r;++x){work.spend(4u);const auto n=x-box.l-2;
            const auto want=n>=0&&static_cast<std::size_t>(n)<parts.size()?std::string_view(parts[static_cast<std::size_t>(n)]):std::string_view(" ");
            if(grid.at({x,y})!=want)return false;
        }return true;
    };
    if(!physical(box.t+1,"note"))return false;
    for(std::size_t row=0;row<rows.size();++row)if(!physical(box.t+2+static_cast<int>(row),rows[row]))return false;
    return true;
}
inline Model parse_model(JsonVal value,Work& work){
    Model result;result.family=value.get_string("family");result.direction=value.get_string("direction");
    value.get("entities").iter([&](JsonVal n){Entity e;e.id=n.get_string("id");e.label=n.get_string("label");e.shape=n.get_string("shape");e.owner=n.get_string("owner");n.get("headers").iter([&](JsonVal h){std::vector<std::string> words;h.iter([&](JsonVal word){words.emplace_back(word.as_str());});e.headers.push_back(std::move(words));});n.get("rows").iter([&](JsonVal row){Row r;r.role=row.get_string("role");row.get("alternatives").iter([&](JsonVal a){r.alternatives.emplace_back(a.as_str());});e.rows.push_back(std::move(r));});result.entities.push_back(std::move(e));});
    value.get("relations").iter([&](JsonVal e){result.relations.push_back({e.get_string("source"),e.get_string("target"),e.get_string("label"),e.get_string("start"),e.get_string("end"),e.get_string("stroke"),static_cast<int>(e.get_int("order"))});});
    value.get("groups").iter([&](JsonVal g){Group group{g.get_string("id"),g.get_string("label"),g.get_string("parent"),g.get_string("direction"),{}};g.get("members").iter([&](JsonVal n){group.members.emplace_back(n.as_str());});result.groups.push_back(std::move(group));});
    value.get("notes").iter([&](JsonVal n){
        // Only the added owner object/payload is new here. Existing complete
        // Model construction bytes remain prepaid by each finite input owner.
        const auto size=result.notes.size(),capacity=result.notes.capacity();
        if(size>=64u||capacity>128u)throw std::runtime_error("Relation Note storage limit exceeded");
        const auto slots=size==capacity?std::max(capacity*2u,size+1u):0u;
        work.spend((slots+(size==capacity?size:0u)+1u)*sizeof(std::string)+64u);
        const auto owner=n.has("owner")?n.get("owner").as_str():std::string_view{};
        work.spend(owner.size()*4u+16u);
        const bool standalone=result.family=="class"&&n.get("position").as_str()=="standalone"&&n.get("target").is_null();
        result.notes.push_back({n.get_string("text"),standalone?std::string{}:n.get_string("target"),n.get_string("position"),std::string(owner)});
    });return result;
}
inline std::vector<Rect> outlines(const Grid& grid,Work& work){
    std::vector<Rect> found;
    auto add=[&](Rect r){if(found.size()>=128)throw std::runtime_error("Relation outline limit exceeded");found.push_back(std::move(r));};
    auto has=[&](int x,int y,int bits){return (glyph(grid.at({x,y})).bits&bits)==bits;};
    for(int y=0;y<grid.height;++y)for(int x=0;x<grid.width;++x){
        work.spend();const auto cell=grid.at({x,y});
        if(cell=="●"||cell=="◉"||cell=="◎")add({x,y,x,y,cell=="●"?"start":"final"});
        if(cell=="━"&&grid.at({x-1,y})!="━"){int r=x;while(grid.at({r+1,y})=="━"){work.spend();++r;}if(r-x>=2)add({x,y,r,y,"bar"});}
        if(cell=="┃"&&grid.at({x,y-1})!="┃"){int b=y;while(grid.at({x,b+1})=="┃"){work.spend();++b;}if(b-y>=2)add({x,y,x,b,"bar"});}
        if(cell=="◇"){
            for(int right=x+6;right<grid.width;++right){work.spend();if(grid.at({right,y})!="◇")continue;
                bool good=y>=2&&y+2<grid.height&&grid.at({x+1,y-1})=="╱"&&grid.at({x+2,y-2})=="╱"&&grid.at({right-1,y-1})=="╲"&&grid.at({right-2,y-2})=="╲"&&grid.at({x+1,y+1})=="╲"&&grid.at({x+2,y+2})=="╲"&&grid.at({right-1,y+1})=="╱"&&grid.at({right-2,y+2})=="╱";
                for(int col=x+3;good&&col<right-2;++col){work.spend();good=has(col,y-2,L|R)&&has(col,y+2,L|R);}if(good)add({x,y-2,right,y+2,"choice"});
            }
        }
        if(cell!="┌"&&cell!="╭")continue;
        for(int r=x+2;r<grid.width;++r){work.spend();if(!has(r,y,L))break;if(grid.at({r,y})=="┐"||grid.at({r,y})=="╮"){
            for(int b=y+2;b<grid.height;++b){work.spend();if(!has(x,b,U))break;
                if((grid.at({x,b})=="└"||grid.at({x,b})=="╰")&&(grid.at({r,b})=="┘"||grid.at({r,b})=="╯")){
                    bool good=true;for(int col=x+1;good&&col<r;++col){work.spend();good=has(col,b,L|R);}for(int row=y+1;good&&row<b;++row){work.spend();good=has(x,row,U|D)&&has(r,row,U|D);}if(good)add({x,y,r,b,cell=="╭"?"rounded":"box"});
                }
                if(!has(x,b,D))break;
            }
        }if(!has(r,y,R))break;}
    }
    return found;
}
struct Content {std::vector<std::string> text;std::vector<int> rows,separators;};
inline Content content(const Grid& grid,const Rect& box,Work& work){
    Content out;
    for(int y=box.t+1;y<box.b;++y){std::string row;bool separator=true;bool stroke=false;
        for(int x=box.l+1;x<box.r;++x){
            work.spend();const auto c=grid.at({x,y});
            if(glyph(c).bits==(L|R))stroke=true;else separator=false;
            if(box.shape=="choice"&&(c=="╱"||c=="╲"))continue;
            row+=c;
        }
        if(separator&&stroke){out.separators.push_back(y);continue;}
        row=trim(row);if(!row.empty()){out.text.push_back(std::move(row));out.rows.push_back(y);}
    }
    return out;
}
// This predicate runs only inside an already successful header alternative.
// header_end is that alternative's physical last header row, including SPACE.
inline bool class_presentation_content(const Grid& grid,const Rect& box,const Entity& entity,const Content& body,int header_end,const ClassPresentation& presentation,Work& work){
    work.spend(3u*sizeof(int)+sizeof(std::size_t)+sizeof(bool)+8u);
    if(entity.shape!="box"||box.shape!="box"||entity.typed_er||header_end<=box.t||header_end>=box.b)return false;
    std::size_t compartments=0;
    for(std::size_t n=0;n<entity.rows.size();++n){work.spend();if(n==0||entity.rows[n].role!=entity.rows[n-1].role)++compartments;}
    const bool expanded=entity.rows.empty()&&presentation.hide_empty_members_box.has_value()&&!*presentation.hide_empty_members_box;
    const auto expected=expanded?2u:compartments;
    if(body.separators.size()!=expected)return false;
    for(const int row:body.separators){
        work.spend();if(row<=header_end||row>=box.b)return false;
        // Interior strokes alone do not prove a complete attached divider.
        // Existing perimeter ownership still rejects unclaimed outward arms.
        work.spend(2u);
        const int left=glyph(grid.at({box.l,row})).bits,right=glyph(grid.at({box.r,row})).bits;
        if((left&(U|D|R))!=(U|D|R)||(right&(U|D|L))!=(U|D|L))return false;
    }
    if(!entity.rows.empty())return true;
    int between=0,below=0,clear=0;
    for(int y=header_end+1;y<box.b;++y){
        work.spend();
        bool divider=false;
        for(const int row:body.separators){work.spend();divider|=row==y;}
        if(divider)continue;
        for(int col=box.l+1;col<box.r;++col){work.spend();if(grid.at({col,y})!=" ")return false;}
        if(expanded){
            if(y>body.separators[0]&&y<body.separators[1])++between;
            if(y>body.separators[1])++below;
        }else ++clear;
    }
    return expanded?(between>=1&&below>=2):clear>=1;
}
// Exact physical prefix, not a crop or a body-label lookup. The source binder
// owns the borrowed title; all allocations and visits share assertion Work.
inline bool class_presentation_title(const Grid& grid,const ClassPresentation& presentation,Work& work){
    if(!presentation.title)return true;
    const auto title=*presentation.title;
    constexpr std::size_t unit=4u*sizeof(std::string)+16u;
    if(title.size()>(std::numeric_limits<std::size_t>::max()-128u)/unit)throw std::runtime_error("Relation oracle work exhausted");
    work.spend(title.size()*unit+128u);
    if(title.empty()||grid.height<2)return false;
    for(const unsigned char c:title){work.spend();if(c<0x20||c==0x7f)return false;}
    const auto glyphs=ftxui::Utf8ToGlyphs(std::string(title));int x=0;
    for(const auto& value:glyphs){
        work.spend(value.size()+1u);const int width=ftxui::string_width(value);
        if(width<=0||width>grid.width-x||grid.at({x,0})!=value)return false;
        for(int continuation=1;continuation<width;++continuation){work.spend();if(grid.at({x+continuation,0})!="")return false;}
        x+=width;
    }
    for(;x<grid.width;++x){work.spend();if(grid.at({x,0})!=" ")return false;}
    for(int col=0;col<grid.width;++col){work.spend();if(grid.at({col,1})!=" ")return false;}
    return true;
}
// The explicit State row profile keeps the old member-token oracle unchanged.
inline bool assertion_utf8(std::string_view);
inline bool state_description_text(std::string_view value,Work& work){
    work.spend(1u);
    if(value.empty()||value.size()>256u)return false;
    work.spend(value.size()*3u);
    if(value.front()==' '||value.back()==' '||!assertion_utf8(value))return false;
    for(std::size_t n=0;n<value.size();++n){
        const auto c=static_cast<unsigned char>(value[n]);
        if(c<0x20||c==0x7f||c=='<'||c=='>'||c=='\\')return false;
        if(c==0xc2&&n+1u<value.size()){const auto next=static_cast<unsigned char>(value[n+1]);if(next>=0x80&&next<=0x9f)return false;}
        if(c==0xe2&&n+2u<value.size()){const auto second=static_cast<unsigned char>(value[n+1]),third=static_cast<unsigned char>(value[n+2]);if((second==0x80&&third>=0xaa&&third<=0xae)||(second==0x81&&third>=0xa6&&third<=0xa9))return false;}
    }
    return true;
}
inline bool state_description_row_domain(const Entity& entity,Work& work){
    work.spend(sizeof(std::size_t)+4u);
    if(entity.shape!="rounded"||entity.typed_er||entity.rows.empty()||entity.rows.size()>128u||entity.headers.size()!=1u||entity.headers[0].size()!=1u||entity.headers[0][0].size()>256u||entity.label.size()>256u)return false;
    work.spend(entity.headers[0][0].size()+entity.label.size()+1u);
    if(entity.headers[0][0]!=entity.label||!state_description_text(entity.label,work))return false;
    for(const auto& row:entity.rows){if(row.role.size()>32u)return false;work.spend(row.role.size()+sizeof(Row));if(row.role!="state_description"||row.alternatives.size()!=1u||!state_description_text(row.alternatives[0],work))return false;}
    return true;
}
inline bool state_description_physical_line(const Grid& grid,const Rect& box,int y,std::string_view expected,Work& work){
    constexpr std::size_t unit=4u*sizeof(std::string)+16u;
    work.spend(expected.size()*unit+128u);
    const auto glyphs=ftxui::Utf8ToGlyphs(std::string(expected));
    int x=box.l+1;
    while(x<box.r){work.spend();if(grid.at({x,y})!=" ")break;++x;}
    for(std::size_t index=0;index<glyphs.size();++index){
        const auto& value=glyphs[index];
        work.spend(value.size()+1u);const int width=ftxui::string_width(value);
        if(width<=0||width>box.r-x||grid.at({x,y})!=value)return false;
        // Utf8ToGlyphs already owns one empty glyph for each wide continuation.
        // Consume that expected cell once while checking the actual empty cell.
        for(int n=1;n<width;++n){work.spend(2u);if(index+1u>=glyphs.size()||!glyphs[index+1u].empty()||grid.at({x+n,y})!="")return false;++index;}
        x+=width;
    }
    for(;x<box.r;++x){work.spend();if(grid.at({x,y})!=" ")return false;}
    return true;
}
inline bool state_description_content(const Grid& grid,const Rect& box,const Entity& entity,const Content& body,Work& work){
    work.spend(entity.rows.size()+8u);
    if(box.shape!="rounded"||body.text.size()!=1u+entity.rows.size()||body.rows.size()!=body.text.size()||body.separators.size()!=1u)return false;
    work.spend(body.text[0].size()+entity.label.size()+1u);
    if(body.text[0]!=entity.label||!state_description_physical_line(grid,box,body.rows[0],entity.label,work))return false;
    const int divider=body.separators[0];
    if(divider<=body.rows[0]||divider>=body.rows[1])return false;
    // Both attached junctions are required, not merely permitted by perimeter.
    work.spend(2u);
    const auto left=glyph(grid.at({box.l,divider})),right=glyph(grid.at({box.r,divider}));
    if(left.stroke||right.stroke||(left.bits&(U|D|R))!=(U|D|R)||(right.bits&(U|D|L))!=(U|D|L))return false;
    for(int x=box.l+1;x<box.r;++x){work.spend();const auto cell=glyph(grid.at({x,divider}));if(cell.bits!=(L|R)||cell.stroke!=0)return false;}
    for(std::size_t n=0;n<entity.rows.size();++n){
        const auto& expected=entity.rows[n].alternatives[0];work.spend(expected.size()+body.text[n+1].size()+1u);
        if(body.text[n+1]!=expected||!state_description_physical_line(grid,box,body.rows[n+1],expected,work))return false;
    }
    return true;
}
inline bool entity_content(const Grid& grid,const Rect& box,const Entity& e,Work& work,const Content* cached=nullptr,const ClassPresentation* presentation=nullptr,bool state_descriptions=false){
    if(e.shape=="start"||e.shape=="final")return box.shape==e.shape;
    if(e.shape=="fork"||e.shape=="join")return box.shape=="bar";
    if(e.shape!=box.shape)return false;
    if(e.typed_er){
        if(e.shape!="box"||e.typed_er->header.empty()||e.typed_er->attributes.empty())return false;
        if(cached&&(cached->text.empty()||cached->text.front()!=e.typed_er->header.front()))return false;
        return er_table_errors(grid,box,*e.typed_er,work).empty();
    }
    std::optional<Content> computed;if(!cached)computed=content(grid,box,work);const auto& body=cached?*cached:*computed;
    if(state_descriptions&&!e.rows.empty())return state_description_content(grid,box,e,body,work);
    for(const auto& header:e.headers){if(body.text.size()!=header.size()+e.rows.size())continue;bool okay=true;
        for(std::size_t n=0;n<header.size();++n)if(body.text[n]!=header[n])okay=false;
        for(std::size_t n=0;n<e.rows.size();++n){const auto& actual=body.text[header.size()+n];if(std::ranges::none_of(e.rows[n].alternatives,[&](const std::string& a){return member_tokens(actual)==member_tokens(a);}))okay=false;}
        if(!okay)continue;
        for(std::size_t n=0;n<e.rows.size();++n){
            const bool new_compartment=n==0||e.rows[n].role!=e.rows[n-1].role;
            if(new_compartment){const int before=body.rows[header.size()+n-1],after=body.rows[header.size()+n];if(std::ranges::none_of(body.separators,[&](int y){return y>before&&y<after;}))okay=false;}
        }
        if(okay){
            if(!presentation)return true;
            if(!header.empty()&&class_presentation_content(grid,box,e,body,body.rows[header.size()-1],*presentation,work))return true;
        }
    }return false;
}
// A source-authored all-SPACE Class alias owns a real header row. Keep the
// shared trimmed Content protocol unchanged for every other entity/profile.
inline bool class_blank_header_content(const Grid& grid,const Rect& box,const Entity& e,Work& work,const Content* cached=nullptr,const ClassPresentation* presentation=nullptr){
    if(e.shape!="box"||box.shape!="box"||e.typed_er||e.label.empty())return entity_content(grid,box,e,work,cached,presentation);
    for(const char value:e.label){work.spend();if(value!=' ')return entity_content(grid,box,e,work,cached,presentation);}
    std::optional<Content> computed;if(!cached)computed=content(grid,box,work);const auto& body=cached?*cached:*computed;
    auto separator_between=[&](int before,int after){bool found=false;for(const int y:body.separators){work.spend();found|=y>before&&y<after;}return found;};
    auto is_blank=[&](std::string_view text){if(text.empty())return false;for(const char value:text){work.spend();if(value!=' ')return false;}return true;};
    for(const auto& header:e.headers){
        work.spend(header.size()+e.rows.size()+1u);if(header.empty())continue;
        std::size_t blanks=0;for(const auto& text:header)blanks+=is_blank(text);
        if(!blanks||body.text.size()!=header.size()-blanks+e.rows.size())continue;
        bool okay=true;std::size_t text_at=0;int last_header=box.t;
        for(const auto& expected:header){
            if(is_blank(expected)){
                const int upper=text_at<body.rows.size()?body.rows[text_at]:box.b;bool found=false;
                for(int y=last_header+1;y<upper&&!found;++y){
                    work.spend();if(separator_between(box.t,y+1))break;
                    bool exact=static_cast<std::size_t>(box.r-box.l-1)>=expected.size();
                    for(int x=box.l+1;x<box.r;++x){work.spend();exact&=grid.at({x,y})==" ";}
                    if(exact){last_header=y;found=true;}
                }
                if(!found){okay=false;break;}
            }else{
                work.spend(expected.size()+1u);
                if(text_at>=body.text.size()||body.rows[text_at]<=last_header||body.text[text_at]!=expected){okay=false;break;}
                last_header=body.rows[text_at++];
            }
        }
        if(!okay||last_header<=box.t||separator_between(box.t,last_header+1))continue;
        // text_at counts only nonblank headers. last_header remains the physical
        // final header row, including a blank one; no header-count subtraction.
        for(std::size_t n=0;n<e.rows.size();++n){
            const auto& actual=body.text[text_at+n];bool matched=false;
            for(const auto& alternative:e.rows[n].alternatives){
                work.spend((actual.size()+alternative.size())*(sizeof(std::string)*2u+8u)+128u);
                if(member_tokens(actual)==member_tokens(alternative)){matched=true;break;}
            }
            if(!matched){okay=false;break;}
            if(n==0||e.rows[n].role!=e.rows[n-1].role){
                const int before=n?body.rows[text_at+n-1]:last_header,after=body.rows[text_at+n];
                if(!separator_between(before,after)){okay=false;break;}
            }
        }
        if(okay&&(!presentation||class_presentation_content(grid,box,e,body,last_header,*presentation,work)))return true;
    }return false;
}


// Source-authored multiline Class labels use exact physical row ownership.
// The existing trimmed Content protocol remains unchanged.
inline bool class_literal_header_profile(const Entity& entity,Work& work){
    if(entity.shape!="box"||entity.typed_er||entity.headers.empty()||entity.headers.front().size()<2u)return false;
    work.spend(entity.label.size()+1u);if(entity.label.find('\n')==std::string::npos)return false;
    // Every source-authored multiline Class label obeys the same physical
    // centering rule. A mutated Model which drops all edge SPACE must still
    // take this path instead of regaining the legacy trimmed comparison.
    return true;
}
inline bool class_centered_header_row(const Grid& grid,const Rect& box,int y,std::string_view expected,Work& work){
    work.spend(expected.size()*(4u*sizeof(std::string)+32u)+128u);
    const auto glyphs=ftxui::Utf8ToGlyphs(std::string(expected));const int width=ftxui::string_width(std::string(expected)),inner=box.r-box.l-1;
    if(y<=box.t||y>=box.b||width<0||width>inner)return false;
    const int start=box.l+1+(inner-width)/2;int x=box.l+1;
    for(;x<start;++x){work.spend();if(grid.at({x,y})!=" ")return false;}
    for(std::size_t at=0;at<glyphs.size();++at){const auto& value=glyphs[at];work.spend(value.size()+1u);const int cells=ftxui::string_width(value);
        if(value.empty()||cells<=0||cells>box.r-x||grid.at({x,y})!=value)return false;
        for(int n=1;n<cells;++n){work.spend(2u);if(at+1u>=glyphs.size()||!glyphs[at+1u].empty()||grid.at({x+n,y})!="")return false;++at;}
        x+=cells;
    }
    if(x!=start+width)return false;
    for(;x<box.r;++x){work.spend();if(grid.at({x,y})!=" ")return false;}
    return true;
}
inline bool class_literal_header_content(const Grid& grid,const Rect& box,const Entity& entity,Work& work,const Content* cached,const ClassPresentation* presentation){
    if(box.shape!="box")return false;
    std::optional<Content> computed;if(!cached)computed=content(grid,box,work);const auto& body=cached?*cached:*computed;
    for(const auto& header:entity.headers){
        work.spend(header.size()+body.text.size()+body.separators.size()+32u);
        if(header.empty()||header.size()>static_cast<std::size_t>(box.b-box.t-1))continue;
        const int end=box.t+static_cast<int>(header.size());bool exact=true;
        for(const int y:body.separators){work.spend();if(y<=end)exact=false;}
        if(!exact)continue;
        for(std::size_t n=0;n<header.size();++n)if(!class_centered_header_row(grid,box,box.t+1+static_cast<int>(n),header[n],work)){exact=false;break;}
        if(!exact)continue;
        std::size_t bytes=0,alternatives=0,following=0;
        for(const auto& text:header){work.spend();bytes+=text.size();}
        for(std::size_t n=0;n<body.text.size();++n){work.spend();if(body.rows[n]>end){++following;bytes+=body.text[n].size();}}
        for(const auto& row:entity.rows){work.spend();alternatives=std::max(alternatives,row.alternatives.size());for(const auto& text:row.alternatives){work.spend();bytes+=text.size();}}
        // Full replacement Content, vector capacities, string payload/copies,
        // member-token comparison and destruction are paid before allocation.
        const auto count=header.size()+following;
        const auto factor=(alternatives+1u)*(entity.headers.size()+1u)*(4u*sizeof(std::string)+16u);
        if(bytes>work.left/factor)throw std::runtime_error("Class literal header content work exhausted");
        work.spend(bytes*factor+count*(4u*sizeof(std::string)+4u*sizeof(int)+32u)+body.separators.size()*4u*sizeof(int)+4u*sizeof(Content)+256u);
        Content restored;restored.text.reserve(count);restored.rows.reserve(count);restored.separators=body.separators;
        for(std::size_t n=0;n<header.size();++n){restored.text.push_back(header[n]);restored.rows.push_back(box.t+1+static_cast<int>(n));}
        for(std::size_t n=0;n<body.text.size();++n)if(body.rows[n]>end){restored.text.push_back(body.text[n]);restored.rows.push_back(body.rows[n]);}
        // Only header text/physical slots were recovered. Existing complete
        // role, divider, member and presentation predicates decide acceptance.
        if(entity_content(grid,box,entity,work,&restored,presentation))return true;
    }
    return false;
}
inline bool class_complete_content(const Grid& grid,const Rect& box,const Entity& entity,Work& work,const Content* cached,const ClassPresentation* presentation){
    if(class_literal_header_profile(entity,work))return class_literal_header_content(grid,box,entity,work,cached,presentation);
    return !entity.label.empty()&&entity.label.front()==' '?class_blank_header_content(grid,box,entity,work,cached,presentation):entity_content(grid,box,entity,work,cached,presentation);
}

inline std::vector<Point> occurrences(const Grid& grid,std::string_view text,Work& work,const RelationTextStarts* starts=nullptr){
    std::vector<Point> out;const auto parts=ftxui::Utf8ToGlyphs(std::string(text));const auto width=ftxui::string_width(std::string(text));
    if(text.empty())return out;
    if(starts&&!parts.empty()&&!parts.front().empty()){
        const auto& candidates=(*starts)[static_cast<unsigned char>(parts.front().front())];
        // The index only proposes physical starts. Every original glyph and
        // width predicate still decides whether this complete text is present.
        for(const auto start:candidates){
            work.spend();if(start.x+width>grid.width)continue;
            int at=start.x;bool good=true;
            for(const auto& c:parts){work.spend();if(grid.at({at,start.y})!=c){good=false;break;}++at;}
            if(good){if(out.size()>=2048)throw std::runtime_error("Relation text candidate limit exceeded");if(out.size()==out.capacity()){
                    const auto capacity=std::min(std::size_t{2048},std::max(std::size_t{1},out.size()*2u));
                    work.spend(capacity*sizeof(Point)+out.size()*(sizeof(Point)+2u)+8u);out.reserve(capacity);
                }
                work.spend();out.push_back(start);}
        }return out;
    }
    for(int y=0;y<grid.height;++y)for(int x=0;x+width<=grid.width;++x){work.spend();int at=x;bool good=true;for(const auto& c:parts){work.spend();if(grid.at({at,y})!=c){good=false;break;}++at;}if(good){if(out.size()>=2048)throw std::runtime_error("Relation text candidate limit exceeded");out.push_back({x,y});}}return out;
}
struct Label {std::string text;Rect box;std::vector<Point> cells;};
inline std::vector<Label> label_blocks(const Grid& grid,std::string_view text,Work& work,const RelationTextStarts* starts=nullptr){
    std::vector<Label> out;const auto rows=lines(text);if(rows.empty()||rows.front().empty())throw std::runtime_error("Unmapped empty-leading relation label");
    int width=0;for(const auto& row:rows)width=std::max(width,ftxui::string_width(row));
    for(const auto start:occurrences(grid,rows.front(),work,starts))for(const bool centered:{false,true}){
        const int left=start.x-(centered?(width-ftxui::string_width(rows.front()))/2:0);
        if(left<0||left+width>grid.width||start.y+static_cast<int>(rows.size())>grid.height)continue;
        Label label{std::string(text),{left-1,start.y-1,left+width,start.y+static_cast<int>(rows.size()),"label"},{}};bool good=true;
        for(std::size_t row=0;good&&row<rows.size();++row){
            const auto cells=ftxui::Utf8ToGlyphs(rows[row]);const int offset=centered?(width-static_cast<int>(cells.size()))/2:0;
            for(int col=0;col<width;++col){work.spend();const int source=col-offset;const auto expected=source>=0&&source<static_cast<int>(cells.size())?std::string_view(cells[static_cast<std::size_t>(source)]):std::string_view(" ");const Point p{left+col,start.y+static_cast<int>(row)};
                if(grid.at(p)!=expected){good=false;break;}if(source>=0&&source<static_cast<int>(cells.size()))label.cells.push_back(p);
            }
        }
        if(good&&std::ranges::none_of(out,[&](const Label& old){return old.box.l==label.box.l&&old.box.t==label.box.t&&old.box.r==label.box.r&&old.box.b==label.box.b;})){if(out.size()>=512)throw std::runtime_error("Relation label block limit exceeded");out.push_back(std::move(label));}
    }
    return out;
}
struct Port {int entity;Point border,route;int direction;std::string marker;std::vector<Point> marker_cells;};
inline std::optional<Port> port(const Grid& grid,const Rect& box,int entity,Point border,int direction){
    const auto outside=move(border,direction);if(!grid.has(outside))return {};
    const auto cell=grid.at(outside);const auto second=grid.at(move(outside,direction));
    const std::array<std::string_view,4> arrow{"▲","▶","▼","◀"},triangle{"△","▷","▽","◁"};
    std::string marker="none";int count=0;
    if(cell==arrow[static_cast<std::size_t>((direction+2)%4)]){marker="arrow";count=1;}
    else if(cell==triangle[static_cast<std::size_t>((direction+2)%4)]){marker="triangle";count=1;}
    else if(cell=="◆"){marker="filled_diamond";count=1;}
    else if(cell=="◇"){marker="open_diamond";count=1;}
    else {
        const std::string pair=std::string(cell)+std::string(second);
        if(pair=="||"){marker="card:one";count=2;}
        else if(pair=="o|"||pair=="|o"){marker="card:zero-one";count=2;}
        else if(pair=="o{"||pair=="}o"||pair=="{o"||pair=="o}"){marker="card:zero-many";count=2;}
        else if(pair=="|{"||pair=="}|"||pair=="{|"||pair=="|}"){marker="card:one-many";count=2;}
    }
    const auto route=move(outside,direction,count);
    if(!(glyph(grid.at(route)).bits&kBits[static_cast<std::size_t>((direction+2)%4)]))return {};
    const bool control=box.shape=="start"||box.shape=="final"||box.shape=="bar"||box.shape=="choice";
    if(!count&&!control&&!(glyph(grid.at(border)).bits&kBits[static_cast<std::size_t>(direction)]))return {};
    Port result{entity,border,route,direction,marker,{}};for(int n=0;n<count;++n)result.marker_cells.push_back(move(outside,direction,n));return result;
}
inline bool class_endpoint_text_domain(std::string_view text,Work& work){
    work.spend(text.size()+1u);
    return !text.empty()&&text.size()<=256&&text.front()!='\n'&&text.back()!='\n'&&
        std::ranges::all_of(text,[](unsigned char c){return c=='\n'||(c>=0x20&&c<=0x7e);});
}
struct Trace {std::size_t a,b;std::array<int,3> strokes{};std::set<int> labels;std::vector<int> path;bool valid=true;};
inline std::vector<std::string> check_scene(const Grid& grid,const Model& model,const std::vector<Rect>& outlines_,const std::vector<int>& assignment,Work& work,bool self_face_policy=false,const ClassPresentation* presentation=nullptr,std::string_view following_token=kRelationsFollowingToken,std::optional<int>* unclaimed_following_row=nullptr){
    work.spend(4u*sizeof(std::string_view)+4u*sizeof(std::optional<int>)+4u*sizeof(void*)+64u);
    if(presentation&&!class_presentation_title(grid,*presentation,work))return {"Class presentation title prefix differs"};
    std::vector<std::string> errors;const auto total=model.entities.size();
    auto entity=[&](std::size_t n)->const Rect&{return outlines_.at(static_cast<std::size_t>(assignment.at(n)));};
    auto group=[&](std::size_t n)->const Rect&{return outlines_.at(static_cast<std::size_t>(assignment.at(total+n)));};
    work.spend(3u*sizeof(std::size_t)+sizeof(bool)+sizeof(std::vector<std::pair<std::size_t,int>>)+12u*sizeof(void*)+64u);
    const bool class_notes=!model.notes.empty()&&(model.notes.front().position=="standalone"||model.notes.front().position=="linked");
    const auto business_count=total+model.groups.size();
    auto note_box=[&](std::size_t n)->const Rect&{return outlines_.at(static_cast<std::size_t>(assignment.at(business_count+n)));};
    auto entity_index=[&](std::string_view id){const auto it=std::ranges::find_if(model.entities,[&](const Entity& e){return e.id==id;});return it==model.entities.end()?-1:static_cast<int>(it-model.entities.begin());};
    auto group_index=[&](std::string_view id){const auto it=std::ranges::find_if(model.groups,[&](const Group& g){return g.id==id;});return it==model.groups.end()?-1:static_cast<int>(it-model.groups.begin());};
    for(std::size_t n=0;n<total;++n)for(std::size_t g=0;g<model.groups.size();++g){bool expected=false;for(auto owner=model.entities[n].owner;!owner.empty();){const int at=group_index(owner);if(at<0)throw std::runtime_error("Unknown owner");if(at==static_cast<int>(g))expected=true;owner=model.groups[static_cast<std::size_t>(at)].parent;}if(entity(n).inside(group(g))!=expected)errors.emplace_back("Entity/group ownership differs");}
    for(std::size_t g=0;g<model.groups.size();++g){const auto& parent=model.groups[g].parent;if(!parent.empty()){const int at=group_index(parent);if(at<0||!group(g).inside(group(static_cast<std::size_t>(at))))errors.emplace_back("Group nesting differs");}}
    if(class_notes)for(std::size_t n=0;n<model.notes.size();++n)for(std::size_t g=0;g<model.groups.size();++g){
        // Includes empty-owner pairs and one bounded first-error publication.
        work.spend(4u*sizeof(std::size_t)+4u*sizeof(int)+512u);
        bool expected=false;auto owner=std::string_view(model.notes[n].owner);
        for(std::size_t depth=0;!owner.empty();++depth){work.spend(owner.size()+32u);if(depth>=16u)throw std::runtime_error("Class Note owner depth limit exceeded");int at=-1;for(std::size_t i=0;i<model.groups.size();++i){work.spend(model.groups[i].id.size()+owner.size()+4u);if(model.groups[i].id==owner)at=static_cast<int>(i);}if(at<0)throw std::runtime_error("Unknown Class Note owner");if(at==static_cast<int>(g))expected=true;owner=model.groups[static_cast<std::size_t>(at)].parent;}
        if(note_box(n).inside(group(g))!=expected)return {"Class Note/group ownership differs"};
    }
    if(!errors.empty())return errors;
    // Group transitions target their actual closed perimeter. Perimeter-only
    // glyphs cannot be reused as a route; crossing junctions retain only the
    // component normal to the perimeter.
    if(class_notes){
        auto overlap=[](const Rect& a,const Rect& b){return a.l<=b.r&&a.r>=b.l&&a.t<=b.b&&a.b>=b.t;};
        for(std::size_t n=0;n<model.notes.size();++n){
            for(std::size_t e=0;e<total;++e){work.spend(16u);if(overlap(note_box(n),entity(e)))return {"Class Note/Class physical roles overlap"};}
            for(std::size_t m=0;m<n;++m){work.spend(16u);if(overlap(note_box(n),note_box(m)))return {"Class Note/Note physical roles overlap"};}
            for(std::size_t g=0;g<model.groups.size();++g){work.spend(16u);if(!note_box(n).inside(group(g))&&overlap(note_box(n),group(g)))return {"Class Note/Group physical roles overlap"};}
        }
    }
    std::vector<Rect> endpoints;for(std::size_t n=0;n<total;++n)endpoints.push_back(entity(n));for(std::size_t g=0;g<model.groups.size();++g)endpoints.push_back(group(g));
    if(class_notes){work.spend((endpoints.size()+model.notes.size())*sizeof(Rect)+endpoints.size()*(sizeof(Rect)+8u)+128u);endpoints.reserve(endpoints.size()+model.notes.size());for(std::size_t n=0;n<model.notes.size();++n){work.spend(note_box(n).shape.size()*4u+sizeof(Rect)+8u);endpoints.push_back(note_box(n));}}
    std::vector<std::pair<std::size_t,int>> linked_notes;
    if(class_notes){work.spend(model.notes.size()*sizeof(std::pair<std::size_t,int>)+64u);linked_notes.reserve(model.notes.size());for(std::size_t n=0;n<model.notes.size();++n){work.spend();if(model.notes[n].position=="linked"){int target=-1;for(std::size_t e=0;e<model.entities.size();++e){work.spend(model.entities[e].id.size()+model.notes[n].target.size()+4u);if(model.entities[e].id==model.notes[n].target)target=static_cast<int>(e);}if(target<0)throw std::runtime_error("Unknown linked Class Note target");linked_notes.emplace_back(n,target);}}}
    auto endpoint_index=[&](std::string_view id){const int n=entity_index(id);if(n>=0)return n;const int g=group_index(id);return g<0?-1:static_cast<int>(total)+g;};
    auto route_bits=[&](Point p){for(std::size_t n=0;n<total;++n){const auto& b=entity(n);if(b.interior(p)||b.boundary(p))return 0;}if(class_notes)for(std::size_t n=0;n<model.notes.size();++n){work.spend();if(note_box(n).interior(p)||note_box(n).boundary(p))return 0;}int bits=glyph(grid.at(p)).bits;for(std::size_t g=0;g<model.groups.size();++g){const auto& b=group(g);if(!b.boundary(p))continue;if(p.y==b.t||p.y==b.b)bits&=~(L|R);if(p.x==b.l||p.x==b.r)bits&=~(U|D);}return bits;};
    std::vector<Port> ports;
    for(std::size_t n=0;n<endpoints.size();++n){if(class_notes&&n>=business_count&&model.notes[n-business_count].position=="standalone")continue;const auto& box=endpoints[n];for(int y=box.t;y<=box.b;++y)for(int x=box.l;x<=box.r;++x){work.spend();const Point p{x,y};if(!box.boundary(p))continue;for(int d=0;d<4;++d){const auto outside=move(p,d);if(box.interior(outside)||box.boundary(outside))continue;if(class_notes&&n>=business_count)work.spend(2u*sizeof(Port)+4u*sizeof(Point)+128u);auto found=port(grid,box,static_cast<int>(n),p,d);if(!found)continue;if(n>=total&&n<business_count&&found->marker=="none"&&(glyph(grid.at(p)).bits&kBits[static_cast<std::size_t>((d+2)%4)]))continue;if(ports.size()>=2048)throw std::runtime_error("Relation port limit exceeded");if(class_notes){
                const auto size=ports.size(),capacity=ports.capacity(),slots=size==capacity?std::min(std::size_t{2048},std::max(size+1u,capacity*2u)):0u;
                work.spend((slots+(size==capacity?size:0u)+1u)*(sizeof(Port)+8u)+128u);
                if(slots)ports.reserve(slots);
            }ports.push_back(std::move(*found));}}}
    // Matched group captions have a distinct source role. Reserve only those
    // actual cells before business label discovery; group interiors remain
    // available to legitimate route text and structural crossings.
    work.spend(grid.cells.size());
    std::vector<bool> used_label_cells(grid.cells.size());
    if(presentation&&presentation->title){
        for(int y=0;y<2;++y)for(int col=0;col<grid.width;++col){work.spend();used_label_cells[static_cast<std::size_t>(grid.index({col,y}))]=true;}
    }
    if(class_notes)for(std::size_t n=0;n<model.notes.size();++n){const auto& b=note_box(n);for(int y=b.t+1;y<b.b;++y)for(int x=b.l+1;x<b.r;++x){work.spend();used_label_cells[static_cast<std::size_t>(grid.index({x,y}))]=true;}}
    std::vector<Point> group_caption_cells;
    for(std::size_t g=0;g<model.groups.size();++g){const auto& b=group(g);bool found=false;
        // Prepay glyph conversion plus vector growth/copies before recording cells.
        work.spend(model.groups[g].label.size()*(2u*sizeof(std::string)+4u*sizeof(Point)+8u)+64u);
        for(int y=b.t+1;y<b.b&&!found;++y){std::string row;for(int x=b.l+1;x<b.r;++x){work.spend();row+=grid.at({x,y});}if(trim(row).empty())continue;
            if(trim(row)!=model.groups[g].label)throw std::runtime_error("Matched group header changed");
            int x=b.l+1;while(x<b.r&&grid.at({x,y})==" "){work.spend();++x;}
            for(const auto& c:ftxui::Utf8ToGlyphs(model.groups[g].label)){work.spend();if(x>=b.r||grid.at({x,y})!=c)throw std::runtime_error("Matched group header cells changed");const Point at{x++,y};if(class_notes)for(std::size_t n=0;n<model.notes.size();++n){work.spend();if(note_box(n).interior(at)||note_box(n).boundary(at))throw std::runtime_error("Class Note/Group caption roles overlap");}used_label_cells[static_cast<std::size_t>(grid.index(at))]=true;group_caption_cells.push_back(at);}found=true;
        }
    }
    std::vector<Label> labels;std::set<std::string> label_text;bool class_text_roles=false;
    for(const auto& e:model.relations){
        if(!e.label.empty())label_text.insert(e.label);
        if(model.family=="class"&&(e.source_text||e.target_text)){
            work.spend();class_text_roles=true;
            for(const auto* text:{&e.source_text,&e.target_text})if(*text){
                work.spend((*text)->size()+sizeof(std::string)+64u);label_text.insert(**text);
            }
        }
    }
    std::optional<RelationTextStarts> text_starts;
    if(label_text.size()>1){
        // One byte is a conservative discriminator, including multi-byte UTF-8.
        // Keep source-selected buckets only; full glyph comparison is unchanged.
        work.spend(sizeof(RelationTextStarts)+sizeof(std::array<bool,256>)+256u);
        text_starts.emplace();std::array<bool,256> wanted{};
        for(const auto& text:label_text){
            work.spend(text.size()*(2u*sizeof(std::string)+8u)+128u);
            const auto first_row=std::string_view(text).substr(0,text.find('\n'));
            const auto parts=ftxui::Utf8ToGlyphs(std::string(first_row));
            if(!parts.empty()&&!parts.front().empty())wanted[static_cast<unsigned char>(parts.front().front())]=true;
        }
        for(int y=0;y<grid.height;++y)for(int x=0;x<grid.width;++x){
            work.spend(2u);const auto cell=grid.at({x,y});if(cell.empty())continue;
            const auto first=static_cast<unsigned char>(cell.front());if(!wanted[first])continue;
            auto& bucket=(*text_starts)[first];
            if(bucket.size()==bucket.capacity()){
                const auto capacity=std::min(grid.cells.size(),std::max(std::size_t{1},bucket.size()*2u));
                work.spend(capacity*sizeof(Point)+bucket.size()*(sizeof(Point)+2u)+8u);bucket.reserve(capacity);
            }
            work.spend();bucket.push_back({x,y});
        }
    }
    for(const auto& text:label_text)for(auto label:label_blocks(grid,text,work,text_starts?&*text_starts:nullptr)){bool inside=false;for(std::size_t n=0;n<total;++n)for(const auto p:label.cells)if(entity(n).interior(p))inside=true;if(class_notes||!group_caption_cells.empty()||(presentation&&presentation->title))for(const auto p:label.cells){work.spend();if(used_label_cells[static_cast<std::size_t>(grid.index(p))])inside=true;}if(!inside){if(labels.size()>=512)throw std::runtime_error("Relation label limit exceeded");labels.push_back(std::move(label));}}
    std::vector<Trace> traces;
    std::vector<int> parent,bridge,queue;
    if(!ports.empty()){
        if(class_notes){work.spend(grid.cells.size()*(3u*sizeof(int)+4u)+3u*sizeof(std::vector<int>)+64u);queue.reserve(grid.cells.size());}
        work.spend(grid.cells.size()*2);
        parent.assign(grid.cells.size(),-2);bridge.assign(grid.cells.size(),-1);
    }
    for(std::size_t a=0;a<ports.size();++a){
        queue.clear();queue.push_back(grid.index(ports[a].route));parent[static_cast<std::size_t>(queue.front())]=-1;
        for(std::size_t cursor=0;cursor<queue.size();++cursor){work.spend();const auto p=grid.point(queue[cursor]);for(int d=0;d<4;++d){work.spend();const auto next=move(p,d);if(!grid.has(next)||!(route_bits(p)&kBits[static_cast<std::size_t>(d)])||!(route_bits(next)&kBits[static_cast<std::size_t>((d+2)%4)]))continue;const int at=grid.index(next);if(parent[static_cast<std::size_t>(at)]==-2){parent[static_cast<std::size_t>(at)]=queue[cursor];queue.push_back(at);}}
            for(std::size_t l=0;l<labels.size();++l){const auto& b=labels[l].box;if(p.y<=b.t||p.y>=b.b)continue;const bool left=p.x==b.l,right=p.x==b.r;if((!left&&!right)||!(route_bits(p)&(left?R:L)))continue;const Point other{left?b.r:b.l,p.y};if(!grid.has(other)||!(route_bits(other)&(left?L:R)))continue;const int at=grid.index(other);if(parent[static_cast<std::size_t>(at)]==-2){parent[static_cast<std::size_t>(at)]=queue[cursor];bridge[static_cast<std::size_t>(at)]=static_cast<int>(l);queue.push_back(at);}}
        }
        for(std::size_t b=a+1;b<ports.size();++b){work.spend();const auto at=grid.index(ports[b].route);if(parent[static_cast<std::size_t>(at)]==-2)continue;if(class_notes)work.spend(2u*sizeof(Trace)+128u);Trace t{a,b,{},{},{}};for(int n=at;n>=0;n=parent[static_cast<std::size_t>(n)]){work.spend();if(class_notes){
                if(t.path.size()>=grid.cells.size())throw std::runtime_error("Relation trace path limit exceeded");
                const auto size=t.path.size(),capacity=t.path.capacity(),slots=size==capacity?std::min(grid.cells.size(),std::max(size+1u,capacity*2u)):0u;
                work.spend((slots+(size==capacity?size:0u)+1u)*sizeof(int)+8u);if(slots)t.path.reserve(slots);
                if(bridge[static_cast<std::size_t>(n)]>=0)work.spend(128u);
            }t.path.push_back(n);if(bridge[static_cast<std::size_t>(n)]>=0)t.labels.insert(bridge[static_cast<std::size_t>(n)]);}
            auto direction=[](Point from,Point to){if(from.x==to.x&&from.y!=to.y)return to.y<from.y?0:2;if(from.y==to.y&&from.x!=to.x)return to.x<from.x?3:1;throw std::runtime_error("Nonorthogonal route witness");};
            for(std::size_t n=0;n<t.path.size();++n){work.spend();const auto p=grid.point(t.path[n]);const auto previous=n?grid.point(t.path[n-1]):move(p,(ports[b].direction+2)%4);const auto next=n+1<t.path.size()?grid.point(t.path[n+1]):move(p,(ports[a].direction+2)%4);
                const int used=kBits[static_cast<std::size_t>(direction(p,previous))]|kBits[static_cast<std::size_t>(direction(p,next))];const int actual=route_bits(p);
                if((actual&used)!=used||(actual&~used)!=0){t.valid=false;break;}
                bool perimeter=false;for(std::size_t g=0;g<model.groups.size();++g)if(group(g).boundary(p))perimeter=true;
                if(!perimeter&&(used==(L|R)||used==(U|D)))++t.strokes[static_cast<std::size_t>(glyph(grid.at(p)).stroke)];
            }
            // One physical label gap can carry several complete text witnesses:
            // e.g. has/one and has/one/two share the same second-line landing.
            // Preserve those identities without resetting BFS or choosing the
            // longest label. Whole-scene matching still owns every text cell.
            if(t.valid&&t.labels.size()==1){
                std::optional<std::pair<Point,Point>> jump;bool multiple=false;
                for(std::size_t n=1;n<t.path.size();++n){
                    work.spend();const auto p=grid.point(t.path[n-1]),q=grid.point(t.path[n]);
                    if(std::abs(p.x-q.x)+std::abs(p.y-q.y)==1)continue;
                    if(jump){multiple=true;break;}jump=std::pair{p,q};
                }
                if(jump&&!multiple&&jump->first.y==jump->second.y){
                    const int left=std::min(jump->first.x,jump->second.x),right=std::max(jump->first.x,jump->second.x),row=jump->first.y;
                    for(std::size_t l=0;l<labels.size();++l){
                        work.spend();const auto& box=labels[l].box;
                        if(t.labels.contains(static_cast<int>(l))||box.l!=left||box.r!=right||row<=box.t||row>=box.b)continue;
                        if(traces.size()>=4096)throw std::runtime_error("Relation trace limit exceeded");
                        if(class_notes){
                            // Clone path/set and replacement set before copying;
                            // reserve both this alternate and the original t.
                            if(traces.size()+2u>4096u)throw std::runtime_error("Relation trace limit exceeded");
                            work.spend(4u*sizeof(Trace)+2u*sizeof(std::vector<int>)+t.path.size()*(2u*sizeof(int)+4u)+
                                (t.labels.size()+1u)*(8u*sizeof(void*)+2u*sizeof(int)+16u)+256u);
                            if(traces.capacity()-traces.size()<2u){
                                const auto capacity=std::min(std::size_t{4096},std::max(traces.size()+2u,traces.capacity()*2u));
                                work.spend((capacity+2u*traces.size()+1u)*(sizeof(Trace)+8u)+64u);traces.reserve(capacity);
                            }
                        }
                        work.spend(t.path.size()+1);auto alternative=t;alternative.labels={static_cast<int>(l)};traces.push_back(std::move(alternative));
                    }
                }
            }
            // Endpoint roles can share a prefix at any one of several real
            // gaps. BFS fixes physical paths, not which complete source text
            // owns a gap. Enumerate those finite text witnesses on the SAME
            // path; the complete Model and ink assignment choose the owner.
            if(t.valid&&class_text_roles&&t.labels.size()>1){
                work.spend(sizeof(std::array<std::pair<Point,Point>,3>)+sizeof(std::array<std::vector<int>,3>)+64u);
                std::array<std::pair<Point,Point>,3> gaps{};std::size_t gap_count=0;bool eligible=true;
                for(std::size_t n=1;n<t.path.size();++n){
                    work.spend();const auto first=grid.point(t.path[n-1]),last=grid.point(t.path[n]);
                    if(std::abs(first.x-last.x)+std::abs(first.y-last.y)==1)continue;
                    if(first.y!=last.y||gap_count==gaps.size()){eligible=false;break;}
                    gaps[gap_count++]={first,last};
                }
                if(eligible&&gap_count>1){
                    std::array<std::vector<int>,3> options;
                    for(std::size_t gap=0;gap<gap_count;++gap){
                        const auto [first,last]=gaps[gap];const int left=std::min(first.x,last.x),right=std::max(first.x,last.x);
                        for(std::size_t l=0;l<labels.size();++l){
                            work.spend();const auto& box=labels[l].box;
                            if(box.l!=left||box.r!=right||first.y<=box.t||first.y>=box.b)continue;
                            auto& list=options[gap];if(list.size()==list.capacity()){
                                const auto capacity=std::min(std::size_t{512},std::max(std::size_t{1},list.size()*2));
                                if(capacity<=list.size())throw std::runtime_error("Relation label limit exceeded");
                                work.spend(capacity*sizeof(int)+list.size()*(sizeof(int)+2u)+8u);list.reserve(capacity);
                            }
                            work.spend();list.push_back(static_cast<int>(l));
                        }
                    }
                    std::array<int,3> chosen{{-1,-1,-1}};
                    auto enumerate=[&](auto&& self,std::size_t gap)->void{
                        work.spend();
                        if(gap<gap_count){
                            for(const int label:options[gap]){
                                bool distinct=true;for(std::size_t n=0;n<gap;++n){work.spend();distinct&=chosen[n]!=label;}
                                if(!distinct)continue;
                                chosen[gap]=label;self(self,gap+1);
                            }return;
                        }
                        bool same=t.labels.size()==gap_count;
                        for(std::size_t n=0;n<gap_count;++n){work.spend(8);same&=t.labels.contains(chosen[n]);}
                        if(same)return;
                        // Keep the original trace's final slot. A capacity or
                        // Work failure remains an explicit oracle resource error.
                        if(traces.size()+1>=4096)throw std::runtime_error("Relation trace limit exceeded");
                        work.spend(2u*sizeof(Trace)+t.path.size()*sizeof(int)+t.labels.size()*64u+gap_count*128u+16u);
                        auto alternative=t;alternative.labels.clear();
                        for(std::size_t n=0;n<gap_count;++n)alternative.labels.insert(chosen[n]);
                        if(traces.capacity()-traces.size()<2u){
                            const auto capacity=std::min(std::size_t{4096},std::max(traces.size()+2u,traces.capacity()*2u));
                            work.spend(capacity*sizeof(Trace)+traces.size()*(sizeof(Trace)+4u)+8u);traces.reserve(capacity);
                        }
                        traces.push_back(std::move(alternative));
                    };
                    enumerate(enumerate,0);
                }
            }
            if(traces.size()>=4096)throw std::runtime_error("Relation trace limit exceeded");if(class_notes){
                const auto size=traces.size(),capacity=traces.capacity(),slots=size==capacity?std::min(std::size_t{4096},std::max(size+1u,capacity*2u)):0u;
                work.spend((slots+(size==capacity?size:0u)+1u)*(sizeof(Trace)+8u)+64u);if(slots)traces.reserve(slots);
            }traces.push_back(std::move(t));}
        // Every first parent write also appends that index to this queue;
        // bridge writes occur only at those same first visits. Traces own copies.
        if(a+1<ports.size()){
            work.spend(queue.size()*3u+1u);
            for(const int index:queue){parent[static_cast<std::size_t>(index)]=-2;bridge[static_cast<std::size_t>(index)]=-1;}
        }
    }
    auto matches=[&](const Relation& e,const Trace& t,bool reverse){if(!t.valid)return false;const auto& a=ports[reverse?t.b:t.a];const auto& b=ports[reverse?t.a:t.b];if(a.entity!=endpoint_index(e.from)||b.entity!=endpoint_index(e.to)||a.marker!=e.start||b.marker!=e.end)return false;if((e.stroke=="solid"&&(t.strokes[1]||t.strokes[2]))||(e.stroke=="dotted"&&(!t.strokes[1]||t.strokes[0]||t.strokes[2])))return false;if(!e.source_text&&!e.target_text){if(e.label.empty())return t.labels.empty();return t.labels.size()==1&&labels[static_cast<std::size_t>(*t.labels.begin())].text==e.label;}
        if(model.family!="class")return false;
        std::array<std::string_view,3> expected{};std::size_t count=0;
        if(e.source_text)expected[count++]=*e.source_text;
        if(!e.label.empty())expected[count++]=e.label;
        if(e.target_text)expected[count++]=*e.target_text;
        if(t.labels.size()!=count)return false;
        // Trace.path is stored from physical b back to a. Walk from the chosen
        // semantic source, retaining each real text gap and its unique owner.
        std::array<int,3> ordered{{-1,-1,-1}};std::size_t found=0;
        work.spend(2u*sizeof(std::array<std::size_t,3>)+sizeof(std::size_t)+32u);
        std::array<std::size_t,3> before_gap{},after_gap{};std::size_t travelled=0;
        for(std::size_t n=1;n<t.path.size();++n){
            work.spend();const auto first=reverse?n-1:t.path.size()-n,second=reverse?n:t.path.size()-n-1;
            const auto p=grid.point(t.path[first]),q=grid.point(t.path[second]);
            work.spend(8);const auto before=travelled;
            const auto distance=static_cast<std::size_t>(std::abs(p.x-q.x)+std::abs(p.y-q.y));travelled+=distance;
            if(distance==1)continue;
            if(p.y!=q.y||found>=count)return false;
            const int left=std::min(p.x,q.x),right=std::max(p.x,q.x);int selected_label=-1;
            for(const int label:t.labels){work.spend();const auto& block=labels[static_cast<std::size_t>(label)];if(block.box.l==left&&block.box.r==right&&p.y>block.box.t&&p.y<block.box.b){if(selected_label>=0)return false;selected_label=label;}}
            if(selected_label<0||std::ranges::find(ordered,selected_label)!=ordered.end())return false;
            const auto& actual=labels[static_cast<std::size_t>(selected_label)].text;work.spend(actual.size()+expected[found].size()+1u);
            if(actual!=expected[found])return false;
            before_gap[found]=before;after_gap[found]=travelled;ordered[found++]=selected_label;
        }
        if(found!=count)return false;
        // End text belongs to its own strict half of this actual directed
        // contour, measured between the real route ports. The text-gap width
        // cancels at its midpoint: compare complete before/after arc lengths.
        // A midpoint tie cannot identify an endpoint owner in this native profile.
        work.spend(8);
        if(e.source_text&&before_gap[0]>=travelled-after_gap[0])return false;
        if(e.target_text&&travelled-after_gap[count-1]>=before_gap[count-1])return false;
        // Distinct source/central/target roles own distinct physical text.
        // Check the actual chosen blocks for EVERY trace, including the
        // original BFS witness and finite same-gap alternatives alike.
        for(std::size_t first=0;first<found;++first)for(std::size_t second=first+1;second<found;++second)
            for(const auto a:labels[static_cast<std::size_t>(ordered[first])].cells)
                for(const auto b:labels[static_cast<std::size_t>(ordered[second])].cells){work.spend();if(a==b)return false;}
        return true;};
    auto matches_note=[&](std::size_t which,int target,const Trace& trace,bool reverse){
        work.spend(32u);if(!trace.valid||!trace.labels.empty()||!trace.strokes[1]||trace.strokes[0]||trace.strokes[2])return false;
        const auto& a=ports[reverse?trace.b:trace.a];const auto& b=ports[reverse?trace.a:trace.b];
        return a.entity==static_cast<int>(business_count+which)&&b.entity==target&&a.marker=="none"&&b.marker=="none";
    };
    const auto demand_count=model.relations.size()+linked_notes.size();
    if(class_notes)work.spend(linked_notes.size()*sizeof(int)+128u);
    std::vector<bool> used(traces.size()),used_ports(ports.size()),used_path(grid.cells.size());std::vector<int> selected(demand_count,-1);
    // Every declared relation has two real endpoint ports and its own shaft.
    // Fork/join bars may own many distinct ports; reusing a physical head or
    // merging two business edges into one shaft does not preserve multiplicity.
    auto assign=[&](auto&& self,std::size_t n)->bool{
        work.spend();if(n==demand_count)return true;
        for(std::size_t t=0;t<traces.size();++t){work.spend();const auto& trace=traces[t];
            if(used[t]||used_ports[trace.a]||used_ports[trace.b])continue;
            bool overlap=false;for(int cell:trace.path){work.spend();if(used_path[static_cast<std::size_t>(cell)]){overlap=true;break;}}if(overlap)continue;
            if(n<model.relations.size()){
                if(!matches(model.relations[n],trace,false)&&!matches(model.relations[n],trace,true))continue;
            }else{
                const auto [which,target]=linked_notes[n-model.relations.size()];
                if(!matches_note(which,target,trace,false)&&!matches_note(which,target,trace,true))continue;
            }
            // Labels belong to business edges. Two disjoint routes cannot
            // share one printed block, including overlapping prefix witnesses.
            bool shared_text=false;
            for(const int label:trace.labels)for(const auto p:labels[static_cast<std::size_t>(label)].cells){
                work.spend();if(used_label_cells[static_cast<std::size_t>(grid.index(p))])shared_text=true;
            }
            if(shared_text)continue;
            for(const int label:trace.labels)for(const auto p:labels[static_cast<std::size_t>(label)].cells){
                work.spend();used_label_cells[static_cast<std::size_t>(grid.index(p))]=true;
            }
            used[t]=true;used_ports[trace.a]=true;used_ports[trace.b]=true;for(int cell:trace.path){work.spend();used_path[static_cast<std::size_t>(cell)]=true;}selected[n]=static_cast<int>(t);
            if(self(self,n+1))return true;
            used[t]=false;used_ports[trace.a]=false;used_ports[trace.b]=false;for(int cell:trace.path){work.spend();used_path[static_cast<std::size_t>(cell)]=false;}
            for(const int label:trace.labels)for(const auto p:labels[static_cast<std::size_t>(label)].cells){
                work.spend();used_label_cells[static_cast<std::size_t>(grid.index(p))]=false;
            }
        }return false;
    };
    if(!assign(assign,0))return {"Relation endpoints/marker/cardinality/stroke/label/multiplicity differ"};
    // The optional native self-layout profile is checked only after every
    // business relationship has its complete generic physical proof.
    if(self_face_policy&&model.family=="er")for(std::size_t n=0;n<model.relations.size();++n){
        work.spend();const auto& edge=model.relations[n];if(edge.from!=edge.to)continue;
        const int at=entity_index(edge.from);if(at<0)continue;const auto& source=model.entities[static_cast<std::size_t>(at)];
        if(source.typed_er&&!source.typed_er->attributes.empty())continue;
        const auto& trace=traces[static_cast<std::size_t>(selected[n])];const auto& first=ports[trace.a];const auto& last=ports[trace.b];const auto& box=entity(static_cast<std::size_t>(at));
        bool exterior=first.direction==last.direction&&(first.direction==1||first.direction==3);
        auto outside=[&](int x){return first.direction==1?x>box.r:x<box.l;};
        for(const int cell:trace.path){work.spend();if(!outside(grid.point(cell).x))exterior=false;}
        for(const int label:trace.labels){work.spend();const auto& bounds=labels[static_cast<std::size_t>(label)].box;if(!outside(bounds.l)||!outside(bounds.r))exterior=false;}
        if(!exterior)errors.emplace_back("Self-loop exterior face policy differs: "+edge.from+"/"+edge.label);
    }
    if(std::ranges::any_of(used_ports,[](bool used){return !used;}))errors.emplace_back("Extra unclaimed real endpoint port/head");
    for(const auto& t:traces){const auto a=ports[t.a].entity,b=ports[t.b].entity;bool known=std::ranges::any_of(model.relations,[&](const Relation& e){return (endpoint_index(e.from)==a&&endpoint_index(e.to)==b)||(endpoint_index(e.from)==b&&endpoint_index(e.to)==a);});
        if(class_notes)for(const auto& [which,to]:linked_notes){work.spend(16u);const auto from=static_cast<int>(business_count+which);known|=(a==from&&b==to)||(a==to&&b==from);}
        if(!known)errors.emplace_back("Route junction creates an unauthored connection");}
    // Direction constrains acyclic ranks. The source order of transition facts
    // is locked separately; a static cycle does not define execution time.
    const auto count=class_notes?business_count:endpoints.size();std::vector<std::vector<bool>> reachable(count,std::vector<bool>(count));
    auto rank_endpoints=[&](const Relation& e){int a=endpoint_index(e.from),b=endpoint_index(e.to);if(model.family=="class"&&e.end=="triangle"&&e.start!="triangle")std::swap(a,b);return std::pair{a,b};};
    for(const auto& e:model.relations){const auto [a,b]=rank_endpoints(e);reachable[static_cast<std::size_t>(a)][static_cast<std::size_t>(b)]=true;}
    for(std::size_t k=0;k<count;++k)for(std::size_t a=0;a<count;++a)for(std::size_t b=0;b<count;++b){work.spend();reachable[a][b]=reachable[a][b]||(reachable[a][k]&&reachable[k][b]);}
    auto parent_of=[&](int n){return n<static_cast<int>(total)?group_index(model.entities[static_cast<std::size_t>(n)].owner):group_index(model.groups[static_cast<std::size_t>(n)-total].parent);};
    for(const auto& e:model.relations){const auto [a,b]=rank_endpoints(e);if(reachable[static_cast<std::size_t>(b)][static_cast<std::size_t>(a)])continue;
        std::set<int> parents;for(int at=parent_of(b);at>=0;at=parent_of(static_cast<int>(total)+at))parents.insert(at);
        int common=-1;for(int at=parent_of(a);at>=0;at=parent_of(static_cast<int>(total)+at))if(parents.contains(at)){common=at;break;}
        auto direction=model.direction;for(int at=common;at>=0;at=parent_of(static_cast<int>(total)+at))if(!model.groups[static_cast<std::size_t>(at)].direction.empty()){direction=model.groups[static_cast<std::size_t>(at)].direction;break;}
        const auto& from=endpoints[static_cast<std::size_t>(a)];const auto& to=endpoints[static_cast<std::size_t>(b)];
        const int dx=to.l+to.r-from.l-from.r,dy=to.t+to.b-from.t-from.b;
        if((direction=="LR"&&dx<=0)||(direction=="RL"&&dx>=0)||((direction=="TD"||direction=="TB")&&dy<=0)||(direction=="BT"&&dy>=0))errors.emplace_back("Acyclic relation rank contradicts scope direction");
    }
    std::vector<bool> claimed(grid.cells.size());auto claim=[&](Point p){if(grid.has(p))claimed[static_cast<std::size_t>(grid.index(p))]=true;};
    if(presentation&&presentation->title){for(int y=0;y<2;++y)for(int col=0;col<grid.width;++col){work.spend();claim({col,y});}}
    std::vector<int> used_port_normals(grid.cells.size());
    for(const int which:selected){const auto& t=traces[static_cast<std::size_t>(which)];for(const auto index:{t.a,t.b})used_port_normals[static_cast<std::size_t>(grid.index(ports[index].border))]|=kBits[static_cast<std::size_t>(ports[index].direction)];for(const int cell:t.path)claim(grid.point(cell));for(const auto p:ports[t.a].marker_cells)claim(p);for(const auto p:ports[t.b].marker_cells)claim(p);for(const int l:t.labels)for(const auto p:labels[static_cast<std::size_t>(l)].cells)claim(p);}
    auto check_perimeter=[&](const Rect& box,const std::vector<int>& separators,bool group_frame){
        for(int y=box.t;y<=box.b;++y)for(int x=box.l;x<=box.r;++x){work.spend();const Point p{x,y};if(!box.boundary(p))continue;
            int expected=0;if(y==box.t||y==box.b){if(x>box.l)expected|=L;if(x<box.r)expected|=R;}if(x==box.l||x==box.r){if(y>box.t)expected|=U;if(y<box.b)expected|=D;}
            int allowed=used_port_normals[static_cast<std::size_t>(grid.index(p))];
            if(std::ranges::find(separators,y)!=separators.end()){if(x==box.l)allowed|=R;if(x==box.r)allowed|=L;}
            if(group_frame&&claimed[static_cast<std::size_t>(grid.index(p))])allowed|=glyph(grid.at(p)).bits&~expected;
            if((glyph(grid.at(p)).bits&~expected&~allowed)!=0)errors.emplace_back("Unclaimed arm on an entity/group/note perimeter");claim(p);
        }
    };
    for(std::size_t n=0;n<total;++n){
        const auto& b=entity(n);
        if(model.entities[n].typed_er){
            const auto table_errors=er_table_errors(grid,b,*model.entities[n].typed_er,work);
            if(!table_errors.empty())return table_errors;
            for(int y=b.t;y<=b.b;++y)for(int x=b.l;x<=b.r;++x){
                work.spend();const Point p{x,y};
                if(b.boundary(p)){
                    int outward=0;if(x==b.l)outward|=L;if(x==b.r)outward|=R;if(y==b.t)outward|=U;if(y==b.b)outward|=D;
                    const int actual_outward=glyph(grid.at(p)).bits&outward;
                    const int selected_outward=used_port_normals[static_cast<std::size_t>(grid.index(p))]&outward;
                    if(actual_outward!=selected_outward)
                        errors.emplace_back("Missing or unclaimed selected outward arm on a validated ER table perimeter");
                }
                claim(p);
            }
        }else{
            if(b.shape=="box"||b.shape=="rounded")check_perimeter(b,content(grid,b,work).separators,false);
            for(int y=b.t;y<=b.b;++y)for(int x=b.l;x<=b.r;++x)claim({x,y});
        }
    }
    for(std::size_t g=0;g<model.groups.size();++g)check_perimeter(group(g),{},true);
    for(const auto point:group_caption_cells){work.spend();claim(point);}
    std::set<std::size_t> used_notes;
    if(!class_notes)for(const auto& note:model.notes){const int target=endpoint_index(note.target);std::vector<std::size_t> candidates;
        for(std::size_t n=0;n<outlines_.size();++n){const auto& candidate=outlines_[n];if(used_notes.contains(n)||(candidate.shape!="box"&&candidate.shape!="rounded"))continue;
            const auto body=content(grid,candidate,work);if(body.text!=lines(note.text))continue;const auto& owner=endpoints[static_cast<std::size_t>(target)];
            const int middle=candidate.t+candidate.b;
            if(middle<owner.t*2||middle>owner.b*2)continue;
            const bool right=note.position=="rightof",left=note.position=="leftof";
            if((!right&&!left)||(right&&candidate.l<=owner.r)||(left&&candidate.r>=owner.l))continue;
            // Without an explicit leader, the aligned nearest entity is the
            // observable target. A note beyond another entity is ambiguous.
            bool nearest=true;for(std::size_t other=0;other<endpoints.size();++other){if(other==static_cast<std::size_t>(target))continue;const auto& box=endpoints[other];if(middle<box.t*2||middle>box.b*2)continue;
                if((right&&box.r>owner.r&&box.r<candidate.l)||(left&&box.l<owner.l&&box.l>candidate.r))nearest=false;
            }if(nearest)candidates.push_back(n);
        }
        if(candidates.size()!=1){errors.emplace_back("Note text/unique target/placement differs");continue;}
        used_notes.insert(candidates.front());const auto& chosen=outlines_[candidates.front()];check_perimeter(chosen,{},false);for(int y=chosen.t;y<=chosen.b;++y)for(int x=chosen.l;x<=chosen.r;++x)claim({x,y});
    }
    if(class_notes)for(std::size_t n=0;n<model.notes.size();++n){
        const auto& chosen=note_box(n);if(!class_note_content(grid,chosen,model.notes[n],work))return {"Chosen Class Note literal cells changed"};
        check_perimeter(chosen,{},false);for(int y=chosen.t;y<=chosen.b;++y)for(int x=chosen.l;x<=chosen.r;++x){work.spend();claim({x,y});}
    }
    auto following=following_row(grid,work,following_token);
    if(following){
        for(int x=0;x<grid.width;++x){
            work.spend(4u);const Point point{x,*following};
            if(grid.at(point)!=" "&&claimed[static_cast<std::size_t>(grid.index(point))]){following.reset();break;}
        }
    }
    for(int y=0;y<grid.height;++y){
        if(following&&y==*following)continue;
        for(int x=0;x<grid.width;++x){work.spend();const auto value=grid.at({x,y});if(value!=" "&&!value.empty()&&!claimed[static_cast<std::size_t>(grid.index({x,y}))])return {"Unclaimed text/control/marker/route glyph exposes a lost or extra fact"};}
    }
    if(errors.empty()){
        work.spend(4u*sizeof(std::optional<int>)+4u*sizeof(void*)+64u);
        if(unclaimed_following_row)*unclaimed_following_row=following;
    }
    return errors;
}
inline std::vector<std::string> validate_impl(const Grid& grid,const Model& model,Work& work,bool self_face_policy=false,const ClassPresentation* presentation=nullptr,bool state_descriptions=false,ClassCardWitness* successful_card=nullptr,std::string_view following_token=kRelationsFollowingToken,std::optional<int>* unclaimed_following_row=nullptr){
    if(successful_card){*successful_card={};work.spend(sizeof(ClassCardWitness)+64u);if(model.family!="class"||!presentation)throw std::runtime_error("Class card witness requires presented Class Model");}
    work.spend(4u*sizeof(std::string_view)+8u*sizeof(std::optional<int>)+4u*sizeof(void*)+64u);
    std::optional<int> chosen_following;
    if(grid.width<=0||grid.width>240||grid.height<=0||grid.height>1040||grid.cells.size()!=static_cast<std::size_t>(grid.width*grid.height)||model.entities.size()>64||model.relations.size()>128||model.groups.size()>64||model.notes.size()>64)return {"Relation oracle bounds exceeded"};
    if(presentation){work.spend(4u);if(model.family!="class")throw std::runtime_error("Class presentation protocol requires Class Model");for(const auto& entity:model.entities){work.spend();if(entity.shape!="box"||entity.typed_er)throw std::runtime_error("Class presentation protocol requires untyped box entities");}}
    if(state_descriptions){work.spend(4u);if(model.family!="state"||presentation)throw std::runtime_error("State description protocol requires unpresented State Model");}
    if(model.family!="er"&&std::ranges::any_of(model.entities,[](const Entity& e){return e.typed_er.has_value();}))return {"Typed ER table attached to another family"};
    if(model.direction!="TB"&&model.direction!="TD"&&model.direction!="BT"&&model.direction!="LR"&&model.direction!="RL")return {"Unknown direction"};
    std::set<std::string> ids;for(const auto& e:model.entities){if(!ids.insert(e.id).second)return {"Duplicate entity identity"};if(e.shape!="box"&&e.shape!="rounded"&&e.shape!="choice"&&e.shape!="start"&&e.shape!="final"&&e.shape!="fork"&&e.shape!="join")return {"Unknown entity shape"};}
    for(const auto& g:model.groups)if(!ids.insert(g.id).second)return {"Duplicate group identity"};
    for(const auto& e:model.entities){
        if(!e.owner.empty()&&std::ranges::none_of(model.groups,[&](const Group& g){return g.id==e.owner;}))return {"Unknown entity owner"};
        if(state_descriptions&&!e.rows.empty()){if(!state_description_row_domain(e,work))throw std::runtime_error("State description row profile domain");}
        else for(const auto& row:e.rows)if((row.role!="attribute"&&row.role!="method")||row.alternatives.empty())return {"Unknown/empty member row"};
    }
    for(std::size_t n=0;n<model.relations.size();++n)if(model.relations[n].order!=static_cast<int>(n))return {"Transition/declaration order facts changed"};
    for(const auto& e:model.relations){if(!ids.contains(e.from)||!ids.contains(e.to))return {"Unknown relation endpoint"};if(e.stroke!="solid"&&e.stroke!="dotted")return {"Unknown relation stroke"};for(const auto& marker:{e.start,e.end})if(marker!="none"&&marker!="arrow"&&marker!="triangle"&&marker!="filled_diamond"&&marker!="open_diamond"&&marker!="card:one"&&marker!="card:zero-one"&&marker!="card:zero-many"&&marker!="card:one-many")return {"Unknown relation marker"};
        if(e.source_text||e.target_text){
            if(model.family!="class")return {"Class endpoint text attached to another family"};
            for(const auto* text:{&e.source_text,&e.target_text})if(*text&&!class_endpoint_text_domain(**text,work))return {"Class endpoint text profile domain"};
        }
    }
    for(const auto& g:model.groups){std::set<std::string> members;for(const auto& id:g.members){const auto n=std::ranges::find_if(model.entities,[&](const Entity& e){return e.id==id;});if(n==model.entities.end()||n->owner!=g.id||!members.insert(id).second)return {"Inconsistent group/member ownership facts"};}}
    for(const auto& e:model.entities)if(!e.owner.empty()){const auto g=std::ranges::find_if(model.groups,[&](const Group& group){return group.id==e.owner;});if(g==model.groups.end()||std::ranges::find(g->members,e.id)==g->members.end())return {"Unlisted owner/member fact"};}
    for(const auto& g:model.groups){std::set<std::string> seen;for(auto at=g.id;!at.empty();){if(!seen.insert(at).second||seen.size()>16)return {"Cyclic/too-deep group parent"};const auto it=std::ranges::find_if(model.groups,[&](const Group& other){return other.id==at;});if(it==model.groups.end())return {"Unknown group parent"};at=it->parent;}}
    work.spend(sizeof(bool)+16u);const bool class_notes=!model.notes.empty()&&(model.notes.front().position=="standalone"||model.notes.front().position=="linked");
    if(class_notes){
        if(model.family!="class")return {"Class Note attached to another family"};
        for(const auto& note:model.notes){
            work.spend(note.owner.size()+note.target.size()+32u);
            if(!class_note_text_domain(note.text,work))return {"Class Note literal text domain"};
            if(!note.owner.empty()&&std::ranges::none_of(model.groups,[&](const Group& g){work.spend(g.id.size()+note.owner.size()+4u);return g.id==note.owner;}))return {"Unknown Class Note owner"};
            if(note.position=="standalone"){if(!note.target.empty())return {"Standalone Note has target"};}
            else if(note.position!="linked"||std::ranges::none_of(model.entities,[&](const Entity& e){work.spend(e.id.size()+note.target.size()+4u);return e.id==note.target;}))return {"Unknown linked Class Note target/mode"};
        }
    }else for(const auto& note:model.notes)if(!ids.contains(note.target))return {"Unknown note target"};
    const auto frames=outlines(grid,work);std::vector<Content> bodies;
    for(const auto& frame:frames){if(frame.shape=="box"||frame.shape=="rounded"||frame.shape=="choice")bodies.push_back(content(grid,frame,work));else bodies.emplace_back();}
    std::vector<std::vector<int>> choices;
    for(const auto& e:model.entities){std::vector<int> found;for(std::size_t n=0;n<frames.size();++n)if((model.family=="class"?class_complete_content(grid,frames[n],e,work,&bodies[n],presentation):entity_content(grid,frames[n],e,work,&bodies[n],presentation,state_descriptions)))found.push_back(static_cast<int>(n));choices.push_back(std::move(found));}
    for(const auto& g:model.groups){std::vector<int> found;for(std::size_t n=0;n<frames.size();++n){const auto& f=frames[n];if(f.shape!="box"&&f.shape!="rounded")continue;const auto& body=bodies[n];if(!body.text.empty()&&body.text.front()==g.label)found.push_back(static_cast<int>(n));}choices.push_back(std::move(found));}
    if(class_notes){
        work.spend((choices.size()+model.notes.size())*sizeof(std::vector<int>)+choices.size()*sizeof(std::vector<int>)+64u);
        choices.reserve(choices.size()+model.notes.size());
        for(const auto& note:model.notes){
            work.spend(2u*frames.size()*sizeof(int)+sizeof(std::vector<int>)+64u);std::vector<int> found;found.reserve(frames.size());
            for(std::size_t n=0;n<frames.size();++n){work.spend();if(class_note_content(grid,frames[n],note,work))found.push_back(static_cast<int>(n));}
            choices.push_back(std::move(found));
        }
    }
    for(std::size_t n=0;n<choices.size();++n)if(choices[n].empty())return {"Missing exact entity/header/member/control/group outline "+std::to_string(n)};
    std::vector<int> assignment(choices.size(),-1);std::vector<bool> used(frames.size());std::size_t tries=0;std::vector<std::string> last{"No exact semantic assignment"};
    auto assign=[&](auto&& self,std::size_t at)->bool{work.spend();if(++tries>10000)throw std::runtime_error("Relation identity assignment exhausted");if(at==choices.size()){
        work.spend(4u*sizeof(std::optional<int>)+64u);std::optional<int> candidate_following;
        last=check_scene(grid,model,frames,assignment,work,self_face_policy,presentation,following_token,&candidate_following);
        if(last.empty()){work.spend(2u*sizeof(std::optional<int>)+8u);chosen_following=candidate_following;return true;}return false;
    }for(const int n:choices[at])if(!used[static_cast<std::size_t>(n)]){assignment[at]=n;used[static_cast<std::size_t>(n)]=true;if(self(self,at+1))return true;used[static_cast<std::size_t>(n)]=false;}return false;};
    if(assign(assign,0)){
        if(successful_card){
            if(model.entities.empty()||assignment.empty()||assignment[0]<0)throw std::runtime_error("Class card witness lacks successful entity assignment");
            const auto at=static_cast<std::size_t>(assignment[0]);
            work.spend(4u*sizeof(Rect)+sizeof(ClassCardWitness)+frames[at].shape.size()*4u+128u);
            successful_card->box=frames[at];successful_card->separator=bodies[at].separators.empty()?std::nullopt:std::optional<int>{bodies[at].separators.front()};
            successful_card->grid=&grid;successful_card->model=&model;successful_card->presentation=presentation;
        }
        work.spend(4u*sizeof(std::optional<int>)+4u*sizeof(void*)+64u);
        if(unclaimed_following_row)*unclaimed_following_row=chosen_following;
        return {};
    }return last;
}
inline std::vector<std::string> validate(const Grid& grid,const Model& model){try{Work work;return validate_impl(grid,model,work);}catch(const std::exception& e){return {e.what()};}}
inline std::string explain(const std::vector<std::string>& errors){std::string out;for(const auto& e:errors)out+=e+"; ";return out;}

inline bool assertion_utf8(std::string_view value){
    if(value.find("\xef\xbf\xbd")!=std::string_view::npos)return false;
    for(std::size_t at=0;at<value.size();){const auto lead=static_cast<unsigned char>(value[at++]);std::uint32_t scalar=lead;int extra=0;
        if(lead>=0xc2&&lead<=0xdf){scalar=lead&0x1f;extra=1;}else if(lead>=0xe0&&lead<=0xef){scalar=lead&0x0f;extra=2;}else if(lead>=0xf0&&lead<=0xf4){scalar=lead&0x07;extra=3;}else if(lead>=0x80)return false;
        if(at+static_cast<std::size_t>(extra)>value.size())return false;
        for(int n=0;n<extra;++n){const auto next=static_cast<unsigned char>(value[at++]);if((next&0xc0)!=0x80)return false;scalar=(scalar<<6)|(next&0x3f);}
        if((extra==1&&scalar<0x80)||(extra==2&&scalar<0x800)||(extra==3&&scalar<0x10000)||scalar>0x10ffff||(scalar>=0xd800&&scalar<=0xdfff))return false;
    }return true;
}

inline std::string er_key_text(const std::vector<std::string>& keys){
    std::string out;for(const auto& key:keys){if(!out.empty())out+=',';out+=key;}return out;
}
inline std::size_t er_field_cost(std::string_view value,Work& work){
    work.spend(value.size());std::size_t cost=0;const auto rows=lines(value);
    for(const auto& row:rows){cost+=ftxui::Utf8ToGlyphs(row).size();}
    return cost+rows.size()-1;
}
inline std::array<std::string,4> er_fields(const ErTableAttribute& a){return {a.type,a.name,er_key_text(a.keys),a.comment};}
inline bool er_attribute_within_limit(const ErTableAttribute& a,Work& work){
    if(a.type.empty()||a.name.empty()||a.keys.size()>85)return false;
    work.spend(a.type.size()+a.name.size()+a.comment.size()+a.keys.size()*3);
    for(const auto& key:a.keys)if(key!="PK"&&key!="FK"&&key!="UK")return false;
    const auto values=er_fields(a);std::size_t cost=1+(!a.keys.empty()?1:0)+(!a.comment.empty()?1:0);
    for(const auto& value:values){cost+=er_field_cost(value,work);}
    return cost<=256;
}
// This component checks the complete table interior and its required border
// structure. Outer route arms remain the existing whole-scene port oracle's
// responsibility; an inward or internal unmodeled arm is never allowed.
inline std::vector<std::string> er_table_errors(const Grid& grid,const Rect& box,const ErTableSpec& model,
                                         Work& work,ErTableEvidence* evidence){
    if(grid.width<=0||grid.width>240||grid.height<=0||grid.height>1040||
       grid.cells.size()!=static_cast<std::size_t>(grid.width*grid.height)||
       box.l<0||box.t<0||box.r>=grid.width||box.b>=grid.height||box.r-box.l<5||
       model.header.empty()||model.header.size()>256||model.attributes.empty()||model.attributes.size()>128)
        return {"ER typed table bounds/required fields differ"};
    std::size_t header_cost=model.header.size()-1;
    for(const auto& line:model.header){work.spend(line.size());header_cost+=ftxui::Utf8ToGlyphs(line).size();}
    if(header_cost>256)return {"ER typed header label limit exceeded"};
    for(const auto& a:model.attributes)if(!er_attribute_within_limit(a,work))return {"ER complete attribute aggregate limit exceeded"};
    const bool has_keys=std::ranges::any_of(model.attributes,[](const auto& a){return !a.keys.empty();});
    const bool has_comments=std::ranges::any_of(model.attributes,[](const auto& a){return !a.comment.empty();});
    std::vector<int> roles{0,1};if(has_keys)roles.push_back(2);if(has_comments)roles.push_back(3);
    const int header_divider=box.t+1+static_cast<int>(model.header.size());
    if(header_divider>=box.b)return {"ER header/table divider missing"};
    std::vector<int> columns{box.l};
    for(int x=box.l+1;x<box.r;++x){work.spend();if(glyph(grid.at({x,header_divider})).bits==(L|R|D))columns.push_back(x);}
    columns.push_back(box.r);
    if(columns.size()!=roles.size()+1)return {"ER typed columns omitted, added or merged"};
    for(std::size_t n=1;n<columns.size();++n)if(columns[n]-columns[n-1]<3)return {"ER column has no independent field/padding cells"};
    std::vector<int> dividers{header_divider};
    for(const auto& a:model.attributes){int height=1;const auto values=er_fields(a);for(const int role:roles)height=std::max(height,static_cast<int>(lines(values[static_cast<std::size_t>(role)]).size()));dividers.push_back(dividers.back()+height+1);}
    if(dividers.back()!=box.b)return {"ER attribute order/row-height/blank-row count changed"};
    const int width=box.r-box.l+1,height=box.b-box.t+1;
    work.spend(static_cast<std::size_t>(width*height));
    Grid expected{width,height,std::vector<std::string>(static_cast<std::size_t>(width*height)," ")};
    auto paint=[&](int x,int y,std::string_view value){work.spend();expected.put({x-box.l,y-box.t},std::string(value));};
    auto text=[&](int x,int y,std::string_view value,int right){
        work.spend(value.size());const auto glyphs=ftxui::Utf8ToGlyphs(std::string(value));
        if(x+static_cast<int>(glyphs.size())>right)return false;
        for(const auto& value_:glyphs){paint(x++,y,value_);}
        return true;
    };
    for(int x=box.l+1;x<box.r;++x){paint(x,box.t,"─");paint(x,box.b,"─");}
    for(int y=box.t+1;y<box.b;++y){paint(box.l,y,"│");paint(box.r,y,"│");}
    paint(box.l,box.t,"┌");paint(box.r,box.t,"┐");paint(box.l,box.b,"└");paint(box.r,box.b,"┘");
    for(std::size_t n=0;n<model.header.size();++n){
        const int cells=ftxui::string_width(model.header[n]);
        if(cells>width-2||!text(box.l+1+(width-2-cells)/2,box.t+1+static_cast<int>(n),model.header[n],box.r))return {"ER complete header exceeds its own table"};
    }
    for(const int y:dividers){
        for(int x=box.l+1;x<box.r;++x)paint(x,y,"─");
        paint(box.l,y,y==box.b?"└":"├");paint(box.r,y,y==box.b?"┘":"┤");
    }
    for(std::size_t c=1;c+1<columns.size();++c){
        const int x=columns[c];for(int y=header_divider+1;y<box.b;++y)paint(x,y,"│");
        for(const int y:dividers)paint(x,y,y==header_divider?"┬":y==box.b?"┴":"┼");
    }
    for(std::size_t a=0;a<model.attributes.size();++a){
        const auto values=er_fields(model.attributes[a]);
        for(std::size_t c=0;c<roles.size();++c){
            const auto rows=lines(values[static_cast<std::size_t>(roles[c])]);
            for(std::size_t row=0;row<rows.size();++row)
                if(!text(columns[c]+2,dividers[a]+1+static_cast<int>(row),rows[row],columns[c+1]-1))return {"ER field exceeds its independent column"};
        }
    }
    for(int y=box.t;y<=box.b;++y)for(int x=box.l;x<=box.r;++x){
        work.spend();const Point at{x,y};const auto actual=grid.at(at),want=expected.at({x-box.l,y-box.t});
        if(actual==want)continue;
        if(box.boundary(at)){
            int outward=0;if(x==box.l)outward|=L;if(x==box.r)outward|=R;if(y==box.t)outward|=U;if(y==box.b)outward|=D;
            const auto got=glyph(actual),need=glyph(want);
            if(need.bits&&got.stroke==0&&got.bits&&((got.bits&~outward)==need.bits))continue;
        }
        return {"ER field identity/column/row/blank cell/junction differs at "+std::to_string(x)+","+std::to_string(y)};
    }
    if(evidence){*evidence={std::move(columns),std::move(dividers)};}
    return {};
}
inline std::vector<std::string> er_table_errors(const Grid& grid,const ErTableSpec& model){
    try{Work work;return er_table_errors(grid,{0,0,grid.width-1,grid.height-1,"box"},model,work);}catch(const std::exception& e){return {e.what()};}
}

inline void class481_require(bool condition,std::string_view message){if(!condition)throw std::runtime_error(std::string(message));}
inline void class481_keys(JsonVal value,std::initializer_list<std::string_view> expected,Work& work){
    class481_require(value.is_obj()&&value.size()==expected.size(),"CLASS481 exact object fields");
    std::set<std::string_view> seen;
    value.iter_obj([&](JsonVal key,JsonVal){work.spend(key.as_str().size()+expected.size()+1);class481_require(key.is_str()&&std::ranges::find(expected,key.as_str())!=expected.end()&&seen.insert(key.as_str()).second,"CLASS481 unknown/duplicate field");});
}
inline std::string_view class481_text(JsonVal value,Work& work,std::size_t limit=65536){
    class481_require(value.is_str()&&value.as_str().size()<=limit,"CLASS481 exact bounded text type");work.spend(value.as_str().size()+1);return value.as_str();
}
inline void class481_hash_text(JsonVal value,Work& work){
    const auto text=class481_text(value,work,64);class481_require(text.size()==64&&std::ranges::all_of(text,[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),"CLASS481 exact SHA text");
}

inline std::string class481_canonical(JsonVal value,Work& work,int depth=0){
    // Every caller passes a frozen row or its subtree (max17,531 encoded
    // bytes), including the bounded parsed controls. Pay the output/copy bound
    // before allocating a sorted key vector or growing the canonical string.
    if(depth==0)work.spend(17531u*8u);
    work.spend();class481_require(depth<=32,"CLASS481 JSON depth bound exceeded");
    if(value.is_obj()){
        std::vector<std::pair<std::string,JsonVal>> entries;value.iter_obj([&](JsonVal key,JsonVal child){work.spend(key.as_str().size()*4+sizeof(std::pair<std::string,JsonVal>));entries.emplace_back(std::string(key.as_str()),child);});
        work.spend(entries.size()*entries.size()+1);std::ranges::sort(entries,{},&std::pair<std::string,JsonVal>::first);
        std::string out="{";std::string previous;bool first=true;
        for(const auto& [key,child]:entries){class481_require(first||key!=previous,"CLASS481 duplicate canonical object key");if(!first)out+=',';first=false;previous=key;json::JsonMutDoc atom;auto encoded=atom.string(key);atom.set_root(encoded);out+=atom.to_string();out+=':';out+=class481_canonical(child,work,depth+1);}out+='}';work.spend(out.size()*2+1);return out;
    }
    if(value.is_arr()){
        std::string out="[";for(std::size_t n=0;n<value.size();++n){work.spend(n+1);if(n)out+=',';out+=class481_canonical(value.at(n),work,depth+1);}out+=']';work.spend(out.size()*2+1);return out;
    }
    if(value.is_str())work.spend(value.as_str().size()*8+64);else work.spend(64);
    class481_require(value.valid(),"CLASS481 canonical missing value");return value.to_string();
}

inline Model class481_model(JsonVal value,Work& work,bool endpoint_text_profile=false,const std::pair<JsonVal,std::size_t>* canonical_size=nullptr){
    // The optional proof is produced only by a successful immutable source
    // binder. It reuses actual canonical length, never a caller-chosen budget.
    work.spend(sizeof(canonical_size)+16u);
    if(canonical_size){
        work.spend(sizeof(*canonical_size)+64u);
        class481_require(canonical_size->first.raw()==value.raw()&&value.valid(),"CLASS481 canonical reuse exact immutable value");
        class481_require(canonical_size->second>0&&canonical_size->second<=17531u,"CLASS481 canonical reuse encoded size domain");
    }
    class481_keys(value,{"family","direction","entities","relations","groups","notes"},work);
    class481_require(value.get_string("family")=="class","CLASS481 Model family");const auto direction=class481_text(value.get("direction"),work,2);
    class481_require(direction=="TB"||direction=="TD"||direction=="BT"||direction=="LR"||direction=="RL","CLASS481 direction enum");
    const auto entities=value.get("entities"),relations=value.get("relations"),groups=value.get("groups"),notes=value.get("notes");
    class481_require(entities.is_arr()&&entities.size()>0&&entities.size()<=64&&relations.is_arr()&&relations.size()<=128&&groups.is_arr()&&groups.size()<=64&&notes.is_arr()&&notes.size()<=64,"CLASS481 Model domain bounds");
    std::set<std::string> ids;
    entities.iter([&](JsonVal e){class481_keys(e,{"id","label","shape","owner","headers","rows"},work);const auto id=class481_text(e.get("id"),work,256);class481_require(!id.empty()&&ids.insert(std::string(id)).second&&e.get_string("shape")=="box","CLASS481 unique box identity");static_cast<void>(class481_text(e.get("label"),work));static_cast<void>(class481_text(e.get("owner"),work,256));const auto headers=e.get("headers"),rows=e.get("rows");class481_require(headers.is_arr()&&headers.size()>0&&headers.size()<=16&&rows.is_arr()&&rows.size()<=128,"CLASS481 header/member arrays");headers.iter([&](JsonVal h){class481_require(h.is_arr()&&h.size()>0&&h.size()<=256,"CLASS481 header alternative");h.iter([&](JsonVal t){static_cast<void>(class481_text(t,work));});});rows.iter([&](JsonVal r){
        if(r.has("source_facts"))class481_keys(r,{"role","alternatives","source_facts"},work);else class481_keys(r,{"role","alternatives"},work);
        const auto role=class481_text(r.get("role"),work,16);class481_require(role=="attribute"||role=="method","CLASS481 member compartment role");const auto alternatives=r.get("alternatives");class481_require(alternatives.is_arr()&&alternatives.size()>0&&alternatives.size()<=16,"CLASS481 member alternatives");alternatives.iter([&](JsonVal t){class481_require(!class481_text(t,work).empty(),"CLASS481 nonempty member text");});
    });});
    groups.iter([&](JsonVal g){class481_keys(g,{"id","label","parent","direction","members"},work);const auto id=class481_text(g.get("id"),work,256);class481_require(!id.empty()&&ids.insert(std::string(id)).second,"CLASS481 unique group identity");for(const auto key:{"label","parent","direction"})static_cast<void>(class481_text(g.get(key),work));class481_require(g.get("direction").as_str().empty(),"CLASS481 explicit group direction requires a reviewed geometry profile");class481_require(g.get("members").is_arr()&&g.get("members").size()<=64,"CLASS481 group members array");g.get("members").iter([&](JsonVal t){static_cast<void>(class481_text(t,work,256));});});
    std::size_t order=0;
    relations.iter([&](JsonVal r){
        if(endpoint_text_profile){class481_keys(r,{"source","target","label","start","end","stroke","order","source_text","target_text"},work);for(const auto key:{"source_text","target_text"}){const auto text=r.get(key);class481_require(text.is_null()||(text.is_str()&&class_endpoint_text_domain(class481_text(text,work,256),work)),"Class endpoint text exact nullable profile");}}
        else class481_keys(r,{"source","target","label","start","end","stroke","order"},work);for(const auto key:{"source","target"})class481_require(ids.contains(std::string(class481_text(r.get(key),work,256))),"CLASS481 known relation endpoint");static_cast<void>(class481_text(r.get("label"),work));for(const auto key:{"start","end"}){const auto marker=class481_text(r.get(key),work,32);class481_require(marker=="none"||marker=="arrow"||marker=="triangle"||marker=="filled_diamond"||marker=="open_diamond","CLASS481 marker enum");}const auto stroke=class481_text(r.get("stroke"),work,16);class481_require(stroke=="solid"||stroke=="dotted","CLASS481 stroke enum");class481_require(r.get("order").is_int()&&r.get_int("order")==static_cast<std::int64_t>(order++),"CLASS481 relation order indices");});
    bool class_note_profile=false,legacy_note_profile=false;
    work.spend(2u*sizeof(bool)+16u);
    notes.iter([&](JsonVal n){
        if(n.has("owner")){
            class_note_profile=true;class481_keys(n,{"text","target","position","owner"},work);
            const auto text=class481_text(n.get("text"),work,256u);class481_require(class_note_text_domain(text,work),"CLASS481 literal Class Note text domain");
            const auto mode=class481_text(n.get("position"),work,16u),owner=class481_text(n.get("owner"),work,256u);
            bool known_owner=owner.empty();groups.iter([&](JsonVal g){const auto id=g.get("id").as_str();work.spend(id.size()+owner.size()+4u);known_owner|=id==owner;});
            class481_require(known_owner,"CLASS481 known Class Note namespace owner");
            if(mode=="standalone")class481_require(n.get("target").is_null(),"CLASS481 standalone Note typed null target");
            else{
                class481_require(mode=="linked","CLASS481 Class Note mode");const auto target=class481_text(n.get("target"),work,256u);bool known=false;
                entities.iter([&](JsonVal e){const auto id=e.get("id").as_str();work.spend(id.size()+target.size()+4u);known|=id==target;});
                class481_require(!target.empty()&&known,"CLASS481 linked Note known Class target");
            }
        }else{
            legacy_note_profile=true;
            class481_keys(n,{"text","target","position"},work);static_cast<void>(class481_text(n.get("text"),work));class481_require(ids.contains(std::string(class481_text(n.get("target"),work,256))),"CLASS481 attached Note owner");const auto side=class481_text(n.get("position"),work,16);class481_require(side=="leftof"||side=="rightof","CLASS481 Note side");
        }
    });
    class481_require(!class_note_profile||!legacy_note_profile,"CLASS481 mixed Note modes");
    std::size_t encoded_size=0;
    if(canonical_size)encoded_size=canonical_size->second;
    else{const auto encoded=class481_canonical(value,work);encoded_size=encoded.size();}
    work.spend(encoded_size*8+256);auto result=parse_model(value,work);
    if(endpoint_text_profile)for(std::size_t n=0;n<result.relations.size();++n){const auto relation=relations.at(n);for(const auto key:{"source_text","target_text"}){const auto text=relation.get(key);if(text.is_null())continue;auto& target=key==std::string_view("source_text")?result.relations[n].source_text:result.relations[n].target_text;target=std::string(text.as_str());}}
    return result;
}
inline bool class481_resource(std::string_view error){return error.find("exhausted")!=std::string_view::npos||error.find("limit exceeded")!=std::string_view::npos||error=="Relation oracle bounds exceeded";}
inline void class481_no_resource(const std::vector<std::string>& errors){for(const auto& error:errors)if(class481_resource(error))throw std::runtime_error("CLASS481 resource failure is not semantic evidence: "+error);}


// Mechanical ER helpers: Native defaults/bodies/Work expressions are unchanged.
inline Model er296_typed_model(JsonVal source_model,Work& work){
    auto model=parse_model(source_model,work);if(model.family!="er")throw std::runtime_error("ER source field decoder used for another family");
    // Raw role strings stay in immutable facts. The reviewed plain ER display
    // removes only outer ASCII horizontal whitespace, including all-space roles;
    // such roles own no text candidate. Never change attribute field whitespace.
    for(auto& relation:model.relations){
        work.spend(relation.label.size()*2+1);
        const auto first=relation.label.find_first_not_of(" \t\r");
        if(first==std::string::npos)relation.label.clear();
        else {
            const auto last=relation.label.find_last_not_of(" \t\r");
            if(first||last+1!=relation.label.size())relation.label=relation.label.substr(first,last-first+1);
        }
    }
    std::size_t index=0;
    source_model.get("entities").iter([&](JsonVal value){
        auto& entity=model.entities.at(index++);
        if(value.get("rows").size()==0)return;
        if(entity.headers.size()!=1)throw std::runtime_error("Unresolved ER header projection");
        work.spend(entity.headers.front().size()+value.get("rows").size());
        for(const auto& line:entity.headers.front())work.spend(line.size());
        ErTableSpec table{entity.headers.front(),{}};
        value.get("rows").iter([&](JsonVal row){
            if(row.get_string("role")!="attribute")throw std::runtime_error("Nonattribute row in ER field facts");
            const auto fields=row.get("source_fields");
            work.spend(fields.get("type").as_str().size()+fields.get("name").as_str().size()+fields.get("comment").as_str().size()+fields.get("keys").size());
            ErTableAttribute attribute{fields.get_string("type"),fields.get_string("name"),{},fields.get_string("comment")};
            fields.get("keys").iter([&](JsonVal key){work.spend(key.as_str().size());attribute.keys.emplace_back(key.as_str());});
            if(attribute.type.empty()||attribute.name.empty())throw std::runtime_error("Incomplete ER typed source fields");
            table.attributes.push_back(std::move(attribute));
        });
        entity.typed_er=std::move(table);
    });
    if(index!=model.entities.size()){throw std::runtime_error("ER entity/source-field identity changed");}
    return model;
}

inline void er_present_require(bool ok,std::string_view error){if(!ok)throw std::runtime_error(std::string(error));}

inline void er_rich13_geometry_result(const std::vector<std::string>& errors){
    for(const auto& error:errors)if(error.find("exhausted")!=std::string::npos||error.find("limit exceeded")!=std::string::npos)throw std::runtime_error("ER rich13 resource failure is not a semantic rejection: "+error);
}

// One canonical body. Its callers own the precharge before any clone/mutation.
template<class Mutate>inline void er_rich13_rejection_core(const Grid& good,const Model& model,Mutate& mutate,Work& work,std::string_view token){
    auto wrong=model;mutate(wrong);
    // Keep the fallback member view coherent with the authoritative typed
    // attributes; a source mutation is not an inconsistent duplicate Model.
    for(auto& entity:wrong.entities)if(entity.typed_er){
        entity.headers={entity.typed_er->header};entity.rows.clear();
        for(const auto& attribute:entity.typed_er->attributes){
            work.spend(attribute.type.size()+attribute.name.size()+attribute.comment.size()+attribute.keys.size()*3+4);
            std::string flat=attribute.type+" "+attribute.name;for(const auto& key:attribute.keys){work.spend(key.size()+1);flat+=' ';flat+=key;}if(!attribute.comment.empty())flat+=" \""+attribute.comment+"\"";
            entity.rows.push_back({"attribute",{std::move(flat)}});
        }
    }
    const auto errors=validate_impl(good,wrong,work,false,nullptr,false,nullptr,token);er_rich13_geometry_result(errors);er_present_require(!errors.empty(),"ER rich13 accepted complete source mutation");
}

// Keep the original Native independent8M construction and debit unchanged.
template<class Mutate>inline void er_rich13_rejects(const Grid& good,const Model& model,Mutate mutate){
    Work work;work.spend(4u*131072u);
    er_rich13_rejection_core(good,model,mutate,work,kRelationsFollowingToken);
}
// Explicit caller-owned ledger: this entry never constructs a Work.
template<class Mutate>inline void er_rich13_rejects_paid(const Grid& good,const Model& model,Mutate& mutate,Work& work,std::string_view token){
    work.spend(4u*131072u);
    er_rich13_rejection_core(good,model,mutate,work,token);
}

struct Class481Mutations {std::map<std::string,int> executed,skipped;std::vector<std::string> errors;};

inline void class285_orphan_copy_prepay(const Grid& grid,Work& work){
    work.spend(grid.cells.size()*(sizeof(std::string)+16u)+256u);
    for(const auto& cell:grid.cells)work.spend(cell.size()*2u+1u);
}

// Local acquisition uses the established paid Screen tariff and the same Grid.
// It is not a second geometry checker or a Work adapter.
inline Grid note13_capture(ftxui::Element element,int width,int height,Work& work){
    work.spend(sizeof(ftxui::Screen)+16u*sizeof(std::size_t)+8u*sizeof(void*)+alignof(ftxui::Pixel)+alignof(std::vector<ftxui::Pixel>)+1024u);
    class481_require(width>0&&width<=240&&height>0&&height<=1040,"Note13 bounded public capture dimensions");
    const auto W=static_cast<std::size_t>(width),H=static_cast<std::size_t>(height),N=W*H;
    constexpr auto PX=sizeof(ftxui::Pixel),PV=sizeof(std::vector<ftxui::Pixel>);
    work.spend((H+1u)*PV+(H+1u)*W*PX+W*PX+N*2u*PX+H*2u*PV);
    work.spend((H+1u)*W*2u+W*4u+N*4u+(H+1u)*W*16u);
    auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(width),ftxui::Dimension::Fixed(height));ftxui::Render(screen,element);
    work.spend(4u*sizeof(Grid)+8u*sizeof(std::size_t)+256u+N*(3u*sizeof(std::string)+8u)+H*4u);
    Grid out{width,height,{}};out.cells.reserve(N);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){const auto& glyph=screen.PixelAt(x,y).character;work.spend(glyph.size()*4u+8u);out.cells.push_back(glyph);}
    return out;
}

// Disjoint finite call-local subshares of the existing16384 lifecycle tariff.
// Dynamic clone/regeneration storage and payload belong to the other terms in
// er206_negative_bound, not to either of these sizeof-based metadata bounds.
inline constexpr std::size_t kEr206OldLocalBound=64u*sizeof(void*)+64u*sizeof(std::size_t)+64u*sizeof(bool)+16u*sizeof(std::string_view)+16u*sizeof(std::string)+16u*sizeof(std::vector<std::string>)+8u*sizeof(Work)+1024u;
inline constexpr std::size_t kEr206ExtractedLocalBound=32u*sizeof(void*)+32u*sizeof(std::size_t)+32u*sizeof(bool)+16u*sizeof(std::string_view)+4u*sizeof(Work)+256u;
static_assert(kEr206OldLocalBound+kEr206ExtractedLocalBound<=16384u);

// New mutators reuse the existing ER helper's independent8M scope. Its initial
// 4*131072 charge owns all finite clone/regeneration/lambda bytes below; prove
// the real sizeof upper bound before calling it, never assume string ABI.
inline void er206_negative_bound(const Model& model,std::size_t encoded,Work& enumeration){
    enumeration.spend(4096u);std::size_t attributes=0;for(const auto& e:model.entities){enumeration.spend(128u);if(e.typed_er)attributes+=e.typed_er->attributes.size();}
    const auto E=model.entities.size(),R=model.relations.size(),G=model.groups.size(),T=attributes;
    const auto objects=sizeof(Model)+E*sizeof(Entity)+E*sizeof(ErTableSpec)+T*sizeof(ErTableAttribute)+R*sizeof(Relation)+G*sizeof(Group)+(8u*E+4u*T+2u*R+2u*G+16u)*sizeof(std::string)+(4u*E+3u*T+2u*G+16u)*sizeof(std::vector<std::string>);
    class481_require(encoded<=1849u&&E<=8u&&T<=6u&&R<=8u&&G<=2u&&16u*objects+(encoded+256u)*64u+16384u<=4u*131072u,"ER206 coherent mutation prepaid storage limit exceeded");
}

// Both paths use this exact Grid copy/paint/whole-validation body.
inline void er206_orphan_rejection_core(const Grid& good,const Model& model,Work& pixel,std::string_view token){
    pixel.spend(good.cells.size()*2u*sizeof(std::string)+8u*sizeof(Grid)+4096u);class285_orphan_copy_prepay(good,pixel);auto orphan=good;bool placed=false;
    for(int x=good.width-1;x>=0;--x){pixel.spend(32u);if(good.at({x,0})!=" ")continue;pixel.spend(256u);orphan.put({x,0},"┼");placed=true;break;}
    class481_require(placed,"ER206 no independent physical orphan cell");const auto extra=validate_impl(orphan,model,pixel,false,nullptr,false,nullptr,token);er_rich13_geometry_result(extra);class481_require(!extra.empty(),"ER206 physical orphan accepted");
}

template<bool SameLedger>inline Class481Mutations er206_mutations_core(const Grid& good,const Model& model,std::size_t encoded,Work& enumeration,std::string_view token){
    enumeration.spend(65536u+sizeof(Class481Mutations)*4u);er206_negative_bound(model,encoded,enumeration);Class481Mutations out;
    auto reject=[&](std::string_view kind,auto mutate){
        enumeration.spend(kind.size()*4u+4u*sizeof(std::string)+512u);
        if constexpr(SameLedger)er_rich13_rejects_paid(good,model,mutate,enumeration,token);
        else er_rich13_rejects(good,model,mutate);
        ++out.executed[std::string(kind)];
    };
    for(std::size_t e=0;e<model.entities.size();++e){
        reject("complete_header",[&](Model& wrong){auto& v=wrong.entities[e];if(v.typed_er)v.typed_er->header={"__ER206_WRONG__"};else v.headers={{"__ER206_WRONG__"}};v.label="__ER206_WRONG__";});
        if(model.entities[e].typed_er)for(std::size_t a=0;a<model.entities[e].typed_er->attributes.size();++a){
            reject("attribute_type",[&](Model& wrong){wrong.entities[e].typed_er->attributes[a].type="__ER206_WRONG__";});
            reject("attribute_name",[&](Model& wrong){wrong.entities[e].typed_er->attributes[a].name="__ER206_WRONG__";});
            reject("attribute_comment",[&](Model& wrong){wrong.entities[e].typed_er->attributes[a].comment="__ER206_WRONG__";});
            reject("attribute_keys",[&](Model& wrong){auto& keys=wrong.entities[e].typed_er->attributes[a].keys;if(keys.empty())keys.push_back("PK");else keys.front()=keys.front()=="PK"?"FK":"PK";});
            reject("attribute_erasure",[&](Model& wrong){auto& entity=wrong.entities[e];auto& list=entity.typed_er->attributes;list.erase(list.begin()+static_cast<std::ptrdiff_t>(a));if(list.empty()){entity.typed_er.reset();entity.rows.clear();}});
            const auto& keys=model.entities[e].typed_er->attributes[a].keys;
            // Empty->PK above already changes count/presence. The thirteen
            // source-owned nonempty arrays also require an independent erase.
            if(!keys.empty())reject("attribute_key_count",[&](Model& wrong){wrong.entities[e].typed_er->attributes[a].keys.clear();});
            if(keys.size()>1u&&keys[0]!=keys[1])reject("ordered_keys",[&](Model& wrong){auto& v=wrong.entities[e].typed_er->attributes[a].keys;std::swap(v[0],v[1]);});else{enumeration.spend(128u);++out.skipped["no_distinct_ordered_key_pair"];}
        }
        if(!model.entities[e].owner.empty())reject("group_membership",[&](Model& wrong){const auto owner=wrong.entities[e].owner;wrong.entities[e].owner.clear();for(auto& g:wrong.groups)if(g.id==owner)std::erase(g.members,wrong.entities[e].id);});
        else{enumeration.spend(128u);++out.skipped["root_entity_has_no_membership_to_erase"];}
        const auto& id=model.entities[e].id;enumeration.spend(model.relations.size()*512u+128u);const bool incident=std::ranges::any_of(model.relations,[&](const Relation& r){return r.from==id||r.to==id;});
        if(!incident)reject("isolated_entity_erasure",[&](Model& wrong){for(auto& g:wrong.groups)std::erase(g.members,id);wrong.entities.erase(wrong.entities.begin()+static_cast<std::ptrdiff_t>(e));});
        else{enumeration.spend(128u);++out.skipped["incident_entity_needs_coherent_graph_removal"];}
    }
    for(std::size_t r=0;r<model.relations.size();++r){
        reject("relation_label",[&](Model& wrong){wrong.relations[r].label="__ER206_WRONG__";});
        reject("start_cardinality",[&](Model& wrong){auto& v=wrong.relations[r].start;v=v=="card:one"?"card:zero-one":"card:one";});
        reject("end_cardinality",[&](Model& wrong){auto& v=wrong.relations[r].end;v=v=="card:one"?"card:zero-one":"card:one";});
        reject("stroke",[&](Model& wrong){auto& v=wrong.relations[r].stroke;v=v=="solid"?"dotted":"solid";});
        const auto& edge=model.relations[r];enumeration.spend(model.entities.size()*512u+model.relations.size()*1024u+128u);
        const auto third=std::ranges::find_if(model.entities,[&](const Entity& e){return e.id!=edge.from&&e.id!=edge.to;});
        if(third!=model.entities.end())reject("third_endpoint",[&](Model& wrong){wrong.relations[r].to=third->id;});else{enumeration.spend(128u);++out.skipped["no_third_entity"];}
        const auto same=std::ranges::count_if(model.relations,[&](const Relation& other){return other.from==edge.from&&other.to==edge.to&&other.label==edge.label&&other.start==edge.start&&other.end==edge.end&&other.stroke==edge.stroke;});
        if(same==1)reject("relation_erasure",[&](Model& wrong){wrong.relations.erase(wrong.relations.begin()+static_cast<std::ptrdiff_t>(r));for(std::size_t n=0;n<wrong.relations.size();++n)wrong.relations[n].order=static_cast<int>(n);});else{enumeration.spend(128u);++out.skipped["identical_duplicate_not_independent_ink"];}
    }
    for(std::size_t g=0;g<model.groups.size();++g){
        reject("group_caption",[&](Model& wrong){wrong.groups[g].label="__ER206_WRONG__";});
        if(!model.groups[g].parent.empty())reject("group_parent",[&](Model& wrong){wrong.groups[g].parent.clear();});else{enumeration.spend(128u);++out.skipped["root_group_has_no_parent_to_erase"];}
    }
    // This is a separate declared legacy8M physical negative, not a public
    // scene reset. Its actual Grid/vector/string payload copy is prepaid.
    enumeration.spend(4096u);
    if constexpr(SameLedger)er206_orphan_rejection_core(good,model,enumeration,token);
    else{Work pixel;er206_orphan_rejection_core(good,model,pixel,kRelationsFollowingToken);}
    ++out.executed["orphan_ink"];
    return out;
}

inline Class481Mutations er206_mutations(const Grid& good,const Model& model,std::size_t encoded,Work& enumeration){
    return er206_mutations_core<false>(good,model,encoded,enumeration,kRelationsFollowingToken);
}
// The explicit token overload is the only same-ledger opt-in. There is no
// second Work pointer, independent scope, reset, or additional budget here.
inline Class481Mutations er206_mutations(const Grid& good,const Model& model,std::size_t encoded,Work& enumeration,std::string_view token){
    enumeration.spend(16u*sizeof(std::string_view)+16u*sizeof(std::size_t)+8u*sizeof(bool)+256u);
    er_present_require(!token.empty()&&token.size()<=240u,"ER206 paid token bounds");
    enumeration.spend(token.size()*4u);
    for(const unsigned char value:token)er_present_require(value>=0x21&&value<=0x7e,"ER206 paid token printable non-SPACE ASCII");
    return er206_mutations_core<true>(good,model,encoded,enumeration,token);
}

} // namespace loom::test::mermaid_relations
