module;

module loom.ui.visual.mermaid;

import std;
import :detail;
import loom.serdes.json;

namespace loom::ui::mermaid::detail {
namespace {
// ER owns this closed source-envelope profile. It validates every field before
// handing the unchanged complete body to the existing graph parser.
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
} // namespace
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
    const auto parsed=detail::parse_envelope(source,work);
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
} // namespace loom::ui::mermaid::detail
