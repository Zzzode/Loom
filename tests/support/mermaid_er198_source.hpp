// Test-private RFC 0034 one-row cohort source binding: the single whole-null
// ER206 original (prior ordinal 198, manifest global 647) that carries the
// unique two-debt set {active_config, raw_entity_group_kind_collision...}.
// Phase 1 (bounded layout-Work allowance) now lets its whole source draw; this
// header is the renderer-free identity/selection foundation for the Phase 2
// owned proof. No producer, filesystem write, FTXUI capture or mutable
// process-global state. JsonVal views borrow immutable caller-owned JsonDocs.
//
// Boundary with the prior cohorts: this header only INCLUDES the sha-frozen
// review helpers (mermaid_er107_authority.hpp, mermaid_er_groupedge_source.hpp);
// it never edits them. A compile-time assertion proves the one cohort path is
// disjoint from BOTH the 107 ER107 pins and the six RFC 0033 group-edge pins
// (RFC 0034 authority design, verifier assertion (ii)).
#pragma once
import std;

#include "mermaid_er107_authority.hpp"
#include "mermaid_er_groupedge_source.hpp"

namespace loom::test::mermaid_er198 {
namespace er107auth = loom::test::mermaid_er107::authority;
namespace groupedge = loom::test::mermaid_er_groupedge;

// The frozen ER206 prior this single-row cohort supplements; never edited.
inline constexpr std::string_view kPriorPath =
    "mermaid_native/er206_native_whole_source_authority.config62.v1.json";
inline constexpr std::string_view kPriorSha =
    "76d54ecbc725e59645501f7079f9387b272172d584703f0ea72f143c83c469db";
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

// Identity-only pin. The proposal-derived whole-scene / model / witness /
// binding / asset / manifest / prior-case shas are added by the later proposal
// increment once that artifact is curated; renderer-free selection and the
// structural disjointness guarantees need only the immutable raw identity.
struct Pin {
    std::string_view path;
    int global, prior_ordinal;
    std::size_t raw_bytes;
    std::string_view raw_sha;
};

inline constexpr std::array<Pin, kCohortSize> kPins{{
    {"er-diagram/visual-check/"
     "nested-er-subgraphs-with-direction-override-handdrawn-look.mmd",
     647, 198, 532,
     "c48919288f56b2cb5bae9050ff75afa41faca478d0d77c0f6cc442e86465954b"},
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

}  // namespace loom::test::mermaid_er198
