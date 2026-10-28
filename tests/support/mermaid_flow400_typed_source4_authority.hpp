// Test-private Source4 typed Flow400 selector.
// It extends no production interface and borrows all JsonVal rows from caller-owned documents.
#pragma once
#include "mermaid_flow400_config21_authority.hpp"

namespace loom::test::mermaid_flow {

inline constexpr std::size_t kFlow400TypedSource4AssetBytes = 13545u;
inline constexpr std::string_view kFlow400TypedSource4AssetSha =
    "b5a64f09fd363020d5bb91c2498541d6bfa88ec17bab8a3fb8eaf3f0b43a472e";

enum class Flow400TypedSource4Kind { Base, Config21, TypedSource4 };

struct Flow400TypedSource4Documents {
    OracleWork asset_work;
    json::JsonDoc supplement;
};

inline void flow400_typed_source4_keys(JsonVal value, std::initializer_list<std::string_view> keys,
                                       std::string_view scope) {
    if (!value.is_obj())
        throw std::runtime_error("Flow400 TypedSource4 " + std::string(scope) + " wrong kind");
    std::array<bool, 16> seen{};
    if (keys.size() > seen.size())
        throw std::runtime_error("Flow400 TypedSource4 internal key bound");
    value.iter_obj([&](JsonVal key, JsonVal) {
        const auto name = flow400_text(key);
        const auto at = std::ranges::find(keys, name);
        if (at == keys.end())
            throw std::runtime_error("Flow400 TypedSource4 " + std::string(scope) + " unknown key");
        const auto index = static_cast<std::size_t>(at - keys.begin());
        if (seen[index])
            throw std::runtime_error("Flow400 TypedSource4 " + std::string(scope) +
                                     " duplicate key");
        seen[index] = true;
    });
    if (value.size() != keys.size())
        throw std::runtime_error("Flow400 TypedSource4 " + std::string(scope) + " missing key");
    for (std::size_t index = 0; index < keys.size(); ++index)
        if (!seen[index])
            throw std::runtime_error("Flow400 TypedSource4 " + std::string(scope) + " missing key");
}

inline void flow400_typed_source4_hex(JsonVal value, const char* message) {
    const auto text = flow400_text(value);
    flow400_require(text.size() == 64u, message);
    for (const unsigned char c : text)
        flow400_require((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'), message);
}

inline std::size_t flow400_typed_source4_expected(JsonVal row) {
    const auto authority = row.get("base_authority");
    const auto ordinal = flow400_number(authority.get("ordinal"));
    const auto expected = flow400_number(row.get("native_requirement").get("exact_rejections"));
    const std::size_t fixed = ordinal == 0u     ? 8u
                              : ordinal == 282u ? 9u
                              : ordinal == 374u ? 5u
                              : ordinal == 375u ? 7u
                                                : 0u;
    flow400_require(fixed > 0u && expected == fixed, "Flow400 TypedSource4 exact mutation count");
    return fixed;
}

inline void flow400_typed_source4_envelope(JsonVal envelope, JsonVal row) {
    flow400_typed_source4_keys(envelope,
                               {"kind", "bytes", "sha256", "text", "description_form", "root_keys",
                                "config", "body_header", "body_sha256"},
                               "envelope");
    const auto source = flow400_text(row.get("source")), text = flow400_text(envelope.get("text"));
    const auto bytes = flow400_number(envelope.get("bytes"));
    flow400_require(bytes == text.size() && digest(text) == flow400_text(envelope.get("sha256")) &&
                        source.starts_with(text),
                    "Flow400 TypedSource4 envelope bytes/SHA");
    const auto kind = flow400_text(envelope.get("kind")),
               form = flow400_text(envelope.get("description_form"));
    flow400_require((kind == "none" && text.empty() && form == "none") ||
                        (kind == "frontmatter" && !text.empty() &&
                         (form == "plain_multiline" || form == "folded")),
                    "Flow400 TypedSource4 envelope form");
    const auto roots = envelope.get("root_keys");
    flow400_require(roots.is_arr(), "Flow400 TypedSource4 envelope root keys");
    roots.iter([](JsonVal key) { static_cast<void>(flow400_text(key)); });
    if (kind == "none")
        flow400_require(roots.size() == 0u && envelope.get("config").is_null(),
                        "Flow400 TypedSource4 bare envelope");
    const auto header = flow400_text(envelope.get("body_header"));
    flow400_require(!header.empty() && source.substr(text.size()).starts_with(header),
                    "Flow400 TypedSource4 body header");
    const auto body = source.substr(text.size());
    flow400_require(digest(body) == flow400_text(envelope.get("body_sha256")),
                    "Flow400 TypedSource4 body SHA");
    if (!envelope.get("config").is_null()) {
        const auto config = envelope.get("config");
        flow400_require(config.is_obj() && config.size() == 1u &&
                            flow400_text(config.get("fontFamily")) == "courier",
                        "Flow400 TypedSource4 closed config");
    }
}

inline void flow400_typed_source4_case_shape(JsonVal row, OracleWork& work) {
    work.spend(8192u);
    flow400_typed_source4_keys(row,
                               {"case_id", "base_authority", "source", "source_sha256",
                                "typed_model_json", "typed_model_sha256", "envelope",
                                "native_requirement", "formal_status"},
                               "case");
    const auto id = flow400_text(row.get("case_id"));
    flow400_require(id == "original_000" || id == "original_282" || id == "original_374" ||
                        id == "original_375",
                    "Flow400 TypedSource4 case identity");
    const auto authority = row.get("base_authority");
    flow400_keys(authority, {"adaptation", "fixture", "manifest_global_ordinal", "model_json",
                             "model_sha256", "oracle_record_sha256", "ordinal", "source_bytes",
                             "source_git_blob", "source_sha256"});
    flow400_require(authority.get("model_json").is_null() &&
                        authority.get("model_sha256").is_null() &&
                        authority.get("adaptation").is_null(),
                    "Flow400 TypedSource4 base must remain null");
    const auto source = flow400_text(row.get("source"));
    flow400_require(source.size() == flow400_number(authority.get("source_bytes")) &&
                        digest(source) == flow400_text(row.get("source_sha256")) &&
                        flow400_text(row.get("source_sha256")) ==
                            flow400_text(authority.get("source_sha256")),
                    "Flow400 TypedSource4 source lock");
    const auto model = flow400_text(row.get("typed_model_json"));
    flow400_require(model.size() <= 2048u &&
                        digest(model) == flow400_text(row.get("typed_model_sha256")),
                    "Flow400 TypedSource4 Model lock");
    work.product(source.size() + model.size(), 32u);
    work.spend(4u * sizeof(json::JsonDoc) + 8192u);
    auto model_document = json::parse(model);
    flow400_require(model_document.has_value(), "Flow400 TypedSource4 Model JSON");
    static_cast<void>(flow_typed_shape_model(model_document->root(), work));
    flow400_typed_source4_envelope(row.get("envelope"), row);
    const auto native = row.get("native_requirement");
    flow400_typed_source4_keys(
        native, {"native_test", "existing_native_asset", "exact_rejections", "mutations"},
        "native requirement");
    static_cast<void>(flow400_text(native.get("native_test")));
    const auto mutations = native.get("mutations");
    // The first plan entry names the three existing whole-scene mutations.
    // Every remaining entry names exactly one additional source mutation.
    const auto planned_rejections = flow400_typed_source4_expected(row);
    flow400_require(mutations.is_arr() && mutations.size() == planned_rejections - 2u &&
                        flow400_text(mutations.at(0u)) == "legacy_three",
                    "Flow400 TypedSource4 native mutation plan");
    mutations.iter([](JsonVal mutation) { static_cast<void>(flow400_text(mutation)); });
    const auto exact = flow400_typed_source4_expected(row);
    const auto ordinal = flow400_number(authority.get("ordinal"));
    const bool exact_tuple = (id == "original_000" && ordinal == 0u && exact == 8u) ||
                             (id == "original_282" && ordinal == 282u && exact == 9u) ||
                             (id == "original_374" && ordinal == 374u && exact == 5u) ||
                             (id == "original_375" && ordinal == 375u && exact == 7u);
    flow400_require(exact_tuple, "Flow400 TypedSource4 case-id/ordinal/mutation tuple");
    flow400_require(flow400_text(row.get("formal_status")) == "unadmitted_design_only",
                    "Flow400 TypedSource4 unadmitted source state");
}

inline void flow400_typed_source4_root(JsonVal root, OracleWork& work) {
    flow400_typed_source4_keys(root,
                               {"schema", "qualification", "base_authority", "config21_authority",
                                "formal_counts", "cases"},
                               "root");
    flow400_require(flow400_text(root.get("schema")) ==
                        "flow400_typed_source4_formal_supplement/v1",
                    "Flow400 TypedSource4 schema");
    const auto base = root.get("base_authority");
    flow400_typed_source4_keys(
        base, {"asset_sha256", "manifest_sha256", "all_sources", "complete_models", "pending"},
        "base authority");
    flow400_require(flow400_text(base.get("asset_sha256")) == kFlow400AssetSha &&
                        flow400_text(base.get("manifest_sha256")) == kFlow400ManifestSha &&
                        flow400_number(base.get("all_sources")) == 400u &&
                        flow400_number(base.get("complete_models")) == 8u &&
                        flow400_number(base.get("pending")) == 392u,
                    "Flow400 TypedSource4 base authority");
    const auto config = root.get("config21_authority");
    flow400_typed_source4_keys(config,
                               {"asset_sha256", "cases", "aggregate_complete_before_source4",
                                "aggregate_pending_before_source4"},
                               "Config21 authority");
    flow400_require(flow400_text(config.get("asset_sha256")) == kFlow400Config21AssetSha &&
                        flow400_number(config.get("cases")) == 21u &&
                        flow400_number(config.get("aggregate_complete_before_source4")) == 29u &&
                        flow400_number(config.get("aggregate_pending_before_source4")) == 371u,
                    "Flow400 TypedSource4 Config21 authority");
    const auto counts = root.get("formal_counts");
    flow400_typed_source4_keys(counts,
                               {"cases", "aggregate_complete_after_all_official_runs",
                                "aggregate_pending_after_all_official_runs",
                                "ultimate_complete_required", "ultimate_pending_required",
                                "official_credit_before_execution"},
                               "formal counts");
    flow400_require(
        flow400_number(counts.get("cases")) == 4u &&
            flow400_number(counts.get("aggregate_complete_after_all_official_runs")) == 33u &&
            flow400_number(counts.get("aggregate_pending_after_all_official_runs")) == 367u &&
            flow400_number(counts.get("ultimate_complete_required")) == 400u &&
            flow400_number(counts.get("ultimate_pending_required")) == 0u &&
            flow400_number(counts.get("official_credit_before_execution")) == 0u,
        "Flow400 TypedSource4 formal counts");
    const auto cases = root.get("cases");
    flow400_require(cases.is_arr() && cases.size() == 4u, "Flow400 TypedSource4 case count");
    std::array<bool, 400> ordinals{};
    std::array<bool, 4> identities{};
    cases.iter([&](JsonVal row) {
        flow400_typed_source4_case_shape(row, work);
        const auto ordinal = flow400_number(row.get("base_authority").get("ordinal"));
        flow400_require(!ordinals[ordinal], "Flow400 TypedSource4 duplicate source ordinal");
        ordinals[ordinal] = true;
        const auto id = flow400_text(row.get("case_id"));
        const auto index = id == "original_000"   ? 0u
                           : id == "original_282" ? 1u
                           : id == "original_374" ? 2u
                                                  : 3u;
        flow400_require(!identities[index], "Flow400 TypedSource4 duplicate case identity");
        identities[index] = true;
    });
    flow400_require(ordinals[0] && ordinals[282] && ordinals[374] && ordinals[375] &&
                        std::ranges::all_of(identities, [](bool value) { return value; }),
                    "Flow400 TypedSource4 selected ordinal closure");
}

inline Flow400TypedSource4Documents flow400_typed_source4_documents() {
    Flow400TypedSource4Documents documents;
    documents.asset_work.spend(4u * sizeof(json::JsonDoc) + 4u * sizeof(OracleWork) + 16384u);
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) /
                      "fixtures/mermaid_native/flow400_typed_source4_formal_supplement.v1.json";
    const auto size = std::filesystem::file_size(path);
    flow400_require(size == kFlow400TypedSource4AssetBytes,
                    "Flow400 TypedSource4 immutable asset bytes");
    documents.asset_work.product(static_cast<std::size_t>(size), 3u);
    const auto bytes = flow400_read(path);
    flow400_require(digest(bytes) == kFlow400TypedSource4AssetSha,
                    "Flow400 TypedSource4 immutable asset SHA");
    auto parsed = json::parse(bytes);
    flow400_require(parsed.has_value(), "Flow400 TypedSource4 asset JSON");
    documents.supplement = std::move(*parsed);
    flow400_typed_source4_root(documents.supplement.root(), documents.asset_work);
    return documents;
}

struct BoundFlow400TypedSource4Authority {
    JsonVal authority, typed_case;
    Flow400TypedSource4Kind kind = Flow400TypedSource4Kind::Base;
    std::size_t exact_rejections = 0u;
};

inline BoundFlow400TypedSource4Authority
flow400_typed_source4_bound_authority(JsonVal base, JsonVal config21, JsonVal typed_source4,
                                      JsonVal fixture, std::size_t ordinal, std::size_t global,
                                      std::string_view source, OracleWork& work) {
    work.spend(8192u + 8u * sizeof(std::size_t) + 4u * sizeof(BoundFlow400TypedSource4Authority));
    const auto prior =
        flow400_config21_bound_authority(base, config21, fixture, ordinal, global, source, work);
    const auto cases = typed_source4.get("cases");
    flow400_require(cases.is_arr() && cases.size() == 4u,
                    "Flow400 TypedSource4 selection denominator");
    JsonVal selected;
    std::size_t matches = 0u;
    cases.iter([&](JsonVal candidate) {
        work.spend(512u);
        if (flow400_number(candidate.get("base_authority").get("ordinal")) == ordinal) {
            selected = candidate;
            ++matches;
        }
    });
    flow400_require(matches <= 1u, "Flow400 TypedSource4 duplicate source selection");
    if (matches == 0u)
        return {prior.row, JsonVal{},
                prior.supplemental ? Flow400TypedSource4Kind::Config21
                                   : Flow400TypedSource4Kind::Base,
                0u};
    flow400_require(!prior.supplemental && base.get("model_json").is_null(),
                    "Flow400 TypedSource4 overlaps prior authority");
    flow400_typed_source4_case_shape(selected, work);
    const auto authority = selected.get("base_authority");
    flow400_bind(authority, fixture, ordinal, global, source, work);
    return {authority, selected, Flow400TypedSource4Kind::TypedSource4,
            flow400_typed_source4_expected(selected)};
}

inline std::optional<Model>
flow400_typed_source4_model(const BoundFlow400TypedSource4Authority& selected, OracleWork& work) {
    if (selected.kind != Flow400TypedSource4Kind::TypedSource4)
        return flow400_model(selected.authority, work);
    const auto bytes = flow400_text(selected.typed_case.get("typed_model_json"));
    flow400_require(digest(bytes) == flow400_text(selected.typed_case.get("typed_model_sha256")),
                    "Flow400 TypedSource4 selected Model SHA");
    work.product(bytes.size(), 32u);
    work.spend(4u * sizeof(json::JsonDoc) + 8192u);
    auto document = json::parse(bytes);
    flow400_require(document.has_value(), "Flow400 TypedSource4 selected Model JSON");
    return flow_typed_shape_model(document->root(), work);
}

}  // namespace loom::test::mermaid_flow
