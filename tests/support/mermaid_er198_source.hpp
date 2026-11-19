// Test-private RFC 0034 one-row cohort source binding: the single whole-null
// ER206 original (prior ordinal 198, manifest global 647) that carries the
// unique two-debt set {active_config, raw_entity_group_kind_collision...}.
// Phase 1 (bounded layout-Work allowance) now lets its whole source draw; this
// header carries the renderer-free identity/selection foundation and the
// proposal component binding for the Phase 2 owned proof. No producer,
// filesystem write, FTXUI capture or mutable process-global state. JsonVal
// views borrow immutable caller-owned JsonDocs.
//
// Boundary with the prior cohorts: this header only INCLUDES the sha-frozen
// review helpers (mermaid_er107_authority.hpp, mermaid_er_groupedge_source.hpp);
// it never edits them. A compile-time assertion proves the one cohort path is
// disjoint from BOTH the 107 ER107 pins and the six RFC 0033 group-edge pins
// (RFC 0034 authority design, verifier assertion (ii)).
#pragma once
import std;
import loom.serdes.json;
import loom.crypto.crypto;

#include "mermaid_er107_authority.hpp"
#include "mermaid_er_groupedge_source.hpp"

namespace loom::test::mermaid_er198 {
using JsonVal = loom::utils::json::JsonVal;
namespace rel = loom::test::mermaid_relations;
namespace er107auth = loom::test::mermaid_er107::authority;
namespace groupedge = loom::test::mermaid_er_groupedge;

// The frozen ER206 prior this single-row cohort supplements; never edited.
inline constexpr std::string_view kPriorPath =
    "mermaid_native/er206_native_whole_source_authority.config62.v1.json";
inline constexpr std::string_view kPriorSha =
    "76d54ecbc725e59645501f7079f9387b272172d584703f0ea72f143c83c469db";
// The one-row RFC 0034 proposal (a distinct artifact from the RFC 0033
// six-case proposal) and the shared immutable manifest.
inline constexpr std::string_view kProposalPath =
    "mermaid_native/er198_handdrawn_complete_proposal.v1.json";
inline constexpr std::string_view kProposalSha =
    "4e2ba98698265da17c5f85c2230bf2fe16fa71fd3ad73d1aeff1d2e48d8ade11";
inline constexpr std::string_view kManifestSha =
    "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b";
inline constexpr std::size_t kCohortSize = 1;
inline constexpr std::size_t kMaxRawBytes = 65536;

// The exact two-debt set that selects ordinal 198 out of all 206 ER206 rows.
// Measured unique across the prior: the six RFC 0033 group-collision rows
// carry only the collision debt; every active_config row either pairs it with
// rich-* presentation / active_entity_presentation / class-definition debts
// or carries active_config alone, so no other row carries this exact pair.
inline constexpr std::string_view kDebtActiveConfig = "active_config";
inline constexpr std::string_view kDebtGroupCollision =
    "raw_entity_group_kind_collision_not_representable_without_loss";

// Full component pin. The seven proposal-derived shas are generated exclusively
// from the approved one-row proposal artifact (increment 2a); they never trust
// digests recomputed by a caller's mutant. Field order matches the RFC 0033
// group-edge Pin so the binding helpers stay interchangeable.
struct Pin {
    std::string_view path;
    int global, prior_ordinal;
    std::size_t raw_bytes;
    std::string_view raw_sha, whole_scene_sha, model_sha, witness_sha, binding_sha,
        asset_case_sha, manifest_record_sha, prior_case_sha;
};

inline constexpr std::array<Pin, kCohortSize> kPins{{
    {"er-diagram/visual-check/"
     "nested-er-subgraphs-with-direction-override-handdrawn-look.mmd",
     647, 198, 532,
     "c48919288f56b2cb5bae9050ff75afa41faca478d0d77c0f6cc442e86465954b",
     "79aa8ab0ceb36c1dac039627f0b2e2cf21057e238fc96bd324ccd45322c77f1b",
     "5b23722eac983ad0980feaa610da4510d1543b15ca9bee1696d45570b9002838",
     "623e5592df66df9e08751fd82eee54326e41e44a8b7bffe8e59d03c163dc44e0",
     "c4fe679daee3541f24e207c41c84393fdaa87158d41703bf789b3f1d6b7735e8",
     "42a7777d12ab5ca8c9f9d278f7fb9db74bb83880efe57dfd925b53fc8ed575be",
     "e7a10965ab8d538d2fcb87e232a83e09638f21e81feab1b4fe5dd3109f06b419",
     "9e72201c469ec1dbff65e7ea594d82518512635508328cfad7ad0b233bad51ce"},
}};

namespace detail {
inline void require(bool ok, std::string_view message) {
    if (!ok) throw std::runtime_error(std::string(message));
}
}  // namespace detail

// Renderer-free cohort selection, same contract as the ER107/group-edge
// selectors: a path resolves at most once and only when its compiled raw sha
// also matches; a swapped source cannot select (it throws rather than silently
// resolving a different cohort member).
inline std::optional<std::size_t> select(std::string_view path,
                                         std::string_view raw_sha) {
    for (std::size_t i = 0; i < kPins.size(); ++i)
        if (kPins[i].path == path) {
            detail::require(kPins[i].raw_sha == raw_sha,
                            "source: ER198 compiled path/raw selector");
            return i;
        }
    return std::nullopt;
}

// Compile-time structural disjointness against BOTH prior cohorts. The RFC
// 0033 assertion only checked ER107; RFC 0034 must also be exclusive with the
// six-case group-edge cohort so a row can never resolve through two proofs.
constexpr bool cohort_paths_disjoint() {
    for (const auto& pin : kPins) {
        for (const auto& e107 : er107auth::kPins)
            if (pin.path == e107.path) return false;
        for (const auto& ge : groupedge::kPins)
            if (pin.path == ge.path) return false;
    }
    return true;
}
static_assert(cohort_paths_disjoint(),
              "ER198 cohort must be path-disjoint from ER107 and ERGroupEdge");

// Bind one proposal case to its immutable pin and re-derive every component
// digest from the reparsed proposal tree (nothing trusts the proposal's own
// hashes). The witness/cross-check/census mechanics are ER206-generic, so they
// reuse the RFC 0033 group-edge helpers rather than duplicating them; only the
// identity pin and cohort bounds are ER198-specific.
inline void verify_case(std::size_t index, JsonVal case_value, std::string_view raw,
                        rel::Work& work) {
    detail::require(index < kCohortSize, "source: ER198 case ordinal range");
    const auto& pin = kPins[index];
    work.spend(raw.size() * 8u + 4096u);
    detail::require(case_value.get_string("path") == pin.path &&
                        case_value.get_int("manifest_global_ordinal") == pin.global &&
                        case_value.get_int("prior_ER206_ordinal") == pin.prior_ordinal &&
                        case_value.get_int("source_bytes") ==
                            static_cast<int64_t>(pin.raw_bytes) &&
                        case_value.get_string("source_sha256") == pin.raw_sha,
                    "source: ER198 identity pins");
    detail::require(loom::utils::crypto::sha256(raw) == pin.raw_sha &&
                        raw.size() == pin.raw_bytes,
                    "source: ER198 live raw sha and byte count");
    const auto check = [&](std::string_view key, std::string_view expected) {
        detail::require(groupedge::detail::digest(case_value.get(key)) == expected,
                        "source: ER198 component digest re-derivation");
    };
    check("whole_scene_model", pin.whole_scene_sha);
    check("source_model", pin.model_sha);
    check("source_witnesses", pin.witness_sha);
    check("source_binding", pin.binding_sha);
    check("source_asset_preimage", pin.asset_case_sha);
    check("manifest_record", pin.manifest_record_sha);
    groupedge::verify_live_witnesses(case_value.get("source_witnesses"), raw, work);
    groupedge::cross_check_scene(case_value.get("whole_scene_model"),
                                 case_value.get("source_model"), work);
    const auto counts = groupedge::derive_field_counts(case_value.get("source_model"),
                                                       case_value.get("source_witnesses"));
    const auto pinned_counts = case_value.get("field_counts");
    detail::require(pinned_counts.get_int("entities") ==
                        static_cast<int64_t>(counts.entities) &&
                        pinned_counts.get_int("relationships") ==
                            static_cast<int64_t>(counts.relationships) &&
                        pinned_counts.get_int("subgraphs") ==
                            static_cast<int64_t>(counts.subgraphs) &&
                        pinned_counts.get_int("direction_declarations") ==
                            static_cast<int64_t>(counts.directions) &&
                        pinned_counts.get_int("attributes") ==
                            static_cast<int64_t>(counts.attributes) &&
                        pinned_counts.get_int("source_witness_count") ==
                            static_cast<int64_t>(counts.witnesses),
                    "source: ER198 independently derived field counts");
}

}  // namespace loom::test::mermaid_er198
