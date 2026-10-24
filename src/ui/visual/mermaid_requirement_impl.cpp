module;

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {


// Requirement is intentionally a private typed language.  These records never
// enter Class, ER, or State parsing and only project to neutral boxes after
// every declaration and endpoint has been resolved.
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
    const auto envelope=detail::parse_envelope(source,work);
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
} // namespace
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
namespace {
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
} // namespace
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
} // namespace loom::ui::mermaid::detail
