// RFC 0030 Phase C: execute imported public inputs through Loom, not the
// upstream renderers. An output report is deliberately not a passing oracle.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>

import std;
import loom.ui.visual.markdown;
import loom.ui.visual.mermaid.testing;
import loom.serdes.json;
import loom.crypto.crypto;

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
using json::JsonVal;
const fs::path kRoot = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_terminal";
constexpr std::array<std::string_view, 5> kVendors{"beautiful_mermaid", "mermaid_ascii", "termaid",
                                                   "mmdflux", "merman"};
constexpr std::string_view kInventorySha256 =
    "d1d7d6541f605a0c7ceb8ca6f65fb25eadf5e7df92be55171bd6207671e51cc3";

std::string read(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

bool relative_asset(std::string_view value) {
    const fs::path path(value);
    if (value.empty() || path.is_absolute() || value.find('\\') != std::string_view::npos)
        return false;
    for (const auto& part : path)
        if (part.empty() || part == "." || part == "..") return false;
    return path.generic_string() == value;
}

void validate_inventory(const fs::path& root, std::string_view lock_bytes) {
    if (loom::utils::crypto::sha256(lock_bytes) != kInventorySha256)
        throw std::runtime_error("Imported inventory changed without independent review");
    auto doc = json::parse(lock_bytes);
    if (!doc) throw std::runtime_error("Invalid inventory JSON");
    const auto lock = doc->root();
    std::set<std::string> names;
    lock.get("files").iter([&](JsonVal file) {
        const auto name = file.get_string("path");
        if (!relative_asset(name) || !names.insert(name).second)
            throw std::runtime_error("Unsafe or duplicate asset: " + name);
        const auto path = root / name;
        if (fs::is_symlink(fs::symlink_status(path)) || !fs::is_regular_file(path))
            throw std::runtime_error("Missing or symlinked asset: " + name);
        const auto bytes = read(path);
        if (bytes.size() != static_cast<std::size_t>(file.get_int("bytes")) ||
            loom::utils::crypto::sha256(bytes) != file.get_string("sha256"))
            throw std::runtime_error("Imported asset changed: " + name);
    });
    std::set<std::string> actual;
    for (const auto vendor : kVendors) {
        const auto directory = root / vendor;
        if (fs::is_symlink(fs::symlink_status(directory)))
            throw std::runtime_error("Vendor root is a symlink");
        for (const auto& file : fs::recursive_directory_iterator(directory)) {
            if (file.is_symlink()) throw std::runtime_error("Symlink in vendor inventory");
            if (file.is_directory()) continue;
            if (!file.is_regular_file()) throw std::runtime_error("Nonregular imported asset");
            actual.insert(file.path().lexically_relative(root).generic_string());
        }
    }
    if (names != actual) throw std::runtime_error("Added or removed imported assets");
    lock.get("vendors").iter([&](JsonVal vendor) {
        const auto bytes = read(root / vendor.get_string("name") / "cases.json");
        if (loom::utils::crypto::sha256(bytes) != vendor.get_string("cases_sha256"))
            throw std::runtime_error("Case manifest changed");
        auto cases_doc = json::parse(bytes);
        if (!cases_doc) throw std::runtime_error("Invalid cases JSON");
        const auto cases = cases_doc->root().get("cases");
        if (cases.size() != static_cast<std::size_t>(vendor.get_int("case_count")))
            throw std::runtime_error("Case denominator changed");
        std::set<std::string> ids, inputs;
        cases.iter([&](JsonVal item) {
            if (!ids.insert(item.get_string("id")).second)
                throw std::runtime_error("Duplicate case identity");
            inputs.insert(item.get_string("sha256"));
        });
        std::string ordered_ids;
        for (const auto& id : ids) {
            if (!ordered_ids.empty()) ordered_ids += '\n';
            ordered_ids += id;
        }
        if (loom::utils::crypto::sha256(ordered_ids) != vendor.get_string("case_ids_sha256") ||
            inputs.size() != static_cast<std::size_t>(vendor.get_int("unique_input_count")))
            throw std::runtime_error("Case identity set changed");
    });
}

std::string capture(ftxui::Element element, int width, int height) {
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, std::move(element));
    std::string result;
    for (int y = 0; y < screen.dimy(); ++y) {
        std::string line;
        for (int x = 0; x < screen.dimx(); ++x)
            line += screen.PixelAt(x, y).character;
        while (!line.empty() && line.back() == ' ')
            line.pop_back();
        result += line + '\n';
    }
    while (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result;
}

std::string fenced(std::string_view source, std::string_view language) {
    std::size_t run = 0, longest = 0;
    for (const char c : source) {
        run = c == '`' ? run + 1 : 0;
        longest = std::max(longest, run);
    }
    const std::string fence(std::max(std::size_t{3}, longest + 1), '`');
    return fence + std::string(language) + "\n" + std::string(source) + "\n" + fence +
           "\n\nAFTER_IMPORTED_MERMAID";
}

struct Observation {
    std::string source, family, sha256;
    std::vector<std::string> sources;
    std::set<std::string> declared_classes;
    std::string status;
    std::string narrow_status;
    bool narrow_source_expected = false;
    std::vector<std::string> errors;
};

std::vector<Observation> load_cases() {
    std::vector<Observation> rows;
    std::map<std::string, std::size_t> identities;
    for (const auto vendor : kVendors) {
        const auto bytes = read(kRoot / vendor / "cases.json");
        auto doc = json::parse(bytes);
        if (!doc) throw std::runtime_error("Invalid cases JSON: " + std::string(vendor));
        const auto cases = doc->root().get("cases");
        if (!cases.is_arr() || cases.size() == 0)
            throw std::runtime_error("Missing public-input inventory: " + std::string(vendor));
        for (std::size_t n = 0; n < cases.size(); ++n) {
            const auto item = cases.at(n);
            const auto source = item.get_string("input");
            const auto sha256 = loom::utils::crypto::sha256(source);
            if (sha256 != item.get_string("sha256"))
                throw std::runtime_error("Source hash mismatch: " + item.get_string("id"));
            // The baseline executes all literal inputs. Classification and
            // semantic mappings are validated by the independently locked port.
            const auto [at, inserted] = identities.emplace(sha256, rows.size());
            if (inserted) {
                Observation row;
                row.source = source;
                row.family = item.get_string("family");
                row.sha256 = sha256;
                rows.push_back(std::move(row));
            }
            auto& row = rows[at->second];
            if (row.source != source) throw std::runtime_error("Source digest collision");
            row.sources.push_back(std::string(vendor) + ":" + item.get_string("id"));
            row.declared_classes.insert(item.get("expectation").get_string("classification"));
        }
    }
    return rows;
}

void observe(Observation& row) {
    auto diagram = loom::ui::render_markdown(fenced(row.source, "mermaid"));
    auto source = loom::ui::render_markdown(fenced(row.source, "text"));
    diagram->ComputeRequirement();
    // Blank spacer columns are part of the public intrinsic layout extent.
    // Trimming captured ink cannot establish whether that full extent fits.
    const int layout_width = diagram->requirement().min_x;
    const int input_lines =
        static_cast<int>(std::count(row.source.begin(), row.source.end(), '\n'));
    const int height =
        std::min(4096, std::max({32, input_lines + 12, diagram->requirement().min_y + 8}));
    const auto wide = capture(diagram, 240, height);
    const auto plain = capture(source, 240, height);
    const auto narrow = capture(diagram, 40, height);
    const auto plain_narrow = capture(source, 40, height);
    const auto restored = capture(diagram, 240, height);
    row.status = wide == plain ? "source_fallback" : "unverified_drawing";
    int visible_width = 0;
    std::istringstream lines(wide);
    for (std::string line; std::getline(lines, line);)
        visible_width = std::max(visible_width, ftxui::string_width(line));
    row.narrow_source_expected =
        row.status == "source_fallback" || std::max(visible_width, layout_width) > 40;
    row.narrow_status = narrow == plain_narrow ? "source_fallback" : "unverified_drawing";
    if ((narrow == plain_narrow) != row.narrow_source_expected)
        row.errors.emplace_back(
            "Narrow output disagrees with the independently measured wide extent/source control");
    if (wide.find("AFTER_IMPORTED_MERMAID") == std::string::npos ||
        narrow.find("AFTER_IMPORTED_MERMAID") == std::string::npos)
        row.errors.emplace_back("Following Markdown was clipped or lost");
    if (wide != restored)
        row.errors.emplace_back("Same-node resize did not restore identical cells");
    if (!row.errors.empty()) row.status = "error";
}

void report(const std::vector<Observation>& rows) {
    json::JsonMutDoc doc;
    auto root = doc.object();
    root.set("schema", 1);
    root.set("kind", "imported_terminal_suite_baseline");
    root.set("qualification",
             "Unverified drawing is not semantic conformance. Source fallback is an unmet positive "
             "drawing obligation. This baseline does not certify the assertion port.");
    root.set("unique_inputs", static_cast<std::int64_t>(rows.size()));
    auto cases = root.make_arr();
    std::map<std::string, std::map<std::string, int>> counts;
    for (const auto& row : rows) {
        auto item = root.make_obj();
        item.set("source_sha256", row.sha256);
        item.set("family", row.family);
        item.set("outcome", row.status);
        item.set("narrow_outcome", row.narrow_status);
        item.set("narrow_source_expected", row.narrow_source_expected);
        auto provenance = root.make_arr();
        for (const auto& id : row.sources)
            provenance.append(root.make_str(id));
        item.add("upstream_case_ids", provenance);
        auto classifications = root.make_arr();
        for (const auto& value : row.declared_classes)
            classifications.append(root.make_str(value));
        item.add("upstream_declared_classifications", classifications);
        auto errors = root.make_arr();
        for (const auto& error : row.errors)
            errors.append(root.make_str(error));
        item.add("errors", errors);
        cases.append(item);
        ++counts[row.family][row.status];
    }
    root.add("cases", cases);
    auto totals = root.make_arr();
    for (const auto& [family, statuses] : counts) {
        auto item = root.make_obj();
        item.set("family", family);
        for (const auto& [status, count] : statuses)
            item.set(status, count);
        totals.append(item);
    }
    root.add("families", totals);
    doc.set_root(root);
    const fs::path path = fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-terminal-baseline.json";
    std::ofstream output(path, std::ios::binary);
    if (!output || !(output << doc.to_pretty_string() << '\n'))
        throw std::runtime_error("Cannot write " + path.string());
}

TEST(MermaidTerminal, BlankBlockSpacerColumnsRemainPartOfTheIntrinsicWidth) {
    constexpr std::string_view source =
        "block-beta\n  columns 5\n  space space Renamed[\"Tag\"] space space";
    auto fallback = ftxui::text("WHOLE_SOURCE_FALLBACK");
    auto node = loom::ui::mermaid::render_mermaid(source, fallback);
    ASSERT_TRUE(node);
    (*node)->ComputeRequirement();
    // Five nine-cell columns, four three-cell gaps and the two-cell root apron.
    EXPECT_EQ((*node)->requirement().min_x, 5 * 9 + 4 * 3 + 2);
    const auto wide = capture(*node, 240, 12);
    EXPECT_NE(wide.find("Tag"), std::string::npos);
    int ink_width = 0;
    std::istringstream lines(wide);
    for (std::string line; std::getline(lines, line);)
        ink_width = std::max(ink_width, ftxui::string_width(line));
    EXPECT_LT(ink_width, 40);
    EXPECT_EQ(capture(*node, 40, 12), "WHOLE_SOURCE_FALLBACK");
    EXPECT_EQ(capture(*node, 240, 12), wide);
    Observation row;
    row.source = source;
    observe(row);
    EXPECT_TRUE(row.errors.empty());
    EXPECT_TRUE(row.narrow_source_expected);
    EXPECT_EQ(row.status, "unverified_drawing");
    EXPECT_EQ(row.narrow_status, "source_fallback");
}

TEST(MermaidTerminal, CompleteImportedInputBaseline) {
    validate_inventory(kRoot, read(kRoot / "inventory.lock.json"));
    auto rows = load_cases();
    ASSERT_FALSE(rows.empty());
    for (auto& row : rows) {
        SCOPED_TRACE(row.sha256);
        observe(row);
        EXPECT_TRUE(row.errors.empty()) << row.family;
    }
    report(rows);
    // This test proves execution/accounting/resize only. The separately
    // reviewed assertion port must establish actual drawing correctness.
}

TEST(MermaidTerminal, ReviewedInventoryAndAllCaseIdentitiesAreIntact) {
    EXPECT_NO_THROW(validate_inventory(kRoot, read(kRoot / "inventory.lock.json")));
}

TEST(MermaidTerminal, InventoryOracleRejectsMutation) {
    const auto bytes = read(kRoot / "inventory.lock.json");
    ASSERT_NO_THROW(validate_inventory(kRoot, bytes));
    EXPECT_THROW(validate_inventory(kRoot, bytes + ' '), std::runtime_error);
    EXPECT_FALSE(relative_asset("../removed-case.json"));
    EXPECT_FALSE(relative_asset("/tmp/foreign-case"));
    EXPECT_FALSE(relative_asset("vendor/../case"));
    const auto temporary =
        fs::temp_directory_path() /
        ("loom-mermaid-inventory-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    struct Cleanup {
        fs::path path;

        ~Cleanup() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    } cleanup{temporary};

    fs::create_directories(temporary);
    for (const auto vendor : kVendors)
        fs::copy(kRoot / vendor, temporary / vendor, fs::copy_options::recursive);
    ASSERT_NO_THROW(validate_inventory(temporary, bytes));
    const auto cases = temporary / "beautiful_mermaid/cases.json";
    const auto original = read(cases);
    // Removing an entire public-case inventory must not become a smaller pass.
    fs::remove(cases);
    EXPECT_THROW(validate_inventory(temporary, bytes), std::runtime_error);
    {
        std::ofstream restored(cases, std::ios::binary);
        restored << original;
    }
    ASSERT_NO_THROW(validate_inventory(temporary, bytes));
    {
        std::ofstream changed(cases, std::ios::binary | std::ios::app);
        changed << ' ';
    }
    EXPECT_THROW(validate_inventory(temporary, bytes), std::runtime_error);
    {
        std::ofstream restored(cases, std::ios::binary);
        restored << original;
    }
    const auto unlisted = temporary / "beautiful_mermaid/unlisted-assertion.txt";
    {
        std::ofstream added(unlisted);
        added << "This must not silently change the case population.";
    }
    EXPECT_THROW(validate_inventory(temporary, bytes), std::runtime_error);
    fs::remove(unlisted);
    ASSERT_NO_THROW(validate_inventory(temporary, bytes));
    const auto asset =
        temporary / "beautiful_mermaid/upstream/src/__tests__/testdata/unicode/single_node.txt";
    const auto asset_bytes = read(asset);
    {
        std::ofstream changed(asset, std::ios::binary | std::ios::app);
        changed << 'X';
    }
    EXPECT_THROW(validate_inventory(temporary, bytes), std::runtime_error);
    {
        std::ofstream restored(asset, std::ios::binary);
        restored << asset_bytes;
    }
    ASSERT_NO_THROW(validate_inventory(temporary, bytes));
    fs::remove(asset);
    EXPECT_THROW(validate_inventory(temporary, bytes), std::runtime_error);
    fs::create_symlink(
        kRoot / "beautiful_mermaid/upstream/src/__tests__/testdata/unicode/single_node.txt", asset);
    EXPECT_THROW(validate_inventory(temporary, bytes), std::runtime_error);
}

// ─────────────────────────────────────────────────────────────────────
// RFC 0031 E6b: per-instance Mermaid layout-rows cache.
// ─────────────────────────────────────────────────────────────────────
namespace e6b {
constexpr std::string_view kChainLR =
    "flowchart LR\nS[Start] --> P[Process] --> Q[Queue] --> E[End]";
constexpr std::string_view kNodeTD = "flowchart TD\nA[X] --> B[Y]";
constexpr std::string_view kAscii = "flowchart LR\nA[Start] -->|go| B[End]";
// Header without any statement: every family rejects it whole-source.
constexpr std::string_view kRejected = "flowchart TD\n";

std::string variant(int n) {
    return "flowchart TD\nA[Alpha" + std::to_string(n) + "] --> B[Beta" + std::to_string(n) + "]";
}
}  // namespace e6b

using loom::ui::mermaid::LayoutRowsCache;

TEST(MermaidRowsCache, IdenticalSourceAndAsciiReusesRows) {
    LayoutRowsCache cache(16, 1u << 20);
    const auto first = cache.get_or_layout(e6b::kNodeTD, false);
    ASSERT_TRUE(first.has_value());
    ASSERT_FALSE(first->empty());
    const auto second = cache.get_or_layout(e6b::kNodeTD, false);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first, *second);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_GT(cache.byte_size(), 0u);
    EXPECT_EQ(cache.misses(), 1u);
    EXPECT_EQ(cache.hits(), 1u);
}

TEST(MermaidRowsCache, AsciiBitPartitionsKey) {
    LayoutRowsCache cache(16, 1u << 20);
    const auto unicode_rows = cache.get_or_layout(e6b::kAscii, false);
    const auto ascii_rows = cache.get_or_layout(e6b::kAscii, true);
    ASSERT_TRUE(unicode_rows.has_value() && !unicode_rows->empty());
    ASSERT_TRUE(ascii_rows.has_value() && !ascii_rows->empty());
    EXPECT_NE(*unicode_rows, *ascii_rows);
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(cache.misses(), 2u);
    EXPECT_EQ(cache.hits(), 0u);
    // Repeats hit their own partition and stay stable.
    EXPECT_EQ(cache.get_or_layout(e6b::kAscii, false), unicode_rows);
    EXPECT_EQ(cache.get_or_layout(e6b::kAscii, true), ascii_rows);
    EXPECT_EQ(cache.hits(), 2u);
    EXPECT_EQ(cache.size(), 2u);
}

TEST(MermaidRowsCache, CrlfBytesArePartOfKey) {
    LayoutRowsCache cache(16, 1u << 20);
    const std::string crlf = "flowchart TD\r\nA[X] --> B[Y]";
    const auto lf_rows = cache.get_or_layout(e6b::kNodeTD, false);
    const auto crlf_rows = cache.get_or_layout(crlf, false);
    ASSERT_TRUE(lf_rows.has_value() && !lf_rows->empty());
    ASSERT_TRUE(crlf_rows.has_value() && !crlf_rows->empty());
    // Two distinct entries even though the parser projects both to the
    // same diagram rows.
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(cache.misses(), 2u);
    EXPECT_EQ(cache.get_or_layout(e6b::kNodeTD, false), lf_rows);
    EXPECT_EQ(cache.get_or_layout(crlf, false), crlf_rows);
    EXPECT_EQ(cache.hits(), 2u);
}

TEST(MermaidRowsCache, NegativeResultCachedAndReproducesFallback) {
    auto fallback = ftxui::text("source-fallback");
    // Uncached baseline: the rejected source returns no diagram.
    ASSERT_FALSE(loom::ui::mermaid::render_mermaid(e6b::kRejected, fallback).has_value());

    LayoutRowsCache cache(16, 1u << 20);
    const auto first = cache.get_or_layout(e6b::kRejected, false);
    ASSERT_TRUE(first.has_value());  // engaged...
    EXPECT_TRUE(first->empty());     // ...empty rejection sentinel
    EXPECT_EQ(cache.size(), 1u);
    const auto second = cache.get_or_layout(e6b::kRejected, false);
    ASSERT_TRUE(second.has_value());
    EXPECT_TRUE(second->empty());
    EXPECT_EQ(cache.misses(), 1u);
    EXPECT_EQ(cache.hits(), 1u);
    // The cached rejection reproduces the whole-source fallback disposition.
    auto again = ftxui::text("source-fallback");
    EXPECT_FALSE(cache.render(e6b::kRejected, false, std::move(again)).has_value());
}

TEST(MermaidRowsCache, EntryCountEvictionIsLru) {
    LayoutRowsCache cache(2, 1u << 20);
    const auto s1 = e6b::variant(1);
    const auto s2 = e6b::variant(2);
    const auto s3 = e6b::variant(3);
    ASSERT_FALSE(cache.get_or_layout(s1, false)->empty());
    ASSERT_FALSE(cache.get_or_layout(s2, false)->empty());
    ASSERT_FALSE(cache.get_or_layout(s3, false)->empty());
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(cache.misses(), 3u);
    // s1 was the LRU front and is gone; re-requesting it is a miss that
    // evicts s2 in turn.
    ASSERT_FALSE(cache.get_or_layout(s1, false)->empty());
    EXPECT_EQ(cache.misses(), 4u);
    EXPECT_EQ(cache.size(), 2u);
    // Order is now [s3, s1]: both hit.
    ASSERT_FALSE(cache.get_or_layout(s3, false)->empty());
    ASSERT_FALSE(cache.get_or_layout(s1, false)->empty());
    EXPECT_EQ(cache.hits(), 2u);
    EXPECT_EQ(cache.size(), 2u);
}

TEST(MermaidRowsCache, ByteCapBoundsRetention) {
    // A 1-byte cap serves every call but retains nothing.
    LayoutRowsCache uncapped(8, 1);
    const auto served = uncapped.get_or_layout(e6b::kNodeTD, false);
    ASSERT_TRUE(served.has_value() && !served->empty());
    EXPECT_EQ(uncapped.size(), 0u);
    EXPECT_EQ(uncapped.byte_size(), 0u);
    const auto served_again = uncapped.get_or_layout(e6b::kNodeTD, false);
    EXPECT_EQ(*served, *served_again);
    EXPECT_EQ(uncapped.misses(), 2u);
    EXPECT_EQ(uncapped.hits(), 0u);

    // A tight-but-positive cap (the larger single-entry size) evicts the
    // prior entry to fit the new one.
    LayoutRowsCache node_seed(8, 1u << 20);
    LayoutRowsCache big_seed(8, 1u << 20);
    ASSERT_FALSE(node_seed.get_or_layout(e6b::kNodeTD, false)->empty());
    ASSERT_FALSE(big_seed.get_or_layout(e6b::variant(9), false)->empty());
    const auto node_bytes = node_seed.byte_size();
    const auto tight_cap = std::max(node_bytes, big_seed.byte_size());
    LayoutRowsCache tight(8, tight_cap);
    ASSERT_FALSE(tight.get_or_layout(e6b::kNodeTD, false)->empty());
    EXPECT_EQ(tight.size(), 1u);
    ASSERT_FALSE(tight.get_or_layout(e6b::variant(9), false)->empty());
    EXPECT_EQ(tight.size(), 1u);
    // kNodeTD was evicted to keep total bytes at/below the cap; re-requesting
    // it is another miss that evicts variant(9) in turn.
    EXPECT_FALSE(tight.get_or_layout(e6b::kNodeTD, false)->empty());
    EXPECT_EQ(tight.misses(), 3u);
    EXPECT_EQ(tight.hits(), 0u);
    EXPECT_LE(tight.byte_size(), tight.byte_capacity());
}

TEST(MermaidRowsCache, ClearDropsEntriesAndCounters) {
    LayoutRowsCache cache(16, 1u << 20);
    ASSERT_FALSE(cache.get_or_layout(e6b::kNodeTD, false)->empty());
    ASSERT_FALSE(cache.get_or_layout(e6b::variant(5), false)->empty());
    ASSERT_FALSE(cache.get_or_layout(e6b::kNodeTD, false)->empty());
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_GT(cache.byte_size(), 0u);
    cache.clear();
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.byte_size(), 0u);
    EXPECT_EQ(cache.hits(), 0u);
    EXPECT_EQ(cache.misses(), 0u);
    // Fresh miss after reset.
    ASSERT_FALSE(cache.get_or_layout(e6b::kNodeTD, false)->empty());
    EXPECT_EQ(cache.misses(), 1u);
}

TEST(MermaidRowsCache, CopiesAndAssignmentOwnIndependentRowsAndLruState) {
    LayoutRowsCache original(2, 1u << 20);
    const auto good = original.get_or_layout(e6b::kNodeTD, false);
    const auto rejected = original.get_or_layout(e6b::kRejected, false);
    ASSERT_TRUE(good && !good->empty());
    ASSERT_TRUE(rejected && rejected->empty());
    auto copied = original;
    LayoutRowsCache assigned(1, 1);
    assigned = original;
    for (auto* cache : {&copied, &assigned}) {
        EXPECT_EQ(cache->get_or_layout(e6b::kNodeTD, false), good);
        EXPECT_EQ(cache->get_or_layout(e6b::kRejected, false), rejected);
        EXPECT_EQ(cache->entry_capacity(), 2u);
        EXPECT_EQ(cache->byte_size(), original.byte_size());
    }
    const auto retained = original.byte_size();
    original.clear();
    EXPECT_EQ(original.size(), 0u);
    EXPECT_GT(copied.byte_size(), 0u);
    EXPECT_EQ(assigned.byte_size(), retained);
    EXPECT_EQ(copied.get_or_layout(e6b::kNodeTD, false), good);
    EXPECT_FALSE(copied.get_or_layout(e6b::variant(77), false)->empty());
    EXPECT_EQ(assigned.get_or_layout(e6b::kRejected, false), rejected);
    const auto& same = assigned;
    assigned = same;
    EXPECT_EQ(assigned.size(), 2u);
    auto art = copied.render(e6b::kNodeTD, false, ftxui::text("fallback"));
    ASSERT_TRUE(art);
    auto baseline = loom::ui::mermaid::render_mermaid(e6b::kNodeTD, ftxui::text("fallback"));
    ASSERT_TRUE(baseline);
    EXPECT_EQ(capture(*art, 240, 40), capture(*baseline, 240, 40));
}

TEST(MermaidRowsCache, Width240And40ShareRowsWithFreshNodeGeometry) {
    LayoutRowsCache cache(16, 1u << 20);
    const auto rows = cache.get_or_layout(e6b::kChainLR, false);
    ASSERT_TRUE(rows.has_value() && !rows->empty());

    auto render_diagram = [&] {
        return cache.render(e6b::kChainLR, false,
                            loom::ui::render_markdown(fenced(std::string(e6b::kChainLR), "text")));
    };
    constexpr int kHeight = 40;
    auto d1 = render_diagram();
    ASSERT_TRUE(d1.has_value());
    const auto wide = capture(*d1, 240, kHeight);
    const auto plain_wide = capture(
        loom::ui::render_markdown(fenced(std::string(e6b::kChainLR), "text")), 240, kHeight);
    const auto plain_narrow =
        capture(loom::ui::render_markdown(fenced(std::string(e6b::kChainLR), "text")), 40, kHeight);
    // The chain draws at 240 but is wider than 40.
    ASSERT_NE(wide, plain_wide);
    ASSERT_NE(wide, plain_narrow);

    auto d2 = render_diagram();
    ASSERT_TRUE(d2.has_value());
    // At 40 the SAME cached rows reproduce the source fallback: the width
    // flip happens inside the fresh MermaidNode, not the cache.
    EXPECT_EQ(capture(*d2, 40, kHeight), plain_narrow);
    auto d3 = render_diagram();
    ASSERT_TRUE(d3.has_value());
    // A third fresh node restores the wide diagram byte-for-byte.
    EXPECT_EQ(capture(*d3, 240, kHeight), wide);
    EXPECT_EQ(cache.misses(), 1u);
    EXPECT_EQ(cache.hits(), 3u);
}

TEST(MermaidRowsCache, MarkdownCallSiteConservesCells) {
    const auto markdown = fenced(std::string(e6b::kNodeTD), "mermaid");
    const auto uncached = capture(loom::ui::render_markdown(markdown), 240, 40);

    loom::ui::MarkdownOptions opts;
    LayoutRowsCache cache(16, 1u << 20);
    opts.mermaid_rows_cache = &cache;
    const auto once = capture(loom::ui::render_markdown(markdown, opts), 240, 40);
    const auto twice = capture(loom::ui::render_markdown(markdown, opts), 240, 40);
    EXPECT_EQ(once, uncached);
    EXPECT_EQ(twice, uncached);
    EXPECT_EQ(cache.misses(), 1u);
    EXPECT_EQ(cache.hits(), 1u);
}
}  // namespace

// RFC 0030: original nonliteral Beautiful Mermaid Flow assertions.
#include "mermaid_flow400_authority.hpp"

namespace terminal_flow_expressions {
namespace f = loom::test::mermaid_flow;

struct SourceFunction {
    std::string_view id;
    int kind;
    std::size_t expressions;
};

constexpr std::array<SourceFunction, 4> kFunctions{{
    {"beautiful_mermaid_e4a085e0bd0b6697", 0, 3},
    {"beautiful_mermaid_cc0fb78b963c854d", 1, 5},
    {"beautiful_mermaid_355b11988fd4e567", 2, 3},
    {"beautiful_mermaid_8253ecd344b35e51", 3, 1},
}};

std::vector<std::string> body_rows(const f::Grid& grid, f::OracleWork& work) {
    const int end = f::flow400_following(grid, work, "AFTER_IMPORTED_MERMAID");
    work.product(static_cast<std::size_t>(end), 3u * sizeof(std::string) + 16u);
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(end));
    for (int y = 0; y < end; ++y) {
        work.product(static_cast<std::size_t>(grid.width), 8u);
        for (int x = 0; x < grid.width; ++x)
            work.product(grid.at({x, y}).size(), 4u);
        result.push_back(grid.row(y));
    }
    return result;
}

// Direct translations of the twelve original expressions, with the following
// Markdown excluded from the source renderer's output role.
std::vector<bool> predicates(const std::vector<std::string>& rows, int kind, f::OracleWork& work) {
    work.spend(1024u);
    auto first = [&](std::string_view text, bool exclude_rules = false) {
        for (std::size_t y = 0; y < rows.size(); ++y) {
            work.product(rows[y].size() + 1u, 8u);
            if (rows[y].find(text) != std::string::npos &&
                (!exclude_rules || (rows[y].find("─") == std::string::npos &&
                                    rows[y].find('-') == std::string::npos)))
                return static_cast<int>(y);
        }
        return -1;
    };
    if (kind == 0) {
        const int a = first("Line1"), b = first("Line2");
        return {a >= 0, b >= 0, b > a};
    }
    if (kind == 1) {
        const int a = first("A", true), b = first("B"), c = first("C");
        return {first("A") >= 0, b >= 0, c >= 0, b > a, c > b};
    }
    if (kind == 2) return {first("One") >= 0, first("Two") >= 0, first("Three") >= 0};
    f::flow400_require(kind == 3, "Unknown source function");
    return {first("┌") >= 0 || first("─") >= 0 || first("│") >= 0};
}

bool all_true(const std::vector<bool>& values) {
    return std::ranges::all_of(values, [](bool value) { return value; });
}

void write_frames(const SourceFunction& function, const std::array<const f::Grid*, 5>& frames,
                  f::OracleWork& work) {
    json::JsonMutDoc doc;
    auto root = doc.object();
    root.set("schema", "beautiful_flow_expression12_actual_frames/v1");
    root.set("id", function.id);
    root.set("qualification",
             "Actual frames acquired before source Model decode; no compatibility conclusion.");
    auto array = root.make_arr();
    constexpr std::array<std::string_view, 5> names{"wide", "ordinary_wide", "narrow",
                                                    "ordinary_narrow", "restored"};
    for (std::size_t n = 0; n < frames.size(); ++n) {
        const auto& frame = *frames[n];
        work.spend(2048u);
        work.product(frame.cells.size(), 48u);
        for (const auto& cell : frame.cells)
            work.product(cell.size(), 16u);
        auto item = root.make_obj();
        item.set("name", names[n]);
        item.set("width", frame.width);
        item.set("height", frame.height);
        auto rows = root.make_arr();
        for (int y = 0; y < frame.height; ++y)
            rows.append(root.make_str(frame.row(y)));
        item.add("rows", rows);
        array.append(item);
    }
    root.add("frames", array);
    doc.set_root(root);
    const auto path = fs::path(LOOM_MERMAID_REPORT_DIR) /
                      ("mermaid-expression12-" + std::string(function.id) + "-frames.json");
    std::ofstream output(path, std::ios::binary);
    f::flow400_require(static_cast<bool>(output) &&
                           static_cast<bool>(output << doc.to_pretty_string() << '\n'),
                       "Cannot publish expression frames");
}
}  // namespace terminal_flow_expressions

TEST(MermaidTerminal, BeautifulFlowOriginalNonliteralExpressions) {
    namespace e = terminal_flow_expressions;
    namespace f = loom::test::mermaid_flow;
    const auto vendor = kRoot / "beautiful_mermaid";
    EXPECT_EQ(f::digest(read(vendor / "upstream/src/__tests__/ascii-multiline.test.ts")),
              "baafc10e0e163fcadc0f99fe9430fae7e57b5fcd50861c2eeadc3727bbb55045");
    EXPECT_EQ(f::digest(read(vendor / "upstream/src/__tests__/ascii.test.ts")),
              "b4c17ccdc061125b1cbe9b3136cf42f387d9ab719a161834bc28de6deafb51c5");
    f::OracleWork setup;
    const auto path = vendor / "cases.json";
    ASSERT_EQ(fs::file_size(path), 484264u);
    setup.product(484264u, 12u);
    const auto bytes = read(path);
    ASSERT_EQ(f::digest(bytes), "4a1fab2e4235ba475f00fe9c687bce0d1cb15f7bacf470c5d287a0ba957a4293");
    auto doc = json::parse(bytes);
    ASSERT_TRUE(doc);
    std::size_t completed = 0, expressions = 0, rejected = 0;
    for (const auto& function : e::kFunctions) {
        SCOPED_TRACE(function.id);
        f::OracleWork work;
        work.remaining = f::kFlow400SceneLimit;
        JsonVal row;
        std::size_t found = 0;
        doc->root().get("cases").iter([&](JsonVal candidate) {
            work.spend(256u);
            if (candidate.get_string("id") == function.id) {
                row = candidate;
                ++found;
            }
        });
        ASSERT_EQ(found, 1u);
        const auto source = row.get_string("input");
        work.product(source.size() + 128u, 32u);
        auto diagram = loom::ui::render_markdown(fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
        const auto wide = f::capture(diagram, 240, 64, &work);
        const auto plain = f::capture(ordinary, 240, 64, &work);
        const auto narrow = f::capture(diagram, 40, 64, &work);
        const auto plain_narrow = f::capture(ordinary, 40, 64, &work);
        const auto restored = f::capture(diagram, 240, 64, &work);
        e::write_frames(function, {&wide, &plain, &narrow, &plain_narrow, &restored}, work);
        for (const auto* frame : {&wide, &plain, &narrow, &plain_narrow, &restored})
            static_cast<void>(f::flow400_following(*frame, work, "AFTER_IMPORTED_MERMAID"));
        ASSERT_EQ(f::digest(source), row.get_string("sha256"));
        const auto encoded = row.get("expectation").get("semantic_model").to_string();
        work.product(encoded.size() + 1u, 128u);
        const f::Model model = f::parse_model(row.get("expectation").get("semantic_model"));
        ASSERT_FALSE(f::flow400_same(wide, plain, work)) << "Source fallback is not a pass";
        ASSERT_TRUE(f::flow400_geometry(wide, model, work, "AFTER_IMPORTED_MERMAID").empty());
        ASSERT_TRUE(f::flow400_same(wide, restored, work));
        if (!f::flow400_same(narrow, plain_narrow, work))
            ASSERT_TRUE(f::flow400_geometry(narrow, model, work, "AFTER_IMPORTED_MERMAID").empty());
        else
            EXPECT_GT(f::flow400_following(narrow, work, "AFTER_IMPORTED_MERMAID"), 0);
        const auto rows = e::body_rows(wide, work);
        const auto good = e::predicates(rows, function.kind, work);
        ASSERT_EQ(good.size(), function.expressions);
        ASSERT_TRUE(e::all_true(good));
        expressions += good.size();
        work.product(rows.size(), 3u * sizeof(std::string) + 16u);
        for (const auto& line : rows)
            work.product(line.size() + 1u, 8u);
        auto wrong = rows;
        auto locate = [&](std::string_view value) {
            for (std::size_t n = 0; n < wrong.size(); ++n) {
                work.product(wrong[n].size() + 1u, 8u);
                if (wrong[n].find(value) != std::string::npos) return n;
            }
            throw std::runtime_error("Missing good-first source predicate witness");
        };
        if (function.kind == 0)
            std::swap(wrong[locate("Line1")], wrong[locate("Line2")]);
        else if (function.kind == 1)
            std::swap(wrong[locate("B")], wrong[locate("C")]);
        else if (function.kind == 2) {
            auto& line = wrong[locate("Two")];
            line.replace(line.find("Two"), 3u, "   ");
        } else {
            for (auto& line : wrong)
                for (const auto glyph : {"┌", "─", "│"}) {
                    for (std::size_t at = line.find(glyph); at != std::string::npos;
                         at = line.find(glyph)) {
                        work.product(line.size() + 1u, 8u);
                        line.replace(at, std::string_view(glyph).size(), " ");
                    }
                }
        }
        ASSERT_FALSE(e::all_true(e::predicates(wrong, function.kind, work)));
        ++rejected;
        ++completed;
    }
    EXPECT_EQ(completed, 4u);
    EXPECT_EQ(expressions, 12u);
    EXPECT_EQ(rejected, 4u);
}
