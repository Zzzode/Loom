module;

module loom.ui.visual.mermaid;

import std;
import :detail;
import loom.serdes.json;

namespace loom::ui::mermaid::detail {
namespace {
// ER owns this closed source-envelope profile. It validates every field before
// handing the unchanged complete body to the existing graph parser.
bool er_config_leaf(std::string_view owner,std::string_view key,const ErMetaValue& value,unsigned& consumed,Work& work,bool projection=false) {
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
        else if(key=="theme") {bit=8u;valid=value.kind==ErMetaValue::Kind::Text&&(value.text=="forest"||(projection&&(value.text=="neo"||value.text=="neo-dark"||value.text=="redux"||value.text=="redux-dark"||value.text=="redux-color"||value.text=="redux-dark-color")));}
        else if(projection&&key=="look") {bit=512u;valid=value.kind==ErMetaValue::Kind::Text&&(value.text=="neo"||value.text=="handDrawn");}
        else if(projection&&key=="htmlLabels") {bit=1024u;valid=value.kind==ErMetaValue::Kind::Boolean&&!value.boolean;}
        else if(key=="fontSize") {bit=16u;valid=value.kind==ErMetaValue::Kind::Number&&std::isfinite(value.number)&&value.number>0;}
        else if(key=="themeCSS") {bit=32u;valid=value.kind==ErMetaValue::Kind::Text&&er_config_text(value.text,true,work);}
    }else if(projection&&owner=="er"&&key=="useMaxWidth") {bit=2048u;valid=value.kind==ErMetaValue::Kind::Boolean&&!value.boolean;
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
        return ErEnvelope{source.substr(newline+1),{},nullptr};
    }
    if(!work.spend(4u*sizeof(bool)+16u))return {};
    const auto parsed=detail::parse_envelope(source,work);
    if(!parsed||!parsed->has_frontmatter)return {};
    bool description_seen=false,config_seen=false,title_seen=false;
    unsigned consumed=0;
    bool projection=false,forest=false;
    int er_namespace_indent=-1;
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
        // The new ER namespace adds one owner-map key. Its node/key storage
        // is paid before emplace; existing namespace/legacy fees stay intact.
        if(parent=="er"&&!work.spend(sizeof(std::pair<const std::string,int>)+
                                   4u*sizeof(void*)+parent.size()+1u))return {};
        const auto [position,inserted]=config_indent.emplace(std::string(parent),event.indent);
        if(!inserted&&position->second!=event.indent)return {};
        if(event.namespace_entry) {
            if(!parent.empty()||(event.path!="flowchart"&&event.path!="themeVariables"&&event.path!="er"))return {};
            if(std::ranges::find(namespaces,event.path)!=namespaces.end())return {};
            if(event.path=="er") {
                const auto capacity=namespaces.size()+1u;
                if(!work.spend(capacity*sizeof(std::string)+namespaces.size()+event.path.size()+1u))return {};
                namespaces.reserve(capacity);
            }
            namespaces.push_back(event.path);
            if(event.path=="er")er_namespace_indent=event.indent;
            continue;
        }
        const std::string_view owner=parent;
        const std::string_view key=dot==std::string_view::npos?std::string_view(event.path):
            std::string_view(event.path).substr(dot+1u);
        if(!owner.empty()&&owner!="flowchart"&&owner!="themeVariables"&&owner!="er")return {};
        // A flattened literal key is not an authored nested ER namespace.
        if(owner=="er"&&(er_namespace_indent<0||event.indent<=er_namespace_indent))return {};
        ErMetaValue decoded;
        if(owner.empty()&&key=="themeCSS"&&event.form==EnvelopeScalarForm::Literal) {
            decoded={ErMetaValue::Kind::Text,event.value};
        } else {
            decoded=er_yaml_value(er_strip_comment(event.value),work);
        }
        if(!er_config_leaf(owner,key,decoded,consumed,work,true))return {};
        if(owner.empty()&&key=="theme") {
            forest=decoded.text=="forest";
            projection|=!forest;
        }
        projection|=(consumed&(512u|1024u|2048u))!=0;
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
    if(projection) {
        // Only the independently reviewed frontmatter profile activates these
        // allocations. Legacy leaves may not silently enter the new contract.
        if(title_seen||forest||(consumed&(1u|2u|16u|32u|64u|128u|256u)))return {};
        std::size_t leaves=0,bytes=0;
        for(const auto& event:parsed->events) {
            if(!work.spend(1u))return {};
            if(event.top_level||event.blank||event.comment||event.namespace_entry)continue;
            if(!work.spend(event.path.size()+event.value.size()+1u))return {};
            ++leaves;bytes+=event.path.size()+event.value.size()+16u;
        }
        // Include initialization of the active-only rank-plan scratch flag.
        if(!work.spend(sizeof(ErPresentation)+64u+leaves*sizeof(std::string)+bytes*4u+
                       sizeof(bool)+8u))return {};
        envelope.presentation=std::make_unique<ErPresentation>();
        auto& profile=*envelope.presentation;
        profile.config_notices.reserve(leaves);
        for(const auto& event:parsed->events) {
            if(!work.spend(1u))return {};
            if(event.top_level||event.blank||event.comment||event.namespace_entry)continue;
            const auto value=er_yaml_value(er_strip_comment(event.value),work);
            // The aggregate byte reservation above owns the final strings and
            // their copies. Reserve once rather than relying on concatenation's
            // implementation-specific intermediate capacities.
            std::string notice;
            const auto value_bytes=value.kind==ErMetaValue::Kind::Text?value.text.size()+2u:5u;
            notice.reserve(8u+event.path.size()+value_bytes);
            notice="config.";notice+=event.path;notice+='=';
            if(value.kind==ErMetaValue::Kind::Text) {
                notice+='"';notice+=value.text;notice+='"';
                if(event.path=="theme")profile.theme=value.text;
                if(event.path=="look")profile.look=value.text;
            }else if(value.kind==ErMetaValue::Kind::Boolean&&!value.boolean)notice+="false";
            else return {};
            profile.config_notices.push_back(std::move(notice));
        }
    }
    envelope.body=parsed->raw_body;
    return envelope;
}
std::optional<std::string> er_projection_wrap(std::string_view text,Work& work) {
    // This profile is deliberately limited to printable ASCII captions. The
    // source authority does not establish arbitrary Unicode/Markdown behavior.
    if(text.size()>1024u||!work.spend(text.size()*6u+sizeof(std::string)+32u))return {};
    for(const unsigned char c:text)if(c<0x20u||c>0x7eu)return {};
    std::size_t rows=1u;
    auto next=[](std::string_view rest) {
        const auto space=rest.rfind(' ',32u);
        return space!=std::string_view::npos&&space>0u?
            std::pair<std::size_t,std::size_t>{space,space+1u}:
            std::pair<std::size_t,std::size_t>{32u,32u};
    };
    for(auto rest=text;rest.size()>32u;++rows)rest.remove_prefix(next(rest).second);
    if(!work.spend(text.size()+rows+1u))return {};
    std::string result;result.reserve(text.size()+rows);
    while(text.size()>32u) {
        const auto [count,consumed]=next(text);
        result.append(text.substr(0,count));result+='\n';text.remove_prefix(consumed);
    }
    result.append(text);return result;
}
std::optional<std::vector<std::string>> er_projection_notices(const RelationDiagram& diagram,Work& work) {
    if(!diagram.er_presentation||diagram.family!=RelationFamily::Er)return {};
    const auto& profile=*diagram.er_presentation;
    if(!work.spend(sizeof(std::vector<std::string>)+512u))return {};
    std::vector<std::string> rows;
    auto append=[&](std::initializer_list<std::string_view> pieces) {
        std::size_t bytes=0;
        for(const auto piece:pieces) {
            if(piece.size()>1024u-bytes)return false;
            bytes+=piece.size();
        }
        if(!work.spend(bytes*4u+sizeof(std::string)+32u))return false;
        std::string record;record.reserve(bytes);
        for(const auto piece:pieces)record.append(piece);
        const auto wrapped=er_projection_wrap(record,work);
        if(!wrapped||!work.spend(wrapped->size()))return false;
        std::size_t count=1u;
        for(const auto c:*wrapped)if(c=='\n')++count;
        if(count>1024u-rows.size()||!work.spend(wrapped->size()+count*sizeof(std::string)))return false;
        const auto needed=rows.size()+count;
        if(needed>rows.capacity()) {
            const auto capacity=std::min<std::size_t>(1024u,std::max(needed,rows.capacity()*2u));
            if(!work.spend(capacity*sizeof(std::string)+rows.size()))return false;
            rows.reserve(capacity);
        }
        std::size_t at=0;
        while(true) {
            const auto end=wrapped->find('\n',at);
            rows.emplace_back(wrapped->substr(at,end==std::string::npos?end:end-at));
            if(end==std::string::npos)break;
            at=end+1u;
        }
        return true;
    };
    if(!append({"ER native projection v1"}))return {};
    bool layout=false,html=false,viewport=false;
    for(const auto& notice:profile.config_notices) {
        if(!work.spend(notice.size()+1u)||!append({notice}))return {};
        layout|=notice=="config.layout=\"elk\"";
        html|=notice=="config.htmlLabels=false";
        viewport|=notice=="config.er.useMaxWidth=false";
    }
    if(!append({"effective theme=",profile.theme,"; look=",profile.look})||
       !append({"look=",profile.look," -> native orthogonal; roughness/relief not reproduced"}))return {};
    if(layout&&!append({"layout=elk -> native directed routing"}))return {};
    if(html&&!append({"htmlLabels=false -> native Markdown subset"}))return {};
    if(viewport&&!append({"er.useMaxWidth=false -> SVG viewport only; native width fallback unchanged"}))return {};
    if(!append({"theme RGB/font/CSS -> terminal roles; foreign assertions pending"}))return {};
    if(profile.theme=="redux-color"||profile.theme=="redux-dark-color") {
        constexpr std::array<std::string_view,12> borders{{"Purple","Cyan","Orange","Cyan","Green","Purple","Red","Yellow","Blue","Green","Blue","Red"}};
        constexpr std::array<std::string_view,12> fills{{"Purple","Cyan","Orange","Cyan","Green","Blue","Red","Yellow","Blue","Green","Blue","Red"}};
        std::size_t ordinal=0;
        for(const auto& entity:diagram.entities) {
            if(!work.spend(1u))return {};
            if(entity.shape==EntityShape::Group)continue;
            if(!work.spend(64u+entity.id.size()))return {};
            const auto slot=ordinal++%12u;
            const auto number=std::to_string(slot);
            if(!append({"entity ",entity.id," slot=",number," border=",borders[slot]," fill=",
                        profile.theme=="redux-dark-color"?std::string_view("Neutral"):fills[slot]}))return {};
        }
    }
    for(const auto& [ordinal,raw]:profile.blank_roles) {
        if(raw.size()>static_cast<std::size_t>(kLabelLimit)||!work.spend(96u+raw.size())||
           std::ranges::any_of(raw,[](char c){return c!=' ';}))return {};
        const auto number=std::to_string(ordinal);
        if(!append({"edge ",number," raw-role=\"",raw,"\" -> no visible words"}))return {};
    }
    return rows;
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
