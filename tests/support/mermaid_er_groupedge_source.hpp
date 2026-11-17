// Test-private RFC 0033 ER group-edge cohort source binding. The parallel,
// independently reviewed six-case cohort for whole-null group-collision
// originals (ER206 prior ordinals 26, 27, 29, 39, 196, 197). No producer,
// filesystem write, FTXUI capture or mutable process-global state. JsonVal
// views borrow immutable caller-owned JsonDocs.
//
// Boundary with ER107: this header only INCLUDES the sha-frozen ER107 review
// helpers (mermaid_er107_authority.hpp, mermaid_relations_oracle.hpp); it never
// edits them. A compile-time assertion proves the six-path cohort and the
// ER107 authority kPins path set are disjoint (RFC 0033 §4.3).
#pragma once
import std;
import loom.serdes.json;
import loom.crypto.crypto;

#include "mermaid_er107_authority.hpp"

namespace loom::test::mermaid_er_groupedge {
using JsonVal = loom::utils::json::JsonVal;
namespace rel = loom::test::mermaid_relations;
namespace er107auth = loom::test::mermaid_er107::authority;

inline constexpr std::string_view kProposalPath =
    "mermaid_native/er_groupedge_complete_proposal.v1.json";
inline constexpr std::string_view kProposalSha =
    "49af23a885bf1162b52724177cda1eacfec3c96683128e3f4f7c4b4450f44404";
inline constexpr std::string_view kPriorPath =
    "mermaid_native/er206_native_whole_source_authority.config62.v1.json";
inline constexpr std::string_view kPriorSha =
    "76d54ecbc725e59645501f7079f9387b272172d584703f0ea72f143c83c469db";
inline constexpr std::string_view kManifestSha =
    "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b";
inline constexpr std::size_t kCohortSize = 6;
inline constexpr std::size_t kMaxRawBytes = 65536;

struct Pin {
    std::string_view path;
    int global, prior_ordinal;
    std::size_t raw_bytes;
    std::string_view raw_sha, whole_scene_sha, model_sha, witness_sha, binding_sha,
        asset_case_sha, manifest_record_sha, prior_case_sha;
};

// Literal component pins generated exclusively from the approved six-case
// proposal artifact. They never trust digests recomputed by a caller's mutant.
inline constexpr std::array<Pin, kCohortSize> kPins{{
    {"er-diagram/nightly-check/should-render-relationships-between-subgraphs.mmd",
     475, 26, 487,
     "fadc9d0bdf07012ac321975361052b75790d3c4e3460cbbc4b318f0bd4720c01",
     "ab87f7202ed6b92f8aeec62e8a129d01e32ab2bac00689fd6b39a1b9d1c7a01f",
     "c9ffb477af8563765d4c8cb3da30491f25966b85beee9fd5d71f3211a350d089",
     "b2404dbb8aa587994276855352e0e3d49ecfeac6556fcb6db284c883cfb4145d",
     "60928efeeab0ec49cc8ca4dad1e2dcc4e122efe19b4e5f7e577d2ea89234c694",
     "87bae37fc580ea591e2037318004300db5f93a81ecc606f67559215bff68034f",
     "a5a1ba9bbd288731fbd5f7707ca8a5324ac6158c65a56938ef289877d521a3f3",
     "73a0ffaeee2669ba4935ede75b6a52365da75ff0c0f0725ca099d2ffa3bdf0a1"},
    {"er-diagram/nightly-check/should-render-subgraphs-with-direction-override.mmd",
     476, 27, 548,
     "3e4b45535efd2b4752dfb30c0b6613908790f9c9ccb4be95596a83bb325ef7de",
     "4e8b3cd507d46ed9f653d0f808d1ad4805ec5f72a39dd7d9cf34bf6e82be5dc7",
     "009484024e10d22b93a39bf5531ec381c0df185c2f8cb417ed79771bbdf5c381",
     "b9ec765c8a66efcadbf5805a221c9464333a4187a15017adf2674685e77dbdff",
     "9280e8fd5b7b5bd5d0b290666b5467d1777ff613004b7545ee85e1ae9fd30292",
     "45397e2e13b6ab4f91fa6f96effbc886f51d88f39640578ed8f5f431b6254915",
     "53de8d771ba90fc86f1f0c2c72852cb3978d2a859b5ae2fdda57b6708e414bdf",
     "9bffb7c249a3b2c0077f78bfdfc7e69a4551d184430566ff2c37fcbd247fbe5d"},
    {"er-diagram/nightly-check/should-render-subgraphs-with-quoted-ids.mmd",
     478, 29, 470,
     "4c4ccd826bd285c3be2bf9e9a8aa551105b195c5dad2628b4b22fbd408050615",
     "a09491c48cdcdf09fed572141f14fa359e34eaab73c3a542582eae008d4fbab9",
     "f450fc9db9962bd71ba25927f826d9083e58257c47fb30e9af0bace566ed97fa",
     "f74c034a8db695cfc4814cd75f490c4d2b4c09bafe19b8fc639398d806e8ceeb",
     "9e54fcca0fc7ec396e45ae246c0ae66e05d89f30080da169f5c8fc02bf2389ff",
     "0981d0334677b67a8a97beae0869736db929d46f5a2beebd144c30cd8a6855cc",
     "bdede2a4b3853f63d3e2381cdadb457cab3783695d2fb9bcf13ff32ee3f126c8",
     "b749a0076fbb2336151ab6abf939369fbf431276d1bd2d442bc62f7e05427cb4"},
    {"er-diagram/pr-check/should-render-relationships-between-subgraph-and-entity.mmd",
     488, 39, 445,
     "8288e93b205ebff44324fbe5f4624c50ed046f5bfa6afcaf85ba0892ffb85b05",
     "5b462a4cfc504488ba05b3818234a903aaaa98f664dc6d9bb7f59ea217ea5097",
     "7d6a6149e0c6e5884e1dc516bb129fa5b41567a556f613d42548ff201f7e2404",
     "eda3d1f8eeecbb8b606ff29f569f04699e21a1f5b604b1880790a6c18d867c49",
     "f7e83c8609e77c19ab09b675353d1f652f24aa13e779e221696881c323526541",
     "a86ac67fc507a89a8012181b6b6ecb0636472e384add5aaacfbb1312dcfc2f52",
     "eaed95f13f9b073d8cfd11c278b6afc53bc52e354f0e836caf398f87a4b7ad5c",
     "4c83062f9800f9abe4d2b8e8ee69abfe4c9e2f52652b66dc3135b8ef1ab8ee77"},
    {"er-diagram/visual-check/nested-er-subgraphs-with-direction-override-default.mmd",
     645, 196, 479,
     "3b3865d9798cfa12e2d906cfc6180ba1f2a8cdd6702ab8baddfdfe20d0d48369",
     "79aa8ab0ceb36c1dac039627f0b2e2cf21057e238fc96bd324ccd45322c77f1b",
     "6ef310fe9d57e71740f27eb05ec2f69ad019dc5b2b9bccc03f56834dabb33973",
     "bd45de105a457760654da4bfc23282df69fecf765dfd48ffb870824d96ef16c1",
     "b38305ee780ff75565bbeb3c6330841a93d6078660fe5b7f70dd805375f10ba3",
     "b475e5e3e9c21be26af4abc88654fbe59e66a73cbb663c5bb9ff50d98fd98ca2",
     "48d8acf476df73b812ab43cd0f1a975c150f16559a02f77146223e8eca5d43b2",
     "1d4508397525aff27763e851bea911a25ce1a94a04920fab9eea579e4406c6b4"},
    {"er-diagram/visual-check/nested-er-subgraphs-with-direction-override-elk-layout.mmd",
     646, 197, 524,
     "80539748fd037b424743f4b29552e93050ef27baf92618f016add5f615091f27",
     "79aa8ab0ceb36c1dac039627f0b2e2cf21057e238fc96bd324ccd45322c77f1b",
     "c6e0ebab31e3c601dd1a6564b399011953b561715a8425f492595bee8ba56e98",
     "ccdf89498361f569024b7da79890509aed75d186177665b2d88dd273ea1b41fd",
     "f0e3607c4cad0fb2b920abb35977126d4a6977ccb40b7695e49c77ee66fb8322",
     "e41348ba5cfc73494045d94735032f95ad6f32a1f142bd30452045c5cb873d82",
     "167fcd3281f6d76445cf3d3f4062e7effc70f3a7240859576d9f0aa180bfadd6",
     "06c9eecbd76054015f803f614f968e5e4c2b6d715fee15184f52a14665f2619e"},
}};

namespace detail {
inline void require(bool ok, std::string_view message) {
    rel::class481_require(ok, message);
}

inline std::string canonical(JsonVal value, rel::Work& work) {
    return rel::class481_canonical(value, work);
}

inline std::string digest(JsonVal value) {
    rel::Work work;
    return loom::utils::crypto::sha256(rel::class481_canonical(value, work));
}

// Production cardinality enum -> terminal marker, the closed four-marker table
// (er_term_marker in mermaid_relations_core_impl.cpp).
inline std::string_view cardinality_marker(std::string_view card) {
    if (card == "ONLY_ONE") return "card:one";
    if (card == "ZERO_OR_ONE") return "card:zero-one";
    if (card == "ZERO_OR_MORE") return "card:zero-many";
    return "card:one-many";  // ONE_OR_MORE
}

inline std::string trim_role(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) return {};
    const auto last = text.find_last_not_of(" \t\r");
    return std::string(text.substr(first, last - first + 1));
}
}  // namespace detail

// Renderer-free cohort selection. A path resolves at most once and only when its
// compiled raw sha also matches; a swapped source cannot select.
inline std::optional<std::size_t> select(std::string_view path, std::string_view raw_sha) {
    for (std::size_t i = 0; i < kPins.size(); ++i)
        if (kPins[i].path == path) {
            detail::require(kPins[i].raw_sha == raw_sha,
                            "source: ERGroupEdge compiled path/raw selector");
            return i;
        }
    return std::nullopt;
}

// Compile-time structural disjointness: no path may belong to both the ER107
// authority cohort and the six-case group-edge cohort.
constexpr bool cohort_paths_disjoint() {
    for (const auto& ge : kPins)
        for (const auto& e107 : er107auth::kPins)
            if (ge.path == e107.path) return false;
    return true;
}
static_assert(cohort_paths_disjoint(),
              "ERGroupEdge cohort and ER107 authority kPins must be path-disjoint");

struct FieldCounts {
    std::size_t entities = 0, relationships = 0, subgraphs = 0, directions = 0,
                attributes = 0, witnesses = 0;
};

// Independently derive the structural field counts from the source_model leg.
inline FieldCounts derive_field_counts(JsonVal source_model, JsonVal witnesses) {
    FieldCounts out;
    out.entities = source_model.get("entities").size();
    out.relationships = source_model.get("relationships").size();
    out.subgraphs = source_model.get("subgraphs").size();
    out.directions = source_model.get("direction_declarations").size();
    source_model.get("entities").iter(
        [&](JsonVal e) { out.attributes += e.get("attributes").size(); });
    out.witnesses = witnesses.size();
    return out;
}

// Live witness checks over the immutable raw bytes (RFC 0033 §4.2 NIT-3):
// bounded span, raw-byte equality, slice digest and start/end line recompute.
// A pinned component sha alone cannot prove the span lands on the right bytes.
inline void verify_live_witnesses(JsonVal witnesses, std::string_view raw, rel::Work& work) {
    std::size_t previous_ordinal = 0;
    bool saw_full_source = false;
    witnesses.iter([&](JsonVal w) {
        work.spend(64u);
        const auto start = static_cast<std::size_t>(w.get_int("source_byte_start"));
        const auto end = static_cast<std::size_t>(w.get_int("source_byte_end"));
        detail::require(start <= end && end <= raw.size(),
                        "source: ERGroupEdge bounded witness byte range");
        const std::string slice(raw.substr(start, end - start));
        detail::require(slice == w.get_string("source_lexeme"),
                        "source: ERGroupEdge witness raw-byte equality");
        detail::require(loom::utils::crypto::sha256(slice) ==
                            w.get_string("source_slice_sha256"),
                        "source: ERGroupEdge witness slice digest");
        detail::require(w.get_int("source_utf8_bytes") == static_cast<int64_t>(end - start),
                        "source: ERGroupEdge witness byte count");
        std::size_t line_start = 1, line_end = 1;
        for (std::size_t i = 0; i < start && i < raw.size(); ++i)
            if (raw[i] == '\n') ++line_start;
        for (std::size_t i = 0; i < std::max(start, end - 1) && i < raw.size(); ++i)
            if (raw[i] == '\n') ++line_end;
        detail::require(w.get_int("source_line_start") == static_cast<int64_t>(line_start) &&
                            w.get_int("source_line_end") == static_cast<int64_t>(line_end),
                        "source: ERGroupEdge witness line recomputation");
        detail::require(w.get_int("ordinal") == static_cast<int64_t>(previous_ordinal),
                        "source: ERGroupEdge dense global witness ordinals");
        ++previous_ordinal;
        if (w.get_string("kind") == "complete_original_source") {
            detail::require(slice == raw, "source: ERGroupEdge whole-original witness");
            saw_full_source = true;
        }
    });
    detail::require(saw_full_source, "source: ERGroupEdge complete-original witness present");
}

// Field-by-field dual-track cross-check between the production whole_scene_model
// and the independent source_model, at ER107 authority.hpp:994-1039 granularity:
// scene direction/population, per-entity id/label/owner/shape, per-relation
// endpoints/role/cardinality/stroke, per-group id/parent/members/direction.
inline void cross_check_scene(JsonVal whole_scene, JsonVal source_model, rel::Work& work) {
    const auto nodes = source_model.get("entities");
    const auto edges = source_model.get("relationships");
    const auto groups = source_model.get("subgraphs");
    const auto wm_entities = whole_scene.get("entities");
    const auto wm_relations = whole_scene.get("relations");
    const auto wm_groups = whole_scene.get("groups");
    detail::require(whole_scene.get_string("family") == "er" &&
                        whole_scene.get_string("direction") ==
                            source_model.get_string("source_db_direction_under_fresh_instance") &&
                        wm_entities.size() == nodes.size() &&
                        wm_relations.size() == edges.size() &&
                        wm_groups.size() == groups.size() &&
                        whole_scene.get("notes").size() == 0,
                    "crosscheck: ERGroupEdge scene population and direction");
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        work.spend(256u);
        const auto s = nodes.at(i), n = wm_entities.at(i);
        std::string label = s.get_string("source_db_label");
        for (std::size_t j = 0; j < s.get("alias_attempts").size(); ++j) {
            work.spend(32u);
            const auto value = s.get("alias_attempts").at(j).get_string("value");
            if (!value.empty()) {
                label = value;
                break;
            }
        }
        const auto owner = s.get("source_parent_subgraph").is_null()
                               ? std::string{}
                               : s.get_string("source_parent_subgraph");
        // The whole-scene keeps the authored logical id; source_db_id is the
        // dense post-cleanup database id (group renumbering per D287) and is
        // deliberately not the terminal identity.
        detail::require(n.get_string("id") == s.get_string("id") &&
                            n.get_string("label") == label &&
                            n.get_string("owner") == owner &&
                            n.get_string("shape") == "box" &&
                            n.get("rows").size() == s.get("attributes").size(),
                        "crosscheck: ERGroupEdge entity id/label/owner/shape/attributes");
    }
    for (std::size_t i = 0; i < edges.size(); ++i) {
        work.spend(256u);
        const auto s = edges.at(i), r = wm_relations.at(i);
        detail::require(r.get_int("order") == s.get_int("ordinal") &&
                            r.get_string("source") == s.get_string("entityA") &&
                            r.get_string("target") == s.get_string("entityB"),
                        "crosscheck: ERGroupEdge relation order and endpoints");
        detail::require(r.get_string("start") ==
                            detail::cardinality_marker(s.get_string("source_left_cardinality")) &&
                            r.get_string("end") ==
                            detail::cardinality_marker(s.get_string("source_right_cardinality")) &&
                            r.get_string("stroke") == s.get_string("source_getData_pattern"),
                        "crosscheck: ERGroupEdge relation cardinality and stroke");
        const std::string role = detail::trim_role(s.get_string("role_literal"));
        detail::require(r.get_string("label") == role,
                        "crosscheck: ERGroupEdge relation role label");
    }
    for (std::size_t i = 0; i < groups.size(); ++i) {
        work.spend(256u);
        const auto s = groups.at(i), g = wm_groups.at(i);
        const auto parent = s.get("source_parent_subgraph").is_null()
                                ? std::string{}
                                : s.get_string("source_parent_subgraph");
        detail::require(g.get_string("id") == s.get_string("id") &&
                            g.get_string("parent") == parent &&
                            g.get("members").size() == s.get("members").size() &&
                            g.get_string("direction") == "",
                        "crosscheck: ERGroupEdge group id/parent/members and direction isolation");
        for (std::size_t k = 0; k < s.get("members").size(); ++k)
            detail::require(g.get("members").at(k).as_str() ==
                                s.get("members").at(k).as_str(),
                            "crosscheck: ERGroupEdge group member ordering");
    }
}

// Verify one proposal case against the live raw bytes and the compiled pin
// table. Every embedded component digest is re-derived; nothing trusts the
// proposal's own hashes.
inline void verify_case(std::size_t index, JsonVal case_value, std::string_view raw,
                        rel::Work& work) {
    detail::require(index < kCohortSize, "source: ERGroupEdge case ordinal range");
    const auto& pin = kPins[index];
    work.spend(raw.size() * 8u + 4096u);
    detail::require(case_value.get_string("path") == pin.path &&
                        case_value.get_int("manifest_global_ordinal") == pin.global &&
                        case_value.get_int("prior_ER206_ordinal") == pin.prior_ordinal &&
                        case_value.get_int("source_bytes") ==
                            static_cast<int64_t>(pin.raw_bytes) &&
                        case_value.get_string("source_sha256") == pin.raw_sha,
                    "source: ERGroupEdge identity pins");
    detail::require(loom::utils::crypto::sha256(raw) == pin.raw_sha &&
                        raw.size() == pin.raw_bytes,
                    "source: ERGroupEdge live raw sha and byte count");
    const auto check = [&](std::string_view key, std::string_view expected) {
        detail::require(detail::digest(case_value.get(key)) == expected,
                        "source: ERGroupEdge component digest re-derivation");
    };
    check("whole_scene_model", pin.whole_scene_sha);
    check("source_model", pin.model_sha);
    check("source_witnesses", pin.witness_sha);
    check("source_binding", pin.binding_sha);
    check("source_asset_preimage", pin.asset_case_sha);
    check("manifest_record", pin.manifest_record_sha);
    verify_live_witnesses(case_value.get("source_witnesses"), raw, work);
    cross_check_scene(case_value.get("whole_scene_model"),
                      case_value.get("source_model"), work);
    const auto counts = derive_field_counts(case_value.get("source_model"),
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
                    "source: ERGroupEdge independently derived field counts");
}
}  // namespace loom::test::mermaid_er_groupedge
