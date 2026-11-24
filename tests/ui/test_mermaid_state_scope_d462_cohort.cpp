// D-462 state scope-caption admission gate.
//
// The two `should-render-composite-states` originals (v1 `stateDiagram` and
// `stateDiagram-v2`) carried an internal transition caption wider than the
// derived composite. `layout_scope` now widens a State composite by the exact
// deficit when the widest internal caption cannot find the `label width + 4`
// free columns `label_place` requires; a scope whose captions already fit
// keeps its previous width. The gate pins the art contract (frame, labels,
// restored cell equality), the ledger pins, and the paired negatives that must
// still stay whole source.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

import std;
import loom.ui.visual.markdown;
import loom.ui.visual.mermaid;
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
const fs::path kLedger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/state-scope-caption-d462-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nS462_SENTINEL";
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

TEST(MermaidStateScopeD462Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_state_scope_caption_d462_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-462");
    EXPECT_EQ(root.get("counts").get_int("total"), 2);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("state-diagram"), 1);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("state-diagram-v2"), 1);
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
    EXPECT_EQ(digest_input.size(), 2u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidStateScopeD462Cohort, AdmittedRowsDrawAndPinTheirStates) {
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
        // Render the narrow width in between so the restored state is a real
        // re-entry, not a repeat of the same capture (D-461 template).
        static_cast<void>(capture(source, "mermaid", kNarrow));
        EXPECT_TRUE(same_surface(wide, capture(source, "mermaid", kWide)))
            << rel << " restored cells differ from the first wide capture";
        // The frame and every declared label are in the art above the footer.
        EXPECT_NE(wide.text.find("┌"), std::string::npos) << rel;
        EXPECT_NE(wide.text.find("└"), std::string::npos) << rel;
        std::string art = wide.text;
        row.get("expected_art_labels").iter([&](JsonVal label) {
            const auto text = label.as_str();
            if (!text.empty()) EXPECT_NE(art.find(text), std::string::npos) << rel << text;
        });
        ++checked;
    });
    EXPECT_EQ(checked, 2);
}

TEST(MermaidStateScopeD462Cohort, ScopeWideningIsTheOnlyChange) {
    // The very caption that forced the widening is painted inside the scope.
    const std::string source =
        "stateDiagram-v2\n"
        "[*] --> NotShooting\n"
        "state NotShooting {\n"
        "[*] --> Idle: Yet another long long oong oong oong label\n"
        "}\n";
    ASSERT_TRUE(draws(source, kWide));
    EXPECT_NE(capture(source, "mermaid", kWide).text.find("Yet another long long"),
              std::string::npos);
    // A caption that fits the derived width leaves the layout at its previous
    // size: the twin without the label keeps a strictly narrower frame.
    const std::string twin =
        "stateDiagram-v2\n"
        "[*] --> NotShooting\n"
        "state NotShooting {\n"
        "[*] --> Idle: EvConfig\n"
        "}\n";
    ASSERT_TRUE(draws(twin, kWide));
    // The widened scope's frame is exactly the caption width plus the two
    // borders plus the `w+4` free run label_place scans (caption 42 -> 48);
    // the caption-free twin keeps a strictly narrower frame, so the branch
    // widens by the deficit only, never blanket.
    const auto frame_width = [](const std::string& text) {
        auto element = loom::ui::mermaid::render_mermaid(text, ftxui::text(text));
        if (!element) return 0;
        auto screen =
            ftxui::Screen::Create(ftxui::Dimension::Fixed(400), ftxui::Dimension::Fixed(kHeight));
        ftxui::Render(screen, *element);
        for (int y = 0; y < kHeight; ++y) {
            if (screen.PixelAt(0, y).character != "┌") continue;
            int width = 0;
            for (int x = 0; x < 400; ++x)
                if (screen.PixelAt(x, y).character != " ") width = x + 1;
            return width;
        }
        return 0;
    };
    constexpr int kCaptionWidth = 42;  // "Yet another long long oong oong oong label"
    EXPECT_EQ(frame_width(source), kCaptionWidth + 6);
    EXPECT_LT(frame_width(twin), kCaptionWidth + 6);
}

TEST(MermaidStateScopeD462Cohort, OverLimitCaptionStaysClosed) {
    // The widening guard is `caption width + 6 <= kWidthLimit` (240). A
    // 240-character caption still parses (kLabelLimit is 256) but needs 246,
    // so the branch must decline and the scene must stay whole source. A
    // 5000-character caption is rejected earlier (parser limit), so it proves
    // nothing about this guard.
    std::string wide_caption(240, 'x');
    const std::string source = "stateDiagram-v2\n[*] --> S\nstate S {\n[*] --> I: " +
                               wide_caption + "\n}\n";
    EXPECT_FALSE(draws(source, kWide));
}

TEST(MermaidStateScopeD462Cohort, UnclosedCompositeStaysClosed) {
    // Paired negative: an unclosed composite still fails closed, so the
    // widening cannot admit a malformed scope.
    EXPECT_FALSE(draws("stateDiagram-v2\nstate NotShooting {\n[*] --> Idle: x\n", kWide));
}
