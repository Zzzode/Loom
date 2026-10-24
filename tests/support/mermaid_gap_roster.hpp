// RFC 0031 E6c: reviewed machine-readable ledger of the mandatory-completion
// capability gaps still open in the native Mermaid renderer.
//
// This is a TEST-ONLY textual support header, included solely by ordinary
// importer TUs (test_mermaid_official.cpp, test_mermaid_charts_semantics.cpp,
// test_mermaid_relations_semantics.cpp). It must never be included by a module
// implementation TU (test_mermaid_flow_ascii.cpp declares module loom.ui...).
//
// The roster file tools/mermaid/e6c-mandatory-gap-roster.json is a reviewed
// ledger artifact, never renderer-output evidence: it is generated once from
// the serialized "mandatory_gaps" report arrays, independently audited row by
// row, and then frozen by the pinned byte size and SHA-256 below. Every
// reporter diffs its observed canonical gap set against the roster in both
// directions, so any renderer-driven change (gain OR loss) fails until the
// ledger and these constants are updated in the same change and re-reviewed.
#pragma once
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
import std;
import loom.serdes.json;
import loom.crypto.crypto;

namespace loom::test::mermaid_gap_roster {

namespace json = loom::utils::json;

inline constexpr std::string_view kSchema = "e6c_mandatory_gap_roster/v1";
inline constexpr std::string_view kRosterRelPath =
    "tools/mermaid/e6c-mandatory-gap-roster.json";

// Frozen ledger pins. The verifier script parses these exact constants, so
// there is a single source of truth for size and SHA-256.
inline constexpr std::size_t kRosterBytes = 62087;
inline constexpr std::string_view kRosterSha256 = "8ebea8348439d0b93dc9e50fdc6101ce321968141a149f155d89acb66da33f2b";

// Flow400CompleteSources deliberately has no roster: its pending/decoded
// membership is determined by immutable Model authority assets and is pinned
// by exact counts instead.
inline constexpr std::array<std::string_view, 11> kGateNames{{
    "official_matrix",
    "merman_xy102",
    "class481",
    "class285",
    "class_endpoint29",
    "state85",
    "class_caption_starvation8",
    "state_reverse_escape",
    "class_notes18",
    "requirement11_p1p6",
    "requirement11_overflow",
}};

inline bool is_gate_name(std::string_view name) {
    return std::ranges::find(kGateNames, name) != kGateNames.end();
}

// Frozen per-gate roster row counts, checked by the Integrity test.
inline constexpr std::array<std::pair<std::string_view, std::size_t>, 11>
    kGateRowCounts{{
        {"official_matrix", 96},
        {"merman_xy102", 20},
        {"class481", 140},
        {"class285", 74},
        {"class_endpoint29", 3},
        {"state85", 14},
        {"class_caption_starvation8", 2},
        {"state_reverse_escape", 1},
        {"class_notes18", 6},
        {"requirement11_p1p6", 1},
        {"requirement11_overflow", 1},
    }};

inline std::filesystem::path roster_path() {
    return std::filesystem::path(LOOM_TESTS_DIR) / ".." / "tools" / "mermaid" /
           "e6c-mandatory-gap-roster.json";
}

struct Roster {
    std::string bytes;
    json::JsonDoc doc;

    json::JsonVal gates() const { return doc.root().get("gates"); }
};

inline std::string read_binary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// Loads the roster and verifies byte size, SHA-256, schema and gate-key set.
// Any failure is recorded as a gtest failure and std::nullopt is returned;
// callers skip the diff when loading failed so they do not double-report.
inline std::optional<Roster> load_verified_roster() {
    try {
        const auto path = roster_path();
        auto bytes = read_binary(path);
        if (bytes.size() != kRosterBytes) {
            ADD_FAILURE() << "E6c roster byte size drift for " << path.string()
                          << ": expected " << kRosterBytes << ", actual "
                          << bytes.size();
            return std::nullopt;
        }
        const auto digest = loom::utils::crypto::sha256(bytes);
        if (digest != kRosterSha256) {
            ADD_FAILURE() << "E6c roster SHA-256 drift for " << path.string()
                          << ": expected " << kRosterSha256 << ", actual " << digest;
            return std::nullopt;
        }
        auto parsed = json::parse(bytes);
        if (!parsed.has_value()) {
            ADD_FAILURE() << "E6c roster does not parse: " << path.string();
            return std::nullopt;
        }
        const auto root = parsed->root();
        if (!root.is_obj() || !root.has("schema") ||
            root.get("schema").as_str() != kSchema) {
            ADD_FAILURE() << "E6c roster schema must be " << kSchema;
            return std::nullopt;
        }
        const auto gates = root.get("gates");
        if (!gates.is_obj()) {
            ADD_FAILURE() << "E6c roster needs a gates object";
            return std::nullopt;
        }
        std::size_t present = 0;
        gates.iter_obj([&](json::JsonVal key, json::JsonVal rows) {
            const std::string name(key.as_str());
            if (!is_gate_name(name)) {
                ADD_FAILURE() << "E6c roster has unknown gate key: " << name;
                return;
            }
            ++present;
            if (!rows.is_arr()) {
                ADD_FAILURE() << "E6c roster gate " << name << " must be an array";
                return;
            }
            const auto expected = std::ranges::find_if(
                kGateRowCounts, [&](const auto& entry) { return entry.first == name; });
            if (rows.size() != expected->second) {
                ADD_FAILURE() << "E6c roster gate " << name << " has " << rows.size()
                              << " rows, frozen count is " << expected->second;
            }
        });
        if (present != kGateNames.size()) {
            ADD_FAILURE() << "E6c roster has " << present << " gates, expected "
                          << kGateNames.size();
            return std::nullopt;
        }
        return Roster{std::move(bytes), std::move(*parsed)};
    } catch (const std::exception& error) {
        ADD_FAILURE() << "E6c roster load failed: " << error.what();
        return std::nullopt;
    }
}

// Canonical JSON for one gap row: object keys sorted, homogeneous string
// arrays (the per-row category lists) sorted. Array rows themselves are
// compared as sets by the caller, so report ordering never matters.
inline std::string canonical_json(const json::JsonVal& value) {
    std::string out;
    if (value.is_obj()) {
        std::vector<std::pair<std::string, std::string>> members;
        value.iter_obj([&](json::JsonVal key, json::JsonVal child) {
            members.emplace_back(std::string(key.as_str()), canonical_json(child));
        });
        std::ranges::sort(members, {}, &std::pair<std::string, std::string>::first);
        out += '{';
        for (std::size_t i = 0; i < members.size(); ++i) {
            if (i) out += ',';
            out += '"';
            out += members[i].first;
            out += "\":";
            out += members[i].second;
        }
        out += '}';
    } else if (value.is_arr()) {
        std::vector<std::string> elements;
        bool all_strings = true;
        value.iter([&](json::JsonVal child) {
            if (!child.is_str()) all_strings = false;
            elements.push_back(canonical_json(child));
        });
        if (all_strings) std::ranges::sort(elements);
        out += '[';
        for (std::size_t i = 0; i < elements.size(); ++i) {
            if (i) out += ',';
            out += elements[i];
        }
        out += ']';
    } else {
        out += value.to_string();
    }
    return out;
}

inline std::set<std::string> canonical_row_set(const json::JsonVal& array_value) {
    std::set<std::string> rows;
    if (array_value.is_arr()) {
        array_value.iter([&](json::JsonVal row) { rows.insert(canonical_json(row)); });
    }
    return rows;
}

struct GapDiff {
    std::vector<std::string> missing;    // roster rows not observed
    std::vector<std::string> unexpected; // observed rows absent from roster

    bool ok() const { return missing.empty() && unexpected.empty(); }

    std::string render(std::string_view gate) const {
        std::ostringstream out;
        out << "E6c mandatory gap drift for gate " << gate;
        for (const auto& row : missing) out << "\n  missing (roster-only): " << row;
        for (const auto& row : unexpected) out << "\n  unexpected (renderer-only): " << row;
        return out.str();
    }
};

inline GapDiff diff_canonical_sets(const std::set<std::string>& expected,
                                   const std::set<std::string>& observed) {
    GapDiff diff;
    std::set_difference(expected.begin(), expected.end(), observed.begin(),
                        observed.end(), std::back_inserter(diff.missing));
    std::set_difference(observed.begin(), observed.end(), expected.begin(),
                        expected.end(), std::back_inserter(diff.unexpected));
    return diff;
}

// Diff a roster gate against the mandatory_gaps array the reporter
// serialized. Callers re-parse their own published report document
// (json::parse(report.to_string()) -> root.get("mandatory_gaps")), so the
// compared bytes are exactly the bytes on disk the Python verifier reads.
// Throws on an unknown gate name.
inline GapDiff diff_gate(const Roster& roster, std::string_view gate,
                         const json::JsonVal& observed_array) {
    if (!is_gate_name(gate))
        throw std::runtime_error("Unknown E6c roster gate: " + std::string(gate));
    const auto expected_rows = roster.gates().get(gate);
    if (!expected_rows.is_arr())
        throw std::runtime_error("E6c roster gate missing: " + std::string(gate));
    return diff_canonical_sets(canonical_row_set(expected_rows),
                               canonical_row_set(observed_array));
}

// Matrix error-message -> stable category mapping. A closed set of exactly
// six reviewed messages; anything else maps to "unknown" and stays a hard
// ADD_FAILURE at the call site, mirroring the parse_lsp_action closed-set
// rule: a new renderer failure must never silently book as a known gap.
inline constexpr std::string_view kUnknownCategory = "unknown";

inline std::string_view matrix_error_category(std::string_view message) {
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 6>
        kMapping{{
            {"Complete mapped original still uses ordinary source at240",
             "mapped_positive_still_source"},
            {"Restored positive retains an unmet wide source requirement",
             "mapped_positive_still_source"},
            {"Expected exact ordinary-code source fallback",
             "ordinary_source_fallback_expected"},
            {"OfficialER206 whole-null art/ordinary/restore unmet; never admission",
             "er206_whole_null_unmet"},
            {"Whole-null Flow has unreviewed art or failed ordinary/restore protocol",
             "flow_whole_null_unmet"},
            {"Whole-null original has unreviewed art or failed ordinary/restore protocol",
             "generic_whole_null_unmet"},
        }};
    for (const auto& [text, category] : kMapping)
        if (message == text) return category;
    return kUnknownCategory;
}

} // namespace loom::test::mermaid_gap_roster
