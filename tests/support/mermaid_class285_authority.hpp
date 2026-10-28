// Test-private immutable OfficialClass285 source authority.
// Existing152/133 history, dagre38 and classifier26 bindings are unchanged.
// Definitions alone perform no I/O; each original caller owns local documents.
#pragma once

#include "mermaid_relations_oracle.hpp"

import loom.crypto.crypto;

namespace loom::test::mermaid_relations {

inline std::string digest(std::string_view text) {
    return loom::utils::crypto::sha256(text);
}

inline std::string read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

inline bool er_present_same_json(JsonVal left, JsonVal right, Work& work, std::size_t depth = 0) {
    work.spend();
    if (depth > 16 || !left.valid() || !right.valid()) return false;
    if (left.is_obj()) {
        if (!right.is_obj() || left.size() != right.size()) return false;
        bool equal = true;
        left.iter_obj([&](JsonVal key, JsonVal value) {
            work.spend();
            if (!equal) return;
            std::size_t left_count = 0, right_count = 0;
            JsonVal paired;
            left.iter_obj([&](JsonVal other, JsonVal) {
                work.spend();
                work.spend(key.as_str().size());
                work.spend(other.as_str().size());
                if (key.as_str() == other.as_str()) ++left_count;
            });
            right.iter_obj([&](JsonVal other, JsonVal candidate) {
                work.spend();
                work.spend(key.as_str().size());
                work.spend(other.as_str().size());
                if (key.as_str() == other.as_str()) {
                    ++right_count;
                    paired = candidate;
                }
            });
            equal = left_count == 1 && right_count == 1 &&
                    er_present_same_json(value, paired, work, depth + 1);
        });
        return equal;
    }
    if (left.is_arr()) {
        if (!right.is_arr() || left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) {
            work.spend();
            work.spend(i);
            work.spend(i);
            if (!er_present_same_json(left.at(i), right.at(i), work, depth + 1)) return false;
        }
        return true;
    }
    if (left.is_str()) {
        if (!right.is_str()) return false;
        work.spend(left.as_str().size());
        work.spend(right.as_str().size());
        return left.as_str() == right.as_str();
    }
    if (left.is_bool()) return right.is_bool() && left.as_bool() == right.as_bool();
    if (left.is_null()) return right.is_null();
    if (left.is_num()) {
        if (left.is_int() != right.is_int() || left.is_double() != right.is_double()) return false;
        work.spend(64);
        return left.to_string() == right.to_string();
    }
    return false;
}

inline constexpr std::string_view kOfficialClass285File =
    "mermaid_native/official_class285_source_facts.candidate.v1.json";
inline constexpr std::string_view kOfficialClass285Sha =
    "9916a1cbc21a9d7e6808c471511cb803f205b9b208019ed3fb7f52add6521da2";
inline constexpr std::size_t kOfficialClass285Bytes = 1622120u;
inline constexpr std::string_view kOfficialClass285ApprovalFile =
    "mermaid_native/official_class285_source_mapping_approval.v1.json";
inline constexpr std::string_view kOfficialClass285ApprovalSha =
    "5d4cef76b3bf2dd56f9a3f3d9c955654d4c970dc40b2030aefbcb8f9005d6715";
inline constexpr std::size_t kOfficialClass285ApprovalBytes = 74234u;
inline constexpr std::string_view kOfficialClass285ManifestSha =
    "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b";
inline constexpr std::size_t kOfficialClass285ManifestBytes = 1272942u;
inline constexpr std::string_view kOfficialClass285DagreFile =
    "mermaid_native/official_class285_dagre_whole_Model_supplement.source_approved.v1.json";
inline constexpr std::string_view kOfficialClass285DagreSha =
    "211bf4a93bb73a52a86f2ed57da272d46f48e5097908453e56604c0488b29790";
inline constexpr std::size_t kOfficialClass285DagreBytes = 129968u;
inline constexpr std::size_t kOfficialClass285DagreAbsent = 38u;

inline std::array<std::size_t, 285> official_class285_dagre_index(JsonVal extra, Work& work) {
    work.spend(3u * sizeof(std::array<std::size_t, 285>) + 2048u);
    const auto cases = extra.get("cases");
    class481_require(cases.is_arr() && cases.size() == 38u,
                     "OfficialClass285 dagre exact38 supplement rows");
    std::array<std::size_t, 285> index{};
    index.fill(kOfficialClass285DagreAbsent);
    std::size_t previous = 0;
    for (std::size_t i = 0; i < 38u; ++i) {
        work.spend(i + 512u);
        const auto row = cases.at(i), ordinal = row.get("application_ordinal");
        class481_require(ordinal.is_int() && ordinal.as_int() >= 0 && ordinal.as_int() < 285,
                         "OfficialClass285 dagre original ordinal domain");
        const auto n = static_cast<std::size_t>(ordinal.as_int());
        class481_require(index[n] == kOfficialClass285DagreAbsent,
                         "OfficialClass285 dagre duplicate original ordinal");
        class481_require(i == 0u || n > previous, "OfficialClass285 dagre original manifest order");
        previous = n;
        index[n] = i;
    }
    return index;
}

inline constexpr std::string_view kOfficialClass285ClassifierFile =
    "mermaid_native/official_class285_classifier_whole_Model_supplement.source_approved.v1.json";
inline constexpr std::string_view kOfficialClass285ClassifierSha =
    "13e26fb28bd7914dee969a74778907833e195c572c2f90c2e8857f67785cc8f0";
inline constexpr std::size_t kOfficialClass285ClassifierBytes = 126983u;
inline constexpr std::size_t kOfficialClass285ClassifierAbsent = 26u;

inline std::array<std::size_t, 285> official_class285_classifier_index(JsonVal extra, Work& work) {
    work.spend(3u * sizeof(std::array<std::size_t, 285>) + 2048u);
    const auto cases = extra.get("cases");
    class481_require(cases.is_arr() && cases.size() == 26u,
                     "OfficialClass285 classifier exact26 supplement rows");
    std::array<std::size_t, 285> index{};
    index.fill(kOfficialClass285ClassifierAbsent);
    std::size_t previous = 0;
    for (std::size_t i = 0; i < 26u; ++i) {
        work.spend(i + 512u);
        const auto row = cases.at(i), ordinal = row.get("application_ordinal");
        class481_require(ordinal.is_int() && ordinal.as_int() >= 0 && ordinal.as_int() < 285,
                         "OfficialClass285 classifier original ordinal domain");
        const auto n = static_cast<std::size_t>(ordinal.as_int());
        class481_require(index[n] == kOfficialClass285ClassifierAbsent,
                         "OfficialClass285 classifier duplicate original ordinal");
        class481_require(i == 0u || n > previous,
                         "OfficialClass285 classifier original manifest order");
        previous = n;
        index[n] = i;
    }
    return index;
}

inline constexpr std::string_view kOfficialClass285UnusedFile =
    "mermaid_native/"
    "official_class285_unused_classDef3_whole_Model_supplement.source_approved.v1.json";
inline constexpr std::string_view kOfficialClass285UnusedSha =
    "5e4af31fb79cc7c39a1ca826a447e018f94259c17500f34a0e39919cf50d0864";
inline constexpr std::size_t kOfficialClass285UnusedBytes = 11648u;
inline constexpr std::size_t kOfficialClass285UnusedAbsent = 3u;

inline std::array<std::size_t, 285> official_class285_unused_index(JsonVal extra, Work& work) {
    work.spend(4u * 285u * sizeof(std::size_t) + 4096u);
    class481_keys(
        extra, {"schema", "source_review", "immutable_v1_sha256", "qualification", "cases"}, work);
    class481_require(class481_text(extra.get("schema"), work, 128) ==
                             "official_class285_unused_classDef3_source_approved_proposal/v1" &&
                         class481_text(extra.get("immutable_v1_sha256"), work, 64) ==
                             kOfficialClass285Sha,
                     "OfficialClass285 unused exact source authority");
    static_cast<void>(class481_text(extra.get("qualification"), work, 2048));
    const auto review = extra.get("source_review");
    class481_keys(review, {"path", "sha256", "scope"}, work);
    class481_require(class481_text(review.get("sha256"), work, 64) ==
                         "2594af2b030f752297aec643e32b3ff6a9863d40c96d9b6aad4f7dc5cac6032f",
                     "OfficialClass285 unused independent source review");
    static_cast<void>(class481_text(review.get("path"), work, 1024));
    static_cast<void>(class481_text(review.get("scope"), work, 1024));
    const auto cases = extra.get("cases");
    class481_require(cases.is_arr() && cases.size() == 3u,
                     "OfficialClass285 unused exact3 supplement rows");
    std::array<std::size_t, 285> index{};
    index.fill(kOfficialClass285UnusedAbsent);
    constexpr std::array<std::size_t, 3> originals{{98u, 160u, 206u}};
    for (std::size_t i = 0; i < 3u; ++i) {
        work.spend(i + 256u);
        const auto ordinal = cases.at(i).get("application_ordinal");
        class481_require(ordinal.is_int() && ordinal.as_int() >= 0 && ordinal.as_int() < 285,
                         "OfficialClass285 unused original ordinal domain");
        const auto n = static_cast<std::size_t>(ordinal.as_int());
        class481_require(index[n] == kOfficialClass285UnusedAbsent,
                         "OfficialClass285 unused duplicate original ordinal");
        class481_require(n == originals[i], "OfficialClass285 unused exact original three order");
        index[n] = i;
    }
    return index;
}

inline constexpr std::string_view kClassNotes18File =
    "mermaid_native/class_notes18_test_first.paper_clearrow_r3.v1.json";
inline constexpr std::string_view kClassNotes18Sha =
    "397b76f72b79f7bd1c233028d5189b015737216f4c7347220859b32a7eff9e2a";
inline constexpr std::size_t kClassNotes18Bytes = 133443u;
inline constexpr std::array<std::string_view, 4> kClassNotes18PaperPins{
    {"ae8e6f837756d9f3282cf119b7834d97db9f1c0dce27fd8bf54bd6e34edb906d",
     "e66f4d1b6d993f5d11bb0f60cb2ac056fe7823c16a412607d39d65e6a64ff513",
     "1738f04b6de04cfe8d9613157a7df88fa8181707f04f0e61c9627def8eb6d227",
     "24b6ae4daf3ddaf0691ccb2649cc80d0d31403b84ed1982853916f476532abcc"}};
inline constexpr std::array<std::pair<std::size_t, std::string_view>, 18> kClassNotes18OriginalPins{
    {{34u, "65bbbba34e25d2357afe699b25db7bcf354bea0e85f0c6567ae67227198628f4"},
     {35u, "8d7a78e7e5da281d12f4c56c1d49055e080c4b3f1773e1af7246c6dfa54e1d8d"},
     {36u, "91ea28cdda612a7b4485a9445428c0dc27860cba2944bbb4f04c2721a556f551"},
     {78u, "e411387be0cee57279276b499afc6410692ddf9329cb19abb432070e47733b04"},
     {95u, "c0954f9c6ae140d0e1b79307892af8dea98d009b2c19f82962ebabf34eca4c39"},
     {116u, "81d8c2e0c03847b92e445ecd0703a6861f9fc72a56259c1f6b316725d8ef1d71"},
     {138u, "42b22bd5a07cb9443a8addfd1a759987a9ad05127bbfc5e34b8c1bffeed5fbaa"},
     {154u, "506c517ed5c381dad58900203fb9d2067653cb033a235d18b5e5a18d440b7f65"},
     {155u, "999538f5baa0136d45a338a056b508b4ecf5a06d011d5a9defc16e62349d5d5a"},
     {181u, "93a9aaca84b59404e1672fc944fef09ba3605bd2dd3d363474109bde05dd0d22"},
     {195u, "fa5632833db5fca70f2df856b21ccbae80cb67a9bcd02b0297ad3325ff1595f0"},
     {202u, "057955f277b03d75164193855fb4ad4eceaa6ee2cc303cc99099593f89018658"},
     {272u, "50c3546dde1d59e3a54d08485479ff16a82155dc03af78db02975733ba6f5278"},
     {273u, "24d99045e06c6065aafc54d6b3c7f2e21976bedf73a640538b894b45daed25a7"},
     {274u, "e03dffb60a42dec8d4559473efb629aff451d6a91135d3aa7d8a6a6f9e57c80a"},
     {275u, "e8df88170d779aed90b9ca128c1d9091a2878971a26b88503edd4b2e0d18be52"},
     {276u, "61a0d98e1b775763120ccea804aa9119d4a774de78e97a68fb6b2c23ab714b57"},
     {277u, "67137a46227a9f9e6deedad8ecbf18e68fad494c4ff902e6d2ea6d4738748564"}}};

inline json::JsonDoc official_class285_notes_document(Work& setup) {
    setup.spend(kClassNotes18Bytes * 3u + 32768u + 4u * sizeof(json::JsonDoc) + 8192u);
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) / "fixtures" / kClassNotes18File;
    class481_require(std::filesystem::file_size(path) == kClassNotes18Bytes,
                     "ClassNotes18 exact fixture stat");
    const auto bytes = read(path);
    class481_require(bytes.size() == kClassNotes18Bytes && digest(bytes) == kClassNotes18Sha,
                     "ClassNotes18 fixture bytes/hash");
    auto parsed = json::parse(bytes);
    class481_require(parsed.has_value(), "ClassNotes18 fixture JSON");
    const auto root = parsed->root();
    class481_keys(root,
                  {"schema", "qualification", "source_candidate_sha256",
                   "source_mapping_review_sha256", "originals", "papers"},
                  setup);
    class481_require(root.get("schema").as_str() == "class_notes18_test_first/v1" &&
                         root.get("source_candidate_sha256").as_str() ==
                             "358784eb86cd57cbcbb09ed81e8628704ed0ef9934af0f66d8efa28217cb8f34" &&
                         root.get("source_mapping_review_sha256").as_str() ==
                             "5203db85e0b1c421079e5dd41b1fd8f430df2f3908faf9da877a1ff1ba0ba040",
                     "ClassNotes18 independently reviewed source proposal lineage");
    class481_require(root.get("papers").is_arr() && root.get("papers").size() == 4u &&
                         root.get("originals").is_arr() && root.get("originals").size() == 18u,
                     "ClassNotes18 complete finite arrays");
    return std::move(*parsed);
}

inline void official_class285_notes_binding(JsonVal row, std::size_t i, JsonVal base, Work& work) {
    class481_require(i < kClassNotes18OriginalPins.size(), "ClassNotes18 original index");
    const auto& pin = kClassNotes18OriginalPins[i];
    class481_require(row.get("application_ordinal").is_int() &&
                         row.get_int("application_ordinal") ==
                             static_cast<std::int64_t>(pin.first) &&
                         digest(class481_canonical(row, work)) == pin.second,
                     "ClassNotes18 exact whole original row");
    class481_require(digest(class481_canonical(base, work)) ==
                             row.get("base_case_canonical_sha256").as_str() &&
                         base.get("whole_native_model_candidate").is_null(),
                     "ClassNotes18 original whole-null/history binding");
    class481_require(er_present_same_json(row.get("source"), base.get("source"), work) &&
                         er_present_same_json(row.get("class_presentation"),
                                              base.get("class_presentation_candidate"), work),
                     "ClassNotes18 complete raw source/presentation binding");
    const auto source = class481_text(row.get("source"), work, 2048u);
    work.spend(source.size() * 3u + 128u);
    class481_require(digest(source) == row.get("source_sha256").as_str(),
                     "ClassNotes18 complete raw source digest");
    const auto bindings = row.get("note_source_bindings"),
               notes = row.get("whole_Model").get("notes");
    class481_require(bindings.is_arr() && bindings.size() == notes.size(),
                     "ClassNotes18 Note cardinality binding");
    for (std::size_t n = 0; n < bindings.size(); ++n) {
        work.spend(n + 64u);
        const auto b = bindings.at(n);
        class481_require(b.get("sourceIndex").is_int() &&
                             b.get_int("sourceIndex") == static_cast<std::int64_t>(n),
                         "ClassNotes18 Note sourceIndex order");
    }
    class481_require(digest(class481_canonical(row.get("whole_Model"), work)) ==
                         row.get("whole_Model_sha256").as_str(),
                     "ClassNotes18 full proposed Model digest");
}

inline constexpr std::size_t kOfficialClass285NotesAbsent = 18u;

inline std::array<std::size_t, 285> official_class285_notes_index(JsonVal root, Work& work) {
    work.spend(6u * 285u * sizeof(std::size_t) + 4096u);
    const auto rows = root.get("originals");
    class481_require(rows.is_arr() && rows.size() == 18u,
                     "OfficialClass285 Notes exact18 original rows");
    std::array<std::size_t, 285> index{};
    index.fill(kOfficialClass285NotesAbsent);
    for (std::size_t i = 0; i < 18u; ++i) {
        work.spend(i + 256u);
        const auto row = rows.at(i), ordinal = row.get("application_ordinal");
        class481_require(ordinal.is_int() && ordinal.as_int() >= 0 && ordinal.as_int() < 285,
                         "OfficialClass285 Notes original ordinal domain");
        const auto at = static_cast<std::size_t>(ordinal.as_int());
        class481_require(index[at] == kOfficialClass285NotesAbsent,
                         "OfficialClass285 Notes duplicate original ordinal");
        class481_require(at == kClassNotes18OriginalPins[i].first,
                         "OfficialClass285 Notes exact original eighteen order");
        index[at] = i;
    }
    return index;
}

struct OfficialClass285Documents {
    json::JsonDoc facts, approval, dagre, classifier, unused, notes;
    std::array<std::size_t, 285> dagre_index, classifier_index, unused_index, notes_index;
    std::size_t setup_left;
};

inline OfficialClass285Documents official_class285_documents() {
    Work setup;
    setup.spend((kOfficialClass285Bytes + kOfficialClass285ApprovalBytes +
                 kOfficialClass285DagreBytes + kOfficialClass285ClassifierBytes +
                 kOfficialClass285UnusedBytes) *
                    3u +
                98304u + 4u * sizeof(OfficialClass285Documents) + 3u * 285u * sizeof(bool) +
                16u * sizeof(int) + 128u);
    const auto base = std::filesystem::path(LOOM_TESTS_DIR) / "fixtures";
    class481_require(std::filesystem::file_size(base / kOfficialClass285File) ==
                             kOfficialClass285Bytes &&
                         std::filesystem::file_size(base / kOfficialClass285ApprovalFile) ==
                             kOfficialClass285ApprovalBytes,
                     "OfficialClass285 immutable document stat");
    const auto bytes = read(base / kOfficialClass285File),
               approval_bytes = read(base / kOfficialClass285ApprovalFile);
    class481_require(bytes.size() == kOfficialClass285Bytes &&
                         digest(bytes) == kOfficialClass285Sha &&
                         approval_bytes.size() == kOfficialClass285ApprovalBytes &&
                         digest(approval_bytes) == kOfficialClass285ApprovalSha,
                     "OfficialClass285 immutable document bytes/hash");
    auto facts = json::parse(bytes), approval = json::parse(approval_bytes);
    class481_require(facts.has_value() && approval.has_value(), "OfficialClass285 document JSON");
    const auto root = facts->root(), review = approval->root();
    class481_require(root.get_string("schema") == "official_class285_source_facts_candidate/v1" &&
                         root.get("cases").is_arr() && root.get("cases").size() == 285u &&
                         root.get_int("applications") == 285,
                     "OfficialClass285 complete original candidate");
    class481_keys(review,
                  {"schema", "qualification", "candidate_bytes", "candidate_sha256",
                   "source_mapping_approval", "original_manifest", "applications",
                   "complete_source_Models", "whole_null_Models",
                   "original_foreign_assertions_certified", "cases"},
                  setup);
    class481_require(review.get_string("schema") ==
                             "official_class285_source_mapping_approval/v1" &&
                         review.get_int("candidate_bytes") ==
                             static_cast<std::int64_t>(kOfficialClass285Bytes) &&
                         review.get_string("candidate_sha256") == kOfficialClass285Sha &&
                         review.get_int("applications") == 285 &&
                         review.get_int("complete_source_Models") == 152 &&
                         review.get_int("whole_null_Models") == 133 &&
                         review.get_int("original_foreign_assertions_certified") == 0 &&
                         review.get("cases").is_arr() && review.get("cases").size() == 285u,
                     "OfficialClass285 exact approval denominator");
    const auto gate = review.get("source_mapping_approval");
    class481_require(
        gate.get_string("verdict") ==
                "LIMITED_ALL285_SOURCE_MAPPING_APPROVE_FOR_SEPARATE_NATIVE_TEST_FIRST_AUTHORING" &&
            gate.get_string("reviewer") == "/root/terminal_union" &&
            gate.get_string("review_sha256") ==
                "457b198c64aca2ad8f65c648e726bb9d074427fe12b234cae68686a1639c4793" &&
            gate.get_string("review_lock_sha256") ==
                "34923a93b0433b671f6243ea97be235685ed72a3038a6ce11fd664f4816fc592",
        "OfficialClass285 real independent source-mapping gate");
    class481_require(root.get("mapping_review").get_string("verdict") == "pending" &&
                         root.get_int("original_foreign_assertions_certified") == 0,
                     "OfficialClass285 immutable proposal history and foreign debt");
    const auto dagre_path = base / kOfficialClass285DagreFile;
    class481_require(std::filesystem::file_size(dagre_path) == kOfficialClass285DagreBytes,
                     "OfficialClass285 dagre immutable supplement stat");
    const auto dagre_bytes = read(dagre_path);
    class481_require(dagre_bytes.size() == kOfficialClass285DagreBytes &&
                         digest(dagre_bytes) == kOfficialClass285DagreSha,
                     "OfficialClass285 dagre immutable supplement hash");
    auto dagre = json::parse(dagre_bytes);
    class481_require(dagre.has_value(), "OfficialClass285 dagre supplement JSON");
    const auto extra = dagre->root();
    class481_keys(extra,
                  {"schema", "author", "qualification", "base_candidate", "base_mapping_review",
                   "source_control_mapping_gate", "source_controls_conservation_record",
                   "supplement_mapping_review", "applications_obligatory",
                   "base_complete_Models_unchanged", "dagre_obligation_applications",
                   "additional_complete_Model_candidates",
                   "whole_nulls_remaining_after_this_single_mapping_delta",
                   "original_foreign_assertions_certified", "cases", "source_only_candidate"},
                  setup);
    class481_require(extra.get_string("schema") ==
                             "official_class285_dagre_whole_Model_supplement_source_approved/v1" &&
                         extra.get_int("applications_obligatory") == 285 &&
                         extra.get_int("base_complete_Models_unchanged") == 152 &&
                         extra.get_int("dagre_obligation_applications") == 60 &&
                         extra.get_int("additional_complete_Model_candidates") == 38 &&
                         extra.get_int("whole_nulls_remaining_after_this_single_mapping_delta") ==
                             95 &&
                         extra.get_int("original_foreign_assertions_certified") == 0,
                     "OfficialClass285 dagre exact source supplement denominator");
    const auto approved = extra.get("supplement_mapping_review");
    class481_keys(approved, {"verdict", "reviewer", "review_sha256", "review_lock_sha256"}, setup);
    class481_require(approved.get_string("verdict") ==
                             "LIMITED_ALL38_COMPLETE_SOURCE_MODEL_SUPPLEMENT_MAPPING_APPROVE_FOR_"
                             "SEPARATE_NATIVE_AUTHORING" &&
                         approved.get_string("reviewer") == "/root/terminal_union" &&
                         approved.get_string("review_sha256") ==
                             "adf2e33dbec8688d3e0ae25f9feba29f73902ec3ed494aba25673f0e3dfbe586" &&
                         approved.get_string("review_lock_sha256") ==
                             "c0e95302d98d4bb6ead94cb45a7b90f20696d85501375f1f5307938b44931460",
                     "OfficialClass285 dagre real independent source mapping review");
    class481_require(extra.get("base_candidate").get_int("bytes") ==
                             static_cast<std::int64_t>(kOfficialClass285Bytes) &&
                         extra.get("base_candidate").get_string("sha256") == kOfficialClass285Sha &&
                         extra.get("base_mapping_review").get_string("sha256") ==
                             "457b198c64aca2ad8f65c648e726bb9d074427fe12b234cae68686a1639c4793" &&
                         extra.get("source_only_candidate").get_int("bytes") == 129348 &&
                         extra.get("source_only_candidate").get_string("sha256") ==
                             "2005a44ab5c04e3be637cb321ae7e2a8c78f936ecfe0402d7349a667ce6de686",
                     "OfficialClass285 dagre immutable base and candidate lineage");
    const auto index = official_class285_dagre_index(extra, setup);
    const auto classifier_path = base / kOfficialClass285ClassifierFile;
    class481_require(std::filesystem::file_size(classifier_path) ==
                         kOfficialClass285ClassifierBytes,
                     "OfficialClass285 classifier immutable supplement stat");
    const auto classifier_bytes = read(classifier_path);
    class481_require(classifier_bytes.size() == kOfficialClass285ClassifierBytes &&
                         digest(classifier_bytes) == kOfficialClass285ClassifierSha,
                     "OfficialClass285 classifier immutable supplement hash");
    auto classifier = json::parse(classifier_bytes);
    class481_require(classifier.has_value(), "OfficialClass285 classifier supplement JSON");
    const auto classified = classifier->root();
    class481_keys(
        classified,
        {"schema", "author", "qualification", "base_candidate", "base_mapping_review",
         "classifier_source_design_gate", "classifier_source_facts", "approved_dagre_supplement",
         "approved_dagre_mapping_gate", "supplement_mapping_review", "applications_obligatory",
         "base_complete_Models_unchanged", "previous_dagre_complete_Models_unchanged",
         "additional_complete_Model_candidates", "whole_nulls_remaining_after_source_mapping",
         "original_foreign_assertions_certified", "cases", "source_only_candidate"},
        setup);
    class481_require(
        classified.get_string("schema") ==
                "official_class285_classifier_whole_Model_supplement_source_approved/v1" &&
            classified.get_int("applications_obligatory") == 285 &&
            classified.get_int("base_complete_Models_unchanged") == 152 &&
            classified.get_int("previous_dagre_complete_Models_unchanged") == 38 &&
            classified.get_int("additional_complete_Model_candidates") == 26 &&
            classified.get_int("whole_nulls_remaining_after_source_mapping") == 69 &&
            classified.get_int("original_foreign_assertions_certified") == 0,
        "OfficialClass285 classifier exact source supplement denominator");
    const auto classifier_gate = classified.get("supplement_mapping_review");
    class481_keys(classifier_gate, {"verdict", "reviewer", "review_sha256", "review_lock_sha256"},
                  setup);
    class481_require(classifier_gate.get_string("verdict") ==
                             "APPROVE_LIMITED_SOURCE_MODEL_AND_NATIVE_TEST_FIRST_AUTHORING" &&
                         classifier_gate.get_string("reviewer") == "/root/corpus_oracle" &&
                         classifier_gate.get_string("review_sha256") ==
                             "40c5b4189a908580e9525a34f2b1e43b5d79bf0f1f4676685ae87f307b1db9e9" &&
                         classifier_gate.get_string("review_lock_sha256") ==
                             "a646a8ef96191a3b3eb55541990e37905149db6f8663cadfde575485da75bb85",
                     "OfficialClass285 classifier real independent source mapping review");
    class481_require(classified.get("base_candidate").get_int("bytes") ==
                             static_cast<std::int64_t>(kOfficialClass285Bytes) &&
                         classified.get("base_candidate").get_string("sha256") ==
                             kOfficialClass285Sha &&
                         classified.get("base_mapping_review").get_string("sha256") ==
                             "457b198c64aca2ad8f65c648e726bb9d074427fe12b234cae68686a1639c4793" &&
                         classified.get("approved_dagre_supplement").get_string("sha256") ==
                             extra.get("source_only_candidate").get_string("sha256") &&
                         classified.get("approved_dagre_mapping_gate").get_string("sha256") ==
                             approved.get_string("review_sha256") &&
                         classified.get("source_only_candidate").get_int("bytes") == 126440 &&
                         classified.get("source_only_candidate").get_string("sha256") ==
                             "cf8dab200666f56a1f6958726c821284513ff3f4eb5ec163228dbe61ab6500c3",
                     "OfficialClass285 classifier immutable source lineage");
    const auto classifier_index = official_class285_classifier_index(classified, setup);
    for (std::size_t n = 0; n < 285u; ++n) {
        setup.spend(64u);
        class481_require(index[n] == kOfficialClass285DagreAbsent ||
                             classifier_index[n] == kOfficialClass285ClassifierAbsent,
                         "OfficialClass285 disjoint dagre38 and classifier26 authorities");
    }
    setup.spend(12288u);  // New path/I/O metadata4096 plus bounded shared report provenance8192.
    const auto unused_path = base / kOfficialClass285UnusedFile;
    class481_require(std::filesystem::file_size(unused_path) == kOfficialClass285UnusedBytes,
                     "OfficialClass285 unused immutable supplement stat");
    const auto unused_bytes = read(unused_path);
    class481_require(unused_bytes.size() == kOfficialClass285UnusedBytes &&
                         digest(unused_bytes) == kOfficialClass285UnusedSha,
                     "OfficialClass285 unused immutable supplement hash");
    auto unused = json::parse(unused_bytes);
    class481_require(unused.has_value(), "OfficialClass285 unused supplement JSON");
    const auto unused_index = official_class285_unused_index(unused->root(), setup);
    for (std::size_t n = 0; n < 285u; ++n) {
        setup.spend(32u);
        class481_require(unused_index[n] == kOfficialClass285UnusedAbsent ||
                             (index[n] == kOfficialClass285DagreAbsent &&
                              classifier_index[n] == kOfficialClass285ClassifierAbsent),
                         "OfficialClass285 unused disjoint old authorities");
    }
    setup.spend(2u * 285u * sizeof(bool) + 16u * sizeof(std::size_t) + 4096u);
    auto notes = official_class285_notes_document(setup);
    const auto notes_index = official_class285_notes_index(notes.root(), setup);
    for (std::size_t n = 0; n < 285u; ++n) {
        setup.spend(48u);
        class481_require(notes_index[n] == kOfficialClass285NotesAbsent ||
                             (index[n] == kOfficialClass285DagreAbsent &&
                              classifier_index[n] == kOfficialClass285ClassifierAbsent &&
                              unused_index[n] == kOfficialClass285UnusedAbsent),
                         "OfficialClass285 Notes disjoint all old authorities");
    }
    return {std::move(*facts),
            std::move(*approval),
            std::move(*dagre),
            std::move(*classifier),
            std::move(*unused),
            std::move(notes),
            index,
            classifier_index,
            unused_index,
            notes_index,
            setup.left};
}

struct OfficialClass285Originals {
    json::JsonDoc manifest;
    std::array<std::size_t, 285> ordinals;
    std::size_t bindings_left;
};

inline OfficialClass285Originals official_class285_originals() {
    // Same separate immutable-authority scope as Class481Originals. It cannot
    // lend credits to document setup or any source/geometry Work.
    Work bindings;
    bindings.spend(kOfficialClass285ManifestBytes * 3u + 65536u +
                   4u * sizeof(OfficialClass285Originals));
    const auto path =
        std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
    class481_require(std::filesystem::file_size(path) == kOfficialClass285ManifestBytes,
                     "OfficialClass285 original manifest stat");
    const auto bytes = read(path);
    class481_require(bytes.size() == kOfficialClass285ManifestBytes &&
                         digest(bytes) == kOfficialClass285ManifestSha,
                     "OfficialClass285 original manifest bytes/hash");
    auto parsed = json::parse(bytes);
    class481_require(parsed.has_value(), "OfficialClass285 original manifest JSON");
    const auto fixtures = parsed->root().get("fixtures");
    class481_require(fixtures.is_arr() && fixtures.size() == 2145u,
                     "OfficialClass285 original manifest complete inventory");
    std::array<std::size_t, 285> ordinals{};
    std::size_t global = 0, count = 0;
    fixtures.iter([&](JsonVal row) {
        bindings.spend(4u * sizeof(std::size_t) + 32u);
        const auto path_text = class481_text(row.get("path"), bindings, 512);
        if (path_text.starts_with("class-diagram/")) {
            class481_require(count < ordinals.size(), "OfficialClass285 original roster overflow");
            ordinals[count++] = global;
        }
        ++global;
    });
    class481_require(count == 285u && global == 2145u,
                     "OfficialClass285 complete original manifest order");
    return {std::move(*parsed), ordinals, bindings.left};
}

inline JsonVal official_class285_dagre_model(JsonVal extra, JsonVal base, std::size_t n,
                                             std::string_view encoded, Work& work) {
    work.spend(8192u + 6u * sizeof(JsonVal) + 512u);
    class481_keys(extra,
                  {"all_remaining_obligations", "application_ordinal",
                   "class_presentation_candidate", "complete_native_Model_candidate",
                   "complete_native_Model_candidate_sha256", "fixture", "foreign_assertion_debts",
                   "id", "metadata_mapping_delta", "new_whole_Model_candidate",
                   "original_foreign_assertions_certified", "removed_single_obligation",
                   "supplement_mapping_review", "v1_complete_row_binding"},
                  work);
    const auto debts = base.get("unresolved_terminal_obligations");
    class481_require(base.get("whole_native_model_candidate").is_null() && debts.is_arr() &&
                         debts.size() == 1u &&
                         debts.at(0).get_string("kind") == "unreviewed_dagre_metadata_contract",
                     "OfficialClass285 dagre only exact original single-debt null");
    class481_require(extra.get("application_ordinal").is_int() &&
                         extra.get_int("application_ordinal") == static_cast<std::int64_t>(n) &&
                         er_present_same_json(extra.get("id"), base.get("id"), work) &&
                         er_present_same_json(extra.get("fixture"), base.get("fixture"), work),
                     "OfficialClass285 dagre complete original identity");
    const auto binding = extra.get("v1_complete_row_binding");
    class481_keys(binding,
                  {"canonical_v1_case_bytes", "canonical_v1_case_sha256", "source_sha256",
                   "source_utf8_bytes", "source_git_blob", "oracle_record",
                   "manifest_global_ordinal", "source_pointer_in_v1", "primary_graph_pointer",
                   "complete_original_manifest_pointer"},
                  work);
    work.spend(encoded.size() * 3u + 1024u);
    class481_require(binding.get("canonical_v1_case_bytes").is_int() &&
                         binding.get_int("canonical_v1_case_bytes") ==
                             static_cast<std::int64_t>(encoded.size()) &&
                         digest(encoded) == binding.get_string("canonical_v1_case_sha256"),
                     "OfficialClass285 dagre full immutable base row");
    for (const auto key : {"source_sha256", "source_utf8_bytes", "source_git_blob", "oracle_record",
                           "manifest_global_ordinal"})
        class481_require(er_present_same_json(binding.get(key), base.get(key), work),
                         "OfficialClass285 dagre complete original source binding");
    work.spend(1024u);
    const auto pointer = std::string("/cases/") + std::to_string(n);
    class481_require(binding.get_string("source_pointer_in_v1") == pointer + "/source" &&
                         binding.get_string("primary_graph_pointer") ==
                             "/success_point_snapshot/graph" &&
                         binding.get_string("complete_original_manifest_pointer") ==
                             pointer + "/complete_manifest_record",
                     "OfficialClass285 dagre complete authority pointers");
    class481_require(
        extra.get("all_remaining_obligations").is_arr() &&
            extra.get("all_remaining_obligations").size() == 0u &&
            extra.get("new_whole_Model_candidate").is_bool() &&
            extra.get("new_whole_Model_candidate").as_bool() &&
            extra.get_int("original_foreign_assertions_certified") == 0 &&
            er_present_same_json(extra.get("removed_single_obligation"), debts.at(0), work),
        "OfficialClass285 dagre sole removed source obligation");
    const auto delta = extra.get("metadata_mapping_delta");
    class481_keys(delta, {"processed_config_path", "value", "disposition"}, work);
    class481_require(delta.get("processed_config_path").is_arr() &&
                         delta.get("processed_config_path").size() == 1u &&
                         delta.get("processed_config_path").at(0).as_str() == "layout" &&
                         delta.get_string("value") == "dagre" &&
                         delta.get_string("disposition") ==
                             "source_reviewed_fixed_terminal_dagre_adaptation",
                     "OfficialClass285 dagre root layout adaptation only");
    class481_require(er_present_same_json(extra.get("class_presentation_candidate"),
                                          base.get("class_presentation_candidate"), work),
                     "OfficialClass285 dagre exact original presentation");
    const auto history = extra.get("supplement_mapping_review");
    class481_keys(history, {"verdict", "reviewer"}, work);
    class481_require(history.get_string("verdict") == "pending" &&
                         history.get("reviewer").is_null(),
                     "OfficialClass285 dagre semantic row pending history preserved");
    const auto meaning = extra.get("complete_native_Model_candidate");
    class481_require(meaning.is_obj(), "OfficialClass285 dagre full Model object");
    class481_hash_text(extra.get("complete_native_Model_candidate_sha256"), work);
    const auto canonical = class481_canonical(meaning, work);
    work.spend(canonical.size() * 3u + 128u);
    class481_require(digest(canonical) ==
                         extra.get_string("complete_native_Model_candidate_sha256"),
                     "OfficialClass285 dagre complete reviewed Model digest");
    return meaning;
}

inline JsonVal official_class285_classifier_model(JsonVal extra, JsonVal base, std::size_t n,
                                                  std::string_view encoded, Work& work) {
    work.spend(8192u + 8u * sizeof(JsonVal) + 512u);
    class481_keys(extra,
                  {"all_original_blocking_obligations", "all_remaining_obligations",
                   "application_ordinal", "class_presentation_candidate",
                   "classifier_member_mappings", "complete_native_Model_candidate",
                   "complete_native_Model_candidate_sha256", "fixture", "foreign_assertion_debts",
                   "id", "metadata_mapping_deltas", "new_whole_Model_candidate",
                   "original_foreign_assertions_certified", "removed_obligations",
                   "supplement_mapping_review", "v1_complete_row_binding"},
                  work);
    const auto debts = base.get("unresolved_terminal_obligations");
    class481_require(base.get("whole_native_model_candidate").is_null() && debts.is_arr() &&
                         debts.size() > 0u,
                     "OfficialClass285 classifier only original whole-null authority");
    std::size_t classifiers = 0, dagre = 0;
    debts.iter([&](JsonVal debt) {
        work.spend(256u);
        const auto kind = debt.get_string("kind");
        if (kind == "member_classifier")
            ++classifiers;
        else if (kind == "unreviewed_dagre_metadata_contract")
            ++dagre;
        else
            class481_require(false, "OfficialClass285 classifier no other unresolved source debt");
    });
    class481_require(classifiers >= 1u && classifiers <= 2u && dagre <= 1u,
                     "OfficialClass285 classifier finite primary debt count");
    class481_require(extra.get("application_ordinal").is_int() &&
                         extra.get_int("application_ordinal") == static_cast<std::int64_t>(n) &&
                         er_present_same_json(extra.get("id"), base.get("id"), work) &&
                         er_present_same_json(extra.get("fixture"), base.get("fixture"), work),
                     "OfficialClass285 classifier complete original identity");
    const auto binding = extra.get("v1_complete_row_binding");
    class481_keys(binding,
                  {"canonical_v1_case_bytes", "canonical_v1_case_sha256", "source_sha256",
                   "source_utf8_bytes", "source_git_blob", "oracle_record",
                   "manifest_global_ordinal", "source_pointer_in_v1", "primary_graph_pointer",
                   "complete_original_manifest_pointer"},
                  work);
    work.spend(encoded.size() * 3u + 1024u);
    class481_require(binding.get("canonical_v1_case_bytes").is_int() &&
                         binding.get_int("canonical_v1_case_bytes") ==
                             static_cast<std::int64_t>(encoded.size()) &&
                         digest(encoded) == binding.get_string("canonical_v1_case_sha256"),
                     "OfficialClass285 classifier full immutable base row");
    for (const auto key : {"source_sha256", "source_utf8_bytes", "source_git_blob", "oracle_record",
                           "manifest_global_ordinal"})
        class481_require(er_present_same_json(binding.get(key), base.get(key), work),
                         "OfficialClass285 classifier complete original source binding");
    work.spend(1024u);
    const auto pointer = std::string("/cases/") + std::to_string(n);
    class481_require(binding.get_string("source_pointer_in_v1") == pointer + "/source" &&
                         binding.get_string("primary_graph_pointer") ==
                             "/success_point_snapshot/graph" &&
                         binding.get_string("complete_original_manifest_pointer") ==
                             pointer + "/complete_manifest_record",
                     "OfficialClass285 classifier complete authority pointers");
    class481_require(
        extra.get("all_remaining_obligations").is_arr() &&
            extra.get("all_remaining_obligations").size() == 0u &&
            extra.get("new_whole_Model_candidate").is_bool() &&
            extra.get("new_whole_Model_candidate").as_bool() &&
            extra.get_int("original_foreign_assertions_certified") == 0 &&
            er_present_same_json(extra.get("all_original_blocking_obligations"), debts, work) &&
            er_present_same_json(extra.get("removed_obligations"), debts, work),
        "OfficialClass285 classifier exact whole removed source obligations");
    const auto deltas = extra.get("metadata_mapping_deltas");
    class481_require(deltas.is_arr() && deltas.size() == dagre,
                     "OfficialClass285 classifier exact optional dagre delta");
    if (dagre) {
        const auto delta = deltas.at(0);
        class481_keys(delta, {"processed_config_path", "value", "disposition"}, work);
        class481_require(delta.get("processed_config_path").is_arr() &&
                             delta.get("processed_config_path").size() == 1u &&
                             delta.get("processed_config_path").at(0).as_str() == "layout" &&
                             delta.get_string("value") == "dagre" &&
                             delta.get_string("disposition") ==
                                 "source_reviewed_fixed_terminal_dagre_adaptation",
                         "OfficialClass285 classifier root dagre adaptation only");
    }
    class481_require(er_present_same_json(extra.get("class_presentation_candidate"),
                                          base.get("class_presentation_candidate"), work),
                     "OfficialClass285 classifier exact original presentation");
    const auto history = extra.get("supplement_mapping_review");
    class481_keys(history, {"verdict", "reviewer"}, work);
    class481_require(history.get_string("verdict") == "pending" &&
                         history.get("reviewer").is_null(),
                     "OfficialClass285 classifier semantic row pending history preserved");
    const auto meaning = extra.get("complete_native_Model_candidate");
    class481_require(meaning.is_obj(), "OfficialClass285 classifier full Model object");
    class481_hash_text(extra.get("complete_native_Model_candidate_sha256"), work);
    const auto canonical = class481_canonical(meaning, work);
    work.spend(canonical.size() * 3u + 128u);
    class481_require(digest(canonical) ==
                         extra.get_string("complete_native_Model_candidate_sha256"),
                     "OfficialClass285 classifier complete reviewed Model digest");
    const auto mappings = extra.get("classifier_member_mappings");
    class481_require(mappings.is_arr() && mappings.size() == classifiers,
                     "OfficialClass285 classifier complete typed member mappings");
    mappings.iter([&](JsonVal mapped) {
        work.spend(4096u);
        const auto fields = mapped.get("primary_decoded_own_fields");
        const auto classifier = class481_text(fields.get("classifier"), work, 1);
        class481_require(classifier == "$" || classifier == "*",
                         "OfficialClass285 classifier typed primary suffix");
        const auto entity = mapped.get("entity_index"), row = mapped.get("row_index");
        const auto entities = meaning.get("entities");
        class481_require(entity.is_int() && entity.as_int() >= 0 && entities.is_arr() &&
                             static_cast<std::uint64_t>(entity.as_int()) < entities.size(),
                         "OfficialClass285 classifier entity Row domain");
        const auto rows = entities.at(static_cast<std::size_t>(entity.as_int())).get("rows");
        class481_require(row.is_int() && row.as_int() >= 0 && rows.is_arr() &&
                             static_cast<std::uint64_t>(row.as_int()) < rows.size(),
                         "OfficialClass285 classifier entity Row domain");
        const auto terminal = mapped.get("terminal_Row"),
                   alternatives = terminal.get("alternatives");
        const auto shown = class481_text(mapped.get("captured_displayText"), work, 256);
        work.spend(shown.size() * 3u + 128u);
        const auto complete = std::string(shown) + std::string(classifier);
        class481_require(
            alternatives.is_arr() && alternatives.size() == 1u && alternatives.at(0).is_str() &&
                alternatives.at(0).as_str() == complete &&
                terminal.get_string("role") == fields.get_string("memberType") &&
                er_present_same_json(terminal, rows.at(static_cast<std::size_t>(row.as_int())),
                                     work),
            "OfficialClass285 classifier complete typed Row retains suffix and literal text");
    });
    return meaning;
}

inline JsonVal official_class285_unused_model(JsonVal extra, JsonVal base, std::size_t n,
                                              std::string_view encoded, Work& work) {
    work.spend(8192u + 8u * sizeof(std::string_view) + 4u * sizeof(std::string));
    class481_keys(extra,
                  {"application_ordinal", "id", "fixture", "v1_complete_row_binding",
                   "complete_source", "complete_native_Model_candidate",
                   "complete_native_Model_candidate_sha256", "class_presentation_candidate",
                   "inactive_style_definition_ledger", "current_class_style_effect",
                   "all_original_blocking_obligations", "terminal_source_profile_deltas",
                   "all_remaining_terminal_obligations", "foreign_assertion_debts",
                   "original_foreign_assertions_certified", "status"},
                  work);
    const auto debts = base.get("unresolved_terminal_obligations");
    class481_require(base.get("whole_native_model_candidate").is_null() && debts.is_arr(),
                     "OfficialClass285 unused only original whole-null authority");
    std::size_t inactive = 0, dagre = 0;
    debts.iter([&](JsonVal value) {
        work.spend(256u);
        const auto kind = class481_text(value.get("kind"), work, 128);
        if (kind == "styleClass_contract")
            ++inactive;
        else if (kind == "unreviewed_dagre_metadata_contract")
            ++dagre;
        else
            class481_require(false, "OfficialClass285 unused no other unresolved source debt");
    });
    class481_require(inactive == 1u && dagre == (n == 98u ? 1u : 0u),
                     "OfficialClass285 unused exact finite original debts");
    class481_require(extra.get("application_ordinal").is_int() &&
                         extra.get_int("application_ordinal") == static_cast<std::int64_t>(n) &&
                         er_present_same_json(extra.get("id"), base.get("id"), work) &&
                         er_present_same_json(extra.get("fixture"), base.get("fixture"), work),
                     "OfficialClass285 unused complete original identity");
    const auto binding = extra.get("v1_complete_row_binding");
    class481_keys(binding,
                  {"canonical_v1_case_bytes", "canonical_v1_case_sha256", "source_sha256",
                   "source_utf8_bytes", "source_git_blob", "oracle_record",
                   "manifest_global_ordinal", "source_pointer_in_v1",
                   "complete_original_manifest_pointer"},
                  work);
    work.spend(encoded.size() * 3u + 128u);
    class481_require(binding.get("canonical_v1_case_bytes").is_int() &&
                         binding.get_int("canonical_v1_case_bytes") ==
                             static_cast<std::int64_t>(encoded.size()) &&
                         class481_text(binding.get("canonical_v1_case_sha256"), work, 64) ==
                             digest(encoded),
                     "OfficialClass285 unused full immutable base row");
    for (const auto key : {"source_sha256", "source_utf8_bytes", "source_git_blob", "oracle_record",
                           "manifest_global_ordinal"})
        class481_require(er_present_same_json(binding.get(key), base.get(key), work),
                         "OfficialClass285 unused complete original source binding");
    work.spend(1024u);
    const auto pointer = "/cases/" + std::to_string(n);
    class481_require(
        class481_text(binding.get("source_pointer_in_v1"), work, 128) == pointer + "/source" &&
            class481_text(binding.get("complete_original_manifest_pointer"), work, 128) ==
                pointer + "/complete_manifest_record",
        "OfficialClass285 unused complete authority pointers");
    class481_require(
        er_present_same_json(extra.get("complete_source"), base.get("source"), work) &&
            er_present_same_json(extra.get("all_original_blocking_obligations"), debts, work) &&
            er_present_same_json(extra.get("foreign_assertion_debts"),
                                 base.get("foreign_assertion_debts"), work) &&
            er_present_same_json(extra.get("class_presentation_candidate"),
                                 base.get("class_presentation_candidate"), work),
        "OfficialClass285 unused complete source presentation and retained foreign debt");
    class481_require(
        extra.get("all_remaining_terminal_obligations").is_arr() &&
            extra.get("all_remaining_terminal_obligations").size() == 0u &&
            extra.get("original_foreign_assertions_certified").is_int() &&
            extra.get_int("original_foreign_assertions_certified") == 0 &&
            class481_text(extra.get("status"), work, 128) ==
                "SOURCE_APPROVED_PROPOSAL_WRAPPER_PENDING_CONCRETE_REVIEW_AND_INSTALLATION",
        "OfficialClass285 unused source-only proposal history");
    const auto deltas = extra.get("terminal_source_profile_deltas");
    class481_require(deltas.is_arr() && deltas.size() == debts.size(),
                     "OfficialClass285 unused complete source deltas");
    for (std::size_t i = 0; i < deltas.size(); ++i) {
        work.spend(i + 256u);
        const auto delta = deltas.at(i);
        class481_keys(delta, {"kind", "disposition", "original_debt_exact"}, work);
        const auto kind = class481_text(debts.at(i).get("kind"), work, 128);
        class481_require(
            class481_text(delta.get("kind"), work, 128) == kind &&
                er_present_same_json(delta.get("original_debt_exact"), debts.at(i), work),
            "OfficialClass285 unused unchanged ordered original debts");
        const auto expected =
            kind == "styleClass_contract"
                ? "finite_inactive_classDef_declarations_consumed_without_current_node_style_effect"
                : "reuse_source_reviewed_fixed_terminal_dagre_adaptation";
        class481_require(class481_text(delta.get("disposition"), work, 128) == expected,
                         "OfficialClass285 unused reviewed finite delta only");
    }
    const auto ledger = extra.get("inactive_style_definition_ledger");
    class481_require(ledger.is_arr() && ledger.size() == 2u,
                     "OfficialClass285 unused complete two declarations");
    const auto source = class481_text(extra.get("complete_source"), work);
    work.spend(source.size() * 8u + 1024u);
    auto trim = [](std::string_view value) {
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
            value.remove_prefix(1);
        while (!value.empty() &&
               (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
            value.remove_suffix(1);
        return value;
    };
    bool body = false;
    std::size_t declarations = 0;
    auto remaining = source;
    while (!remaining.empty()) {
        const auto end = remaining.find('\n');
        const auto line = trim(remaining.substr(0, end));
        remaining = end == std::string_view::npos ? std::string_view{} : remaining.substr(end + 1u);
        if (!body) {
            if (line == "classDiagram" || line == "classDiagram-v2") body = true;
            continue;
        }
        if (!line.starts_with("classDef ")) continue;
        class481_require(declarations < ledger.size(),
                         "OfficialClass285 unused unlisted source declaration");
        const auto pair = ledger.at(declarations++);
        class481_require(pair.is_arr() && pair.size() == 2u,
                         "OfficialClass285 unused typed declaration pair");
        const auto id = class481_text(pair.at(0), work, 64);
        const auto fields = pair.at(1);
        class481_keys(fields, {"id", "styles", "textStyles"}, work);
        class481_require(!id.empty() && class481_text(fields.get("id"), work, 64) == id &&
                             std::string_view("default").find(id) == std::string_view::npos,
                         "OfficialClass285 unused whole-source default substring boundary");
        auto text = trim(line.substr(8u));
        const auto split = text.find_first_of(" \t");
        class481_require(split != std::string_view::npos && text.substr(0, split) == id,
                         "OfficialClass285 unused declaration source order and identity");
        const auto raw_style = trim(text.substr(split));
        const auto styles = fields.get("styles"), text_styles = fields.get("textStyles");
        class481_require(styles.is_arr() && styles.size() == 1u &&
                             class481_text(styles.at(0), work, 256) == raw_style &&
                             text_styles.is_arr(),
                         "OfficialClass285 unused complete raw source styles");
        if (raw_style.find("color") == std::string_view::npos)
            class481_require(text_styles.size() == 0u,
                             "OfficialClass285 unused no invented textStyles");
        else {
            work.spend(raw_style.size() * 16u + 256u);
            std::string expected(raw_style);
            const auto fill = expected.find("fill");
            if (fill != std::string::npos) expected.replace(fill, 4u, "bgFill");
            class481_require(text_styles.size() == 1u &&
                                 class481_text(text_styles.at(0), work, 256) == expected,
                             "OfficialClass285 unused complete primary textStyles");
        }
    }
    class481_require(body && declarations == 2u,
                     "OfficialClass285 unused all source declarations consumed");
    const auto effects = extra.get("current_class_style_effect");
    class481_require(effects.is_arr() && effects.size() == 1u,
                     "OfficialClass285 unused complete current Class style effect");
    const auto effect = effects.at(0);
    class481_keys(effect, {"class_id", "cssClasses", "styles", "absent_own_properties"}, work);
    class481_require(class481_text(effect.get("class_id"), work, 64) == "Class10" &&
                         class481_text(effect.get("cssClasses"), work, 64) == "default" &&
                         effect.get("styles").is_arr() && effect.get("styles").size() == 0u,
                     "OfficialClass285 unused unchanged default Class style");
    const auto absent = effect.get("absent_own_properties");
    class481_require(absent.is_arr() && absent.size() == 4u,
                     "OfficialClass285 unused exact absent interaction properties");
    std::size_t at = 0;
    for (const auto key : {"haveCallback", "tooltip", "link", "linkTarget"})
        class481_require(class481_text(absent.at(at++), work, 32) == key,
                         "OfficialClass285 unused absence is not false or null");
    const auto meaning = extra.get("complete_native_Model_candidate");
    class481_keys(meaning, {"family", "direction", "entities", "relations", "groups", "notes"},
                  work);
    class481_require(class481_text(meaning.get("family"), work, 16) == "class" &&
                         class481_text(meaning.get("direction"), work, 2) == "TB",
                     "OfficialClass285 unused complete Class direction");
    const auto entities = meaning.get("entities");
    class481_require(entities.is_arr() && entities.size() == 1u,
                     "OfficialClass285 unused complete one Class");
    const auto entity = entities.at(0);
    class481_keys(entity, {"id", "label", "owner", "shape", "headers", "rows"}, work);
    class481_require(class481_text(entity.get("id"), work, 64) == "class:436c6173733130" &&
                         class481_text(entity.get("label"), work, 64) == "Class10" &&
                         class481_text(entity.get("owner"), work, 64).empty() &&
                         class481_text(entity.get("shape"), work, 16) == "box",
                     "OfficialClass285 unused complete source Class identity");
    class481_require(entity.get("rows").is_arr() && entity.get("rows").size() == 0u,
                     "OfficialClass285 unused no invented members");
    const auto headers = entity.get("headers");
    class481_require(headers.is_arr() && headers.size() == 1u && headers.at(0).is_arr() &&
                         headers.at(0).size() == 1u &&
                         class481_text(headers.at(0).at(0), work, 64) == "Class10",
                     "OfficialClass285 unused exact source header");
    for (const auto key : {"relations", "groups", "notes"})
        class481_require(meaning.get(key).is_arr() && meaning.get(key).size() == 0u,
                         "OfficialClass285 unused no omitted or invented graph facts");
    const auto canonical = class481_canonical(meaning, work);
    work.spend(canonical.size() * 3u + 128u);
    class481_require(class481_text(extra.get("complete_native_Model_candidate_sha256"), work, 64) ==
                             "d6568c8d0a279e4a9c3452b238319acb3a24e043055e89f9c678864b5904cdc0" &&
                         digest(canonical) ==
                             "d6568c8d0a279e4a9c3452b238319acb3a24e043055e89f9c678864b5904cdc0",
                     "OfficialClass285 unused exact independently reconstructed Model");
    return meaning;
}

inline void official_class285_unused_consume(std::array<bool, 285>& consumed, std::size_t n,
                                             const OfficialClass285Documents& docs, Work& work) {
    work.spend(32u);
    class481_require(n < 285u && docs.unused_index[n] != kOfficialClass285UnusedAbsent &&
                         !consumed[n],
                     "OfficialClass285 unused unique whole source consumption");
    consumed[n] = true;
}

inline JsonVal official_class285_notes_model(JsonVal row, JsonVal base, std::size_t i, Work& work) {
    work.spend(8192u + 8u * sizeof(JsonVal) + 256u);
    class481_keys(row,
                  {"application_ordinal", "base_case_canonical_sha256", "class_presentation",
                   "note_source_bindings", "source", "source_sha256", "whole_Model",
                   "whole_Model_sha256"},
                  work);
    official_class285_notes_binding(row, i, base, work);
    class481_require(base.get("foreign_assertion_debts").is_arr() &&
                         base.get("foreign_assertion_debts").size() > 0u,
                     "OfficialClass285 Notes original foreign debts retained");
    return row.get("whole_Model");
}

inline void official_class285_notes_consume(std::array<bool, 285>& seen, std::size_t n,
                                            const OfficialClass285Documents& docs, Work& work) {
    work.spend(128u);
    class481_require(n < 285u && docs.notes_index[n] != kOfficialClass285NotesAbsent && !seen[n],
                     "OfficialClass285 Notes unique whole source consumption");
    seen[n] = true;
}

struct OfficialClass285Bound {
    JsonVal meaning;
    ClassPresentation presentation;
    bool dagre = false, classifier = false, unused_definitions = false, notes = false;
};

inline OfficialClass285Bound official_class285_bind(JsonVal row, std::size_t n,
                                                    const OfficialClass285Documents& docs,
                                                    const OfficialClass285Originals& originals,
                                                    Work& work) {
    work.spend(32768u + 4u * sizeof(OfficialClass285Bound) + n * 2u + 64u);
    class481_require(n < 285u, "OfficialClass285 ordinal range");
    class481_keys(row,
                  {"id",
                   "application_ordinal",
                   "manifest_global_ordinal",
                   "fixture",
                   "complete_manifest_record",
                   "source",
                   "source_utf8_bytes",
                   "source_sha256",
                   "source_git_blob",
                   "oracle_record",
                   "mapping_review",
                   "source_admission",
                   "whole_native_model_candidate",
                   "whole_native_model_candidate_sha256",
                   "class_presentation_candidate",
                   "unresolved_terminal_obligations",
                   "foreign_assertion_debts",
                   "mapping_witnesses",
                   "known_shape_check",
                   "candidate_status",
                   "metadata_authority"},
                  work);
    const auto pin = docs.approval.root().get("cases").at(n);
    const auto ordinal = originals.ordinals[n];
    work.spend(ordinal + 1u);
    class481_require(
        row.get("application_ordinal").is_int() &&
            row.get_int("application_ordinal") == static_cast<std::int64_t>(n) &&
            pin.get_int("application_ordinal") == static_cast<std::int64_t>(n) &&
            row.get("manifest_global_ordinal").is_int() &&
            row.get_int("manifest_global_ordinal") == static_cast<std::int64_t>(ordinal) &&
            pin.get_int("manifest_global_ordinal") == static_cast<std::int64_t>(ordinal),
        "OfficialClass285 exact original order");
    const auto original = originals.manifest.root().get("fixtures").at(ordinal);
    const auto fixture = class481_text(row.get("fixture"), work, 512);
    class481_require(original.get_string("path") == fixture &&
                         fixture.starts_with("class-diagram/") &&
                         er_present_same_json(original, row.get("complete_manifest_record"), work),
                     "OfficialClass285 complete original manifest dictionary");
    const auto input = class481_text(row.get("source"), work);
    class481_require(row.get("source_utf8_bytes").is_int() &&
                         row.get_int("source_utf8_bytes") ==
                             static_cast<std::int64_t>(input.size()) &&
                         original.get_int("bytes") == static_cast<std::int64_t>(input.size()) &&
                         digest(input) == row.get_string("source_sha256") &&
                         original.get_string("sha256") == row.get_string("source_sha256") &&
                         original.get_string("git_blob") == row.get_string("source_git_blob"),
                     "OfficialClass285 complete original source");
    work.spend(input.size() * 3u + fixture.size() * 4u + 512u);
    const auto source_path = std::filesystem::path(LOOM_TESTS_DIR) /
                             "fixtures/mermaid_official/cases" / std::string(fixture);
    class481_require(std::filesystem::file_size(source_path) == input.size() &&
                         read(source_path) == input,
                     "OfficialClass285 actual complete original file");
    const auto meaning = row.get("whole_native_model_candidate"),
               debts = row.get("unresolved_terminal_obligations");
    class481_require(debts.is_arr() && row.get("foreign_assertion_debts").is_arr() &&
                         row.get("foreign_assertion_debts").size() > 0,
                     "OfficialClass285 complete retained debts");
    if (pin.get("whole_Model_sha256").is_null())
        class481_require(meaning.is_null() &&
                             row.get("whole_native_model_candidate_sha256").is_null() &&
                             debts.size() > 0,
                         "OfficialClass285 null remains mandatory debt");
    else {
        class481_require(meaning.is_obj() && debts.size() == 0 &&
                             row.get_string("whole_native_model_candidate_sha256") ==
                                 pin.get_string("whole_Model_sha256"),
                         "OfficialClass285 complete mapped Model");
        class481_require(digest(class481_canonical(meaning, work)) ==
                             pin.get_string("whole_Model_sha256"),
                         "OfficialClass285 complete Model digest");
    }
    const auto presented = row.get("class_presentation_candidate");
    class481_keys(presented,
                  {"diagram_title", "authored_hide_empty_members_box", "terminal_empty_default",
                   "title_role"},
                  work);
    class481_require(presented.get("diagram_title").is_null() &&
                         presented.get_string("terminal_empty_default") ==
                             "compact_independent_of_envelope" &&
                         presented.get_string("title_role") == "absent",
                     "OfficialClass285 exact source presentation");
    ClassPresentation presentation;
    const auto hide = presented.get("authored_hide_empty_members_box");
    class481_require(hide.is_null() || (hide.is_bool() && hide.as_bool()),
                     "OfficialClass285 finite source compact setting");
    if (hide.is_bool()) presentation.hide_empty_members_box = hide.as_bool();
    class481_require(row.get("mapping_review").get_string("verdict") == "pending" &&
                         row.get_string("known_shape_check") ==
                             "passed_locked_finite_semantic_shapes",
                     "OfficialClass285 preserved mapping history");
    const auto encoded = class481_canonical(row, work);
    class481_require(encoded.size() == static_cast<std::uint64_t>(
                                           pin.get_int("canonical_candidate_case_bytes")) &&
                         digest(encoded) == pin.get_string("canonical_candidate_case_sha256"),
                     "OfficialClass285 exact independently reviewed whole row");
    work.spend(2u * sizeof(std::size_t) + 16u);
    const auto notes_index = docs.notes_index[n];
    if (notes_index != kOfficialClass285NotesAbsent) {
        class481_require(notes_index < 18u &&
                             docs.unused_index[n] == kOfficialClass285UnusedAbsent &&
                             docs.classifier_index[n] == kOfficialClass285ClassifierAbsent &&
                             docs.dagre_index[n] == kOfficialClass285DagreAbsent,
                         "OfficialClass285 Notes selected disjoint index domain");
        work.spend(notes_index + 1u);
        return {official_class285_notes_model(docs.notes.root().get("originals").at(notes_index),
                                              row, notes_index, work),
                presentation,
                false,
                false,
                false,
                true};
    }
    work.spend(2u * sizeof(std::size_t) + 16u);
    const auto unused_index = docs.unused_index[n];
    if (unused_index != kOfficialClass285UnusedAbsent) {
        class481_require(unused_index < 3u &&
                             docs.classifier_index[n] == kOfficialClass285ClassifierAbsent &&
                             docs.dagre_index[n] == kOfficialClass285DagreAbsent,
                         "OfficialClass285 unused selected disjoint index domain");
        work.spend(unused_index + 1u);
        return {official_class285_unused_model(docs.unused.root().get("cases").at(unused_index),
                                               row, n, encoded, work),
                presentation, false, false, true};
    }
    work.spend(6u * sizeof(std::size_t) + 32u);
    const auto classifier_index = docs.classifier_index[n], extra_index = docs.dagre_index[n];
    if (classifier_index != kOfficialClass285ClassifierAbsent) {
        class481_require(classifier_index < 26u && extra_index == kOfficialClass285DagreAbsent,
                         "OfficialClass285 classifier selected disjoint index domain");
        work.spend(classifier_index + 1u);
        return {
            official_class285_classifier_model(
                docs.classifier.root().get("cases").at(classifier_index), row, n, encoded, work),
            presentation, false, true};
    }
    if (extra_index == kOfficialClass285DagreAbsent) return {meaning, presentation, false};
    class481_require(extra_index < 38u, "OfficialClass285 dagre selected index domain");
    work.spend(extra_index + 1u);
    return {official_class285_dagre_model(docs.dagre.root().get("cases").at(extra_index), row, n,
                                          encoded, work),
            presentation, true};
}

}  // namespace loom::test::mermaid_relations
