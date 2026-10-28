// RFC 0030 Phase B: immutable upstream inputs, independently declared outcomes,
// and production FTXUI pixels. No test-side Mermaid parser is used.
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

import std;
import loom.ui.visual.markdown;
import loom.ui.visual.mermaid.testing;
import loom.ui.foundation.design_tokens;
import loom.ui.foundation.theme_provider;
import loom.serdes.json;
import loom.crypto.crypto;

#include "mermaid_block58_official.hpp"
#include "mermaid_chart_cell_oracle.hpp"
#include "mermaid_class285_authority.hpp"
#include "mermaid_er107_official.hpp"
#include "mermaid_er206_authority.hpp"
#include "mermaid_flow400_authority.hpp"
#include "mermaid_flow400_config21_authority.hpp"
#include "mermaid_flow400_linkprofile3_authority.hpp"
#include "mermaid_flow_extra7_controls.hpp"
#include "mermaid_flow_group_endpoint15_oracle.hpp"
#include "mermaid_flow_shape_alias_authority.hpp"
#include "mermaid_flow_title_authority.hpp"
#include "mermaid_gap_roster.hpp"
#include "mermaid_mindmap10_authority.hpp"

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
namespace gap_roster = loom::test::mermaid_gap_roster;
using json::JsonVal;
constexpr std::string_view kManifestSha256 =
    "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b";
constexpr std::string_view kSentinel = "AFTER_MERMAID";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
const fs::path kRoot = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";

bool update_snapshots() {
    const auto* value = std::getenv("LOOM_UPDATE_SNAPSHOTS");
    return value != nullptr && std::string_view(value) == "1";
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_file(const fs::path& path, std::string_view contents) {
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output || !output.write(contents.data(), static_cast<std::streamsize>(contents.size())))
        throw std::runtime_error("Cannot write " + path.string());
}

std::string digest(std::string_view bytes) {
    return loom::utils::crypto::sha256(bytes);
}

bool hex_string(std::string_view value, std::size_t length) {
    return value.size() == length && std::ranges::all_of(value, [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

bool safe_relative(std::string_view name) {
    const fs::path path{name};
    if (name.empty() || path.is_absolute() || name.find('\\') != std::string_view::npos)
        return false;
    for (const auto& part : path)
        if (part == ".." || part == "." || part.empty()) return false;
    return path.generic_string() == name;
}

void require(bool condition, std::string message) {
    if (!condition) throw std::runtime_error(std::move(message));
}

void validate_witness(JsonVal expected, std::string_view source) {
    const auto witness = expected.get("witness");
    require(witness.is_obj(), "Source expectation lacks a concrete witness");
    const auto offset = witness.get_int("offset");
    const auto text = witness.get_string("text");
    require(offset >= 0 && !text.empty() && static_cast<std::size_t>(offset) <= source.size(),
            "Invalid witness offset/text");
    require(source.substr(static_cast<std::size_t>(offset), text.size()) == text,
            "Witness does not match source bytes");
}

void validate_expectation(JsonVal expected, std::string_view source) {
    require(!expected.get_string("reason").empty(), "Missing expectation reason");
    const auto choice = expected.get_string("expected");
    require(choice == "drawn" || choice == "source", "Unknown declared outcome");
    if (choice == "source") {
        validate_witness(expected, source);
        return;
    }
    const auto checks = expected.get("checks");
    require(checks.is_obj() && checks.get("labels").is_arr() && checks.get("labels").size() > 0 &&
                checks.get("arrows").is_int() && checks.get_int("arrows") >= 0 &&
                checks.get("ordering").is_arr(),
            "Drawing lacks reviewed semantic checks");
    checks.get("labels").iter([](JsonVal label) {
        require(label.is_str() && !label.as_str().empty(), "Empty semantic label");
    });
    checks.get("ordering").iter([](JsonVal ordering) {
        require(!ordering.get_string("before").empty() && !ordering.get_string("after").empty() &&
                    (ordering.get_string("axis") == "x" || ordering.get_string("axis") == "y"),
                "Invalid semantic ordering");
    });
    require(safe_relative(expected.get_string("snapshot")) &&
                fs::path(expected.get_string("snapshot")).extension() == ".txt",
            "Drawing lacks a safe reviewed snapshot name");
}

// The digest locks expectation metadata as well as the fixture content hashes.
// Inventory checks reject symlinks and unlisted files; no network or Git binary
// is required to verify the vendored corpus.
void validate_manifest(const fs::path& root, std::string_view manifest_bytes,
                       bool allow_missing_snapshots = false) {
    require(digest(manifest_bytes) == kManifestSha256, "Reviewed manifest SHA-256 mismatch");
    auto document = json::parse(manifest_bytes);
    require(document.has_value(), "Cannot parse manifest");
    const auto manifest = document->root();
    require(manifest.get_int("schema") == 1, "Unknown manifest schema");
    const auto upstream = manifest.get("upstream");
    require(upstream.get_string("repository") == "https://github.com/mermaid-js/mermaid" &&
                upstream.get_string("tag") == "mermaid@12.2.0" &&
                upstream.get_string("tag_object") == "ee1fdbcebe057cae346232efb6df066215565bec" &&
                upstream.get_string("commit") == "1fb7869866dd619168d840291c9c54b4f129758b" &&
                upstream.get_string("tree") == "58f6617da68d7f2b8a58161175db5db5a971cd82",
            "Upstream pin changed");
    require(digest(read_file(root / "LICENSE")) == upstream.get_string("license_sha256"),
            "Upstream license hash mismatch");
    const auto fixtures = manifest.get("fixtures");
    require(fixtures.is_arr() && fixtures.size() == 2145,
            "Official inventory must contain 2145 files");
    std::set<std::string> expected_files{"manifest.json", "LICENSE"};
    std::set<std::string> optional_snapshots;
    std::set<std::string> types;
    std::size_t total_bytes = 0;
    int diagnostics = 0;
    std::map<std::string, int> diagnostics_by_type;
    for (std::size_t index = 0; index < fixtures.size(); ++index) {
        const auto fixture = fixtures.at(index);
        const auto name = fixture.get_string("path");
        require(safe_relative(name) && fs::path(name).extension() == ".mmd", "Unsafe fixture path");
        require(expected_files.insert("cases/" + name).second, "Duplicate fixture path");
        types.insert((*fs::path(name).begin()).string());
        const auto source = read_file(root / "cases" / name);
        require(fixture.get_int("bytes") >= 0 &&
                    static_cast<std::size_t>(fixture.get_int("bytes")) == source.size(),
                "Byte count mismatch: " + name);
        total_bytes += source.size();
        require(digest(source) == fixture.get_string("sha256"),
                "Fixture SHA-256 mismatch: " + name);
        require(hex_string(fixture.get_string("git_blob"), 40), "Invalid locked Git blob: " + name);
        auto register_snapshot = [&](JsonVal expectation, bool original) {
            if (expectation.get_string("expected") != "drawn") return;
            require(expectation.get_string("snapshot") ==
                        digest(name).substr(0, 16) + (original ? ".original.txt" : ".txt"),
                    "Snapshot name must identify its original fixture and track");
            const auto snapshot = "snapshots/" + expectation.get_string("snapshot");
            require(expected_files.insert(snapshot).second, "Duplicate track snapshot");
            if (allow_missing_snapshots) optional_snapshots.insert(snapshot);
        };
        validate_expectation(fixture.get("raw"), source);
        register_snapshot(fixture.get("raw"), true);
        if (fixture.has("diagnostic")) {
            ++diagnostics;
            ++diagnostics_by_type[(*fs::path(name).begin()).string()];
            const auto diagnostic = fixture.get("diagnostic");
            const auto offset = diagnostic.get_int("offset");
            require(offset >= 0 && static_cast<std::size_t>(offset) < source.size(),
                    "Invalid diagnostic body offset");
            const auto body = std::string_view(source).substr(static_cast<std::size_t>(offset));
            require(digest(body) == diagnostic.get_string("sha256"),
                    "Derived body SHA-256 mismatch: " + name);
            validate_expectation(diagnostic, body);
            register_snapshot(diagnostic, false);
        }
    }
    require(total_bytes == 1214349 && types.size() == 37 && diagnostics == 593,
            "Pinned corpus totals changed");
    require(diagnostics_by_type ==
                std::map<std::string, int>{{"flowchart", 400}, {"sequence", 193}},
            "Diagnostic track type totals changed");
    require(!fs::is_symlink(fs::symlink_status(root)), "Corpus root is a symlink");
    std::set<std::string> actual_files;
    std::set<std::string> expected_directories;
    for (const auto& file : expected_files) {
        for (auto parent = fs::path(file).parent_path(); !parent.empty();
             parent = parent.parent_path())
            expected_directories.insert(parent.generic_string());
    }
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        require(!entry.is_symlink(), "Symlink in corpus: " + entry.path().string());
        if (entry.is_directory()) {
            require(expected_directories.contains(
                        entry.path().lexically_relative(root).generic_string()),
                    "Unlisted corpus directory: " + entry.path().string());
            continue;
        }
        require(entry.is_regular_file(), "Non-regular corpus entry");
        const auto relative = entry.path().lexically_relative(root).generic_string();
        require(expected_files.contains(relative), "Unlisted corpus file: " + relative);
        actual_files.insert(relative);
    }
    for (const auto& expected : expected_files)
        require(actual_files.contains(expected) || optional_snapshots.contains(expected),
                "Missing corpus file: " + expected);
}

// Additive, test-only policy. The immutable official manifest remains historical.
const fs::path kNativeRoot = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_native";
constexpr std::string_view kProjectionFile = "sequence_official_projection_supplement.json";
constexpr std::string_view kProjectionSha256 =
    "b6c06a9240cdfd987ef2e627b76f09d3d9c4da2ca855083a610b52d806777282";
constexpr std::string_view kProjectionFactFile = "sequence_official_fractional1_facts.json";
constexpr std::string_view kProjectionFactSha256 =
    "66e42d2d47f8e7cbdb70b001c48c861061ffd77fb020bcf429bf4bbb36178958";
constexpr std::string_view kProjectionModelSchema = "sequence_native_full_model_number_text/v1";
constexpr std::string_view kProjectionModelSha256 =
    "c179c2a5530eb51d1f977430a1ab1ad622971bd2007b14d5a899d93dc7d76b3a";
constexpr std::string_view kProjectionSnapshotFile = "sequence_official_fractional1_body.txt";
enum class ProjectionKind { SequenceBody, XYOriginal };

struct ProjectionBinding {
    std::string_view policy_file, policy_sha, fact_file, fact_sha;
    std::string_view model_schema, model_sha, snapshot_file;
    ProjectionKind kind = ProjectionKind::SequenceBody;
};

constexpr std::array<ProjectionBinding, 3> kProjectionBindings{
    {{kProjectionFile, kProjectionSha256, kProjectionFactFile, kProjectionFactSha256,
      kProjectionModelSchema, kProjectionModelSha256, kProjectionSnapshotFile},
     {"sequence_official_linebreaks_projection_supplement.json",
      "14cddb28dbe83f1e96288ebda00874d8a13797e45abcdbc6c27e7a44abdce4f5",
      "sequence_official_linebreaks_facts.json",
      "3cb45fa91233380fa09b140842a71ff97896479f528501c91326407c532f4fc0",
      "sequence_native_full_model_legacy_integer/v1",
      "a0b7b9f0a2c07f5b6da412e29c1aea6cc57af6880f354ed0715338818803c1af",
      "sequence_official_linebreaks_body.txt"},
     {"xy_official_short_title_projection_supplement.json",
      "05ef17a1213023740db04f19f884325d310105328b98bdbaf8018b21c19678de",
      "xy_official_short_title_source_facts.json",
      "00dd7b8d32638825d990f4bd5aaec1e2dda1bd0cc37c875d0f052ff7939d3db5",
      "xy_source_full_semantics_with_foreign_layout/v1",
      "3018f9355d47cb2d5a464d5f6b3cdac7dbdf71c61284132f58cb47e1f0060d0a",
      "xy_official_short_title_raw.txt", ProjectionKind::XYOriginal}}};

std::string projection_canonical(JsonVal value) {
    if (value.is_obj()) {
        std::map<std::string, std::string> fields;
        value.iter_obj([&](JsonVal key, JsonVal child) {
            require(fields.emplace(std::string(key.as_str()), projection_canonical(child)).second,
                    "Projection duplicate object key");
        });
        std::string result = "{";
        for (const auto& [key, child] : fields) {
            if (result.size() > 1) result += ',';
            json::JsonMutDoc quoted;
            quoted.set_root(quoted.string(key));
            result += quoted.to_string() + ':' + child;
        }
        return result + '}';
    }
    if (value.is_arr()) {
        std::string result = "[";
        value.iter([&](JsonVal child) {
            if (result.size() > 1) result += ',';
            result += projection_canonical(child);
        });
        return result + ']';
    }
    require(static_cast<bool>(value), "Projection missing canonical value");
    return value.to_string();
}

void projection_fields(JsonVal object, std::initializer_list<std::string_view> names) {
    require(object.is_obj(), "Projection expected object");
    std::set<std::string> required;
    for (const auto name : names)
        required.emplace(name);
    std::set<std::string> seen;
    object.iter_obj([&](JsonVal key, JsonVal) {
        const std::string name(key.as_str());
        require(seen.insert(name).second, "Projection duplicate object key");
        require(required.contains(name), "Projection unknown field: " + name);
    });
    require(seen == required, "Projection missing field");
}

void projection_string(JsonVal value, std::string_view context) {
    require(value.is_str() && !value.as_str().empty(),
            "Projection string type: " + std::string(context));
}

void projection_int(JsonVal value, std::int64_t expected, std::string_view context) {
    require(value.is_int() && value.as_int() == expected,
            "Projection integer/type mismatch: " + std::string(context));
}

std::string projection_summary(JsonVal model, const ProjectionBinding& binding) {
    if (binding.model_schema == "sequence_native_full_model_legacy_integer/v1") {
        require(model.get("actors").is_arr() && model.get("actors").size() == 4 &&
                    model.get("messages").is_arr() && model.get("messages").size() == 4 &&
                    model.get("notes").is_arr() && model.get("notes").size() == 4,
                "Projection full Model shape mismatch");
        json::JsonMutDoc document;
        auto checks = document.object();
        auto labels = checks.make_arr();
        std::set<std::string> seen;
        auto add_rows = [&](JsonVal text) {
            projection_string(text, "multiline display text");
            std::istringstream rows{std::string(text.as_str())};
            std::string row;
            while (std::getline(rows, row))
                if (!row.empty() && seen.insert(row).second) labels.append(checks.make_str(row));
        };
        model.get("actors").iter([&](JsonVal actor) { add_rows(actor.get("label")); });
        model.get("messages").iter([&](JsonVal message) {
            require(message.get("number").is_int() && message.get_int("number") == 0 &&
                        message.get_string("arrow") == "Filled",
                    "Projection summary arrow/number protocol mismatch");
            add_rows(message.get("label"));
        });
        model.get("notes").iter([&](JsonVal note) { add_rows(note.get("text")); });
        checks.add("labels", labels);
        checks.set("arrows", 4);
        checks.add("ordering", checks.make_arr());
        document.set_root(checks);
        const auto parsed = json::parse(document.to_string());
        require(parsed.has_value(), "Cannot construct projection summary");
        return projection_canonical(parsed->root());
    }
    // This is the existing official harness's summary, not a second geometry
    // oracle. The separately reviewed native test enforces the full Model.
    const auto actors = model.get("actors");
    const auto messages = model.get("messages");
    require(actors.is_arr() && actors.size() == 2 && messages.is_arr() && messages.size() == 4,
            "Projection full Model shape mismatch");
    json::JsonMutDoc document;
    auto checks = document.object();
    auto labels = checks.make_arr();
    std::vector<std::string> text;
    actors.iter([&](JsonVal actor) {
        projection_string(actor.get("label"), "actor label");
        text.push_back(actor.get_string("label"));
    });
    messages.iter([&](JsonVal message) {
        projection_string(message.get("label"), "message label");
        projection_string(message.get("number"), "message number");
        require(message.get_string("arrow") == "Unheaded",
                "Projection summary arrow protocol mismatch");
        text.push_back(message.get_string("number") + ". " + message.get_string("label"));
    });
    for (const auto& label : text)
        labels.append(checks.make_str(label));
    checks.add("labels", labels);
    checks.set("arrows", 0);
    auto ordering = checks.make_arr();
    auto append_order = [&](std::size_t before, std::size_t after, const char* axis) {
        auto item = checks.make_obj();
        item.set("before", text[before]);
        item.set("after", text[after]);
        item.set("axis", axis);
        ordering.append(item);
    };
    append_order(0, 1, "x");
    for (std::size_t i = 2; i + 1 < text.size(); ++i)
        append_order(i, i + 1, "y");
    checks.add("ordering", ordering);
    document.set_root(checks);
    auto immutable = json::parse(document.to_string());
    require(immutable.has_value(), "Cannot construct projection summary");
    return projection_canonical(immutable->root());
}

// The parsed semantic validator is also the mutation-test entry point. The
// aggregate byte pins are checked separately by the real loader, so mutations
// below must reach the particular tuple/schema/binding check they exercise.
void validate_projection(JsonVal policy, JsonVal facts, JsonVal manifest, std::string_view raw,
                         std::string_view snapshot, const ProjectionBinding& binding) {
    projection_fields(policy, {"schema", "baseline_manifest_sha256", "source_facts", "promotions"});
    projection_int(policy.get("schema"), 1, "schema");
    require(policy.get("baseline_manifest_sha256").is_str() &&
                policy.get_string("baseline_manifest_sha256") == kManifestSha256,
            "Projection baseline manifest mismatch");
    const auto reference = policy.get("source_facts");
    projection_fields(reference, {"path", "sha256", "schema", "model_schema", "case_index"});
    require(reference.get("path").is_str() && reference.get_string("path") == binding.fact_file &&
                safe_relative(reference.get_string("path")),
            "Projection source-fact path mismatch");
    require(reference.get("sha256").is_str() && reference.get_string("sha256") == binding.fact_sha,
            "Projection source-fact SHA mismatch");
    projection_int(reference.get("schema"), 1, "source-fact schema");
    projection_int(reference.get("case_index"), 0, "source-fact case index");
    require(reference.get("model_schema").is_str() &&
                reference.get_string("model_schema") == binding.model_schema,
            "Projection model schema mismatch");
    projection_int(facts.get("schema"), 1, "actual source-fact schema");
    require(binding.model_schema == kProjectionModelSchema
                ? (facts.get("model_schema").is_str() &&
                   facts.get_string("model_schema") == binding.model_schema)
                : !facts.has("model_schema"),
            "Projection actual model schema mismatch");
    require(facts.get("cases").is_arr() && facts.get("cases").size() == 1,
            "Projection source-fact case inventory mismatch");
    const auto fact = facts.get("cases").at(0);
    require(fact.get_string("model_schema") == binding.model_schema,
            "Projection case model schema mismatch");
    require(policy.get("promotions").is_arr(), "Projection promotions type mismatch");
    std::set<std::pair<std::string, std::string>> seen;
    policy.get("promotions").iter([&](JsonVal entry) {
        projection_fields(entry, {"path", "track", "raw_sha256", "body_offset_utf8_bytes",
                                  "body_sha256", "full_model_sha256", "baseline_expectation_sha256",
                                  "effective", "snapshot_sha256", "reason"});
        projection_string(entry.get("path"), "fixture path");
        const auto path = entry.get_string("path");
        require(safe_relative(path) && fs::path(path).extension() == ".mmd",
                "Projection unsafe fixture path");
        require(entry.get("track").is_str() && entry.get_string("track") == "body_diagnostic",
                "Projection unknown track");
        require(seen.emplace(path, entry.get_string("track")).second,
                "Projection duplicate promotion");
        JsonVal fixture;
        manifest.get("fixtures").iter([&](JsonVal candidate) {
            if (candidate.get_string("path") == path) fixture = candidate;
        });
        require(static_cast<bool>(fixture) && fixture.has("diagnostic"),
                "Projection unknown fixture/body track");
        require(path == fact.get_string("path") && path == fact.get_string("id"),
                "Projection source-fact identity mismatch");
        const auto diagnostic = fixture.get("diagnostic");
        projection_string(entry.get("raw_sha256"), "raw SHA");
        require(entry.get_string("raw_sha256") == digest(raw) &&
                    entry.get_string("raw_sha256") == fixture.get_string("sha256") &&
                    entry.get_string("raw_sha256") == fact.get_string("raw_sha256") &&
                    fact.get("raw").is_str() && fact.get_string("raw") == raw &&
                    fact.get("raw_bytes").is_int() && fact.get_int("raw_bytes") >= 0 &&
                    static_cast<std::size_t>(fact.get_int("raw_bytes")) == raw.size(),
                "Projection raw binding mismatch");
        const auto offset = entry.get("body_offset_utf8_bytes");
        require(offset.is_int() && offset.as_int() >= 0 &&
                    static_cast<std::size_t>(offset.as_int()) < raw.size() &&
                    diagnostic.get("offset").is_int() &&
                    offset.as_int() == diagnostic.get_int("offset") &&
                    fact.get("body_offset_utf8_bytes").is_int() &&
                    offset.as_int() == fact.get_int("body_offset_utf8_bytes"),
                "Projection body offset/type mismatch");
        const auto body = raw.substr(static_cast<std::size_t>(offset.as_int()));
        require(entry.get("body_sha256").is_str() &&
                    entry.get_string("body_sha256") == digest(body) &&
                    entry.get_string("body_sha256") == diagnostic.get_string("sha256") &&
                    entry.get_string("body_sha256") == fact.get_string("body_sha256") &&
                    fact.get("body").is_str() && fact.get_string("body") == body,
                "Projection body binding mismatch");
        require(entry.get("baseline_expectation_sha256").is_str() &&
                    entry.get_string("baseline_expectation_sha256") ==
                        digest(projection_canonical(diagnostic)),
                "Projection baseline expectation mismatch");
        const auto model = fact.get("body_model");
        require(model.is_obj(), "Projection full Model type mismatch");
        const auto model_sha = digest(projection_canonical(model));
        require(entry.get("full_model_sha256").is_str() &&
                    entry.get_string("full_model_sha256") == model_sha &&
                    model_sha == fact.get_string("body_model_sha256") &&
                    model_sha == binding.model_sha,
                "Projection full Model binding mismatch");
        const auto effective = entry.get("effective");
        projection_fields(effective, {"expected", "reason", "checks", "snapshot"});
        require(effective.get("expected").is_str() && effective.get_string("expected") == "drawn",
                "Projection effective outcome mismatch");
        projection_string(effective.get("reason"), "effective reason");
        projection_string(entry.get("reason"), "promotion reason");
        const auto checks = effective.get("checks");
        projection_fields(checks, {"labels", "arrows", "ordering"});
        require(checks.get("ordering").is_arr(), "Projection ordering type mismatch");
        checks.get("ordering").iter([](JsonVal order) {
            projection_fields(order, {"before", "after", "axis"});
        });
        require(projection_canonical(checks) == projection_summary(model, binding),
                "Projection Model/check summary mismatch");
        projection_string(effective.get("snapshot"), "snapshot name");
        const auto name = effective.get_string("snapshot");
        require(safe_relative(name) && fs::path(name).filename() == fs::path(name) &&
                    name == binding.snapshot_file,
                "Projection unsafe/unknown snapshot name");
        require(entry.get("snapshot_sha256").is_str() &&
                    hex_string(entry.get_string("snapshot_sha256"), 64) &&
                    entry.get_string("snapshot_sha256") == digest(snapshot),
                "Projection snapshot SHA mismatch");
        validate_expectation(effective, body);
    });
    require(seen.size() == 1, "Projection missing promotion");
}

constexpr std::string_view kXYProjectionScope =
    "Reviewed fixed-cell native projection retains the complete authored title and data; "
    "original SVG height-driven title omission, font metrics and browser pixels remain "
    "non-equivalent pending.";
constexpr std::string_view kXYProjectionReason =
    "reviewed-native-xy-fixed-cell-title-preserving-projection";

void xy_projection_number(JsonVal value, double expected, std::string_view field) {
    require(value.is_num() && std::isfinite(value.as_double()) && value.as_double() == expected,
            "XY projection numeric/type mismatch: " + std::string(field));
}

void validate_xy_projection(JsonVal policy, JsonVal facts, JsonVal manifest, std::string_view raw,
                            std::string_view snapshot, const ProjectionBinding& binding) {
    require(binding.kind == ProjectionKind::XYOriginal, "XY projection descriptor kind mismatch");
    projection_fields(policy, {"schema", "baseline_manifest_sha256", "source_facts", "promotions"});
    projection_int(policy.get("schema"), 1, "XY policy schema");
    require(policy.get("baseline_manifest_sha256").is_str() &&
                policy.get_string("baseline_manifest_sha256") == kManifestSha256,
            "XY projection baseline manifest mismatch");
    const auto reference = policy.get("source_facts");
    projection_fields(reference, {"path", "sha256", "model_schema"});
    require(reference.get("path").is_str() && reference.get_string("path") == binding.fact_file &&
                safe_relative(reference.get_string("path")),
            "XY projection source-fact path mismatch");
    require(reference.get("sha256").is_str() &&
                reference.get_string("sha256") == binding.fact_sha &&
                reference.get("model_schema").is_str() &&
                reference.get_string("model_schema") == binding.model_schema,
            "XY projection source-fact reference mismatch");
    require(policy.get("promotions").is_arr() && policy.get("promotions").size() == 1,
            "XY projection promotion inventory mismatch");
    const auto entry = policy.get("promotions").at(0);
    projection_fields(entry, {"path", "track", "raw_sha256", "raw_bytes", "body_offset_utf8_bytes",
                              "body_sha256", "full_model_sha256", "baseline_expectation_sha256",
                              "effective", "snapshot_sha256", "reason"});
    projection_string(entry.get("path"), "XY fixture path");
    const auto path = entry.get_string("path");
    require(safe_relative(path) && fs::path(path).extension() == ".mmd",
            "XY projection unsafe fixture path");
    require(entry.get("track").is_str() && entry.get_string("track") == "original",
            "XY projection unknown track");
    JsonVal fixture;
    std::size_t matches = 0;
    manifest.get("fixtures").iter([&](JsonVal candidate) {
        if (candidate.get_string("path") == path) {
            fixture = candidate;
            ++matches;
        }
    });
    require(matches == 1 && fixture.get("raw").is_obj(),
            "XY projection unknown/duplicate raw fixture");
    const auto source = facts.get("source");
    require(source.is_obj(), "XY projection source identity mismatch");
    projection_fields(source, {"path", "fixture_path", "bytes", "sha256", "git_blob",
                               "upstream_path", "commit", "raw_text", "body_offset", "body_bytes",
                               "body_sha256", "physical_witnesses"});
    require(source.get("path").is_str() && source.get_string("path") == path &&
                source.get("fixture_path").is_str() &&
                source.get_string("fixture_path") ==
                    "tests/fixtures/mermaid_official/cases/" + path &&
                source.get("upstream_path").is_str() &&
                source.get_string("upstream_path") == "e2e/diagrams/" + path &&
                source.get_string("commit") == manifest.get("upstream").get_string("commit") &&
                source.get_string("git_blob") == fixture.get_string("git_blob"),
            "XY projection source identity mismatch");
    projection_int(entry.get("raw_bytes"), 222, "XY raw bytes");
    projection_int(source.get("bytes"), 222, "XY fact raw bytes");
    require(raw.size() == 222 && source.get("raw_text").is_str() &&
                source.get_string("raw_text") == raw && entry.get("raw_sha256").is_str() &&
                entry.get_string("raw_sha256") == digest(raw) &&
                entry.get_string("raw_sha256") == fixture.get_string("sha256") &&
                source.get_string("sha256") == entry.get_string("raw_sha256"),
            "XY projection raw binding mismatch");
    projection_int(entry.get("body_offset_utf8_bytes"), 58, "XY body offset");
    projection_int(source.get("body_offset"), 58, "XY fact body offset");
    projection_int(source.get("body_bytes"), 164, "XY body bytes");
    require(raw.size() > 58 && entry.get("body_sha256").is_str() &&
                entry.get_string("body_sha256") == digest(raw.substr(58)) &&
                source.get_string("body_sha256") == entry.get_string("body_sha256"),
            "XY projection body binding mismatch");
    const auto witnesses = source.get("physical_witnesses");
    require(witnesses.is_arr() && witnesses.size() == 11,
            "XY projection witness inventory mismatch");
    std::size_t cursor = 0;
    for (std::size_t n = 0; n < witnesses.size(); ++n) {
        const auto witness = witnesses.at(n);
        projection_fields(witness, {"line", "byte_offset", "byte_length", "sha256", "text"});
        projection_int(witness.get("line"), static_cast<std::int64_t>(n + 1), "XY witness line");
        projection_int(witness.get("byte_offset"), static_cast<std::int64_t>(cursor),
                       "XY witness offset");
        const auto newline = raw.find('\n', cursor);
        require(newline != std::string_view::npos, "XY projection witness line boundary mismatch");
        const auto length = newline + 1 - cursor;
        projection_int(witness.get("byte_length"), static_cast<std::int64_t>(length),
                       "XY witness length");
        require(witness.get("text").is_str() &&
                    witness.get_string("text") == raw.substr(cursor, length) &&
                    witness.get("sha256").is_str() &&
                    witness.get_string("sha256") == digest(raw.substr(cursor, length)),
                "XY projection witness bytes/SHA mismatch");
        cursor += length;
    }
    require(cursor == raw.size(), "XY projection witness coverage mismatch");
    const auto model = facts.get("source_model");
    projection_fields(model,
                      {"schema", "orientation", "title", "xAxis", "yAxis", "plots", "accessibility",
                       "metadata", "source_annotations", "init_directives"});
    require(model.get("schema").is_str() && model.get_string("schema") == binding.model_schema &&
                model.get("orientation").is_str() &&
                model.get_string("orientation") == "vertical" && model.get("title").is_str() &&
                model.get_string("title") == "Too short for a title",
            "XY projection title/schema/orientation mismatch");
    for (const auto name : {"xAxis", "yAxis"}) {
        const auto axis = model.get(name);
        projection_fields(axis, {"type", "title", "min", "max"});
        require(axis.get("type").is_str() && axis.get_string("type") == "linear" &&
                    axis.get("title").is_str() &&
                    axis.get_string("title") ==
                        (std::string_view(name) == "xAxis" ? "Elapsed Time" : "Percentage (%)"),
                "XY projection axis identity mismatch");
        xy_projection_number(axis.get("min"), 0, name);
        xy_projection_number(axis.get("max"), std::string_view(name) == "xAxis" ? 10 : 100, name);
    }
    require(model.get("plots").is_arr() && model.get("plots").size() == 1,
            "XY projection plot inventory mismatch");
    const auto plot = model.get("plots").at(0);
    projection_fields(plot, {"type", "title", "values", "point_labels", "authored_order",
                             "source_derived_samples"});
    require(plot.get("type").is_str() && plot.get_string("type") == "line" &&
                plot.get("title").is_str() && plot.get_string("title") == "CPU",
            "XY projection plot identity mismatch");
    projection_int(plot.get("authored_order"), 0, "XY plot order");
    const auto values = plot.get("values"), labels = plot.get("point_labels"),
               samples = plot.get("source_derived_samples");
    require(values.is_arr() && values.size() == 10 && labels.is_arr() && labels.size() == 10 &&
                samples.is_arr() && samples.size() == 10,
            "XY projection sample inventory mismatch");
    constexpr std::array<double, 10> x{0,
                                       1.1111111111111112,
                                       2.2222222222222223,
                                       3.3333333333333335,
                                       4.444444444444445,
                                       5.555555555555555,
                                       6.666666666666667,
                                       7.777777777777779,
                                       8.88888888888889,
                                       10};
    constexpr std::array<std::string_view, 10> exact{"0",    "10/9", "20/9", "10/3", "40/9",
                                                     "50/9", "20/3", "70/9", "80/9", "10"};
    for (std::size_t n = 0; n < 10; ++n) {
        const auto sample = samples.at(n);
        projection_fields(sample, {"index", "x_exact", "x_binary64", "y"});
        projection_int(sample.get("index"), static_cast<std::int64_t>(n), "XY sample order");
        require(sample.get("x_exact").is_str() && sample.get_string("x_exact") == exact[n],
                "XY projection rational x mismatch");
        xy_projection_number(sample.get("x_binary64"), x[n], "sample x");
        xy_projection_number(sample.get("y"), 10.0 * static_cast<double>(n + 1), "sample y");
        xy_projection_number(values.at(n), 10.0 * static_cast<double>(n + 1), "value");
        require(labels.at(n).is_str() && labels.at(n).as_str().empty(),
                "XY projection point label mismatch");
    }
    const auto metadata = model.get("metadata");
    projection_fields(metadata, {"config", "title", "description"});
    projection_fields(metadata.get("config"), {"xyChart"});
    projection_fields(metadata.get("config").get("xyChart"), {"width", "height"});
    projection_int(metadata.get("config").get("xyChart").get("width"), 1200, "XY metadata width");
    projection_int(metadata.get("config").get("xyChart").get("height"), 60, "XY metadata height");
    require(metadata.get("title").is_null() && metadata.get("description").is_null(),
            "XY projection metadata title/description mismatch");
    projection_fields(model.get("accessibility"), {"title", "description"});
    for (const auto name : {"title", "description"})
        require(model.get("accessibility").get(name).is_str() &&
                    model.get("accessibility").get_string(name).empty(),
                "XY projection accessibility mismatch");
    for (const auto name : {"source_annotations", "init_directives"})
        require(model.get(name).is_arr() && model.get(name).size() == 0,
                "XY projection annotation/init mismatch");
    const auto model_sha = digest(projection_canonical(model));
    require(facts.get("source_model_sha256").is_str() &&
                facts.get_string("source_model_sha256") == model_sha &&
                entry.get("full_model_sha256").is_str() &&
                entry.get_string("full_model_sha256") == model_sha &&
                model_sha == binding.model_sha,
            "XY projection full Model binding mismatch");
    require(entry.get("baseline_expectation_sha256").is_str() &&
                entry.get_string("baseline_expectation_sha256") ==
                    digest(projection_canonical(fixture.get("raw"))),
            "XY projection raw baseline expectation mismatch");
    const auto effective = entry.get("effective");
    projection_fields(effective, {"expected", "reason", "checks", "snapshot"});
    require(effective.get("expected").is_str() && effective.get_string("expected") == "drawn" &&
                effective.get("reason").is_str() &&
                effective.get_string("reason") == kXYProjectionReason,
            "XY projection effective outcome/profile mismatch");
    require(entry.get("reason").is_str() &&
                entry.get_string("reason") ==
                    "Complete raw source and metadata are bound. Fixed-cell native projection "
                    "preserves the authored title; original height60 SVG title-omission/font/pixel "
                    "predicates remain non-equivalent pending.",
            "XY projection foreign pending scope mismatch");
    const auto checks = effective.get("checks");
    projection_fields(checks, {"labels", "arrows", "ordering"});
    const std::array<std::string, 4> expected_labels{
        model.get_string("title"), model.get("xAxis").get_string("title"),
        model.get("yAxis").get_string("title"), plot.get_string("title")};
    require(checks.get("labels").is_arr() && checks.get("labels").size() == expected_labels.size(),
            "XY projection summary label inventory mismatch");
    for (std::size_t n = 0; n < expected_labels.size(); ++n)
        require(checks.get("labels").at(n).is_str() &&
                    checks.get("labels").at(n).as_str() == expected_labels[n],
                "XY projection summary label mismatch");
    projection_int(checks.get("arrows"), 0, "XY summary arrows");
    require(checks.get("ordering").is_arr() && checks.get("ordering").size() == 0,
            "XY projection summary ordering mismatch");
    require(effective.get("snapshot").is_str() && safe_relative(effective.get_string("snapshot")) &&
                fs::path(effective.get_string("snapshot")).filename() ==
                    fs::path(effective.get_string("snapshot")) &&
                effective.get_string("snapshot") == binding.snapshot_file,
            "XY projection unsafe/unknown snapshot name");
    require(entry.get("snapshot_sha256").is_str() &&
                hex_string(entry.get_string("snapshot_sha256"), 64) &&
                entry.get_string("snapshot_sha256") == digest(snapshot),
            "XY projection snapshot SHA mismatch");
    validate_expectation(effective, raw);
}

std::string_view projection_track(const ProjectionBinding& binding) {
    switch (binding.kind) {
    case ProjectionKind::SequenceBody:
        return "body_diagnostic";
    case ProjectionKind::XYOriginal:
        return "original";
    }
    throw std::runtime_error("Projection unknown descriptor kind");
}

JsonVal projection_source_path(JsonVal facts, const ProjectionBinding& binding) {
    switch (binding.kind) {
    case ProjectionKind::SequenceBody:
        return facts.get("cases").at(0).get("path");
    case ProjectionKind::XYOriginal:
        return facts.get("source").get("path");
    }
    throw std::runtime_error("Projection unknown descriptor kind");
}

void validate_projection_binding(JsonVal policy, JsonVal facts, JsonVal manifest,
                                 std::string_view raw, std::string_view snapshot,
                                 const ProjectionBinding& binding) {
    switch (binding.kind) {
    case ProjectionKind::SequenceBody:
        validate_projection(policy, facts, manifest, raw, snapshot, binding);
        return;
    case ProjectionKind::XYOriginal:
        validate_xy_projection(policy, facts, manifest, raw, snapshot, binding);
        return;
    }
    throw std::runtime_error("Projection unknown descriptor kind");
}

std::optional<std::size_t> projection_select(std::span<const json::JsonDoc> policies,
                                             std::span<const ProjectionBinding> bindings,
                                             std::string_view path, std::string_view track) {
    require(policies.size() == bindings.size(), "Projection selector inventory mismatch");
    require(track == "original" || track == "body_diagnostic", "Projection selector unknown track");
    std::optional<std::size_t> selected;
    for (std::size_t n = 0; n < bindings.size(); ++n) {
        const auto expected_track = projection_track(bindings[n]);
        const auto promotions = policies[n].root().get("promotions");
        require(promotions.is_arr() && promotions.size() == 1,
                "Projection selector promotion inventory mismatch");
        const auto promotion = promotions.at(0);
        require(promotion.get("track").is_str() && promotion.get_string("track") == expected_track,
                "Projection selector kind/track mismatch");
        if (promotion.get_string("path") == path && expected_track == track) {
            require(!selected, "Projection duplicate promotion across bindings");
            selected = n;
        }
    }
    return selected;
}

void projection_byte_pins(std::string_view policy, std::string_view facts,
                          const ProjectionBinding& binding) {
    require(digest(policy) == binding.policy_sha, "Projection supplement SHA-256 mismatch");
    require(digest(facts) == binding.fact_sha, "Projection actual source-fact SHA-256 mismatch");
}

void projection_regular_file(const fs::path& root, const fs::path& path) {
    require(!fs::is_symlink(fs::symlink_status(root)) && fs::is_directory(root),
            "Projection unsafe native root");
    require(path.parent_path() == root && !fs::is_symlink(fs::symlink_status(path)) &&
                fs::is_regular_file(path),
            "Projection unsafe native asset");
}

void projection_consumed(std::size_t count) {
    require(count == 1, "Projection missing/duplicate/unconsumed application");
}

// SCRATCH: asset pins reviewed separately; harness installation remains gated.
// The three original ProjectionBinding objects and their validators stay intact.
enum class RoleBatchKind { SequenceRoleBodyBatch };

struct RoleBatchBinding {
    std::string_view policy_file, policy_sha, fact_file, fact_sha, model_schema;
    RoleBatchKind kind = RoleBatchKind::SequenceRoleBodyBatch;
};

constexpr RoleBatchBinding kRoleBatchBinding{
    "sequence_official_body106_projection_supplement.json",
    "0ff9a6e7eeaa492fb59face21f72418411151c61bb7cd807320e4a19f735c124",
    "sequence_official_body106_facts.json",
    "0c96612c8974cc18c66496f2c76bcc3678c8647134567741cd140a2531a96fa6",
    "sequence_native_single_role_number_text/v1"};
constexpr std::size_t kRoleBatchIndex = kProjectionBindings.size();
constexpr std::size_t kRoleBatchLegacyCases = 38, kRoleBatchLegacyPromotions = 51;
constexpr std::size_t kRoleBatchRoleCases = 41, kRoleBatchRolePromotions = 57;
constexpr std::size_t kRoleBatchPriorCases = 57, kRoleBatchPriorPromotions = 73;
constexpr std::size_t kRoleBatchCases = 106, kRoleBatchPromotions = 122;
constexpr std::string_view kRoleBatchConfigFactsFile = "sequence_official_config49_facts.json";
constexpr std::string_view kRoleBatchConfigFactsSha =
    "699e1956198b1caa55fe02a0483700ac8994b8c83eaa9f6a33b2fde2ed66edb6";
constexpr std::string_view kRoleBatchConfigScope =
    "Reviewed complete configuration-bearing body native fixed-cell projection; original raw "
    "metadata, theme/CSS/font/margins/accessibility/browser/SVG/API predicates remain pending.";
constexpr std::string_view kRoleBatchLegacyScope =
    "Reviewed complete38 body native Role projection; original raw metadata, browser/SVG/ASCII/API "
    "predicates remain pending.";
constexpr std::string_view kRoleBatchAppendedScope =
    "Reviewed complete body native Role projection; original raw metadata, browser/SVG/ASCII/API "
    "predicates remain pending.";
constexpr std::string_view kRoleBatchHalfScope =
    "Reviewed complete half-arrow body native Unicode terminal projection; original raw "
    "description, browser/SVG/ASCII/API predicates remain pending.";
constexpr std::string_view kRoleBatchScope =
    "Reviewed native body batch: original38/51, fragment3/6, half16/16 and configuration49/49; "
    "original raw metadata, browser/SVG/ASCII/API predicates remain pending.";

std::string_view role_batch_case_scope(std::size_t index) {
    require(index < kRoleBatchCases, "Role batch scope case outside inventory");
    return index < kRoleBatchLegacyCases  ? kRoleBatchLegacyScope
           : index < kRoleBatchRoleCases  ? kRoleBatchAppendedScope
           : index < kRoleBatchPriorCases ? kRoleBatchHalfScope
                                          : kRoleBatchConfigScope;
}

struct Selected {
    std::size_t binding, promotion;
};

void role_batch_kind(const RoleBatchBinding& binding) {
    require(binding.kind == RoleBatchKind::SequenceRoleBodyBatch,
            "Role batch unknown descriptor kind");
}

void role_batch_text(JsonVal text, std::string_view context) {
    require(text.is_str(), "Role batch text/type mismatch: " + std::string(context));
}
enum class RoleBatchNumbers { HiddenOnly, DisplayText };

JsonVal role_batch_model(JsonVal fact, std::size_t index) {
    require(index < kRoleBatchCases, "Role batch Model index outside inventory");
    return fact.get(index < kRoleBatchPriorCases ? "model" : "full_native_model_candidate");
}

std::size_t role_batch_occurrences(JsonVal fact, std::size_t index) {
    require(index < kRoleBatchCases, "Role batch occurrence index outside inventory");
    return index < kRoleBatchPriorCases ? fact.get("original_bindings").size() : 1;
}

std::string role_batch_summary(JsonVal model,
                               RoleBatchNumbers numbers = RoleBatchNumbers::HiddenOnly) {
    require(numbers == RoleBatchNumbers::HiddenOnly || numbers == RoleBatchNumbers::DisplayText,
            "Role batch unknown number profile");
    projection_fields(model,
                      {"title", "actors", "messages", "notes", "fragments", "boxes", "order"});
    for (const auto name : {"actors", "messages", "notes", "fragments", "boxes", "order"})
        require(model.get(name).is_arr(), "Role batch Model collection/type mismatch");
    const auto actors = model.get("actors");
    require(actors.size() > 0 && actors.size() <= 32, "Role batch actor inventory");
    require(model.get("messages").size() <= 128 && model.get("notes").size() <= 64 &&
                model.get("fragments").size() <= 64 && model.get("boxes").size() <= 64 &&
                model.get("order").size() <= 512,
            "Role batch Model inventory cap");
    json::JsonMutDoc doc;
    auto checks = doc.object(), labels = checks.make_arr();
    std::set<std::string> seen;
    int triangles = 0;
    auto add = [&](JsonVal value) {
        role_batch_text(value, "visible text");
        std::istringstream rows{std::string(value.as_str())};
        std::string row;
        while (std::getline(rows, row)) {
            // Count source-visible scanner-set glyphs per actual occurrence,
            // before deduplicating the lightweight label membership list.
            for (const auto& glyph : ftxui::Utf8ToGlyphs(row))
                if (glyph == "▶" || glyph == "◀" || glyph == "▼" || glyph == "▲") ++triangles;
            if (!row.empty() && seen.insert(row).second) labels.append(checks.make_str(row));
        }
    };
    auto text = [&](const std::string& value) {
        json::JsonMutDoc tmp;
        tmp.set_root(tmp.string(value));
        auto parsed = json::parse(tmp.to_string());
        require(parsed.has_value(), "Role batch text construction");
        add(parsed->root());
    };
    auto index = [&](JsonVal value, std::size_t bound, std::string_view field) {
        require(value.is_int() && value.as_int() >= 0 &&
                    static_cast<std::size_t>(value.as_int()) < bound,
                "Role batch index/type mismatch: " + std::string(field));
    };
    add(model.get("title"));
    const std::set<std::string> roles{"Participant", "Actor",    "Boundary",    "Control",
                                      "Entity",      "Database", "Collections", "Queue"};
    actors.iter([&](JsonVal actor) {
        projection_fields(actor, {"id", "label", "role"});
        projection_string(actor.get("id"), "Role actor id");
        projection_string(actor.get("label"), "Role actor alias");
        require(actor.get("role").is_str() && roles.contains(actor.get_string("role")),
                "Role batch unknown Role");
        if (actor.get_string("role") == "Control") ++triangles;
        add(actor.get("label"));
    });
    const std::set<std::string> arrows{"Filled",
                                       "Both",
                                       "Unheaded",
                                       "Cross",
                                       "Async",
                                       "FilledHalfTop",
                                       "FilledHalfBottom",
                                       "OpenHalfTop",
                                       "OpenHalfBottom",
                                       "FilledHalfTopReverse",
                                       "FilledHalfBottomReverse",
                                       "OpenHalfTopReverse",
                                       "OpenHalfBottomReverse"};
    model.get("messages").iter([&](JsonVal message) {
        projection_fields(message, {"from", "to", "label", "arrow", "dashed", "central_from",
                                    "central_to", "number"});
        index(message.get("from"), actors.size(), "sender");
        index(message.get("to"), actors.size(), "receiver");
        require(message.get("arrow").is_str() && arrows.contains(message.get_string("arrow")),
                "Role batch unknown Arrow");
        require(message.get("number").is_str(),
                numbers == RoleBatchNumbers::HiddenOnly
                    ? "Role batch DisplayText hidden-number mismatch"
                    : "Role batch DisplayText number type mismatch");
        const auto number = message.get("number").as_str();
        if (numbers == RoleBatchNumbers::HiddenOnly)
            require(number.empty(), "Role batch DisplayText hidden-number mismatch");
        else if (!number.empty()) {
            require(number.size() <= 256 &&
                        number.find_first_not_of("0123456789.e+-") == std::string_view::npos,
                    "Role batch DisplayText number spelling mismatch");
            double value = 0;
            const auto [end, error] =
                std::from_chars(number.data(), number.data() + number.size(), value);
            require(error == std::errc{} && end == number.data() + number.size() &&
                        std::isfinite(value) && value > 0,
                    "Role batch DisplayText positive finite number mismatch");
        }
        for (const auto name : {"dashed", "central_from", "central_to"})
            require(message.get(name).is_bool(), "Role batch message flag/type mismatch");
        triangles += message.get_string("arrow") == "Filled" ? 1
                     : message.get_string("arrow") == "Both" ? 2
                                                             : 0;
        role_batch_text(message.get("label"), "message label");
        if (numbers == RoleBatchNumbers::DisplayText && !number.empty())
            text(std::string(number) + ". " + message.get_string("label"));
        else
            add(message.get("label"));
    });
    model.get("notes").iter([&](JsonVal note) {
        projection_fields(note, {"actors", "text", "position", "after"});
        require(note.get("actors").is_arr() && note.get("actors").size() > 0,
                "Role batch note owner inventory");
        note.get("actors").iter([&](JsonVal actor) { index(actor, actors.size(), "note owner"); });
        require(note.get("position").is_str() && (note.get_string("position") == "left" ||
                                                  note.get_string("position") == "right" ||
                                                  note.get_string("position") == "over"),
                "Role batch note position");
        require(note.get("after").is_int() && note.get_int("after") >= -1 &&
                    note.get_int("after") < static_cast<std::int64_t>(model.get("messages").size()),
                "Role batch note chronology/type");
        add(note.get("text"));
    });
    const std::set<std::string> kinds{"loop",     "opt",      "alt",   "par",
                                      "par_over", "critical", "break", "rect"};
    model.get("fragments").iter([&](JsonVal fragment) {
        projection_fields(fragment, {"kind", "label", "parent", "dividers"});
        const auto kind = fragment.get_string("kind");
        require(fragment.get("kind").is_str() && kinds.contains(kind),
                "Role batch unknown fragment kind");
        role_batch_text(fragment.get("label"), "fragment label");
        require(fragment.get("parent").is_int() && fragment.get_int("parent") >= -1 &&
                    fragment.get_int("parent") <
                        static_cast<std::int64_t>(model.get("fragments").size()),
                "Role batch fragment parent/type");
        require(fragment.get("dividers").is_arr(), "Role batch fragment divider/type");
        text(kind +
             (fragment.get_string("label").empty() ? "" : " " + fragment.get_string("label")));
        fragment.get("dividers").iter([&](JsonVal divider) {
            role_batch_text(divider, "fragment divider");
            const std::string head = kind == "alt"        ? "else"
                                     : kind == "par"      ? "and"
                                     : kind == "critical" ? "option"
                                                          : "";
            require(!head.empty(), "Role batch divider on nonbranching fragment");
            text(head + (divider.as_str().empty() ? "" : " " + std::string(divider.as_str())));
        });
    });
    model.get("boxes").iter([&](JsonVal box) {
        projection_fields(box, {"actors", "title", "color"});
        require(box.get("actors").is_arr() && box.get("actors").size() > 0,
                "Role batch box owner inventory");
        box.get("actors").iter([&](JsonVal actor) { index(actor, actors.size(), "box owner"); });
        add(box.get("title"));
        role_batch_text(box.get("color"), "box authored color");
        if (!box.get_string("color").empty()) text("color: " + box.get_string("color"));
    });
    const std::map<std::string, std::size_t> events{{"Message", model.get("messages").size()},
                                                    {"Note", model.get("notes").size()},
                                                    {"Open", model.get("fragments").size()},
                                                    {"Divide", model.get("fragments").size()},
                                                    {"Close", model.get("fragments").size()},
                                                    {"Create", actors.size()},
                                                    {"Destroy", actors.size()},
                                                    {"Activate", actors.size()},
                                                    {"Deactivate", actors.size()}};
    model.get("order").iter([&](JsonVal event) {
        projection_fields(event, {"kind", "index", "part"});
        require(event.get("kind").is_str() && events.contains(event.get_string("kind")),
                "Role batch unknown ordered kind");
        index(event.get("index"), events.at(event.get_string("kind")), "ordered event");
        require(event.get("part").is_int() && event.get_int("part") >= 0,
                "Role batch event part/type");
    });
    require(labels.size() > 0, "Role batch empty visible text summary");
    checks.add("labels", labels);
    checks.set("arrows", triangles);
    // Full native geometry owns chronological/endpoint identity. Empty here
    // avoids ambiguous first-substring label_position order assignments.
    checks.add("ordering", checks.make_arr());
    doc.set_root(checks);
    auto immutable = json::parse(doc.to_string());
    require(immutable.has_value(), "Role batch summary construction");
    return projection_canonical(immutable->root());
}

std::string role_batch_snapshot_name(JsonVal fact, std::size_t index = 0) {
    require(index < kRoleBatchCases, "Role batch snapshot profile outside inventory");
    return std::string(index < kRoleBatchPriorCases ? "sequence_official_role_"
                                                    : "sequence_official_config_") +
           fact.get_string("body_sha256") + ".txt";
}

using RoleAssetReader = std::function<std::string(std::string_view)>;

void validate_role_batch(JsonVal policy, JsonVal facts, JsonVal manifest,
                         const RoleAssetReader& raw_read, const RoleAssetReader& snapshot_read,
                         JsonVal config_authority,
                         const RoleBatchBinding& binding = kRoleBatchBinding) {
    require(config_authority.get("schema").is_int() && config_authority.get_int("schema") == 1 &&
                config_authority.get_string("immutable_manifest_sha256") == kManifestSha256 &&
                config_authority.get_string("number_encoding") == "DisplayText" &&
                config_authority.get("cases").is_arr() &&
                config_authority.get("cases").size() == 49,
            "Role batch config source authority schema");
    role_batch_kind(binding);
    projection_fields(policy,
                      {"schema", "kind", "baseline_manifest_sha256", "source_facts", "promotions"});
    projection_int(policy.get("schema"), 1, "Role batch policy schema");
    require(policy.get("kind").is_str() &&
                policy.get_string("kind") == "sequence-role-body-batch/v1",
            "Role batch policy kind mismatch");
    require(policy.get("baseline_manifest_sha256").is_str() &&
                policy.get_string("baseline_manifest_sha256") == kManifestSha256,
            "Role batch baseline manifest mismatch");
    const auto ref = policy.get("source_facts");
    projection_fields(ref, {"path", "sha256", "schema", "model_schema"});
    projection_int(ref.get("schema"), 1, "Role batch referenced fact schema");
    require(ref.get("path").is_str() && safe_relative(ref.get_string("path")) &&
                ref.get_string("path") == binding.fact_file,
            "Role batch source-fact path mismatch");
    require(ref.get("sha256").is_str() && ref.get_string("sha256") == binding.fact_sha &&
                ref.get("model_schema").is_str() &&
                ref.get_string("model_schema") == binding.model_schema,
            "Role batch source-fact reference mismatch");
    projection_int(facts.get("schema"), 1, "Role batch fact schema");
    require(facts.get("immutable_manifest_sha256").is_str() &&
                facts.get_string("immutable_manifest_sha256") == kManifestSha256,
            "Role batch fact baseline mismatch");
    const auto cases = facts.get("cases");
    require(cases.is_arr() && cases.size() == kRoleBatchCases,
            "Role batch complete case inventory mismatch");
    std::set<std::string> bodies;
    std::array<std::size_t, kRoleBatchCases> expected{}, actual{};
    for (std::size_t n = 0; n < cases.size(); ++n) {
        const auto fact = cases.at(n);
        const auto body = fact.get_string("body"), body_sha = fact.get_string("body_sha256");
        require(fact.get("body").is_str() && !body.empty() &&
                    body.size() <= (n < kRoleBatchPriorCases ? 1000u : 65536u) &&
                    fact.get("body_sha256").is_str() && digest(body) == body_sha &&
                    bodies.insert(body_sha).second,
                "Role batch exact whole body mismatch/duplicate");
        require(fact.get("id").is_str() &&
                    fact.get_string("id") ==
                        std::string(n < kRoleBatchRoleCases    ? "official-role-body/"
                                    : n < kRoleBatchPriorCases ? "official-half-body/"
                                                               : "official-config-body/") +
                            body_sha.substr(0, 16),
                "Role batch source case identity mismatch");
        projection_int(fact.get("body_bytes"), static_cast<std::int64_t>(body.size()),
                       "Role batch body size");
        if (n < kRoleBatchPriorCases)
            require(fact.get("number_encoding").is_str() &&
                        fact.get_string("number_encoding") == "DisplayText",
                    "Role batch case number encoding");
        const auto model = role_batch_model(fact, n);
        require(fact.get("model_canonical_sha256").is_str() &&
                    digest(projection_canonical(model)) ==
                        fact.get_string("model_canonical_sha256"),
                "Role batch fact Model hash mismatch");
        (void)role_batch_summary(model, n < kRoleBatchPriorCases ? RoleBatchNumbers::HiddenOnly
                                                                 : RoleBatchNumbers::DisplayText);
        if (n >= kRoleBatchRoleCases && n < kRoleBatchPriorCases) {
            const auto statements = fact.get("complete_statement_bindings"),
                       model = fact.get("model");
            require(statements.is_arr() && statements.size() == 4 &&
                        statements.at(0).get_string("kind") == "header" &&
                        statements.at(0).get_string("source_statement") == "sequenceDiagram",
                    "Role batch half complete statement inventory");
            require(model.get("messages").size() == 3 && model.get("order").size() == 3,
                    "Role batch half complete message inventory");
            for (std::size_t message = 0; message < 3; ++message) {
                const auto statement = statements.at(message + 1);
                require(statement.get_string("kind") == "Message" &&
                            projection_canonical(statement.get("native_message")) ==
                                projection_canonical(model.get("messages").at(message)) &&
                            projection_canonical(statement.get("native_order")) ==
                                projection_canonical(model.get("order").at(message)),
                        "Role batch half source Message/order witness mismatch");
            }
        }
        if (n < kRoleBatchPriorCases) {
            const auto originals = fact.get("original_bindings");
            require(originals.is_arr() && originals.size() > 0 &&
                        originals.size() <= kRoleBatchPromotions,
                    "Role batch source occurrence inventory mismatch");
            expected[n] = originals.size();
        } else {
            require(fact.get("fixture").is_str() && safe_relative(fact.get_string("fixture")) &&
                        fact.get_string("fixture").starts_with("sequence/"),
                    "Role batch config source fixture namespace");
            expected[n] = 1;
        }
    }
    const auto promotions = policy.get("promotions");
    require(promotions.is_arr() && promotions.size() == kRoleBatchPromotions,
            "Role batch promotion inventory mismatch");
    std::set<std::pair<std::string, std::string>> seen;
    promotions.iter([&](JsonVal entry) {
        projection_fields(entry, {"path", "track", "case_index", "case_id", "raw_sha256",
                                  "body_offset_utf8_bytes", "body_sha256", "full_model_sha256",
                                  "baseline_expectation_sha256", "effective", "snapshot_sha256",
                                  "reason"});
        projection_string(entry.get("path"), "Role batch original path");
        const auto path = entry.get_string("path");
        require(safe_relative(path) && path.starts_with("sequence/") &&
                    fs::path(path).extension() == ".mmd",
                "Role batch unsafe fixture path");
        require(entry.get("track").is_str() && entry.get_string("track") == "body_diagnostic",
                "Role batch unknown track");
        require(seen.emplace(path, "body_diagnostic").second, "Role batch duplicate promotion");
        require(entry.get("case_index").is_int() && entry.get_int("case_index") >= 0 &&
                    entry.get_int("case_index") < static_cast<std::int64_t>(kRoleBatchCases),
                "Role batch case index/type mismatch");
        const auto n = static_cast<std::size_t>(entry.get_int("case_index"));
        const auto fact = cases.at(n);
        require(entry.get("case_id").is_str() &&
                    entry.get_string("case_id") == fact.get_string("id"),
                "Role batch promotion case identity mismatch");
        JsonVal fixture, source_binding;
        std::size_t fixture_matches = 0, binding_matches = 0;
        manifest.get("fixtures").iter([&](JsonVal item) {
            if (item.get_string("path") == path) {
                fixture = item;
                ++fixture_matches;
            }
        });
        if (n < kRoleBatchPriorCases)
            fact.get("original_bindings").iter([&](JsonVal item) {
                if (item.get_string("path") == path) {
                    source_binding = item;
                    ++binding_matches;
                }
            });
        else if (fact.get_string("fixture") == path) {
            source_binding = fact;
            ++binding_matches;
        }
        require(fixture_matches == 1 && binding_matches == 1 && fixture.get("diagnostic").is_obj(),
                "Role batch original path/case binding missing or ambiguous");
        require(projection_canonical(fixture) ==
                    projection_canonical(source_binding.get("original_manifest_record")),
                "Role batch original manifest object mismatch");
        const auto raw = raw_read(path);
        const auto diagnostic = fixture.get("diagnostic");
        if (n < kRoleBatchPriorCases) {
            require(source_binding.get("raw_path").is_str() &&
                        source_binding.get_string("raw_path") ==
                            "tests/fixtures/mermaid_official/cases/" + path &&
                        source_binding.get("raw_text").is_str() &&
                        source_binding.get_string("raw_text") == raw &&
                        source_binding.get_string("raw_git_blob") == fixture.get_string("git_blob"),
                    "Role batch raw binding mismatch");
        } else
            require(raw.size() <= 65536 && source_binding.get("raw").is_str() &&
                        source_binding.get_string("raw") == raw &&
                        source_binding.get("original_manifest_record").get_string("git_blob") ==
                            fixture.get_string("git_blob"),
                    "Role batch raw binding mismatch");
        require(entry.get("raw_sha256").is_str() && entry.get_string("raw_sha256") == digest(raw) &&
                    entry.get_string("raw_sha256") == fixture.get_string("sha256") &&
                    entry.get_string("raw_sha256") == source_binding.get_string("raw_sha256"),
                "Role batch raw binding mismatch");
        projection_int(source_binding.get("raw_bytes"), static_cast<std::int64_t>(raw.size()),
                       "Role batch raw bytes");
        require(entry.get("body_offset_utf8_bytes").is_int() &&
                    entry.get_int("body_offset_utf8_bytes") >= 0 &&
                    entry.get_int("body_offset_utf8_bytes") == diagnostic.get_int("offset") &&
                    source_binding.get("body_offset_utf8_bytes").is_int() &&
                    source_binding.get_int("body_offset_utf8_bytes") ==
                        entry.get_int("body_offset_utf8_bytes"),
                "Role batch body offset/type mismatch");
        const auto offset = static_cast<std::size_t>(entry.get_int("body_offset_utf8_bytes"));
        require(offset <= raw.size(), "Role batch body offset outside raw source");
        const auto body = std::string_view(raw).substr(offset);
        if (n < kRoleBatchPriorCases)
            require(source_binding.get("raw_metadata_prefix").is_str() &&
                        source_binding.get_string("raw_metadata_prefix") ==
                            std::string_view(raw).substr(0, offset),
                    "Role batch exact body binding mismatch");
        require(body == fact.get_string("body") && entry.get("body_sha256").is_str() &&
                    entry.get_string("body_sha256") == digest(body) &&
                    entry.get_string("body_sha256") == diagnostic.get_string("sha256") &&
                    entry.get_string("body_sha256") == fact.get_string("body_sha256") &&
                    entry.get_string("body_sha256") == source_binding.get_string("body_sha256"),
                "Role batch exact body binding mismatch");
        require(entry.get("baseline_expectation_sha256").is_str() &&
                    entry.get_string("baseline_expectation_sha256") ==
                        digest(projection_canonical(diagnostic)),
                "Role batch baseline expectation mismatch");
        require(entry.get("full_model_sha256").is_str() &&
                    entry.get_string("full_model_sha256") ==
                        fact.get_string("model_canonical_sha256"),
                "Role batch full Model binding mismatch");
        const auto effective = entry.get("effective");
        projection_fields(effective, {"expected", "reason", "checks", "snapshot"});
        require(effective.get("expected").is_str() && effective.get_string("expected") == "drawn",
                "Role batch effective outcome mismatch");
        require(entry.get("reason").is_str() &&
                    entry.get_string("reason") == role_batch_case_scope(n) &&
                    effective.get("reason").is_str() &&
                    effective.get_string("reason") == role_batch_case_scope(n),
                "Role batch foreign pending scope mismatch");
        require(projection_canonical(effective.get("checks")) ==
                    role_batch_summary(role_batch_model(fact, n),
                                       n < kRoleBatchPriorCases ? RoleBatchNumbers::HiddenOnly
                                                                : RoleBatchNumbers::DisplayText),
                "Role batch Model/check summary mismatch");
        require(effective.get("snapshot").is_str() &&
                    safe_relative(effective.get_string("snapshot")) &&
                    fs::path(effective.get_string("snapshot")).filename() ==
                        fs::path(effective.get_string("snapshot")) &&
                    effective.get_string("snapshot") == role_batch_snapshot_name(fact, n),
                "Role batch unsafe/unknown snapshot name");
        const auto snapshot = snapshot_read(effective.get_string("snapshot"));
        require(!snapshot.empty() && entry.get("snapshot_sha256").is_str() &&
                    hex_string(entry.get_string("snapshot_sha256"), 64) &&
                    entry.get_string("snapshot_sha256") == digest(snapshot),
                "Role batch snapshot SHA mismatch");
        projection_int(source_binding.get("body_bytes"), static_cast<std::int64_t>(body.size()),
                       "Role batch bound body bytes");
        validate_expectation(effective, body);
        if (n >= kRoleBatchPriorCases)
            require(projection_canonical(fact) ==
                        projection_canonical(
                            config_authority.get("cases").at(n - kRoleBatchPriorCases)),
                    "Role batch complete config source authority mismatch");
        ++actual[n];
    });
    for (std::size_t n = 0; n < kRoleBatchCases; ++n)
        require(actual[n] == expected[n], "Role batch case occurrence mismatch/unreferenced case");
}

struct RoleBatchDocuments {
    json::JsonDoc policy, facts, config_authority;
};

json::JsonDoc load_role_config_authority() {
    projection_regular_file(kNativeRoot, kNativeRoot / kRoleBatchConfigFactsFile);
    const auto bytes = read_file(kNativeRoot / kRoleBatchConfigFactsFile);
    require(digest(bytes) == kRoleBatchConfigFactsSha,
            "Role batch actual config authority byte pin mismatch");
    auto doc = json::parse(bytes);
    require(doc.has_value(), "Cannot parse Role batch actual config source authority");
    return std::move(*doc);
}

RoleBatchDocuments load_role_batch(JsonVal manifest) {
    role_batch_kind(kRoleBatchBinding);
    for (const auto name : {kRoleBatchBinding.policy_file, kRoleBatchBinding.fact_file})
        projection_regular_file(kNativeRoot, kNativeRoot / name);
    const auto policy_bytes = read_file(kNativeRoot / kRoleBatchBinding.policy_file),
               fact_bytes = read_file(kNativeRoot / kRoleBatchBinding.fact_file);
    require(hex_string(kRoleBatchBinding.policy_sha, 64),
            "Role batch pending policy pin: draft must not execute");
    require(digest(policy_bytes) == kRoleBatchBinding.policy_sha &&
                digest(fact_bytes) == kRoleBatchBinding.fact_sha,
            "Role batch aggregate byte pins mismatch");
    auto policy = json::parse(policy_bytes), facts = json::parse(fact_bytes);
    require(policy && facts, "Cannot parse Role batch policy/facts");
    auto config = load_role_config_authority();
    validate_role_batch(
        policy->root(), facts->root(), manifest,
        [](std::string_view path) { return read_file(kRoot / "cases" / path); },
        [](std::string_view path) {
            projection_regular_file(kNativeRoot, kNativeRoot / path);
            return read_file(kNativeRoot / path);
        },
        config.root());
    return {std::move(*policy), std::move(*facts), std::move(config)};
}

std::optional<Selected>
projection_select_complete(std::span<const json::JsonDoc> policies, JsonVal batch_policy,
                           std::string_view path, std::string_view track,
                           const RoleBatchBinding& binding = kRoleBatchBinding) {
    role_batch_kind(binding);
    const auto old = projection_select(policies, kProjectionBindings, path, track);
    std::optional<Selected> selected;
    if (old) selected = Selected{*old, 0};
    const auto entries = batch_policy.get("promotions");
    require(entries.is_arr() && entries.size() == kRoleBatchPromotions,
            "Role batch selector inventory mismatch");
    for (std::size_t n = 0; n < entries.size(); ++n) {
        const auto entry = entries.at(n);
        require(entry.get("track").is_str() && entry.get_string("track") == "body_diagnostic",
                "Role batch selector track mismatch");
        const auto source = entry.get_string("path");
        require(safe_relative(source) && source.starts_with("sequence/"),
                "Role batch selector unsafe path");
        require(!projection_select(policies, kProjectionBindings, source, "body_diagnostic"),
                "Role batch duplicate identity across descriptors");
        if (source == path && track == "body_diagnostic") {
            require(!selected, "Role batch duplicate selected promotion");
            selected = Selected{kRoleBatchIndex, n};
        }
    }
    return selected;
}

struct RoleBatchConsumption {
    std::array<std::size_t, kRoleBatchPromotions> promotions{};
    std::array<std::size_t, kRoleBatchCases> cases{};
};

void role_batch_consume(RoleBatchConsumption& counts, JsonVal policy, std::size_t promotion) {
    require(promotion < counts.promotions.size(),
            "Role batch consumed promotion outside inventory");
    const auto entry = policy.get("promotions").at(promotion);
    require(entry.get("case_index").is_int() && entry.get_int("case_index") >= 0 &&
                entry.get_int("case_index") < static_cast<std::int64_t>(kRoleBatchCases),
            "Role batch consumed case outside inventory");
    require(++counts.promotions[promotion] == 1, "Role batch duplicate consumed promotion");
    ++counts.cases[static_cast<std::size_t>(entry.get_int("case_index"))];
}

void role_batch_consumed(const RoleBatchConsumption& counts, JsonVal facts) {
    for (const auto count : counts.promotions)
        require(count == 1, "Role batch missing/unconsumed promotion");
    for (std::size_t n = 0; n < kRoleBatchCases; ++n)
        require(counts.cases[n] == role_batch_occurrences(facts.get("cases").at(n), n),
                "Role batch consumed per-case occurrence mismatch");
}

// Fixed fifth descriptor: whole original sources only. Full geometry remains
// owned by Native262; this validator binds its independently reviewed authority.
constexpr std::size_t kRaw193Index = kRoleBatchIndex + 1, kRaw193Count = 193;
constexpr std::string_view kRaw193Scope =
    "Reviewed complete original raw native terminal projection; original "
    "API/SVG/CSS/font/HTML/browser/ASCII predicates remain pending.";
constexpr std::string_view kRaw193Protocol = "explicit_original_source_row_and_active_model/v1";
constexpr std::string_view kRaw193Defaults = "Only absent fields documented by the pinned active "
                                             "authority; no arbitrary missing-field tolerance.";
enum class Raw193Kind { WholeOriginal };

struct Raw193Binding {
    std::string_view policy_file, policy_sha, fact_file, fact_sha;
    std::uintmax_t policy_bytes, fact_bytes;
    Raw193Kind kind = Raw193Kind::WholeOriginal;
};

constexpr Raw193Binding kRaw193Binding{
    "sequence_official_raw193_projection_supplement.json",
    "a29ba76e0833e65337c85ad231ffa694dad5f2c37b3d46fc6dc97d9c2f699a52",
    "sequence_official_raw193_facts.json",
    "c6f43f07f32cd01e092bf2974d10583d5b120183206192fb01661c2896fe2c8b",
    252371,
    1409931};

struct Raw193Authority {
    std::string_view file, sha;
    std::uintmax_t bytes;
};

constexpr std::array<Raw193Authority, 4> kRaw193Authorities{
    {{"sequence_official_description77_par_over_admitted_facts.json",
      "213845caeebc2f438dce4f33b61981ce37bfbc667317b580bb9ba323431844e8", 1211414},
     {"sequence_official_config110_facts.json",
      "25f2e7c71b9dde1cee5421cdcc4c630d16b6cc51b0d34db23808a26307518f77", 1161434},
     {"sequence_official_config116_legacy6_facts.json",
      "9ca671329fdc3abc3c43177cf12315417c3277936f76cf53731fa8f0955308a7", 120671},
     {"sequence_official_config110_lifetime_supplement.json",
      "ca362583cb29c7e65dc3f8d316edd3c27d3d4f3de8bb9bccf4bb035081e4877c", 15064}}};
constexpr std::array<std::string_view, 193> kRaw193ReviewedEvidence{{
    "deeba6506e108bc3fb2d46cd915015a80a4950a743a1e540e67c30e3751e0468",
    "7ffbbed3169c2ec414ac6b542d53a984bcd8831a5754fa4f8e96267bd9087f71",
    "52e736ab3de553a631f1f2867d49cfe84482c5688348e1e04a87c66f292bcbc9",
    "eca7907b6ab16b7f447d5eb6fc55d269e55c41f7a51615a5aa2323dedc46b244",
    "1d61195e53a8c11b46953645072b8eb6fd4610fed051afade4bbd6803e934a51",
    "6b5510c1b06aab9a3522a0d30363a58d55c1126b4e226fff11460ed09d2b0536",
    "6f8b2c07b6436f17e452bba035a127c8e08e38ef664c0547fa0dedd5a748b087",
    "dcd0d1cd3fabb43789d9eee984ec9c75140f0d8ad2fe337a9c00302cf96f5e36",
    "46375ed3b8c464a0e0a15d9366b84f38e7170e011604308a357d86501719bd0a",
    "1090d8f5850fed04b2e67eac1741e5d0a53d638da8a2b312a786b1c6dd6ff192",
    "2a7f67889b3533f87257b6cea8112ba064fa9dd5428d8da232d63004f141783d",
    "555313aaeeff0a2aba21ea36e5a3b8175654d588f1c936532143b9ae11291c08",
    "a02e55bda63f45b389b7c1334d70d292bf4205b1550df777d0dabd37b87e09ac",
    "2461b6b296bce9d4929188f2aebd67871ffd75acd18caa7b162ab8e79f1ca658",
    "5706b1ec1596d33bfade1955369091ebf2a3329f1e1c513e062e3161f0ef7ca5",
    "0e114dd07daba902bba825e39db9b9d90a6a113ac5e2b95359acbc88d22ef658",
    "49d89e32211996de0911c8982be0c2283f4c6416ce03c25c0b48167e594fab35",
    "9675b40051590e883f6f0b401c320babf315581589045f3e5eb133d4312f8cea",
    "8c0ab3aa643743c17b4ad9c129c4ca753afef58187b49750c269a9e33ca4e5c7",
    "86c591b4fe911bc689df3025c82cbcaa8eb720de14485b7ac1fd109028bfcd34",
    "fd94ff24009f372ea64830e507db69eb6547bb17f5cbb9368a75940e72102c5c",
    "5e27f69f71a7de9c240d05cb5c7ceb4af00b687f743dee12230a60b3afc7788e",
    "0402892539de277f5dfd271da5eecad3208108e52bdf957855476c9c10acb38b",
    "3d21ce0fa45f6232323861351d1ec22bbcfa89c385848bcb0567180a9d2a2262",
    "e058d29131cf8360a22c0854123df3f96a7f0ddc3968892004d65e799f044d00",
    "f350d8b21a6c81557e9b831a935b905b4c9a1fcccf4b3da033e1af04d34da7a6",
    "b4141c5820d38c8969dd6e5fb9c94f2f8c7d67761c7c4c734231d6b58db8ec18",
    "4f20c38276bb445ea99f698b989f8b0b33ebcce5b79b0b5d52ed2cceefa268cc",
    "8d1f8583d02bb651e4fb4f93747f8e0f300c50e609ae9667bfb17a9b892ddb88",
    "9da340c807bea780e877a8152e04bfb242ee640d53c558798bfa8005f0877939",
    "fe852d8a2894f9e43f998c12cd055eb2f75029e6f26274c3f97ec6e8e0023138",
    "29627bc8eb832951f76a7cd050b80fed6a0d7ab48d1fa559fa424b192a8a7eb1",
    "ac3d36c0030e59a32b702020eecf10ec4e946dbda1d3bf8abf648805dc2155aa",
    "7fae4c9676afbdac468e0f013467af4eaa8b4949db2b90b95f279fab6e7bba9b",
    "ffb1db6ca4e8bef4991ce0ce75ceaf1002a29dd6b191084650fd86580a0bd9ee",
    "cf92749e71bbcb785ad5a26ebfefb17d34e1d26d69c51cd2f2c07a6aaa0ffe7f",
    "02e1884d973f831cac16053a753631fb0043e7e3fef5e3c32447ed98beee4d2b",
    "ad96cc69144b8076a74ca952b826aa32c8c0d36c4033bd62f1b1298b689fbb9a",
    "f839eb2056eeadcff3fb4edfd92d7f134285fd0c3ac04c8b788c53ffed7da5a8",
    "1714ae98958f0ac7f0a70fdc166d3218025bbc2936884f8eef18726557df81ea",
    "3e5a83fb792b031010302494989a0b33a5616751d423aa1528e04ed082004499",
    "00b6d9cd00e7d50a044941f3560eca71a48ec8b1371b51a97921d5b0407ef8e6",
    "d5e46e60198a5762d55bc217c23aebfaeed4ef4fb94a0c74364ebc7806fcfe06",
    "65338a542bdf0d09a9bf6aafc8b81169ac60ab082149bcc0a2adc0ff4f765d5d",
    "b00567da8caf0f9fb845945623961e15438f01380dc50e5c68bfbca317768c10",
    "48707a4effe10313f5cfbf77f0fa36c02c47395a2c4f72786d2e2d3a058cc16e",
    "4a336cdb40f85654eb576d84ecc76d4d914741b0bcc95690c604333c4973b31c",
    "f7900c751d910fa289c89b5849856b20d448ca233b108db91889f487894927bf",
    "1926d751c85c7e1aca58fd53afdb1e22b7f7a05076f826c68637a636c7e6950c",
    "06801183a93114b1fa8cb2dc68ed5a3f1ff16fca3b66f7a8f5eb0cd9f6ff2337",
    "cc2f2f027308ae2760b8564202043f95bfc7db79111b40aca6e1358ced3824d8",
    "7d93cb29a1dc1550a9910f4464691d42b00fbe81a59aa71ed470a6e04b533a49",
    "8b8dbd78a72107386dafaa2938e7fd0595aea8ba8014135d53fb61ccaa0e3b25",
    "f7acee2597a099cf29105d8ac38cbe36af9ef5148dfde2dc64725cd1acbfddbe",
    "0d4efa1d530831ee217bf170e20405141c22d3dcc8473d037514728b17886ad4",
    "50d4f472d63b0c7353ca76f6d778a0e3b176732af5106fa09512399ad948500a",
    "22de3884383064732aa74336452cd43b52c79be72afd6cbf10ebb4e3296889ac",
    "855ebd0342adaf793bce971270ce525716b1005f2b4d08fc0e9a182d9549e159",
    "5b45e17ad7507df0ff289924a4f81b71ad3ef64719e572e4734521c8636ead50",
    "7f0c39472f81f7fbabd993c644d84f84767e6f6184cb02b6daf14ca4c152ed60",
    "8c1dd87e5b82f598d3a8375c2a772ed9746c11c6292b4e73ad6d1f1801425313",
    "76c9928708384888ead7a868a64ab9b2ae2879f22ceabf305bbe5bc892ef6b9a",
    "8ec650a877655ee19d2f56e73bb26a5d647a00bbc976f4c8cd9a7ecd4bc6ddc2",
    "63de5d4239f5cd25797a9d22ed467bc522f7167e41b93b07d5e333a2b3a203bb",
    "1fd83fb4712013d2856bfea99678697048e84bc4b114d3e9086c3f6be6a0c704",
    "cb1b6dea619afa269205dc37529d1d9689463a36504c7e2c85d9e975c53dfd2c",
    "88bb1482b84b18cafd5165c99db77ad314dc04b025961a03943b4e98eff5555a",
    "a98879b71fb5418bdc62e125002ab4e905f804e70945de8f98d42cf0c1ebb6cb",
    "13776f73a744c438e74511f5927012b37702620d583f7c04a72e5d57341ebb65",
    "cea7fd32e32a89f8a3b90218d232d062f0160c6f689fe06d3cbd7f421e989b82",
    "25c503d9d479ca810da558e447143db3a7b3687a2f5794f0051a8d76aff53f0a",
    "fdd614f3631515b8c30a2ed6b3bacc4c936dbf2222dd3c2ef9ef14ef7e4a82a0",
    "4abb10587c4caebce269e42905eb72a68f2d2fc9f21d029ffbef94d1d18ae1f6",
    "6eb7287e580a9c647b49607f8db382dd43abc61bfea6d94f82565811b52a88d0",
    "2860dc5085a0128cb6d0412a2cf772346f50c45b2166bed71d37463d86ea3ff3",
    "e0650e83acde1890d83176a05bfd819af215b35f38551e3fa055d878b93583d8",
    "fbf7fabe67ce30a97f88eb8ab22ffce360a3dc02d74af584daad104ac5294831",
    "5ed56d1b34487964331a13bba8dfd49c8cf5af516aa24561f62b706535eee18c",
    "22e097bd5cae61c38b8f7b3e19381d38bdb208a233506317d936017d64a3943e",
    "4763a2e319af4e380deea6991017fc08d1ebf6ccee808110d3b3530da561ed1a",
    "9342476ef07b1179e28fb0537a72b10a6fa05b3f532dfe69b9345ffad598bf59",
    "a506f4f1cd0aa92d492541e4f66792d3d372197d00bbab4efe18a05bb322e482",
    "1d6257b1389449b704a4e1add431833012ccff8428d37203b85533143eddd211",
    "3287a7e29045302434705cbc6b866149b49089f6a315ad472d5e8a3a23ee2f28",
    "876fa9e597894034bb1e174de3ff129a4118a2d98a71ae41ceda5eb5b09eba9a",
    "1dcff5cca852d92a1e10133a1d1fe6a6ba42adae699d42ad779f4f465bf2cd67",
    "a2fda9aa77cc02be7af1a004869d8b780e130729d24e55a8bddea3f03ef0ddbd",
    "7d2e538fab07e649d97ce0c21f680a590d0641eae2beee3942f985387a04f9e9",
    "e8d2240be96d099c3939f77000c619d976f1268a5c230855bca1ccbc5723960e",
    "2404a4f09ffe8132a49fb050ee1b5955ccf7231e0566e2a02cb91f3245b50767",
    "ff6fe6b454cd8bd3ce9a40b7256c3cde42a2d12acaf2ffdcbe4163f4ec516bee",
    "620b70ea699aa55632b62e583a083a153131cb03a8bb6a2a32f3e2a0c195a19b",
    "f0774b7fa802bef48b5b49097685444085a1cb4784a7df0e11036c9ac263507d",
    "b8fa9c6c86e68bcb3aa5c24ad0611bb347c0f5089b64f09f58dcb420557df71d",
    "2a964827007ade4f4eeafd3ef0390519a3e2609142d4410798884ef6beb4d5d9",
    "dc74db8cdd75be92a56a18936fa80865e02d48d97691392a56040fe8ae4f7add",
    "99b4fbe3b7b1bd82bb039ce39e03815a4303c9f2c78fc79c6d5bd7b15ea5008b",
    "d176a0865c0f85a689d5a304bfca37af15dd76b127bfd41acdb83dbd0b18ddbe",
    "4dc0b580b6eea4059b863cfd7104d23163b3acdf512300aec3900e6d0e3f94b6",
    "a865260b03f640488d890d6731da94e272d1069528b1e3ab83240cc2283794c4",
    "0de106c8a7f485c2c037c9f10026e9b36985ce9a6a7cb93163df2c871af07cfb",
    "b543b9ee6e0d09cdfcd36135feaf03a937d0462c7f2a2cc1a55c9f1fb5a9209c",
    "4bf0805ff114be569f289f1565fe7a3726d46c511d0d0d3c5c2878034f16c6a0",
    "e065efa2e194773b18c5643b916f3e1d39657962b579c3b387723572314e0fb8",
    "651553a5088768b0bd742bc9c31cb6f09fe1b70ee1c35651ae5607cf8bc2245e",
    "979976bab149387a60ce74c3a1a04d313c191100b773f5cfee26b6a3c6bdd7a0",
    "547bf0a561385a3fb135caa0310c22d0defd33c531eafebe38a0d86e322f610d",
    "7089826b8ef5b4f9950424e2ffa6e3bd2d4c7a0968c29f67fddf80e9c0f295e6",
    "89b31610eeaabbee9982aaf6c4560ef1ab6a7f57b24afe4c2062cbd8d0f8b3da",
    "2091cec461d752cfeabd14c5b2548eee0d585a3b324d8827798d7b561bfd0d19",
    "87ebf30ea2e4eee81a32b04ef749700f94da1d601bf0725bc7b7aa9acb4e9af2",
    "23e96dd7210a499f4e94886f838e2ca170f2cd81ae90749a2afdc2e67d7dbcba",
    "cc80088c01a74b9958d3b28de1b4b63b81f324b1711b7df2332ce3e859457846",
    "1e9e6652f38b51bfcb4588f6d831c911d676e9666179babd94c19a0ffa132a2f",
    "b78cc6c7955a0fc1ed74b463e4fa5a043945461b9243c94ae19a768e925b770e",
    "dde0d90c84293da4235317a16f035d1c513b7ff0c47ba76a18f8786e6469468b",
    "5b950a2fd6fc57293e92a1df449af373c5918b9b5d99c661b1aedbec72196b80",
    "45b2a3d35306f09875fcaf26e220f44f6ef9233c03f95ff061f775d9ca5173dd",
    "071f4fb8fba4046db8c035399cae6a83a0486cc6b967861c540dc7c985d9e554",
    "8adfdb8935bd8485f8f0de5a2f41eca30b5f7301b947db1b1660738b6229a656",
    "75a3dc2b71b37add9950aa8eeef3043cc5ba8c880833778f86ef66253668eb38",
    "dd66c4bb5405346050c37d26579592f2c7bfc8a9a2c9e45a660918b73f8ece03",
    "531431fc6b42934d09a075214e70fc82f2d2a05cc3a805fc0ed380bc39a7eb5d",
    "68c667f86f520c12fd5a2c4dc30f93aa152d0631e2af3286f65ccbafa64737c8",
    "ccea79c8b8b6f43159599dffe42953ab99316cea36004f4d91171600c07e3f44",
    "2ded4a28601f765b31f45e6c1af124712568d45ffa99c11c5c341ce7783d358a",
    "acab470fa176d75a501b832ee237e75386e477ec8a862a8a90aea7dd90db4dcf",
    "c3dfdba3d8927d962b07eaa52c40e3b764e4d67900238ac5a6111a4d7043c8e0",
    "6e63ba6701443ec51b08f0322940e6dcce23869b910b19ddd882c070c2a38d8e",
    "3f62ab49e01dce8c32b0ad821f480299e88d1125e19ba53211daa53f07ecce27",
    "38225022cee062727d6de0fb00f109b0378955906bc0c91df81840ad6c5f22f3",
    "1741b6e8dfbe6177af3e749730bf28264ea5ac93c4a2b14b77a37a75f993e4ca",
    "8efb6d9ff8f8a812e98e200edfec3d4a0f3ece02ac1e25084cd78da3d161a1e6",
    "034f0cadb4b2ba5c5f51f2a6b66f7dfa3a1714759f3d9d0b6b9edeadb9f9512a",
    "e8294d185d62057c2ec037be9b7b8b94af5a57b0fdc1df949998bd6e3c6bf6b4",
    "c0041aef1550d774df6373247efeb9fa8df0c0ae88011ed52b7f29f6c98f079c",
    "7b64fca1be571526d4e1680201088d1d26468682f9345c514193c006a84ceb1f",
    "32b396bdc9669b4b11f64b5f84868668cec42f512c310d6b7fc509508bb5956d",
    "a6abd5ecbd6d83965b8e3e90765a4b4828cf5a86d5ff8409feda955b3c70a698",
    "f66b8f40b5867cb94cab5767699b15b7bdf86b5382dd8c3141c00143f24e9a88",
    "de1f853fa230175489247742e1c1adaf66ad99f6a5caa5ea8a1a93087e5dc40f",
    "7d443718ddfb7fdb11829d55343571040d23aa7a3bb0c4ff1af614e50911cb5b",
    "f45537f83de64374108a56378c9886b03cbed0a87da1e245daf14213809906c5",
    "0cf610ca643ac036e10e70d2b010edb608bb04f113d7bc052ff185160c43cba9",
    "4322e08fc1a0ffea3a6fcc1d727c3ef4266afd46a4168b22f3f8e79902b7f1dc",
    "ae0094a5ed4a2f78efd10f8fb5868419394d4f47e8d3540104939d8b5bf83bad",
    "fda984fc88933101341d21ad9a6b27a3da4ac64063e6f2be4fe266a6a63987f7",
    "6c14238b3c38cabb26f7a6dcc82c20623b18106fb1899bfcf8b1b36abb25b9e9",
    "0772801852abf194a493a61e53c24b5bb238c79ecbbaeffda9ea19f963afbd92",
    "daee6bae61ab0e44ef47aa763d43709fb6252187f6dbdd185af666a6a0356fee",
    "15ab33a0c8a99382cccf4508d1be3f5be450e0f536bae973a1bc477d623b1ccf",
    "883653fc9a1c4b86b98d84e5715e012f7e136b5bab4dca7110a4537740ffa796",
    "114e27feba319a17d21f5a25704180161e5b490537e95e93a6f3a1350e2f7686",
    "cc1db1f9fe56d9ba80c55a886478d4b3b786b36ac893a97a8913cd2ada3d4f58",
    "6d890bb46f493aeecedb72e6f23a8c55c7dc7cd29bcec14dfcea47a32c9e1e17",
    "5430b061e136d84259853249f8d68699a0429004b19a7cabb0b46dc9ee9941a0",
    "e5170f370ae9363569a50b99b6f7d210a5012b449d209096bf18a0e4b40f7f40",
    "aa7a64efe425d2ac948a3d0528e8366c45835fba9d871e65940e42f75bb0fe82",
    "5e964eb3e33b3276a951a4ad429b8bc82786393c74c1ceb62d6951f35e739bda",
    "61744d0c88ff102d45c9203172deea6fc3040781bdf6ca44e9581ba5e4adaac9",
    "fc3c142d04794ee48ed7f6318d1bed642775e6482cf20c57aa56138fab2f0db7",
    "85b9fbdde03f889e1233615310cf29f0d7bbc535b1683b915b2cc6d65090b9dc",
    "d5cb5da16a5983a21eec75b72d8d1c96f33c3b1cf3df18b1d59febde2f8e8f1a",
    "5d5fe67b913ce858b19f3fe4ff06ccc3cbd5598278e36e911846dc42b1747905",
    "dcb8d8f0b7d2807bb5ee8f5194756d689abb6bdb8e65ca06d89bc4dec7c01a45",
    "a95b37cd9ac202fed8f3a3fb1d53cd6b58162f325940dfef6d2ed08b4b9340cb",
    "bbec5f1760f6439296ad195f5a4f4fcc3efb30f566e72f20ead471f865515f65",
    "e2bd845a80b98ad98e8e962996398a9196bef2613ec2124f496a4d894c58115e",
    "e54d9533f8d0437aec7bc982877607500647e52b25284b1ab8f3b1406d6c4651",
    "3238fb70c5f1d2243f40b431625cfb42f5120c97fb4693eb3b6042d09f12f28c",
    "c0781b3481b5454368a4e06ab1f60a7855471ae87d737321e5e2813392912e0c",
    "19aa7d989c9f638d23c2ad4aae946490aa015cde3f46fabc6712475e990ae05d",
    "fb4b1927713c9af3bda24991d89baba0cd40f521336b5a587866a1571e010f13",
    "74f426a9698a2c1592b330f7b77f96007f050f982c561fd7706b9b5647b538d3",
    "7bede0d02b31bf5a05c6f8bfafe8bc00458fe2dbbfaf754112a60037693db88c",
    "d69347ba79a821a6846fb22db72899cffd06bdcdc4fddf04ceac1afc702450d0",
    "e7685476702cbcf393dea3f619060d3238d354a1d74b7ec95ad46232169667a6",
    "9860a491f7cc9505ee1ef1504a83e0a06b8595168003b788ecf3426d96ab4568",
    "0c5ba9eace95f8cdb4aaf4e97b99b96e8e19a30a727ba3dc5bcee026279d0057",
    "1d42f9f5d99a2824fc0154aa335ebcc445568629bbecc9952787a5e9f806445d",
    "8ceb2eafeb88cd12217a649046ca7f1ee0a559163bea5d2d8b2bc88d3013ce2e",
    "bab2f455e0f5e680a9857ed944d6831d86f9f4669931964dcbcb8d167fc5ab38",
    "fb5125a9034af2ddffa8506b0a53b59601ed9c0fd3b5045ab0dbcafb6366226a",
    "922cc56bffefca56ad556de18d852b35c07737b1a264f5f1452cf2386244181d",
    "3cab4187f1526b5c52f8a9bc41ad52b1eefbb2f7593b146528c46793e16c2165",
    "c4d7c9da3a5b8594ad18820d57c91953c6e31a8bf3a7ce06f7293b111c91f5a4",
    "33a8be49d0aec05c46fdf976db46d2c557badb297a2bd5ece9dd7921087f62fe",
    "0386c745d3e7fff3a518dfaac98e6e3b30975810bfe727b95ec697ae09173f24",
    "f288eb8244f19bb41c447212860f2bac33cc1b8c3136c303dfa49841f14a4b50",
    "ceba5e37e55a3db49d3c8027503f50a70790317b85bb6fcfb2d04bd0326c1585",
    "b645986a9bb0de37b40546d7e37b27d4e52546bdc7bef5968b131ec2edd629d2",
    "41e47d34f3a7a8a2fcff0d8dff4730ca7a4b4f8d66c643dc74620561ed92fb6d",
    "9ef61989e27eb4653a648c2e69312700d768eebe9b74f537ec60bd97bb09e43f",
}};

std::string raw193_asset(std::string_view name, std::uintmax_t bytes, std::string_view sha) {
    require(safe_relative(name) && fs::path(name).filename() == fs::path(name),
            "Raw193 unsafe flat asset path");
    const auto path = kNativeRoot / name;
    projection_regular_file(kNativeRoot, path);
    require(fs::file_size(path) == bytes && bytes <= 2000000,
            "Raw193 immutable asset byte bound mismatch");
    // Fixed stat/SHA bounds follow the existing official asset-loader pattern.
    // Native262 owns scene resource/geometry checks; this harness adds no ledger.
    auto contents = read_file(path);
    require(contents.size() == bytes && digest(contents) == sha,
            "Raw193 immutable asset SHA mismatch");
    return contents;
}

std::vector<std::string> raw193_keys(JsonVal object) {
    require(object.is_obj(), "Raw193 expected object");
    std::vector<std::string> keys;
    object.iter_obj([&](JsonVal key, JsonVal) { keys.emplace_back(key.as_str()); });
    std::ranges::sort(keys);
    require(std::adjacent_find(keys.begin(), keys.end()) == keys.end(),
            "Raw193 duplicate object field");
    return keys;
}

std::string raw193_schema(JsonVal model, std::string_view profile, std::string_view numbers) {
    const auto fields = raw193_keys(model);
    const auto actors = model.get("actors");
    require(actors.is_arr() && actors.size() > 0 && actors.size() <= 32,
            "Raw193 actor inventory/type mismatch");
    json::JsonMutDoc doc;
    auto root = doc.object(), present = root.make_arr(), actor_fields = root.make_arr();
    for (const auto& field : fields)
        present.append(root.make_str(field));
    for (const auto& field : raw193_keys(actors.at(0)))
        actor_fields.append(root.make_str(field));
    root.add("present_fields", present);
    root.add("actor_fields", actor_fields);
    root.set("label_encoding", profile == "source_wrap" ? "LabelTextWrap" : "String");
    root.set("number_encoding", numbers);
    root.set("profile", profile);
    root.set("defaults", kRaw193Defaults);
    doc.set_root(root);
    auto parsed = json::parse(doc.to_string());
    require(parsed.has_value(), "Raw193 schema serialization");
    return projection_canonical(parsed->root());
}

std::string raw193_model_summary(JsonVal model, std::string_view profile,
                                 std::string_view numbers) {
    require(profile == "fixed" || profile == "source_wrap", "Raw193 unknown source profile");
    require(numbers == "DisplayText" || numbers == "LegacyInteger",
            "Raw193 unknown number encoding");
    const std::set<std::string> allowed{"title",     "actors", "messages", "notes",
                                        "fragments", "boxes",  "order"};
    for (const auto& key : raw193_keys(model))
        require(allowed.contains(key), "Raw193 unknown Model field");
    for (const auto key : {"actors", "messages", "notes"})
        require(model.get(key).is_arr(), "Raw193 required Model collection/type mismatch");
    require(model.get("actors").size() > 0 && model.get("actors").size() <= 32 &&
                model.get("messages").size() <= 128 && model.get("notes").size() <= 64,
            "Raw193 summary inventory bound");
    for (const auto key : {"fragments", "boxes", "order"})
        if (model.has(key))
            require(model.get(key).is_arr() &&
                        model.get(key).size() <= (std::string_view(key) == "order" ? 512u : 64u),
                    "Raw193 summary optional inventory bound");
    json::JsonMutDoc doc;
    auto normalized = doc.object(), stable = normalized.make_arr();
    std::set<std::string> stable_seen;
    normalized.set("title", model.has("title") ? model.get_string("title") : "");
    if (model.has("title")) require(model.get("title").is_str(), "Raw193 title/type mismatch");
    const auto label = [&](JsonVal value) {
        if (profile == "source_wrap") {
            projection_fields(value, {"text", "wrap"});
            require(value.get("text").is_str() && value.get("wrap").is_bool(),
                    "Raw193 SourceWrap Label/type mismatch");
            return value.get_string("text");
        }
        require(value.is_str(), "Raw193 fixed Label/type mismatch");
        return std::string(value.as_str());
    };
    auto actors = normalized.make_arr();
    const auto actor_shape = raw193_keys(model.get("actors").at(0));
    const bool person = actor_shape == std::vector<std::string>{"id", "label", "person"};
    require(person || actor_shape == std::vector<std::string>{"id", "label", "role"},
            "Raw193 actor schema mismatch");
    model.get("actors").iter([&](JsonVal actor) {
        require(raw193_keys(actor) == actor_shape, "Raw193 mixed actor schema");
        auto copy = normalized.make_obj();
        projection_string(actor.get("id"), "Raw193 actor identity");
        copy.set("id", actor.get_string("id"));
        const auto text = label(actor.get("label"));
        copy.set("label", text);
        if (person) {
            require(actor.get("person").is_bool(), "Raw193 Person boolean/type mismatch");
            copy.set("role", actor.get("person").as_bool() ? "Actor" : "Participant");
        } else {
            require(actor.get("role").is_str(), "Raw193 Role string/type mismatch");
            copy.set("role", actor.get_string("role"));
        }
        if (profile == "source_wrap" && !text.empty() && text.size() <= 12 &&
            std::ranges::all_of(text,
                                [](unsigned char c) {
                                    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                           (c >= '0' && c <= '9');
                                }) &&
            stable_seen.insert(text).second)
            stable.append(normalized.make_str(text));
        actors.append(copy);
    });
    normalized.add("actors", actors);
    auto messages = normalized.make_arr();
    model.get("messages").iter([&](JsonVal message) {
        projection_fields(message, {"from", "to", "label", "arrow", "dashed", "central_from",
                                    "central_to", "number"});
        auto copy = doc.copy_val(message);
        copy.set("label", label(message.get("label")));
        const auto number = message.get("number");
        if (numbers == "LegacyInteger") {
            require(number.is_int() && number.as_int() >= 0 &&
                        number.as_int() <= std::numeric_limits<int>::max(),
                    "Raw193 LegacyInteger number/type mismatch");
            copy.set("number",
                     number.as_int() == 0 ? std::string{} : std::to_string(number.as_int()));
        } else
            require(number.is_str(), "Raw193 DisplayText number/type mismatch");
        messages.append(copy);
    });
    normalized.add("messages", messages);
    auto notes = normalized.make_arr();
    model.get("notes").iter([&](JsonVal note) {
        projection_fields(note, {"actors", "text", "position", "after"});
        auto copy = doc.copy_val(note);
        copy.set("text", label(note.get("text")));
        notes.append(copy);
    });
    normalized.add("notes", notes);
    for (const auto field : {"fragments", "boxes", "order"}) {
        if (model.has(field)) {
            require(model.get(field).is_arr(), "Raw193 optional collection/type mismatch");
            normalized.add(field, doc.copy_val(model.get(field)));
        } else
            normalized.add(field, normalized.make_arr());
    }
    doc.set_root(normalized);
    auto parsed = json::parse(doc.to_string());
    require(parsed.has_value(), "Raw193 summary-only adaptation serialization");
    auto checks = json::parse(role_batch_summary(parsed->root(), RoleBatchNumbers::DisplayText));
    require(checks.has_value(), "Raw193 summary serialization");
    if (profile != "source_wrap") return projection_canonical(checks->root());
    require(stable.size() > 0, "Raw193 SourceWrap lacks source-stable short alias");
    // This membership list deliberately does not claim wrapped-row geometry.
    // Complete immutable Model/native proof + exact snapshot own that evidence.
    json::JsonMutDoc output;
    auto result = output.copy_val(checks->root());
    auto aliases = result.make_arr();
    stable.iter([&](json::JsonMutVal value, std::size_t) {
        aliases.append(result.make_str(value.as_str()));
    });
    result.add("labels", aliases);
    output.set_root(result);
    auto immutable = json::parse(output.to_string());
    require(immutable.has_value(), "Raw193 Wrap summary serialization");
    return projection_canonical(immutable->root());
}

std::string raw193_summary(JsonVal fact) {
    return raw193_model_summary(fact.get("model"), fact.get_string("profile"),
                                fact.get_string("number_encoding"));
}

struct Raw193Documents {
    json::JsonDoc policy, facts;
    std::vector<json::JsonDoc> authorities;
};

void validate_raw193(JsonVal policy, JsonVal facts, JsonVal manifest,
                     std::span<const json::JsonDoc> authorities, const RoleAssetReader& raw_read,
                     const RoleAssetReader& snapshot_read, std::string_view actual_fact_sha,
                     const Raw193Binding& binding = kRaw193Binding) {
    require(binding.kind == Raw193Kind::WholeOriginal, "Raw193 unknown descriptor kind");
    projection_fields(policy, {"schema", "kind", "baseline_manifest_sha256", "source_facts",
                               "scope", "promotions"});
    projection_int(policy.get("schema"), 1, "Raw193 policy schema");
    require(policy.get_string("kind") == "SequenceWholeRawOriginal193",
            "Raw193 policy kind mismatch");
    require(policy.get_string("baseline_manifest_sha256") == kManifestSha256,
            "Raw193 manifest reference mismatch");
    projection_fields(policy.get("source_facts"), {"path", "sha256", "protocol"});
    const auto reference = policy.get("source_facts");
    require(reference.get_string("path") == binding.fact_file &&
                reference.get_string("sha256") == actual_fact_sha &&
                reference.get_string("protocol") == kRaw193Protocol,
            "Raw193 source fact reference mismatch");
    require(policy.get_string("scope") == kRaw193Scope, "Raw193 scope mismatch");
    projection_fields(facts, {"schema", "qualification", "immutable_manifest_sha256",
                              "complete_source_count", "cases", "active_authority_protocol",
                              "authority_files"});
    require(facts.get_string("schema") == "official_sequence_whole_raw_facts/v1" &&
                facts.get_string("active_authority_protocol") == kRaw193Protocol,
            "Raw193 facts protocol mismatch");
    require(facts.get_string("immutable_manifest_sha256") == kManifestSha256,
            "Raw193 facts manifest mismatch");
    projection_int(facts.get("complete_source_count"), 193, "Raw193 facts count");
    require(authorities.size() == 4 && facts.get("authority_files").is_arr() &&
                facts.get("authority_files").size() == 4,
            "Raw193 independent authority inventory mismatch");
    for (std::size_t a = 0; a < 4; ++a) {
        const auto file = facts.get("authority_files").at(a);
        projection_fields(file, {"file", "bytes", "sha256"});
        require(file.get_string("file") == kRaw193Authorities[a].file &&
                    file.get_string("sha256") == kRaw193Authorities[a].sha,
                "Raw193 independent authority reference mismatch");
        projection_int(file.get("bytes"), static_cast<std::int64_t>(kRaw193Authorities[a].bytes),
                       "Raw193 authority byte count");
    }
    const auto cases = facts.get("cases"), promotions = policy.get("promotions");
    require(cases.is_arr() && promotions.is_arr() && cases.size() == 193 &&
                promotions.size() == 193,
            "Raw193 case/promotion inventory mismatch");
    std::set<std::string> paths, ids, raw_hashes, snapshots;
    std::array<std::size_t, 193> frequency{};
    std::size_t wraps = 0, legacy = 0, persons = 0, aligned = 0, historical_null = 0;
    for (std::size_t n = 0; n < 193; ++n) {
        const auto fact = cases.at(n);
        projection_fields(fact, {"id",
                                 "path",
                                 "track",
                                 "input",
                                 "input_sha256",
                                 "input_utf8_bytes",
                                 "body_offset_utf8_bytes",
                                 "body_sha256",
                                 "baseline_expectation_canonical_sha256",
                                 "profile",
                                 "number_encoding",
                                 "source_model_schema_fields",
                                 "model",
                                 "model_canonical_sha256",
                                 "whole_source_authority",
                                 "source_review_hash_references",
                                 "historical_original_model_null",
                                 "source_row_canonical_sha256",
                                 "native262_all_evidence",
                                 "source_rect_constraints",
                                 "frontmatter_node_tree",
                                 "candidate_snapshot",
                                 "scope",
                                 "source_row_binding",
                                 "active_model_schema",
                                 "original_manifest_record"});
        const auto path = fact.get_string("path"), id = fact.get_string("id"),
                   raw_sha = fact.get_string("input_sha256");
        require(safe_relative(path) && path.starts_with("sequence/"),
                "Raw193 unsafe/nonsequence fixture path");
        require(fact.get_string("track") == "original", "Raw193 fact unknown track");
        require(hex_string(raw_sha, 64) && id == "official-raw-sequence/" + raw_sha.substr(0, 16),
                "Raw193 source identity mismatch");
        require(paths.insert(path).second && ids.insert(id).second &&
                    raw_hashes.insert(raw_sha).second,
                "Raw193 duplicate source identity");
        JsonVal original;
        std::size_t found = 0;
        manifest.get("fixtures").iter([&](JsonVal fixture) {
            if (fixture.get_string("path") == path) {
                original = fixture;
                ++found;
            }
        });
        require(found == 1, "Raw193 actual manifest path binding missing");
        require(fact.get("input").is_str() && fact.get("input").as_str().size() <= 65536,
                "Raw193 declared whole source size cap");
        const auto source = raw_read(path);
        require(source.size() <= 65536, "Raw193 whole source size cap");
        require(fact.get("input").is_str() && fact.get_string("input") == source &&
                    digest(source) == raw_sha && original.get_string("sha256") == raw_sha,
                "Raw193 exact whole source binding mismatch");
        projection_int(fact.get("input_utf8_bytes"), static_cast<std::int64_t>(source.size()),
                       "Raw193 raw byte count");
        require(projection_canonical(fact.get("original_manifest_record")) ==
                    projection_canonical(original),
                "Raw193 complete manifest dictionary mismatch");
        const auto diagnostic = original.get("diagnostic");
        require(diagnostic.is_obj(), "Raw193 diagnostic provenance missing");
        const auto offset = diagnostic.get_int("offset");
        require(offset >= 0 && static_cast<std::size_t>(offset) <= source.size(),
                "Raw193 actual body offset outside source");
        projection_int(fact.get("body_offset_utf8_bytes"), offset, "Raw193 body offset");
        require(digest(std::string_view(source).substr(static_cast<std::size_t>(offset))) ==
                        fact.get_string("body_sha256") &&
                    diagnostic.get_string("sha256") == fact.get_string("body_sha256"),
                "Raw193 exact body provenance mismatch");
        require(digest(projection_canonical(original.get("raw"))) ==
                    fact.get_string("baseline_expectation_canonical_sha256"),
                "Raw193 baseline expectation mismatch");
        const auto binding_row = fact.get("source_row_binding");
        projection_fields(binding_row, {"authority_index", "case_index", "model_authority_index"});
        require(binding_row.get("authority_index").is_int() &&
                    binding_row.get_int("authority_index") >= 0 &&
                    binding_row.get_int("authority_index") < 3,
                "Raw193 source authority index/type mismatch");
        const auto ai = static_cast<std::size_t>(binding_row.get_int("authority_index"));
        const auto rows = authorities[ai].root().get("cases");
        require(binding_row.get("case_index").is_int() && binding_row.get_int("case_index") >= 0 &&
                    static_cast<std::size_t>(binding_row.get_int("case_index")) < rows.size(),
                "Raw193 authority row index/type mismatch");
        const auto ri = static_cast<std::size_t>(binding_row.get_int("case_index"));
        const auto row = rows.at(ri);
        require(row.get_string("path") == path && row.get_string("input") == source &&
                    row.get_string("input_sha256") == raw_sha,
                "Raw193 independent whole source authority mismatch");
        require(digest(projection_canonical(row)) == fact.get_string("source_row_canonical_sha256"),
                "Raw193 immutable source row digest mismatch");
        require(binding_row.get("model_authority_index").is_int(),
                "Raw193 active authority index/type mismatch");
        const auto mi = binding_row.get_int("model_authority_index");
        require(mi == static_cast<std::int64_t>(ai) || (ai == 1 && mi == 3),
                "Raw193 active/source authority pairing mismatch");
        const bool supplemented = mi == 3;
        const auto active = supplemented ? authorities[3].root() : row;
        require(fact.get("historical_original_model_null").is_bool() &&
                    fact.get("historical_original_model_null").as_bool() == supplemented &&
                    row.get("model").is_null() == supplemented,
                "Raw193 original null history mismatch");
        if (supplemented) {
            ++historical_null;
            const auto sb = active.get("source_binding");
            require(sb.get_string("input_sha256") == raw_sha && sb.get_string("path") == path &&
                        sb.get_string("original_row_canonical_sha256") ==
                            fact.get_string("source_row_canonical_sha256"),
                    "Raw193 supplemental whole source mismatch");
        }
        require(active.get_string("number_encoding") == fact.get_string("number_encoding") &&
                    (supplemented ? fact.get_string("profile") == "fixed"
                                  : row.get_string("profile") == fact.get_string("profile")),
                "Raw193 active source profile mismatch");
        json::JsonMutDoc history_doc;
        auto history = history_doc.object();
        if (row.has("source_model_schema"))
            history.add("source_model_schema",
                        history_doc.copy_val(row.get("source_model_schema")));
        history.add("number_encoding", history_doc.copy_val(row.get("number_encoding")));
        history_doc.set_root(history);
        auto history_parsed = json::parse(history_doc.to_string());
        require(history_parsed.has_value(), "Raw193 historical schema construction");
        require(projection_canonical(history_parsed->root()) ==
                    projection_canonical(fact.get("source_model_schema_fields")),
                "Raw193 immutable declared schema history mismatch");
        json::JsonMutDoc proof_doc;
        auto proof = proof_doc.object();
        proof.add("review", proof_doc.copy_val(fact.get("source_review_hash_references")));
        proof.add("native", proof_doc.copy_val(fact.get("native262_all_evidence")));
        proof.add("snapshot", proof_doc.copy_val(fact.get("candidate_snapshot")));
        proof_doc.set_root(proof);
        auto proof_parsed = json::parse(proof_doc.to_string());
        require(proof_parsed.has_value(), "Raw193 proof serialization");
        require(digest(projection_canonical(proof_parsed->root())) == kRaw193ReviewedEvidence[n],
                "Raw193 independent review/native262/snapshot proof reference mismatch");
        const auto summary = raw193_summary(fact);
        require(raw193_schema(fact.get("model"), fact.get_string("profile"),
                              fact.get_string("number_encoding")) ==
                    projection_canonical(fact.get("active_model_schema")),
                "Raw193 declared active schema mismatch");
        require(raw193_schema(active.get("model"), fact.get_string("profile"),
                              fact.get_string("number_encoding")) ==
                    projection_canonical(fact.get("active_model_schema")),
                "Raw193 independent active schema mismatch");
        const auto full_model = projection_canonical(fact.get("model")),
                   actual_model = projection_canonical(active.get("model"));
        require(full_model == actual_model &&
                    digest(full_model) == fact.get_string("model_canonical_sha256"),
                "Raw193 complete independent Model mismatch");
        const auto authority = fact.get("whole_source_authority");
        projection_fields(authority, {"freeze_file_index", "json_pointer", "authority_file",
                                      "authority_file_sha256", "canonical_sha256"});
        constexpr std::array<std::int64_t, 4> freeze_indices{260, 255, 259, 258};
        projection_int(authority.get("freeze_file_index"),
                       freeze_indices[static_cast<std::size_t>(mi)],
                       "Raw193 authority freeze pointer");
        const auto pointer = supplemented ? "/model" : "/cases/" + std::to_string(ri) + "/model";
        require(authority.get_string("authority_file") ==
                        "tests/fixtures/mermaid_native/" +
                            std::string(kRaw193Authorities[static_cast<std::size_t>(mi)].file) &&
                    authority.get_string("authority_file_sha256") ==
                        kRaw193Authorities[static_cast<std::size_t>(mi)].sha &&
                    authority.get_string("json_pointer") == pointer &&
                    authority.get_string("canonical_sha256") == digest(full_model),
                "Raw193 independent active Model reference mismatch");
        require(projection_canonical(row.get("frontmatter_node_tree")) ==
                    projection_canonical(fact.get("frontmatter_node_tree")),
                "Raw193 complete metadata tree mismatch");
        json::JsonMutDoc constraints_doc;
        auto constraints = constraints_doc.array();
        if (row.has("config_complete_leaf_inventory"))
            row.get("config_complete_leaf_inventory").iter([&](JsonVal leaf) {
                if (leaf.get_string("path") == "sequence.noteAlign") {
                    const auto value = leaf.get_string("value");
                    require(value == "left" || value == "center" || value == "right",
                            "Raw193 source NoteRect alignment value");
                    constraints.append(constraints_doc.string(value));
                }
            });
        constraints_doc.set_root(constraints);
        auto constraints_parsed = json::parse(constraints_doc.to_string());
        require(constraints_parsed.has_value(), "Raw193 constraints serialization");
        require(projection_canonical(constraints_parsed->root()) ==
                    projection_canonical(fact.get("source_rect_constraints")),
                "Raw193 chosen NoteRect source constraints mismatch");
        if (constraints.size() > 0) ++aligned;
        const auto snapshot = fact.get("candidate_snapshot");
        projection_fields(snapshot, {"file", "sha256"});
        const auto expected_file = "sequence_official_raw_" + raw_sha + ".txt";
        require(snapshot.get_string("file") == expected_file &&
                    snapshots.insert(expected_file).second,
                "Raw193 unsafe/duplicate snapshot identity");
        const auto pixels = snapshot_read(expected_file);
        require(pixels.size() <= 256000, "Raw193 snapshot byte cap");
        require(digest(pixels) == snapshot.get_string("sha256"), "Raw193 snapshot SHA mismatch");
        require(!pixels.empty() && pixels.back() == '\n' &&
                    pixels.find('\r') == std::string::npos && pixels.ends_with("AFTER_MERMAID\n") &&
                    pixels.find("AFTER_MERMAID") == pixels.rfind("AFTER_MERMAID"),
                "Raw193 snapshot sentinel/LF protocol mismatch");
        require(fact.get_string("scope") ==
                    "Reviewed complete original raw native terminal source semantics only; "
                    "original API/SVG/CSS/font/HTML/browser/ASCII exact predicates remain pending.",
                "Raw193 source foreign predicate scope mismatch");
        const auto promotion = promotions.at(n);
        projection_fields(promotion, {"case_index", "case_id", "path", "track", "raw_sha256",
                                      "body_offset_utf8_bytes", "body_sha256", "full_model_sha256",
                                      "baseline_expectation_sha256", "snapshot_sha256", "reason",
                                      "effective"});
        require(promotion.get("case_index").is_int() && promotion.get_int("case_index") >= 0 &&
                    promotion.get_int("case_index") < 193,
                "Raw193 promotion case index/type mismatch");
        const auto ci = static_cast<std::size_t>(promotion.get_int("case_index"));
        require(++frequency[ci] == 1, "Raw193 duplicate promotion case");
        require(ci == n && promotion.get_string("case_id") == id &&
                    promotion.get_string("path") == path,
                "Raw193 promotion source identity mismatch");
        require(promotion.get_string("track") == "original", "Raw193 promotion unknown track");
        require(promotion.get_string("raw_sha256") == raw_sha &&
                    promotion.get_string("body_sha256") == fact.get_string("body_sha256"),
                "Raw193 promotion raw/body mismatch");
        projection_int(promotion.get("body_offset_utf8_bytes"), offset,
                       "Raw193 promotion body offset");
        require(promotion.get_string("full_model_sha256") == digest(full_model) &&
                    promotion.get_string("baseline_expectation_sha256") ==
                        fact.get_string("baseline_expectation_canonical_sha256"),
                "Raw193 promotion Model/baseline mismatch");
        require(promotion.get_string("reason") == kRaw193Scope,
                "Raw193 promotion pending scope mismatch");
        const auto effective = promotion.get("effective");
        projection_fields(effective, {"expected", "reason", "snapshot", "checks"});
        require(effective.get_string("expected") == "drawn" &&
                    effective.get_string("reason") == kRaw193Scope,
                "Raw193 effective outcome/scope mismatch");
        require(effective.get_string("snapshot") == expected_file &&
                    promotion.get_string("snapshot_sha256") == snapshot.get_string("sha256"),
                "Raw193 effective snapshot binding mismatch");
        require(projection_canonical(effective.get("checks")) == summary,
                "Raw193 source-derived lightweight summary mismatch");
        validate_expectation(effective, source);
        wraps += fact.get_string("profile") == "source_wrap";
        legacy += fact.get_string("number_encoding") == "LegacyInteger";
        persons += fact.get("model").get("actors").at(0).has("person");
    }
    require(wraps == 20 && legacy == 23 && persons == 24 && aligned == 5 && historical_null == 1,
            "Raw193 complete source partition mismatch");
    for (const auto count : frequency)
        require(count == 1, "Raw193 unreferenced source case");
}

Raw193Documents load_raw193(JsonVal manifest) {
    auto policy = json::parse(raw193_asset(kRaw193Binding.policy_file, kRaw193Binding.policy_bytes,
                                           kRaw193Binding.policy_sha));
    auto facts = json::parse(
        raw193_asset(kRaw193Binding.fact_file, kRaw193Binding.fact_bytes, kRaw193Binding.fact_sha));
    require(policy && facts, "Raw193 cannot parse policy/facts");
    std::vector<json::JsonDoc> authorities;
    authorities.reserve(4);
    for (const auto& authority : kRaw193Authorities) {
        auto parsed = json::parse(raw193_asset(authority.file, authority.bytes, authority.sha));
        require(parsed.has_value(), "Raw193 cannot parse independent authority");
        authorities.push_back(std::move(*parsed));
    }
    validate_raw193(
        policy->root(), facts->root(), manifest, authorities,
        [](std::string_view path) {
            require(fs::file_size(kRoot / "cases" / path) <= 65536,
                    "Raw193 physical source byte cap");
            return read_file(kRoot / "cases" / path);
        },
        [](std::string_view path) {
            projection_regular_file(kNativeRoot, kNativeRoot / path);
            require(fs::file_size(kNativeRoot / path) <= 256000,
                    "Raw193 physical snapshot byte cap");
            return read_file(kNativeRoot / path);
        },
        kRaw193Binding.fact_sha);
    return {std::move(*policy), std::move(*facts), std::move(authorities)};
}

std::optional<Selected> raw193_select(std::span<const json::JsonDoc> policies, JsonVal body_policy,
                                      JsonVal raw_policy, std::string_view path,
                                      std::string_view track) {
    require(track == "original" || track == "body_diagnostic", "Raw193 selector unknown track");
    auto selected = projection_select_complete(policies, body_policy, path, track);
    const auto entries = raw_policy.get("promotions");
    require(entries.is_arr() && entries.size() == 193, "Raw193 selector inventory mismatch");
    std::set<std::pair<std::string, std::string>> identities;
    for (std::size_t n = 0; n < entries.size(); ++n) {
        const auto entry = entries.at(n);
        const auto name = entry.get_string("path"), target = entry.get_string("track");
        require(target == "original" && safe_relative(name) && name.starts_with("sequence/"),
                "Raw193 selector track/path mismatch");
        require(identities.emplace(name, target).second, "Raw193 selector duplicate tuple");
        require(!projection_select(policies, kProjectionBindings, name, target),
                "Raw193 duplicate identity across descriptors");
        if (name == path && target == track) {
            require(!selected, "Raw193 duplicate identity across descriptors");
            selected = Selected{kRaw193Index, n};
        }
    }
    return selected;
}

struct Raw193Consumption {
    std::array<std::size_t, 193> promotions{}, cases{};
};

void raw193_consume(Raw193Consumption& counts, JsonVal policy, std::size_t promotion) {
    require(promotion < 193, "Raw193 consumed promotion outside inventory");
    const auto entry = policy.get("promotions").at(promotion);
    require(entry.get("case_index").is_int() && entry.get_int("case_index") >= 0 &&
                entry.get_int("case_index") < 193,
            "Raw193 consumed case outside inventory");
    const auto ci = static_cast<std::size_t>(entry.get_int("case_index"));
    require(++counts.promotions[promotion] == 1 && ++counts.cases[ci] == 1,
            "Raw193 duplicate consumed source/application");
}

void raw193_consumed(const Raw193Consumption& counts) {
    for (const auto count : counts.promotions)
        require(count == 1, "Raw193 missing/unconsumed promotion");
    for (const auto count : counts.cases)
        require(count == 1, "Raw193 missing/unconsumed source case");
}

constexpr std::size_t kBody22Index = kRaw193Index + 1;
constexpr std::string_view kBody22Scope =
    "Reviewed complete diagnostic-body native terminal projection; raw metadata and original "
    "API/SVG/CSS/font/HTML/browser/ASCII exact predicates remain pending.";
constexpr std::string_view kBody22Protocol = "explicit_body_source_and_active_model/v1";
enum class Body22Kind { WholeBody };

struct Body22Binding {
    std::string_view policy_file, policy_sha, fact_file, fact_sha;
    std::uintmax_t policy_bytes, fact_bytes;
    Body22Kind kind = Body22Kind::WholeBody;
};

constexpr Body22Binding kBody22Binding{
    "sequence_official_body22_projection_supplement.json",
    "7f1bd5b66da867c1fa0f9d8eeb9e1190d8f26c7f3d2e98064e4cd01b62e14f35",
    "sequence_official_body22_refs.json",
    "0d8731566810e98f7d8520d43bfdba20f3d33b41f3176aa60b282182b883f9a5",
    63825,
    40076};
constexpr std::array<std::string_view, 22> kBody22ReviewedEvidence{{
    "6ef91c9b0214544b7a3f16370e69f936321893c95bcd41fac77ff0249d15a9ca",
    "b58ded9f420c69949e8b4d3d04f7abfe514d8432711474f74c55e7bb525e232c",
    "803b34ee44f5948801a6ffc5c0a83f6b5df75a7a49c7c15e4315d5656f2bb39b",
    "2dfca49eb557c39b71128568ebf82df06f94eea900501244946799a4e8abb398",
    "285f050a3d296f3556d0ea78ed2bb341569e101fb6136aa064e819e6b892bd5d",
    "f88b03721a4936d054404ff241374bf59897ad97165371b19bc0c7cad520db8e",
    "a01571072aa61e04e8f27b2950e615d5d790e2d17526a95efcee512e6135ad2b",
    "7a3b1d8aafa78a9f69323bed589aa4a3a5219fc02ba08638c868cd970d305a17",
    "f26c957dc51bb61a5245e71b030a7e772146f9fa8e6283cdb6364fb675bfdb4d",
    "cb63e73be3e792f99268ec6b2be7d43839dd06bf8614800d4b33742b16399d40",
    "563584ed2eaf431daf339c7ecea4751290a38644b399199f07ae45383a6671f4",
    "27d0f50f58798a5d3dbcdbc0fb931e39ebbaecaaec4d70e5924d018b17e9ab00",
    "9325bc349a24187047f4c7a3abf2b02da36039d23f3fcc7c6e555e44e9dede1b",
    "77f05dbd90013dc641befec7716ce950989037a43730251336e3caba8c29c9d8",
    "34fc9ee9dc5bc28220393d000935fe10b72a73677e91061975a3c5de6d4ca449",
    "994928ec535cbf4be94502de739fb2792e794a773d1b48bcf10153b4b0761a90",
    "ceb2d4021202aee1c305587bc0581c7f9c761c34af8e34ba46b8c7dc05823696",
    "e9f44e246a970f513661643d0e741c598a1d99283e5b983fa3663945c41bb186",
    "62604e2ec029194e3e050d887af222a0b39699147dba7bc8c1141f9bdade5af9",
    "1fecf199962450a578bc6e78d70dd71161dc53d220418d1874d53f832cbc8c91",
    "0bbc86c7720faaedf69c1eec44218c807156550f27ac9273ed3364548c367c72",
    "7b25164687c9effd9bb3cbe8ab7efa337bf28f14be655bfef63a38db3b3302bf",
}};

// Complete diagnostic bodies have a separate fixed authority and consumption
// contract. Original raw193 defaults and descriptor remain unchanged.
struct Body22Documents {
    json::JsonDoc policy, facts;
};

struct Body22Model {
    JsonVal source_row, active;
    std::size_t authority_index, row_index;
    bool supplemented;
};

Body22Model body22_model(JsonVal fact, std::span<const json::JsonDoc> authorities) {
    require(authorities.size() == 4, "Body22 independent authority inventory mismatch");
    const auto authority = fact.get("model_authority");
    projection_fields(authority, {"authority_file", "authority_file_sha256", "canonical_sha256",
                                  "freeze_file_index", "json_pointer"});
    const auto file = authority.get_string("authority_file"),
               pointer = authority.get_string("json_pointer");
    std::size_t ai = 4;
    for (const std::size_t n : {0u, 1u, 3u})
        if (file == "tests/fixtures/mermaid_native/" + std::string(kRaw193Authorities[n].file))
            ai = n;
    require(ai < 4 && authority.get_string("authority_file_sha256") == kRaw193Authorities[ai].sha,
            "Body22 independent authority file/SHA mismatch");
    constexpr std::array<std::int64_t, 4> freeze_indices{260, 255, 259, 258};
    projection_int(authority.get("freeze_file_index"), freeze_indices[ai],
                   "Body22 independent authority freeze pointer");
    if (ai == 3) {
        require(pointer == "/model", "Body22 supplement Model pointer mismatch");
        const auto active = authorities[3].root();
        const auto sb = active.get("source_binding");
        require(sb.get_string("path") == fact.get_string("path") &&
                    sb.get_string("input_sha256") == fact.get_string("whole_raw_sha256"),
                "Body22 supplemental whole source mismatch");
        JsonVal original;
        std::size_t matches = 0, index = 0;
        const auto rows = authorities[1].root().get("cases");
        for (std::size_t n = 0; n < rows.size(); ++n)
            if (rows.at(n).get_string("path") == fact.get_string("path")) {
                original = rows.at(n);
                ++matches;
                index = n;
            }
        require(matches == 1 && original.get("model").is_null() &&
                    original.get("number_encoding").is_null(),
                "Body22 original null/schema history mismatch");
        require(digest(projection_canonical(original)) ==
                    sb.get_string("original_row_canonical_sha256"),
                "Body22 supplemental original row digest mismatch");
        return {original, active, ai, index, true};
    }
    require(pointer.starts_with("/cases/") && pointer.ends_with("/model"),
            "Body22 Model pointer syntax mismatch");
    const auto digits = std::string_view(pointer).substr(7, pointer.size() - 13);
    std::size_t index = 0;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), index);
    const auto rows = authorities[ai].root().get("cases");
    require(parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size() &&
                std::to_string(index) == digits && rows.is_arr() && index < rows.size(),
            "Body22 Model pointer index mismatch");
    const auto row = rows.at(index);
    return {row, row, ai, index, false};
}

std::string body22_summary(JsonVal model, std::string_view profile, std::string_view numbers) {
    projection_fields(model,
                      {"title", "actors", "messages", "notes", "fragments", "boxes", "order"});
    require(numbers == "DisplayText", "Body22 active number encoding mismatch");
    require(model.get("actors").is_arr() && model.get("actors").size() > 0,
            "Body22 actor inventory mismatch");
    model.get("actors").iter(
        [](JsonVal actor) { projection_fields(actor, {"id", "label", "role"}); });
    return raw193_model_summary(model, profile, numbers);
}

void validate_body22(JsonVal policy, JsonVal facts, JsonVal manifest,
                     std::span<const json::JsonDoc> authorities, const RoleAssetReader& raw_read,
                     const RoleAssetReader& snapshot_read, std::string_view actual_fact_sha,
                     const Body22Binding& binding = kBody22Binding) {
    require(binding.kind == Body22Kind::WholeBody, "Body22 unknown descriptor kind");
    projection_fields(policy, {"schema", "kind", "baseline_manifest_sha256", "source_facts",
                               "scope", "promotions"});
    projection_int(policy.get("schema"), 1, "Body22 policy schema");
    require(policy.get_string("kind") == "SequenceWholeBody22", "Body22 policy kind mismatch");
    require(policy.get_string("baseline_manifest_sha256") == kManifestSha256,
            "Body22 manifest reference mismatch");
    const auto reference = policy.get("source_facts");
    projection_fields(reference, {"path", "sha256", "protocol"});
    require(actual_fact_sha == binding.fact_sha &&
                reference.get_string("path") == binding.fact_file &&
                reference.get_string("sha256") == actual_fact_sha &&
                reference.get_string("protocol") == kBody22Protocol,
            "Body22 source reference mismatch");
    require(policy.get_string("scope") == kBody22Scope, "Body22 policy scope mismatch");
    projection_fields(facts, {"cases", "immutable_manifest_sha256", "qualification", "schema",
                              "whole_body_count"});
    require(facts.get_string("schema") ==
                "official_sequence_body22_authority_references/proposed-v1",
            "Body22 fact schema mismatch");
    require(facts.get_string("immutable_manifest_sha256") == kManifestSha256,
            "Body22 fact manifest mismatch");
    projection_int(facts.get("whole_body_count"), 22, "Body22 whole body count");
    projection_string(facts.get("qualification"), "Body22 historical qualification");
    const auto cases = facts.get("cases"), promotions = policy.get("promotions");
    require(cases.is_arr() && cases.size() == 22 && promotions.is_arr() && promotions.size() == 22,
            "Body22 case/promotion inventory mismatch");
    std::set<std::string> paths, ids, hashes, snapshots;
    std::array<std::size_t, 22> frequency{};
    std::size_t wraps = 0, supplemented = 0, narrow_draw = 0;
    for (std::size_t n = 0; n < 22; ++n) {
        const auto fact = cases.at(n);
        projection_fields(fact, {"baseline_expectation_canonical_sha256", "body_offset_utf8_bytes",
                                 "id", "input", "input_sha256", "input_utf8_bytes",
                                 "model_authority", "model_schema", "model_sha256",
                                 "number_encoding", "path", "profile", "source_note_alignment",
                                 "source_rect_constraints", "track", "whole_raw_sha256"});
        const auto path = fact.get_string("path"), id = fact.get_string("id"),
                   body_sha = fact.get_string("input_sha256"),
                   raw_sha = fact.get_string("whole_raw_sha256"),
                   profile = fact.get_string("profile");
        require(safe_relative(path) && path.starts_with("sequence/"),
                "Body22 unsafe/nonsequence source path");
        require(fact.get_string("track") == "body_diagnostic", "Body22 fact unknown track");
        require(hex_string(body_sha, 64) && hex_string(raw_sha, 64) &&
                    id == "official-body22/" + body_sha,
                "Body22 source identity mismatch");
        require(paths.insert(path).second && ids.insert(id).second &&
                    hashes.insert(body_sha).second,
                "Body22 duplicate source identity");
        JsonVal original;
        std::size_t found = 0;
        manifest.get("fixtures").iter([&](JsonVal fixture) {
            if (fixture.get_string("path") == path) {
                original = fixture;
                ++found;
            }
        });
        require(found == 1, "Body22 actual manifest path binding missing");
        const auto raw = raw_read(path);
        require(raw.size() <= 65536 && digest(raw) == raw_sha &&
                    original.get_string("sha256") == raw_sha,
                "Body22 exact whole raw binding mismatch");
        const auto diagnostic = original.get("diagnostic");
        require(diagnostic.is_obj() && diagnostic.get("offset").is_int(),
                "Body22 diagnostic provenance missing");
        const auto offset = diagnostic.get_int("offset");
        require(offset >= 0 && static_cast<std::size_t>(offset) <= raw.size(),
                "Body22 actual offset outside source");
        projection_int(fact.get("body_offset_utf8_bytes"), offset, "Body22 body offset");
        const auto body = std::string_view(raw).substr(static_cast<std::size_t>(offset));
        require(fact.get("input").is_str() && fact.get("input").as_str() == body &&
                    digest(body) == body_sha && diagnostic.get_string("sha256") == body_sha,
                "Body22 complete body source binding mismatch");
        projection_int(fact.get("input_utf8_bytes"), static_cast<std::int64_t>(body.size()),
                       "Body22 body byte count");
        require(digest(projection_canonical(diagnostic)) ==
                    fact.get_string("baseline_expectation_canonical_sha256"),
                "Body22 baseline diagnostic mismatch");
        require(fact.get_string("number_encoding") == "DisplayText" &&
                    (profile == "fixed" || profile == "source_wrap"),
                "Body22 profile/number mismatch");
        require(fact.get_string("model_schema") ==
                    std::string(profile == "source_wrap"
                                    ? "sequence_native_source_wrap_number_text/v1"
                                    : "sequence_native_single_role_number_text/v1"),
                "Body22 declared Model schema mismatch");
        require(fact.get("source_note_alignment").is_null() && fact.has("source_note_alignment") &&
                    fact.get("source_rect_constraints").is_arr() &&
                    fact.get("source_rect_constraints").size() == 0,
                "Body22 default chosen NoteRect constraints mismatch");
        const auto model = body22_model(fact, authorities);
        const auto row = model.source_row, active = model.active;
        require(row.get_string("path") == path && row.get_string("input") == raw &&
                    row.get_string("input_sha256") == raw_sha,
                "Body22 independent whole raw authority mismatch");
        require(
            active.get_string("number_encoding") == "DisplayText" &&
                (model.supplemented ? profile == "fixed" : row.get_string("profile") == profile),
            "Body22 active source profile mismatch");
        if (row.has("config_complete_leaf_inventory"))
            row.get("config_complete_leaf_inventory").iter([](JsonVal leaf) {
                require(leaf.get_string("path") != "sequence.noteAlign",
                        "Body22 unexpected source NoteRect constraint");
            });
        const auto summary = body22_summary(active.get("model"), profile, "DisplayText"),
                   full_model = projection_canonical(active.get("model"));
        require(digest(full_model) == fact.get_string("model_sha256") &&
                    fact.get("model_authority").get_string("canonical_sha256") ==
                        fact.get_string("model_sha256"),
                "Body22 complete independent Model mismatch");
        const auto promotion = promotions.at(n);
        projection_fields(promotion, {"case_index", "case_id", "path", "track", "raw_sha256",
                                      "body_offset_utf8_bytes", "body_sha256", "full_model_sha256",
                                      "baseline_expectation_sha256", "snapshot_sha256", "reason",
                                      "effective", "native_evidence"});
        require(promotion.get("case_index").is_int() && promotion.get_int("case_index") >= 0 &&
                    promotion.get_int("case_index") < 22,
                "Body22 promotion case index/type mismatch");
        const auto ci = static_cast<std::size_t>(promotion.get_int("case_index"));
        require(++frequency[ci] == 1, "Body22 duplicate promotion case");
        require(ci == n && promotion.get_string("case_id") == id &&
                    promotion.get_string("path") == path,
                "Body22 promotion source identity mismatch");
        require(promotion.get_string("track") == "body_diagnostic",
                "Body22 promotion unknown track");
        require(promotion.get_string("raw_sha256") == raw_sha &&
                    promotion.get_string("body_sha256") == body_sha,
                "Body22 promotion raw/body mismatch");
        projection_int(promotion.get("body_offset_utf8_bytes"), offset,
                       "Body22 promotion body offset");
        require(promotion.get_string("full_model_sha256") == digest(full_model) &&
                    promotion.get_string("baseline_expectation_sha256") ==
                        fact.get_string("baseline_expectation_canonical_sha256"),
                "Body22 promotion Model/baseline mismatch");
        require(promotion.get_string("reason") == kBody22Scope,
                "Body22 promotion pending scope mismatch");
        const auto effective = promotion.get("effective");
        projection_fields(effective, {"expected", "reason", "snapshot", "checks"});
        require(effective.get_string("expected") == "drawn" &&
                    effective.get_string("reason") == kBody22Scope,
                "Body22 effective outcome/scope mismatch");
        const auto expected_file = "sequence_official_body22_" + body_sha + ".txt";
        require(effective.get_string("snapshot") == expected_file &&
                    snapshots.insert(expected_file).second,
                "Body22 unsafe/duplicate snapshot identity");
        require(projection_canonical(effective.get("checks")) == summary,
                "Body22 source-derived lightweight summary mismatch");
        const auto pixels = snapshot_read(expected_file);
        require(pixels.size() <= 256000 &&
                    digest(pixels) == promotion.get_string("snapshot_sha256"),
                "Body22 snapshot SHA/byte mismatch");
        require(!pixels.empty() && pixels.back() == '\n' &&
                    pixels.find('\r') == std::string::npos && pixels.ends_with("AFTER_MERMAID\n") &&
                    pixels.find("AFTER_MERMAID") == pixels.rfind("AFTER_MERMAID"),
                "Body22 snapshot sentinel/LF protocol mismatch");
        const auto native = promotion.get("native_evidence");
        projection_fields(
            native, {"source_review_sha256", "native_source_sha256", "native_report_sha256",
                     "native_row_index", "native_row_canonical_sha256", "wide_physical_sha256",
                     "narrow_physical_sha256", "restored_physical_sha256", "frame_height",
                     "full_geometry_and_meaningful_mutants", "narrow_source_fallback",
                     "independent_first_release_review_sha256", "first_dual_parity_sha256"});
        projection_int(native.get("native_row_index"), static_cast<std::int64_t>(n),
                       "Body22 native row identity");
        require(native.get("narrow_source_fallback").is_bool(),
                "Body22 native narrow outcome type");
        int width = 0;
        std::istringstream pixel_rows(pixels);
        std::string pixel_row;
        while (std::getline(pixel_rows, pixel_row))
            width = std::max(width, ftxui::string_width(pixel_row));
        require(native.get("narrow_source_fallback").as_bool() == (width > kNarrow),
                "Body22 native/snapshot narrow lifecycle mismatch");
        narrow_draw += width <= kNarrow;
        json::JsonMutDoc proof_doc;
        auto proof = proof_doc.object(), snapshot = proof.make_obj();
        snapshot.set("file", expected_file);
        snapshot.set("sha256", digest(pixels));
        proof.add("fact", proof_doc.copy_val(fact));
        proof.add("native", proof_doc.copy_val(native));
        proof.add("snapshot", snapshot);
        proof_doc.set_root(proof);
        const auto parsed = json::parse(proof_doc.to_string());
        require(parsed.has_value(), "Body22 evidence serialization");
        require(digest(projection_canonical(parsed->root())) == kBody22ReviewedEvidence[n],
                "Body22 independent source/native264/snapshot evidence mismatch");
        validate_expectation(effective, body);
        wraps += profile == "source_wrap";
        supplemented += model.supplemented;
    }
    require(wraps == 17 && supplemented == 1 && narrow_draw == 12,
            "Body22 complete source partition mismatch");
    for (const auto count : frequency)
        require(count == 1, "Body22 unreferenced source case");
}

Body22Documents load_body22(JsonVal manifest, std::span<const json::JsonDoc> authorities) {
    auto policy = json::parse(raw193_asset(kBody22Binding.policy_file, kBody22Binding.policy_bytes,
                                           kBody22Binding.policy_sha));
    auto facts = json::parse(
        raw193_asset(kBody22Binding.fact_file, kBody22Binding.fact_bytes, kBody22Binding.fact_sha));
    require(policy && facts, "Body22 cannot parse policy/facts");
    validate_body22(
        policy->root(), facts->root(), manifest, authorities,
        [](std::string_view path) {
            projection_regular_file((kRoot / "cases" / path).parent_path(), kRoot / "cases" / path);
            require(fs::file_size(kRoot / "cases" / path) <= 65536,
                    "Body22 physical source byte cap");
            return read_file(kRoot / "cases" / path);
        },
        [](std::string_view path) {
            projection_regular_file(kNativeRoot, kNativeRoot / path);
            require(fs::file_size(kNativeRoot / path) <= 256000,
                    "Body22 physical snapshot byte cap");
            return read_file(kNativeRoot / path);
        },
        kBody22Binding.fact_sha);
    return {std::move(*policy), std::move(*facts)};
}

std::optional<Selected> body22_select(std::span<const json::JsonDoc> policies, JsonVal role_policy,
                                      JsonVal raw_policy, JsonVal policy, std::string_view path,
                                      std::string_view track,
                                      Body22Kind kind = Body22Kind::WholeBody) {
    require(kind == Body22Kind::WholeBody, "Body22 selector unknown descriptor kind");
    auto selected = raw193_select(policies, role_policy, raw_policy, path, track);
    const auto entries = policy.get("promotions");
    require(entries.is_arr() && entries.size() == 22, "Body22 selector inventory mismatch");
    std::set<std::pair<std::string, std::string>> identities;
    for (std::size_t n = 0; n < 22; ++n) {
        const auto entry = entries.at(n);
        const auto name = entry.get_string("path"), target = entry.get_string("track");
        require(target == "body_diagnostic" && safe_relative(name) && name.starts_with("sequence/"),
                "Body22 selector track/path mismatch");
        require(identities.emplace(name, target).second, "Body22 selector duplicate tuple");
        if (name == path && target == track) {
            require(!selected, "Body22 duplicate identity across descriptors");
            selected = Selected{kBody22Index, n};
        }
    }
    // The entrance selector already validates every legacy descriptor. Raw193
    // is original-only and the complete new inventory is body-diagnostic-only.
    // Compare the remaining legacy tuples once against the new identity set.
    for (const auto& legacy : policies)
        legacy.root().get("promotions").iter([&](JsonVal entry) {
            require(!identities.contains({entry.get_string("path"), entry.get_string("track")}),
                    "Body22 duplicate identity across descriptors");
        });
    role_policy.get("promotions").iter([&](JsonVal entry) {
        require(!identities.contains({entry.get_string("path"), entry.get_string("track")}),
                "Body22 duplicate identity across descriptors");
    });
    return selected;
}

struct Body22Consumption {
    std::array<std::size_t, 22> promotions{}, cases{};
};

void body22_consume(Body22Consumption& counts, JsonVal policy, std::size_t promotion) {
    require(promotion < 22, "Body22 consumed promotion outside inventory");
    const auto entry = policy.get("promotions").at(promotion);
    require(entry.get("case_index").is_int() && entry.get_int("case_index") >= 0 &&
                entry.get_int("case_index") < 22,
            "Body22 consumed case outside inventory");
    const auto ci = static_cast<std::size_t>(entry.get_int("case_index"));
    require(++counts.promotions[promotion] == 1 && ++counts.cases[ci] == 1,
            "Body22 duplicate consumed application/case");
}

void body22_consumed(const Body22Consumption& counts) {
    for (const auto n : counts.promotions)
        require(n == 1, "Body22 missing/unconsumed promotion");
    for (const auto n : counts.cases)
        require(n == 1, "Body22 missing/unconsumed source case");
}

struct Frame {
    std::vector<std::vector<std::string>> cells;
    std::string text;
    int content_width = 0;
};
// StageB is deliberately suite-local. Existing null captures do not use this
// verification ledger; only the five finite controls select the paid path.
namespace relations_oracle = loom::test::mermaid_relations;
namespace flow_oracle = loom::test::mermaid_flow;

void stage_b_product(relations_oracle::Work& work, std::size_t count, std::size_t unit) {
    if (unit && count > work.left / unit) {
        work.spend(work.left + 1u);
        return;
    }
    work.spend(count * unit);
}

Frame stage_b_paid_capture(const ftxui::Element& element, int width, int height,
                           relations_oracle::Work& work) {
    work.spend(16u * sizeof(std::size_t) + 8u * sizeof(void*) + 256u);
    relations_oracle::class481_require(width > 0 && width <= 240 && height > 0 && height <= 1040,
                                       "StageB paid capture dimensions");
    const auto W = static_cast<std::size_t>(width), H = static_cast<std::size_t>(height), C = W * H;
    constexpr auto PX = sizeof(ftxui::Pixel), PV = sizeof(std::vector<ftxui::Pixel>);
    work.spend(sizeof(ftxui::Screen) + 8u * sizeof(std::size_t) + alignof(ftxui::Pixel) +
               alignof(std::vector<ftxui::Pixel>) + 256u);
    stage_b_product(work, H + 1u, PV);
    stage_b_product(work, (H + 1u) * W, PX);
    stage_b_product(work, W, PX);
    stage_b_product(work, C, 2u * PX);
    stage_b_product(work, H, 2u * PV);
    stage_b_product(work, (H + 1u) * W, 2u);
    stage_b_product(work, W, 4u);
    stage_b_product(work, C, 4u);
    stage_b_product(work, (H + 1u) * W, 16u);
    // All Screen storage/default writes/row fill copies precede construction.
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, element);
    stage_b_product(work, C, 2u);
    stage_b_product(work, H, 4u);
    work.spend(256u);
    std::size_t payload = 0;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const auto& character = screen.PixelAt(x, y).character;
            stage_b_product(work, character.size(), 8u);
            work.spend(8u);
            // Paid eight-byte shares bound payload well below SIZE_MAX.
            payload += character.size();
        }
    work.spend(4u * sizeof(Frame) + 8u * sizeof(ftxui::Element) + 256u);
    stage_b_product(work, H, sizeof(std::vector<std::string>));
    stage_b_product(work, C, sizeof(std::string) + 8u);
    stage_b_product(work, C, 2u * sizeof(std::string));
    stage_b_product(work, H,
                    3u * sizeof(std::vector<std::string>) + 4u * sizeof(std::string) + 64u);
    stage_b_product(work, H, 4u);
    // Payload shares paid above cover retained cells, row text and final text.
    Frame frame;
    frame.cells.reserve(H);
    frame.text.reserve(payload + H);
    for (int y = 0; y < height; ++y) {
        std::size_t row_bytes = 0;
        for (int x = 0; x < width; ++x) {
            work.spend();
            row_bytes += screen.PixelAt(x, y).character.size();
        }
        std::vector<std::string> row;
        row.reserve(W);
        std::string text;
        text.reserve(row_bytes);
        int last_nonblank = -1;
        for (int x = 0; x < width; ++x) {
            const auto& character = screen.PixelAt(x, y).character;
            row.emplace_back(character);
            text.append(character);
            if (character != " " && !character.empty()) last_nonblank = x;
        }
        frame.content_width = std::max(frame.content_width, last_nonblank + 1);
        while (!text.empty() && text.back() == ' ') {
            work.spend();
            text.pop_back();
        }
        frame.text.append(text);
        frame.text.push_back('\n');
        frame.cells.push_back(std::move(row));
    }
    while (!frame.text.empty() && frame.text.back() == '\n') {
        work.spend();
        frame.text.pop_back();
    }
    return frame;
}

relations_oracle::Grid frame_grid(const Frame& frame, int width, int height,
                                  relations_oracle::Work& work) {
    work.spend(4u * sizeof(relations_oracle::Grid) + 12u * sizeof(std::size_t) + 256u);
    relations_oracle::class481_require(width > 0 && width <= 240 && height > 0 && height <= 1040,
                                       "StageB Frame expected dimensions");
    const auto W = static_cast<std::size_t>(width), H = static_cast<std::size_t>(height), C = W * H;
    relations_oracle::class481_require(frame.cells.size() == H,
                                       "StageB Frame physical height differs");
    for (const auto& row : frame.cells) {
        work.spend(4u);
        relations_oracle::class481_require(row.size() == W,
                                           "StageB Frame physical row width differs");
    }
    stage_b_product(work, C, 3u * sizeof(std::string) + 8u);
    relations_oracle::Grid result{width, height, {}};
    result.cells.reserve(C);
    for (const auto& row : frame.cells)
        for (const auto& cell : row) {
            stage_b_product(work, cell.size(), 2u);
            work.spend(8u);
            result.cells.emplace_back(cell);
        }
    return result;
}

Frame capture(ftxui::Element element, int width, int height,
              relations_oracle::Work* paid_work = nullptr) {
    if (paid_work) return stage_b_paid_capture(element, width, height, *paid_work);
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, element);
    Frame frame;
    frame.cells.reserve(static_cast<std::size_t>(height));
    frame.text.reserve(static_cast<std::size_t>(width * height));
    for (int y = 0; y < screen.dimy(); ++y) {
        std::vector<std::string> row;
        std::string text;
        row.reserve(static_cast<std::size_t>(width));
        text.reserve(static_cast<std::size_t>(width));
        int last_nonblank = -1;
        for (int x = 0; x < screen.dimx(); ++x) {
            const auto character = screen.PixelAt(x, y).character;
            row.push_back(character);
            text += character;
            if (character != " " && !character.empty()) last_nonblank = x;
        }
        frame.content_width = std::max(frame.content_width, last_nonblank + 1);
        while (!text.empty() && text.back() == ' ')
            text.pop_back();
        frame.text += text + '\n';
        frame.cells.push_back(std::move(row));
    }
    while (!frame.text.empty() && frame.text.back() == '\n')
        frame.text.pop_back();
    return frame;
}

std::string fenced(std::string_view source, std::string_view language,
                   std::string_view following_token = kSentinel) {
    std::size_t longest = 0, run = 0;
    for (const auto byte : source) {
        run = byte == '`' ? run + 1 : 0;
        longest = std::max(longest, run);
    }
    const std::string fence(std::max(std::size_t{3}, longest + 1), '`');
    return fence + std::string(language) + "\n" + std::string(source) + "\n" + fence + "\n\n" +
           std::string(following_token);
}

std::optional<std::pair<int, int>> label_position(const Frame& frame, std::string_view label) {
    for (std::size_t y = 0; y < frame.cells.size(); ++y) {
        std::string row;
        std::vector<int> columns;
        for (std::size_t x = 0; x < frame.cells[y].size(); ++x) {
            for (std::size_t byte = 0; byte < frame.cells[y][x].size(); ++byte)
                columns.push_back(static_cast<int>(x));
            row += frame.cells[y][x];
        }
        if (row.find(kSentinel) != std::string::npos) break;
        const auto found = row.find(label);
        if (found != std::string::npos) return std::pair{columns.at(found), static_cast<int>(y)};
    }
    return std::nullopt;
}

int arrows(const Frame& frame) {
    int count = 0;
    for (const auto& row : frame.cells)
        for (const auto& cell : row)
            if (cell == "▶" || cell == "◀" || cell == "▼" || cell == "▲") ++count;
    return count;
}

std::vector<std::string> semantic_errors(const Frame& frame, JsonVal checks) {
    std::vector<std::string> errors;
    checks.get("labels").iter([&](JsonVal label) {
        if (!label_position(frame, label.as_str()))
            errors.push_back("Missing label: " + std::string(label.as_str()));
    });
    if (arrows(frame) != checks.get_int("arrows"))
        errors.push_back("Arrow count differs from declared oracle");
    checks.get("ordering").iter([&](JsonVal ordering) {
        const auto before = label_position(frame, ordering.get_string("before"));
        const auto after = label_position(frame, ordering.get_string("after"));
        const bool x = ordering.get_string("axis") == "x";
        if (!before || !after ||
            (x ? before->first >= after->first : before->second >= after->second))
            errors.push_back("Declared " + ordering.get_string("axis") + " ordering failed: " +
                             ordering.get_string("before") + " / " + ordering.get_string("after"));
    });
    return errors;
}

std::string classify(const Frame& actual, const Frame& ordinary, bool expect_drawn, JsonVal checks,
                     std::string_view snapshot, std::vector<std::string>& errors) {
    const auto initial_errors = errors.size();
    if (actual.text.find(kSentinel) == std::string::npos)
        errors.emplace_back("Following-text sentinel missing");
    if (expect_drawn) {
        if (actual.cells == ordinary.cells) errors.emplace_back("Unexpected source fallback");
        const auto semantics = semantic_errors(actual, checks);
        errors.insert(errors.end(), semantics.begin(), semantics.end());
        if (actual.text != snapshot) errors.emplace_back("Reviewed glyph snapshot mismatch");
    } else if (actual.cells != ordinary.cells)
        errors.emplace_back("Expected exact ordinary-code source fallback");
    return errors.size() == initial_errors ? (expect_drawn ? "drawn" : "source_fallback") : "error";
}

struct Observation {
    std::string path, track, type, source_sha256, expected, reason;
    std::array<std::string, 3> status{"error", "error", "error"};
    std::array<std::string, 3> expected_states;
    std::array<std::string, 3> decision_reasons;
    std::vector<std::string> errors;
    std::string projection_baseline, projection_effective;
    std::size_t projection_binding = 0;
    std::string role_case_id, role_model_sha, role_body_sha;
    std::size_t role_case_index = 0;
    std::string raw_source_authority, raw_active_schema, raw_source_constraints;
};

namespace official_er107 = loom::test::mermaid_er107::official;
namespace er107 = loom::test::mermaid_er107;
constexpr auto kOfficialEr107Contract = official_er107::kContract;
constexpr std::string_view kOfficialEr107MatrixEvidence = "mermaid-official-er107-matrix.jsonl";

bool official_er107_owned(const Observation& row) {
    require(row.raw_active_schema == kOfficialEr107Contract && row.track == "original" &&
                row.type == "er-diagram" && row.role_case_index < er107::kPins.size(),
            "ER107 owned Matrix identity");
    const auto& pin = er107::kPins[row.role_case_index];
    const auto& projection = er107::authority::kPins[row.role_case_index];
    const auto authority = json::parse(row.raw_source_authority);
    require(authority.has_value() && authority->root().is_obj() && authority->root().size() == 2 &&
                authority->root().get_string("source_facts_sha256") == er107::kFactsSha &&
                authority->root().get("retained_ER206").is_obj(),
            "ER107 owned retained source authority");
    require(row.path == pin.path && row.source_sha256 == pin.raw_sha &&
                row.role_model_sha == projection.complete_sha &&
                row.role_body_sha == projection.scene_sha,
            "ER107 owned Matrix source/Model pins");
    const auto proof = json::parse(row.raw_source_constraints);
    require(proof.has_value(), "ER107 owned Matrix proof JSON");
    const bool ready = official_er107::owned(proof->root());
    if (ready)
        require(row.errors.empty() && row.status[0] == "drawn" && row.status[2] == "drawn" &&
                    row.status[1] == (proof->root().get("narrow_exact_ordinary").as_bool()
                                          ? "source_fallback"
                                          : "drawn"),
                "ER107 owned Matrix complete states");
    return ready;
}

namespace block58 = loom::test::mermaid_block58;
namespace official_block58 = block58::official;
constexpr auto kBlock58Contract = official_block58::kContract;

Observation run_official_block58(const official_block58::Documents& documents, JsonVal fixture,
                                 std::size_t index, std::ostream& diagnostics) {
    require(index < block58::kPins.size(), "Block58 compiled source index");
    const auto& pin = block58::kPins[index];
    Observation row;
    row.path = fixture.get_string("path");
    row.track = "original";
    row.type = "block";
    row.source_sha256 = fixture.get_string("sha256");
    row.expected = "drawn";
    row.reason =
        "RFC 0030 independent Block58 complete-original local proof; external join required";
    row.expected_states = {"drawn", "unresolved", "drawn"};
    row.decision_reasons.fill(row.reason);
    row.raw_active_schema = kBlock58Contract;
    row.raw_source_authority = block58::kSourceFactsSha;
    row.role_case_index = index;
    row.role_model_sha = pin.source_model_sha;
    row.role_body_sha = pin.terminal_model_sha;
    row.projection_baseline = projection_canonical(fixture.get("raw"));
    json::JsonMutDoc report, compact;
    auto record = report.object(), proof = compact.object();
    official_block58::pin_metadata(record, pin);
    official_block58::pin_metadata(proof, pin);
    bool ready = false, captured = false, bound = false, geometry = false, narrow = false;
    std::size_t semantic = 0, physical = 0, captures = 0;
    try {
        require(row.path == pin.path && row.source_sha256 == pin.raw_sha,
                "source: Official Block58 fixture identity");
        const auto raw = official_block58::read(kRoot / "cases" / row.path, block58::kMaxRawBytes);
        block58::OracleWork capture_work;
        capture_work.left = 768'000'000;
        loom::ui::mermaid::LayoutRowsCache cache(4, 4 * 1024 * 1024);
        // Each group owns five public Markdown frames; all fifteen precede the
        // first typed source case binding. The genuine hit frames are retained.
        const auto uncached = official_block58::observe(raw, capture_work);
        captures += 5;
        official_block58::publish(uncached, record);
        const auto cached = official_block58::observe(raw, capture_work, &cache);
        captures += 5;
        auto cached_record = record.make_obj();
        official_block58::publish(cached, cached_record);
        record.add("cached", cached_record);
        const auto hit = official_block58::observe(raw, capture_work, &cache);
        captures += 5;
        auto hit_record = record.make_obj();
        official_block58::publish(hit, hit_record);
        record.add("cache_hit", hit_record);
        captured = captures == 15;
        require(cache.hits() > 0, "protocol: Official public Markdown cache hit");
        block58::OracleWork binding;
        const auto facts = documents.bind(row.path, raw, binding);
        bound = true;
        block58::OracleWork deriving;
        const auto expected = block58::authority::derive(facts, raw.size(), deriving);
        const auto colors = official_block58::palette();
        block58::OracleWork checking;
        checking.left = 768'000'000;
        const auto normalized =
            official_block58::validate(uncached, expected, colors, true, checking);
        static_cast<void>(official_block58::validate(cached, expected, colors, true, checking));
        static_cast<void>(official_block58::validate(hit, expected, colors, true, checking));
        require(uncached == cached && cached == hit,
                "physical: Official all fifteen cache/uncached glyph and style frames");
        const auto mutations = block58::authority::exercise(normalized, expected, colors, 0, 0,
                                                            facts.terminal_model, checking);
        semantic = mutations.semantic;
        physical = mutations.physical;
        // Test the inherited bit without weakening the full plane checker.
        auto bad_dim = uncached[0];
        bad_dim.cells.front().other_attributes ^= 2;
        bool rejected = false;
        try {
            static_cast<void>(official_block58::normalize_dim(bad_dim, expected, true, checking));
        } catch (const std::exception& error) {
            if (block58::authority::resource_error(error.what())) throw;
            rejected = true;
        }
        require(rejected, "physical: missing Markdown dim mutant survived");
        ++physical;
        geometry = true;
        narrow = expected.geometry.metrics.width > 40;
        ready = true;
        row.status = {"drawn", narrow ? "source_fallback" : "drawn", "drawn"};
        row.expected_states[1] = narrow ? "source" : "drawn";
    } catch (const std::exception& error) {
        row.errors.push_back(error.what());
    }
    for (auto value : {record, proof}) {
        value.set("local_ready", ready);
        value.set("captures", static_cast<std::int64_t>(captures));
        value.set("captures_before_binding", captured);
        value.set("bound_after_captures", bound);
        value.set("geometry_and_restoration_good", geometry);
        value.set("narrow_exact_ordinary", ready && narrow);
        value.set("narrow_geometry_good", ready && !narrow);
        value.set("uncached_cached_five_planes_equal", ready);
        value.set("cache_hit_observed", ready);
        value.set("markdown_dim_color", true);
        value.set("inherited_other_attributes", 2);
        value.set("semantic_mutations_rejected", static_cast<std::int64_t>(semantic));
        value.set("physical_mutations_rejected", static_cast<std::int64_t>(physical));
        if (!row.errors.empty()) value.set("error", row.errors.front());
    }
    report.set_root(record);
    diagnostics << report.to_string() << '\n';
    require(diagnostics.good(), "protocol: Official Block58 actual frame evidence write");
    compact.set_root(proof);
    row.raw_source_constraints = compact.to_string();
    return row;
}

bool official_block58_owned(const Observation& row) {
    require(row.role_case_index < block58::kPins.size(), "source: Official Block58 ordinal bound");
    const auto& pin = block58::kPins[row.role_case_index];
    require(row.raw_active_schema == kBlock58Contract &&
                row.raw_source_authority == block58::kSourceFactsSha && row.track == "original" &&
                row.type == "block" && row.path == pin.path && row.source_sha256 == pin.raw_sha &&
                row.role_model_sha == pin.source_model_sha &&
                row.role_body_sha == pin.terminal_model_sha,
            "source: Official Block58 owned row identity");
    const auto parsed = json::parse(row.raw_source_constraints);
    require(parsed.has_value(), "source: Official Block58 proof JSON");
    const auto proof = parsed->root();
    official_block58::check_pins(proof, pin);
    json::JsonMutDoc expected;
    auto keys = expected.object();
    official_block58::pin_metadata(keys, pin);
    expected.set_root(keys);
    const auto key_doc = json::parse(expected.to_string());
    require(key_doc.has_value(), "protocol: Official Block58 proof keys");
    const std::set<std::string> runtime{"local_ready",
                                        "captures",
                                        "captures_before_binding",
                                        "bound_after_captures",
                                        "geometry_and_restoration_good",
                                        "narrow_exact_ordinary",
                                        "narrow_geometry_good",
                                        "uncached_cached_five_planes_equal",
                                        "cache_hit_observed",
                                        "markdown_dim_color",
                                        "inherited_other_attributes",
                                        "semantic_mutations_rejected",
                                        "physical_mutations_rejected",
                                        "error"};
    proof.iter_obj([&](JsonVal key, JsonVal) {
        require(key_doc->root().has(key.as_str()) || runtime.contains(std::string(key.as_str())),
                "source: Official Block58 foreign proof key");
    });
    const auto yes = [&](std::string_view key) {
        const auto value = proof.get(key);
        require(value.is_bool(), "source: Official Block58 proof boolean");
        return value.as_bool();
    };
    if (!yes("local_ready")) {
        require(!row.errors.empty() && row.status[0] == "error" && row.status[2] == "error",
                "source: failed Block58 proof disposition");
        return false;
    }
    require(row.errors.empty() && row.status[0] == "drawn" && row.status[2] == "drawn" &&
                !proof.has("error"),
            "source: Official Block58 good row disposition");
    require(proof.get("captures").is_int() && proof.get_int("captures") == 15 &&
                yes("captures_before_binding") && yes("bound_after_captures") &&
                yes("geometry_and_restoration_good") && yes("uncached_cached_five_planes_equal") &&
                yes("cache_hit_observed") && yes("markdown_dim_color") &&
                proof.get("inherited_other_attributes").is_int() &&
                proof.get_int("inherited_other_attributes") == 2,
            "source: Official Block58 complete capture responsibility");
    require(proof.get("semantic_mutations_rejected").is_int() &&
                proof.get_int("semantic_mutations_rejected") >= 6 &&
                proof.get("physical_mutations_rejected").is_int() &&
                proof.get_int("physical_mutations_rejected") >= 8,
            "source: Official Block58 good-first mutation responsibility");
    require(row.status[1] == "source_fallback"
                ? yes("narrow_exact_ordinary") && !yes("narrow_geometry_good")
                : row.status[1] == "drawn" && yes("narrow_geometry_good") &&
                      !yes("narrow_exact_ordinary"),
            "source: Official Block58 exact narrow disposition");
    return true;
}

std::string official_block58_verdict(const std::vector<Observation>& rows) {
    json::JsonMutDoc document;
    auto root = document.object(), cases = root.make_arr();
    std::array<bool, 58> consumed{};
    int ready = 0, selected = 0;
    for (const auto& row : rows)
        if (row.raw_active_schema == kBlock58Contract) {
            const bool good = official_block58_owned(row);
            require(!consumed.at(row.role_case_index),
                    "source: Official Block58 duplicate verdict original");
            consumed[row.role_case_index] = true;
            const auto proof = json::parse(row.raw_source_constraints);
            require(proof.has_value(), "source: Official Block58 compact proof");
            cases.append(document.copy_val(proof->root()));
            ready += good;
            ++selected;
        }
    root.set("schema", "block58_official_complete_original/v1");
    root.set("contract", kBlock58Contract);
    root.set("source_facts_sha256", block58::kSourceFactsSha);
    root.set("terminal_facts_sha256", block58::kTerminalFactsSha);
    root.set("source_lock_sha256", block58::kSourceLock);
    root.set("official_manifest_sha256", block58::kManifestSha);
    root.set("boundary_sha256", block58::boundary::kAssetSha);
    root.set("reviewed_design_sha256", block58::authority::kReviewedDesignSha);
    root.set("authority_adaptation", block58::authority::kAdaptation);
    root.set("authority_review_status", block58::authority::kAuthorityReviewStatus);
    root.set("limitations", block58::authority::kLimitations);
    root.set("selected", selected);
    root.set("local_ready", ready);
    root.set("admitted", 0);
    root.set("drawing_credit", 0);
    root.set("formal_admission", false);
    root.set("join_required", true);
    root.set("foreign_assertions_certified", 0);
    root.set("native_report_input", false);
    root.set("actual_color_cells_file", official_block58::kEvidence);
    root.add("cases", cases);
    document.set_root(root);
    return document.to_string();
}

// Mindmap owns its public observations here; the shared oracle never captures
// or publishes and this consumer never reads a Native report.
namespace mindmap10 = loom::test::mermaid_mindmap10;
constexpr std::string_view kMindmap10Contract = "Mindmap10/complete-original/v1";

struct OfficialMindmapFrame {
    mindmap10::Grid grid;
    std::vector<std::string> foreground, background;
};

OfficialMindmapFrame official_mindmap_capture(ftxui::Element node, int width, int height,
                                              mindmap10::Work& work) {
    require(width > 0 && width <= 240 && height > 0 && height <= 1040,
            "Mindmap10 bounded Official frame");
    const auto cells = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    work.spend(cells * (3u * sizeof(std::string) + 64u) + 512u);
    OfficialMindmapFrame frame;
    frame.grid.width = width;
    frame.grid.height = height;
    frame.grid.cells.reserve(cells);
    frame.foreground.reserve(cells);
    frame.background.reserve(cells);
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, node);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const auto& cell = screen.PixelAt(x, y);
            frame.grid.cells.push_back(cell.character);
            frame.foreground.push_back(cell.foreground_color.Print(false));
            frame.background.push_back(cell.background_color.Print(true));
        }
    return frame;
}

bool official_mindmap_surface(const OfficialMindmapFrame& a, const OfficialMindmapFrame& b) {
    return a.grid.width == b.grid.width && a.grid.height == b.grid.height &&
           a.grid.cells == b.grid.cells && a.background == b.background &&
           a.foreground == b.foreground;
}

void official_mindmap_publish(const OfficialMindmapFrame& frame, json::JsonMutVal item,
                              mindmap10::Work& work) {
    work.spend(frame.grid.cells.size() * 128u + 512u);
    item.set("width", frame.grid.width);
    item.set("height", frame.grid.height);
    auto glyphs = item.make_arr();
    std::string bytes;
    for (std::size_t n = 0; n < frame.grid.cells.size(); ++n) {
        glyphs.append(item.make_str(frame.grid.cells[n]));
        for (const auto* text :
             {&frame.grid.cells[n], &frame.foreground[n], &frame.background[n]}) {
            bytes += std::to_string(text->size());
            bytes += ':';
            bytes += *text;
        }
    }
    item.add("glyph_cells", glyphs);
    item.set("cell_sha256", digest(bytes));
    for (const auto& [name, colors] :
         std::array<std::pair<std::string_view, const std::vector<std::string>*>, 2>{
             {{"foreground_runs", &frame.foreground}, {"background_runs", &frame.background}}}) {
        auto runs = item.make_arr();
        for (std::size_t begin = 0; begin < colors->size();) {
            std::size_t end = begin + 1;
            while (end < colors->size() && (*colors)[end] == (*colors)[begin])
                ++end;
            auto run = item.make_arr();
            run.append(item.make_num(static_cast<int64_t>(end - begin)));
            run.append(item.make_str((*colors)[begin]));
            runs.append(run);
            begin = end;
        }
        item.add(name, runs);
    }
}

std::optional<std::size_t> official_mindmap_selected(JsonVal facts, std::size_t global,
                                                     std::string_view path) {
    const auto cases = facts.get("cases");
    require(cases.is_arr() && cases.size() == 10u, "Mindmap10 complete selection");
    for (std::size_t n = 0; n < cases.size(); ++n)
        if (cases.at(n).get_string("path") == path) {
            require(cases.at(n).get_int("manifest_global_ordinal") == static_cast<int64_t>(global),
                    "Mindmap10 selection ordinal");
            return n;
        }
    return std::nullopt;
}

Observation run_official_mindmap(JsonVal facts, JsonVal manifest, JsonVal fixture,
                                 std::size_t index, std::ostream& diagnostics) {
    Observation row;
    row.path = fixture.get_string("path");
    row.track = "original";
    row.type = "mindmap";
    row.source_sha256 = fixture.get_string("sha256");
    row.expected = "drawn";
    row.reason = "RFC 0030 Mindmap10 independently source-bound complete original";
    row.expected_states = {"drawn", "unresolved", "drawn"};
    row.decision_reasons.fill(row.reason);
    row.raw_active_schema = kMindmap10Contract;
    row.raw_source_authority = mindmap10::kFactsSha;
    row.role_case_index = index;
    json::JsonMutDoc report;
    auto record = report.object();
    mindmap10::Work work;
    bool admitted = false, captured = false, bound = false, geometry = false,
         narrow_ordinary = false, narrow_geometry = false;
    std::size_t semantic = 0, physical = 0, captures = 0;
    record.set("path", row.path);
    record.set("source_sha256", row.source_sha256);
    try {
        const auto path = kRoot / "cases" / row.path;
        require(fs::is_regular_file(path) && fs::file_size(path) <= 65536u,
                "Mindmap10 bounded complete source");
        const auto source = read_file(path);
        auto node = loom::ui::render_markdown(fenced(source, "mermaid", kSentinel));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text", kSentinel));
        node->ComputeRequirement();
        ordinary->ComputeRequirement();
        const int height = std::min(
            1040,
            std::max(40, std::max(node->requirement().min_y, ordinary->requirement().min_y) + 12));
        std::array<OfficialMindmapFrame, 5> frames;
        constexpr std::array<int, 5> widths{240, 40, 240, 240, 40};
        auto published = record.make_arr();
        for (std::size_t n = 0; n < frames.size(); ++n) {
            frames[n] = official_mindmap_capture(n < 3u ? node : ordinary, widths[n], height, work);
            ++captures;
            auto entry = record.make_obj();
            official_mindmap_publish(frames[n], entry, work);
            published.append(entry);
        }
        captured = captures == 5u;
        record.add("five_frames", published);
        record.set("captures_before_binding", captured);
        const auto fact = facts.get("cases").at(index);
        const auto model = mindmap10::bind(facts, fact, source, manifest, work);
        bound = true;
        row.role_model_sha = fact.get_string("complete_model_sha256");
        row.role_body_sha = fact.get_string("terminal_projection_sha256");
        require(official_mindmap_surface(frames[0], frames[2]) &&
                    frames[0].foreground == frames[2].foreground,
                "Mindmap10 restored color cells");
        require(!official_mindmap_surface(frames[0], frames[3]),
                "Mindmap10 complete original still uses source");
        const bool narrow_source = official_mindmap_surface(frames[1], frames[4]);
        narrow_ordinary = narrow_source;
        for (const auto n : {0u, 1u, 2u}) {
            if (n == 1u && narrow_source) continue;
            const auto errors = mindmap10::validate(frames[n].grid, model, kSentinel, work);
            require(errors.empty(), errors.empty() ? "Mindmap10 geometry" : errors.front());
        }
        geometry = true;
        narrow_geometry = !narrow_source;
        const auto proof = mindmap10::exercise(
            frames[0].grid, model, fact.get("required_good_first_mutations"), kSentinel, work);
        require(proof.errors.empty(),
                proof.errors.empty() ? "Mindmap10 mutations" : proof.errors.front());
        semantic = proof.semantic;
        physical = proof.physical;
        require(semantic + physical == fact.get("required_good_first_mutations").size(),
                "Mindmap10 complete mutation responsibility");
        row.status = {"drawn", narrow_source ? "source_fallback" : "drawn", "drawn"};
        row.expected_states[1] = narrow_source ? "source" : "drawn";
        admitted = true;
    } catch (const std::exception& error) {
        row.errors.push_back(error.what());
    }
    record.set("admitted", admitted);
    record.set("semantic_mutations_rejected", static_cast<int64_t>(semantic));
    record.set("physical_mutations_rejected", static_cast<int64_t>(physical));
    record.set("work_remaining", static_cast<int64_t>(work.left));
    if (!row.errors.empty()) record.set("error", row.errors.front());
    report.set_root(record);
    diagnostics << report.to_string() << '\n';
    json::JsonMutDoc compact;
    auto proof = compact.object();
    proof.set("source_facts_sha256", mindmap10::kFactsSha);
    proof.set("source_sha256", row.source_sha256);
    proof.set("complete_model_sha256", row.role_model_sha);
    proof.set("terminal_projection_sha256", row.role_body_sha);
    proof.set("captures", static_cast<int64_t>(captures));
    proof.set("captures_before_binding", captured);
    proof.set("bound_after_captures", bound);
    proof.set("geometry_and_restoration_good", geometry);
    proof.set("local_ready", admitted);
    proof.set("narrow_exact_ordinary", narrow_ordinary);
    proof.set("narrow_geometry_good", narrow_geometry);
    proof.set("semantic_mutations_rejected", static_cast<int64_t>(semantic));
    proof.set("physical_mutations_rejected", static_cast<int64_t>(physical));
    proof.set("foreign_assertions_certified", 0);
    proof.set("native_report_input", false);
    compact.set_root(proof);
    row.raw_source_constraints = compact.to_string();
    return row;
}

bool official_mindmap_owned(const Observation& row, JsonVal facts) {
    require(row.track == "original" && row.type == "mindmap" &&
                row.raw_source_authority == mindmap10::kFactsSha,
            "Mindmap10 owned source identity");
    const auto info = json::parse(row.raw_source_constraints);
    require(info.has_value(), "Mindmap10 proof JSON");
    const auto proof = info->root();
    constexpr std::array<std::string_view, 15> keys{"source_facts_sha256",
                                                    "source_sha256",
                                                    "complete_model_sha256",
                                                    "terminal_projection_sha256",
                                                    "captures",
                                                    "captures_before_binding",
                                                    "bound_after_captures",
                                                    "geometry_and_restoration_good",
                                                    "local_ready",
                                                    "semantic_mutations_rejected",
                                                    "physical_mutations_rejected",
                                                    "foreign_assertions_certified",
                                                    "native_report_input",
                                                    "narrow_exact_ordinary",
                                                    "narrow_geometry_good"};
    require(proof.is_obj() && proof.size() == keys.size(), "Mindmap10 closed proof fields");
    for (std::size_t n = 0; n < keys.size(); ++n)
        require(proof.has(keys[n]), "Mindmap10 missing proof field");
    const auto yes = [&](std::string_view key) {
        require(proof.get(key).is_bool(), "Mindmap10 proof boolean");
        return proof.get(key).as_bool();
    };
    const auto number = [&](std::string_view key) {
        const auto value = proof.get(key);
        require(value.is_int() && value.as_int() >= 0 && value.as_int() <= 64,
                "Mindmap10 proof count");
        return static_cast<std::size_t>(value.as_int());
    };
    require(proof.get_string("source_facts_sha256") == mindmap10::kFactsSha &&
                proof.get_string("source_sha256") == row.source_sha256 &&
                number("foreign_assertions_certified") == 0u && !yes("native_report_input"),
            "Mindmap10 source-only proof boundary");
    const bool admitted = yes("local_ready");
    if (!admitted) {
        require(!row.errors.empty() && row.status[0] != "drawn" && row.status[2] != "drawn",
                "Mindmap10 failed proof earns no drawing");
        return false;
    }
    const auto cases = facts.get("cases");
    require(row.role_case_index < cases.size(), "Mindmap10 owned index");
    const auto fact = cases.at(row.role_case_index);
    require(fact.get_string("path") == row.path &&
                fact.get_string("source_sha256") == row.source_sha256 &&
                fact.get_string("complete_model_sha256") == row.role_model_sha &&
                fact.get_string("terminal_projection_sha256") == row.role_body_sha &&
                proof.get_string("complete_model_sha256") == row.role_model_sha &&
                proof.get_string("terminal_projection_sha256") == row.role_body_sha,
            "Mindmap10 complete source/Model/projection join");
    require(row.errors.empty() && row.status[0] == "drawn" && row.status[2] == "drawn" &&
                (row.status[1] == "drawn" || row.status[1] == "source_fallback") &&
                number("captures") == 5u && yes("captures_before_binding") &&
                yes("bound_after_captures") && yes("geometry_and_restoration_good") &&
                number("semantic_mutations_rejected") + number("physical_mutations_rejected") ==
                    fact.get("required_good_first_mutations").size(),
            "Mindmap10 mandatory complete geometry and good-first controls");
    require(row.status[1] == "drawn" ? yes("narrow_geometry_good") && !yes("narrow_exact_ordinary")
                                     : yes("narrow_exact_ordinary") && !yes("narrow_geometry_good"),
            "Mindmap10 narrow complete geometry or exact ordinary source");
    return true;
}

// Shapes4 owns its Work, source binding, mutation evidence and publications.
// Only the Official-local PixelAt capture/serialization primitives are reused.
namespace shapes4 = loom::test::mermaid_shapes4;
constexpr std::string_view kShapes4Contract = "MindmapShapes4/complete-original/v1";

std::optional<std::size_t> official_shapes4_selected(JsonVal facts, std::size_t global,
                                                     std::string_view path) {
    const auto cases = facts.get("cases");
    require(cases.is_arr() && cases.size() == 5u, "Shapes4 complete selection");
    for (std::size_t n = 0; n < cases.size(); ++n)
        if (cases.at(n).has("terminal_projection_sha256") &&
            cases.at(n).get_string("path") == path) {
            require(cases.at(n).get_int("manifest_global_ordinal") == static_cast<int64_t>(global),
                    "Shapes4 selection ordinal");
            return n;
        }
    return std::nullopt;
}

Observation run_official_shapes4(JsonVal facts, JsonVal manifest, JsonVal fixture,
                                 std::size_t index, std::ostream& diagnostics) {
    Observation row;
    row.path = fixture.get_string("path");
    row.track = "original";
    row.type = "mindmap";
    row.source_sha256 = fixture.get_string("sha256");
    row.expected = "drawn";
    row.reason = "RFC 0030 Shapes4 independently source-bound complete original";
    row.expected_states = {"drawn", "unresolved", "drawn"};
    row.decision_reasons.fill(row.reason);
    row.raw_active_schema = kShapes4Contract;
    row.raw_source_authority = shapes4::kFactsSha;
    row.role_case_index = index;
    json::JsonMutDoc report;
    auto record = report.object();
    shapes4::Work work;
    bool admitted = false, captured = false, bound = false, geometry = false,
         narrow_ordinary = false, narrow_geometry = false;
    std::size_t semantic = 0, physical = 0, captures = 0;
    record.set("path", row.path);
    record.set("source_sha256", row.source_sha256);
    try {
        const auto path = kRoot / "cases" / row.path;
        require(fs::is_regular_file(path) && fs::file_size(path) <= 65536u,
                "Shapes4 bounded complete source");
        const auto source = read_file(path);
        auto node = loom::ui::render_markdown(fenced(source, "mermaid", kSentinel));
        auto ordinary = loom::ui::render_markdown(fenced(source, "mermaid-source", kSentinel));
        node->ComputeRequirement();
        ordinary->ComputeRequirement();
        const int height = std::min(
            1040,
            std::max(40, std::max(node->requirement().min_y, ordinary->requirement().min_y) + 12));
        std::array<OfficialMindmapFrame, 5> frames;
        constexpr std::array<int, 5> widths{240, 40, 240, 240, 40};
        auto published = record.make_arr();
        for (std::size_t n = 0; n < frames.size(); ++n) {
            frames[n] = official_mindmap_capture(n < 3u ? node : ordinary, widths[n], height, work);
            ++captures;
            auto entry = record.make_obj();
            official_mindmap_publish(frames[n], entry, work);
            published.append(entry);
        }
        captured = captures == 5u;
        record.add("five_frames", published);
        record.set("captures_before_binding", captured);
        const auto fact = facts.get("cases").at(index);
        require(fact.get_string("path") == row.path &&
                    fact.get_string("source_sha256") == row.source_sha256,
                "Shapes4 consumer source identity");
        const auto model = shapes4::bind(facts, fact, source, manifest, work);
        bound = true;
        require(shapes4::validate_model(model, work).empty(), "Shapes4 valid complete Model");
        row.role_model_sha = fact.get_string("complete_model_sha256");
        row.role_body_sha = fact.get_string("terminal_projection_sha256");
        require(official_mindmap_surface(frames[0], frames[2]) &&
                    frames[0].foreground == frames[2].foreground,
                "Shapes4 restored color cells");
        require(!official_mindmap_surface(frames[0], frames[3]),
                "Shapes4 complete original still uses source");
        const bool narrow_source = official_mindmap_surface(frames[1], frames[4]);
        narrow_ordinary = narrow_source;
        for (const auto n : {0u, 1u, 2u}) {
            if (n == 1u && narrow_source) continue;
            const auto errors = shapes4::validate(frames[n].grid, model, kSentinel, work);
            require(errors.empty(), errors.empty() ? "Shapes4 geometry" : errors.front());
        }
        geometry = true;
        narrow_geometry = !narrow_source;
        const auto proof = shapes4::exercise(frames[0].grid, model, kSentinel, work);
        require(proof.errors.empty(),
                proof.errors.empty() ? "Shapes4 mutations" : proof.errors.front());
        semantic = proof.semantic;
        physical = proof.physical;
        require(semantic == (index == 4u ? 5u : 2u) && physical == (index == 4u ? 7u : 3u),
                "Shapes4 complete mutation responsibility");
        row.status = {"drawn", narrow_source ? "source_fallback" : "drawn", "drawn"};
        row.expected_states[1] = narrow_source ? "source" : "drawn";
        admitted = true;
    } catch (const std::exception& error) {
        row.errors.push_back(error.what());
        semantic = physical = 0;
    }
    record.set("admitted", admitted);
    record.set("semantic_mutations_rejected", static_cast<int64_t>(semantic));
    record.set("physical_mutations_rejected", static_cast<int64_t>(physical));
    record.set("work_remaining", static_cast<int64_t>(work.left));
    if (!row.errors.empty()) record.set("error", row.errors.front());
    report.set_root(record);
    diagnostics << report.to_string() << '\n';
    json::JsonMutDoc compact;
    auto proof = compact.object();
    proof.set("source_facts_sha256", shapes4::kFactsSha);
    proof.set("source_sha256", row.source_sha256);
    proof.set("complete_model_sha256", row.role_model_sha);
    proof.set("terminal_projection_sha256", row.role_body_sha);
    proof.set("captures", static_cast<int64_t>(captures));
    proof.set("captures_before_binding", captured);
    proof.set("bound_after_captures", bound);
    proof.set("geometry_and_restoration_good", geometry);
    proof.set("local_ready", admitted);
    proof.set("narrow_exact_ordinary", narrow_ordinary);
    proof.set("narrow_geometry_good", narrow_geometry);
    proof.set("semantic_mutations_rejected", static_cast<int64_t>(semantic));
    proof.set("physical_mutations_rejected", static_cast<int64_t>(physical));
    proof.set("foreign_assertions_certified", 0);
    proof.set("native_report_input", false);
    compact.set_root(proof);
    row.raw_source_constraints = compact.to_string();
    return row;
}

bool official_shapes4_owned(const Observation& row, JsonVal facts) {
    require(row.raw_active_schema == kShapes4Contract && row.track == "original" &&
                row.type == "mindmap" && row.raw_source_authority == shapes4::kFactsSha,
            "Shapes4 owned source identity");
    const auto info = json::parse(row.raw_source_constraints);
    require(info.has_value(), "Shapes4 proof JSON");
    const auto proof = info->root();
    constexpr std::array<std::string_view, 15> keys{"source_facts_sha256",
                                                    "source_sha256",
                                                    "complete_model_sha256",
                                                    "terminal_projection_sha256",
                                                    "captures",
                                                    "captures_before_binding",
                                                    "bound_after_captures",
                                                    "geometry_and_restoration_good",
                                                    "local_ready",
                                                    "semantic_mutations_rejected",
                                                    "physical_mutations_rejected",
                                                    "foreign_assertions_certified",
                                                    "native_report_input",
                                                    "narrow_exact_ordinary",
                                                    "narrow_geometry_good"};
    require(proof.is_obj() && proof.size() == keys.size(), "Shapes4 closed proof fields");
    for (std::size_t n = 0; n < keys.size(); ++n)
        require(proof.has(keys[n]), "Shapes4 missing proof field");
    const auto yes = [&](std::string_view key) {
        require(proof.get(key).is_bool(), "Shapes4 proof boolean");
        return proof.get(key).as_bool();
    };
    const auto number = [&](std::string_view key) {
        const auto value = proof.get(key);
        require(value.is_int() && value.as_int() >= 0 && value.as_int() <= 64,
                "Shapes4 proof count");
        return static_cast<std::size_t>(value.as_int());
    };
    require(proof.get_string("source_facts_sha256") == shapes4::kFactsSha &&
                proof.get_string("source_sha256") == row.source_sha256 &&
                number("foreign_assertions_certified") == 0u && !yes("native_report_input"),
            "Shapes4 source-only proof boundary");
    const bool admitted = yes("local_ready");
    if (!admitted) {
        require(!row.errors.empty() && row.status[0] != "drawn" && row.status[2] != "drawn",
                "Shapes4 failed proof earns no drawing");
        return false;
    }
    const auto cases = facts.get("cases");
    require(cases.is_arr() && cases.size() == 5u && row.role_case_index > 0u &&
                row.role_case_index < cases.size(),
            "Shapes4 owned index");
    const auto fact = cases.at(row.role_case_index);
    const auto selected = official_shapes4_selected(
        facts, static_cast<std::size_t>(fact.get_int("manifest_global_ordinal")), row.path);
    require(selected && *selected == row.role_case_index, "Shapes4 closed selected identity");
    require(fact.has("terminal_projection_sha256") && fact.get_string("path") == row.path &&
                fact.get_string("source_sha256") == row.source_sha256 &&
                fact.get_string("complete_model_sha256") == row.role_model_sha &&
                fact.get_string("terminal_projection_sha256") == row.role_body_sha &&
                proof.get_string("complete_model_sha256") == row.role_model_sha &&
                proof.get_string("terminal_projection_sha256") == row.role_body_sha,
            "Shapes4 complete source/Model/projection join");
    require(row.errors.empty() && row.status[0] == "drawn" && row.status[2] == "drawn" &&
                (row.status[1] == "drawn" || row.status[1] == "source_fallback") &&
                number("captures") == 5u && yes("captures_before_binding") &&
                yes("bound_after_captures") && yes("geometry_and_restoration_good") &&
                number("semantic_mutations_rejected") == (row.role_case_index == 4u ? 5u : 2u) &&
                number("physical_mutations_rejected") == (row.role_case_index == 4u ? 7u : 3u),
            "Shapes4 mandatory complete geometry and good-first controls");
    require(row.status[1] == "drawn" ? yes("narrow_geometry_good") && !yes("narrow_exact_ordinary")
                                     : yes("narrow_exact_ordinary") && !yes("narrow_geometry_good"),
            "Shapes4 narrow complete geometry or exact ordinary source");
    return true;
}

std::string official_shapes4_verdict(const std::vector<Observation>& rows, JsonVal facts) {
    json::JsonMutDoc document;
    auto root = document.object(), cases = root.make_arr();
    int ready = 0;
    std::size_t selected = 0;
    for (const auto& row : rows)
        if (row.raw_active_schema == kShapes4Contract) {
            auto value = root.make_obj();
            value.set("path", row.path);
            const auto proof = json::parse(row.raw_source_constraints);
            require(proof.has_value(), "Shapes4 compact proof");
            proof->root().iter_obj([&](JsonVal key, JsonVal field) {
                value.add(key.as_str(), document.copy_val(field));
            });
            ready += official_shapes4_owned(row, facts);
            cases.append(value);
            ++selected;
        }
    require(selected == 4u, "Shapes4 compact exactly four complete originals");
    root.set("schema", "mindmap_shapes4_official_local_verdict/v1");
    root.set("source_facts_sha256", shapes4::kFactsSha);
    root.set("local_ready", ready);
    root.add("cases", cases);
    document.set_root(root);
    return document.to_string();
}

// Requirement11 formal-matrix support is deliberately file-local.  The
// existing header owns only private typed decode and Grid geometry; this file
// owns source selection, Work ledgers, publication, report rows, and counting.
using relations_oracle::class481_keys;
using relations_oracle::class481_require;
using relations_oracle::class481_resource;
using relations_oracle::D;
using relations_oracle::Glyph;
using relations_oracle::glyph;
using relations_oracle::Grid;
using relations_oracle::kBits;
using relations_oracle::L;
using relations_oracle::Label;
using relations_oracle::label_blocks;
using relations_oracle::move;
using relations_oracle::outlines;
using relations_oracle::Point;
using relations_oracle::R;
using relations_oracle::Rect;
using relations_oracle::U;
using relations_oracle::Work;

// The private Requirement header expects this established test-local publisher.
// It serializes actual Grid cells as reproduction evidence only; Grid cells,
// never rows reconstructed from JSON, remain the geometry authority.
void official285_publish_frame(const Grid& grid, std::string_view name, json::JsonMutVal root,
                               json::JsonMutVal item, Work& work) {
    work.spend(8u * sizeof(std::size_t) + sizeof(std::string) + 128u);
    class481_require(grid.width > 0 && grid.width <= 240 && grid.height > 0 &&
                         grid.height <= 1040 &&
                         grid.cells.size() == static_cast<std::size_t>(grid.width * grid.height),
                     "Requirement11 publication Grid bounds");
    std::size_t raw = 0u, escaped = static_cast<std::size_t>(grid.height) * 3u + 2u;
    for (const auto& cell : grid.cells) {
        work.spend(cell.size() + 1u);
        raw += cell.size();
        for (const unsigned char c : cell)
            escaped += c == '"' || c == '\\' || c == '\b' || c == '\f' || c == '\n' || c == '\r' ||
                               c == '\t'
                           ? 2u
                           : (c < 0x20u ? 6u : 1u);
    }
    work.spend(grid.cells.size() * 2u + raw * 4u + escaped * 3u +
               static_cast<std::size_t>(grid.height) *
                   (sizeof(std::string) + sizeof(std::size_t) + 128u) +
               1024u);
    auto rows = root.make_arr();
    for (int y = 0; y < grid.height; ++y) {
        std::size_t bytes = 0u;
        for (int x = 0; x < grid.width; ++x)
            bytes += grid.at({x, y}).size();
        std::string row;
        row.reserve(bytes);
        for (int x = 0; x < grid.width; ++x)
            row.append(grid.at({x, y}));
        rows.append(root.make_str(row));
    }
    item.add(name, rows);
}

#include "mermaid_requirement11_oracle.hpp"
namespace r11 = requirement11_native;

constexpr std::string_view kOfficialRequirement11Contract = "Requirement11NativeWholeScene/v1";
constexpr std::string_view kOfficialRequirement11Authority =
    "immutable_requirement11_source_models_and_envelopes";
constexpr std::string_view kOfficialRequirement11AuthoritySha =
    "b67e4f54976289707df038317734e8df02c4029c9f466ef92ebaabb36f56ba66";
constexpr std::string_view kOfficialRequirement11ModelsSha =
    "fbf7b6b432c8185f046e95e43fa6da83d3a15b0ce7a39daba4a7ad334ae7fd2d";
constexpr std::string_view kOfficialRequirement11EnvelopesSha =
    "2731b7e8e35e7cb85bf05e7b9b5e9ba3925d1b01172020ee979c8a0b41578f58";
constexpr std::size_t kOfficialRequirement11ReportReserve = 65536u;

struct Requirement11OfficialPin {
    std::size_t selection, ordinal, global, bytes, semantic, physical;
    std::string_view path, sha, git_blob;
};

constexpr std::array<Requirement11OfficialPin, 11> kOfficialRequirement11Pins{{
    {0u, 12u, 1400u, 1566u, 2u, 4u,
     "requirement/nightly-check/a-not-so-simple-requirement-diagram-default.mmd",
     "0d663c6f705c243b7938a610b414942ca159053a8ef3e4dd0338b5cd3e81528e",
     "2a81033126285761b9e04feeb4339fafc88c8ad8"},
    {1u, 18u, 1406u, 372u, 2u, 3u,
     "requirement/nightly-check/a-requirement-diagram-with-bt-direction-default.mmd",
     "4c5bd8f6e4d37eb38b652b44910fbcd19d341947439b5198dce072d98dba7062",
     "3bbe027f5ce657023d8ce8402c145c610cb3cae0"},
    {2u, 21u, 1409u, 207u, 1u, 1u,
     "requirement/nightly-check/a-requirement-diagram-with-empty-information-default.mmd",
     "29e1dbfc28307c32d863b8f87912d6009ed98a313a1fc19eb0fa2449dadc8321",
     "e06490c3505e36586ba29716d724a9f794a2ece7"},
    {3u, 24u, 1412u, 372u, 2u, 3u,
     "requirement/nightly-check/a-requirement-diagram-with-lr-direction-default.mmd",
     "f950ce72b3330c7d431c25159856576c6203f01cd7ea74f84ab37d6b681f8cd3",
     "09dfddd2ca590bd562aa5c131b2dc6619c345399"},
    {4u, 27u, 1415u, 372u, 2u, 3u,
     "requirement/nightly-check/a-requirement-diagram-with-rl-direction-default.mmd",
     "c80dcf6db0a78fd625b0518119ba53558fde7542866c72febebfdea924b29740",
     "7a9fe520852c4c96b87fb9c7292e3235b02c22e0"},
    {5u, 30u, 1418u, 350u, 2u, 3u,
     "requirement/nightly-check/a-requirement-diagram-with-tb-direction-default.mmd",
     "2c597fd583728204b1454c54f9645449c639e533b0c425085933360a0a86ecc6",
     "2bd1b07cbd74f29bb78279c6c123f36808fae5eb"},
    {6u, 33u, 1421u, 334u, 2u, 3u,
     "requirement/nightly-check/a-simple-requirement-diagram-default.mmd",
     "33a883e2a8536ae5d42d3baa5c7e666602ea81435ae840b8e13c43ac5a5aab20",
     "f4b2a467f109fcc9a5d078dc48bb7046009b177c"},
    {7u, 96u, 1484u, 265u, 1u, 1u,
     "requirement/nightly-check/requirements-and-elements-with-and-without-information-default.mmd",
     "de9241221c9e39174077a3ae6cd94f8b90e06efeb41388eed27ae5b6b0affd73",
     "cbc9de36286ac32cb086a5de2bd236ebb12451ed"},
    {8u, 99u, 1487u, 356u, 1u, 1u,
     "requirement/nightly-check/requirements-and-elements-with-long-and-short-text-default.mmd",
     "f1a7236f3f3ac0e6bf621046f1cced10c65535bdb7d265226999888a3278136c",
     "82de1672b89c463c0683e68c9dc462a80c371506"},
    {9u, 111u, 1499u, 371u, 1u, 1u,
     "requirement/nightly-check/requirements-and-elements-with-quoted-text-for-spaces-default.mmd",
     "1fbeae16c5c88aacc7dca198913f9e0278ef876c123274b6cb286035eb74d8b5",
     "ebb3dcfa6433bb365f2523bb5364b71f9cce4cb1"},
    {10u, 168u, 1556u, 909u, 2u, 4u, "requirement/pr-check/sample.mmd",
     "b8d798507c5cf892622bd855d64770381eab6d3aa4e7c3c21a4e8d7a60eefa82",
     "afaf628dc7e97897325513f30b11013a38e0cb1a"},
}};

struct Requirement11Documents {
    json::JsonDoc authority, models, envelopes;
};

struct Requirement11Progress {
    std::size_t captures = 0u, published = 0u, semantic_rejected = 0u, physical_rejected = 0u;
    int height = 0;
    bool captured_before_binding = false, bound = false, decoded = false, wide_source = false,
         narrow_ordinary = false;
    bool wide_good = false, narrow_good = false, restored_good = false, restored_equal = false,
         good_baseline = false, mutated = false, exception = false, resource = false,
         admitted = false;
    const char* phase = "acquire";
    std::array<std::optional<std::size_t>, 10> remaining{};
};

struct Requirement11Counts {
    std::size_t visited = 0u, observed = 0u, published = 0u, bound = 0u, decoded = 0u,
                admitted = 0u, mutated = 0u, errors = 0u, resources = 0u;
};

std::optional<std::size_t> requirement11_selected_index(std::size_t global, std::string_view path) {
    for (std::size_t i = 0u; i < kOfficialRequirement11Pins.size(); ++i)
        if (kOfficialRequirement11Pins[i].global == global &&
            kOfficialRequirement11Pins[i].path == path)
            return i;
    return std::nullopt;
}

void requirement11_asset_pin(JsonVal row, const Requirement11OfficialPin& pin, Work& setup) {
    setup.spend(1024u + pin.path.size() * 8u);
    require(row.get("ordinal").is_int() &&
                row.get_int("ordinal") == static_cast<int64_t>(pin.ordinal) &&
                row.get("path").is_str() && row.get_string("path") == pin.path &&
                row.get("source_bytes").is_int() &&
                row.get_int("source_bytes") == static_cast<int64_t>(pin.bytes) &&
                row.get("source_sha256").is_str() && row.get_string("source_sha256") == pin.sha,
            "Requirement11 immutable source/Model envelope pin");
}

Requirement11Documents requirement11_documents(Work& setup) {
    const auto root = kNativeRoot / "requirement11";
    const std::array<std::pair<std::string_view, std::string_view>, 3> assets{{
        {"requirement11_native_authority.json", kOfficialRequirement11AuthoritySha},
        {"requirement11_models.proposed.json", kOfficialRequirement11ModelsSha},
        {"requirement11_source_envelopes.proposed.json", kOfficialRequirement11EnvelopesSha},
    }};
    setup.spend(3u * sizeof(json::JsonDoc) + 3u * 4096u + 65536u);
    std::array<json::JsonDoc, 3> parsed;
    for (std::size_t i = 0u; i < assets.size(); ++i) {
        const auto path = root / assets[i].first;
        projection_regular_file(root, path);
        const auto raw = r11::load(path, setup);
        require(raw.size() <= 65536u && digest(raw) == assets[i].second,
                "Requirement11 immutable source asset SHA");
        auto document = json::parse(raw);
        require(document.has_value(), "Requirement11 immutable source asset JSON");
        parsed[i] = std::move(*document);
    }
    const auto authority = parsed[0].root(), models = parsed[1].root(),
               envelopes = parsed[2].root();
    require(
        authority.get("schema").is_str() &&
            authority.get_string("schema") == "requirement11_native_test_authority/v1" &&
            authority.get("cases").is_arr() &&
            authority.get("cases").size() == kOfficialRequirement11Pins.size() &&
            models.get("schema").is_str() &&
            models.get_string("schema") == "requirement11_source_semantic_model/v1" &&
            models.get("models").is_arr() &&
            models.get("models").size() == kOfficialRequirement11Pins.size() &&
            envelopes.get("schema").is_str() &&
            envelopes.get_string("schema") == "requirement11_owner_private_source_envelopes/v1" &&
            envelopes.get("graph_models_raw_sha256").is_str() &&
            envelopes.get_string("graph_models_raw_sha256") == kOfficialRequirement11ModelsSha &&
            envelopes.get("cases").is_arr() &&
            envelopes.get("cases").size() == kOfficialRequirement11Pins.size(),
        "Requirement11 immutable complete asset schemas");
    for (std::size_t i = 0u; i < kOfficialRequirement11Pins.size(); ++i) {
        const auto& pin = kOfficialRequirement11Pins[i];
        const auto native = authority.get("cases").at(i);
        require(native.get("ordinal").is_int() &&
                    native.get_int("ordinal") == static_cast<int64_t>(pin.ordinal) &&
                    native.get("bytes").is_int() &&
                    native.get_int("bytes") == static_cast<int64_t>(pin.bytes) &&
                    native.get("sha256").is_str() && native.get_string("sha256") == pin.sha,
                "Requirement11 immutable native authority tuple");
        requirement11_asset_pin(models.get("models").at(i), pin, setup);
        requirement11_asset_pin(envelopes.get("cases").at(i), pin, setup);
    }
    return {std::move(parsed[0]), std::move(parsed[1]), std::move(parsed[2])};
}

void requirement11_error(Observation& row, std::string_view message) {
    if (row.errors.size() < 6u) row.errors.emplace_back(message.substr(0u, 192u));
}

std::string requirement11_info(const Requirement11Progress& p, const Observation& row,
                               const Requirement11OfficialPin& pin, std::size_t remaining,
                               std::size_t initial = r11::kSceneLimit) {
    json::JsonMutDoc doc;
    auto info = doc.object();
    info.set("selection_index", static_cast<int64_t>(pin.selection));
    info.set("requirement_ordinal", static_cast<int64_t>(pin.ordinal));
    info.set("manifest_global_ordinal", static_cast<int64_t>(pin.global));
    info.set("path", pin.path);
    info.set("source_sha256", row.source_sha256);
    info.set("git_blob", pin.git_blob);
    info.set("models_asset_sha256", kOfficialRequirement11ModelsSha);
    info.set("envelopes_asset_sha256", kOfficialRequirement11EnvelopesSha);
    info.set("source_authority_sha256", kOfficialRequirement11AuthoritySha);
    info.set("captures", static_cast<int64_t>(p.captures));
    info.set("published_frames", static_cast<int64_t>(p.published));
    info.set("captures_before_binding", p.captured_before_binding);
    info.set("source_bound_after_captures", p.bound);
    info.set("Model_decoded_after_binding", p.decoded);
    info.set("wide_source_fallback", p.wide_source);
    info.set("narrow_exact_ordinary", p.narrow_ordinary);
    info.set("wide_geometry_good", p.wide_good);
    info.set("narrow_geometry_good", p.narrow_good);
    info.set("restored_geometry_good", p.restored_good);
    info.set("restored_cells_equal", p.restored_equal);
    info.set("good_baseline_once", p.good_baseline);
    info.set("semantic_mutations_expected", static_cast<int64_t>(pin.semantic));
    info.set("semantic_mutations_rejected", static_cast<int64_t>(p.semantic_rejected));
    info.set("physical_mutations_expected", static_cast<int64_t>(pin.physical));
    info.set("physical_mutations_rejected", static_cast<int64_t>(p.physical_rejected));
    info.set("exception", p.exception);
    info.set("resource_exhausted", p.resource);
    info.set("foreign_assertions_certified", 0);
    info.set("whole_source_admitted", p.admitted);
    info.set("phase", p.phase);
    info.set("initial_budget", static_cast<int64_t>(initial));
    info.set("work_remaining", static_cast<int64_t>(remaining));
    auto ledger = info.make_arr();
    for (const auto& value : p.remaining) {
        if (value)
            ledger.append(ledger.make_num(static_cast<int64_t>(*value)));
        else
            ledger.append(ledger.make_null());
    }
    info.add("phase_remaining", ledger);
    info.set("error",
             row.errors.empty() ? std::string_view{""} : std::string_view(row.errors.front()));
    doc.set_root(info);
    const auto text = doc.to_string();
    require(text.size() <= 4096u, "Requirement11 prepaid owned metadata bound");
    return text;
}

void requirement11_accumulate_owned(JsonVal info, const Observation& row, Requirement11Counts& out,
                                    const Requirement11Documents& docs) {
    constexpr std::array<std::string_view, 34> keys{{"selection_index",
                                                     "requirement_ordinal",
                                                     "manifest_global_ordinal",
                                                     "path",
                                                     "source_sha256",
                                                     "git_blob",
                                                     "models_asset_sha256",
                                                     "envelopes_asset_sha256",
                                                     "source_authority_sha256",
                                                     "captures",
                                                     "published_frames",
                                                     "captures_before_binding",
                                                     "source_bound_after_captures",
                                                     "Model_decoded_after_binding",
                                                     "wide_source_fallback",
                                                     "narrow_exact_ordinary",
                                                     "wide_geometry_good",
                                                     "narrow_geometry_good",
                                                     "restored_geometry_good",
                                                     "restored_cells_equal",
                                                     "good_baseline_once",
                                                     "semantic_mutations_expected",
                                                     "semantic_mutations_rejected",
                                                     "physical_mutations_expected",
                                                     "physical_mutations_rejected",
                                                     "exception",
                                                     "resource_exhausted",
                                                     "foreign_assertions_certified",
                                                     "whole_source_admitted",
                                                     "phase",
                                                     "initial_budget",
                                                     "work_remaining",
                                                     "phase_remaining",
                                                     "error"}};
    require(info.is_obj() && info.size() == keys.size() &&
                std::ranges::all_of(keys, [&](std::string_view key) { return info.has(key); }),
            "Requirement11 owned exact report fields");
    auto number = [&](std::string_view key, std::size_t maximum) {
        const auto v = info.get(key);
        require(v.is_int() && v.as_int() >= 0 && static_cast<std::size_t>(v.as_int()) <= maximum,
                "Requirement11 owned report integer");
        return static_cast<std::size_t>(v.as_int());
    };
    auto yes = [&](std::string_view key) {
        const auto v = info.get(key);
        require(v.is_bool(), "Requirement11 owned report bool");
        return v.as_bool();
    };
    const auto index = number("selection_index", kOfficialRequirement11Pins.size() - 1u);
    const auto& pin = kOfficialRequirement11Pins[index];
    require(
        row.raw_active_schema == kOfficialRequirement11Contract &&
            row.raw_source_authority == kOfficialRequirement11Authority &&
            row.track == "original" && row.type == "requirement" && row.role_case_index == index &&
            row.path == pin.path && row.source_sha256 == pin.sha &&
            row.role_model_sha == kOfficialRequirement11ModelsSha && row.role_body_sha == pin.sha,
        "Requirement11 owned row/source identity");
    require(number("requirement_ordinal", 168u) == pin.ordinal &&
                number("manifest_global_ordinal", 2144u) == pin.global &&
                info.get("path").is_str() && info.get_string("path") == pin.path &&
                info.get("source_sha256").is_str() && info.get_string("source_sha256") == pin.sha &&
                info.get("git_blob").is_str() && info.get_string("git_blob") == pin.git_blob,
            "Requirement11 owned static source tuple");
    require(info.get("models_asset_sha256").is_str() &&
                info.get_string("models_asset_sha256") == kOfficialRequirement11ModelsSha &&
                info.get("envelopes_asset_sha256").is_str() &&
                info.get_string("envelopes_asset_sha256") == kOfficialRequirement11EnvelopesSha &&
                info.get("source_authority_sha256").is_str() &&
                info.get_string("source_authority_sha256") == kOfficialRequirement11AuthoritySha &&
                number("foreign_assertions_certified", 0u) == 0u,
            "Requirement11 owned immutable authority pins");
    const bool captured = yes("captures_before_binding"),
               bound = yes("source_bound_after_captures"),
               decoded = yes("Model_decoded_after_binding"),
               wide_source = yes("wide_source_fallback"), ordinary = yes("narrow_exact_ordinary"),
               wide = yes("wide_geometry_good"), narrow = yes("narrow_geometry_good"),
               restored = yes("restored_geometry_good"), equal = yes("restored_cells_equal"),
               baseline = yes("good_baseline_once"), exception = yes("exception"),
               resource = yes("resource_exhausted"), admitted = yes("whole_source_admitted");
    const auto captures = number("captures", 5u), published = number("published_frames", 5u),
               semantic_expected = number("semantic_mutations_expected", 2u),
               semantic = number("semantic_mutations_rejected", 2u),
               physical_expected = number("physical_mutations_expected", 4u),
               physical = number("physical_mutations_rejected", 4u);
    require(captures <= 5u && published <= 5u &&
                (!captured || (captures == 5u && published == 5u)) && (!bound || captured) &&
                (!decoded || bound),
            "Requirement11 captures/binding/decode order");
    require(semantic_expected == pin.semantic && physical_expected == pin.physical,
            "Requirement11 source-derived exact mutation applicability");
    require(number("initial_budget", r11::kSceneLimit) == r11::kSceneLimit &&
                number("work_remaining", r11::kSceneLimit) <= r11::kSceneLimit &&
                info.get("phase").is_str() && info.get("phase").as_str().size() <= 48u &&
                info.get("error").is_str() && info.get("error").as_str().size() <= 192u,
            "Requirement11 owned budget/error bounds");
    const auto ledger = info.get("phase_remaining");
    require(ledger.is_arr() && ledger.size() == 10u, "Requirement11 owned phase ledger shape");
    std::size_t prior = r11::kSceneLimit;
    ledger.iter([&](JsonVal value) {
        if (value.is_null()) return;
        require(value.is_int() && value.as_int() >= 0 &&
                    static_cast<std::size_t>(value.as_int()) <= prior,
                "Requirement11 owned monotonic phase ledger");
        prior = static_cast<std::size_t>(value.as_int());
    });
    const auto model = docs.models.root().get("models").at(index),
               envelope = docs.envelopes.root().get("cases").at(index);
    require(model.get("ordinal").is_int() &&
                model.get_int("ordinal") == static_cast<int64_t>(pin.ordinal) &&
                model.get_string("path") == pin.path &&
                model.get_int("source_bytes") == static_cast<int64_t>(pin.bytes) &&
                model.get_string("source_sha256") == pin.sha && envelope.get("ordinal").is_int() &&
                envelope.get_int("ordinal") == static_cast<int64_t>(pin.ordinal) &&
                envelope.get_string("path") == pin.path &&
                envelope.get_int("source_bytes") == static_cast<int64_t>(pin.bytes) &&
                envelope.get_string("source_sha256") == pin.sha,
            "Requirement11 report references exact immutable Model/envelope tuple");
    // The narrow outcome has exactly two disjoint valid states.  A fallback
    // cannot be relabelled as a drawing, and a complete narrow drawing cannot
    // be relabelled as fallback to satisfy an independent disjunction.
    const bool narrow_state = ordinary ? (!narrow && row.status[1] == "source_fallback")
                                       : (narrow && row.status[1] == "drawn");
    if (admitted)
        require(captured && bound && decoded && !wide_source && wide && narrow_state && restored &&
                    equal && baseline && semantic == semantic_expected &&
                    physical == physical_expected && !exception && !resource &&
                    row.errors.empty() && row.status[0] == "drawn" && row.status[2] == "drawn",
                "Requirement11 per-state proof cannot bypass whole admission");
    out.observed += captured;
    out.published += published == 5u;
    out.bound += bound;
    out.decoded += decoded;
    out.admitted += admitted;
    out.mutated += baseline && semantic == semantic_expected && physical == physical_expected;
    out.errors += !row.errors.empty();
    out.resources += resource;
}

Requirement11Counts requirement11_counts(const std::vector<Observation>& rows,
                                         const Requirement11Documents* docs) {
    Requirement11Counts out;
    std::array<bool, kOfficialRequirement11Pins.size()> seen{};
    for (const auto& row : rows) {
        if (row.raw_active_schema != kOfficialRequirement11Contract) continue;
        require(docs != nullptr && row.role_case_index < seen.size() && !seen[row.role_case_index],
                "Requirement11 duplicate or unknown source consumption");
        seen[row.role_case_index] = true;
        ++out.visited;
        require(row.raw_source_constraints.size() <= 4096u, "Requirement11 owned metadata size");
        const auto parsed = json::parse(row.raw_source_constraints);
        require(parsed.has_value(), "Requirement11 owned metadata JSON");
        requirement11_accumulate_owned(parsed->root(), row, out, *docs);
    }
    if (out.visited)
        require(out.visited == seen.size() &&
                    std::ranges::all_of(seen, [](bool used) { return used; }),
                "Requirement11 missing complete source consumption");
    return out;
}

Observation run_official_requirement11_case(
    JsonVal fixture, std::size_t global, std::size_t index, const Requirement11Documents& docs,
    std::array<bool, kOfficialRequirement11Pins.size()>& consumed, Work& work,
    std::ostream& diagnostics, std::size_t observation_size, std::size_t observation_capacity) {
    // `require_semantic_rejected` copies its Scene before it spends Work.  The
    // existing fixed report reserve pays the bounded one-or-two semantic copies
    // up front; changing it would alter the frozen source-local 128M tariff.
    const auto& pin = kOfficialRequirement11Pins.at(index);
    work.spend(kOfficialRequirement11ReportReserve + 8u * sizeof(Observation) +
               8u * sizeof(Requirement11Progress) + 8u * sizeof(r11::Frames) + 8192u);
    Observation row;
    row.errors.reserve(6u);
    Requirement11Progress progress;
    row.raw_active_schema = kOfficialRequirement11Contract;
    row.raw_source_authority = kOfficialRequirement11Authority;
    row.track = "original";
    row.type = "requirement";
    row.expected = "drawn";
    row.expected_states = {"drawn", "drawn_or_exact_source", "drawn"};
    row.reason = "Complete Requirement terminal whole-scene contract; foreign assertions pending";
    row.decision_reasons.fill("Five PixelAt captures, source binding, complete Model geometry, "
                              "restore, and exact source-derived mutants");
    row.decision_reasons[1] = "Complete40 geometry OR exact ordinary source lifecycle";
    row.role_case_index = index;
    row.role_case_id = "requirement/" + std::to_string(pin.ordinal);
    row.role_model_sha = std::string(kOfficialRequirement11ModelsSha);
    row.role_body_sha = std::string(pin.sha);
    row.projection_baseline = projection_canonical(fixture.get("raw"));
    json::JsonMutDoc diagnostic;
    auto data = diagnostic.object();
    try {
        require(observation_size <= 2738u && observation_capacity <= 8192u && !consumed[index],
                "Requirement11 bounded unique observation slot");
        // The caller appends this row after return.  Charge the exact local
        // vector slot/relocation envelope before any source/capture allocation.
        static_assert(std::is_nothrow_move_constructible_v<Observation>);
        const auto slots = observation_size == observation_capacity
                               ? std::max(2u * observation_capacity, observation_size + 1u)
                               : 0u;
        stage_b_product(work, slots + (slots ? observation_size : 0u) + 1u,
                        2u * sizeof(Observation) + 16u);
        consumed[index] = true;
        const auto path = fixture.get_string("path");
        require(global == pin.global && path == pin.path && fixture.get("bytes").is_int() &&
                    fixture.get_int("bytes") == static_cast<int64_t>(pin.bytes) &&
                    fixture.get_string("sha256") == pin.sha &&
                    fixture.get_string("git_blob") == pin.git_blob,
                "Requirement11 official manifest pin");
        row.path = path;
        work.spend(pin.bytes * 4u + 4096u);
        const auto source = read_file(kRoot / "cases" / row.path);
        require(source.size() == pin.bytes && digest(source) == pin.sha,
                "Requirement11 actual official source bytes");
        row.source_sha256 = digest(source);
        data.set("kind", "official_requirement11_actual_scene/v1");
        data.set("selection_index", static_cast<int64_t>(index));
        data.set("fixture", row.path);
        data.set("source_sha256", row.source_sha256);
        progress.phase = "five_actual_Screen_PixelAt_captures";
        const auto frames = r11::observe(source, work);
        progress.captures = 5u;
        progress.captured_before_binding = true;
        progress.height = frames.wide.height;
        progress.remaining[0] = work.left;
        progress.phase = "publish";
        r11::publish(frames, data, data, work);
        progress.published = 5u;
        progress.remaining[1] = work.left;
        r11::lifecycle(frames, work);
        progress.phase = "source_binding";
        const auto body = r11::bind_envelope(docs.envelopes.root().get("cases").at(index), source,
                                             static_cast<int>(pin.ordinal), work);
        require(body.starts_with("requirementDiagram\n"), "Requirement11 bound envelope body");
        progress.bound = true;
        progress.remaining[2] = work.left;
        progress.phase = "Model_decode";
        const auto scene = r11::decode_scene(docs.models.root().get("models").at(index), work);
        require(scene.ordinal == static_cast<int>(pin.ordinal) && scene.source_sha == pin.sha,
                "Requirement11 decoded source Model identity");
        progress.decoded = true;
        progress.remaining[3] = work.left;
        progress.phase = "wide_geometry";
        progress.wide_source = frames.wide == frames.source240;
        require(!progress.wide_source,
                "Requirement11 mandatory complete source fell back at 240 columns");
        r11::Geometry wide;
        const auto wide_errors = r11::geometry(frames.wide, scene, work, &wide);
        require(wide_errors.empty(), wide_errors.empty() ? "" : wide_errors.front());
        progress.wide_good = true;
        progress.good_baseline = true;
        progress.remaining[4] = work.left;
        row.status[0] = "drawn";
        progress.phase = "narrow";
        if (frames.narrow == frames.source40) {
            progress.narrow_ordinary = true;
            row.status[1] = "source_fallback";
        } else {
            const auto narrow_errors = r11::geometry(frames.narrow, scene, work);
            require(narrow_errors.empty(), narrow_errors.empty() ? "" : narrow_errors.front());
            progress.narrow_good = true;
            row.status[1] = "drawn";
        }
        progress.remaining[5] = work.left;
        progress.phase = "restored_geometry";
        const auto restored_errors = r11::geometry(frames.restored, scene, work);
        require(restored_errors.empty(), restored_errors.empty()
                                             ? "Requirement11 restored geometry differs"
                                             : restored_errors.front());
        progress.restored_good = true;
        progress.restored_equal = frames.wide == frames.restored;
        require(progress.restored_equal, "Requirement11 same Markdown Node restore differs");
        row.status[2] = "drawn";
        progress.remaining[6] = work.left;
        progress.phase = "mutations";
        r11::require_semantic_rejected(
            scene, [](r11::Scene& bad) { bad.nodes.front().ref.name.clear(); }, work,
            "Requirement11 logical-name semantic mutant");
        ++progress.semantic_rejected;
        if (!scene.edges.empty()) {
            r11::require_semantic_rejected(
                scene, [](r11::Scene& bad) { bad.edges.front().order = 99; }, work,
                "Requirement11 relation-order semantic mutant");
            ++progress.semantic_rejected;
        }
        const auto reject_physical = [&](auto mutate, std::string_view name) {
            require(progress.good_baseline, "Requirement11 good-first actual wide geometry");
            work.spend(frames.wide.cells.size() * (2u * sizeof(std::string) + 8u) + 4096u);
            auto bad = frames.wide;
            mutate(bad, wide);
            const auto rejected = r11::geometry(bad, scene, work);
            require(!rejected.empty(), std::string(name));
            ++progress.physical_rejected;
        };
        if (!wide.circles.empty())
            reject_physical(
                [](Grid& bad, const r11::Geometry& evidence) {
                    bad.put(evidence.circles.front().marker, ">");
                },
                "Requirement11 contains circle-plus physical mutant");
        if (!wide.arrows.empty())
            reject_physical(
                [](Grid& bad, const r11::Geometry& evidence) {
                    bad.put(evidence.arrows.front().marker, "◆");
                },
                "Requirement11 target open-arrow physical mutant");
        if (!wide.edge_paths.empty())
            reject_physical(
                [](Grid& bad, const r11::Geometry& evidence) {
                    bad.put(bad.point(evidence.edge_paths.front().front()), "X");
                },
                "Requirement11 route-stroke physical mutant");
        reject_physical(
            [](Grid& bad, const r11::Geometry& evidence) {
                Point point;
                require(r11::first_blank(bad, evidence, point),
                        "Requirement11 orphan-ink blank witness");
                bad.put(point, "X");
            },
            "Requirement11 orphan-ink physical mutant");
        require(progress.semantic_rejected == pin.semantic &&
                    progress.physical_rejected == pin.physical,
                "Requirement11 exact source-derived mutation set");
        progress.mutated = true;
        progress.remaining[7] = work.left;
        progress.phase = "complete";
        progress.admitted = true;
    } catch (const std::exception& error) {
        progress.exception = true;
        const std::string_view message(error.what());
        progress.resource = work.left == 0u || class481_resource(message);
        requirement11_error(row, message);
        progress.admitted = false;
    }
    row.raw_source_constraints = requirement11_info(progress, row, pin, work.left);
    data.set("contract", kOfficialRequirement11Contract);
    data.set("actual", row.raw_source_constraints);
    data.set("foreign_assertions_certified", 0);
    diagnostic.set_root(data);
    diagnostics << diagnostic.to_string() << '\n';
    require(static_cast<bool>(diagnostics), "Requirement11 actual UTF8 Grid evidence write");
    return row;
}
// Pie7 formal matrix support is deliberately file-local. The header owns only
// the pure color-cell/Pie geometry; this owner controls sources, ledgers and
// Official provenance.
namespace pie7 = loom::test::mermaid_chart;
constexpr std::string_view kOfficialPie7Contract = "Pie7NativeWholeScene/v1";
constexpr std::string_view kOfficialPie7Authority = "immutable_pie7_source_models";
constexpr std::string_view kOfficialPie7AuthoritySha =
    "a3fb35a55dc325111e76ffa8a9b463c77eb89f9183e11047bd798e97f6b805df";
constexpr std::size_t kOfficialPie7SceneLimit = 128000000u, kOfficialPie7SetupLimit = 8000000u,
                      kOfficialPie7ReportReserve = 65536u;

struct OfficialPie7Pin {
    std::size_t selection, ordinal, global, bytes, semantic, physical, lifecycle;
    std::string_view path, sha, git_blob, model_sha;
};

constexpr std::array<OfficialPie7Pin, 7> kOfficialPie7Pins{
    {{0u, 11u, 1346u, 290u, 3u, 1u, 2u,
      "pie/nightly-check/should-render-a-simple-pie-diagram-with-long-labels.mmd",
      "9b515d1c744ef17a69899d6025c45737576a71d373cf0aa60541e7ad949547d5",
      "c43f24cc3c451b8b252981ecc511a5646d37b0a0",
      "d3023db0c6866b7c34cfdc6ae85774dfe6641b6119040789cc7b7008c7edd4ee"},
     {1u, 12u, 1347u, 382u, 5u, 12u, 2u,
      "pie/nightly-check/"
      "should-render-pie-slices-only-for-non-zero-values-but-shows-all-legends.mmd",
      "b78c67f92fed5ef813da2ec3ab3a290670abe324f7a49e39b2160b62e4f5bcaf",
      "ccd8f918c1232277d2f5dcfc8076f372ad596d67",
      "b534c6523ee5561459329d1229e4f3d1bb73d4956769e40e28521657c2dcf48f"},
     {2u, 18u, 1353u, 361u, 3u, 1u, 2u,
      "pie/pr-check/should-render-a-pie-diagram-with-a-long-title-without-clipping.mmd",
      "092b8799be12b4198b16c3bf1f8d69f1c64b8f747823e6e47b06c5d231a61484",
      "78f68fc0a1658d18d91e3b42fed389635cb0ddf9",
      "bf5413542114e5f4cd91acfb687bf13e8bd39f49438e8635d91673873fab9efc"},
     {3u, 20u, 1355u, 243u, 3u, 1u, 2u,
      "pie/pr-check/should-render-a-pie-diagram-with-showdata.mmd",
      "39d37e1d901b67c7ff927facdf1a9c91e011ccae7bb54cf3bf4265672ed7bc8e",
      "7359c0bc58aacc341c6b8760d1a4e65339d43c4e",
      "2082ad690551b811dc764ac7cf7f96067557875ca88b5a4a4315222b1e8f1a66"},
     {4u, 21u, 1356u, 280u, 3u, 1u, 2u,
      "pie/pr-check/should-render-a-simple-pie-diagram-with-capital-letters-for-labels.mmd",
      "7e94c33e07afb9658b8b92ba8c0b44f6bfee17189c7f79f9404c76a8e60393f6",
      "8d0851752ce29b5b0bb8dfabca5fa10e016ad569",
      "abc9956be8717ebc8cba8e5df83609d857f2d54f84227d6a02f4438167f44283"},
     {5u, 22u, 1357u, 246u, 3u, 1u, 2u, "pie/pr-check/should-render-a-simple-pie-diagram.mmd",
      "41e12fdff5e90954b0375a3bead342a0336cd38cac5f0a94c2d6fc814f14ef5d",
      "205f58d2dad823d296a554aa40c51da51585c10d",
      "1a75a921185924687ae64735507f458205dbd6692abef18ab96bc2f078914dbb"},
     {6u, 23u, 1358u, 276u, 3u, 1u, 2u, "pie/pr-check/simple-sports.mmd",
      "ca811be701e227efde886ddcf7cd91d9c38ee5b37db6bd0093e0e6799a630ec5",
      "8108979a79ead440a5ae3231556fac27f1cebbf9",
      "1a75a921185924687ae64735507f458205dbd6692abef18ab96bc2f078914dbb"}}};

struct OfficialPie7Documents {
    json::JsonDoc authority;
};

struct OfficialPie7Progress {
    std::size_t captures = 0u, published = 0u, semantic = 0u, physical = 0u, lifecycle = 0u;
    int height = 0;
    bool captured_before_bind = false, bound = false, decoded = false, wide_source = false,
         narrow_ordinary = false, wide_good = false, narrow_good = false, restored_good = false,
         restored_equal = false, baseline = false, mutated = false, exception = false,
         resource = false, admitted = false;
    const char* phase = "acquire";
    std::array<std::optional<std::size_t>, 10> remaining{};
};

struct OfficialPie7Counts {
    std::size_t visited = 0u, observed = 0u, published = 0u, bound = 0u, decoded = 0u,
                admitted = 0u, mutated = 0u, errors = 0u, resources = 0u;
};

using OfficialPie7Frames = std::array<pie7::Grid, 5>;

std::optional<std::size_t> official_pie7_selected(std::size_t global, std::string_view path) {
    for (std::size_t i = 0u; i < kOfficialPie7Pins.size(); ++i)
        if (kOfficialPie7Pins[i].global == global && kOfficialPie7Pins[i].path == path) return i;
    return std::nullopt;
}

void official_pie7_need(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}

bool official_pie7_resource(std::string_view message) {
    return message.find("exhausted") != std::string_view::npos ||
           message.find("budget") != std::string_view::npos;
}

std::string official_pie7_read(const fs::path& path, pie7::Work& work) {
    const auto bytes = fs::file_size(path);
    official_pie7_need(bytes <= 65536u, "OfficialPie7 bounded immutable asset");
    work.product(static_cast<std::size_t>(bytes), 8u);
    work.spend(4096u);
    std::ifstream input(path, std::ios::binary);
    official_pie7_need(input.good(), "OfficialPie7 asset open");
    std::string raw{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    official_pie7_need(raw.size() == bytes, "OfficialPie7 complete asset read");
    return raw;
}

void official_pie7_asset_pin(JsonVal row, const OfficialPie7Pin& pin, pie7::Work& work) {
    work.product(pin.path.size(), 8u);
    work.spend(1024u);
    official_pie7_need(row.get("ordinal").is_int() &&
                           row.get_int("ordinal") == static_cast<int64_t>(pin.ordinal) &&
                           row.get_string("source_path") ==
                               "tests/fixtures/mermaid_official/cases/" + std::string(pin.path) &&
                           row.get_int("source_bytes") == static_cast<int64_t>(pin.bytes) &&
                           row.get_string("source_sha256") == pin.sha &&
                           row.get_string("Model_canonical_sha256") == pin.model_sha &&
                           row.get("Model_canonical_json").is_str(),
                       "OfficialPie7 immutable exact source Model pin");
    const auto canonical = row.get("Model_canonical_json").as_str();
    work.product(canonical.size(), 16u);
    official_pie7_need(digest(canonical) == pin.model_sha, "OfficialPie7 canonical Model bytes");
    const auto parsed = json::parse(canonical);
    official_pie7_need(parsed.has_value(), "OfficialPie7 canonical Model JSON");
    const auto a = pie7::pie_model(parsed->root()), b = pie7::pie_model(row.get("Model"));
    official_pie7_need(a.title == b.title && a.show_data == b.show_data &&
                           a.slices.size() == b.slices.size(),
                       "OfficialPie7 complete Model shape");
    for (std::size_t n = 0; n < a.slices.size(); ++n)
        official_pie7_need(a.slices[n].label == b.slices[n].label &&
                               pie7::exact(a.slices[n].value, b.slices[n].value),
                           "OfficialPie7 complete Model owner/value/order");
}

OfficialPie7Documents load_official_pie7_documents(pie7::Work& setup) {
    const auto root = kNativeRoot / "pie7";
    const auto path = root / "pie7_native_authority.json";
    projection_regular_file(root, path);
    const auto raw = official_pie7_read(path, setup);
    official_pie7_need(digest(raw) == kOfficialPie7AuthoritySha,
                       "OfficialPie7 immutable authority SHA");
    auto parsed = json::parse(raw);
    official_pie7_need(parsed.has_value(), "OfficialPie7 immutable authority JSON");
    const auto value = parsed->root();
    official_pie7_need(value.get_string("schema") == "pie7_whole_source_native_test_authority/v1" &&
                           value.get("cases").is_arr() &&
                           value.get("cases").size() == kOfficialPie7Pins.size(),
                       "OfficialPie7 authority schema/case count");
    for (std::size_t i = 0u; i < kOfficialPie7Pins.size(); ++i)
        official_pie7_asset_pin(value.get("cases").at(i), kOfficialPie7Pins[i], setup);
    return {std::move(*parsed)};
}

void official_pie7_error(Observation& row, std::string_view message) {
    if (row.errors.size() < 6u) row.errors.emplace_back(message.substr(0u, 192u));
}

void official_pie7_publish(const pie7::Grid& grid, std::string_view name, json::JsonMutVal root,
                           json::JsonMutVal item, pie7::Work& work) {
    pie7::bounded(grid);
    work.spend(grid.cells.size() * (4u * sizeof(pie7::Cell) + 256u) + 4096u);
    auto frame = root.make_obj(), cells = root.make_arr(), rows = root.make_arr();
    frame.set("width", grid.width);
    frame.set("height", grid.height);
    for (const auto& cell : grid.cells) {
        auto value = root.make_obj();
        value.set("text", cell.text);
        value.set("foreground", cell.foreground);
        value.set("background", cell.background);
        cells.append(value);
    }
    for (int y = 0; y < grid.height; ++y)
        rows.append(root.make_str(grid.row(y)));
    frame.add("cells", cells);
    frame.add("rows", rows);
    item.add(name, frame);
}

OfficialPie7Frames official_pie7_observe(std::string_view source, json::JsonMutVal root,
                                         json::JsonMutVal item, pie7::Work& work) {
    work.product(source.size() + 128u, 32u * sizeof(std::string) + 256u);
    work.spend(16384u);
    auto drawing = loom::ui::render_markdown(fenced(source, "mermaid", kSentinel));
    auto ordinary = loom::ui::render_markdown(fenced(source, "text", kSentinel));
    drawing->ComputeRequirement();
    ordinary->ComputeRequirement();
    const int height =
        std::max(32, std::max(drawing->requirement().min_y, ordinary->requirement().min_y) + 12);
    official_pie7_need(height <= pie7::kMaxHeight, "OfficialPie7 public capture height");
    OfficialPie7Frames frames;
    constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
    constexpr std::array<std::string_view, 5> names{
        {"wide_240", "narrow_40", "restored_240", "source_240", "source_40"}};
    for (std::size_t n = 0; n < frames.size(); ++n) {
        frames[n] = pie7::capture_paid(n < 3u ? drawing : ordinary, widths[n], height, work);
        official_pie7_publish(frames[n], names[n], root, item, work);
    }
    return frames;
}

std::vector<std::string> official_pie7_geometry(const pie7::Grid& grid, const pie7::Pie& model,
                                                pie7::Work& work) {
    work.spend(grid.cells.size() * (2u * sizeof(pie7::Cell) + 64u) + model.slices.size() * 1024u +
               4096u);
    return pie7::pie_errors(grid, model, work, kSentinel);
}

void official_pie7_lifecycle(const OfficialPie7Frames& frames, pie7::Work& work) {
    for (const auto& grid : frames)
        official_pie7_need(pie7::find_text(grid, kSentinel, work).size() == 1u,
                           "OfficialPie7 unique following token");
    official_pie7_need(frames[0] == frames[2], "OfficialPie7 same-node restore");
    if (pie7::same_source_surface(frames[0], frames[3]))
        official_pie7_need(pie7::same_source_surface(frames[1], frames[4]),
                           "OfficialPie7 ordinary lifecycle");
}

pie7::Point official_pie7_unique(const pie7::Grid& grid, std::string_view text, pie7::Work& work) {
    const auto found = pie7::find_text(grid, text, work);
    official_pie7_need(found.size() == 1u, "OfficialPie7 unique physical witness");
    return found.front();
}

int official_pie7_blank_row(const pie7::Grid& grid, pie7::Work& work) {
    for (int y = 0; y < grid.height; ++y) {
        bool blank = true;
        for (int x = 0; x < grid.width; ++x) {
            work.spend();
            if (grid.at({x, y}).text != " ") blank = false;
        }
        if (blank) return y;
    }
    throw std::runtime_error("OfficialPie7 no blank mutation row");
}

void official_pie7_lifecycle_rejected(const OfficialPie7Frames& good, pie7::Work& work, auto mutate,
                                      std::string_view name) {
    official_pie7_lifecycle(good, work);
    std::size_t cells = 0u;
    for (const auto& frame : good)
        cells += frame.cells.size();
    work.spend(cells * (2u * sizeof(pie7::Cell) + 32u) + sizeof(OfficialPie7Frames) + 4096u);
    auto bad = good;
    mutate(bad);
    bool rejected = false;
    try {
        official_pie7_lifecycle(bad, work);
    } catch (const std::exception& error) {
        if (official_pie7_resource(error.what())) throw;
        rejected = true;
    }
    official_pie7_need(rejected, name);
}

std::string official_pie7_info(const OfficialPie7Progress& p, const Observation& row,
                               const OfficialPie7Pin& pin, std::size_t remaining) {
    json::JsonMutDoc document;
    auto info = document.object();
    info.set("selection_index", static_cast<int64_t>(pin.selection));
    info.set("pie_ordinal", static_cast<int64_t>(pin.ordinal));
    info.set("manifest_global_ordinal", static_cast<int64_t>(pin.global));
    info.set("path", pin.path);
    info.set("source_sha256", row.source_sha256);
    info.set("git_blob", pin.git_blob);
    info.set("source_authority_sha256", kOfficialPie7AuthoritySha);
    info.set("Model_canonical_sha256", pin.model_sha);
    info.set("captures", static_cast<int64_t>(p.captures));
    info.set("published_frames", static_cast<int64_t>(p.published));
    info.set("captures_before_binding", p.captured_before_bind);
    info.set("source_bound_after_captures", p.bound);
    info.set("Model_decoded_after_binding", p.decoded);
    info.set("wide_source_fallback", p.wide_source);
    info.set("narrow_exact_ordinary", p.narrow_ordinary);
    info.set("wide_geometry_good", p.wide_good);
    info.set("narrow_geometry_good", p.narrow_good);
    info.set("restored_geometry_good", p.restored_good);
    info.set("restored_cells_equal", p.restored_equal);
    info.set("good_baseline_once", p.baseline);
    info.set("semantic_mutations_expected", static_cast<int64_t>(pin.semantic));
    info.set("semantic_mutations_rejected", static_cast<int64_t>(p.semantic));
    info.set("physical_mutations_expected", static_cast<int64_t>(pin.physical));
    info.set("physical_mutations_rejected", static_cast<int64_t>(p.physical));
    info.set("lifecycle_mutations_expected", static_cast<int64_t>(pin.lifecycle));
    info.set("lifecycle_mutations_rejected", static_cast<int64_t>(p.lifecycle));
    info.set("exception", p.exception);
    info.set("resource_exhausted", p.resource);
    info.set("foreign_assertions_certified", 0);
    info.set("whole_source_admitted", p.admitted);
    info.set("phase", p.phase);
    info.set("initial_budget", static_cast<int64_t>(kOfficialPie7SceneLimit));
    info.set("work_remaining", static_cast<int64_t>(remaining));
    auto ledger = info.make_arr();
    for (const auto& value : p.remaining) {
        if (value)
            ledger.append(ledger.make_num(static_cast<int64_t>(*value)));
        else
            ledger.append(ledger.make_null());
    }
    info.add("phase_remaining", ledger);
    info.set("error",
             row.errors.empty() ? std::string_view{""} : std::string_view(row.errors.front()));
    document.set_root(info);
    const auto text = document.to_string();
    official_pie7_need(text.size() <= 4096u, "OfficialPie7 metadata bound");
    return text;
}

void official_pie7_accumulate_owned(JsonVal info, const Observation& row, OfficialPie7Counts& out,
                                    const OfficialPie7Documents& docs) {
    constexpr std::array<std::string_view, 35> keys{{"selection_index",
                                                     "pie_ordinal",
                                                     "manifest_global_ordinal",
                                                     "path",
                                                     "source_sha256",
                                                     "git_blob",
                                                     "source_authority_sha256",
                                                     "Model_canonical_sha256",
                                                     "captures",
                                                     "published_frames",
                                                     "captures_before_binding",
                                                     "source_bound_after_captures",
                                                     "Model_decoded_after_binding",
                                                     "wide_source_fallback",
                                                     "narrow_exact_ordinary",
                                                     "wide_geometry_good",
                                                     "narrow_geometry_good",
                                                     "restored_geometry_good",
                                                     "restored_cells_equal",
                                                     "good_baseline_once",
                                                     "semantic_mutations_expected",
                                                     "semantic_mutations_rejected",
                                                     "physical_mutations_expected",
                                                     "physical_mutations_rejected",
                                                     "lifecycle_mutations_expected",
                                                     "lifecycle_mutations_rejected",
                                                     "exception",
                                                     "resource_exhausted",
                                                     "foreign_assertions_certified",
                                                     "whole_source_admitted",
                                                     "phase",
                                                     "initial_budget",
                                                     "work_remaining",
                                                     "phase_remaining",
                                                     "error"}};
    official_pie7_need(
        info.is_obj() && info.size() == keys.size() &&
            std::ranges::all_of(keys, [&](std::string_view key) { return info.has(key); }),
        "OfficialPie7 exact metadata fields");
    auto number = [&](std::string_view key, std::size_t max) {
        const auto value = info.get(key);
        official_pie7_need(value.is_int() && value.as_int() >= 0 &&
                               static_cast<std::size_t>(value.as_int()) <= max,
                           "OfficialPie7 metadata number");
        return static_cast<std::size_t>(value.as_int());
    };
    auto yes = [&](std::string_view key) {
        const auto value = info.get(key);
        official_pie7_need(value.is_bool(), "OfficialPie7 metadata bool");
        return value.as_bool();
    };
    const auto index = number("selection_index", kOfficialPie7Pins.size() - 1u);
    const auto& pin = kOfficialPie7Pins[index];
    official_pie7_need(row.raw_active_schema == kOfficialPie7Contract &&
                           row.raw_source_authority == kOfficialPie7Authority &&
                           row.track == "original" && row.type == "pie" &&
                           row.role_case_index == index && row.path == pin.path &&
                           row.source_sha256 == pin.sha && row.role_model_sha == pin.model_sha &&
                           row.role_body_sha == pin.sha,
                       "OfficialPie7 row identity");
    official_pie7_need(
        number("pie_ordinal", 23u) == pin.ordinal &&
            number("manifest_global_ordinal", 2144u) == pin.global &&
            info.get_string("path") == pin.path && info.get_string("source_sha256") == pin.sha &&
            info.get_string("git_blob") == pin.git_blob &&
            info.get_string("source_authority_sha256") == kOfficialPie7AuthoritySha &&
            info.get_string("Model_canonical_sha256") == pin.model_sha &&
            number("foreign_assertions_certified", 0u) == 0u,
        "OfficialPie7 immutable pin");
    const auto captures = number("captures", 5u), published = number("published_frames", 5u),
               semantic = number("semantic_mutations_rejected", 5u),
               physical = number("physical_mutations_rejected", 12u),
               lifecycle = number("lifecycle_mutations_rejected", 2u);
    const auto captured = yes("captures_before_binding"),
               bound = yes("source_bound_after_captures"),
               decoded = yes("Model_decoded_after_binding"),
               wide_source = yes("wide_source_fallback"), ordinary = yes("narrow_exact_ordinary"),
               wide = yes("wide_geometry_good"), narrow = yes("narrow_geometry_good"),
               restored = yes("restored_geometry_good"), equal = yes("restored_cells_equal"),
               baseline = yes("good_baseline_once"), exception = yes("exception"),
               resource = yes("resource_exhausted"), admitted = yes("whole_source_admitted");
    official_pie7_need(captures == 5u && published == 5u && captured && bound && decoded,
                       "OfficialPie7 capture/bind/decode order");
    official_pie7_need(number("semantic_mutations_expected", 5u) == pin.semantic &&
                           number("physical_mutations_expected", 12u) == pin.physical &&
                           number("lifecycle_mutations_expected", 2u) == pin.lifecycle,
                       "OfficialPie7 expected count tuple");
    const auto initial = number("initial_budget", kOfficialPie7SceneLimit),
               remaining = number("work_remaining", kOfficialPie7SceneLimit);
    official_pie7_need(initial == kOfficialPie7SceneLimit && remaining <= initial &&
                           info.get("phase").is_str() && info.get("phase").as_str().size() <= 48u &&
                           info.get("error").is_str() && info.get("error").as_str().size() <= 192u,
                       "OfficialPie7 budget/error bounds");
    const auto ledger = info.get("phase_remaining");
    official_pie7_need(ledger.is_arr() && ledger.size() == 10u, "OfficialPie7 phase ledger shape");
    std::size_t prior = initial;
    for (std::size_t phase = 0u; phase < ledger.size(); ++phase) {
        const auto value = ledger.at(phase);
        if (phase >= 8u) {
            official_pie7_need(value.is_null(), "OfficialPie7 unused phase ledger slot");
            continue;
        }
        official_pie7_need(value.is_int() && value.as_int() >= 0 &&
                               static_cast<std::size_t>(value.as_int()) <= prior,
                           "OfficialPie7 monotonic phase ledger");
        prior = static_cast<std::size_t>(value.as_int());
    }
    official_pie7_need(prior == remaining, "OfficialPie7 terminal phase/work ledger identity");
    const bool narrow_state = ordinary ? (!narrow && row.status[1] == "source_fallback")
                                       : (narrow && row.status[1] == "drawn");
    if (admitted)
        official_pie7_need(!wide_source && wide && narrow_state && restored && equal && baseline &&
                               semantic == pin.semantic && physical == pin.physical &&
                               lifecycle == pin.lifecycle && !exception && !resource &&
                               row.errors.empty() && row.status[0] == "drawn" &&
                               row.status[2] == "drawn" && info.get_string("phase") == "complete" &&
                               info.get_string("error").empty(),
                           "OfficialPie7 admission conjunction");
    const auto source_case = docs.authority.root().get("cases").at(index);
    official_pie7_need(source_case.get_string("Model_canonical_sha256") == pin.model_sha &&
                           source_case.get_string("source_sha256") == pin.sha &&
                           source_case.get_string("source_path") ==
                               "tests/fixtures/mermaid_official/cases/" + std::string(pin.path),
                       "OfficialPie7 document lifetime pin");
    out.observed += captured;
    out.published += published == 5u;
    out.bound += bound;
    out.decoded += decoded;
    out.admitted += admitted;
    out.mutated += baseline && semantic == pin.semantic && physical == pin.physical &&
                   lifecycle == pin.lifecycle;
    out.errors += !row.errors.empty();
    out.resources += resource;
}

OfficialPie7Counts official_pie7_counts(const std::vector<Observation>& rows,
                                        const OfficialPie7Documents* docs) {
    OfficialPie7Counts out;
    std::array<bool, kOfficialPie7Pins.size()> seen{};
    for (const auto& row : rows) {
        if (row.raw_active_schema != kOfficialPie7Contract) continue;
        official_pie7_need(docs && row.role_case_index < seen.size() && !seen[row.role_case_index],
                           "OfficialPie7 duplicate/unknown source");
        seen[row.role_case_index] = true;
        ++out.visited;
        official_pie7_need(row.raw_source_constraints.size() <= 4096u,
                           "OfficialPie7 metadata size");
        const auto parsed = json::parse(row.raw_source_constraints);
        official_pie7_need(parsed.has_value(), "OfficialPie7 metadata JSON");
        official_pie7_accumulate_owned(parsed->root(), row, out, *docs);
    }
    if (out.visited)
        official_pie7_need(out.visited == seen.size() &&
                               std::ranges::all_of(seen, [](bool used) { return used; }),
                           "OfficialPie7 missing complete source");
    return out;
}

Observation run_official_pie7_case(JsonVal fixture, std::size_t global, std::size_t index,
                                   const OfficialPie7Documents& docs,
                                   std::array<bool, kOfficialPie7Pins.size()>& consumed,
                                   pie7::Work& work, std::ostream& diagnostics,
                                   std::size_t observation_size, std::size_t observation_capacity) {
    const auto& pin = kOfficialPie7Pins.at(index);
    work.spend(kOfficialPie7ReportReserve + 8u * sizeof(Observation) +
               8u * sizeof(OfficialPie7Progress) + 8u * sizeof(OfficialPie7Frames) + 8192u);
    Observation row;
    row.errors.reserve(6u);
    OfficialPie7Progress progress;
    row.raw_active_schema = kOfficialPie7Contract;
    row.raw_source_authority = kOfficialPie7Authority;
    row.track = "original";
    row.type = "pie";
    row.expected = "drawn";
    row.expected_states = {"drawn", "drawn_or_exact_source", "drawn"};
    row.reason = "Complete Pie7 terminal whole-scene contract; foreign assertions pending";
    row.decision_reasons.fill("Five color PixelAt captures, source binding, complete Model "
                              "geometry, restore, and exact source-derived mutants");
    row.decision_reasons[1] = "Complete40 geometry OR exact ordinary source lifecycle";
    row.role_case_index = index;
    row.role_case_id = "pie/" + std::to_string(pin.ordinal);
    row.role_model_sha = std::string(pin.model_sha);
    row.role_body_sha = std::string(pin.sha);
    row.projection_baseline = projection_canonical(fixture.get("raw"));
    json::JsonMutDoc diagnostic;
    auto data = diagnostic.object();
    try {
        official_pie7_need(observation_size <= 2738u && observation_capacity <= 8192u &&
                               !consumed[index],
                           "OfficialPie7 bounded unique Observation slot");
        static_assert(std::is_nothrow_move_constructible_v<Observation>);
        const auto slots = observation_size == observation_capacity
                               ? std::max(2u * observation_capacity, observation_size + 1u)
                               : 0u;
        work.product(slots + (slots ? observation_size : 0u) + 1u, 2u * sizeof(Observation) + 16u);
        consumed[index] = true;
        const auto path = fixture.get_string("path");
        official_pie7_need(global == pin.global && path == pin.path &&
                               fixture.get_int("bytes") == static_cast<int64_t>(pin.bytes) &&
                               fixture.get_string("sha256") == pin.sha &&
                               fixture.get_string("git_blob") == pin.git_blob,
                           "OfficialPie7 manifest tuple");
        row.path = path;
        work.product(pin.bytes, 4u);
        work.spend(4096u);
        const auto source_path = kRoot / "cases" / row.path;
        official_pie7_need(fs::file_size(source_path) == pin.bytes,
                           "OfficialPie7 actual source file-size pin");
        const auto source = read_file(source_path);
        official_pie7_need(source.size() == pin.bytes && digest(source) == pin.sha,
                           "OfficialPie7 actual complete source");
        row.source_sha256 = digest(source);
        data.set("kind", "official_pie7_actual_scene/v1");
        data.set("selection_index", static_cast<int64_t>(index));
        data.set("fixture", row.path);
        data.set("source_sha256", row.source_sha256);
        progress.phase = "five_actual_Screen_PixelAt_captures";
        const auto frames = official_pie7_observe(source, data, data, work);
        progress.captures = 5u;
        progress.captured_before_bind = true;
        progress.height = frames[0].height;
        progress.remaining[0] = work.left;
        progress.phase = "publish_complete";
        progress.published = 5u;
        progress.remaining[1] = work.left;
        official_pie7_lifecycle(frames, work);
        progress.phase = "complete_source_binding";
        const auto source_case = docs.authority.root().get("cases").at(index);
        official_pie7_need(source.size() ==
                                   static_cast<std::size_t>(source_case.get_int("source_bytes")) &&
                               digest(source) == source_case.get_string("source_sha256"),
                           "OfficialPie7 authority/raw source identity");
        const auto prefix = source_case.get_string("complete_prefix");
        official_pie7_need(prefix.size() ==
                                   static_cast<std::size_t>(source_case.get_int("prefix_bytes")) &&
                               source.starts_with(prefix) &&
                               digest(std::string_view(source).substr(prefix.size())) ==
                                   source_case.get_string("body_sha256"),
                           "OfficialPie7 full prefix/body binding");
        progress.bound = true;
        progress.remaining[2] = work.left;
        progress.phase = "complete_Model_decode";
        const auto canonical = source_case.get("Model_canonical_json");
        official_pie7_need(canonical.is_str(), "OfficialPie7 canonical Model string");
        work.product(canonical.as_str().size(), 16u * sizeof(std::string) + 128u);
        work.spend(8192u);
        official_pie7_need(digest(canonical.as_str()) == pin.model_sha,
                           "OfficialPie7 canonical Model SHA");
        const auto parsed = json::parse(canonical.as_str());
        official_pie7_need(parsed.has_value(), "OfficialPie7 canonical Model parse");
        const auto model = pie7::pie_model(parsed->root()),
                   recorded = pie7::pie_model(source_case.get("Model"));
        official_pie7_need(model.slices.size() >= 2u && model.slices.size() <= 7u &&
                               model.title == recorded.title &&
                               model.show_data == recorded.show_data &&
                               model.slices.size() == recorded.slices.size(),
                           "OfficialPie7 complete Model shape");
        for (std::size_t n = 0; n < model.slices.size(); ++n)
            official_pie7_need(model.slices[n].label == recorded.slices[n].label &&
                                   pie7::exact(model.slices[n].value, recorded.slices[n].value),
                               "OfficialPie7 complete Model owner/value/order");
        progress.decoded = true;
        progress.remaining[3] = work.left;
        progress.phase = "wide_geometry";
        progress.wide_source = pie7::same_source_surface(frames[0], frames[3]);
        official_pie7_need(!progress.wide_source,
                           "OfficialPie7 mandatory wide drawing is ordinary source");
        const auto wide = official_pie7_geometry(frames[0], model, work);
        official_pie7_need(wide.empty(), wide.empty() ? "" : wide.front());
        progress.wide_good = true;
        progress.baseline = true;
        row.status[0] = "drawn";
        progress.remaining[4] = work.left;
        progress.phase = "narrow";
        if (pie7::same_source_surface(frames[1], frames[4])) {
            progress.narrow_ordinary = true;
            row.status[1] = "source_fallback";
        } else {
            const auto errors = official_pie7_geometry(frames[1], model, work);
            official_pie7_need(errors.empty(), errors.empty() ? "" : errors.front());
            progress.narrow_good = true;
            row.status[1] = "drawn";
        }
        progress.remaining[5] = work.left;
        progress.phase = "restored_geometry";
        const auto restored = official_pie7_geometry(frames[2], model, work);
        official_pie7_need(restored.empty(),
                           restored.empty() ? "OfficialPie7 restored geometry" : restored.front());
        progress.restored_good = true;
        progress.restored_equal = frames[0] == frames[2];
        official_pie7_need(progress.restored_equal, "OfficialPie7 same node restore differs");
        row.status[2] = "drawn";
        progress.remaining[6] = work.left;
        progress.phase = "good_first_mutations";
        auto model_rejected = [&](auto mutate, std::string_view name) {
            official_pie7_need(progress.baseline, "OfficialPie7 good baseline");
            work.spend(model.slices.size() * 1024u + 4096u);
            auto bad = model;
            mutate(bad);
            const auto errors = official_pie7_geometry(frames[0], bad, work);
            official_pie7_need(!errors.empty(), name);
            ++progress.semantic;
        };
        auto grid_rejected = [&](auto mutate, std::string_view name) {
            official_pie7_need(progress.baseline, "OfficialPie7 good baseline");
            work.spend(frames[0].cells.size() * (2u * sizeof(pie7::Cell) + 32u) + 4096u);
            auto bad = frames[0];
            mutate(bad);
            const auto errors = official_pie7_geometry(bad, model, work);
            official_pie7_need(!errors.empty(), name);
            ++progress.physical;
        };
        model_rejected([](pie7::Pie& bad) { bad.slices.pop_back(); }, "OfficialPie7 dropped owner");
        model_rejected([](pie7::Pie& bad) { bad.slices.front().value *= 2; },
                       "OfficialPie7 changed total");
        official_pie7_need(model.slices.size() > 1u &&
                               model.slices.front().label != model.slices.back().label,
                           "OfficialPie7 meaningful owner exchange");
        model_rejected(
            [](pie7::Pie& bad) { std::swap(bad.slices.front().label, bad.slices.back().label); },
            "OfficialPie7 exchanged owners");
        grid_rejected(
            [&](pie7::Grid& bad) { bad.edit({0, official_pie7_blank_row(bad, work)}).text = "X"; },
            "OfficialPie7 orphan ink");
        official_pie7_lifecycle_rejected(
            frames, work,
            [&](OfficialPie7Frames& bad) {
                bad[0].edit(official_pie7_unique(bad[0], kSentinel, work)).text = " ";
            },
            "OfficialPie7 following lifecycle mutation");
        ++progress.lifecycle;
        official_pie7_lifecycle_rejected(
            frames, work, [](OfficialPie7Frames& bad) { bad[2].edit({0, 0}).text = "X"; },
            "OfficialPie7 restore lifecycle mutation");
        ++progress.lifecycle;
        if (pin.ordinal == 12u) {
            for (const auto glyph : {std::string_view{"█"}, std::string_view{"."}})
                grid_rejected(
                    [&](pie7::Grid& bad) {
                        bad.edit(official_pie7_unique(bad, "·", work)).text = glyph;
                    },
                    "OfficialPie7 subcell glyph");
            for (const double value : {0.0, 2.0})
                model_rejected([&](pie7::Pie& bad) { bad.slices.back().value = value; },
                               "OfficialPie7 exact Rats raw Model value");
            grid_rejected(
                [&](pie7::Grid& bad) {
                    const auto p = official_pie7_unique(bad, "(1; below 0.5 cell)", work);
                    pie7::write(bad, p, "(2; below 0.5 cell)");
                },
                "OfficialPie7 raw annotation");
            grid_rejected(
                [&](pie7::Grid& bad) {
                    const auto p = official_pie7_unique(bad, "0.2%", work);
                    pie7::write(bad, p, "0.0%");
                },
                "OfficialPie7 percentage");
            grid_rejected(
                [&](pie7::Grid& bad) {
                    const auto p = official_pie7_unique(bad, "Rats", work);
                    for (int x = 0; x < bad.width; ++x)
                        bad.edit({x, p.y}).text = " ";
                },
                "OfficialPie7 missing tiny owner");
            grid_rejected(
                [&](pie7::Grid& bad) {
                    const auto p = official_pie7_unique(bad, pie7::kPieSubcellLegend, work);
                    pie7::write(bad, p,
                                "Scale: 400 cells = 100%; dot = positive share below 0.5 cell");
                },
                "OfficialPie7 scale");
            grid_rejected(
                [&](pie7::Grid& bad) {
                    const auto p = official_pie7_unique(bad, pie7::kPieSubcellLegend, work);
                    for (int x = 0; x < bad.width; ++x)
                        bad.edit({x, p.y}).text = " ";
                },
                "OfficialPie7 missing legend");
            grid_rejected(
                [&](pie7::Grid& bad) {
                    const int y = official_pie7_blank_row(bad, work);
                    pie7::write(bad, {0, y}, pie7::kPieSubcellLegend);
                },
                "OfficialPie7 duplicate legend");
            grid_rejected(
                [&](pie7::Grid& bad) {
                    const auto dot = official_pie7_unique(bad, "·", work);
                    const auto cats = official_pie7_unique(bad, "Cats", work);
                    bad.edit(dot).text = " ";
                    bad.edit({dot.x, cats.y}).text = "·";
                },
                "OfficialPie7 reassigned dot");
            grid_rejected(
                [&](pie7::Grid& bad) {
                    const auto p = official_pie7_unique(bad, "(1; below 0.5 cell)", work);
                    for (int x = p.x; x < p.x + 19; ++x)
                        bad.edit({x, p.y}).text = " ";
                },
                "OfficialPie7 missing annotation");
            grid_rejected(
                [&](pie7::Grid& bad) {
                    const auto p = official_pie7_unique(bad, "(1; below 0.5 cell)", work);
                    const int y = official_pie7_blank_row(bad, work);
                    for (int x = p.x; x < bad.width; ++x)
                        bad.edit({x, p.y}).text = " ";
                    pie7::write(bad, {0, y}, "(1; below 0.5 cell)");
                },
                "OfficialPie7 moved annotation");
        }
        official_pie7_need(progress.semantic == pin.semantic && progress.physical == pin.physical &&
                               progress.lifecycle == pin.lifecycle,
                           "OfficialPie7 exact mutation catalog");
        progress.mutated = true;
        progress.phase = "complete";
        progress.admitted = true;
        progress.remaining[7] = work.left;
    } catch (const std::exception& error) {
        progress.exception = true;
        progress.resource = work.left == 0u || official_pie7_resource(error.what());
        official_pie7_error(row, error.what());
        progress.admitted = false;
        for (std::size_t phase = 0u; phase < 8u; ++phase)
            if (!progress.remaining[phase]) progress.remaining[phase] = work.left;
    }
    row.raw_source_constraints = official_pie7_info(progress, row, pin, work.left);
    data.set("contract", kOfficialPie7Contract);
    data.set("actual", row.raw_source_constraints);
    data.set("foreign_assertions_certified", 0);
    diagnostic.set_root(data);
    diagnostics << diagnostic.to_string() << '\n';
    official_pie7_need(static_cast<bool>(diagnostics), "OfficialPie7 diagnostics");
    return row;
}

// Fixed terminal contract only; no new policy warehouse or glyph goldens.
constexpr std::string_view kStageCContract = "ClassNativeWholeScene/v1";
constexpr std::size_t kStageCVerificationBudget = 128000000u;
constexpr std::size_t kStageCReportReserve = 131072u;
enum class StageCPhase {
    Acquire,
    Capture,
    Bind,
    Decode,
    Compare,
    Wide,
    Narrow,
    Restored,
    Complete
};

struct StageCProgress {
    StageCPhase phase = StageCPhase::Acquire;
    int height = 0;
    bool captured = false, bound = false, decoded = false, pending = false, admitted = false;
    bool restored_equal = false, narrow_ordinary = false, wide_source = false, dagre = false,
         classifier = false, unused_definitions = false, notes = false, exception = false,
         resource = false;
    std::array<std::optional<std::size_t>, 8> remaining{};
};

struct StageCCounts {
    std::size_t visited = 0, observed = 0, bound = 0, decoded = 0, pending = 0, admitted = 0,
                dagre = 0, classifier = 0, unused_definitions = 0, notes = 0, errors = 0,
                resources = 0;
};

Observation run_class285_case(JsonVal, std::size_t,
                              const relations_oracle::OfficialClass285Documents&,
                              const relations_oracle::OfficialClass285Originals&,
                              relations_oracle::Work&, std::size_t, std::size_t);
StageCCounts stage_c_counts(const std::vector<Observation>&);

// ER originals use one explicit-token same-ledger whole-scene contract.
constexpr std::string_view kOfficialEr206Contract = "ERNativeWholeScene/v1";
constexpr std::string_view kOfficialEr206AuthoritySha =
    "76d54ecbc725e59645501f7079f9387b272172d584703f0ea72f143c83c469db";
constexpr std::string_view kOfficialEr206Authority =
    "er206_native_whole_source_authority.config62.v1.json";
constexpr std::size_t kOfficialEr206ReportReserve = 131072u;
// Each positive source prepays four applicability/digest passes before any
// capture: builder, Matrix summary, report provenance and formal summary.
// This is additional to, and never borrowed from, the bounded error reserve.
constexpr std::size_t kOfficialErMutationPlanCharge =
    16u * sizeof(relations_oracle::Class481Mutations) +
    128u * (sizeof(std::pair<const std::string, int>) + 4u * sizeof(void*) + sizeof(std::string)) +
    128u * 1849u + 32768u;
using OfficialErFrames = std::array<std::optional<relations_oracle::Grid>, 5>;
enum class OfficialErPhase {
    Acquire,
    Capture,
    Publish,
    Bind,
    Decode,
    Compare,
    Wide,
    Narrow,
    Restored,
    Mutations,
    Complete
};

struct OfficialErProgress {
    OfficialErPhase phase = OfficialErPhase::Acquire;
    int height = 0;
    std::size_t captures = 0, published = 0, typed_rejections = 0, orphan_rejections = 0;
    bool bound = false, decoded = false, pending = false, admitted = false, restored = false,
         narrow_ordinary = false;
    bool wide_good = false, narrow_good = false, restored_good = false, positive_unmet = false,
         mutated = false, exception = false, resource = false;
    std::array<std::optional<std::size_t>, 10> remaining{};
    std::string mutation_proof_sha;
};

struct OfficialErCounts {
    std::size_t visited = 0, observed = 0, published = 0, bound = 0, decoded = 0, pending = 0,
                admitted = 0, mutated = 0, unmet = 0, errors = 0, resources = 0;
};

Observation run_official_er206_case(JsonVal, std::size_t, std::size_t,
                                    const relations_oracle::Er206Documents&, std::array<bool, 206>&,
                                    relations_oracle::Work&, std::ostream&, std::size_t,
                                    std::size_t, JsonVal er107_local_proof = JsonVal{});

Observation official_er206_legacy_view(const Observation& row) {
    require(row.raw_active_schema == kOfficialEr107Contract &&
                row.role_case_index < er107::kPins.size(),
            "ER107 requires retained ER206 source identity");
    const auto authority = json::parse(row.raw_source_authority);
    require(authority.has_value() &&
                authority->root().get_string("source_facts_sha256") == er107::kFactsSha &&
                authority->root().get("retained_ER206").is_obj(),
            "ER107 requires retained ER206 closed proof");
    Observation legacy;
    legacy.path = row.path;
    legacy.track = row.track;
    legacy.type = row.type;
    legacy.source_sha256 = row.source_sha256;
    legacy.status = row.status;
    legacy.errors = row.errors;
    legacy.projection_baseline = row.projection_baseline;
    legacy.raw_active_schema = kOfficialEr206Contract;
    legacy.raw_source_authority = kOfficialEr206Authority;
    legacy.role_case_index =
        static_cast<std::size_t>(er107::kPins[row.role_case_index].prior_ordinal);
    legacy.raw_source_constraints = projection_canonical(authority->root().get("retained_ER206"));
    require(legacy.role_case_index < relations_oracle::kEr206Pins.size() &&
                relations_oracle::kEr206Pins[legacy.role_case_index].model_bytes == 0,
            "ER107 supplements only source-bound ER206 null Models");
    return legacy;
}

void official_er_accumulate_owned(JsonVal, const Observation&, OfficialErCounts&,
                                  const relations_oracle::Er206Documents&);
OfficialErCounts official_er_counts(const std::vector<Observation>&,
                                    const relations_oracle::Er206Documents*);

// Separate whole-source Flow proof; Observation/Frame/Class layout is unchanged.
constexpr std::string_view kOfficialFlow400Contract = "FlowNativeWholeScene/v1";
constexpr std::size_t kOfficialFlowReportReserve = 65536u;
using OfficialFlowFrames = std::array<std::optional<flow_oracle::Grid>, 5>;

struct OfficialFlowProgress {
    bool captured = false, bound = false, decoded = false, pending = false, admitted = false;
    bool restored = false, narrow_ordinary = false, mutated = false, resource = false;
    std::size_t captures = 0;
    int height = 0;
    const char* phase = "acquire";
    std::array<std::size_t, 7> remaining{};
};

struct OfficialFlowCounts {
    std::size_t visited = 0, observed = 0, bound = 0, decoded = 0, pending = 0, admitted = 0,
                mutated = 0, errors = 0, resources = 0;
};

Observation run_official_flow400_case(JsonVal, std::size_t, std::size_t, std::string_view,
                                      const flow_oracle::Flow400Documents&,
                                      flow_oracle::OracleWork&, std::ostream&, std::size_t,
                                      std::size_t, JsonVal = JsonVal{}, JsonVal = JsonVal{},
                                      JsonVal = JsonVal{}, JsonVal = JsonVal{},
                                      JsonVal = JsonVal{});
OfficialFlowCounts official_flow_counts(const std::vector<Observation>&, JsonVal = JsonVal{},
                                        JsonVal = JsonVal{}, JsonVal = JsonVal{},
                                        JsonVal = JsonVal{});
constexpr std::string_view kFlowRepeatBody2Contract = "FlowRepeatBodySameRun/v1";

struct FlowRepeatBody2Pin {
    std::size_t ordinal, global, bytes, manifest_bytes, diagnostic_bytes;
    std::string_view path, source_sha, git_blob, manifest_sha, diagnostic_sha, model_sha;
};

constexpr std::array<FlowRepeatBody2Pin, 2> kFlowRepeatBody2Pins{{
    {1u, 662u, 166u, 580u, 203u,
     "flowchart/nightly-check/8326-should-merge-repeated-subgraph-id-with-nested-subgraph.mmd",
     "182263ec291bfca591c1ad1871db860357d18642ec6760898ca9136b72d3bec6",
     "6c01c10ad5b6f502123aa13424bb5aacc0064bbb",
     "e32474a3d54beac6130ea88f4df86a57077a9a140560c90041da524f4e719c08",
     "b0d1e8009dbacab935277115dad7cd7402e2f4446dd522b8a19baf7f6916e09a",
     "fa9c3c8fca6cb916f45d17da5ed19eb4605f913b46d02a85efe37f31f5138c9a"},
    {2u, 663u, 129u, 560u, 203u,
     "flowchart/nightly-check/8326-should-merge-repeated-subgraph-ids.mmd",
     "497a06be0c2bcf5e8f5cf0504b5944b8eef12462a5651a03b28b37dd3b331c59",
     "c2626170cec5c91c96e7b52e0e43a9e4f1665b91",
     "7ea5acdb19690c35aa88d99c54eac8fc4e81afe837d84bab2d6ee0028eab7628",
     "7a888bf03898c2570b1e178b243e25f8d8d6e3f9ae9a6c656ed1fedb549f0de4",
     "3d1a63388843526beb05a9658162be2e2c19b1da13ac5ea6bb7cfdf1ebc26e72"},
}};
void official_flow_accumulate_owned(JsonVal, const Observation&, OfficialFlowCounts&,
                                    JsonVal = JsonVal{}, JsonVal = JsonVal{}, JsonVal = JsonVal{},
                                    JsonVal = JsonVal{});
void flow_body2_reserve(std::vector<Observation>&, flow_oracle::OracleWork&);
void flow_body2_consume(std::array<bool, 2>&, std::size_t, flow_oracle::OracleWork&);
Observation reuse_flow_body2(JsonVal, std::size_t, std::size_t, std::string_view, std::string_view,
                             const Observation&, const flow_oracle::Flow400Documents&,
                             flow_oracle::OracleWork&);
OfficialFlowCounts flow_body2_counts(const std::vector<Observation>&);

constexpr std::string_view kGroupEndpoint15BodyContract = "GroupEndpoint15BodyWholeScene/v1";

struct GroupEndpoint15BodyFact {
    std::size_t ordinal, global;
    std::string_view path, raw_sha;
    std::size_t offset, body_bytes;
    std::string_view body_sha, body_utf8, model_sha, expectation_canonical, expectation_sha;
};

constexpr std::array<GroupEndpoint15BodyFact, 15> kGroupEndpoint15BodyFacts{{
    {46u, 707u, "flowchart/nightly-check/dagre/66-dagre-more-nested-subgraph-cases-tb.mmd",
     "4a950aa7c9c78e06ceb6e0f27b7f830b169fcfe9f30c58ff5b0a2811c58c1bdb", 343u, 97u,
     "620d1c078fda0014ae6fce959a0ea04f25275ab751e6a8a352a86c576f736ff0", R"GE15BODY(flowchart TB
  subgraph two
  b1
  end
  subgraph three
  c2
  end

  three --> two
  two --> c2
)GE15BODY",
     "d00eb510cd0c1b73fc660696d7b6ab6f73c84ab0822dc6f8c35482915c7eb55e",
     R"GE15BODY({"expected":"source","offset":343,"reason":"group-edge-endpoint-outside-current-batch","sha256":"620d1c078fda0014ae6fce959a0ea04f25275ab751e6a8a352a86c576f736ff0","witness":{"offset":70,"text":"three --> two"}})GE15BODY",
     "11fcb53df8cd847daaa618b56cc576021ecbdffbf3e8a589eb89c1bd252f6514"},
    {48u, 709u, "flowchart/nightly-check/dagre/67-dagre-more-nested-subgraph-cases-rl.mmd",
     "7c76370e0e7abbcf5accefa43ebe7347ce0f6d9dca17b85ac0db3ae8a094406a", 343u, 97u,
     "afa7b03d093275805f6d77436f0338da7973f99ae392e58a843a1ca0f8587c07", R"GE15BODY(flowchart RL
  subgraph two
  b1
  end
  subgraph three
  c2
  end

  three --> two
  two --> c2
)GE15BODY",
     "c5b016a5b9fdcb7e7c988afd6f2f39d2fffcd885fb0ad8a1d8239fdf75377350",
     R"GE15BODY({"expected":"source","offset":343,"reason":"group-edge-endpoint-outside-current-batch","sha256":"afa7b03d093275805f6d77436f0338da7973f99ae392e58a843a1ca0f8587c07","witness":{"offset":70,"text":"three --> two"}})GE15BODY",
     "157a2ba93b2ff6a0b2804333938e58a7ba7a92735dd81781e508019adbc2e90e"},
    {49u, 710u, "flowchart/nightly-check/dagre/68-dagre-more-nested-subgraph-cases-bt.mmd",
     "6c1b8e69ef6ff81dc9fec44a178902606d821d6a8c02b5d303585fad93430985", 343u, 97u,
     "25d186cca53d5c932d4013de776b52d7f66684c1cf87559a73251d8af24189ac", R"GE15BODY(flowchart BT
  subgraph two
  b1
  end
  subgraph three
  c2
  end

  three --> two
  two --> c2
)GE15BODY",
     "8e490093952a8a04201227d1424e71497f5588640de30395e2fbe3ccbac3ebaa",
     R"GE15BODY({"expected":"source","offset":343,"reason":"group-edge-endpoint-outside-current-batch","sha256":"25d186cca53d5c932d4013de776b52d7f66684c1cf87559a73251d8af24189ac","witness":{"offset":70,"text":"three --> two"}})GE15BODY",
     "0d8670a833e9b8a848db65e1ab0dc50c313fa0f9be9325afabc0bd87bc34126d"},
    {50u, 711u, "flowchart/nightly-check/dagre/69-dagre-more-nested-subgraph-cases-lr.mmd",
     "152078a8abc87d36a815f03a8350ec8cdbf9ea83bdf4ee180462d3af1594727d", 343u, 97u,
     "da2ffc916e9e54a753a1c775a5efa4ab8c3d4b02e66705451e4991b0415433b8", R"GE15BODY(flowchart LR
  subgraph two
  b1
  end
  subgraph three
  c2
  end

  three --> two
  two --> c2
)GE15BODY",
     "a56f18f6c7501d7bdc31ca029d453f20bef916001ad68c041f16a6b9145deccc",
     R"GE15BODY({"expected":"source","offset":343,"reason":"group-edge-endpoint-outside-current-batch","sha256":"da2ffc916e9e54a753a1c775a5efa4ab8c3d4b02e66705451e4991b0415433b8","witness":{"offset":70,"text":"three --> two"}})GE15BODY",
     "5f8bb35bf6a1dfe7c0f6715942d53a50f8fde7bc5129e1c8d3fb620cff19eaa9"},
    {51u, 712u,
     "flowchart/nightly-check/dagre/"
     "70-dagre-handle-nested-subgraph-cases-tb-link-out-and-link-between-subgraphs.mmd",
     "ad8cccaaff50ae5e3cbbbfcf45bf67b740bb03ba1dcef0a29f2bab439c2d568d", 327u, 105u,
     "6de83f06d3b4771cad44d11dd1f3d4fcb4181ebd25f01112528d50b8cd31f769", R"GE15BODY(flowchart TB
  subgraph S1
   sub1 -->sub2
  end
  subgraph S2
   sub4
  end
  S1 --> S2
  sub1 --> sub4
)GE15BODY",
     "407c0db75d264af506aed01ad48bd769844108b738350d25c176ca07803019f0",
     R"GE15BODY({"expected":"source","offset":327,"reason":"group-edge-endpoint-outside-current-batch","sha256":"6de83f06d3b4771cad44d11dd1f3d4fcb4181ebd25f01112528d50b8cd31f769","witness":{"offset":79,"text":"S1 --> S2"}})GE15BODY",
     "cbe13a7f6064e3a47080e6a539a2a8020e340484dd58a969a2fba3bc8e4c922f"},
    {52u, 713u,
     "flowchart/nightly-check/dagre/"
     "71-dagre-handle-nested-subgraph-cases-rl-link-out-and-link-between-subgraphs.mmd",
     "0097824ef99719abe027b6c72a8dd3af82d42ae7e2743e9d1c22b49754e9823a", 327u, 105u,
     "d9a7b59a5ee27f7ac4bed198b0d1b97508eec1e01e101cb2653bda42a658f6ea", R"GE15BODY(flowchart RL
  subgraph S1
   sub1 -->sub2
  end
  subgraph S2
   sub4
  end
  S1 --> S2
  sub1 --> sub4
)GE15BODY",
     "51267f963e816dd1481918e5e3fbdbc875c225c48441c65a000e08a0b7607691",
     R"GE15BODY({"expected":"source","offset":327,"reason":"group-edge-endpoint-outside-current-batch","sha256":"d9a7b59a5ee27f7ac4bed198b0d1b97508eec1e01e101cb2653bda42a658f6ea","witness":{"offset":79,"text":"S1 --> S2"}})GE15BODY",
     "feb9d3b7cf214a64d19092009510eb19cc9a3fa1d13d6241cb2a09e799fc929f"},
    {53u, 714u,
     "flowchart/nightly-check/dagre/"
     "72-dagre-handle-nested-subgraph-cases-bt-link-out-and-link-between-subgraphs.mmd",
     "7bcbd6376a538b18d80a893f78fb95336663fd8d45d72f78f42168cf91cf590b", 327u, 105u,
     "3d9b21236da5a7206b2de34fb629a26a79a7848065a67cdb87388b8a18b0719d", R"GE15BODY(flowchart BT
  subgraph S1
   sub1 -->sub2
  end
  subgraph S2
   sub4
  end
  S1 --> S2
  sub1 --> sub4
)GE15BODY",
     "c78dd6aead03a757ec041033eaaac9cee774f300b0b3e1868234cd36462e0175",
     R"GE15BODY({"expected":"source","offset":327,"reason":"group-edge-endpoint-outside-current-batch","sha256":"3d9b21236da5a7206b2de34fb629a26a79a7848065a67cdb87388b8a18b0719d","witness":{"offset":79,"text":"S1 --> S2"}})GE15BODY",
     "0ccef5ad88fc89a05cbadc0f9a7dd88a178f826cad78bb913d37d0446d67c0ea"},
    {56u, 717u,
     "flowchart/nightly-check/dagre/"
     "74-dagre-handle-nested-subgraph-cases-rl-link-out-and-link-between-subgraphs.mmd",
     "294576e02acdb10b8432a886ce70b402ee5cd46391c3b4cde9c1ea954a2b6b86", 324u, 105u,
     "d9a7b59a5ee27f7ac4bed198b0d1b97508eec1e01e101cb2653bda42a658f6ea", R"GE15BODY(flowchart RL
  subgraph S1
   sub1 -->sub2
  end
  subgraph S2
   sub4
  end
  S1 --> S2
  sub1 --> sub4
)GE15BODY",
     "51267f963e816dd1481918e5e3fbdbc875c225c48441c65a000e08a0b7607691",
     R"GE15BODY({"expected":"source","offset":324,"reason":"group-edge-endpoint-outside-current-batch","sha256":"d9a7b59a5ee27f7ac4bed198b0d1b97508eec1e01e101cb2653bda42a658f6ea","witness":{"offset":79,"text":"S1 --> S2"}})GE15BODY",
     "925edaf140c73162e05cbba111e348b214edaab7603f8ae0132a1acec9cfc7b7"},
    {329u, 990u, "flowchart/visual-check/66-more-nested-subgraph-cases-tb.mmd",
     "4f8673f25e71d8e16db8f2d1442363475ac006415d8e3deb3e43571e7593f3e2", 306u, 97u,
     "620d1c078fda0014ae6fce959a0ea04f25275ab751e6a8a352a86c576f736ff0", R"GE15BODY(flowchart TB
  subgraph two
  b1
  end
  subgraph three
  c2
  end

  three --> two
  two --> c2
)GE15BODY",
     "d00eb510cd0c1b73fc660696d7b6ab6f73c84ab0822dc6f8c35482915c7eb55e",
     R"GE15BODY({"expected":"source","offset":306,"reason":"group-edge-endpoint-outside-current-batch","sha256":"620d1c078fda0014ae6fce959a0ea04f25275ab751e6a8a352a86c576f736ff0","witness":{"offset":70,"text":"three --> two"}})GE15BODY",
     "c3f61468223ccc1a85bddf0bcbfcd6f858d4d161817fa2a54e89d20d2ba60883"},
    {331u, 992u, "flowchart/visual-check/67-more-nested-subgraph-cases-rl.mmd",
     "8d1eb583b7f85940c51969ed8b9f73e8257b19a64dab652583d664b7101deff4", 306u, 97u,
     "afa7b03d093275805f6d77436f0338da7973f99ae392e58a843a1ca0f8587c07", R"GE15BODY(flowchart RL
  subgraph two
  b1
  end
  subgraph three
  c2
  end

  three --> two
  two --> c2
)GE15BODY",
     "c5b016a5b9fdcb7e7c988afd6f2f39d2fffcd885fb0ad8a1d8239fdf75377350",
     R"GE15BODY({"expected":"source","offset":306,"reason":"group-edge-endpoint-outside-current-batch","sha256":"afa7b03d093275805f6d77436f0338da7973f99ae392e58a843a1ca0f8587c07","witness":{"offset":70,"text":"three --> two"}})GE15BODY",
     "3ef79a1a6f7664ec5decf73d2ffa0e64dceadc0998708422d2c4300530b29812"},
    {333u, 994u, "flowchart/visual-check/68-more-nested-subgraph-cases-bt.mmd",
     "16008964dbead8130d94ea1ce29241be41b853abc501798181bab1e082d735ab", 306u, 97u,
     "25d186cca53d5c932d4013de776b52d7f66684c1cf87559a73251d8af24189ac", R"GE15BODY(flowchart BT
  subgraph two
  b1
  end
  subgraph three
  c2
  end

  three --> two
  two --> c2
)GE15BODY",
     "8e490093952a8a04201227d1424e71497f5588640de30395e2fbe3ccbac3ebaa",
     R"GE15BODY({"expected":"source","offset":306,"reason":"group-edge-endpoint-outside-current-batch","sha256":"25d186cca53d5c932d4013de776b52d7f66684c1cf87559a73251d8af24189ac","witness":{"offset":70,"text":"three --> two"}})GE15BODY",
     "51d2da0ec9443f6ca789ad29cd4cf13e93cd4fb23628860e245e605de420bcb4"},
    {335u, 996u, "flowchart/visual-check/69-more-nested-subgraph-cases-lr.mmd",
     "03f48a035d877bb984d721661e374943bb561bdfee7ff31b85d0b6e69351b5e6", 306u, 97u,
     "da2ffc916e9e54a753a1c775a5efa4ab8c3d4b02e66705451e4991b0415433b8", R"GE15BODY(flowchart LR
  subgraph two
  b1
  end
  subgraph three
  c2
  end

  three --> two
  two --> c2
)GE15BODY",
     "a56f18f6c7501d7bdc31ca029d453f20bef916001ad68c041f16a6b9145deccc",
     R"GE15BODY({"expected":"source","offset":306,"reason":"group-edge-endpoint-outside-current-batch","sha256":"da2ffc916e9e54a753a1c775a5efa4ab8c3d4b02e66705451e4991b0415433b8","witness":{"offset":70,"text":"three --> two"}})GE15BODY",
     "cd8c92f3c812a4de5f81a31eb55ed003e290875ff927c9948d54e410a7399484"},
    {340u, 1001u,
     "flowchart/visual-check/"
     "70-handle-nested-subgraph-cases-tb-link-out-and-link-between-subgraphs.mmd",
     "31cfba9707996355e2702000a2162b3b913bdca209c99ba1790abe4111590e99", 360u, 105u,
     "6de83f06d3b4771cad44d11dd1f3d4fcb4181ebd25f01112528d50b8cd31f769", R"GE15BODY(flowchart TB
  subgraph S1
   sub1 -->sub2
  end
  subgraph S2
   sub4
  end
  S1 --> S2
  sub1 --> sub4
)GE15BODY",
     "407c0db75d264af506aed01ad48bd769844108b738350d25c176ca07803019f0",
     R"GE15BODY({"expected":"source","offset":360,"reason":"group-edge-endpoint-outside-current-batch","sha256":"6de83f06d3b4771cad44d11dd1f3d4fcb4181ebd25f01112528d50b8cd31f769","witness":{"offset":79,"text":"S1 --> S2"}})GE15BODY",
     "29fcdb0a2fcf082cd62c09809db4e07dc2efdca94cc703b55f91ee70145060d4"},
    {343u, 1004u,
     "flowchart/visual-check/"
     "71-handle-nested-subgraph-cases-rl-link-out-and-link-between-subgraphs.mmd",
     "811d29c78d2a51cb35f6e239d6ffedfd5d3886d8f06aed13001938d05db17294", 313u, 105u,
     "d9a7b59a5ee27f7ac4bed198b0d1b97508eec1e01e101cb2653bda42a658f6ea", R"GE15BODY(flowchart RL
  subgraph S1
   sub1 -->sub2
  end
  subgraph S2
   sub4
  end
  S1 --> S2
  sub1 --> sub4
)GE15BODY",
     "51267f963e816dd1481918e5e3fbdbc875c225c48441c65a000e08a0b7607691",
     R"GE15BODY({"expected":"source","offset":313,"reason":"group-edge-endpoint-outside-current-batch","sha256":"d9a7b59a5ee27f7ac4bed198b0d1b97508eec1e01e101cb2653bda42a658f6ea","witness":{"offset":79,"text":"S1 --> S2"}})GE15BODY",
     "96874acf97640db050449286d146585165de36ea555dbe16a280ca35b9e1dd4d"},
    {344u, 1005u,
     "flowchart/visual-check/"
     "72-handle-nested-subgraph-cases-bt-link-out-and-link-between-subgraphs.mmd",
     "7b9ccfdcdf87a068f7573ee3c73e6963df992477ce82deb007e1b174b41ee3a1", 313u, 105u,
     "3d9b21236da5a7206b2de34fb629a26a79a7848065a67cdb87388b8a18b0719d", R"GE15BODY(flowchart BT
  subgraph S1
   sub1 -->sub2
  end
  subgraph S2
   sub4
  end
  S1 --> S2
  sub1 --> sub4
)GE15BODY",
     "c78dd6aead03a757ec041033eaaac9cee774f300b0b3e1868234cd36462e0175",
     R"GE15BODY({"expected":"source","offset":313,"reason":"group-edge-endpoint-outside-current-batch","sha256":"3d9b21236da5a7206b2de34fb629a26a79a7848065a67cdb87388b8a18b0719d","witness":{"offset":79,"text":"S1 --> S2"}})GE15BODY",
     "83f1e2ce92b06b88527854e1cc13f11cbf903412660f4855ae7d3a75a2d829e5"},
}};

struct GroupEndpoint15BodyCounts {
    std::size_t visited = 0, observed = 0, bound = 0, decoded = 0, admitted = 0, mutated = 0,
                errors = 0, resources = 0;
};

const GroupEndpoint15BodyFact& group_endpoint15_body_fact(std::size_t ordinal, std::size_t global) {
    const auto found =
        std::ranges::find_if(kGroupEndpoint15BodyFacts, [&](const GroupEndpoint15BodyFact& fact) {
            return fact.ordinal == ordinal && fact.global == global;
        });
    if (found == kGroupEndpoint15BodyFacts.end())
        throw std::runtime_error("GroupEndpoint15 body exact tuple");
    return *found;
}

std::string official_flow_group_endpoint15_body_info(const OfficialFlowProgress&, std::size_t,
                                                     std::size_t, std::size_t, std::size_t, bool,
                                                     const GroupEndpoint15BodyFact&);
Observation run_official_group_endpoint15_body(JsonVal, std::size_t, std::size_t, std::string_view,
                                               const flow_oracle::Flow400Documents&,
                                               const flow_oracle::GroupEndpoint15Documents&,
                                               flow_oracle::OracleWork&, std::ostream&, std::size_t,
                                               std::size_t);
GroupEndpoint15BodyCounts
group_endpoint15_body_counts(const std::vector<Observation>&,
                             const flow_oracle::GroupEndpoint15Documents&);

// RFC 0031 E2a reviewed cohort lookup. Returns the pre-registered draw/source
// disposition for a path, or nullopt when the path is outside the cohort.
const std::map<std::string, bool>& e2a_cohort_map() {
    static const std::map<std::string, bool> map = [] {
        std::map<std::string, bool> rows;
        const auto ledger_path = kRoot / "../../../tools/mermaid/e2a-cohort.json";
        auto parsed = json::parse(read_file(ledger_path));
        require(parsed.has_value(), "E2a cohort ledger must parse");
        require(parsed->root().get("review").get_string("verdict") == "approved",
                "E2a cohort ledger must be independently approved");
        parsed->root().get("rows").iter([&](const JsonVal row) {
            rows.emplace(row.get_string("path"), row.get_string("prediction") == "predicted-draw");
        });
        return rows;
    }();
    return map;
}

std::optional<bool> e2a_cohort_row(std::string_view path) {
    const auto& rows = e2a_cohort_map();
    const auto found = rows.find(std::string(path));
    if (found == rows.end()) return std::nullopt;
    return found->second;
}

Observation run_case(std::string path, std::string track, std::string_view source, JsonVal expected,
                     const fs::path& snapshot_root = kRoot / "snapshots") {
    Observation row;
    row.path = std::move(path);
    row.track = std::move(track);
    row.type = (*fs::path(row.path).begin()).string();
    row.source_sha256 = digest(source);
    row.expected = expected.get_string("expected");
    row.expected_states = {row.expected, row.expected == "source" ? "source" : "unresolved",
                           row.expected};
    row.reason = expected.get_string("reason");
    row.decision_reasons.fill(row.reason);
    try {
        auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
        element->ComputeRequirement();
        const auto lines = static_cast<int>(std::count(source.begin(), source.end(), '\n')) + 1;
        const int height =
            std::min(1040, std::max({20, lines + 12, element->requirement().min_y + 8}));
        std::string snapshot;
        int snapshot_width = 0;
        const bool drawing = row.expected == "drawn";
        if (drawing && fs::exists(snapshot_root / expected.get_string("snapshot"))) {
            snapshot = read_file(snapshot_root / expected.get_string("snapshot"));
            if (!snapshot.empty() && snapshot.back() == '\n') snapshot.pop_back();
        }
        const auto wide = capture(element, kWide, height);
        const auto plain_wide = capture(ordinary, kWide, height);
        // Preserve unexpected art as an unreviewed diagnostic. Classification
        // remains an error until a separately reviewed expectation is applied.
        if (!drawing && wide.cells != plain_wide.cells)
            write_file(fs::path(LOOM_MERMAID_REPORT_DIR) /
                           ("mermaid-unreviewed-drawing-" + digest(source) + "-" +
                            digest(row.path + "|" + row.track).substr(0, 16) + ".txt"),
                       wide.text + '\n');
        // Regeneration is explicit, only after all independent semantics pass.
        if (drawing && update_snapshots() && snapshot_root == kRoot / "snapshots") {
            const auto semantics = semantic_errors(wide, expected.get("checks"));
            std::string details;
            for (const auto& error : semantics)
                details += error + "; ";
            if (wide.cells == plain_wide.cells) details += "Unexpected source fallback; ";
            require(semantics.empty() && wide.cells != plain_wide.cells &&
                        wide.text.find(kSentinel) != std::string::npos,
                    "Refusing to generate a semantically invalid drawing snapshot: " + details);
            snapshot = wide.text;
            write_file(snapshot_root / expected.get_string("snapshot"), snapshot + '\n');
        }
        if (drawing) {
            require(!snapshot.empty(), "Missing reviewed drawing snapshot");
            std::istringstream lines_in(snapshot);
            std::string line;
            while (std::getline(lines_in, line))
                snapshot_width = std::max(snapshot_width, ftxui::string_width(line));
        }
        row.status[0] =
            classify(wide, plain_wide, drawing, expected.get("checks"), snapshot, row.errors);
        const auto narrow = capture(element, kNarrow, height);
        const auto plain_narrow = capture(ordinary, kNarrow, height);
        const bool narrow_drawing = drawing && snapshot_width <= kNarrow;
        row.expected_states[1] = narrow_drawing ? "drawn" : "source";
        if (drawing && !narrow_drawing)
            row.decision_reasons[1] =
                "Supported diagram's reviewed width " + std::to_string(snapshot_width) +
                " exceeds 40 columns; ordinary source fallback is expected only at this width";
        row.status[1] = classify(narrow, plain_narrow, narrow_drawing, expected.get("checks"),
                                 snapshot, row.errors);
        const auto restored = capture(element, kWide, height);
        row.status[2] =
            classify(restored, plain_wide, drawing, expected.get("checks"), snapshot, row.errors);
        if (restored.cells != wide.cells) {
            row.errors.emplace_back("Same-node wide/narrow/wide restoration changed pixels");
            row.status[2] = "error";
        }
    } catch (const std::exception& error) {
        row.errors.push_back(error.what());
    }
    return row;
}

constexpr std::string_view kFlowShapeAliasBodyContract = "flow_shape_alias_body_native/v2";
constexpr std::string_view kFlowShapeAliasBodyAuthority =
    "immutable_Flow400_FlowShapeAlias_body_batch";

Observation run_official_flow_shape_alias_body(JsonVal fixture, std::size_t global,
                                               std::string_view raw_source, JsonVal diagnostic,
                                               std::string_view body) {
    const auto body_case =
        std::ranges::find_if(flow_oracle::kFlowShapeAliasBodyCases,
                             [&](const auto& candidate) { return candidate.global == global; });
    require(body_case != flow_oracle::kFlowShapeAliasBodyCases.end(),
            "Flow shape-alias body global authority");
    const auto* alias = flow_oracle::flow_shape_alias_case(body_case->ordinal);
    require(alias != nullptr, "Flow shape-alias body source model authority");
    std::string effective_expectation = "{\"expected\":\"drawn\",\"reason\":\"source-bound-shape-"
                                        "alias-body\",\"checks\":{\"labels\":[";
    for (std::size_t index = 0u; index < alias->node_count; ++index) {
        if (index) effective_expectation += ',';
        effective_expectation += '\"';
        effective_expectation += alias->labels[index];
        effective_expectation += '\"';
    }
    effective_expectation += "],\"arrows\":0,\"ordering\":[]},\"snapshot\":\"";
    effective_expectation += body_case->snapshot;
    effective_expectation += "\"}";
    auto expected_source = json::parse(effective_expectation);
    require(expected_source.has_value(), "Flow shape-alias body effective expectation JSON");
    const auto snapshot = read_file(kNativeRoot / body_case->snapshot);
    require(!snapshot.empty() && snapshot.back() == '\n',
            "Flow shape-alias body snapshot line ending");
    const auto snapshot_without_newline =
        std::string_view(snapshot).substr(0u, snapshot.size() - 1u);
    require(digest(snapshot_without_newline) == body_case->snapshot_sha,
            "Flow shape-alias body snapshot source pin");

    // run_case captures wide, narrow, restored, and both ordinary source frames.
    // Bind the selected manifest row and exact body only after those captures.
    auto row = run_case(std::string(alias->fixture), "body_diagnostic", body,
                        expected_source->root(), kNativeRoot);
    flow_oracle::flow_shape_alias_body_bind(fixture, diagnostic, body_case->ordinal, global,
                                            raw_source, body);
    row.raw_active_schema = kFlowShapeAliasBodyContract;
    row.raw_source_authority = kFlowShapeAliasBodyAuthority;
    row.role_case_index = body_case->ordinal;
    row.role_body_sha = std::string(body_case->sha);
    row.role_model_sha = std::string(alias->model_sha);
    row.projection_baseline = projection_canonical(diagnostic);

    const bool narrow_valid = row.status[1] == "drawn" || (row.status[1] == "source_fallback" &&
                                                           row.expected_states[1] == "source");
    const bool drawn =
        row.errors.empty() && row.status[0] == "drawn" && narrow_valid && row.status[2] == "drawn";
    json::JsonMutDoc evidence;
    auto info = evidence.object();
    info.set("contract", kFlowShapeAliasBodyContract);
    info.set("source_authority", kFlowShapeAliasBodyAuthority);
    info.set("manifest_global_ordinal", static_cast<int64_t>(global));
    info.set("flow_ordinal", static_cast<int64_t>(body_case->ordinal));
    info.set("raw_source_sha256", std::string(alias->source_sha));
    info.set("body_offset", static_cast<int64_t>(body_case->offset));
    info.set("body_sha256", std::string(body_case->sha));
    info.set("body_bytes", static_cast<int64_t>(body.size()));
    info.set("whole_model_sha256", row.role_model_sha);
    info.set("snapshot_sha256", std::string(body_case->snapshot_sha));
    info.set("source_bound_after_five_captures", true);
    info.set("body_admitted", drawn);
    info.set("semantic_assertions_expected", static_cast<int64_t>(alias->node_count + 1u));
    info.set("semantic_assertions_passed",
             drawn ? static_cast<int64_t>(alias->node_count + 1u) : 0);
    info.set("source_fallback_credit", 0);
    evidence.set_root(info);
    row.raw_source_constraints = evidence.to_string();
    return row;
}

bool official_flow_shape_alias_body_owned(const Observation& row) {
    if (row.raw_active_schema != kFlowShapeAliasBodyContract) return false;
    const auto parsed = json::parse(row.raw_source_constraints);
    require(parsed.has_value(), "Flow shape-alias body evidence JSON");
    const auto info = parsed->root();
    const auto* body_case = flow_oracle::flow_shape_alias_body_case(row.role_case_index);
    const auto* alias = flow_oracle::flow_shape_alias_case(row.role_case_index);
    require(body_case != nullptr && alias != nullptr, "Flow shape-alias body selected authority");
    const auto semantic_count = static_cast<int64_t>(alias->node_count + 1u);
    require(info.size() == 15u && info.get_string("contract") == kFlowShapeAliasBodyContract &&
                info.get_string("source_authority") == kFlowShapeAliasBodyAuthority &&
                info.get_int("manifest_global_ordinal") ==
                    static_cast<int64_t>(body_case->global) &&
                info.get_int("flow_ordinal") == static_cast<int64_t>(body_case->ordinal) &&
                info.get_string("raw_source_sha256") == alias->source_sha &&
                info.get_int("body_offset") == static_cast<int64_t>(body_case->offset) &&
                info.get_string("body_sha256") == body_case->sha &&
                info.get_int("body_bytes") == static_cast<int64_t>(body_case->body.size()) &&
                info.get_string("whole_model_sha256") == alias->model_sha &&
                info.get_string("snapshot_sha256") == body_case->snapshot_sha &&
                info.get("source_bound_after_five_captures").is_bool() &&
                info.get("source_bound_after_five_captures").as_bool() &&
                info.get("body_admitted").is_bool() && info.get("body_admitted").as_bool() &&
                info.get_int("semantic_assertions_expected") == semantic_count &&
                info.get_int("semantic_assertions_passed") == semantic_count &&
                info.get_int("source_fallback_credit") == 0,
            "Flow shape-alias body exact source/capture/model/semantic evidence");
    const bool narrow_valid = row.status[1] == "drawn" || (row.status[1] == "source_fallback" &&
                                                           row.expected_states[1] == "source");
    require(row.track == "body_diagnostic" && row.type == "flowchart" &&
                row.path == alias->fixture && row.source_sha256 == body_case->sha &&
                row.role_body_sha == body_case->sha && row.role_model_sha == alias->model_sha &&
                row.errors.empty() && row.status[0] == "drawn" && narrow_valid &&
                row.status[2] == "drawn",
            "Flow shape-alias body native drawing admission");
    return true;
}

// RFC 0031 E6c: canonical mandatory-gap rows for the capability matrix.
// Known per-row error messages map to stable category codes; anything the
// closed mapper does not recognize is reported into unknown_messages and
// stays a hard ADD_FAILURE at the single call site. This is the one
// construction site shared by the JSON report and the roster diff.
struct MatrixGapRow {
    std::string fixture;
    std::string track;
    std::vector<std::string> categories;
};

std::vector<MatrixGapRow> matrix_mandatory_gap_rows(const std::vector<Observation>& rows,
                                                    std::vector<std::string>* unknown_messages) {
    std::vector<MatrixGapRow> gap_rows;
    for (const auto& row : rows) {
        if (row.errors.empty()) continue;
        std::vector<std::string> categories;
        bool unknown = false;
        for (const auto& error : row.errors) {
            const auto category = gap_roster::matrix_error_category(error);
            if (category == gap_roster::kUnknownCategory) {
                unknown = true;
                if (unknown_messages)
                    unknown_messages->push_back(row.path + " [" + row.track + "]: " + error);
            } else {
                categories.push_back(std::string(category));
            }
        }
        if (unknown) continue;
        std::ranges::sort(categories);
        categories.erase(std::ranges::unique(categories).begin(), categories.end());
        gap_rows.push_back({row.path, row.track, std::move(categories)});
    }
    return gap_rows;
}

void serialize_matrix_gap_rows(json::JsonMutVal root, const std::vector<MatrixGapRow>& gap_rows) {
    auto gaps = root.make_arr();
    for (const auto& gap_row : gap_rows) {
        auto item = root.make_obj();
        item.set("fixture", gap_row.fixture);
        item.set("track", gap_row.track);
        auto categories = root.make_arr();
        for (const auto& category : gap_row.categories)
            categories.append(root.make_str(category));
        item.add("gaps", categories);
        gaps.append(item);
    }
    root.add("mandatory_gaps", gaps);
}

void emit_report(const std::vector<Observation>& rows,
                 const std::vector<std::string>& integrity_errors,
                 bool projection_validated = false, bool role_consumption_validated = false,
                 bool raw_consumption_validated = false, bool body22_consumption_validated = false,
                 const relations_oracle::Er206Documents* er_documents = nullptr,
                 const Requirement11Documents* requirement11_documents = nullptr,
                 const OfficialPie7Documents* pie7_documents = nullptr,
                 const flow_oracle::Flow400TypedSource4Documents* typed_source4_documents = nullptr,
                 const flow_oracle::Flow400ClassicDocuments* classic_documents = nullptr,
                 const flow_oracle::Flow400LinkProfile3Documents* linkprofile3_documents = nullptr,
                 const flow_oracle::GroupEndpoint15Documents* group_endpoint15_documents = nullptr,
                 const json::JsonDoc* mindmap_facts = nullptr,
                 const json::JsonDoc* shapes4_facts = nullptr,
                 bool er107_aggregate_validated = false) {
    fs::path path = fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-official-matrix.json";
    if (const auto* override_path = std::getenv("LOOM_MERMAID_MATRIX_PATH")) path = override_path;
    json::JsonMutDoc document;
    auto root = document.object();
    root.set("schema", 1);
    root.set("upstream_tag", "mermaid@12.2.0");
    root.set("upstream_commit", "1fb7869866dd619168d840291c9c54b4f129758b");
    root.set("manifest_sha256", kManifestSha256);
    root.set("original_drawing_coverage_note",
             "Body diagnostics are separate; source fallback is not successful drawing.");
    auto projection = root.make_obj();
    projection.set("path", kProjectionFile);
    projection.set("sha256", kProjectionSha256);
    projection.set("source_fact_path", kProjectionFactFile);
    projection.set("source_fact_sha256", kProjectionFactSha256);
    projection.set("validated", projection_validated);
    projection.set("applications",
                   static_cast<int>(std::ranges::count_if(rows, [](const auto& row) {
                       return !row.projection_effective.empty() && row.projection_binding == 0;
                   })));
    projection.set("scope", "Body-only reviewed native projection; raw metadata expectations and "
                            "original denominator remain unchanged.");
    root.add("projection_supplement", projection);
    auto bindings = root.make_arr();
    for (std::size_t n = 0; n < kProjectionBindings.size(); ++n) {
        const auto& binding = kProjectionBindings[n];
        auto item = root.make_obj();
        item.set("path", binding.policy_file);
        item.set("sha256", binding.policy_sha);
        item.set("source_fact_path", binding.fact_file);
        item.set("source_fact_sha256", binding.fact_sha);
        item.set("full_model_sha256", binding.model_sha);
        item.set("validated", projection_validated);
        item.set("applications", static_cast<int>(std::ranges::count_if(rows, [&](const auto& row) {
                     return !row.projection_effective.empty() && row.projection_binding == n;
                 })));
        if (binding.kind == ProjectionKind::XYOriginal) {
            item.set("scope", kXYProjectionScope);
            item.set("foreign_svg_title_omission_font_browser_predicates",
                     "non_equivalent_pending");
        }
        bindings.append(item);
    }
    auto role_binding = root.make_obj();
    role_binding.set("path", kRoleBatchBinding.policy_file);
    role_binding.set("sha256", kRoleBatchBinding.policy_sha);
    role_binding.set("source_fact_path", kRoleBatchBinding.fact_file);
    role_binding.set("source_fact_sha256", kRoleBatchBinding.fact_sha);
    role_binding.set("model_schema", kRoleBatchBinding.model_schema);
    role_binding.set("source_cases", static_cast<int>(kRoleBatchCases));
    role_binding.set("validated", projection_validated);
    role_binding.set("consumption_validated", role_consumption_validated);
    role_binding.set("scope", kRoleBatchScope);
    role_binding.set(
        "applications", static_cast<int>(std::ranges::count_if(rows, [](const auto& row) {
            return !row.projection_effective.empty() && row.projection_binding == kRoleBatchIndex;
        })));
    bindings.append(role_binding);
    auto raw_binding = root.make_obj();
    raw_binding.set("path", kRaw193Binding.policy_file);
    raw_binding.set("sha256", kRaw193Binding.policy_sha);
    raw_binding.set("source_fact_path", kRaw193Binding.fact_file);
    raw_binding.set("source_fact_sha256", kRaw193Binding.fact_sha);
    raw_binding.set("source_cases", 193);
    raw_binding.set(
        "applications", static_cast<int>(std::ranges::count_if(rows, [](const auto& row) {
            return !row.projection_effective.empty() && row.projection_binding == kRaw193Index;
        })));
    raw_binding.set("validated", projection_validated);
    raw_binding.set("consumption_validated", raw_consumption_validated);
    raw_binding.set("authority_protocol", kRaw193Protocol);
    raw_binding.set("scope", kRaw193Scope);
    bindings.append(raw_binding);
    auto body22_binding = root.make_obj();
    body22_binding.set("path", kBody22Binding.policy_file);
    body22_binding.set("sha256", kBody22Binding.policy_sha);
    body22_binding.set("source_fact_path", kBody22Binding.fact_file);
    body22_binding.set("source_fact_sha256", kBody22Binding.fact_sha);
    body22_binding.set("source_cases", 22);
    body22_binding.set(
        "applications", static_cast<int>(std::ranges::count_if(rows, [](const auto& row) {
            return !row.projection_effective.empty() && row.projection_binding == kBody22Index;
        })));
    body22_binding.set("validated", projection_validated);
    body22_binding.set("consumption_validated", body22_consumption_validated);
    body22_binding.set("authority_protocol", kBody22Protocol);
    body22_binding.set("scope", kBody22Scope);
    bindings.append(body22_binding);
    root.add("projection_bindings", bindings);
    auto integrity = root.make_arr();
    for (const auto& error : integrity_errors)
        integrity.append(root.make_str(error));
    root.add("integrity_errors", integrity);
    auto cases = root.make_arr();
    using Counts = std::array<std::map<std::string, int>, 3>;
    std::map<std::pair<std::string, std::string>, Counts> totals;
    for (const auto& row : rows) {
        auto entry = root.make_obj();
        entry.set("fixture", row.path);
        entry.set("track", row.track);
        entry.set("type", row.type);
        entry.set("source_sha256", row.source_sha256);
        entry.set("expected", row.expected);
        entry.set("reason", row.reason);
        auto outcomes = root.make_obj();
        auto expected_states = root.make_obj();
        auto decision_reasons = root.make_obj();
        constexpr std::array<const char*, 3> labels{"wide_240", "narrow_40", "restored_240"};
        for (std::size_t i = 0; i < labels.size(); ++i) {
            outcomes.set(labels[i], row.status[i]);
            expected_states.set(labels[i], row.expected_states[i]);
            decision_reasons.set(labels[i], row.decision_reasons[i]);
            ++totals[{row.track, row.type}][i][row.status[i]];
            ++totals[{row.track, "ALL"}][i][row.status[i]];
        }
        entry.add("observed", outcomes);
        entry.add("expected_per_state", expected_states);
        entry.add("decision_reasons", decision_reasons);
        auto errors = root.make_arr();
        for (const auto& error : row.errors)
            errors.append(root.make_str(error));
        entry.add("errors", errors);
        if (row.raw_active_schema == kOfficialEr206Contract ||
            row.raw_active_schema == kOfficialEr107Contract) {
            std::optional<Observation> retained;
            if (row.raw_active_schema == kOfficialEr107Contract)
                retained.emplace(official_er206_legacy_view(row));
            const auto& er_row = retained ? *retained : row;
            const auto metadata = json::parse(er_row.raw_source_constraints);
            require(metadata.has_value(), "OfficialER206 provenance JSON");
            require(er_documents != nullptr, "OfficialER206 provenance source authority lifetime");
            OfficialErCounts checked;
            official_er_accumulate_owned(metadata->root(), er_row, checked, *er_documents);
            auto provenance = root.make_obj();
            provenance.set("contract", kOfficialEr206Contract);
            provenance.set("source_case_ordinal", static_cast<int64_t>(er_row.role_case_index));
            provenance.set("source_authority", kOfficialEr206Authority);
            provenance.set("source_authority_sha256", kOfficialEr206AuthoritySha);
            provenance.set("source_review_sha256",
                           "6843f46ee38a55d3d75358f421770df59dc762a373a97753c3bf68b526ecc12a");
            provenance.set("manifest_sha256", kManifestSha256);
            provenance.set("foreign_assertions_certified", 0);
            provenance.set("glyph_golden_required", false);
            provenance.set("source_metadata_config_and_foreign_debts",
                           "All original20-key source fields retained by canonical row SHA and "
                           "immutable authority ordinal");
            provenance.set("actual_UTF8_row_arrays_file",
                           "mermaid-official-er206-whole-source.jsonl");
            if (er_row.role_model_sha.empty())
                provenance.add("whole_Model_sha256", provenance.make_null());
            else
                provenance.set("whole_Model_sha256", er_row.role_model_sha);
            if (row.projection_baseline.empty())
                provenance.add("original_manifest_expectation", provenance.make_null());
            else {
                const auto baseline = json::parse(row.projection_baseline);
                require(baseline.has_value(), "OfficialER206 original raw policy JSON");
                provenance.add("original_manifest_expectation",
                               document.copy_val(baseline->root()));
            }
            provenance.add("actual", document.copy_val(metadata->root()));
            entry.add("ER_native_whole_source", provenance);
        }
        if (row.raw_active_schema == kOfficialFlow400Contract) {
            const auto metadata = json::parse(row.raw_source_constraints),
                       baseline = json::parse(row.projection_baseline);
            require(metadata.has_value() && baseline.has_value(),
                    "OfficialFlow400 owned provenance JSON");
            auto provenance = root.make_obj();
            provenance.set("contract", kOfficialFlow400Contract);
            provenance.set("source_case_ordinal", static_cast<int64_t>(row.role_case_index));
            provenance.set("source_authority_sha256", flow_oracle::kFlow400AssetSha);
            provenance.add("source_supplement_sha256",
                           document.copy_val(metadata->root().get("source_supplement_sha256")));
            provenance.set("source_manifest_sha256", flow_oracle::kFlow400ManifestSha);
            if (row.role_model_sha.empty())
                provenance.add("whole_Model_sha256", provenance.make_null());
            else
                provenance.set("whole_Model_sha256", row.role_model_sha);
            provenance.set("source_authority", row.raw_source_authority);
            provenance.set("foreign_assertions_certified", 0);
            provenance.set("original_foreign_debts",
                           "retained_by_original_source_mapping_and_manifest_ordinal");
            provenance.set("glyph_golden_required", false);
            provenance.set("actual_frames_file", "mermaid-official-flow400-whole-source.jsonl");
            provenance.add("original_manifest_expectation", document.copy_val(baseline->root()));
            provenance.add("actual", document.copy_val(metadata->root()));
            entry.add("Flow_native_whole_source", provenance);
        }
        if (row.raw_active_schema == kFlowRepeatBody2Contract) {
            const auto metadata = json::parse(row.raw_source_constraints),
                       baseline = json::parse(row.projection_baseline);
            require(metadata.has_value() && baseline.has_value() && row.role_case_index < 2u,
                    "FlowBody2 owned provenance JSON");
            const auto& pin = kFlowRepeatBody2Pins[row.role_case_index];
            auto provenance = root.make_obj();
            provenance.set("contract", kFlowRepeatBody2Contract);
            provenance.set("proof_reuse", "same_run_byte_identical_original");
            provenance.set("body_separate_capture", false);
            provenance.set("original_flow_ordinal", static_cast<int64_t>(pin.ordinal));
            provenance.set("manifest_global_ordinal", static_cast<int64_t>(pin.global));
            provenance.set("diagnostic_offset", 0);
            provenance.set("source_authority_sha256", flow_oracle::kFlow400AssetSha);
            provenance.set("source_manifest_sha256", flow_oracle::kFlow400ManifestSha);
            provenance.set("body_source_gate_sha256",
                           "d53761bc94835603fb5dbca1ebf70707bc1dc52cd998a67a89f4620ae2dd7b56");
            provenance.set("whole_Model_sha256", pin.model_sha);
            provenance.set("body_sha256", pin.source_sha);
            provenance.set("foreign_assertions_certified", 0);
            provenance.set("actual_frames_file", "mermaid-official-flow400-whole-source.jsonl");
            provenance.set("actual_frames_reference",
                           "same-run original ordinal " + std::to_string(pin.ordinal));
            provenance.add("historical_diagnostic_expectation",
                           document.copy_val(baseline->root()));
            provenance.add("same_run_body_proof", document.copy_val(metadata->root()));
            entry.add("Flow_repeat_body_same_run", provenance);
        }
        if (row.raw_active_schema == kGroupEndpoint15BodyContract) {
            const auto metadata = json::parse(row.raw_source_constraints),
                       baseline = json::parse(row.projection_baseline);
            require(metadata.has_value() && baseline.has_value() &&
                        group_endpoint15_documents != nullptr,
                    "OfficialFlow400 GroupEndpoint15 body provenance JSON/lifetime");
            auto provenance = root.make_obj();
            provenance.set("contract", kGroupEndpoint15BodyContract);
            provenance.set("track", "body_diagnostic");
            provenance.set("raw_source_authority", row.raw_source_authority);
            provenance.set("body_sha256", row.role_body_sha);
            provenance.set("whole_Model_sha256", row.role_model_sha);
            provenance.set("source_supplement_sha256",
                           flow_oracle::kFlow400GroupEndpoint15AssetSha);
            provenance.set("original_files_added", 0);
            provenance.set("foreign_assertions_certified", 0);
            provenance.set("actual_frames_file", "mermaid-official-flow400-whole-source.jsonl");
            provenance.set("native_report_input", false);
            provenance.add("historical_diagnostic_expectation",
                           document.copy_val(baseline->root()));
            provenance.add("actual", document.copy_val(metadata->root()));
            entry.add("Flow_group_endpoint15_body", provenance);
        }
        if (row.raw_active_schema == kFlowShapeAliasBodyContract) {
            const auto metadata = json::parse(row.raw_source_constraints),
                       baseline = json::parse(row.projection_baseline);
            require(metadata.has_value() && baseline.has_value() &&
                        official_flow_shape_alias_body_owned(row),
                    "Flow shape-alias body source-owned provenance");
            auto provenance = root.make_obj();
            provenance.set("contract", kFlowShapeAliasBodyContract);
            provenance.set("source_authority", kFlowShapeAliasBodyAuthority);
            provenance.set("track", "body_diagnostic");
            const auto* body_case = flow_oracle::flow_shape_alias_body_case(row.role_case_index);
            require(body_case != nullptr, "Flow shape-alias body report authority");
            provenance.set("source_case_ordinal", static_cast<int64_t>(body_case->ordinal));
            provenance.set("manifest_global_ordinal", static_cast<int64_t>(body_case->global));
            provenance.set("source_manifest_sha256", flow_oracle::kFlow400ManifestSha);
            provenance.set("whole_model_sha256", row.role_model_sha);
            provenance.set("body_sha256", row.role_body_sha);
            provenance.set("source_fallback_credit", 0);
            provenance.set("original_files_added", 0);
            provenance.add("historical_diagnostic_expectation",
                           document.copy_val(baseline->root()));
            provenance.add("actual", document.copy_val(metadata->root()));
            entry.add("Flow_shape_alias_body", provenance);
        }
        if (row.raw_active_schema == kStageCContract) {
            const auto metadata = json::parse(row.raw_source_constraints);
            require(metadata.has_value(), "StageC owned provenance JSON");
            auto provenance = root.make_obj();
            provenance.set("contract", kStageCContract);
            provenance.set("source_case_ordinal", static_cast<std::int64_t>(row.role_case_index));
            provenance.set("source_authority", row.raw_source_authority);
            if (row.role_model_sha.empty())
                provenance.add("whole_Model_sha256", provenance.make_null());
            else
                provenance.set("whole_Model_sha256", row.role_model_sha);
            provenance.set("source_facts_sha256", relations_oracle::kOfficialClass285Sha);
            provenance.set("mapping_approval_sha256",
                           relations_oracle::kOfficialClass285ApprovalSha);
            provenance.set("dagre38_sha256", relations_oracle::kOfficialClass285DagreSha);
            provenance.set("classifier26_sha256", relations_oracle::kOfficialClass285ClassifierSha);
            provenance.set("unused_classDef3_sha256", relations_oracle::kOfficialClass285UnusedSha);
            provenance.set("Notes18_fixture_sha256", relations_oracle::kClassNotes18Sha);
            provenance.set("Notes18_source_review_sha256",
                           "5203db85e0b1c421079e5dd41b1fd8f430df2f3908faf9da877a1ff1ba0ba040");
            provenance.set("foreign_assertions_certified", 0);
            provenance.set("glyph_golden_required", false);
            provenance.set("original_foreign_debts", "retained_by_existing_authority_and_ordinal");
            provenance.add("actual", document.copy_val(metadata->root()));
            entry.add("Class_native_whole_source", provenance);
        }
        if (row.raw_active_schema == kOfficialRequirement11Contract) {
            const auto metadata = json::parse(row.raw_source_constraints);
            require(metadata.has_value() && requirement11_documents != nullptr,
                    "Requirement11 owned provenance JSON/lifetime");
            Requirement11Counts checked;
            requirement11_accumulate_owned(metadata->root(), row, checked,
                                           *requirement11_documents);
            auto provenance = root.make_obj();
            provenance.set("contract", kOfficialRequirement11Contract);
            provenance.set("source_case_ordinal", static_cast<int64_t>(row.role_case_index));
            provenance.set("source_authority", kOfficialRequirement11Authority);
            provenance.set("source_authority_sha256", kOfficialRequirement11AuthoritySha);
            provenance.set("models_asset_sha256", kOfficialRequirement11ModelsSha);
            provenance.set("envelopes_asset_sha256", kOfficialRequirement11EnvelopesSha);
            provenance.set("official_manifest_sha256", kManifestSha256);
            provenance.set("foreign_assertions_certified", 0);
            provenance.set("glyph_golden_required", false);
            provenance.set("actual_UTF8_row_arrays_file",
                           "mermaid-official-requirement11-whole-source.jsonl");
            provenance.set("geometry_authority", "actual FTXUI Screen::PixelAt Grid cells; "
                                                 "published UTF8 rows are reproduction evidence");
            provenance.set("native_report_input", false);
            const auto baseline = json::parse(row.projection_baseline);
            require(baseline.has_value(), "Requirement11 original manifest provenance JSON");
            provenance.add("original_manifest_expectation", document.copy_val(baseline->root()));
            provenance.add("actual", document.copy_val(metadata->root()));
            entry.add("Requirement11_native_whole_source", provenance);
        }
        if (row.raw_active_schema == kOfficialPie7Contract) {
            const auto metadata = json::parse(row.raw_source_constraints);
            require(metadata.has_value() && pie7_documents != nullptr,
                    "OfficialPie7 owned provenance JSON/lifetime");
            OfficialPie7Counts checked;
            official_pie7_accumulate_owned(metadata->root(), row, checked, *pie7_documents);
            auto provenance = root.make_obj();
            provenance.set("contract", kOfficialPie7Contract);
            provenance.set("source_case_ordinal", static_cast<int64_t>(row.role_case_index));
            provenance.set("source_authority", kOfficialPie7Authority);
            provenance.set("source_authority_sha256", kOfficialPie7AuthoritySha);
            provenance.set("Model_canonical_sha256", row.role_model_sha);
            provenance.set("official_manifest_sha256", kManifestSha256);
            provenance.set("foreign_assertions_certified", 0);
            provenance.set("glyph_golden_required", false);
            provenance.set("actual_color_cells_file", "mermaid-official-pie7-whole-source.jsonl");
            provenance.set("geometry_authority", "actual FTXUI Screen::PixelAt color cells; "
                                                 "published rows are reproduction evidence");
            provenance.set("native_report_input", false);
            const auto baseline = json::parse(row.projection_baseline);
            require(baseline.has_value(), "OfficialPie7 original manifest provenance JSON");
            provenance.add("original_manifest_expectation", document.copy_val(baseline->root()));
            provenance.add("actual", document.copy_val(metadata->root()));
            entry.add("Pie7_native_whole_source", provenance);
        }
        if (row.raw_active_schema == kOfficialEr107Contract) {
            const bool ready = official_er107_owned(row);
            const auto metadata = json::parse(row.raw_source_constraints);
            require(metadata.has_value(), "ER107 Matrix source-bound proof JSON");
            auto proof = root.make_obj();
            proof.set("contract", kOfficialEr107Contract);
            proof.set("source_case_ordinal", static_cast<int64_t>(row.role_case_index));
            proof.set("actual_color_cells_file", kOfficialEr107MatrixEvidence);
            proof.set("local_ready", ready);
            proof.set("admitted", false);
            proof.set("join_required", true);
            proof.set("foreign_assertions_certified", 0);
            proof.set("native_report_input", false);
            const auto baseline = json::parse(row.projection_baseline);
            require(baseline.has_value(), "ER107 original manifest expectation JSON");
            proof.add("original_manifest_expectation", document.copy_val(baseline->root()));
            proof.set("original_foreign_obligations",
                      "Retained in frozen ER206, B1 complete-source, presentation and terminal "
                      "Models; no browser RGB/CSS/SVG/font certification.");
            proof.add("actual", document.copy_val(metadata->root()));
            entry.add("ER107_native_whole_source", proof);
        }
        if (row.raw_active_schema == kBlock58Contract) {
            const bool ready = official_block58_owned(row);
            const auto metadata = json::parse(row.raw_source_constraints);
            require(metadata.has_value(), "Block58 owned provenance JSON");
            auto proof = root.make_obj();
            proof.set("contract", kBlock58Contract);
            proof.set("source_case_ordinal", static_cast<int64_t>(row.role_case_index));
            proof.set("actual_color_cells_file", official_block58::kEvidence);
            proof.set("local_ready", ready);
            proof.set("admitted", false);
            proof.set("join_required", true);
            proof.set("foreign_assertions_certified", 0);
            proof.set("native_report_input", false);
            const auto baseline = json::parse(row.projection_baseline);
            require(baseline.has_value() && baseline->root().is_obj(),
                    "Block58 original manifest expectation JSON");
            proof.add("original_manifest_expectation", document.copy_val(baseline->root()));
            const auto& pin = block58::kPins.at(row.role_case_index);
            auto obligations = root.make_obj();
            obligations.set("status", "retained_not_certified");
            obligations.set("foreign_assertions_certified", 0);
            obligations.set("foreign_rgb_css_svg_assertions_certified", false);
            obligations.set(
                "scope",
                "Original upstream semantic/config/style/assertion obligations remain attached to "
                "their immutable manifest record and complete source Models; native terminal "
                "projection does not certify foreign RGB, CSS or SVG equivalence.");
            auto manifest_ref = root.make_obj();
            manifest_ref.set("path", "tests/fixtures/mermaid_official/manifest.json");
            manifest_ref.set("sha256", block58::kManifestSha);
            manifest_ref.set("json_pointer", "/fixtures/" + std::to_string(pin.global));
            manifest_ref.set("record_sha256", pin.manifest_record_sha);
            obligations.add("source_manifest", manifest_ref);
            auto source_ref = root.make_obj();
            source_ref.set(
                "path", "tests/fixtures/mermaid_native/block58_complete_source_proposal.v1.json");
            source_ref.set("sha256", block58::kSourceFactsSha);
            source_ref.set("json_pointer",
                           "/cases/" + std::to_string(row.role_case_index) + "/complete_model");
            source_ref.set("model_sha256", pin.source_model_sha);
            obligations.add("complete_source_model", source_ref);
            auto terminal_ref = root.make_obj();
            terminal_ref.set(
                "path",
                "tests/fixtures/mermaid_native/block58_terminal_projection_proposal.v1.json");
            terminal_ref.set("sha256", block58::kTerminalFactsSha);
            terminal_ref.set("json_pointer", "/cases/" + std::to_string(row.role_case_index) +
                                                 "/proposed_terminal_model");
            terminal_ref.set("model_sha256", pin.terminal_model_sha);
            obligations.add("terminal_projection_model", terminal_ref);
            proof.add("original_foreign_obligations", obligations);
            proof.add("actual", document.copy_val(metadata->root()));
            entry.add("Block58_native_whole_source", proof);
        }
        if (row.raw_active_schema == kMindmap10Contract) {
            require(mindmap_facts != nullptr, "Mindmap10 owned facts lifetime");
            const bool admitted = official_mindmap_owned(row, mindmap_facts->root());
            const auto metadata = json::parse(row.raw_source_constraints);
            require(metadata.has_value(), "Mindmap10 metadata JSON");
            auto proof = root.make_obj();
            proof.set("contract", kMindmap10Contract);
            proof.set("actual_color_cells_file", "mermaid-official-mindmap10.jsonl");
            proof.set("local_ready", admitted);
            proof.set("native_report_input", false);
            proof.add("actual", document.copy_val(metadata->root()));
            entry.add("Mindmap10_native_whole_source", proof);
        }
        if (row.raw_active_schema == kShapes4Contract) {
            require(shapes4_facts != nullptr, "MindmapShapes4 owned facts lifetime");
            const bool admitted = official_shapes4_owned(row, shapes4_facts->root());
            const auto metadata = json::parse(row.raw_source_constraints);
            require(metadata.has_value(), "MindmapShapes4 metadata JSON");
            auto proof = root.make_obj();
            proof.set("contract", kShapes4Contract);
            proof.set("actual_color_cells_file", "mermaid-official-shapes4.jsonl");
            proof.set("local_ready", admitted);
            proof.set("native_report_input", false);
            proof.add("actual", document.copy_val(metadata->root()));
            entry.add("MindmapShapes4_native_whole_source", proof);
        }
        if (!row.projection_effective.empty()) {
            auto baseline = json::parse(row.projection_baseline);
            auto effective = json::parse(row.projection_effective);
            require(baseline && effective, "Cannot serialize validated projection provenance");
            auto provenance = root.make_obj();
            if (row.projection_binding == kBody22Index) {
                provenance.set("supplement_sha256", kBody22Binding.policy_sha);
                provenance.set("source_fact_sha256", kBody22Binding.fact_sha);
                provenance.set("full_model_sha256", row.role_model_sha);
                provenance.set("source_case_id", row.role_case_id);
                provenance.set("source_case_index", static_cast<int>(row.role_case_index));
                provenance.set("body_sha256", row.role_body_sha);
                provenance.set("scope", kBody22Scope);
                provenance.set("authority_protocol", kBody22Protocol);
                provenance.set("source_model_schema", row.raw_active_schema);
                provenance.set("number_encoding", "DisplayText");
                provenance.add("source_note_alignment", provenance.make_null());
                auto authority = json::parse(row.raw_source_authority),
                     constraints = json::parse(row.raw_source_constraints);
                require(authority && constraints, "Cannot serialize Body22 provenance");
                provenance.add("whole_source_authority", document.copy_val(authority->root()));
                provenance.add("source_rect_constraints", document.copy_val(constraints->root()));
            } else if (row.projection_binding == kRaw193Index) {
                provenance.set("supplement_sha256", kRaw193Binding.policy_sha);
                provenance.set("source_fact_sha256", kRaw193Binding.fact_sha);
                provenance.set("full_model_sha256", row.role_model_sha);
                provenance.set("source_case_id", row.role_case_id);
                provenance.set("source_case_index", static_cast<int>(row.role_case_index));
                provenance.set("body_sha256", row.role_body_sha);
                provenance.set("scope", kRaw193Scope);
                provenance.set("authority_protocol", kRaw193Protocol);
                auto authority = json::parse(row.raw_source_authority),
                     schema = json::parse(row.raw_active_schema),
                     constraints = json::parse(row.raw_source_constraints);
                require(authority && schema && constraints, "Cannot serialize raw193 provenance");
                provenance.add("whole_source_authority", document.copy_val(authority->root()));
                provenance.add("active_model_schema", document.copy_val(schema->root()));
                provenance.add("source_rect_constraints", document.copy_val(constraints->root()));
            } else if (row.projection_binding == kRoleBatchIndex) {
                provenance.set("supplement_sha256", kRoleBatchBinding.policy_sha);
                provenance.set("source_fact_sha256", kRoleBatchBinding.fact_sha);
                provenance.set("full_model_sha256", row.role_model_sha);
                provenance.set("source_case_id", row.role_case_id);
                provenance.set("source_case_index", static_cast<int>(row.role_case_index));
                provenance.set("body_sha256", row.role_body_sha);
                provenance.set("scope", role_batch_case_scope(row.role_case_index));
            } else {
                const auto& binding = kProjectionBindings.at(row.projection_binding);
                provenance.set("supplement_sha256", binding.policy_sha);
                provenance.set("source_fact_sha256", binding.fact_sha);
                provenance.set("full_model_sha256", binding.model_sha);
                if (binding.kind == ProjectionKind::XYOriginal) {
                    provenance.set("scope", kXYProjectionScope);
                    provenance.set("foreign_svg_title_omission_font_browser_predicates",
                                   "non_equivalent_pending");
                }
            }
            provenance.add("baseline", document.copy_val(baseline->root()));
            provenance.add("effective", document.copy_val(effective->root()));
            entry.add("reviewed_projection", provenance);
        }
        cases.append(entry);
    }
    root.add("cases", cases);
    serialize_matrix_gap_rows(root, matrix_mandatory_gap_rows(rows, nullptr));
    int originals = 0, originals_drawn = 0;
    const auto classes = stage_c_counts(rows);
    const auto flows = official_flow_counts(
        rows, typed_source4_documents ? typed_source4_documents->supplement.root() : JsonVal{},
        classic_documents ? classic_documents->supplement.root() : JsonVal{},
        linkprofile3_documents ? linkprofile3_documents->supplement.root() : JsonVal{},
        group_endpoint15_documents ? group_endpoint15_documents->asset.root() : JsonVal{});
    const auto ers = official_er_counts(rows, er_documents);
    const auto requirements = requirement11_counts(rows, requirement11_documents);
    const auto pies = official_pie7_counts(rows, pie7_documents);
    for (const auto& row : rows)
        if (row.track == "original") {
            ++originals;
            // Class formal coverage requires the complete whole-source contract;
            // per-state aggregates above still preserve an independently good wide.
            if (row.raw_active_schema == kStageCContract ||
                row.raw_active_schema == kOfficialFlow400Contract ||
                row.raw_active_schema == kOfficialEr206Contract ||
                row.raw_active_schema == kOfficialRequirement11Contract ||
                row.raw_active_schema == kOfficialPie7Contract ||
                row.raw_active_schema == kMindmap10Contract ||
                row.raw_active_schema == kShapes4Contract ||
                row.raw_active_schema == kBlock58Contract ||
                row.raw_active_schema == kOfficialEr107Contract)
                continue;
            if (row.status[0] == "drawn") ++originals_drawn;
        }
    originals_drawn += static_cast<int>(classes.admitted);
    originals_drawn += static_cast<int>(flows.admitted);
    originals_drawn += static_cast<int>(ers.admitted);
    originals_drawn += static_cast<int>(requirements.admitted);
    originals_drawn += static_cast<int>(pies.admitted);
    std::size_t mindmap_selected = 0, mindmap_admitted = 0;
    for (const auto& row : rows)
        if (row.raw_active_schema == kMindmap10Contract) {
            ++mindmap_selected;
            require(mindmap_facts != nullptr, "Mindmap10 summary facts");
            mindmap_admitted += official_mindmap_owned(row, mindmap_facts->root());
        }
    auto mindmap_summary = root.make_obj();
    mindmap_summary.set("selected", static_cast<int64_t>(mindmap_selected));
    mindmap_summary.set("local_ready", static_cast<int64_t>(mindmap_admitted));
    mindmap_summary.set("admitted", 0);
    mindmap_summary.set("join_required", true);
    mindmap_summary.set("base_admitted_originals", originals_drawn);
    mindmap_summary.set("source_facts_sha256", mindmap10::kFactsSha);
    mindmap_summary.set("foreign_assertions_certified", 0);
    root.add("Mindmap10_native_whole_source", mindmap_summary);
    std::size_t shapes4_selected = 0, shapes4_ready = 0;
    for (const auto& row : rows)
        if (row.raw_active_schema == kShapes4Contract) {
            ++shapes4_selected;
            require(shapes4_facts != nullptr, "Shapes4 summary facts");
            shapes4_ready += official_shapes4_owned(row, shapes4_facts->root());
        }
    auto shapes4_summary = root.make_obj();
    shapes4_summary.set("selected", static_cast<int64_t>(shapes4_selected));
    shapes4_summary.set("local_ready", static_cast<int64_t>(shapes4_ready));
    shapes4_summary.set("admitted", 0);
    shapes4_summary.set("base_admitted_originals", 626);
    shapes4_summary.set("join_required", true);
    shapes4_summary.set("prerequisite_join", "Mindmap10_native_whole_source");
    shapes4_summary.set("source_facts_sha256", shapes4::kFactsSha);
    shapes4_summary.set("foreign_assertions_certified", 0);
    root.add("MindmapShapes4_native_whole_source", shapes4_summary);
    int block_selected = 0, block_ready = 0;
    for (const auto& row : rows)
        if (row.raw_active_schema == kBlock58Contract) {
            ++block_selected;
            block_ready += official_block58_owned(row);
        }
    auto block_summary = root.make_obj();
    block_summary.set("selected", block_selected);
    block_summary.set("local_ready", block_ready);
    block_summary.set("admitted", 0);
    block_summary.set("join_required", true);
    block_summary.set("base_admitted_originals", 630);
    block_summary.set("source_facts_sha256", block58::kSourceFactsSha);
    block_summary.set("terminal_facts_sha256", block58::kTerminalFactsSha);
    block_summary.set("foreign_assertions_certified", 0);
    block_summary.set("native_report_input", false);
    block_summary.set("prerequisite_join", "MindmapShapes4_native_whole_source");
    root.add("Block58_native_whole_source", block_summary);
    int er107_selected = 0, er107_ready = 0;
    for (const auto& row : rows)
        if (row.raw_active_schema == kOfficialEr107Contract) {
            ++er107_selected;
            er107_ready += official_er107_owned(row);
        }
    auto er107_summary = root.make_obj();
    er107_summary.set("contract", kOfficialEr107Contract);
    er107_summary.set("selected", er107_selected);
    er107_summary.set("local_ready", er107_aggregate_validated ? er107_ready : 0);
    er107_summary.set("admitted", 0);
    er107_summary.set("base_admitted_originals", 688);
    er107_summary.set("join_required", true);
    er107_summary.set("aggregate_after_all_captures", er107_aggregate_validated);
    er107_summary.set("root_components_validated", er107_aggregate_validated);
    er107_summary.set("source_facts_sha256", er107::kFactsSha);
    er107_summary.set("projection_facts_sha256", er107::authority::kProjectionSha);
    er107_summary.set("presentation_facts_sha256", er107::authority::kPresentationSha);
    er107_summary.set("contract_sha256", er107::authority::kContractSnapshotSha);
    er107_summary.set("manifest_sha256", er107::kManifestSha);
    er107_summary.set("source_lock_sha256", er107::kSourceLock);
    er107_summary.set("foreign_assertions_certified", 0);
    er107_summary.set("native_report_input", false);
    er107_summary.set("drawing_credit", 0);
    er107_summary.set("formal_admission", false);
    er107_summary.set("actual_color_cells_file", kOfficialEr107MatrixEvidence);
    er107_summary.set("prerequisite_join", "Block58_native_whole_source");
    root.add("ER107_native_whole_source", er107_summary);
    auto coverage = root.make_obj();
    coverage.set("drawn_at_240", originals_drawn);
    coverage.set("original_files_executed", originals);
    coverage.set("pinned_original_files", 2145);
    coverage.set("percent_of_pinned_original_files", 100.0 * originals_drawn / 2145.0);
    root.add("original_drawing_coverage", coverage);
    auto class_summary = root.make_obj();
    class_summary.set("contract", kStageCContract);
    class_summary.set("whole_sources", static_cast<std::int64_t>(classes.visited));
    class_summary.set("complete_five_captures", static_cast<std::int64_t>(classes.observed));
    class_summary.set("complete_bindings", static_cast<std::int64_t>(classes.bound));
    class_summary.set("Models_decoded", static_cast<std::int64_t>(classes.decoded));
    class_summary.set("whole_null_pending", static_cast<std::int64_t>(classes.pending));
    class_summary.set("whole_sources_admitted", static_cast<std::int64_t>(classes.admitted));
    class_summary.set("resource_errors", static_cast<std::int64_t>(classes.resources));
    class_summary.set("target_whole_sources_admitted", static_cast<std::int64_t>(285));
    class_summary.set("mandatory_remaining", static_cast<std::int64_t>(48));
    class_summary.set("dagre38_consumed", static_cast<std::int64_t>(classes.dagre));
    class_summary.set("classifier26_consumed", static_cast<std::int64_t>(classes.classifier));
    class_summary.set("unused_classDef3_consumed",
                      static_cast<std::int64_t>(classes.unused_definitions));
    class_summary.set("original_foreign_assertions_certified", 0);
    class_summary.set("glyph_golden_required", false);
    class_summary.set(
        "scope", "Class original terminal whole-scene contract; browser/API/SVG/foreign snapshots "
                 "remain unproved. Per-state Drawn and whole-source admission differ.");
    root.add("Class_native_whole_source", class_summary);
    auto flow_summary = root.make_obj();
    flow_summary.set("contract", kOfficialFlow400Contract);
    flow_summary.set("whole_sources", static_cast<int64_t>(flows.visited));
    flow_summary.set("complete_five_captures", static_cast<int64_t>(flows.observed));
    flow_summary.set("complete_bindings", static_cast<int64_t>(flows.bound));
    flow_summary.set("Models_decoded", static_cast<int64_t>(flows.decoded));
    flow_summary.set("whole_null_pending", static_cast<int64_t>(flows.pending));
    flow_summary.set("whole_sources_admitted", static_cast<int64_t>(flows.admitted));
    flow_summary.set("target_whole_sources_admitted", static_cast<int64_t>(400));
    flow_summary.set("mandatory_remaining", static_cast<int64_t>(flows.pending));
    flow_summary.set("meaningfully_mutated", static_cast<int64_t>(flows.mutated));
    flow_summary.set("resource_errors", static_cast<int64_t>(flows.resources));
    if (classic_documents)
        flow_summary.set("classic_supplement_sha256", flow_oracle::kFlow400ClassicAssetSha);
    else
        flow_summary.add("classic_supplement_sha256", flow_summary.make_null());
    if (group_endpoint15_documents)
        flow_summary.set("group_endpoint15_supplement_sha256",
                         flow_oracle::kFlow400GroupEndpoint15AssetSha);
    else
        flow_summary.add("group_endpoint15_supplement_sha256", flow_summary.make_null());
    flow_summary.set("original_foreign_assertions_certified", 0);
    flow_summary.set("glyph_golden_required", false);
    flow_summary.set("diagram_title_sources",
                     static_cast<int64_t>(std::ranges::count_if(rows, [](const Observation& row) {
                         return row.raw_active_schema == kOfficialFlow400Contract &&
                                row.raw_source_authority ==
                                    "immutable_Flow400_8Models_plus_DiagramTitle1";
                     })));
    flow_summary.set("title_top_margin_sources",
                     static_cast<int64_t>(std::ranges::count_if(rows, [](const Observation& row) {
                         return row.raw_active_schema == kOfficialFlow400Contract &&
                                row.raw_source_authority ==
                                    "immutable_Flow400_8Models_plus_TitleTopMargin1";
                     })));
    flow_summary.set("shape_alias_sources",
                     static_cast<int64_t>(std::ranges::count_if(rows, [](const Observation& row) {
                         return row.raw_active_schema == kOfficialFlow400Contract &&
                                row.raw_source_authority ==
                                    "immutable_Flow400_8Models_plus_FlowShapeAlias16";
                     })));
    flow_summary.set(
        "scope", "Original400 Flow whole-scene contract; immutable Base8, Config21, TypedSource4, "
                 "Classic, LinkProfile3, GroupEndpoint15, DiagramTitle1, TitleTopMargin1, and "
                 "FlowShapeAlias16 source-owned Models are counted only from this consumer's owned "
                 "original rows; body diagnostics and foreign assertions remain separate");
    root.add("Flow_native_whole_source", flow_summary);
    auto er_summary = root.make_obj();
    er_summary.set("contract", kOfficialEr206Contract);
    er_summary.set("whole_sources", static_cast<int64_t>(ers.visited));
    er_summary.set("complete_five_captures", static_cast<int64_t>(ers.observed));
    er_summary.set("five_actual_UTF8_row_arrays_published", static_cast<int64_t>(ers.published));
    er_summary.set("complete_bindings", static_cast<int64_t>(ers.bound));
    er_summary.set("complete_Models_decoded", static_cast<int64_t>(ers.decoded));
    er_summary.set("whole_null_pending", static_cast<int64_t>(ers.pending));
    er_summary.set("whole_sources_admitted", static_cast<int64_t>(ers.admitted));
    er_summary.set("target_whole_sources_admitted", static_cast<int64_t>(206));
    er_summary.set("mandatory_remaining", static_cast<int64_t>(144));
    er_summary.set("meaningfully_mutated", static_cast<int64_t>(ers.mutated));
    er_summary.set("failed_rows", static_cast<int64_t>(ers.errors));
    er_summary.set("mandatory_positive_source_fallbacks", static_cast<int64_t>(ers.unmet));
    er_summary.set("resource_errors", static_cast<int64_t>(ers.resources));
    er_summary.set("valid_empty_admissions", 0);
    er_summary.set("original_foreign_assertions_certified", 0);
    er_summary.set("glyph_golden_required", false);
    er_summary.set(
        "scope",
        "Complete206 original ER sources;62 mandatory whole Models/144 pending. Null art is not "
        "Source/drawing/empty admission; browser/API/SVG assertions remain pending.");
    root.add("ER_native_whole_source", er_summary);
    auto requirement_summary = root.make_obj();
    requirement_summary.set("contract", kOfficialRequirement11Contract);
    requirement_summary.set("whole_sources", static_cast<int64_t>(requirements.visited));
    requirement_summary.set("complete_five_captures", static_cast<int64_t>(requirements.observed));
    requirement_summary.set("five_actual_UTF8_row_arrays_published",
                            static_cast<int64_t>(requirements.published));
    requirement_summary.set("complete_bindings", static_cast<int64_t>(requirements.bound));
    requirement_summary.set("complete_Models_decoded", static_cast<int64_t>(requirements.decoded));
    requirement_summary.set("whole_sources_admitted", static_cast<int64_t>(requirements.admitted));
    requirement_summary.set("target_whole_sources_admitted", static_cast<int64_t>(172));
    requirement_summary.set("mandatory_remaining", static_cast<int64_t>(161));
    requirement_summary.set("meaningfully_mutated", static_cast<int64_t>(requirements.mutated));
    requirement_summary.set("failed_rows", static_cast<int64_t>(requirements.errors));
    requirement_summary.set("resource_errors", static_cast<int64_t>(requirements.resources));
    requirement_summary.set("original_foreign_assertions_certified", 0);
    requirement_summary.set("glyph_golden_required", false);
    requirement_summary.set(
        "scope",
        "Complete11 Requirement originals only; source fallback, native reports, producer seams, "
        "browser/API/SVG assertions and remaining161 originals do not admit coverage.");
    root.add("Requirement11_native_whole_source", requirement_summary);
    auto pie7_summary = root.make_obj();
    pie7_summary.set("contract", kOfficialPie7Contract);
    pie7_summary.set("whole_sources", static_cast<int64_t>(pies.visited));
    pie7_summary.set("complete_five_captures", static_cast<int64_t>(pies.observed));
    pie7_summary.set("five_actual_color_cells_published", static_cast<int64_t>(pies.published));
    pie7_summary.set("complete_bindings", static_cast<int64_t>(pies.bound));
    pie7_summary.set("complete_Models_decoded", static_cast<int64_t>(pies.decoded));
    pie7_summary.set("whole_sources_admitted", static_cast<int64_t>(pies.admitted));
    pie7_summary.set("target_whole_sources_admitted", static_cast<int64_t>(24));
    pie7_summary.set("mandatory_remaining", static_cast<int64_t>(17));
    pie7_summary.set("meaningfully_mutated", static_cast<int64_t>(pies.mutated));
    pie7_summary.set("failed_rows", static_cast<int64_t>(pies.errors));
    pie7_summary.set("resource_errors", static_cast<int64_t>(pies.resources));
    pie7_summary.set("original_foreign_assertions_certified", 0);
    pie7_summary.set("glyph_golden_required", false);
    pie7_summary.set("scope", "Complete7 Pie originals only; remaining17 Pie originals and "
                              "browser/API/SVG assertions do not admit coverage.");
    root.add("Pie7_native_whole_source", pie7_summary);
    const auto body_flows = flow_body2_counts(rows);
    auto body_summary = root.make_obj();
    body_summary.set("contract", kFlowRepeatBody2Contract);
    body_summary.set("applications", static_cast<int64_t>(body_flows.visited));
    body_summary.set("descriptor_bound", static_cast<int64_t>(body_flows.bound));
    body_summary.set("whole_Model_proofs", static_cast<int64_t>(body_flows.decoded));
    body_summary.set("same_run_body_admitted", static_cast<int64_t>(body_flows.admitted));
    body_summary.set("resource_errors", static_cast<int64_t>(body_flows.resources));
    body_summary.set("body_separate_capture", false);
    body_summary.set("original_files_added", 0);
    body_summary.set("foreign_assertions_certified", 0);
    root.add("Flow_repeat_body_same_run", body_summary);
    if (group_endpoint15_documents) {
        const auto group_bodies = group_endpoint15_body_counts(rows, *group_endpoint15_documents);
        auto group_body_summary = root.make_obj();
        group_body_summary.set("contract", kGroupEndpoint15BodyContract);
        group_body_summary.set("applications", static_cast<int64_t>(group_bodies.visited));
        group_body_summary.set("complete_five_captures",
                               static_cast<int64_t>(group_bodies.observed));
        group_body_summary.set("complete_bindings", static_cast<int64_t>(group_bodies.bound));
        group_body_summary.set("complete_Models_decoded",
                               static_cast<int64_t>(group_bodies.decoded));
        group_body_summary.set("body_diagnostics_admitted",
                               static_cast<int64_t>(group_bodies.admitted));
        group_body_summary.set("meaningfully_mutated", static_cast<int64_t>(group_bodies.mutated));
        group_body_summary.set("failed_rows", static_cast<int64_t>(group_bodies.errors));
        group_body_summary.set("resource_errors", static_cast<int64_t>(group_bodies.resources));
        group_body_summary.set("original_files_added", 0);
        group_body_summary.set("foreign_assertions_certified", 0);
        group_body_summary.set(
            "scope", "Fifteen independently captured GroupEndpoint15 body diagnostics. They do not "
                     "add original-file drawing credit and cannot reuse raw-source admission.");
        root.add("Flow_group_endpoint15_body_diagnostic", group_body_summary);
    }
    const auto shape_alias_body_selected = std::ranges::count_if(rows, [](const Observation& row) {
        return row.raw_active_schema == kFlowShapeAliasBodyContract;
    });
    const auto shape_alias_body_admitted = std::ranges::count_if(
        rows, [](const Observation& row) { return official_flow_shape_alias_body_owned(row); });
    auto shape_alias_body_summary = root.make_obj();
    shape_alias_body_summary.set("contract", kFlowShapeAliasBodyContract);
    shape_alias_body_summary.set("selected", static_cast<int64_t>(shape_alias_body_selected));
    shape_alias_body_summary.set("admitted", static_cast<int64_t>(shape_alias_body_admitted));
    shape_alias_body_summary.set("source_fallback_credit", 0);
    shape_alias_body_summary.set("original_files_added", 0);
    shape_alias_body_summary.set("source_authority", kFlowShapeAliasBodyAuthority);
    root.add("Flow_shape_alias_body_diagnostics", shape_alias_body_summary);
    auto aggregates = root.make_arr();
    std::ostringstream markdown;
    markdown << "# Official Mermaid terminal capability matrix\n\n"
             << "Pinned `mermaid@12.2.0` / `1fb7869866dd619168d840291c9c54b4f129758b`. "
             << "Original files and body-only diagnostics have separate denominators. "
             << "Source fallback does not establish drawing compatibility. "
             << "Class originals use independently source-bound whole-scene "
                "geometry/presentation/following and restoration, without glyph goldens; original "
                "foreign assertions remain unproved. "
             << "Every case uses the same Node at 240 → 40 → 240 columns and a following "
                "`AFTER_MERMAID` sentinel.\n\n"
             << "| Track | Type | Width/state | Drawn | Correct source fallback | Error |\n"
             << "|---|---|---|---:|---:|---:|\n";
    constexpr std::array<const char*, 3> labels{"wide_240", "narrow_40", "restored_240"};
    for (const auto& [key, widths] : totals)
        for (std::size_t i = 0; i < widths.size(); ++i) {
            auto count = [&](std::string_view status) {
                const auto found = widths[i].find(std::string(status));
                return found == widths[i].end() ? 0 : found->second;
            };
            auto aggregate = root.make_obj();
            aggregate.set("track", key.first);
            aggregate.set("type", key.second);
            aggregate.set("state", labels[i]);
            for (const auto* status : {"drawn", "source_fallback", "error"})
                aggregate.set(status, count(status));
            aggregates.append(aggregate);
            markdown << "| " << key.first << " | " << key.second << " | " << labels[i] << " | "
                     << count("drawn") << " | " << count("source_fallback") << " | "
                     << count("error") << " |\n";
        }
    root.add("aggregates", aggregates);
    document.set_root(root);
    write_file(path, document.to_pretty_string() + '\n');
    markdown << "\n## Per-input outcomes\n\n| Fixture | Track | Expected | 240 | 40 | Restored 240 "
                "| Declared reason | Narrow decision |\n"
             << "|---|---|---|---|---|---|---|---|\n";
    auto escape = [](std::string text) {
        std::string escaped;
        for (const auto c : text) {
            if (c == '|')
                escaped += "\\|";
            else if (c == '\n' || c == '\r')
                escaped += ' ';
            else
                escaped += c;
        }
        return escaped;
    };
    for (const auto& row : rows)
        markdown << "| " << escape(row.path) << " | " << row.track << " | " << row.expected << " | "
                 << row.status[0] << " | " << row.status[1] << " | " << row.status[2] << " | "
                 << escape(row.reason) << " | " << escape(row.decision_reasons[1]) << " |\n";
    for (const auto& error : integrity_errors)
        markdown << "\nIntegrity error: " << escape(error) << '\n';
    auto markdown_path = path;
    markdown_path.replace_extension(".md");
    write_file(markdown_path, markdown.str());
}

TEST(MermaidCapabilityRoster, Integrity) {
    auto loaded = gap_roster::load_verified_roster();
    ASSERT_TRUE(loaded.has_value());
    for (const auto gate : gap_roster::kGateNames) {
        const auto rows = loaded->gates().get(gate);
        ASSERT_TRUE(rows.is_arr()) << gate;
        std::set<std::string> canonical;
        rows.iter([&](json::JsonVal row) {
            const auto inserted = canonical.insert(gap_roster::canonical_json(row)).second;
            EXPECT_TRUE(inserted) << "duplicate canonical gap row in roster gate " << gate;
        });
    }
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest.has_value());
    std::set<std::string> fixtures;
    manifest->root().get("fixtures").iter([&](json::JsonVal fixture) {
        fixtures.insert(fixture.get_string("path"));
    });
    loaded->gates().get("official_matrix").iter([&](json::JsonVal row) {
        const auto fixture = row.get("fixture").as_str();
        EXPECT_TRUE(fixtures.contains(std::string(fixture)))
            << "roster fixture missing from manifest: " << fixture;
    });
}

TEST(MermaidCapabilityRoster, UnknownMatrixErrorCategoryIsRejected) {
    EXPECT_EQ(
        gap_roster::matrix_error_category("synthetic string that is not a reviewed matrix error"),
        gap_roster::kUnknownCategory);
    EXPECT_EQ(gap_roster::matrix_error_category("Expected exact ordinary-code source fallback"),
              "ordinary_source_fallback_expected");
    EXPECT_EQ(gap_roster::matrix_error_category(
                  "Complete mapped original still uses ordinary source at240"),
              "mapped_positive_still_source");
}

TEST(MermaidOfficialMindmap10, OwnedAdmissionRejectsIncompleteOrBorrowedProofs) {
    const auto raw = read_file(kRoot.parent_path() / std::string(mindmap10::kFactsRelativePath));
    ASSERT_EQ(digest(raw), mindmap10::kFactsSha);
    const auto facts = json::parse(raw), manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(facts && manifest);
    const auto fact = facts->root().get("cases").at(0);
    const auto fixture = manifest->root()
                             .get("fixtures")
                             .at(static_cast<std::size_t>(fact.get_int("manifest_global_ordinal")));
    std::ostringstream actual_frames;
    const auto good =
        run_official_mindmap(facts->root(), manifest->root(), fixture, 0, actual_frames);
    ASSERT_TRUE(good.errors.empty());
    ASSERT_TRUE(official_mindmap_owned(good, facts->root()));
    const auto reject = [&](auto mutate, std::string_view guard) {
        auto parsed = json::parse(good.raw_source_constraints);
        ASSERT_TRUE(parsed);
        json::JsonMutDoc document;
        auto root = document.copy_val(parsed->root());
        document.set_root(root);
        mutate(root);
        auto changed = good;
        changed.raw_source_constraints = document.to_string();
        bool rejected = false;
        try {
            static_cast<void>(official_mindmap_owned(changed, facts->root()));
        } catch (const std::exception& error) {
            EXPECT_NE(std::string_view(error.what()).find(guard), std::string_view::npos)
                << error.what();
            rejected = true;
        }
        EXPECT_TRUE(rejected) << guard;
    };
    reject([](auto root) { root.set("source_facts_sha256", std::string(64, '0')); },
           "source-only proof boundary");
    reject([](auto root) { root.set("complete_model_sha256", std::string(64, '0')); },
           "source/Model/projection join");
    reject([](auto root) { root.set("terminal_projection_sha256", std::string(64, '0')); },
           "source/Model/projection join");
    reject([](auto root) { root.set("captures", 4); }, "mandatory complete geometry");
    reject([](auto root) { root.set("bound_after_captures", false); },
           "mandatory complete geometry");
    reject([](auto root) { root.set("geometry_and_restoration_good", false); },
           "mandatory complete geometry");
    reject([](auto root) { root.set("semantic_mutations_rejected", 0); },
           "mandatory complete geometry");
    reject([](auto root) { root.set("native_report_input", true); }, "source-only proof boundary");
    reject([](auto root) { root.set("narrow_exact_ordinary", true); }, "narrow complete geometry");
    reject([](auto root) { root.set("local_ready", false); }, "failed proof earns no drawing");
    reject([](auto root) { root.set("extra", true); }, "closed proof fields");
    auto body = good;
    body.track = "body_diagnostic";
    EXPECT_THROW(static_cast<void>(official_mindmap_owned(body, facts->root())), std::exception);
}

TEST(MermaidOfficialShapes4, CompleteOriginalsCaptureBeforeBindingAndLocalVerdict) {
    const auto raw = read_file(kRoot.parent_path() / std::string(shapes4::kFactsRelativePath));
    ASSERT_EQ(digest(raw), shapes4::kFactsSha);
    const auto facts = json::parse(raw), manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(facts && manifest);
    std::vector<Observation> rows;
    std::ostringstream frames;
    ASSERT_TRUE(frames.good());
    std::size_t nodes = 0, edges = 0, semantic = 0, physical = 0;
    const auto cases = facts->root().get("cases");
    ASSERT_EQ(cases.size(), 5u);
    for (std::size_t n = 0; n < cases.size(); ++n) {
        const auto fact = cases.at(n);
        const auto path = fact.get_string("path");
        const auto global = static_cast<std::size_t>(fact.get_int("manifest_global_ordinal"));
        const auto selected = official_shapes4_selected(facts->root(), global, path);
        if (n == 0u) {
            EXPECT_FALSE(selected);
            continue;
        }
        ASSERT_EQ(selected, n);
        SCOPED_TRACE(path);
        const auto fixture = manifest->root().get("fixtures").at(global);
        auto row = run_official_shapes4(facts->root(), manifest->root(), fixture, n, frames);
        EXPECT_TRUE(row.errors.empty()) << (row.errors.empty() ? "" : row.errors.front());
        EXPECT_TRUE(official_shapes4_owned(row, facts->root()));
        const auto proof = json::parse(row.raw_source_constraints);
        ASSERT_TRUE(proof);
        semantic += static_cast<std::size_t>(proof->root().get_int("semantic_mutations_rejected"));
        physical += static_cast<std::size_t>(proof->root().get_int("physical_mutations_rejected"));
        nodes += fact.get("complete_model").get("nodes").size();
        edges += fact.get("parent_relations").size();
        rows.push_back(std::move(row));
    }
    ASSERT_TRUE(frames.good());
    ASSERT_EQ(rows.size(), 4u);
    EXPECT_EQ(nodes, 5u);
    EXPECT_EQ(edges, 1u);
    EXPECT_EQ(semantic, 11u);
    EXPECT_EQ(physical, 16u);
    EXPECT_EQ(semantic + physical, 27u);
    const auto compact = json::parse(official_shapes4_verdict(rows, facts->root()));
    ASSERT_TRUE(compact);
    EXPECT_EQ(compact->root().size(), 4u);
    EXPECT_EQ(compact->root().get_string("schema"), "mindmap_shapes4_official_local_verdict/v1");
    EXPECT_EQ(compact->root().get_int("local_ready"), 4);
    compact->root().get("cases").iter([](JsonVal row) { EXPECT_EQ(row.size(), 16u); });
    // Full reproduction evidence is Official-owned, ordered, and outside compact.
    std::istringstream published(frames.str());
    std::string line;
    std::size_t count = 0;
    while (std::getline(published, line)) {
        const auto record = json::parse(line);
        ASSERT_TRUE(record);
        ++count;
        EXPECT_TRUE(record->root().get("captures_before_binding").as_bool());
        const auto five = record->root().get("five_frames");
        ASSERT_EQ(five.size(), 5u);
        constexpr std::array<int, 5> widths{240, 40, 240, 240, 40};
        for (std::size_t n = 0; n < five.size(); ++n) {
            const auto frame = five.at(n);
            EXPECT_EQ(frame.get_int("width"), widths[n]);
            EXPECT_EQ(frame.get("glyph_cells").size(),
                      static_cast<std::size_t>(widths[n] * frame.get_int("height")));
            EXPECT_TRUE(frame.get("foreground_runs").is_arr());
            EXPECT_TRUE(frame.get("background_runs").is_arr());
            EXPECT_EQ(frame.get_string("cell_sha256").size(), 64u);
        }
    }
    EXPECT_EQ(count, 4u);
}

TEST(MermaidOfficialShapes4, OwnedAdmissionRejectsIncompleteOrBorrowedProofs) {
    const auto raw = read_file(kRoot.parent_path() / std::string(shapes4::kFactsRelativePath));
    ASSERT_EQ(digest(raw), shapes4::kFactsSha);
    const auto facts = json::parse(raw), manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(facts && manifest);
    const auto fact = facts->root().get("cases").at(1);
    const auto fixture = manifest->root()
                             .get("fixtures")
                             .at(static_cast<std::size_t>(fact.get_int("manifest_global_ordinal")));
    std::ostringstream actual_frames;
    const auto good =
        run_official_shapes4(facts->root(), manifest->root(), fixture, 1, actual_frames);
    ASSERT_TRUE(good.errors.empty());
    ASSERT_TRUE(official_shapes4_owned(good, facts->root()));
    const auto reject = [&](auto mutate, std::string_view name, std::string_view guard) {
        SCOPED_TRACE(name);
        const auto parsed = json::parse(good.raw_source_constraints);
        ASSERT_TRUE(parsed);
        json::JsonMutDoc document;
        auto root = document.copy_val(parsed->root());
        document.set_root(root);
        mutate(root);
        auto changed = good;
        changed.raw_source_constraints = document.to_string();
        bool rejected = false;
        try {
            static_cast<void>(official_shapes4_owned(changed, facts->root()));
        } catch (const std::exception& error) {
            EXPECT_NE(std::string_view(error.what()).find(guard), std::string_view::npos)
                << error.what();
            rejected = true;
        }
        EXPECT_TRUE(rejected);
    };
    reject([](auto r) { r.set("source_facts_sha256", std::string(64, '0')); }, "stale_asset_hash",
           "source-only proof boundary");
    reject([](auto r) { r.set("source_sha256", std::string(64, '0')); }, "stale_raw_hash",
           "source-only proof boundary");
    reject([](auto r) { r.set("complete_model_sha256", std::string(64, '0')); }, "stale_model_hash",
           "source/Model/projection join");
    reject([](auto r) { r.set("terminal_projection_sha256", std::string(64, '0')); },
           "stale_projection_hash", "source/Model/projection join");
    reject([](auto r) { r.set("native_report_input", true); }, "borrowed_native",
           "source-only proof boundary");
    reject([](auto r) { r.set("foreign_assertions_certified", 1); }, "foreign_assertsite_credit",
           "source-only proof boundary");
    reject([](auto r) { r.set("captures", 4); }, "missing_capture", "mandatory complete geometry");
    reject([](auto r) { r.set("captures_before_binding", false); }, "capture_order",
           "mandatory complete geometry");
    reject([](auto r) { r.set("bound_after_captures", false); }, "bind_order",
           "mandatory complete geometry");
    reject([](auto r) { r.set("geometry_and_restoration_good", false); }, "restoration_failure",
           "mandatory complete geometry");
    reject([](auto r) { r.set("semantic_mutations_rejected", 1); }, "partial_semantic",
           "mandatory complete geometry");
    reject([](auto r) { r.set("physical_mutations_rejected", 2); }, "partial_physical",
           "mandatory complete geometry");
    reject(
        [](auto r) {
            r.set("semantic_mutations_rejected", 3);
            r.set("physical_mutations_rejected", 2);
        },
        "same_total_wrong_split", "mandatory complete geometry");
    reject(
        [](auto r) {
            r.set("narrow_exact_ordinary", true);
            r.set("narrow_geometry_good", true);
        },
        "narrow_both_claimed", "narrow complete geometry");
    reject(
        [](auto r) {
            r.set("narrow_exact_ordinary", false);
            r.set("narrow_geometry_good", false);
        },
        "narrow_neither_proved", "narrow complete geometry");
    reject([](auto r) { r.set("local_ready", false); }, "failed_proof_drawn",
           "failed proof earns no drawing");
    reject([](auto r) { r.set("extra", true); }, "unknown_proof_field", "closed proof fields");
    reject([](auto r) { static_cast<void>(r.remove("captures")); }, "missing_proof_field",
           "closed proof fields");
    reject([](auto r) { r.set("captures", true); }, "wrong_field_type", "proof count");
    reject([](auto r) { r.set("native_report_input", 0); }, "wrong_boolean_type", "proof boolean");
    const auto reject_row = [&](auto mutate, std::string_view name) {
        SCOPED_TRACE(name);
        auto changed = good;
        mutate(changed);
        EXPECT_THROW(static_cast<void>(official_shapes4_owned(changed, facts->root())),
                     std::exception);
    };
    reject_row([](auto& r) { r.track = "body_diagnostic"; }, "body_track");
    reject_row([](auto& r) { r.raw_active_schema = kMindmap10Contract; },
               "borrowed_original10_contract");
    reject_row([](auto& r) { r.errors.emplace_back("geometry: owned row failure"); },
               "owned_errors_fail_admission");
    reject_row([](auto& r) { r.role_case_index = 0; }, "BangCloud_not_admitted");
    reject_row([](auto& r) { r.status[2] = "source_fallback"; }, "partial_restored_state");
    reject_row([](auto& r) { r.source_sha256 = std::string(64, '0'); }, "stale_row_source");
    // Actual consumer failures retain errors and earn no mutation/drawing credit.
    json::JsonMutDoc invalid;
    auto changed = invalid.copy_val(facts->root());
    invalid.set_root(changed);
    changed.set("schema", "invalid");
    std::ostringstream failed_frames;
    const auto invalid_facts = json::parse(invalid.to_string());
    ASSERT_TRUE(invalid_facts);
    const auto failed =
        run_official_shapes4(invalid_facts->root(), manifest->root(), fixture, 1, failed_frames);
    ASSERT_FALSE(failed.errors.empty());
    EXPECT_FALSE(official_shapes4_owned(failed, facts->root()));
    const auto proof = json::parse(failed.raw_source_constraints);
    ASSERT_TRUE(proof);
    EXPECT_EQ(proof->root().get_int("semantic_mutations_rejected"), 0);
    EXPECT_EQ(proof->root().get_int("physical_mutations_rejected"), 0);
    json::JsonMutDoc stale;
    auto stale_fixture = stale.copy_val(fixture);
    stale.set_root(stale_fixture);
    stale_fixture.set("sha256", std::string(64, '0'));
    const auto stale_record = json::parse(stale.to_string());
    ASSERT_TRUE(stale_record);
    const auto source_failed = run_official_shapes4(facts->root(), manifest->root(),
                                                    stale_record->root(), 1, failed_frames);
    ASSERT_FALSE(source_failed.errors.empty());
    EXPECT_FALSE(official_shapes4_owned(source_failed, facts->root()));
    const auto source_proof = json::parse(source_failed.raw_source_constraints);
    ASSERT_TRUE(source_proof);
    EXPECT_EQ(source_proof->root().get_int("semantic_mutations_rejected"), 0);
    EXPECT_EQ(source_proof->root().get_int("physical_mutations_rejected"), 0);
    // Same source-highlighter profile with exact-dispatch diagram bypass. A
    // foreground-only difference must never be accepted as ordinary fallback.
    const auto wrapping = facts->root().get("cases").at(4);
    const auto source = read_file(kRoot / "cases" / wrapping.get_string("path"));
    auto mermaid = loom::ui::render_markdown(fenced(source, "mermaid", kSentinel));
    auto ordinary = loom::ui::render_markdown(fenced(source, "mermaid-source", kSentinel));
    shapes4::Work control_work;
    const auto narrow = official_mindmap_capture(mermaid, 40, 40, control_work);
    auto ordinary_narrow = official_mindmap_capture(ordinary, 40, 40, control_work);
    ASSERT_TRUE(official_mindmap_surface(narrow, ordinary_narrow));
    ASSERT_FALSE(ordinary_narrow.foreground.empty());
    ordinary_narrow.foreground[0] += "foreground-only-control";
    EXPECT_EQ(narrow.grid.cells, ordinary_narrow.grid.cells);
    EXPECT_EQ(narrow.background, ordinary_narrow.background);
    EXPECT_FALSE(official_mindmap_surface(narrow, ordinary_narrow))
        << "foreground_only_mismatch_is_not_exact_ordinary";
}

// Standalone source/proof controls keep all evidence in memory. Only the full
// OfficialMatrix run publishes formal matrix and Block58 local verdict files.
TEST(MermaidOfficialBlock58, CompiledSelectionOwnsEveryCompleteOriginalAndRejectsStaleIdentity) {
    official_block58::Documents documents;
    const auto fixtures = documents.manifest.root().get("fixtures");
    std::set<std::size_t> selected;
    for (std::size_t n = 0; n < fixtures.size(); ++n) {
        const auto fixture = fixtures.at(n);
        const auto pick =
            official_block58::select(n, fixture.get_string("path"), fixture.get_string("sha256"));
        if (pick) EXPECT_TRUE(selected.insert(*pick).second);
    }
    ASSERT_EQ(selected.size(), 58u);
    const auto& pin = block58::kPins.front();
    EXPECT_THROW(static_cast<void>(official_block58::select(
                     static_cast<std::size_t>(pin.global + 1), pin.path, pin.raw_sha)),
                 std::exception);
    EXPECT_THROW(static_cast<void>(official_block58::select(static_cast<std::size_t>(pin.global),
                                                            pin.path, "wrong")),
                 std::exception);
    EXPECT_FALSE(official_block58::select(0, "mindmap/not-a-block.mmd", pin.raw_sha));
}

TEST(MermaidOfficialBlock58, OwnedPublicMarkdownProofRejectsMissingPinsFramesAndBorrowedMetadata) {
    official_block58::Documents documents;
    const auto& pin = block58::kPins.front();
    std::ostringstream frames;
    const auto fixture =
        documents.manifest.root().get("fixtures").at(static_cast<std::size_t>(pin.global));
    const auto good = run_official_block58(documents, fixture, 0, frames);
    ASSERT_TRUE(good.errors.empty()) << (good.errors.empty() ? "" : good.errors.front());
    ASSERT_TRUE(official_block58_owned(good));
    EXPECT_EQ(good.projection_baseline, projection_canonical(fixture.get("raw")));
    const auto actual = json::parse(frames.str());
    ASSERT_TRUE(actual);
    const auto evidence = actual->root();
    for (const auto group : {evidence, evidence.get("cached"), evidence.get("cache_hit")}) {
        const auto captured = group.get("five_frames");
        ASSERT_TRUE(captured.is_arr());
        ASSERT_EQ(captured.size(), 5u);
        for (std::size_t i = 0; i < 5; ++i) {
            const auto frame = captured.at(i);
            EXPECT_TRUE(hex_string(frame.get_string("cell_sha256"), 64));
            std::int64_t cells = 0;
            frame.get("complete_cell_runs").iter([&](JsonVal run) {
                ASSERT_EQ(run.size(), 7u);
                cells += run.at(0).as_int();
            });
            EXPECT_EQ(cells, frame.get_int("width") * frame.get_int("height"));
        }
    }
    const auto rejects = [&](auto mutate) {
        auto changed = good;
        const auto parsed = json::parse(changed.raw_source_constraints);
        ASSERT_TRUE(parsed);
        json::JsonMutDoc doc;
        auto root = doc.copy_val(parsed->root());
        doc.set_root(root);
        mutate(root);
        changed.raw_source_constraints = doc.to_string();
        EXPECT_THROW(static_cast<void>(official_block58_owned(changed)), std::exception);
    };
    for (const auto key :
         {"source_sha256", "complete_model_sha256", "terminal_model_sha256",
          "source_witnesses_sha256", "manifest_record_sha256", "terminal_source_binding_sha256",
          "source_facts_sha256", "terminal_facts_sha256", "official_manifest_sha256",
          "source_lock_sha256", "boundary_sha256", "reviewed_design_sha256"})
        rejects([&](auto root) { root.set(key, "wrong"); });
    for (const auto key :
         {"captures_before_binding", "bound_after_captures", "geometry_and_restoration_good",
          "uncached_cached_five_planes_equal", "cache_hit_observed", "markdown_dim_color"})
        rejects([&](auto root) { root.set(key, false); });
    rejects([](auto root) { root.set("captures", 10); });
    rejects([](auto root) { root.set("inherited_other_attributes", 0); });
    rejects([](auto root) { root.set("manifest_global_ordinal", -1); });
    rejects([](auto root) { root.set("semantic_mutations_rejected", 0); });
    rejects([](auto root) { root.set("physical_mutations_rejected", 0); });
    rejects([](auto root) { root.set("native_report_input", true); });
    rejects([](auto root) { root.set("foreign_assertions_certified", 1); });
    rejects([](auto root) { root.set("drawing_credit", 1); });
    rejects([](auto root) { root.set("foreign_plane", true); });
    rejects([](auto root) { root.set("local_ready", false); });
    auto wrong_row = good;
    wrong_row.track = "body_diagnostic";
    EXPECT_THROW(static_cast<void>(official_block58_owned(wrong_row)), std::exception);
    wrong_row = good;
    wrong_row.raw_active_schema = kShapes4Contract;
    EXPECT_THROW(static_cast<void>(official_block58_owned(wrong_row)), std::exception);
    wrong_row = good;
    ++wrong_row.role_case_index;
    EXPECT_THROW(static_cast<void>(official_block58_owned(wrong_row)), std::exception);
    block58::OracleWork work;
    const auto raw =
        official_block58::read(kRoot / "cases" / std::string(pin.path), block58::kMaxRawBytes);
    EXPECT_THROW(static_cast<void>(documents.bind(pin.path, raw + " ", work)), std::exception);
}

TEST(MermaidOfficialBlock58, DimFalseAndTrueRemainExactPublicMarkdownAttributes) {
    official_block58::Documents documents;
    const auto& pin = block58::kPins.front();
    const auto raw =
        official_block58::read(kRoot / "cases" / std::string(pin.path), block58::kMaxRawBytes);
    block58::OracleWork captures;
    captures.left = 768'000'000;
    const auto plain = official_block58::observe(raw, captures, nullptr, false),
               dim = official_block58::observe(raw, captures, nullptr, true);
    block58::OracleWork binding;
    const auto facts = documents.bind(pin.path, raw, binding);
    block58::OracleWork deriving;
    const auto expected = block58::authority::derive(facts, raw.size(), deriving);
    const auto colors = official_block58::palette();
    block58::OracleWork checking;
    checking.left = 768'000'000;
    const auto a = official_block58::validate(plain, expected, colors, false, checking),
               b = official_block58::validate(dim, expected, colors, true, checking);
    EXPECT_EQ(a, b);
    EXPECT_NE(plain[0], dim[0]);
}

// ER107's independent sidecar does not replace ER206's frozen closed proof,
// change manifest expectations, or publish a formal matrix/join marker.
TEST(MermaidOfficialEr107, CompleteOriginalsOwnFifteenFramesBeforeCaseAndFinalAggregateBinding) {
    official_er107::ThemeGuard theme_guard;
    loom::ui::design::theme::set_theme(loom::ui::design::theme::ThemeVariant::Dark);
    const auto verdict_path =
        fs::path(LOOM_MERMAID_REPORT_DIR) / std::string(official_er107::kVerdict);
    fs::remove(verdict_path);  // An interrupted attempt cannot leave an old ready verdict.
    official_er107::Documents documents;
    std::ofstream full(fs::path(LOOM_MERMAID_REPORT_DIR) / std::string(official_er107::kEvidence),
                       std::ios::binary);
    ASSERT_TRUE(full.good());
    json::JsonMutDoc compact;
    auto root = compact.object(), cases = root.make_arr();
    std::array<bool, 107> captures{};
    std::size_t ready = 0, errors = 0, resources = 0, semantic = 0, physical = 0;
    for (std::size_t i = 0; i < er107::kPins.size(); ++i) {
        SCOPED_TRACE(er107::kPins[i].path);
        const auto result = official_er107::run(documents, i, full);
        captures[i] = result.captured;
        const auto parsed = json::parse(result.proof);
        ASSERT_TRUE(parsed);
        const auto proof = parsed->root();
        EXPECT_EQ(official_er107::owned(proof), result.ready);
        if (result.ready)
            ++ready;
        else {
            ++errors;
            ADD_FAILURE() << proof.get_string("phase") << ": " << proof.get_string("error");
        }
        resources += result.resource;
        semantic += result.semantic;
        physical += result.physical;
        cases.append(compact.copy_val(proof));
    }
    full.flush();
    ASSERT_TRUE(full.good());
    bool aggregate = false;
    try {
        documents.finish(captures);
        aggregate = true;
    } catch (const std::exception& failure) {
        ++errors;
        resources += er107::authority::resource(failure.what());
        root.set("aggregate_error", failure.what());
        ADD_FAILURE() << failure.what();
    }
    root.set("schema", "er107_official_complete_original/v1");
    root.set("contract", official_er107::kContract);
    root.set("source_facts_sha256", er107::kFactsSha);
    root.set("projection_facts_sha256", er107::authority::kProjectionSha);
    root.set("presentation_facts_sha256", er107::authority::kPresentationSha);
    root.set("contract_sha256", er107::authority::kContractSnapshotSha);
    root.set("source_helper_sha256", official_er107::kSourceHelperSha);
    root.set("prior_authority_sha256", er107::kPriorSha);
    root.set("manifest_sha256", er107::kManifestSha);
    root.set("source_lock_sha256", er107::kSourceLock);
    root.set("authority_adaptation", er107::authority::kAdaptation);
    root.set("authority_review_status", er107::authority::kB3ReviewStatus);
    root.set("limitations", er107::authority::kB3Limitations);
    root.set("selected", 107);
    root.set("local_ready", static_cast<std::int64_t>(aggregate ? ready : 0));
    root.set("errors", static_cast<std::int64_t>(errors));
    root.set("resource_errors", static_cast<std::int64_t>(resources));
    root.set("captured_originals", static_cast<std::int64_t>(std::ranges::count(captures, true)));
    root.set("captures_per_original", 15);
    root.set("aggregate_after_all_captures", aggregate);
    root.set("root_components_validated", aggregate);
    root.set("semantic_mutations_rejected", static_cast<std::int64_t>(semantic));
    root.set("physical_mutations_rejected", static_cast<std::int64_t>(physical));
    root.set("foreign_assertions_certified", 0);
    root.set("native_report_input", false);
    root.set("drawing_credit", 0);
    root.set("admitted", 0);
    root.set("formal_admission", false);
    root.set("join_required", true);
    root.set("base_admitted_originals", 688);
    root.set("actual_color_cells_file", official_er107::kEvidence);
    root.add("cases", cases);
    compact.set_root(root);
    write_file(verdict_path, compact.to_string());
    EXPECT_TRUE(aggregate);
    EXPECT_EQ(ready, 107u);
    EXPECT_EQ(errors, 0u);
    EXPECT_EQ(resources, 0u);
}

TEST(MermaidOfficialEr107, ExactOriginalSelectorAndCaptureOrderingCannotBecomePreflightAdmission) {
    official_er107::Documents documents;
    std::set<std::size_t> seen;
    for (std::size_t i = 0; i < er107::kPins.size(); ++i) {
        const auto& pin = er107::kPins[i];
        const auto record =
            documents.manifest.root().get("fixtures").at(static_cast<std::size_t>(pin.global));
        const auto selected =
            official_er107::select(static_cast<std::size_t>(pin.global), record.get_string("path"),
                                   record.get_string("sha256"));
        ASSERT_TRUE(selected);
        EXPECT_EQ(*selected, i);
        EXPECT_TRUE(seen.insert(*selected).second);
    }
    const auto& pin = er107::kPins.front();
    EXPECT_THROW(static_cast<void>(official_er107::select(static_cast<std::size_t>(pin.global + 1),
                                                          pin.path, pin.raw_sha)),
                 std::exception);
    EXPECT_THROW(static_cast<void>(official_er107::select(static_cast<std::size_t>(pin.global),
                                                          pin.path, "wrong")),
                 std::exception);
    EXPECT_FALSE(official_er107::select(0, "er-diagram/not-selected.mmd", pin.raw_sha));
    official_er107::Captures none;
    er107::OracleWork work;
    EXPECT_THROW(static_cast<void>(documents.bind(0, "", none, work)), std::exception);
    std::array<bool, 107> incomplete{};
    EXPECT_THROW(documents.finish(incomplete), std::exception);
}

TEST(MermaidOfficialEr107, OwnedGoodFirstProofRejectsBorrowedMissingAndRecomputedSourceMetadata) {
    official_er107::ThemeGuard theme_guard;
    loom::ui::design::theme::set_theme(loom::ui::design::theme::ThemeVariant::Dark);
    official_er107::Documents documents;
    std::ostringstream frames;
    const auto result = official_er107::run(documents, 0, frames);
    const auto parsed = json::parse(result.proof);
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(result.ready) << parsed->root().get_string("error");
    ASSERT_TRUE(official_er107::owned(parsed->root()));
    const auto actual = json::parse(frames.str());
    ASSERT_TRUE(actual);
    for (const auto group : {"uncached", "cache_miss", "cache_hit"}) {
        const auto five = actual->root().get("capture_groups").get(group).get("five_frames");
        ASSERT_EQ(five.size(), 5u);
        for (std::size_t n = 0; n < 5; ++n) {
            const auto frame = five.at(n);
            EXPECT_TRUE(hex_string(frame.get_string("cell_sha256"), 64));
            std::int64_t area = 0;
            frame.get("complete_cell_runs").iter([&](JsonVal run) {
                ASSERT_EQ(run.size(), 8u);
                area += run.at(0).as_int();
            });
            EXPECT_EQ(area, frame.get_int("width") * frame.get_int("height"));
        }
    }
    const auto reject = [&](auto mutate) {
        json::JsonMutDoc bad;
        auto root = bad.copy_val(parsed->root());
        bad.set_root(root);
        mutate(root);
        const auto changed = json::parse(bad.to_string());
        ASSERT_TRUE(changed);
        EXPECT_THROW(static_cast<void>(official_er107::owned(changed->root())), std::exception);
    };
    for (const auto key :
         {"source_sha256", "source_git_blob", "source_case_sha256", "source_model_sha256",
          "source_witnesses_sha256", "manifest_record_sha256", "prior_ER206_case_sha256",
          "terminal_model_sha256", "whole_scene_model_sha256", "source_binding_sha256",
          "complete_model_sha256", "projection_facts_sha256", "contract_sha256"})
        reject([&](auto root) { root.set(key, "wrong"); });
    for (const auto key :
         {"captures_before_binding", "bound_after_captures", "full_frames_published",
          "geometry_and_restoration_good", "cache_hit_observed",
          "uncached_cached_five_planes_equal", "markdown_dim_color"})
        reject([&](auto root) { root.set(key, false); });
    reject([](auto root) { root.set("captures", 10); });
    reject([](auto root) { root.set("semantic_mutations_rejected", 0); });
    reject([](auto root) { root.set("physical_mutations_rejected", 0); });
    reject([](auto root) { root.set("verification_resource_exhausted", true); });
    reject([](auto root) { root.set("verification_phase_count", 3); });
    reject([](auto root) { root.set("verification_phase_cap", 1024000000); });
    reject([](auto root) {
        root.set("verification_aggregate_cap", static_cast<std::int64_t>(4096000000));
    });
    reject([](auto root) { root.set("native_report_input", true); });
    reject([](auto root) { root.set("foreign_assertions_certified", 1); });
    reject([](auto root) { root.set("drawing_credit", 1); });
    reject([](auto root) { root.set("body_diagnostic", true); });
    reject([](auto root) { root.set("local_ready", false); });
    // Resource failures are distinguished from useful semantic mutant failures.
    const auto& pin = er107::kPins.front();
    const auto raw =
        official_er107::read(kRoot / "cases" / std::string(pin.path), er107::kMaxRawBytes);
    official_er107::Captures captured;
    er107::OracleWork capturing;
    official_er107::observe(captured, raw, capturing);
    er107::OracleWork zero;
    zero.left = 0;
    bool resource = false;
    try {
        static_cast<void>(documents.bind(0, raw, captured, zero));
    } catch (const std::exception& failure) {
        resource = er107::authority::resource(failure.what());
    }
    EXPECT_TRUE(resource);
    er107::OracleWork changed_raw;
    EXPECT_THROW(static_cast<void>(documents.bind(0, raw + " ", captured, changed_raw)),
                 std::exception);
}

TEST(MermaidOfficial, PinnedInventoryIntegrity) {
    EXPECT_NO_THROW(
        validate_manifest(kRoot, read_file(kRoot / "manifest.json"), update_snapshots()));
}

TEST(MermaidCapabilityReport, OfficialMatrix) {
    std::vector<Observation> observations;
    std::vector<std::string> integrity_errors;
    bool projection_validated = false, role_consumption_validated = false,
         raw_consumption_validated = false, body22_consumption_validated = false;
    bool er107_aggregate_validated = false;
    std::optional<flow_oracle::Flow400TypedSource4Documents> official_flow_typed_source4_documents;
    std::optional<flow_oracle::Flow400ClassicDocuments> official_flow_classic_documents;
    std::optional<flow_oracle::Flow400LinkProfile3Documents> official_flow_linkprofile3_documents;
    std::optional<flow_oracle::GroupEndpoint15Documents> official_flow_group_endpoint15_documents;
    relations_oracle::Work official_er_setup;
    official_er_setup.spend(4u * sizeof(std::optional<relations_oracle::Er206Documents>) +
                            8u * sizeof(std::size_t) + 1024u);
    std::optional<relations_oracle::Er206Documents> official_er_documents;
    Work official_requirement11_setup{r11::kSetupLimit};
    official_requirement11_setup.spend(4u * sizeof(std::optional<Requirement11Documents>) +
                                       8u * sizeof(std::size_t) + 1024u);
    std::optional<Requirement11Documents> official_requirement11_documents;
    pie7::Work official_pie7_setup{kOfficialPie7SetupLimit};
    official_pie7_setup.spend(4u * sizeof(std::optional<OfficialPie7Documents>) +
                              8u * sizeof(std::size_t) + 1024u);
    std::optional<OfficialPie7Documents> official_pie7_documents;
    std::optional<json::JsonDoc> official_mindmap_facts, official_shapes4_facts;
    try {
        const auto manifest_bytes = read_file(kRoot / "manifest.json");
        validate_manifest(kRoot, manifest_bytes, update_snapshots());
        auto document = json::parse(manifest_bytes);
        const auto fixtures = document->root().get("fixtures");
        std::vector<json::JsonDoc> policies;
        policies.reserve(kProjectionBindings.size());
        for (const auto& binding : kProjectionBindings) {
            for (const auto file : {binding.policy_file, binding.fact_file, binding.snapshot_file})
                projection_regular_file(kNativeRoot, kNativeRoot / file);
            const auto policy_bytes = read_file(kNativeRoot / binding.policy_file),
                       fact_bytes = read_file(kNativeRoot / binding.fact_file);
            projection_byte_pins(policy_bytes, fact_bytes, binding);
            auto policy = json::parse(policy_bytes), fact_document = json::parse(fact_bytes);
            require(policy && fact_document, "Cannot parse reviewed projection policy/facts");
            const auto path = projection_source_path(fact_document->root(), binding);
            require(path.is_str() && safe_relative(path.as_str()),
                    "Projection unsafe actual source path");
            validate_projection_binding(policy->root(), fact_document->root(), document->root(),
                                        read_file(kRoot / "cases" / path.as_str()),
                                        read_file(kNativeRoot / binding.snapshot_file), binding);
            policies.push_back(std::move(*policy));
        }
        auto role_batch = load_role_batch(document->root());
        RoleBatchConsumption role_counts;
        auto raw_batch = load_raw193(document->root());
        Raw193Consumption raw_counts;
        auto body22_batch = load_body22(document->root(), raw_batch.authorities);
        Body22Consumption body22_counts;
        official_er_documents.emplace(relations_oracle::er206_document(official_er_setup));
        official_requirement11_documents.emplace(
            requirement11_documents(official_requirement11_setup));
        official_requirement11_setup.spend(
            2145u * (128u + 2u * sizeof(std::string_view)) +
            4u * sizeof(std::array<bool, kOfficialRequirement11Pins.size()>) + 65536u);
        std::array<bool, kOfficialRequirement11Pins.size()> official_requirement11_consumed{};
        std::ofstream official_requirement11_diagnostics(
            fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-official-requirement11-whole-source.jsonl",
            std::ios::binary);
        require(static_cast<bool>(official_requirement11_diagnostics),
                "Requirement11 diagnostic output unavailable");
        official_pie7_documents.emplace(load_official_pie7_documents(official_pie7_setup));
        official_pie7_setup.product(2145u, 128u + 2u * sizeof(std::string_view));
        official_pie7_setup.spend(4u * sizeof(std::array<bool, kOfficialPie7Pins.size()>) + 65536u);
        std::array<bool, kOfficialPie7Pins.size()> official_pie7_consumed{};
        std::ofstream official_pie7_diagnostics(fs::path(LOOM_MERMAID_REPORT_DIR) /
                                                    "mermaid-official-pie7-whole-source.jsonl",
                                                std::ios::binary);
        require(static_cast<bool>(official_pie7_diagnostics),
                "OfficialPie7 diagnostic output unavailable");
        // Independent setup owns selector visits/file controls only. Public
        // sources below each own one128M; no setup credit reaches geometry.
        official_er_setup.spend(2145u * (128u + 2u * sizeof(std::string_view)) +
                                4u * sizeof(std::array<bool, 206>) + 65536u);
        std::array<bool, 206> official_er_consumed{};
        std::size_t official_er_ordinal = 0;
        std::ofstream official_er_diagnostics(fs::path(LOOM_MERMAID_REPORT_DIR) /
                                                  "mermaid-official-er206-whole-source.jsonl",
                                              std::ios::binary);
        require(static_cast<bool>(official_er_diagnostics),
                "OfficialER206 diagnostic output unavailable");
        const auto class_documents = relations_oracle::official_class285_documents();
        const auto class_originals = relations_oracle::official_class285_originals();
        // The existing strict400 loader owns independent local documents and setup ledgers.
        auto official_flow_documents = flow_oracle::flow400_documents();
        auto official_flow_config21_documents = flow_oracle::flow400_config21_documents();
        official_flow_typed_source4_documents.emplace(
            flow_oracle::flow400_typed_source4_documents());
        official_flow_classic_documents.emplace(flow_oracle::flow400_classic_documents());
        official_flow_linkprofile3_documents.emplace(flow_oracle::flow400_linkprofile3_documents());
        official_flow_group_endpoint15_documents.emplace(flow_oracle::group_endpoint15_documents());
        official_flow_documents.compact_work.spend(4096u + 4u * sizeof(std::array<bool, 400>) +
                                                   8u * sizeof(std::size_t));
        std::array<bool, 400> official_flow_consumed{};
        std::size_t official_flow_ordinal = 0;
        official_flow_documents.compact_work.spend(4u * sizeof(std::array<bool, 2>) +
                                                   8u * sizeof(std::size_t) + 1024u);
        std::array<bool, 2> flow_body2_consumed{};
        std::array<bool, 15> group_endpoint15_original_consumed{}, group_endpoint15_body_consumed{};
        std::ofstream official_flow_diagnostics(fs::path(LOOM_MERMAID_REPORT_DIR) /
                                                    "mermaid-official-flow400-whole-source.jsonl",
                                                std::ios::binary);
        require(static_cast<bool>(official_flow_diagnostics),
                "OfficialFlow400 diagnostic output unavailable");
        projection_validated = true;
        std::array<std::size_t, kProjectionBindings.size()> applications{};
        auto promotion_at = [&](Selected selected) {
            if (selected.binding == kBody22Index)
                return body22_batch.policy.root().get("promotions").at(selected.promotion);
            if (selected.binding == kRaw193Index)
                return raw_batch.policy.root().get("promotions").at(selected.promotion);
            return selected.binding == kRoleBatchIndex
                       ? role_batch.policy.root().get("promotions").at(selected.promotion)
                       : policies.at(selected.binding)
                             .root()
                             .get("promotions")
                             .at(selected.promotion);
        };
        auto projected = [&](const std::string& path, std::string_view track,
                             std::string_view source, JsonVal baseline, Selected selected) {
            const auto promotion = promotion_at(selected);
            if (selected.binding == kBody22Index)
                body22_consume(body22_counts, body22_batch.policy.root(), selected.promotion);
            else if (selected.binding == kRaw193Index)
                raw193_consume(raw_counts, raw_batch.policy.root(), selected.promotion);
            else if (selected.binding == kRoleBatchIndex)
                role_batch_consume(role_counts, role_batch.policy.root(), selected.promotion);
            else
                ++applications.at(selected.binding);
            auto row =
                run_case(path, std::string(track), source, promotion.get("effective"), kNativeRoot);
            row.projection_binding = selected.binding;
            row.projection_baseline = projection_canonical(baseline);
            row.projection_effective = projection_canonical(promotion);
            if (selected.binding == kRoleBatchIndex) {
                row.role_case_index = static_cast<std::size_t>(promotion.get_int("case_index"));
                const auto fact = role_batch.facts.root().get("cases").at(row.role_case_index);
                row.role_case_id = fact.get_string("id");
                row.role_model_sha = fact.get_string("model_canonical_sha256");
                row.role_body_sha = fact.get_string("body_sha256");
            }
            if (selected.binding == kBody22Index) {
                row.role_case_index = selected.promotion;
                const auto fact = body22_batch.facts.root().get("cases").at(row.role_case_index);
                row.role_case_id = fact.get_string("id");
                row.role_model_sha = fact.get_string("model_sha256");
                row.role_body_sha = fact.get_string("input_sha256");
                row.raw_source_authority = projection_canonical(fact.get("model_authority"));
                row.raw_active_schema = fact.get_string("model_schema");
                row.raw_source_constraints =
                    projection_canonical(fact.get("source_rect_constraints"));
            }
            if (selected.binding == kRaw193Index) {
                row.role_case_index = selected.promotion;
                const auto fact = raw_batch.facts.root().get("cases").at(row.role_case_index);
                row.role_case_id = fact.get_string("id");
                row.role_model_sha = fact.get_string("model_canonical_sha256");
                row.role_body_sha = fact.get_string("body_sha256");
                row.raw_source_authority = projection_canonical(fact.get("whole_source_authority"));
                row.raw_active_schema = projection_canonical(fact.get("active_model_schema"));
                row.raw_source_constraints =
                    projection_canonical(fact.get("source_rect_constraints"));
            }
            return row;
        };
        const auto mindmap_path = kRoot.parent_path() / std::string(mindmap10::kFactsRelativePath);
        require(fs::is_regular_file(mindmap_path) && fs::file_size(mindmap_path) <= 65536u,
                "Mindmap10 bounded approved asset");
        mindmap10::Work mindmap_setup;
        mindmap_setup.spend(static_cast<std::size_t>(fs::file_size(mindmap_path)) * 32u + 4096u);
        const auto mindmap_raw = read_file(mindmap_path);
        require(digest(mindmap_raw) == mindmap10::kFactsSha, "Mindmap10 approved asset SHA");
        auto mindmap_parsed = json::parse(mindmap_raw);
        require(mindmap_parsed.has_value(), "Mindmap10 asset JSON");
        official_mindmap_facts.emplace(std::move(*mindmap_parsed));
        std::array<bool, 10> mindmap_consumed{};
        std::ofstream mindmap_diagnostics(fs::path(LOOM_MERMAID_REPORT_DIR) /
                                              "mermaid-official-mindmap10.jsonl",
                                          std::ios::binary);
        require(mindmap_diagnostics.good(), "Mindmap10 Official diagnostic output");
        const auto shapes4_path = kRoot.parent_path() / std::string(shapes4::kFactsRelativePath);
        require(fs::is_regular_file(shapes4_path) && fs::file_size(shapes4_path) <= 131072u,
                "Shapes4 bounded approved asset");
        shapes4::Work shapes4_setup;
        shapes4_setup.spend(static_cast<std::size_t>(fs::file_size(shapes4_path)) * 32u + 4096u);
        const auto shapes4_raw = read_file(shapes4_path);
        require(digest(shapes4_raw) == shapes4::kFactsSha, "Shapes4 approved asset SHA");
        auto shapes4_parsed = json::parse(shapes4_raw);
        require(shapes4_parsed.has_value(), "Shapes4 asset JSON");
        official_shapes4_facts.emplace(std::move(*shapes4_parsed));
        std::array<bool, 5> shapes4_consumed{};
        std::ofstream shapes4_diagnostics(
            fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-official-shapes4.jsonl", std::ios::binary);
        require(shapes4_diagnostics.good(), "Shapes4 Official diagnostic output");
        official_er107::Documents er107_documents;
        std::array<bool, 107> er107_selected{}, er107_captured{};
        std::ofstream er107_diagnostics(fs::path(LOOM_MERMAID_REPORT_DIR) /
                                            std::string(kOfficialEr107MatrixEvidence),
                                        std::ios::binary);
        require(er107_diagnostics.good(), "ER107 Matrix independent diagnostic output");
        official_block58::Documents block_documents;
        std::array<bool, 58> block_consumed{};
        std::ofstream block_diagnostics(fs::path(LOOM_MERMAID_REPORT_DIR) /
                                            std::string(official_block58::kEvidence),
                                        std::ios::binary);
        require(block_diagnostics.good(), "Block58 Official diagnostic output");
        for (std::size_t i = 0; i < fixtures.size(); ++i) {
            const auto fixture = fixtures.at(i);
            // Borrowed selection before path/source ownership. Every selected
            // original starts its own single paid verification ledger.
            const auto class_path = fixture.get("path");
            if (class_path.is_str() && class_path.as_str().starts_with("block/"))
                if (const auto selected = official_block58::select(i, class_path.as_str(),
                                                                   fixture.get_string("sha256"))) {
                    require(!block_consumed[*selected], "Block58 original consumed twice");
                    block_consumed[*selected] = true;
                    observations.push_back(run_official_block58(block_documents, fixture, *selected,
                                                                block_diagnostics));
                    continue;
                }
            if (class_path.is_str() && class_path.as_str().starts_with("mindmap/"))
                if (const auto selected = official_mindmap_selected(official_mindmap_facts->root(),
                                                                    i, class_path.as_str())) {
                    require(!mindmap_consumed[*selected], "Mindmap10 original consumed twice");
                    mindmap_consumed[*selected] = true;
                    observations.push_back(run_official_mindmap(official_mindmap_facts->root(),
                                                                document->root(), fixture,
                                                                *selected, mindmap_diagnostics));
                    continue;
                }

            if (class_path.is_str() && class_path.as_str().starts_with("mindmap/"))
                if (const auto selected = official_shapes4_selected(official_shapes4_facts->root(),
                                                                    i, class_path.as_str())) {
                    require(!shapes4_consumed[*selected], "Shapes4 original consumed twice");
                    shapes4_consumed[*selected] = true;
                    observations.push_back(run_official_shapes4(official_shapes4_facts->root(),
                                                                document->root(), fixture,
                                                                *selected, shapes4_diagnostics));
                    continue;
                }

            // RFC 0031 E2a: config-free gitGraph/state originals are governed
            // by the independently reviewed cohort ledger, not the manifest's
            // source expectation. The per-row pixel gate is test_mermaid_e2a_cohort;
            // here the matrix records the ledger disposition with zero row error.
            if (class_path.is_str()) {
                const auto cohort = e2a_cohort_row(class_path.as_str());
                if (cohort) {
                    const bool drawn = *cohort;
                    Observation row;
                    row.path = std::string(class_path.as_str());
                    row.track = "original";
                    row.type = (*fs::path(row.path).begin()).string();
                    row.source_sha256 = fixture.get_string("sha256");
                    row.expected = drawn ? "drawn" : "source";
                    row.reason = "RFC-0031 E2a reviewed cohort";
                    const char* wide = drawn ? "drawn" : "source_fallback";
                    row.status = {wide, "source_fallback", wide};
                    row.expected_states = {drawn ? "drawn" : "source", "source",
                                           drawn ? "drawn" : "source"};
                    row.decision_reasons.fill(row.reason);
                    observations.push_back(std::move(row));
                    continue;
                }
            }
            if (class_path.is_str() && class_path.as_str().starts_with("er-diagram/")) {
                if (const auto selected = official_er107::select(i, class_path.as_str(),
                                                                 fixture.get_string("sha256"))) {
                    require(!er107_selected[*selected], "ER107 Matrix source selected twice");
                    er107_selected[*selected] = true;
                    official_er107::ThemeGuard er107_theme;
                    loom::ui::design::theme::set_theme(loom::ui::design::theme::ThemeVariant::Dark);
                    const auto observed =
                        official_er107::run(er107_documents, *selected, er107_diagnostics);
                    er107_captured[*selected] = observed.captured;
                    const auto proof = json::parse(observed.proof);
                    require(proof.has_value(), "ER107 Matrix actual owned proof JSON");
                    require(official_er107::owned(proof->root()) == observed.ready,
                            "ER107 Matrix actual local verdict disagreement");
                    relations_oracle::Work retained_work{relations_oracle::kEr206SceneLimit};
                    auto row = run_official_er206_case(fixture, i, official_er_ordinal++,
                                                       *official_er_documents, official_er_consumed,
                                                       retained_work, official_er_diagnostics,
                                                       observations.size(), observations.capacity(),
                                                       observed.ready ? proof->root() : JsonVal{});
                    // Keep the frozen ER206 closed proof, original null Model
                    // and source counters in source authority, not extra rows.
                    const auto legacy = json::parse(row.raw_source_constraints);
                    require(legacy.has_value(), "ER107 retained ER206 proof JSON");
                    json::JsonMutDoc authority;
                    auto retained = authority.object();
                    retained.set("source_facts_sha256", er107::kFactsSha);
                    retained.add("retained_ER206", authority.copy_val(legacy->root()));
                    authority.set_root(retained);
                    row.raw_source_authority = authority.to_string();
                    row.raw_source_constraints = observed.proof;
                    row.raw_active_schema = kOfficialEr107Contract;
                    row.role_case_index = *selected;
                    row.role_model_sha = er107::authority::kPins[*selected].complete_sha;
                    row.role_body_sha = er107::authority::kPins[*selected].scene_sha;
                    row.expected = "pending";
                    row.reason = "ER107 complete original locally verified; independent strict "
                                 "formal join pending";
                    row.decision_reasons.fill(row.reason);
                    if (observed.ready) {
                        row.status = {"drawn",
                                      proof->root().get("narrow_exact_ordinary").as_bool()
                                          ? "source_fallback"
                                          : "drawn",
                                      "drawn"};
                        row.expected_states = {"drawn",
                                               proof->root().get("narrow_exact_ordinary").as_bool()
                                                   ? "source"
                                                   : "drawn",
                                               "drawn"};
                    } else
                        row.errors.push_back(proof->root().get_string("error"));
                    observations.push_back(std::move(row));
                    continue;
                }
                relations_oracle::Work er_work{relations_oracle::kEr206SceneLimit};
                observations.push_back(run_official_er206_case(
                    fixture, i, official_er_ordinal++, *official_er_documents, official_er_consumed,
                    er_work, official_er_diagnostics, observations.size(),
                    observations.capacity()));
                continue;
            }
            if (class_path.is_str() && class_path.as_str().starts_with("class-diagram/")) {
                relations_oracle::Work class_work{kStageCVerificationBudget};
                observations.push_back(
                    run_class285_case(fixture, i, class_documents, class_originals, class_work,
                                      observations.size(), observations.capacity()));
                continue;
            }
            if (class_path.is_str())
                if (const auto selected = requirement11_selected_index(i, class_path.as_str())) {
                    const auto generic_policy =
                        body22_select(policies, role_batch.policy.root(), raw_batch.policy.root(),
                                      body22_batch.policy.root(), class_path.as_str(), "original");
                    require(!generic_policy.has_value(),
                            "Requirement11 source may not silently displace existing generic "
                            "policy provenance");
                    Work requirement11_work{r11::kSceneLimit};
                    observations.push_back(run_official_requirement11_case(
                        fixture, i, *selected, *official_requirement11_documents,
                        official_requirement11_consumed, requirement11_work,
                        official_requirement11_diagnostics, observations.size(),
                        observations.capacity()));
                    continue;
                }
            if (class_path.is_str())
                if (const auto selected = official_pie7_selected(i, class_path.as_str())) {
                    const auto generic_policy =
                        body22_select(policies, role_batch.policy.root(), raw_batch.policy.root(),
                                      body22_batch.policy.root(), class_path.as_str(), "original");
                    require(!generic_policy.has_value(),
                            "OfficialPie7 source may not displace generic policy provenance");
                    pie7::Work pie_work{kOfficialPie7SceneLimit};
                    observations.push_back(run_official_pie7_case(
                        fixture, i, *selected, *official_pie7_documents, official_pie7_consumed,
                        pie_work, official_pie7_diagnostics, observations.size(),
                        observations.capacity()));
                    continue;
                }
            // Only complete original Flow rows select a new verification ledger.
            // The subsequent body_diagnostic block below is deliberately literal.
            std::optional<flow_oracle::OracleWork> official_flow_work;
            if (class_path.is_str() && class_path.as_str().starts_with("flowchart/")) {
                official_flow_work.emplace(
                    flow_oracle::OracleWork{flow_oracle::kFlow400SceneLimit});
                auto& paid = *official_flow_work;
                paid.spend(kOfficialFlowReportReserve + 8u * sizeof(Observation) +
                           8u * sizeof(OfficialFlowProgress) + 8u * sizeof(OfficialFlowFrames) +
                           16384u);
                const auto bytes = flow_oracle::flow400_number(fixture.get("bytes"));
                require(bytes > 0u && bytes <= 65536u && class_path.as_str().size() <= 512u,
                        "OfficialFlow400 finite acquire fields");
                paid.product(bytes, 32u);
                paid.product(class_path.as_str().size(), 12u);
                paid.product(kSentinel.size(), 8u);
                // Only these exact source-owned diagnostic applications reserve
                // their report/error/copy lifecycle before original geometry.
                if (i == 662u || i == 663u) {
                    flow_body2_consume(flow_body2_consumed, i - 662u, paid);
                    flow_body2_reserve(observations, paid);
                }
            }
            const auto path = fixture.get_string("path");
            const auto source = read_file(kRoot / "cases" / path);
            if (official_flow_work) {
                require(official_flow_ordinal < 400u &&
                            !official_flow_consumed[official_flow_ordinal],
                        "OfficialFlow400 duplicate source consumption");
                const auto current_flow_ordinal = official_flow_ordinal++;
                official_flow_consumed[current_flow_ordinal] = true;
                if (const auto selected =
                        flow_oracle::group_endpoint15_tuple_index(current_flow_ordinal, i)) {
                    require(!group_endpoint15_original_consumed[*selected],
                            "OfficialFlow400 GroupEndpoint15 duplicate original consume");
                    group_endpoint15_original_consumed[*selected] = true;
                }
                observations.push_back(run_official_flow400_case(
                    fixture, i, current_flow_ordinal, source, official_flow_documents,
                    *official_flow_work, official_flow_diagnostics, observations.size(),
                    observations.capacity(), official_flow_config21_documents.supplement.root(),
                    official_flow_typed_source4_documents->supplement.root(),
                    official_flow_classic_documents->supplement.root(),
                    official_flow_linkprofile3_documents->supplement.root(),
                    official_flow_group_endpoint15_documents->asset.root()));
            } else {
                const auto original =
                    body22_select(policies, role_batch.policy.root(), raw_batch.policy.root(),
                                  body22_batch.policy.root(), path, "original");
                if (original)
                    observations.push_back(
                        projected(path, "original", source, fixture.get("raw"), *original));
                else
                    observations.push_back(run_case(path, "original", source, fixture.get("raw")));
            }
            if (fixture.has("diagnostic")) {
                const auto diagnostic = fixture.get("diagnostic");
                const auto body = std::string_view(source).substr(
                    static_cast<std::size_t>(diagnostic.get_int("offset")));
                const auto body_group_index =
                    official_flow_work
                        ? flow_oracle::group_endpoint15_tuple_index(official_flow_ordinal - 1u, i)
                        : std::optional<std::size_t>{};
                if (body_group_index) {
                    require(!group_endpoint15_body_consumed[*body_group_index],
                            "OfficialFlow400 GroupEndpoint15 duplicate body consume");
                    group_endpoint15_body_consumed[*body_group_index] = true;
                    flow_oracle::OracleWork body_work{flow_oracle::kFlow400SceneLimit};
                    observations.push_back(run_official_group_endpoint15_body(
                        fixture, i, official_flow_ordinal - 1u, source, official_flow_documents,
                        *official_flow_group_endpoint15_documents, body_work,
                        official_flow_diagnostics, observations.size(), observations.capacity()));
                } else if (i == 662u || i == 663u) {
                    require(official_flow_work.has_value() && !observations.empty(),
                            "FlowBody2 requires current original scene ledger/result");
                    const auto index = i - 662u;
                    auto& paid = *official_flow_work;
                    // The application flag/report reserve was paid before
                    // original geometry; exhaustion cannot skip the body row.
                    // Fully build/copy before push can invalidate back(). No
                    // original reference or JsonVal escapes its owning lifetime.
                    auto body_row =
                        reuse_flow_body2(fixture, i, index, source, body, observations.back(),
                                         official_flow_documents, paid);
                    observations.push_back(std::move(body_row));
                } else if (std::ranges::any_of(
                               flow_oracle::kFlowShapeAliasBodyCases, [&](const auto& candidate) {
                                   const auto* alias =
                                       flow_oracle::flow_shape_alias_case(candidate.ordinal);
                                   return !candidate.snapshot_sha.empty() && alias != nullptr &&
                                          path == alias->fixture;
                               })) {
                    observations.push_back(
                        run_official_flow_shape_alias_body(fixture, i, source, diagnostic, body));
                } else {
                    const auto selected =
                        body22_select(policies, role_batch.policy.root(), raw_batch.policy.root(),
                                      body22_batch.policy.root(), path, "body_diagnostic");
                    if (selected)
                        observations.push_back(
                            projected(path, "body_diagnostic", body, diagnostic, *selected));
                    else
                        observations.push_back(run_case(path, "body_diagnostic", body, diagnostic));
                }
            }
        }
        require(std::ranges::all_of(er107_selected, [](bool selected) { return selected; }),
                "ER107 Matrix exactly107 selected originals consumed");
        er107_diagnostics.flush();
        require(er107_diagnostics.good(), "ER107 Matrix all actual frame evidence flushed");
        er107_documents.finish(er107_captured);
        er107_aggregate_validated = true;
        EXPECT_EQ(std::ranges::count_if(observations,
                                        [](const Observation& row) {
                                            return row.raw_active_schema ==
                                                       kOfficialEr107Contract &&
                                                   official_er107_owned(row);
                                        }),
                  107);
        require(official_er_ordinal == 206u && static_cast<bool>(official_er_diagnostics),
                "OfficialER206 complete original/evidence enumeration");
        relations_oracle::er206_all_consumed(official_er_consumed, official_er_setup);
        const auto ers = official_er_counts(observations, &*official_er_documents);
        EXPECT_EQ(ers.visited, 206u);
        EXPECT_EQ(ers.observed, 206u);
        EXPECT_EQ(ers.published, 206u);
        EXPECT_EQ(ers.bound, 206u);
        EXPECT_EQ(ers.decoded, 62u);
        EXPECT_EQ(ers.pending, 144u);
        EXPECT_EQ(ers.errors, 6u);
        EXPECT_EQ(ers.resources, 0u);
        EXPECT_EQ(ers.admitted, 62u);
        EXPECT_EQ(ers.mutated, 62u);
        require(static_cast<bool>(official_requirement11_diagnostics) &&
                    std::ranges::all_of(official_requirement11_consumed,
                                        [](bool used) { return used; }),
                "Requirement11 missing/unconsumed complete source");
        const auto requirements =
            requirement11_counts(observations, &*official_requirement11_documents);
        EXPECT_EQ(requirements.visited, 11u);
        EXPECT_EQ(requirements.observed, 11u);
        EXPECT_EQ(requirements.published, 11u);
        EXPECT_EQ(requirements.bound, 11u);
        EXPECT_EQ(requirements.decoded, 11u);
        EXPECT_EQ(requirements.mutated, 11u);
        EXPECT_EQ(requirements.errors, 0u);
        EXPECT_EQ(requirements.resources, 0u);
        EXPECT_EQ(requirements.admitted, 11u);
        require(static_cast<bool>(official_pie7_diagnostics) &&
                    std::ranges::all_of(official_pie7_consumed, [](bool used) { return used; }),
                "OfficialPie7 missing/unconsumed complete source");
        const auto pies = official_pie7_counts(observations, &*official_pie7_documents);
        EXPECT_EQ(pies.visited, 7u);
        EXPECT_EQ(pies.observed, 7u);
        EXPECT_EQ(pies.published, 7u);
        EXPECT_EQ(pies.bound, 7u);
        EXPECT_EQ(pies.decoded, 7u);
        EXPECT_EQ(pies.mutated, 7u);
        EXPECT_EQ(pies.errors, 0u);
        EXPECT_EQ(pies.resources, 0u);
        EXPECT_EQ(pies.admitted, 7u);
        require(official_flow_ordinal == 400u &&
                    std::ranges::all_of(official_flow_consumed, [](bool n) { return n; }),
                "OfficialFlow400 missing/unconsumed complete source");
        require(
            std::ranges::all_of(group_endpoint15_original_consumed,
                                [](bool used) { return used; }) &&
                std::ranges::all_of(group_endpoint15_body_consumed, [](bool used) { return used; }),
            "OfficialFlow400 GroupEndpoint15 raw/body consume");
        require(static_cast<bool>(official_flow_diagnostics),
                "OfficialFlow400 incomplete actual scene output");
        require(std::ranges::all_of(flow_body2_consumed, [](bool used) { return used; }),
                "FlowBody2 missing complete descriptor consumption");
        const auto body_flows = flow_body2_counts(observations);
        EXPECT_EQ(body_flows.visited, 2u);
        EXPECT_EQ(body_flows.bound, 2u);
        EXPECT_EQ(body_flows.decoded, 2u);
        EXPECT_EQ(body_flows.admitted, 2u);
        EXPECT_EQ(body_flows.mutated, 2u);
        EXPECT_EQ(body_flows.errors, 0u);
        EXPECT_EQ(body_flows.resources, 0u);
        const auto flows = official_flow_counts(
            observations, official_flow_typed_source4_documents->supplement.root(),
            official_flow_classic_documents->supplement.root(),
            official_flow_linkprofile3_documents->supplement.root(),
            official_flow_group_endpoint15_documents->asset.root());
        const auto group_bodies =
            group_endpoint15_body_counts(observations, *official_flow_group_endpoint15_documents);
        EXPECT_EQ(group_bodies.visited, 15u);
        EXPECT_EQ(group_bodies.observed, 15u);
        EXPECT_EQ(group_bodies.bound, 15u);
        EXPECT_EQ(group_bodies.decoded, 15u);
        EXPECT_EQ(group_bodies.admitted, 15u);
        EXPECT_EQ(group_bodies.mutated, 15u);
        EXPECT_EQ(group_bodies.errors, 0u);
        EXPECT_EQ(group_bodies.resources, 0u);
        const auto shape_alias_body_selected =
            std::ranges::count_if(observations, [](const Observation& row) {
                return row.raw_active_schema == kFlowShapeAliasBodyContract;
            });
        const auto shape_alias_body_admitted =
            std::ranges::count_if(observations, [](const Observation& row) {
                return official_flow_shape_alias_body_owned(row);
            });
        EXPECT_EQ(shape_alias_body_selected, 14u);
        EXPECT_EQ(shape_alias_body_admitted, 14u);
        EXPECT_EQ(flows.visited, 400u);
        EXPECT_EQ(flows.observed, 400u);
        EXPECT_EQ(flows.bound, 400u);
        official_flow_config21_documents.asset_work.product(observations.size(), 128u);
        const auto config21_selected =
            std::ranges::count_if(observations, [](const Observation& row) {
                return row.raw_active_schema == kOfficialFlow400Contract &&
                       row.raw_source_authority == "immutable_Flow400_8Models_plus_Config21";
            });
        EXPECT_EQ(config21_selected, 21);
        const auto typed_source4_selected =
            std::ranges::count_if(observations, [](const Observation& row) {
                return row.raw_active_schema == kOfficialFlow400Contract &&
                       row.raw_source_authority == "immutable_Flow400_8Models_plus_TypedSource4";
            });
        EXPECT_EQ(typed_source4_selected, 4);
        const auto classic_selected =
            std::ranges::count_if(observations, [](const Observation& row) {
                return row.raw_active_schema == kOfficialFlow400Contract &&
                       row.raw_source_authority == "immutable_Flow400_8Models_plus_Classic";
            });
        EXPECT_EQ(classic_selected, 6);
        const auto linkprofile3_selected =
            std::ranges::count_if(observations, [](const Observation& row) {
                return row.raw_active_schema == kOfficialFlow400Contract &&
                       row.raw_source_authority == "immutable_Flow400_8Models_plus_LinkProfile3";
            });
        EXPECT_EQ(linkprofile3_selected, 4);
        const auto group_endpoint15_selected =
            std::ranges::count_if(observations, [](const Observation& row) {
                return row.raw_active_schema == kOfficialFlow400Contract &&
                       row.raw_source_authority == "immutable_Flow400_8Models_plus_GroupEndpoint15";
            });
        EXPECT_EQ(group_endpoint15_selected, 15);
        const auto title_selected = std::ranges::count_if(observations, [](const Observation& row) {
            return row.raw_active_schema == kOfficialFlow400Contract &&
                   row.raw_source_authority == "immutable_Flow400_8Models_plus_DiagramTitle1";
        });
        EXPECT_EQ(title_selected, 1);
        const auto title_margin_selected =
            std::ranges::count_if(observations, [](const Observation& row) {
                return row.raw_active_schema == kOfficialFlow400Contract &&
                       row.raw_source_authority == "immutable_Flow400_8Models_plus_TitleTopMargin1";
            });
        EXPECT_EQ(title_margin_selected, 1);
        const auto issue4648_selected =
            std::ranges::count_if(observations, [](const Observation& row) {
                return row.raw_active_schema == kOfficialFlow400Contract &&
                       row.raw_source_authority == "immutable_Flow400_8Models_plus_Issue4648";
            });
        EXPECT_EQ(issue4648_selected, 1);
        const auto shape_alias_selected =
            std::ranges::count_if(observations, [](const Observation& row) {
                return row.raw_active_schema == kOfficialFlow400Contract &&
                       row.raw_source_authority ==
                           "immutable_Flow400_8Models_plus_FlowShapeAlias16";
            });
        EXPECT_EQ(shape_alias_selected, 16);
        // Shape aliases include the independently checked filled Fork/Join
        // contour. Body diagnostics add no original-file drawing credit.
        EXPECT_EQ(flows.decoded, 77u);
        EXPECT_EQ(flows.pending, 323u);
        EXPECT_EQ(flows.errors, 10u);
        EXPECT_EQ(flows.resources, 0u);
        EXPECT_EQ(flows.admitted, 77u);
        EXPECT_EQ(flows.mutated, 77u);
        for (const auto count : applications)
            projection_consumed(count);
        role_batch_consumed(role_counts, role_batch.facts.root());
        role_consumption_validated = true;
        raw193_consumed(raw_counts);
        raw_consumption_validated = true;
        body22_consumed(body22_counts);
        body22_consumption_validated = true;
        json::JsonMutDoc mindmap_verdict;
        auto mv = mindmap_verdict.object(), mr = mv.make_arr();
        int mindmap_ready = 0;
        for (const auto& row : observations)
            if (row.raw_active_schema == kMindmap10Contract) {
                auto value = mv.make_obj();
                value.set("path", row.path);
                const auto proof = json::parse(row.raw_source_constraints);
                require(proof.has_value(), "Mindmap10 compact proof");
                proof->root().iter_obj([&](JsonVal key, JsonVal field) {
                    value.add(key.as_str(), mindmap_verdict.copy_val(field));
                });
                if (!row.errors.empty()) value.set("error", row.errors.front());
                mindmap_ready += official_mindmap_owned(row, official_mindmap_facts->root());
                mr.append(value);
            }
        mv.set("schema", "mindmap10_official_local_verdict/v1");
        mv.set("local_ready", mindmap_ready);
        mv.set("source_facts_sha256", mindmap10::kFactsSha);
        mv.add("cases", mr);
        mindmap_verdict.set_root(mv);
        write_file(fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-mindmap10-official-verdict.json",
                   mindmap_verdict.to_string());
        require(std::ranges::all_of(mindmap_consumed, [](bool value) { return value; }),
                "Mindmap10 all complete originals consumed");
        EXPECT_EQ(std::ranges::count_if(observations,
                                        [&](const auto& row) {
                                            return row.raw_active_schema == kMindmap10Contract &&
                                                   official_mindmap_owned(
                                                       row, official_mindmap_facts->root());
                                        }),
                  10);
        write_file(fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-shapes4-official-verdict.json",
                   official_shapes4_verdict(observations, official_shapes4_facts->root()));
        require(!shapes4_consumed[0] && std::ranges::all_of(shapes4_consumed | std::views::drop(1),
                                                            [](bool value) { return value; }),
                "Shapes4 exactly four originals consumed; Bang/Cloud excluded");
        require(std::ranges::all_of(block_consumed, [](bool value) { return value; }),
                "Block58 exactly 58 complete originals consumed");
        EXPECT_EQ(std::ranges::count_if(observations,
                                        [](const auto& row) {
                                            return row.raw_active_schema == kBlock58Contract &&
                                                   official_block58_owned(row);
                                        }),
                  58);
        write_file(fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-block58-official-verdict.json",
                   official_block58_verdict(observations));
        const auto classes = stage_c_counts(observations);
        EXPECT_EQ(classes.visited, 285u);
        EXPECT_EQ(classes.observed, 285u);
        EXPECT_EQ(classes.bound, 285u);
        EXPECT_EQ(classes.decoded, 237u);
        EXPECT_EQ(classes.dagre, 38u);
        EXPECT_EQ(classes.classifier, 26u);
        EXPECT_EQ(classes.unused_definitions, 3u);
        EXPECT_EQ(classes.notes, 18u);
        EXPECT_EQ(classes.pending, 48u);
        EXPECT_EQ(classes.errors, 10u);
        EXPECT_EQ(classes.resources, 0u);
        // Twenty complete cardinality originals now close through one bounded
        // caption-first retry. Six decoded Note profiles remain whole source;
        // the other 48 source Models stay pending and earn no drawing credit.
        EXPECT_EQ(classes.admitted, 231u);
    } catch (const std::exception& error) {
        integrity_errors.push_back(error.what());
    }
    EXPECT_NO_THROW(emit_report(
        observations, integrity_errors, projection_validated, role_consumption_validated,
        raw_consumption_validated, body22_consumption_validated,
        official_er_documents ? &*official_er_documents : nullptr,
        official_requirement11_documents ? &*official_requirement11_documents : nullptr,
        official_pie7_documents ? &*official_pie7_documents : nullptr,
        official_flow_typed_source4_documents ? &*official_flow_typed_source4_documents : nullptr,
        official_flow_classic_documents ? &*official_flow_classic_documents : nullptr,
        official_flow_linkprofile3_documents ? &*official_flow_linkprofile3_documents : nullptr,
        official_flow_group_endpoint15_documents ? &*official_flow_group_endpoint15_documents
                                                 : nullptr,
        official_mindmap_facts ? &*official_mindmap_facts : nullptr,
        official_shapes4_facts ? &*official_shapes4_facts : nullptr, er107_aggregate_validated));
    for (const auto& error : integrity_errors)
        ADD_FAILURE() << error;
    EXPECT_EQ(observations.size(), 2145u + 593u);
    // RFC 0031 E6c: declared capability gaps are report-only and must match
    // the reviewed roster exactly; only unrecognized error strings stay hard
    // failures, so a new renderer failure can never book as a known gap.
    std::vector<std::string> unknown_messages;
    const auto observed_gap_rows = matrix_mandatory_gap_rows(observations, &unknown_messages);
    for (const auto& message : unknown_messages)
        ADD_FAILURE() << message;
    if (auto roster = gap_roster::load_verified_roster()) {
        fs::path matrix_path = fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-official-matrix.json";
        if (const auto* override_path = std::getenv("LOOM_MERMAID_MATRIX_PATH"))
            matrix_path = override_path;
        auto published = json::parse(read_file(matrix_path));
        ASSERT_TRUE(published.has_value())
            << "Cannot re-read published matrix report " << matrix_path;
        const auto gap_diff = gap_roster::diff_gate(*roster, "official_matrix",
                                                    published->root().get("mandatory_gaps"));
        EXPECT_TRUE(gap_diff.ok()) << gap_diff.render("official_matrix");
        EXPECT_EQ(observed_gap_rows.size(), std::size_t(78));
    }
}

TEST(MermaidOfficial, OracleRejectsChangedLabelsMissingArrowsAndWrongFallback) {
    const auto checks_document = json::parse(
        R"({"labels":["Origin","Destination"],"arrows":1,"ordering":[{"before":"Origin","after":"Destination","axis":"x"}]})");
    ASSERT_TRUE(checks_document.has_value());
    const auto checks = checks_document->root();
    const auto source = "flowchart LR\nA[Origin] --> B[Destination]";
    const auto ordinary = capture(loom::ui::render_markdown(fenced(source, "text")), kWide, 40);
    auto correct = capture(loom::ui::render_markdown(fenced(source, "mermaid")), kWide, 40);
    ASSERT_TRUE(semantic_errors(correct, checks).empty());
    auto changed = correct;
    for (auto& row : changed.cells)
        for (auto& cell : row)
            if (cell == "O") cell = "X";
    EXPECT_FALSE(semantic_errors(changed, checks).empty());
    auto missing = correct;
    for (auto& row : missing.cells)
        for (auto& cell : row)
            if (cell == "▶") cell = "─";
    EXPECT_FALSE(semantic_errors(missing, checks).empty());
    std::vector<std::string> errors;
    EXPECT_EQ(classify(correct, ordinary, false, checks, {}, errors), "error");
    errors.clear();
    EXPECT_EQ(classify(ordinary, ordinary, true, checks, correct.text, errors), "error");
    errors.clear();
    EXPECT_EQ(classify(correct, ordinary, true, checks, correct.text, errors), "drawn");
}

TEST(MermaidOfficial, OracleRejectsManifestCorruption) {
    auto bytes = read_file(kRoot / "manifest.json");
    ASSERT_EQ(digest(bytes), kManifestSha256);
    bytes += ' ';
    EXPECT_THROW(validate_manifest(kRoot, bytes), std::runtime_error);
    EXPECT_FALSE(safe_relative("../escape.mmd"));
    EXPECT_FALSE(safe_relative("/escape.mmd"));
    EXPECT_FALSE(safe_relative("flowchart/../escape.mmd"));
}

std::string projection_mutate(std::string_view bytes,
                              const std::function<void(json::JsonMutVal)>& change) {
    auto parsed = json::parse(bytes);
    require(parsed.has_value(), "Cannot parse mutation input");
    json::JsonMutDoc document;
    auto root = document.copy_val(parsed->root());
    change(root);
    document.set_root(root);
    return document.to_string();
}

TEST(MermaidOfficial, ReviewedProjectionBindsCompleteSourceModelAndSnapshot) {
    for (const auto& binding : kProjectionBindings) {
        if (binding.kind != ProjectionKind::SequenceBody) continue;
        SCOPED_TRACE(binding.fact_file);
        const auto policy_bytes = read_file(kNativeRoot / binding.policy_file);
        const auto fact_bytes = read_file(kNativeRoot / binding.fact_file);
        EXPECT_NO_THROW(projection_byte_pins(policy_bytes, fact_bytes, binding));
        auto policy = json::parse(policy_bytes);
        auto facts = json::parse(fact_bytes);
        auto manifest = json::parse(read_file(kRoot / "manifest.json"));
        ASSERT_TRUE(policy && facts && manifest);
        const auto fact = facts->root().get("cases").at(0);
        const auto raw = read_file(kRoot / "cases" / fact.get_string("path"));
        const auto snapshot = read_file(kNativeRoot / binding.snapshot_file);
        EXPECT_NO_THROW(validate_projection(policy->root(), facts->root(), manifest->root(), raw,
                                            snapshot, binding));
        EXPECT_NO_THROW(projection_consumed(1));
        EXPECT_THROW(projection_consumed(0), std::runtime_error);
        EXPECT_THROW(projection_consumed(2), std::runtime_error);
        EXPECT_THROW(projection_byte_pins(policy_bytes + " ", fact_bytes, binding),
                     std::runtime_error);
        EXPECT_THROW(projection_byte_pins(policy_bytes, fact_bytes + " ", binding),
                     std::runtime_error);
        // The added snapshot is not one of the immutable 84 historical snapshots.
        EXPECT_FALSE(fs::exists(kRoot / "snapshots" / binding.snapshot_file));
    }
}

TEST(MermaidOfficial, ProjectionSemanticMutationsReachExactTupleValidators) {
    for (const auto& binding : kProjectionBindings) {
        if (binding.kind != ProjectionKind::SequenceBody) continue;
        SCOPED_TRACE(binding.fact_file);
        const auto policy_bytes = read_file(kNativeRoot / binding.policy_file);
        const auto fact_bytes = read_file(kNativeRoot / binding.fact_file);
        auto facts = json::parse(fact_bytes);
        auto manifest = json::parse(read_file(kRoot / "manifest.json"));
        ASSERT_TRUE(facts && manifest);
        const auto raw =
            read_file(kRoot / "cases" / facts->root().get("cases").at(0).get_string("path"));
        const auto snapshot = read_file(kNativeRoot / binding.snapshot_file);
        // Deliberately bypass only aggregate pins here. Every failure below must
        // reach the indicated production-path semantic validator, not a file hash.
        auto rejects = [&](std::string_view policy_text, std::string_view facts_text,
                           std::string_view raw_text, std::string_view snapshot_text,
                           std::string_view expected_error) {
            auto policy = json::parse(policy_text);
            auto source_facts = json::parse(facts_text);
            ASSERT_TRUE(policy && source_facts);
            try {
                validate_projection(policy->root(), source_facts->root(), manifest->root(),
                                    raw_text, snapshot_text, binding);
                ADD_FAILURE() << "Mutation was accepted; expected " << expected_error;
            } catch (const std::runtime_error& error) {
                EXPECT_NE(std::string_view(error.what()).find(expected_error),
                          std::string_view::npos)
                    << error.what();
            }
        };
        auto policy_rejects = [&](const std::function<void(json::JsonMutVal)>& change,
                                  std::string_view error) {
            rejects(projection_mutate(policy_bytes, change), fact_bytes, raw, snapshot, error);
        };
        auto entry = [](json::JsonMutVal root) { return root.get("promotions").at(0); };
        policy_rejects([](auto root) { root.set("schema", true); },
                       "integer/type mismatch: schema");
        policy_rejects([](auto root) { root.set("extra", 1); }, "unknown field");
        policy_rejects([](auto root) { static_cast<void>(root.remove("schema")); },
                       "missing field");
        policy_rejects([](auto root) { root.set("baseline_manifest_sha256", "wrong"); },
                       "baseline manifest mismatch");
        policy_rejects([](auto root) { root.get("source_facts").set("path", "../escape.json"); },
                       "source-fact path mismatch");
        policy_rejects([](auto root) { root.get("source_facts").set("sha256", "wrong"); },
                       "source-fact SHA mismatch");
        policy_rejects([](auto root) { root.get("source_facts").set("schema", "1"); },
                       "source-fact schema");
        policy_rejects([](auto root) { root.get("source_facts").set("case_index", 1); },
                       "source-fact case index");
        policy_rejects([](auto root) { root.get("source_facts").set("model_schema", "other"); },
                       "model schema mismatch");
        policy_rejects([](auto root) { root.add("promotions", root.make_obj()); },
                       "promotions type mismatch");
        policy_rejects([](auto root) { root.add("promotions", root.make_arr()); },
                       "missing promotion");
        auto original_policy = json::parse(policy_bytes);
        ASSERT_TRUE(original_policy);
        auto duplicate_entry = policy_bytes;
        duplicate_entry.insert(duplicate_entry.rfind(']'),
                               "," + original_policy->root().get("promotions").at(0).to_string());
        rejects(duplicate_entry, fact_bytes, raw, snapshot, "duplicate promotion");
        policy_rejects([&](auto root) { entry(root).set("path", "../escape.mmd"); },
                       "unsafe fixture path");
        policy_rejects([&](auto root) { entry(root).set("path", "sequence/missing.mmd"); },
                       "unknown fixture/body track");
        policy_rejects([&](auto root) { entry(root).set("track", "original"); }, "unknown track");
        policy_rejects([&](auto root) { entry(root).set("raw_sha256", "wrong"); },
                       "raw binding mismatch");
        policy_rejects([&](auto root) { entry(root).set("body_offset_utf8_bytes", 201); },
                       "body offset/type mismatch");
        policy_rejects([&](auto root) { entry(root).set("body_offset_utf8_bytes", "200"); },
                       "body offset/type mismatch");
        policy_rejects([&](auto root) { entry(root).set("body_sha256", "wrong"); },
                       "body binding mismatch");
        policy_rejects([&](auto root) { entry(root).set("baseline_expectation_sha256", "wrong"); },
                       "baseline expectation mismatch");
        policy_rejects([&](auto root) { entry(root).set("full_model_sha256", "wrong"); },
                       "full Model binding mismatch");
        policy_rejects([&](auto root) { entry(root).get("effective").set("expected", "source"); },
                       "effective outcome mismatch");
        policy_rejects([&](auto root) { entry(root).get("effective").set("reason", true); },
                       "string type: effective reason");
        policy_rejects([&](auto root) { entry(root).set("reason", ""); },
                       "string type: promotion reason");
        policy_rejects(
            [&](auto root) { entry(root).get("effective").get("checks").set("arrows", 1); },
            "Model/check summary mismatch");
        policy_rejects(
            [&](auto root) { entry(root).get("effective").get("checks").set("arrows", false); },
            "Model/check summary mismatch");
        policy_rejects(
            [&](auto root) { entry(root).get("effective").get("checks").set("ordering", "wrong"); },
            "ordering type mismatch");
        policy_rejects(
            [&](auto root) { entry(root).get("effective").set("snapshot", "../escape.txt"); },
            "unsafe/unknown snapshot name");
        policy_rejects([&](auto root) { entry(root).set("snapshot_sha256", "wrong"); },
                       "snapshot SHA mismatch");
        rejects(policy_bytes, fact_bytes, raw + " ", snapshot, "raw binding mismatch");
        rejects(policy_bytes, fact_bytes, raw, snapshot + " ", "snapshot SHA mismatch");
        auto fact_rejects = [&](const std::function<void(json::JsonMutVal)>& change,
                                std::string_view error) {
            rejects(policy_bytes, projection_mutate(fact_bytes, change), raw, snapshot, error);
        };
        fact_rejects([](auto root) { root.set("schema", true); }, "actual source-fact schema");
        fact_rejects([](auto root) { root.set("model_schema", "other"); },
                     "actual model schema mismatch");
        fact_rejects([](auto root) { root.add("cases", root.make_arr()); },
                     "source-fact case inventory mismatch");
        fact_rejects([](auto root) { root.get("cases").at(0).set("id", "other"); },
                     "source-fact identity mismatch");
        fact_rejects([](auto root) { root.get("cases").at(0).set("body", "changed"); },
                     "body binding mismatch");
        fact_rejects([](auto root) { root.get("cases").at(0).set("body_model", false); },
                     "full Model type mismatch");
        fact_rejects(
            [](auto root) {
                root.get("cases").at(0).get("body_model").get("messages").at(0).set("number", 10);
            },
            "full Model binding mismatch");
        fact_rejects([](auto root) { root.get("cases").at(0).set("body_model_sha256", "wrong"); },
                     "full Model binding mismatch");

        const auto& other =
            kProjectionBindings[binding.fact_file == kProjectionBindings[0].fact_file ? 1 : 0];
        policy_rejects([&](auto root) { root.get("source_facts").set("path", other.fact_file); },
                       "source-fact path mismatch");
        policy_rejects(
            [&](auto root) { root.get("source_facts").set("model_schema", other.model_schema); },
            "model schema mismatch");
        policy_rejects([&](auto root) { entry(root).set("full_model_sha256", other.model_sha); },
                       "full Model binding mismatch");
        policy_rejects(
            [&](auto root) { entry(root).get("effective").set("snapshot", other.snapshot_file); },
            "unsafe/unknown snapshot name");
        // yyjson retains duplicate JSON members; require_fields must reject them.
        auto duplicate = policy_bytes;
        duplicate.insert(duplicate.find('{') + 1, "\"schema\":1,");
        rejects(duplicate, fact_bytes, raw, snapshot, "duplicate object key");
    }
}

TEST(MermaidOfficial, RawXYProjectionBindsCompleteSourceModelMetadataAndSnapshot) {
    const auto& binding = kProjectionBindings.at(2);
    ASSERT_EQ(binding.kind, ProjectionKind::XYOriginal);
    const auto policy_bytes = read_file(kNativeRoot / binding.policy_file);
    const auto fact_bytes = read_file(kNativeRoot / binding.fact_file);
    for (const auto file : {binding.policy_file, binding.fact_file, binding.snapshot_file})
        EXPECT_NO_THROW(projection_regular_file(kNativeRoot, kNativeRoot / file));
    EXPECT_THROW(projection_regular_file(kNativeRoot, kNativeRoot / ".." / binding.fact_file),
                 std::runtime_error);
    EXPECT_NO_THROW(projection_byte_pins(policy_bytes, fact_bytes, binding));
    EXPECT_THROW(projection_byte_pins(policy_bytes + " ", fact_bytes, binding), std::runtime_error);
    EXPECT_THROW(projection_byte_pins(policy_bytes, fact_bytes + " ", binding), std::runtime_error);
    auto policy = json::parse(policy_bytes), facts = json::parse(fact_bytes);
    auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(policy && facts && manifest);
    const auto path = projection_source_path(facts->root(), binding);
    ASSERT_TRUE(path.is_str());
    const auto raw = read_file(kRoot / "cases" / path.as_str());
    const auto snapshot = read_file(kNativeRoot / binding.snapshot_file);
    EXPECT_NO_THROW(validate_projection_binding(policy->root(), facts->root(), manifest->root(),
                                                raw, snapshot, binding));
    EXPECT_NO_THROW(projection_consumed(1));
    EXPECT_THROW(projection_consumed(0), std::runtime_error);
    EXPECT_THROW(projection_consumed(2), std::runtime_error);
    EXPECT_FALSE(fs::exists(kRoot / "snapshots" / binding.snapshot_file));
    const auto temporary =
        fs::temp_directory_path() /
        ("loom-xy-projection-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    struct Cleanup {
        fs::path path;

        ~Cleanup() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    } cleanup{temporary};

    ASSERT_TRUE(fs::create_directory(temporary));
    fs::create_symlink(kNativeRoot / binding.fact_file, temporary / "fact.json");
    EXPECT_THROW(projection_regular_file(temporary, temporary / "fact.json"), std::runtime_error);
    fs::create_directory_symlink(kNativeRoot, temporary / "root");
    EXPECT_THROW(
        projection_regular_file(temporary / "root", temporary / "root" / binding.fact_file),
        std::runtime_error);
    const std::array<ProjectionBinding, 1> single{binding};
    std::vector<json::JsonDoc> policies;
    policies.push_back(std::move(*policy));
    EXPECT_EQ(projection_select(policies, single, path.as_str(), "original"), 0u);
    EXPECT_FALSE(projection_select(policies, single, path.as_str(), "body_diagnostic"));
    EXPECT_FALSE(projection_select(policies, single, "xychart/missing.mmd", "original"));
    EXPECT_THROW(projection_select(policies, single, path.as_str(), "unknown"), std::runtime_error);
    auto duplicate = json::parse(policy_bytes);
    ASSERT_TRUE(duplicate);
    policies.push_back(std::move(*duplicate));
    const std::array<ProjectionBinding, 2> repeated{binding, binding};
    EXPECT_THROW(projection_select(policies, repeated, path.as_str(), "original"),
                 std::runtime_error);
    auto wrong_track_kind = binding;
    wrong_track_kind.kind = ProjectionKind::SequenceBody;
    const std::array<ProjectionBinding, 2> mismatched{binding, wrong_track_kind};
    EXPECT_THROW(projection_select(policies, mismatched, path.as_str(), "original"),
                 std::runtime_error);
    auto invalid = binding;
    invalid.kind = static_cast<ProjectionKind>(-1);
    EXPECT_THROW(projection_track(invalid), std::runtime_error);
    EXPECT_THROW(projection_source_path(facts->root(), invalid), std::runtime_error);
    EXPECT_THROW(validate_projection_binding(policies[0].root(), facts->root(), manifest->root(),
                                             raw, snapshot, invalid),
                 std::runtime_error);
    const std::array<ProjectionBinding, 2> invalid_bindings{binding, invalid};
    EXPECT_THROW(projection_select(policies, invalid_bindings, path.as_str(), "original"),
                 std::runtime_error);
}

TEST(MermaidOfficial, RawXYProjectionMutationsReachExactTupleValidators) {
    const auto& binding = kProjectionBindings.at(2);
    const auto policy_bytes = read_file(kNativeRoot / binding.policy_file);
    const auto fact_bytes = read_file(kNativeRoot / binding.fact_file);
    auto facts = json::parse(fact_bytes),
         manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(facts && manifest);
    const auto raw = read_file(kRoot / "cases" / facts->root().get("source").get_string("path"));
    const auto snapshot = read_file(kNativeRoot / binding.snapshot_file);
    auto rejects = [&](std::string_view policy_text, std::string_view facts_text,
                       std::string_view raw_text, std::string_view snapshot_text,
                       std::string_view expected_error) {
        auto policy = json::parse(policy_text), altered_facts = json::parse(facts_text);
        ASSERT_TRUE(policy && altered_facts);
        try {
            validate_projection_binding(policy->root(), altered_facts->root(), manifest->root(),
                                        raw_text, snapshot_text, binding);
            ADD_FAILURE() << "Mutation was accepted; expected " << expected_error;
        } catch (const std::runtime_error& error) {
            EXPECT_NE(std::string_view(error.what()).find(expected_error), std::string_view::npos)
                << error.what();
        }
    };
    auto entry = [](json::JsonMutVal root) { return root.get("promotions").at(0); };
    auto policy_rejects = [&](const std::function<void(json::JsonMutVal)>& change,
                              std::string_view error) {
        rejects(projection_mutate(policy_bytes, change), fact_bytes, raw, snapshot, error);
    };
    auto fact_rejects = [&](const std::function<void(json::JsonMutVal)>& change,
                            std::string_view error) {
        rejects(policy_bytes, projection_mutate(fact_bytes, change), raw, snapshot, error);
    };
    // Reach semantic guards even after the fact and promotion Model digests are synchronized.
    auto model_rejects = [&](const std::function<void(json::JsonMutVal)>& change,
                             std::string_view error) {
        auto changed =
            projection_mutate(fact_bytes, [&](auto root) { change(root.get("source_model")); });
        auto parsed = json::parse(changed);
        ASSERT_TRUE(parsed);
        const auto sha = digest(projection_canonical(parsed->root().get("source_model")));
        changed =
            projection_mutate(changed, [&](auto root) { root.set("source_model_sha256", sha); });
        const auto changed_policy = projection_mutate(
            policy_bytes, [&](auto root) { entry(root).set("full_model_sha256", sha); });
        rejects(changed_policy, changed, raw, snapshot, error);
    };
    policy_rejects([](auto root) { root.set("schema", true); }, "XY policy schema");
    policy_rejects([](auto root) { root.set("schema", "1"); }, "XY policy schema");
    policy_rejects([](auto root) { root.set("extra", 1); }, "unknown field");
    policy_rejects([](auto root) { static_cast<void>(root.remove("schema")); }, "missing field");
    policy_rejects([](auto root) { root.set("baseline_manifest_sha256", "wrong"); },
                   "XY projection baseline manifest mismatch");
    policy_rejects([](auto root) { root.get("source_facts").set("path", "../escape.json"); },
                   "XY projection source-fact path mismatch");
    policy_rejects([](auto root) { root.get("source_facts").set("sha256", "wrong"); },
                   "XY projection source-fact reference mismatch");
    policy_rejects(
        [](auto root) {
            root.get("source_facts")
                .set("model_schema", "sequence_native_full_model_number_text/v1");
        },
        "XY projection source-fact reference mismatch");
    policy_rejects([](auto root) { root.add("promotions", root.make_arr()); },
                   "XY projection promotion inventory mismatch");
    policy_rejects([&](auto root) { entry(root).set("path", "../escape.mmd"); },
                   "XY projection unsafe fixture path");
    policy_rejects([&](auto root) { entry(root).set("path", "xychart/missing.mmd"); },
                   "XY projection unknown/duplicate raw fixture");
    policy_rejects([&](auto root) { entry(root).set("track", "body_diagnostic"); },
                   "XY projection unknown track");
    policy_rejects([&](auto root) { entry(root).set("raw_bytes", true); }, "XY raw bytes");
    policy_rejects([&](auto root) { entry(root).set("raw_sha256", "wrong"); },
                   "XY projection raw binding mismatch");
    for (const int offset : {-1, 59, 223})
        policy_rejects([&](auto root) { entry(root).set("body_offset_utf8_bytes", offset); },
                       "XY body offset");
    policy_rejects([&](auto root) { entry(root).set("body_offset_utf8_bytes", "58"); },
                   "XY body offset");
    policy_rejects([&](auto root) { entry(root).set("body_sha256", "wrong"); },
                   "XY projection body binding mismatch");
    std::string diagnostic_sha;
    manifest->root().get("fixtures").iter([&](JsonVal fixture) {
        if (diagnostic_sha.empty() && fixture.has("diagnostic"))
            diagnostic_sha = digest(projection_canonical(fixture.get("diagnostic")));
    });
    ASSERT_FALSE(diagnostic_sha.empty());
    policy_rejects(
        [&](auto root) { entry(root).set("baseline_expectation_sha256", diagnostic_sha); },
        "XY projection raw baseline expectation mismatch");
    policy_rejects([&](auto root) { entry(root).set("full_model_sha256", "wrong"); },
                   "XY projection full Model binding mismatch");
    policy_rejects([&](auto root) { entry(root).get("effective").set("expected", "source"); },
                   "XY projection effective outcome/profile mismatch");
    policy_rejects(
        [&](auto root) { entry(root).set("reason", "Original browser predicates passed"); },
        "XY projection foreign pending scope mismatch");
    policy_rejects(
        [&](auto root) { entry(root).get("effective").get("checks").set("arrows", true); },
        "XY summary arrows");
    policy_rejects(
        [&](auto root) {
            entry(root).get("effective").get("checks").add("labels", root.make_arr());
        },
        "XY projection summary label inventory mismatch");
    policy_rejects(
        [&](auto root) {
            auto labels = root.make_arr();
            for (const auto label : {"Wrong title", "Elapsed Time", "Percentage (%)", "CPU"})
                labels.append(root.make_str(label));
            entry(root).get("effective").get("checks").add("labels", labels);
        },
        "XY projection summary label mismatch");
    policy_rejects(
        [&](auto root) { entry(root).get("effective").get("checks").set("ordering", true); },
        "XY projection summary ordering mismatch");
    policy_rejects(
        [&](auto root) { entry(root).get("effective").set("snapshot", "../escape.txt"); },
        "XY projection unsafe/unknown snapshot name");
    policy_rejects([&](auto root) { entry(root).add("snapshot_sha256", root.make_null()); },
                   "XY projection snapshot SHA mismatch");
    policy_rejects([&](auto root) { entry(root).set("snapshot_sha256", "wrong"); },
                   "XY projection snapshot SHA mismatch");
    rejects(policy_bytes, fact_bytes, raw.substr(58), snapshot,
            "XY projection raw binding mismatch");
    rejects(policy_bytes, fact_bytes, raw + " ", snapshot, "XY projection raw binding mismatch");
    rejects(policy_bytes, fact_bytes, raw, snapshot + " ", "XY projection snapshot SHA mismatch");
    fact_rejects([](auto root) { root.get("source").set("git_blob", "wrong"); },
                 "XY projection source identity mismatch");
    fact_rejects([](auto root) { root.get("source").set("unexpected", 1); }, "unknown field");
    fact_rejects([](auto root) { root.get("source").set("body_bytes", 163); }, "XY body bytes");
    fact_rejects([](auto root) { root.get("source").add("physical_witnesses", root.make_arr()); },
                 "XY projection witness inventory mismatch");
    fact_rejects(
        [](auto root) {
            root.get("source").get("physical_witnesses").at(4).set("byte_offset", 40);
        },
        "XY witness offset");
    fact_rejects(
        [](auto root) {
            root.get("source").get("physical_witnesses").at(4).set("byte_length", true);
        },
        "XY witness length");
    fact_rejects(
        [](auto root) { root.get("source").get("physical_witnesses").at(4).set("text", "other"); },
        "XY projection witness bytes/SHA mismatch");
    fact_rejects(
        [](auto root) {
            root.get("source").get("physical_witnesses").at(4).set("sha256", "wrong");
        },
        "XY projection witness bytes/SHA mismatch");
    fact_rejects([](auto root) { root.set("source_model_sha256", "wrong"); },
                 "XY projection full Model binding mismatch");
    fact_rejects([](auto root) { root.set("source_model", false); }, "expected object");
    model_rejects([](auto model) { model.set("schema", "other"); },
                  "XY projection title/schema/orientation mismatch");
    model_rejects([](auto model) { model.set("title", ""); },
                  "XY projection title/schema/orientation mismatch");
    model_rejects([](auto model) { model.get("xAxis").set("type", "band"); },
                  "XY projection axis identity mismatch");
    model_rejects([](auto model) { model.get("yAxis").set("max", 101); },
                  "XY projection numeric/type mismatch: yAxis");
    model_rejects([](auto model) { model.get("xAxis").set("min", false); },
                  "XY projection numeric/type mismatch: xAxis");
    model_rejects([](auto model) { model.get("plots").at(0).set("type", "bar"); },
                  "XY projection plot identity mismatch");
    model_rejects([](auto model) { model.get("plots").at(0).set("title", "Other"); },
                  "XY projection plot identity mismatch");
    model_rejects([](auto model) { model.get("plots").at(0).add("values", model.make_arr()); },
                  "XY projection sample inventory mismatch");
    model_rejects(
        [](auto model) {
            model.get("plots").at(0).get("source_derived_samples").at(3).set("index", 2);
        },
        "XY sample order");
    model_rejects(
        [](auto model) {
            model.get("plots").at(0).get("source_derived_samples").at(3).set("y", 41);
        },
        "XY projection numeric/type mismatch: sample y");
    model_rejects(
        [](auto model) {
            model.get("plots")
                .at(0)
                .get("source_derived_samples")
                .at(3)
                .set("x_binary64", 3.333333333333333);
        },
        "XY projection numeric/type mismatch: sample x");
    model_rejects(
        [](auto model) {
            model.get("plots")
                .at(0)
                .get("source_derived_samples")
                .at(7)
                .set("x_binary64", 70.0 / 9.0);
        },
        "XY projection numeric/type mismatch: sample x");
    model_rejects(
        [](auto model) {
            model.get("plots")
                .at(0)
                .get("source_derived_samples")
                .at(3)
                .set("x_binary64", "3.3333333333333335");
        },
        "XY projection numeric/type mismatch: sample x");
    model_rejects(
        [](auto model) {
            model.get("plots").at(0).get("source_derived_samples").at(3).set("x_exact", "30/9");
        },
        "XY projection rational x mismatch");
    model_rejects(
        [](auto model) {
            auto values = model.make_arr();
            for (int n = 1; n <= 10; ++n)
                values.append(model.make_num(n == 7 ? 71 : n * 10));
            model.get("plots").at(0).add("values", values);
        },
        "XY projection numeric/type mismatch: value");
    model_rejects([](auto model) { static_cast<void>(model.remove("metadata")); }, "missing field");
    model_rejects(
        [](auto model) { model.get("metadata").get("config").get("xyChart").set("width", 1201); },
        "XY metadata width");
    model_rejects(
        [](auto model) { model.get("metadata").get("config").get("xyChart").set("height", "60"); },
        "XY metadata height");
    model_rejects(
        [](auto model) { model.get("metadata").get("config").get("xyChart").set("height", true); },
        "XY metadata height");
    model_rejects([](auto model) { model.get("metadata").set("title", "Lost source title"); },
                  "XY projection metadata title/description mismatch");
    model_rejects([](auto model) { model.get("accessibility").set("title", "invented"); },
                  "XY projection accessibility mismatch");
    model_rejects(
        [](auto model) {
            auto items = model.make_arr();
            items.append(model.make_str("extra"));
            model.add("init_directives", items);
        },
        "XY projection annotation/init mismatch");
    for (std::size_t n = 0; n < 2; ++n) {
        const auto& other = kProjectionBindings[n];
        rejects(policy_bytes, read_file(kNativeRoot / other.fact_file), raw, snapshot,
                "XY projection source identity mismatch");
        policy_rejects([&](auto root) { root.get("source_facts").set("path", other.fact_file); },
                       "XY projection source-fact path mismatch");
        policy_rejects(
            [&](auto root) { entry(root).get("effective").set("snapshot", other.snapshot_file); },
            "XY projection unsafe/unknown snapshot name");
        policy_rejects([&](auto root) { entry(root).set("full_model_sha256", other.model_sha); },
                       "XY projection full Model binding mismatch");
    }
    auto duplicate = policy_bytes;
    duplicate.insert(duplicate.find('{') + 1, "\"schema\":1,");
    rejects(duplicate, fact_bytes, raw, snapshot, "duplicate object key");
    auto original = json::parse(policy_bytes);
    ASSERT_TRUE(original);
    duplicate = policy_bytes;
    duplicate.insert(duplicate.rfind(']'),
                     "," + original->root().get("promotions").at(0).to_string());
    rejects(duplicate, fact_bytes, raw, snapshot, "XY projection promotion inventory mismatch");
}

}  // namespace

TEST(MermaidOfficial, RoleBodyBatchBindsEveryCompleteCaseAndEveryOriginalApplication) {
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest);
    auto batch = load_role_batch(manifest->root());
    ASSERT_EQ(batch.facts.root().get("cases").size(), kRoleBatchCases);
    ASSERT_EQ(batch.policy.root().get("promotions").size(), kRoleBatchPromotions);
    const auto legacy_fact_bytes = read_file(kNativeRoot / "sequence_official_role38_facts.json");
    const auto legacy_policy_bytes =
        read_file(kNativeRoot / "sequence_official_roles38_projection_supplement.json");
    const auto appended_bytes =
        read_file(kNativeRoot / "sequence_official_role_fragment6_facts.json");
    ASSERT_EQ(digest(legacy_fact_bytes),
              "8cb922743c1a430dbbefa04ac7c02f2b0d992737d6b6b7ebba98380de199a773");
    ASSERT_EQ(digest(legacy_policy_bytes),
              "2ab37e03f0fd670c2c5e4d03362f26eef37ac005265fa21c130287b78afb7e48");
    ASSERT_EQ(digest(appended_bytes),
              "11099c79ad2743267e943ae9343604497a8d72a6351bb7f7f421e3841843c21d");
    const auto legacy_facts = json::parse(legacy_fact_bytes),
               legacy_policy = json::parse(legacy_policy_bytes),
               appended = json::parse(appended_bytes);
    ASSERT_TRUE(legacy_facts && legacy_policy && appended);
    for (std::size_t n = 0; n < kRoleBatchLegacyCases; ++n)
        EXPECT_EQ(projection_canonical(batch.facts.root().get("cases").at(n)),
                  projection_canonical(legacy_facts->root().get("cases").at(n)));
    for (std::size_t n = 0; n < kRoleBatchLegacyPromotions; ++n)
        EXPECT_EQ(projection_canonical(batch.policy.root().get("promotions").at(n)),
                  projection_canonical(legacy_policy->root().get("promotions").at(n)));
    for (std::size_t n = kRoleBatchLegacyCases; n < kRoleBatchRoleCases; ++n)
        EXPECT_EQ(
            projection_canonical(batch.facts.root().get("cases").at(n)),
            projection_canonical(appended->root().get("cases").at(n - kRoleBatchLegacyCases)));
    const auto parent_fact_bytes = read_file(kNativeRoot / "sequence_official_role41_facts.json");
    const auto parent_policy_bytes =
        read_file(kNativeRoot / "sequence_official_roles41_projection_supplement.json");
    const auto half_bytes = read_file(kNativeRoot / "sequence_official_half16_facts.json");
    ASSERT_EQ(digest(parent_fact_bytes),
              "15db9135e6bdd51a9ce498addeb16ee62448642f871f3571abd0afda5466dedd");
    ASSERT_EQ(digest(parent_policy_bytes),
              "9cedac099638e503c8f98ae6b78887544c27ce84676278ece98fe52958d0395b");
    ASSERT_EQ(digest(half_bytes),
              "c646f6602c2af4f15e56c334e89df1f467a227ff00c35a13aa200df8bcbdfee4");
    const auto parent_facts = json::parse(parent_fact_bytes),
               parent_policy = json::parse(parent_policy_bytes), half = json::parse(half_bytes);
    ASSERT_TRUE(parent_facts && parent_policy && half);
    for (std::size_t n = 0; n < kRoleBatchRoleCases; ++n)
        EXPECT_EQ(projection_canonical(batch.facts.root().get("cases").at(n)),
                  projection_canonical(parent_facts->root().get("cases").at(n)));
    for (std::size_t n = 0; n < kRoleBatchRolePromotions; ++n)
        EXPECT_EQ(projection_canonical(batch.policy.root().get("promotions").at(n)),
                  projection_canonical(parent_policy->root().get("promotions").at(n)));
    for (std::size_t n = kRoleBatchRoleCases; n < kRoleBatchPriorCases; ++n) {
        EXPECT_EQ(projection_canonical(batch.facts.root().get("cases").at(n)),
                  projection_canonical(half->root().get("cases").at(n - kRoleBatchRoleCases)));
        const auto checks =
            json::parse(role_batch_summary(batch.facts.root().get("cases").at(n).get("model")));
        ASSERT_TRUE(checks);
        EXPECT_EQ(checks->root().get_int("arrows"), 0);
    }
    const auto prior_fact_bytes = read_file(kNativeRoot / "sequence_official_role57_facts.json"),
               prior_policy_bytes =
                   read_file(kNativeRoot / "sequence_official_roles57_projection_supplement.json");
    ASSERT_EQ(digest(prior_fact_bytes),
              "9dbb2b2faf9b8fa85b41730df983fdd059ea859f180f896cc71fd33e54a69709");
    ASSERT_EQ(digest(prior_policy_bytes),
              "b2d66a03dcf570c081a42773d949fb75746c69797f5da17ccec3f32205f7d67c");
    const auto prior_facts = json::parse(prior_fact_bytes),
               prior_policy = json::parse(prior_policy_bytes);
    ASSERT_TRUE(prior_facts && prior_policy);
    for (std::size_t n = 0; n < kRoleBatchPriorCases; ++n)
        EXPECT_EQ(projection_canonical(batch.facts.root().get("cases").at(n)),
                  projection_canonical(prior_facts->root().get("cases").at(n)));
    for (std::size_t n = 0; n < kRoleBatchPriorPromotions; ++n)
        EXPECT_EQ(projection_canonical(batch.policy.root().get("promotions").at(n)),
                  projection_canonical(prior_policy->root().get("promotions").at(n)));
    for (std::size_t n = kRoleBatchPriorCases; n < kRoleBatchCases; ++n)
        EXPECT_EQ(projection_canonical(batch.facts.root().get("cases").at(n)),
                  projection_canonical(
                      batch.config_authority.root().get("cases").at(n - kRoleBatchPriorCases)));
    std::vector<json::JsonDoc> old;
    for (const auto& binding : kProjectionBindings) {
        auto policy = json::parse(read_file(kNativeRoot / binding.policy_file));
        ASSERT_TRUE(policy);
        old.push_back(std::move(*policy));
    }
    RoleBatchConsumption consumed;
    const auto entries = batch.policy.root().get("promotions");
    for (std::size_t n = 0; n < entries.size(); ++n) {
        const auto entry = entries.at(n);
        const auto selected = projection_select_complete(
            old, batch.policy.root(), entry.get_string("path"), "body_diagnostic");
        ASSERT_TRUE(selected);
        EXPECT_EQ(selected->binding, kRoleBatchIndex);
        EXPECT_EQ(selected->promotion, n);
        role_batch_consume(consumed, batch.policy.root(), n);
        EXPECT_FALSE(projection_select_complete(old, batch.policy.root(), entry.get_string("path"),
                                                "original"));
        EXPECT_FALSE(
            fs::exists(kRoot / "snapshots" / entry.get("effective").get_string("snapshot")));
    }
    EXPECT_NO_THROW(role_batch_consumed(consumed, batch.facts.root()));
    bool same_model_distinct_bodies = false;
    const auto cases = batch.facts.root().get("cases");
    for (std::size_t n = 0; n < cases.size(); ++n)
        for (std::size_t m = n + 1; m < cases.size(); ++m)
            if (cases.at(n).get_string("model_canonical_sha256") ==
                cases.at(m).get_string("model_canonical_sha256")) {
                same_model_distinct_bodies = true;
                EXPECT_NE(cases.at(n).get_string("body_sha256"),
                          cases.at(m).get_string("body_sha256"));
                EXPECT_NE(role_batch_snapshot_name(cases.at(n), n),
                          role_batch_snapshot_name(cases.at(m), m));
            }
    EXPECT_TRUE(same_model_distinct_bodies);
}

TEST(MermaidOfficial, RoleBodyBatchParsedMutationsReachTheirSpecificTupleAndSchemaGuards) {
    const auto policy_bytes = read_file(kNativeRoot / kRoleBatchBinding.policy_file),
               fact_bytes = read_file(kNativeRoot / kRoleBatchBinding.fact_file);
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest);
    const auto parsed_policy = json::parse(policy_bytes);
    ASSERT_TRUE(parsed_policy);
    const auto first_path = parsed_policy->root().get("promotions").at(0).get_string("path");
    const auto config_authority = load_role_config_authority();
    auto raw_read = [](std::string_view path) { return read_file(kRoot / "cases" / path); };
    auto snapshot_read = [](std::string_view path) {
        projection_regular_file(kNativeRoot, kNativeRoot / path);
        return read_file(kNativeRoot / path);
    };
    auto rejects = [&](std::string policy_text, const std::string& facts_text,
                       std::string_view expected_error, bool poison_raw = false,
                       bool poison_snapshot = false) {
        // Refresh the aggregate fact reference and descriptor pins after each
        // mutation, then call the parsed validator. A generic byte-pin failure
        // cannot satisfy the expected branch-specific diagnostic assertion.
        const auto fact_sha = digest(facts_text);
        policy_text = projection_mutate(
            policy_text, [&](auto root) { root.get("source_facts").set("sha256", fact_sha); });
        const auto policy_sha = digest(policy_text);
        auto binding = kRoleBatchBinding;
        binding.fact_sha = fact_sha;
        binding.policy_sha = policy_sha;
        auto policy = json::parse(policy_text), facts = json::parse(facts_text);
        ASSERT_TRUE(policy && facts);
        try {
            validate_role_batch(
                policy->root(), facts->root(), manifest->root(),
                [&](std::string_view path) {
                    auto raw = raw_read(path);
                    if (poison_raw && path == first_path) raw += ' ';
                    return raw;
                },
                [&](std::string_view path) {
                    auto snapshot = snapshot_read(path);
                    if (poison_snapshot) snapshot += ' ';
                    return snapshot;
                },
                config_authority.root(), binding);
            ADD_FAILURE() << "Role batch parsed mutation accepted; expected " << expected_error;
        } catch (const std::runtime_error& error) {
            EXPECT_NE(std::string_view(error.what()).find(expected_error), std::string_view::npos)
                << error.what();
        }
    };
    auto entry = [](auto root) { return root.get("promotions").at(0); };
    auto policy_rejects = [&](const std::function<void(json::JsonMutVal)>& change,
                              std::string_view message) {
        rejects(projection_mutate(policy_bytes, change), fact_bytes, message);
    };
    policy_rejects([](auto root) { root.set("unknown", true); }, "unknown field");
    policy_rejects([](auto root) { static_cast<void>(root.remove("schema")); }, "missing field");
    policy_rejects([](auto root) { root.set("schema", true); }, "integer/type mismatch");
    policy_rejects([](auto root) { root.set("kind", "unknown"); }, "policy kind mismatch");
    policy_rejects([](auto root) { root.set("baseline_manifest_sha256", "wrong"); },
                   "baseline manifest mismatch");
    policy_rejects([](auto root) { root.get("source_facts").set("path", "../escape.json"); },
                   "source-fact path mismatch");
    policy_rejects([](auto root) { root.get("source_facts").set("schema", false); },
                   "integer/type mismatch");
    policy_rejects([](auto root) { root.get("source_facts").set("model_schema", "foreign"); },
                   "source-fact reference mismatch");
    policy_rejects([&](auto root) { entry(root).set("path", "../escape.mmd"); },
                   "unsafe fixture path");
    policy_rejects([&](auto root) { entry(root).set("path", "sequence/missing.mmd"); },
                   "path/case binding missing");
    policy_rejects([&](auto root) { entry(root).set("track", "original"); }, "unknown track");
    policy_rejects([&](auto root) { entry(root).set("case_index", false); },
                   "case index/type mismatch");
    policy_rejects(
        [&](auto root) { entry(root).set("case_index", static_cast<int>(kRoleBatchCases)); },
        "case index/type mismatch");
    policy_rejects([&](auto root) { entry(root).set("case_id", "other"); },
                   "case identity mismatch");
    policy_rejects([&](auto root) { entry(root).set("raw_sha256", "wrong"); },
                   "raw binding mismatch");
    policy_rejects([&](auto root) { entry(root).set("body_offset_utf8_bytes", false); },
                   "body offset/type mismatch");
    policy_rejects([&](auto root) { entry(root).set("body_offset_utf8_bytes", 0); },
                   "body offset/type mismatch");
    policy_rejects([&](auto root) { entry(root).set("body_sha256", "wrong"); },
                   "exact body binding mismatch");
    policy_rejects([&](auto root) { entry(root).set("full_model_sha256", "wrong"); },
                   "full Model binding mismatch");
    policy_rejects([&](auto root) { entry(root).set("baseline_expectation_sha256", "wrong"); },
                   "baseline expectation mismatch");
    policy_rejects([&](auto root) { entry(root).get("effective").set("expected", "source"); },
                   "effective outcome mismatch");
    policy_rejects([&](auto root) { entry(root).set("reason", "All browser predicates passed"); },
                   "foreign pending scope mismatch");
    policy_rejects(
        [&](auto root) { entry(root).get("effective").get("checks").set("arrows", false); },
        "Model/check summary mismatch");
    policy_rejects(
        [&](auto root) {
            entry(root).get("effective").get("checks").add("labels", root.make_arr());
        },
        "Model/check summary mismatch");
    policy_rejects(
        [&](auto root) { entry(root).get("effective").set("snapshot", "../escape.txt"); },
        "unsafe/unknown snapshot name");
    policy_rejects([&](auto root) { entry(root).set("snapshot_sha256", "wrong"); },
                   "snapshot SHA mismatch");
    // The count-neutral appended reason is not permission to relabel old rows.
    policy_rejects(
        [](auto root) {
            root.get("promotions")
                .at(kRoleBatchLegacyPromotions)
                .set("reason", kRoleBatchLegacyScope);
        },
        "foreign pending scope mismatch");
    policy_rejects(
        [](auto root) { root.get("promotions").at(0).set("reason", kRoleBatchAppendedScope); },
        "foreign pending scope mismatch");
    policy_rejects(
        [](auto root) {
            root.get("promotions")
                .at(kRoleBatchLegacyPromotions)
                .get("effective")
                .set("reason", kRoleBatchLegacyScope);
        },
        "foreign pending scope mismatch");
    EXPECT_EQ(role_batch_case_scope(kRoleBatchLegacyCases - 1), kRoleBatchLegacyScope);
    EXPECT_EQ(role_batch_case_scope(kRoleBatchLegacyCases), kRoleBatchAppendedScope);
    EXPECT_THROW(role_batch_case_scope(kRoleBatchCases), std::runtime_error);
    policy_rejects(
        [](auto root) {
            root.get("promotions").at(kRoleBatchLegacyPromotions).set("case_index", 37);
        },
        "case identity mismatch");
    policy_rejects(
        [](auto root) {
            root.get("promotions").at(kRoleBatchLegacyPromotions).set("body_offset_utf8_bytes", 0);
        },
        "body offset/type mismatch");
    policy_rejects(
        [](auto root) {
            root.get("promotions").at(kRoleBatchLegacyPromotions).set("raw_sha256", "wrong");
        },
        "raw binding mismatch");
    policy_rejects(
        [](auto root) {
            root.get("promotions").at(kRoleBatchLegacyPromotions).set("body_sha256", "wrong");
        },
        "exact body binding mismatch");
    policy_rejects(
        [](auto root) {
            root.get("promotions").at(kRoleBatchLegacyPromotions).set("full_model_sha256", "wrong");
        },
        "full Model binding mismatch");
    policy_rejects(
        [](auto root) {
            root.get("promotions").at(kRoleBatchLegacyPromotions).set("snapshot_sha256", "wrong");
        },
        "snapshot SHA mismatch");
    auto inventory = [&](std::size_t count, bool duplicate) {
        json::JsonMutDoc edited;
        auto root = edited.copy_val(parsed_policy->root()), entries = root.make_arr();
        for (std::size_t n = 0; n < count; ++n)
            entries.append(
                edited.copy_val(parsed_policy->root()
                                    .get("promotions")
                                    .at(duplicate && n == 1 ? 0 : n % kRoleBatchPromotions)));
        root.add("promotions", entries);
        edited.set_root(root);
        return edited.to_string();
    };
    rejects(inventory(kRoleBatchPromotions + 1, false), fact_bytes, "promotion inventory mismatch");
    rejects(inventory(kRoleBatchPromotions - 1, false), fact_bytes, "promotion inventory mismatch");
    rejects(inventory(kRoleBatchPromotions, true), fact_bytes, "duplicate promotion");
    rejects(policy_bytes, fact_bytes, "raw binding mismatch", true, false);
    rejects(policy_bytes, fact_bytes, "snapshot SHA mismatch", false, true);
    auto model_rejects = [&](const std::function<void(json::JsonMutVal)>& change,
                             std::string_view message, std::size_t case_index = 0,
                             bool refresh_checks = false) {
        const auto field =
            case_index < kRoleBatchPriorCases ? "model" : "full_native_model_candidate";
        auto changed = projection_mutate(
            fact_bytes, [&](auto root) { change(root.get("cases").at(case_index).get(field)); });
        const auto parsed = json::parse(changed);
        ASSERT_TRUE(parsed);
        const auto model_sha = digest(projection_canonical(
            role_batch_model(parsed->root().get("cases").at(case_index), case_index)));
        changed = projection_mutate(changed, [&](auto root) {
            root.get("cases").at(case_index).set("model_canonical_sha256", model_sha);
        });
        std::optional<json::JsonDoc> updated_checks;
        if (refresh_checks) {
            auto checks = json::parse(role_batch_summary(
                role_batch_model(parsed->root().get("cases").at(case_index), case_index),
                case_index < kRoleBatchPriorCases ? RoleBatchNumbers::HiddenOnly
                                                  : RoleBatchNumbers::DisplayText));
            require(checks.has_value(), "Role batch parsed mutant summary construction");
            updated_checks.emplace(std::move(*checks));
        }
        const auto policy = projection_mutate(policy_bytes, [&](auto root) {
            for (std::size_t n = 0; n < root.get("promotions").size(); ++n)
                if (root.get("promotions").at(n).get("case_index").as_int() ==
                    static_cast<std::int64_t>(case_index)) {
                    auto item = root.get("promotions").at(n);
                    item.set("full_model_sha256", model_sha);
                    if (refresh_checks) {
                        require(updated_checks.has_value(),
                                "Role batch parsed mutant summary construction");
                        auto checks = root.make_obj(), labels = root.make_arr();
                        updated_checks->root().get("labels").iter(
                            [&](JsonVal text) { labels.append(root.make_str(text.as_str())); });
                        checks.add("labels", labels);
                        checks.set("arrows", updated_checks->root().get_int("arrows"));
                        checks.add("ordering", root.make_arr());
                        item.get("effective").add("checks", checks);
                    }
                }
        });
        rejects(policy, changed, message);
    };
    auto original_model_facts = json::parse(fact_bytes);
    ASSERT_TRUE(original_model_facts);
    std::optional<std::size_t> fragment_case;
    for (std::size_t n = 0; n < kRoleBatchCases && !fragment_case; ++n)
        if (original_model_facts->root().get("cases").at(n).get("model").get("fragments").size() >
            0)
            fragment_case = n;
    ASSERT_TRUE(fragment_case);
    model_rejects([](auto model) { model.get("fragments").at(0).set("kind", "Unknown"); },
                  "unknown fragment kind", *fragment_case);
    model_rejects([](auto model) { model.get("actors").at(0).set("role", "Unknown"); },
                  "unknown Role");
    model_rejects([](auto model) { model.get("messages").at(0).set("arrow", "Unknown"); },
                  "unknown Arrow");
    model_rejects([](auto model) { model.get("messages").at(0).set("number", 0); },
                  "DisplayText hidden-number mismatch");
    model_rejects([](auto model) { model.get("messages").at(0).set("number", "1"); },
                  "DisplayText hidden-number mismatch");
    model_rejects([](auto model) { model.get("order").at(0).set("kind", "Unknown"); },
                  "unknown ordered kind");
    model_rejects([](auto model) { model.get("messages").at(0).set("central_to", 0); },
                  "message flag/type mismatch");
    model_rejects([](auto model) { model.get("actors").at(0).set("role", "Control"); },
                  "Model/check summary mismatch");
    for (std::size_t n = kRoleBatchLegacyCases; n < kRoleBatchRoleCases; ++n)
        model_rejects([](auto model) { model.get("fragments").at(0).set("kind", "Unknown"); },
                      "unknown fragment kind", n);
    EXPECT_EQ(role_batch_case_scope(kRoleBatchRoleCases), kRoleBatchHalfScope);
    for (std::size_t n = kRoleBatchRoleCases; n < kRoleBatchPriorCases; ++n) {
        SCOPED_TRACE(n);
        const auto promotion = kRoleBatchRolePromotions + n - kRoleBatchRoleCases;
        model_rejects([](auto model) { model.get("messages").at(0).set("arrow", "HalfUnknown"); },
                      "unknown Arrow", n);
        model_rejects(
            [](auto model) {
                const auto old = model.get("messages").at(0).get("arrow").as_str();
                const auto changed = old.ends_with("Reverse")
                                         ? std::string(old.substr(0, old.size() - 7))
                                         : std::string(old) + "Reverse";
                model.get("messages").at(0).set("arrow", changed);
            },
            "half source Message/order witness mismatch", n);
        model_rejects([](auto model) { model.get("order").at(0).set("index", 1); },
                      "half source Message/order witness mismatch", n);
        policy_rejects(
            [&](auto root) { root.get("promotions").at(promotion).set("case_index", 0); },
            "case identity mismatch");
        policy_rejects(
            [&](auto root) { root.get("promotions").at(promotion).set("raw_sha256", "wrong"); },
            "raw binding mismatch");
        policy_rejects(
            [&](auto root) { root.get("promotions").at(promotion).set("body_sha256", "wrong"); },
            "exact body binding mismatch");
        policy_rejects(
            [&](auto root) {
                root.get("promotions").at(promotion).set("snapshot_sha256", "wrong");
            },
            "snapshot SHA mismatch");
        policy_rejects(
            [&](auto root) {
                root.get("promotions")
                    .at(promotion)
                    .get("effective")
                    .get("checks")
                    .set("arrows", 1);
            },
            "Model/check summary mismatch");
        policy_rejects(
            [&](auto root) {
                root.get("promotions").at(promotion).set("reason", kRoleBatchAppendedScope);
            },
            "foreign pending scope mismatch");
        const auto wrong_id = projection_mutate(fact_bytes, [&](auto root) {
            root.get("cases").at(n).set(
                "id",
                std::string("official-role-body/") +
                    std::string(root.get("cases").at(n).get("body_sha256").as_str().substr(0, 16)));
        });
        rejects(policy_bytes, wrong_id, "source case identity mismatch");
    }
    EXPECT_EQ(role_batch_case_scope(kRoleBatchPriorCases - 1), kRoleBatchHalfScope);
    EXPECT_EQ(role_batch_case_scope(kRoleBatchPriorCases), kRoleBatchConfigScope);
    EXPECT_THROW(role_batch_model(config_authority.root().get("cases").at(0), kRoleBatchCases),
                 std::runtime_error);
    // Every new source uses its direct fields, with one promotion and an actual
    // independently pinned source authority. Refresh coupled hashes below.
    for (std::size_t n = kRoleBatchPriorCases; n < kRoleBatchCases; ++n) {
        SCOPED_TRACE(n);
        const auto promotion = kRoleBatchPriorPromotions + n - kRoleBatchPriorCases;
        policy_rejects(
            [&](auto root) {
                root.get("promotions")
                    .at(promotion)
                    .set("case_index", static_cast<int>(kRoleBatchPriorCases - 1));
            },
            "case identity mismatch");
        policy_rejects(
            [&](auto root) { root.get("promotions").at(promotion).set("track", "original"); },
            "unknown track");
        policy_rejects(
            [&](auto root) {
                root.get("promotions").at(promotion).set("reason", kRoleBatchHalfScope);
            },
            "foreign pending scope mismatch");
        policy_rejects(
            [&](auto root) {
                root.get("promotions").at(promotion).set("body_offset_utf8_bytes", 0);
            },
            "body offset/type mismatch");
        policy_rejects(
            [&](auto root) { root.get("promotions").at(promotion).set("raw_sha256", "wrong"); },
            "raw binding mismatch");
        policy_rejects(
            [&](auto root) { root.get("promotions").at(promotion).set("body_sha256", "wrong"); },
            "exact body binding mismatch");
        policy_rejects(
            [&](auto root) {
                root.get("promotions").at(promotion).set("snapshot_sha256", "wrong");
            },
            "snapshot SHA mismatch");
        const auto wrong_metadata = projection_mutate(fact_bytes, [&](auto root) {
            root.get("cases").at(n).get("leading_init_authored").set("unreviewed", true);
        });
        rejects(policy_bytes, wrong_metadata, "complete config source authority mismatch");
        const auto missing_witness = projection_mutate(fact_bytes, [&](auto root) {
            root.get("cases").at(n).add("all_body_physical_statement_bindings", root.make_arr());
        });
        rejects(policy_bytes, missing_witness, "complete config source authority mismatch");
        const auto meaning =
            role_batch_model(config_authority.root().get("cases").at(n - kRoleBatchPriorCases), n);
        if (meaning.get("messages").size() > 0) {
            model_rejects([](auto model) { model.get("messages").at(0).set("number", 0); },
                          "DisplayText number type mismatch", n);
            model_rejects([](auto model) { model.get("messages").at(0).set("number", "999"); },
                          "complete config source authority mismatch", n, true);
            model_rejects(
                [](auto model) {
                    const auto old = model.get("messages").at(0).get("central_from").as_bool();
                    model.get("messages").at(0).set("central_from", !old);
                },
                "complete config source authority mismatch", n, true);
            model_rejects([](auto model) { model.get("messages").at(0).set("arrow", "Unknown"); },
                          "unknown Arrow", n);
        }
    }
    auto duplicate_keys = policy_bytes;
    duplicate_keys.insert(duplicate_keys.find('{') + 1, "\"schema\":1,");
    rejects(duplicate_keys, fact_bytes, "duplicate object key");
    // Add an unreferenced raw-source occurrence without duplicating any actual
    // promoted path; the final frequency guard, not a duplicate-path check, fires.
    const auto original_facts = json::parse(fact_bytes);
    ASSERT_TRUE(original_facts);
    json::JsonMutDoc wrong_frequency;
    auto changed_root = wrong_frequency.copy_val(original_facts->root());
    auto extra = wrong_frequency.copy_val(
        original_facts->root().get("cases").at(0).get("original_bindings").at(0));
    extra.set("path", "sequence/not-promoted.mmd");
    changed_root.get("cases").at(0).get("original_bindings").append(extra);
    wrong_frequency.set_root(changed_root);
    rejects(policy_bytes, wrong_frequency.to_string(),
            "case occurrence mismatch/unreferenced case");
}

TEST(MermaidOfficial, RoleBodyBatchSelectorAndConsumedOnceKeepBothDescriptorCoordinates) {
    const auto policy = json::parse(read_file(kNativeRoot / kRoleBatchBinding.policy_file));
    const auto facts = json::parse(read_file(kNativeRoot / kRoleBatchBinding.fact_file));
    ASSERT_TRUE(policy && facts);
    std::vector<json::JsonDoc> old;
    for (const auto& binding : kProjectionBindings) {
        auto doc = json::parse(read_file(kNativeRoot / binding.policy_file));
        ASSERT_TRUE(doc);
        old.push_back(std::move(*doc));
    }
    const auto path = policy->root().get("promotions").at(0).get_string("path");
    auto bad = kRoleBatchBinding;
    bad.kind = static_cast<RoleBatchKind>(-1);
    EXPECT_THROW(projection_select_complete(old, policy->root(), path, "body_diagnostic", bad),
                 std::runtime_error);
    EXPECT_THROW(projection_select_complete(old, policy->root(), path, "unknown"),
                 std::runtime_error);
    EXPECT_FALSE(
        projection_select_complete(old, policy->root(), "sequence/missing.mmd", "body_diagnostic"));
    const auto first = projection_select_complete(old, policy->root(), path, "body_diagnostic");
    ASSERT_TRUE(first);
    EXPECT_EQ(first->binding, kRoleBatchIndex);
    EXPECT_EQ(first->promotion, 0u);
    const auto duplicate = projection_mutate(old[0].root().to_string(), [&](auto root) {
        root.get("promotions").at(0).set("path", path);
    });
    auto duplicate_doc = json::parse(duplicate);
    ASSERT_TRUE(duplicate_doc);
    old[0] = std::move(*duplicate_doc);
    EXPECT_THROW(projection_select_complete(old, policy->root(), path, "body_diagnostic"),
                 std::runtime_error);
    RoleBatchConsumption consumed;
    EXPECT_THROW(role_batch_consumed(consumed, facts->root()), std::runtime_error);
    for (std::size_t n = 0; n < kRoleBatchPromotions; ++n)
        role_batch_consume(consumed, policy->root(), n);
    EXPECT_NO_THROW(role_batch_consumed(consumed, facts->root()));
    EXPECT_THROW(role_batch_consume(consumed, policy->root(), 0), std::runtime_error);
    consumed.promotions[0] = 1;
    --consumed.cases[0];
    ++consumed.cases[1];
    EXPECT_THROW(role_batch_consumed(consumed, facts->root()), std::runtime_error);
}

TEST(MermaidOfficial, ConfigBodyBatchKeepsExactVisibleNumberSummaryAndClosedLegacyProfile) {
    const auto authority = load_role_config_authority();
    const auto first = authority.root().get("cases").at(0).get("full_native_model_candidate");
    ASSERT_TRUE(first.get("messages").size() > 0);
    const auto bytes = projection_canonical(first);
    for (const auto number : {"", "1", "0.5", "10.11", "1e2"}) {
        SCOPED_TRACE(number);
        const auto changed = projection_mutate(
            bytes, [&](auto root) { root.get("messages").at(0).set("number", number); });
        const auto model = json::parse(changed);
        ASSERT_TRUE(model);
        const auto checks =
            json::parse(role_batch_summary(model->root(), RoleBatchNumbers::DisplayText));
        ASSERT_TRUE(checks);
        const auto label = model->root().get("messages").at(0).get_string("label");
        const auto visible =
            std::string(number).empty() ? label : std::string(number) + ". " + label;
        bool found = false;
        checks->root().get("labels").iter(
            [&](JsonVal text) { found |= text.is_str() && text.as_str() == visible; });
        EXPECT_TRUE(found);
        // Other original numbered messages keep the legacy hidden-only path
        // closed even when this one synthetic field is hidden.
        EXPECT_THROW(role_batch_summary(model->root()), std::runtime_error);
    }
    for (const auto number : {"0", "-1", "nan", "inf", "1e309", "1E3", " 1", "1\n2", "1. x"}) {
        SCOPED_TRACE(number);
        const auto changed = projection_mutate(
            bytes, [&](auto root) { root.get("messages").at(0).set("number", number); });
        const auto model = json::parse(changed);
        ASSERT_TRUE(model);
        EXPECT_THROW(role_batch_summary(model->root(), RoleBatchNumbers::DisplayText),
                     std::runtime_error);
    }
    const auto too_long = projection_mutate(
        bytes, [](auto root) { root.get("messages").at(0).set("number", std::string(257, '1')); });
    const auto long_model = json::parse(too_long);
    ASSERT_TRUE(long_model);
    EXPECT_THROW(role_batch_summary(long_model->root(), RoleBatchNumbers::DisplayText),
                 std::runtime_error);
    const auto wrong_type =
        projection_mutate(bytes, [](auto root) { root.get("messages").at(0).set("number", true); });
    const auto typed = json::parse(wrong_type);
    ASSERT_TRUE(typed);
    EXPECT_THROW(role_batch_summary(typed->root(), RoleBatchNumbers::DisplayText),
                 std::runtime_error);
    EXPECT_THROW(role_batch_summary(first, static_cast<RoleBatchNumbers>(-1)), std::runtime_error);
    EXPECT_EQ(role_batch_case_scope(kRoleBatchPriorCases - 1), kRoleBatchHalfScope);
    EXPECT_EQ(role_batch_case_scope(kRoleBatchPriorCases), kRoleBatchConfigScope);
    EXPECT_THROW(role_batch_case_scope(kRoleBatchCases), std::runtime_error);
    int source_cases = 0, numbered = 0, hidden = 0;
    authority.root().get("cases").iter([&](JsonVal source) {
        ++source_cases;
        const auto model = source.get("full_native_model_candidate");
        EXPECT_NO_THROW(role_batch_summary(model, RoleBatchNumbers::DisplayText));
        model.get("messages").iter([&](JsonVal message) {
            if (message.get_string("number").empty())
                ++hidden;
            else
                ++numbered;
        });
    });
    EXPECT_EQ(source_cases, 49);
    EXPECT_EQ(numbered, 249);
    EXPECT_EQ(hidden, 73);
}

TEST(MermaidOfficial,
     WholeRaw193BindsActualAuthoritiesHistoricalSchemasAndEveryOriginalApplication) {
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest);
    auto raw = load_raw193(manifest->root());
    auto body = load_role_batch(manifest->root());
    std::vector<json::JsonDoc> singleton;
    for (const auto& binding : kProjectionBindings) {
        auto policy = json::parse(read_file(kNativeRoot / binding.policy_file));
        ASSERT_TRUE(policy);
        singleton.push_back(std::move(*policy));
    }
    Raw193Consumption consumed;
    std::size_t wraps = 0, legacy = 0, person = 0, aligned = 0, null_history = 0;
    for (std::size_t n = 0; n < kRaw193Count; ++n) {
        const auto fact = raw.facts.root().get("cases").at(n);
        const auto path = fact.get_string("path");
        const auto selected =
            raw193_select(singleton, body.policy.root(), raw.policy.root(), path, "original");
        ASSERT_TRUE(selected);
        EXPECT_EQ(selected->binding, kRaw193Index);
        EXPECT_EQ(selected->promotion, n);
        raw193_consume(consumed, raw.policy.root(), n);
        EXPECT_EQ(raw193_summary(fact),
                  projection_canonical(
                      raw.policy.root().get("promotions").at(n).get("effective").get("checks")));
        wraps += fact.get_string("profile") == "source_wrap";
        legacy += fact.get_string("number_encoding") == "LegacyInteger";
        person += fact.get("model").get("actors").at(0).has("person");
        aligned += fact.get("source_rect_constraints").size() > 0;
        if (fact.get("historical_original_model_null").as_bool()) {
            ++null_history;
            EXPECT_TRUE(fact.get("source_model_schema_fields").get("number_encoding").is_null());
            EXPECT_EQ(fact.get("active_model_schema").get_string("number_encoding"), "DisplayText");
            EXPECT_EQ(fact.get_string("model_canonical_sha256"),
                      "46ce3bfdedcf3ed5a01d001f9001a92bdba50089517f93ee79412b0794204e14");
        }
        EXPECT_FALSE(
            fs::exists(kRoot / "snapshots" / fact.get("candidate_snapshot").get_string("file")));
        const auto body_selection = raw193_select(singleton, body.policy.root(), raw.policy.root(),
                                                  path, "body_diagnostic");
        if (body_selection) EXPECT_NE(body_selection->binding, kRaw193Index);
    }
    EXPECT_EQ(wraps, 20u);
    EXPECT_EQ(legacy, 23u);
    EXPECT_EQ(person, 24u);
    EXPECT_EQ(aligned, 5u);
    EXPECT_EQ(null_history, 1u);
    EXPECT_NO_THROW(raw193_consumed(consumed));
}

TEST(MermaidOfficial, WholeRaw193ParsedMutationsReachWholeSourceSchemaConstraintAndSnapshotGuards) {
    const auto policy_bytes = read_file(kNativeRoot / kRaw193Binding.policy_file),
               fact_bytes = read_file(kNativeRoot / kRaw193Binding.fact_file);
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest);
    auto actual = load_raw193(manifest->root());
    auto rejects = [&](std::string policy_text, const std::string& fact_text,
                       std::string_view diagnostic, bool bad_source = false,
                       bool bad_snapshot = false, Raw193Kind kind = Raw193Kind::WholeOriginal) {
        policy_text = projection_mutate(policy_text, [&](auto root) {
            root.get("source_facts").set("sha256", digest(fact_text));
        });
        auto policy = json::parse(policy_text), facts = json::parse(fact_text);
        ASSERT_TRUE(policy && facts);
        auto binding = kRaw193Binding;
        binding.kind = kind;
        try {
            validate_raw193(
                policy->root(), facts->root(), manifest->root(), actual.authorities,
                [&](std::string_view path) {
                    auto source = read_file(kRoot / "cases" / path);
                    if (bad_source) source += ' ';
                    return source;
                },
                [&](std::string_view path) {
                    auto snapshot = read_file(kNativeRoot / path);
                    if (bad_snapshot) snapshot += ' ';
                    return snapshot;
                },
                digest(fact_text), binding);
            ADD_FAILURE() << "Raw193 parsed mutation accepted; expected " << diagnostic;
        } catch (const std::runtime_error& error) {
            EXPECT_NE(std::string_view(error.what()).find(diagnostic), std::string_view::npos)
                << error.what();
        }
    };
    auto policy_change = [&](const std::function<void(json::JsonMutVal)>& edit,
                             std::string_view message) {
        rejects(projection_mutate(policy_bytes, edit), fact_bytes, message);
    };
    auto fact_change = [&](std::size_t n, const std::function<void(json::JsonMutVal)>& edit,
                           std::string_view message, bool refresh_model = false) {
        const auto changed =
            projection_mutate(fact_bytes, [&](auto root) { edit(root.get("cases").at(n)); });
        std::string p = policy_bytes, f = changed;
        if (refresh_model) {
            auto parsed = json::parse(changed);
            ASSERT_TRUE(parsed);
            const auto hash =
                digest(projection_canonical(parsed->root().get("cases").at(n).get("model")));
            f = projection_mutate(f, [&](auto root) {
                root.get("cases").at(n).set("model_canonical_sha256", hash);
                root.get("cases").at(n).get("whole_source_authority").set("canonical_sha256", hash);
            });
            p = projection_mutate(
                p, [&](auto root) { root.get("promotions").at(n).set("full_model_sha256", hash); });
        }
        rejects(p, f, message);
    };
    policy_change([](auto r) { r.set("unknown", true); }, "unknown field");
    policy_change([](auto r) { r.set("kind", "body"); }, "policy kind mismatch");
    policy_change([](auto r) { r.set("baseline_manifest_sha256", "wrong"); },
                  "manifest reference mismatch");
    policy_change([](auto r) { r.get("source_facts").set("path", "../escape.json"); },
                  "source fact reference mismatch");
    policy_change([](auto r) { r.get("promotions").at(0).set("track", "body_diagnostic"); },
                  "promotion unknown track");
    policy_change([](auto r) { r.get("promotions").at(0).set("case_index", false); },
                  "promotion case index/type mismatch");
    policy_change([](auto r) { r.get("promotions").at(1).set("case_index", 0); },
                  "duplicate promotion case");
    policy_change([](auto r) { r.get("promotions").at(0).set("body_sha256", "wrong"); },
                  "promotion raw/body mismatch");
    policy_change(
        [](auto r) { r.get("promotions").at(0).get("effective").set("snapshot", "../escape.txt"); },
        "effective snapshot binding mismatch");
    policy_change(
        [](auto r) { r.get("promotions").at(0).get("effective").get("checks").set("arrows", 999); },
        "lightweight summary mismatch");
    policy_change(
        [](auto r) { r.get("promotions").at(0).set("reason", "all upstream predicates passed"); },
        "pending scope mismatch");
    fact_change(0, [](auto r) { r.set("track", "body_diagnostic"); }, "fact unknown track");
    fact_change(0, [](auto r) { r.set("input_utf8_bytes", 1); }, "Raw193 raw byte count");
    fact_change(0, [](auto r) { r.set("body_offset_utf8_bytes", 0); }, "Raw193 body offset");
    fact_change(
        0, [](auto r) { r.get("original_manifest_record").set("git_blob", "wrong"); },
        "complete manifest dictionary mismatch");
    fact_change(
        0, [](auto r) { r.get("source_row_binding").set("case_index", 1); },
        "independent whole source authority mismatch");
    fact_change(
        0,
        [](auto r) { r.get("source_model_schema_fields").set("number_encoding", "LegacyInteger"); },
        "immutable declared schema history mismatch");
    fact_change(
        0, [](auto r) { r.get("active_model_schema").set("number_encoding", "LegacyInteger"); },
        "declared active schema mismatch");
    fact_change(
        0, [](auto r) { r.get("model").get("actors").at(0).set("role", "Unknown"); },
        "Role batch unknown Role", true);
    fact_change(
        0, [](auto r) { r.get("model").get("messages").at(0).set("number", 1); },
        "DisplayText number/type mismatch", true);
    fact_change(
        0,
        [](auto r) { r.get("model").get("messages").at(0).set("label", "Wrong but schema valid"); },
        "complete independent Model mismatch", true);
    fact_change(
        0, [](auto r) { r.get("model").get("messages").at(0).set("number", "2"); },
        "complete independent Model mismatch", true);
    fact_change(
        0, [](auto r) { r.get("candidate_snapshot").set("sha256", "wrong"); },
        "independent review/native262/snapshot proof reference mismatch");
    fact_change(
        0, [](auto r) { r.get("candidate_snapshot").set("file", "../escape.txt"); },
        "independent review/native262/snapshot proof reference mismatch");
    fact_change(
        0, [](auto r) { r.get("whole_source_authority").set("json_pointer", "/cases/1/model"); },
        "independent active Model reference mismatch");
    fact_change(
        0, [](auto r) { r.get("model").set("unknown", true); }, "unknown Model field", true);
    fact_change(
        0, [](auto r) { r.get("native262_all_evidence").at(0).set("geometry_mutations_total", 0); },
        "independent review/native262/snapshot proof reference mismatch");
    rejects(policy_bytes, fact_bytes, "exact whole source binding mismatch", true);
    rejects(policy_bytes, fact_bytes, "snapshot SHA mismatch", false, true);
    rejects(policy_bytes, fact_bytes, "unknown descriptor kind", false, false,
            static_cast<Raw193Kind>(99));
    // par_over is a known native fragment; its nonbranching divider boundary stays closed.
    fact_change(
        34, [](auto r) { r.get("model").get("fragments").at(0).set("kind", "Unknown"); },
        "unknown fragment kind", true);
    fact_change(
        34,
        [](auto r) {
            auto fragment = r.get("model").get("fragments").at(0);
            auto dividers = fragment.make_arr();
            dividers.append(fragment.make_str("unreviewed"));
            fragment.add("dividers", dividers);
        },
        "divider on nonbranching fragment", true);
    std::size_t wrap_count = 0, rect_count = 0;
    bool legacy = false, person = false;
    for (std::size_t n = 0; n < 193; ++n) {
        const auto fact = actual.facts.root().get("cases").at(n);
        if (fact.get_string("profile") == "source_wrap") {
            ++wrap_count;
            fact_change(
                n,
                [](auto r) { r.get("model").get("actors").at(0).get("label").set("wrap", "true"); },
                "SourceWrap Label/type mismatch", true);
            fact_change(
                n,
                [](auto r) {
                    auto label = r.get("model").get("actors").at(0).get("label");
                    label.set("wrap", !label.get("wrap").as_bool());
                },
                "complete independent Model mismatch", true);
        }
        if (fact.get("source_rect_constraints").size() > 0) {
            ++rect_count;
            fact_change(
                n,
                [](auto r) {
                    auto values = r.make_arr();
                    values.append(r.make_str("center"));
                    r.add("source_rect_constraints", values);
                },
                "chosen NoteRect source constraints mismatch");
        }
        if (!legacy && fact.get_string("number_encoding") == "LegacyInteger" &&
            fact.get("model").get("messages").size() > 0) {
            legacy = true;
            fact_change(
                n, [](auto r) { r.get("model").get("messages").at(0).set("number", "0"); },
                "LegacyInteger number/type mismatch", true);
        }
        if (!person && fact.get("model").get("actors").at(0).has("person")) {
            person = true;
            fact_change(
                n, [](auto r) { r.get("model").get("actors").at(0).set("person", 0); },
                "Person boolean/type mismatch", true);
        }
    }
    EXPECT_EQ(wrap_count, 20u);
    EXPECT_EQ(rect_count, 5u);
    EXPECT_TRUE(legacy && person);
}

TEST(MermaidOfficial, WholeRaw193RejectsCrossBatchDuplicatesUnknownTracksAndUnconsumedSources) {
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest);
    auto raw = load_raw193(manifest->root());
    auto body = load_role_batch(manifest->root());
    std::vector<json::JsonDoc> singleton;
    for (const auto& binding : kProjectionBindings) {
        auto p = json::parse(read_file(kNativeRoot / binding.policy_file));
        ASSERT_TRUE(p);
        singleton.push_back(std::move(*p));
    }
    const auto path = raw.facts.root().get("cases").at(0).get_string("path");
    EXPECT_THROW(raw193_select(singleton, body.policy.root(), raw.policy.root(), path, "unknown"),
                 std::runtime_error);
    EXPECT_FALSE(raw193_select(singleton, body.policy.root(), raw.policy.root(),
                               "sequence/missing.mmd", "original"));
    const auto raw_bytes = read_file(kNativeRoot / kRaw193Binding.policy_file);
    const auto duplicated = projection_mutate(
        raw_bytes, [&](auto root) { root.get("promotions").at(1).set("path", path); });
    auto duplicate = json::parse(duplicated);
    ASSERT_TRUE(duplicate);
    EXPECT_THROW(raw193_select(singleton, body.policy.root(), duplicate->root(), path, "original"),
                 std::runtime_error);
    const auto singleton_bytes = read_file(kNativeRoot / kProjectionBindings[2].policy_file);
    auto crossed = json::parse(projection_mutate(
        singleton_bytes, [&](auto root) { root.get("promotions").at(0).set("path", path); }));
    ASSERT_TRUE(crossed);
    singleton[2] = std::move(*crossed);
    EXPECT_THROW(raw193_select(singleton, body.policy.root(), raw.policy.root(), path, "original"),
                 std::runtime_error);
    Raw193Consumption counts;
    EXPECT_THROW(raw193_consumed(counts), std::runtime_error);
    raw193_consume(counts, raw.policy.root(), 0);
    EXPECT_THROW(raw193_consume(counts, raw.policy.root(), 0), std::runtime_error);
    Raw193Consumption complete;
    for (std::size_t n = 0; n < 193; ++n)
        raw193_consume(complete, raw.policy.root(), n);
    EXPECT_NO_THROW(raw193_consumed(complete));
    complete.cases[192] = 0;
    EXPECT_THROW(raw193_consumed(complete), std::runtime_error);
    EXPECT_THROW(raw193_asset("../escape.json", 1, "wrong"), std::runtime_error);
    EXPECT_THROW(raw193_asset(kRaw193Authorities[0].file, 1, kRaw193Authorities[0].sha),
                 std::runtime_error);
}

TEST(MermaidOfficial, WholeBody22BindsEveryActualBodyActiveAuthorityAndNativeSnapshot) {
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest);
    auto raw = load_raw193(manifest->root());
    auto body = load_body22(manifest->root(), raw.authorities);
    auto role = load_role_batch(manifest->root());
    std::vector<json::JsonDoc> singleton;
    for (const auto& b : kProjectionBindings) {
        auto p = json::parse(read_file(kNativeRoot / b.policy_file));
        ASSERT_TRUE(p);
        singleton.push_back(std::move(*p));
    }
    Body22Consumption counts;
    std::size_t wraps = 0, supplements = 0;
    for (std::size_t n = 0; n < 22; ++n) {
        const auto fact = body.facts.root().get("cases").at(n),
                   promotion = body.policy.root().get("promotions").at(n);
        const auto selected =
            body22_select(singleton, role.policy.root(), raw.policy.root(), body.policy.root(),
                          fact.get_string("path"), "body_diagnostic");
        ASSERT_TRUE(selected);
        EXPECT_EQ(selected->binding, kBody22Index);
        EXPECT_EQ(selected->promotion, n);
        body22_consume(counts, body.policy.root(), n);
        const auto model = body22_model(fact, raw.authorities);
        supplements += model.supplemented;
        wraps += fact.get_string("profile") == "source_wrap";
        EXPECT_EQ(
            body22_summary(model.active.get("model"), fact.get_string("profile"), "DisplayText"),
            projection_canonical(promotion.get("effective").get("checks")));
        EXPECT_EQ(digest(projection_canonical(model.active.get("model"))),
                  fact.get_string("model_sha256"));
        EXPECT_TRUE(fact.get("source_note_alignment").is_null());
        EXPECT_EQ(fact.get("source_rect_constraints").size(), 0u);
        const auto old = raw193_select(singleton, role.policy.root(), raw.policy.root(),
                                       fact.get_string("path"), "original");
        ASSERT_TRUE(old);
        EXPECT_EQ(old->binding, kRaw193Index);
        if (model.supplemented) {
            EXPECT_TRUE(model.source_row.get("model").is_null());
            EXPECT_TRUE(model.source_row.get("number_encoding").is_null());
            EXPECT_EQ(model.active.get_string("number_encoding"), "DisplayText");
        }
    }
    EXPECT_EQ(wraps, 17u);
    EXPECT_EQ(supplements, 1u);
    EXPECT_NO_THROW(body22_consumed(counts));
}

TEST(MermaidOfficial, WholeBody22ParsedMutationsReachExactSourceSchemaEvidenceAndSnapshotGuards) {
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest);
    auto raw = load_raw193(manifest->root());
    const auto p = read_file(kNativeRoot / kBody22Binding.policy_file),
               f = read_file(kNativeRoot / kBody22Binding.fact_file);
    auto rejects = [&](const std::string& policy_text, const std::string& fact_text,
                       std::string_view diagnostic, bool bad_source = false,
                       bool bad_snapshot = false, Body22Kind kind = Body22Kind::WholeBody) {
        auto policy = json::parse(policy_text), facts = json::parse(fact_text);
        ASSERT_TRUE(policy && facts);
        auto binding = kBody22Binding;
        binding.kind = kind;
        try {
            validate_body22(
                policy->root(), facts->root(), manifest->root(), raw.authorities,
                [&](std::string_view path) {
                    return read_file(kRoot / "cases" / path) + (bad_source ? "wrong" : "");
                },
                [&](std::string_view path) {
                    return read_file(kNativeRoot / path) + (bad_snapshot ? "wrong" : "");
                },
                kBody22Binding.fact_sha, binding);
            ADD_FAILURE() << "Body22 mutation accepted; expected " << diagnostic;
        } catch (const std::runtime_error& error) {
            EXPECT_NE(std::string_view(error.what()).find(diagnostic), std::string_view::npos)
                << error.what();
        }
    };
    auto policy_change = [&](const std::function<void(json::JsonMutVal)>& edit,
                             std::string_view diagnostic) {
        rejects(projection_mutate(p, edit), f, diagnostic);
    };
    auto fact_change = [&](const std::function<void(json::JsonMutVal)>& edit,
                           std::string_view diagnostic) {
        rejects(p, projection_mutate(f, [&](auto root) { edit(root.get("cases").at(0)); }),
                diagnostic);
    };
    policy_change([](auto r) { r.set("unknown", true); }, "unknown field");
    policy_change([](auto r) { r.set("kind", "Raw193"); }, "policy kind mismatch");
    policy_change([](auto r) { r.set("baseline_manifest_sha256", "wrong"); },
                  "manifest reference mismatch");
    policy_change([](auto r) { r.get("source_facts").set("path", "../escape"); },
                  "source reference mismatch");
    policy_change([](auto r) { r.set("scope", "all foreign predicates passed"); },
                  "policy scope mismatch");
    policy_change([](auto r) { r.get("promotions").at(0).set("case_index", true); },
                  "promotion case index/type mismatch");
    policy_change([](auto r) { r.get("promotions").at(1).set("case_index", 0); },
                  "duplicate promotion case");
    policy_change([](auto r) { r.get("promotions").at(0).set("track", "original"); },
                  "promotion unknown track");
    policy_change([](auto r) { r.get("promotions").at(0).set("body_sha256", "wrong"); },
                  "promotion raw/body mismatch");
    policy_change([](auto r) { r.get("promotions").at(0).set("full_model_sha256", "wrong"); },
                  "promotion Model/baseline mismatch");
    policy_change([](auto r) { r.get("promotions").at(0).set("reason", "all complete"); },
                  "pending scope mismatch");
    policy_change(
        [](auto r) { r.get("promotions").at(0).get("effective").set("snapshot", "../escape"); },
        "unsafe/duplicate snapshot identity");
    policy_change(
        [](auto r) { r.get("promotions").at(0).get("effective").get("checks").set("arrows", 999); },
        "lightweight summary mismatch");
    policy_change(
        [](auto r) { r.get("promotions").at(0).get("native_evidence").set("native_row_index", 1); },
        "native row identity");
    policy_change(
        [](auto r) {
            r.get("promotions").at(0).get("native_evidence").set("source_review_sha256", "wrong");
        },
        "source/native264/snapshot evidence mismatch");
    policy_change(
        [](auto r) {
            auto native = r.get("promotions").at(0).get("native_evidence");
            native.set("narrow_source_fallback", !native.get("narrow_source_fallback").as_bool());
        },
        "narrow lifecycle mismatch");
    fact_change([](auto r) { r.set("unknown", true); }, "unknown field");
    fact_change([](auto r) { r.set("track", "original"); }, "fact unknown track");
    fact_change([](auto r) { r.set("input_utf8_bytes", 1); }, "body byte count");
    fact_change([](auto r) { r.set("body_offset_utf8_bytes", 0); }, "body offset");
    fact_change([](auto r) { r.set("input", "wrong"); }, "complete body source binding mismatch");
    fact_change([](auto r) { r.set("baseline_expectation_canonical_sha256", "wrong"); },
                "baseline diagnostic mismatch");
    fact_change([](auto r) { r.set("number_encoding", "LegacyInteger"); },
                "profile/number mismatch");
    fact_change([](auto r) { r.set("model_schema", "unknown"); }, "declared Model schema mismatch");
    fact_change([](auto r) { r.set("source_note_alignment", "left"); },
                "default chosen NoteRect constraints mismatch");
    fact_change([](auto r) { r.get("model_authority").set("authority_file", "wrong"); },
                "authority file/SHA mismatch");
    fact_change([](auto r) { r.get("model_authority").set("json_pointer", "/cases/9/model"); },
                "independent whole raw authority mismatch");
    fact_change([](auto r) { r.set("model_sha256", "wrong"); },
                "complete independent Model mismatch");
    // Actual duplicate member bytes: JsonMutVal.set would replace the key.
    rejects(p, "{\"schema\":0," + f.substr(1), "duplicate object key");
    rejects(p, f, "exact whole raw binding mismatch", true);
    rejects(p, f, "snapshot SHA/byte mismatch", false, true);
    rejects(p, f, "unknown descriptor kind", false, false, static_cast<Body22Kind>(99));
    auto docs = load_body22(manifest->root(), raw.authorities);
    const auto fact = docs.facts.root().get("cases").at(0);
    const auto model = body22_model(fact, raw.authorities).active.get("model");
    const auto model_text = projection_canonical(model);
    auto invalid_model = [&](const std::function<void(json::JsonMutVal)>& edit) {
        auto changed = json::parse(projection_mutate(model_text, edit));
        ASSERT_TRUE(changed);
        EXPECT_THROW(body22_summary(changed->root(), "source_wrap", "DisplayText"),
                     std::runtime_error);
    };
    invalid_model([](auto r) { r.get("actors").at(0).set("role", "Unknown"); });
    invalid_model([](auto r) { r.get("actors").at(0).get("label").set("wrap", 1); });
    invalid_model([](auto r) { r.get("messages").at(0).set("arrow", "Unknown"); });
    invalid_model([](auto r) { r.get("messages").at(0).set("number", 1); });
    for (const auto number : {"-1", "nan", "0", "1e999"})
        invalid_model([&](auto r) { r.get("messages").at(0).set("number", number); });
}

TEST(MermaidOfficial, WholeBody22SelectorAndConsumptionPreserveEveryLegacyDescriptorCoordinate) {
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest);
    auto raw = load_raw193(manifest->root());
    auto role = load_role_batch(manifest->root());
    auto body = load_body22(manifest->root(), raw.authorities);
    std::vector<json::JsonDoc> singleton;
    for (const auto& b : kProjectionBindings) {
        auto p = json::parse(read_file(kNativeRoot / b.policy_file));
        ASSERT_TRUE(p);
        singleton.push_back(std::move(*p));
    }
    const auto path = body.facts.root().get("cases").at(0).get_string("path");
    const auto p = read_file(kNativeRoot / kBody22Binding.policy_file);
    EXPECT_THROW(body22_select(singleton, role.policy.root(), raw.policy.root(), body.policy.root(),
                               path, "unknown"),
                 std::runtime_error);
    EXPECT_THROW(body22_select(singleton, role.policy.root(), raw.policy.root(), body.policy.root(),
                               path, "original", static_cast<Body22Kind>(99)),
                 std::runtime_error);
    EXPECT_FALSE(body22_select(singleton, role.policy.root(), raw.policy.root(), body.policy.root(),
                               "sequence/missing.mmd", "body_diagnostic"));
    auto duplicate = json::parse(
        projection_mutate(p, [&](auto r) { r.get("promotions").at(1).set("path", path); }));
    ASSERT_TRUE(duplicate);
    EXPECT_THROW(body22_select(singleton, role.policy.root(), raw.policy.root(), duplicate->root(),
                               path, "body_diagnostic"),
                 std::runtime_error);
    auto wrong_track = json::parse(
        projection_mutate(p, [](auto r) { r.get("promotions").at(0).set("track", "original"); }));
    ASSERT_TRUE(wrong_track);
    EXPECT_THROW(body22_select(singleton, role.policy.root(), raw.policy.root(),
                               wrong_track->root(), path, "original"),
                 std::runtime_error);
    auto cross = json::parse(
        projection_mutate(read_file(kNativeRoot / kRoleBatchBinding.policy_file),
                          [&](auto r) { r.get("promotions").at(0).set("path", path); }));
    ASSERT_TRUE(cross);
    EXPECT_THROW(body22_select(singleton, cross->root(), raw.policy.root(), body.policy.root(),
                               path, "body_diagnostic"),
                 std::runtime_error);
    Body22Consumption counts;
    EXPECT_THROW(body22_consumed(counts), std::runtime_error);
    body22_consume(counts, body.policy.root(), 0);
    EXPECT_THROW(body22_consume(counts, body.policy.root(), 0), std::runtime_error);
    EXPECT_THROW(body22_consume(counts, body.policy.root(), 22), std::runtime_error);
    Body22Consumption complete;
    for (std::size_t n = 0; n < 22; ++n)
        body22_consume(complete, body.policy.root(), n);
    EXPECT_NO_THROW(body22_consumed(complete));
    complete.cases[21] = 0;
    EXPECT_THROW(body22_consumed(complete), std::runtime_error);
}

namespace {
constexpr std::string_view kStageBSources = R"STAGEBSOURCE({
  "qualification": "All inputs and complete Models are authored from current owning syntax, not runtime observations/reports. Future actual success is unknown; Model/cell bytes are not generated from drawings. No official285/policy/foreign assertion admission.",
  "schema": "stageB_complete_source_protocol_controls/proposed-v1",
  "sources": [
    {
      "Model_canonical_sha256": "a338bbcdff7b74add00d3630e38e41ffcd7ed74e27f60d567136cecdaf1b089f",
      "complete_Class_Model": {
        "direction": "TB",
        "entities": [
          {
            "headers": [
              [
                "X"
              ]
            ],
            "id": "X",
            "label": "X",
            "owner": "",
            "rows": [],
            "shape": "box"
          }
        ],
        "family": "class",
        "groups": [],
        "notes": [],
        "relations": []
      },
      "complete_source": "---\ntitle: AFTER_RELATIONS_SEMANTICS\n---\nclassDiagram\nclass X\n",
      "expected": "complete_geometry_and_distinct_public_footer",
      "following_token": "AFTER_RELATIONS_SEMANTICS",
      "independent_source_obligations": [
        "Title is source-owned row0; it must be claimed normally, never globally suppressed.",
        "A separate appended footer has the same spelling and must be the only exempt semantic role.",
        "Removing only the real footer preserves bare geometry when otherwise complete, but cannot certify required public following."
      ],
      "name": "complete_Class_title_equals_old_default",
      "ordinal": 0,
      "presentation": {
        "hide_empty_members_box": null,
        "title": "AFTER_RELATIONS_SEMANTICS"
      },
      "source_sha256": "be843152bab3c7025609ed8f6e1a932ddaeea8077c8414a7ce8e618851e8bb12",
      "source_utf8_bytes": 62
    },
    {
      "Model_canonical_sha256": "a338bbcdff7b74add00d3630e38e41ffcd7ed74e27f60d567136cecdaf1b089f",
      "complete_Class_Model": {
        "direction": "TB",
        "entities": [
          {
            "headers": [
              [
                "X"
              ]
            ],
            "id": "X",
            "label": "X",
            "owner": "",
            "rows": [],
            "shape": "box"
          }
        ],
        "family": "class",
        "groups": [],
        "notes": [],
        "relations": []
      },
      "complete_source": "---\ntitle: AFTER_MERMAID\n---\nclassDiagram\nclass X\n",
      "expected": "complete_geometry_and_distinct_public_footer",
      "following_token": "AFTER_MERMAID",
      "independent_source_obligations": [
        "Title is source-owned row0; it must be claimed normally, never globally suppressed.",
        "A separate appended footer has the same spelling and must be the only exempt semantic role.",
        "Removing only the real footer preserves bare geometry when otherwise complete, but cannot certify required public following."
      ],
      "name": "complete_Class_title_equals_official_token",
      "ordinal": 1,
      "presentation": {
        "hide_empty_members_box": null,
        "title": "AFTER_MERMAID"
      },
      "source_sha256": "3d5fa9cdcb3be7bf9705eaaaf189828d1ed57effcd87bbfd70cc5c8e6dcb2803",
      "source_utf8_bytes": 50
    },
    {
      "Model_canonical_sha256": "e9dd2d4aea89eb9cc57d9f9674e188a25b531d50744dbb882ee90b956a840104",
      "complete_Class_Model": {
        "direction": "TB",
        "entities": [
          {
            "headers": [
              [
                "AFTER_MERMAID"
              ]
            ],
            "id": "A",
            "label": "AFTER_MERMAID",
            "owner": "",
            "rows": [],
            "shape": "box"
          },
          {
            "headers": [
              [
                "AFTER_RELATIONS_SEMANTICS"
              ]
            ],
            "id": "B",
            "label": "AFTER_RELATIONS_SEMANTICS",
            "owner": "",
            "rows": [],
            "shape": "box"
          }
        ],
        "family": "class",
        "groups": [],
        "notes": [],
        "relations": [
          {
            "end": "arrow",
            "label": "AFTER_MERMAID",
            "order": 0,
            "source": "A",
            "source_text": null,
            "start": "none",
            "stroke": "solid",
            "target": "B",
            "target_text": null
          }
        ]
      },
      "complete_source": "classDiagram\nclass A[\"AFTER_MERMAID\"]\nclass B[\"AFTER_RELATIONS_SEMANTICS\"]\nA --> B : AFTER_MERMAID\n",
      "expected": "complete_geometry_and_distinct_public_footer",
      "following_token": "AFTER_MERMAID",
      "independent_source_obligations": [
        "Both complete entity labels and the business relation label remain mandatory.",
        "Changing/removing a business occurrence cannot be hidden by the following token context.",
        "Positive is a source-authored complete Model, not a captured glyph expectation."
      ],
      "name": "complete_Class_labels_and_caption_contain_both_tokens",
      "ordinal": 2,
      "presentation": {
        "hide_empty_members_box": null,
        "title": null
      },
      "source_sha256": "eab703ff35486e27527dd1f7755f233b04f58cdc8497a7b329d8289e76329ad3",
      "source_utf8_bytes": 99
    },
    {
      "Model_canonical_sha256": "8559689bcde0c86f6e50fad3929ef13a5237594b2e6b5ce63fdc2c55cfe49bfe",
      "complete_Class_Model": {
        "direction": "TB",
        "entities": [
          {
            "headers": [
              [
                "界"
              ]
            ],
            "id": "A",
            "label": "界",
            "owner": "",
            "rows": [],
            "shape": "box"
          },
          {
            "headers": [
              [
                "é"
              ]
            ],
            "id": "B",
            "label": "é",
            "owner": "",
            "rows": [],
            "shape": "box"
          }
        ],
        "family": "class",
        "groups": [],
        "notes": [],
        "relations": [
          {
            "end": "arrow",
            "label": "",
            "order": 0,
            "source": "A",
            "source_text": null,
            "start": "none",
            "stroke": "solid",
            "target": "B",
            "target_text": null
          }
        ]
      },
      "complete_source": "classDiagram\nclass A[\"界\"]\nclass B[\"é\"]\nA --> B\n",
      "expected": "complete_geometry_and_distinct_public_footer",
      "following_token": "AFTER_MERMAID",
      "independent_source_obligations": [
        "Preserve actual CJK empty continuation strings and combining grapheme strings through Frame-to-Grid adaptation.",
        "A wrong continuation cell must reach and fail the complete physical predicate, never be repaired by the adapter."
      ],
      "name": "complete_Class_CJK_and_combining_cells",
      "ordinal": 3,
      "presentation": {
        "hide_empty_members_box": null,
        "title": null
      },
      "source_sha256": "8a3b3c5165a2a7d191f69ce3204cf96dfaa83d0d8f2c4ff90357e67f894fec88",
      "source_utf8_bytes": 51
    },
    {
      "Model_canonical_sha256": null,
      "complete_Class_Model": null,
      "complete_source": "unknownDiagram\nAFTER_MERMAID\nAFTER_RELATIONS_SEMANTICS\n",
      "expected": "whole_source_finite_protocol_control_only",
      "following_token": "AFTER_MERMAID",
      "independent_source_obligations": [
        "Unknown diagram header remains a finite whole-source control, not claimed upstream-invalid.",
        "All five actual frames are captured before source binding; wide/narrow/restored must equal complete ordinary-code references.",
        "At40/240 columns each literal source row fits. Independently require ordered exact nonSPACE physical rows unknownDiagram,AFTER_MERMAID,AFTER_RELATIONS_SEMANTICS,AFTER_MERMAID; every row is consecutive single ASCII cells and surrounding real SPACE.",
        "This finite source-owned sequence proves two body roles plus distinct final appended footer for this case only; it is not a new general plain-text parser or a generic Source/footer certificate."
      ],
      "name": "whole_Source_contains_both_footer_spellings",
      "ordinal": 4,
      "presentation": null,
      "source_sha256": "15b6f151a53026c338542d01ff698021499e79e4d8045a1e5cc3042827e59a09",
      "source_utf8_bytes": 55
    }
  ],
  "status": "PENDING_INDEPENDENT_DESIGN_REVIEW_NO_NATIVE_OR_CPP"
}
)STAGEBSOURCE";
constexpr std::string_view kStageBSourcesSha =
    "41939622e55c625190b798f06d724a87fa9162b6fde82175a5dc3c428254e69b";

struct StageBFrames {
    Frame wide, ordinary, narrow, small, restored;
    int height = 0;
};

StageBFrames stage_b_observe(std::string_view source, std::string_view token,
                             relations_oracle::Work& work) {
    work.spend(sizeof(StageBFrames) + 16u * sizeof(ftxui::Element) + 8u * sizeof(std::size_t) +
               256u);
    relations_oracle::class481_require(!source.empty() && source.size() <= 65536u &&
                                           token.size() > 0u && token.size() <= 240u,
                                       "StageB complete source/token bounds");
    for (const unsigned char c : token) {
        work.spend(4u);
        relations_oracle::class481_require(c >= 0x21 && c <= 0x7e, "StageB token ASCII");
    }
    const auto bound = 3u * source.size() + 2u * token.size() + 64u;
    stage_b_product(work, bound,
                    2u * (32u + 4u * sizeof(std::string) + 4u * sizeof(std::string_view)));
    work.spend(256u);
    const auto node = loom::ui::render_markdown(fenced(source, "mermaid", token));
    const auto ordinary = loom::ui::render_markdown(fenced(source, "text", token));
    node->ComputeRequirement();
    ordinary->ComputeRequirement();
    const int height = std::max({1, node->requirement().min_y, ordinary->requirement().min_y});
    relations_oracle::class481_require(height <= 1040,
                                       "StageB sufficient capture height exceeds bound");
    // Both requirements include the following paragraph. No guessed margin,
    // largest-screen credit or second source ledger is used.
    return {capture(node, 240, height, &work), capture(ordinary, 240, height, &work),
            capture(node, 40, height, &work),  capture(ordinary, 40, height, &work),
            capture(node, 240, height, &work), height};
}

bool stage_b_same_cells(const Frame& first, const Frame& second, relations_oracle::Work& work) {
    work.spend(4u * sizeof(std::size_t) + 128u);
    for (const auto* frame : {&first, &second}) {
        for (const auto& row : frame->cells) {
            work.spend(4u);
            for (const auto& cell : row) {
                stage_b_product(work, cell.size(), 2u);
                work.spend(4u);
            }
        }
    }
    return first.cells == second.cells;
}

struct StageBBound {
    relations_oracle::Model model;
    relations_oracle::ClassPresentation presentation;
    std::size_t model_bytes = 0;
};

std::optional<StageBBound> stage_b_bind(JsonVal row, std::size_t ordinal,
                                        std::string_view actual_source,
                                        relations_oracle::Work& work) {
    using relations_oracle::class481_keys;
    using relations_oracle::class481_text;
    work.spend(4u * sizeof(StageBBound) + 512u);
    class481_keys(row,
                  {"Model_canonical_sha256", "complete_Class_Model", "complete_source", "expected",
                   "following_token", "independent_source_obligations", "name", "ordinal",
                   "presentation", "source_sha256", "source_utf8_bytes"},
                  work);
    relations_oracle::class481_require(ordinal < 5u && row.get("ordinal").is_int() &&
                                           row.get_int("ordinal") ==
                                               static_cast<std::int64_t>(ordinal),
                                       "StageB exact original control order");
    constexpr std::array<std::string_view, 5> sources{
        {"be843152bab3c7025609ed8f6e1a932ddaeea8077c8414a7ce8e618851e8bb12",
         "3d5fa9cdcb3be7bf9705eaaaf189828d1ed57effcd87bbfd70cc5c8e6dcb2803",
         "eab703ff35486e27527dd1f7755f233b04f58cdc8497a7b329d8289e76329ad3",
         "8a3b3c5165a2a7d191f69ce3204cf96dfaa83d0d8f2c4ff90357e67f894fec88",
         "15b6f151a53026c338542d01ff698021499e79e4d8045a1e5cc3042827e59a09"}};
    constexpr std::array<std::string_view, 4> meanings{
        {"a338bbcdff7b74add00d3630e38e41ffcd7ed74e27f60d567136cecdaf1b089f",
         "a338bbcdff7b74add00d3630e38e41ffcd7ed74e27f60d567136cecdaf1b089f",
         "e9dd2d4aea89eb9cc57d9f9674e188a25b531d50744dbb882ee90b956a840104",
         "8559689bcde0c86f6e50fad3929ef13a5237594b2e6b5ce63fdc2c55cfe49bfe"}};
    const auto source = class481_text(row.get("complete_source"), work, 65536u);
    relations_oracle::class481_require(
        source == actual_source && row.get("source_utf8_bytes").is_int() &&
            row.get_int("source_utf8_bytes") == static_cast<std::int64_t>(source.size()),
        "StageB full source bytes/length");
    stage_b_product(work, source.size(), 2u);
    work.spend(256u);
    relations_oracle::class481_require(digest(source) == sources[ordinal] &&
                                           class481_text(row.get("source_sha256"), work, 64u) ==
                                               sources[ordinal],
                                       "StageB exact complete source SHA");
    relations_oracle::class481_require(!class481_text(row.get("name"), work, 128u).empty(),
                                       "StageB source name");
    constexpr std::array<std::size_t, 5> obligation_counts{{3u, 3u, 3u, 2u, 4u}};
    relations_oracle::class481_require(row.get("independent_source_obligations").is_arr() &&
                                           row.get("independent_source_obligations").size() ==
                                               obligation_counts[ordinal],
                                       "StageB full source obligations");
    row.get("independent_source_obligations").iter([&](JsonVal item) {
        relations_oracle::class481_require(!class481_text(item, work, 1024u).empty(),
                                           "StageB obligation text");
    });
    const auto token = class481_text(row.get("following_token"), work, 240u);
    relations_oracle::class481_require(
        token == (ordinal == 0u ? relations_oracle::kRelationsFollowingToken : kSentinel),
        "StageB explicit finite token authority");
    if (ordinal == 4u) {
        relations_oracle::class481_require(row.get("complete_Class_Model").is_null() &&
                                               row.get("Model_canonical_sha256").is_null() &&
                                               row.get("presentation").is_null() &&
                                               class481_text(row.get("expected"), work) ==
                                                   "whole_source_finite_protocol_control_only",
                                           "StageB Source keeps Model/profile unknown");
        return std::nullopt;
    }
    relations_oracle::class481_require(class481_text(row.get("expected"), work) ==
                                           "complete_geometry_and_distinct_public_footer",
                                       "StageB mandatory full positive");
    const auto meaning = row.get("complete_Class_Model");
    const auto canonical = relations_oracle::class481_canonical(meaning, work);
    stage_b_product(work, canonical.size(), 2u);
    work.spend(256u);
    relations_oracle::class481_require(
        digest(canonical) == meanings[ordinal] &&
            class481_text(row.get("Model_canonical_sha256"), work, 64u) == meanings[ordinal],
        "StageB complete independent Model hash");
    work.spend(sizeof(std::pair<JsonVal, std::size_t>) + 64u);
    const std::pair<JsonVal, std::size_t> size_proof{meaning, canonical.size()};
    auto model = relations_oracle::class481_model(meaning, work, true, &size_proof);
    const auto presentation = row.get("presentation");
    class481_keys(presentation, {"hide_empty_members_box", "title"}, work);
    relations_oracle::class481_require(presentation.get("hide_empty_members_box").is_null(),
                                       "StageB old compact default remains unmodified");
    relations_oracle::ClassPresentation result;
    if (ordinal < 2u) {
        const auto title = class481_text(presentation.get("title"), work, 240u);
        relations_oracle::class481_require(title == token,
                                           "StageB complete authored title binding");
        result.title = title;
    } else
        relations_oracle::class481_require(presentation.get("title").is_null(),
                                           "StageB absent title");
    return StageBBound{std::move(model), result, canonical.size()};
}

void stage_b_finite_source_rows(const Frame& frame, relations_oracle::Work& work) {
    constexpr std::array<std::string_view, 4> expected{
        {"unknownDiagram", "AFTER_MERMAID", "AFTER_RELATIONS_SEMANTICS", "AFTER_MERMAID"}};
    work.spend(8u * sizeof(std::size_t) + 128u);
    std::size_t found = 0;
    for (const auto& row : frame.cells) {
        work.spend(4u);
        std::size_t first = 0;
        while (first < row.size()) {
            work.spend(4u);
            if (row[first] != " ") break;
            ++first;
        }
        if (first == row.size()) continue;
        relations_oracle::class481_require(found < expected.size(),
                                           "StageB Source extra physical row");
        const auto want = expected[found++];
        relations_oracle::class481_require(want.size() <= row.size() - first,
                                           "StageB Source clipped row");
        for (std::size_t n = 0; n < want.size(); ++n) {
            work.spend(4u);
            relations_oracle::class481_require(row[first + n].size() == 1u &&
                                                   row[first + n][0] == want[n],
                                               "StageB Source exact single-ASCII row");
        }
        for (std::size_t x = first + want.size(); x < row.size(); ++x) {
            work.spend(4u);
            relations_oracle::class481_require(row[x] == " ",
                                               "StageB Source row suffix must be SPACE");
        }
    }
    relations_oracle::class481_require(found == expected.size(),
                                       "StageB complete source rows and following content");
}

TEST(MermaidOfficialStageB, FullPhysicalFrameAdapterPreservesCellsAndBounds) {
    try {
        relations_oracle::Work work;
        work.spend(65536u);
        RecordProperty("stageB_sizeof_Pixel", std::to_string(sizeof(ftxui::Pixel)));
        RecordProperty("stageB_sizeof_PixelRow", std::to_string(sizeof(std::vector<ftxui::Pixel>)));
        RecordProperty("stageB_sizeof_string", std::to_string(sizeof(std::string)));
        Frame frame;
        frame.cells = {{"界", "", " "}, {"é", "x", " "}};
        frame.text = "DO_NOT_USE_NORMALIZED_TEXT";
        frame.content_width = 999;
        const auto exact = frame_grid(frame, 3, 2, work);
        const std::vector<std::string> wanted{"界", "", " ", "é", "x", " "};
        EXPECT_EQ(exact.width, 3);
        EXPECT_EQ(exact.height, 2);
        EXPECT_EQ(exact.cells, wanted);
        Frame packed;
        packed.cells = {{"AFTER", "_MERMAID", ""}};
        const auto raw = frame_grid(packed, 3, 1, work);
        EXPECT_EQ(raw.cells, packed.cells[0]);
        EXPECT_FALSE(relations_oracle::following_row(raw, work, kSentinel).has_value());
        // The exact adapter must also preserve malformed physical input.
        work.spend(4u * sizeof(Frame) + 4u * sizeof(relations_oracle::Rect) +
                   60u * (3u * sizeof(std::string) + 32u) + 4096u);
        Frame header;
        header.cells = {
            {"┌", "─", "─", "─", "┐"}, {"│", "界", "", " ", "│"}, {"└", "─", "─", "─", "┘"}};
        const relations_oracle::Rect box{0, 0, 4, 2, "box"};
        const auto good_header = frame_grid(header, 5, 3, work);
        ASSERT_TRUE(relations_oracle::class_centered_header_row(good_header, box, 1, "界", work));
        auto malformed = header;
        malformed.cells[1][2] = " ";
        const auto bad_header = frame_grid(malformed, 5, 3, work);
        EXPECT_EQ(bad_header.at({2, 1}), " ");
        EXPECT_FALSE(relations_oracle::class_centered_header_row(bad_header, box, 1, "界", work));
        Frame ragged;
        ragged.cells = {{"界", "", " "}, {"x", " "}};
        EXPECT_THROW(static_cast<void>(frame_grid(ragged, 3, 2, work)), std::runtime_error);
        EXPECT_THROW(static_cast<void>(frame_grid(frame, 2, 2, work)), std::runtime_error);
        EXPECT_THROW(static_cast<void>(frame_grid(frame, 3, 1, work)), std::runtime_error);
        EXPECT_THROW(static_cast<void>(frame_grid(frame, 0, 2, work)), std::runtime_error);
        EXPECT_THROW(static_cast<void>(frame_grid(frame, 241, 2, work)), std::runtime_error);
        EXPECT_GT(work.left, 0u) << "Shape errors must not be resource exhaustion";
        relations_oracle::Work zero;
        zero.left = 0;
        EXPECT_THROW(static_cast<void>(frame_grid(frame, 3, 2, zero)), std::runtime_error);
    } catch (const std::exception& error) {
        ADD_FAILURE() << error.what();
    }
}

TEST(MermaidOfficialStageB, PaidFiveCaptureProtocolBindsCompleteSourcesAfterObservation) {
    int complete = 0, finite_source = 0;
    for (std::size_t ordinal = 0; ordinal < 5u; ++ordinal) {
        try {
            relations_oracle::Work work;
            work.spend(2048u);
            SCOPED_TRACE(ordinal);
            work.spend(kStageBSources.size() * 3u + 4096u);
            const auto parsed = json::parse(kStageBSources);
            ASSERT_TRUE(parsed.has_value());
            const auto catalog = parsed->root();
            const auto row = catalog.get("sources").at(ordinal);
            // Input acquisition only. Source/hash/schema/Model binding is below
            // all five public captures and shares this exact same source Work.
            const auto input =
                relations_oracle::class481_text(row.get("complete_source"), work, 65536u);
            const auto token =
                relations_oracle::class481_text(row.get("following_token"), work, 240u);
            const auto frames = stage_b_observe(input, token, work);
            const auto after_capture = work.left;
            relations_oracle::class481_keys(catalog,
                                            {"qualification", "schema", "sources", "status"}, work);
            relations_oracle::class481_require(
                catalog.get("sources").is_arr() && catalog.get("sources").size() == 5u &&
                    catalog.get("schema").as_str() ==
                        "stageB_complete_source_protocol_controls/proposed-v1",
                "StageB exact control document");
            relations_oracle::class481_require(
                !relations_oracle::class481_text(catalog.get("qualification"), work, 2048u).empty(),
                "StageB qualification text");
            relations_oracle::class481_require(
                relations_oracle::class481_text(catalog.get("status"), work, 128u) ==
                    "PENDING_INDEPENDENT_DESIGN_REVIEW_NO_NATIVE_OR_CPP",
                "StageB immutable historical proposal lineage");
            stage_b_product(work, kStageBSources.size(), 2u);
            work.spend(256u);
            relations_oracle::class481_require(digest(kStageBSources) == kStageBSourcesSha,
                                               "StageB immutable full control document");
            const auto bound = stage_b_bind(row, ordinal, input, work);
            const auto after_bind = work.left;
            ASSERT_TRUE(stage_b_same_cells(frames.wide, frames.restored, work));
            if (!bound) {
                ASSERT_EQ(ordinal, 4u);
                EXPECT_TRUE(stage_b_same_cells(frames.wide, frames.ordinary, work));
                EXPECT_TRUE(stage_b_same_cells(frames.narrow, frames.small, work));
                for (const auto* frame : {&frames.wide, &frames.ordinary, &frames.narrow,
                                          &frames.small, &frames.restored})
                    stage_b_finite_source_rows(*frame, work);
                ++finite_source;
            } else {
                ASSERT_LT(ordinal, 4u);
                ASSERT_FALSE(stage_b_same_cells(frames.wide, frames.ordinary, work))
                    << "Complete Class source still fell back";
                auto wide = frame_grid(frames.wide, 240, frames.height, work);
                work.spend(4u * sizeof(std::optional<int>) +
                           4u * sizeof(relations_oracle::ClassCardWitness) + 256u);
                std::optional<int> following;
                relations_oracle::ClassCardWitness card;
                const auto good = relations_oracle::validate_impl(wide, bound->model, work, false,
                                                                  &bound->presentation, false,
                                                                  &card, token, &following);
                ASSERT_TRUE(good.empty()) << relations_oracle::explain(good);
                ASSERT_TRUE(following.has_value())
                    << "Spelling alone is not a following-role certificate";
                {
                    auto restored = frame_grid(frames.restored, 240, frames.height, work);
                    work.spend(4u * sizeof(std::optional<int>));
                    std::optional<int> current;
                    const auto errors = relations_oracle::validate_impl(
                        restored, bound->model, work, false, &bound->presentation, false, nullptr,
                        token, &current);
                    ASSERT_TRUE(errors.empty()) << relations_oracle::explain(errors);
                    ASSERT_TRUE(current.has_value());
                }
                if (!stage_b_same_cells(frames.narrow, frames.small, work)) {
                    auto narrow = frame_grid(frames.narrow, 40, frames.height, work);
                    work.spend(4u * sizeof(std::optional<int>));
                    std::optional<int> current;
                    const auto errors = relations_oracle::validate_impl(
                        narrow, bound->model, work, false, &bound->presentation, false, nullptr,
                        token, &current);
                    ASSERT_TRUE(errors.empty()) << relations_oracle::explain(errors);
                    ASSERT_TRUE(current.has_value());
                }
                if (bound->presentation.title) {
                    // Removing only the real footer leaves the source-owned
                    // same-spelling title intact and cannot certify following.
                    stage_b_product(work, wide.cells.size(), 3u * sizeof(std::string) + 8u);
                    work.spend(4u * sizeof(relations_oracle::Grid) +
                               4u * sizeof(std::optional<int>) + 128u);
                    for (const auto& cell : wide.cells) {
                        stage_b_product(work, cell.size(), 2u);
                        work.spend(4u);
                    }
                    auto nofooter = wide;
                    stage_b_product(work, static_cast<std::size_t>(wide.width),
                                    sizeof(std::string) + 16u);
                    for (int x = 0; x < wide.width; ++x)
                        nofooter.put({x, *following}, " ");
                    std::optional<int> absent{*following};
                    const auto mock = relations_oracle::validate_impl(
                        nofooter, bound->model, work, false, &bound->presentation, false, nullptr,
                        token, &absent);
                    ASSERT_TRUE(mock.empty()) << relations_oracle::explain(mock);
                    EXPECT_FALSE(absent.has_value());
                }
                // Good whole geometry and distinct following role precede all
                // schema-valid semantic and physical corruption checks.
                stage_b_product(work, bound->model_bytes + 256u, 8u * sizeof(std::string) + 64u);
                work.spend(256u);
                auto wrong = bound->model;
                wrong.entities[0].label = "FOREIGN";
                wrong.entities[0].headers[0][0] = "FOREIGN";
                const auto wrong_errors = relations_oracle::validate_impl(
                    wide, wrong, work, false, &bound->presentation, false, nullptr, token);
                relations_oracle::class481_no_resource(wrong_errors);
                EXPECT_FALSE(wrong_errors.empty())
                    << "Footer collision cannot hide changed complete business header";
                if (bound->presentation.title) {
                    work.spend(4u * sizeof(relations_oracle::ClassPresentation) + 128u);
                    auto wrong_title = bound->presentation;
                    wrong_title.title = "WRONG_TITLE";
                    const auto errors = relations_oracle::validate_impl(
                        wide, bound->model, work, false, &wrong_title, false, nullptr, token);
                    relations_oracle::class481_no_resource(errors);
                    EXPECT_FALSE(errors.empty());
                }
                if (ordinal == 3u) {
                    ASSERT_TRUE(card.box.has_value());
                    work.spend(4u * sizeof(int) + 128u);
                    const int x = card.box->l + 1 + (card.box->r - card.box->l - 1 - 2) / 2,
                              y = card.box->t + 1;
                    ASSERT_EQ(wide.at({x, y}), "界");
                    ASSERT_EQ(wide.at({x + 1, y}), "");
                    ASSERT_TRUE(relations_oracle::class_centered_header_row(wide, *card.box, y,
                                                                            "界", work));
                    stage_b_product(work, wide.cells.size(), 3u * sizeof(std::string) + 8u);
                    for (const auto& cell : wide.cells) {
                        stage_b_product(work, cell.size(), 2u);
                        work.spend();
                    }
                    auto changed = wide;
                    work.spend(64u);
                    changed.put({x + 1, y}, " ");
                    // A physical protocol negative of the existing exact row
                    // predicate, not fabricated generic-geometry rejection.
                    EXPECT_FALSE(relations_oracle::class_centered_header_row(changed, *card.box, y,
                                                                             "界", work));
                }
                ++complete;
            }
            work.spend(2048u);
            RecordProperty("stageB_source_" + std::to_string(ordinal) + "_height",
                           std::to_string(frames.height));
            RecordProperty("stageB_source_" + std::to_string(ordinal) + "_after_capture_remaining",
                           std::to_string(after_capture));
            RecordProperty("stageB_source_" + std::to_string(ordinal) + "_after_bind_remaining",
                           std::to_string(after_bind));
            RecordProperty("stageB_source_" + std::to_string(ordinal) + "_remaining",
                           std::to_string(work.left));
        } catch (const std::exception& error) {
            ADD_FAILURE() << error.what();
        }
    }
    EXPECT_EQ(complete, 4);
    EXPECT_EQ(finite_source, 1);
}
}  // namespace

namespace {
// Official owns only observation, binding and reporting. Geometry stays shared.
void stage_c_error(Observation& row, std::string_view message) {
    // Entry reserve pays at most six128-byte errors and their publication copies.
    if (row.errors.size() < 6u) row.errors.emplace_back(message.substr(0, 128u));
}

void stage_c_geometry(const StageBFrames& frames, const relations_oracle::Model& model,
                      const relations_oracle::ClassPresentation& presentation,
                      relations_oracle::Work& work, Observation& row, StageCProgress& progress,
                      std::string_view token) {
    using namespace relations_oracle;
    progress.phase = StageCPhase::Compare;
    progress.restored_equal = stage_b_same_cells(frames.wide, frames.restored, work);
    const bool wide_source = stage_b_same_cells(frames.wide, frames.ordinary, work);
    progress.wide_source = wide_source;
    const bool narrow_source = stage_b_same_cells(frames.narrow, frames.small, work);
    progress.remaining[4] = work.left;
    auto drawn = [&](const Frame& frame, int width, StageCPhase phase, std::size_t at) {
        progress.phase = phase;
        auto grid = frame_grid(frame, width, frames.height, work);
        work.spend(4u * sizeof(std::optional<int>) + 128u);
        std::optional<int> following;
        const auto errors = validate_impl(grid, model, work, false, &presentation, false, nullptr,
                                          token, &following);
        // Preserve resource classification from the original diagnostic before
        // no_resource wraps it; its exact bounds diagnostic is not a substring.
        for (const auto& error : errors) {
            stage_b_product(work, error.size(), 8u);
            work.spend(16u);
            if (class481_resource(error)) progress.resource = true;
        }
        class481_no_resource(errors);
        if (!errors.empty()) {
            stage_c_error(row, "Whole Class geometry or presentation differs");
            return false;
        }
        if (!following) {
            stage_c_error(row, "Successful whole Class assignment lacks distinct following role");
            return false;
        }
        progress.remaining[at] = work.left;
        return true;
    };
    if (wide_source)
        stage_c_error(row, "Complete mapped original still uses ordinary source at240");
    else if (drawn(frames.wide, 240, StageCPhase::Wide, 5u))
        row.status[0] = "drawn";
    progress.phase = StageCPhase::Narrow;
    if (narrow_source) {
        // The finite official originals exclude the token from source bytes.
        // This is physical ordinary protocol, not a graph/following assignment.
        auto grid = frame_grid(frames.narrow, 40, frames.height, work);
        if (following_row(grid, work, token)) {
            row.status[1] = "source_fallback";
            progress.narrow_ordinary = true;
        } else
            stage_c_error(row, "Exact narrow ordinary fallback lost physical following protocol");
        progress.remaining[6] = work.left;
    } else if (drawn(frames.narrow, 40, StageCPhase::Narrow, 6u))
        row.status[1] = "drawn";
    if (wide_source)
        stage_c_error(row, "Restored positive retains an unmet wide source requirement");
    else {
        const bool restored_good = drawn(frames.restored, 240, StageCPhase::Restored, 7u);
        if (!progress.restored_equal)
            stage_c_error(row, "Same-node240/40/240 restoration changed physical cells");
        else if (restored_good)
            row.status[2] = "drawn";
    }
    progress.admitted = progress.bound && progress.decoded && progress.captured &&
                        row.errors.empty() && row.status[0] == "drawn" &&
                        (row.status[1] == "drawn" || row.status[1] == "source_fallback") &&
                        row.status[2] == "drawn";
}

Observation run_class285_case(JsonVal fixture, std::size_t global,
                              const relations_oracle::OfficialClass285Documents& docs,
                              const relations_oracle::OfficialClass285Originals& originals,
                              relations_oracle::Work& work, std::size_t observation_size,
                              std::size_t observation_capacity) {
    using namespace relations_oracle;
    // This same source ledger prepays bounded failure and all Class publication.
    // No catch path allocates an unbounded e.what() or borrows another ledger.
    work.spend(kStageCReportReserve + 8u * sizeof(Observation) + 4u * sizeof(StageCProgress) +
               4096u);
    Observation row;
    StageCProgress progress;
    row.errors.reserve(6u);
    row.raw_active_schema = kStageCContract;
    row.track = "original";
    row.type = "class-diagram";
    row.expected = "drawn";
    row.expected_states = {"drawn", "drawn_or_exact_source", "drawn"};
    row.reason = "Whole original Class terminal contract; foreign assertions pending";
    row.decision_reasons.fill("Complete Class geometry, presentation and following role");
    row.decision_reasons[1] = "Predeclared40-column complete geometry OR exact ordinary source";
    try {
        // All original observations are noexcept movable, so vector relocation
        // moves the owned strings rather than copying their heap payloads.
        static_assert(std::is_nothrow_move_constructible_v<Observation>);
        work.spend(8u * sizeof(std::size_t) + 128u);
        class481_require(observation_size <= 2738u && observation_capacity <= 8192u,
                         "StageC observation vector bound");
        const auto slots = observation_size == observation_capacity
                               ? std::max(observation_capacity * 2u, observation_size + 1u)
                               : 0u;
        stage_b_product(work, slots + (slots ? observation_size : 0u) + 1u,
                        2u * sizeof(Observation) + 16u);
        progress.phase = StageCPhase::Acquire;
        work.spend(285u * 4u + 4096u);
        const auto found = std::ranges::find(originals.ordinals, global);
        class481_require(found != originals.ordinals.end(),
                         "StageC original global ordinal outside Class roster");
        row.role_case_index = static_cast<std::size_t>(found - originals.ordinals.begin());
        const auto path = class481_text(fixture.get("path"), work, 512u);
        const auto bytes = fixture.get("bytes");
        class481_require(bytes.is_int() && bytes.as_int() > 0 && bytes.as_int() <= 65536,
                         "StageC finite original input byte count");
        work.spend(path.size() * 12u + static_cast<std::size_t>(bytes.as_int()) * 8u + 8192u);
        row.path = path;
        class481_require(path.starts_with("class-diagram/") && safe_relative(path) &&
                             !fixture.has("diagnostic"),
                         "StageC original-only safe Class tuple");
        const auto input = read_file(kRoot / "cases" / row.path);
        progress.remaining[0] = work.left;
        progress.phase = StageCPhase::Capture;
        auto frames = stage_b_observe(input, kSentinel, work);
        progress.captured = true;
        progress.height = frames.height;
        progress.remaining[1] = work.left;
        progress.phase = StageCPhase::Bind;
        work.spend(input.size() * 4u + row.role_case_index + 1024u);
        class481_require(input.size() == static_cast<std::size_t>(bytes.as_int()),
                         "StageC actual complete original bytes");
        row.source_sha256 = digest(input);
        // All285 current source tuples have this finite protocol property. It
        // is not a parser restriction or an arbitrary-source footer theorem.
        class481_require(input.find(kSentinel) == std::string::npos,
                         "StageC source token collision requires separate source-role review");
        const auto source_row = docs.facts.root().get("cases").at(row.role_case_index);
        class481_require(source_row.get("source").is_str() &&
                             source_row.get("source").as_str() == input,
                         "StageC observed source equals approved whole authority");
        const auto selected =
            official_class285_bind(source_row, row.role_case_index, docs, originals, work);
        progress.bound = true;
        progress.dagre = selected.dagre;
        progress.classifier = selected.classifier;
        progress.unused_definitions = selected.unused_definitions;
        progress.notes = selected.notes;
        progress.remaining[2] = work.left;
        row.raw_source_authority = selected.notes ? "source_approved_Notes18"
                                   : selected.unused_definitions
                                       ? "source_approved_unused_classDef3"
                                   : selected.classifier ? "source_approved_classifier26"
                                   : selected.dagre      ? "source_approved_dagre38"
                                                         : "original152_or_whole_null";
        if (selected.meaning.is_null()) {
            progress.pending = true;
            row.expected = "pending";
            row.expected_states.fill("unadmitted_whole_source");
            row.reason = "Complete original semantic profile remains unadmitted; exact debts "
                         "retained by authority ordinal";
            const bool wide_source = stage_b_same_cells(frames.wide, frames.ordinary, work);
            const bool narrow_source = stage_b_same_cells(frames.narrow, frames.small, work);
            progress.restored_equal = stage_b_same_cells(frames.wide, frames.restored, work);
            progress.wide_source = wide_source;
            auto physical = [&](const Frame& frame, int width) {
                auto grid = frame_grid(frame, width, frames.height, work);
                return following_row(grid, work, kSentinel).has_value();
            };
            if (wide_source && physical(frames.wide, 240)) row.status[0] = "source_fallback";
            if (narrow_source && physical(frames.narrow, 40)) {
                row.status[1] = "source_fallback";
                progress.narrow_ordinary = true;
            }
            if (wide_source && progress.restored_equal && physical(frames.restored, 240))
                row.status[2] = "source_fallback";
            row.decision_reasons.fill(
                "Whole-null unadmitted; only exact ordinary physical source protocol can pass");
            if (row.status[0] == "error" || row.status[1] == "error" || row.status[2] == "error")
                stage_c_error(
                    row,
                    "Whole-null original has unreviewed art or failed ordinary/restore protocol");
        } else {
            progress.phase = StageCPhase::Decode;
            const auto model = class481_model(selected.meaning, work, true);
            progress.decoded = true;
            const auto encoded = class481_canonical(selected.meaning, work);
            work.spend(encoded.size() * 3u + 128u);
            row.role_model_sha = digest(encoded);
            progress.remaining[3] = work.left;
            stage_c_geometry(frames, model, selected.presentation, work, row, progress, kSentinel);
        }
        progress.phase = StageCPhase::Complete;
    } catch (const std::exception& error) {
        progress.exception = true;
        const char* text = error.what();
        std::size_t length = 0;
        if (text)
            while (length < 256u && text[length])
                ++length;
        const std::string_view bounded(text ? text : "", length);
        progress.resource = progress.resource || work.left == 0u || class481_resource(bounded);
        if (text)
            stage_c_error(row, bounded);
        else
            stage_c_error(row, "Class exception without observable bounded message");
        progress.admitted = false;
    }
    // <=2048 serialized bytes and <=64 object/array/scalar nodes. Entry reserve
    // pays construction, three later report parses/copies, summary and final JSON/MD.
    json::JsonMutDoc report;
    auto info = report.object();
    info.set("original_global_ordinal", static_cast<std::int64_t>(global));
    info.set("source_case_ordinal", static_cast<std::int64_t>(row.role_case_index));
    info.set("phase", static_cast<int>(progress.phase));
    info.set("height", progress.height);
    info.set("five_captures_complete", progress.captured);
    info.set("source_bound", progress.bound);
    info.set("whole_Model_decoded", progress.decoded);
    info.set("whole_null_pending", progress.pending);
    info.set("wide_equals_ordinary", progress.wide_source);
    info.set("whole_source_admitted", progress.admitted);
    info.set("restored_cells_equal", progress.restored_equal);
    info.set("narrow_ordinary_protocol", progress.narrow_ordinary);
    info.set("dagre38", progress.dagre);
    info.set("classifier26", progress.classifier);
    info.set("unused_classDef3", progress.unused_definitions);
    info.set("Notes18", progress.notes);
    info.set("exception", progress.exception);
    info.set("verification_resource_exhausted", progress.resource);
    info.set("initial_verification_budget", static_cast<std::int64_t>(kStageCVerificationBudget));
    info.set("verification_remaining", static_cast<std::int64_t>(work.left));
    auto costs = report.array();
    for (const auto& value : progress.remaining) {
        if (value)
            costs.append(report.number(static_cast<std::int64_t>(*value)));
        else
            costs.append(report.null());
    }
    info.add("phase_remaining", costs);
    report.set_root(info);
    row.raw_source_constraints = report.to_string();
    if (row.raw_source_constraints.size() > 2048u)
        throw std::runtime_error("StageC prepaid report bound violated");
    return row;
}

StageCCounts stage_c_counts(const std::vector<Observation>& rows) {
    // All Class report parsing/stack/set-free counters and serialization shares
    // were prepaid by each source owner. No document or Model pointer escapes.
    StageCCounts counts;
    std::array<bool, 285> seen{};
    for (const auto& row : rows) {
        if (row.raw_active_schema != kStageCContract) continue;
        require(row.role_case_index < 285u && !seen[row.role_case_index],
                "StageC duplicate or unknown consumed original");
        seen[row.role_case_index] = true;
        ++counts.visited;
        require(row.raw_source_constraints.size() <= 2048u, "StageC bounded owned report");
        const auto parsed = json::parse(row.raw_source_constraints);
        require(parsed.has_value(), "StageC owned report JSON");
        const auto info = parsed->root();
        auto yes = [&](std::string_view key) {
            const auto value = info.get(key);
            require(value.is_bool(), "StageC report bool type");
            return value.as_bool();
        };
        counts.observed += yes("five_captures_complete");
        counts.bound += yes("source_bound");
        counts.decoded += yes("whole_Model_decoded");
        const bool admitted = yes("whole_source_admitted");
        if (admitted)
            require(yes("five_captures_complete") && yes("source_bound") &&
                        yes("whole_Model_decoded") && !yes("whole_null_pending") &&
                        yes("restored_cells_equal") && row.errors.empty() &&
                        row.status[0] == "drawn" && row.status[2] == "drawn" &&
                        (row.status[1] == "drawn" || row.status[1] == "source_fallback"),
                    "StageC per-state proof cannot bypass whole-source admission");
        counts.pending += yes("whole_null_pending");
        counts.admitted += admitted;
        counts.dagre += yes("dagre38");
        counts.classifier += yes("classifier26");
        counts.unused_definitions += yes("unused_classDef3");
        counts.notes += yes("Notes18");
        counts.errors += !row.errors.empty();
        counts.resources += yes("verification_resource_exhausted");
    }
    return counts;
}

TEST(MermaidOfficialStageC, DirectWholeSceneContractKeepsBusinessFooterAndProtocolRoles) {
    for (const std::size_t ordinal : {1u, 2u, 3u}) {
        relations_oracle::Work work{kStageCVerificationBudget};
        work.spend(kStageCReportReserve + kStageBSources.size() * 3u + 32768u);
        const auto document = json::parse(kStageBSources);
        ASSERT_TRUE(document);
        const auto source = document->root().get("sources").at(ordinal);
        const auto input =
            relations_oracle::class481_text(source.get("complete_source"), work, 65536u);
        const auto token =
            relations_oracle::class481_text(source.get("following_token"), work, 240u);
        auto frames = stage_b_observe(input, token, work);
        const auto bound = stage_b_bind(source, ordinal, input, work);
        ASSERT_TRUE(bound);
        Observation row;
        row.errors.reserve(6u);
        StageCProgress progress;
        progress.captured = progress.bound = progress.decoded = true;
        stage_c_geometry(frames, bound->model, bound->presentation, work, row, progress, token);
        ASSERT_TRUE(progress.admitted);
        ASSERT_TRUE(row.errors.empty());
        work.spend((bound->model_bytes + 256u) * (8u * sizeof(std::string) + 64u) + 1024u);
        auto wrong = bound->model;
        wrong.entities[0].label = "FOREIGN";
        wrong.entities[0].headers[0][0] = "FOREIGN";
        Observation rejected;
        rejected.errors.reserve(6u);
        StageCProgress changed;
        changed.captured = changed.bound = changed.decoded = true;
        stage_c_geometry(frames, wrong, bound->presentation, work, rejected, changed, token);
        EXPECT_FALSE(changed.admitted);
        EXPECT_EQ(rejected.status[0], "error");
        EXPECT_FALSE(rejected.errors.empty());
        work.spend(4u * sizeof(StageBFrames) + 8u * sizeof(Observation) + 4096u);
        stage_b_product(work, frames.restored.cells.size(),
                        3u * sizeof(std::vector<std::string>) + 64u);
        for (const auto& cells : frames.restored.cells) {
            stage_b_product(work, cells.size(), 3u * sizeof(std::string) + 16u);
            for (const auto& text : cells) {
                stage_b_product(work, text.size(), 2u);
                work.spend(4u);
            }
        }
        stage_b_product(work, frames.restored.text.size(), 2u);
        work.spend(1u);
        auto restored = frames.restored;
        work.spend(8u * sizeof(std::size_t) + 128u);
        const auto size = frames.restored.cells[0].size(),
                   capacity = frames.restored.cells[0].capacity();
        const auto slots = size == capacity ? std::max(2u * capacity, size + 1u) : 0u;
        stage_b_product(work, slots + (slots ? size : 0u) + 1u, 2u * sizeof(std::string) + 16u);
        work.spend(64u);
        frames.restored.cells[0].push_back(" ");
        Observation malformed;
        malformed.errors.reserve(6u);
        StageCProgress bad;
        bad.captured = bad.bound = bad.decoded = true;
        bool protocol_rejected = false;
        try {
            stage_c_geometry(frames, bound->model, bound->presentation, work, malformed, bad,
                             token);
        } catch (const std::exception& error) {
            ASSERT_GT(work.left, 0u) << "Resource failure cannot certify malformed-frame rejection";
            EXPECT_EQ(std::string_view(error.what()), "StageB Frame physical row width differs");
            protocol_rejected = true;
        }
        EXPECT_TRUE(protocol_rejected);
        EXPECT_FALSE(bad.admitted);
        EXPECT_EQ(malformed.status[0], "drawn");
        // This actual good-wide/bad-restored result cannot earn formal Class
        // coverage, even though its honest wide per-state aggregate is Drawn.
        work.spend(32768u + 8u * sizeof(Observation) + 4096u);
        malformed.raw_active_schema = kStageCContract;
        malformed.role_case_index = 0u;
        json::JsonMutDoc metadata;
        auto info = metadata.object();
        info.set("five_captures_complete", bad.captured);
        info.set("source_bound", bad.bound);
        info.set("whole_Model_decoded", bad.decoded);
        info.set("whole_null_pending", false);
        info.set("whole_source_admitted", bad.admitted);
        info.set("restored_cells_equal", bad.restored_equal);
        info.set("dagre38", false);
        info.set("classifier26", false);
        info.set("unused_classDef3", false);
        info.set("Notes18", false);
        info.set("verification_resource_exhausted", false);
        metadata.set_root(info);
        malformed.raw_source_constraints = metadata.to_string();
        std::vector<Observation> one;
        one.push_back(std::move(malformed));
        const auto tally = stage_c_counts(one);
        EXPECT_EQ(tally.visited, 1u);
        EXPECT_EQ(tally.admitted, 0u);
        // Parsed protocol negative, not a semantic rejection: changing only
        // the owned admission bit must not bypass its actual per-state checks.
        info.set("whole_source_admitted", true);
        one[0].raw_source_constraints = metadata.to_string();
        bool admission_rejected = false;
        try {
            static_cast<void>(stage_c_counts(one));
        } catch (const std::exception& error) {
            ASSERT_GT(work.left, 0u);
            EXPECT_EQ(std::string_view(error.what()),
                      "StageC per-state proof cannot bypass whole-source admission");
            admission_rejected = true;
        }
        EXPECT_TRUE(admission_rejected);
        work.spend(4u * sizeof(Frame) + 128u);
        frames.restored = std::move(restored);
    }
}
}  // namespace

namespace {
// Flow-only acquisition and classification. Existing Frame/Class paths stay exact.
void official_flow_error(Observation& row, std::string_view message) {
    if (row.errors.size() < 6u) row.errors.emplace_back(message.substr(0, 128u));
}

std::string official_flow_info(const OfficialFlowProgress& p, std::size_t remaining,
                               bool config21 = false, bool diagram_title = false,
                               std::size_t title_mutations = 0u, bool shape_alias = false,
                               std::size_t shape_alias_mutations = 0u) {
    // Each scene prepays this bounded metadata and later matrix copies/parses.
    json::JsonMutDoc doc;
    auto info = doc.object();
    info.set("five_captures_complete", p.captured);
    info.set("source_bound", p.bound);
    info.set("whole_Model_decoded", p.decoded);
    info.set("whole_null_pending", p.pending);
    info.set("whole_source_admitted", p.admitted);
    info.set("restored_cells_equal", p.restored);
    info.set("narrow_ordinary", p.narrow_ordinary);
    info.set("semantic_mutations_complete", p.mutated);
    info.set("verification_resource_exhausted", p.resource);
    info.set("captured_frames", static_cast<int64_t>(p.captures));
    if (config21)
        info.set("source_supplement_sha256", flow_oracle::kFlow400Config21AssetSha);
    else
        info.add("source_supplement_sha256", info.make_null());
    info.set("phase", p.phase);
    info.set("remaining", static_cast<int64_t>(remaining));
    if (diagram_title)
        info.set("diagram_title_mutations_rejected", static_cast<int64_t>(title_mutations));
    if (shape_alias) {
        auto mutations = info.make_obj();
        mutations.set("expected", 2);
        mutations.set("rejected", static_cast<int64_t>(shape_alias_mutations));
        info.add("shape_alias_mutations", mutations);
    }
    info.set("height", p.height);
    auto costs = info.make_arr();
    for (const auto n : p.remaining)
        costs.append(info.make_num(static_cast<int64_t>(n)));
    info.add("phase_remaining", costs);
    doc.set_root(info);
    const auto text = doc.to_string();
    require(text.size() <= 2048u, "OfficialFlow400 prepaid metadata bound");
    return text;
}

std::size_t official_flow_typed_source4_phase_checkpoints(std::string_view phase) {
    if (phase == "acquire" || phase == "capture") return 0u;
    if (phase == "publication") return 1u;
    if (phase == "bind" || phase == "decode") return 2u;
    if (phase == "pending_physical" || phase == "compare") return 3u;
    if (phase == "wide" || phase == "narrow" || phase == "narrow_source") return 4u;
    if (phase == "restored") return 5u;
    if (phase == "mutations") return 6u;
    if (phase == "complete") return 7u;
    require(false, "OfficialFlow400 TypedSource4 known phase");
    return 0u;
}

std::string official_flow_typed_source4_info(const OfficialFlowProgress& p, std::size_t remaining,
                                             std::size_t expected, std::size_t rejected) {
    // TypedSource4 has two extra owned fields. Base and Config21 retain their
    // existing fifteen-key serializer and are never routed through this branch.
    json::JsonMutDoc doc;
    auto info = doc.object();
    info.set("five_captures_complete", p.captured);
    info.set("source_bound", p.bound);
    info.set("whole_Model_decoded", p.decoded);
    info.set("whole_null_pending", p.pending);
    info.set("whole_source_admitted", p.admitted);
    info.set("restored_cells_equal", p.restored);
    info.set("narrow_ordinary", p.narrow_ordinary);
    info.set("semantic_mutations_complete", p.mutated);
    info.set("verification_resource_exhausted", p.resource);
    info.set("captured_frames", static_cast<int64_t>(p.captures));
    info.set("source_supplement_sha256", flow_oracle::kFlow400TypedSource4AssetSha);
    info.set("typed_mutations_expected", static_cast<int64_t>(expected));
    info.set("typed_mutations_rejected", static_cast<int64_t>(rejected));
    info.set("phase", p.phase);
    info.set("remaining", static_cast<int64_t>(remaining));
    info.set("height", p.height);
    auto costs = info.make_arr();
    const auto observed = official_flow_typed_source4_phase_checkpoints(p.phase);
    for (std::size_t index = 0u; index < p.remaining.size(); ++index) {
        if (index < observed)
            costs.append(info.make_num(static_cast<int64_t>(p.remaining[index])));
        else
            costs.append(info.make_null());
    }
    info.add("phase_remaining", costs);
    doc.set_root(info);
    const auto text = doc.to_string();
    require(text.size() <= 2048u, "OfficialFlow400 prepaid TypedSource4 metadata bound");
    return text;
}

void official_flow_typed_source4_keys(JsonVal value) {
    constexpr std::array<std::string_view, 17> keys{
        {"five_captures_complete", "source_bound", "whole_Model_decoded", "whole_null_pending",
         "whole_source_admitted", "restored_cells_equal", "narrow_ordinary",
         "semantic_mutations_complete", "verification_resource_exhausted", "captured_frames",
         "phase", "remaining", "height", "phase_remaining", "source_supplement_sha256",
         "typed_mutations_expected", "typed_mutations_rejected"}};
    require(value.is_obj() && value.size() == keys.size(),
            "OfficialFlow400 TypedSource4 metadata key count");
    std::array<bool, keys.size()> seen{};
    value.iter_obj([&](JsonVal key, JsonVal) {
        const auto text = flow_oracle::flow400_text(key);
        const auto at = std::ranges::find(keys, text);
        require(at != keys.end(), "OfficialFlow400 TypedSource4 metadata unknown key");
        const auto index = static_cast<std::size_t>(at - keys.begin());
        require(!seen[index], "OfficialFlow400 TypedSource4 metadata duplicate key");
        seen[index] = true;
    });
    require(std::ranges::all_of(seen, [](bool present) { return present; }),
            "OfficialFlow400 TypedSource4 metadata missing key");
}

std::size_t official_flow_typed_source4_identity(JsonVal typed_source4, const Observation& row) {
    require(typed_source4.is_obj() && row.role_case_index < 400u,
            "OfficialFlow400 TypedSource4 authority unavailable");
    const auto cases = typed_source4.get("cases");
    require(cases.is_arr() && cases.size() == 4u,
            "OfficialFlow400 TypedSource4 authority denominator");
    JsonVal selected;
    std::size_t matches = 0u;
    cases.iter([&](JsonVal candidate) {
        const auto authority = candidate.get("base_authority");
        if (authority.is_obj() &&
            flow_oracle::flow400_number(authority.get("ordinal")) == row.role_case_index) {
            selected = candidate;
            ++matches;
        }
    });
    require(matches == 1u, "OfficialFlow400 TypedSource4 authority selection");
    const auto authority = selected.get("base_authority");
    require(row.path == flow_oracle::flow400_text(authority.get("fixture")) &&
                row.source_sha256 == flow_oracle::flow400_text(selected.get("source_sha256")) &&
                row.role_model_sha == flow_oracle::flow400_text(selected.get("typed_model_sha256")),
            "OfficialFlow400 inconsistent TypedSource4 authority metadata");
    return flow_oracle::flow400_typed_source4_expected(selected);
}

std::size_t official_flow_typed_source4_mutations(const flow_oracle::Grid& good,
                                                  const flow_oracle::Model& model,
                                                  JsonVal source4_case,
                                                  flow_oracle::OracleWork& work) {
    using flow_oracle::flow400_require;
    using flow_oracle::Point;
    using flow_oracle::Shape;
    const auto baseline = flow_oracle::flow400_geometry(good, model, work, kSentinel);
    flow400_require(baseline.empty(), "OfficialFlow400 TypedSource4 mutation baseline");
    const auto id = flow_oracle::flow400_text(source4_case.get("case_id"));
    const auto expected = flow_oracle::flow400_typed_source4_expected(source4_case);
    std::size_t rejected = flow_oracle::flow400_mutations(good, model, work, kSentinel);
    flow400_require(rejected == 3u, "OfficialFlow400 TypedSource4 legacy mutation count");
    work.spend(256u * sizeof(flow_oracle::ShapeFrame) + 8192u);
    const auto contours = flow_oracle::closed_frames(good, work);
    const auto node_index = [&](std::string_view node_id) {
        const auto node = std::ranges::find_if(
            model.nodes, [&](const auto& value) { return value.id == node_id; });
        flow400_require(node != model.nodes.end(),
                        "OfficialFlow400 TypedSource4 selected node absent");
        return static_cast<std::size_t>(node - model.nodes.begin());
    };
    const auto frame = [&](std::size_t index) {
        const auto found =
            flow_oracle::flow_typed_frame_for_node(good, contours, model.nodes[index], work);
        flow400_require(found.has_value(), "OfficialFlow400 TypedSource4 unique selected contour");
        return *found;
    };
    const auto semantic = [&](auto&& mutate, std::string_view name) {
        flow_oracle::flow_typed_expect_model_rejected(good, model, work, mutate, name, kSentinel);
        ++rejected;
    };
    const auto physical = [&](auto&& mutate, std::string_view name) {
        flow_oracle::flow_typed_expect_rejected(good, model, work, mutate, name, kSentinel);
        ++rejected;
    };
    if (id == "original_000") {
        const auto b = node_index("B"), e = node_index("E");
        semantic([&](auto& bad) { bad.nodes[b].shape = Shape::Rectangle; },
                 "Official Source4 original000 B diamond identity");
        semantic([&](auto& bad) { bad.nodes[e].shape = Shape::Rectangle; },
                 "Official Source4 original000 E diamond identity");
        flow400_require(model.edges.size() == 6u && model.edges[1].label == "Yes" &&
                            model.edges[2].label == "No",
                        "Official Source4 original000 label order");
        semantic([](auto& bad) { std::swap(bad.edges[1].label, bad.edges[2].label); },
                 "Official Source4 original000 Yes/No semantic swap");
        const auto yes = flow_oracle::flow_typed_caption_blocks(good, "Yes", work),
                   no = flow_oracle::flow_typed_caption_blocks(good, "No", work);
        flow400_require(yes.size() == 2u && no.size() == 2u,
                        "Official Source4 original000 repeated labels");
        physical(
            [&](auto& bad) { flow_oracle::flow_typed_erase_label(bad, yes.front(), "Yes", work); },
            "Official Source4 original000 erase Yes");
        const auto orphan = flow_oracle::flow_typed_orphan_label_start(good, "No", contours, work);
        flow400_require(orphan.has_value(), "Official Source4 original000 No orphan witness");
        physical(
            [&](auto& bad) {
                flow_oracle::flow_typed_erase_label(bad, no.back(), "No", work);
                flow_oracle::flow_typed_write_at(bad, *orphan, "No", work);
                flow400_require(flow_oracle::flow_typed_caption_blocks(bad, "No", work).size() ==
                                    2u,
                                "Official Source4 original000 No count preserved");
            },
            "Official Source4 original000 No off owner path");
    } else if (id == "original_282") {
        const auto b = node_index("B"), c = node_index("C");
        const auto rounded = frame(b);
        const auto decision = frame(c);
        semantic([&](auto& bad) { bad.nodes[b].shape = Shape::Rectangle; },
                 "Official Source4 original282 B rounded identity");
        physical([&](auto& bad) { bad.put({rounded.left, rounded.top}, "┌"); },
                 "Official Source4 original282 B mixed rounded corner");
        semantic([&](auto& bad) { bad.nodes[c].shape = Shape::Rectangle; },
                 "Official Source4 original282 C diamond identity");
        const Point shoulder{decision.left + 1, decision.top + 1};
        flow400_require(good.at(shoulder) == "╱", "Official Source4 original282 diagonal witness");
        physical([&](auto& bad) { bad.put(shoulder, " "); },
                 "Official Source4 original282 C missing diagonal");
        semantic(
            [&](auto& bad) {
                const auto at = bad.nodes[c].label.find('\n');
                flow400_require(at != std::string::npos,
                                "Official Source4 original282 multiline Model witness");
                bad.nodes[c].label.erase(at, 1u);
            },
            "Official Source4 original282 flattened C Model");
        const auto labels =
            flow_oracle::flow_typed_caption_blocks(good, model.nodes[c].label, work);
        flow400_require(labels.size() == 1u && labels.front().starts.size() == 3u,
                        "Official Source4 original282 C three-line label witness");
        physical([&](auto& bad) { bad.put(labels.front().starts[1], " "); },
                 "Official Source4 original282 C middle label glyph");
    } else if (id == "original_374") {
        const auto n1 = node_index("n1");
        const auto decision = frame(n1);
        semantic([&](auto& bad) { bad.nodes[n1].shape = Shape::Rectangle; },
                 "Official Source4 original374 diamond identity");
        const Point shoulder{decision.left + 1, decision.top + 1};
        flow400_require(good.at(shoulder) == "╱", "Official Source4 original374 diagonal witness");
        physical([&](auto& bad) { bad.put(shoulder, " "); },
                 "Official Source4 original374 missing diagonal");
    } else {
        flow400_require(id == "original_375", "OfficialFlow400 TypedSource4 unknown case mutation");
        const auto n1 = node_index("n1"), n3 = node_index("n3");
        const auto rounded = frame(n1);
        semantic([&](auto& bad) { bad.nodes[n1].shape = Shape::Rectangle; },
                 "Official Source4 original375 rounded identity");
        physical([&](auto& bad) { bad.put({rounded.left, rounded.top}, "┌"); },
                 "Official Source4 original375 rounded mixed corner");
        semantic([&](auto& bad) { bad.nodes[n3].shape = Shape::Rounded; },
                 "Official Source4 original375 explicit rect identity");
        const auto rectangles =
            flow_oracle::frame_candidates(good, contours, "Untitled Node", Shape::Rectangle, work);
        flow400_require(rectangles.size() == 2u,
                        "Official Source4 original375 two source rectangle contours");
        const auto& rectangle = contours[static_cast<std::size_t>(rectangles.front())];
        flow400_require(good.at({rectangle.left, rectangle.top}) == "┌",
                        "Official Source4 original375 rectangle corner witness");
        physical([&](auto& bad) { bad.put({rectangle.left, rectangle.top}, "╭"); },
                 "Official Source4 original375 one required rectangle mixed corner");
    }
    flow400_require(rejected == expected, "OfficialFlow400 TypedSource4 exact mutation count");
    return rejected;
}

std::string official_flow_classic_info(const OfficialFlowProgress& p, std::size_t remaining,
                                       std::size_t expected, std::size_t rejected) {
    json::JsonMutDoc doc;
    auto info = doc.object();
    info.set("five_captures_complete", p.captured);
    info.set("source_bound", p.bound);
    info.set("whole_Model_decoded", p.decoded);
    info.set("whole_null_pending", p.pending);
    info.set("whole_source_admitted", p.admitted);
    info.set("restored_cells_equal", p.restored);
    info.set("narrow_ordinary", p.narrow_ordinary);
    info.set("semantic_mutations_complete", p.mutated);
    info.set("verification_resource_exhausted", p.resource);
    info.set("captured_frames", static_cast<int64_t>(p.captures));
    info.set("source_supplement_sha256", flow_oracle::kFlow400ClassicAssetSha);
    info.set("classic_mutations_expected", static_cast<int64_t>(expected));
    info.set("classic_mutations_rejected", static_cast<int64_t>(rejected));
    info.set("phase", p.phase);
    info.set("remaining", static_cast<int64_t>(remaining));
    info.set("height", p.height);
    auto costs = info.make_arr();
    const auto observed = official_flow_typed_source4_phase_checkpoints(p.phase);
    for (std::size_t index = 0u; index < p.remaining.size(); ++index) {
        if (index < observed)
            costs.append(info.make_num(static_cast<int64_t>(p.remaining[index])));
        else
            costs.append(info.make_null());
    }
    info.add("phase_remaining", costs);
    doc.set_root(info);
    const auto text = doc.to_string();
    require(text.size() <= 2048u, "OfficialFlow400 prepaid Classic metadata bound");
    return text;
}

void official_flow_classic_keys(JsonVal value) {
    constexpr std::array<std::string_view, 17> keys{
        {"five_captures_complete", "source_bound", "whole_Model_decoded", "whole_null_pending",
         "whole_source_admitted", "restored_cells_equal", "narrow_ordinary",
         "semantic_mutations_complete", "verification_resource_exhausted", "captured_frames",
         "phase", "remaining", "height", "phase_remaining", "source_supplement_sha256",
         "classic_mutations_expected", "classic_mutations_rejected"}};
    require(value.is_obj() && value.size() == keys.size(),
            "OfficialFlow400 Classic metadata key count");
    std::array<bool, keys.size()> seen{};
    value.iter_obj([&](JsonVal key, JsonVal) {
        const auto text = flow_oracle::flow400_text(key);
        const auto at = std::ranges::find(keys, text);
        require(at != keys.end(), "OfficialFlow400 Classic metadata unknown key");
        const auto index = static_cast<std::size_t>(at - keys.begin());
        require(!seen[index], "OfficialFlow400 Classic metadata duplicate key");
        seen[index] = true;
    });
    require(std::ranges::all_of(seen, [](bool present) { return present; }),
            "OfficialFlow400 Classic metadata missing key");
}

std::string official_flow_group_endpoint15_info(const OfficialFlowProgress& p,
                                                std::size_t remaining, std::size_t expected,
                                                std::size_t rejected,
                                                std::size_t manifest_global_ordinal) {
    json::JsonMutDoc doc;
    auto info = doc.object();
    info.set("five_captures_complete", p.captured);
    info.set("source_bound", p.bound);
    info.set("whole_Model_decoded", p.decoded);
    info.set("whole_null_pending", p.pending);
    info.set("whole_source_admitted", p.admitted);
    info.set("restored_cells_equal", p.restored);
    info.set("narrow_ordinary", p.narrow_ordinary);
    info.set("semantic_mutations_complete", p.mutated);
    info.set("verification_resource_exhausted", p.resource);
    info.set("captured_frames", static_cast<int64_t>(p.captures));
    info.set("source_supplement_sha256", flow_oracle::kFlow400GroupEndpoint15AssetSha);
    info.set("manifest_global_ordinal", static_cast<int64_t>(manifest_global_ordinal));
    info.set("group_endpoint15_mutations_expected", static_cast<int64_t>(expected));
    info.set("group_endpoint15_mutations_rejected", static_cast<int64_t>(rejected));
    info.set("phase", p.phase);
    info.set("remaining", static_cast<int64_t>(remaining));
    info.set("height", p.height);
    auto costs = info.make_arr();
    const auto observed = official_flow_typed_source4_phase_checkpoints(p.phase);
    for (std::size_t index = 0u; index < p.remaining.size(); ++index) {
        if (index < observed)
            costs.append(info.make_num(static_cast<int64_t>(p.remaining[index])));
        else
            costs.append(info.make_null());
    }
    info.add("phase_remaining", costs);
    doc.set_root(info);
    const auto text = doc.to_string();
    require(text.size() <= 2048u, "OfficialFlow400 prepaid GroupEndpoint15 metadata bound");
    return text;
}

void official_flow_group_endpoint15_keys(JsonVal value) {
    constexpr std::array<std::string_view, 18> keys{
        {"five_captures_complete", "source_bound", "whole_Model_decoded", "whole_null_pending",
         "whole_source_admitted", "restored_cells_equal", "narrow_ordinary",
         "semantic_mutations_complete", "verification_resource_exhausted", "captured_frames",
         "phase", "remaining", "height", "phase_remaining", "source_supplement_sha256",
         "manifest_global_ordinal", "group_endpoint15_mutations_expected",
         "group_endpoint15_mutations_rejected"}};
    require(value.is_obj() && value.size() == keys.size(),
            "OfficialFlow400 GroupEndpoint15 metadata key count");
    std::array<bool, keys.size()> seen{};
    value.iter_obj([&](JsonVal key, JsonVal) {
        const auto text = flow_oracle::flow400_text(key);
        const auto at = std::ranges::find(keys, text);
        require(at != keys.end(), "OfficialFlow400 GroupEndpoint15 metadata unknown key");
        const auto index = static_cast<std::size_t>(at - keys.begin());
        require(!seen[index], "OfficialFlow400 GroupEndpoint15 metadata duplicate key");
        seen[index] = true;
    });
    require(std::ranges::all_of(seen, [](bool present) { return present; }),
            "OfficialFlow400 GroupEndpoint15 metadata missing key");
}

std::size_t official_flow_group_endpoint15_identity(JsonVal group_endpoint15,
                                                    const Observation& row, JsonVal info) {
    require(group_endpoint15.is_obj() && row.role_case_index < 400u,
            "OfficialFlow400 GroupEndpoint15 authority unavailable");
    const auto records = group_endpoint15.get("records");
    const auto manifest_global_ordinal =
        flow_oracle::flow400_number(info.get("manifest_global_ordinal"));
    require(records.is_arr() && records.size() == flow_oracle::kFlow400GroupEndpoint15Tuples.size(),
            "OfficialFlow400 GroupEndpoint15 authority denominator");
    JsonVal selected;
    std::size_t matches = 0u;
    records.iter([&](JsonVal candidate) {
        if (flow_oracle::flow400_number(candidate.get("ordinal")) == row.role_case_index) {
            selected = candidate;
            ++matches;
        }
    });
    require(matches == 1u, "OfficialFlow400 GroupEndpoint15 authority selection");
    const auto tuple = flow_oracle::group_endpoint15_tuple(selected);
    require(manifest_global_ordinal == tuple.manifest_global_ordinal,
            "OfficialFlow400 GroupEndpoint15 manifest global metadata");
    require(row.path == tuple.fixture && row.source_sha256 == tuple.source_sha256 &&
                row.role_model_sha ==
                    flow_oracle::flow400_text(selected.get("canonical_model_sha256")),
            "OfficialFlow400 inconsistent GroupEndpoint15 authority metadata");
    return flow_oracle::group_endpoint15_expected_mutations(selected);
}

std::size_t official_flow_group_endpoint15_mutations(const flow_oracle::Grid& good,
                                                     const flow_oracle::GroupEndpoint15Model& model,
                                                     JsonVal record,
                                                     flow_oracle::OracleWork& work) {
    const auto expected = flow_oracle::group_endpoint15_expected_mutations(record);
    const auto rejected = flow_oracle::group_endpoint15_mutations(good, model, work, kSentinel);
    require(rejected == expected, "OfficialFlow400 GroupEndpoint15 exact mutation execution");
    return rejected;
}

std::string official_flow_linkprofile3_info(const OfficialFlowProgress& p, std::size_t remaining,
                                            std::size_t expected, std::size_t rejected) {
    json::JsonMutDoc doc;
    auto info = doc.object();
    info.set("five_captures_complete", p.captured);
    info.set("source_bound", p.bound);
    info.set("whole_Model_decoded", p.decoded);
    info.set("whole_null_pending", p.pending);
    info.set("whole_source_admitted", p.admitted);
    info.set("restored_cells_equal", p.restored);
    info.set("narrow_ordinary", p.narrow_ordinary);
    info.set("semantic_mutations_complete", p.mutated);
    info.set("verification_resource_exhausted", p.resource);
    info.set("captured_frames", static_cast<int64_t>(p.captures));
    info.set("source_supplement_sha256", flow_oracle::kFlow400LinkProfile3AssetSha);
    info.set("linkprofile3_mutations_expected", static_cast<int64_t>(expected));
    info.set("linkprofile3_mutations_rejected", static_cast<int64_t>(rejected));
    info.set("phase", p.phase);
    info.set("remaining", static_cast<int64_t>(remaining));
    info.set("height", p.height);
    auto costs = info.make_arr();
    const auto observed = official_flow_typed_source4_phase_checkpoints(p.phase);
    for (std::size_t index = 0u; index < p.remaining.size(); ++index) {
        if (index < observed)
            costs.append(info.make_num(static_cast<int64_t>(p.remaining[index])));
        else
            costs.append(info.make_null());
    }
    info.add("phase_remaining", costs);
    doc.set_root(info);
    const auto text = doc.to_string();
    require(text.size() <= 2048u, "OfficialFlow400 prepaid LinkProfile3 metadata bound");
    return text;
}

void official_flow_linkprofile3_keys(JsonVal value) {
    constexpr std::array<std::string_view, 17> keys{
        {"five_captures_complete", "source_bound", "whole_Model_decoded", "whole_null_pending",
         "whole_source_admitted", "restored_cells_equal", "narrow_ordinary",
         "semantic_mutations_complete", "verification_resource_exhausted", "captured_frames",
         "phase", "remaining", "height", "phase_remaining", "source_supplement_sha256",
         "linkprofile3_mutations_expected", "linkprofile3_mutations_rejected"}};
    require(value.is_obj() && value.size() == keys.size(),
            "OfficialFlow400 LinkProfile3 metadata key count");
    std::array<bool, keys.size()> seen{};
    value.iter_obj([&](JsonVal key, JsonVal) {
        const auto text = flow_oracle::flow400_text(key);
        const auto at = std::ranges::find(keys, text);
        require(at != keys.end(), "OfficialFlow400 LinkProfile3 metadata unknown key");
        const auto index = static_cast<std::size_t>(at - keys.begin());
        require(!seen[index], "OfficialFlow400 LinkProfile3 metadata duplicate key");
        seen[index] = true;
    });
    require(std::ranges::all_of(seen, [](bool present) { return present; }),
            "OfficialFlow400 LinkProfile3 metadata missing key");
}

std::size_t official_flow_linkprofile3_identity(JsonVal linkprofile3, const Observation& row) {
    require(linkprofile3.is_obj() && row.role_case_index < 400u,
            "OfficialFlow400 LinkProfile3 authority unavailable");
    const auto cases = linkprofile3.get("cases");
    require(cases.is_arr() && cases.size() == 4u,
            "OfficialFlow400 LinkProfile3 authority denominator");
    JsonVal selected;
    std::size_t matches = 0u;
    cases.iter([&](JsonVal candidate) {
        const auto authority = candidate.get("base_authority");
        if (authority.is_obj() &&
            flow_oracle::flow400_number(authority.get("ordinal")) == row.role_case_index) {
            selected = candidate;
            ++matches;
        }
    });
    require(matches == 1u, "OfficialFlow400 LinkProfile3 authority selection");
    const auto authority = selected.get("base_authority");
    require(row.path == flow_oracle::flow400_text(authority.get("fixture")) &&
                row.source_sha256 == flow_oracle::flow400_text(selected.get("source_sha256")) &&
                row.role_model_sha == flow_oracle::flow400_text(selected.get("link_model_sha256")),
            "OfficialFlow400 inconsistent LinkProfile3 authority metadata");
    return flow_oracle::flow400_linkprofile3_expected(selected);
}

std::size_t official_flow_classic_identity(JsonVal classic, const Observation& row) {
    require(classic.is_obj() && row.role_case_index < 400u,
            "OfficialFlow400 Classic authority unavailable");
    const auto cases = classic.get("cases");
    require(cases.is_arr() && cases.size() == 6u, "OfficialFlow400 Classic authority denominator");
    JsonVal selected;
    std::size_t matches = 0u;
    cases.iter([&](JsonVal candidate) {
        const auto authority = candidate.get("base_authority");
        if (authority.is_obj() &&
            flow_oracle::flow400_number(authority.get("ordinal")) == row.role_case_index) {
            selected = candidate;
            ++matches;
        }
    });
    require(matches == 1u, "OfficialFlow400 Classic authority selection");
    const auto authority = selected.get("base_authority");
    require(row.path == flow_oracle::flow400_text(authority.get("fixture")) &&
                row.source_sha256 == flow_oracle::flow400_text(selected.get("source_sha256")) &&
                row.role_model_sha ==
                    flow_oracle::flow400_text(selected.get("classic_model_sha256")),
            "OfficialFlow400 inconsistent Classic authority metadata");
    return flow_oracle::flow400_classic_expected(selected);
}

std::size_t official_flow_classic_mutations(const flow_oracle::Grid& good,
                                            const flow_oracle::Model& model, JsonVal classic_case,
                                            flow_oracle::OracleWork& work) {
    using flow_oracle::flow400_require;
    using flow_oracle::Point;
    using flow_oracle::Shape;
    const auto baseline = flow_oracle::flow400_geometry(good, model, work, kSentinel);
    flow400_require(baseline.empty(), "OfficialFlow400 Classic mutation baseline");
    const auto expected = flow_oracle::flow400_classic_expected(classic_case);
    const auto id = flow_oracle::flow400_text(classic_case.get("case_id"));
    const auto index = [&](std::string_view id) {
        const auto found =
            std::ranges::find_if(model.nodes, [&](const auto& node) { return node.id == id; });
        flow400_require(found != model.nodes.end(), "OfficialFlow400 Classic selected node absent");
        return static_cast<std::size_t>(found - model.nodes.begin());
    };
    std::size_t rejected = 0u;
    const auto semantic = [&](auto&& mutate, std::string_view name) {
        flow_oracle::flow_typed_expect_model_rejected(good, model, work, mutate, name, kSentinel);
        ++rejected;
    };
    const auto physical = [&](auto&& mutate, std::string_view name) {
        flow_oracle::flow_typed_expect_rejected(good, model, work, mutate, name, kSentinel);
        ++rejected;
    };
    const auto extra_ink = [&](std::string_view name) {
        work.spend(4096u);
        std::optional<Point> blank;
        for (int y = 0; y < good.height && !blank; ++y)
            for (int x = 0; x < good.width; ++x) {
                work.spend(8u);
                if (good.at({x, y}) == " ") {
                    blank = Point{x, y};
                    break;
                }
            }
        flow400_require(blank.has_value(), "OfficialFlow400 Classic extra-ink blank witness");
        physical([&](auto& bad) { bad.put(*blank, "§"); }, name);
    };
    if (id == "original_13_dagre_title_top_margin_zero") {
        const auto start = index("A");
        semantic([&](auto& bad) { bad.nodes[start].shape = Shape::Circle; },
                 "Official Classic13 titleTopMargin node shape");
        semantic([&](auto& bad) { bad.nodes[start].label = "A absent"; },
                 "Official Classic13 titleTopMargin node label");
        semantic([](auto& bad) { bad.edges.front().to = bad.edges.front().from; },
                 "Official Classic13 titleTopMargin edge endpoint");
        extra_ink("Official Classic13 titleTopMargin extra ink");
    } else if (id == "original_40_diamond_fanout") {
        const auto decision = index("A");
        semantic([&](auto& bad) { bad.nodes[decision].shape = Shape::Rectangle; },
                 "Official Classic40 diamond identity");
        semantic([&](auto& bad) { bad.nodes[decision].label = "A absent"; },
                 "Official Classic40 decision label");
        semantic([](auto& bad) { bad.edges[1].to = bad.edges[1].from; },
                 "Official Classic40 fanout target");
        extra_ink("Official Classic40 extra ink");
    } else if (id == "original_41_diamond_fanout_in_subgraph") {
        const auto decision = index("A");
        semantic([&](auto& bad) { bad.nodes[decision].shape = Shape::Rectangle; },
                 "Official Classic41 diamond identity");
        semantic([&](auto& bad) { bad.nodes[decision].label = "A absent"; },
                 "Official Classic41 decision label");
        semantic([](auto& bad) { bad.edges[1].to = bad.edges[1].from; },
                 "Official Classic41 fanout target");
        semantic(
            [](auto& bad) {
                flow400_require(!bad.groups.empty() && bad.groups.front().nodes.size() == 1u,
                                "Official Classic41 subbe membership witness");
                bad.groups.front().nodes.clear();
            },
            "Official Classic41 subbe membership");
        extra_ink("Official Classic41 extra ink");
    } else if (id == "original_244_dagre_subgraphs") {
        const auto database = index("D"), client = index("A");
        semantic([&](auto& bad) { bad.nodes[database].shape = Shape::Circle; },
                 "Official Classic244 cylinder-to-circle");
        semantic([&](auto& bad) { bad.nodes[client].label = "Client absent"; },
                 "Official Classic244 Client label");
        semantic([](auto& bad) { bad.edges.front().to = bad.edges.front().from; },
                 "Official Classic244 first edge endpoint");
        semantic(
            [](auto& bad) {
                flow400_require(!bad.groups.empty() && bad.groups.front().nodes.size() == 2u,
                                "Official Classic244 Frontend witness");
                bad.groups.front().nodes.erase(bad.groups.front().nodes.begin());
            },
            "Official Classic244 Frontend membership");
        extra_ink("Official Classic244 extra ink");
    } else if (id == "original_295_classic_trapezoids") {
        const auto christmas = index("A");
        semantic([&](auto& bad) { bad.nodes[christmas].shape = Shape::TrapezoidAlt; },
                 "Official Classic295 trapezoid identity");
        semantic([&](auto& bad) { bad.nodes[christmas].label = "Christmas absent"; },
                 "Official Classic295 Christmas label");
        semantic([](auto& bad) { bad.edges.front().to = bad.edges.front().from; },
                 "Official Classic295 first edge endpoint");
        extra_ink("Official Classic295 extra ink");
    } else {
        flow400_require(id == "original_376_stadium",
                        "OfficialFlow400 Classic unknown mutation row");
        const auto start = index("A");
        semantic([&](auto& bad) { bad.nodes[start].shape = Shape::Circle; },
                 "Official Classic376 stadium identity");
        semantic([&](auto& bad) { bad.nodes[start].label = "Start absent"; },
                 "Official Classic376 Start label");
        semantic([](auto& bad) { bad.edges.front().to = bad.edges.front().from; },
                 "Official Classic376 first edge endpoint");
        extra_ink("Official Classic376 extra ink");
    }
    flow400_require(rejected == expected &&
                        expected == ((id == "original_244_dagre_subgraphs" ||
                                      id == "original_41_diamond_fanout_in_subgraph")
                                         ? 5u
                                         : 4u),
                    "OfficialFlow400 Classic mutation source catalogue");
    return rejected;
}

std::size_t official_flow_linkprofile3_mutations(const flow_oracle::Grid& good,
                                                 const flow_oracle::Model& model,
                                                 const flow_oracle::LinkProfile& profile,
                                                 JsonVal link_case, flow_oracle::OracleWork& work) {
    using flow_oracle::flow400_require;
    using flow_oracle::LinkMarker;
    using flow_oracle::Point;
    using flow_oracle::ShapeFrame;
    const flow_oracle::LinkProfileMutationScene mutations(good, model, profile, work, kSentinel);
    const auto expected = flow_oracle::flow400_linkprofile3_expected(link_case);
    const auto id = flow_oracle::flow400_text(link_case.get("case_id"));
    std::size_t rejected = 0u;
    const auto semantic = [&](auto&& mutate, std::string_view name) {
        mutations.model(mutate, name);
        ++rejected;
    };
    const auto paired_semantic = [&](auto&& mutate, std::string_view name) {
        // The expectation remains a valid closed Model/Profile. It must be
        // rejected by the actual captured Grid, never by profile schema drift.
        flow_oracle::flow400_copy_model(model, work);
        flow_oracle::flow400_linkprofile3_copy(profile, work);
        work.spend(2048u);
        auto bad_model = model;
        auto bad_profile = profile;
        mutate(bad_model, bad_profile);
        flow400_require(flow_oracle::linkprofile3_profile_shape(bad_model, bad_profile),
                        "OfficialFlow400 LinkProfile3 paired semantic Model/Profile validity");
        const auto errors =
            flow_oracle::flow_linkprofile3_geometry(good, bad_model, bad_profile, work, kSentinel);
        if (errors.empty())
            throw std::runtime_error(std::string(name) + " physical expectation admitted");
        for (const auto& error : errors)
            if (flow_oracle::flow400_resource(error))
                throw std::runtime_error(std::string(name) + " became resource credit");
        ++rejected;
    };
    const auto physical = [&](auto&& mutate, std::string_view name) {
        mutations.physical(mutate, name);
        ++rejected;
    };
    const auto arrow = [&]() {
        std::optional<Point> found;
        for (int y = 0; y < good.height && !found; ++y)
            for (int x = 0; x < good.width; ++x)
                if (flow_oracle::glyph(good.at({x, y})).arrow >= 0) {
                    found = Point{x, y};
                    break;
                }
        flow400_require(found.has_value(), "OfficialFlow400 LinkProfile3 point-marker witness");
        return *found;
    };
    const auto contours = flow_oracle::closed_frames(good, work);
    const auto extra_ink = [&](std::string_view name) {
        const auto footer = flow_oracle::flow400_following(good, work, kSentinel);
        std::optional<Point> blank;
        for (int y = 0; y < footer && !blank; ++y)
            for (int x = 0; x < good.width; ++x) {
                work.spend();
                const Point point{x, y};
                if (good.at(point) == " ") {
                    blank = point;
                    break;
                }
            }
        flow400_require(blank.has_value(), "OfficialFlow400 LinkProfile3 extra-ink witness");
        physical([&](flow_oracle::Grid& bad) { bad.put(*blank, "§"); }, name);
    };
    if (id == "link_profile_019" || id == "link_profile_291" || id == "link_profile_294") {
        constexpr std::size_t marker_edge = 4u, minimum_edge = 7u, label_edge = 5u;
        flow400_require(profile.edges.size() > minimum_edge && model.edges.size() > label_edge,
                        "OfficialFlow400 LinkProfile3 source edge catalogue");
        const int minimum_target = model.edges[minimum_edge].to;
        flow400_require(minimum_target >= 0 &&
                            static_cast<std::size_t>(minimum_target) < model.nodes.size() &&
                            std::ranges::none_of(model.edges,
                                                 [&](const flow_oracle::Edge& edge) {
                                                     return edge.from == minimum_target;
                                                 }),
                        "OfficialFlow400 LinkProfile3 minimum target leaf witness");
        paired_semantic(
            [&](flow_oracle::Model& bad_model, flow_oracle::LinkProfile& bad_profile) {
                bad_model.edges[marker_edge].start = false;
                bad_profile.edges[marker_edge].source = LinkMarker::None;
            },
            "Official LinkProfile3 point-start wrong physical expectation");
        paired_semantic(
            [&](flow_oracle::Model&, flow_oracle::LinkProfile& bad_profile) {
                ++bad_profile.edges[minimum_edge].minimum_length;
                ++bad_profile.node_ranks[static_cast<std::size_t>(minimum_target)];
            },
            "Official LinkProfile3 source minimum-length wrong physical expectation");
        semantic([&](auto& bad) { bad.edges[label_edge].label = "Label 1 absent"; },
                 "Official LinkProfile3 Label 1 semantic text");
        const auto marker = arrow();
        physical(
            [&](auto& bad) {
                const auto direction = flow_oracle::glyph(bad.at(marker)).arrow;
                bad.put(marker, (direction == 0 || direction == 2) ? "│" : "─");
            },
            "Official LinkProfile3 point marker removed");
        const auto labels = flow_oracle::flow_typed_caption_blocks(good, "Label 2", work);
        flow400_require(labels.size() == 1u, "OfficialFlow400 LinkProfile3 Label 2 physical block");
        const auto orphan =
            flow_oracle::flow_typed_orphan_label_start(good, "Label 2", contours, work);
        flow400_require(orphan.has_value(), "OfficialFlow400 LinkProfile3 Label 2 orphan witness");
        physical(
            [&](auto& bad) {
                flow_oracle::flow_typed_erase_label(bad, labels.front(), "Label 2", work);
                flow_oracle::flow_typed_write_at(bad, *orphan, "Label 2", work);
            },
            "Official LinkProfile3 Label 2 off-route caption");
        extra_ink("Official LinkProfile3 extra ink");
    } else {
        flow400_require(id == "link_profile_298" && model.edges.size() == 4u &&
                            profile.edges.size() == 4u,
                        "OfficialFlow400 LinkProfile3 open-edge catalogue");
        const int minimum_target = model.edges[0u].to;
        flow400_require(minimum_target >= 0 &&
                            static_cast<std::size_t>(minimum_target) < model.nodes.size() &&
                            std::ranges::none_of(model.edges,
                                                 [&](const flow_oracle::Edge& edge) {
                                                     return edge.from == minimum_target;
                                                 }),
                        "OfficialFlow400 LinkProfile3 open minimum target leaf witness");
        semantic([](auto& bad) { bad.edges[1u].label = "invented"; },
                 "Official LinkProfile3 C-to-D invented label");
        paired_semantic(
            [&](flow_oracle::Model&, flow_oracle::LinkProfile& bad_profile) {
                ++bad_profile.edges[0u].minimum_length;
                ++bad_profile.node_ranks[static_cast<std::size_t>(minimum_target)];
            },
            "Official LinkProfile3 open source minimum-length wrong physical expectation");
        const auto marker = arrow();
        physical(
            [&](auto& bad) {
                const auto direction = flow_oracle::glyph(bad.at(marker)).arrow;
                bad.put(marker, (direction == 0 || direction == 2) ? "│" : "─");
            },
            "Official LinkProfile3 bidirectional marker removed");
        const auto node =
            std::ranges::find_if(model.nodes, [](const auto& value) { return value.id == "A"; });
        flow400_require(node != model.nodes.end(), "OfficialFlow400 LinkProfile3 A node");
        const auto source_frame =
            flow_oracle::flow_typed_frame_for_node(good, contours, *node, work);
        flow400_require(source_frame.has_value(), "OfficialFlow400 LinkProfile3 A contour");
        const Point port{(source_frame->left + source_frame->right + 1) / 2, source_frame->bottom};
        flow400_require(good.at(port) == "┬",
                        "OfficialFlow400 LinkProfile3 open source contour-arm witness");
        physical([&](auto& bad) { bad.put(port, "│"); },
                 "Official LinkProfile3 open source contour arms");
        const auto labels = flow_oracle::flow_typed_caption_blocks(good, "bidirectional", work);
        flow400_require(labels.size() == 1u, "OfficialFlow400 LinkProfile3 bidirectional caption");
        const auto orphan =
            flow_oracle::flow_typed_orphan_label_start(good, "bidirectional", contours, work);
        flow400_require(orphan.has_value(), "OfficialFlow400 LinkProfile3 spare caption witness");
        physical(
            [&](auto& bad) {
                flow_oracle::flow_typed_write_at(bad, *orphan, "bidirectional", work);
            },
            "Official LinkProfile3 spare caption");
        extra_ink("Official LinkProfile3 extra ink");
    }
    flow400_require(rejected == expected && expected == 6u,
                    "OfficialFlow400 LinkProfile3 source mutation catalogue");
    return rejected;
}

void official_flow_publish_grid(json::JsonMutVal parent, std::string_view key,
                                const flow_oracle::Grid& grid, flow_oracle::OracleWork& work) {
    flow_oracle::flow400_require(grid.width > 0 && grid.width <= 240 && grid.height > 0 &&
                                     grid.height <= 1040 &&
                                     grid.cells.size() == static_cast<std::size_t>(grid.width) *
                                                              static_cast<std::size_t>(grid.height),
                                 "OfficialFlow400 publication Grid bounds");
    work.spend(4u * sizeof(json::JsonMutDoc) + 8u * sizeof(json::JsonMutVal) + 512u);
    work.product(grid.cells.size(), 8u);
    std::size_t payload = 0;
    for (const auto& cell : grid.cells) {
        flow_oracle::flow400_require(cell.size() <=
                                         std::numeric_limits<std::size_t>::max() - payload,
                                     "OfficialFlow400 resource payload overflow");
        payload += cell.size();
    }
    // Full raw cells, including empty wide-cell continuations, remain evidence.
    // JSON node/string storage, copies, escaping, serialization/hash/write shares
    // are all paid before building the object, not after geometry can exhaust.
    work.product(grid.cells.size(), 4u * sizeof(json::JsonMutVal) + 4u * sizeof(std::string) + 96u);
    work.product(payload, 64u);
    work.product(static_cast<std::size_t>(grid.height), 4u * sizeof(json::JsonMutVal) + 64u);
    auto object = parent.make_obj();
    object.set("width", grid.width);
    object.set("height", grid.height);
    auto rows = object.make_arr();
    for (int y = 0; y < grid.height; ++y) {
        auto row = rows.make_arr();
        for (int x = 0; x < grid.width; ++x)
            row.append(row.make_str(grid.at({x, y})));
        rows.append(row);
    }
    object.add("cells", rows);
    parent.add(key, object);
}

std::size_t
official_flow_geometry(const OfficialFlowFrames& frames, const flow_oracle::Model& model,
                       flow_oracle::OracleWork& work, Observation& row, OfficialFlowProgress& p,
                       JsonVal typed_source4_case = JsonVal{}, JsonVal classic_case = JsonVal{},
                       JsonVal linkprofile3_case = JsonVal{},
                       const flow_oracle::LinkProfile* linkprofile3_profile = nullptr,
                       JsonVal group_endpoint15_case = JsonVal{},
                       const flow_oracle::GroupEndpoint15Model* group_endpoint15_model = nullptr,
                       std::string_view diagram_title = {}) {
    using flow_oracle::flow400_require;
    p.phase = "compare";
    p.restored = flow_oracle::flow400_same(*frames[0], *frames[2], work);
    const bool wide_source = flow_oracle::flow400_same(*frames[0], *frames[3], work);
    const bool narrow_source = flow_oracle::flow400_same(*frames[1], *frames[4], work);
    p.remaining[3] = work.remaining;
    auto drawn = [&](std::size_t index, const char* phase) {
        p.phase = phase;
        const auto errors =
            !diagram_title.empty()   ? flow_oracle::flow_title_geometry(*frames[index], model, work,
                                                                        diagram_title, kSentinel)
            : group_endpoint15_model ? flow_oracle::group_endpoint15_geometry(
                                           *frames[index], *group_endpoint15_model, work, kSentinel)
            : linkprofile3_profile
                ? flow_oracle::flow_linkprofile3_geometry(*frames[index], model,
                                                          *linkprofile3_profile, work, kSentinel)
                : flow_oracle::flow400_geometry(*frames[index], model, work, kSentinel);
        for (const auto& error : errors) {
            work.product(error.size(), 8u);
            work.spend(16u);
            if (flow_oracle::flow400_resource(error)) p.resource = true;
        }
        if (!errors.empty()) {
            official_flow_error(row, errors.front());
            return false;
        }
        return true;
    };
    if (wide_source)
        official_flow_error(row, "Complete mapped Flow original still uses ordinary source at240");
    else if (drawn(0u, "wide"))
        row.status[0] = "drawn";
    if (narrow_source) {
        p.phase = "narrow_source";
        static_cast<void>(flow_oracle::flow400_following(*frames[1], work, kSentinel));
        p.narrow_ordinary = true;
        row.status[1] = "source_fallback";
    } else if (drawn(1u, "narrow"))
        row.status[1] = "drawn";
    p.remaining[4] = work.remaining;
    if (!wide_source && drawn(2u, "restored") && p.restored) row.status[2] = "drawn";
    if (!p.restored)
        official_flow_error(row, "Same Flow Node240/40/240 changed physical restored cells");
    p.remaining[5] = work.remaining;
    std::size_t typed_rejections = 0u;
    if (row.errors.empty() && row.status[0] == "drawn" && row.status[2] == "drawn" &&
        (row.status[1] == "drawn" || row.status[1] == "source_fallback")) {
        p.phase = "mutations";
        if (group_endpoint15_case.is_obj()) {
            flow400_require(group_endpoint15_model != nullptr,
                            "OfficialFlow400 GroupEndpoint15 decoded profile missing");
            typed_rejections = official_flow_group_endpoint15_mutations(
                *frames[0], *group_endpoint15_model, group_endpoint15_case, work);
        } else if (linkprofile3_case.is_obj()) {
            flow400_require(linkprofile3_profile != nullptr,
                            "OfficialFlow400 LinkProfile3 decoded profile missing");
            typed_rejections = official_flow_linkprofile3_mutations(
                *frames[0], model, *linkprofile3_profile, linkprofile3_case, work);
        } else if (typed_source4_case.is_obj())
            typed_rejections =
                official_flow_typed_source4_mutations(*frames[0], model, typed_source4_case, work);
        else if (classic_case.is_obj())
            typed_rejections =
                official_flow_classic_mutations(*frames[0], model, classic_case, work);
        else if (!diagram_title.empty())
            typed_rejections =
                flow_oracle::flow_title_mutations(*frames[0], model, work, kSentinel);
        else {
            typed_rejections = flow_oracle::flow400_mutations(*frames[0], model, work, kSentinel);
            flow400_require(typed_rejections > 0u,
                            "OfficialFlow400 missing meaningful semantic/physical mutants");
        }
        p.mutated = true;
    }
    p.remaining[6] = work.remaining;
    p.admitted =
        p.captured && p.bound && p.decoded && !p.pending && !p.resource && p.restored &&
        p.mutated && row.errors.empty() && row.status[0] == "drawn" && row.status[2] == "drawn" &&
        (row.status[1] == "drawn" || (row.status[1] == "source_fallback" && p.narrow_ordinary));
    return typed_rejections;
}

Observation run_official_flow400_case(JsonVal fixture, std::size_t global, std::size_t ordinal,
                                      std::string_view source,
                                      const flow_oracle::Flow400Documents& docs,
                                      flow_oracle::OracleWork& work, std::ostream& diagnostics,
                                      std::size_t observation_size,
                                      std::size_t observation_capacity, JsonVal config21,
                                      JsonVal typed_source4, JsonVal classic, JsonVal linkprofile3,
                                      JsonVal group_endpoint15) {
    // Source/fence/path acquisition and fixed error/report reserves precede the
    // caller's actual read. This function uses that same scene ledger throughout.
    Observation row;
    row.errors.reserve(6u);
    OfficialFlowProgress p;
    OfficialFlowFrames frames;
    row.raw_active_schema = kOfficialFlow400Contract;
    row.track = "original";
    row.type = "flowchart";
    row.expected = "drawn";
    row.expected_states = {"drawn", "drawn_or_exact_source", "drawn"};
    row.reason = "Complete original Flow terminal whole-scene contract; foreign assertions pending";
    row.decision_reasons.fill(
        "Complete Flow geometry, strict independent following and restoration");
    row.decision_reasons[1] =
        "Predeclared40-column complete geometry OR exact ordinary full source";
    row.role_case_index = ordinal;
    row.raw_source_authority = "immutable_Flow400_whole_8Models_392null";
    const bool diagram_title_selected = ordinal == flow_oracle::kFlowTitleOrdinal;
    const bool title_margin_selected = ordinal == flow_oracle::kFlowTitleMarginOrdinal;
    const bool shape_alias_selected = flow_oracle::flow_shape_alias_case(ordinal) != nullptr;
    const bool issue4648_selected = ordinal == flow_oracle::kFlow400Issue4648Ordinal;
    JsonVal typed_source4_case, classic_case, linkprofile3_case, group_endpoint15_case;
    std::size_t typed_expected = 0u, typed_rejected = 0u;
    bool typed_source4_selected = false, classic_selected = false, linkprofile3_selected = false,
         group_endpoint15_selected = false;
    // Foreign manifest provenance is not Model binding. The caller's fixed
    // reserve pays it before captures can exhaust this source's scene ledger.
    row.projection_baseline = projection_canonical(fixture.get("raw"));
    require(row.projection_baseline.size() <= 512u, "OfficialFlow400 foreign baseline bound");
    json::JsonMutDoc diagnostic;
    auto data = diagnostic.object();
    try {
        work.spend(8192u + 8u * sizeof(std::size_t));
        require(observation_size <= 2738u && observation_capacity <= 8192u,
                "OfficialFlow400 observation vector bound");
        static_assert(std::is_nothrow_move_constructible_v<Observation>);
        const auto slots = observation_size == observation_capacity
                               ? std::max(2u * observation_capacity, observation_size + 1u)
                               : 0u;
        work.product(slots + (slots ? observation_size : 0u) + 1u, 2u * sizeof(Observation) + 16u);
        const auto path = flow_oracle::flow400_text(fixture.get("path"));
        row.path = path;
        require(path.starts_with("flowchart/") && safe_relative(path) && path.ends_with(".mmd"),
                "OfficialFlow400 original path");
        require(ordinal < 400u && source.size() <= 65536u,
                "OfficialFlow400 complete original bounds");
        if (diagram_title_selected)
            work.spend(65536u + 16u * sizeof(json::JsonVal) + 16u * sizeof(flow_oracle::Model) +
                       32u * sizeof(flow_oracle::ShapeFrame) + 1024u);
        if (title_margin_selected)
            work.spend(65536u + 16u * sizeof(json::JsonVal) + 16u * sizeof(flow_oracle::Model) +
                       32u * sizeof(flow_oracle::ShapeFrame) + 1024u);
        if (shape_alias_selected)
            work.spend(65536u + 16u * sizeof(json::JsonVal) + 16u * sizeof(flow_oracle::Model) +
                       32u * sizeof(flow_oracle::ShapeFrame) + 1024u);
        if (issue4648_selected)
            work.spend(65536u + 16u * sizeof(json::JsonVal) + 16u * sizeof(flow_oracle::Model) +
                       32u * sizeof(flow_oracle::ShapeFrame) + 1024u);
        const bool typed_source4_candidate =
            typed_source4.is_obj() &&
            (ordinal == 0u || ordinal == 282u || ordinal == 374u || ordinal == 375u);
        // Source4's selector scan, typed Model, two metadata scalars, clone ledger,
        // and report nodes are paid before any of this source's five captures.
        if (typed_source4_candidate)
            work.spend(65536u + 16u * sizeof(json::JsonVal) + 16u * sizeof(flow_oracle::Model) +
                       32u * sizeof(flow_oracle::ShapeFrame) + 1024u);
        // The closed five-row Classic selector always reserves its bounded scan;
        // the selected source additionally prepays Model, clone, and metadata work.
        work.spend(8192u + 8u * sizeof(std::size_t) + 4u * sizeof(json::JsonVal) + 5u * 512u);
        if (classic.is_obj() && (ordinal == 13u || ordinal == 40u || ordinal == 244u ||
                                 ordinal == 295u || ordinal == 376u))
            work.spend(65536u + 16u * sizeof(json::JsonVal) + 16u * sizeof(flow_oracle::Model) +
                       32u * sizeof(flow_oracle::ShapeFrame) + 1024u);
        // LinkProfile3 always reserves its closed four-row scan. The selected
        // full-source rows prepay their Model/Profile and report lifecycle.
        work.spend(8192u + 8u * sizeof(std::size_t) + 4u * sizeof(json::JsonVal) + 4u * 512u);
        if (linkprofile3.is_obj() &&
            (ordinal == 19u || ordinal == 291u || ordinal == 294u || ordinal == 298u))
            work.spend(65536u + 16u * sizeof(json::JsonVal) + 16u * sizeof(flow_oracle::Model) +
                       16u * sizeof(flow_oracle::LinkProfile) +
                       32u * sizeof(flow_oracle::ShapeFrame) + 1024u);
        if (group_endpoint15.is_obj() && flow_oracle::group_endpoint15_candidate_ordinal(ordinal))
            work.spend(65536u + 16u * sizeof(json::JsonVal) +
                       16u * sizeof(flow_oracle::GroupEndpoint15Model) +
                       32u * sizeof(flow_oracle::ShapeFrame) + 1024u);
        data.set("kind", "official_Flow400_actual_scene/v1");
        data.set("ordinal", static_cast<int64_t>(ordinal));
        data.set("manifest_global_ordinal", static_cast<int64_t>(global));
        data.set("fixture", path);
        p.phase = "capture";
        auto element = loom::ui::render_markdown(fenced(source, "mermaid", kSentinel));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text", kSentinel));
        element->ComputeRequirement();
        ordinary->ComputeRequirement();
        p.height = std::max({1, element->requirement().min_y, ordinary->requirement().min_y}) + 1;
        require(p.height <= 1040, "OfficialFlow400 complete requirement exceeds capture bounds");
        constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
        for (std::size_t n = 0; n < frames.size(); ++n) {
            frames[n] =
                flow_oracle::capture(n < 3u ? element : ordinary, widths[n], p.height, &work);
            ++p.captures;
        }
        p.captured = true;
        p.remaining[0] = work.remaining;
        p.phase = "publication";
        constexpr std::array<std::string_view, 5> names{
            {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
        for (std::size_t n = 0; n < frames.size(); ++n)
            official_flow_publish_grid(data, names[n], *frames[n], work);
        p.remaining[1] = work.remaining;
        p.phase = "bind";
        const auto base = docs.compact.root().get("cases").at(ordinal);
        auto authority = base;
        std::optional<flow_oracle::BoundFlow400TypedSource4Authority> typed_selection;
        if (typed_source4.is_obj()) {
            typed_selection.emplace(flow_oracle::flow400_typed_source4_bound_authority(
                base, config21, typed_source4, fixture, ordinal, global, source, work));
            authority = typed_selection->authority;
            if (classic.is_obj())
                classic_case = flow_oracle::flow400_classic_select(
                    *typed_selection, classic, fixture, ordinal, global, source, work);
            if (linkprofile3.is_obj() && !classic_case.is_obj())
                linkprofile3_case = flow_oracle::flow400_linkprofile3_select(
                    *typed_selection, classic_case, linkprofile3, fixture, ordinal, global, source,
                    work);
            if (linkprofile3_case.is_obj()) {
                linkprofile3_selected = true;
                typed_expected = flow_oracle::flow400_linkprofile3_expected(linkprofile3_case);
                authority = linkprofile3_case.get("base_authority");
                row.raw_source_authority = "immutable_Flow400_8Models_plus_LinkProfile3";
                row.role_model_sha =
                    flow_oracle::flow400_text(linkprofile3_case.get("link_model_sha256"));
                data.set("source_supplement_sha256", flow_oracle::kFlow400LinkProfile3AssetSha);
                data.set("link_profile_sha256",
                         flow_oracle::flow400_text(linkprofile3_case.get("link_profile_sha256")));
            } else if (classic_case.is_obj()) {
                classic_selected = true;
                typed_expected = flow_oracle::flow400_classic_expected(classic_case);
                authority = classic_case.get("base_authority");
                row.raw_source_authority = "immutable_Flow400_8Models_plus_Classic";
                row.role_model_sha =
                    flow_oracle::flow400_text(classic_case.get("classic_model_sha256"));
                data.set("source_supplement_sha256", flow_oracle::kFlow400ClassicAssetSha);
            } else if (typed_selection->kind ==
                       flow_oracle::Flow400TypedSource4Kind::TypedSource4) {
                typed_source4_selected = true;
                typed_source4_case = typed_selection->typed_case;
                typed_expected = typed_selection->exact_rejections;
                row.raw_source_authority = "immutable_Flow400_8Models_plus_TypedSource4";
                // This is a source-pinned Model identity after selection, not a
                // decode claim. whole_Model_decoded remains false on a decoder failure.
                row.role_model_sha =
                    flow_oracle::flow400_text(typed_source4_case.get("typed_model_sha256"));
                data.set("source_supplement_sha256", flow_oracle::kFlow400TypedSource4AssetSha);
            } else if (typed_selection->kind == flow_oracle::Flow400TypedSource4Kind::Config21) {
                row.raw_source_authority = "immutable_Flow400_8Models_plus_Config21";
                data.set("source_supplement_sha256", flow_oracle::kFlow400Config21AssetSha);
            }
        } else if (config21.is_obj()) {
            const auto selected = flow_oracle::flow400_config21_bound_authority(
                base, config21, fixture, ordinal, global, source, work);
            authority = selected.row;
            if (selected.supplemental) {
                row.raw_source_authority = "immutable_Flow400_8Models_plus_Config21";
                data.set("source_supplement_sha256", flow_oracle::kFlow400Config21AssetSha);
            }
        } else {
            work.spend(4096u + 4u * sizeof(json::JsonVal) + 8u * sizeof(std::size_t));
            flow_oracle::flow400_bind(base, fixture, ordinal, global, source, work);
        }
        if (diagram_title_selected) {
            flow_oracle::flow_title_bind(fixture, ordinal, global, source, work);
            row.raw_source_authority = "immutable_Flow400_8Models_plus_DiagramTitle1";
            row.role_model_sha = std::string(flow_oracle::kFlowTitleModelSha);
        }
        if (title_margin_selected) {
            flow_oracle::flow_title_margin_bind(fixture, ordinal, global, source, work);
            row.raw_source_authority = "immutable_Flow400_8Models_plus_TitleTopMargin1";
            row.role_model_sha = std::string(flow_oracle::kFlowTitleMarginModelSha);
        }
        if (shape_alias_selected) {
            flow_oracle::flow_shape_alias_bind(docs, fixture, ordinal, global, source, work);
            row.raw_source_authority = "immutable_Flow400_8Models_plus_FlowShapeAlias16";
            row.role_model_sha =
                std::string(flow_oracle::flow_shape_alias_case(ordinal)->model_sha);
        }
        if (issue4648_selected) {
            flow_oracle::flow400_issue4648_bind(docs, source, work);
            row.raw_source_authority = "immutable_Flow400_8Models_plus_Issue4648";
            row.role_model_sha = std::string(flow_oracle::kFlow400Issue4648ModelSha);
        }
        std::optional<flow_oracle::BoundGroupEndpoint15> group_selection;
        if (group_endpoint15.is_obj() && flow_oracle::group_endpoint15_candidate_ordinal(ordinal)) {
            group_selection = flow_oracle::group_endpoint15_select(
                base, fixture, group_endpoint15.get("records"), ordinal, global, source, work);
            require(group_selection.has_value(),
                    "OfficialFlow400 GroupEndpoint15 selected tuple missing");
            group_endpoint15_selected = true;
            group_endpoint15_case = group_selection->record;
            authority = group_selection->authority;
            typed_expected =
                flow_oracle::group_endpoint15_expected_mutations(group_endpoint15_case);
            row.raw_source_authority = "immutable_Flow400_8Models_plus_GroupEndpoint15";
            row.role_model_sha =
                flow_oracle::flow400_text(group_endpoint15_case.get("canonical_model_sha256"));
            data.set("source_supplement_sha256", flow_oracle::kFlow400GroupEndpoint15AssetSha);
        }
        p.bound = true;
        work.product(source.size(), 4u);
        work.spend(1024u);
        row.source_sha256 = digest(source);
        require(source.find(kSentinel) == std::string_view::npos,
                "OfficialFlow400 finite source token collision needs separate role review");
        data.set("source_sha256", row.source_sha256);
        data.set("source_git_blob", flow_oracle::flow400_text(authority.get("source_git_blob")));
        data.set("oracle_record_sha256",
                 flow_oracle::flow400_text(authority.get("oracle_record_sha256")));
        p.phase = "decode";
        std::optional<flow_oracle::Model> model;
        std::optional<flow_oracle::LinkProfile> linkprofile3_profile;
        std::optional<flow_oracle::GroupEndpoint15Model> group_endpoint15_model;
        if (diagram_title_selected)
            model.emplace(flow_oracle::flow_title_model(work));
        else if (title_margin_selected)
            model.emplace(flow_oracle::flow_title_margin_model(work));
        else if (shape_alias_selected)
            model.emplace(flow_oracle::flow_shape_alias_model(ordinal, work));
        else if (issue4648_selected)
            model.emplace(flow_oracle::flow400_issue4648_model(work));
        else if (group_endpoint15_selected) {
            group_endpoint15_model.emplace(
                flow_oracle::group_endpoint15_model(group_endpoint15_case.get("model"), work));
            model.emplace(group_endpoint15_model->base);
        } else if (linkprofile3_selected) {
            auto decoded = flow_oracle::flow400_linkprofile3_model(linkprofile3_case, work);
            model.emplace(std::move(decoded.model));
            linkprofile3_profile.emplace(std::move(decoded.profile));
        } else if (classic_selected)
            model.emplace(flow_oracle::flow400_classic_model(classic_case, work));
        else if (typed_source4_selected)
            model = flow_oracle::flow400_typed_source4_model(*typed_selection, work);
        else
            model = flow_oracle::flow400_model(authority, work);
        p.remaining[2] = work.remaining;
        if (!model) {
            p.pending = true;
            row.expected = "pending";
            row.expected_states.fill("unadmitted_whole_source");
            row.reason = "Complete Flow original profile remains whole-null; all "
                         "source/config/foreign debts retained";
            p.phase = "pending_physical";
            for (const auto& frame : frames)
                static_cast<void>(flow_oracle::flow400_following(*frame, work, kSentinel));
            p.restored = flow_oracle::flow400_same(*frames[0], *frames[2], work);
            const bool wide_source = flow_oracle::flow400_same(*frames[0], *frames[3], work);
            p.narrow_ordinary = flow_oracle::flow400_same(*frames[1], *frames[4], work);
            if (wide_source) row.status[0] = "source_fallback";
            if (p.narrow_ordinary) row.status[1] = "source_fallback";
            if (wide_source && p.restored) row.status[2] = "source_fallback";
            if (!wide_source || !p.narrow_ordinary || !p.restored)
                official_flow_error(
                    row, "Whole-null Flow has unreviewed art or failed ordinary/restore protocol");
        } else {
            p.decoded = true;
            work.spend(1024u);
            if (!typed_source4_selected && !classic_selected && !linkprofile3_selected &&
                !group_endpoint15_selected && !diagram_title_selected && !title_margin_selected &&
                !shape_alias_selected && !issue4648_selected)
                row.role_model_sha = flow_oracle::flow400_text(authority.get("model_sha256"));
            data.set("whole_Model_sha256", row.role_model_sha);
            typed_rejected = official_flow_geometry(
                frames, *model, work, row, p, typed_source4_case, classic_case, linkprofile3_case,
                linkprofile3_profile ? &*linkprofile3_profile : nullptr, group_endpoint15_case,
                group_endpoint15_model ? &*group_endpoint15_model : nullptr,
                diagram_title_selected ? flow_oracle::kFlowTitleText : std::string_view{});
            if (shape_alias_selected && typed_rejected != 2u) {
                official_flow_error(
                    row, "OfficialFlow400 shape alias requires exactly two rejected mutations");
                p.mutated = false;
                p.admitted = false;
            }
            if (typed_source4_selected) {
                data.set("typed_mutations_expected", static_cast<int64_t>(typed_expected));
                data.set("typed_mutations_rejected", static_cast<int64_t>(typed_rejected));
            }
            if (classic_selected) {
                data.set("classic_mutations_expected", static_cast<int64_t>(typed_expected));
                data.set("classic_mutations_rejected", static_cast<int64_t>(typed_rejected));
            }
            if (linkprofile3_selected) {
                data.set("linkprofile3_mutations_expected", static_cast<int64_t>(typed_expected));
                data.set("linkprofile3_mutations_rejected", static_cast<int64_t>(typed_rejected));
            }
            if (group_endpoint15_selected) {
                data.set("group_endpoint15_mutations_expected",
                         static_cast<int64_t>(typed_expected));
                data.set("group_endpoint15_mutations_rejected",
                         static_cast<int64_t>(typed_rejected));
            }
        }
        p.phase = "complete";
    } catch (const std::exception& error) {
        const char* text = error.what();
        std::size_t n = 0;
        if (text)
            while (n < 128u && text[n])
                ++n;
        const std::string_view bounded(text ? text : "", n);
        p.resource = p.resource || work.remaining == 0u || flow_oracle::flow400_resource(bounded);
        official_flow_error(row, bounded);
        p.admitted = false;
    }
    row.raw_source_constraints =
        group_endpoint15_selected ? official_flow_group_endpoint15_info(
                                        p, work.remaining, typed_expected, typed_rejected, global)
        : linkprofile3_selected
            ? official_flow_linkprofile3_info(p, work.remaining, typed_expected, typed_rejected)
        : classic_selected
            ? official_flow_classic_info(p, work.remaining, typed_expected, typed_rejected)
        : typed_source4_selected
            ? official_flow_typed_source4_info(p, work.remaining, typed_expected, typed_rejected)
            : official_flow_info(
                  p, work.remaining,
                  row.raw_source_authority == "immutable_Flow400_8Models_plus_Config21",
                  diagram_title_selected, typed_rejected, shape_alias_selected, typed_rejected);
    data.set("contract", kOfficialFlow400Contract);
    data.set("actual", row.raw_source_constraints);
    data.set("foreign_assertions_certified", 0);
    diagnostic.set_root(data);
    diagnostics << diagnostic.to_string() << '\n';
    require(static_cast<bool>(diagnostics), "OfficialFlow400 actual evidence write failed");
    return row;
}

void official_flow_accumulate_owned(JsonVal info, const Observation& row,
                                    OfficialFlowCounts& counts, JsonVal typed_source4,
                                    JsonVal classic, JsonVal linkprofile3,
                                    JsonVal group_endpoint15) {
    const bool typed = row.raw_source_authority == "immutable_Flow400_8Models_plus_TypedSource4";
    const bool classic_selected =
        row.raw_source_authority == "immutable_Flow400_8Models_plus_Classic";
    const bool link_selected =
        row.raw_source_authority == "immutable_Flow400_8Models_plus_LinkProfile3";
    const bool group_selected =
        row.raw_source_authority == "immutable_Flow400_8Models_plus_GroupEndpoint15";
    const bool title_selected =
        row.raw_source_authority == "immutable_Flow400_8Models_plus_DiagramTitle1";
    const bool title_margin_selected =
        row.raw_source_authority == "immutable_Flow400_8Models_plus_TitleTopMargin1";
    const bool shape_alias_selected =
        row.raw_source_authority == "immutable_Flow400_8Models_plus_FlowShapeAlias16";
    const bool issue4648_selected =
        row.raw_source_authority == "immutable_Flow400_8Models_plus_Issue4648";
    if (group_selected)
        official_flow_group_endpoint15_keys(info);
    else if (link_selected)
        official_flow_linkprofile3_keys(info);
    else if (typed)
        official_flow_typed_source4_keys(info);
    else if (classic_selected)
        official_flow_classic_keys(info);
    else if (shape_alias_selected)
        flow_oracle::flow400_keys(info, {"five_captures_complete", "source_bound",
                                         "whole_Model_decoded", "whole_null_pending",
                                         "whole_source_admitted", "restored_cells_equal",
                                         "narrow_ordinary", "semantic_mutations_complete",
                                         "verification_resource_exhausted", "captured_frames",
                                         "phase", "remaining", "height", "phase_remaining",
                                         "source_supplement_sha256", "shape_alias_mutations"});
    else if (title_selected)
        flow_oracle::flow400_keys(
            info,
            {"five_captures_complete", "source_bound", "whole_Model_decoded", "whole_null_pending",
             "whole_source_admitted", "restored_cells_equal", "narrow_ordinary",
             "semantic_mutations_complete", "verification_resource_exhausted", "captured_frames",
             "phase", "remaining", "height", "phase_remaining", "source_supplement_sha256",
             "diagram_title_mutations_rejected"});
    else
        flow_oracle::flow400_keys(
            info,
            {"five_captures_complete", "source_bound", "whole_Model_decoded", "whole_null_pending",
             "whole_source_admitted", "restored_cells_equal", "narrow_ordinary",
             "semantic_mutations_complete", "verification_resource_exhausted", "captured_frames",
             "phase", "remaining", "height", "phase_remaining", "source_supplement_sha256"});
    auto yes = [&](std::string_view key) {
        const auto value = info.get(key);
        require(value.is_bool(), "OfficialFlow400 owned bool type");
        return value.as_bool();
    };
    const bool captured = yes("five_captures_complete"), bound = yes("source_bound"),
               decoded = yes("whole_Model_decoded"), pending = yes("whole_null_pending");
    const bool admitted = yes("whole_source_admitted"), restored = yes("restored_cells_equal"),
               ordinary = yes("narrow_ordinary"), mutated = yes("semantic_mutations_complete"),
               resource = yes("verification_resource_exhausted");
    const auto capture_count = flow_oracle::flow400_number(info.get("captured_frames"));
    require(capture_count <= 5u && (!captured || capture_count == 5u),
            "OfficialFlow400 actual five-frame proof");
    require(!(decoded && pending), "OfficialFlow400 decoded/null exclusivity");
    const auto phase = info.get("phase");
    require(phase.is_str() && phase.as_str().size() <= 32u, "OfficialFlow400 owned phase");
    const auto remaining = flow_oracle::flow400_number(info.get("remaining"));
    require(remaining <= flow_oracle::kFlow400SceneLimit, "OfficialFlow400 remaining bound");
    require(info.get("height").is_int() && info.get("height").as_int() >= 0 &&
                info.get("height").as_int() <= 1040,
            "OfficialFlow400 height bound");
    const auto costs = info.get("phase_remaining");
    require(costs.is_arr() && costs.size() == 7u, "OfficialFlow400 phase ledger shape");
    if (typed || classic_selected || link_selected || group_selected) {
        const auto observed = official_flow_typed_source4_phase_checkpoints(phase.as_str());
        std::size_t prior = flow_oracle::kFlow400SceneLimit, last = flow_oracle::kFlow400SceneLimit;
        for (std::size_t index = 0u; index < costs.size(); ++index) {
            const auto value = costs.at(index);
            if (index >= observed) {
                require(value.is_null(),
                        typed ? "OfficialFlow400 TypedSource4 unobserved phase ledger"
                        : link_selected
                            ? "OfficialFlow400 LinkProfile3 unobserved phase ledger"
                            : "OfficialFlow400 selected source unobserved phase ledger");
                continue;
            }
            require(value.is_int() && value.as_int() >= 0 &&
                        static_cast<std::size_t>(value.as_int()) <= prior,
                    group_selected  ? "OfficialFlow400 GroupEndpoint15 monotonic phase ledger"
                    : typed         ? "OfficialFlow400 TypedSource4 monotonic phase ledger"
                    : link_selected ? "OfficialFlow400 LinkProfile3 monotonic phase ledger"
                                    : "OfficialFlow400 selected source monotonic phase ledger");
            prior = static_cast<std::size_t>(value.as_int());
            last = prior;
        }
        if (admitted)
            require(observed == costs.size() && last == remaining && phase.as_str() == "complete",
                    group_selected  ? "OfficialFlow400 GroupEndpoint15 admitted phase/work ledger"
                    : typed         ? "OfficialFlow400 TypedSource4 admitted phase/work ledger"
                    : link_selected ? "OfficialFlow400 LinkProfile3 admitted phase/work ledger"
                                    : "OfficialFlow400 selected source admitted phase/work ledger");
        else if (observed)
            require(remaining <= last,
                    group_selected  ? "OfficialFlow400 GroupEndpoint15 failed phase/work ledger"
                    : typed         ? "OfficialFlow400 TypedSource4 failed phase/work ledger"
                    : link_selected ? "OfficialFlow400 LinkProfile3 failed phase/work ledger"
                                    : "OfficialFlow400 selected source failed row ledger");
    } else
        costs.iter([&](JsonVal n) {
            require(flow_oracle::flow400_number(n) <= flow_oracle::kFlow400SceneLimit,
                    "OfficialFlow400 phase ledger bound");
        });
    if (admitted)
        require(captured && bound && decoded && !pending && restored && mutated && !resource &&
                    row.role_model_sha.size() == 64u && row.errors.empty() &&
                    row.status[0] == "drawn" && row.status[2] == "drawn" &&
                    (row.status[1] == "drawn" || (row.status[1] == "source_fallback" && ordinary)),
                "OfficialFlow400 per-state proof cannot bypass whole-source admission");
    const bool selected = row.raw_source_authority == "immutable_Flow400_8Models_plus_Config21";
    bool same_run_body = false;
    if (row.raw_active_schema == kFlowRepeatBody2Contract &&
        row.raw_source_authority == "same_run_original_Flow400_offset0" &&
        row.track == "body_diagnostic" && row.type == "flowchart" &&
        row.role_case_index < kFlowRepeatBody2Pins.size()) {
        const auto& pin = kFlowRepeatBody2Pins[row.role_case_index];
        same_run_body = row.path == pin.path && row.source_sha256 == pin.source_sha &&
                        row.role_model_sha == pin.model_sha && row.role_body_sha == pin.source_sha;
    }
    require(
        selected || typed || classic_selected || link_selected || group_selected ||
            title_selected || title_margin_selected || shape_alias_selected || issue4648_selected ||
            row.raw_source_authority == "immutable_Flow400_whole_8Models_392null" || same_run_body,
        "OfficialFlow400 unknown source authority");
    if (shape_alias_selected) {
        const auto* candidate = flow_oracle::flow_shape_alias_case(row.role_case_index);
        require(candidate && row.path == candidate->fixture &&
                    row.source_sha256 == candidate->source_sha &&
                    row.role_model_sha == candidate->model_sha,
                "OfficialFlow400 shape-alias source/model authority");
        const auto mutations = info.get("shape_alias_mutations");
        flow_oracle::flow400_keys(mutations, {"expected", "rejected"});
        const auto expected = flow_oracle::flow400_number(mutations.get("expected"));
        const auto rejected = flow_oracle::flow400_number(mutations.get("rejected"));
        require(expected == 2u && rejected <= expected,
                "OfficialFlow400 shape-alias mutation count bounds");
        if (admitted || mutated)
            require(admitted && mutated && rejected == expected && row.errors.empty() && !resource,
                    "OfficialFlow400 exact shape-alias mutation proof");
        else
            require(!admitted && !mutated && (!row.errors.empty() || resource),
                    "OfficialFlow400 failed shape-alias row requires diagnostics");
    }
    if (issue4648_selected)
        require(row.path == flow_oracle::kFlow400Issue4648Fixture &&
                    row.source_sha256 == flow_oracle::kFlow400Issue4648SourceSha &&
                    row.role_model_sha == flow_oracle::kFlow400Issue4648ModelSha,
                "OfficialFlow400 Issue4648 source/model authority");
    if (title_selected) {
        require(row.path == flow_oracle::kFlowTitleFixture &&
                    row.source_sha256 == flow_oracle::kFlowTitleSourceSha &&
                    row.role_model_sha == flow_oracle::kFlowTitleModelSha,
                "OfficialFlow400 diagram title source/model authority");
        const auto rejected =
            flow_oracle::flow400_number(info.get("diagram_title_mutations_rejected"));
        require(rejected <= 4u, "OfficialFlow400 diagram title mutation bound");
        if (admitted || mutated)
            require(admitted && mutated && rejected == 4u && row.errors.empty() && !resource,
                    "OfficialFlow400 exact diagram title mutation proof");
        else
            require(!admitted && !mutated && (!row.errors.empty() || resource),
                    "OfficialFlow400 failed diagram title row requires diagnostics");
    }
    if (title_margin_selected)
        require(row.path == flow_oracle::kFlowTitleMarginFixture &&
                    row.source_sha256 == flow_oracle::kFlowTitleMarginSourceSha &&
                    row.role_model_sha == flow_oracle::kFlowTitleMarginModelSha,
                "OfficialFlow400 titleTopMargin source/model authority");
    const auto supplement = info.get("source_supplement_sha256");
    if (typed || classic_selected || link_selected || group_selected) {
        const auto expected =
            group_selected  ? official_flow_group_endpoint15_identity(group_endpoint15, row, info)
            : link_selected ? official_flow_linkprofile3_identity(linkprofile3, row)
            : typed         ? official_flow_typed_source4_identity(typed_source4, row)
                            : official_flow_classic_identity(classic, row);
        const auto asset = group_selected  ? flow_oracle::kFlow400GroupEndpoint15AssetSha
                           : link_selected ? flow_oracle::kFlow400LinkProfile3AssetSha
                           : typed         ? flow_oracle::kFlow400TypedSource4AssetSha
                                           : flow_oracle::kFlow400ClassicAssetSha;
        require(supplement.is_str() && supplement.as_str() == asset,
                group_selected  ? "OfficialFlow400 inconsistent GroupEndpoint15 authority metadata"
                : link_selected ? "OfficialFlow400 inconsistent LinkProfile3 authority metadata"
                : typed         ? "OfficialFlow400 inconsistent TypedSource4 authority metadata"
                                : "OfficialFlow400 inconsistent selected authority metadata");
        const auto planned = flow_oracle::flow400_number(
            info.get(group_selected  ? "group_endpoint15_mutations_expected"
                     : link_selected ? "linkprofile3_mutations_expected"
                     : typed         ? "typed_mutations_expected"
                                     : "classic_mutations_expected"));
        const auto rejected = flow_oracle::flow400_number(
            info.get(group_selected  ? "group_endpoint15_mutations_rejected"
                     : link_selected ? "linkprofile3_mutations_rejected"
                     : typed         ? "typed_mutations_rejected"
                                     : "classic_mutations_rejected"));
        require(planned == expected && rejected <= expected,
                group_selected  ? "OfficialFlow400 GroupEndpoint15 mutation bound metadata"
                : link_selected ? "OfficialFlow400 LinkProfile3 mutation bound metadata"
                : typed         ? "OfficialFlow400 TypedSource4 mutation bound metadata"
                                : "OfficialFlow400 selected mutation bound metadata");
        if (admitted || mutated)
            require(admitted && mutated && rejected == expected && row.errors.empty() && !resource,
                    group_selected  ? "OfficialFlow400 GroupEndpoint15 exact mutation metadata"
                    : link_selected ? "OfficialFlow400 LinkProfile3 exact mutation metadata"
                    : typed         ? "OfficialFlow400 TypedSource4 exact mutation metadata"
                                    : "OfficialFlow400 selected exact mutation metadata");
        else
            require(!admitted && !mutated && (!row.errors.empty() || resource),
                    group_selected
                        ? "OfficialFlow400 GroupEndpoint15 failed row requires diagnostics"
                    : link_selected ? "OfficialFlow400 LinkProfile3 failed row requires diagnostics"
                    : typed         ? "OfficialFlow400 TypedSource4 failed row requires diagnostics"
                                    : "OfficialFlow400 selected failed row requires diagnostics");
    } else
        require(selected ? (supplement.is_str() &&
                            supplement.as_str() == flow_oracle::kFlow400Config21AssetSha)
                         : supplement.is_null(),
                "OfficialFlow400 inconsistent Config21 authority metadata");
    counts.observed += captured;
    counts.bound += bound;
    counts.decoded += decoded;
    counts.pending += pending;
    counts.admitted += admitted;
    counts.errors += !row.errors.empty();
    counts.resources += resource;
    counts.mutated += mutated;
}

OfficialFlowCounts official_flow_counts(const std::vector<Observation>& rows, JsonVal typed_source4,
                                        JsonVal classic, JsonVal linkprofile3,
                                        JsonVal group_endpoint15) {
    // Per-source reservations pay repeated bounded metadata parses/copies here
    // and in emit_report. No JSON Model or borrowing view escapes its document.
    OfficialFlowCounts counts;
    std::array<bool, 400> seen{};
    for (const auto& row : rows) {
        if (row.raw_active_schema != kOfficialFlow400Contract) continue;
        require(row.track == "original" && row.type == "flowchart" && row.role_case_index < 400u &&
                    !seen[row.role_case_index],
                "OfficialFlow400 duplicate/unknown/or wrongtrack consumption");
        seen[row.role_case_index] = true;
        ++counts.visited;
        require(row.raw_source_constraints.size() <= 2048u,
                "OfficialFlow400 owned metadata bounds");
        const auto parsed = json::parse(row.raw_source_constraints);
        require(parsed.has_value(), "OfficialFlow400 owned metadata JSON");
        official_flow_accumulate_owned(parsed->root(), row, counts, typed_source4, classic,
                                       linkprofile3, group_endpoint15);
    }
    return counts;
}

std::string official_flow_group_endpoint15_body_info(const OfficialFlowProgress& p,
                                                     std::size_t remaining, std::size_t expected,
                                                     std::size_t rejected, std::size_t published,
                                                     bool input_exact,
                                                     const GroupEndpoint15BodyFact& fact) {
    json::JsonMutDoc doc;
    auto info = doc.object();
    info.set("five_captures_complete", p.captured);
    info.set("raw_source_bound", p.bound);
    info.set("whole_Model_decoded", p.decoded);
    info.set("body_admitted", p.admitted);
    info.set("restored_cells_equal", p.restored);
    info.set("narrow_ordinary", p.narrow_ordinary);
    info.set("semantic_mutations_complete", p.mutated);
    info.set("verification_resource_exhausted", p.resource);
    info.set("captured_frames", static_cast<int64_t>(p.captures));
    info.set("published_frames", static_cast<int64_t>(published));
    info.set("phase", p.phase);
    info.set("remaining", static_cast<int64_t>(remaining));
    info.set("height", p.height);
    info.set("source_supplement_sha256", flow_oracle::kFlow400GroupEndpoint15AssetSha);
    info.set("manifest_global_ordinal", static_cast<int64_t>(fact.global));
    info.set("raw_source_sha256", fact.raw_sha);
    info.set("body_sha256", fact.body_sha);
    info.set("body_offset", static_cast<int64_t>(fact.offset));
    info.set("body_expectation_sha256", fact.expectation_sha);
    info.set("body_model_sha256", fact.model_sha);
    info.set("group_endpoint15_mutations_expected", static_cast<int64_t>(expected));
    info.set("group_endpoint15_mutations_rejected", static_cast<int64_t>(rejected));
    info.set("body_track", "body_diagnostic");
    info.set("body_input_exact", input_exact);
    auto costs = info.make_arr();
    const auto observed = official_flow_typed_source4_phase_checkpoints(p.phase);
    for (std::size_t index = 0u; index < p.remaining.size(); ++index) {
        if (index < observed)
            costs.append(info.make_num(static_cast<int64_t>(p.remaining[index])));
        else
            costs.append(info.make_null());
    }
    info.add("phase_remaining", costs);
    doc.set_root(info);
    const auto text = doc.to_string();
    require(text.size() <= 3072u, "OfficialFlow400 GroupEndpoint15 body metadata bound");
    return text;
}

void official_flow_group_endpoint15_body_keys(JsonVal value) {
    constexpr std::array<std::string_view, 25> keys{{"five_captures_complete",
                                                     "raw_source_bound",
                                                     "whole_Model_decoded",
                                                     "body_admitted",
                                                     "restored_cells_equal",
                                                     "narrow_ordinary",
                                                     "semantic_mutations_complete",
                                                     "verification_resource_exhausted",
                                                     "captured_frames",
                                                     "published_frames",
                                                     "phase",
                                                     "remaining",
                                                     "height",
                                                     "source_supplement_sha256",
                                                     "manifest_global_ordinal",
                                                     "raw_source_sha256",
                                                     "body_sha256",
                                                     "body_offset",
                                                     "body_expectation_sha256",
                                                     "body_model_sha256",
                                                     "group_endpoint15_mutations_expected",
                                                     "group_endpoint15_mutations_rejected",
                                                     "phase_remaining",
                                                     "body_track",
                                                     "body_input_exact"}};
    require(value.is_obj() && value.size() == keys.size(),
            "OfficialFlow400 GroupEndpoint15 body metadata key count");
    std::array<bool, keys.size()> seen{};
    value.iter_obj([&](JsonVal key, JsonVal) {
        const auto text = flow_oracle::flow400_text(key);
        const auto at = std::ranges::find(keys, text);
        require(at != keys.end(), "OfficialFlow400 GroupEndpoint15 body metadata unknown key");
        const auto index = static_cast<std::size_t>(at - keys.begin());
        require(!seen[index], "OfficialFlow400 GroupEndpoint15 body metadata duplicate key");
        seen[index] = true;
    });
    require(std::ranges::all_of(seen, [](bool present) { return present; }),
            "OfficialFlow400 GroupEndpoint15 body metadata missing key");
}

Observation run_official_group_endpoint15_body(
    JsonVal fixture, std::size_t global, std::size_t ordinal, std::string_view raw_source,
    const flow_oracle::Flow400Documents& flow400,
    const flow_oracle::GroupEndpoint15Documents& group, flow_oracle::OracleWork& work,
    std::ostream& diagnostics, std::size_t observation_size, std::size_t observation_capacity) {
    const auto& fact = group_endpoint15_body_fact(ordinal, global);
    Observation row;
    OfficialFlowProgress p;
    OfficialFlowFrames frames;
    row.raw_active_schema = kGroupEndpoint15BodyContract;
    row.raw_source_authority = "immutable_Flow400_GroupEndpoint15_body_diagnostic";
    row.track = "body_diagnostic";
    row.type = "flowchart";
    row.role_case_index = ordinal;
    row.path = std::string(fact.path);
    // These are immutable intended identities, not a bind/decode claim. The
    // explicit body_input_exact and raw_source_bound flags carry lifecycle.
    row.source_sha256 = std::string(fact.body_sha);
    row.role_body_sha = std::string(fact.body_sha);
    row.role_model_sha = std::string(fact.model_sha);
    row.projection_baseline = std::string(fact.expectation_canonical);
    row.expected = "drawn";
    row.expected_states = {{"drawn", "drawn_or_exact_source", "drawn"}};
    row.reason = "GroupEndpoint15 body diagnostic has independent source/model/geometry ownership";
    row.decision_reasons.fill("Independent GroupEndpoint15 body geometry and restoration");
    json::JsonMutDoc diagnostic;
    auto data = diagnostic.object();
    std::size_t rejected = 0u, published = 0u;
    bool input_exact = false;
    try {
        work.spend(65536u + 8u * sizeof(Observation) + 8u * sizeof(OfficialFlowFrames) +
                   8u * sizeof(std::size_t) + 4096u);
        require(observation_size <= 4096u && observation_capacity <= 8192u,
                "OfficialFlow400 GroupEndpoint15 body observation bounds");
        const auto slots = observation_size == observation_capacity
                               ? std::max(2u * observation_capacity, observation_size + 1u)
                               : 0u;
        work.product(slots + (slots ? observation_size : 0u) + 1u, 2u * sizeof(Observation) + 16u);
        row.errors.reserve(6u);
        require(raw_source.size() > fact.offset &&
                    raw_source.substr(0u, fact.offset).size() == fact.offset,
                "OfficialFlow400 GroupEndpoint15 body offset bounds");
        const auto body = raw_source.substr(fact.offset);
        p.phase = "capture";
        auto element = loom::ui::render_markdown(fenced(body, "mermaid", kSentinel));
        auto ordinary = loom::ui::render_markdown(fenced(body, "text", kSentinel));
        element->ComputeRequirement();
        ordinary->ComputeRequirement();
        p.height = std::max({1, element->requirement().min_y, ordinary->requirement().min_y}) + 1;
        require(p.height <= 1040, "OfficialFlow400 GroupEndpoint15 body height");
        constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
        for (std::size_t index = 0u; index < frames.size(); ++index) {
            frames[index] = flow_oracle::capture(index < 3u ? element : ordinary, widths[index],
                                                 p.height, &work);
            ++p.captures;
        }
        p.captured = true;
        p.remaining[0] = work.remaining;
        p.phase = "publication";
        constexpr std::array<std::string_view, 5> names{
            {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
        for (std::size_t index = 0u; index < frames.size(); ++index) {
            official_flow_publish_grid(data, names[index], *frames[index], work);
            ++published;
        }
        p.remaining[1] = work.remaining;
        require(p.captures == 5u && published == 5u,
                "OfficialFlow400 GroupEndpoint15 body frames before bind");
        require(fixture.get("path").is_str() && fixture.get("path").as_str() == fact.path &&
                    flow_oracle::flow400_text(fixture.get("sha256")) == fact.raw_sha,
                "OfficialFlow400 GroupEndpoint15 body raw fixture identity");
        const auto canonical = projection_canonical(fixture.get("diagnostic"));
        work.product(canonical.size(), 8u);
        require(canonical == fact.expectation_canonical &&
                    digest(canonical) == fact.expectation_sha,
                "OfficialFlow400 GroupEndpoint15 body immutable diagnostic expectation");
        require(body == fact.body_utf8 && body.size() == fact.body_bytes &&
                    digest(body) == fact.body_sha,
                "OfficialFlow400 GroupEndpoint15 exact body bytes");
        input_exact = true;
        p.phase = "bind";
        const auto base = flow400.compact.root().get("cases").at(ordinal);
        const auto selected = flow_oracle::group_endpoint15_select(
            base, fixture, group.asset.root().get("records"), ordinal, global, raw_source, work);
        require(selected.has_value() && flow_oracle::flow400_text(selected->record.get(
                                            "canonical_model_sha256")) == fact.model_sha,
                "OfficialFlow400 GroupEndpoint15 body selected tuple/Model");
        p.bound = true;
        p.phase = "decode";
        const auto model = flow_oracle::group_endpoint15_model(selected->record.get("model"), work);
        p.decoded = true;
        p.remaining[2] = work.remaining;
        rejected = official_flow_geometry(frames, model.base, work, row, p, JsonVal{}, JsonVal{},
                                          JsonVal{}, nullptr, selected->record, &model);
        p.phase = "complete";
    } catch (const std::exception& error) {
        const auto text = std::string_view(error.what());
        p.resource = p.resource || work.remaining == 0u || flow_oracle::flow400_resource(text);
        official_flow_error(row, text);
        p.admitted = false;
    }
    row.raw_source_constraints = official_flow_group_endpoint15_body_info(
        p, work.remaining, 7u, rejected, published, input_exact, fact);
    data.set("kind", "official_Flow400_group_endpoint15_body/v1");
    data.set("contract", kGroupEndpoint15BodyContract);
    data.set("ordinal", static_cast<int64_t>(ordinal));
    data.set("manifest_global_ordinal", static_cast<int64_t>(global));
    data.set("fixture", fact.path);
    data.set("actual", row.raw_source_constraints);
    data.set("foreign_assertions_certified", 0);
    diagnostic.set_root(data);
    diagnostics << diagnostic.to_string() << '\n';
    require(static_cast<bool>(diagnostics), "OfficialFlow400 GroupEndpoint15 body evidence write");
    return row;
}

GroupEndpoint15BodyCounts
group_endpoint15_body_counts(const std::vector<Observation>& rows,
                             const flow_oracle::GroupEndpoint15Documents& group) {
    GroupEndpoint15BodyCounts counts;
    std::array<bool, 15> consumed{};
    for (const auto& row : rows) {
        if (row.raw_active_schema != kGroupEndpoint15BodyContract) continue;
        require(row.track == "body_diagnostic" && row.type == "flowchart" &&
                    row.role_case_index < 400u,
                "OfficialFlow400 GroupEndpoint15 body track");
        const auto parsed = json::parse(row.raw_source_constraints);
        require(parsed.has_value(), "OfficialFlow400 GroupEndpoint15 body metadata JSON");
        const auto info = parsed->root();
        official_flow_group_endpoint15_body_keys(info);
        const auto manifest_global_ordinal =
            flow_oracle::flow400_number(info.get("manifest_global_ordinal"));
        const auto global = std::ranges::find_if(
            kGroupEndpoint15BodyFacts, [&](const GroupEndpoint15BodyFact& fact) {
                return fact.ordinal == row.role_case_index &&
                       fact.global == manifest_global_ordinal && fact.path == row.path;
            });
        require(global != kGroupEndpoint15BodyFacts.end(),
                "OfficialFlow400 GroupEndpoint15 body authority selection");
        const auto index = static_cast<std::size_t>(global - kGroupEndpoint15BodyFacts.begin());
        require(!consumed[index], "OfficialFlow400 GroupEndpoint15 duplicate body consume");
        consumed[index] = true;
        ++counts.visited;
        require(row.raw_source_authority == "immutable_Flow400_GroupEndpoint15_body_diagnostic" &&
                    row.source_sha256 == global->body_sha &&
                    row.role_body_sha == global->body_sha &&
                    row.role_model_sha == global->model_sha,
                "OfficialFlow400 GroupEndpoint15 body row identity");
        require(row.projection_baseline == global->expectation_canonical,
                "OfficialFlow400 GroupEndpoint15 body canonical provenance");
        const auto records = group.asset.root().get("records");
        JsonVal record;
        std::size_t record_matches = 0u;
        records.iter([&](JsonVal candidate) {
            if (flow_oracle::flow400_number(candidate.get("ordinal")) == global->ordinal &&
                flow_oracle::flow400_number(candidate.get("manifest_global_ordinal")) ==
                    manifest_global_ordinal) {
                record = candidate;
                ++record_matches;
            }
        });
        require(record_matches == 1u,
                "OfficialFlow400 GroupEndpoint15 body source record selection");
        const auto tuple = flow_oracle::group_endpoint15_tuple(record);
        require(tuple.ordinal == global->ordinal &&
                    tuple.manifest_global_ordinal == manifest_global_ordinal &&
                    tuple.manifest_global_ordinal == global->global &&
                    tuple.fixture == global->path && tuple.source_sha256 == global->raw_sha &&
                    flow_oracle::flow400_text(record.get("canonical_model_sha256")) ==
                        global->model_sha,
                "OfficialFlow400 GroupEndpoint15 body source/model authority");
        const auto yes = [&](std::string_view key) {
            const auto value = info.get(key);
            require(value.is_bool(), "OfficialFlow400 GroupEndpoint15 body metadata bool");
            return value.as_bool();
        };
        const bool captured = yes("five_captures_complete"), bound = yes("raw_source_bound"),
                   decoded = yes("whole_Model_decoded"), admitted = yes("body_admitted"),
                   restored = yes("restored_cells_equal"), ordinary = yes("narrow_ordinary"),
                   mutated = yes("semantic_mutations_complete"),
                   resource = yes("verification_resource_exhausted");
        const auto captures = flow_oracle::flow400_number(info.get("captured_frames"));
        const auto published = flow_oracle::flow400_number(info.get("published_frames"));
        require(captures <= 5u && published <= 5u &&
                    (!captured || (captures == 5u && published == 5u)),
                "OfficialFlow400 GroupEndpoint15 body five capture/publication frames");
        require(flow_oracle::flow400_text(info.get("source_supplement_sha256")) ==
                        flow_oracle::kFlow400GroupEndpoint15AssetSha &&
                    flow_oracle::flow400_text(info.get("raw_source_sha256")) == global->raw_sha &&
                    flow_oracle::flow400_text(info.get("body_sha256")) == global->body_sha &&
                    flow_oracle::flow400_number(info.get("body_offset")) == global->offset &&
                    flow_oracle::flow400_text(info.get("body_expectation_sha256")) ==
                        global->expectation_sha &&
                    flow_oracle::flow400_text(info.get("body_model_sha256")) == global->model_sha &&
                    flow_oracle::flow400_text(info.get("body_track")) == "body_diagnostic",
                "OfficialFlow400 GroupEndpoint15 body metadata provenance");
        const auto phase = info.get("phase");
        require(phase.is_str() && phase.as_str().size() <= 32u,
                "OfficialFlow400 GroupEndpoint15 body phase");
        const auto remaining = flow_oracle::flow400_number(info.get("remaining"));
        require(remaining <= flow_oracle::kFlow400SceneLimit,
                "OfficialFlow400 GroupEndpoint15 body remaining bound");
        require(info.get("height").is_int() && info.get("height").as_int() >= 0 &&
                    info.get("height").as_int() <= 1040,
                "OfficialFlow400 GroupEndpoint15 body height bound");
        const auto costs = info.get("phase_remaining");
        require(costs.is_arr() && costs.size() == 7u,
                "OfficialFlow400 GroupEndpoint15 body phase ledger shape");
        const auto observed = official_flow_typed_source4_phase_checkpoints(phase.as_str());
        std::size_t prior = flow_oracle::kFlow400SceneLimit, last = flow_oracle::kFlow400SceneLimit;
        for (std::size_t phase_index = 0u; phase_index < costs.size(); ++phase_index) {
            const auto value = costs.at(phase_index);
            if (phase_index >= observed) {
                require(value.is_null(),
                        "OfficialFlow400 GroupEndpoint15 body unobserved phase ledger");
                continue;
            }
            require(value.is_int() && value.as_int() >= 0 &&
                        static_cast<std::size_t>(value.as_int()) <= prior,
                    "OfficialFlow400 GroupEndpoint15 body monotonic phase ledger");
            prior = static_cast<std::size_t>(value.as_int());
            last = prior;
        }
        const auto expected =
                       flow_oracle::flow400_number(info.get("group_endpoint15_mutations_expected")),
                   rejected =
                       flow_oracle::flow400_number(info.get("group_endpoint15_mutations_rejected"));
        require(expected == 7u && rejected <= expected,
                "OfficialFlow400 GroupEndpoint15 body mutation bound");
        const bool input_exact = yes("body_input_exact");
        if (admitted)
            require(
                input_exact && captured && bound && decoded && restored && mutated && !resource &&
                    row.errors.empty() && rejected == expected && observed == costs.size() &&
                    last == remaining && phase.as_str() == "complete" && row.status[0] == "drawn" &&
                    row.status[2] == "drawn" &&
                    (row.status[1] == "drawn" || (row.status[1] == "source_fallback" && ordinary)),
                "OfficialFlow400 GroupEndpoint15 body admission");
        else {
            require(!admitted && !mutated && (!row.errors.empty() || resource),
                    "OfficialFlow400 GroupEndpoint15 body failed diagnostics");
            if (observed)
                require(remaining <= last,
                        "OfficialFlow400 GroupEndpoint15 body failed phase/work ledger");
        }
        counts.observed += captured;
        counts.bound += bound;
        counts.decoded += decoded;
        counts.admitted += admitted;
        counts.mutated += mutated;
        counts.errors += !row.errors.empty();
        counts.resources += resource;
    }
    static_cast<void>(group);
    return counts;
}

TEST(MermaidOfficialFlow400, OwnedWholeSceneAndForgedAdmissionDoNotUseWideOnly) {
    using flow_oracle::flow400_require;
    flow_oracle::OracleWork work{flow_oracle::kFlow400SceneLimit};
    work.spend(kOfficialFlowReportReserve + 65536u + 16u * sizeof(Observation) +
               8u * sizeof(OfficialFlowFrames));
    constexpr std::string_view source = "flowchart LR\nBusiness[\"AFTER_MERMAID\"]\n";
    work.product(source.size(), 32u);
    auto node = loom::ui::render_markdown(fenced(source, "mermaid", kSentinel));
    auto ordinary = loom::ui::render_markdown(fenced(source, "text", kSentinel));
    node->ComputeRequirement();
    ordinary->ComputeRequirement();
    const int height = std::max({1, node->requirement().min_y, ordinary->requirement().min_y}) + 1;
    flow400_require(height <= 1040, "OfficialFlow400 control height");
    OfficialFlowFrames frames;
    constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
    for (std::size_t n = 0; n < frames.size(); ++n)
        frames[n] = flow_oracle::capture(n < 3u ? node : ordinary, widths[n], height, &work);
    work.product(source.size(), 4u);
    flow400_require(digest(source) ==
                        "d976fd133de62e58dce8a7f47cef95f93f3be9ebce81d412831b2b7cefe2e8f7",
                    "OfficialFlow400 control source SHA");
    work.spend(16384u + 8u * sizeof(flow_oracle::Model) + 8u * sizeof(flow_oracle::Node));
    const flow_oracle::Model model{"LR", {{"Business", std::string(kSentinel)}}, {}, {}};
    Observation good;
    good.errors.reserve(6u);
    OfficialFlowProgress p;
    p.captured = p.bound = p.decoded = true;
    p.captures = 5u;
    p.height = height;
    official_flow_geometry(frames, model, work, good, p);
    ASSERT_TRUE(p.admitted);
    ASSERT_TRUE(good.errors.empty());
    EXPECT_NO_THROW(flow_oracle::flow_typed_expect_model_rejected(
        *frames[0], model, work, [](auto& bad) { bad.nodes.front().label = "wrong"; },
        "Official Flow400 custom following token", kSentinel));
    flow_oracle::flow400_copy_grid(*frames[2], work);
    auto saved = *frames[2];
    const auto footer = flow_oracle::flow400_following(*frames[2], work, kSentinel);
    work.spend(32u);
    frames[2]->put({0, footer}, "X");
    Observation bad;
    bad.errors.reserve(6u);
    OfficialFlowProgress changed;
    changed.captured = changed.bound = changed.decoded = true;
    changed.captures = 5u;
    changed.height = height;
    bool physical_rejected = false;
    try {
        official_flow_geometry(frames, model, work, bad, changed);
    } catch (const std::exception& error) {
        ASSERT_GT(work.remaining, 0u);
        EXPECT_EQ(std::string_view(error.what()),
                  "Flow400 protocol exact independent following row");
        physical_rejected = true;
    }
    EXPECT_TRUE(physical_rejected);
    EXPECT_EQ(bad.status[0], "drawn");
    EXPECT_FALSE(changed.admitted);
    // The owned bit is insufficient: bad restoration cannot earn coverage.
    work.spend(65536u + 8u * sizeof(Observation));
    bad.raw_active_schema = kOfficialFlow400Contract;
    bad.track = "original";
    bad.type = "flowchart";
    bad.role_case_index = 0u;
    bad.role_model_sha = std::string(64u, 'a');
    changed.admitted = true;
    bad.raw_source_constraints = official_flow_info(changed, work.remaining);
    work.product(2u, 4u * sizeof(Observation) + 4096u);
    std::vector<Observation> forged;
    forged.push_back(std::move(bad));
    bool rejected = false;
    try {
        static_cast<void>(official_flow_counts(forged));
    } catch (const std::exception& error) {
        ASSERT_GT(work.remaining, 0u);
        EXPECT_EQ(std::string_view(error.what()),
                  "OfficialFlow400 per-state proof cannot bypass whole-source admission");
        rejected = true;
    }
    EXPECT_TRUE(rejected);
    work.spend(4u * sizeof(flow_oracle::Grid) + 128u);
    frames[2] = std::move(saved);
    flow_oracle::OracleWork zero{0u};
    bool zero_rejected = false;
    try {
        static_cast<void>(flow_oracle::capture(node, 240, height, &zero));
    } catch (const std::exception& error) {
        EXPECT_EQ(std::string_view(error.what()), "Bounded cell-oracle work exhausted");
        EXPECT_EQ(zero.remaining, 0u);
        zero_rejected = true;
    }
    EXPECT_TRUE(zero_rejected);
}
}  // namespace

namespace {
TEST(MermaidOfficialFlow400, Issue4648ArrowlessNestedSubgraphRequiresOwnedGeometryAndControls) {
    flow_oracle::OracleWork work{flow_oracle::kFlow400SceneLimit};
    work.spend(kOfficialFlowReportReserve + 32768u + 8u * sizeof(OfficialFlowFrames));
    const auto docs = flow_oracle::flow400_documents();
    const auto source =
        read_file(kRoot / "cases" / std::string(flow_oracle::kFlow400Issue4648Fixture));
    ASSERT_EQ(source.size(), 529u);
    auto drawing = loom::ui::render_markdown(fenced(source, "mermaid", kSentinel));
    auto ordinary = loom::ui::render_markdown(fenced(source, "text", kSentinel));
    drawing->ComputeRequirement();
    ordinary->ComputeRequirement();
    const int height =
        std::max({32, drawing->requirement().min_y + 8, ordinary->requirement().min_y + 8,
                  static_cast<int>(std::count(source.begin(), source.end(), '\n')) + 12});
    ASSERT_LE(height, 1040);
    constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
    OfficialFlowFrames frames;
    for (std::size_t index = 0u; index < frames.size(); ++index)
        frames[index] =
            flow_oracle::capture(index < 3u ? drawing : ordinary, widths[index], height, &work);
    flow_oracle::flow400_issue4648_bind(docs, source, work);
    const auto model = flow_oracle::flow400_issue4648_model(work);
    Observation row;
    row.errors.reserve(6u);
    row.path = std::string(flow_oracle::kFlow400Issue4648Fixture);
    row.source_sha256 = std::string(flow_oracle::kFlow400Issue4648SourceSha);
    row.raw_active_schema = kOfficialFlow400Contract;
    row.raw_source_authority = "immutable_Flow400_issue4648_Arrowless1";
    row.track = "original";
    row.type = "flowchart";
    row.role_case_index = flow_oracle::kFlow400Issue4648Ordinal;
    row.role_model_sha = std::string(flow_oracle::kFlow400Issue4648ModelSha);
    OfficialFlowProgress progress;
    progress.captured = progress.bound = progress.decoded = true;
    progress.captures = 5u;
    progress.height = height;
    const auto generic_mutations = official_flow_geometry(frames, model, work, row, progress);
    EXPECT_TRUE(row.errors.empty()) << flow_oracle::errors_text(row.errors);
    EXPECT_TRUE(progress.admitted);
    EXPECT_TRUE(progress.mutated);
    for (std::size_t index = 0u; index < frames.size(); ++index)
        ASSERT_TRUE(frames[index].has_value());

    auto wrong_endpoint = model;
    wrong_endpoint.edges[2].to = 1;
    const auto wrong_endpoint_errors =
        flow_oracle::flow400_geometry(*frames[0], wrong_endpoint, work, kSentinel);
    EXPECT_FALSE(wrong_endpoint_errors.empty());
    auto wrong_marker = model;
    wrong_marker.edges[2].end = true;
    const auto wrong_marker_errors =
        flow_oracle::flow400_geometry(*frames[0], wrong_marker, work, kSentinel);
    EXPECT_FALSE(wrong_marker_errors.empty());
    auto wrong_group = model;
    wrong_group.groups[0].direction = "TD";
    const auto wrong_group_errors =
        flow_oracle::flow400_geometry(*frames[0], wrong_group, work, kSentinel);
    EXPECT_FALSE(wrong_group_errors.empty());
    auto missing_edge = model;
    missing_edge.edges.pop_back();
    const auto missing_edge_errors =
        flow_oracle::flow400_geometry(*frames[0], missing_edge, work, kSentinel);
    EXPECT_FALSE(missing_edge_errors.empty());
    ASSERT_EQ(frames[0]->at({35, 28}), "│");
    flow_oracle::flow400_copy_grid(*frames[0], work);
    auto broken_route = *frames[0];
    broken_route.put({35, 28}, " ");
    const auto broken_route_errors =
        flow_oracle::flow400_geometry(broken_route, model, work, kSentinel);
    EXPECT_FALSE(broken_route_errors.empty());
    flow_oracle::flow400_copy_grid(*frames[0], work);
    auto branched_route = *frames[0];
    branched_route.put({35, 28}, "┼");
    const auto branched_route_errors =
        flow_oracle::flow400_geometry(branched_route, model, work, kSentinel);
    EXPECT_FALSE(branched_route_errors.empty());
    flow_oracle::flow400_copy_grid(*frames[0], work);
    auto unexpected_arrow = *frames[0];
    unexpected_arrow.put({35, 28}, "▼");
    const auto unexpected_arrow_errors =
        flow_oracle::flow400_geometry(unexpected_arrow, model, work, kSentinel);
    EXPECT_FALSE(unexpected_arrow_errors.empty());
    flow_oracle::flow400_copy_grid(*frames[0], work);
    auto orphan_route = *frames[0];
    orphan_route.put({100, 10}, "│");
    const auto orphan_route_errors =
        flow_oracle::flow400_geometry(orphan_route, model, work, kSentinel);
    EXPECT_FALSE(orphan_route_errors.empty());
    flow_oracle::flow400_copy_grid(*frames[0], work);
    auto multiple_orphans = *frames[0];
    multiple_orphans.put({100, 10}, "│");
    multiple_orphans.put({102, 10}, "│");
    const auto orphan_errors =
        flow_oracle::flow400_geometry(multiple_orphans, model, work, kSentinel);
    ASSERT_FALSE(orphan_errors.empty());
    EXPECT_LE(orphan_errors.size(), 1u);
    for (const auto* errors : {&wrong_endpoint_errors, &wrong_marker_errors, &wrong_group_errors,
                               &missing_edge_errors, &broken_route_errors, &branched_route_errors,
                               &unexpected_arrow_errors, &orphan_route_errors, &orphan_errors})
        for (const auto& error : *errors)
            EXPECT_FALSE(flow_oracle::flow400_resource(error));

    std::size_t profile_rejections = 0u;
    auto reject_profile = [&](std::string changed) {
        work.product(changed.size(), 4u);
        work.spend(1024u);
        auto invalid = json::parse(changed);
        ASSERT_TRUE(invalid.has_value());
        std::size_t groups = 0u, members = 0u;
        bool rejected = false;
        try {
            flow_oracle::flow400_shape(invalid->root(), groups, members);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        EXPECT_TRUE(rejected);
        profile_rejections += rejected;
    };
    const auto edge_at = flow_oracle::kFlow400Issue4648ModelJson.find("\"source\":\"D3\"");
    ASSERT_NE(edge_at, std::string_view::npos);
    auto labeled = std::string(flow_oracle::kFlow400Issue4648ModelJson);
    const auto label_at = labeled.find("\"label\":\"\"", edge_at);
    ASSERT_NE(label_at, std::string::npos);
    labeled.replace(label_at, std::string_view("\"label\":\"\"").size(), "\"label\":\"label\"");
    reject_profile(std::move(labeled));
    for (const auto style : {std::string_view{"dotted"}, std::string_view{"thick"}}) {
        auto styled = std::string(flow_oracle::kFlow400Issue4648ModelJson);
        const auto style_at = styled.find("\"style\":\"solid\"", edge_at);
        ASSERT_NE(style_at, std::string::npos);
        styled.replace(style_at, std::string_view("\"style\":\"solid\"").size(),
                       "\"style\":\"" + std::string(style) + "\"");
        reject_profile(std::move(styled));
    }
    auto reverse = std::string(flow_oracle::kFlow400Issue4648ModelJson);
    const auto start_at = reverse.find("\"hasArrowStart\":false", edge_at);
    ASSERT_NE(start_at, std::string::npos);
    reverse.replace(start_at, std::string_view("\"hasArrowStart\":false").size(),
                    "\"hasArrowStart\":true");
    reject_profile(std::move(reverse));
    const auto semantic_count = static_cast<std::size_t>(!wrong_endpoint_errors.empty()) +
                                static_cast<std::size_t>(!wrong_marker_errors.empty()) +
                                static_cast<std::size_t>(!wrong_group_errors.empty()) +
                                static_cast<std::size_t>(!missing_edge_errors.empty()) +
                                profile_rejections;
    const auto physical_count = static_cast<std::size_t>(!broken_route_errors.empty()) +
                                static_cast<std::size_t>(!branched_route_errors.empty()) +
                                static_cast<std::size_t>(!unexpected_arrow_errors.empty()) +
                                static_cast<std::size_t>(!orphan_route_errors.empty()) +
                                static_cast<std::size_t>(!orphan_errors.empty());
    EXPECT_EQ(semantic_count, 8u);
    EXPECT_EQ(profile_rejections, 4u);
    EXPECT_EQ(physical_count, 5u);
    EXPECT_GT(generic_mutations, 0u);
#ifdef LOOM_MERMAID_REPORT_DIR
    json::JsonMutDoc verdict;
    auto root = verdict.object();
    root.set("schema", "flow400_issue4648_official_verdict/v1");
    root.set("source_facts_sha256", flow_oracle::kFlow400Issue4648FactsSha);
    root.set("case_id", "flow400_nested_external1_357");
    root.set("ordinal", static_cast<int64_t>(flow_oracle::kFlow400Issue4648Ordinal));
    root.set("manifest_global_ordinal",
             static_cast<int64_t>(flow_oracle::kFlow400Issue4648GlobalOrdinal));
    root.set("fixture", flow_oracle::kFlow400Issue4648Fixture);
    root.set("source_sha256", flow_oracle::kFlow400Issue4648SourceSha);
    root.set("model_sha256", flow_oracle::kFlow400Issue4648ModelSha);
    root.set("captures", static_cast<int64_t>(progress.captures));
    root.set("captures_before_binding", progress.captured && progress.captures == 5u);
    root.set("source_bound_after_captures", progress.bound);
    root.set("wide_geometry_good", progress.admitted);
    root.set("restored_geometry_good", progress.restored);
    root.set("narrow_exact_ordinary", progress.narrow_ordinary);
    root.set("narrow_geometry_good", progress.narrow_ordinary ? false : row.status[1] == "drawn");
    root.set("semantic_mutations_rejected", static_cast<int64_t>(semantic_count));
    root.set("physical_mutations_rejected", static_cast<int64_t>(physical_count));
    root.set("profile_controls_rejected", static_cast<int64_t>(profile_rejections));
    root.set("generic_mutations_rejected", static_cast<int64_t>(generic_mutations));
    root.set("resource_errors", progress.resource ? 1 : 0);
    root.set("source_fallback_credit", 0);
    root.set("admitted", progress.admitted && row.errors.empty());
    verdict.set_root(root);
    write_file(fs::path(LOOM_MERMAID_REPORT_DIR) /
                   "mermaid-flow400-issue4648-official-verdict.json",
               verdict.to_string() + "\n");
#endif
}
}  // namespace

namespace {
// Two immutable offset-zero body descriptors only. No original binder widening.
void flow_body2_consume(std::array<bool, 2>& used, std::size_t index,
                        flow_oracle::OracleWork& work) {
    work.spend(sizeof(bool) + 64u);
    require(index < used.size() && !used[index], "FlowBody2 duplicate/unknown descriptor");
    used[index] = true;
}

void flow_body2_reserve(std::vector<Observation>& rows, flow_oracle::OracleWork& work) {
    work.spend(4u * sizeof(std::size_t) + 8u * sizeof(bool) + 256u);
    require(rows.size() <= 2738u && rows.capacity() <= 8192u,
            "FlowBody2 bounded observation capacity");
    const auto available = rows.capacity() - rows.size();
    if (available < 2u) {
        // This is exactly the capacity that two ordinary pushes would reach;
        // pre-reserving it cannot perturb subsequent unrelated row capacity.
        auto target = rows.capacity();
        for (std::size_t step = 0; step < 2u; ++step)
            if (rows.size() + step == target)
                target = std::max(2u * target, rows.size() + step + 1u);
        require(target <= 8192u, "FlowBody2 bounded reserve target");
        work.product(target + rows.size(), 2u * sizeof(Observation) + 16u);
        rows.reserve(target);
    }
    work.spend(kOfficialFlowReportReserve + 4u * sizeof(Observation) + 16u * sizeof(std::string) +
               4096u);
    work.product(1u, 2u * sizeof(Observation) + 16u);  // body push/move, paid before geometry
}

void flow_body2_copy_charge(const Observation& from, flow_oracle::OracleWork& work) {
    work.spend(4u * sizeof(Observation) + 23u * sizeof(const std::string*) +
               4u * sizeof(std::vector<std::string>) + 1024u);
    const std::array<const std::string*, 23> fields{{&from.path,
                                                     &from.track,
                                                     &from.type,
                                                     &from.source_sha256,
                                                     &from.expected,
                                                     &from.reason,
                                                     &from.status[0],
                                                     &from.status[1],
                                                     &from.status[2],
                                                     &from.expected_states[0],
                                                     &from.expected_states[1],
                                                     &from.expected_states[2],
                                                     &from.decision_reasons[0],
                                                     &from.decision_reasons[1],
                                                     &from.decision_reasons[2],
                                                     &from.projection_baseline,
                                                     &from.projection_effective,
                                                     &from.role_case_id,
                                                     &from.role_model_sha,
                                                     &from.role_body_sha,
                                                     &from.raw_source_authority,
                                                     &from.raw_active_schema,
                                                     &from.raw_source_constraints}};
    for (const auto* text : fields) {
        work.spend(32u);
        work.product(text->size() + 1u, 4u);
    }
    require(from.errors.size() <= 6u, "FlowBody2 bounded error copy");
    work.product(from.errors.size(), 3u * sizeof(std::string) + 16u);
    for (const auto& error : from.errors) {
        work.spend(16u);
        work.product(error.size() + 1u, 4u);
    }
}

void flow_body2_bind(JsonVal fixture, std::size_t global, std::size_t index,
                     std::string_view source, std::string_view body,
                     const flow_oracle::Flow400Documents& docs, flow_oracle::OracleWork& work) {
    work.spend(4u * sizeof(JsonVal) + 4u * sizeof(std::size_t) + 1024u);
    require(index < 2u, "FlowBody2 finite index");
    const auto& pin = kFlowRepeatBody2Pins[index];
    work.spend(pin.ordinal + global + 2u);
    work.product(pin.manifest_bytes + pin.diagnostic_bytes + pin.bytes + 512u, 32u);
    work.spend(8192u);
    require(global == pin.global && flow_oracle::flow400_text(fixture.get("path")) == pin.path,
            "FlowBody2 literal global/path tuple");
    require(flow_oracle::flow400_number(fixture.get("bytes")) == pin.bytes &&
                source.size() == pin.bytes && body.size() == pin.bytes && source == body,
            "FlowBody2 exact complete byte-identical source/suffix");
    const auto diagnostic = fixture.get("diagnostic");
    flow_oracle::flow400_keys(diagnostic, {"expected", "reason", "witness", "offset", "sha256"});
    require(diagnostic.get("offset").is_int() && diagnostic.get_int("offset") == 0,
            "FlowBody2 exact typed offset zero");
    require(flow_oracle::flow400_text(fixture.get("sha256")) == pin.source_sha &&
                flow_oracle::flow400_text(fixture.get("git_blob")) == pin.git_blob &&
                flow_oracle::flow400_text(diagnostic.get("sha256")) == pin.source_sha &&
                digest(source) == pin.source_sha && digest(body) == pin.source_sha,
            "FlowBody2 full and suffix identity");
    require(digest(projection_canonical(fixture)) == pin.manifest_sha &&
                digest(projection_canonical(diagnostic)) == pin.diagnostic_sha,
            "FlowBody2 complete historical policies and witness unchanged");
    const auto authority = docs.compact.root().get("cases").at(pin.ordinal);
    require(flow_oracle::flow400_number(authority.get("ordinal")) == pin.ordinal &&
                flow_oracle::flow400_number(authority.get("manifest_global_ordinal")) ==
                    pin.global &&
                flow_oracle::flow400_text(authority.get("fixture")) == pin.path &&
                flow_oracle::flow400_text(authority.get("source_sha256")) == pin.source_sha &&
                flow_oracle::flow400_text(authority.get("model_sha256")) == pin.model_sha &&
                flow_oracle::flow400_text(authority.get("adaptation")) == "plain_literal_flow",
            "FlowBody2 existing exact complete authority");
    const auto model = flow_oracle::flow400_text(authority.get("model_json"));
    work.product(model.size(), 4u);
    work.spend(256u);
    require(digest(model) == pin.model_sha, "FlowBody2 existing canonical Model identity");
}

Observation reuse_flow_body2(JsonVal fixture, std::size_t global, std::size_t index,
                             std::string_view source, std::string_view body,
                             const Observation& original, const flow_oracle::Flow400Documents& docs,
                             flow_oracle::OracleWork& work) {
    // Bounded error/report/string metadata and body-vector write were reserved
    // before the same original capture could exhaust its ledger.
    Observation row;
    row.errors.reserve(6u);
    bool bound = false, admitted = false, resource = false;
    std::string proof;
    require(index < 2u, "FlowBody2 bounded descriptor before fields");
    const auto& pin = kFlowRepeatBody2Pins[index];
    row.path = pin.path;
    row.track = "body_diagnostic";
    row.type = "flowchart";
    row.role_case_index = index;
    row.raw_active_schema = kFlowRepeatBody2Contract;
    row.expected = "drawn";
    row.expected_states = {"drawn", "drawn_or_exact_source", "drawn"};
    row.reason =
        "Byte-identical offset-zero body requires current original complete whole-scene proof";
    row.decision_reasons.fill(row.reason);
    // Small immutable diagnostic baseline publication fits the pre-geometry
    // body reserve even when the original ledger is already exhausted.
    row.projection_baseline = projection_canonical(fixture.get("diagnostic"));
    try {
        flow_body2_bind(fixture, global, index, source, body, docs, work);
        bound = true;
        work.spend(32768u);
        work.product(original.raw_source_constraints.size(), 32u);
        require(original.raw_active_schema == kOfficialFlow400Contract &&
                    original.track == "original" && original.type == "flowchart" &&
                    original.role_case_index == pin.ordinal && original.path == pin.path &&
                    original.source_sha256 == pin.source_sha &&
                    original.role_model_sha == pin.model_sha,
                "FlowBody2 requires same current original identity/Model");
        require(original.raw_source_constraints.size() <= 2048u, "FlowBody2 original proof bound");
        const auto parsed = json::parse(original.raw_source_constraints);
        require(parsed.has_value(), "FlowBody2 current original proof JSON");
        OfficialFlowCounts checked;
        official_flow_accumulate_owned(parsed->root(), original, checked);
        require(checked.observed == 1u && checked.bound == 1u && checked.decoded == 1u &&
                    checked.pending == 0u && checked.admitted == 1u && checked.mutated == 1u &&
                    checked.errors == 0u && checked.resources == 0u,
                "FlowBody2 current original complete admitted conjunction unavailable");
        flow_body2_copy_charge(original, work);
        row = original;
        row.path = pin.path;
        row.track = "body_diagnostic";
        row.role_case_index = index;
        row.raw_active_schema = kFlowRepeatBody2Contract;
        row.raw_source_authority = "same_run_original_Flow400_offset0";
        row.role_body_sha = pin.source_sha;
        row.reason = "Byte-identical offset-zero body reuses same-run original complete scene";
        row.decision_reasons.fill(row.reason);
        row.projection_baseline = projection_canonical(fixture.get("diagnostic"));
        work.product(original.raw_source_constraints.size(), 8u);
        work.spend(4096u);
        proof = original.raw_source_constraints;
        admitted = true;
    } catch (const std::exception& error) {
        const char* text = error.what();
        std::size_t n = 0;
        if (text)
            while (n < 128u && text[n])
                ++n;
        const std::string_view bounded(text ? text : "", n);
        resource = work.remaining == 0u || flow_oracle::flow400_resource(bounded);
        official_flow_error(row, bounded);
        admitted = false;
        proof.clear();
    }
    row.track = "body_diagnostic";
    row.type = "flowchart";
    row.role_case_index = index;
    row.raw_active_schema = kFlowRepeatBody2Contract;
    // This metadata explicitly distinguishes copied original phase evidence
    // from this body reuse's actual remaining Work. No new capture is claimed.
    json::JsonMutDoc metadata;
    auto value = metadata.object();
    value.set("descriptor_bound", bound);
    value.set("body_admitted", admitted);
    value.set("body_separate_capture", false);
    value.set("original_flow_ordinal", static_cast<int64_t>(pin.ordinal));
    value.set("diagnostic_offset", 0);
    value.set("remaining_after_reuse", static_cast<int64_t>(work.remaining));
    value.set("resource_exhausted", resource);
    if (proof.empty())
        value.add("original_owned_proof", value.make_null());
    else
        value.add("original_owned_proof", metadata.raw_json(proof));
    metadata.set_root(value);
    row.raw_source_constraints = metadata.to_string();
    require(row.raw_source_constraints.size() <= 4096u, "FlowBody2 prepaid metadata bound");
    return row;
}

OfficialFlowCounts flow_body2_counts(const std::vector<Observation>& rows) {
    OfficialFlowCounts totals;
    std::array<bool, 2> seen{};
    for (const auto& row : rows) {
        if (row.raw_active_schema != kFlowRepeatBody2Contract) continue;
        require(row.track == "body_diagnostic" && row.type == "flowchart" &&
                    row.role_case_index < 2u && !seen[row.role_case_index],
                "FlowBody2 duplicate/wrongtrack identity");
        const auto& pin = kFlowRepeatBody2Pins[row.role_case_index];
        require(row.path == pin.path, "FlowBody2 owned path");
        seen[row.role_case_index] = true;
        ++totals.visited;
        require(row.raw_source_constraints.size() <= 4096u, "FlowBody2 owned metadata size");
        const auto parsed = json::parse(row.raw_source_constraints);
        require(parsed.has_value(), "FlowBody2 owned metadata JSON");
        const auto info = parsed->root();
        flow_oracle::flow400_keys(info, {"descriptor_bound", "body_admitted",
                                         "body_separate_capture", "original_flow_ordinal",
                                         "diagnostic_offset", "remaining_after_reuse",
                                         "resource_exhausted", "original_owned_proof"});
        auto yes = [&](std::string_view key) {
            const auto v = info.get(key);
            require(v.is_bool(), "FlowBody2 strict owned bool");
            return v.as_bool();
        };
        const bool bound = yes("descriptor_bound"), admitted = yes("body_admitted"),
                   separate = yes("body_separate_capture"), resource = yes("resource_exhausted");
        require(!separate &&
                    flow_oracle::flow400_number(info.get("original_flow_ordinal")) == pin.ordinal &&
                    flow_oracle::flow400_number(info.get("diagnostic_offset")) == 0u &&
                    flow_oracle::flow400_number(info.get("remaining_after_reuse")) <=
                        flow_oracle::kFlow400SceneLimit,
                "FlowBody2 strict same-run domain");
        totals.bound += bound;
        totals.resources += resource;
        totals.errors += !row.errors.empty();
        const auto proof = info.get("original_owned_proof");
        if (admitted) {
            require(bound && !resource && row.errors.empty() &&
                        row.source_sha256 == pin.source_sha &&
                        row.role_model_sha == pin.model_sha &&
                        row.role_body_sha == pin.source_sha && proof.is_obj(),
                    "FlowBody2 admitted bit cannot bypass identity/binding");
            OfficialFlowCounts checked;
            official_flow_accumulate_owned(proof, row, checked);
            require(checked.observed == 1u && checked.bound == 1u && checked.decoded == 1u &&
                        checked.pending == 0u && checked.admitted == 1u && checked.mutated == 1u &&
                        checked.errors == 0u && checked.resources == 0u,
                    "FlowBody2 admitted bit cannot bypass original complete proof");
            ++totals.observed;
            ++totals.decoded;
            ++totals.admitted;
            ++totals.mutated;
        } else
            require(proof.is_null() && !row.errors.empty(),
                    "FlowBody2 failed reuse must retain a mandatory error");
    }
    return totals;
}

TEST(MermaidOfficialFlowBody2, SameRunCompleteProofRequiresExactBodyAndRestoration) {
    auto docs = flow_oracle::flow400_documents();
    flow_oracle::OracleWork work{flow_oracle::kFlow400SceneLimit};
    work.spend(kOfficialFlowReportReserve + 8u * sizeof(Observation) +
               8u * sizeof(OfficialFlowProgress) + 8u * sizeof(OfficialFlowFrames) + 16384u);
    const auto fixture = docs.manifest.root().get("fixtures").at(662u);
    const auto& pin = kFlowRepeatBody2Pins[0];
    work.product(pin.bytes, 32u);
    work.product(pin.path.size(), 12u);
    work.product(kSentinel.size(), 8u);
    work.spend(1024u);
    const auto source = read_file(kRoot / "cases" / pin.path);
    std::vector<Observation> records;
    flow_body2_reserve(records, work);
    std::ostringstream diagnostic;
    auto original = run_official_flow400_case(fixture, 662u, 1u, source, docs, work, diagnostic,
                                              records.size(), records.capacity());
    ASSERT_TRUE(original.errors.empty());
    const auto good = reuse_flow_body2(fixture, 662u, 0u, source, source, original, docs, work);
    ASSERT_TRUE(good.errors.empty());
    flow_body2_copy_charge(good, work);
    records.push_back(good);
    const auto counted = flow_body2_counts(records);
    ASSERT_EQ(counted.admitted, 1u);
    auto rejected = [&](Observation wrong, JsonVal described, std::string_view actual,
                        std::string_view suffix) {
        flow_body2_reserve(records, work);
        const auto bad = reuse_flow_body2(described, 662u, 0u, actual, suffix, wrong, docs, work);
        EXPECT_FALSE(bad.errors.empty());
        EXPECT_GT(work.remaining, 0u);
        const auto metadata = json::parse(bad.raw_source_constraints);
        ASSERT_TRUE(metadata.has_value());
        EXPECT_FALSE(metadata->root().get("body_admitted").as_bool());
        EXPECT_FALSE(metadata->root().get("resource_exhausted").as_bool());
    };
    // Callback lambda owns copies only after the caller pays their real fields.
    for (const auto flag :
         {"restored_cells_equal", "semantic_mutations_complete", "five_captures_complete"}) {
        flow_body2_copy_charge(original, work);
        auto wrong = original;
        work.product(original.raw_source_constraints.size(), 24u);
        work.spend(8192u);
        wrong.raw_source_constraints =
            projection_mutate(original.raw_source_constraints, [&](json::JsonMutVal info) {
                info.set(flag, false);
                info.set("whole_source_admitted", true);
            });
        rejected(std::move(wrong), fixture, source, source);
    }
    {
        flow_body2_copy_charge(original, work);
        auto wrong = original;
        work.spend(256u);
        wrong.path = "flowchart/foreign.mmd";
        rejected(std::move(wrong), fixture, source, source);
    }
    {
        flow_body2_copy_charge(original, work);
        auto wrong = original;
        work.spend(256u);
        wrong.track = "body_diagnostic";
        rejected(std::move(wrong), fixture, source, source);
    }
    {
        flow_body2_copy_charge(original, work);
        auto wrong = original;
        work.spend(256u);
        wrong.role_model_sha = std::string(64u, '0');
        rejected(std::move(wrong), fixture, source, source);
    }
    {
        flow_body2_copy_charge(original, work);
        auto wrong = original;
        work.product(original.raw_source_constraints.size(), 24u);
        work.spend(8192u);
        wrong.raw_source_constraints =
            projection_mutate(original.raw_source_constraints, [](json::JsonMutVal info) {
                info.set("whole_source_admitted", false);
            });
        rejected(std::move(wrong), fixture, source, source);
    }
    {
        flow_body2_copy_charge(original, work);
        work.product(source.size() + 1u, 4u);
        work.spend(256u);
        const auto changed = source + " ";
        rejected(original, fixture, changed, changed);
    }
    {
        flow_body2_copy_charge(original, work);
        work.spend(65536u);
        const auto changed =
            projection_mutate(projection_canonical(fixture),
                              [](json::JsonMutVal row) { row.get("diagnostic").set("offset", 1); });
        const auto parsed = json::parse(changed);
        ASSERT_TRUE(parsed.has_value());
        rejected(original, parsed->root(), source, std::string_view(source).substr(1u));
    }
    {
        flow_body2_copy_charge(good, work);
        auto forged = good;
        work.product(good.raw_source_constraints.size(), 24u);
        work.spend(8192u);
        forged.raw_source_constraints =
            projection_mutate(good.raw_source_constraints, [](json::JsonMutVal v) {
                v.get("original_owned_proof").set("restored_cells_equal", false);
                v.set("body_admitted", true);
            });
        work.spend(4u * sizeof(Observation) + 8192u);
        std::vector<Observation> one;
        one.push_back(std::move(forged));
        bool denied = false;
        try {
            static_cast<void>(flow_body2_counts(one));
        } catch (const std::exception& error) {
            EXPECT_GT(work.remaining, 0u);
            EXPECT_FALSE(flow_oracle::flow400_resource(error.what()));
            denied = true;
        }
        EXPECT_TRUE(denied);
    }
    {
        flow_body2_copy_charge(good, work);
        flow_body2_copy_charge(good, work);
        work.spend(8u * sizeof(Observation) + 8192u);
        std::vector<Observation> duplicate_rows;
        duplicate_rows.reserve(2u);
        duplicate_rows.push_back(good);
        duplicate_rows.push_back(good);
        bool denied = false;
        try {
            static_cast<void>(flow_body2_counts(duplicate_rows));
        } catch (const std::exception& error) {
            EXPECT_FALSE(flow_oracle::flow400_resource(error.what()));
            denied = true;
        }
        EXPECT_TRUE(denied);
    }
    std::array<bool, 2> used{};
    flow_body2_consume(used, 0u, work);
    bool duplicate = false;
    try {
        flow_body2_consume(used, 0u, work);
    } catch (const std::exception& error) {
        EXPECT_FALSE(flow_oracle::flow400_resource(error.what()));
        duplicate = true;
    }
    EXPECT_TRUE(duplicate);
    EXPECT_FALSE(used[1]);
    flow_oracle::OracleWork zero{0u};
    bool resource = false;
    try {
        flow_body2_bind(fixture, 662u, 0u, source, source, docs, zero);
    } catch (const std::exception& error) {
        resource = flow_oracle::flow400_resource(error.what());
    }
    EXPECT_TRUE(resource);
    // Reserve the failure record and its bounded checks on this same scene
    // ledger, then exhaust it. No new allowance or failed-ledger retry exists.
    std::vector<Observation> exhausted_rows;
    flow_body2_reserve(exhausted_rows, work);
    work.spend(65536u + 4u * sizeof(OfficialFlowCounts) + 16u * sizeof(JsonVal) + 4096u);
    work.spend(work.remaining);
    EXPECT_EQ(work.remaining, 0u);
    auto exhausted = reuse_flow_body2(fixture, 662u, 0u, source, source, original, docs, work);
    ASSERT_FALSE(exhausted.errors.empty());
    EXPECT_EQ(exhausted.track, "body_diagnostic");
    const auto exhausted_metadata = json::parse(exhausted.raw_source_constraints);
    ASSERT_TRUE(exhausted_metadata.has_value());
    const auto failure = exhausted_metadata->root();
    EXPECT_FALSE(failure.get("descriptor_bound").as_bool());
    EXPECT_FALSE(failure.get("body_admitted").as_bool());
    EXPECT_TRUE(failure.get("resource_exhausted").as_bool());
    EXPECT_TRUE(failure.get("original_owned_proof").is_null());
    EXPECT_EQ(failure.get_int("remaining_after_reuse"), 0);
    exhausted_rows.push_back(std::move(exhausted));
    const auto failed_counts = flow_body2_counts(exhausted_rows);
    EXPECT_EQ(exhausted_rows.size(), 1u);
    EXPECT_EQ(failed_counts.visited, 1u);
    EXPECT_EQ(failed_counts.admitted, 0u);
    EXPECT_EQ(failed_counts.errors, 1u);
    EXPECT_EQ(failed_counts.resources, 1u);
    EXPECT_EQ(work.remaining, 0u);
}
}  // namespace

// Original206 ER consumer only. No Native report or frame-derived expectation.
namespace {
void official_er_error(Observation& row, std::string_view error) {
    if (row.errors.size() < 6u) row.errors.emplace_back(error.substr(0u, 128u));
}

// Only immutable, already approved ER206 whole_Model JSON enters this local
// applicability calculation. It neither decodes a second Model nor reads art.
relations_oracle::Class481Mutations official_er_expected_mutations(JsonVal meaning) {
    using namespace relations_oracle;
    const auto entities = meaning.get("entities"), relations = meaning.get("relations"),
               groups = meaning.get("groups");
    require(meaning.get("family").as_str() == "er" && entities.is_arr() && relations.is_arr() &&
                groups.is_arr() && entities.size() <= 8u && relations.size() <= 8u &&
                groups.size() <= 2u,
            "OfficialER206 mutation source bounds");
    Class481Mutations plan;
    std::size_t attributes = 0;
    auto add = [&](std::string_view key) { ++plan.executed[std::string(key)]; };
    auto skip = [&](std::string_view key) { ++plan.skipped[std::string(key)]; };
    entities.iter([&](JsonVal entity) {
        const auto rows = entity.get("rows");
        require(rows.is_arr() && rows.size() <= 6u && attributes + rows.size() <= 6u,
                "OfficialER206 mutation attribute bounds");
        attributes += rows.size();
        add("complete_header");
        rows.iter([&](JsonVal row) {
            const auto keys = row.get("source_fields").get("keys");
            require(keys.is_arr() && keys.size() <= 3u, "OfficialER206 mutation key bounds");
            add("attribute_type");
            add("attribute_name");
            add("attribute_comment");
            add("attribute_keys");
            add("attribute_erasure");
            if (keys.size() > 0u) add("attribute_key_count");
            if (keys.size() > 1u && keys.at(0u).as_str() != keys.at(1u).as_str())
                add("ordered_keys");
            else
                skip("no_distinct_ordered_key_pair");
        });
        if (!entity.get("owner").as_str().empty())
            add("group_membership");
        else
            skip("root_entity_has_no_membership_to_erase");
        const auto id = entity.get("id").as_str();
        bool incident = false;
        relations.iter([&](JsonVal edge) {
            if (edge.get("source").as_str() == id || edge.get("target").as_str() == id)
                incident = true;
        });
        if (!incident)
            add("isolated_entity_erasure");
        else
            skip("incident_entity_needs_coherent_graph_removal");
    });
    // er296_typed_model changes only these outer ASCII role-label bytes. In
    // particular, raw all-SPACE labels must compare as empty duplicate roles.
    auto label = [](JsonVal edge) {
        const auto raw = edge.get("label").as_str();
        const auto first = raw.find_first_not_of(" \t\r");
        if (first == std::string_view::npos) return std::string_view{};
        const auto last = raw.find_last_not_of(" \t\r");
        return raw.substr(first, last - first + 1u);
    };
    relations.iter([&](JsonVal edge) {
        add("relation_label");
        add("start_cardinality");
        add("end_cardinality");
        add("stroke");
        const auto from = edge.get("source").as_str(), to = edge.get("target").as_str();
        bool third = false;
        entities.iter([&](JsonVal entity) {
            const auto id = entity.get("id").as_str();
            if (id != from && id != to) third = true;
        });
        if (third)
            add("third_endpoint");
        else
            skip("no_third_entity");
        std::size_t same = 0;
        relations.iter([&](JsonVal other) {
            if (other.get("source").as_str() == from && other.get("target").as_str() == to &&
                label(other) == label(edge) &&
                other.get("start").as_str() == edge.get("start").as_str() &&
                other.get("end").as_str() == edge.get("end").as_str() &&
                other.get("stroke").as_str() == edge.get("stroke").as_str())
                ++same;
        });
        if (same == 1u)
            add("relation_erasure");
        else
            skip("identical_duplicate_not_independent_ink");
    });
    groups.iter([&](JsonVal group) {
        add("group_caption");
        if (!group.get("parent").as_str().empty())
            add("group_parent");
        else
            skip("root_group_has_no_parent_to_erase");
    });
    add("orphan_ink");
    return plan;
}

std::size_t official_er_typed_count(const relations_oracle::Class481Mutations& plan) {
    std::size_t typed = 0;
    for (const auto& [kind, count] : plan.executed)
        if (kind != "orphan_ink") typed += static_cast<std::size_t>(count);
    return typed;
}

std::string official_er_mutation_digest(const relations_oracle::Class481Mutations& plan) {
    require(plan.executed.size() <= 32u && plan.skipped.size() <= 32u,
            "OfficialER206 mutation count-map bounds");
    std::string text;
    text.reserve(4096u);
    text = "ER206MutationCounts/v1\nexecuted\n";
    auto append = [&](const auto& counts) {
        for (const auto& [key, count] : counts) {
            require(key.size() <= 64u && count > 0 && count <= 512,
                    "OfficialER206 mutation entry bounds");
            text.append(key);
            text.push_back('=');
            text.append(std::to_string(count));
            text.push_back(';');
        }
    };
    append(plan.executed);
    text.append("\nskipped\n");
    append(plan.skipped);
    require(text.size() <= 4096u, "OfficialER206 mutation canonical bytes bound");
    return digest(text);
}

void official_er_complete_mutations(const relations_oracle::Class481Mutations& actual,
                                    const relations_oracle::Class481Mutations& expected) {
    require(actual.errors.empty() && actual.executed == expected.executed &&
                actual.skipped == expected.skipped,
            "OfficialER206 source-bound complete mutation execution");
}

// Each source's fixed reserve pays this <=4096-byte owned value and its later
// bounded summary/provenance parses/copies, including after Work exhaustion.
std::string official_er_info(const OfficialErProgress& p, const Observation& row,
                             std::size_t global, std::size_t left) {
    json::JsonMutDoc doc;
    auto info = doc.object();
    info.set("source_case_ordinal", static_cast<int64_t>(row.role_case_index));
    info.set("original_global_ordinal", static_cast<int64_t>(global));
    info.set("source_sha256", row.source_sha256);
    info.set("whole_Model_sha256", row.role_model_sha);
    info.set("source_authority_sha256", kOfficialEr206AuthoritySha);
    info.set("foreign_assertions_certified", 0);
    info.set("captures", static_cast<int64_t>(p.captures));
    info.set("published_frames", static_cast<int64_t>(p.published));
    info.set("source_bound", p.bound);
    info.set("whole_Model_decoded", p.decoded);
    info.set("whole_null_pending", p.pending);
    info.set("whole_source_admitted", p.admitted);
    info.set("restored_cells_equal", p.restored);
    info.set("narrow_ordinary_protocol", p.narrow_ordinary);
    info.set("wide_semantic_footer", p.wide_good);
    info.set("narrow_semantic_footer", p.narrow_good);
    info.set("restored_semantic_footer", p.restored_good);
    info.set("semantic_mutations_complete", p.mutated);
    info.set("typed_rejections", static_cast<int64_t>(p.typed_rejections));
    info.set("physical_orphan_rejections", static_cast<int64_t>(p.orphan_rejections));
    info.set("complete_mutation_counts_sha256", p.mutation_proof_sha);
    info.set("mandatory_positive_source_fallback", p.positive_unmet);
    info.set("exception", p.exception);
    info.set("resource_exhausted", p.resource);
    info.set("height", p.height);
    info.set("phase", static_cast<int>(p.phase));
    info.set("initial_budget", static_cast<int64_t>(relations_oracle::kEr206SceneLimit));
    info.set("remaining", static_cast<int64_t>(left));
    auto costs = info.make_arr();
    for (const auto& n : p.remaining) {
        if (n)
            costs.append(info.make_num(static_cast<int64_t>(*n)));
        else
            costs.append(info.make_null());
    }
    info.add("phase_remaining", costs);
    doc.set_root(info);
    auto text = doc.to_string();
    require(text.size() <= 4096u, "OfficialER206 prepaid owned metadata bound");
    return text;
}

void official_er_publish_frame(json::JsonMutVal parent, std::string_view key,
                               const relations_oracle::Grid& grid, relations_oracle::Work& work) {
    // Same actual UTF8 row-array tariff as Native official285_publish_frame.
    // Full Grid cells remain the checker authority; rows are not cell replay.
    work.spend(8u * sizeof(std::size_t) + sizeof(std::string) + 128u);
    require(grid.width > 0 && grid.width <= 240 && grid.height > 0 && grid.height <= 1040 &&
                grid.cells.size() ==
                    static_cast<std::size_t>(grid.width) * static_cast<std::size_t>(grid.height),
            "OfficialER206 publication Grid bounds");
    std::size_t raw = 0, escaped = static_cast<std::size_t>(grid.height) * 3u + 2u;
    for (const auto& cell : grid.cells) {
        stage_b_product(work, cell.size(), 1u);
        work.spend(1u);
        raw += cell.size();
        for (const unsigned char c : cell) {
            if (c == '"' || c == '\\' || c == '\b' || c == '\f' || c == '\n' || c == '\r' ||
                c == '\t')
                escaped += 2u;
            else
                escaped += c < 0x20 ? 6u : 1u;
        }
    }
    stage_b_product(work, grid.cells.size(), 2u);
    stage_b_product(work, raw, 4u);
    stage_b_product(work, escaped, 3u);
    stage_b_product(work, static_cast<std::size_t>(grid.height),
                    sizeof(std::string) + sizeof(std::size_t) + 128u);
    work.spend(1024u);
    auto rows = parent.make_arr();
    for (int y = 0; y < grid.height; ++y) {
        std::size_t bytes = 0;
        for (int x = 0; x < grid.width; ++x)
            bytes += grid.at({x, y}).size();
        std::string row;
        row.reserve(bytes);
        for (int x = 0; x < grid.width; ++x)
            row.append(grid.at({x, y}));
        rows.append(rows.make_str(row));
    }
    parent.add(key, rows);
}

OfficialErFrames official_er_capture(std::string_view input, relations_oracle::Work& work,
                                     OfficialErProgress& p, json::JsonMutVal data) {
    work.spend((input.size() + 128u) * (32u * sizeof(std::string) + 256u) + 8192u +
               8u * sizeof(OfficialErFrames) + 8u * sizeof(int));
    require(input.size() <= 820u, "OfficialER206 complete source bound");
    auto node = loom::ui::render_markdown(fenced(input, "mermaid", kSentinel)),
         plain = loom::ui::render_markdown(fenced(input, "text", kSentinel));
    node->ComputeRequirement();
    plain->ComputeRequirement();
    const int required = std::max({1, node->requirement().min_y, plain->requirement().min_y});
    require(required < 1040, "OfficialER206 complete viewport limit exceeded");
    p.height = required + 1;
    OfficialErFrames frames;
    constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
    constexpr std::array<std::string_view, 5> names{{"wide_frame", "narrow_frame", "restored_frame",
                                                     "ordinary_wide_frame",
                                                     "ordinary_narrow_frame"}};
    data.set("height", p.height);
    auto dimensions = data.make_arr();
    for (const auto w : widths)
        dimensions.append(dimensions.make_num(w));
    data.add("frame_widths", dimensions);
    for (std::size_t n = 0; n < frames.size(); ++n) {
        p.phase = OfficialErPhase::Capture;
        frames[n] =
            relations_oracle::note13_capture(n < 3u ? node : plain, widths[n], p.height, work);
        ++p.captures;
        p.phase = OfficialErPhase::Publish;
        official_er_publish_frame(data, names[n], *frames[n], work);
        ++p.published;
    }
    p.remaining[1] = work.left;
    p.remaining[2] = work.left;
    return frames;
}

bool official_er_same(const relations_oracle::Grid& a, const relations_oracle::Grid& b,
                      relations_oracle::Work& work) {
    work.spend(8u * sizeof(std::size_t) + 128u);
    if (a.width != b.width || a.height != b.height || a.cells.size() != b.cells.size())
        return false;
    for (std::size_t n = 0; n < a.cells.size(); ++n) {
        stage_b_product(work, a.cells[n].size(), 2u);
        stage_b_product(work, b.cells[n].size(), 2u);
        work.spend(8u);
    }
    return a.cells == b.cells;
}

void official_er_geometry(const OfficialErFrames& frames, const relations_oracle::Model& model,
                          JsonVal meaning, std::size_t encoded, relations_oracle::Work& work,
                          Observation& row, OfficialErProgress& p) {
    using namespace relations_oracle;
    work.spend(8u * sizeof(OfficialErProgress) + 4u * sizeof(std::optional<int>) + 512u);
    for (std::size_t n = 5u; n < p.remaining.size(); ++n)
        p.remaining[n].reset();
    p.positive_unmet = false;
    p.admitted = false;
    p.wide_good = false;
    p.narrow_good = false;
    p.restored_good = false;
    p.narrow_ordinary = false;
    p.mutated = false;
    p.typed_rejections = 0;
    p.orphan_rejections = 0;
    p.mutation_proof_sha.clear();
    p.phase = OfficialErPhase::Compare;
    p.restored = official_er_same(*frames[0], *frames[2], work);
    const bool wide_source = official_er_same(*frames[0], *frames[3], work),
               narrow_source = official_er_same(*frames[1], *frames[4], work);
    p.remaining[5] = work.left;
    auto drawn = [&](std::size_t n, OfficialErPhase phase, std::size_t cost) {
        p.phase = phase;
        const auto physical = following_row(*frames[n], work, kSentinel);
        if (!physical) {
            official_er_error(row, "OfficialER206 drawn physical following role");
            return false;
        }
        work.spend(4u * sizeof(std::optional<int>) + 128u);
        std::optional<int> footer;
        const auto errors = validate_impl(*frames[n], model, work, false, nullptr, false, nullptr,
                                          kSentinel, &footer);
        for (const auto& error : errors) {
            stage_b_product(work, error.size(), 8u);
            work.spend(16u);
            if (class481_resource(error)) p.resource = true;
        }
        class481_no_resource(errors);
        if (!errors.empty()) {
            official_er_error(row, errors.front());
            return false;
        }
        if (!footer || footer != physical) {
            official_er_error(row, "OfficialER206 distinct semantic following role missing");
            return false;
        }
        p.remaining[cost] = work.left;
        return true;
    };
    if (wide_source) {
        p.positive_unmet = true;
        if (following_row(*frames[0], work, kSentinel)) row.status[0] = "source_fallback";
        official_er_error(row, "OfficialER206 mandatory complete positive remains source fallback");
    } else if ((p.wide_good = drawn(0u, OfficialErPhase::Wide, 6u)))
        row.status[0] = "drawn";
    p.phase = OfficialErPhase::Narrow;
    if (narrow_source) {
        p.narrow_ordinary = following_row(*frames[1], work, kSentinel).has_value();
        if (p.narrow_ordinary)
            row.status[1] = "source_fallback";
        else
            official_er_error(row, "OfficialER206 exact ordinary narrow following role missing");
        p.remaining[7] = work.left;
    } else if ((p.narrow_good = drawn(1u, OfficialErPhase::Narrow, 7u)))
        row.status[1] = "drawn";
    // A restored source remains unmet even if its cells equal a failed wide.
    if (!wide_source && (p.restored_good = drawn(2u, OfficialErPhase::Restored, 8u)) && p.restored)
        row.status[2] = "drawn";
    if (wide_source && p.restored && following_row(*frames[2], work, kSentinel))
        row.status[2] = "source_fallback";
    if (!p.restored)
        official_er_error(row, "OfficialER206 same-node restoration changed complete Grid");
    if (p.wide_good && (p.narrow_good || p.narrow_ordinary) && p.restored_good && p.restored &&
        row.errors.empty()) {
        p.phase = OfficialErPhase::Mutations;
        const auto expected = official_er_expected_mutations(meaning);
        const auto changes = er206_mutations(*frames[0], model, encoded, work, kSentinel);
        work.spend((changes.executed.size() + changes.skipped.size()) * 1024u + 8192u);
        official_er_complete_mutations(changes, expected);
        p.typed_rejections = official_er_typed_count(changes);
        p.orphan_rejections = static_cast<std::size_t>(changes.executed.at("orphan_ink"));
        p.mutation_proof_sha = official_er_mutation_digest(changes);
        require(p.typed_rejections > 0u && p.orphan_rejections == 1u,
                "OfficialER206 complete coherent/physical mutants missing");
        p.mutated = true;
        p.remaining[9] = work.left;
    }
    p.admitted = p.captures == 5u && p.published == 5u && p.bound && p.decoded && !p.pending &&
                 !p.exception && !p.resource && p.wide_good &&
                 (p.narrow_good || p.narrow_ordinary) && p.restored_good && p.restored &&
                 p.mutated && row.errors.empty();
}

Observation run_official_er206_case(JsonVal fixture, std::size_t global, std::size_t ordinal,
                                    const relations_oracle::Er206Documents& docs,
                                    std::array<bool, 206>& consumed, relations_oracle::Work& work,
                                    std::ostream& diagnostics, std::size_t size,
                                    std::size_t capacity, JsonVal er107_local_proof) {
    using namespace relations_oracle;
    work.spend(kOfficialEr206ReportReserve + 8u * sizeof(Observation) +
               8u * sizeof(OfficialErProgress) + 8u * sizeof(OfficialErFrames) + 16384u +
               6u * 4096u);
    if (ordinal < kEr206Pins.size() && kEr206Pins[ordinal].model_bytes > 0u)
        work.spend(4u * kOfficialErMutationPlanCharge);
    Observation row;
    row.errors.reserve(6u);
    OfficialErProgress p;
    row.raw_active_schema = kOfficialEr206Contract;
    row.raw_source_authority = kOfficialEr206Authority;
    row.track = "original";
    row.type = "er-diagram";
    row.role_case_index = ordinal;
    row.expected = "drawn";
    row.expected_states = {"drawn", "drawn_or_exact_source", "drawn"};
    row.reason = "Complete original ER whole-scene contract; original foreign assertions pending";
    row.decision_reasons.fill(
        "Complete ER typed geometry, independent following, restoration and meaningful negatives");
    row.decision_reasons[1] = "Predeclared40-column full ER geometry OR exact ordinary source";
    json::JsonMutDoc diagnostic;
    auto data = diagnostic.object();
    try {
        work.spend(8192u + 16u * sizeof(std::size_t));
        require(ordinal < kEr206Pins.size() && size <= 2738u && capacity <= 8192u,
                "OfficialER206 source/vector bounds");
        const auto& pin = kEr206Pins[ordinal];
        static_assert(std::is_nothrow_move_constructible_v<Observation>);
        const auto slots = size == capacity ? std::max(capacity * 2u, size + 1u) : 0u;
        stage_b_product(work, slots + (slots ? size : 0u) + 1u, 2u * sizeof(Observation) + 16u);
        const auto path = class481_text(fixture.get("path"), work, 512u);
        const auto bytes = fixture.get("bytes");
        require(bytes.is_int() && bytes.as_int() > 0 && bytes.as_int() <= 820 &&
                    path.starts_with("er-diagram/") && safe_relative(path) &&
                    !fixture.has("diagnostic"),
                "OfficialER206 finite original-only acquire");
        work.spend(path.size() * 12u + static_cast<std::size_t>(bytes.as_int()) * 32u + 8192u);
        row.path = path;
        const auto actual = kRoot / "cases" / row.path;
        require(fs::file_size(actual) == static_cast<std::size_t>(bytes.as_int()),
                "OfficialER206 original file acquire size");
        const auto input = read_file(actual);
        stage_b_product(work, input.size(), 4u);
        work.spend(1024u);
        row.source_sha256 = digest(input);
        row.projection_baseline = class481_canonical(fixture.get("raw"), work);
        require(row.projection_baseline.size() <= 1024u,
                "OfficialER206 bounded historical raw policy");
        stage_b_product(work, row.projection_baseline.size(), 32u);
        work.spend(4096u);
        data.set("kind", "official_ER206_actual_scene/v1");
        data.set("ordinal", static_cast<int64_t>(ordinal));
        data.set("manifest_global_ordinal", static_cast<int64_t>(global));
        data.set("fixture", row.path);
        data.set("source", input);
        data.set("source_sha256", row.source_sha256);
        p.remaining[0] = work.left;
        auto frames = official_er_capture(input, work, p, data);
        p.phase = OfficialErPhase::Bind;
        work.spend(ordinal + pin.global + 8192u);
        require(p.captures == 5u && p.published == 5u,
                "OfficialER206 capture/publication precedes own binding");
        const auto current = class481_canonical(fixture, work);
        stage_b_product(work, current.size(), 4u);
        work.spend(1024u);
        require(global == pin.global && row.path == pin.path && digest(current) == pin.manifest_sha,
                "OfficialER206 exact current manifest tuple");
        const auto source = docs.authority.root().get("cases").at(ordinal);
        require(source.get("source").is_str() && source.get("source").as_str() == input,
                "OfficialER206 observed source equals complete authority");
        const auto meaning = er206_binding(source, ordinal, docs, work);
        er206_consume(ordinal, consumed, work);
        require(input.find(kSentinel) == std::string::npos,
                "OfficialER206 frozen original token collision requires review");
        row.role_model_sha = pin.model_sha;
        p.bound = true;
        p.remaining[3] = work.left;
        if (meaning.is_null()) {
            p.pending = true;
            row.expected = "pending";
            row.expected_states.fill("unadmitted_whole_source");
            row.reason = "Whole-null ER original retains complete source/config/foreign debts; no "
                         "art or empty credit";
            p.phase = OfficialErPhase::Compare;
            p.restored = official_er_same(*frames[0], *frames[2], work);
            const bool wide = official_er_same(*frames[0], *frames[3], work),
                       narrow_source = official_er_same(*frames[1], *frames[4], work);
            const bool a = following_row(*frames[0], work, kSentinel).has_value(),
                       b = following_row(*frames[1], work, kSentinel).has_value(),
                       c = following_row(*frames[2], work, kSentinel).has_value();
            p.narrow_ordinary = narrow_source && b;
            if (er107_local_proof.is_obj()) {
                // A second independently captured source profile can explain
                // new art. The frozen ER206 Model stays null/pending and its
                // decoded/admitted/mutation flags remain false.
                require(official_er107::owned(er107_local_proof) &&
                            er107_local_proof.get_string("path") == row.path &&
                            er107_local_proof.get_string("source_sha256") == row.source_sha256 &&
                            er107_local_proof.get_int("prior_ER206_ordinal") ==
                                static_cast<int64_t>(ordinal) &&
                            er107_local_proof.get_int("manifest_global_ordinal") ==
                                static_cast<int64_t>(global),
                        "OfficialER206 supplemental ER107 exact owned source proof");
                require(!wide && p.restored && a && b && c &&
                            p.narrow_ordinary ==
                                er107_local_proof.get("narrow_exact_ordinary").as_bool(),
                        "OfficialER206 supplemental ER107 actual wide/narrow/restored agreement");
                row.status = {"drawn", p.narrow_ordinary ? "source_fallback" : "drawn", "drawn"};
                data.set("supplemental_local_contract", kOfficialEr107Contract);
                data.set("supplemental_formal_admission", false);
            } else {
                if (wide && a) row.status[0] = "source_fallback";
                if (p.narrow_ordinary) row.status[1] = "source_fallback";
                if (wide && c && p.restored) row.status[2] = "source_fallback";
            }
            if (row.status[0] == "error" || row.status[1] == "error" || row.status[2] == "error")
                official_er_error(
                    row, "OfficialER206 whole-null art/ordinary/restore unmet; never admission");
            p.remaining[5] = work.left;
        } else {
            p.phase = OfficialErPhase::Decode;
            const auto model = er206_decode(meaning, pin, work);
            p.decoded = true;
            p.remaining[4] = work.left;
            official_er_geometry(frames, model, meaning, pin.model_bytes, work, row, p);
        }
        p.phase = OfficialErPhase::Complete;
    } catch (const std::exception& error) {
        p.exception = true;
        const char* text = error.what();
        std::size_t n = 0;
        while (text && n < 128u && text[n])
            ++n;
        const std::string_view bounded =
            text ? std::string_view(text, n)
                 : std::string_view("OfficialER206 unobservable exception");
        p.resource = p.resource || work.left == 0u || class481_resource(bounded);
        official_er_error(row, bounded);
        p.admitted = false;
    }
    row.raw_source_constraints = official_er_info(p, row, global, work.left);
    data.set("contract", kOfficialEr206Contract);
    data.set("actual", row.raw_source_constraints);
    data.set("foreign_assertions_certified", 0);
    auto failures = data.make_arr();
    for (const auto& error : row.errors)
        failures.append(failures.make_str(error));
    data.add("errors", failures);
    diagnostic.set_root(data);
    diagnostics << diagnostic.to_string() << '\n';
    require(static_cast<bool>(diagnostics), "OfficialER206 actual row-array evidence write failed");
    return row;
}

void official_er_accumulate_owned(JsonVal info, const Observation& row, OfficialErCounts& out,
                                  const relations_oracle::Er206Documents& docs) {
    constexpr std::array<std::string_view, 29> keys{{"source_case_ordinal",
                                                     "original_global_ordinal",
                                                     "source_sha256",
                                                     "whole_Model_sha256",
                                                     "source_authority_sha256",
                                                     "foreign_assertions_certified",
                                                     "captures",
                                                     "published_frames",
                                                     "source_bound",
                                                     "whole_Model_decoded",
                                                     "whole_null_pending",
                                                     "whole_source_admitted",
                                                     "restored_cells_equal",
                                                     "narrow_ordinary_protocol",
                                                     "wide_semantic_footer",
                                                     "narrow_semantic_footer",
                                                     "restored_semantic_footer",
                                                     "semantic_mutations_complete",
                                                     "typed_rejections",
                                                     "physical_orphan_rejections",
                                                     "complete_mutation_counts_sha256",
                                                     "mandatory_positive_source_fallback",
                                                     "exception",
                                                     "resource_exhausted",
                                                     "height",
                                                     "phase",
                                                     "initial_budget",
                                                     "remaining",
                                                     "phase_remaining"}};
    require(info.is_obj() && info.size() == keys.size() &&
                std::ranges::all_of(keys, [&](std::string_view key) { return info.has(key); }),
            "OfficialER206 owned exact fields");
    auto number = [&](std::string_view key, std::size_t maximum) {
        const auto v = info.get(key);
        require(v.is_int() && v.as_int() >= 0 && static_cast<std::size_t>(v.as_int()) <= maximum,
                "OfficialER206 owned integer bounds");
        return static_cast<std::size_t>(v.as_int());
    };
    auto yes = [&](std::string_view key) {
        const auto v = info.get(key);
        require(v.is_bool(), "OfficialER206 owned bool type");
        return v.as_bool();
    };
    require(row.role_case_index < relations_oracle::kEr206Pins.size(),
            "OfficialER206 owned ordinal domain");
    const auto& pin = relations_oracle::kEr206Pins[row.role_case_index];
    require(row.track == "original" && row.type == "er-diagram" && row.path == pin.path &&
                row.raw_source_authority == kOfficialEr206Authority &&
                number("source_case_ordinal", 205u) == row.role_case_index &&
                number("original_global_ordinal", 2144u) == pin.global,
            "OfficialER206 owned exact source tuple");
    require(info.get("source_authority_sha256").is_str() &&
                info.get("source_authority_sha256").as_str() == kOfficialEr206AuthoritySha &&
                number("foreign_assertions_certified", 0u) == 0u,
            "OfficialER206 owned authority and foreign scope");
    const bool bound = yes("source_bound"), decoded = yes("whole_Model_decoded"),
               pending = yes("whole_null_pending"), admitted = yes("whole_source_admitted"),
               restored = yes("restored_cells_equal"), ordinary = yes("narrow_ordinary_protocol"),
               wide = yes("wide_semantic_footer"), narrow = yes("narrow_semantic_footer"),
               back = yes("restored_semantic_footer"), mutated = yes("semantic_mutations_complete"),
               unmet = yes("mandatory_positive_source_fallback"), exception = yes("exception"),
               resource = yes("resource_exhausted");
    const auto captured = number("captures", 5u), published = number("published_frames", 5u),
               typed = number("typed_rejections", 512u),
               orphan = number("physical_orphan_rejections", 1u);
    require(published <= captured && !(decoded && pending) && (!decoded || bound) &&
                (!pending || bound) && (!bound || (captured == 5u && published == 5u)),
            "OfficialER206 owned lifecycle order");
    const auto source = info.get("source_sha256"), model = info.get("whole_Model_sha256");
    require(source.is_str() && model.is_str() && source.as_str() == row.source_sha256 &&
                model.as_str() == row.role_model_sha,
            "OfficialER206 owned copied identity fields");
    if (bound)
        require(row.source_sha256 == pin.source_sha && row.role_model_sha == pin.model_sha &&
                    pending == (pin.model_bytes == 0u),
                "OfficialER206 owned approved source/Model identity");
    require(!decoded || pin.model_bytes > 0u,
            "OfficialER206 owned null cannot decode or admit art");
    require(number("initial_budget", relations_oracle::kEr206SceneLimit) ==
                relations_oracle::kEr206SceneLimit,
            "OfficialER206 owned single initial budget");
    const auto left = number("remaining", relations_oracle::kEr206SceneLimit);
    number("height", 1040u);
    number("phase", static_cast<std::size_t>(OfficialErPhase::Complete));
    const auto costs = info.get("phase_remaining");
    require(costs.is_arr() && costs.size() == 10u, "OfficialER206 owned phase ledger shape");
    std::size_t previous = relations_oracle::kEr206SceneLimit;
    costs.iter([&](JsonVal v) {
        if (v.is_null()) return;
        require(v.is_int() && v.as_int() >= 0 && static_cast<std::size_t>(v.as_int()) <= previous,
                "OfficialER206 owned phase ledger monotonic");
        previous = static_cast<std::size_t>(v.as_int());
    });
    require(left <= previous, "OfficialER206 owned final ledger monotonic");
    require(!unmet || (decoded && !pending && !wide),
            "OfficialER206 owned positive-unmet classification");
    require(!mutated || (typed > 0u && orphan == 1u),
            "OfficialER206 owned meaningful mutation proof");
    const auto proof = info.get("complete_mutation_counts_sha256");
    require(proof.is_str() && proof.as_str().size() <= 64u,
            "OfficialER206 owned mutation digest type");
    if (mutated) {
        require(bound && decoded && !pending, "OfficialER206 owned mutation source lifecycle");
        const auto meaning =
            docs.authority.root().get("cases").at(row.role_case_index).get("whole_model");
        require(!meaning.is_null(), "OfficialER206 owned mutation source must be complete");
        const auto expected = official_er_expected_mutations(meaning);
        require(typed == official_er_typed_count(expected) &&
                    orphan == static_cast<std::size_t>(expected.executed.at("orphan_ink")) &&
                    proof.as_str() == official_er_mutation_digest(expected),
                "OfficialER206 source-bound complete mutation proof");
    }
    if (admitted)
        require(captured == 5u && published == 5u && bound && decoded && !pending && !unmet &&
                    restored && wide && (narrow || ordinary) && back && mutated && !exception &&
                    !resource && row.errors.empty() && row.status[0] == "drawn" &&
                    row.status[2] == "drawn" &&
                    ((narrow && row.status[1] == "drawn") ||
                     (ordinary && row.status[1] == "source_fallback")),
                "OfficialER206 per-state proof cannot bypass whole admission");
    out.observed += captured == 5u;
    out.published += published == 5u;
    out.bound += bound;
    out.decoded += decoded;
    out.pending += pending;
    out.admitted += admitted;
    out.mutated += mutated;
    out.unmet += unmet;
    out.errors += !row.errors.empty();
    out.resources += resource;
}

OfficialErCounts official_er_counts(const std::vector<Observation>& rows,
                                    const relations_oracle::Er206Documents* docs) {
    // Every ER source prepaid these bounded parses/counters/report copies.
    OfficialErCounts out;
    std::array<bool, 206> seen{};
    for (const auto& row : rows) {
        if (row.raw_active_schema != kOfficialEr206Contract &&
            row.raw_active_schema != kOfficialEr107Contract)
            continue;
        std::optional<Observation> retained;
        if (row.raw_active_schema == kOfficialEr107Contract)
            retained.emplace(official_er206_legacy_view(row));
        const auto& er_row = retained ? *retained : row;
        require(er_row.role_case_index < seen.size() && !seen[er_row.role_case_index],
                "OfficialER206 duplicate or unknown original consumption");
        seen[er_row.role_case_index] = true;
        ++out.visited;
        require(docs != nullptr, "OfficialER206 summary source authority lifetime");
        require(er_row.raw_source_constraints.size() <= 4096u,
                "OfficialER206 owned metadata bounds");
        const auto parsed = json::parse(er_row.raw_source_constraints);
        require(parsed.has_value(), "OfficialER206 owned metadata JSON");
        official_er_accumulate_owned(parsed->root(), er_row, out, *docs);
    }
    return out;
}
}  // namespace

namespace {
TEST(MermaidOfficialEr206, WholeProofRequiresEveryStateAndCannotBeForgedByWideOnly) {
    using namespace relations_oracle;
    Work setup;
    const auto docs = er206_document(setup);
    setup.spend(65536u);
    Work work{kEr206SceneLimit};
    work.spend(kOfficialEr206ReportReserve + 131072u + 16u * sizeof(Observation) +
               16u * sizeof(OfficialErProgress) + 8u * sizeof(OfficialErFrames) + 6u * 4096u +
               4u * kOfficialErMutationPlanCharge);
    const auto& pin = kEr206Pins[0];
    work.spend(pin.global + pin.source_bytes * 32u + pin.path.size() * 12u + 65536u);
    const auto fixture = docs.manifest.root().get("fixtures").at(pin.global);
    const auto input = read_file(kRoot / "cases" / pin.path);
    json::JsonMutDoc evidence;
    auto data = evidence.object();
    OfficialErProgress p;
    p.remaining[0] = work.left;
    auto frames = official_er_capture(input, work, p, data);
    work.spend(65536u);
    const auto authority = docs.authority.root().get("cases").at(0u);
    ASSERT_EQ(p.captures, 5u);
    ASSERT_EQ(p.published, 5u);
    ASSERT_EQ(authority.get("source").as_str(), input);
    const auto meaning = er206_binding(authority, 0u, docs, work);
    ASSERT_FALSE(meaning.is_null());
    p.bound = true;
    p.remaining[3] = work.left;
    const auto model = er206_decode(meaning, pin, work);
    p.decoded = true;
    p.remaining[4] = work.left;
    // Fixed source ordinal0 is Photograph with exactly two typed attributes and
    // no relation/group. Applicability comes from that whole source Model.
    ASSERT_EQ(model.entities.size(), 1u);
    ASSERT_TRUE(model.entities[0].typed_er.has_value());
    ASSERT_EQ(model.entities[0].typed_er->attributes.size(), 2u);
    ASSERT_TRUE(model.relations.empty());
    ASSERT_TRUE(model.groups.empty());
    work.spend(16384u);
    std::size_t expected = model.entities.size() + model.entities.size();
    for (const auto& e : model.entities)
        for (const auto& a : e.typed_er->attributes) {
            expected += 5u;
            if (!a.keys.empty()) ++expected;
            if (a.keys.size() > 1u && a.keys[0] != a.keys[1]) ++expected;
        }
    Observation good;
    good.errors.reserve(6u);
    good.path = pin.path;
    good.track = "original";
    good.type = "er-diagram";
    good.raw_active_schema = kOfficialEr206Contract;
    good.raw_source_authority = kOfficialEr206Authority;
    good.role_case_index = 0u;
    good.source_sha256 = digest(input);
    good.role_model_sha = pin.model_sha;
    good.projection_baseline = class481_canonical(fixture.get("raw"), work);
    official_er_geometry(frames, model, meaning, pin.model_bytes, work, good, p);
    ASSERT_TRUE(p.admitted);
    ASSERT_TRUE(good.errors.empty());
    EXPECT_EQ(p.typed_rejections, expected);
    EXPECT_EQ(p.orphan_rejections, 1u);
    p.phase = OfficialErPhase::Complete;
    good.raw_source_constraints = official_er_info(p, good, pin.global, work.left);
    work.spend(65536u + 8u * sizeof(Observation) + good.raw_source_constraints.size() * 32u);
    std::vector<Observation> one;
    one.push_back(good);
    const auto baseline = official_er_counts(one, &docs);
    ASSERT_EQ(baseline.visited, 1u);
    ASSERT_EQ(baseline.admitted, 1u);
    ASSERT_EQ(baseline.errors, 0u);
    auto exact_failure = [&](const std::exception& error, std::string_view expected_error) {
        const char* text = error.what();
        std::size_t n = 0;
        while (text && n < 128u && text[n])
            ++n;
        const std::string_view message = text ? std::string_view(text, n) : std::string_view{};
        require(work.left > 0u && !class481_resource(message) && message == expected_error,
                "OfficialER206 control requires exact nonresource reader diagnostic");
    };
    auto reject = [&](std::string_view expected_error, auto mutate, bool real_duplicate = false) {
        // Before the deep Observation/JSON clone, own fixed string/vector
        // controls and64*actual bounded metadata/source provenance bytes.
        work.spend(65536u + kOfficialErMutationPlanCharge + 16u * sizeof(Observation) +
                   (good.raw_source_constraints.size() + good.projection_baseline.size() +
                    good.path.size() + 1024u) *
                       64u);
        auto bad = good;
        json::JsonMutDoc changed;
        auto value = changed.raw_json(good.raw_source_constraints);
        changed.set_root(value);
        mutate(value, bad);
        bad.raw_source_constraints = real_duplicate ? std::string("{\"source_bound\":true,") +
                                                          good.raw_source_constraints.substr(1u)
                                                    : changed.to_string();
        std::vector<Observation> rows;
        rows.push_back(std::move(bad));
        bool rejected = false;
        try {
            static_cast<void>(official_er_counts(rows, &docs));
        } catch (const std::exception& error) {
            exact_failure(error, expected_error);
            rejected = true;
        }
        EXPECT_TRUE(rejected);
    };
    reject("OfficialER206 owned exact fields",
           [](json::JsonMutVal v, Observation&) { v.set("unknown", 1); });
    reject("OfficialER206 owned exact fields", [](json::JsonMutVal, Observation&) {}, true);
    reject("OfficialER206 owned exact source tuple",
           [](json::JsonMutVal v, Observation&) { v.set("source_case_ordinal", 1); });
    reject("OfficialER206 owned exact source tuple",
           [](json::JsonMutVal v, Observation&) { v.set("original_global_ordinal", 450); });
    reject("OfficialER206 owned exact source tuple",
           [](json::JsonMutVal, Observation& r) { r.path = "er-diagram/foreign.mmd"; });
    reject("OfficialER206 owned approved source/Model identity",
           [](json::JsonMutVal v, Observation& r) {
               r.source_sha256 = std::string(64u, 'a');
               v.set("source_sha256", r.source_sha256);
           });
    reject("OfficialER206 owned approved source/Model identity",
           [](json::JsonMutVal v, Observation& r) {
               r.role_model_sha = std::string(64u, 'a');
               v.set("whole_Model_sha256", r.role_model_sha);
           });
    reject("OfficialER206 owned lifecycle order",
           [](json::JsonMutVal v, Observation&) { v.set("captures", 4); });
    reject("OfficialER206 owned lifecycle order",
           [](json::JsonMutVal v, Observation&) { v.set("published_frames", 4); });
    reject("OfficialER206 owned lifecycle order",
           [](json::JsonMutVal v, Observation&) { v.set("whole_null_pending", true); });
    reject("OfficialER206 per-state proof cannot bypass whole admission",
           [](json::JsonMutVal v, Observation&) { v.set("wide_semantic_footer", false); });
    reject("OfficialER206 per-state proof cannot bypass whole admission",
           [](json::JsonMutVal v, Observation&) { v.set("restored_cells_equal", false); });
    reject("OfficialER206 per-state proof cannot bypass whole admission",
           [](json::JsonMutVal v, Observation&) { v.set("semantic_mutations_complete", false); });
    reject("OfficialER206 per-state proof cannot bypass whole admission",
           [](json::JsonMutVal v, Observation&) { v.set("resource_exhausted", true); });
    reject("OfficialER206 owned meaningful mutation proof",
           [](json::JsonMutVal v, Observation&) { v.set("typed_rejections", 0); });
    // Same complete good source, all other proof/status fields unchanged:
    // partial execution is not interchangeable with a complete typed set.
    reject("OfficialER206 source-bound complete mutation proof",
           [](json::JsonMutVal v, Observation&) { v.set("typed_rejections", 1); });
    work.spend(3u * kOfficialErMutationPlanCharge + 65536u);
    const auto expected_plan = official_er_expected_mutations(meaning);
    auto wrong_kind = expected_plan;
    const auto kind = wrong_kind.executed.find("complete_header");
    ASSERT_NE(kind, wrong_kind.executed.end());
    if (--kind->second == 0) wrong_kind.executed.erase(kind);
    ++wrong_kind.executed["__wrong_complete_header_kind__"];
    ASSERT_EQ(official_er_typed_count(wrong_kind), official_er_typed_count(expected_plan));
    const auto wrong_kind_sha = official_er_mutation_digest(wrong_kind);
    reject("OfficialER206 source-bound complete mutation proof",
           [&](json::JsonMutVal v, Observation&) {
               v.set("complete_mutation_counts_sha256", wrong_kind_sha);
           });
    auto wrong_skip = expected_plan;
    ASSERT_FALSE(wrong_skip.skipped.empty());
    const auto skipped = wrong_skip.skipped.begin();
    if (--skipped->second == 0) wrong_skip.skipped.erase(skipped);
    ++wrong_skip.skipped["__wrong_skip_kind__"];
    const auto wrong_skip_sha = official_er_mutation_digest(wrong_skip);
    reject("OfficialER206 source-bound complete mutation proof",
           [&](json::JsonMutVal v, Observation&) {
               v.set("complete_mutation_counts_sha256", wrong_skip_sha);
           });
    // The builder's shared comparison also rejects either domain before it
    // can set semantic_mutations_complete; no new pixel rejection is claimed.
    for (const auto* incomplete : {&wrong_kind, &wrong_skip}) {
        bool rejected = false;
        try {
            official_er_complete_mutations(*incomplete, expected_plan);
        } catch (const std::exception& error) {
            exact_failure(error, "OfficialER206 source-bound complete mutation execution");
            rejected = true;
        }
        EXPECT_TRUE(rejected);
    }
    // A physically defective restored footer preserves the real good wide;
    // neither a wide-only status nor a forged admitted bit may clear it.
    work.spend(frames[2]->cells.size() * 2u * sizeof(std::string) + 8u * sizeof(Grid) + 4096u);
    class285_orphan_copy_prepay(*frames[2], work);
    auto saved = *frames[2];
    const auto footer = following_row(*frames[2], work, kSentinel);
    ASSERT_TRUE(footer.has_value());
    work.spend(static_cast<std::size_t>(frames[2]->width) * 8u + 256u);
    int x = 0;
    while (x < frames[2]->width && frames[2]->at({x, *footer}) == " ")
        ++x;
    ASSERT_LT(x, frames[2]->width);
    frames[2]->put({x, *footer}, "X");
    official_er_publish_frame(data, "real_bad_restored_frame", *frames[2], work);
    work.spend(65536u + kOfficialErMutationPlanCharge + 16u * sizeof(Observation));
    auto bad = good;
    bad.errors.clear();
    bad.status = {"error", "error", "error"};
    auto changed = p;
    official_er_geometry(frames, model, meaning, pin.model_bytes, work, bad, changed);
    EXPECT_EQ(bad.status[0], "drawn");
    EXPECT_FALSE(changed.admitted);
    EXPECT_FALSE(changed.restored);
    EXPECT_FALSE(changed.resource);
    ASSERT_FALSE(bad.errors.empty());
    work.spend(65536u);
    changed.admitted = true;
    bad.raw_source_constraints = official_er_info(changed, bad, pin.global, work.left);
    bool forged = false;
    try {
        OfficialErCounts checked;
        const auto doc = json::parse(bad.raw_source_constraints);
        ASSERT_TRUE(doc.has_value());
        official_er_accumulate_owned(doc->root(), bad, checked, docs);
    } catch (const std::exception& error) {
        exact_failure(error, "OfficialER206 per-state proof cannot bypass whole admission");
        forged = true;
    }
    EXPECT_TRUE(forged);
    work.spend(4u * sizeof(Grid) + 4096u);
    frames[2] = std::move(saved);
    // Same Grid/Model identity-only exchange is observationally equivalent;
    // no semantic rejection is claimed for it.
    data.set("equal_Grid_Model_identity_swap_skipped", true);
    data.set("typed_negative_count_derived_from_complete_Model", static_cast<int64_t>(expected));
    work.spend(8192u + 8u * sizeof(Work) + 8u * sizeof(ftxui::Element));
    auto zero_element = ftxui::text("resource-only");
    Work zero{0u};
    bool resource = false;
    try {
        static_cast<void>(note13_capture(zero_element, 40, 4, zero));
    } catch (const std::exception& error) {
        const char* text = error.what();
        std::size_t n = 0;
        while (text && n < 128u && text[n])
            ++n;
        resource = std::string_view(text ? text : "", n) == "Relation oracle work exhausted";
    }
    EXPECT_TRUE(resource);
    EXPECT_EQ(zero.left, 0u);
    data.set("zero_Work_is_resource_not_semantic_credit", resource);
    data.set("remaining", static_cast<int64_t>(work.left));
    evidence.set_root(data);
    // Dynamic row publication already prepaid the serializer/result/write
    // shares; the source reserve owns these bounded control fields/file nodes.
    write_file(fs::path(LOOM_MERMAID_REPORT_DIR) /
                   "mermaid-official-er206-owned-proof-control.json",
               evidence.to_string());
}
}  // namespace

namespace {
TEST(MermaidOfficialFlow400, RejectsInconsistentConfig21AuthorityMetadata) {
    flow_oracle::OracleWork work;
    work.spend(65536u + 16u * sizeof(Observation) + 16u * sizeof(OfficialFlowProgress));
    OfficialFlowProgress progress;
    Observation selected;
    selected.raw_source_authority = "immutable_Flow400_8Models_plus_Config21";
    auto good = json::parse(official_flow_info(progress, work.remaining, true));
    ASSERT_TRUE(good);
    OfficialFlowCounts baseline;
    EXPECT_NO_THROW(official_flow_accumulate_owned(good->root(), selected, baseline));
    EXPECT_EQ(baseline.admitted, 0u);  // A metadata guard baseline is not a drawing.
    auto absent = json::parse(official_flow_info(progress, work.remaining, false));
    ASSERT_TRUE(absent);
    auto reject = [&](JsonVal metadata, const Observation& row) {
        bool rejected = false;
        OfficialFlowCounts counts;
        try {
            official_flow_accumulate_owned(metadata, row, counts);
        } catch (const std::exception& error) {
            EXPECT_EQ(std::string_view(error.what()),
                      "OfficialFlow400 inconsistent Config21 authority metadata");
            EXPECT_FALSE(flow_oracle::flow400_resource(error.what()));
            rejected = true;
        }
        EXPECT_TRUE(rejected);
    };
    reject(absent->root(), selected);
    Observation legacy;
    legacy.raw_source_authority = "immutable_Flow400_whole_8Models_392null";
    OfficialFlowCounts old_baseline;
    EXPECT_NO_THROW(official_flow_accumulate_owned(absent->root(), legacy, old_baseline));
    reject(good->root(), legacy);
    auto changed = good->root().to_string();
    const auto at = changed.find(flow_oracle::kFlow400Config21AssetSha);
    ASSERT_NE(at, std::string::npos);
    changed.replace(at, flow_oracle::kFlow400Config21AssetSha.size(), std::string(64u, '0'));
    auto altered = json::parse(changed);
    ASSERT_TRUE(altered);
    reject(altered->root(), selected);
    // These two immutable offset-zero diagnostic tuples reuse a same-run
    // original proof. A wrapper is not a Config21 source or a new drawing.
    work.spend(65536u + 24u * sizeof(Observation) + 24u * sizeof(OfficialFlowCounts));
    for (std::size_t index = 0; index < kFlowRepeatBody2Pins.size(); ++index) {
        const auto& pin = kFlowRepeatBody2Pins[index];
        Observation body;
        body.raw_active_schema = kFlowRepeatBody2Contract;
        body.raw_source_authority = "same_run_original_Flow400_offset0";
        body.track = "body_diagnostic";
        body.type = "flowchart";
        body.role_case_index = index;
        body.path = pin.path;
        body.source_sha256 = pin.source_sha;
        body.role_model_sha = pin.model_sha;
        body.role_body_sha = pin.source_sha;
        OfficialFlowCounts valid;
        EXPECT_NO_THROW(official_flow_accumulate_owned(absent->root(), body, valid));
        EXPECT_EQ(valid.admitted, 0u);
        reject(good->root(), body);
        for (int mutation = 0; mutation < 9; ++mutation) {
            auto wrong = body;
            switch (mutation) {
            case 0:
                wrong.path += ".foreign";
                break;
            case 1:
                wrong.source_sha256 = std::string(64u, '0');
                break;
            case 2:
                wrong.role_model_sha = std::string(64u, '0');
                break;
            case 3:
                wrong.role_body_sha = std::string(64u, '0');
                break;
            case 4:
                wrong.track = "original";
                break;
            case 5:
                wrong.role_case_index = kFlowRepeatBody2Pins.size();
                break;
            case 6:
                wrong.raw_active_schema = kOfficialFlow400Contract;
                break;
            case 7:
                wrong.type = "classDiagram";
                break;
            default:
                wrong.raw_source_authority = "unknown_same_run_authority";
                break;
            }
            bool rejected = false;
            OfficialFlowCounts counts;
            try {
                official_flow_accumulate_owned(absent->root(), wrong, counts);
            } catch (const std::exception& error) {
                EXPECT_EQ(std::string_view(error.what()),
                          "OfficialFlow400 unknown source authority");
                EXPECT_FALSE(flow_oracle::flow400_resource(error.what()));
                rejected = true;
            }
            EXPECT_TRUE(rejected);
            EXPECT_EQ(counts.admitted, 0u);
        }
    }
}

TEST(MermaidOfficialFlow400, TypedSource4OwnedMetadataRequiresExactProvenanceAndMutationCounts) {
    // This parser control owns no capture or drawing credit. MermaidCapabilityReport.OfficialMatrix
    // is the only Official path that captures five grids and can admit a source.
    auto typed = flow_oracle::flow400_typed_source4_documents();
    flow_oracle::OracleWork work{flow_oracle::kFlow400SceneLimit};
    work.spend(65536u + 32u * sizeof(Observation) + 16u * sizeof(OfficialFlowProgress) +
               16u * sizeof(OfficialFlowCounts));
    const auto cases = typed.supplement.root().get("cases");
    ASSERT_TRUE(cases.is_arr());
    ASSERT_EQ(cases.size(), 4u);
    const std::array<std::size_t, 4> expected{{8u, 9u, 5u, 7u}};
    for (std::size_t index = 0u; index < cases.size(); ++index) {
        const auto source4_case = cases.at(index), authority = source4_case.get("base_authority");
        const auto ordinal = flow_oracle::flow400_number(authority.get("ordinal"));
        EXPECT_EQ(flow_oracle::flow400_typed_source4_expected(source4_case), expected.at(index));
        Observation row;
        row.errors.reserve(6u);
        row.raw_active_schema = kOfficialFlow400Contract;
        row.raw_source_authority = "immutable_Flow400_8Models_plus_TypedSource4";
        row.track = "original";
        row.type = "flowchart";
        row.role_case_index = ordinal;
        row.path = flow_oracle::flow400_text(authority.get("fixture"));
        row.source_sha256 = flow_oracle::flow400_text(source4_case.get("source_sha256"));
        row.role_model_sha = flow_oracle::flow400_text(source4_case.get("typed_model_sha256"));
        row.status = {"drawn", "drawn", "drawn"};
        OfficialFlowProgress progress;
        progress.captured = progress.bound = progress.decoded = progress.admitted =
            progress.restored = progress.mutated = true;
        progress.captures = 5u;
        progress.height = 1;
        progress.phase = "complete";
        progress.remaining.fill(work.remaining);
        row.raw_source_constraints = official_flow_typed_source4_info(
            progress, work.remaining, expected.at(index), expected.at(index));
        const auto parsed = json::parse(row.raw_source_constraints);
        ASSERT_TRUE(parsed.has_value());
        OfficialFlowCounts accepted;
        EXPECT_NO_THROW(
            official_flow_accumulate_owned(parsed->root(), row, accepted, typed.supplement.root()));
        EXPECT_EQ(accepted.admitted, 1u);
        EXPECT_EQ(accepted.mutated, 1u);
        EXPECT_EQ(accepted.resources, 0u);
        if (index == 0u) {
            auto failed = row;
            failed.errors.emplace_back("source-authored geometry failure");
            OfficialFlowProgress geometry;
            geometry.captured = geometry.bound = geometry.decoded = true;
            geometry.captures = 5u;
            geometry.height = 1;
            geometry.phase = "wide";
            geometry.remaining.fill(work.remaining);
            failed.raw_source_constraints =
                official_flow_typed_source4_info(geometry, work.remaining, expected.at(index), 0u);
            const auto parsed_failed = json::parse(failed.raw_source_constraints);
            ASSERT_TRUE(parsed_failed.has_value());
            OfficialFlowCounts geometry_counts;
            EXPECT_NO_THROW(official_flow_accumulate_owned(
                parsed_failed->root(), failed, geometry_counts, typed.supplement.root()));
            EXPECT_EQ(geometry_counts.observed, 1u);
            EXPECT_EQ(geometry_counts.bound, 1u);
            EXPECT_EQ(geometry_counts.decoded, 1u);
            EXPECT_EQ(geometry_counts.admitted, 0u);
            EXPECT_EQ(geometry_counts.mutated, 0u);
            EXPECT_EQ(geometry_counts.errors, 1u);
            EXPECT_EQ(geometry_counts.resources, 0u);
            auto early = row;
            early.errors.emplace_back("typed Model decoder rejected source-bound payload");
            OfficialFlowProgress decode;
            decode.captured = decode.bound = true;
            decode.captures = 5u;
            decode.height = 1;
            decode.phase = "decode";
            decode.remaining.fill(work.remaining);
            early.raw_source_constraints =
                official_flow_typed_source4_info(decode, work.remaining, expected.at(index), 0u);
            const auto parsed_early = json::parse(early.raw_source_constraints);
            ASSERT_TRUE(parsed_early.has_value());
            OfficialFlowCounts decode_counts;
            EXPECT_NO_THROW(official_flow_accumulate_owned(parsed_early->root(), early,
                                                           decode_counts, typed.supplement.root()));
            EXPECT_EQ(decode_counts.observed, 1u);
            EXPECT_EQ(decode_counts.bound, 1u);
            EXPECT_EQ(decode_counts.decoded, 0u);
            EXPECT_EQ(decode_counts.admitted, 0u);
            EXPECT_EQ(decode_counts.mutated, 0u);
            EXPECT_EQ(decode_counts.errors, 1u);
            EXPECT_EQ(decode_counts.resources, 0u);
            auto exhausted = row;
            exhausted.errors.emplace_back("Bounded cell-oracle work exhausted");
            OfficialFlowProgress resource;
            resource.captured = resource.bound = resource.decoded = resource.resource = true;
            resource.captures = 5u;
            resource.height = 1;
            resource.phase = "mutations";
            resource.remaining.fill(0u);
            exhausted.raw_source_constraints =
                official_flow_typed_source4_info(resource, 0u, expected.at(index), 0u);
            const auto parsed_exhausted = json::parse(exhausted.raw_source_constraints);
            ASSERT_TRUE(parsed_exhausted.has_value());
            OfficialFlowCounts resource_counts;
            EXPECT_NO_THROW(official_flow_accumulate_owned(
                parsed_exhausted->root(), exhausted, resource_counts, typed.supplement.root()));
            EXPECT_EQ(resource_counts.observed, 1u);
            EXPECT_EQ(resource_counts.bound, 1u);
            EXPECT_EQ(resource_counts.decoded, 1u);
            EXPECT_EQ(resource_counts.admitted, 0u);
            EXPECT_EQ(resource_counts.mutated, 0u);
            EXPECT_EQ(resource_counts.errors, 1u);
            EXPECT_EQ(resource_counts.resources, 1u);
        }
        const auto reject = [&](std::string raw, Observation forged, std::string_view message) {
            work.product(raw.size(), 32u);
            work.spend(4096u);
            const auto altered = json::parse(raw);
            ASSERT_TRUE(altered.has_value());
            OfficialFlowCounts rejected;
            bool denied = false;
            try {
                official_flow_accumulate_owned(altered->root(), forged, rejected,
                                               typed.supplement.root());
            } catch (const std::exception& error) {
                EXPECT_EQ(std::string_view(error.what()), message);
                EXPECT_FALSE(flow_oracle::flow400_resource(error.what()));
                denied = true;
            }
            EXPECT_TRUE(denied);
            EXPECT_EQ(rejected.admitted, 0u);
        };
        reject(projection_mutate(row.raw_source_constraints,
                                 [&](json::JsonMutVal value) {
                                     value.set("typed_mutations_expected",
                                               static_cast<int64_t>(expected.at(index) - 1u));
                                 }),
               row, "OfficialFlow400 TypedSource4 mutation bound metadata");
        reject(projection_mutate(row.raw_source_constraints,
                                 [&](json::JsonMutVal value) {
                                     value.set("typed_mutations_rejected",
                                               static_cast<int64_t>(expected.at(index) - 1u));
                                 }),
               row, "OfficialFlow400 TypedSource4 exact mutation metadata");
        reject(projection_mutate(row.raw_source_constraints,
                                 [](json::JsonMutVal value) {
                                     value.set("phase", "wide");
                                     auto original = value.get("phase_remaining");
                                     auto replacement = value.make_arr();
                                     for (std::size_t n = 0u; n < original.size(); ++n) {
                                         if (n < 4u)
                                             replacement.append(
                                                 value.make_num(original.at(n).as_int()));
                                         else
                                             replacement.append(value.make_null());
                                     }
                                     value.add("phase_remaining", replacement);
                                 }),
               row, "OfficialFlow400 TypedSource4 admitted phase/work ledger");
        reject(projection_mutate(row.raw_source_constraints,
                                 [](json::JsonMutVal value) {
                                     auto original = value.get("phase_remaining");
                                     auto replacement = value.make_arr();
                                     for (std::size_t n = 0u; n < original.size(); ++n) {
                                         if (n == 0u)
                                             replacement.append(value.make_num(1));
                                         else if (n == 1u)
                                             replacement.append(value.make_num(2));
                                         else
                                             replacement.append(
                                                 value.make_num(original.at(n).as_int()));
                                     }
                                     value.add("phase_remaining", replacement);
                                 }),
               row, "OfficialFlow400 TypedSource4 monotonic phase ledger");
        reject(projection_mutate(
                   row.raw_source_constraints,
                   [](json::JsonMutVal value) {
                       auto original = value.get("phase_remaining");
                       auto replacement = value.make_arr();
                       for (std::size_t n = 0u; n < original.size(); ++n) {
                           if (n + 1u == original.size())
                               replacement.append(value.make_num(original.at(n).as_int() - 1));
                           else
                               replacement.append(value.make_num(original.at(n).as_int()));
                       }
                       value.add("phase_remaining", replacement);
                   }),
               row, "OfficialFlow400 TypedSource4 admitted phase/work ledger");
        reject(projection_mutate(row.raw_source_constraints,
                                 [](json::JsonMutVal value) {
                                     value.set("source_supplement_sha256", std::string(64u, '0'));
                                 }),
               row, "OfficialFlow400 inconsistent TypedSource4 authority metadata");
        reject(projection_mutate(row.raw_source_constraints,
                                 [](json::JsonMutVal value) { value.set("forged_extra", true); }),
               row, "OfficialFlow400 TypedSource4 metadata key count");
        auto wrong_source = row;
        wrong_source.source_sha256 = std::string(64u, '0');
        reject(row.raw_source_constraints, std::move(wrong_source),
               "OfficialFlow400 inconsistent TypedSource4 authority metadata");
        auto wrong_model = row;
        wrong_model.role_model_sha = std::string(64u, '0');
        reject(row.raw_source_constraints, std::move(wrong_model),
               "OfficialFlow400 inconsistent TypedSource4 authority metadata");
        auto wrong_path = row;
        wrong_path.path += ".foreign";
        reject(row.raw_source_constraints, std::move(wrong_path),
               "OfficialFlow400 inconsistent TypedSource4 authority metadata");
    }
}

TEST(MermaidOfficialFlow400, ClassicOwnedMetadataRequiresExactProvenanceAndMutationCounts) {
    // This parser control earns no drawing credit. MermaidCapabilityReport.OfficialMatrix is
    // the only Official consumer that publishes five actual grids before bind.
    auto classic = flow_oracle::flow400_classic_documents();
    flow_oracle::OracleWork work{flow_oracle::kFlow400SceneLimit};
    work.spend(65536u + 16u * sizeof(Observation) + 8u * sizeof(OfficialFlowProgress) +
               8u * sizeof(OfficialFlowCounts));
    const auto cases = classic.supplement.root().get("cases");
    ASSERT_TRUE(cases.is_arr());
    ASSERT_EQ(cases.size(), 6u);
    for (std::size_t index = 0u; index < cases.size(); ++index) {
        const auto selected = cases.at(index);
        const auto authority = selected.get("base_authority");
        const auto expected = flow_oracle::flow400_classic_expected(selected);
        Observation row;
        row.errors.reserve(6u);
        row.raw_active_schema = kOfficialFlow400Contract;
        row.raw_source_authority = "immutable_Flow400_8Models_plus_Classic";
        row.track = "original";
        row.type = "flowchart";
        row.role_case_index = flow_oracle::flow400_number(authority.get("ordinal"));
        row.path = flow_oracle::flow400_text(authority.get("fixture"));
        row.source_sha256 = flow_oracle::flow400_text(selected.get("source_sha256"));
        row.role_model_sha = flow_oracle::flow400_text(selected.get("classic_model_sha256"));
        row.status = {"drawn", "drawn", "drawn"};
        OfficialFlowProgress good;
        good.captured = good.bound = good.decoded = good.admitted = good.restored = good.mutated =
            true;
        good.captures = 5u;
        good.height = 1;
        good.phase = "complete";
        good.remaining.fill(work.remaining);
        row.raw_source_constraints =
            official_flow_classic_info(good, work.remaining, expected, expected);
        auto parsed = json::parse(row.raw_source_constraints);
        ASSERT_TRUE(parsed.has_value());
        OfficialFlowCounts accepted;
        EXPECT_NO_THROW(official_flow_accumulate_owned(parsed->root(), row, accepted, JsonVal{},
                                                       classic.supplement.root()));
        EXPECT_EQ(accepted.admitted, 1u);
        EXPECT_EQ(accepted.mutated, 1u);
        auto reject = [&](std::string raw, Observation forged, std::string_view expected_error) {
            auto altered = json::parse(raw);
            ASSERT_TRUE(altered.has_value());
            OfficialFlowCounts denied;
            bool rejected = false;
            try {
                official_flow_accumulate_owned(altered->root(), forged, denied, JsonVal{},
                                               classic.supplement.root());
            } catch (const std::exception& error) {
                EXPECT_EQ(std::string_view(error.what()), expected_error);
                EXPECT_FALSE(flow_oracle::flow400_resource(error.what()));
                rejected = true;
            }
            EXPECT_TRUE(rejected);
            EXPECT_EQ(denied.admitted, 0u);
        };
        reject(projection_mutate(row.raw_source_constraints,
                                 [&](json::JsonMutVal value) {
                                     value.set("classic_mutations_expected",
                                               static_cast<int64_t>(expected - 1u));
                                 }),
               row, "OfficialFlow400 selected mutation bound metadata");
        reject(projection_mutate(row.raw_source_constraints,
                                 [&](json::JsonMutVal value) {
                                     value.set("classic_mutations_rejected",
                                               static_cast<int64_t>(expected - 1u));
                                 }),
               row, "OfficialFlow400 selected exact mutation metadata");
        auto wrong_model = row;
        wrong_model.role_model_sha = std::string(64u, '0');
        reject(row.raw_source_constraints, std::move(wrong_model),
               "OfficialFlow400 inconsistent Classic authority metadata");
        auto failed = row;
        failed.errors.emplace_back("source-authored Classic geometry failure");
        OfficialFlowProgress failed_progress;
        failed_progress.captured = failed_progress.bound = failed_progress.decoded = true;
        failed_progress.captures = 5u;
        failed_progress.height = 1;
        failed_progress.phase = "wide";
        failed_progress.remaining.fill(work.remaining);
        failed.raw_source_constraints =
            official_flow_classic_info(failed_progress, work.remaining, expected, 0u);
        const auto failed_meta = json::parse(failed.raw_source_constraints);
        ASSERT_TRUE(failed_meta.has_value());
        OfficialFlowCounts failed_counts;
        EXPECT_NO_THROW(official_flow_accumulate_owned(failed_meta->root(), failed, failed_counts,
                                                       JsonVal{}, classic.supplement.root()));
        EXPECT_EQ(failed_counts.admitted, 0u);
        EXPECT_EQ(failed_counts.mutated, 0u);
        EXPECT_EQ(failed_counts.errors, 1u);
    }
}

TEST(MermaidOfficialFlow400, LinkProfile3OwnedMetadataRequiresExactProvenanceAndMutationCounts) {
    // This parser control owns no capture or drawing result. MermaidCapabilityReport.OfficialMatrix
    // remains the only Official path that can admit a LinkProfile3 source.
    auto linkprofile3 = flow_oracle::flow400_linkprofile3_documents();
    flow_oracle::OracleWork work{flow_oracle::kFlow400SceneLimit};
    work.spend(65536u + 24u * sizeof(Observation) + 16u * sizeof(OfficialFlowProgress) +
               16u * sizeof(OfficialFlowCounts));
    const auto cases = linkprofile3.supplement.root().get("cases");
    ASSERT_TRUE(cases.is_arr());
    ASSERT_EQ(cases.size(), 4u);
    for (std::size_t index = 0u; index < cases.size(); ++index) {
        const auto selected = cases.at(index), authority = selected.get("base_authority");
        const auto expected = flow_oracle::flow400_linkprofile3_expected(selected);
        Observation row;
        row.errors.reserve(6u);
        row.raw_active_schema = kOfficialFlow400Contract;
        row.raw_source_authority = "immutable_Flow400_8Models_plus_LinkProfile3";
        row.track = "original";
        row.type = "flowchart";
        row.role_case_index = flow_oracle::flow400_number(authority.get("ordinal"));
        row.path = flow_oracle::flow400_text(authority.get("fixture"));
        row.source_sha256 = flow_oracle::flow400_text(selected.get("source_sha256"));
        row.role_model_sha = flow_oracle::flow400_text(selected.get("link_model_sha256"));
        row.status = {"drawn", "drawn", "drawn"};
        OfficialFlowProgress good;
        good.captured = good.bound = good.decoded = good.admitted = good.restored = good.mutated =
            true;
        good.captures = 5u;
        good.height = 1;
        good.phase = "complete";
        good.remaining.fill(work.remaining);
        row.raw_source_constraints =
            official_flow_linkprofile3_info(good, work.remaining, expected, expected);
        auto parsed = json::parse(row.raw_source_constraints);
        ASSERT_TRUE(parsed.has_value());
        OfficialFlowCounts accepted;
        EXPECT_NO_THROW(official_flow_accumulate_owned(parsed->root(), row, accepted, JsonVal{},
                                                       JsonVal{}, linkprofile3.supplement.root()));
        EXPECT_EQ(accepted.admitted, 1u);
        EXPECT_EQ(accepted.mutated, 1u);
        auto reject = [&](std::string raw, Observation forged, std::string_view expected_error) {
            auto altered = json::parse(raw);
            ASSERT_TRUE(altered.has_value());
            OfficialFlowCounts denied;
            bool rejected = false;
            try {
                official_flow_accumulate_owned(altered->root(), forged, denied, JsonVal{},
                                               JsonVal{}, linkprofile3.supplement.root());
            } catch (const std::exception& error) {
                EXPECT_EQ(std::string_view(error.what()), expected_error);
                EXPECT_FALSE(flow_oracle::flow400_resource(error.what()));
                rejected = true;
            }
            EXPECT_TRUE(rejected);
            EXPECT_EQ(denied.admitted, 0u);
        };
        reject(projection_mutate(row.raw_source_constraints,
                                 [&](json::JsonMutVal value) {
                                     value.set("linkprofile3_mutations_expected",
                                               static_cast<int64_t>(expected - 1u));
                                 }),
               row, "OfficialFlow400 LinkProfile3 mutation bound metadata");
        reject(projection_mutate(row.raw_source_constraints,
                                 [&](json::JsonMutVal value) {
                                     value.set("linkprofile3_mutations_rejected",
                                               static_cast<int64_t>(expected - 1u));
                                 }),
               row, "OfficialFlow400 LinkProfile3 exact mutation metadata");
        auto wrong_model = row;
        wrong_model.role_model_sha = std::string(64u, '0');
        reject(row.raw_source_constraints, std::move(wrong_model),
               "OfficialFlow400 inconsistent LinkProfile3 authority metadata");
        auto failed = row;
        failed.errors.emplace_back("source-authored LinkProfile3 geometry failure");
        OfficialFlowProgress failed_progress;
        failed_progress.captured = failed_progress.bound = failed_progress.decoded = true;
        failed_progress.captures = 5u;
        failed_progress.height = 1;
        failed_progress.phase = "wide";
        failed_progress.remaining.fill(work.remaining);
        failed.raw_source_constraints =
            official_flow_linkprofile3_info(failed_progress, work.remaining, expected, 0u);
        const auto failed_meta = json::parse(failed.raw_source_constraints);
        ASSERT_TRUE(failed_meta.has_value());
        OfficialFlowCounts failed_counts;
        EXPECT_NO_THROW(official_flow_accumulate_owned(failed_meta->root(), failed, failed_counts,
                                                       JsonVal{}, JsonVal{},
                                                       linkprofile3.supplement.root()));
        EXPECT_EQ(failed_counts.admitted, 0u);
        EXPECT_EQ(failed_counts.mutated, 0u);
        EXPECT_EQ(failed_counts.errors, 1u);
    }
}

TEST(MermaidOfficialFlow400, GroupEndpoint15OwnedMetadataRequiresExactProvenanceAndMutationCounts) {
    // This is a parser authority guard. It owns no actual capture or drawing
    // credit; only MermaidCapabilityReport.OfficialMatrix owns the source-first scenes.
    const auto group = flow_oracle::group_endpoint15_documents();
    const auto selected = group.asset.root().get("records").at(0u);
    const auto tuple = flow_oracle::group_endpoint15_tuple(selected);
    const auto expected = flow_oracle::group_endpoint15_expected_mutations(selected);
    flow_oracle::OracleWork work{flow_oracle::kFlow400SceneLimit};
    work.spend(65536u + 24u * sizeof(Observation) + 16u * sizeof(OfficialFlowProgress) +
               16u * sizeof(OfficialFlowCounts));
    Observation row;
    row.errors.reserve(6u);
    row.raw_active_schema = kOfficialFlow400Contract;
    row.raw_source_authority = "immutable_Flow400_8Models_plus_GroupEndpoint15";
    row.track = "original";
    row.type = "flowchart";
    row.role_case_index = tuple.ordinal;
    row.path = std::string(tuple.fixture);
    row.source_sha256 = std::string(tuple.source_sha256);
    row.role_model_sha = flow_oracle::flow400_text(selected.get("canonical_model_sha256"));
    row.status = {{"drawn", "drawn", "drawn"}};
    OfficialFlowProgress good;
    good.captured = good.bound = good.decoded = good.admitted = good.restored = good.mutated = true;
    good.captures = 5u;
    good.height = 1;
    good.phase = "complete";
    good.remaining.fill(work.remaining);
    row.raw_source_constraints = official_flow_group_endpoint15_info(
        good, work.remaining, expected, expected, tuple.manifest_global_ordinal);
    {
        const auto parsed = json::parse(row.raw_source_constraints);
        ASSERT_TRUE(parsed.has_value());
        OfficialFlowCounts accepted;
        EXPECT_NO_THROW(official_flow_accumulate_owned(parsed->root(), row, accepted, JsonVal{},
                                                       JsonVal{}, JsonVal{}, group.asset.root()));
        EXPECT_EQ(accepted.admitted, 1u);
        EXPECT_EQ(accepted.mutated, 1u);
    }
    const auto reject = [&](std::string raw, Observation forged, std::string_view expected_error) {
        const auto parsed = json::parse(raw);
        ASSERT_TRUE(parsed.has_value());
        OfficialFlowCounts denied;
        bool rejected = false;
        try {
            official_flow_accumulate_owned(parsed->root(), forged, denied, JsonVal{}, JsonVal{},
                                           JsonVal{}, group.asset.root());
        } catch (const std::exception& error) {
            EXPECT_EQ(std::string_view(error.what()), expected_error);
            EXPECT_FALSE(flow_oracle::flow400_resource(error.what()));
            rejected = true;
        }
        EXPECT_TRUE(rejected);
        EXPECT_EQ(denied.admitted, 0u);
    };
    reject(projection_mutate(row.raw_source_constraints,
                             [&](json::JsonMutVal value) {
                                 value.set("group_endpoint15_mutations_expected",
                                           static_cast<int64_t>(expected - 1u));
                             }),
           row, "OfficialFlow400 GroupEndpoint15 mutation bound metadata");
    reject(projection_mutate(row.raw_source_constraints,
                             [&](json::JsonMutVal value) {
                                 value.set("group_endpoint15_mutations_rejected",
                                           static_cast<int64_t>(expected - 1u));
                             }),
           row, "OfficialFlow400 GroupEndpoint15 exact mutation metadata");
    reject(projection_mutate(row.raw_source_constraints,
                             [](json::JsonMutVal value) {
                                 value.set("source_supplement_sha256", std::string(64u, '0'));
                             }),
           row, "OfficialFlow400 inconsistent GroupEndpoint15 authority metadata");
    reject(projection_mutate(row.raw_source_constraints,
                             [&](json::JsonMutVal value) {
                                 value.set(
                                     "manifest_global_ordinal",
                                     static_cast<int64_t>(tuple.manifest_global_ordinal + 1u));
                             }),
           row, "OfficialFlow400 GroupEndpoint15 manifest global metadata");
    reject(projection_mutate(row.raw_source_constraints,
                             [](json::JsonMutVal value) {
                                 value.set("phase", "wide");
                                 auto original = value.get("phase_remaining");
                                 auto replacement = value.make_arr();
                                 for (std::size_t index = 0u; index < original.size(); ++index) {
                                     if (index < 4u)
                                         replacement.append(
                                             value.make_num(original.at(index).as_int()));
                                     else
                                         replacement.append(value.make_null());
                                 }
                                 value.add("phase_remaining", replacement);
                             }),
           row, "OfficialFlow400 GroupEndpoint15 admitted phase/work ledger");
    auto wrong_source = row;
    wrong_source.source_sha256 = std::string(64u, '0');
    reject(row.raw_source_constraints, std::move(wrong_source),
           "OfficialFlow400 inconsistent GroupEndpoint15 authority metadata");
    auto wrong_model = row;
    wrong_model.role_model_sha = std::string(64u, '0');
    reject(row.raw_source_constraints, std::move(wrong_model),
           "OfficialFlow400 inconsistent GroupEndpoint15 authority metadata");
    auto failed = row;
    failed.errors.emplace_back("source-authored GroupEndpoint15 geometry failure");
    OfficialFlowProgress failed_progress;
    failed_progress.captured = failed_progress.bound = failed_progress.decoded = true;
    failed_progress.captures = 5u;
    failed_progress.height = 1;
    failed_progress.phase = "wide";
    failed_progress.remaining.fill(work.remaining);
    failed.raw_source_constraints = official_flow_group_endpoint15_info(
        failed_progress, work.remaining, expected, 0u, tuple.manifest_global_ordinal);
    const auto failed_metadata = json::parse(failed.raw_source_constraints);
    ASSERT_TRUE(failed_metadata.has_value());
    OfficialFlowCounts failed_counts;
    EXPECT_NO_THROW(official_flow_accumulate_owned(failed_metadata->root(), failed, failed_counts,
                                                   JsonVal{}, JsonVal{}, JsonVal{},
                                                   group.asset.root()));
    EXPECT_EQ(failed_counts.admitted, 0u);
    EXPECT_EQ(failed_counts.mutated, 0u);
    EXPECT_EQ(failed_counts.errors, 1u);
}

TEST(MermaidOfficialFlow400, GroupEndpoint15BodyMetadataRequiresExactProvenance) {
    // Parser-only authority control. It creates no frame, renderer result, or
    // formal credit; MermaidCapabilityReport.OfficialMatrix owns the real five-frame path.
    const auto group = flow_oracle::group_endpoint15_documents();
    const auto& fact = kGroupEndpoint15BodyFacts.front();
    flow_oracle::OracleWork work{flow_oracle::kFlow400SceneLimit};
    work.spend(65536u + 16u * sizeof(Observation) + 16u * sizeof(OfficialFlowProgress) +
               16u * sizeof(GroupEndpoint15BodyCounts));
    Observation row;
    row.errors.reserve(6u);
    row.raw_active_schema = kGroupEndpoint15BodyContract;
    row.raw_source_authority = "immutable_Flow400_GroupEndpoint15_body_diagnostic";
    row.track = "body_diagnostic";
    row.type = "flowchart";
    row.role_case_index = fact.ordinal;
    row.path = std::string(fact.path);
    row.source_sha256 = std::string(fact.body_sha);
    row.role_body_sha = std::string(fact.body_sha);
    row.role_model_sha = std::string(fact.model_sha);
    row.projection_baseline = std::string(fact.expectation_canonical);
    row.status = {{"drawn", "drawn", "drawn"}};
    OfficialFlowProgress good;
    good.captured = good.bound = good.decoded = good.admitted = good.restored = good.mutated = true;
    good.captures = 5u;
    good.height = 1;
    good.phase = "complete";
    good.remaining.fill(work.remaining);
    row.raw_source_constraints =
        official_flow_group_endpoint15_body_info(good, work.remaining, 7u, 7u, 5u, true, fact);
    {
        GroupEndpoint15BodyCounts accepted;
        std::vector<Observation> rows{row};
        EXPECT_NO_THROW(accepted = group_endpoint15_body_counts(rows, group));
        EXPECT_EQ(accepted.visited, 1u);
        EXPECT_EQ(accepted.admitted, 1u);
        EXPECT_EQ(accepted.mutated, 1u);
    }
    const auto reject = [&](std::string raw, Observation forged, std::string_view expected_error) {
        const auto parsed = json::parse(raw);
        ASSERT_TRUE(parsed.has_value());
        forged.raw_source_constraints = std::move(raw);
        GroupEndpoint15BodyCounts denied;
        bool rejected = false;
        try {
            std::vector<Observation> rows{std::move(forged)};
            static_cast<void>(group_endpoint15_body_counts(rows, group));
        } catch (const std::exception& error) {
            EXPECT_EQ(std::string_view(error.what()), expected_error);
            EXPECT_FALSE(flow_oracle::flow400_resource(error.what()));
            rejected = true;
        }
        EXPECT_TRUE(rejected);
        EXPECT_EQ(denied.admitted, 0u);
    };
    auto wrong_offset = projection_mutate(row.raw_source_constraints, [](json::JsonMutVal value) {
        value.set("body_offset", static_cast<int64_t>(1));
    });
    reject(std::move(wrong_offset), row,
           "OfficialFlow400 GroupEndpoint15 body metadata provenance");
    auto wrong_digest = projection_mutate(row.raw_source_constraints, [](json::JsonMutVal value) {
        value.set("body_expectation_sha256", std::string(64u, '0'));
    });
    reject(std::move(wrong_digest), row,
           "OfficialFlow400 GroupEndpoint15 body metadata provenance");
    auto wrong_raw = projection_mutate(row.raw_source_constraints, [](json::JsonMutVal value) {
        value.set("raw_source_sha256", std::string(64u, '0'));
    });
    reject(std::move(wrong_raw), row, "OfficialFlow400 GroupEndpoint15 body metadata provenance");
    auto wrong_global = projection_mutate(row.raw_source_constraints, [&](json::JsonMutVal value) {
        value.set("manifest_global_ordinal", static_cast<int64_t>(fact.global + 1u));
    });
    reject(std::move(wrong_global), row,
           "OfficialFlow400 GroupEndpoint15 body authority selection");
    auto wrong_witness = row;
    wrong_witness.projection_baseline = "{}";
    reject(row.raw_source_constraints, std::move(wrong_witness),
           "OfficialFlow400 GroupEndpoint15 body canonical provenance");
    auto cross_track = row;
    cross_track.track = "original";
    reject(row.raw_source_constraints, std::move(cross_track),
           "OfficialFlow400 GroupEndpoint15 body track");
    auto wrong_authority = row;
    wrong_authority.raw_source_authority = "immutable_Flow400_8Models_plus_GroupEndpoint15";
    reject(row.raw_source_constraints, std::move(wrong_authority),
           "OfficialFlow400 GroupEndpoint15 body row identity");
}
}  // namespace

namespace {
TEST(MermaidOfficialRequirement11, SourceBoundOwnedAdmissionRejectsForgedMetadata) {
    // This is a report-parser control, not a drawing result. The only capture
    // path is the selected-original runner above; this control proves that a
    // fabricated row cannot acquire an official admission by changing its
    // source, lifecycle, budget, mutation, or field-cardinality metadata.
    Work setup{r11::kSetupLimit};
    const auto docs = requirement11_documents(setup);
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest.has_value());
    const auto fixtures = manifest->root().get("fixtures");
    ASSERT_EQ(fixtures.size(), 2145u);
    for (std::size_t global = 0u; global < fixtures.size(); ++global) {
        const auto path = fixtures.at(global).get_string("path");
        const auto selected = requirement11_selected_index(global, path);
        const auto expected =
            std::ranges::find_if(kOfficialRequirement11Pins, [&](const auto& pin) {
                return pin.global == global && pin.path == path;
            });
        EXPECT_EQ(selected.has_value(), expected != kOfficialRequirement11Pins.end());
        if (selected)
            EXPECT_EQ(*selected,
                      static_cast<std::size_t>(expected - kOfficialRequirement11Pins.begin()));
    }
    const auto& pin = kOfficialRequirement11Pins.front();
    const auto source = read_file(kRoot / "cases" / pin.path);
    ASSERT_EQ(source.size(), pin.bytes);
    ASSERT_EQ(digest(source), pin.sha);
    Work work{r11::kSceneLimit};
    const auto body = r11::bind_envelope(docs.envelopes.root().get("cases").at(0u), source,
                                         static_cast<int>(pin.ordinal), work);
    ASSERT_TRUE(body.starts_with("requirementDiagram\n"));
    const auto scene = r11::decode_scene(docs.models.root().get("models").at(0u), work);
    ASSERT_EQ(scene.ordinal, static_cast<int>(pin.ordinal));
    ASSERT_EQ(scene.source_sha, pin.sha);
    Observation row;
    row.raw_active_schema = kOfficialRequirement11Contract;
    row.raw_source_authority = kOfficialRequirement11Authority;
    row.track = "original";
    row.type = "requirement";
    row.role_case_index = 0u;
    row.path = std::string(pin.path);
    row.source_sha256 = std::string(pin.sha);
    row.role_model_sha = std::string(kOfficialRequirement11ModelsSha);
    row.role_body_sha = std::string(pin.sha);
    row.status = {"drawn", "drawn", "drawn"};
    Requirement11Progress good;
    good.captures = good.published = 5u;
    good.captured_before_binding = good.bound = good.decoded = true;
    good.wide_good = good.narrow_good = good.restored_good = good.restored_equal =
        good.good_baseline = good.mutated = true;
    good.semantic_rejected = pin.semantic;
    good.physical_rejected = pin.physical;
    good.phase = "complete";
    good.admitted = true;
    good.remaining[0] = work.left;
    auto parse = [&](const Requirement11Progress& progress,
                     std::size_t initial = r11::kSceneLimit) {
        row.raw_source_constraints = requirement11_info(progress, row, pin, work.left, initial);
        auto value = json::parse(row.raw_source_constraints);
        require(value.has_value(), "Requirement11 forged-metadata control JSON");
        return std::move(*value);
    };
    {
        auto metadata = parse(good);
        Requirement11Counts counts;
        EXPECT_NO_THROW(requirement11_accumulate_owned(metadata.root(), row, counts, docs));
        EXPECT_EQ(counts.admitted, 1u);
        EXPECT_EQ(counts.mutated, 1u);
    }
    auto rejected = [&](const Requirement11Progress& forged,
                        std::size_t initial = r11::kSceneLimit) {
        auto metadata = parse(forged, initial);
        Requirement11Counts counts;
        EXPECT_THROW(requirement11_accumulate_owned(metadata.root(), row, counts, docs),
                     std::runtime_error);
        EXPECT_EQ(counts.admitted, 0u);
    };
    auto no_baseline = good;
    no_baseline.good_baseline = false;
    rejected(no_baseline);
    auto no_restore = good;
    no_restore.restored_equal = false;
    rejected(no_restore);
    // Fallback and drawing are a disjoint pair: neither label can be forged.
    auto ordinary_as_drawing = good;
    ordinary_as_drawing.narrow_good = false;
    ordinary_as_drawing.narrow_ordinary = true;
    row.status[1] = "drawn";
    rejected(ordinary_as_drawing);
    row.status[1] = "source_fallback";
    rejected(good);
    row.status[1] = "drawn";
    auto missing_mutation = good;
    --missing_mutation.physical_rejected;
    rejected(missing_mutation);
    auto resource = good;
    resource.resource = true;
    rejected(resource);
    rejected(good, r11::kSceneLimit - 1u);
    const auto saved_source = row.source_sha256;
    row.source_sha256 = std::string(64u, '0');
    rejected(good);
    row.source_sha256 = saved_source;
    auto exact = parse(good).root().to_string();
    const auto extra = json::parse(
        projection_mutate(exact, [](json::JsonMutVal value) { value.set("forged_extra", true); }));
    ASSERT_TRUE(extra.has_value());
    Requirement11Counts counts;
    EXPECT_THROW(requirement11_accumulate_owned(extra->root(), row, counts, docs),
                 std::runtime_error);
}
}  // namespace

namespace {
TEST(MermaidOfficialPie7, SourceBoundOwnedAdmissionRejectsForgedMetadata) {
    // This checks the emitted Official-owned proof only. It parses the frozen
    // seven-source authority and rejects fabricated reporting state; it does
    // not render, read a native report, or borrow a native result.
    pie7::Work setup{kOfficialPie7SetupLimit};
    const auto docs = load_official_pie7_documents(setup);
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest.has_value());
    const auto fixtures = manifest->root().get("fixtures");
    ASSERT_EQ(fixtures.size(), 2145u);
    for (std::size_t global = 0u; global < fixtures.size(); ++global) {
        const auto path = fixtures.at(global).get_string("path");
        const auto selected = official_pie7_selected(global, path);
        const auto expected = std::ranges::find_if(kOfficialPie7Pins, [&](const auto& pin) {
            return pin.global == global && pin.path == path;
        });
        EXPECT_EQ(selected.has_value(), expected != kOfficialPie7Pins.end());
        if (selected)
            EXPECT_EQ(*selected, static_cast<std::size_t>(expected - kOfficialPie7Pins.begin()));
    }
    const auto& pin = kOfficialPie7Pins.front();
    const auto source = read_file(kRoot / "cases" / pin.path);
    ASSERT_EQ(source.size(), pin.bytes);
    ASSERT_EQ(digest(source), pin.sha);
    const auto source_case = docs.authority.root().get("cases").at(0u);
    ASSERT_EQ(source_case.get_string("source_path"),
              "tests/fixtures/mermaid_official/cases/" + std::string(pin.path));
    ASSERT_EQ(source_case.get_string("source_sha256"), pin.sha);
    ASSERT_EQ(source_case.get_string("Model_canonical_sha256"), pin.model_sha);
    Observation row;
    row.raw_active_schema = kOfficialPie7Contract;
    row.raw_source_authority = kOfficialPie7Authority;
    row.track = "original";
    row.type = "pie";
    row.role_case_index = 0u;
    row.path = std::string(pin.path);
    row.source_sha256 = std::string(pin.sha);
    row.role_model_sha = std::string(pin.model_sha);
    row.role_body_sha = std::string(pin.sha);
    row.status = {"drawn", "drawn", "drawn"};
    OfficialPie7Progress good;
    good.captures = good.published = 5u;
    good.captured_before_bind = good.bound = good.decoded = true;
    good.wide_good = good.narrow_good = good.restored_good = good.restored_equal = good.baseline =
        good.mutated = true;
    good.semantic = pin.semantic;
    good.physical = pin.physical;
    good.lifecycle = pin.lifecycle;
    good.phase = "complete";
    good.admitted = true;
    for (std::size_t phase = 0u; phase < 8u; ++phase)
        good.remaining[phase] = kOfficialPie7SceneLimit - 1024u;
    auto parse = [&](const OfficialPie7Progress& progress) {
        row.raw_source_constraints =
            official_pie7_info(progress, row, pin, kOfficialPie7SceneLimit - 1024u);
        auto value = json::parse(row.raw_source_constraints);
        official_pie7_need(value.has_value(), "OfficialPie7 forged-metadata control JSON");
        return std::move(*value);
    };
    {
        auto metadata = parse(good);
        OfficialPie7Counts counts;
        EXPECT_NO_THROW(official_pie7_accumulate_owned(metadata.root(), row, counts, docs));
        EXPECT_EQ(counts.admitted, 1u);
        EXPECT_EQ(counts.mutated, 1u);
    }
    auto rejected = [&](const OfficialPie7Progress& forged) {
        auto metadata = parse(forged);
        OfficialPie7Counts counts;
        EXPECT_THROW(official_pie7_accumulate_owned(metadata.root(), row, counts, docs),
                     std::runtime_error);
        EXPECT_EQ(counts.admitted, 0u);
    };
    auto no_baseline = good;
    no_baseline.baseline = false;
    rejected(no_baseline);
    auto no_restore = good;
    no_restore.restored_equal = false;
    rejected(no_restore);
    // Drawing and source fallback are disjoint report states.
    auto ordinary_as_drawing = good;
    ordinary_as_drawing.narrow_good = false;
    ordinary_as_drawing.narrow_ordinary = true;
    row.status[1] = "drawn";
    rejected(ordinary_as_drawing);
    row.status[1] = "source_fallback";
    rejected(good);
    row.status[1] = "drawn";
    auto missing_mutation = good;
    --missing_mutation.lifecycle;
    rejected(missing_mutation);
    auto resource = good;
    resource.resource = true;
    rejected(resource);
    const auto saved_source = row.source_sha256;
    row.source_sha256 = std::string(64u, '0');
    rejected(good);
    row.source_sha256 = saved_source;
    const auto saved_model = row.role_model_sha;
    row.role_model_sha = std::string(64u, '0');
    rejected(good);
    row.role_model_sha = saved_model;
    const auto saved_authority = row.raw_source_authority;
    row.raw_source_authority = "foreign_authority";
    rejected(good);
    row.raw_source_authority = saved_authority;
    auto reject_metadata = [&](auto mutate) {
        const auto changed = json::parse(projection_mutate(parse(good).root().to_string(), mutate));
        ASSERT_TRUE(changed.has_value());
        OfficialPie7Counts forged;
        EXPECT_THROW(official_pie7_accumulate_owned(changed->root(), row, forged, docs),
                     std::runtime_error);
        EXPECT_EQ(forged.admitted, 0u);
    };
    reject_metadata([](json::JsonMutVal value) { value.set("initial_budget", 127999999); });
    reject_metadata([](json::JsonMutVal value) { value.set("phase", "wide_geometry"); });
    reject_metadata([](json::JsonMutVal value) {
        auto original = value.get("phase_remaining");
        auto replacement = value.make_arr();
        for (std::size_t n = 0; n < original.size(); ++n) {
            const auto entry = original.at(n);
            if (n == 7u)
                replacement.append(value.make_num(128000000));
            else if (entry.is_null())
                replacement.append(value.make_null());
            else
                replacement.append(value.make_num(entry.as_int()));
        }
        value.add("phase_remaining", replacement);
    });
    const auto exact = parse(good).root().to_string();
    const auto extra = json::parse(
        projection_mutate(exact, [](json::JsonMutVal value) { value.set("forged_extra", true); }));
    ASSERT_TRUE(extra.has_value());
    OfficialPie7Counts counts;
    EXPECT_THROW(official_pie7_accumulate_owned(extra->root(), row, counts, docs),
                 std::runtime_error);
}
}  // namespace

namespace {
TEST(MermaidOfficialFlow400, Extra7RawSourceAdmission) {
    auto documents = flow_oracle::group_endpoint_extra7_documents();
    auto control_documents = flow_oracle::group_endpoint_extra7_control_documents();
    const auto records = documents.asset.root().get("records"),
               source_controls = control_documents.controls.root();
    const auto manifest = json::parse(read_file(kRoot / "manifest.json"));
    ASSERT_TRUE(manifest.has_value());
    const auto fixtures = manifest->root().get("fixtures");
    ASSERT_EQ(fixtures.size(), 2145u);
    json::JsonMutDoc local_verdict;
    auto local_cases = local_verdict.array();
    std::size_t visited = 0u, wide_drawn = 0u, restored_drawn = 0u, narrow_drawn = 0u,
                narrow_source_fallback = 0u;
    std::size_t semantic_rejections = 0u, physical_rejections = 0u, decoder_successes = 0u,
                resources = 0u;
    records.iter([&](JsonVal record) {
        flow_oracle::OracleWork work{flow_oracle::kFlow400SceneLimit};
        const auto tuple = flow_oracle::group_endpoint_extra7_tuple(record);
        JsonVal fixture;
        std::size_t fixture_matches = 0u;
        fixtures.iter([&](JsonVal candidate) {
            if (candidate.get_string("path") == tuple.fixture) {
                fixture = candidate;
                ++fixture_matches;
            }
        });
        ASSERT_EQ(fixture_matches, 1u) << tuple.fixture;
        const auto source = read_file(kRoot / "cases" / std::string(tuple.fixture));
        ASSERT_EQ(source.size(), flow_oracle::flow400_number(record.get("source_bytes")));
        ASSERT_EQ(source.size(), fixture.get_int("bytes"));
        ASSERT_EQ(digest(source), tuple.source_sha256);
        ASSERT_EQ(digest(source), fixture.get_string("sha256"));
        ASSERT_EQ(fixture.get_string("git_blob"),
                  flow_oracle::flow400_text(record.get("source_git_blob")));

        const auto drawing = loom::ui::render_markdown(fenced(source, "mermaid", kSentinel));
        const auto plain = loom::ui::render_markdown(fenced(source, "text", kSentinel));
        auto element = ftxui::vbox({drawing});
        auto ordinary = ftxui::vbox({plain});
        element->ComputeRequirement();
        ordinary->ComputeRequirement();
        const int height =
            std::max({32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8});
        ASSERT_LE(height, 1040);
        std::array<std::optional<flow_oracle::Grid>, 5> frames;
        std::array<std::string, 5> frame_hashes{};
        constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
        for (std::size_t index = 0u; index < frames.size(); ++index) {
            frames[index] =
                flow_oracle::capture(index < 3u ? element : ordinary, widths[index], height, &work);
            frame_hashes[index] =
                flow_oracle::group_endpoint_extra7_frame_sha256(*frames[index], &work);
        }
        for (const auto& frame : frames)
            static_cast<void>(flow_oracle::flow400_following(*frame, work, kSentinel));
        ASSERT_TRUE(flow_oracle::flow400_same(*frames[0], *frames[2], work)) << tuple.fixture;
        const bool wide_source = flow_oracle::flow400_same(*frames[0], *frames[3], work);
        const bool narrow_source = flow_oracle::flow400_same(*frames[1], *frames[4], work);
        ASSERT_FALSE(wide_source) << "Official source fallback is not drawing credit: "
                                  << tuple.fixture;

        flow_oracle::group_endpoint_extra7_record(record, work);
        const auto model = flow_oracle::group_endpoint_extra7_model(record.get("model"), work);
        const auto profile = flow_oracle::group_endpoint_extra7_profile(model, work);
        flow_oracle::GroupEndpointGeometryWitness witness;
        const auto wide = flow_oracle::flow_group_endpoint_geometry_with_witness(
            *frames[0], model.base, profile, work, kSentinel, witness);
        ASSERT_TRUE(wide.empty()) << tuple.fixture << ": " << (wide.empty() ? "" : wide.front());
        ++wide_drawn;
        const auto restored = flow_oracle::flow_group_endpoint_geometry(*frames[2], model.base,
                                                                        profile, work, kSentinel);
        ASSERT_TRUE(restored.empty())
            << tuple.fixture << ": " << (restored.empty() ? "" : restored.front());
        ++restored_drawn;
        if (narrow_source)
            ++narrow_source_fallback;
        else {
            const auto narrow = flow_oracle::flow_group_endpoint_geometry(*frames[1], model.base,
                                                                          profile, work, kSentinel);
            ASSERT_TRUE(narrow.empty())
                << tuple.fixture << ": " << (narrow.empty() ? "" : narrow.front());
            ++narrow_drawn;
        }
        const auto controls_row =
            flow_oracle::group_endpoint_extra7_control_row(source_controls, tuple);
        const auto controls = flow_oracle::group_endpoint_extra7_run_controls(
            record, controls_row, *frames[0], model, profile, witness, work, "official_raw",
            kSentinel);
        semantic_rejections += controls.semantic_rejections;
        physical_rejections += controls.physical_rejections;
        decoder_successes += controls.decoder_successes;
        ASSERT_NE(work.remaining, 0u) << tuple.fixture;
        flow_oracle::group_endpoint_extra7_append_case(
            local_verdict, local_cases, record, frame_hashes,
            narrow_source ? "source_fallback" : "drawn", controls);
        ++visited;
    });
    EXPECT_EQ(visited, 7u);
    EXPECT_EQ(wide_drawn, 7u);
    EXPECT_EQ(restored_drawn, 7u);
    EXPECT_EQ(narrow_drawn, 3u);
    EXPECT_EQ(narrow_source_fallback, 4u);
    EXPECT_EQ(semantic_rejections, 35u);
    EXPECT_EQ(physical_rejections, 28u);
    EXPECT_EQ(decoder_successes, 35u);
    EXPECT_EQ(resources, 0u);
    flow_oracle::group_endpoint_extra7_finish_verdict(
        local_verdict, local_cases, "flow400_extra7_official_local_verdict/v1", "official_raw",
        "test_mermaid_official", "test_mermaid_official", visited, visited * 5u, visited * 5u,
        semantic_rejections, physical_rejections, resources, 0u);
    flow_oracle::group_endpoint_extra7_write_atomic(
        fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-flow400-extra7-official-verdict.json",
        local_verdict.to_string() + "\n");
}

TEST(MermaidOfficialFlowchartShapes, CompleteOfficialShapeConfigSourcesOwnFiveFrames) {
    using flow_oracle::Model;
    using flow_oracle::Shape;

    struct Case {
        std::string_view path, sha;
        Model model;
    };

    const std::array cases{
        Case{"flowchart/visual-check/should-render-a-diamond-shaped-node-using-shape-config.mmd",
             "10a38ba8df398ccaf36e51e7f84dccc2a71af21c005e177d30d2c304ea209890",
             {"BT",
              {{"n2", "Untitled Node", Shape::Rectangle}, {"n1", "Diamond", Shape::Decision}},
              {{0, 1, "", "solid", false, true}},
              {}}},
        Case{"flowchart/visual-check/should-render-a-rounded-rectangle-and-a-normal-rectangle.mmd",
             "1384844adca4511a7a6f7917ebd6a9563d7dc877e5a4208f7acfc5e32abafacb",
             {"BT",
              {{"n2", "Untitled Node", Shape::Rectangle},
               {"n1", "Rounded Rectangle", Shape::Rounded},
               {"n3", "Untitled Node", Shape::Rectangle}},
              {{0, 1, "", "solid", false, true}, {2, 1, "", "solid", false, true}},
              {}}},
        Case{"flowchart/visual-check/should-render-a-stadium-shaped-node.mmd",
             "3497ab671f384701e964e3b71127e46ad889c6b67eb4fd18576bbc5a58309b8e",
             {"TB",
              {{"A", "Start", Shape::Stadium},
               {"n1", "Untitled Node", Shape::Rectangle},
               {"n2", "Untitled Node", Shape::Rectangle}},
              {{0, 1, "", "solid", false, true}, {0, 2, "", "solid", false, true}},
              {}}},
    };
    for (const auto& item : cases) {
        SCOPED_TRACE(item.path);
        const auto source = read_file(kRoot / "cases" / std::string(item.path));
        ASSERT_EQ(digest(source), item.sha) << "Pinned complete official source changed";
        auto drawing = loom::ui::render_markdown(fenced(source, "mermaid", flow_oracle::kSentinel));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text", flow_oracle::kSentinel));
        drawing->ComputeRequirement();
        ordinary->ComputeRequirement();
        const int height =
            std::max({32, drawing->requirement().min_y + 8, ordinary->requirement().min_y + 8});
        ASSERT_LE(height, 1040);
        constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
        std::array<std::optional<flow_oracle::Grid>, 5> frames;
        flow_oracle::OracleWork work{flow_oracle::kFlow400SceneLimit};
        for (std::size_t n = 0; n < frames.size(); ++n)
            frames[n] = flow_oracle::capture(n < 3u ? drawing : ordinary, widths[n], height, &work);
        for (const auto& frame : frames)
            static_cast<void>(flow_oracle::flow400_following(*frame, work, flow_oracle::kSentinel));
        ASSERT_TRUE(flow_oracle::flow400_same(*frames[0], *frames[2], work));
        ASSERT_FALSE(flow_oracle::flow400_same(*frames[0], *frames[3], work))
            << "Official full-width source fell back";

        const auto wide =
            flow_oracle::flow400_geometry(*frames[0], item.model, work, flow_oracle::kSentinel);
        ASSERT_TRUE(wide.empty()) << (wide.empty() ? "" : wide.front());
        const bool narrow_source = flow_oracle::flow400_same(*frames[1], *frames[4], work);
        int content_width = 0;
        const auto following =
            flow_oracle::flow400_following(*frames[0], work, flow_oracle::kSentinel);
        for (int y = 0; y < following; ++y)
            for (int x = 0; x < frames[0]->width; ++x)
                if (frames[0]->at({x, y}) != " ") content_width = std::max(content_width, x + 1);
        if (content_width > 40)
            ASSERT_TRUE(narrow_source)
                << "Overwide shape case must use exact narrow source fallback";
        else {
            ASSERT_FALSE(narrow_source) << "Narrow shape case unexpectedly fell back";
            const auto narrow =
                flow_oracle::flow400_geometry(*frames[1], item.model, work, flow_oracle::kSentinel);
            EXPECT_TRUE(narrow.empty()) << (narrow.empty() ? "" : narrow.front());
        }
        EXPECT_NE(work.remaining, 0u) << "Official owned frame/geometry work exhausted";
    }
}

TEST(MermaidOfficialFlowchartConfig, BothMaxWidthSettingsPreserveFullCellSemantics) {
    using flow_oracle::Model;
    using flow_oracle::Shape;
    constexpr std::array sources{
        std::string_view{"---\nconfig:\n  flowchart:\n    useMaxWidth: true\n---\nflowchart "
                         "TD\nA[Start] --> B[Done]\n"},
        std::string_view{"---\nconfig:\n  flowchart:\n    useMaxWidth: false\n---\nflowchart "
                         "TD\nA[Start] --> B[Done]\n"},
    };
    const Model expected{"TB",
                         {{"A", "Start", Shape::Rectangle}, {"B", "Done", Shape::Rectangle}},
                         {{0, 1, "", "solid", false, true}},
                         {}};
    for (const auto source : sources) {
        auto drawing = loom::ui::render_markdown(fenced(source, "mermaid", flow_oracle::kSentinel));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text", flow_oracle::kSentinel));
        drawing->ComputeRequirement();
        ordinary->ComputeRequirement();
        const int height =
            std::max({32, drawing->requirement().min_y + 8, ordinary->requirement().min_y + 8});
        constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
        flow_oracle::OracleWork work{flow_oracle::kFlow400SceneLimit};
        std::array<std::optional<flow_oracle::Grid>, 5> frames;
        for (std::size_t n = 0; n < frames.size(); ++n)
            frames[n] = flow_oracle::capture(n < 3u ? drawing : ordinary, widths[n], height, &work);
        for (std::size_t n = 0; n < frames.size(); ++n) {
            SCOPED_TRACE(n);
            std::string token_row;
            for (int y = 0; y < frames[n]->height; ++y) {
                const auto row = frames[n]->row(y);
                if (row.find(flow_oracle::kSentinel) != std::string::npos) token_row = row;
            }
            ASSERT_FALSE(token_row.empty()) << "Official config source lost following content";
            static_cast<void>(
                flow_oracle::flow400_following(*frames[n], work, flow_oracle::kSentinel));
        }
        ASSERT_TRUE(flow_oracle::flow400_same(*frames[0], *frames[2], work));
        EXPECT_FALSE(flow_oracle::flow400_same(*frames[0], *frames[3], work));
        EXPECT_FALSE(flow_oracle::flow400_same(*frames[1], *frames[4], work));
        EXPECT_TRUE(
            flow_oracle::flow400_geometry(*frames[0], expected, work, flow_oracle::kSentinel)
                .empty());
        EXPECT_TRUE(
            flow_oracle::flow400_geometry(*frames[1], expected, work, flow_oracle::kSentinel)
                .empty());
        EXPECT_NE(work.remaining, 0u);
    }
}
}  // namespace
