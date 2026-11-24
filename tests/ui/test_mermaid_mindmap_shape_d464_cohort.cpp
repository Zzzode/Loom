// D-464 mindmap shape and `::icon` annotation cohort gate.
//
// Five mindmap originals were blocked only by the wider shape vocabulary
// (bang `))text((`, cloud `)text(`, hex `{{text}}`) and the anonymous root
// shorthand (`((text))`), or by `::icon(...)` annotation lines. The terminal
// projection maps bang/hex to the square frame, cloud to the rounded frame,
// and never draws an icon glyph; every substitution is disclosed on the
// canonical footer. The gate pins the wide/narrow/restored states, restored
// cell equality, the captions and the disclosure lines.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

import std;
import loom.ui.visual.markdown;
import loom.serdes.json;
import loom.crypto.crypto;

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
using json::JsonDoc;
using json::JsonVal;

const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";
const fs::path kCases = kFixtures / "cases";
const fs::path kManifest = kFixtures / "manifest.json";
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/mindmap-shape-d464-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct CellCapture {
    std::vector<std::string> chars;
    std::string text;
};

CellCapture capture(const std::string& source, const char* language, int width) {
    const std::string fenced =
        "```" + std::string(language) + "\n" + source + "\n```\n\nS464_SENTINEL";
    auto element = loom::ui::render_markdown(fenced);
    CellCapture out;
    if (!element) return out;
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, element);
    out.chars.reserve(static_cast<std::size_t>(width) * kHeight);
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < width; ++x) {
            out.chars.push_back(screen.PixelAt(x, y).character);
            out.text += screen.PixelAt(x, y).character;
        }
    return out;
}

bool same_surface(const CellCapture& a, const CellCapture& b) { return a.chars == b.chars; }
bool draws(const std::string& source, int width) {
    return !same_surface(capture(source, "mermaid", width), capture(source, "text", width));
}

JsonDoc parse_doc(const fs::path& path) {
    auto parsed = json::parse(read_file(path));
    if (!parsed) throw std::runtime_error("cannot parse " + path.string());
    return std::move(*parsed);
}
}  // namespace

TEST(MermaidMindmapShapeD464Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_mindmap_shape_d464_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-464");
    EXPECT_EQ(root.get("counts").get_int("total"), 5);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("mindmap"), 5);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    const auto manifest = parse_doc(kManifest);
    EXPECT_EQ(root.get("upstream").get_string("manifest_sha256"),
              loom::utils::crypto::sha256(read_file(kManifest)));
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    std::vector<std::pair<std::string, std::string>> digest_input;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  row.get_string("sha256"))
            << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 5u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidMindmapShapeD464Cohort, AdmittedRowsDrawAndPinTheirStates) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto states = row.get("expected_states");
        ASSERT_TRUE(draws(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow state";
        const auto wide = capture(source, "mermaid", kWide);
        // Real re-entry: render the narrow width in between.
        static_cast<void>(capture(source, "mermaid", kNarrow));
        EXPECT_TRUE(same_surface(wide, capture(source, "mermaid", kWide)))
            << rel << " restored cells differ from the first wide capture";
        row.get("expected_captions").iter([&](JsonVal caption) {
            const auto text = caption.as_str();
            if (!text.empty()) EXPECT_NE(wide.text.find(text), std::string::npos) << rel << text;
        });
        std::size_t previous = 0;
        row.get("expected_notices").iter([&](JsonVal notice) {
            const auto text = notice.as_str();
            const auto at = wide.text.find(text);
            ASSERT_NE(at, std::string::npos) << rel << " notice: " << text;
            // The disclosure order is part of the contract: layout, bang,
            // cloud, hex, icons.
            EXPECT_GT(at, previous) << rel << " notice order: " << text;
            previous = at;
        });
        ++checked;
    });
    EXPECT_EQ(checked, 5);
}

TEST(MermaidMindmapShapeD464Cohort, MalformedShapesAndIconsStayClosed) {
    // Paired negatives: the wider grammar never admits a malformed tree.
    EXPECT_FALSE(draws("mindmap\nroot\n::icon()\n", kWide)) << "empty icon payload";
    EXPECT_FALSE(draws("mindmap\nroot\n::icon(mdi fire)\n", kWide))
        << "icon-only annotation without a shape";
    EXPECT_FALSE(draws("mindmap\nroot((x))\n::icon( x)\n", kWide))
        << "icon payload with a leading space";
    EXPECT_FALSE(draws("mindmap\nroot((x))\n::icon(a(b))\n", kWide))
        << "nested parentheses in the icon payload";
    EXPECT_FALSE(draws("mindmap\nroot((x))\n::icon(" + std::string(65, 'x') + ")\n", kWide))
        << "icon payload over the 64-byte bound";
    EXPECT_FALSE(draws("mindmap\n::icon(mdi fire)\nroot\n", kWide)) << "icon before any node";
    EXPECT_FALSE(draws("mindmap\nroot))bang[\n", kWide)) << "mismatched close";
    EXPECT_FALSE(draws("mindmap\nroot\nchild((text))\n", kWide))
        << "anonymous shorthand outside the root";
    EXPECT_FALSE(draws("mindmap\nbad id((x))\n", kWide)) << "invalid node id";
    EXPECT_FALSE(draws("mindmap\nroot\n::icon(mdi fire)\n", kWide))
        << "icon-only annotation without a shape";
    // Paired positives: the admitted forms draw.
    EXPECT_TRUE(draws("mindmap\nroot((x))\n", kWide));
    EXPECT_TRUE(draws("mindmap\nroot\n  ::icon(mdi fire)\n  child{{y}}\n", kWide));
}
