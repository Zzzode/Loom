// Strict counting boundary; bounded metadata/counter work is reserved by run().
#pragma once
#include "mermaid_class_defined6_run.hpp"

namespace loom::test::mermaid_class_defined6 {
inline constexpr std::size_t kProofLimit = 4096u;

inline std::string proof(std::size_t selected, std::size_t ordinal, std::size_t global,
                         const Run& run) {
    require(selected < kPins.size(), "ClassDefined6 proof index");
    const auto& pin = kPins[selected];
    const auto& p = run.progress;
    loom::utils::json::JsonMutDoc document;
    auto root = document.object();
    root.set("schema", kContract);
    root.set("source_authority", kAuthority);
    root.set("fixture", pin.fixture);
    root.set("source_ordinal", static_cast<std::int64_t>(ordinal));
    root.set("global_ordinal", static_cast<std::int64_t>(global));
    root.set("source_bytes", static_cast<std::int64_t>(pin.source_bytes));
    root.set("expected_source_sha256", pin.source_sha);
    root.set("actual_source_sha256", run.actual_source_sha);
    root.set("model_bytes", static_cast<std::int64_t>(pin.model_bytes));
    root.set("expected_Model_sha256", pin.model_sha);
    root.set("decoded_Model_sha256", run.decoded_model_sha);
    root.set("asset_sha256", kAssetSha);
    root.set("phase", static_cast<int>(p.phase));
    root.set("captured", static_cast<std::int64_t>(p.captured));
    root.set("published", p.published);
    root.set("bound", p.bound);
    root.set("decoded", p.decoded);
    root.set("wide_outcome", p.outcomes[0]);
    root.set("restored_outcome", p.outcomes[1]);
    root.set("restored_equal", p.restored_equal);
    root.set("narrow_source", p.narrow_source);
    root.set("narrow_policy", "glyph_background_flags_hyperlink; caller_language_foreground");
    root.set("negative_expected", static_cast<std::int64_t>(kPhysicalNegatives));
    root.set("negative_actual", static_cast<std::int64_t>(p.actual_negatives));
    root.set("negative_credited", static_cast<std::int64_t>(p.credited_negatives));
    root.set("exception", p.exception);
    root.set("inconclusive", p.inconclusive);
    root.set("resource", p.resource);
    root.set("semantic_failure", p.semantic_failure);
    root.set("admitted", p.admitted);
    auto balances = document.array();
    for (const auto left : p.remaining)
        balances.append(document.number(left));
    root.add("remaining", balances);
    root.set("foreign_assertions_certified", 0);
    document.set_root(root);
    auto text = document.to_string();
    require(text.size() <= kProofLimit, "ClassDefined6 bounded publication proof");
    return text;
}

struct ProofBinding {
    std::string_view fixture, track, type, source_sha, model_sha, authority, schema;
    std::size_t ordinal = 0u, global = 0u;
    std::array<std::string_view, 3> status;
    bool trusted_owner = false, has_errors = false;
    unsigned trusted_failure_flags = 0u;
};

struct Counted {
    bool observed = false, bound = false, decoded = false, admitted = false, resource = false;
};

inline bool hash_text(std::string_view text) {
    return text.size() == 64u && std::ranges::all_of(text, [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

inline Counted count(JsonVal value, const ProofBinding& row) {
    require(row.trusted_owner && row.schema == kContract && row.authority == kAuthority &&
                row.track == "original" && row.type == "class-diagram",
            "ClassDefined6 trusted original domain/provenance");
    const auto selected = index(row.fixture);
    require(selected.has_value(), "ClassDefined6 pinned fixture identity");
    const auto& pin = kPins[*selected];
    require(row.ordinal == pin.ordinal && row.global == pin.global,
            "ClassDefined6 original ordinal/global identity");
    constexpr std::array<std::string_view, 32> fields{{"schema",
                                                       "source_authority",
                                                       "fixture",
                                                       "source_ordinal",
                                                       "global_ordinal",
                                                       "source_bytes",
                                                       "expected_source_sha256",
                                                       "actual_source_sha256",
                                                       "model_bytes",
                                                       "expected_Model_sha256",
                                                       "decoded_Model_sha256",
                                                       "asset_sha256",
                                                       "phase",
                                                       "captured",
                                                       "published",
                                                       "bound",
                                                       "decoded",
                                                       "wide_outcome",
                                                       "restored_outcome",
                                                       "restored_equal",
                                                       "narrow_source",
                                                       "narrow_policy",
                                                       "negative_expected",
                                                       "negative_actual",
                                                       "negative_credited",
                                                       "exception",
                                                       "inconclusive",
                                                       "resource",
                                                       "semantic_failure",
                                                       "admitted",
                                                       "remaining",
                                                       "foreign_assertions_certified"}};
    require(value.is_obj() && value.size() == fields.size(), "ClassDefined6 exact proof fields");
    std::array<bool, fields.size()> seen{};
    value.iter_obj([&](JsonVal key, JsonVal) {
        require(key.is_str() && key.as_str().size() <= 64u, "ClassDefined6 bounded proof key");
        const auto at = std::ranges::find(fields, key.as_str());
        require(at != fields.end(), "ClassDefined6 unknown proof field");
        const auto n = static_cast<std::size_t>(at - fields.begin());
        require(!seen[n], "ClassDefined6 duplicate proof field");
        seen[n] = true;
    });
    const auto text = [&](std::string_view key, std::size_t maximum = 512u) {
        const auto field = value.get(key);
        require(field.is_str() && field.as_str().size() <= maximum,
                "ClassDefined6 bounded proof text");
        return field.as_str();
    };
    const auto number = [&](std::string_view key) {
        const auto field = value.get(key);
        require(field.is_int(), "ClassDefined6 integer proof type");
        return field.as_int();
    };
    const auto yes = [&](std::string_view key) {
        const auto field = value.get(key);
        require(field.is_bool(), "ClassDefined6 boolean proof type");
        return field.as_bool();
    };
    require(text("schema") == kContract && text("source_authority") == kAuthority &&
                text("fixture") == pin.fixture &&
                number("source_ordinal") == static_cast<std::int64_t>(pin.ordinal) &&
                number("global_ordinal") == static_cast<std::int64_t>(pin.global) &&
                number("source_bytes") == static_cast<std::int64_t>(pin.source_bytes) &&
                text("expected_source_sha256") == pin.source_sha &&
                number("model_bytes") == static_cast<std::int64_t>(pin.model_bytes) &&
                text("expected_Model_sha256") == pin.model_sha && text("asset_sha256") == kAssetSha,
            "ClassDefined6 complete pinned source/Model/asset proof");
    const auto actual = text("actual_source_sha256", 64u),
               model = text("decoded_Model_sha256", 64u);
    require(actual == row.source_sha && model == row.model_sha,
            "ClassDefined6 publication/observation identity agreement");
    require((actual.empty() || hash_text(actual)) && (model.empty() || hash_text(model)),
            "ClassDefined6 actual digest types");
    const auto phase = number("phase"), captured = number("captured"),
               wide = number("wide_outcome"), restored = number("restored_outcome");
    require(phase >= 0 && phase <= 8 && captured >= 0 && captured <= 5 && wide >= -1 && wide <= 2 &&
                restored >= -1 && restored <= 2,
            "ClassDefined6 finite phase/outcome domain");
    const bool published = yes("published"), bound = yes("bound"), decoded = yes("decoded"),
               equal = yes("restored_equal"), narrow = yes("narrow_source"),
               exception = yes("exception"), unknown = yes("inconclusive"),
               resource = yes("resource"), semantic = yes("semantic_failure"),
               admitted = yes("admitted");
    require(row.trusted_failure_flags ==
                (static_cast<unsigned>(resource) | (static_cast<unsigned>(unknown) << 1u) |
                 (static_cast<unsigned>(semantic) << 2u)),
            "ClassDefined6 trusted failure classification");
    require(!published || captured == 5, "ClassDefined6 published five-capture hierarchy");
    require(!bound || published, "ClassDefined6 bind/publication hierarchy");
    require(decoded == bound, "ClassDefined6 complete owning decode/bind hierarchy");
    require(!decoded || (actual == pin.source_sha && model == pin.model_sha),
            "ClassDefined6 decoded immutable identities");
    require(decoded || model.empty(), "ClassDefined6 undecoded Model may not claim identity");
    require((wide == -1 && restored == -1) || decoded,
            "ClassDefined6 geometry requires complete source Model");
    require((wide != 1 && restored != 1) || semantic, "ClassDefined6 mismatch reason consistency");
    require((wide != 2 && restored != 2) || unknown, "ClassDefined6 unknown reason consistency");
    require(!resource || unknown, "ClassDefined6 resource is inconclusive");
    require(wide == -1 || phase >= 4, "ClassDefined6 wide outcome phase");
    require(restored == -1 || (phase >= 5 && wide == 0),
            "ClassDefined6 restored outcome hierarchy");
    require(!equal || (phase >= 5 && wide == 0 && restored == 0),
            "ClassDefined6 equality requires two complete widths");
    require(!narrow || (phase >= 6 && equal), "ClassDefined6 narrow source phase");
    require(text("narrow_policy") ==
                    "glyph_background_flags_hyperlink; caller_language_foreground" &&
                number("foreign_assertions_certified") == 0,
            "ClassDefined6 exact source/foreign policies");
    const auto negative = number("negative_actual"), credited = number("negative_credited");
    require(number("negative_expected") == static_cast<std::int64_t>(kPhysicalNegatives) &&
                negative >= 0 && negative <= static_cast<std::int64_t>(kPhysicalNegatives) &&
                credited == (admitted ? static_cast<std::int64_t>(kPhysicalNegatives) : 0),
            "ClassDefined6 decisive negative counts");
    require(negative == 0 || (phase >= 7 && narrow),
            "ClassDefined6 negatives require complete positive protocol");
    const auto balances = value.get("remaining");
    require(balances.is_arr() && balances.size() == 8u, "ClassDefined6 bounded phase balances");
    std::int64_t previous = static_cast<std::int64_t>(kSceneLimit - kReportReserve);
    bool missing = false;
    for (std::size_t i = 0; i < 8u; ++i) {
        const auto field = balances.at(i);
        require(field.is_int(), "ClassDefined6 balance integer");
        const auto left = field.as_int();
        require(left >= -1 && left <= static_cast<std::int64_t>(kSceneLimit - kReportReserve),
                "ClassDefined6 balance bounds");
        if (left == -1) {
            missing = true;
            require(phase <= static_cast<std::int64_t>(i),
                    "ClassDefined6 missing completed-phase balance");
        } else {
            require(!missing && left <= previous && phase >= static_cast<std::int64_t>(i),
                    "ClassDefined6 monotonic reached-phase balances");
            previous = left;
        }
    }
    if (admitted) {
        require(phase == 8 && captured == 5 && published && bound && decoded && wide == 0 &&
                    restored == 0 && equal && narrow && !exception && !unknown && !resource &&
                    !semantic && negative == static_cast<std::int64_t>(kPhysicalNegatives) &&
                    !row.has_errors,
                "ClassDefined6 complete source admission proof");
        require(row.status[0] == "drawn" && row.status[1] == "source_fallback" &&
                    row.status[2] == "drawn",
                "ClassDefined6 admitted width statuses");
    } else {
        require(exception && row.has_errors && credited == 0,
                "ClassDefined6 failed proof has no admission/negative credit");
        for (const auto state : row.status)
            require(state == "error",
                    "ClassDefined6 failed owned row retained a drawing or source status");
    }
    return {captured == 5, bound, decoded, admitted, resource};
}
}  // namespace loom::test::mermaid_class_defined6
