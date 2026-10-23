// Test-private Flow400 config21 supplement binder.
// It validates immutable source/config/projection/debt metadata and delegates
// every Model decode and cell-geometry decision to the existing Flow400 code.
#pragma once
#include "mermaid_flow400_authority.hpp"

namespace loom::test::mermaid_flow {

inline constexpr std::size_t kFlow400Config21AssetBytes = 124702u;
inline constexpr std::string_view kFlow400Config21AssetSha =
    "6011a1ff418dcb2491e65e3a77205237d5b24483d755a9826dbbe32fd537c9e6";

inline void flow400_config21_keys(JsonVal value,std::initializer_list<std::string_view> keys,
                                  std::string_view scope) {
    if (!value.is_obj()) throw std::runtime_error("Flow400 Config21 " + std::string(scope) + " wrong kind");
    std::array<bool,16> seen{};
    if (keys.size()>seen.size()) throw std::runtime_error("Flow400 Config21 internal key bound");
    value.iter_obj([&](JsonVal key,JsonVal) {
        const auto name=flow400_text(key);const auto at=std::ranges::find(keys,name);
        if(at==keys.end())throw std::runtime_error("Flow400 Config21 "+std::string(scope)+" unknown key");
        const auto index=static_cast<std::size_t>(at-keys.begin());
        if(seen[index])throw std::runtime_error("Flow400 Config21 "+std::string(scope)+" duplicate key");
        seen[index]=true;
    });
    if(value.size()!=keys.size())throw std::runtime_error("Flow400 Config21 "+std::string(scope)+" missing key");
    for(std::size_t index=0;index<keys.size();++index)
        if(!seen[index])throw std::runtime_error("Flow400 Config21 "+std::string(scope)+" missing key");
}

inline void flow400_config21_hex(JsonVal value,const char* message) {
    const auto text=flow400_text(value);
    flow400_require(text.size()==64u,message);
    for(const unsigned char c:text)
        flow400_require((c>='0'&&c<='9')||(c>='a'&&c<='f'),message);
}

inline void flow400_config21_strings(JsonVal value,const char* message,bool nonempty=true) {
    flow400_require(value.is_arr(),message);
    value.iter([&](JsonVal item) {
        const auto text=flow400_text(item);
        flow400_require(!nonempty||!text.empty(),message);
    });
}

inline void flow400_config21_config(JsonVal config) {
    flow400_config21_keys(config,{"after_public_finally_config_graph_reference","authored_loose_effective_strict",
        "complete_raw_config_lexemes","description_form","physical_frontmatter_span",
        "processed_config_post_addDirective","raw_frontmatter_root_keys","stock_finally_evidence_reference"},"config");
    const auto after=config.get("after_public_finally_config_graph_reference");
    flow400_config21_keys(after,{"canonical_sha256","record_pointer"},"after-finally reference");
    flow400_config21_hex(after.get("canonical_sha256"),"Flow400 Config21 after-finally SHA");
    static_cast<void>(flow400_text(after.get("record_pointer")));
    flow400_require(config.get("authored_loose_effective_strict").is_bool(),"Flow400 Config21 strict projection type");
    const auto form=flow400_text(config.get("description_form"));
    flow400_require(form=="plain continuation"||form=="folded >-","Flow400 Config21 description form");
    const auto roots=config.get("raw_frontmatter_root_keys");
    flow400_require(roots.is_arr()&&roots.size()==2u&&flow400_text(roots.at(0))=="description"&&
        flow400_text(roots.at(1))=="config","Flow400 Config21 frontmatter roots");
    const auto span=config.get("physical_frontmatter_span");
    flow400_config21_keys(span,{"length_utf8_bytes","offset_utf8_bytes","sha256"},"frontmatter span");
    static_cast<void>(flow400_number(span.get("length_utf8_bytes")));static_cast<void>(flow400_number(span.get("offset_utf8_bytes")));
    flow400_config21_hex(span.get("sha256"),"Flow400 Config21 frontmatter SHA");
    const auto stock=config.get("stock_finally_evidence_reference");
    flow400_config21_keys(stock,{"canonical_sha256","record_pointer"},"stock-finally reference");
    flow400_config21_hex(stock.get("canonical_sha256"),"Flow400 Config21 stock-finally SHA");
    static_cast<void>(flow400_text(stock.get("record_pointer")));
    flow400_require(config.get("processed_config_post_addDirective").is_obj(),"Flow400 Config21 processed config kind");
    const auto lexemes=config.get("complete_raw_config_lexemes");
    flow400_require(lexemes.is_arr()&&lexemes.size()>0u,"Flow400 Config21 raw config lexemes");
    lexemes.iter([&](JsonVal lexeme) {
        flow400_config21_keys(lexeme,{"length_utf8_bytes","line","offset_utf8_bytes","raw_value","raw_yaml_path_hint","sha256"},"config lexeme");
        static_cast<void>(flow400_number(lexeme.get("length_utf8_bytes")));static_cast<void>(flow400_number(lexeme.get("line")));
        static_cast<void>(flow400_number(lexeme.get("offset_utf8_bytes")));static_cast<void>(flow400_text(lexeme.get("raw_value")));
        static_cast<void>(flow400_text(lexeme.get("raw_yaml_path_hint")));flow400_config21_hex(lexeme.get("sha256"),"Flow400 Config21 config lexeme SHA");
    });
}

inline void flow400_config21_case_shape(JsonVal row,OracleWork& work) {
    work.spend(4096u);
    flow400_config21_keys(row,{"authority","debts","mapping_canonical_sha256","metadata","projection",
        "source_contract","source_proposal_row_canonical_sha256","source_witness_canonical_sha256"},"case");
    const auto authority=row.get("authority");
    flow400_keys(authority,{"ordinal","manifest_global_ordinal","fixture","source_bytes","source_sha256",
        "source_git_blob","oracle_record_sha256","model_json","model_sha256","adaptation"});
    flow400_require(flow400_number(authority.get("ordinal"))<400u,"Flow400 Config21 source ordinal range");
    const auto contract=flow400_text(row.get("source_contract"));
    flow400_require(contract=="PLAIN11"||contract=="STRICT_PLAIN10","Flow400 Config21 source contract");
    flow400_config21_hex(row.get("source_proposal_row_canonical_sha256"),"Flow400 Config21 proposal-row SHA");
    flow400_config21_hex(row.get("mapping_canonical_sha256"),"Flow400 Config21 mapping SHA");
    flow400_require(flow400_text(row.get("mapping_canonical_sha256"))==flow400_text(row.get("source_witness_canonical_sha256")),
        "Flow400 Config21 source witness binding");
    const auto metadata=row.get("metadata");
    flow400_config21_keys(metadata,{"config","config_canonical_sha256","full_metadata_canonical_sha256","parent_metadata_exact"},"metadata");
    const auto config=metadata.get("config");flow400_config21_config(config);
    flow400_config21_hex(metadata.get("config_canonical_sha256"),"Flow400 Config21 config SHA shape");
    flow400_require(digest(config.to_string())==flow400_text(metadata.get("config_canonical_sha256")),"Flow400 Config21 config SHA");
    flow400_config21_hex(metadata.get("full_metadata_canonical_sha256"),"Flow400 Config21 full metadata SHA");
    flow400_require(metadata.get("parent_metadata_exact").is_obj(),"Flow400 Config21 parent metadata kind");
    const auto projection=row.get("projection");
    flow400_config21_keys(projection,{"adaptations","production_spelling_obligations","source_witness","terminal_projection_foreign_gaps"},"projection");
    flow400_config21_strings(projection.get("adaptations"),"Flow400 Config21 adaptations");
    flow400_config21_strings(projection.get("production_spelling_obligations"),"Flow400 Config21 spelling obligations",false);
    flow400_config21_strings(projection.get("terminal_projection_foreign_gaps"),"Flow400 Config21 terminal debts");
    const auto witness=projection.get("source_witness");
    flow400_config21_keys(witness,{"canonical_mapping_sha256","file","mapped_terminal_leaves","ordinal"},"source witness");
    flow400_config21_hex(witness.get("canonical_mapping_sha256"),"Flow400 Config21 witness SHA");
    flow400_require(flow400_text(witness.get("canonical_mapping_sha256"))==flow400_text(row.get("mapping_canonical_sha256"))&&
        flow400_number(witness.get("ordinal"))==flow400_number(authority.get("ordinal"))&&
        flow400_text(witness.get("file"))=="SOURCE_WITNESSES.json","Flow400 Config21 witness source binding");
    const auto debts=row.get("debts");
    flow400_config21_keys(debts,{"foreign_certified","remaining_foreign_debts_exact_from_parent","removed_source_mapping_debts_proposed"},"debts");
    flow400_require(debts.get("foreign_certified").is_bool()&&!debts.get("foreign_certified").as_bool(),"Flow400 Config21 foreign certification");
    flow400_require(debts.get("remaining_foreign_debts_exact_from_parent").is_arr()&&debts.get("removed_source_mapping_debts_proposed").is_arr(),
        "Flow400 Config21 debt arrays");
}

inline void flow400_config21_root(JsonVal root,OracleWork& work) {
    flow400_config21_keys(root,{"cases","counts","existing_authority","schema","source_design_review_lock","source_model_proposal_lock"},"root");
    flow400_require(flow400_text(root.get("schema"))=="flow400_config21_native_supplement/v1","Flow400 Config21 schema");
    const auto old=root.get("existing_authority");
    flow400_config21_keys(old,{"all_sources","asset_sha256","complete_models","manifest_sha256","pending"},"existing authority");
    flow400_require(flow400_number(old.get("all_sources"))==400u&&flow400_number(old.get("complete_models"))==8u&&
        flow400_number(old.get("pending"))==392u&&flow400_text(old.get("asset_sha256"))==kFlow400AssetSha&&
        flow400_text(old.get("manifest_sha256"))==kFlow400ManifestSha,"Flow400 Config21 existing authority binding");
    const auto design=root.get("source_design_review_lock");
    flow400_config21_keys(design,{"canonical_full_compact_noLF_sha256","raw_sha256"},"source-design lock");
    flow400_require(flow400_text(design.get("canonical_full_compact_noLF_sha256"))=="894ecde0f0b36b117eb4c9fb12bca2e08c525874e5e76594feb281b0b080143d"&&
        flow400_text(design.get("raw_sha256"))=="84ce3dd7e2bc1b467e52158c0ea45f63bf5373e00f65c170c629b5b46d21b548",
        "Flow400 Config21 source-design lock");
    const auto model_lock=root.get("source_model_proposal_lock");
    flow400_config21_keys(model_lock,{"independent_checks_raw_sha256","independent_review_raw_sha256","proposal_raw_sha256",
        "proposal_set_lock_raw_sha256","verdict"},"source-Model lock");
    flow400_require(flow400_text(model_lock.get("proposal_raw_sha256"))=="6f855a5155b1a004df41134f22087c4518d6e3f751a9adec14196b3697b30ebb"&&
        flow400_text(model_lock.get("proposal_set_lock_raw_sha256"))=="9a4a4ff87d3893325790e101aabc81cc722f71917806a3c4a7643253b85574d0"&&
        flow400_text(model_lock.get("independent_checks_raw_sha256"))=="17406509aed828ddbffbb93f542aa4d848606836f0cd5d4ff852fd973fd73ce2"&&
        flow400_text(model_lock.get("independent_review_raw_sha256"))=="5d21259bde9ddc6a5e947945bc9ea4f8a077793ad9a95a1b0fd7b788a6b2313a"&&
        flow400_text(model_lock.get("verdict"))=="APPROVE_LIMITED_ALL21_SOURCE_MODEL_PROPOSALS_ONLY","Flow400 Config21 source-Model lock");
    const auto counts=root.get("counts");
    flow400_config21_keys(counts,{"cases","edges","empty_groups","groups","max_model_bytes","model_bytes","nodes","plain","source_bytes","strict","syntax_gaps"},"counts");
    flow400_require(flow400_number(counts.get("cases"))==21u&&flow400_number(counts.get("source_bytes"))==7841u&&
        flow400_number(counts.get("nodes"))==55u&&flow400_number(counts.get("edges"))==43u&&flow400_number(counts.get("groups"))==18u&&
        flow400_number(counts.get("empty_groups"))==2u&&flow400_number(counts.get("max_model_bytes"))==870u&&
        flow400_number(counts.get("model_bytes"))==10272u&&flow400_number(counts.get("plain"))==11u&&
        flow400_number(counts.get("strict"))==10u&&flow400_number(counts.get("syntax_gaps"))==9u,"Flow400 Config21 totals");
    const auto cases=root.get("cases");flow400_require(cases.is_arr()&&cases.size()==21u,"Flow400 Config21 case count");
    std::array<bool,400> seen{};
    cases.iter([&](JsonVal row) {
        flow400_config21_case_shape(row,work);const auto ordinal=flow400_number(row.get("authority").get("ordinal"));
        flow400_require(!seen[ordinal],"Flow400 Config21 duplicate source ordinal");seen[ordinal]=true;
    });
}

struct Flow400Config21Documents { OracleWork asset_work; json::JsonDoc supplement; };

inline Flow400Config21Documents flow400_config21_documents() {
    Flow400Config21Documents docs;docs.asset_work.spend(4u*sizeof(json::JsonDoc)+4u*sizeof(OracleWork)+8192u);
    const auto path=std::filesystem::path(LOOM_TESTS_DIR)/"fixtures/mermaid_native/flow400_config21_native_supplement.v1.json";
    const auto size=std::filesystem::file_size(path);flow400_require(size==kFlow400Config21AssetBytes,"Flow400 Config21 immutable asset bytes");
    docs.asset_work.product(static_cast<std::size_t>(size),3u);const auto bytes=flow400_read(path);
    flow400_require(digest(bytes)==kFlow400Config21AssetSha,"Flow400 Config21 immutable asset SHA");
    auto parsed=json::parse(bytes);flow400_require(parsed.has_value(),"Flow400 Config21 asset JSON");docs.supplement=std::move(*parsed);
    flow400_config21_root(docs.supplement.root(),docs.asset_work);return docs;
}

inline void flow400_config21_bind(JsonVal row,JsonVal fixture,std::size_t ordinal,std::size_t global,
                                  std::string_view source,OracleWork& work) {
    flow400_config21_case_shape(row,work);const auto authority=row.get("authority");
    flow400_require(flow400_number(authority.get("ordinal"))==ordinal,"Flow400 Config21 selected ordinal");
    flow400_bind(authority,fixture,ordinal,global,source,work);
}

inline bool flow400_config21_admission_allowed(bool source_wide,bool wide_geometry,bool narrow_geometry,
                                                bool restored,std::size_t rejected,std::size_t required) {
    return !source_wide&&wide_geometry&&narrow_geometry&&restored&&required>=2u&&rejected==required;
}

// A borrowed authority remains owned by the caller's immutable documents.
// Both complete400 consumers use the same source-only selection/binding rule.
struct BoundFlow400Authority { JsonVal row; bool supplemental=false; };
inline BoundFlow400Authority flow400_config21_bound_authority(JsonVal base,JsonVal supplement,
        JsonVal fixture,std::size_t ordinal,std::size_t global,std::string_view source,OracleWork& work) {
    work.spend(4096u+4u*sizeof(BoundFlow400Authority)+8u*sizeof(std::size_t)+4u*sizeof(bool));
    const auto cases=supplement.get("cases");
    flow400_require(cases.is_arr()&&cases.size()==21u,"Flow400 Config21 selection denominator");
    JsonVal selected;std::size_t matches=0u;
    cases.iter([&](JsonVal candidate){
        work.spend(256u);
        if(flow400_number(candidate.get("authority").get("ordinal"))==ordinal){selected=candidate;++matches;}
    });
    flow400_require(matches<=1u,"Flow400 Config21 duplicate source selection");
    flow400_bind(base,fixture,ordinal,global,source,work);
    if(matches==0u)return {base,false};
    flow400_require(base.get("model_json").is_null(),"Flow400 Config21 overlaps immutable base Model");
    flow400_config21_bind(selected,fixture,ordinal,global,source,work);
    return {selected.get("authority"),true};
}

} // namespace loom::test::mermaid_flow
