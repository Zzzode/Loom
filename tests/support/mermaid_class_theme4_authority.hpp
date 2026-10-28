// Proposal-only source authority for four Official Class theme originals.
// This header pins candidate facts and reusable control expectations; it does
// not add a Native/Official consumer or grant Mermaid drawing credit.
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace loom::test::mermaid_class_theme4::authority {

struct SourcePin {
    std::size_t application_ordinal;
    std::size_t manifest_global_ordinal;
    std::string_view fixture;
    std::string_view theme;
    std::size_t source_bytes;
    std::string_view source_sha256;
    std::string_view source_git_blob;
    std::string_view graph_body_sha256;
};

inline constexpr std::string_view kProposalFile =
    "mermaid_native/class_theme4_source_model_proposal.v1.json";
inline constexpr std::size_t kProposalBytes = 74188u;
inline constexpr std::string_view kProposalSha256 =
    "e763f471a5e23066d9e879626592e6570192c157249735771915a5ded62db9f3";
inline constexpr std::string_view kGraphModelSha256 =
    "6e09a7e5fd950b894a383d166a00461e34722f2886e4d4d2638ef7d50e9288c4";
inline constexpr std::string_view kSharedGraphBodySha256 =
    "7667dc74e5ce8d3eec7e41a2917c9f4fdbfbbafb121d5550e684e88a283adc14";
inline constexpr std::size_t kSharedGraphBodyBytes = 789u;
inline constexpr std::size_t kEntityCount = 20u;
inline constexpr std::size_t kMemberCount = 11u;
inline constexpr std::size_t kRelationCount = 19u;
inline constexpr std::size_t kMultiplicityCount = 2u;
inline constexpr std::size_t kUnresolvedAccessibilityFacts = 2u;

inline constexpr std::string_view kClass285FactsFile =
    "mermaid_native/official_class285_source_facts.candidate.v1.json";
inline constexpr std::size_t kClass285FactsBytes = 1622120u;
inline constexpr std::string_view kClass285FactsSha256 =
    "9916a1cbc21a9d7e6808c471511cb803f205b9b208019ed3fb7f52add6521da2";
inline constexpr std::string_view kClass285MappingFile =
    "mermaid_native/official_class285_source_mapping_approval.v1.json";
inline constexpr std::size_t kClass285MappingBytes = 74234u;
inline constexpr std::string_view kClass285MappingSha256 =
    "5d4cef76b3bf2dd56f9a3f3d9c955654d4c970dc40b2030aefbcb8f9005d6715";
inline constexpr std::string_view kOfficialManifestFile =
    "mermaid_official/manifest.json";
inline constexpr std::size_t kOfficialManifestBytes = 1272942u;
inline constexpr std::string_view kOfficialManifestSha256 =
    "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b";

inline constexpr std::array<SourcePin, 4> kSources{{
    {263u, 411u, "class-diagram/visual-check/dark-theme.mmd", "dark", 1125u,
     "df1262f42e99476d5e45c0ec6ecb934f14bc117908df948a4af48cc8a5372e51",
     "afe9c24e614dafabb31a469276c740d45eb73224", kSharedGraphBodySha256},
    {264u, 412u, "class-diagram/visual-check/default-theme.mmd", "default", 1131u,
     "760b8a5bd13cd3f57ed310ac4a0f291522b61a02e4738c620b0bb0e98310d241",
     "38f472c3b45613c3babeee454e5caf521193d2ad", kSharedGraphBodySha256},
    {271u, 419u, "class-diagram/visual-check/forest-theme.mmd", "forest", 1129u,
     "abac4c611f8dc5170a164b7662ff37e167297552f13bc5b14d83c151cd5e45d5",
     "beefb46adc22d58d0b383dd90a89a32faa89526a", kSharedGraphBodySha256},
    {284u, 432u, "class-diagram/visual-check/neutral-theme.mmd", "neutral", 1131u,
     "daa2d2b04208bde399fdc53975c887a328311e98095dbdf169e51d51ee8cb60f",
     "f81102895b4cc6b13a9621809d272d9d7fc3c5c7", kSharedGraphBodySha256},
}};

// Expected execution counts when the existing Class481 semantic mutation
// helper is later run only after the complete positive geometry passes.
struct MutationCount {
    std::string_view kind;
    std::size_t count;
};
inline constexpr std::array<MutationCount, 10> kExpectedMutations{{
    {"complete_header", 20u},
    {"header_literal", 20u},
    {"member_value", 11u},
    {"member_erasure", 11u},
    {"relation_label", 19u},
    {"endpoint_marker", 38u},
    {"stroke", 19u},
    {"third_endpoint", 19u},
    {"relation_erasure", 19u},
    {"orphan_ink", 1u},
}};

inline constexpr std::array<std::string_view, 2> kUnresolvedAccessibility{{
    "accTitle: This is a title",
    "accDescr: This is a description",
}};

static_assert(kSources.size() == 4u);
static_assert(kEntityCount == 4u + 16u);
static_assert(kRelationCount == 3u + 8u + 8u);
static_assert(kMultiplicityCount == 2u);
static_assert(kExpectedMutations.size() == 10u);

} // namespace loom::test::mermaid_class_theme4::authority
