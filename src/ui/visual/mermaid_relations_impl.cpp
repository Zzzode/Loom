module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import loom.serdes.json;

namespace loom::ui::mermaid::detail {
namespace {
// UML and state diagrams share entity identity and orthogonal relationships;
// their grammars and endpoint decorations remain explicit below.
std::string_view trim_relation(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) return {};
    const auto last = value.find_last_not_of(" \t\r");
    return value.substr(first,last-first+1);
}
// Validate decoded label content independently of each grammar's delimiters.
std::optional<std::string> relation_literal_label(std::string_view value) {
    for (const char introducer : {'&','#'}) {
        for (auto at=value.find(introducer);at!=std::string_view::npos;at=value.find(introducer,at+1)) {
            const auto end=value.find(';',at+1);
            if (end!=std::string_view::npos && end>at+1 && value.substr(at+1,end-at-1).find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#")==std::string_view::npos) return {};
        }
    }
    std::string decoded;
    for (std::size_t n = 0; n < value.size();) {
        if (value.substr(n).starts_with("<br>")) { decoded += '\n'; n += 4; }
        else if (value.substr(n).starts_with("<br/>")) { decoded += '\n'; n += 5; }
        else if (value.substr(n).starts_with("<br />")) { decoded += '\n'; n += 6; }
        else { if (value[n] == '<' || value[n] == '>' || value[n] == '`' || value[n] == '\\') return {}; decoded += value[n++]; }
    }
    std::size_t cells = decoded.empty() ? 0 : label_rows(decoded).size()-1;
    bool visible = false;
    for (const auto row : label_rows(decoded)) {
        if (!row.empty() && !validated_plain_label(row)) return {};
        if (!row.empty()) visible = true;
        cells += ftxui::Utf8ToGlyphs(std::string(row)).size();
    }
    if (!visible || cells > kLabelLimit) return {};
    return decoded;
}
std::optional<std::string> relation_label(std::string_view value) {
    value = trim_relation(value);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1,value.size()-2);
    return relation_literal_label(value);
}
std::optional<std::string> identifier(std::string_view& rest,bool quoted = false) {
    rest = trim_relation(rest);
    if (rest.empty()) return {};
    if (quoted && rest.front() == '"') {
        const auto end = rest.find('"',1); if (end == std::string_view::npos) return {};
        std::string result(rest.substr(0,end+1)); rest = trim_relation(rest.substr(end+1)); return result;
    }
    std::size_t n = 0;
    while (n < rest.size()) {
        const unsigned char c = static_cast<unsigned char>(rest[n]);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c >= 0x80)) break;
        ++n;
    }
    if (!n) return {};
    std::string result(rest.substr(0,n)); if (!unicode_text(result)) return {};
    rest = trim_relation(rest.substr(n)); return result;
}

// Class-only token ownership. Dots remain literal Class identity; namespace
// components are split only by the namespace declaration below. Backtick IDs
// preserve physical LF rows without relaxing shared scalar validation.
std::optional<std::string> class_identifier(std::string_view& rest,Work& work,bool* backtick=nullptr) {
    rest=trim_relation(rest);if(rest.empty())return {};
    if(backtick)*backtick=false;
    if(!work.spend(rest.size()*(8+sizeof(std::string_view)+sizeof(std::string))+1))return {};
    if(rest.front()=='`') {
        const auto end=rest.find('`',1);if(end==std::string_view::npos||end==1)return {};
        const auto id=rest.substr(1,end-1);std::size_t cells=0;
        for(const auto row:label_rows(id)){if(row.empty()||!unicode_text(row))return {};cells+=ftxui::Utf8ToGlyphs(std::string(row)).size()+1;}
        if(cells>kLabelLimit)return {};
        if(backtick)*backtick=true;std::string result(id);rest=trim_relation(rest.substr(end+1));return result;
    }
    std::size_t n=0;
    while(n<rest.size()) {
        const auto c=static_cast<unsigned char>(rest[n]);
        if((c=='.'||c=='-')&&n+1<rest.size()&&rest[n+1]==c)break; // relationship, not Class identity
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c=='-'||c>=0x80))break;
        ++n;
    }
    if(!n||!unicode_text(rest.substr(0,n)))return {};
    std::string result(rest.substr(0,n));rest=trim_relation(rest.substr(n));return result;
}
std::optional<std::string> class_display_label(std::string_view,Work&,const std::string_view*);
// A suffix is source-owned only when physically adjacent to an unquoted ID.
struct ClassOperand { std::string id; std::optional<std::string_view> type; };
std::optional<ClassOperand> class_operand(std::string_view& rest,Work& work,std::string_view source,bool& source_checked) {
    if(!work.spend(4u*sizeof(ClassOperand)+8u*sizeof(std::string_view)+8u*sizeof(bool)+64u))return {};
    const auto original=trim_relation(rest);bool backtick=false;
    auto id=class_identifier(rest,work,&backtick);if(!id)return {};
    ClassOperand result{std::move(*id),{}};
    // class_identifier trims its remainder. Inspect the original token boundary
    // rather than accidentally accepting a separated `A ~U~` operand.
    if(backtick||original.size()<=result.id.size()||original[result.id.size()]!='~')return result;
    if(!source_checked){
        if(source.size()>kSourceLimit||!work.spend(source.size()*2u+8u*sizeof(std::size_t)+64u))return {};
        // The Class walker owns physical LF rows. A bare CR is an upstream
        // statement boundary, never whitespace joining an operand and edge.
        for(std::size_t index=0;index<source.size();++index)
            if(source[index]=='\r'&&(index+1u==source.size()||source[index+1u]!='\n'))return {};
        source_checked=true;
    }
    if(rest.size()>kSourceLimit||!work.spend(rest.size()*(16u+4u*sizeof(std::string_view)+4u*sizeof(std::string))+256u))return {};
    const auto end=rest.find('~',1);if(end==std::string_view::npos||end==1)return {};
    const auto payload=rest.substr(1,end-1);
    if(payload.find_first_of("~<>[]{}\"'`\\\r\n")!=std::string_view::npos)return {};
    const auto checked=class_display_label(payload,work,nullptr);
    if(!checked||*checked!=payload||checked->find('\n')!=std::string::npos)return {};
    result.type=payload;rest=trim_relation(rest.substr(end+1));return result;
}
std::optional<std::vector<std::string_view>> class_statement_lines(std::string_view source,Work& work) {
    if(!work.spend(source.size()*(4+2*sizeof(std::string_view))+1))return {};
    std::vector<std::string_view> result;bool member_body=false;
    while(!source.empty()) {
        const auto physical=source.find('\n');const auto first=trim_relation(source.substr(0,physical));
        // Exclusive MEMBER owns quotes/backticks as literal physical-row data.
        if(first.starts_with("%%")){if(physical==std::string_view::npos)break;source.remove_prefix(physical+1);continue;}
        std::size_t end=0;
        if(member_body){end=physical==std::string_view::npos?source.size():physical;}
        else{
            if(!work.spend(8u*sizeof(bool)+32u))return {};
            const bool declaration=first.starts_with("class ");bool backtick=false,alias=false,label=false;
            const bool note=first.size()>4u&&first.starts_with("note")&&(first[4]==' '||first[4]=='\t');
            bool note_string=false,note_closed=false;
            for(;end<source.size();++end){const char c=source[end];
                if(note_string){if(c=='"'){note_string=false;note_closed=true;}continue;}
                if(alias){if(c=='"')alias=false;continue;}
                if(backtick){if(c=='`')backtick=false;continue;}
                if(c=='\n')break;
                if(label)continue;
                if(c=='`'){backtick=true;continue;}
                if(note&&!note_closed&&c=='"'){note_string=true;continue;}
                if(declaration&&c=='['&&end+1<source.size()&&source[end+1]=='"'){alias=true;++end;continue;}
                if(c==':')label=true;
            }
            if(backtick||alias||note_string)return {};
        }
        const auto line=trim_relation(source.substr(0,end));if(!line.empty())result.push_back(line);
        if(member_body&&line=="}")member_body=false;
        else if(!member_body&&line.starts_with("class ")&&line.ends_with("{"))member_body=true;
        if(end==source.size())break;source.remove_prefix(end+1);
    }
    return result;
}
// Class NOTE STR is literal source text, not LABEL/HTML/Markdown decoding.
// Only physical CRLF normalization is shared with pinned preprocessing.
std::optional<std::string> class_note_text(std::string_view raw,std::string_view original,bool& source_checked,Work& work) {
    if(raw.empty()||raw.size()>kSourceLimit||!work.spend(raw.size()*(8u*sizeof(std::string)+4u*sizeof(std::string_view)+32u)+512u))return {};
    if(!source_checked){
        if(!work.spend(original.size()*2u+8u*sizeof(std::size_t)+64u))return {};
        for(std::size_t n=0;n<original.size();++n)
            if(original[n]=='\r'&&(n+1u==original.size()||original[n+1u]!='\n'))return {};
        source_checked=true;
    }
    std::string text;text.reserve(raw.size());
    for(std::size_t n=0;n<raw.size();++n){if(raw[n]=='\r'){if(n+1u==raw.size()||raw[n+1u]!='\n')return {};continue;}text.push_back(raw[n]);}
    if(text.size()>256u)return {};
    const auto rows=label_rows(text);std::size_t cells=4u;
    if(rows.size()>static_cast<std::size_t>(kHeightLimit-3))return {};
    for(const auto row:rows){
        if(!unicode_text(row))return {};
        for(const auto token:{std::string_view("\xe2\x80\x8b"),std::string_view("\xe2\x80\x8c"),std::string_view("\xe2\x80\x8d"),std::string_view("\xe2\x80\x8e"),std::string_view("\xe2\x80\x8f"),std::string_view("\xef\xbb\xbf")})if(row.find(token)!=std::string_view::npos)return {};
        const auto glyphs=ftxui::Utf8ToGlyphs(std::string(row));std::string roundtrip;
        for(const auto& glyph:glyphs)roundtrip+=glyph;
        if(roundtrip!=row||cells>kLabelLimit||glyphs.size()>kLabelLimit-cells)return {};
        cells+=glyphs.size();
    }
    return text;
}
// Class STR delimiters are removed by their declaration owner. LABEL/MEMBER
// outer quotes and Markdown punctuation remain data. No HTML/entity registry.
std::optional<std::string> class_display_label(std::string_view value,Work& work,const std::string_view* plain_angle_source=nullptr){
    if(value.size()>kSourceLimit||!work.spend(value.size()*(32u+2u*sizeof(std::string)+2u*sizeof(std::string_view))+128u))return {};
    std::string decoded;decoded.reserve(value.size());bool physical_checked=false;
    for(std::size_t at=0;at<value.size();){
        const auto tail=value.substr(at);std::size_t used=0;
        for(const auto token:{"<br>","<br/>","<br />"})if(tail.starts_with(token)){used=std::string_view(token).size();break;}
        if(used){decoded+='\n';at+=used;continue;}
        const char c=value[at];
        if(c=='<'||c=='>'){
            // Only central LABEL opts into source-literal standalone angles.
            // Existing lowercase BR tokens were consumed above; other tags,
            // entities and text owners keep their closed profile.
            if(!plain_angle_source||!work.spend(8))return {};
            if(!physical_checked) {
                // Raw CR is normalized by primary preprocessing before grammar.
                // Admit ordinary CRLF, but do not merge unnormalized bare-CR
                // statements after an operand trim. Decoded BR is not raw CR.
                if(plain_angle_source->size()>kSourceLimit||!work.spend(2u*plain_angle_source->size()+sizeof(bool)+1u))return {};
                for(std::size_t n=0;n<plain_angle_source->size();++n)
                    if((*plain_angle_source)[n]=='\r'&&(n+1==plain_angle_source->size()||(*plain_angle_source)[n+1]!='\n'))return {};
                physical_checked=true;
            }
            const auto boundary=[](char byte){return byte==' '||byte=='\t';};
            if((at&&!boundary(value[at-1]))||(at+1<value.size()&&!boundary(value[at+1])))return {};
        }
        if(c=='\\'||c=='\r')return {};
        if(c=='&'||c=='#'){
            auto end=at+1;while(end<value.size()){
                const auto next=static_cast<unsigned char>(value[end]);
                if(!((next>='a'&&next<='z')||(next>='A'&&next<='Z')||(next>='0'&&next<='9')||next=='#'))break;
                ++end;
            }
            if(end>at+1&&end<value.size()&&value[end]==';')return {};
            decoded.append(value.substr(at,end-at));at=end;continue;
        }
        decoded+=c;++at;
    }
    const auto rows=label_rows(decoded);std::size_t cells=rows.empty()?0:rows.size()-1;bool visible=false;
    for(const auto row:rows){
        if(row.empty())continue;
        if(row.find_first_not_of(' ')==std::string_view::npos)cells+=row.size();
        else{const auto checked=validated_plain_label(row);if(!checked)return {};cells+=static_cast<std::size_t>(ftxui::string_width(*checked));}
        visible=true;
    }
    if(!visible||cells>kLabelLimit)return {};
    return decoded;
}

// ER's terminal text profile keeps Markdown punctuation literal. Only the
// reviewed comment tag vocabulary is plain text; this is not HTML rendering.
// Prepay decoding, copies and glyph/row safety passes before allocation.
std::optional<std::string> er_literal_label(std::string_view value,Work& work,bool comment=false) {
    if(value.size()>kSourceLimit||!work.spend(value.size()*12+1))return {};
    std::string decoded;decoded.reserve(value.size());
    for(std::size_t at=0;at<value.size();) {
        const auto tail=value.substr(at);std::size_t used=0;
        for(const auto token:{"<br>","<br/>","<br />"})if(tail.starts_with(token)){used=std::string_view(token).size();break;}
        if(used){decoded+='\n';at+=used;continue;}
        if(comment)for(const auto token:{"<strong>","</strong>"})if(tail.starts_with(token)){used=std::string_view(token).size();break;}
        if(used){decoded.append(tail.substr(0,used));at+=used;continue;}
        const auto c=value[at];if(c=='<'||c=='>'||c=='\\')return {};
        if(c=='&'||c=='#') {
            auto end=at+1;
            while(end<value.size()) {
                const auto next=static_cast<unsigned char>(value[end]);
                if(!((next>='a'&&next<='z')||(next>='A'&&next<='Z')||(next>='0'&&next<='9')||next=='#'))break;
                ++end;
            }
            if(end>at+1&&end<value.size()&&value[end]==';')return {};
            decoded.append(value.substr(at,end-at));at=end;continue;
        }
        decoded+=c;++at;
    }
    const auto rows=label_rows(decoded);std::size_t cells=rows.empty()?0:rows.size()-1;bool visible=false;
    for(const auto row:rows) {
        if(!row.empty()&&!validated_plain_label(row))return {};
        if(!row.empty())visible=true;
        cells+=ftxui::Utf8ToGlyphs(std::string(row)).size();
    }
    if(!visible||cells>kLabelLimit)return {};
    return decoded;
}
enum class RelationFamily { Class, Er, State, Requirement };
enum class EntityShape { Box, Rounded, Choice, Start, Final, Fork, Join, Group, Note };
enum class Marker { None, Arrow, Triangle, FilledDiamond, OpenDiamond, One, ZeroOne, ZeroMany, OneMany, RequirementCirclePlus, RequirementOpenArrow };
struct Member { std::string text; bool method = false; };
struct ErAttribute { std::string type,name; std::vector<std::string> keys; std::string comment; };
struct RelationEntity {
    std::string id,label,annotation; EntityShape shape = EntityShape::Box;
    int owner = -1; std::vector<Member> members; bool declared = false;
    std::vector<ErAttribute> er_attributes={}; bool er_alias=false;
};
struct ClassEndpointText {std::string text;int w=0,h=0;};
struct ClassEndpointTexts {std::optional<ClassEndpointText> source,target;};
enum class RelationshipKind : unsigned char { Business, ClassNoteLeader };
struct Relationship { int from = -1,to = -1; Marker start = Marker::None,end = Marker::Arrow; bool dotted = false; std::string label; std::unique_ptr<ClassEndpointTexts> endpoint_texts; RelationshipKind kind=RelationshipKind::Business; };
// The new trailing tag can add alignment padding; do not call it free storage.
constexpr std::size_t kRelationshipKindStorage=sizeof(RelationshipKind)+alignof(Relationship)-1u;
struct PendingClassNote { int entity=-1;std::string target; };
const ClassEndpointText* class_caption(const Relationship& edge,bool source) {
    if(!edge.endpoint_texts)return nullptr;
    const auto& caption=source?edge.endpoint_texts->source:edge.endpoint_texts->target;
    return caption?&*caption:nullptr;
}
// Account only for newly introduced pointer allocation/moves/destruction, not
// a retrospective rewrite of the pre-existing Relationship allocation ledger.
bool append_relationship(std::vector<Relationship>& edges,Relationship&& edge,Work& work) {
    if(edges.size()>=128||!work.spend(2u*sizeof(std::unique_ptr<ClassEndpointTexts>)+2u*kRelationshipKindStorage))return false;
    if(edges.size()==edges.capacity()) {
        const auto capacity=std::min(std::size_t{128},std::max(std::size_t{1},edges.size()*2));
        if(!work.spend((sizeof(std::unique_ptr<ClassEndpointTexts>)+kRelationshipKindStorage)*(capacity+3u*edges.size())))return false;
        edges.reserve(capacity);
    }
    edges.push_back(std::move(edge));return true;
}
bool class_endpoint_text(std::string_view& rest,Relationship& edge,bool source,Work& work) {
    // Preserve the existing zero-visible-token STR behavior without inventing
    // visible text or discharging foreign presence/summary predicates.
    if(rest.starts_with("\"\"")){rest=trim_relation(rest.substr(2));return true;}
    if(rest.empty()||rest.front()!='"'||rest.size()>kSourceLimit||!work.spend(rest.size()+1))return false;
    const auto close=rest.find('"',1);if(close==std::string_view::npos)return false;
    auto decoded=class_display_label(trim_relation(rest.substr(1,close-1)),work);if(!decoded)return false;
    if(!work.spend(decoded->size()*(8u*sizeof(std::string_view)+4u)+3u*sizeof(ClassEndpointText)+16u))return false;
    const int width=label_width(*decoded),height=static_cast<int>(label_rows(*decoded).size());
    if(width<1||width>kWidthLimit-4||height<1||height>kHeightLimit-2)return false;
    if(!edge.endpoint_texts) {
        if(!work.spend(2u*sizeof(ClassEndpointTexts)+16u))return false;
        edge.endpoint_texts=std::make_unique<ClassEndpointTexts>();
    }
    auto& caption=source?edge.endpoint_texts->source:edge.endpoint_texts->target;
    if(caption)return false;
    caption.emplace(ClassEndpointText{std::move(*decoded),width,height});
    rest=trim_relation(rest.substr(close+1));return true;
}

struct RelationNote { int target = -1; bool right = true; std::string text; };
// Two source-owned bytes avoid per-entry string allocation. The vector is
// immutable before any effective-direction view is retained by layout/search.
struct StateScopeDirection { int owner=-1;std::array<char,2> value{}; };
struct StateTextRecord { int entity=-1;bool first_equals_default=false;std::vector<std::string> body; };
struct RelationDiagram {
    RelationFamily family = RelationFamily::Class; std::string direction = "TB";
    std::vector<RelationEntity> entities; std::vector<Relationship> edges; std::vector<RelationNote> notes;
    std::optional<std::string> diagram_title;
    std::optional<bool> class_hide_empty_members_box;
    bool class_namespaces=false;
    bool class_notes=false;
    std::vector<StateScopeDirection> state_scope_directions;
    std::vector<int> class_typed_entities;
    std::vector<StateTextRecord> state_texts;
    std::optional<std::string_view> state_direction(int owner,Work& work) const {
        // Empty/default paths have no ancestry scan or entry allocation.
        if(state_scope_directions.empty())return std::string_view(direction);
        if(family!=RelationFamily::State||!work.spend(3u*sizeof(std::optional<std::string_view>)+32u))return {};
        int at=owner;std::size_t depth=0;
        while(at>=0){
            if(!work.spend(16u)||depth++>=kGroupDepthLimit||static_cast<std::size_t>(at)>=entities.size()||
               static_cast<std::size_t>(at)>=64u+kGroupLimit||entities[static_cast<std::size_t>(at)].shape!=EntityShape::Group)return {};
            for(const auto& entry:state_scope_directions){
                if(!work.spend(sizeof(StateScopeDirection)+8u))return {};
                if(entry.owner==at)return std::string_view(entry.value.data(),entry.value.size());
            }
            at=entities[static_cast<std::size_t>(at)].owner;
        }
        if(at!=-1)return {};
        return std::string_view(direction);
    }
    // A size sentinel denotes no authored episode; nullopt denotes failure.
    std::optional<std::size_t> state_text_index(int target,Work& work) const {
        if(!work.spend(4u*sizeof(std::optional<std::size_t>)+32u)||family!=RelationFamily::State||target<0||static_cast<std::size_t>(target)>=entities.size())return {};
        for(std::size_t n=0;n<state_texts.size();++n){
            if(!work.spend(sizeof(int)+sizeof(bool)+8u))return {};
            if(state_texts[n].entity==target)return n;
        }
        return state_texts.size();
    }
    bool state_fragment(int target,std::string&& text,Work& work) {
        if(text.empty()||text.size()>kSourceLimit||!work.spend(4u*sizeof(std::string)+4u*sizeof(StateTextRecord)+text.size()*2u+64u))return false;
        const auto found=state_text_index(target,work);if(!found)return false;
        auto& entity=entities[static_cast<std::size_t>(target)];
        if(entity.shape!=EntityShape::Rounded||!entity.members.empty()||!entity.annotation.empty())return false;
        if(*found==state_texts.size()){
            if(state_texts.size()>=64u||!work.spend(entity.label.size()+text.size()+1u))return false;
            const bool equal=text==entity.label;
            if(state_texts.empty()){
                if(!work.spend(64u*sizeof(StateTextRecord)+alignof(StateTextRecord)+3u*sizeof(std::vector<StateTextRecord>)+32u))return false;
                state_texts.reserve(64u);
            }
            if(!work.spend(3u*sizeof(StateTextRecord)+2u*sizeof(std::vector<std::string>)+2u*sizeof(std::string)+text.size()*2u+64u))return false;
            state_texts.push_back({target,equal,{}});entity.label=std::move(text);return true;
        }
        auto& record=state_texts[*found];
        if(!work.spend(entity.label.size()+text.size()+16u))return false;
        if(record.first_equals_default||entity.label.size()>256u||text.size()>256u||
           entity.label.find('\n')!=std::string::npos||text.find('\n')!=std::string::npos||record.body.size()>=128u)return false;
        if(record.body.size()==record.body.capacity()){
            const auto capacity=std::min(std::size_t{128},std::max(std::size_t{1},record.body.size()*2u));
            if(!work.spend((capacity+2u*record.body.size()+1u)*sizeof(std::string)+alignof(std::string)+32u))return false;
            record.body.reserve(capacity);
        }
        if(!work.spend(text.size()*2u+3u*sizeof(std::string)+32u))return false;
        record.body.push_back(std::move(text));return true;
    }
    int find(std::string_view id) const { for (std::size_t n = 0; n < entities.size(); ++n) if (entities[n].id == id) return static_cast<int>(n); return -1; }

    int class_find(std::string_view id,bool group,Work& work) const {
        for(std::size_t n=0;n<entities.size();++n){if(!work.spend(id.size()+entities[n].id.size()+2u))return -1;if(entities[n].shape!=EntityShape::Note&&(entities[n].shape==EntityShape::Group)==group&&entities[n].id==id)return static_cast<int>(n);}return -1;
    }
    std::optional<bool> class_typed(int target,Work& work) const {
        if(!work.spend(4u*sizeof(std::optional<bool>)+4u*sizeof(int)+32u)||target<0||
           static_cast<std::size_t>(target)>=entities.size()||entities[static_cast<std::size_t>(target)].shape==EntityShape::Group)return {};
        for(const int at:class_typed_entities){if(!work.spend(sizeof(int)+8u))return {};if(at==target)return true;}
        return false;
    }
    int class_entity(const std::string& id,int owner,Work& work,bool declaration=false,std::optional<std::string_view> type={}) {
        const int old=class_find(id,false,work);if(work.remaining==0)return -1;
        if(old>=0){if(declaration&&!entities[static_cast<std::size_t>(old)].declared)entities[static_cast<std::size_t>(old)].owner=owner;return old;}
        if(entities.size()>=64+kGroupLimit||!work.spend(id.size()*4+sizeof(RelationEntity)+1))return -1;
        auto label=relation_label(id);if(!label)return -1;
        if(type){
            if(type->empty()||type->size()>kSourceLimit||label->size()>kSourceLimit||class_typed_entities.size()>=64u)return -1;
            const auto bytes=label->size()+type->size()+2u;
            if(!work.spend(type->size()*8u+bytes*(16u+4u*sizeof(std::string)+4u*sizeof(std::string_view))+256u))return -1;
            label->push_back('<');label->append(*type);label->push_back('>');
            if(label->find('\n')!=std::string::npos||ftxui::Utf8ToGlyphs(*label).size()>kLabelLimit)return -1;
            if(class_typed_entities.empty()){
                if(!work.spend(64u*sizeof(int)+alignof(int)+4u*sizeof(std::vector<int>)+64u))return -1;
                class_typed_entities.reserve(64u);
            }
            if(!work.spend(4u*sizeof(int)+32u))return -1;
        }
        entities.push_back({std::string(id),*label,{},EntityShape::Box,owner,{},false});
        const int target=static_cast<int>(entities.size()-1);
        if(type)class_typed_entities.push_back(target);
        return target;
    }
    int class_namespace(std::string_view id,std::string_view label,int owner,Work& work,bool declared) {
        const int old=class_find(id,true,work);if(work.remaining==0)return -1;
        if(old>=0){auto& group=entities[static_cast<std::size_t>(old)];if(group.owner!=owner)return -1;if(declared){if(group.declared&&group.label!=label)return -1;if(!work.spend(label.size()+1))return -1;group.label=std::string(label);group.declared=true;}return old;}
        if(entities.size()>=64+kGroupLimit||!work.spend(id.size()+label.size()+sizeof(RelationEntity)+1))return -1;
        entities.push_back({std::string(id),std::string(label),{},EntityShape::Group,owner,{},declared});class_namespaces=true;return static_cast<int>(entities.size()-1);
    }
    int control(EntityShape shape,int owner,Work& work) {
        // Anonymous [*] is one start or final per non-concurrent State scope.
        // Its private NUL-prefixed identity cannot match an authored state ID.
        if(!work.spend(4u*sizeof(int)+4u*sizeof(std::size_t)+64u))return -1;
        for(std::size_t n=0;n<entities.size();++n) {
            // Only owner/shape and the first ID byte are inspected; no ID copy
            // or full string scan is needed to distinguish anonymous controls.
            if(!work.spend(6u*sizeof(int)+2u*sizeof(std::string_view)+32u))return -1;
            const auto& entity=entities[n];
            if(entity.owner==owner&&entity.shape==shape&&!entity.id.empty()&&entity.id.front()=='\0')return static_cast<int>(n);
        }
        if (entities.size() >= 64+kGroupLimit) return -1;
        // No valid source identifier can contain NUL. Anonymous controls cannot
        // collide with even an explicitly quoted user state name.
        std::string id(1,'\0');id += std::to_string(entities.size());
        entities.push_back({std::move(id),{},{},shape,owner,{},false});
        return static_cast<int>(entities.size()-1);
    }
    int entity(std::string id,int owner,Work* er_work=nullptr) {
        const int old = find(id); if (old >= 0) return old;
        if (entities.size() >= 64+kGroupLimit) return -1;
        const auto label = family==RelationFamily::Er&&er_work?er_literal_label(id,*er_work):relation_label(id); if (!label) return -1;
        entities.push_back({std::move(id),*label,{},family == RelationFamily::State ? EntityShape::Rounded : EntityShape::Box,owner,{},false});
        return static_cast<int>(entities.size()-1);
    }
};
// ER attributes retain their physical field roles independently of Class members.
std::string er_keys_text(const std::vector<std::string>& keys) {
    std::string text;
    for (const auto& key:keys) { if (!text.empty()) text+=',';text+=key; }
    return text;
}
std::array<std::string,4> er_attribute_fields(const ErAttribute& attribute) {
    return {attribute.type,attribute.name,er_keys_text(attribute.keys),attribute.comment};
}
bool er_attribute_budget(const ErAttribute& attribute,Work& work) {
    if (attribute.type.empty()||attribute.name.empty()||attribute.keys.size()>85) return false;
    if (!work.spend(attribute.type.size()+attribute.name.size()+attribute.comment.size()+attribute.keys.size()*3)) return false;
    const auto fields=er_attribute_fields(attribute);
    std::size_t cells=1+(!attribute.keys.empty()?1:0)+(!attribute.comment.empty()?1:0);
    for (const auto& field:fields) {
        const auto rows=label_rows(field);cells+=rows.size()-1;
        for (const auto row:rows) {
            if (!work.spend(row.size()+1)) return false;
            cells+=ftxui::Utf8ToGlyphs(std::string(row)).size();
        }
    }
    return cells<=kLabelLimit;
}
struct ErTableGeometry {
    std::vector<int> roles,widths,heights;
    int width=0,height=0;
};
std::optional<ErTableGeometry> er_table_geometry(const RelationEntity& entity,Work& work) {
    if (entity.er_attributes.empty()||entity.er_attributes.size()>128) return {};
    ErTableGeometry table;table.roles={0,1};
    if (std::ranges::any_of(entity.er_attributes,[](const auto& attribute){return !attribute.keys.empty();})) table.roles.push_back(2);
    if (std::ranges::any_of(entity.er_attributes,[](const auto& attribute){return !attribute.comment.empty();})) table.roles.push_back(3);
    table.widths.assign(table.roles.size(),3);table.height=2+static_cast<int>(label_rows(entity.label).size());
    for (const auto& attribute:entity.er_attributes) {
        if (!er_attribute_budget(attribute,work)) return {};
        const auto fields=er_attribute_fields(attribute);int height=1;
        for (std::size_t column=0;column<table.roles.size();++column) {
            const auto& field=fields[static_cast<std::size_t>(table.roles[column])];
            height=std::max(height,static_cast<int>(label_rows(field).size()));
            table.widths[column]=std::max(table.widths[column],label_width(field)+2);
        }
        table.heights.push_back(height);table.height+=height+1;
    }
    table.width=1;for (const int width:table.widths) table.width+=width+1;
    const int header=label_width(entity.label)+2;
    if (header>table.width) {table.widths.back()+=header-table.width;table.width=header;}
    if (table.width>kWidthLimit||table.height>kHeightLimit) return {};
    return table;
}
bool er_ascii_equal(std::string_view left,std::string_view right) {
    if (left.size()!=right.size()) return false;
    for (std::size_t n=0;n<left.size();++n) {
        auto c=left[n];if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
        auto d=right[n];if(d>='A'&&d<='Z')d=static_cast<char>(d-'A'+'a');
        if(c!=d)return false;
    }
    return true;
}
std::size_t er_metadata_space(std::string_view text);
struct ErCursor {
    std::string_view source;Work& work;std::size_t at=0;bool healthy=true,attribute_block=false;
    bool spend(std::size_t count=1) {if(!healthy||!work.spend(count)){healthy=false;return false;}return true;}
    static bool word(unsigned char c) {return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c>=0x80;}
    // Common preprocessing owns only whole %% comment lines. Inside the
    // exclusive attribute block, inline %% bytes may instead belong to the
    // earlier greedy generic WORD. Every cursor entry uses this same test.
    bool attribute_comment() {
        if(source.size()-at<=2||source[at+2]=='\n'||source[at+2]=='\r')return false;
        auto begin=at;
        while(begin) {
            if(!spend())return false;
            const auto last=static_cast<unsigned char>(source[begin-1]);
            if(last=='\n'||last=='\r')break;
            if(begin>=3&&(last==0xa8||last==0xa9)) {
                if(!spend(3))return false;
                if(source.substr(begin-3,2)=="\xe2\x80")break;
            }
            --begin;
        }
        for(auto n=begin;n<at;) {
            const auto size=attribute_space(n);
            if(!healthy||!size||size>at-n)return false;
            n+=size;
        }
        return true;
    }
    bool skip() {
        while(at<source.size()) {
            if(source[at]==' '||source[at]=='\t'||source[at]=='\r'||source[at]=='\n'){if(!spend())return false;++at;continue;}
            // Unsupported directives stay closed before comment ownership.
            // Quoted/generic token contents never re-enter skip() internally.
            if(source.substr(at).starts_with("%%{")){healthy=false;return false;}
            if(source.substr(at).starts_with("%%")&&(!attribute_block||attribute_comment())) {
                while(at<source.size()&&source[at]!='\n'&&(!attribute_block||source[at]!='\r')){if(!spend())return false;++at;}
                continue;
            }
            if(!healthy)return false;
            break;
        }
        return healthy;
    }
    bool take(std::string_view token,bool boundary=false) {
        if(!skip()||!spend(token.size()))return false;
        if(source.size()-at<token.size()||!er_ascii_equal(source.substr(at,token.size()),token))return false;
        if(boundary&&at+token.size()<source.size()&&word(static_cast<unsigned char>(source[at+token.size()])))return false;
        at+=token.size();return true;
    }
    char peek() {if(!skip()||!spend()||at==source.size())return '\0';return source[at];}
    std::optional<std::string_view> quoted(char quote) {
        if(!skip()||at==source.size()||source[at]!=quote)return {};
        const auto start=++at;
        while(at<source.size()&&source[at]!=quote) {
            if(source[at]=='\n'||source[at]=='\r'||source[at]=='\\'||!spend())return {};
            ++at;
        }
        if(at==source.size()||!spend())return {};
        const auto value=source.substr(start,at-start);++at;return value;
    }
    std::optional<std::string> name() {
        if(!skip())return {};
        std::string_view value;
        if(peek()=='"') {const auto text=quoted('"');if(!text||text->find('%')!=std::string_view::npos||trim_relation(*text)!=*text)return {};value=*text;}
        else {
            const auto start=at;
            while(at<source.size()) {
                const auto c=static_cast<unsigned char>(source[at]);
                if(!word(c)&&c!='-'&&c!='.'&&c!='*')break;
                if(!spend())return {};
                ++at;
            }
            value=source.substr(start,at-start);
            for(const auto reserved:{"erDiagram","style","classDef","class","subgraph","end","one","many","to"})
                if(er_ascii_equal(value,reserved))return {};
        }
        const auto label=er_literal_label(value,work);if(!label)return {};
        return std::string(value);
    }
    bool attribute_skip() {
        if(!skip())return false;
        while(at<source.size()) {
            const auto size=attribute_space(at);if(!healthy)return false;
            if(!size)break;
            at+=size;if(!skip())return false;
        }return healthy;
    }
    bool attribute_key_prefix() {
        if(!attribute_skip())return false;
        for(const auto key:{"PK","FK","UK"}) {
            if(!spend(3))return false;
            if(source.size()-at<2||!er_ascii_equal(source.substr(at,2),key))continue;
            const auto next=source.size()-at>2?static_cast<unsigned char>(source[at+2]):0;
            if(!((next>='a'&&next<='z')||(next>='A'&&next<='Z')||(next>='0'&&next<='9')||next=='_'))return true;
        }return false;
    }
    std::optional<std::string> attribute_key() {
        if(!attribute_key_prefix()||!healthy)return {};
        const auto token=source.substr(at,2);at+=2;
        for(const auto key:{"PK","FK","UK"})if(er_ascii_equal(token,key))return std::string(key);
        return {};
    }
    std::size_t attribute_space(std::size_t pos) {
        if(!spend())return 0;
        if(static_cast<unsigned char>(source[pos])<0x80)return std::string_view(" \t\r\n\v\f").find(source[pos])==std::string_view::npos?0:1;
        if(!spend(31))return 0;
        return er_metadata_space(source.substr(pos));
    }
    // Ordered block lexer: KEY, greedy generic WORD, normal WORD, backtick
    // state, then COMMENT. A probe skips only whitespace; the owning WORD
    // advances the matched token bytes.
    std::optional<std::string_view> generic_attribute_word() {
        if(!attribute_skip())return {};
        auto first=at;
        for(;first<source.size();++first) {
            if(attribute_space(first)||!healthy)return {};
            if(source[first]=='~')break;
        }
        if(first==source.size())return {};
        auto last=std::string_view::npos;
        for(auto n=first+1;n<source.size();++n) {
            if(!spend(8))return {};
            const auto tail=source.substr(n);
            if(source[n]=='\r'||source[n]=='\n'||tail.starts_with("\xe2\x80\xa8")||tail.starts_with("\xe2\x80\xa9"))break;
            if(source[n]=='~')last=n;
        }
        if(last==std::string_view::npos)return {};
        auto end=last+1;
        while(end<source.size()) {
            if(attribute_space(end)||!healthy)break;
            ++end;
        }
        if(!healthy)return {};
        return source.substr(at,end-at);
    }
    std::optional<std::string> attribute_word(bool type) {
        if(attribute_key_prefix()||!healthy)return {};
        std::string_view value;
        if(const auto generic=generic_attribute_word()){value=*generic;at+=value.size();}
        else if(!healthy)return {};
        else if(peek()=='`') {const auto text=quoted('`');if(!text)return {};value=*text;}
        else {
            if(at==source.size())return {};
            const auto first=static_cast<unsigned char>(source[at]);
            if(!((first>='a'&&first<='z')||(first>='A'&&first<='Z')||first=='_'||first=='*'||first>=0x80))return {};
            const auto start=at;
            while(at<source.size()) {
                const auto c=static_cast<unsigned char>(source[at]);
                if(!word(c)&&std::string_view("-[]().,*").find(static_cast<char>(c))==std::string_view::npos)break;
                if(!spend())return {};
                ++at;
            }
            value=source.substr(start,at-start);
        }
        const auto label=er_literal_label(value,work);if(!label)return {};
        std::string result=*label;
        if(type) {
            if(!attribute_skip())return {};
            if(at<source.size()&&source[at]=='?') {
                const auto generic=generic_attribute_word();if(!healthy)return {};
                if(!generic&&take("?"))result+='?';
            }
        }
        return result;
    }
};
// Known node paint and accessibility metadata have no terminal glyph effect.
// Parse their complete owning statement; unknown fields retain the full source.
bool er_presentation_ids(std::string_view text,Work& work,bool initial=false) {
    if(text.empty())return false;
    while(!text.empty()) {
        const auto comma=text.find(',');const auto item=trim_relation(text.substr(0,comma));
        if(item.empty()||!work.spend(item.size()+1)||!unicode_text(item))return false;
        for(const auto value:item) {
            const auto c=static_cast<unsigned char>(value);
            if(!ErCursor::word(c)&&c!='-'&&c!='.'&&c!='*')return false;
        }
        if(initial) {
            for(const auto keyword:{"erDiagram","style","classDef","class","subgraph","end","one","many","to"})
                if(er_ascii_equal(item,keyword))return false;
            const auto dot=item.find('.');
            if(item.find_first_not_of("0123456789")==std::string_view::npos)return false;
            if(dot!=std::string_view::npos&&dot>0&&dot+1<item.size()&&
               item.substr(0,dot).find_first_not_of("0123456789")==std::string_view::npos&&
               item.substr(dot+1).find_first_not_of("0123456789")==std::string_view::npos)return false;
        }else if(item.find('.')!=std::string_view::npos)return false;
        if(comma==std::string_view::npos)return true;
        text=text.substr(comma+1);if(text.empty())return false;
    }
    return false;
}
bool er_presentation_number(std::string_view text,bool positive) {
    // STYLE's token/grammar set does not admit a decimal dot, even where
    // ordinary CSS would. Numeric spellings must satisfy both grammars.
    if(text.find('.')!=std::string_view::npos)return false;
    double value=0;const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    return !text.empty()&&result.ec==std::errc{}&&result.ptr==text.data()+text.size()&&
        std::isfinite(value)&&(positive?value>0:value>=0);
}
bool er_presentation_css(std::string_view text,Work& work) {
    text=trim_relation(text);
    if(text.ends_with(';'))text=trim_relation(text.substr(0,text.size()-1));
    if(text.empty())return false;
    while(!text.empty()) {
        const auto comma=text.find(',');const auto field=trim_relation(text.substr(0,comma));
        if(!work.spend(field.size()+1))return false;
        const auto colon=field.find(':');if(colon==std::string_view::npos)return false;
        const auto key=trim_relation(field.substr(0,colon)),value=trim_relation(field.substr(colon+1));
        if(value.empty())return false;
        if(key=="fill"||key=="stroke"||key=="color") {
            if(value.front()=='#') {
                const auto digits=value.substr(1);
                if(digits.size()!=3&&digits.size()!=6)return false;
                if(digits.find_first_not_of("0123456789abcdefABCDEF")!=std::string_view::npos)return false;
            }else if(value!="red")return false;
        }else if(key=="stroke-width") {
            auto number=value;if(!number.ends_with("px"))return false;number.remove_suffix(2);
            if(!er_presentation_number(number,true))return false;
        }else if(key=="stroke-dasharray") {
            auto rest=value;
            while(!rest.empty()) {
                const auto end=rest.find_first_of(" \t");
                if(!er_presentation_number(rest.substr(0,end),false))return false;
                if(end==std::string_view::npos)break;
                rest=trim_relation(rest.substr(end));
            }
        }else return false;
        if(comma==std::string_view::npos)return true;
        text=trim_relation(text.substr(comma+1));if(text.empty())return false;
    }
    return false;
}
bool er_presentation_line(ErCursor& cursor,bool class_binding) {
    auto end=cursor.at;
    if(!cursor.spend())return false;
    while(end<cursor.source.size()&&cursor.source[end]!='\n') {
        if(!cursor.spend())return false;++end;
    }
    const auto raw=cursor.source.substr(cursor.at,end-cursor.at);
    if(raw.empty()||(raw.front()!=' '&&raw.front()!='\t')||!cursor.spend(raw.size()*3+16))return false;
    const auto line=trim_relation(raw);const auto split=line.find_first_of(" \t");
    if(split==std::string_view::npos)return false;
    const auto targets=trim_relation(line.substr(0,split)),values=trim_relation(line.substr(split));
    if(!er_presentation_ids(targets,cursor.work,class_binding))return false;
    if(class_binding?!er_presentation_ids(values,cursor.work,true):!er_presentation_css(values,cursor.work))return false;
    cursor.at=end;return true;
}
bool er_presentation_keyword(ErCursor& cursor,std::string_view keyword) {
    const auto saved=cursor.at;
    if(!cursor.take(keyword,true))return false;
    if(cursor.at<cursor.source.size()) {
        const auto c=cursor.source[cursor.at];
        if(c!=' '&&c!='\t'&&c!='\r'&&c!='\n'){cursor.at=saved;return false;}
    }
    return true;
}
// ECMAScript's fixed whitespace set owns accessibility introducers and the
// generic ATTRIBUTE_WORD prefix/suffix. No shared Unicode grammar is changed.
std::size_t er_metadata_space(std::string_view text) {
    if(text.empty())return 0;
    if(std::string_view(" \t\n\v\f\r").find(text.front())!=std::string_view::npos)return 1;
    for(const auto value:{std::string_view("\xc2\xa0"),std::string_view("\xe1\x9a\x80"),
        std::string_view("\xe2\x80\xa8"),std::string_view("\xe2\x80\xa9"),std::string_view("\xe2\x80\xaf"),
        std::string_view("\xe2\x81\x9f"),std::string_view("\xe3\x80\x80"),std::string_view("\xef\xbb\xbf")})
        if(text.starts_with(value))return value.size();
    if(text.size()>=3&&static_cast<unsigned char>(text[0])==0xe2&&static_cast<unsigned char>(text[1])==0x80&&
       static_cast<unsigned char>(text[2])>=0x80&&static_cast<unsigned char>(text[2])<=0x8a)return 3;
    return 0;
}
// Return0 for an ordinary entity name,1 for consumed metadata,-1 for malformed
// metadata. The lexical introducer may contain NEWLINE or fixed Unicode space;
// quoted data and longer identifiers remain in their existing owning grammar.
int er_accessibility(ErCursor& cursor) {
    const auto saved=cursor.at;
    if(!cursor.spend(8))return -1;
    const auto token=cursor.source.substr(saved,8);bool description=false;
    if(er_ascii_equal(token,"accTitle"))description=false;
    else if(er_ascii_equal(token,"accDescr"))description=true;
    else return 0;
    auto at=saved+8;
    while(const auto size=er_metadata_space(cursor.source.substr(at))) {
        if(!cursor.spend(size))return -1;at+=size;
    }
    if(at==cursor.source.size()||(cursor.source[at]!=':'&&(!description||cursor.source[at]!='{')))return 0;
    const bool block=cursor.source[at]=='{';const auto begin=at+1;
    auto end=begin;
    if(!cursor.spend())return -1;
    while(end<cursor.source.size()&&cursor.source[end]!=(block?'}':'\n')) {
        if(!cursor.spend())return -1;++end;
    }
    if(block&&end==cursor.source.size())return -1;
    const auto raw=cursor.source.substr(begin,end-begin);
    if(!cursor.spend(raw.size()*3+16))return -1;
    // This closed profile contains safe plain metadata. Markup, escapes and
    // entities require a separate source contract rather than silent omission.
    if(raw.find_first_of("<>&\\`")!=std::string_view::npos||trim_relation(raw).empty())return -1;
    for(auto row:label_rows(raw))if(!row.empty()&&!unicode_text(row))return -1;
    cursor.at=end+(block?1:0);return 1;
}
bool er_inline_classes(ErCursor& cursor,std::size_t entity_end) {
    auto at=cursor.at;
    while(at<cursor.source.size()&&(cursor.source[at]==' '||cursor.source[at]=='\t'||cursor.source[at]=='\r')) {
        if(!cursor.spend())return false;++at;
    }
    if(!cursor.source.substr(at).starts_with(":::"))return true;
    // Earlier optional alias probes may have skipped ordinary whitespace.
    // An inline class cannot adopt an entity on a different physical line.
    if(cursor.source.substr(entity_end,at-entity_end).find('\n')!=std::string_view::npos)return false;
    if(!cursor.spend(3))return false;
    at+=3;
    while(true) {
        while(at<cursor.source.size()&&(cursor.source[at]==' '||cursor.source[at]=='\t'||cursor.source[at]=='\r')) {
            if(!cursor.spend())return false;++at;
        }
        const auto begin=at;
        while(at<cursor.source.size()) {
            const auto c=static_cast<unsigned char>(cursor.source[at]);
            if(!ErCursor::word(c)&&c!='-'&&c!='.'&&c!='*')break;
            if(!cursor.spend())return false;++at;
        }
        if(!er_presentation_ids(cursor.source.substr(begin,at-begin),cursor.work,true))return false;
        while(at<cursor.source.size()&&(cursor.source[at]==' '||cursor.source[at]=='\t'||cursor.source[at]=='\r')) {
            if(!cursor.spend())return false;++at;
        }
        if(at==cursor.source.size()||cursor.source[at]!=','){cursor.at=at;return true;}
        if(!cursor.spend())return false;++at;
    }
}

std::optional<Marker> er_cardinality(ErCursor& cursor) {
    if(!cursor.skip()||!cursor.spend(2))return {};
    // A healthy terminal literal 1 is an entity token, not a relation marker.
    if(!cursor.spend(sizeof(std::size_t)+sizeof(bool)+8u))return {};
    if(cursor.at<cursor.source.size()&&cursor.source[cursor.at]=='1') {
        if(!cursor.spend(sizeof(std::size_t)+3u*sizeof(bool)+8u))return {};
        const auto saved=cursor.at;
        ++cursor.at;
        const bool scanned=cursor.skip();
        const bool terminal=scanned&&cursor.healthy&&cursor.at==cursor.source.size();
        cursor.at=saved;
        if(!scanned||!cursor.healthy)return {};
        if(terminal)return {};
    }
    if(cursor.source.size()-cursor.at>=2&&(cursor.source[cursor.at]=='u'||cursor.source[cursor.at]=='U')&&
       std::string_view(".-|").find(cursor.source[cursor.at+1])!=std::string_view::npos){++cursor.at;return Marker::FilledDiamond;}
    struct Entry {std::string_view text;Marker marker;bool word;};
    constexpr Entry entries[]{
        {"one or zero",Marker::ZeroOne,true},{"zero or one",Marker::ZeroOne,true},
        {"one or more",Marker::OneMany,true},{"one or many",Marker::OneMany,true},
        {"zero or more",Marker::ZeroMany,true},{"zero or many",Marker::ZeroMany,true},
        {"only one",Marker::One,true},{"many(0)",Marker::ZeroMany,false},{"many(1)",Marker::OneMany,false},
        {"many",Marker::ZeroMany,true},{"one",Marker::One,true},{"1+",Marker::OneMany,false},{"0+",Marker::ZeroMany,false},
        {"||",Marker::One,false},{"o|",Marker::ZeroOne,false},{"|o",Marker::ZeroOne,false},
        {"o{",Marker::ZeroMany,false},{"}o",Marker::ZeroMany,false},{"|{",Marker::OneMany,false},{"}|",Marker::OneMany,false},
        {"1",Marker::One,true}};
    for(const auto& entry:entries)if(cursor.take(entry.text,entry.word))return entry.marker;
    return {};
}
// ER group ownership is assigned only when a complete group closes. Frames
// retain statement results; nested groups contribute their own identity only.
struct ErGroupFrame {int group=-1;std::size_t count=0;std::array<int,64> members{};};
struct ErGroupParse {
    std::array<ErGroupFrame,kGroupDepthLimit> frames{};
    std::array<bool,64> claimed{};
    std::size_t depth=0,count=0;
};
// Newly activated Group operands prepay their existing decoder's temporary
// strings, glyphs, row vectors and copies before calling the unchanged lexer.
// This bound is one physical token row, not a copied or reparsed document.
bool er_group_operand(ErCursor& cursor,bool attribute=false) {
    if(!(attribute?cursor.attribute_skip():cursor.skip()))return false;
    if(!cursor.spend(4u*sizeof(std::size_t)+4u*sizeof(bool)+32u))return false;
    auto end=cursor.at;
    while(end<cursor.source.size()) {
        if(!cursor.spend())return false;
        if(cursor.source[end]=='\r'||cursor.source[end]=='\n')break;
        ++end;
    }
    const auto bytes=end-cursor.at;
    return cursor.spend(bytes*(24u*sizeof(std::string)+16u*sizeof(std::string_view)+96u)+
        8u*sizeof(std::string)+4u*sizeof(std::vector<std::string_view>)+128u);
}
// A finite INITIAL-token guard for the newly activated Grouped domain.
// Nongroup calls record unsafe ambiguity but retain the old parser result.
bool er_group_token(std::string_view raw,std::string_view tail,ErCursor& cursor) {
    if(!cursor.spend(8u*sizeof(std::string_view)+8u*sizeof(std::size_t)+64u))return false;
    if(raw.empty())return false;
    if(raw.front()=='"')return true;
    if(static_cast<unsigned char>(raw.front())>=0x80)return true;
    bool ascii_word=true;
    for(const auto c:raw){
        if(!cursor.spend())return false;
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')){ascii_word=false;break;}
    }
    // The unchanged name() already rejected exact reserved words. ASCII word
    // suffixes cannot introduce a generated JavaScript word boundary.
    if(ascii_word&&!(raw.front()>='0'&&raw.front()<='9'))return true;
    if(raw.front()>='0'&&raw.front()<='9') {
        if(raw=="1"){
            if(!cursor.spend(4u*sizeof(std::size_t)+4u*sizeof(bool)+32u))return false;
            if(tail.starts_with("--")||tail.starts_with("..")||tail.starts_with(".-")||tail.starts_with("-."))return false;
            std::size_t at=0;bool separated=false;
            while(at<tail.size()){
                if(!cursor.spend(32u))return false;
                const auto space=er_metadata_space(tail.substr(at));if(!space)break;
                at+=space;separated=true;
            }
            if(separated&&at<tail.size()){
                const auto c=static_cast<unsigned char>(tail[at]);
                if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='"'||c=='\'')return false;
            }
        }
        bool dot=false;std::size_t digits=0;
        for(const auto c:raw){
            if(!cursor.spend(4u))return false;
            if(c>='0'&&c<='9'){++digits;continue;}
            if(c=='.'&&!dot&&digits){dot=true;digits=0;continue;}
            return false;
        }
        if(!digits)return false;
    }
    for(const auto word:{"erDiagram","style","classDef","class","subgraph","end","one","many","to"}) {
        if(!cursor.spend(2u*sizeof(std::string_view)+sizeof(std::size_t)+16u))return false;
        const auto size=std::string_view(word).size();
        if(!cursor.spend(size+8u))return false;
        if(raw.size()<size||!er_ascii_equal(raw.substr(0,size),word))continue;
        if(raw.size()==size)return false;
        const auto c=static_cast<unsigned char>(raw[size]);
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;
    }
    if(!cursor.spend(16u))return false;
    if(raw.starts_with("--")||raw.starts_with("..")||raw.starts_with(".-")||raw.starts_with("-."))return false;
    const char next=raw.size()>1u?raw[1]:tail.empty()?'\0':tail.front();
    if((raw.front()=='u'||raw.front()=='U')&&std::string_view(".-|").find(next)!=std::string_view::npos)return false;
    return true;
}
// INITIAL direction rules precede quoted ENTITY_NAME and have no leading or
// trailing word boundary. Only this actual statement/header row and directly
// adjacent whitespace are inspected; metadata/comments/block fields stay out.
bool er_group_initial_row(ErCursor& cursor) {
    if(!cursor.spend(8u*sizeof(std::size_t)+8u*sizeof(bool)+64u))return false;
    auto end=cursor.at;
    while(end<cursor.source.size()){
        if(!cursor.spend())return false;
        if(cursor.source[end]=='\r'||cursor.source[end]=='\n')break;
        ++end;
    }
    for(auto at=cursor.at;end-at>=9u;++at){
        if(!cursor.spend(16u))return false;
        if(!er_ascii_equal(cursor.source.substr(at,9u),"direction"))continue;
        auto value=at+9u;bool separated=false;
        while(value<cursor.source.size()){
            if(!cursor.spend(32u))return false;
            const auto space=er_metadata_space(cursor.source.substr(value));if(!space)break;
            value+=space;separated=true;
        }
        if(!separated)continue;
        if(!cursor.spend(16u))return false;
        for(const auto direction:{"TB","BT","RL","LR"})
            if(er_ascii_equal(cursor.source.substr(value,2u),direction))return false;
    }
    return true;
}
struct ErGroupHeader {std::string id,label;};
std::optional<ErGroupHeader> er_group_header(ErCursor& cursor) {
    if(!cursor.spend(8u*sizeof(std::size_t)+8u*sizeof(std::string)+8u*sizeof(bool)+128u))return {};
    auto end=cursor.at;
    while(end<cursor.source.size()) {
        if(!cursor.spend())return {};
        if(cursor.source[end]=='\r'||cursor.source[end]=='\n')break;
        ++end;
    }
    // The finite profile requires a physical separator and never lets skip()
    // adopt the first business statement on the following physical line.
    if(end==cursor.source.size()||end==cursor.at||
       (cursor.source[cursor.at]!=' '&&cursor.source[cursor.at]!='\t'))return {};
    const auto raw=cursor.source.substr(cursor.at,end-cursor.at);
    if(!cursor.spend(3u*sizeof(ErCursor)+raw.size()*(24u*sizeof(std::string)+16u*sizeof(std::string_view)+96u)+256u))return {};
    ErCursor header{raw,cursor.work};
    if(!er_group_initial_row(cursor))return {};
    if(!header.skip())return {};const auto id_start=header.at;
    const auto id=header.name();if(!id||!er_group_token(raw.substr(id_start,header.at-id_start),cursor.source.substr(cursor.at+header.at),header)||!header.skip())return {};
    std::string title=*id;
    if(header.at<raw.size()) {
        if(raw[header.at]!='['||!header.spend())return {};
        ++header.at;title.clear();bool first=true;
        while(header.skip()&&header.at<raw.size()&&raw[header.at]!=']') {
            if(!er_group_operand(header))return {};
            const auto token_start=header.at;
            const auto token=header.name();if(!token||!er_group_token(raw.substr(token_start,header.at-token_start),cursor.source.substr(cursor.at+header.at),header))return {};
            if(!header.spend((title.size()+token->size()+1u)*4u+4u*sizeof(std::string)+32u))return {};
            if(!first)title+=' ';title+=*token;first=false;
        }
        if(first||!header.take("]")||!header.skip()||header.at!=raw.size())return {};
    }
    if(!header.healthy||!header.spend(title.size()*(24u*sizeof(std::string)+16u*sizeof(std::string_view)+96u)+128u))return {};
    const auto label=er_literal_label(title,header.work);if(!label||label->find('\n')!=std::string::npos)return {};
    if(cursor.source[end]=='\r') {
        if(end+1u==cursor.source.size()||cursor.source[end+1u]!='\n'||!cursor.spend(2u))return {};
        ++end;
    }
    if(!cursor.spend(id->size()*2u+label->size()*2u+4u*sizeof(ErGroupHeader)+32u))return {};
    cursor.at=end+1u;return ErGroupHeader{*id,*label};
}

std::optional<RelationDiagram> parse_plain_er(std::string_view source,Work& work) {
    ErCursor cursor{source,work};if(!cursor.take("erDiagram",true))return {};
    RelationDiagram diagram;diagram.family=RelationFamily::Er;
    // A complete header-only ER document is a valid empty artifact. Keep this
    // admission before body parsing: unknown or metadata-only bodies cannot be
    // discarded as empty, and malformed directives still fail cursor.skip().
    if(cursor.skip()&&cursor.at==source.size())return diagram;
    // Fixed new parse-local state/closures are paid even for nongroup ER.
    if(!cursor.spend(4u*sizeof(std::unique_ptr<ErGroupParse>)+8u*sizeof(bool)+
        40u*sizeof(void*)+64u))return {};
    std::unique_ptr<ErGroupParse> groups;bool presentation_seen=false,group_tokens_supported=true;
    auto name=[&]()->std::optional<std::string>{
        if(!cursor.spend(8u*sizeof(std::optional<std::string>)+8u*sizeof(void*)+4u*sizeof(std::size_t)+64u)||!cursor.skip())return {};
        const auto start=cursor.at;
        if(groups&&!er_group_operand(cursor))return {};
        auto token=cursor.name();if(!token)return {};
        if(!er_group_token(source.substr(start,cursor.at-start),source.substr(cursor.at),cursor)){
            group_tokens_supported=false;if(!cursor.healthy||groups)return {};
        }
        return token;
    };
    auto remember=[&](int index)->bool {
        if(!cursor.spend(8u))return false;
        if(!groups||groups->depth==0)return true;
        if(index<0||static_cast<std::size_t>(index)>=diagram.entities.size())return false;
        auto& frame=groups->frames[groups->depth-1u];
        for(std::size_t n=0;n<frame.count;++n){if(!cursor.spend(2u*sizeof(int)+4u))return false;if(frame.members[n]==index)return true;}
        if(frame.count==frame.members.size()||!cursor.spend(2u*sizeof(int)+2u*sizeof(std::size_t)+8u))return false;
        frame.members[frame.count++]=index;return true;
    };
    auto entity=[&](std::string id){
        if(!cursor.spend(8u))return -1;
        if(!groups)return diagram.entity(std::move(id),-1,&work);
        if(!cursor.spend(4u*sizeof(int)+32u))return -1;
        for(std::size_t n=0;n<diagram.entities.size();++n){
            const auto& existing=diagram.entities[n];
            if(!cursor.spend(id.size()+existing.id.size()+2u*sizeof(int)+8u))return -1;
            if(existing.id==id)return existing.shape==EntityShape::Group?-1:static_cast<int>(n);
        }
        if(diagram.entities.size()>=64u||!cursor.spend(id.size()*(24u*sizeof(std::string)+16u*sizeof(std::string_view)+96u)+
            4u*sizeof(RelationEntity)+alignof(RelationEntity)+128u))return -1;
        return diagram.entity(std::move(id),-1,&work);
    };
    auto inline_classes=[&](std::size_t entity_end)->bool {
        if(!cursor.spend(4u*sizeof(std::size_t)+4u*sizeof(bool)+16u))return false;
        auto at=cursor.at;
        while(at<source.size()&&(source[at]==' '||source[at]=='\t'||source[at]=='\r')){if(!cursor.spend())return false;++at;}
        if(!cursor.spend(3u))return false;
        const bool present=source.substr(at).starts_with(":::");
        if(present&&groups&&!cursor.spend((source.size()-cursor.at)*(24u*sizeof(std::string)+16u*sizeof(std::string_view)+96u)+128u))return false;
        if(!er_inline_classes(cursor,entity_end))return false;
        if(present){if(groups)return false;presentation_seen=true;}
        return true;
    };
    while(cursor.skip()&&cursor.at<source.size()) {
        if(!cursor.spend(4u*sizeof(bool)+8u))return {};
        const auto accessibility=er_accessibility(cursor);
        if(accessibility<0)return {};
        if(accessibility>0){if(groups&&groups->depth)return {};continue;}
        if(er_presentation_keyword(cursor,"subgraph")) {
            if(presentation_seen||!group_tokens_supported)return {};
            const auto header=er_group_header(cursor);if(!header)return {};
            if(!groups){
                if(!cursor.spend(2u*sizeof(ErGroupParse)+alignof(ErGroupParse)+128u+
                    64u*sizeof(RelationEntity)+2u*diagram.entities.size()*sizeof(RelationEntity)+
                    4u*sizeof(std::vector<RelationEntity>)))return {};
                groups=std::make_unique<ErGroupParse>();diagram.entities.reserve(64u);
            }
            if(groups->depth>=kGroupDepthLimit||groups->count>=kGroupLimit||diagram.entities.size()>=64u)return {};
            for(const auto& existing:diagram.entities){
                if(!cursor.spend(existing.id.size()+header->id.size()+8u))return {};
                if(existing.id==header->id)return {};
            }
            if(!cursor.spend(4u*sizeof(RelationEntity)+2u*(header->id.size()+header->label.size())+
                2u*sizeof(ErGroupFrame)+64u))return {};
            const auto index=static_cast<int>(diagram.entities.size());
            diagram.entities.push_back({header->id,header->label,{},EntityShape::Group,-1,{},true});
            auto& frame=groups->frames[groups->depth++];frame.group=index;frame.count=0;++groups->count;
            continue;
        }
        if(er_presentation_keyword(cursor,"end")) {
            if(!groups||groups->depth==0||!cursor.spend(sizeof(ErGroupFrame)+8u*sizeof(int)+64u))return {};
            auto& frame=groups->frames[groups->depth-1u];
            for(std::size_t n=0;n<frame.count;++n){
                if(!cursor.spend(4u*sizeof(int)+2u*sizeof(bool)+16u))return {};
                const auto index=static_cast<std::size_t>(frame.members[n]);
                if(index>=diagram.entities.size()||index==static_cast<std::size_t>(frame.group))return {};
                if(!groups->claimed[index]){diagram.entities[index].owner=frame.group;groups->claimed[index]=true;}
            }
            const int closed=frame.group;--groups->depth;
            if(!remember(closed))return {};
            continue;
        }
        if(er_presentation_keyword(cursor,"classDef")||er_presentation_keyword(cursor,"style")) {
            if(groups&&!cursor.spend((source.size()-cursor.at)*(24u*sizeof(std::string)+16u*sizeof(std::string_view)+96u)+128u))return {};
            if(!er_presentation_line(cursor,false)||groups)return {};
            presentation_seen=true;continue;
        }
        if(er_presentation_keyword(cursor,"class")) {
            if(groups&&!cursor.spend((source.size()-cursor.at)*(24u*sizeof(std::string)+16u*sizeof(std::string_view)+96u)+128u))return {};
            if(!er_presentation_line(cursor,true)||groups)return {};
            presentation_seen=true;continue;
        }
        if(cursor.take("direction",true)) {
            if(groups&&groups->depth)return {};
            const auto direction=cursor.name();if(!direction)return {};
            bool matched=false;for(const auto value:{"TB","BT","LR","RL"})if(er_ascii_equal(*direction,value)){diagram.direction=value;matched=true;break;}
            if(!matched)return {};
            continue;
        }
        if(!er_group_initial_row(cursor)){group_tokens_supported=false;if(!cursor.healthy||groups)return {};}
        const auto id=name();if(!id)return {};const int from=entity(*id);if(from<0||!remember(from))return {};
        auto& node=diagram.entities[static_cast<std::size_t>(from)];
        auto inline_entity_end=cursor.at;
        if(cursor.take("[")) {
            const auto alias=name();if(!alias||!cursor.take("]"))return {};
            if(!node.er_alias){const auto label=er_literal_label(*alias,work);if(!label)return {};node.label=*label;node.er_alias=true;}
            inline_entity_end=cursor.at;
        }
        if(!inline_classes(inline_entity_end))return {};
        if(cursor.take("{")) {
            cursor.attribute_block=true;
            while(cursor.attribute_skip()) {
                if(cursor.peek()=='}') {
                    const auto generic=cursor.generic_attribute_word();if(!cursor.healthy)return {};
                    if(!generic)break;
                }
                if(groups&&(!cursor.spend(4u*sizeof(ErAttribute)+128u)||!er_group_operand(cursor,true)))return {};
                ErAttribute attribute;const auto type=cursor.attribute_word(true);
                if(groups&&!er_group_operand(cursor,true))return {};
                const auto field=cursor.attribute_word(false);if(!type||!field)return {};
                attribute.type=*type;attribute.name=*field;
                auto key=[&](){
                    if(groups){
                        const auto size=attribute.keys.size(),capacity=attribute.keys.capacity();
                        const auto next=size==capacity?std::max(std::size_t{1},2u*size):std::size_t{0};
                        if(!cursor.spend((next+2u*size+1u)*sizeof(std::string)+
                            4u*sizeof(std::string)+32u))return std::optional<std::string>{};
                    }
                    return cursor.attribute_key();
                };
                if(const auto first=key()) {
                    attribute.keys.push_back(*first);
                    while(cursor.peek()==',') {
                        const auto generic=cursor.generic_attribute_word();if(!cursor.healthy)return {};
                        if(generic)break;
                        if(!cursor.take(","))return {};
                        const auto next=key();if(!next||attribute.keys.size()>=85)return {};attribute.keys.push_back(*next);
                    }
                }
                if(cursor.peek()=='"') {
                    const auto generic=cursor.generic_attribute_word();if(!cursor.healthy)return {};
                    // A quote owned by the earlier generic rule is the next
                    // ATTRIBUTE_WORD, not this attribute's COMMENT delimiter.
                    if(!generic) {
                        if(groups&&!er_group_operand(cursor,true))return {};
                        const auto text=cursor.quoted('"');if(!text)return {};
                        if(!text->empty()){const auto label=er_literal_label(*text,work,true);if(!label)return {};attribute.comment=*label;}
                    }
                }
                if(groups){
                    const auto bytes=attribute.type.size()+attribute.name.size()+attribute.comment.size()+attribute.keys.size()*3u;
                    const auto size=node.er_attributes.size(),capacity=node.er_attributes.capacity();
                    const auto next=size==capacity?std::max(std::size_t{1},2u*size):std::size_t{0};
                    if(!cursor.spend(bytes*(24u*sizeof(std::string)+16u*sizeof(std::string_view)+96u)+
                        (next+2u*size+1u)*sizeof(ErAttribute)+128u))return {};
                }
                if(!er_attribute_budget(attribute,work)||node.er_attributes.size()>=128)return {};
                node.er_attributes.push_back(std::move(attribute));
            }
            if(!cursor.take("}"))return {};
            cursor.attribute_block=false;
            node.declared=true;continue;
        }
        const auto start=er_cardinality(cursor);if(!start)continue;
        if(!work.spend(4u*sizeof(std::unique_ptr<ClassEndpointTexts>)))return {};
        if(!work.spend(4u*kRelationshipKindStorage+16u))return {};
        Relationship edge;edge.from=from;edge.start=*start;
        if(cursor.take("--")||cursor.take("to",true))edge.dotted=false;
        else if(cursor.take("..")||cursor.take(".-")||cursor.take("-.")||cursor.take("optionally to",true))edge.dotted=true;
        else return {};
        const auto end=er_cardinality(cursor);if(!end)return {};edge.end=*end;
        const auto target=name();if(!target)return {};edge.to=entity(*target);
        if(edge.to<0||!remember(edge.to)||!inline_classes(cursor.at)||!cursor.take(":"))return {};
        if(groups&&!er_group_operand(cursor))return {};
        if(cursor.peek()=='"') {
            const auto text=cursor.quoted('"');if(!text)return {};
            // The plain edge display profile trims outer ASCII whitespace;
            // whitespace-only Markdown role text has no visible label.
            if(!trim_relation(*text).empty()){const auto label=er_literal_label(trim_relation(*text),work);if(!label)return {};edge.label=*label;}
        }
        else {const auto label=name();if(!label)return {};const auto text=er_literal_label(*label,work);if(!text)return {};edge.label=*text;}
        if(diagram.edges.size()>=128)return {};
        if(groups){
            const auto size=diagram.edges.size(),capacity=diagram.edges.capacity();
            const auto next=size==capacity?std::min(std::size_t{128},std::max(std::size_t{1},size*2u)):std::size_t{0};
            if(!cursor.spend((next+2u*size+1u)*sizeof(Relationship)+edge.label.size()*2u+128u))return {};
        }
        if(!append_relationship(diagram.edges,std::move(edge),work))return {};
    }
    if(!cursor.healthy||(groups&&groups->depth)||diagram.entities.empty()||diagram.entities.size()>64)return {};
    return diagram;
}

// ER owns this closed source-envelope profile. It validates every field before
// handing the unchanged complete body to the existing graph parser.
struct ErEnvelope {std::string_view body;std::optional<std::string> title;};
struct ErMetaValue {
    enum class Kind { Text,Number,Boolean,Other } kind=Kind::Other;
    std::string_view text;double number=0;bool boolean=false;
};
// String payloads are omitted or substituted by the native projection, never
// evaluated as CSS. Whitespace separators are allowed only in stylesheet data.
bool er_config_text(std::string_view text,bool stylesheet,Work& work) {
    if(text.size()>kSourceLimit||!work.spend(text.size()*2+1))return false;
    if(!stylesheet)return !text.empty()&&unicode_text(text);
    std::size_t start=0;
    for(std::size_t n=0;n<text.size();++n)if(text[n]=='\t'||text[n]=='\r'||text[n]=='\n') {
        if(!unicode_text(text.substr(start,n-start)))return false;
        start=n+1;
    }
    return unicode_text(text.substr(start));
}
ErMetaValue er_yaml_value(std::string_view value,Work& work) {
    if(!work.spend(value.size()*6+1)||value.empty())return {};
    if(value.front()=='\''||value.front()=='"') {
        const auto quote=value.front();
        if(value.size()<2||value.back()!=quote)return {};
        value=value.substr(1,value.size()-2);
        // The reviewed scalar profile needs no YAML escape processing.
        if(value.find(quote)!=std::string_view::npos||value.find('\\')!=std::string_view::npos)return {};
        return {ErMetaValue::Kind::Text,value};
    }
    if(value=="false"||value=="False"||value=="FALSE"||value=="true"||value=="True"||value=="TRUE")
        return {ErMetaValue::Kind::Boolean,{},0,value=="true"||value=="True"||value=="TRUE"};
    if(value=="null"||value=="Null"||value=="NULL"||value=="~"||value.find_first_of("{}[]&*!|>\\\"'")!=std::string_view::npos)return {};
    if(value.front()=='@'||value.front()=='`'||value.front()=='%'||
       ((value.front()=='-'||value.front()=='?'||value.front()==':')&&
        (value.size()==1||value[1]==' '||value[1]=='\t')))return {};
    for(std::size_t n=0;n<value.size();++n)
        if(value[n]==':'&&(n+1==value.size()||value[n+1]==' '||value[n+1]=='\t'))return {};
    auto numeric=value;
    if(numeric.front()=='+'||numeric.front()=='-')numeric.remove_prefix(1);
    if(numeric.empty())return {ErMetaValue::Kind::Text,value};
    // These JSON_SCHEMA numeric forms are outside the finite decimal profile.
    if(numeric==".inf"||numeric==".Inf"||numeric==".INF"||value==".nan"||value==".NaN"||value==".NAN")return {};
    if(numeric.size()>2&&numeric[0]=='0'&&(numeric[1]=='b'||numeric[1]=='o'||numeric[1]=='x')) {
        const char radix=numeric[1];bool digits=true;
        for(const unsigned char c:numeric.substr(2))digits&=radix=='b'?(c=='0'||c=='1'):
            radix=='o'?(c>='0'&&c<='7'):((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'));
        if(digits)return {};
    }
    // Match the pinned decimal syntax before using the local finite reader.
    // In particular 4.3.0 does not classify 1_0 as numeric.
    std::size_t n=0;
    if(value[n]=='+'||value[n]=='-')++n;
    const auto first=n;
    while(n<value.size()&&value[n]>='0'&&value[n]<='9')++n;
    bool decimal=n>first;
    if(n<value.size()&&value[n]=='.') {
        ++n;const auto fraction=n;
        while(n<value.size()&&value[n]>='0'&&value[n]<='9')++n;
        decimal=decimal||(first==0&&n>fraction);
    }
    if(n<value.size()&&(value[n]=='e'||value[n]=='E')) {
        ++n;if(n<value.size()&&(value[n]=='+'||value[n]=='-'))++n;
        const auto exponent=n;while(n<value.size()&&value[n]>='0'&&value[n]<='9')++n;
        decimal&=n>exponent;
    }
    if(decimal&&n==value.size()) {
        auto finite=value;if(finite.front()=='+')finite.remove_prefix(1);
        double number=0;const auto parsed=std::from_chars(finite.data(),finite.data()+finite.size(),number);
        if(parsed.ec==std::errc{}&&parsed.ptr==finite.data()+finite.size()&&std::isfinite(number))
            return {ErMetaValue::Kind::Number,{},number};
        return {}; // Out-of-range forms remain whole source, not display text.
    }
    return {ErMetaValue::Kind::Text,value};
}
bool er_config_leaf(std::string_view owner,std::string_view key,const ErMetaValue& value,unsigned& consumed,Work& work) {
    if(!work.spend(owner.size()+key.size()+1))return false;
    unsigned bit=0;bool valid=false;
    if(owner.empty()) {
        if(key=="logLevel") {
            bit=1u;
            constexpr std::array<std::string_view,6> names{{"trace","debug","info","warn","error","fatal"}};
            valid=value.kind==ErMetaValue::Kind::Text?std::ranges::find(names,value.text)!=names.end():
                value.kind==ErMetaValue::Kind::Number&&value.number>=0&&value.number<=5&&std::trunc(value.number)==value.number;
        }else if(key=="loglevel") {
            // This legacy spelling is not an alias of the canonical option.
            bit=2u;valid=value.kind==ErMetaValue::Kind::Number&&std::isfinite(value.number);
        }else if(key=="layout") {bit=4u;valid=value.kind==ErMetaValue::Kind::Text&&value.text=="elk";}
        else if(key=="theme") {bit=8u;valid=value.kind==ErMetaValue::Kind::Text&&value.text=="forest";}
        else if(key=="fontSize") {bit=16u;valid=value.kind==ErMetaValue::Kind::Number&&std::isfinite(value.number)&&value.number>0;}
        else if(key=="themeCSS") {bit=32u;valid=value.kind==ErMetaValue::Kind::Text&&er_config_text(value.text,true,work);}
    }else if(owner=="flowchart") {
        if(key=="htmlLabels") {bit=64u;valid=value.kind==ErMetaValue::Kind::Boolean&&!value.boolean;}
        else if(key=="defaultRenderer") {bit=128u;valid=value.kind==ErMetaValue::Kind::Text&&value.text=="elk";}
    }else if(owner=="themeVariables"&&key=="fontSize") {bit=256u;valid=value.kind==ErMetaValue::Kind::Text?er_config_text(value.text,false,work):
            value.kind==ErMetaValue::Kind::Number&&std::isfinite(value.number)&&value.number>0;}
    if(!bit||!valid||(consumed&bit))return false;
    consumed|=bit;return true;
}
bool er_json_config(loom::utils::json::JsonVal object,unsigned& consumed,Work& work,std::string_view owner={}) {
    if(!object.is_obj()||object.size()==0||object.size()>9)return false;
    bool valid=true;unsigned namespaces=0;
    object.iter_obj([&](loom::utils::json::JsonVal name,loom::utils::json::JsonVal value){
        if(!valid)return;
        if(!work.spend(name.as_str().size()+1)){valid=false;return;}
        const auto key=name.as_str();
        if(owner.empty()&&(key=="flowchart"||key=="themeVariables")) {
            const unsigned bit=key=="flowchart"?1u:2u;
            if(namespaces&bit){valid=false;return;}namespaces|=bit;
            valid=er_json_config(value,consumed,work,key);return;
        }
        ErMetaValue decoded;
        if(value.is_str())decoded={ErMetaValue::Kind::Text,value.as_str()};
        else if(value.is_num())decoded={ErMetaValue::Kind::Number,{},value.as_double()};
        else if(value.is_bool())decoded={ErMetaValue::Kind::Boolean,{},0,value.as_bool()};
        valid=er_config_leaf(owner,key,decoded,consumed,work);
    });return valid;
}
// ER documentation is source-owned opaque text, never graph/title/config data.
// Keep its finite safety boundary local: unicode_text permits BOM and U+200B.
bool er_description_text(std::string_view text,Work& work) {
    if(text.size()>kSourceLimit||!work.spend(text.size()*6u+4u*sizeof(std::string_view)+4u*sizeof(std::size_t)+32u))return false;
    return text.find("\xef\xbb\xbf")==std::string_view::npos&&
        text.find("\xe2\x80\x8b")==std::string_view::npos&&unicode_text(text);
}
bool er_description_value(std::string_view value,std::string_view& source,Work& work) {
    if(value.size()>kSourceLimit||!work.spend(value.size()*4u+8u*sizeof(std::string_view)+8u*sizeof(std::size_t)+8u*sizeof(bool)+64u))return false;
    if(value!=">-") {
        if(value.empty()||!er_description_text(value,work)||
           std::string_view(",[]{}#&*!|>'\"%@`").find(value.front())!=std::string_view::npos)return false;
        if((value.front()=='-'||value.front()=='?')&&(value.size()==1u||value[1]==' '))return false;
        for(std::size_t at=0;at<value.size();++at) {
            if(value[at]=='#'&&at>0u&&value[at-1u]==' ')return true;
            if(value[at]==':'&&(at+1u==value.size()||value[at+1u]==' '))return false;
        }
        return true;
    }
    // LF searches across consumed lines plus the single unconsumed root peek
    // are prepaid once; every later line visit and payload scan is also owned.
    if(source.size()>kSourceLimit||!work.spend(source.size()+8u*sizeof(bool)+32u))return false;
    bool content_seen=false;
    while(!source.empty()) {
        const auto newline=source.find('\n');auto line=source.substr(0,newline);
        if(!work.spend(line.size()*2u+4u*sizeof(std::string_view)+4u*sizeof(std::size_t)+4u*sizeof(bool)+16u))return false;
        if(line.ends_with('\r')){if(newline==std::string_view::npos)return false;line.remove_suffix(1u);}
        if(line.find_first_of("\t\r")!=std::string_view::npos)return false;
        const auto indent=line.find_first_not_of(' ');
        if(indent==std::string_view::npos){if(!line.empty()&&line.size()!=2u)return false;}
        else {
            if(indent==0u)return content_seen;
            if(indent!=2u||!er_description_text(line.substr(2u),work))return false;
            content_seen=true;
        }
        source=newline==std::string_view::npos?std::string_view{}:source.substr(newline+1u);
    }
    return content_seen; // The outer owner still requires its closing fence.
}
std::string_view er_strip_comment(std::string_view value) {
    char quote=0;
    for(std::size_t n=0;n<value.size();++n) {
        const char c=value[n];
        if(quote){if(c==quote)quote=0;continue;}
        if(c=='\''||c=='"'){quote=c;continue;}
        if(c=='#'&&(n==0||value[n-1]==' '||value[n-1]=='\t'))return trim_relation(value.substr(0,n));
    }
    return value;
}
std::optional<ErEnvelope> er_source_envelope(std::string_view source,Work& work) {
    if(source.starts_with("%%{")) {
        const auto newline=source.find('\n');auto line=trim_relation(source.substr(0,newline));
        if(!work.spend(line.size()*8+64)||newline==std::string_view::npos||!line.starts_with("%%{init:")||!line.ends_with("}%%"))return {};
        const auto payload=trim_relation(line.substr(8,line.size()-11));
        const auto document=loom::utils::json::parse(payload);unsigned consumed=0;
        if(!document||!er_json_config(document->root(),consumed,work)||!consumed)return {};
        return ErEnvelope{source.substr(newline+1),{}};
    }
    if(!work.spend(4u*sizeof(bool)+16u))return {};
    const auto parsed=parse_envelope(source,work);
    if(!parsed||!parsed->has_frontmatter)return {};
    bool description_seen=false,config_seen=false,title_seen=false;
    unsigned consumed=0;
    std::vector<std::string> namespaces;
    std::map<std::string,int> config_indent;
    ErEnvelope envelope;
    for(const auto& event:parsed->events) {
        if(event.blank||event.comment)continue;
        if(event.top_level) {
            if(event.path=="description") {if(description_seen)return {};description_seen=true;continue;}
            if(event.path=="config") {if(config_seen)return {};config_seen=true;continue;}
            if(event.path=="title") {
                if(title_seen)return {};
                title_seen=true;
                const auto decoded=er_yaml_value(er_strip_comment(event.value),work);
                if(decoded.kind!=ErMetaValue::Kind::Text||decoded.text.empty()||decoded.text.size()>kLabelLimit||
                   !work.spend(decoded.text.size()*4+1))return {};
                bool ink=false;for(const unsigned char c:decoded.text){if(c<0x20||c>0x7e)return {};ink|=c!=' ';}
                if(!ink)return {};
                envelope.title=std::string(decoded.text);
                continue;
            }
            return {};
        }
        const auto dot=event.path.rfind('.');
        const std::string_view parent=dot==std::string_view::npos?std::string_view{}:
            std::string_view(event.path).substr(0,dot);
        const auto [position,inserted]=config_indent.emplace(std::string(parent),event.indent);
        if(!inserted&&position->second!=event.indent)return {};
        if(event.namespace_entry) {
            if(!parent.empty()||(event.path!="flowchart"&&event.path!="themeVariables"))return {};
            if(std::ranges::find(namespaces,event.path)!=namespaces.end())return {};
            namespaces.push_back(event.path);
            continue;
        }
        const std::string_view owner=parent;
        const std::string_view key=dot==std::string_view::npos?std::string_view(event.path):
            std::string_view(event.path).substr(dot+1u);
        if(!owner.empty()&&owner!="flowchart"&&owner!="themeVariables")return {};
        ErMetaValue decoded;
        if(owner.empty()&&key=="themeCSS"&&event.form==EnvelopeScalarForm::Literal) {
            decoded={ErMetaValue::Kind::Text,event.value};
        } else {
            decoded=er_yaml_value(er_strip_comment(event.value),work);
        }
        if(!er_config_leaf(owner,key,decoded,consumed,work))return {};
    }
    if(description_seen) {
        if(!parsed->description)return {};
        const auto& description=*parsed->description;
        if(er_strip_comment(description.header)==">-") {
            bool content=false;
            std::size_t at=0;
            while(at<description.text.size()) {
                const auto newline=description.text.find('\n',at);
                const auto line=description.text.substr(at,newline==std::string_view::npos?
                    std::string_view::npos:newline-at);
                at=newline==std::string_view::npos?description.text.size():newline+1u;
                if(line.find_first_of("\t\r")!=std::string_view::npos)return {};
                const auto indent=line.find_first_not_of(' ');
                if(indent==std::string_view::npos) {
                    if(!line.empty()&&line.size()!=2u)return {};
                    continue;
                }
                if(indent!=2u||!er_description_text(line.substr(2u),work))return {};
                content=true;
            }
            if(!content)return {};
        }else{
            auto value=er_strip_comment(description.header);
            auto empty=std::string_view{};
            if(!er_description_value(value,empty,work))return {};
        }
    }
    if(config_seen&&!consumed)return {};
    for(const auto& name:namespaces) {
        bool child=false;
        for(const auto& entry:parsed->config) {
            if(entry.path.size()>name.size()&&entry.path.starts_with(name)&&entry.path[name.size()]=='.') {
                child=true;break;
            }
        }
        if(!child)return {};
    }
    if(!title_seen&&!description_seen&&!consumed)return {};
    envelope.body=parsed->raw_body;
    return envelope;
}

// RFC 0031 E2a: stateDiagram(-v2) admits a description-only frontmatter
// envelope, optionally with a title the relations layout already projects.
// config and init stay whole-source (E2b); the description is never rendered.
struct StateEnvelope {
    std::string_view body;
    std::optional<std::string> title;
};
std::optional<StateEnvelope> state_source_envelope(std::string_view source,Work& work) {
    if(!source.starts_with("---")||!work.spend(4u*sizeof(bool)+16u))return {};
    const auto parsed=parse_envelope(source,work);
    if(!parsed||!parsed->has_frontmatter||!parsed->init.empty())return {};
    bool description_seen=false,title_seen=false;
    StateEnvelope result;
    for(const auto& event:parsed->events) {
        if(event.blank||event.comment)continue;
        if(!event.top_level)return {};
        if(event.path=="description"){if(description_seen)return {};description_seen=true;continue;}
        if(event.path=="config")return {};
        if(event.path=="title") {
            if(title_seen)return {};
            title_seen=true;
            const auto decoded=er_yaml_value(er_strip_comment(event.value),work);
            if(decoded.kind!=ErMetaValue::Kind::Text||decoded.text.empty()||decoded.text.size()>kLabelLimit||
               !work.spend(decoded.text.size()*4+1))return {};
            bool ink=false;for(const unsigned char c:decoded.text){if(c<0x20||c>0x7e)return {};ink|=c!=' ';}
            if(!ink)return {};
            result.title=std::string(decoded.text);
            continue;
        }
        return {};
    }
    if(!description_seen||!parsed->description)return {};
    const auto& description=*parsed->description;
    if(description.form!=EnvelopeScalarForm::Folded||description.header!=">-")return {};
    bool content=false;
    for(std::size_t at=0;at<description.text.size();) {
        const auto newline=description.text.find('\n',at);
        const auto line=description.text.substr(at,newline==std::string_view::npos?
            std::string_view::npos:newline-at);
        at=newline==std::string_view::npos?description.text.size():newline+1u;
        if(line.find_first_of("\t\r")!=std::string_view::npos)return {};
        const auto indent=line.find_first_not_of(' ');
        if(indent==std::string_view::npos){if(!line.empty()&&line.size()!=2u)return {};continue;}
        if(indent!=2u||!er_description_text(line.substr(2u),work))return {};
        content=true;
    }
    if(!content)return {};
    if(!parsed->raw_body.starts_with("stateDiagram"))return {};
    result.body=parsed->raw_body;
    return result;
}

// Class owns its finite metadata schema. ER scalar/leaf/envelope grammar stays
// unchanged; this stack never retries an ER or Sequence parser.
struct ClassEnvelope {
    std::string_view body;
    std::optional<std::string> title;
    std::optional<bool> hide_empty_members_box;
    std::optional<bool> hierarchical_namespaces;
};
struct ClassMeta {
    std::optional<bool> hide;
    std::optional<bool> root_html,class_html;
    std::optional<bool> hierarchical_namespaces;
};
bool class_meta_leaf(std::string_view owner,std::string_view key,const ErMetaValue& value,
                     unsigned& consumed,ClassMeta& meta,Work& work) {
    if(!work.spend(owner.size()+key.size()+sizeof(ClassMeta)*4u+32u))return false;
    if(value.kind==ErMetaValue::Kind::Text&&!er_config_text(value.text,false,work))return false;
    unsigned bit=0;bool valid=false;
    const bool text=value.kind==ErMetaValue::Kind::Text,number=value.kind==ErMetaValue::Kind::Number;
    const bool boolean=value.kind==ErMetaValue::Kind::Boolean;
    const bool positive=number&&std::isfinite(value.number)&&value.number>0;
    if(owner.empty()) {
        if(key=="logLevel") {
            bit=1u;constexpr std::array<std::string_view,6> names{{"trace","debug","info","warn","error","fatal"}};
            valid=text?std::ranges::find(names,value.text)!=names.end():number&&value.number>=0&&value.number<=5&&std::trunc(value.number)==value.number;
        }else if(key=="htmlLabels"){bit=2u;valid=boolean;}
        else if(key=="fontSize"){bit=4u;valid=positive;}
        else if(key=="theme") {
            bit=8u;constexpr std::array<std::string_view,12> themes{{"default","base","dark","forest","neutral","neo","neo-dark","redux","redux-dark","redux-color","redux-dark-color","null"}};
            valid=text&&std::ranges::find(themes,value.text)!=themes.end();
        }
        else if(key=="look"){bit=16u;valid=text&&(value.text=="classic"||value.text=="handDrawn"||value.text=="neo");}
        else if(key=="layout"){bit=32u;valid=text&&(value.text=="elk"||value.text=="dagre");}
    }else if(owner=="class") {
        if(key=="hideEmptyMembersBox"){bit=64u;valid=boolean;}
        else if(key=="htmlLabels"){bit=128u;valid=boolean;}
        else if(key=="hierarchicalNamespaces"){bit=65536u;valid=boolean;}
    }else if(owner=="flowchart"&&key=="htmlLabels"){bit=256u;valid=boolean;}
    else if(owner=="themeVariables") {
        if(key=="fontSize") {
            bit=512u;valid=positive;
            if(text&&value.text.ends_with("px")) {
                const auto digits=value.text.substr(0,value.text.size()-2);
                // Finite decimal pixels, not an arbitrary CSS expression.
                bool decimal=!digits.empty();bool dot=false;
                for(const unsigned char c:digits){if(c=='.'&&!dot){dot=true;continue;}if(c<'0'||c>'9')decimal=false;}
                double pixels=0;const auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),pixels);
                valid=decimal&&parsed.ec==std::errc{}&&parsed.ptr==digits.data()+digits.size()&&std::isfinite(pixels)&&pixels>0;
            }
        }else {
            constexpr std::array<std::string_view,6> colors{{"primaryColor","primaryTextColor","primaryBorderColor","lineColor","secondaryColor","tertiaryColor"}};
            for(std::size_t n=0;n<colors.size();++n)if(key==colors[n])bit=1024u<<n;
            if(bit&&text&&(value.text.size()==4||value.text.size()==7)&&value.text.front()=='#') {
                valid=true;for(const unsigned char c:value.text.substr(1))valid&=(c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');
            }
        }
    }
    if(!bit||!valid||(consumed&bit))return false;
    consumed|=bit;
    if(bit==64u)meta.hide=value.boolean;
    if(bit==65536u)meta.hierarchical_namespaces=value.boolean;
    if(owner.empty()&&key=="htmlLabels")meta.root_html=value.boolean;
    if(owner=="class"&&key=="htmlLabels")meta.class_html=value.boolean;
    return true;
}
bool class_meta_namespace(std::string_view name) {
    return name=="class"||name=="flowchart"||name=="themeVariables";
}
bool class_json_meta(loom::utils::json::JsonVal object,ClassMeta& meta,Work& work,std::string_view owner={}) {
    if(!work.spend(8u*sizeof(void*)+8u*sizeof(unsigned)+64u)||!object.is_obj()||object.size()==0||object.size()>16)return false;
    unsigned consumed=0,namespaces=0;bool valid=true;
    object.iter_obj([&](loom::utils::json::JsonVal name,loom::utils::json::JsonVal value){
        if(!valid)return;
        const auto key=name.as_str();if(!work.spend(key.size()*4u+16u)){valid=false;return;}
        if(owner.empty()&&class_meta_namespace(key)) {
            const unsigned bit=key=="class"?1u:key=="flowchart"?2u:4u;
            if(namespaces&bit){valid=false;return;}namespaces|=bit;
            valid=class_json_meta(value,meta,work,key);return;
        }
        ErMetaValue decoded;
        if(value.is_str())decoded={ErMetaValue::Kind::Text,value.as_str()};
        else if(value.is_num())decoded={ErMetaValue::Kind::Number,{},value.as_double()};
        else if(value.is_bool())decoded={ErMetaValue::Kind::Boolean,{},0,value.as_bool()};
        valid=class_meta_leaf(owner,key,decoded,consumed,meta,work);
    });
    return valid;
}
bool class_init_meta(std::string_view& source,ClassMeta& meta,Work& work) {
    if(!work.spend(source.size()*4u+128u)||!source.starts_with("%%{"))return false;
    std::size_t close=std::string_view::npos;char quote=0;bool escaped=false;
    for(std::size_t at=3;at+2<source.size();++at) {
        const char c=source[at];
        if(escaped){escaped=false;continue;}
        if(quote&&c=='\\'){escaped=true;continue;}
        if(quote){if(c==quote)quote=0;continue;}
        if(c=='\''||c=='"'){quote=c;continue;}
        if(source.substr(at,3)=="}%%"){close=at;break;}
    }
    if(close==std::string_view::npos)return false;
    auto argument=source.substr(3,close-3);
    const auto first=argument.find_first_not_of(" \t\r\n");if(first==std::string_view::npos)return false;
    argument.remove_prefix(first);
    const auto colon=argument.find(':');if(colon==std::string_view::npos||trim_relation(argument.substr(0,colon))!="init")return false;
    argument.remove_prefix(colon+1);
    // Wrapper/tail scanning, normalization/copy and JSON parsing have separate
    // prepaid bounds. No decoded JsonVal escapes this local JsonDoc.
    if(!work.spend(argument.size()*12u+64u))return false;
    std::string normalized;normalized.reserve(argument.size()*2u);
    for(std::size_t at=0;at<argument.size();) {
        const char q=argument[at];
        if(q!='\''&&q!='"'){normalized+=q;++at;continue;}
        const auto begin=at++;bool escape=false;
        for(;at<argument.size();++at) {
            const char c=argument[at];
            if(q=='\''&&c=='\\')return false;
            if(escape){escape=false;continue;}
            if(c=='\\'){escape=true;continue;}
            if(c==q)break;
            if(static_cast<unsigned char>(c)<0x20)return false;
        }
        if(at==argument.size())return false;
        if(q=='"')normalized.append(argument.substr(begin,at-begin+1));
        else {
            const auto text=argument.substr(begin+1,at-begin-1);
            if(!unicode_text(text))return false;
            normalized+='"';for(const char c:text){if(c=='"')normalized+='\\';normalized+=c;}normalized+='"';
        }
        ++at;
    }
    if(!work.spend(normalized.size()*8u+64u))return false;
    const auto document=loom::utils::json::parse(normalized);
    if(!document||!class_json_meta(document->root(),meta,work))return false;
    auto tail=source.substr(close+3);const auto newline=tail.find('\n');auto suffix=tail.substr(0,newline);
    if(suffix.ends_with('\r'))suffix.remove_suffix(1);
    if(!trim_relation(suffix).empty())return false;
    source=newline==std::string_view::npos?std::string_view{}:tail.substr(newline+1);
    return true;
}
// Description is opaque frontmatter documentation in the pinned Class owner.
// It never contributes a class/member/title or a visible diagram caption.
bool class_description_scalar(std::string_view value,Work& work) {
    if(!work.spend(value.size()*6u+64u))return false;
    if(value.find("\xef\xbb\xbf")!=std::string_view::npos||value.find("\xe2\x80\x8b")!=std::string_view::npos)return false;
    while(true) {
        const auto tab=value.find('\t');
        if(!unicode_text(value.substr(0,tab)))return false;
        if(tab==std::string_view::npos)return true;
        value.remove_prefix(tab+1);
    }
}
std::optional<ClassEnvelope> class_source_envelope(std::string_view source,Work& work) {
    if(source.size()>kSourceLimit||!work.spend(8u*sizeof(ClassEnvelope)+8u*sizeof(ClassMeta)+128u))return {};
    ClassEnvelope envelope;ClassMeta meta;
    std::string_view body=source;
    if(source.starts_with("---")) {
        const auto parsed=parse_envelope(source,work);
        if(!parsed||!parsed->has_frontmatter)return {};
        bool config_seen=false,title_seen=false,description_seen=false;
        std::map<std::string,unsigned> owner_seen;
        std::vector<std::string> namespaces;
        for(const auto& event:parsed->events) {
            if(event.blank||event.comment)continue;
            if(event.top_level) {
                if(event.path=="title") {if(title_seen)return {};title_seen=true;}
                else if(event.path=="description") {if(description_seen)return {};description_seen=true;}
                else if(event.path=="config") {if(config_seen)return {};config_seen=true;}
                else return {};
                continue;
            }
            if(event.namespace_entry) {
                if(event.path.find('.')!=std::string_view::npos||!class_meta_namespace(event.path))return {};
                if(std::ranges::find(namespaces,event.path)!=namespaces.end())return {};
                namespaces.push_back(event.path);
                continue;
            }
            const auto dot=event.path.rfind('.');
            const std::string_view owner=dot==std::string_view::npos?std::string_view{}:
                std::string_view(event.path).substr(0,dot);
            const std::string_view key=dot==std::string_view::npos?std::string_view(event.path):
                std::string_view(event.path).substr(dot+1u);
            if(!owner.empty()&&!class_meta_namespace(owner))return {};
            const auto decoded=er_yaml_value(er_strip_comment(event.value),work);
            auto& consumed=owner_seen[std::string(owner)];
            if(!class_meta_leaf(owner,key,decoded,consumed,meta,work))return {};
        }
        if(config_seen&&parsed->config.empty()&&namespaces.empty())return {};
        for(const auto& name:namespaces) {
            bool child=false;
            for(const auto& entry:parsed->config) {
                if(entry.path.size()>name.size()&&entry.path.starts_with(name)&&entry.path[name.size()]=='.') {
                    child=true;break;
                }
            }
            if(!child)return {};
        }
        if(parsed->title) {
            const auto decoded=er_yaml_value(er_strip_comment(parsed->title->header),work);
            if(decoded.kind!=ErMetaValue::Kind::Text||decoded.text.empty()||decoded.text.size()>kLabelLimit||
               !work.spend(decoded.text.size()*4u+1u))return {};
            bool ink=false;for(const unsigned char c:decoded.text){if(c<0x20||c>0x7e)return {};ink|=c!=' ';}
            if(!ink)return {};
            envelope.title=std::string(decoded.text);
        }
        if(parsed->description) {
            const auto& description=*parsed->description;
            const auto description_header=er_strip_comment(description.header);
            if(!(description.form==EnvelopeScalarForm::Folded&&
                 (description_header==">"||description_header==">-")))return {};
            std::optional<std::size_t> indentation;std::size_t leading_blank_indent=0;
            std::size_t at=0;
            while(at<description.text.size()) {
                const auto newline=description.text.find('\n',at);
                const auto line=description.text.substr(at,newline==std::string_view::npos?
                    std::string_view::npos:newline-at);
                at=newline==std::string_view::npos?description.text.size():newline+1u;
                const auto indent=line.find_first_not_of(' ');
                if(indent==std::string_view::npos) {
                    leading_blank_indent=std::max(leading_blank_indent,line.size());continue;
                }
                if(line[indent]=='\t')return {};
                if(!indentation) {if(indent<leading_blank_indent)return {};indentation=indent;}
                if(indent<*indentation||!class_description_scalar(line.substr(*indentation),work))return {};
            }
        }
        body=parsed->raw_body;
    } else if(!source.starts_with("%%{")) {
        return {};
    }
    auto skip=[&](){
        while(!body.empty()) {
            const auto end=body.find('\n');auto raw=body.substr(0,end);
            if(!work.spend(raw.size()+1u))return false;
            if(raw.ends_with('\r'))raw.remove_suffix(1);
            const auto line=trim_relation(raw);
            if(!line.empty()&&(!line.starts_with("%%")||line.starts_with("%%{")))return true;
            body=end==std::string_view::npos?std::string_view{}:body.substr(end+1);
        }
        return true;
    };
    if(!skip())return {};
    if(body.starts_with("%%{")) {if(!class_init_meta(body,meta,work)||!skip())return {};}
    const auto end=body.find('\n');const auto header=trim_relation(body.substr(0,end));
    if(!work.spend(header.size()+sizeof(ClassMeta)*4u+32u)||(header!="classDiagram"&&header!="classDiagram-v2"))return {};
    const bool effective_html=meta.root_html.value_or(meta.class_html.value_or(false));
    static_cast<void>(effective_html);
    envelope.body=body;envelope.hide_empty_members_box=meta.hide;envelope.hierarchical_namespaces=meta.hierarchical_namespaces;return envelope;
}

// Decode only the complete annotation delimiter owned by Class grammar.
// Other entities, HTML and double encoding keep their existing source boundary.
std::optional<std::string> class_encoded_annotation(std::string_view text,Work& work) {
    if(text.size()<16u||text.size()-16u+4u>kLabelLimit||
       !text.starts_with("&lt;&lt;")||!text.ends_with("&gt;&gt;"))return {};
    const auto token=text.substr(8u,text.size()-16u);
    if(!work.spend(text.size()*(8u+4u*sizeof(std::string))+token.size()*(token.size()+1u)+128u)||
       !unicode_text(token))return {};
    const auto label=relation_label(token);
    if(!label||label->find('\n')!=std::string::npos)return {};
    return "<<"+std::string(token)+">>";
}

// ClassTypes field whitespace is ECMAScript whitespace. The enclosing member
// operation prepays every byte pass before calling these private view helpers.
std::string_view class_member_trim(std::string_view value) {
    while(const auto size=er_metadata_space(value))value.remove_prefix(size);
    std::size_t last=0;
    for(std::size_t at=0;at<value.size();){
        if(const auto size=er_metadata_space(value.substr(at)))at+=size;
        else last=++at;
    }
    return value.substr(0,last);
}
std::size_t class_member_tildes(std::string_view value) {
    return static_cast<std::size_t>(std::ranges::count(value,'~'));
}
std::optional<std::string> class_member_generic_field(std::string_view value,bool& changed) {
    std::string out;out.reserve(value.size());
    auto append_set=[&](std::string_view set){
        std::size_t count=class_member_tildes(set);
        if(count<=1u){out.append(set);return;}
        if(count%2u&&set.starts_with('~')){out+='~';set.remove_prefix(1u);--count;}
        const auto half=count/2u;std::size_t ordinal=0;
        for(const char byte:set){
            if(byte!='~'){out+=byte;continue;}
            ++ordinal;
            if(ordinal<=half){out+='<';changed=true;}
            else if(ordinal>count-half){out+='>';changed=true;}
            else out+='~';
        }
    };
    for(std::size_t at=0;;){
        const auto comma=value.find(',',at);
        const auto end=comma==std::string_view::npos?value.size():comma;
        auto next_end=end;
        if(comma!=std::string_view::npos){
            const auto next=value.find(',',comma+1u);next_end=next==std::string_view::npos?value.size():next;
            if(class_member_tildes(value.substr(at,end-at))==1u&&
               class_member_tildes(value.substr(comma+1u,next_end-comma-1u))==1u){
                if(next!=std::string_view::npos){
                    const auto third=value.find(',',next+1u),third_end=third==std::string_view::npos?value.size():third;
                    // The fixed common output.pop would discard an already
                    // combined business prefix. Keep this finite profile closed.
                    if(class_member_tildes(value.substr(next+1u,third_end-next-1u))==1u)return {};
                }
                append_set(value.substr(at,next_end-at));
                if(next_end==value.size())break;
                out+=',';at=next_end+1u;continue;
            }
        }
        append_set(value.substr(at,end-at));
        if(comma==std::string_view::npos)break;
        out+=',';at=comma+1u;
    }
    return out;
}
std::optional<Member> class_member_display(std::string_view raw,Work& work) {
    if(raw.size()>kSourceLimit)return {};
    const auto open=raw.find('('),close=raw.rfind(')');const bool method=open!=std::string_view::npos;
    if((!method&&close!=std::string_view::npos)||(method&&(close==std::string_view::npos||open>close)))return {};
    auto literal=class_display_label(raw,work);if(!literal||literal->find('\n')!=std::string::npos)return {};
    const auto n=literal->size();
    if(!work.spend(n+32u))return {};
    const bool typed_suffix=!literal->empty()&&(literal->back()=='$'||literal->back()=='*');
    const auto decoded_close=literal->rfind(')');
    const bool typed_slot=method&&decoded_close!=std::string::npos&&decoded_close+1u<literal->size()&&((*literal)[decoded_close+1u]=='$'||(*literal)[decoded_close+1u]=='*');
    if(class_member_tildes(*literal)<=1u&&!typed_suffix&&!typed_slot)return Member{std::move(*literal),method};
    // All field/segment passes and retained strings are paid before scanning or
    // allocating. Final plain validation has its own glyph/storage tariff below.
    if(!work.spend(64u*n+8u*(n+4u)+8u*sizeof(Member)+12u*sizeof(std::string_view)+
                   8u*sizeof(std::size_t)+6u*sizeof(std::string)+512u))return {};
    const auto value=class_member_trim(*literal);
    std::string_view id=value,parameters,returned;char visibility=0,classifier=0;
    const auto visible=[](char byte){return byte=='#'||byte=='+'||byte=='~'||byte=='-';};
    if(method){
        const auto last=value.rfind(')');
        if(last==std::string_view::npos)return {};
        const auto first=value.rfind('(',last);
        if(first==std::string_view::npos||first==0u)return {};
        if(visible(value.front())&&first>1u){visibility=value.front();id=value.substr(1u,first-1u);}
        else id=value.substr(0,first);
        parameters=class_member_trim(value.substr(first+1u,last-first-1u));
        auto tail=value.substr(last+1u);
        if(!tail.empty()){
            if(const auto size=er_metadata_space(tail))tail.remove_prefix(size);
            else if(tail.front()=='$'||tail.front()=='*'){classifier=tail.front();tail.remove_prefix(1u);}
        }
        returned=class_member_trim(tail);
        if(!classifier&&!returned.empty()&&(returned.back()=='$'||returned.back()=='*')){classifier=returned.back();returned.remove_suffix(1u);}
    }else{
        if(!value.empty()&&visible(value.front())){visibility=value.front();id.remove_prefix(1u);}
        if(!id.empty()&&(id.back()=='$'||id.back()=='*')){classifier=id.back();id.remove_suffix(1u);}
    }
    if(classifier&&class_member_trim(id).empty())return {};
    const bool leading_space=!id.empty()&&id.front()==' ';
    id=class_member_trim(id);std::string normalized_id;normalized_id.reserve(id.size()+1u);
    if(leading_space)normalized_id+=' ';
    normalized_id.append(id);
    bool changed=false;
    auto shown_id=class_member_generic_field(normalized_id,changed);if(!shown_id)return {};
    auto shown_parameters=class_member_generic_field(parameters,changed);if(!shown_parameters)return {};
    auto shown_return=class_member_generic_field(returned,changed);if(!shown_return)return {};
    if(!changed)return Member{std::move(*literal),method};
    std::string shown;shown.reserve(n+4u);
    if(visibility)shown+=visibility;
    shown+=*shown_id;
    if(method){shown+='(';shown+=*shown_parameters;shown+=')';if(!returned.empty()){shown+=" : ";shown+=*shown_return;}}
    const auto display=class_member_trim(shown);
    if(display.size()!=shown.size())shown=std::string(display);
    if(classifier)shown+=classifier;
    // Utf8ToGlyphs reserves one string slot per UTF8 byte. This pays storage,
    // construction/moves, roundtrip/copy buffers and scans before plain checking.
    if(!work.spend(shown.size()*(4u*sizeof(std::string)+32u)+256u))return {};
    auto safe=validated_plain_label(shown);if(!safe)return {};
    return Member{std::move(*safe),method};
}

// State descriptions and block Notes share the original physical-line guard.
bool state_source_lines(std::string_view source,bool& checked,Work& work) {
    if(!work.spend(4u*sizeof(std::string_view)+8u*sizeof(bool)+32u))return false;
    if(checked)return true;
    if(source.size()>kSourceLimit||!work.spend(source.size()*2u+8u*sizeof(std::size_t)+64u))return false;
    for(std::size_t n=0;n<source.size();++n)
        if(source[n]=='\r'&&(n+1u==source.size()||source[n+1u]!='\n'))return false;
    checked=true;return true;
}
std::optional<std::string> state_description_label(std::string_view raw,Work& work) {
    if(raw.size()>kSourceLimit||!work.spend(raw.size()*(32u+4u*sizeof(std::string)+4u*sizeof(std::string_view))+256u))return {};
    const auto value=trim_relation(raw);
    if(value.empty()||value.size()>256u||value.front()==':'||value.find_first_of("{}")!=std::string_view::npos)return {};
    // The reused atom-safety decoder can search suffixes quadratically.
    if(!work.spend(value.size()*(value.size()+1u)))return {};
    // Quotes in a description are data; only its alias owner removes STR quotes.
    auto text=relation_literal_label(value);
    if(!text||text->size()>256u||text->find('\n')!=std::string::npos)return {};
    return text;
}
struct StateNoteBlock { std::string text;std::size_t after=0; };
std::optional<StateNoteBlock> state_note_block(std::string_view original,std::size_t header,Work& work) {
    if(original.size()>kSourceLimit||header>original.size()||
       !work.spend(original.size()*8u+16u*sizeof(std::size_t)+8u*sizeof(std::string_view)+256u))return {};
    const auto newline=original.find('\n',header);if(newline==std::string_view::npos)return {};
    const auto begin=newline+1u;std::size_t cursor=begin,close=original.size(),after=0,total=0,rows=0;
    while(cursor<original.size()){
        if(!work.spend(64u))return {};
        const auto next=original.find('\n',cursor);const auto end=next==std::string_view::npos?original.size():next;
        const auto value=trim_relation(original.substr(cursor,end-cursor));
        if(value.size()>=8u&&er_ascii_equal(value.substr(0,8),"end note")){
            const auto ch=value.size()==8u?0u:static_cast<unsigned char>(value[8]);
            const bool word=(ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_';
            if(value.size()==8u||!word){
                // A boundary marker with residue cannot silently lose its tail.
                if(value.size()!=8u)return {};
                close=cursor;after=next==std::string_view::npos?original.size():next+1u;break;
            }
        }
        if(value.empty()||value.find("%%")!=std::string_view::npos||er_metadata_space(value))return {};
        // Only boundary whitespace is closed; internal Unicode/data stay literal.
        auto scalar=value.size()-1u;std::size_t continuation=0;
        while(scalar>0&&(static_cast<unsigned char>(value[scalar])&0xc0u)==0x80u){if(!work.spend()||++continuation>3u)return {};--scalar;}
        if(er_metadata_space(value.substr(scalar)))return {};
        const std::size_t separator=rows?1u:0u;
        if(separator>kSourceLimit-total||value.size()>kSourceLimit-total-separator)return {};
        total+=separator+value.size();++rows;
        cursor=next==std::string_view::npos?original.size():next+1u;
    }
    if(!after||!rows||close==original.size())return {};
    // Prepay exact join storage and reused decoder worst suffix scans before
    // allocation/decoding. The original source bound makes multiplication safe.
    if(!work.spend(total*(total+1u)+total*(32u+4u*sizeof(std::string)+4u*sizeof(std::string_view))+
                   (total+1u)*3u+8u*sizeof(StateNoteBlock)+256u))return {};
    std::string joined;joined.reserve(total);cursor=begin;
    while(cursor<close){
        if(!work.spend(16u))return {};
        const auto next=original.find('\n',cursor);if(next==std::string_view::npos||next>close)return {};
        const auto value=trim_relation(original.substr(cursor,next-cursor));
        if(!joined.empty())joined+='\n';
        joined.append(value);cursor=next+1u;
    }
    if(joined.size()!=total)return {};
    auto text=relation_literal_label(joined);if(!text)return {};
    return StateNoteBlock{std::move(*text),after};
}
// This finite Class profile retains declarations only when the pinned
// defineClass operation cannot affect any implicit-default class in this source.
// Accepted classes have exactly the implicit CSS class string "default";
// explicit CSS associations remain closed in their owning parser branches.
bool class_inactive_definition(std::string_view line,std::string_view original,
                               std::vector<std::string_view>& definitions,
                               bool& source_checked,Work& work) {
    if(!work.spend(line.size()*12u+16u*sizeof(std::string_view)+8u*sizeof(std::size_t)+128u))return false;
    if(!source_checked){
        if(original.size()>kSourceLimit||!work.spend(original.size()*2u+8u*sizeof(std::size_t)+64u))return false;
        for(std::size_t index=0;index<original.size();++index)
            if(original[index]=='\r'&&(index+1u==original.size()||original[index+1u]!='\n'))return false;
        source_checked=true;
    }
    auto rest=trim_relation(line.substr(8u));
    const auto split=rest.find_first_of(" \t");
    if(split==std::string_view::npos||split==0u||split>64u)return false;
    const auto name=rest.substr(0,split);
    for(std::size_t n=0;n<name.size();++n){
        const auto c=static_cast<unsigned char>(name[n]);
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||
             (n>0u&&(c>='0'&&c<='9'))))return false;
    }
    // INITIAL classList requires ALPHA. Fixed lexer words (including call's
    // state transition and the single-letter aggregation token) are not IDs.
    if(!work.spend(4u*17u*sizeof(std::string_view)+128u))return false;
    constexpr std::array<std::string_view,17> reserved{{
        "classDiagram","style","classDef","namespace","class","cssClass",
        "callback","link","click","note","href","call",
        "_self","_blank","_parent","_top","o"}};
    for(const auto keyword:reserved){
        if(!work.spend(name.size()+keyword.size()+16u)||name==keyword)return false;
    }
    rest=trim_relation(rest.substr(split));
    if(rest.empty()||definitions.size()>=64u)return false;
    for(const auto previous:definitions){
        if(!work.spend(previous.size()+name.size()+16u)||previous==name)return false;
    }
    unsigned seen=0;
    while(!rest.empty()){
        const auto comma=rest.find(',');
        const auto item=trim_relation(rest.substr(0,comma));
        const auto colon=item.find(':');
        if(colon==std::string_view::npos)return false;
        const auto key=trim_relation(item.substr(0,colon));
        const auto value=trim_relation(item.substr(colon+1u));
        const unsigned bit=key=="fill"?1u:key=="stroke"?2u:key=="color"?4u:key=="stroke-width"?8u:0u;
        if(!bit||(seen&bit)||value.empty()||(bit==8u&&seen==0u))return false;
        seen|=bit;
        if(bit==8u){
            if(!value.ends_with("px"))return false;
            const auto digits=value.substr(0,value.size()-2u);
            if(digits.empty()||digits.size()>4u||digits.front()=='0')return false;
            unsigned pixels=0;
            for(const char c:digits){if(c<'0'||c>'9')return false;pixels=pixels*10u+static_cast<unsigned>(c-'0');}
            if(pixels==0u||pixels>1024u)return false;
        }else{
            if((value.size()!=4u&&value.size()!=7u)||value.front()!='#')return false;
            for(const char c:value.substr(1u))
                if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
        }
        if(comma==std::string_view::npos)break;
        rest=trim_relation(rest.substr(comma+1u));if(rest.empty())return false;
    }
    // Conservative whole-source profile: a default-matching ID is closed even
    // before the first class. Do not infer future stylesheet behavior merely
    // from the absence of current vertices at the declaration statement.
    if(!work.spend(name.size()+32u)||
       std::string_view("default").find(name)!=std::string_view::npos)return false;
    if(definitions.empty()){
        // Empty reserve: allocation only; later writes fit this finite bound.
        if(!work.spend(64u*sizeof(std::string_view)+alignof(std::string_view)+64u))return false;
        definitions.reserve(64u);
    }
    if(!work.spend(sizeof(std::string_view)*2u+16u))return false;
    definitions.push_back(name);return true;
}


// Requirement is intentionally a private typed language.  These records never
// enter Class, ER, or State parsing and only project to neutral boxes after
// every declaration and endpoint has been resolved.
enum class RequirementKind : unsigned char {
    Requirement, FunctionalRequirement, InterfaceRequirement,
    PerformanceRequirement, PhysicalRequirement, DesignConstraint,
};
enum class RequirementRelation : unsigned char {
    Contains, Copies, Derives, Satisfies, Verifies, Refines, Traces,
};
enum class Risk : unsigned char { Low, Medium, High };
enum class Verification : unsigned char { Analysis, Demonstration, Inspection, Test };
enum class RequirementRole : unsigned char { Requirement, Element };
struct NodeRef {
    RequirementRole role;
    std::size_t index = 0;
    bool operator==(const NodeRef&) const = default;
};
struct RequirementFields {
    RequirementKind kind;
    std::string logical_name;
    std::optional<std::string> id, text;
    std::optional<Risk> risk;
    std::optional<Verification> verification;
};
struct ElementFields {
    std::string logical_name;
    std::optional<std::string> type, doc_ref;
};
using RequirementNode = std::variant<RequirementFields, ElementFields>;
struct RequirementEdge {
    RequirementRelation kind;
    NodeRef source, target;
    std::size_t statement_order = 0;
};
struct RequirementEnvelope {
    std::string_view body;
    // Both physical and decoded forms are kept through typed parse.  They are
    // inert metadata, never a title or text substituted into a box.
    std::string raw_description, decoded_description;
};
struct RequirementDiagram {
    RequirementEnvelope envelope;
    std::string direction = "TB";
    std::vector<RequirementNode> nodes;
    std::vector<RequirementEdge> edges;
};
struct PendingRequirementEdge {
    RequirementRelation kind;
    std::string source, target;
    std::size_t statement_order = 0;
};
struct RequirementDeclaration {
    RequirementNode node;
    bool inline_empty = false;
};

char requirement_fold(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}
bool requirement_equal(std::string_view left,std::string_view right) {
    if(left.size()!=right.size())return false;
    for(std::size_t at=0;at<left.size();++at)
        if(requirement_fold(left[at])!=requirement_fold(right[at]))return false;
    return true;
}
bool requirement_word_boundary(std::string_view value,std::size_t at) {
    return at>=value.size()||value[at]==' '||value[at]=='\t';
}
std::optional<std::string_view> requirement_keyword_tail(std::string_view line,std::string_view keyword) {
    if(line.size()<keyword.size()||!requirement_equal(line.substr(0,keyword.size()),keyword)||
       !requirement_word_boundary(line,keyword.size()))return {};
    return trim_relation(line.substr(keyword.size()));
}
bool requirement_bare_header(std::string_view source) {
    const auto end=source.find('\n');
    return requirement_equal(trim_relation(source.substr(0,end)),"requirementDiagram");
}
bool requirement_space(char value) {
    return value==' '||value=='\t'||value=='\r'||value=='\f'||value=='\v';
}
std::optional<std::string_view> requirement_greedy_direction(std::string_view line,Work& work) {
    // In INITIAL, the locked lexer tries whole-physical-line rules in this
    // order: .*direction\s+TB[^\n]*, then BT, RL, LR.  Those rules run
    // before quote-state entry, so a matching quoted fragment is deliberately
    // not filtered here. This parser applies the same priority only to a
    // top-level physical statement before declaration/relationship handling.
    if(!work.spend(line.size()*8u+4u*sizeof(std::size_t)+64u))return {};
    constexpr std::array<std::string_view,4> priority{{"TB","BT","RL","LR"}};
    constexpr std::string_view token="direction";
    for(const auto direction:priority) {
        for(std::size_t at=0;at+token.size()<=line.size();++at) {
            if(!requirement_equal(line.substr(at,token.size()),token))continue;
            std::size_t value=at+token.size();
            if(value==line.size()||!requirement_space(line[value]))continue;
            while(value<line.size()&&requirement_space(line[value]))++value;
            if(value+direction.size()<=line.size()&&requirement_equal(line.substr(value,direction.size()),direction))return direction;
        }
    }
    return {};
}
bool requirement_literal_prepay(std::string_view value,Work& work) {
    // relation_literal_label allocates its decoded string and copies each UTF-8
    // row before it can return a checked value.  Charge its bounded scans and
    // temporaries before invoking it; final projection strings retain their
    // own existing charge at the caller.
    const auto bytes=value.size(),rows=bytes+1u;
    // Utf8ToGlyphs reserves one string slot per input byte.  Its decoded
    // string, row views, temporary row strings and glyph slots are all bounded
    // before the helper call, using the established relation-TU storage rate.
    constexpr std::size_t per_row=16u+4u*sizeof(std::string)+4u*sizeof(std::string_view);
    constexpr std::size_t fixed=4u*sizeof(std::string)+128u;
    if(bytes>kLabelLimit||bytes>work.remaining/rows)return false;
    const auto scan=bytes*rows;
    const auto remaining=work.remaining-scan;
    if(rows>remaining/per_row)return false;
    const auto temporaries=rows*per_row;
    if(fixed>remaining-temporaries)return false;
    return work.spend(scan+temporaries+fixed);
}
std::optional<std::string> requirement_string(std::string_view value,Work& work) {
    value=trim_relation(value);
    if(value.empty()||value.size()>kLabelLimit||value.find_first_of("\r\n{}")!=std::string_view::npos)return {};
    if(value.front()=='"') {
        if(value.size()<2u||value.back()!='"')return {};
        const auto inner=value.substr(1,value.size()-2u);
        if(inner.find_first_of("\r\n\\\"")!=std::string_view::npos||
           !work.spend(inner.size()*8u+sizeof(std::string)+64u))return {};
        return std::string(inner);
    }
    if(value.find('"')!=std::string_view::npos||!work.spend(value.size()*8u+sizeof(std::string)+64u))return {};
    return std::string(value);
}
std::optional<std::string> requirement_scalar(std::string_view value,Work& work) {
    // A present empty scalar is only the explicit quoted spelling.  Omitted
    // fields remain nullopt, so projection never creates a blank metadata row.
    return requirement_string(value,work);
}
std::optional<RequirementKind> requirement_kind(std::string_view value) {
    if(requirement_equal(value,"requirement"))return RequirementKind::Requirement;
    if(requirement_equal(value,"functionalRequirement"))return RequirementKind::FunctionalRequirement;
    if(requirement_equal(value,"interfaceRequirement"))return RequirementKind::InterfaceRequirement;
    if(requirement_equal(value,"performanceRequirement"))return RequirementKind::PerformanceRequirement;
    if(requirement_equal(value,"physicalRequirement"))return RequirementKind::PhysicalRequirement;
    if(requirement_equal(value,"designConstraint"))return RequirementKind::DesignConstraint;
    return {};
}
std::optional<RequirementRelation> requirement_relation(std::string_view value) {
    if(requirement_equal(value,"contains"))return RequirementRelation::Contains;
    if(requirement_equal(value,"copies"))return RequirementRelation::Copies;
    if(requirement_equal(value,"derives"))return RequirementRelation::Derives;
    if(requirement_equal(value,"satisfies"))return RequirementRelation::Satisfies;
    if(requirement_equal(value,"verifies"))return RequirementRelation::Verifies;
    if(requirement_equal(value,"refines"))return RequirementRelation::Refines;
    if(requirement_equal(value,"traces"))return RequirementRelation::Traces;
    return {};
}
std::optional<Risk> requirement_risk(std::string_view value) {
    if(requirement_equal(value,"low"))return Risk::Low;
    if(requirement_equal(value,"medium"))return Risk::Medium;
    if(requirement_equal(value,"high"))return Risk::High;
    return {};
}
std::optional<Verification> requirement_verification(std::string_view value) {
    if(requirement_equal(value,"analysis"))return Verification::Analysis;
    if(requirement_equal(value,"demonstration"))return Verification::Demonstration;
    if(requirement_equal(value,"inspection"))return Verification::Inspection;
    if(requirement_equal(value,"test"))return Verification::Test;
    return {};
}
std::string_view requirement_kind_text(RequirementKind value) {
    switch(value) {
    case RequirementKind::Requirement:return "Requirement";
    case RequirementKind::FunctionalRequirement:return "Functional Requirement";
    case RequirementKind::InterfaceRequirement:return "Interface Requirement";
    case RequirementKind::PerformanceRequirement:return "Performance Requirement";
    case RequirementKind::PhysicalRequirement:return "Physical Requirement";
    case RequirementKind::DesignConstraint:return "Design Constraint";
    }
    return {};
}
std::string_view requirement_risk_text(Risk value) {
    switch(value) {
    case Risk::Low:return "Low";case Risk::Medium:return "Medium";case Risk::High:return "High";
    }
    return {};
}
std::string_view requirement_verification_text(Verification value) {
    switch(value) {
    case Verification::Analysis:return "Analysis";case Verification::Demonstration:return "Demonstration";
    case Verification::Inspection:return "Inspection";case Verification::Test:return "Test";
    }
    return {};
}
std::string_view requirement_relation_text(RequirementRelation value) {
    switch(value) {
    case RequirementRelation::Contains:return "contains";case RequirementRelation::Copies:return "copies";
    case RequirementRelation::Derives:return "derives";case RequirementRelation::Satisfies:return "satisfies";
    case RequirementRelation::Verifies:return "verifies";case RequirementRelation::Refines:return "refines";
    case RequirementRelation::Traces:return "traces";
    }
    return {};
}

std::optional<RequirementEnvelope> requirement_envelope(std::string_view source,Work& work) {
    if(source.size()>kSourceLimit||source.find('\r')!=std::string_view::npos||
       !source.starts_with("---\n")||!work.spend(source.size()*2u+96u))return {};
    const auto envelope=parse_envelope(source,work);
    if(!envelope||!envelope->has_frontmatter||!envelope->description)return {};
    for(const auto& event:envelope->events) {
        if(event.blank||event.comment)continue;
        if(event.top_level&&event.path!="description")return {};
        if(!event.top_level)return {};
    }
    const auto& description=*envelope->description;
    std::string raw(envelope->raw_frontmatter),decoded;
    if(description.form==EnvelopeScalarForm::Folded&&description.header==">-") {
        bool payload=false;
        std::size_t at=0;
        while(at<description.text.size()) {
            const auto newline=description.text.find('\n',at);
            const auto line=description.text.substr(at,newline==std::string_view::npos?
                std::string_view::npos:newline-at);
            at=newline==std::string_view::npos?description.text.size():newline+1u;
            if(line.empty())continue;
            if(line.size()<3u||!line.starts_with("  ")||line.starts_with("   ")||
               line.find('\t')!=std::string_view::npos)return {};
            if(payload)decoded.push_back(' ');
            decoded.append(line.substr(2));
            payload=true;
        }
        if(!payload)return {};
    } else if(description.form==EnvelopeScalarForm::Plain&&!description.header.empty()) {
        const auto scalar=description.header;
        if(scalar.empty()||scalar.front()==' '||scalar.front()=='\''||scalar.front()=='"'
           ||scalar.front()=='|'||scalar.front()=='>'||scalar.find('\t')!=std::string_view::npos)return {};
        decoded=scalar;
    } else {
        return {};
    }
    if(!envelope->raw_body.starts_with("requirementDiagram"))return {};
    return RequirementEnvelope{envelope->raw_body,std::move(raw),std::move(decoded)};
}
bool requirement_same_role_name(const RequirementNode& node,RequirementRole role,std::string_view name) {
    if(role==RequirementRole::Requirement) {
        const auto* value=std::get_if<RequirementFields>(&node);
        return value&&value->logical_name==name;
    }
    const auto* value=std::get_if<ElementFields>(&node);
    return value&&value->logical_name==name;
}
bool requirement_append_node(RequirementDiagram& diagram,RequirementNode&& node,Work& work) {
    const auto role=std::holds_alternative<RequirementFields>(node)?RequirementRole::Requirement:RequirementRole::Element;
    const auto& name=role==RequirementRole::Requirement?std::get<RequirementFields>(node).logical_name:
                                                       std::get<ElementFields>(node).logical_name;
    if(name.size()>kLabelLimit||!work.spend(name.size()*8u+sizeof(RequirementNode)+64u))return false;
    for(const auto& old:diagram.nodes) {
        if(!work.spend(name.size()+sizeof(RequirementNode)+8u))return false;
        if(requirement_same_role_name(old,role,name))return true; // first same-kind definition wins
    }
    if(diagram.nodes.size()>=64u)return false;
    if(diagram.nodes.size()==diagram.nodes.capacity()) {
        const auto capacity=std::min(std::size_t{64u},std::max(std::size_t{1u},diagram.nodes.size()*2u));
        if(!work.spend((capacity+diagram.nodes.size()+1u)*sizeof(RequirementNode)+128u))return false;
        diagram.nodes.reserve(capacity);
    }
    diagram.nodes.push_back(std::move(node));return true;
}
bool requirement_append_pending(std::vector<PendingRequirementEdge>& edges,PendingRequirementEdge&& edge,Work& work) {
    if(edges.size()>=128u||edge.source.size()>kLabelLimit||edge.target.size()>kLabelLimit||
       !work.spend((edge.source.size()+edge.target.size())*8u+sizeof(PendingRequirementEdge)+64u))return false;
    if(edges.size()==edges.capacity()) {
        const auto capacity=std::min(std::size_t{128u},std::max(std::size_t{1u},edges.size()*2u));
        if(!work.spend((capacity+edges.size()+1u)*sizeof(PendingRequirementEdge)+128u))return false;
        edges.reserve(capacity);
    }
    edges.push_back(std::move(edge));return true;
}
bool requirement_append_edge(RequirementDiagram& diagram,RequirementEdge edge,Work& work) {
    if(diagram.edges.size()>=128u||!work.spend(sizeof(RequirementEdge)+64u))return false;
    if(diagram.edges.size()==diagram.edges.capacity()) {
        const auto capacity=std::min(std::size_t{128u},std::max(std::size_t{1u},diagram.edges.size()*2u));
        if(!work.spend((capacity+diagram.edges.size()+1u)*sizeof(RequirementEdge)+128u))return false;
        diagram.edges.reserve(capacity);
    }
    diagram.edges.push_back(edge);return true;
}
std::optional<NodeRef> requirement_resolve(const RequirementDiagram& diagram,std::string_view name,Work& work) {
    // The source DB checks Requirement before Element when names collide.
    for(const auto desired:{RequirementRole::Requirement,RequirementRole::Element}) {
        for(std::size_t at=0;at<diagram.nodes.size();++at) {
            if(!work.spend(name.size()+sizeof(RequirementNode)+8u))return {};
            if(requirement_same_role_name(diagram.nodes[at],desired,name))return NodeRef{desired,at};
        }
    }
    return {};
}
bool requirement_field(RequirementNode& node,std::string_view line,Work& work) {
    const auto colon=line.find(':');
    if(colon==std::string_view::npos)return false;
    const auto key=trim_relation(line.substr(0,colon));
    const auto value=requirement_scalar(line.substr(colon+1),work);
    if(!value||!work.spend(key.size()+value->size()*6u+64u))return false;
    if(auto* fields=std::get_if<RequirementFields>(&node)) {
        if(requirement_equal(key,"id")) {if(!fields->id)fields->id=std::move(*value);return true;}
        if(requirement_equal(key,"text")) {if(!fields->text)fields->text=std::move(*value);return true;}
        if(requirement_equal(key,"risk")) {
            const auto parsed=requirement_risk(*value);if(!parsed)return false;
            if(!fields->risk)fields->risk=*parsed;return true;
        }
        if(requirement_equal(key,"verifymethod")) {
            const auto parsed=requirement_verification(*value);if(!parsed)return false;
            if(!fields->verification)fields->verification=*parsed;return true;
        }
        return false;
    }
    auto& fields=std::get<ElementFields>(node);
    if(requirement_equal(key,"type")) {if(!fields.type)fields.type=std::move(*value);return true;}
    if(requirement_equal(key,"docRef")) {if(!fields.doc_ref)fields.doc_ref=std::move(*value);return true;}
    return false;
}
std::optional<RequirementDeclaration> requirement_declaration(std::string_view line,Work& work) {
    const auto open=line.rfind('{');
    if(open==std::string_view::npos)return {};
    const auto after=trim_relation(line.substr(open+1));
    const bool inline_empty=after=="}";
    if(!after.empty()&&!inline_empty)return {};
    const auto before=trim_relation(line.substr(0,open));
    for(const auto keyword:{"functionalRequirement","interfaceRequirement","performanceRequirement",
                            "physicalRequirement","designConstraint","requirement","element"}) {
        const auto tail=requirement_keyword_tail(before,keyword);if(!tail)continue;
        const auto name=requirement_string(*tail,work);if(!name||!work.spend(before.size()+name->size()*6u+64u))return {};
        if(requirement_equal(keyword,"element"))return RequirementDeclaration{RequirementNode{ElementFields{std::move(*name),{}, {}}},inline_empty};
        const auto kind=requirement_kind(keyword);if(!kind)return {};
        return RequirementDeclaration{RequirementNode{RequirementFields{*kind,std::move(*name),{}, {}, {}, {}}},inline_empty};
    }
    return {};
}
std::optional<std::size_t> requirement_arrow(std::string_view line,std::string_view token) {
    bool quote=false;
    for(std::size_t at=0;at+token.size()<=line.size();++at) {
        if(line[at]=='"')quote=!quote;
        if(!quote&&line.substr(at,token.size())==token)return at;
    }
    return {};
}
std::optional<std::size_t> requirement_separator(std::string_view line,bool last) {
    bool quote=false;std::optional<std::size_t> result;
    for(std::size_t at=0;at<line.size();++at) {
        if(line[at]=='"')quote=!quote;
        if(!quote&&line[at]=='-') {
            result=at;if(!last)return result;
        }
    }
    return result;
}
std::optional<PendingRequirementEdge> requirement_relationship(std::string_view line,std::size_t order,Work& work) {
    const auto forward=requirement_arrow(line,"->"),reverse=requirement_arrow(line,"<-");
    if(forward&&reverse)return {};
    const bool reversed=reverse.has_value();
    const auto arrow=reversed?reverse:forward;if(!arrow)return {};
    const auto left=trim_relation(line.substr(0,*arrow));
    const auto right=trim_relation(line.substr(*arrow+2u));
    const auto split=reversed?requirement_separator(right,false):requirement_separator(left,true);
    if(!split)return {};
    const auto source_text=reversed?trim_relation(right.substr(*split+1u)):trim_relation(left.substr(0,*split));
    const auto relation_text=reversed?trim_relation(right.substr(0,*split)):trim_relation(left.substr(*split+1u));
    const auto target_text=reversed?left:right;
    const auto source=requirement_string(source_text,work),target=requirement_string(target_text,work);
    const auto kind=requirement_relation(relation_text);
    if(!source||!target||!kind||!work.spend(line.size()*6u+source->size()+target->size()+128u))return {};
    return PendingRequirementEdge{*kind,std::move(*source),std::move(*target),order};
}
std::optional<RequirementDiagram> parse_requirement(std::string_view source,Work& work) {
    if(source.empty()||source.size()>kSourceLimit||!work.spend(source.size()))return {};
    std::optional<RequirementEnvelope> envelope;
    if(source.starts_with("---")) {
        // A prefixed source is admitted only after the strict complete-prefix
        // reader closes it; a convenient body suffix can never rescue it.
        envelope=requirement_envelope(source,work);if(!envelope)return {};
    } else {
        if(!requirement_bare_header(source)||!work.spend(sizeof(RequirementEnvelope)+64u))return {};
        envelope=RequirementEnvelope{source,{},{}};
    }
    if(!work.spend(2u*sizeof(std::vector<RequirementNode>)+2u*sizeof(std::vector<RequirementEdge>)+
                   sizeof(RequirementDiagram)+128u))return {};
    RequirementDiagram diagram{std::move(*envelope),"TB",{}, {}};
    std::vector<PendingRequirementEdge> pending;
    if(!work.spend(sizeof(std::vector<PendingRequirementEdge>)+128u))return {};
    std::size_t at=0,statement_order=0;bool header=false;
    const auto next_line=[&](std::string_view& line)->bool {
        if(at>diagram.envelope.body.size())return false;
        const auto end=diagram.envelope.body.find('\n',at);
        if(end==std::string_view::npos) {
            if(at==diagram.envelope.body.size())return false;
            line=trim_relation(diagram.envelope.body.substr(at));at=diagram.envelope.body.size();
            return work.spend(line.size()+1u);
        }
        line=trim_relation(diagram.envelope.body.substr(at,end-at));at=end+1u;
        return work.spend(line.size()+1u);
    };
    for(;;) {
        std::string_view line;
        if(!next_line(line))break;
        if(line.empty()||line.starts_with("%%"))continue;
        if(!header) {
            if(!requirement_equal(line,"requirementDiagram"))return {};
            header=true;continue;
        }
        if(const auto direction=requirement_greedy_direction(line,work)) {
            if(!work.spend(direction->size()*4u+sizeof(std::string)+16u))return {};
            diagram.direction=std::string(*direction);continue; // greedy: last accepted line wins
        }
        if(auto declaration=requirement_declaration(line,work)) {
            auto node=std::move(declaration->node);bool closed=declaration->inline_empty;
            while(!closed) {
                std::string_view field;if(!next_line(field))return {};
                if(field.empty()||field.starts_with("%%"))continue;
                if(field=="}") {closed=true;break;}
                if(!requirement_field(node,field,work))return {};
            }
            if(!requirement_append_node(diagram,std::move(node),work))return {};
            continue;
        }
        auto edge=requirement_relationship(line,statement_order,work);
        if(!edge)return {};
        if(!requirement_append_pending(pending,std::move(*edge),work))return {};
        ++statement_order;
    }
    if(!header||diagram.nodes.empty())return {};
    for(const auto& edge:pending) {
        const auto source= requirement_resolve(diagram,edge.source,work);
        const auto target= requirement_resolve(diagram,edge.target,work);
        if(!source||!target)return {}; // unresolved endpoint rejects the whole source
        if(!requirement_append_edge(diagram,{edge.kind,*source,*target,edge.statement_order},work))return {};
    }
    return diagram;
}
std::optional<std::string> requirement_identity(RequirementRole role,std::string_view name,Work& work) {
    if(name.size()>kLabelLimit||!work.spend(name.size()*8u+96u))return {};
    std::string result=role==RequirementRole::Requirement?"requirement:":"element:";
    result.reserve(result.size()+name.size()*2u);
    constexpr char hex[]="0123456789abcdef";
    for(const unsigned char byte:name) {result.push_back(hex[byte>>4u]);result.push_back(hex[byte&15u]);}
    return result;
}
std::optional<std::string> requirement_box_label(std::string_view stereotype,std::string_view name,Work& work) {
    // Stereotype delimiters are fixed Requirement presentation, never parsed
    // source markup.  The source name remains independently plain-text
    // validated before joining the two visible rows.
    if(!requirement_literal_prepay(name,work))return {};
    const auto checked=relation_literal_label(name);
    if(!checked||*checked!=name||!work.spend((stereotype.size()+name.size())*8u+sizeof(std::string)+64u))return {};
    std::string label="<<";label.append(stereotype);label.append(">>\n");label.append(name);
    if(label_width(label)>kLabelLimit)return {};
    return label;
}
std::optional<std::string> requirement_member_text(std::string_view key,std::string_view value,Work& work) {
    if(!requirement_literal_prepay(value,work))return {};
    const auto checked=relation_literal_label(value);
    if(!checked||*checked!=value||!work.spend((key.size()+value.size())*8u+sizeof(std::string)+64u))return {};
    return std::string(key)+": "+std::string(value);
}
std::optional<RelationDiagram> project_requirement(const RequirementDiagram& parsed,Work& work) {
    if(parsed.nodes.empty()||parsed.nodes.size()>64u||parsed.edges.size()>128u||
       !work.spend(parsed.nodes.size()*(sizeof(RelationEntity)+4u*sizeof(Member)+128u)+
                   parsed.edges.size()*(sizeof(Relationship)+128u)+256u))return {};
    if(!work.spend(parsed.direction.size()*4u+sizeof(std::string)+32u))return {};
    RelationDiagram result;result.family=RelationFamily::Requirement;result.direction=parsed.direction;
    result.entities.reserve(parsed.nodes.size());result.edges.reserve(parsed.edges.size());
    if(!work.spend(parsed.nodes.size()*sizeof(int)+128u))return {};
    std::vector<int> projected;projected.reserve(parsed.nodes.size());
    for(std::size_t at=0;at<parsed.nodes.size();++at) {
        const auto role=std::holds_alternative<RequirementFields>(parsed.nodes[at])?RequirementRole::Requirement:RequirementRole::Element;
        const auto name=role==RequirementRole::Requirement?std::string_view(std::get<RequirementFields>(parsed.nodes[at]).logical_name):
                                                            std::string_view(std::get<ElementFields>(parsed.nodes[at]).logical_name);
        const auto id=requirement_identity(role,name,work);if(!id)return {};
        std::string label;std::vector<Member> rows;
        if(role==RequirementRole::Requirement) {
            const auto& fields=std::get<RequirementFields>(parsed.nodes[at]);
            const auto kind=requirement_kind_text(fields.kind);
            if(!work.spend(4u*sizeof(Member)+sizeof(std::vector<Member>)+128u))return {};
            const auto title=requirement_box_label(kind,name,work);if(!title)return {};
            label=std::move(*title);rows.reserve(4u);
            const auto row=[&](std::string_view key,std::string_view value)->bool {
                const auto text=requirement_member_text(key,value,work);if(!text||!work.spend(sizeof(Member)+32u))return false;
                rows.push_back({std::move(*text),false});return true;
            };
            if(fields.id&&!row("ID",*fields.id))return {};
            if(fields.text&&!row("Text",*fields.text))return {};
            if(fields.risk&&!row("Risk",requirement_risk_text(*fields.risk)))return {};
            if(fields.verification&&!row("Verification",requirement_verification_text(*fields.verification)))return {};
        } else {
            const auto& fields=std::get<ElementFields>(parsed.nodes[at]);
            if(!work.spend(2u*sizeof(Member)+sizeof(std::vector<Member>)+128u))return {};
            const auto title=requirement_box_label("Element",name,work);if(!title)return {};
            label=std::move(*title);rows.reserve(2u);
            const auto row=[&](std::string_view key,std::string_view value)->bool {
                const auto text=requirement_member_text(key,value,work);if(!text||!work.spend(sizeof(Member)+32u))return false;
                rows.push_back({std::move(*text),false});return true;
            };
            if(fields.type&&!row("Type",*fields.type))return {};
            if(fields.doc_ref&&!row("Doc Ref",*fields.doc_ref))return {};
        }
        RelationEntity entity{std::move(*id),std::move(label),{},EntityShape::Box,-1,std::move(rows),true};
        result.entities.push_back(std::move(entity));projected.push_back(static_cast<int>(at));
    }
    for(const auto& edge:parsed.edges) {
        if(edge.source.index>=projected.size()||edge.target.index>=projected.size())return {};
        Relationship projected_edge;projected_edge.from=projected[edge.source.index];projected_edge.to=projected[edge.target.index];
        const auto relation_name=requirement_relation_text(edge.kind);
        if(!work.spend(relation_name.size()*8u+sizeof(std::string)+64u))return {};
        projected_edge.label="<<"+std::string(relation_name)+">>";
        if(edge.kind==RequirementRelation::Contains) {
            projected_edge.start=Marker::RequirementCirclePlus;projected_edge.end=Marker::None;projected_edge.dotted=false;
        } else {
            projected_edge.start=Marker::None;projected_edge.end=Marker::RequirementOpenArrow;projected_edge.dotted=true;
        }
        if(!append_relationship(result.edges,std::move(projected_edge),work))return {};
    }
    return result;
}

std::optional<RelationDiagram> parse_relations(std::string_view source,Work& work,const ClassEnvelope* class_envelope=nullptr) {
    if (source.empty() || source.size() > kSourceLimit || !work.spend(source.size())) return {};
    // All parser branches construct at most one Diagram; pay the new field
    // before either the unchanged plain-ER helper or Class/State construction.
    if(!work.spend(16u*sizeof(std::optional<bool>)+8u*sizeof(std::vector<int>)+alignof(std::vector<int>)+8u*sizeof(bool)+64u))return {};
    // Parse-local forward targets own their strings only until full-source resolve.
    // The empty vector and Diagram's active scalar exist on every family path.
    if(!work.spend(4u*sizeof(std::vector<PendingClassNote>)+alignof(std::vector<PendingClassNote>)+
        8u*sizeof(bool)+8u*alignof(RelationDiagram)+64u))return {};
    std::vector<PendingClassNote> class_note_targets;
    bool class_generic_source_checked=false;
    // New parse-local borrowed declaration ledger has fixed lifetime work in all families.
    if(!work.spend(8u*sizeof(std::vector<std::string_view>)+alignof(std::vector<std::string_view>)+64u))return {};
    std::vector<std::string_view> inactive_class_definitions;
    if(!work.spend(8u*sizeof(std::vector<StateTextRecord>)+alignof(std::vector<StateTextRecord>)+8u*sizeof(bool)+64u))return {};
    bool state_source_lines_checked=false;
    if(!work.spend(8u*sizeof(bool)+alignof(bool)+16u))return {};
    bool state_block_note_seen=false;
    // The local predicate exists for every invocation; pay its real lifetime
    // before either the unchanged ER early branch or Class/State construction.
    if(!work.spend(8u*sizeof(bool)+4u*sizeof(std::size_t)+32u))return {};
    const bool class_flat_namespaces=class_envelope&&class_envelope->hierarchical_namespaces==false;
    const auto original=source;
    if(class_flat_namespaces){
        // New false-profile declarations use the LF-owned Class walker. Keep
        // CRLF intact and close bare-CR sources before any trim can join tokens.
        if(!work.spend(original.size()*2u+8u*sizeof(std::size_t)+64u))return {};
        for(std::size_t at=0;at<original.size();++at)
            if(original[at]=='\r'&&(at+1u==original.size()||original[at+1u]!='\n'))return {};
    }
    if(!class_envelope&&(original.starts_with("---")||original.starts_with("%%{"))) {
        // RFC 0031 E2a: distinguish state frontmatter from ER before parsing.
        // A bounded structural peek finds the closing fence and the first real
        // body line; the ER Work-charge sequence below is left byte-identical.
        bool state_route=false;
        if(original.starts_with("---")) {
            auto scan=original;
            const auto first_newline=scan.find('\n');
            if(first_newline==std::string_view::npos)return {};
            scan.remove_prefix(first_newline+1u);
            bool closed=false;
            while(!scan.empty()) {
                const auto at=scan.find('\n');
                auto line=scan.substr(0,at);
                if(line.ends_with('\r'))line.remove_suffix(1);
                scan=at==std::string_view::npos?std::string_view{}:scan.substr(at+1u);
                if(!closed){if(line=="---")closed=true;continue;}
                line=trim_relation(line);
                if(line.empty()||line.starts_with("%%"))continue;
                state_route=line=="stateDiagram-v2"||line=="stateDiagram";
                break;
            }
        }
        if(state_route) {
            auto state=state_source_envelope(original,work);if(!state)return {};
            auto diagram=parse_relations(state->body,work);if(!diagram)return {};
            if(state->title)diagram->diagram_title=*state->title;
            return diagram;
        }
        auto envelope=er_source_envelope(original,work);if(!envelope)return {};
        ErCursor header{envelope->body,work};if(!header.take("erDiagram",true))return {};
        auto diagram=parse_plain_er(envelope->body,work);if(!diagram)return {};
        diagram->diagram_title=std::move(envelope->title);return diagram;
    }
    std::vector<std::string_view> lines;
    while (!source.empty()) {
        const auto at = source.find('\n'); const auto line = trim_relation(source.substr(0,at));
        if (line.starts_with("%%{")) return {};
        if (!line.empty() && !line.starts_with("%%")) lines.push_back(line);
        if (at == std::string_view::npos) break;
        source.remove_prefix(at+1);
    }
    if (lines.empty()) return {};
    const auto first=lines.front();
    if(class_envelope&&first!="classDiagram"&&first!="classDiagram-v2")return {};
    if(first.size()>=9&&er_ascii_equal(first.substr(0,9),"erDiagram")&&
       (first.size()==9||first[9]==' '||first[9]=='\t'||first[9]=='\r'))return parse_plain_er(original,work);
    RelationDiagram diagram;
    if(class_envelope) {
        const auto bytes=class_envelope->title?class_envelope->title->size():0u;
        if(!work.spend(bytes*2u+4u*sizeof(ClassEnvelope)+32u))return {};
        diagram.diagram_title=class_envelope->title;
        diagram.class_hide_empty_members_box=class_envelope->hide_empty_members_box;
    }
    if (lines[0] == "classDiagram" || lines[0] == "classDiagram-v2") diagram.family = RelationFamily::Class;
    else if (lines[0] == "stateDiagram-v2" || lines[0] == "stateDiagram") diagram.family = RelationFamily::State;
    else return {};
    if(diagram.family==RelationFamily::Class){auto logical=class_statement_lines(original,work);if(!logical)return {};lines=std::move(*logical);}
    std::vector<int> scopes; int member = -1; std::size_t groups = 0;
    if(class_flat_namespaces){
        // Both vectors are empty here. Allocate their existing finite bounds
        // before any borrowed entity reference or lexical scope insertion.
        // Existing entity constructors separately pay each inserted object.
        if(!work.spend((64u+kGroupLimit)*sizeof(RelationEntity)+alignof(RelationEntity)+
            2u*kGroupDepthLimit*sizeof(int)+alignof(int)+8u*sizeof(std::size_t)+128u))return {};
        diagram.entities.reserve(64u+kGroupLimit);scopes.reserve(kGroupDepthLimit);
    }
    auto owner = [&] { return scopes.empty() ? -1 : scopes.back(); };
    auto add_member = [&](int target,std::string_view text) {
        if (target < 0 || text.empty()) return false;
        auto& entity = diagram.entities[static_cast<std::size_t>(target)];
        if(diagram.family==RelationFamily::Class&&text.starts_with("&lt;&lt;")) {
            if(!entity.annotation.empty())return false;
            const auto annotation=class_encoded_annotation(text,work);if(!annotation)return false;
            entity.annotation=*annotation;return true;
        }
        if (text.starts_with("<<") && text.ends_with(">>")) {
            if (diagram.family != RelationFamily::Class || !entity.annotation.empty() || !unicode_text(text) || text.size() > kLabelLimit) return false;
            const auto annotation=relation_label(text.substr(2,text.size()-4));
            if (!annotation || annotation->find('\n')!=std::string::npos) return false;
            entity.annotation = text; return true;
        }
        if(diagram.family==RelationFamily::Class){
            if(!work.spend(text.size()*4u+sizeof(Member)+64u)||text.find_first_of("{}")!=std::string_view::npos)return false;
            auto displayed=class_member_display(text,work);if(!displayed||entity.members.size()>=128)return false;
            // Pay the owning Class vector mutation for every member, including
            // a plain member that follows generic/classified declarations.
            if(!work.spend(4u*sizeof(std::size_t)+64u))return false;
            const auto member_size=entity.members.size(),member_capacity=entity.members.capacity();
            const auto new_slots=member_size==member_capacity?std::max(2u*member_capacity,member_size+1u):0u;
            const auto relocated=member_size==member_capacity?member_size:0u;
            if(!work.spend((new_slots+relocated+1u)*(sizeof(Member)+4u)))return false;
            const bool method=displayed->method;
            // UML has ordered attribute and method compartments, independent of
            // the interleaving of their source declarations. Preserve each order.
            if(!method&&!entity.members.empty()&&entity.members.back().method){
                if(!work.spend((entity.members.size()*2u+1u)*(sizeof(Member)+4u)))return false;
                const auto first_method=std::ranges::find_if(entity.members,[](const Member& item){return item.method;});
                entity.members.insert(first_method,std::move(*displayed));
            }else entity.members.push_back(std::move(*displayed));
            return true;
        }
        const auto label = relation_label(text); if (!label || label->find('\n') != std::string::npos || entity.members.size() >= 128) return false;
        const bool method = text.find('(') != std::string_view::npos;
        if (method && (text.find(')') == std::string_view::npos || diagram.family != RelationFamily::Class)) return false;
        // Generic type tildes and package-visibility prefixes remain literal
        // member text. Static/abstract decoration still needs its own profile.
        if (text.find_first_of("$*") != std::string_view::npos) return false;
        entity.members.push_back({*label,method}); return true;
    };
    for (std::size_t n = 1; n < lines.size(); ++n) {
        auto line = lines[n]; if (!work.spend(line.size()+1)) return {};
        if (member >= 0) {
            if (line == "}") { member = -1; continue; }
            if (!add_member(member,line)) return {};
            continue;
        }
        // Pinned namespace classStatements owns direct class declarations and
        // nested namespaces, not general relation/member statements. Members
        // inside an actual class body were consumed by the branch above.
        if(class_flat_namespaces&&owner()>=0&&line!="}"&&
           !line.starts_with("namespace ")&&!line.starts_with("class "))return {};
        if (line == "}") { if (scopes.empty()) return {}; scopes.pop_back(); continue; }
        if(diagram.family==RelationFamily::Class&&line.size()>8u&&line.starts_with("classDef")&&
           (line[8u]==' '||line[8u]=='\t')){
            if(owner()>=0||!class_inactive_definition(line,original,inactive_class_definitions,
                                                     class_generic_source_checked,work))return {};
            continue;
        }
        if (line.starts_with("direction ")) {
            const auto direction = trim_relation(line.substr(10));
            // Only a complete State direction value is a DirectionStatement;
            // otherwise normal Id/transition parsing owns `direction --> X`.
            // Unknown residue still fails that existing complete grammar.
            if(!scopes.empty()&&diagram.family!=RelationFamily::State)return {};
            if(diagram.family==RelationFamily::State&&!work.spend(direction.size()+16u))return {};
            const bool valid_direction=direction=="TB"||direction=="TD"||direction=="BT"||direction=="LR"||direction=="RL";
            if(diagram.family!=RelationFamily::State||valid_direction){
                if(!valid_direction)return {};
                if(scopes.empty()){diagram.direction=direction;continue;}
                if(diagram.family!=RelationFamily::State||!work.spend(sizeof(StateScopeDirection)*4u+64u))return {};
                const int owner=scopes.back();
                if(owner<0||static_cast<std::size_t>(owner)>=diagram.entities.size()||
                   diagram.entities[static_cast<std::size_t>(owner)].shape!=EntityShape::Group)return {};
                for(const auto& entry:diagram.state_scope_directions){
                    if(!work.spend(sizeof(StateScopeDirection)+8u)||entry.owner==owner)return {};
                }
                if(diagram.state_scope_directions.size()>=kGroupLimit)return {};
                if(diagram.state_scope_directions.empty()){
                    // First reserve has no old elements to relocate. Later
                    // inserts fit this already paid existing64-group bound.
                    if(!work.spend(kGroupLimit*sizeof(StateScopeDirection)+alignof(StateScopeDirection)+3u*sizeof(std::vector<StateScopeDirection>)+64u))return {};
                    diagram.state_scope_directions.reserve(kGroupLimit);
                }
                diagram.state_scope_directions.push_back({owner,{{direction[0],direction[1]}}});continue;
            }
        }
        if(diagram.family==RelationFamily::Class&&line.size()>4u&&line.starts_with("note")&&(line[4]==' '||line[4]=='\t')) {
            if(!work.spend(line.size()*(8u+4u*sizeof(std::string_view))+4u*sizeof(PendingClassNote)+4u*sizeof(std::string)+256u))return {};
            auto rest=trim_relation(line.substr(4));std::string target;
            if(rest.size()>3u&&rest.starts_with("for")&&(rest[3]==' '||rest[3]=='\t')){
                if(!line.starts_with("note for"))return {};
                rest=trim_relation(rest.substr(3));auto id=class_identifier(rest,work);if(!id)return {};target=std::move(*id);
            }
            if(rest.size()<2u||rest.front()!='"')return {};
            const auto close=rest.find('"',1u);
            if(close==std::string_view::npos||!trim_relation(rest.substr(close+1u)).empty())return {};
            auto text=class_note_text(rest.substr(1u,close-1u),original,class_generic_source_checked,work);if(!text)return {};
            if(class_note_targets.size()>=64u||diagram.entities.size()>=64u+kGroupLimit)return {};
            if(class_note_targets.empty()){
                if(!work.spend(64u*sizeof(PendingClassNote)+alignof(PendingClassNote)+64u))return {};
                class_note_targets.reserve(64u);
                // Allocate the existing finite entity bound before Note insertion;
                // subsequent Class declarations can move no borrowed Note object.
                if(diagram.entities.capacity()<64u+kGroupLimit){
                    if(!work.spend(((64u+kGroupLimit)+2u*diagram.entities.size())*sizeof(RelationEntity)+alignof(RelationEntity)+64u))return {};
                    diagram.entities.reserve(64u+kGroupLimit);
                }
            }
            if(!work.spend(3u*sizeof(RelationEntity)+3u*sizeof(PendingClassNote)+target.size()*2u+text->size()*2u+256u))return {};
            std::string id(1u,'\0');id+="note";id+=std::to_string(class_note_targets.size());
            const int index=static_cast<int>(diagram.entities.size());
            diagram.entities.push_back({std::move(id),std::move(*text),{},EntityShape::Note,owner(),{},true});
            class_note_targets.push_back({index,std::move(target)});diagram.class_notes=true;continue;
        }
        if (diagram.family == RelationFamily::State && line.starts_with("note ")) {
            line = trim_relation(line.substr(5)); bool right;
            if (line.starts_with("right of ")) { right = true; line = trim_relation(line.substr(9)); }
            else if (line.starts_with("left of ")) { right = false; line = trim_relation(line.substr(8)); }
            else return {};
            if(!work.spend(line.size()+4u*sizeof(std::string_view)+32u))return {};
            const bool block=line.find(':')==std::string_view::npos;
            if(block||state_block_note_seen){
                // Header ID and newly reachable inline tails are owned before
                // allocation. Old all-inline decoding expressions stay literal.
                if(!work.spend(line.size()*(16u+4u*sizeof(std::string)+4u*sizeof(std::string_view))+256u))return {};
                if(block&&!state_source_lines(original,state_source_lines_checked,work))return {};
                if(state_block_note_seen&&!block&&!work.spend(line.size()*(line.size()+1u)))return {};
                for(const auto& entity:diagram.entities)if(!work.spend(entity.id.size()+line.size()+32u))return {};
            }
            const auto id = identifier(line);
            if(block){
                if(!id||!line.empty()||diagram.notes.size()>=64u)return {};
                const int target=diagram.find(*id);if(target<0)return {};
                // State entries are unchanged string_views into original, not
                // Class logical-line copies; use their physical byte position.
                const auto header=static_cast<std::size_t>(lines[n].data()-original.data());
                if(header>original.size()||lines[n].size()>original.size()-header||
                   !work.spend(lines[n].size()+16u)||original.substr(header,lines[n].size())!=lines[n])return {};
                auto parsed=state_note_block(original,header,work);if(!parsed)return {};
                if(diagram.notes.capacity()<64u){
                    if(!work.spend((64u+2u*diagram.notes.size()+1u)*sizeof(RelationNote)+alignof(RelationNote)+4u*sizeof(std::string)+128u))return {};
                    diagram.notes.reserve(64u);
                }
                if(!work.spend(parsed->text.size()*2u+4u*sizeof(RelationNote)+4u*sizeof(std::string)+128u))return {};
                diagram.notes.push_back({target,right,std::move(parsed->text)});state_block_note_seen=true;
                while(n+1u<lines.size()){
                    if(!work.spend(4u*sizeof(std::size_t)+16u))return {};
                    const auto next=static_cast<std::size_t>(lines[n+1u].data()-original.data());
                    if(next>=parsed->after)break;
                    ++n;
                }
                continue;
            }
            if (!id || line.empty() || line.front() != ':') return {};
            const int target = diagram.find(*id); const auto text = relation_label(line.substr(1));
            if (target < 0 || !text || diagram.notes.size() >= 64) return {};
            diagram.notes.push_back({target,right,*text}); continue;
        }

        if(diagram.family==RelationFamily::Class&&line.starts_with("namespace ")) {
            line=trim_relation(line.substr(10));bool quoted=false;const auto raw=class_identifier(line,work,&quoted);if(!raw||!line.ends_with("{"))return {};
            line=trim_relation(line.substr(0,line.size()-1));std::optional<std::string> alias;
            if(!line.empty()){
                // ClassLabel owns exactly one STR. Its first closing quote
                // must be the outer quote; neither extra aliases nor residue
                // can be decoded as a single label in any Class mode.
                if(!work.spend(line.size()*2u+4u*sizeof(std::string_view)+4u*sizeof(std::size_t)+64u)||
                   line.size()<4u||!line.starts_with("[\"")||!line.ends_with("\"]")||
                   line.find('"',2u)!=line.size()-2u)return {};
                // False discards the visible alias, but still owns its full
                // existing decoder and quadratic delimiter-search fee.
                if(class_flat_namespaces){
                    if(!work.spend(4u*sizeof(std::size_t)+32u)||line.size()>work.remaining/(line.size()+1u)||
                       !work.spend(line.size()*(line.size()+1u)+(line.size()+1u)*(16u+4u*sizeof(std::string)+4u*sizeof(std::string_view))+4u*sizeof(std::string)+128u))return {};
                }
                alias=relation_label(line.substr(1,line.size()-2));if(!alias||alias->find('\n')!=std::string::npos)return {};
            }
            if(class_flat_namespaces){
                if(scopes.size()>=kGroupDepthLimit||n+1u>=lines.size()||lines[n+1u]=="}")return {};
                const int lexical_parent=owner();
                const std::string_view prefix=lexical_parent<0?std::string_view{}:
                    std::string_view(diagram.entities[static_cast<std::size_t>(lexical_parent)].id);
                if(!work.spend((prefix.size()+raw->size())*4u+16u*sizeof(std::string_view)+
                    8u*sizeof(std::size_t)+4u*sizeof(std::string)+128u))return {};
                std::size_t depth=1u;
                for(const char c:prefix)if(c=='.')++depth;
                if(!prefix.empty())++depth;
                for(std::size_t begin=0;;){
                    const auto end=raw->find('.',begin);const auto part=std::string_view(*raw).substr(begin,end==std::string::npos?end:end-begin);
                    if(part.empty()||trim_relation(part)!=part||part.find_first_of("\r\n\t")!=std::string_view::npos)return {};
                    if(end==std::string::npos)break;
                    ++depth;begin=end+1u;
                }
                if(depth>kGroupDepthLimit)return {};
                const auto bytes=prefix.size()+(!prefix.empty()?1u:0u)+raw->size();
                if(bytes>kSourceLimit||!work.spend((bytes+1u)*4u+4u*sizeof(std::string)+64u))return {};
                std::string qualified;qualified.reserve(bytes);qualified.append(prefix);if(!prefix.empty())qualified.push_back('.');qualified.append(*raw);
                if(!work.spend(4u*sizeof(std::size_t)+32u)||bytes>work.remaining/(bytes+1u)||
                   !work.spend(bytes*(bytes+1u)+(bytes+1u)*(16u+4u*sizeof(std::string)+4u*sizeof(std::string_view))+4u*sizeof(std::string)+128u))return {};
                const auto visible=relation_label(qualified);
                if(!visible||*visible!=qualified||visible->find('\n')!=std::string::npos)return {};
                const auto before=diagram.entities.size();
                const int next=diagram.class_namespace(qualified,*visible,-1,work,true);if(next<0)return {};
                if(diagram.entities.size()!=before&&++groups>kGroupLimit)return {};
                if(!work.spend(4u*sizeof(int)+32u))return {};
                scopes.push_back(next);continue;
            }
            int parent=owner();std::string_view remaining=*raw;std::string prefix=parent<0?std::string{}:diagram.entities[static_cast<std::size_t>(parent)].id;
            if(!work.spend(raw->size()*4+prefix.size()+1))return {};
            for(;;){const auto dot=quoted?std::string_view::npos:remaining.find('.');const auto part=remaining.substr(0,dot);if(part.empty())return {};
                if(!work.spend(prefix.size()+part.size()+1))return {};
                if(!prefix.empty())prefix+='.';prefix+=part;
                const bool final=dot==std::string_view::npos;const auto label=final&&alias?alias:relation_label(part);if(!label)return {};
                const auto old_size=diagram.entities.size();const int next=diagram.class_namespace(prefix,*label,parent,work,final);if(next<0)return {};
                if(diagram.entities.size()!=old_size&&++groups>kGroupLimit)return {};
                parent=next;if(final)break;remaining.remove_prefix(dot+1);
            }
            std::size_t depth=0;for(int at=parent;at>=0;at=diagram.entities[static_cast<std::size_t>(at)].owner){if(!work.spend()||++depth>kGroupDepthLimit)return {};}
            scopes.push_back(parent);continue;
        }
        if(diagram.family==RelationFamily::Class&&line.starts_with("&lt;&lt;")){
            if(!work.spend(line.size()*8u+128u))return {};
            const auto end=line.find("&gt;&gt;",8u);if(end==std::string_view::npos||end==8u)return {};
            const auto token=line.substr(8u,end-8u);for(const auto c:token){const auto u=static_cast<unsigned char>(c);if(!((u>='a'&&u<='z')||(u>='A'&&u<='Z')||(u>='0'&&u<='9')||u=='_'||u=='-'||u>=0x80))return {};}
            const auto tail=line.substr(end+8u);if(tail.find('\r')!=std::string_view::npos)return {};
            auto rest=trim_relation(tail);const auto operand=class_operand(rest,work,original,class_generic_source_checked);if(!operand||!rest.empty())return {};
            const int target=diagram.class_entity(operand->id,owner(),work,false,operand->type);if(target<0||!add_member(target,line.substr(0,end+8u)))return {};continue;
        }
        if(diagram.family==RelationFamily::Class&&line.starts_with("<<")){
            if(!work.spend(line.size()*8u+128u))return {};
            const auto end=line.find(">>",2);if(end==std::string_view::npos||end==2)return {};
            const auto token=line.substr(2,end-2);for(const auto c:token){const auto u=static_cast<unsigned char>(c);if(!((u>='a'&&u<='z')||(u>='A'&&u<='Z')||(u>='0'&&u<='9')||u=='_'||u=='-'||u>=0x80))return {};}
            auto rest=trim_relation(line.substr(end+2));const auto operand=class_operand(rest,work,original,class_generic_source_checked);if(!operand||!rest.empty())return {};
            const int target=diagram.class_entity(operand->id,owner(),work,false,operand->type);if(target<0||!add_member(target,line.substr(0,end+2)))return {};continue;
        }
        if (diagram.family == RelationFamily::Class && line.starts_with("class ")) {
            line = trim_relation(line.substr(6)); const auto operand=class_operand(line,work,original,class_generic_source_checked);if(!operand)return {};
            const int target=diagram.class_entity(operand->id,owner(),work,true,operand->type);if(target<0)return {};
            auto& entity=diagram.entities[static_cast<std::size_t>(target)];
            if(!work.spend(line.size()*2u+8u*sizeof(bool)+32u))return {};
            if(entity.declared){
                if(entity.owner!=owner())return {};
                if(!operand->type&&line=="{"){member=target;continue;}
                if(!line.empty()&&line!="{}")return {};
                continue;
            }
            entity.declared=true;
            if(line.starts_with("[\"")){
                const auto typed=diagram.class_typed(target,work);
                if(!typed||*typed||operand->type)return {};
                if(!work.spend(line.size()*4u+128u))return {};
                const auto end=line.find('"',2);if(end==std::string_view::npos||end+1>=line.size()||line[end+1]!=']')return {};
                const auto label=class_display_label(line.substr(2,end-2),work);if(!label)return {};
                entity.label=*label;line=trim_relation(line.substr(end+2));
            }
            if (line == "{") { member = target; continue; }
            if (line.empty()||line=="{}") continue;
            return {};
        }
        if (diagram.family == RelationFamily::State && line.starts_with("state ")) {
            // The initial identifier allocation precedes all alias/suffix probes.
            if(!work.spend(line.size()*(16u+4u*sizeof(std::string)+4u*sizeof(std::string_view))+8u*sizeof(std::optional<std::string>)+256u))return {};
            line = trim_relation(line.substr(6)); auto id = identifier(line,true); if (!id) return {};
            std::optional<std::string> alias_label,suffix;
            const bool alias=id->front()=='"'&&line.size()>=2&&er_ascii_equal(line.substr(0,2),"as")&&
                (line.size()==2||line[2]==' '||line[2]=='\t');
            if(alias) {
                // Preserve the existing alias allocation/lookup/growth tariff.
                const auto bytes=id->size()+line.size();
                if(!work.spend(bytes*(16u+4u*sizeof(std::string)+4u*sizeof(std::string_view))+
                    (diagram.entities.size()*3u+1u)*sizeof(RelationEntity)+
                    diagram.entities.size()*(line.size()+1u)+256u))return {};
                auto remaining=trim_relation(line.substr(2));const auto selected=identifier(remaining);
                if(!selected||(!remaining.empty()&&remaining!="{"&&remaining.front()!=':'))return {};
                const auto raw=std::string_view(*id).substr(1,id->size()-2);
                if(remaining.starts_with(":")){
                    const auto title=trim_relation(raw);
                    if(title.size()>256u||!work.spend(title.size()*(title.size()+1u)))return {};
                }
                alias_label=relation_label(trim_relation(raw));if(!alias_label)return {};
                id=*selected;line=remaining;
                if(line.starts_with(":")){
                    if(!state_source_lines(original,state_source_lines_checked,work))return {};
                    suffix=state_description_label(line.substr(1),work);if(!suffix)return {};
                    line={};
                }
            }else if (id->front() == '"') *id = id->substr(1,id->size()-2);
            const int target = diagram.entity(*id,owner()); if (target < 0) return {};
            auto& entity = diagram.entities[static_cast<std::size_t>(target)];
            if (entity.declared) return {};
            const auto previous=diagram.state_text_index(target,work);if(!previous)return {};
            if(*previous<diagram.state_texts.size()&&!line.empty())return {};
            entity.declared = true;
            if(alias_label){
                if(line=="{")entity.label=std::move(*alias_label);
                else if(!diagram.state_fragment(target,std::move(*alias_label),work))return {};
            }
            if(suffix&&!diagram.state_fragment(target,std::move(*suffix),work))return {};
            if (line == "{") {
                if (++groups > kGroupLimit || scopes.size() >= kGroupDepthLimit) return {};
                entity.shape = EntityShape::Group; scopes.push_back(target); continue;
            }
            if (line == "<<choice>>") entity.shape = EntityShape::Choice;
            else if (line == "<<fork>>") { entity.shape = EntityShape::Fork; entity.label.clear(); }
            else if (line == "<<join>>") { entity.shape = EntityShape::Join; entity.label.clear(); }
            else if (!line.empty()) return {};
            continue;
        }
        if(!work.spend(8u*sizeof(std::optional<std::string_view>)+64u))return {};
        if(diagram.family==RelationFamily::State&&!work.spend(line.size()*(16u+4u*sizeof(std::string)+4u*sizeof(std::string_view))+256u))return {};
        auto rest = line;
        std::optional<std::string> from;
        std::optional<std::string_view> from_type;
        const bool from_control=diagram.family == RelationFamily::State && rest.starts_with("[*]");
        if (from_control) {
            from = std::string{}; rest = trim_relation(rest.substr(3));
        } else if(diagram.family==RelationFamily::Class){
            auto operand=class_operand(rest,work,original,class_generic_source_checked);if(!operand)return {};
            from=std::move(operand->id);from_type=operand->type;
        } else from=identifier(rest);
        if (!from) return {};
        if(diagram.family==RelationFamily::State&&!from_control&&rest.starts_with(":")){
            if(!state_source_lines(original,state_source_lines_checked,work))return {};
            auto text=state_description_label(rest.substr(1),work);if(!text)return {};
            if(!work.spend((diagram.entities.size()*3u+1u)*sizeof(RelationEntity)+from->size()*4u+4u*sizeof(std::string)+256u))return {};
            for(const auto& existing:diagram.entities)
                if(!work.spend(existing.id.size()+from->size()+sizeof(std::string)+8u))return {};
            const int target=diagram.entity(*from,owner());if(target<0||!diagram.state_fragment(target,std::move(*text),work))return {};
            continue;
        }
        if(diagram.family==RelationFamily::State&&!from_control&&rest.empty()) {
            // Follow the owning State lexer tokens, not a general reserved-ID
            // table: bare direction/as/width remain ordinary IDs here.
            for(const auto keyword:{"note","click","href","classDef","class","style","state","scale"}) {
                if(!work.spend(from->size()+1u))return {};
                if(er_ascii_equal(*from,keyword))return {};
            }
            if(scopes.empty()){
                if(!work.spend(from->size()+1u))return {};
                if(er_ascii_equal(*from,"default"))return {};
            }
            if(!work.spend(from->size()*(16u+4u*sizeof(std::string)+4u*sizeof(std::string_view))+
                (diagram.entities.size()*3u+1u)*sizeof(RelationEntity)+
                diagram.entities.size()*(from->size()+1u)+256u))return {};
            if(diagram.entity(*from,owner())<0)return {};
            continue; // A declaration never allocates a Relationship payload.
        }
        const int a = from_control?diagram.control(EntityShape::Start,owner(),work):diagram.family==RelationFamily::Class?diagram.class_entity(*from,owner(),work,false,from_type):diagram.entity(*from,owner()); if (a < 0) return {};
        if (diagram.family == RelationFamily::Class && rest.starts_with(":")) {
            // STYLE_SEPARATOR is source syntax, not two leading member colons.
            if(!work.spend(8u)||rest.starts_with(":::"))return {};
            if (!add_member(a,trim_relation(rest.substr(1)))) return {};
            continue;
        }
        if(!work.spend(4u*sizeof(std::unique_ptr<ClassEndpointTexts>)))return {};
        if(!work.spend(4u*kRelationshipKindStorage+16u))return {};
        Relationship edge; edge.from = a;
        if(diagram.family==RelationFamily::Class&&rest.starts_with("\"")){if(!class_endpoint_text(rest,edge,true,work))return {};}
        if(diagram.family==RelationFamily::Class){
            edge.start=Marker::None;edge.end=Marker::None;
            if(!work.spend(rest.size()*2u+128u))return {};
            if(rest.starts_with("<|")){edge.start=Marker::Triangle;rest.remove_prefix(2);}
            else if(rest.starts_with("<")){edge.start=Marker::Arrow;rest.remove_prefix(1);}
            else if(rest.starts_with("*")){edge.start=Marker::FilledDiamond;rest.remove_prefix(1);}
            else if(rest.starts_with("o")){edge.start=Marker::OpenDiamond;rest.remove_prefix(1);}
            if(rest.starts_with("--"))rest.remove_prefix(2);
            else if(rest.starts_with("..")){edge.dotted=true;rest.remove_prefix(2);}
            else return {};
            // Suffix markers are adjacent to the line. Do not trim and steal
            // the first 'o' of a following Class name such as 'other'.
            if(rest.starts_with("|>")){edge.end=Marker::Triangle;rest.remove_prefix(2);}
            else if(rest.starts_with(">")){edge.end=Marker::Arrow;rest.remove_prefix(1);}
            else if(rest.starts_with("*")){edge.end=Marker::FilledDiamond;rest.remove_prefix(1);}
            else if(rest.starts_with("o")){edge.end=Marker::OpenDiamond;rest.remove_prefix(1);}
            rest=trim_relation(rest);
        }else{
            struct Operator { std::string_view text; Marker start,end; bool dotted; };
            constexpr Operator operators[]{
                {"<|--",Marker::Triangle,Marker::None,false},{"<|..",Marker::Triangle,Marker::None,true},
                {"..|>",Marker::None,Marker::Triangle,true},{"--|>",Marker::None,Marker::Triangle,false},
                {"*--",Marker::FilledDiamond,Marker::None,false},{"o--",Marker::OpenDiamond,Marker::None,false},
                {"--*",Marker::None,Marker::FilledDiamond,false},{"--o",Marker::None,Marker::OpenDiamond,false},
                {"-->",Marker::None,Marker::Arrow,false},{"..>",Marker::None,Marker::Arrow,true},
                {"<--",Marker::Arrow,Marker::None,false},{"<..",Marker::Arrow,Marker::None,true},
                {"--",Marker::None,Marker::None,false},{"..",Marker::None,Marker::None,true}};
            bool found = false;
            for (const auto& op : operators) if (rest.starts_with(op.text)) {
                if (diagram.family == RelationFamily::State && op.text != "-->") return {};
                edge.start = op.start; edge.end = op.end; edge.dotted = op.dotted;
                rest = trim_relation(rest.substr(op.text.size())); found = true; break;
            }
            if (!found) return {};
        }
        if(diagram.family==RelationFamily::Class&&rest.starts_with("\"")){if(!class_endpoint_text(rest,edge,false,work))return {};}
        std::optional<std::string> to;
        std::optional<std::string_view> to_type;
        const bool to_control=diagram.family == RelationFamily::State && rest.starts_with("[*]");
        if (to_control) {
            to = std::string{}; rest = trim_relation(rest.substr(3));
        } else if(diagram.family==RelationFamily::Class){
            auto operand=class_operand(rest,work,original,class_generic_source_checked);if(!operand)return {};
            to=std::move(operand->id);to_type=operand->type;
        } else to=identifier(rest);
        if (!to) return {};
        edge.to = to_control?diagram.control(EntityShape::Final,owner(),work):diagram.family==RelationFamily::Class?diagram.class_entity(*to,owner(),work,false,to_type):diagram.entity(*to,owner()); if (edge.to < 0) return {};
        if (!rest.empty()) {
            if (rest.front() != ':') return {};
            const auto label = diagram.family==RelationFamily::Class?class_display_label(trim_relation(rest.substr(1)),work,&original):relation_label(rest.substr(1)); if (!label) return {}; edge.label = *label;
        }
        if (diagram.edges.size() >= 128) return {};
        if(!append_relationship(diagram.edges,std::move(edge),work))return {};
    }
    if(state_block_note_seen){
        if(!work.spend(4u*sizeof(std::size_t)+32u))return {};
        for(std::size_t a=0;a<diagram.notes.size();++a)for(std::size_t b=a+1u;b<diagram.notes.size();++b){
            if(!work.spend(2u*sizeof(int)+4u)||diagram.notes[a].target==diagram.notes[b].target)return {};
        }
    }
    // ClassDB resolves a Note attachment against the complete Class map. It
    // neither creates an implicit target nor substitutes the namespace owner.
    for(const auto& note:class_note_targets){
        if(!work.spend(sizeof(PendingClassNote)+note.target.size()+32u))return {};
        if(note.target.empty())continue;
        const int target=diagram.class_find(note.target,false,work);if(target<0||work.remaining==0)return {};
        if(!work.spend(4u*sizeof(Relationship)+4u*kRelationshipKindStorage+128u))return {};
        Relationship leader;leader.from=note.entity;leader.to=target;leader.start=Marker::None;leader.end=Marker::None;
        leader.dotted=true;leader.kind=RelationshipKind::ClassNoteLeader;
        if(diagram.edges.size()==diagram.edges.capacity()){
            const auto capacity=std::min(std::size_t{128},std::max(std::size_t{1},diagram.edges.size()*2u));
            if(!work.spend((capacity+2u*diagram.edges.size()+1u)*sizeof(Relationship)+alignof(Relationship)+32u))return {};
        }
        if(!append_relationship(diagram.edges,std::move(leader),work))return {};
    }
    if (member >= 0 || !scopes.empty() || diagram.entities.empty()) return {};
    const auto ordinary = std::ranges::count_if(diagram.entities,[](const RelationEntity& e) { return e.shape != EntityShape::Group; });
    if (ordinary > 64 || (diagram.family==RelationFamily::Class&&ordinary==0)) return {};
    // A declaration may promote an earlier implicit state to a composite. Its
    // original relationship identity continues to address the real perimeter.
    for (std::size_t n = 0; n < diagram.entities.size(); ++n) {
        const auto& entity = diagram.entities[n];
        if (entity.owner >= 0 && entity.owner == static_cast<int>(n)) return {};
        if(!work.spend(sizeof(EntityShape)+1u))return {};
        std::size_t aggregate = entity.shape==EntityShape::Note?4u:0u;
        for (const auto row : label_rows(entity.label)) aggregate += ftxui::Utf8ToGlyphs(std::string(row)).size();
        aggregate += ftxui::Utf8ToGlyphs(entity.annotation).size();
        if(diagram.family==RelationFamily::State&&!diagram.state_texts.empty()){
            const auto record=diagram.state_text_index(static_cast<int>(n),work);if(!record)return {};
            if(*record<diagram.state_texts.size())for(const auto& row:diagram.state_texts[*record].body){
                if(!work.spend(row.size()*(4u*sizeof(std::string)+16u)+128u))return {};
                const auto cells=ftxui::Utf8ToGlyphs(row).size();
                if(aggregate>kLabelLimit||cells>kLabelLimit-aggregate)return {};
                aggregate+=cells;
            }
        }
        if (aggregate > kLabelLimit) return {};
    }
    return diagram;
}

struct RelationBox { int x=0,y=0,w=0,h=0; int right() const { return x+w-1; } int bottom() const { return y+h-1; } };
struct EndpointPort { Point border,normal,route; Marker marker = Marker::None; };
bool er_cardinality_marker(Marker marker) {
    return marker==Marker::One||marker==Marker::ZeroOne||marker==Marker::ZeroMany||marker==Marker::OneMany;
}
int marker_size(Marker marker) { return marker == Marker::None ? 0 : er_cardinality_marker(marker) ? 2 : 1; }
std::string_view marker_glyph(Marker marker,Point inward) {
    if (marker == Marker::Arrow) return arrow(inward);
    if (marker == Marker::Triangle || marker == Marker::RequirementOpenArrow)
        return inward.x > 0 ? "▷" : inward.x < 0 ? "◁" : inward.y > 0 ? "▽" : "△";
    if (marker == Marker::RequirementCirclePlus) return "⊕";
    if (marker == Marker::FilledDiamond) return "◆";
    return "◇";
}
struct RelationLayout {
    const RelationDiagram& diagram; Work& work; std::vector<RelationBox> boxes;
    std::vector<std::vector<int>> children; std::vector<std::vector<int>> scope_edges;
    std::vector<std::vector<int>> ordered; std::vector<int> degrees;
    bool class_endpoint_active=false;
    bool compound_scope=false;
    std::unique_ptr<std::array<int,64>> class_band_rows;
    explicit RelationLayout(const RelationDiagram& d,Work& budget):diagram(d),work(budget),boxes(d.entities.size()),children(d.entities.size()+1),scope_edges(d.entities.size()+1),ordered(d.entities.size()+1),degrees(d.entities.size()),compound_scope(d.class_namespaces) {}

    int common_scope(int from,int to) {
        if(diagram.family==RelationFamily::Er&&compound_scope&&!work.spend(
            2u*sizeof(std::array<bool,64+kGroupLimit>)+8u*sizeof(int)+64u))return -2;
        if(!work.spend(64+kGroupLimit))return -2;
        std::array<bool,64+kGroupLimit> ancestors{};
        for(int at=diagram.entities[static_cast<std::size_t>(from)].owner;at>=0;at=diagram.entities[static_cast<std::size_t>(at)].owner){if(!work.spend()||static_cast<std::size_t>(at)>=diagram.entities.size()||static_cast<std::size_t>(at)>=ancestors.size())return -2;ancestors[static_cast<std::size_t>(at)]=true;}
        for(int at=diagram.entities[static_cast<std::size_t>(to)].owner;at>=0;at=diagram.entities[static_cast<std::size_t>(at)].owner){if(!work.spend()||static_cast<std::size_t>(at)>=diagram.entities.size()||static_cast<std::size_t>(at)>=ancestors.size())return -2;if(ancestors[static_cast<std::size_t>(at)])return at;}
        return -1;
    }
    int scope_child(int endpoint,int owner) {
        int at=endpoint;
        for(std::size_t depth=0;depth<=kGroupDepthLimit;++depth){if(!work.spend()||at<0||static_cast<std::size_t>(at)>=diagram.entities.size())return -1;const int parent=diagram.entities[static_cast<std::size_t>(at)].owner;if(parent==owner)return at;at=parent;}
        return -1;
    }
    bool prepare() {
        if(diagram.family==RelationFamily::Er){
            if(!work.spend(4u*sizeof(bool)+16u))return false;
            for(const auto& entity:diagram.entities){
                if(!work.spend(sizeof(EntityShape)+8u))return false;
                if(entity.shape==EntityShape::Group){compound_scope=true;break;}
            }
        }
        // Same-owner State scenes retain their existing scope projection.
        // Cross-owner edges use the existing LCA and structural transit rules.
        if(diagram.family==RelationFamily::State) {
            if(!work.spend(8u*sizeof(int)+16u))return false;
            for(const auto& edge:diagram.edges) {
                if(!work.spend(8u*sizeof(int)+8u)||edge.from<0||edge.to<0||
                   static_cast<std::size_t>(edge.from)>=diagram.entities.size()||
                   static_cast<std::size_t>(edge.to)>=diagram.entities.size())return false;
                if(diagram.entities[static_cast<std::size_t>(edge.from)].owner!=
                   diagram.entities[static_cast<std::size_t>(edge.to)].owner){compound_scope=true;break;}
            }
        }
        for (std::size_t n=0;n<diagram.entities.size();++n) children[static_cast<std::size_t>(diagram.entities[n].owner+1)].push_back(static_cast<int>(n));
        for (std::size_t n=0;n<diagram.edges.size();++n) {
            const auto& e=diagram.edges[n];
            if(!work.spend(4u*sizeof(RelationshipKind)+8u))return false;
            if(e.kind==RelationshipKind::ClassNoteLeader){
                if(diagram.family!=RelationFamily::Class||!diagram.class_notes||e.from<0||e.to<0||
                   static_cast<std::size_t>(e.from)>=diagram.entities.size()||static_cast<std::size_t>(e.to)>=diagram.entities.size()||
                   diagram.entities[static_cast<std::size_t>(e.from)].shape!=EntityShape::Note||
                   diagram.entities[static_cast<std::size_t>(e.to)].shape!=EntityShape::Box||
                   e.start!=Marker::None||e.end!=Marker::None||!e.dotted||!e.label.empty()||e.endpoint_texts)return false;
            }
            if(e.endpoint_texts){if(!work.spend())return false;class_endpoint_active=true;}
            const int owner=compound_scope?common_scope(e.from,e.to):diagram.entities[static_cast<std::size_t>(e.from)].owner;
            if(owner< -1)return false;
            if (!compound_scope&&owner!=diagram.entities[static_cast<std::size_t>(e.to)].owner) return false;
            scope_edges[static_cast<std::size_t>(owner+1)].push_back(static_cast<int>(n)); ++degrees[static_cast<std::size_t>(e.from)]; ++degrees[static_cast<std::size_t>(e.to)];
        }
        // Eligibility only: keep the existing reciprocal planner/router. Raw
        // endpoints matter here; inheritance-normalized adjacency is different.
        if (diagram.family==RelationFamily::Class&&!diagram.class_namespaces&&
            (diagram.direction=="TB"||diagram.direction=="BT")&&diagram.notes.empty()&&
            diagram.entities.size()>=3&&diagram.entities.size()<=64&&
            diagram.edges.size()>=6&&diagram.edges.size()<=128) {
            class_reciprocal=true;
            for (std::size_t n=0;n<diagram.entities.size();++n) {
                if (!work.spend()) return false;
                const auto& entity=diagram.entities[n];
                if (entity.owner!=-1||entity.shape!=EntityShape::Box||entity.er_alias||
                    !entity.er_attributes.empty()||!entity.members.empty()||!entity.annotation.empty()||degrees[n]==0) {
                    class_reciprocal=false;break;
                }
            }
            if (class_reciprocal) for (const auto& edge:diagram.edges) {
                if (!work.spend()) return false;
                if (edge.from==edge.to) {class_reciprocal=false;break;}
                std::size_t same=0,reverse=0;
                for (const auto& other:diagram.edges) {
                    if (!work.spend()) return false;
                    same+=other.from==edge.from&&other.to==edge.to;
                    reverse+=other.from==edge.to&&other.to==edge.from;
                }
                if (same!=1||reverse!=1) {class_reciprocal=false;break;}
            }
        }
        if(class_reciprocal&&class_endpoint_active) {
            if(!work.spend(2u*sizeof(std::array<int,64>)+64u+16u))return false;
            class_band_rows=std::make_unique<std::array<int,64>>();
            for(std::size_t n=0;n<diagram.entities.size();++n)(*class_band_rows)[n]=degrees[n];
            for(const auto& edge:diagram.edges) {
                if(!work.spend(2))return false;
                for(const bool source:{true,false})if(const auto* caption=class_caption(edge,source)) {
                    if(!work.spend(4))return false;
                    const auto index=static_cast<std::size_t>(source?edge.from:edge.to);
                    if(index>=diagram.entities.size()||caption->h<1||caption->h>=kHeightLimit||(*class_band_rows)[index]>kHeightLimit-caption->h-1)return false;
                    (*class_band_rows)[index]+=caption->h+1;
                }
            }
        }
        return measure(-1,0);
    }
    // A leaf DAG can induce a namespace cycle. Such namespaces need disjoint
    // perpendicular bands, with overlapping extents along the flow axis.
    // nullopt is a failed ledger/plan; false leaves the old, ineligible plan.
    std::optional<bool> compound_class_columns(const std::vector<std::vector<bool>>& root_reach) {
        if(!work.spend(16u*sizeof(std::size_t)+16u*sizeof(int)+128u))return {};
        const auto size=diagram.entities.size();
        if(size>64u+kGroupLimit||children[0].size()<2u)return false;
        for(std::size_t n=0;n<size;++n) {
            if(!work.spend(16u))return {};
            const auto& entity=diagram.entities[n];
            if(entity.shape==EntityShape::Group) {
                if(entity.owner!=-1||children[n+1u].empty())return false;
            } else {
                if(entity.shape!=EntityShape::Box||entity.owner<0||static_cast<std::size_t>(entity.owner)>=size||
                    diagram.entities[static_cast<std::size_t>(entity.owner)].shape!=EntityShape::Group)return false;
            }
        }
        bool quotient_cycle=false;
        for(std::size_t a=0;a<children[0].size();++a)for(std::size_t b=a+1u;b<children[0].size();++b) {
            if(!work.spend(4u))return {};
            quotient_cycle|=root_reach[a][b]&&root_reach[b][a];
        }
        if(!quotient_cycle)return false;
        struct Plan {
            std::array<std::array<bool,64>,64> adjacent{};
            std::array<int,64> leaves{},indegree{},rank{},column_width{},column_x{},cursor{};
            std::array<int,64+kGroupLimit> leaf_index{};
            std::array<RelationBox,64+kGroupLimit> boxes{};
        };
        if(!work.spend(2u*sizeof(Plan)+alignof(Plan)+256u))return {};
        Plan plan;plan.leaf_index.fill(-1);std::size_t count=0;
        for(std::size_t n=0;n<size;++n) {
            if(!work.spend(8u+sizeof(RelationBox)))return {};
            plan.boxes[n]=boxes[n];
            if(diagram.entities[n].shape==EntityShape::Group)continue;
            if(count>=64u)return {};
            plan.leaves[count]=static_cast<int>(n);plan.leaf_index[n]=static_cast<int>(count++);
        }
        int gap=10;
        for(const auto& edge:diagram.edges) {
            if(!work.spend(16u+edge.label.size()*(4u*sizeof(std::string_view)+8u)))return {};
            if(edge.from<0||edge.to<0||static_cast<std::size_t>(edge.from)>=size||static_cast<std::size_t>(edge.to)>=size)return {};
            int from=plan.leaf_index[static_cast<std::size_t>(edge.from)],to=plan.leaf_index[static_cast<std::size_t>(edge.to)];
            if(from<0||to<0)return {};
            if(edge.end==Marker::Triangle&&edge.start!=Marker::Triangle)std::swap(from,to);
            if(!plan.adjacent[static_cast<std::size_t>(from)][static_cast<std::size_t>(to)]) {
                plan.adjacent[static_cast<std::size_t>(from)][static_cast<std::size_t>(to)]=true;++plan.indegree[static_cast<std::size_t>(to)];
            }
            gap=std::max(gap,label_width(edge.label)+2*std::max(marker_size(edge.start),marker_size(edge.end))+4);
        }
        int levels=1;
        for(std::size_t step=0;step<count;++step) {
            std::size_t selected=count;
            for(std::size_t n=0;n<count;++n) {
                if(!work.spend(2u))return {};
                if(plan.indegree[n]==0){selected=n;break;}
            }
            // Actual leaf cycles retain the established SCC layout.
            if(selected==count)return false;
            plan.indegree[selected]=-1;levels=std::max(levels,plan.rank[selected]+1);
            for(std::size_t to=0;to<count;++to) {
                if(!work.spend(4u))return {};
                if(plan.adjacent[selected][to]) {
                    --plan.indegree[to];plan.rank[to]=std::max(plan.rank[to],plan.rank[selected]+1);
                }
            }
        }
        for(std::size_t n=0;n<count;++n) {
            if(!work.spend(4u))return {};
            const auto& box=boxes[static_cast<std::size_t>(plan.leaves[n])];
            if(box.w<1||box.w>kWidthLimit||box.h<1||box.h>kHeightLimit)return {};
            auto& width=plan.column_width[static_cast<std::size_t>(plan.rank[n])];width=std::max(width,box.w);
        }
        const int margin=std::max(12,static_cast<int>(diagram.edges.size())*2+6);
        if(margin>kWidthLimit/2||gap>kWidthLimit)return {};
        int x=margin+6;
        for(int rank=0;rank<levels;++rank) {
            if(!work.spend(4u))return {};
            const auto at=static_cast<std::size_t>(rank);plan.column_x[at]=x;
            if(plan.column_width[at]>kWidthLimit-x)return {};
            x+=plan.column_width[at];
            if(rank+1<levels){if(gap>kWidthLimit-x)return {};x+=gap;}
        }
        int y=margin,min_x=kWidthLimit,max_x=-1;
        for(const int group:children[0]) {
            if(!work.spend(64u*sizeof(int)+32u))return {};
            plan.cursor.fill(0);const auto at=static_cast<std::size_t>(group);
            const auto& label=diagram.entities[at].label;
            if(!work.spend(label.size()*(4u*sizeof(std::string_view)+8u)+64u))return {};
            const int header_rows=static_cast<int>(label_rows(label).size());
            if(header_rows>kHeightLimit-4||y>kHeightLimit-header_rows-4)return {};
            const int top=y+header_rows+4;int bottom=top,left=kWidthLimit,right=-1;
            for(std::size_t n=0;n<count;++n) {
                if(!work.spend(16u))return {};
                const auto leaf=static_cast<std::size_t>(plan.leaves[n]);
                if(diagram.entities[leaf].owner!=group)continue;
                const auto rank=static_cast<std::size_t>(plan.rank[n]);auto& box=plan.boxes[leaf];
                if(plan.cursor[rank]>kHeightLimit-top||box.h>kHeightLimit-top-plan.cursor[rank])return {};
                box.x=plan.column_x[rank]+(plan.column_width[rank]-box.w)/2;box.y=top+plan.cursor[rank];
                bottom=std::max(bottom,box.y+box.h);left=std::min(left,box.x);right=std::max(right,box.x+box.w);
                if(box.h>kHeightLimit-plan.cursor[rank])return {};
                plan.cursor[rank]+=box.h;
                if(gap>kHeightLimit-plan.cursor[rank])return {};
                plan.cursor[rank]+=gap;
            }
            if(right<0||bottom>kHeightLimit-6||right-left>kWidthLimit-12)return {};
            auto& box=plan.boxes[at];box.x=left-6;box.y=y;box.w=right-left+12;box.h=bottom-y+6;
            const int title_width=label_width(label)+4;
            if(title_width>kWidthLimit)return {};
            if(title_width>box.w){box.x-=(title_width-box.w)/2;box.w=title_width;}
            min_x=std::min(min_x,box.x);max_x=std::max(max_x,box.x+box.w);
            if(box.h>kHeightLimit-y)return {};
            y+=box.h;
            if(gap>kHeightLimit-y)return {};
            y+=gap;
        }
        const int shift=margin-min_x,width=max_x-min_x+2*margin;
        if(width<1||width>kWidthLimit||y<gap||margin>kHeightLimit-(y-gap))return {};
        const int height=y-gap+margin;
        // Validate the complete candidate before committing any live box.
        for(std::size_t n=0;n<size;++n) {
            if(!work.spend(16u))return {};
            auto& box=plan.boxes[n];box.x+=shift;
            if(diagram.direction=="RL")box.x=width-box.x-box.w;
            if(box.x<0||box.y<0||box.w>width-box.x||box.h>height-box.y)return {};
        }
        for(std::size_t n=0;n<size;++n) {
            if(!work.spend(16u))return {};
            const int owner=diagram.entities[n].owner;if(owner<0)continue;
            const auto& leaf=plan.boxes[n];const auto& group=plan.boxes[static_cast<std::size_t>(owner)];
            if(leaf.x<=group.x||leaf.y<=group.y||leaf.x+leaf.w>=group.x+group.w||leaf.y+leaf.h>=group.y+group.h)return {};
        }
        if(!work.spend(size*(sizeof(RelationBox)+4u)+16u))return {};
        for(std::size_t n=0;n<size;++n)boxes[n]=plan.boxes[n];
        canvas_width=width;canvas_height=height;return true;
    }
    bool measure(int owner,std::size_t depth) {
        // New scalar/flag storage exists even on unchanged family branches.
        if(!work.spend(3u*sizeof(std::string_view)+alignof(std::string_view)+8u*sizeof(bool)))return false;
        const bool state_scoped=!diagram.state_scope_directions.empty();std::string_view scope_direction;
        if(state_scoped){const auto selected=diagram.state_direction(owner,work);if(!selected)return false;scope_direction=*selected;}
        if (depth>kGroupDepthLimit) return false;
        const auto slot=static_cast<std::size_t>(owner+1); const auto& list=children[slot];
        if(list.empty()) {
            if(diagram.family==RelationFamily::Er&&!work.spend(4u*sizeof(bool)+8u))return false;
            if(owner<0||(!diagram.class_namespaces&&!(diagram.family==RelationFamily::Er&&
                diagram.entities[static_cast<std::size_t>(owner)].shape==EntityShape::Group)))return false;
            const auto& e=diagram.entities[static_cast<std::size_t>(owner)];
            if(diagram.family==RelationFamily::Er&&!work.spend(e.label.size()*(24u*sizeof(std::string)+16u*sizeof(std::string_view)+96u)+128u))return false;
            if(!work.spend(e.label.size()*4+1))return false;
            auto& box=boxes[static_cast<std::size_t>(owner)];box.w=std::max(5,label_width(e.label)+4);box.h=std::max(3,static_cast<int>(label_rows(e.label).size())+2);return box.w<=kWidthLimit&&box.h<=kHeightLimit;
        }
        const std::size_t count=list.size();
        if(diagram.family==RelationFamily::Er&&compound_scope){
            if(count>64u||scope_edges[slot].size()>128u||!work.spend(
                count*count*(8u+2u*sizeof(int))+count*count*count*4u+
                count*(8u*sizeof(std::vector<bool>)+16u*sizeof(int)+32u)+
                scope_edges[slot].size()*(count*2u*sizeof(int)+32u)+256u))return false;
        }
        std::vector<std::vector<bool>> reach(count,std::vector<bool>(count));
        std::vector<std::vector<bool>> adjacent(count,std::vector<bool>(count));
        for (const int index:scope_edges[slot]) {
            const auto& e=diagram.edges[static_cast<std::size_t>(index)];
            int from=e.from,to=e.to;
            if(compound_scope){from=scope_child(from,owner);to=scope_child(to,owner);if(from<0||to<0)return false;}
            // UML inheritance ranks parents before children regardless of the
            // declaration's business direction. Endpoint markers stay unchanged.
            if (diagram.family==RelationFamily::Class && e.end==Marker::Triangle && e.start!=Marker::Triangle) std::swap(from,to);
            const auto a=static_cast<std::size_t>(std::ranges::find(list,from)-list.begin());
            const auto b=static_cast<std::size_t>(std::ranges::find(list,to)-list.begin());
            if(compound_scope&&(a>=count||b>=count))return false;
            reach[a][b]=true;adjacent[a][b]=true;
        }
        for (std::size_t k=0;k<count;++k) for (std::size_t a=0;a<count;++a) for (std::size_t b=0;b<count;++b) { if (!work.spend()) return false; reach[a][b]=reach[a][b]||(reach[a][k]&&reach[k][b]); }
        std::vector<bool> placed(count);
        for (std::size_t step=0;step<count;++step) {
            std::size_t next=count;
            for (std::size_t a=0;a<count;++a) if (!placed[a]) {
                bool incoming=false; for (std::size_t b=0;b<count;++b) if (!placed[b]&&reach[b][a]&&!reach[a][b]) incoming=true;
                if (!incoming) {
                    if (next==count) next=a;
                    // Prefer consecutive endpoints for a direct two-state cycle,
                    // reducing space for unrelated vertices between its legs.
                    if (!ordered[slot].empty()) {
                        const auto previous=static_cast<std::size_t>(std::ranges::find(list,ordered[slot].back())-list.begin());
                        if (adjacent[previous][a]&&adjacent[a][previous]) { next=a;break; }
                    }
                }
            }
            if (next==count) return false;
            placed[next]=true; ordered[slot].push_back(list[next]);
        }
        int maximum=0,total=0;
        const bool horizontal=state_scoped?(scope_direction=="LR"||scope_direction=="RL"):(diagram.direction=="LR"||diagram.direction=="RL");
        int gap=10,header_self_edges=0,er_role_envelope=0;
        if(diagram.family==RelationFamily::State&&horizontal&&!diagram.notes.empty()) {
            // A physical side Note may face the next child, not an outer
            // canvas edge. Reserve both source-measured Note envelopes in
            // each actual neighbour gap; RL reverses child order, not sides.
            if(!work.spend(12u*sizeof(int)+128u))return false;
            const bool reverse=state_scoped?scope_direction=="RL":diagram.direction=="RL";
            for(std::size_t n=1;n<ordered[slot].size();++n) {
                if(!work.spend(8u*sizeof(int)+32u))return false;
                const int left=ordered[slot][reverse?n:n-1],right=ordered[slot][reverse?n-1:n];
                int left_room=0,right_room=0;
                for(const auto& note:diagram.notes) {
                    if(!work.spend(24u))return false;
                    if(!((note.target==left&&note.right)||(note.target==right&&!note.right)))continue;
                    if(!work.spend(note.text.size()*(4u*sizeof(std::string_view)+8u)+64u))return false;
                    const int room=label_width(note.text)+10;
                    if(note.target==left)left_room=std::max(left_room,room);
                    else right_room=std::max(right_room,room);
                }
                gap=std::max(gap,left_room+right_room);
            }
        }
        if (diagram.family==RelationFamily::Er) for (const int edge_index:scope_edges[slot]) {
            const auto& edge=diagram.edges[static_cast<std::size_t>(edge_index)];
            if (!work.spend(edge.label.size()+1)) return false;
            // Label placement reserves two blank columns on each side of its
            // complete text. The canvas must admit that envelope even when the
            // vertical node gap depends only on the number of label rows.
            if (!horizontal&&!work.spend(edge.label.size()+1)) return false;
            const int role_width=label_width(edge.label);
            if (!edge.label.empty()) er_role_envelope=std::max(er_role_envelope,role_width+4);
            const int clearance=horizontal?role_width+2*std::max(marker_size(edge.start),marker_size(edge.end))+4:
                static_cast<int>(label_rows(edge.label).size())+2;
            gap=std::max(gap,clearance);
            if (edge.from==edge.to&&diagram.entities[static_cast<std::size_t>(edge.from)].er_attributes.empty()) {
                if (!work.spend()) return false;
                ++header_self_edges;
            }
        }
        if (diagram.family==RelationFamily::Class) for (const int edge_index:scope_edges[slot]) {
            const auto& edge=diagram.edges[static_cast<std::size_t>(edge_index)];
            if (!work.spend(edge.label.size()+1)) return false;
            if (!horizontal&&!work.spend(edge.label.size()+1)) return false;
            const int clearance=horizontal?label_width(edge.label)+2*std::max(marker_size(edge.start),marker_size(edge.end))+4:
                static_cast<int>(label_rows(edge.label).size())+2;
            gap=std::max(gap,clearance);
            if(edge.endpoint_texts) {
                if(!work.spend(edge.label.size()*(4u*sizeof(std::string_view)+4u)+12u))return false;
                const auto* source=class_caption(edge,true);const auto* target=class_caption(edge,false);
                const int from=marker_size(edge.start)+1+(source?source->w+2:0);
                const int to=marker_size(edge.end)+1+(target?target->w+2:0);
                const int central=edge.label.empty()?2:label_width(edge.label)+6;
                gap=std::max(gap,horizontal?from+to+central:std::max(source?source->h+2:0,target?target->h+2:0));
            }
        }
        int first_self_extra=0,last_self_extra=0,self_margin=0;
        for (const int index:ordered[slot]) {
            if(!work.spend(sizeof(EntityShape)+1u))return false;
            const auto& e=diagram.entities[static_cast<std::size_t>(index)]; auto& box=boxes[static_cast<std::size_t>(index)];
            if(diagram.family==RelationFamily::Er&&compound_scope){
                if(!work.spend(e.label.size()*(32u*sizeof(std::string)+32u*sizeof(std::string_view)+128u)+256u))return false;
                for(const auto& attribute:e.er_attributes){
                    const auto bytes=attribute.type.size()+attribute.name.size()+attribute.comment.size()+attribute.keys.size()*3u;
                    if(!work.spend(bytes*(32u*sizeof(std::string)+32u*sizeof(std::string_view)+128u)+
                        32u*sizeof(int)+16u*sizeof(std::vector<int>)+256u))return false;
                }
            }
            if (e.shape==EntityShape::Group) { if (!measure(index,depth+1)) return false; }
            else if (e.shape==EntityShape::Start||e.shape==EntityShape::Final) box.w=box.h=1;
            else if (e.shape==EntityShape::Fork||e.shape==EntityShape::Join) { box.w=std::max(7,degrees[static_cast<std::size_t>(index)]*2+1); box.h=1; }
            else if (diagram.family==RelationFamily::Er&&!e.er_attributes.empty()) {
                const auto table=er_table_geometry(e,work);if(!table)return false;
                box.w=table->width;box.h=table->height;
            } else if(e.shape==EntityShape::Note) {
                if(diagram.family!=RelationFamily::Class||!work.spend(e.label.size()*(8u*sizeof(std::string_view)+4u*sizeof(std::string)+16u)+256u))return false;
                const int rows=static_cast<int>(label_rows(e.label).size());
                box.w=std::max(4,label_width(e.label))+4;box.h=rows+3;
                if(box.w>kWidthLimit||box.h>kHeightLimit)return false;
            } else {
                int width=label_width(e.label),height=static_cast<int>(label_rows(e.label).size());
                if (!e.annotation.empty()) { width=std::max(width,label_width(e.annotation)); ++height; }
                bool first=true,previous=false;
                for (const auto& member:e.members) { width=std::max(width,label_width(member.text)); if (first||previous!=member.method) ++height; first=false;previous=member.method;++height; }
                if(diagram.family==RelationFamily::State&&!diagram.state_texts.empty()){
                    const auto record=diagram.state_text_index(index,work);if(!record)return false;
                    if(*record<diagram.state_texts.size()&&!diagram.state_texts[*record].body.empty()){
                        const auto& body=diagram.state_texts[*record].body;
                        if(e.shape!=EntityShape::Rounded||height!=1||!e.annotation.empty()||!e.members.empty()||body.size()>static_cast<std::size_t>(kHeightLimit-height-3))return false;
                        if(!work.spend(body.size()+12u*sizeof(int)+32u))return false;
                        height+=1+static_cast<int>(body.size());
                        for(const auto& row:body){
                            if(!work.spend(row.size()*(4u*sizeof(std::string)+4u*sizeof(std::string_view)+8u)+32u))return false;
                            width=std::max(width,label_width(row));
                        }
                    }
                }
                if(diagram.family==RelationFamily::Class) {
                    if(!work.spend(8u))return false;
                    if(e.members.empty()) {
                        if(!work.spend(4u*sizeof(std::optional<bool>)+8u))return false;
                        if(diagram.class_hide_empty_members_box==false) {
                            if(height>kHeightLimit-4)return false;
                            height+=4;
                        }
                    }
                }
                box.w=std::max({width+4,5,degrees[static_cast<std::size_t>(index)]+3}); box.h=std::max(height+2,3);
                // Keep a clear side-port row beneath class contents. Connecting
                // through a name/member row would replace its visible border.
                if (diagram.family==RelationFamily::Class) ++box.h;
                // Each paired Class endpoint gets a clear body row below its
                // complete header. No extra text decode or member is introduced.
                if (class_reciprocal) {
                    if(class_band_rows) {
                        if(!work.spend(3))return false;
                        const int rows=(*class_band_rows)[static_cast<std::size_t>(index)];
                        if(rows>kHeightLimit-height-2)return false;
                        box.h=std::max(box.h,height+rows+2);
                    } else box.h=std::max(box.h,height+degrees[static_cast<std::size_t>(index)]+2);
                }
                // Captioned incidences may all need the same exterior face.
                // Reserve one complete clearance interval per incidence; an
                // available opposite face is not guaranteed by route selection.
                if(class_endpoint_active&&!diagram.class_namespaces&&!class_reciprocal&&degrees[static_cast<std::size_t>(index)]>1) {
                    if(!work.spend(3u*sizeof(int)+8u))return false;
                    int captions=0,caption_rows=0;
                    for(const int edge_index:scope_edges[slot]) {
                        if(!work.spend(4u))return false;
                        const auto& edge=diagram.edges[static_cast<std::size_t>(edge_index)];
                        for(const bool source:{true,false}) {
                            if(!work.spend(3u))return false;
                            if((source?edge.from:edge.to)!=index)continue;
                            if(const auto* caption=class_caption(edge,source)) {
                                if(!work.spend(8u))return false;
                                if(caption->h<1||caption->h>kHeightLimit-2)return false;
                                const int span=caption->h+2;
                                if(caption_rows>kHeightLimit-span)return false;
                                ++captions;caption_rows+=span;
                            }
                        }
                    }
                    if(captions>1) {
                        if(!work.spend(2u))return false;
                        box.h=std::max(box.h,caption_rows);
                    }
                }
                // Every incident ER endpoint can need its own row on one face.
                // Opposite-face capacity cannot replace that local attachment.
                if (diagram.family==RelationFamily::Er) box.h=std::max(box.h,degrees[static_cast<std::size_t>(index)]+2);
                if (e.shape==EntityShape::Choice) { box.w=std::max(box.w,width+8);box.h=5; }
            }
            if (diagram.family==RelationFamily::Er&&header_self_edges>=2&&e.er_attributes.empty()) {
                int count=0,room=0;
                for (const int edge_index:scope_edges[slot]) {
                    if (!work.spend()) return false;
                    const auto& edge=diagram.edges[static_cast<std::size_t>(edge_index)];
                    if (edge.from!=index||edge.to!=index) continue;
                    ++count;
                    if (!work.spend(edge.label.size()+1)) return false;
                    const int label_room=edge.label.empty()?0:label_width(edge.label)+4;
                    room=std::max({room,marker_size(edge.start)+1,marker_size(edge.end)+1,label_room});
                }
                if (count>=2) {
                    // Both exterior faces admit a complete parallel-self label.
                    // Reserve only source-measured deficits in the outer lanes.
                    if (horizontal) {
                        if (index==ordered[slot].front()) first_self_extra=std::max(0,room-(owner<0?4:6));
                        if (index==ordered[slot].back()) last_self_extra=std::max(0,room-(owner<0?14:16));
                    } else self_margin=std::max(self_margin,room);
                }
            }
            maximum=std::max(maximum,horizontal?box.h:box.w); total+=(horizontal?box.w:box.h)+gap;
        }
        // The final routing lane remains ten cells; only real inter-node gaps
        // depend on ER labels. Other relation families retain the old geometry.
        total+=10-gap;
        // Every scope reserves routing lanes outside its child frames. These
        // are real geometry, independent of source labels or fixture identity.
        int margin=std::max({owner<0?12:6,static_cast<int>(scope_edges[slot].size())*2+6,self_margin});
        if(class_endpoint_active) {
            int old_offset=1,caption_offset=1;
            for(const int edge_index:scope_edges[slot]) {
                if(!work.spend(12))return false;
                const auto& edge=diagram.edges[static_cast<std::size_t>(edge_index)];
                old_offset=std::max({old_offset,marker_size(edge.start)+1,marker_size(edge.end)+1});
                for(const bool source:{true,false})if(const auto* caption=class_caption(edge,source))
                    caption_offset=std::max(caption_offset,marker_size(source?edge.start:edge.end)+caption->w+4);
            }
            if(class_band_rows)margin+=std::max(0,caption_offset-old_offset);
            else margin=std::max(margin,caption_offset);
        }
        if(class_endpoint_active&&(diagram.class_namespaces||horizontal)) {
            // Every endpoint text needs clearance inside its direct scope.
            // A namespace edge belongs to its LCA; descendant Group boxes
            // already contain their captions, so do not pad ancestors for them.
            if(!work.spend(sizeof(int)+4u))return false;
            int own_padding=0;
            for(const auto& edge:diagram.edges) {
                if(!work.spend(4))return false;
                for(const bool source:{true,false}) {
                    if(!work.spend(4))return false;
                    const auto* caption=class_caption(edge,source);if(!caption)continue;
                    const int index=source?edge.from:edge.to;
                    if(index<0||static_cast<std::size_t>(index)>=diagram.entities.size())return false;
                    if(diagram.entities[static_cast<std::size_t>(index)].owner!=owner)continue;
                    if(!work.spend(4))return false;
                    // Full clearance reaches marker+width+4 from the Class
                    // border. A Group frame needs one further strict cell.
                    own_padding=std::max(own_padding,marker_size(source?edge.start:edge.end)+caption->w+4+(owner>=0?1:0));
                }
            }
            if(own_padding) {
                if(!work.spend(6))return false;
                if(horizontal) {
                    // Horizontal scopes use first/last advances for their x
                    // padding; margin affects y only. RL mirrors these ends.
                    first_self_extra=std::max(first_self_extra,std::max(0,own_padding-(owner<0?4:6)));
                    last_self_extra=std::max(last_self_extra,std::max(0,own_padding-(owner<0?14:12)));
                } else margin=std::max(margin,own_padding);
            }
        }
        for (const auto& note:diagram.notes) if (std::ranges::find(list,note.target)!=list.end()) margin=std::max(margin,label_width(note.text)+10);
        if(diagram.family==RelationFamily::State&&horizontal&&!diagram.notes.empty()) {
            // Notes keep their physical side in every flow direction. The
            // vertical margin cannot protect a horizontal scope's outer faces.
            if(!work.spend(8u*sizeof(int)+128u))return false;
            for(const auto& note:diagram.notes) {
                if(!work.spend(16u))return false;
                const bool from_first=(state_scoped?scope_direction=="LR":diagram.direction=="LR")?!note.right:note.right;
                if(note.target!=(from_first?ordered[slot].front():ordered[slot].back()))continue;
                if(!work.spend(note.text.size()*(4u*sizeof(std::string_view)+8u)+64u))return false;
                const int room=label_width(note.text)+10;
                if(from_first)first_self_extra=std::max(first_self_extra,std::max(0,room-(owner<0?4:6)));
                else last_self_extra=std::max(last_self_extra,std::max(0,room-(owner<0?14:12)));
            }
        }
        if(diagram.family==RelationFamily::Er&&compound_scope&&owner>=0){
            const auto bytes=diagram.entities[static_cast<std::size_t>(owner)].label.size();
            if(!work.spend(bytes*(8u*sizeof(std::string_view)+16u)+128u))return false;
        }
        const int header_rows=owner<0?0:static_cast<int>(label_rows(diagram.entities[static_cast<std::size_t>(owner)].label).size());
        if (owner>=0) margin=std::max(margin,header_rows+3);
        int top=owner<0?4:std::max(6,header_rows+3);
        if(((diagram.family==RelationFamily::Class&&diagram.class_namespaces)||
            (diagram.family==RelationFamily::Er&&compound_scope))&&owner>=0&&!horizontal){
            // An entry channel must fit a complete cross-scope label rectangle
            // plus one routing row on either side, without touching the header
            // or the first child. Inspect ancestry, not only direct ownership.
            auto inside_group=[&](int endpoint)->std::optional<bool>{
                if(!work.spend()||endpoint<0||static_cast<std::size_t>(endpoint)>=diagram.entities.size())return {};
                int at=diagram.entities[static_cast<std::size_t>(endpoint)].owner;
                for(std::size_t depth=0;at>=0;++depth){
                    if(!work.spend()||depth>kGroupDepthLimit||static_cast<std::size_t>(at)>=diagram.entities.size())return {};
                    if(at==owner)return true;
                    at=diagram.entities[static_cast<std::size_t>(at)].owner;
                }
                return false;
            };
            int cross_label_rows=0;
            for(const auto& edge:diagram.edges){
                if(!work.spend())return false;
                if(edge.label.empty()&&!edge.endpoint_texts)continue;
                const auto from=inside_group(edge.from),to=inside_group(edge.to);
                if(!from||!to)return false;
                if(*from==*to)continue;
                // Cover scan, string-view vector allocation and growth/copies
                // before label_rows; no persistent cache or extra route search.
                if(!work.spend(edge.label.size()*(4u*sizeof(std::string_view)+1u)+64u))return false;
                cross_label_rows=std::max(cross_label_rows,static_cast<int>(label_rows(edge.label).size()));
                if(edge.endpoint_texts){if(!work.spend(2))return false;for(const bool source:{true,false})if(const auto* caption=class_caption(edge,source))cross_label_rows=std::max(cross_label_rows,caption->h);}
            }
            if(cross_label_rows)top=std::max(top,header_rows+cross_label_rows+6);
        }
        // A proper plain ER or class DAG has real peers in a rank. Plan their order
        // before the one existing physical placement/router pass.
        if ((diagram.family==RelationFamily::Er||diagram.family==RelationFamily::Class)&&owner<0&&diagram.direction=="TB"&&diagram.notes.empty()) {
            bool layered=count<=64&&count==diagram.entities.size();
            for (const int index:list) {
                if (!work.spend()) return false;
                const auto& entity=diagram.entities[static_cast<std::size_t>(index)];
                if (entity.owner!=-1||entity.shape!=EntityShape::Box||entity.er_alias||!entity.annotation.empty()||
                    !entity.members.empty()||!entity.er_attributes.empty()||degrees[static_cast<std::size_t>(index)]==0) layered=false;
            }
            std::size_t links_count=0;
            if (layered) for (std::size_t a=0;a<count;++a) for (std::size_t b=0;b<count;++b) {
                if (!work.spend()) return false;
                if (a==b&&reach[a][a]) layered=false;
                if (adjacent[a][b]) ++links_count;
            }
            if (links_count!=scope_edges[slot].size()) layered=false; // Repeated ordered endpoints are not this profile.
            if (layered) {
                if (!work.spend(3*count)) return false;
                std::vector<std::size_t> rank(count),position(count),topological(count);
                for (std::size_t n=0;n<count;++n) {
                    if (!work.spend(count)) return false;
                    topological[n]=static_cast<std::size_t>(std::ranges::find(list,ordered[slot][n])-list.begin());
                }
                std::size_t levels=1;
                for (std::size_t n=0;n<count;++n) {
                    const auto vertex=topological[n];
                    for (std::size_t prior=0;prior<count;++prior) {
                        if (!work.spend()) return false;
                        if (!adjacent[topological[prior]][vertex]) continue;
                        if (prior>=n) return false; // Existing topological order must precede every proper edge.
                        rank[vertex]=std::max(rank[vertex],rank[topological[prior]]+1);
                    }
                    levels=std::max(levels,rank[vertex]+1);
                }
                bool peers=false;
                for (std::size_t a=0;a<count;++a) for (std::size_t b=0;b<count;++b) {
                    if (!work.spend()) return false;
                    if (a!=b&&rank[a]==rank[b]) peers=true;
                    if (adjacent[a][b]&&rank[b]!=rank[a]+1) layered=false;
                }
                if (layered&&peers) {
                    if (!work.spend(4*count+levels+1+2*links_count)) return false;
                    std::vector<std::size_t> plan,best,starts(levels+1);
                    plan.reserve(count);best.reserve(count);
                    struct NeighborKey {std::size_t sum=0,count=1;};
                    std::vector<NeighborKey> keys(count);
                    std::vector<std::pair<std::size_t,std::size_t>> links;links.reserve(links_count);
                    for (std::size_t r=0;r<levels;++r) {
                        starts[r]=plan.size();
                        for (const auto vertex:topological) {if (!work.spend()) return false;if (rank[vertex]==r) plan.push_back(vertex);}
                    }
                    starts[levels]=plan.size();
                    for (std::size_t a=0;a<count;++a) for (std::size_t b=0;b<count;++b) {
                        if (!work.spend()) return false;
                        if (adjacent[a][b]) links.emplace_back(a,b);
                    }
                    auto positions=[&]() {
                        for (std::size_t r=0;r<levels;++r) for (auto n=starts[r];n<starts[r+1];++n) {
                            if (!work.spend()) return false;position[plan[n]]=n-starts[r];
                        }
                        return true;
                    };
                    auto crossings=[&](std::size_t& score) {
                        if (!positions()) return false;score=0;
                        for (std::size_t a=0;a<links.size();++a) for (std::size_t b=a+1;b<links.size();++b) {
                            if (!work.spend()) return false;
                            const auto [from,to]=links[a];const auto [other_from,other_to]=links[b];
                            if (rank[from]!=rank[other_from]||from==other_from||to==other_to) continue;
                            if ((position[from]<position[other_from])!=(position[to]<position[other_to])) ++score;
                        }
                        return true;
                    };
                    auto sweep=[&](bool downward) {
                        for (std::size_t step=1;step<levels;++step) {
                            const auto r=downward?step:levels-1-step;
                            if (!positions()) return false;
                            // Freeze all rational keys before moving any peer.
                            for (auto n=starts[r];n<starts[r+1];++n) {
                                const auto vertex=plan[n];std::size_t sum=0,neighbors=0;
                                for (std::size_t other=0;other<count;++other) {
                                    if (!work.spend()) return false;
                                    if (!(downward?adjacent[other][vertex]:adjacent[vertex][other])) continue;
                                    if (position[other]>std::numeric_limits<std::size_t>::max()-sum) return false;
                                    sum+=position[other];++neighbors;
                                }
                                keys[vertex]=neighbors?NeighborKey{sum,neighbors}:NeighborKey{position[vertex],1};
                            }
                            for (auto n=starts[r]+1;n<starts[r+1];++n) {
                                if (!work.spend()) return false;
                                const auto vertex=plan[n];auto at=n;
                                while (at>starts[r]) {
                                    if (!work.spend(2)) return false;
                                    const auto left=keys[vertex],right=keys[plan[at-1]];
                                    if (left.sum>std::numeric_limits<std::size_t>::max()/right.count||
                                        right.sum>std::numeric_limits<std::size_t>::max()/left.count) return false;
                                    if (left.sum*right.count>=right.sum*left.count) break;
                                    if (!work.spend()) return false;plan[at]=plan[at-1];--at;
                                }
                                plan[at]=vertex;
                            }
                        }
                        return true;
                    };
                    std::size_t best_score=0;
                    if (!crossings(best_score)||!work.spend(count)) return false;best=plan;
                    for (const bool downward:{true,false}) {
                        std::size_t score=0;if (!sweep(downward)||!crossings(score)) return false;
                        if (score<best_score) {if (!work.spend(count)) return false;best=plan;best_score=score;}
                    }
                    int peer_gap=10;
                    for (const int index:scope_edges[slot]) {
                        const auto& edge=diagram.edges[static_cast<std::size_t>(index)];
                        if (!work.spend(edge.label.size()+1)) return false;
                        peer_gap=std::max(peer_gap,label_width(edge.label)+2*std::max(marker_size(edge.start),marker_size(edge.end))+4);
                        if(edge.endpoint_texts) {
                            if(!work.spend(edge.label.size()*(4u*sizeof(std::string_view)+4u)+12u))return false;
                            const auto* source=class_caption(edge,true);const auto* target=class_caption(edge,false);
                            const int from=marker_size(edge.start)+1+(source?source->w+2:0),to=marker_size(edge.end)+1+(target?target->w+2:0);
                            peer_gap=std::max(peer_gap,from+to+(edge.label.empty()?2:label_width(edge.label)+6));
                        }
                    }
                    if (!work.spend(2*levels)) return false;
                    std::vector<int> row_width(levels),row_height(levels);
                    int widest=0,body_height=0;
                    bool rank_fits=true;
                    for (std::size_t r=0;r<levels;++r) {
                        for (auto n=starts[r];n<starts[r+1];++n) {
                            if (!work.spend()) return false;
                            const auto& box=boxes[static_cast<std::size_t>(list[best[n]])];
                            const int separation=n==starts[r]?0:peer_gap;
                            if (box.w>kWidthLimit||separation>kWidthLimit-row_width[r]||box.w>kWidthLimit-row_width[r]-separation) {rank_fits=false;break;}
                            row_width[r]+=separation+box.w;row_height[r]=std::max(row_height[r],box.h);
                        }
                        if (!rank_fits) break;
                        const int separation=r?gap:0;
                        if (row_height[r]>kHeightLimit||separation>kHeightLimit-body_height||row_height[r]>kHeightLimit-body_height-separation) {rank_fits=false;break;}
                        body_height+=separation+row_height[r];widest=std::max(widest,row_width[r]);
                    }
                    if (margin>kWidthLimit/2||widest>kWidthLimit-2*margin||body_height>kHeightLimit-18) rank_fits=false;
                    // A class rank plan is optional: decide whether it fits before
                    // moving any box, otherwise keep the existing linear plan.
                    if (!rank_fits&&diagram.family==RelationFamily::Er) return false;
                    if (rank_fits) {
                        const int width=std::max(widest+2*margin,er_role_envelope),height=body_height+18;
                        if (width>kWidthLimit) return false;
                        int y=4;
                        for (std::size_t r=0;r<levels;++r) {
                            int x=(width-row_width[r])/2;
                            for (auto n=starts[r];n<starts[r+1];++n) {
                                if (!work.spend()) return false;
                                const int index=list[best[n]];translate(index,x,y);x+=boxes[static_cast<std::size_t>(index)].w+peer_gap;
                            }
                            y+=row_height[r]+gap;
                        }
                        canvas_width=width;canvas_height=height;ranked_peers=true;return true;
                    }
                }
            }
        }
        if(owner<0&&diagram.family==RelationFamily::Class&&diagram.class_namespaces&&horizontal&&
            diagram.notes.empty()&&!class_endpoint_active) {
            if(!work.spend(4u*sizeof(std::optional<bool>)+32u))return false;
            const auto compound=compound_class_columns(reach);if(!compound)return false;if(*compound)return true;
        }
        int width=horizontal?total+8+first_self_extra+last_self_extra:maximum+2*margin;
        const int height=horizontal?maximum+2*margin:total+8+(owner<0?0:top-6);
        width=std::max(width,er_role_envelope);
        if (owner>=0) width=std::max(width,label_width(diagram.entities[static_cast<std::size_t>(owner)].label)+4);
        // Pay the new eligibility predicate on every final linear scope.
        if(!work.spend(8u*sizeof(bool)+16u))return false;
        if(diagram.family==RelationFamily::Er&&owner<0&&!horizontal&&header_self_edges==1) {
            if(!work.spend(12u*sizeof(int)+4u*sizeof(std::size_t)+4u*sizeof(const Relationship*)+128u))return false;
            if(width<1||width>kWidthLimit)return false;
            for(const int edge_index:scope_edges[slot]) {
                if(!work.spend(16u))return false;
                const auto& edge=diagram.edges[static_cast<std::size_t>(edge_index)];
                if(edge.from!=edge.to)continue;
                const auto& entity=diagram.entities[static_cast<std::size_t>(edge.from)];
                if(entity.shape!=EntityShape::Box||!entity.er_attributes.empty())continue;
                const auto& box=boxes[static_cast<std::size_t>(edge.from)];
                if(box.w<1||box.w>width||edge.label.size()>kSourceLimit)return false;
                // label_width owns split-row vector growth and temporary strings.
                if(!work.spend((edge.label.size()+1u)*(8u*sizeof(std::string_view)+4u*sizeof(std::string)+64u)+
                    4u*sizeof(std::vector<std::string_view>)+256u))return false;
                const int label_room=edge.label.empty()?0:label_width(edge.label)+4;
                const int required=std::max({marker_size(edge.start)+1,marker_size(edge.end)+1,label_room});
                const int difference=width-box.w;
                // x=floor(difference/2); the right exterior is its ceil peer.
                // One self-loop needs one face, not two equally wide faces.
                const int room=difference/2+difference%2;
                const int deficit=std::max(0,required-room);
                if(!work.spend(8u))return false;
                if(deficit>(kWidthLimit-width)/2)return false;
                width+=2*deficit;
                break;
            }
        }
        if(diagram.family==RelationFamily::Requirement&&owner<0&&horizontal&&width>kWidthLimit) {
            // Condense only the existing strict reach order. Mutually reachable
            // nodes and incomparable peers can share one complete column.
            if(count==0u||count>64u||ordered[slot].size()!=count||scope_edges[slot].size()>128u)return false;
            if(!work.spend(64u*(10u*sizeof(int)+sizeof(Point)+sizeof(bool))+count*count*128u+1024u))return false;
            std::array<int,64> local{},rank{},column_width{},column_height{},column_count{},column_x{},next_y{};
            std::array<Point,64> positions{};std::array<bool,64> seen{};
            if(!work.spend(64u*64u*sizeof(unsigned char)+scope_edges[slot].size()*(8u*sizeof(int)+32u)+128u))return false;
            std::array<std::array<unsigned char,64>,64> parallel_counts{};
            int label_w=0,label_h=0,marker_room=0,parallel=1;
            for(const int edge_index:scope_edges[slot]) {
                const auto& edge=diagram.edges[static_cast<std::size_t>(edge_index)];
                if(edge.from<0||edge.to<0||edge.from>=64||edge.to>=64)return false;
                auto& multiplicity=parallel_counts[static_cast<std::size_t>(edge.from)][static_cast<std::size_t>(edge.to)];
                ++multiplicity;parallel=std::max(parallel,static_cast<int>(multiplicity));
                if(edge.label.size()>kSourceLimit||!work.spend((edge.label.size()+1u)*(8u*sizeof(std::string_view)+4u*sizeof(std::string)+64u)+256u))return false;
                label_w=std::max(label_w,label_width(edge.label));
                label_h=std::max(label_h,static_cast<int>(label_rows(edge.label).size()));
                marker_room=std::max({marker_room,marker_size(edge.start)+1,marker_size(edge.end)+1});
            }
            // Each extra directed parallel caption needs its complete row,
            // both padding rows and one independent connector clearance row.
            const int extra_gap=(parallel-1)*(label_h+3);
            const int gap_x=std::max(10,label_w+2*marker_room+4),gap_y=std::max(10,label_h+2*marker_room+4)+extra_gap;
            const int column_margin=std::max(margin,label_w+2*marker_room+4);
            if(column_margin>kWidthLimit/2||column_margin>kHeightLimit/2)return false;
            int columns=0;
            for(std::size_t n=0;n<count;++n) {
                const int index=ordered[slot][n];
                const auto at=std::ranges::find(list,index);if(at==list.end())return false;
                const auto position=static_cast<std::size_t>(at-list.begin());if(seen[position])return false;seen[position]=true;local[n]=static_cast<int>(position);
                const auto& entity=diagram.entities[static_cast<std::size_t>(index)];const auto& box=boxes[static_cast<std::size_t>(index)];
                if(entity.owner!=-1||entity.shape!=EntityShape::Box||!children[static_cast<std::size_t>(index+1)].empty()||box.w<1||box.w>kWidthLimit||box.h<1||box.h>kHeightLimit)return false;
                int level=0;
                for(std::size_t previous=0;previous<n;++previous) {
                    const auto before=static_cast<std::size_t>(local[previous]);
                    if(reach[before][position]&&!reach[position][before])level=std::max(level,rank[before]+1);
                }
                if(level<0||level>=static_cast<int>(count))return false;rank[position]=level;columns=std::max(columns,level+1);
                const auto r=static_cast<std::size_t>(level);column_width[r]=std::max(column_width[r],box.w);
                const int separation=column_count[r]?gap_y:0;
                if(separation>kHeightLimit-column_height[r]||box.h>kHeightLimit-column_height[r]-separation)return false;
                column_height[r]+=separation+box.h;++column_count[r];
            }
            int plan_width=2*column_margin,body_height=0;
            for(int r=0;r<columns;++r) {
                const auto col=static_cast<std::size_t>(r);if(column_count[col]<1||column_width[col]<1)return false;
                const int separation=r?gap_x:0;
                if(separation>kWidthLimit-plan_width||column_width[col]>kWidthLimit-plan_width-separation)return false;
                column_x[col]=plan_width-column_margin+separation;plan_width+=separation+column_width[col];body_height=std::max(body_height,column_height[col]);
            }
            if(body_height>kHeightLimit-2*column_margin)return false;const int plan_height=body_height+2*column_margin;
            for(int r=0;r<columns;++r)next_y[static_cast<std::size_t>(r)]=(plan_height-column_height[static_cast<std::size_t>(r)])/2;
            for(std::size_t n=0;n<count;++n) {
                const auto position=static_cast<std::size_t>(local[n]),col=static_cast<std::size_t>(rank[position]);
                const auto& box=boxes[static_cast<std::size_t>(ordered[slot][n])];
                int x=column_x[col]+(column_width[col]-box.w)/2;const int y=next_y[col];
                if(diagram.direction=="RL")x=plan_width-x-box.w;
                if(x<column_margin||y<column_margin||box.w>plan_width-column_margin-x||box.h>plan_height-column_margin-y)return false;
                positions[position]={x,y};next_y[col]+=box.h+gap_y;
            }
            // Validate the entire plan before mutating one box or canvas bound.
            for(std::size_t a=0;a<count;++a)for(std::size_t b=0;b<count;++b) {
                if(reach[a][b]&&!reach[b][a]&&rank[a]>=rank[b])return false;
                if(a>=b)continue;
                const auto& first=boxes[static_cast<std::size_t>(list[a])];const auto& second=boxes[static_cast<std::size_t>(list[b])];
                const auto p=positions[a],q=positions[b];
                if(p.x<q.x+second.w&&q.x<p.x+first.w&&p.y<q.y+second.h&&q.y<p.y+first.h)return false;
            }
            for(std::size_t n=0;n<count;++n)translate(list[n],positions[n].x,positions[n].y);
            canvas_width=plan_width;canvas_height=plan_height;return true;
        }
        if (width>kWidthLimit||height>kHeightLimit) return false;
        int advance=horizontal?(owner<0?4:6)+first_self_extra:top;
        for (const int index:ordered[slot]) {
            auto& box=boxes[static_cast<std::size_t>(index)];
            const int x=horizontal?((state_scoped?scope_direction=="RL":diagram.direction=="RL")?width-advance-box.w:advance):(width-box.w)/2;
            const int y=horizontal?(height-box.h)/2:((state_scoped?scope_direction=="BT":diagram.direction=="BT")?height-advance-box.h:advance);
            translate(index,x,y); advance+=(horizontal?box.w:box.h)+gap;
        }
        if (owner>=0) { boxes[static_cast<std::size_t>(owner)].w=width;boxes[static_cast<std::size_t>(owner)].h=height; }
        else { canvas_width=width;canvas_height=height; }
        return true;
    }
    void translate(int index,int x,int y) {
        auto& box=boxes[static_cast<std::size_t>(index)]; const int dx=x-box.x,dy=y-box.y;box.x=x;box.y=y;
        auto shift=[&](auto&& self,int at)->void { for (const int child:children[static_cast<std::size_t>(at+1)]) { auto& b=boxes[static_cast<std::size_t>(child)]; b.x+=dx;b.y+=dy;self(self,child); } };
        shift(shift,index);
    }
    int canvas_width=0,canvas_height=0;
    bool ranked_peers=false;
    bool class_reciprocal=false;
};
// Source-only two-face decision. No layout, label, cardinality or route is copied
// from output, and no future endpoint is reserved in the occupancy canvas.
struct ReciprocalEdgePlan {int face=0,from_row=0,to_row=0,inset=0;};
struct ReciprocalPlan {std::vector<ReciprocalEdgePlan> edges;int left_spine=0,right_spine=0;};
bool reciprocal_plan(const RelationDiagram& diagram,const RelationLayout& layout,int owner,Work& work,std::optional<ReciprocalPlan>& result) {
    // Only constant-time facts participate in the zero-new-scan exit.
    const bool class_profile=diagram.family==RelationFamily::Class&&layout.class_reciprocal&&owner<0;
    if(!class_profile&&(diagram.family!=RelationFamily::Er||diagram.direction!="TB"||owner>=0||diagram.edges.size()<8))return true;
    const auto vertices=diagram.entities.size(),edges=diagram.edges.size();
    if(vertices>64||edges>128||!diagram.notes.empty()||layout.ranked_peers)return true;
    if(!class_profile) for(std::size_t n=0;n<vertices;++n) {
        if(!work.spend())return false;const auto& entity=diagram.entities[n];
        if(entity.owner!=-1||entity.shape!=EntityShape::Box||entity.er_alias||!entity.er_attributes.empty()||!entity.members.empty()||!entity.annotation.empty()||layout.degrees[n]==0)return true;
    }
    if(layout.ordered[0].size()!=vertices||layout.scope_edges[0].size()!=edges)return true;
    struct Pair {int lo=-1,hi=-1;std::array<int,2> edge{-1,-1};int count=0,color=-1;};
    // The bound64 is validated before storage, including inapplicable inputs
    // with more than64 distinct endpoint pairs. Every initialization is prepaid.
    if(!work.spend(64*10+edges*3+16))return false;
    std::array<Pair,64> pairs{};std::array<int,64> positions{},queue{},incident{};positions.fill(-1);
    std::size_t pair_count=0;int previous_bottom=-1;
    for(std::size_t n=0;n<vertices;++n) {
        const auto order=class_profile&&diagram.direction=="BT"?vertices-1-n:n;
        if(!work.spend())return false;const int id=layout.ordered[0][order];
        if(id<0||static_cast<std::size_t>(id)>=vertices||positions[static_cast<std::size_t>(id)]!=-1)return false;
        if(class_profile) {
            if(!work.spend(4))return false;const auto& box=layout.boxes[static_cast<std::size_t>(id)];
            if(box.y<0||box.bottom()>=layout.canvas_height||box.y<=previous_bottom)return false;
            previous_bottom=box.bottom();
        }
        positions[static_cast<std::size_t>(id)]=static_cast<int>(n);
    }
    for(const int index:layout.scope_edges[0]) {
        if(!work.spend())return false;const auto& edge=diagram.edges[static_cast<std::size_t>(index)];
        if(edge.from<0||edge.to<0||static_cast<std::size_t>(edge.from)>=vertices||static_cast<std::size_t>(edge.to)>=vertices)return false;
        if(edge.from==edge.to)return true;
        const int a=positions[static_cast<std::size_t>(edge.from)],b=positions[static_cast<std::size_t>(edge.to)];
        const int lo=std::min(a,b),hi=std::max(a,b);std::size_t pair=0;
        for(;pair<pair_count;++pair){if(!work.spend())return false;if(pairs[pair].lo==lo&&pairs[pair].hi==hi)break;}
        if(pair==pair_count) {
            if(pair_count==pairs.size())return true;
            if(!work.spend(6))return false;pairs[pair_count++]={lo,hi,{-1,-1},0,-1};
        }
        auto& entry=pairs[pair];if(entry.count==2)return true;
        if(entry.count==1) {
            const auto& prior=diagram.edges[static_cast<std::size_t>(entry.edge[0])];
            if(prior.from!=edge.to||prior.to!=edge.from)return true;
        }
        if(!work.spend())return false;entry.edge[static_cast<std::size_t>(entry.count++)]=index;
    }
    for(std::size_t n=0;n<pair_count;++n){if(!work.spend())return false;if(pairs[n].count!=2)return true;}
    auto conflict=[](const Pair& a,const Pair& b){return (a.lo<b.lo&&b.lo<a.hi&&a.hi<b.hi)||(b.lo<a.lo&&a.lo<b.hi&&b.hi<a.hi);};
    bool interleaves=false;
    for(std::size_t seed=0;seed<pair_count;++seed) {
        if(!work.spend())return false;if(pairs[seed].color!=-1)continue;
        if(!work.spend(2))return false;std::size_t begin=0,end=1;queue[0]=static_cast<int>(seed);pairs[seed].color=0;
        while(begin<end) {
            if(!work.spend())return false;const int at=queue[begin++];
            for(std::size_t n=0;n<pair_count;++n) {
                if(!work.spend())return false;if(!conflict(pairs[static_cast<std::size_t>(at)],pairs[n]))continue;
                interleaves=true;const int color=1-pairs[static_cast<std::size_t>(at)].color;
                if(pairs[n].color==-1){if(end>=queue.size()||!work.spend(2))return false;pairs[n].color=color;queue[end++]=static_cast<int>(n);}
                else if(pairs[n].color!=color)return true;
            }
        }
    }
    if(!interleaves&&!class_profile)return true;
    // The first three fields were prepaid before eligibility; pay the new
    // fourth field only for an active plan, before allocation/initialization.
    if(!work.spend(edges))return false;
    ReciprocalPlan plan;plan.edges.resize(edges);
    int minimum=layout.canvas_width,maximum=-1,marker=0;
    for(std::size_t n=0;n<vertices;++n){if(!work.spend())return false;minimum=std::min(minimum,layout.boxes[n].x);maximum=std::max(maximum,layout.boxes[n].right());}
    for(const auto& edge:diagram.edges){if(!work.spend())return false;marker=std::max({marker,marker_size(edge.start),marker_size(edge.end)});}
    int offset=marker+1;
    if(layout.class_band_rows)for(const auto& edge:diagram.edges) {
        if(!work.spend(4))return false;
        for(const bool source:{true,false})if(const auto* caption=class_caption(edge,source))offset=std::max(offset,marker_size(source?edge.start:edge.end)+caption->w+4);
    }
    plan.left_spine=minimum-offset;plan.right_spine=maximum+offset;
    if(plan.left_spine<=0||plan.right_spine>=layout.canvas_width-1)return false;
    for(std::size_t vertex=0;vertex<vertices;++vertex)for(const int face:{-1,1}) {
        if(class_profile&&!work.spend(2))return false;
        // The qualified Class measure reserves D blank rows immediately above
        // bottom, after all H header rows; ER keeps its existing y+1 protocol.
        const int capacity=layout.class_band_rows?(*layout.class_band_rows)[vertex]:layout.degrees[vertex];
        const int first_row=class_profile?layout.boxes[vertex].bottom()-capacity:layout.boxes[vertex].y+1;
        int band_cursor=0;
        std::size_t used=0;const int position=positions[vertex];
        for(std::size_t n=0;n<pair_count;++n) {
            if(!work.spend())return false;const auto& pair=pairs[n];
            if((pair.color==0?-1:1)!=face||(pair.lo!=position&&pair.hi!=position))continue;
            if(!work.spend())return false;incident[used++]=static_cast<int>(n);
        }
        auto before=[&](int a,int b) {
            const auto& first=pairs[static_cast<std::size_t>(a)];const auto& second=pairs[static_cast<std::size_t>(b)];
            const int other_a=first.lo==position?first.hi:first.lo,other_b=second.lo==position?second.hi:second.lo;
            // Above: nearest first. Below: farthest first. Pair source order
            // resolves ties; keys are independent of any current sort state.
            return std::tuple{other_a<position?0:1,-other_a,a}<std::tuple{other_b<position?0:1,-other_b,b};
        };
        for(std::size_t n=1;n<used;++n) {
            if(!work.spend())return false;const int value=incident[n];std::size_t at=n;
            while(at>0){if(!work.spend())return false;if(!before(value,incident[at-1]))break;if(!work.spend())return false;incident[at]=incident[at-1];--at;}
            if(!work.spend())return false;incident[at]=value;
        }
        for(std::size_t block=0;block<used;++block) {
            const auto& pair=pairs[static_cast<std::size_t>(incident[block])];const auto& box=layout.boxes[vertex];
            for(int local=0;local<2;++local) {
                const int rail=layout.class_band_rows?(position==pair.hi?local:1-local):local;
                int row=first_row+static_cast<int>(block*2)+(position==pair.hi?rail:1-rail);
                if(layout.class_band_rows) {
                    if(!work.spend(8))return false;
                    const auto& edge=diagram.edges[static_cast<std::size_t>(pair.edge[static_cast<std::size_t>(rail)])];
                    const auto* caption=class_caption(edge,edge.from==static_cast<int>(vertex));
                    const int band=caption?caption->h+2:1;
                    if(band>capacity-band_cursor)return false;
                    row=first_row+band_cursor+(caption?1:0);band_cursor+=band;
                }
                if(!work.spend(3))return false;
                if(row<=box.y||row>=box.bottom())return false;
                const int edge_index=pair.edge[static_cast<std::size_t>(rail)];auto& port=plan.edges[static_cast<std::size_t>(edge_index)];port.face=face;
                if(diagram.edges[static_cast<std::size_t>(edge_index)].from==static_cast<int>(vertex))port.from_row=row;else port.to_row=row;
            }
        }
    }
    // Count actual same-page port intervals strictly enclosing this arc.
    // These source slots are fixed before routing; no canvas occupancy probe.
    for(std::size_t n=0;n<edges;++n) {
        if(!work.spend())return false;
        auto& arc=plan.edges[n];const int lo=std::min(arc.from_row,arc.to_row),hi=std::max(arc.from_row,arc.to_row);
        for(const auto& outer:plan.edges) {
            if(!work.spend())return false;
            if(arc.face==outer.face&&std::min(outer.from_row,outer.to_row)<lo&&hi<std::max(outer.from_row,outer.to_row)) {
                if(!work.spend())return false;++arc.inset;
            }
        }
    }
    for(std::size_t n=0;n<edges;++n) {
        if(!work.spend(diagram.edges[n].label.size()*2+2))return false;
        const auto& port=plan.edges[n];const int room=port.face<0?plan.left_spine:layout.canvas_width-1-plan.right_spine;
        if(!port.face||!port.from_row||!port.to_row||port.inset>=room||
           port.from_row<port.inset||port.from_row>=layout.canvas_height-port.inset||
           port.to_row<port.inset||port.to_row>=layout.canvas_height-port.inset||
           (!diagram.edges[n].label.empty()&&label_width(diagram.edges[n].label)+4>room-port.inset))return false;
    }
    result=std::move(plan);return true;
}


// Read-only source ancestry and physical straight-frame permission. Existing
// A-star remains the only route search; no Group becomes a business endpoint.
struct CompoundScopeRoute {
    const RelationLayout& layout;Work& work;std::array<unsigned char,64+kGroupLimit> scopes{};std::array<int,64+kGroupLimit> header_bottom{};
    std::array<std::size_t,kGroupLimit> transit_groups{};std::size_t transit_count=0;
    CompoundScopeRoute(const RelationLayout& l,Work& w):layout(l),work(w){}
    bool prepare(const Relationship& edge,int owner) {
        if(layout.diagram.entities.size()>scopes.size())return false;
        for(int at=owner;at>=0;at=layout.diagram.entities[static_cast<std::size_t>(at)].owner){if(!work.spend()||static_cast<std::size_t>(at)>=layout.diagram.entities.size())return false;scopes[static_cast<std::size_t>(at)]=2;}
        for(const int endpoint:{edge.from,edge.to})for(int at=layout.diagram.entities[static_cast<std::size_t>(endpoint)].owner;at!=owner;at=layout.diagram.entities[static_cast<std::size_t>(at)].owner){if(!work.spend()||at<0||static_cast<std::size_t>(at)>=layout.diagram.entities.size())return false;scopes[static_cast<std::size_t>(at)]=1;}
        // Only endpoint-ancestry groups can lend a straight structural frame.
        // Index them once; repeated A-star steps need not scan ordinary classes.
        for(std::size_t n=0;n<layout.diagram.entities.size();++n){
            if(!work.spend())return false;if(scopes[n]!=1)continue;
            if(transit_count==transit_groups.size())return false;transit_groups[transit_count++]=n;
            const auto& e=layout.diagram.entities[n];
            if((layout.diagram.family==RelationFamily::State||layout.diagram.family==RelationFamily::Er)&&!work.spend(e.label.size()*(4u*sizeof(std::string_view)+8u)+2u*sizeof(std::vector<std::string_view>)+64u))return false;
            if(!work.spend(e.label.size()*4+1))return false;header_bottom[n]=layout.boxes[n].y+static_cast<int>(label_rows(e.label).size())+1;
        }
        return true;
    }
    bool inside(Point p) const {
        for(std::size_t n=0;n<layout.diagram.entities.size();++n){
            if(!work.spend())return false;const auto& e=layout.diagram.entities[n];const auto& b=layout.boxes[n];
            const bool contains=p.x>=b.x&&p.x<=b.right()&&p.y>=b.y&&p.y<=b.bottom();if(!contains)continue;
            if(e.shape!=EntityShape::Group||scopes[n]==0)return false;
            if(scopes[n]==2)continue;
            if(p.y<=header_bottom[n])return false;
            if((p.x==b.x||p.x==b.right())&&(p.y==b.y||p.y==b.bottom()))return false;
        }
        return true;
    }
    unsigned structural(Point p) const {
        for(std::size_t at=0;at<transit_count;++at){
            if(!work.spend())return 0;const auto n=transit_groups[at];const auto& b=layout.boxes[n];
            if(p.y<=header_bottom[n])continue;
            if((p.x==b.x||p.x==b.right())&&p.y>b.y&&p.y<b.bottom())return kUp|kDown;
            if((p.y==b.y||p.y==b.bottom())&&p.x>b.x&&p.x<b.right())return kLeft|kRight;
        }
        return 0;
    }
    bool step(Point from,Point to) const {
        for(const auto point:{from,to}){const auto bits=structural(point);if(bits==(kUp|kDown)&&from.x==to.x)return false;if(bits==(kLeft|kRight)&&from.y==to.y)return false;}
        return true;
    }
};

struct RouteSearch {
    Canvas& canvas; const RelationLayout& layout; int owner; Work& work;
    std::vector<int> distance,parent; std::vector<unsigned> stamp; unsigned generation=0;
    std::string_view state_scope_direction;
    int self_face_normal=0,self_face_border=0;
    const CompoundScopeRoute* compound_route=nullptr;const std::vector<Point>* compound_reservations=nullptr;
    int reciprocal_face=0,reciprocal_spine=0,reciprocal_inset=0;Point reciprocal_start{},reciprocal_goal{};
    RouteSearch(Canvas& c,const RelationLayout& l,int scope):canvas(c),layout(l),owner(scope),work(c.work),distance(c.cells.size()),parent(c.cells.size()),stamp(c.cells.size()) {}
    bool inside(Point p) const {
        if (!canvas.inside(p)) return false;
        if(reciprocal_face) {
            if(!work.spend())return false;
            if(p.y<reciprocal_inset||p.y>=canvas.height-reciprocal_inset||
               (reciprocal_face<0?p.x<reciprocal_inset:p.x>=canvas.width-reciprocal_inset))return false;
            const bool page=reciprocal_face<0?p.x<reciprocal_spine:p.x>reciprocal_spine;
            const bool anchor=p.x==reciprocal_spine&&(p==reciprocal_start||p==reciprocal_goal);
            if(!page&&!anchor)return false;
        }
        if ((self_face_normal>0&&p.x<=self_face_border)||(self_face_normal<0&&p.x>=self_face_border)) return false;
        if (owner>=0) { const auto& b=layout.boxes[static_cast<std::size_t>(owner)]; if (p.x<=b.x||p.x>=b.right()||p.y<=b.y||p.y>=b.bottom()) return false; }
        if(compound_route)return compound_route->inside(p);
        for (const int index:layout.children[static_cast<std::size_t>(owner+1)]) {
            const auto& b=layout.boxes[static_cast<std::size_t>(index)];
            if (p.x>=b.x&&p.x<=b.right()&&p.y>=b.y&&p.y<=b.bottom()) return false;
        }
        return true;
    }
    bool free(Point p) const {
        if(!compound_route)return inside(p)&&!canvas.at(p).blocked;
        if(!inside(p))return false;if(!canvas.at(p).blocked)return true;
        const auto bits=compound_route->structural(p);if(!bits||canvas.at(p).bits!=bits||canvas.at(p).glyph!=line_glyph(bits))return false;
        if(compound_reservations)for(const auto point:*compound_reservations){if(!work.spend()||point==p)return false;}
        return work.remaining!=0;
    }
    bool step(Point from,Point to) const {return !compound_route||compound_route->step(from,to);}
    std::optional<std::vector<Point>> path(Point start,Point goal) {
        if (!free(start)||!free(goal)||!work.spend()) return {};
        ++generation;
        struct Candidate { int estimate,cost,index; bool operator>(const Candidate& other) const { return std::tie(estimate,cost,index)>std::tie(other.estimate,other.cost,other.index); } };
        std::priority_queue<Candidate,std::vector<Candidate>,std::greater<>> open;
        const auto heuristic=[&](Point p) { return std::abs(p.x-goal.x)+std::abs(p.y-goal.y); };
        const int first=canvas.index(start),last=canvas.index(goal);stamp[static_cast<std::size_t>(first)]=generation;distance[static_cast<std::size_t>(first)]=0;parent[static_cast<std::size_t>(first)]=-1;open.push({heuristic(start),0,first});
        while (!open.empty()) {
            if (!work.spend()) return {};
            const auto current=open.top();open.pop();if (current.cost!=distance[static_cast<std::size_t>(current.index)]) continue;
            if (current.index==last) {
                std::vector<Point> result;
                for (int at=last;at>=0;at=parent[static_cast<std::size_t>(at)]) { if (!work.spend()) return {}; result.push_back({at%canvas.width,at/canvas.width}); }
                std::ranges::reverse(result);return result;
            }
            const Point p{current.index%canvas.width,current.index/canvas.width};
            for (const auto step:kSteps) {
                if (!work.spend()) return {};
                const auto next=p+step;if (!free(next)||(compound_route&&!this->step(p,next))) continue;
                const int at=canvas.index(next),cost=current.cost+1;
                if (stamp[static_cast<std::size_t>(at)]==generation&&distance[static_cast<std::size_t>(at)]<=cost) continue;
                stamp[static_cast<std::size_t>(at)]=generation;distance[static_cast<std::size_t>(at)]=cost;parent[static_cast<std::size_t>(at)]=current.index;open.push({cost+heuristic(next),cost,at});
            }
        }
        return {};
    }
};
// A geometrically empty label box can lie inside an unrelated closed face.
// Connected free-cell regions rule out those false candidates before A* work.
std::optional<std::vector<int>> free_regions(RouteSearch& search) {
    auto& canvas=search.canvas;
    if (!canvas.work.spend(canvas.cells.size())) return {};
    std::vector<int> regions(canvas.cells.size(),-1);int region=0;
    for (std::size_t cell=0;cell<regions.size();++cell) {
        if (!canvas.work.spend()) return {};
        const Point start{static_cast<int>(cell)%canvas.width,static_cast<int>(cell)/canvas.width};
        if (regions[cell]>=0||!search.free(start)) continue;
        std::vector<Point> queue{start};regions[cell]=region;
        for (std::size_t n=0;n<queue.size();++n) for (const auto step:kSteps) {
            if (!canvas.work.spend()) return {};
            const auto next=queue[n]+step;if (!search.free(next)||(search.compound_route&&!search.step(queue[n],next))) continue;
            const auto index=static_cast<std::size_t>(canvas.index(next));
            if (regions[index]<0) { regions[index]=region;queue.push_back(next); }
        }
        ++region;
    }
    return regions;
}
bool draw_box(Canvas& canvas,const RelationBox& box,bool rounded) {
    if (!canvas.segment({box.x,box.y},{box.right(),box.y})||!canvas.segment({box.right(),box.y},{box.right(),box.bottom()})||!canvas.segment({box.right(),box.bottom()},{box.x,box.bottom()})||!canvas.segment({box.x,box.bottom()},{box.x,box.y})) return false;
    if (rounded) return canvas.put({box.x,box.y},"╭")&&canvas.put({box.right(),box.y},"╮")&&canvas.put({box.right(),box.bottom()},"╯")&&canvas.put({box.x,box.bottom()},"╰");
    return true;
}
bool draw_er_table(Canvas& canvas,const RelationEntity& entity,const RelationBox& box) {
    const auto table=er_table_geometry(entity,canvas.work);
    if(!table||table->width!=box.w||table->height!=box.h||!draw_box(canvas,box,false))return false;
    if(!canvas.multiline({box.x+1,box.y+1},entity.label,box.w-2))return false;
    const int header_divider=box.y+1+static_cast<int>(label_rows(entity.label).size());
    int row=header_divider;
    for(std::size_t index=0;index<entity.er_attributes.size();++index) {
        if(!canvas.segment({box.x,row},{box.right(),row}))return false;
        const auto fields=er_attribute_fields(entity.er_attributes[index]);int column=box.x;
        for(std::size_t c=0;c<table->roles.size();++c) {
            const auto text=label_rows(fields[static_cast<std::size_t>(table->roles[c])]);
            for(std::size_t y=0;y<text.size();++y)if(!text[y].empty()&&!canvas.text({column+2,row+1+static_cast<int>(y)},text[y]))return false;
            column+=table->widths[c]+1;
        }
        row+=table->heights[index]+1;
    }
    int column=box.x;
    for(std::size_t c=0;c+1<table->roles.size();++c) {
        column+=table->widths[c]+1;
        if(!canvas.segment({column,header_divider},{column,box.bottom()}))return false;
    }
    return row==box.bottom();
}
bool draw_entity(Canvas& canvas,const RelationEntity& entity,const RelationBox& box,bool expanded_empty,const StateTextRecord* state_text=nullptr) {
    if(!canvas.work.spend(sizeof(EntityShape)+1u))return false;
    if(entity.shape==EntityShape::Note){
        if(!canvas.work.spend(entity.label.size()*(8u*sizeof(std::string)+4u*sizeof(std::string_view)+32u)+256u))return false;
        const auto rows=label_rows(entity.label);
        if(box.w<std::max(4,label_width(entity.label))+4||box.h!=static_cast<int>(rows.size())+3||
           !draw_box(canvas,box,false)||!canvas.text({box.x+2,box.y+1},"note"))return false;
        int row=box.y+2;
        for(const auto text:rows)if(!canvas.text({box.x+2,row++},text))return false;
        return true;
    }
    if(!entity.er_attributes.empty())return draw_er_table(canvas,entity,box);
    if (entity.shape==EntityShape::Start||entity.shape==EntityShape::Final) return canvas.put({box.x,box.y},entity.shape==EntityShape::Start?"●":"◉");
    if (entity.shape==EntityShape::Fork||entity.shape==EntityShape::Join) {
        for (int x=box.x;x<=box.right();++x) if (!canvas.put({x,box.y},"━")) return false;
        return true;
    }
    if (entity.shape==EntityShape::Choice) {
        const int middle=box.y+2;
        if (!canvas.put({box.x,middle},"◇")||!canvas.put({box.right(),middle},"◇")) return false;
        for (int n=1;n<=2;++n) if (!canvas.put({box.x+n,middle-n},"╱")||!canvas.put({box.x+n,middle+n},"╲")||!canvas.put({box.right()-n,middle-n},"╲")||!canvas.put({box.right()-n,middle+n},"╱")) return false;
        if (!canvas.segment({box.x+3,box.y},{box.right()-3,box.y})||!canvas.segment({box.x+3,box.bottom()},{box.right()-3,box.bottom()})) return false;
        for (int x=box.x+3;x<=box.right()-3;++x) {
            canvas.at({x,box.y}).bits=kLeft|kRight;
            canvas.at({x,box.bottom()}).bits=kLeft|kRight;
        }
        return canvas.text({box.x+(box.w-label_width(entity.label))/2,middle},entity.label);
    }
    if (label_width(entity.label)>box.w-2 || static_cast<int>(label_rows(entity.label).size())>box.h-2) return false;
    if (!draw_box(canvas,box,entity.shape==EntityShape::Rounded)) return false;
    int row=box.y+1;
    if (!canvas.multiline({box.x+1,row},entity.label,box.w-2)) return false;
    row+=static_cast<int>(label_rows(entity.label).size());
    if (entity.shape==EntityShape::Group) return true;
    if (!entity.annotation.empty()&&!canvas.multiline({box.x+1,row++},entity.annotation,box.w-2)) return false;
    if(state_text&&!state_text->body.empty()){
        if(!canvas.work.spend(12u*sizeof(int)+32u)||entity.shape!=EntityShape::Rounded||!entity.members.empty()||!entity.annotation.empty()||row>=box.bottom()||state_text->body.size()>static_cast<std::size_t>(box.bottom()-row-1))return false;
        if(!canvas.segment({box.x,row},{box.right(),row}))return false;
        ++row;
        for(const auto& text:state_text->body){
            if(!canvas.work.spend(text.size()*(4u*sizeof(std::string)+16u)+128u)||!canvas.text({box.x+2,row++},text))return false;
        }
        return true;
    }
    if(expanded_empty&&entity.members.empty()) {
        // D1/attribute blank/D2/method blank, followed by a separate clear row.
        if(row>box.bottom()-5||!canvas.work.spend(8u))return false;
        if(!canvas.segment({box.x,row},{box.right(),row})||!canvas.segment({box.x,row+2},{box.right(),row+2}))return false;
        return true;
    }
    bool first=true,previous=false;
    for (const auto& member:entity.members) {
        if (first||previous!=member.method) { if (!canvas.segment({box.x,row},{box.right(),row})) return false; ++row; }
        first=false;previous=member.method;
        if (!canvas.text({box.x+2,row++},member.text)) return false;
    }
    return true;
}
std::vector<EndpointPort> ports_for(const RelationEntity& entity,const RelationBox& box,Marker marker,const RouteSearch& search) {
    std::vector<EndpointPort> ports;
    auto add=[&](Point border,Point normal) {
        if (er_cardinality_marker(marker)&&normal.y) return;
        const int size=marker_size(marker); const Point route{border.x+normal.x*(size+1),border.y+normal.y*(size+1)};
        for (int n=1;n<=size+1;++n) if (!search.free({border.x+normal.x*n,border.y+normal.y*n})) return;
        ports.push_back({border,normal,route,marker});
    };
    if (entity.shape==EntityShape::Start||entity.shape==EntityShape::Final) { for (const auto step:kSteps) add({box.x,box.y},step); }
    else if (entity.shape==EntityShape::Fork||entity.shape==EntityShape::Join) {
        for (int x=box.x+1;x<box.right();++x) { add({x,box.y},{0,-1});add({x,box.y},{0,1}); }
        add({box.x,box.y},{-1,0});add({box.right(),box.y},{1,0});
    } else if (entity.shape==EntityShape::Choice) {
        add({box.x,box.y+2},{-1,0});add({box.right(),box.y+2},{1,0});
        for (int x=box.x+3;x<=box.right()-3;++x) { add({x,box.y},{0,-1});add({x,box.bottom()},{0,1}); }
    } else {
        for (int y=box.y+1;y<box.bottom();++y) { add({box.x,y},{-1,0});add({box.right(),y},{1,0}); }
        for (int x=box.x+1;x<box.right();++x) { add({x,box.y},{0,-1});add({x,box.bottom()},{0,1}); }
    }
    return ports;
}
bool paint_path(Canvas& canvas,std::span<const Point> path,Point before,Point after,bool dotted,const CompoundScopeRoute* compound_route=nullptr) {
    for (std::size_t n=0;n<path.size();++n) {
        const Point p=path[n],previous=n?path[n-1]:before,next=n+1<path.size()?path[n+1]:after;
        const unsigned route_bits=direction_bits(previous-p)|direction_bits(next-p);
        const auto structural=compound_route?compound_route->structural(p):0u;
        const unsigned bits=route_bits|structural;
        auto glyph=line_glyph(bits);
        if (dotted&&bits==(kLeft|kRight)) glyph="┄";
        if (dotted&&bits==(kUp|kDown)) glyph="┆";
        if (!canvas.put(p,glyph)) return false;
        canvas.at(p).bits=bits;
    }
    return true;
}
bool paint_port(Canvas& canvas,const EndpointPort& port,bool attach_er=false) {
    const int size=marker_size(port.marker);
    // Perimeter geometry keeps its genuine corner family. A markerless
    // relationship adds exactly the used normal to its selected port.
    if (port.marker==Marker::None||attach_er) {
        auto& cell=canvas.at(port.border);
        if (cell.bits) { cell.bits|=direction_bits(port.normal);if (!canvas.put(port.border,line_glyph(cell.bits))) return false; }
    }
    for (int n=1;n<=size;++n) {
        std::string_view glyph;
        if (size==1) glyph=marker_glyph(port.marker,Point{}-port.normal);
        else if (port.marker==Marker::One) glyph="|";
        else if (port.marker==Marker::ZeroOne) glyph=n==1?"o":"|";
        else if (port.marker==Marker::ZeroMany) glyph=n==1?"o":port.normal.x>0?"{":"}";
        else glyph=n==1?"|":port.normal.x>0?"{":"}";
        if (!canvas.put({port.border.x+n*port.normal.x,port.border.y+n*port.normal.y},glyph)) return false;
    }
    return true;
}
struct LabelPlacement { Point left,right; int x=0,y=0,w=0,h=0; };
struct LabelCursor { int radius=0,dy=0;bool negative=false; };
std::optional<LabelPlacement> label_place(Canvas& canvas,const RouteSearch& search,const Relationship& edge,const EndpointPort& from,const EndpointPort& to,std::span<const int> regions,int region,LabelCursor& cursor,const std::array<Point,2>* body_anchors=nullptr) {
    // The selected-page path repeats label measurement; prepay its temporary
    // row/string work each time, without adding work to the disabled path.
    if(search.reciprocal_face&&!canvas.work.spend(edge.label.size()*4+4))return {};
    if(body_anchors&&!canvas.work.spend(edge.label.size()*(8u*sizeof(std::string_view)+4u)+4u))return {};
    const int w=label_width(edge.label),h=static_cast<int>(label_rows(edge.label).size());
    Point middle{(from.route.x+to.route.x)/2,(from.route.y+to.route.y)/2};
    if(body_anchors){middle={(body_anchors->front().x+body_anchors->back().x)/2,(body_anchors->front().y+body_anchors->back().y)/2};}
    if(!search.state_scope_direction.empty()&&!canvas.work.spend(8u))return {};
    const bool horizontal=search.state_scope_direction.empty()?(search.layout.diagram.direction=="LR"||search.layout.diagram.direction=="RL"):(search.state_scope_direction=="LR"||search.state_scope_direction=="RL");
    const bool backward=horizontal?to.border.x<=from.border.x:to.border.y<=from.border.y;
    if(search.reciprocal_face) {
        if(!canvas.work.spend())return {};
        const int first=search.reciprocal_face<0?search.reciprocal_inset:search.reciprocal_spine+1;
        const int last=search.reciprocal_face<0?search.reciprocal_spine-1:canvas.width-1-search.reciprocal_inset;
        middle.x=(first+last)/2;
    } else if (backward&&!horizontal) middle.x=std::max(from.route.x,to.route.x)+w/2+2;
    // Search compact nearby label positions first; an explicit box reservation
    // prevents routes from silently traversing a multiline label's blank rows.
    while (cursor.radius<std::max(canvas.width,canvas.height)) {
        const int dy=cursor.dy,dx=cursor.radius-std::abs(dy),sign=cursor.negative?-1:1;
        if (!cursor.negative) cursor.negative=true;
        else { cursor.negative=false;if (++cursor.dy>cursor.radius) { ++cursor.radius;cursor.dy=-cursor.radius; } }
        if (!dx&&sign<0) continue;
        if (!canvas.work.spend()) return {};
        const int x=middle.x-w/2+sign*dx,y=middle.y+dy;
        const Point left{x-1,y},right{x+w,y};
        if (!canvas.inside(left)||!canvas.inside(right)) continue;
        if(search.reciprocal_face) {
            if(!canvas.work.spend())return {};
            if(search.reciprocal_face<0?x+w+1>=search.reciprocal_spine:x-2<=search.reciprocal_spine)continue;
        }
        if (!regions.empty()&&(regions[static_cast<std::size_t>(canvas.index(left))]!=region||regions[static_cast<std::size_t>(canvas.index(right))]!=region)) continue;
        bool okay=true;
        for (int row=y-1;okay&&row<=y+h;++row) for (int col=x-2;col<=x+w+1;++col) {
            if (!canvas.work.spend()||!search.free({col,row})||(search.compound_route&&search.compound_route->structural({col,row}))) { okay=false;break; }
        }
        if (okay) return LabelPlacement{left,right,x,y,w,h};
    }
    return {};
}
// Source-owned endpoint rectangles interrupt the actual selected route. They
// never replace or concatenate the optional central business label.
bool connect_class_caption_pair(Canvas& canvas,RouteSearch& search,const Relationship& edge,const EndpointPort& p,const EndpointPort& q,const ReciprocalEdgePlan* planned,int spine) {
    const auto* source=class_caption(edge,true);const auto* target=class_caption(edge,false);
    if((!source&&!target)||(source&&!p.normal.x)||(target&&!q.normal.x)||search.compound_reservations)return false;
    struct Caption {const ClassEndpointText* text=nullptr;LabelPlacement box{};Point near{},far{};};
    if(!canvas.work.spend(2u*sizeof(Caption)+2u*sizeof(std::array<Point,2>)+64u))return false;
    auto endpoint=[&](const ClassEndpointText* text,const EndpointPort& port)->Caption {
        if(!text)return {};
        const Point near=port.route+port.normal,far{port.route.x+port.normal.x*(text->w+2),port.route.y};
        const int left=std::min(near.x,far.x);
        return {text,{{left,near.y},{left+text->w+1,near.y},left+1,near.y,text->w,text->h},near,far};
    };
    const Caption from=endpoint(source,p),to=endpoint(target,q);
    auto overlap=[](const LabelPlacement& a,const LabelPlacement& b) {
        return a.x-2<=b.x+b.w+1&&b.x-2<=a.x+a.w+1&&a.y-1<=b.y+b.h&&b.y-1<=a.y+a.h;
    };
    auto contains=[](const LabelPlacement& box,Point point) {
        return point.x>=box.x-2&&point.x<=box.x+box.w+1&&point.y>=box.y-1&&point.y<=box.y+box.h;
    };
    if(source&&target&&overlap(from.box,to.box))return false;
    auto physical_free=[&](Point point) {
        if(!canvas.work.spend(search.layout.diagram.entities.size()+2))return false;
        return search.free(point);
    };
    // Check complete physical endpoint clearance before any page clipping.
    for(const auto* caption:{&from,&to})if(caption->text) {
        const auto& box=caption->box;
        if(box.w<1||box.w>canvas.width-4||box.h<1||box.h>canvas.height-2||
           box.x<2||box.x+box.w+1>=canvas.width||box.y<1||box.y+box.h>=canvas.height)return false;
        for(int y=box.y-1;y<=box.y+box.h;++y)for(int x=box.x-2;x<=box.x+box.w+1;++x) {
            const Point point{x,y};
            if(!physical_free(point)||(search.compound_route&&search.compound_route->structural(point)))return false;
        }
    }
    // A bare opposite endpoint's marker is also a real role, even though it
    // has no caption rectangle. No text clearance may consume that marker.
    for(const auto& port:{p,q})for(int n=1;n<=marker_size(port.marker);++n) {
        if(!canvas.work.spend(3))return false;
        const Point point{port.border.x+port.normal.x*n,port.border.y+port.normal.y*n};
        if((source&&contains(from.box,point))||(target&&contains(to.box,point))||!physical_free(point))return false;
    }
    const Point source_outer=source?from.far:p.route,target_outer=target?to.far:q.route;
    const Point start=planned?Point{spine,p.route.y}:source_outer,goal=planned?Point{spine,q.route.y}:target_outer;
    if(start==goal)return false;
    auto connector=[&](Point first,Point last,const Caption* foreign)->std::optional<std::vector<Point>> {
        if(first.y!=last.y||!canvas.inside(first)||!canvas.inside(last))return {};
        const auto count=static_cast<std::size_t>(std::abs(last.x-first.x))+1;
        if(count>static_cast<std::size_t>(canvas.width)||!canvas.work.spend(count*(sizeof(Point)+4u)+sizeof(std::vector<Point>)))return {};
        std::vector<Point> points;points.reserve(count);const int direction=last.x>=first.x?1:-1;
        for(std::size_t n=0;n<count;++n) {
            if(!canvas.work.spend())return {};
            const Point point{first.x+static_cast<int>(n)*direction,first.y};
            if((foreign->text&&contains(foreign->box,point))||!physical_free(point)||(n&& !search.step(points.back(),point)))return {};
            points.push_back(point);
        }
        return points;
    };
    // A fixed outward continuation cannot double back through its own text.
    if(planned&&((source&&(start.x-source_outer.x)*p.normal.x<1)||(target&&(goal.x-target_outer.x)*q.normal.x<1)))return false;
    const auto prefix=connector(source_outer,start,&to),suffix=connector(goal,target_outer,&from);
    if(!prefix||!suffix)return false;
    const std::array<Point,2> source_near{p.route,source?from.near:p.route},target_near{target?to.near:q.route,q.route};
    if(source)for(const auto point:source_near)if((target&&contains(to.box,point))||!physical_free(point))return false;
    if(target)for(const auto point:target_near)if((source&&contains(from.box,point))||!physical_free(point))return false;
    struct Reservations {
        RouteSearch& search;const std::vector<Point>* before;
        std::vector<Point> cells;std::vector<unsigned char> previous;
        ~Reservations(){release(0);search.compound_reservations=before;}
        void release(std::size_t keep) {
            while(cells.size()>keep){const Point point=cells.back();search.canvas.at(point).blocked=previous.back()!=0;cells.pop_back();previous.pop_back();}
        }
        bool add(Point point) {
            auto& canvas=search.canvas;
            if(!canvas.inside(point))return false;
            for(const auto own:cells){if(!canvas.work.spend())return false;if(own==point)return true;}
            if(!search.free(point)||!canvas.work.spend(sizeof(Point)+sizeof(unsigned char)+8u))return false;
            if(cells.size()==cells.capacity()) {
                if(cells.size()>=canvas.cells.size())return false;
                const auto capacity=std::min(canvas.cells.size(),std::max(std::size_t{1},cells.size()*2));
                if(!canvas.work.spend((capacity+cells.size())*(sizeof(Point)+sizeof(unsigned char))+4u))return false;
                cells.reserve(capacity);previous.reserve(capacity);
            }
            previous.push_back(static_cast<unsigned char>(canvas.at(point).blocked));cells.push_back(point);canvas.at(point).blocked=true;return true;
        }
    };
    if(!canvas.work.spend(sizeof(Reservations)))return false;
    Reservations reservations{search,search.compound_reservations,{},{}};
    search.compound_reservations=&reservations.cells;
    auto reserve=[&](Point point){return point==start||point==goal||reservations.add(point);};
    for(const auto& port:{p,q})for(int n=1;n<=marker_size(port.marker);++n) {
        if(!canvas.work.spend()||!reserve({port.border.x+port.normal.x*n,port.border.y+port.normal.y*n}))return false;
    }
    for(const auto* caption:{&from,&to})if(caption->text) {
        const auto& box=caption->box;
        for(int y=box.y-1;y<=box.y+box.h;++y)for(int x=box.x-1;x<=box.x+box.w;++x) {
            const Point point{x,y};if(!canvas.work.spend())return false;
            if(point==caption->near||point==caption->far)continue;
            if(point==start||point==goal||!reserve(point))return false;
        }
    }
    if(source)for(const auto point:source_near)if(!reserve(point))return false;
    if(target)for(const auto point:target_near)if(!reserve(point))return false;
    for(const auto points:{std::span<const Point>(*prefix),std::span<const Point>(*suffix)})for(const auto point:points)if(!reserve(point))return false;
    struct PageScope {
        RouteSearch& search;int face,spine,inset;Point start,goal;
        ~PageScope(){search.reciprocal_face=face;search.reciprocal_spine=spine;search.reciprocal_inset=inset;search.reciprocal_start=start;search.reciprocal_goal=goal;}
    };
    if(!canvas.work.spend(sizeof(PageScope)))return false;
    PageScope page{search,search.reciprocal_face,search.reciprocal_spine,search.reciprocal_inset,search.reciprocal_start,search.reciprocal_goal};
    if(planned){search.reciprocal_face=planned->face;search.reciprocal_spine=spine;search.reciprocal_inset=planned->inset;search.reciprocal_start=start;search.reciprocal_goal=goal;}
    auto joined=[&](std::span<const Point> before,std::span<const Point> middle,std::span<const Point> after)->std::optional<std::vector<Point>> {
        if(middle.empty()||(!before.empty()&&before.back()!=middle.front())||(!after.empty()&&after.front()!=middle.back()))return {};
        const auto limit=canvas.cells.size();
        if(before.size()>limit||middle.size()>limit||after.size()>limit||before.size()>std::numeric_limits<std::size_t>::max()-middle.size()||before.size()+middle.size()>std::numeric_limits<std::size_t>::max()-after.size())return {};
        const auto count=before.size()+middle.size()+after.size()-(before.empty()?0u:1u)-(after.empty()?0u:1u);
        if(count>canvas.cells.size()||!canvas.work.spend(count*(sizeof(Point)+2u)+sizeof(std::vector<Point>)))return {};
        std::vector<Point> result;result.reserve(count);
        for(const auto point:before){if(!canvas.work.spend())return {};result.push_back(point);}
        for(std::size_t n=before.empty()?0:1;n<middle.size();++n){if(!canvas.work.spend())return {};result.push_back(middle[n]);}
        for(std::size_t n=after.empty()?0:1;n<after.size();++n){if(!canvas.work.spend())return {};result.push_back(after[n]);}
        return result;
    };
    const Point before=source?source_outer-p.normal:p.route-p.normal,after=target?target_outer-q.normal:q.route-q.normal;
    auto paint_roles=[&](const LabelPlacement* central)->bool {
        for(const auto* caption:{source,target})if(caption&&!canvas.work.spend(caption->text.size()*(4u*sizeof(std::string_view)+4u*sizeof(std::string)+4u)+16u))return false;
        if(central&&!canvas.work.spend(edge.label.size()*(4u*sizeof(std::string_view)+4u*sizeof(std::string)+4u)+16u))return false;
        if(source&&(!paint_path(canvas,source_near,p.route-p.normal,from.near+p.normal,edge.dotted,search.compound_route)||!canvas.multiline({from.box.x,from.box.y},source->text,source->w)))return false;
        if(target&&(!paint_path(canvas,target_near,to.near+q.normal,q.route-q.normal,edge.dotted,search.compound_route)||!canvas.multiline({to.box.x,to.box.y},target->text,target->w)))return false;
        if(central&&!canvas.multiline({central->x,central->y},edge.label,central->w))return false;
        if(!paint_port(canvas,p,false)||!paint_port(canvas,q,false))return false;
        for(const auto* box:{source?&from.box:nullptr,target?&to.box:nullptr,central})if(box) {
            const auto width=static_cast<std::size_t>(box->w+2),height=static_cast<std::size_t>(box->h+2);
            if(width>static_cast<std::size_t>(canvas.width)||height>static_cast<std::size_t>(canvas.height)||!canvas.work.spend(width*height))return false;
            for(int y=box->y-1;y<=box->y+box->h;++y)for(int x=box->x-1;x<=box->x+box->w;++x)canvas.at({x,y}).blocked=true;
        }
        return true;
    };
    if(edge.label.empty()) {
        const auto path=search.path(start,goal);if(!path)return false;
        const auto complete=joined(*prefix,*path,*suffix);if(!complete)return false;
        // No temporary destructor may clear newly painted persistent cells.
        reservations.release(0);
        if(!paint_path(canvas,*complete,before,after,edge.dotted,search.compound_route))return false;
        return paint_roles(nullptr);
    }
    if(!search.path(start,goal))return false;
    LabelCursor cursor;const std::array<Point,2> body_anchors{start,goal};
    while(const auto label=label_place(canvas,search,edge,p,q,{},-1,cursor,&body_anchors)) {
        if((source&&overlap(label.value(),from.box))||(target&&overlap(label.value(),to.box)))continue;
        const auto base=reservations.cells.size();
        struct CandidateScope {Reservations& reservations;std::size_t base;~CandidateScope(){reservations.release(base);}};
        if(!canvas.work.spend(sizeof(CandidateScope)))return false;
        CandidateScope candidate_scope{reservations,base};
        bool allowed=true;
        for(int y=label->y-1;allowed&&y<=label->y+label->h;++y)for(int x=label->x-1;x<=label->x+label->w;++x) {
            const Point point{x,y};if(!canvas.work.spend())return false;
            if(point==label->left||point==label->right)continue;
            if(point==start||point==goal){allowed=false;break;}
            if(!reserve(point))return false;
        }
        if(!allowed)continue;
        if(!canvas.work.spend(8))return false;
        const auto distance=[](Point a,Point b){return std::abs(a.x-b.x)+std::abs(a.y-b.y);};
        bool reverse=goal.y<start.y;
        const int forward=distance(start,label->left)+distance(label->right,goal),backward=distance(start,label->right)+distance(label->left,goal);
        if(forward!=backward)reverse=backward<forward;
        const Point entrance=reverse?label->right:label->left,exit=reverse?label->left:label->right,across=reverse?Point{-1,0}:Point{1,0};
        // The actual body goal must use the authoritative dynamic reservation
        // list as well as blocked, including namespace structural exemptions.
        const auto target_base=reservations.cells.size();
        if(goal!=entrance&&!reservations.add(goal))return false;
        const auto left=search.path(start,entrance);reservations.release(target_base);
        if(!left)continue;
        for(const auto point:*left)if(!reservations.add(point))return false;
        const auto right=search.path(exit,goal);if(!right)continue;
        const auto first=joined(*prefix,*left,{}),last=joined({},*right,*suffix);if(!first||!last)return false;
        // paint_path needs the actual one-cell direction across the central gap;
        // keep placement contacts untouched and supply that direction explicitly.
        reservations.release(0);
        if(!paint_path(canvas,*first,before,entrance+across,edge.dotted,search.compound_route)||!paint_path(canvas,*last,exit-across,after,edge.dotted,search.compound_route))return false;
        return paint_roles(&*label);
    }
    return false;
}

bool connect_relation(Canvas& canvas,RouteSearch& search,const RelationDiagram& diagram,const RelationLayout& layout,const Relationship& edge) {
    if(!canvas.work.spend(4u*sizeof(bool)+16u))return false;
    const bool note_leader=edge.kind==RelationshipKind::ClassNoteLeader;
    if(note_leader){
        const auto& a=layout.boxes[static_cast<std::size_t>(edge.from)];const auto& b=layout.boxes[static_cast<std::size_t>(edge.to)];
        if(a.w<3||a.h<3||b.w<3||b.h<3||a.w>kWidthLimit||b.w>kWidthLimit||a.h>kHeightLimit||b.h>kHeightLimit)return false;
        const auto ports=static_cast<std::size_t>(2*(a.w+a.h+b.w+b.h));
        if(!canvas.work.spend(ports*(4u*sizeof(EndpointPort)+16u)+256u))return false;
    }
    auto from=ports_for(diagram.entities[static_cast<std::size_t>(edge.from)],layout.boxes[static_cast<std::size_t>(edge.from)],edge.start,search);
    auto to=ports_for(diagram.entities[static_cast<std::size_t>(edge.to)],layout.boxes[static_cast<std::size_t>(edge.to)],edge.end,search);
    const bool header_self=diagram.family==RelationFamily::Er&&edge.from==edge.to&&diagram.entities[static_cast<std::size_t>(edge.from)].er_attributes.empty();
    int self_label_room=0;
    if (header_self) {
        if (!canvas.work.spend(edge.label.size()+1)) return false;
        if (!edge.label.empty()) self_label_room=label_width(edge.label)+4;
    }
    std::optional<std::vector<int>> regions;
    // Class and ER connectors prove endpoint and label connectivity with the
    // actual split A* paths below. State retains its free-region prefilter.
    if (diagram.family==RelationFamily::State && !edge.label.empty()) { regions=free_regions(search);if (!regions) return false; }
    // Every perimeter has at most 2*(bounded width+height) ports. Store only
    // the necessary index width; the score and complete candidate order stay.
    using PortIndex=std::uint16_t;
    static_assert(2*(kWidthLimit+kHeightLimit)<std::numeric_limits<PortIndex>::max());
    if(!canvas.work.spend(2u*sizeof(std::size_t)+8u))return false;
    if(from.size()>std::numeric_limits<PortIndex>::max()||to.size()>std::numeric_limits<PortIndex>::max())return false;
    struct Pair { PortIndex a,b;int distance; };
    std::vector<Pair> pairs;
    if(note_leader){
        const auto count=from.size()*to.size();
        // Backing allocation plus each temporary and retained Pair. Existing
        // per-pair generation visits remain charged below.
        if(!canvas.work.spend(count*(4u*sizeof(Pair)+16u)+4u*sizeof(std::vector<Pair>)+128u))return false;
        pairs.reserve(count);
    }
    const auto& a_box=layout.boxes[static_cast<std::size_t>(edge.from)];const auto& b_box=layout.boxes[static_cast<std::size_t>(edge.to)];
    const Point delta{b_box.x+b_box.right()-a_box.x-a_box.right(),b_box.y+b_box.bottom()-a_box.y-a_box.bottom()};
    Point requirement_normal{};bool requirement_forward=false;
    if(diagram.family==RelationFamily::Requirement) {
        // Direction and committed box separation are invariant across all
        // perimeter pairs. Pay and compute them once for this connector.
        if(!canvas.work.spend(16u*sizeof(int)+8u*sizeof(bool)+64u))return false;
        if(edge.from!=edge.to) {
            if(diagram.direction=="LR"&&b_box.x>a_box.right()){requirement_normal={1,0};requirement_forward=true;}
            else if(diagram.direction=="RL"&&b_box.right()<a_box.x){requirement_normal={-1,0};requirement_forward=true;}
            else if(diagram.direction=="TB"&&b_box.y>a_box.bottom()){requirement_normal={0,1};requirement_forward=true;}
            else if(diagram.direction=="BT"&&b_box.bottom()<a_box.y){requirement_normal={0,-1};requirement_forward=true;}
            else if((diagram.direction=="LR"||diagram.direction=="RL")&&
                    a_box.x<=b_box.right()&&b_box.x<=a_box.right()&&b_box.y>a_box.bottom()) {
                // Horizontal SCC columns stack their members vertically. Route
                // each descending edge across the actual gap; upward cycle
                // closures keep the existing exterior-face search.
                requirement_normal={0,1};requirement_forward=true;
            }
        }
    }
    for (std::size_t a=0;a<from.size();++a) for (std::size_t b=0;b<to.size();++b) {
        if (!canvas.work.spend()) return false;
        const auto& p=from[a];const auto& q=to[b];
        if(requirement_forward) {
            if(!canvas.work.spend(4u))return false;
            if(p.normal!=requirement_normal||q.normal!=Point{-requirement_normal.x,-requirement_normal.y})continue;
        }
        if (p.route==q.route||p.border==q.border) continue;
        if (header_self) {
            if (!p.normal.x||p.normal!=q.normal) continue;
            const int room=p.normal.x>0?canvas.width-1-a_box.right():a_box.x;
            const int required=std::max({marker_size(p.marker)+1,marker_size(q.marker)+1,self_label_room});
            if (room<required) continue;
        }
        if (regions&&(*regions)[static_cast<std::size_t>(canvas.index(p.route))]!=(*regions)[static_cast<std::size_t>(canvas.index(q.route))]) continue;
        int score=std::abs(p.route.x-q.route.x)+std::abs(p.route.y-q.route.y);
        score+=std::abs(p.border.x*2-a_box.x-a_box.right())+std::abs(p.border.y*2-a_box.y-a_box.bottom());
        score+=std::abs(q.border.x*2-b_box.x-b_box.right())+std::abs(q.border.y*2-b_box.y-b_box.bottom());
        if (p.normal.x*delta.x+p.normal.y*delta.y<0) score+=8;
        if (q.normal.x*delta.x+q.normal.y*delta.y>0) score+=8;
        if (diagram.family==RelationFamily::Class) {
            if (p.normal.x && p.border.y!=a_box.bottom()-1) score+=64;
            if (q.normal.x && q.border.y!=b_box.bottom()-1) score+=64;
        }
        if(!search.state_scope_direction.empty()&&!canvas.work.spend(8u))return false;
        const bool horizontal=search.state_scope_direction.empty()?(diagram.direction=="LR"||diagram.direction=="RL"):(search.state_scope_direction=="LR"||search.state_scope_direction=="RL");
        const bool reverse_rank=diagram.family==RelationFamily::Class && edge.end==Marker::Triangle && edge.start!=Marker::Triangle;
        const bool backward=horizontal?(reverse_rank?delta.x>=0:delta.x<=0):(reverse_rank?delta.y>=0:delta.y<=0);
        if (backward) {
            const Point outside=horizontal?Point{0,1}:Point{1,0};
            if (p.normal!=outside) score+=64;
            if (q.normal!=outside) score+=64;
        }
        pairs.push_back({static_cast<PortIndex>(a),static_cast<PortIndex>(b),score});
    }
    if(note_leader){
        if(pairs.size()>1u){
            if(!canvas.work.spend(pairs.size()*(8u*sizeof(int)+16u)+4u*sizeof(int)+4u*sizeof(std::size_t)+128u))return false;
            int minimum=pairs.front().distance,maximum=minimum;
            for(const auto& pair:pairs){minimum=std::min(minimum,pair.distance);maximum=std::max(maximum,pair.distance);}
            // Each coordinate lies inside the bounded canvas. Manhattan and
            // both centering terms total at most 3*(width+height); orientation
            // adds at most 16, Class row bias 128 and backward bias 128.
            constexpr int score_limit=3*(kWidthLimit+kHeightLimit)+16+128+128;
            if(minimum<0||maximum>score_limit)return false;
            const auto span=static_cast<std::size_t>(maximum-minimum)+1u,count=pairs.size();
            // Pay count/offset storage and visits, prefix work, sorted backing
            // allocation/value initialization, full Pair reads/writes and swap
            // before creating either buffer. All candidates remain present.
            if(!canvas.work.spend(span*(6u*sizeof(std::size_t)+16u)+
                count*(4u*sizeof(Pair)+4u*sizeof(std::size_t)+16u)+
                8u*sizeof(std::vector<Pair>)+4u*sizeof(std::vector<std::size_t>)+128u))return false;
            std::vector<std::size_t> next(span,0u);
            for(const auto& pair:pairs)++next[static_cast<std::size_t>(pair.distance-minimum)];
            std::size_t offset=0;
            for(auto& slot:next){const auto frequency=slot;slot=offset;offset+=frequency;}
            std::vector<Pair> ordered(count);
            // Enumerating the input in its original order keeps equal scores
            // stable, exactly as the existing comparison sort does.
            for(const auto& pair:pairs)ordered[next[static_cast<std::size_t>(pair.distance-minimum)]++]=pair;
            pairs.swap(ordered);
        }
    }else std::ranges::stable_sort(pairs,{},&Pair::distance);
    for (const auto pair:pairs) {
        if (!canvas.work.spend()) return false;
        const auto p=from[pair.a],q=to[pair.b];
        // A self loop may surround exterior space, never its own source box.
        // The scope guard also clears clipping on every failed pair/return.
        struct SelfFaceScope {
            RouteSearch& search;bool active;
            ~SelfFaceScope() { if (active) { search.self_face_normal=0;search.self_face_border=0; } }
        } self_face{search,header_self};
        if (header_self) { search.self_face_normal=p.normal.x;search.self_face_border=p.border.x; }
        if(edge.endpoint_texts) {
            if(connect_class_caption_pair(canvas,search,edge,p,q,nullptr,0))return true;
            if(canvas.work.remaining==0)return false;
            continue;
        }
        std::vector<Point> reservations;
        struct CompoundReservationScope {
            RouteSearch& search;const std::vector<Point>* before;bool active;
            ~CompoundReservationScope(){if(active)search.compound_reservations=before;}
        } compound_reservation_scope{search,search.compound_reservations,search.compound_route!=nullptr};
        if(search.compound_route)search.compound_reservations=&reservations;
        auto unreserve=[&](Point point){canvas.at(point).blocked=search.compound_route&&search.compound_route->structural(point)!=0;};
        auto reserve=[&](Point point) { if (point==p.route||point==q.route) return; if (search.free(point)) { canvas.at(point).blocked=true;reservations.push_back(point); } };
        for (const auto& port:{p,q}) for (int n=1;n<=marker_size(port.marker);++n) reserve({port.border.x+port.normal.x*n,port.border.y+port.normal.y*n});
        if(requirement_forward) {
            // A forward arrow/marker has a straight exterior stem before the
            // body can bend. Temporary reservations use existing cleanup.
            if(!canvas.work.spend(4u*(2u*sizeof(Point)+16u)+64u))return false;
            for(const auto& port:{p,q}) {
                const Point side{-port.normal.y,port.normal.x};
                reserve(port.route+side);reserve(port.route-side);
            }
        }
        if (edge.label.empty()) {
            const auto path=search.path(p.route,q.route);
            for (const auto point:reservations) unreserve(point);
            if (!path) continue;
            if (!paint_path(canvas,*path,p.route-p.normal,q.route-q.normal,edge.dotted,search.compound_route)||!paint_port(canvas,p,diagram.family==RelationFamily::Er)||!paint_port(canvas,q,diagram.family==RelationFamily::Er)) return false;
            return true;
        }
        if (!search.path(p.route,q.route)) { for (const auto point:reservations) unreserve(point);continue; }
        LabelCursor cursor;
        while (const auto label=label_place(canvas,search,edge,p,q,regions?std::span<const int>(*regions):std::span<const int>{},regions?(*regions)[static_cast<std::size_t>(canvas.index(p.route))]:-1,cursor)) {
            const auto marker_reservations=reservations.size();
            for (int y=label->y-1;y<=label->y+label->h;++y) for (int x=label->x-1;x<=label->x+label->w;++x) {
                const Point point{x,y};if (point==label->left||point==label->right) continue;reserve(point);
            }
            bool reverse_label=q.border.y<p.border.y;
            if(diagram.family==RelationFamily::Er&&edge.from!=edge.to&&p.normal.x&&
               (p.normal==q.normal||layout.ranked_peers)&&
               diagram.entities[static_cast<std::size_t>(edge.from)].er_attributes.empty()&&
               diagram.entities[static_cast<std::size_t>(edge.to)].er_attributes.empty()){
                // A peer-rank plan can expose opposite endpoint faces. Choose
                // label direction from actual ports; legacy ties stay unchanged.
                if(!canvas.work.spend(8))return false;
                const auto distance=[](Point a,Point b){return std::abs(a.x-b.x)+std::abs(a.y-b.y);};
                const int forward_cost=distance(p.route,label->left)+distance(label->right,q.route);
                const int reverse_cost=distance(p.route,label->right)+distance(label->left,q.route);
                if(forward_cost!=reverse_cost)reverse_label=reverse_cost<forward_cost;
            }
            const Point entrance=reverse_label?label->right:label->left;
            const Point exit=reverse_label?label->left:label->right;
            const Point across=reverse_label?Point{-1,0}:Point{1,0};
            const bool reserve_target=q.route!=entrance;
            if (reserve_target) canvas.at(q.route).blocked=true;
            auto left=search.path(p.route,entrance);
            if (reserve_target) canvas.at(q.route).blocked=false;
            if (left) for (const auto point:*left) { canvas.at(point).blocked=true;reservations.push_back(point); }
            auto right=left?search.path(exit,q.route):std::nullopt;
            for (std::size_t n=marker_reservations;n<reservations.size();++n) unreserve(reservations[n]);
            reservations.resize(marker_reservations);
            if (!left||!right) continue;
            for (const auto point:reservations) unreserve(point);
            if (!paint_path(canvas,*left,p.route-p.normal,entrance+across,edge.dotted,search.compound_route)||!paint_path(canvas,*right,exit-across,q.route-q.normal,edge.dotted,search.compound_route)||!canvas.multiline({label->x,label->y},edge.label,label->w)||!paint_port(canvas,p,diagram.family==RelationFamily::Er)||!paint_port(canvas,q,diagram.family==RelationFamily::Er)) return false;
            // Labels own their complete compact rectangle, including padding
            // and explicit blank rows, for the rest of this layout.
            for (int y=label->y-1;y<=label->y+label->h;++y) for (int x=label->x-1;x<=label->x+label->w;++x) canvas.at({x,y}).blocked=true;
            return true;
        }
        for (const auto point:reservations) unreserve(point);
    }
    return false;
}
// Fixed source ports and real normal connectors reuse the same A*/label/paint
// primitives. The default all-pair connector above is deliberately unchanged.
bool connect_reciprocal_relation(Canvas& canvas,RouteSearch& search,const RelationDiagram& diagram,const RelationLayout& layout,const Relationship& edge,const ReciprocalPlan& plan,std::size_t edge_index) {
    if(edge_index>=plan.edges.size())return false;const auto& route=plan.edges[edge_index];
    const auto& a=layout.boxes[static_cast<std::size_t>(edge.from)];const auto& b=layout.boxes[static_cast<std::size_t>(edge.to)];
    const std::size_t candidates=2*static_cast<std::size_t>(a.w+a.h+b.w+b.h-8);
    // Prepay both original physical port scans and their vector growth/copies.
    // EndpointPort has three Points and one marker. Per-candidate scope checks
    // visit at most V child boxes and at most three marker/normal positions.
    if(!canvas.work.spend(candidates*(64+8*diagram.entities.size())+32))return false;
    const auto from=ports_for(diagram.entities[static_cast<std::size_t>(edge.from)],a,edge.start,search);
    const auto to=ports_for(diagram.entities[static_cast<std::size_t>(edge.to)],b,edge.end,search);
    auto selected=[&](const std::vector<EndpointPort>& ports,int row,std::optional<EndpointPort>& result) {
        for(const auto& port:ports) {
            if(!canvas.work.spend())return false;
            if(port.normal.x!=route.face||port.normal.y||port.border.y!=row)continue;
            if(result||!canvas.work.spend(7))return false;result=port;
        }
        return result.has_value();
    };
    std::optional<EndpointPort> from_port,to_port;
    if(!selected(from,route.from_row,from_port)||!selected(to,route.to_row,to_port))return false;
    const auto p=*from_port,q=*to_port;const int spine=route.face<0?plan.left_spine:plan.right_spine;
    if(edge.endpoint_texts)return connect_class_caption_pair(canvas,search,edge,p,q,&route,spine);
    const Point start{spine,p.route.y},goal{spine,q.route.y};
    if(start==goal)return false;
    auto connector=[&](Point first,Point last)->std::optional<std::vector<Point>> {
        if(first.y!=last.y||!canvas.inside(first)||!canvas.inside(last))return {};
        const std::size_t count=static_cast<std::size_t>(std::abs(last.x-first.x))+1;
        if(count>static_cast<std::size_t>(canvas.width)||!canvas.work.spend(count*(6+diagram.entities.size())+2))return {};
        std::vector<Point> points;points.reserve(count);const int step=last.x>=first.x?1:-1;
        for(std::size_t n=0;n<count;++n) {
            const Point point{first.x+static_cast<int>(n)*step,first.y};
            if(!canvas.work.spend()||!search.free(point))return {};points.push_back(point);
        }
        return points;
    };
    // All original physical checks happen before the active page clip, including
    // genuine normal route points between the entity and common spine.
    const auto prefix=connector(p.route,start);if(!prefix)return false;
    const auto suffix=connector(goal,q.route);if(!suffix)return false;
    struct OwnReservations {
        Canvas& canvas;std::vector<Point> cells;
        ~OwnReservations(){release(0);}
        void release(std::size_t keep){while(cells.size()>keep){const auto point=cells.back();cells.pop_back();canvas.at(point).blocked=false;}}
        bool add(Point point) {
            if(!canvas.inside(point)||canvas.at(point).blocked||!canvas.work.spend(8))return false;
            if(cells.size()==cells.capacity()) {
                if(cells.size()==canvas.cells.size())return false;
                const auto capacity=std::min(canvas.cells.size(),std::max(std::size_t{1},cells.size()*2));
                if(!canvas.work.spend(2*capacity+2*cells.size()))return false;cells.reserve(capacity);
            }
            cells.push_back(point);canvas.at(point).blocked=true;return true;
        }
    } reservations{canvas,{}};
    auto reserve=[&](Point point){return point==start||point==goal||reservations.add(point);};
    for(const auto& port:{p,q})for(int n=1;n<=marker_size(port.marker);++n)
        if(!reserve({port.border.x+port.normal.x*n,port.border.y+port.normal.y*n}))return false;
    for(const auto& points:{std::cref(*prefix),std::cref(*suffix)})for(const auto point:points.get())if(!reserve(point))return false;
    struct PageScope {
        RouteSearch& search;int old_face,old_spine,old_inset;Point old_start,old_goal;
        ~PageScope(){search.reciprocal_face=old_face;search.reciprocal_spine=old_spine;search.reciprocal_inset=old_inset;search.reciprocal_start=old_start;search.reciprocal_goal=old_goal;}
    } page{search,search.reciprocal_face,search.reciprocal_spine,search.reciprocal_inset,search.reciprocal_start,search.reciprocal_goal};
    search.reciprocal_face=route.face;search.reciprocal_spine=spine;search.reciprocal_inset=route.inset;search.reciprocal_start=start;search.reciprocal_goal=goal;
    auto joined=[&](std::span<const Point> before,const std::vector<Point>& middle,std::span<const Point> after)->std::optional<std::vector<Point>> {
        if(middle.empty()||(!before.empty()&&before.back()!=middle.front())||(!after.empty()&&after.front()!=middle.back()))return {};
        const auto count=before.size()+middle.size()+after.size()-(before.empty()?0:1)-(after.empty()?0:1);
        if(count>canvas.cells.size()||!canvas.work.spend(count*4+4))return {};
        std::vector<Point> result;result.reserve(count);
        for(const auto point:before){if(!canvas.work.spend())return {};result.push_back(point);}
        for(std::size_t n=before.empty()?0:1;n<middle.size();++n){if(!canvas.work.spend())return {};result.push_back(middle[n]);}
        for(std::size_t n=after.empty()?0:1;n<after.size();++n){if(!canvas.work.spend())return {};result.push_back(after[n]);}
        return result;
    };
    if(edge.label.empty()) {
        const auto body=search.path(start,goal);if(!body)return false;
        const auto complete=joined(*prefix,*body,*suffix);if(!complete)return false;
        reservations.release(0);
        return paint_path(canvas,*complete,p.route-p.normal,q.route-q.normal,edge.dotted)&&paint_port(canvas,p,diagram.family==RelationFamily::Er)&&paint_port(canvas,q,diagram.family==RelationFamily::Er);
    }
    if(!search.path(start,goal))return false;
    LabelCursor cursor;
    while(const auto label=label_place(canvas,search,edge,p,q,{},-1,cursor)) {
        const auto base=reservations.cells.size();
        struct LabelReservations {OwnReservations& own;std::size_t base;~LabelReservations(){own.release(base);}} label_scope{reservations,base};
        for(int y=label->y-1;y<=label->y+label->h;++y)for(int x=label->x-1;x<=label->x+label->w;++x) {
            const Point point{x,y};if(point==label->left||point==label->right)continue;
            if(!canvas.work.spend()||!reserve(point))return false;
        }
        bool reverse=goal.y<start.y;
        if(!canvas.work.spend(8))return false;
        const auto distance=[](Point first,Point last){return std::abs(first.x-last.x)+std::abs(first.y-last.y);};
        const int forward_cost=distance(start,label->left)+distance(label->right,goal),reverse_cost=distance(start,label->right)+distance(label->left,goal);
        if(forward_cost!=reverse_cost)reverse=reverse_cost<forward_cost;
        const Point entrance=reverse?label->right:label->left,exit=reverse?label->left:label->right,across=reverse?Point{-1,0}:Point{1,0};
        struct GoalReservation {Canvas& canvas;Point point;bool active=false;~GoalReservation(){if(active)canvas.at(point).blocked=false;}} target{canvas,goal};
        // Protect the actual search goal, never the clipped-out normal point.
        if(!canvas.work.spend(2)||!search.free(goal))return false;canvas.at(goal).blocked=true;target.active=true;
        auto left=search.path(start,entrance);canvas.at(goal).blocked=false;target.active=false;
        if(!left)continue;
        // These are real first-half cells, temporarily owned to prevent reuse.
        for(const auto point:*left)if(!canvas.work.spend()||!reservations.add(point))return false;
        auto right=search.path(exit,goal);if(!right)continue;
        const auto first=joined(*prefix,*left,{}),last=joined({},*right,*suffix);if(!first||!last)return false;
        // No temporary guard may undo newly painted path/label cells on success.
        reservations.release(0);
        if(!paint_path(canvas,*first,p.route-p.normal,entrance+across,edge.dotted)||!paint_path(canvas,*last,exit-across,q.route-q.normal,edge.dotted)||!canvas.multiline({label->x,label->y},edge.label,label->w)||!paint_port(canvas,p,diagram.family==RelationFamily::Er)||!paint_port(canvas,q,diagram.family==RelationFamily::Er))return false;
        const auto width=static_cast<std::size_t>(label->w+2),height=static_cast<std::size_t>(label->h+2);
        if(width>static_cast<std::size_t>(canvas.width)||height>static_cast<std::size_t>(canvas.height)||!canvas.work.spend(width*height))return false;
        for(int y=label->y-1;y<=label->y+label->h;++y)for(int x=label->x-1;x<=label->x+label->w;++x)canvas.at({x,y}).blocked=true;
        return true;
    }
    return false;
}

bool route_scope(Canvas& canvas,const RelationDiagram& diagram,const RelationLayout& layout,int owner) {
    for (const int child:layout.children[static_cast<std::size_t>(owner+1)]) if (diagram.entities[static_cast<std::size_t>(child)].shape==EntityShape::Group&&!route_scope(canvas,diagram,layout,child)) return false;
    if (layout.scope_edges[static_cast<std::size_t>(owner+1)].empty()) return true;
    std::optional<ReciprocalPlan> reciprocal;
    if(!reciprocal_plan(diagram,layout,owner,canvas.work,reciprocal))return false;
    if (!canvas.work.spend(canvas.cells.size()*3)) return false;
    // Every created search owns this new view, including default families.
    if(!canvas.work.spend(3u*sizeof(std::string_view)+alignof(std::string_view)+8u))return false;
    if(diagram.family==RelationFamily::Er&&layout.compound_scope){
        const auto count=layout.scope_edges[static_cast<std::size_t>(owner+1)].size();
        std::size_t levels=0;for(auto n=count;n>1u;n=(n+1u)/2u)++levels;
        if(count>128u||canvas.cells.size()>static_cast<std::size_t>(kWidthLimit)*static_cast<std::size_t>(kHeightLimit)||
           !canvas.work.spend(canvas.cells.size()*(4u*sizeof(int)+2u*sizeof(unsigned))+
                count*(6u*sizeof(int)+32u)+count*levels*(16u*sizeof(int)+32u)+
                8u*sizeof(RouteSearch)+8u*sizeof(std::vector<int>)+256u))return false;
    }
    RouteSearch search(canvas,layout,owner);
    if(!diagram.state_scope_directions.empty()){
        const auto selected=diagram.state_direction(owner,canvas.work);if(!selected)return false;search.state_scope_direction=*selected;
    }
    auto edges=layout.scope_edges[static_cast<std::size_t>(owner+1)];
    auto length=[&](int n) { const auto& e=diagram.edges[static_cast<std::size_t>(n)];const auto& a=layout.boxes[static_cast<std::size_t>(e.from)];const auto& b=layout.boxes[static_cast<std::size_t>(e.to)];return std::abs(a.x-b.x)+std::abs(a.y-b.y); };
    std::ranges::stable_sort(edges,[&](int a,int b) { return length(a)<length(b); });
    for (const int edge:edges) {
        const auto index=static_cast<std::size_t>(edge);
        std::optional<CompoundScopeRoute> compound_route;
        if(layout.compound_scope&&diagram.entities[static_cast<std::size_t>(diagram.edges[index].from)].owner!=diagram.entities[static_cast<std::size_t>(diagram.edges[index].to)].owner){if(!canvas.work.spend(3u*(64u+kGroupLimit)+16u+sizeof(std::array<std::size_t,kGroupLimit>)+sizeof(std::size_t)))return false;if((diagram.family==RelationFamily::State||diagram.family==RelationFamily::Er)&&!canvas.work.spend(3u*sizeof(CompoundScopeRoute)+32u))return false;compound_route.emplace(layout,canvas.work);if(!compound_route->prepare(diagram.edges[index],owner))return false;}
        struct CompoundRouteScope {RouteSearch& search;const CompoundScopeRoute* before;~CompoundRouteScope(){search.compound_route=before;}} compound_route_scope{search,search.compound_route};
        if(compound_route)search.compound_route=&*compound_route;
        const bool connected=reciprocal?connect_reciprocal_relation(canvas,search,diagram,layout,diagram.edges[index],*reciprocal,index):connect_relation(canvas,search,diagram,layout,diagram.edges[index]);
        if(!connected)return false;
    }
    return true;
}
} // namespace

std::optional<std::vector<std::string>> relations_layout(std::string_view source,Work& work,bool class_envelope,bool requirement_input) {
    // New bool/pointer forwarding and the bounded three-reference local closure
    // are charged even on default ER/State/Class paths, without a Work reset.
    if(!work.spend(8u*sizeof(void*)+8u*sizeof(bool)))return {};
    // The empty vector's real storage/initialization/moves/destruction are not
    // free for Class/ER/default State. No default heap or ancestry scan follows.
    if(!work.spend(3u*sizeof(std::vector<StateScopeDirection>)+alignof(std::vector<StateScopeDirection>)+8u))return {};
    const auto diagram=[&]() -> std::optional<RelationDiagram> {
        // Dispatch already resolved this source owner. Legacy Class/ER/State
        // never enter the Requirement prefix reader or pay its variable work.
        if(requirement_input){
            if(class_envelope)return {};
            const auto parsed=parse_requirement(source,work);if(!parsed)return {};
            return project_requirement(*parsed,work);
        }
        if(!class_envelope)return parse_relations(source,work);
        const auto envelope=class_source_envelope(source,work);if(!envelope)return {};
        return parse_relations(envelope->body,work,&*envelope);
    }();
    if (!diagram) return {};
    if(diagram->family==RelationFamily::Er&&diagram->entities.empty()) {
        // Titled-empty composition has no reviewed complete-scene profile yet.
        if(diagram->diagram_title)return {};
        return std::vector<std::string>{""};
    }
    // New private active-hint/row-array pointer initialization and destruction.
    if(!work.spend(3u*sizeof(std::unique_ptr<std::array<int,64>>)+sizeof(bool)+alignof(std::unique_ptr<std::array<int,64>>)-1u))return {};
    if(!work.spend(4u*sizeof(bool)+16u))return {};
    if(diagram->class_notes){
        const auto entities=diagram->entities.size(),edges=diagram->edges.size();
        // Existing layout containers, two per-scope bool matrices and bounded
        // vertex/edge-index growth are paid before constructing the new scene.
        if(!work.spend(entities*(6u*sizeof(RelationBox)+24u*sizeof(std::vector<int>)+32u*sizeof(int)+64u)+
            entities*entities*8u+edges*(16u*sizeof(int)+32u)+1024u))return {};
    }
    // Only ER gains this bounded presence scan; no Group allocation is made
    // for its unchanged nongroup path. The activated layout's vector backing,
    // default writes, nested matrices and index relocations precede the ctor.
    if(diagram->family==RelationFamily::Er){
        if(!work.spend(4u*sizeof(bool)+4u*sizeof(std::size_t)+32u))return {};
        bool grouped=false;
        for(const auto& entity:diagram->entities){if(!work.spend(sizeof(EntityShape)+8u))return {};if(entity.shape==EntityShape::Group){grouped=true;break;}}
        if(grouped){
            const auto entities=diagram->entities.size(),edges=diagram->edges.size();
            if(entities>64u||edges>128u||!work.spend(entities*(6u*sizeof(RelationBox)+24u*sizeof(std::vector<int>)+32u*sizeof(int)+64u)+
                entities*entities*8u+entities*std::min(entities,kGroupDepthLimit)*(2u*sizeof(RelationBox)+8u*sizeof(int)+16u)+
                edges*(16u*sizeof(int)+32u)+1024u))return {};
        }
    }
    RelationLayout layout(*diagram,work);if (!layout.prepare()) return {};
    if(diagram->diagram_title&&(diagram->diagram_title->size()>static_cast<std::size_t>(kWidthLimit)||layout.canvas_height>kHeightLimit-2))return {};
    if (!work.spend(static_cast<std::size_t>(layout.canvas_width*layout.canvas_height))) return {};
    Canvas canvas(layout.canvas_width,layout.canvas_height,work);
    if(!work.spend(8u*sizeof(bool)))return {};
    bool expanded=false;
    if(diagram->family==RelationFamily::Class) {
        if(!work.spend(4u*sizeof(std::optional<bool>)+8u))return {};
        expanded=diagram->class_hide_empty_members_box==false;
    }
    for (std::size_t n=0;n<diagram->entities.size();++n) {
        if(!work.spend(2u*sizeof(bool)+4u*sizeof(const StateTextRecord*)+16u))return {};
        const StateTextRecord* text=nullptr;
        if(diagram->family==RelationFamily::State&&!diagram->state_texts.empty()){
            const auto record=diagram->state_text_index(static_cast<int>(n),work);if(!record)return {};
            if(*record<diagram->state_texts.size())text=&diagram->state_texts[*record];
        }
        if(diagram->family==RelationFamily::Er&&layout.compound_scope){
            const auto& entity=diagram->entities[n];
            if(!work.spend(entity.label.size()*(32u*sizeof(std::string)+32u*sizeof(std::string_view)+128u)+256u))return {};
            for(const auto& attribute:entity.er_attributes){
                const auto bytes=attribute.type.size()+attribute.name.size()+attribute.comment.size()+attribute.keys.size()*3u;
                if(!work.spend(bytes*(32u*sizeof(std::string)+32u*sizeof(std::string_view)+128u)+
                    32u*sizeof(int)+16u*sizeof(std::vector<int>)+256u))return {};
            }
        }
        if(!draw_entity(canvas,diagram->entities[n],layout.boxes[n],expanded,text))return {};
    }
    if((diagram->family==RelationFamily::State||diagram->family==RelationFamily::Er)&&layout.compound_scope&&!work.spend(diagram->entities.size()*(sizeof(int)+8u)+16u))return {};
    if(layout.compound_scope)for(std::size_t n=0;n<diagram->entities.size();++n){
        const auto& e=diagram->entities[n];if(e.shape!=EntityShape::Group)continue;
        if((diagram->family==RelationFamily::State||diagram->family==RelationFamily::Er)&&!work.spend(e.label.size()*(4u*sizeof(std::string_view)+8u)+2u*sizeof(std::vector<std::string_view>)+64u))return {};
        if(!work.spend(e.label.size()*4+1))return {};const auto& b=layout.boxes[n];const int end=std::min(b.bottom()-1,b.y+static_cast<int>(label_rows(e.label).size())+1);
        if(!work.spend(static_cast<std::size_t>(std::max(0,end-b.y))*static_cast<std::size_t>(std::max(0,b.w-2))))return {};
        for(int y=b.y+1;y<=end;++y)for(int x=b.x+1;x<b.right();++x)canvas.at({x,y}).blocked=true;
    }
    std::vector<RelationBox> note_boxes;
    for (const auto& note:diagram->notes) {
        const auto& target=layout.boxes[static_cast<std::size_t>(note.target)];
        const int width=label_width(note.text)+4,height=static_cast<int>(label_rows(note.text).size())+2;
        const int x=note.right?target.right()+5:target.x-width-4;
        const RelationBox box{x,target.y+(target.h-height)/2,width,height};
        if (box.x<0||box.y<0||box.right()>=canvas.width||box.bottom()>=canvas.height) return {};
        auto overlaps=[&](const RelationBox& other){return box.x<=other.right()&&box.right()>=other.x&&box.y<=other.bottom()&&box.bottom()>=other.y;};
        // Notes must not overwrite another note or an authored entity. Group
        // interiors may host notes, but their border/header cells stay intact.
        for (std::size_t n=0;n<diagram->entities.size();++n) {
            if (!work.spend()) return {};
            if (diagram->entities[n].shape!=EntityShape::Group&&overlaps(layout.boxes[n])) return {};
        }
        for (const auto& previous:note_boxes) { if (!work.spend()||overlaps(previous)) return {}; }
        for (int y=box.y;y<=box.bottom();++y) for (int x=box.x;x<=box.right();++x) {
            if (!work.spend()||canvas.at({x,y}).blocked||canvas.at({x,y}).glyph!=" ") return {};
        }
        if (!draw_box(canvas,box,false)||!canvas.multiline({box.x+1,box.y+1},note.text,width-2)) return {};
        note_boxes.push_back(box);
    }
    if (!route_scope(canvas,*diagram,layout,-1)) return {};
    auto result=canvas.lines();if (result.empty()) return {};
    if(!diagram->diagram_title)return result;
    if(result.size()>static_cast<std::size_t>(kHeightLimit-2)||!work.spend(result.size()+diagram->diagram_title->size()+2))return {};
    if(diagram->family==RelationFamily::Class&&!work.spend((result.size()+2u)*2u*sizeof(std::string)+result.size()*2u+diagram->diagram_title->size()*2u+64u))return {};
    std::vector<std::string> titled;titled.reserve(result.size()+2);
    titled.push_back(*diagram->diagram_title);titled.emplace_back();
    for(auto& line:result)titled.push_back(std::move(line));
    return titled;
}
} // namespace loom::ui::mermaid::detail
