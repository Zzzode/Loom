// RFC 0031 FS-3a durable style-projection cohort gate. Public API only (no
// module declaration): enforces tools/mermaid/fs3-cohort.json against the live
// renderer, the pinned manifest and the immutable FlowStyle4 authority asset.
// The four predicted-draw rows are byte-bound to that independently authored
// authority (complete original + separate body Models); their bodies render
// natively at 240 columns with the classDef/class/style statements consumed
// into projected attributes and disclosed in a verbatim projection legend,
// stay on ordinary source fallback at 40 columns, and restore
// pixel-identically. The handDrawn-look row additionally keeps whole-original
// source at every width (look metadata stays FS-8). Every one of the 66
// blocked bodies stays ordinary source at every width.
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
import loom.serdes.json;
import loom.crypto.crypto;

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
using json::JsonDoc;
using json::JsonVal;

const fs::path kTestsDir{LOOM_TESTS_DIR};
const fs::path kCases = kTestsDir / "fixtures/mermaid_official/cases";
const fs::path kManifest = kTestsDir / "fixtures/mermaid_official/manifest.json";
const fs::path kLedger = kTestsDir / "../tools/mermaid/fs3-cohort.json";
const fs::path kStyle4 =
    kTestsDir / "fixtures/mermaid_native/flow_style4_source_models.proposal.v1.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 200;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source +
           "\n```\n\nFS3_COHORT_SENTINEL";
}

struct CellCapture {
    std::vector<std::string> chars;
    std::string text;
};
CellCapture capture(const std::string& source, const char* language, int width) {
    auto element = loom::ui::render_markdown(fenced(source, language));
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
bool same_surface(const CellCapture& a, const CellCapture& b) {
    return a.chars == b.chars;
}

JsonDoc parse_doc(const fs::path& path) {
    auto parsed = json::parse(read_file(path));
    if (!parsed) throw std::runtime_error("cannot parse " + path.string());
    return std::move(*parsed);
}

struct Row {
    std::string path;
    bool draw = false;
    std::array<std::string, 3> original_states;
    std::array<std::string, 3> body_states;
};
std::vector<Row> read_rows(const JsonVal& root) {
    std::vector<Row> rows;
    auto load = [](JsonVal states) {
        return std::array<std::string, 3>{states.get_string("wide_240"),
                                          states.get_string("narrow_40"),
                                          states.get_string("restored_240")};
    };
    root.get("rows").iter([&](const JsonVal value) {
        rows.push_back({value.get_string("path"),
                        value.get_string("prediction") == "predicted-draw",
                        load(value.get("expected_states")),
                        load(value.get("expected_body_states"))});
    });
    return rows;
}
std::string body_after_frontmatter(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    return source.substr(close + 5);
}
}  // namespace

TEST(MermaidFs3Cohort, LedgerMatchesSchemaReviewAndStyle4Authority) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_fs3_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 70);
    EXPECT_EQ(counts.get_int("predicted_draw"), 4);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 66);
    ASSERT_EQ(root.get("review").get_string("verdict"), "approved");

    std::set<std::string> vocabulary;
    root.get("blocker_vocabulary").iter(
        [&](const JsonVal value) { vocabulary.emplace(value.as_str()); });
    EXPECT_EQ(vocabulary.size(), 11u);
    for (const auto* kind : {"flow-style-subgraph", "flow-style-init-directive",
                             "flow-style-named-color", "flow-style-unsupported-css-key",
                             "flow-style-linkstyle", "flow-style-non-rectangle-shape",
                             "flow-style-fanout", "flow-style-inline-class-shorthand",
                             "flow-style-default-class", "flow-style-special-edge",
                             "flow-style-bad-identifier"})
        EXPECT_TRUE(vocabulary.contains(kind)) << kind;

    // The draw set is exactly the four fixtures the immutable FlowStyle4
    // authority independently authored, and ledger shas bind to that asset.
    const auto authority = parse_doc(kStyle4);
    struct AuthorityPin {
        std::string source_sha;
        std::string body_sha;
    };
    std::map<std::string, AuthorityPin> pins;
    authority.root().get("cases").iter([&](const JsonVal value) {
        pins.emplace(value.get_string("fixture"),
                     AuthorityPin{value.get_string("source_sha256"),
                                  value.get_string("body_sha256")});
    });
    ASSERT_EQ(pins.size(), 4u);
    std::set<std::string> draw_paths;
    root.get("rows").iter([&](const JsonVal row) {
        if (row.get_string("prediction") != "predicted-draw") return;
        const auto path = row.get_string("path");
        draw_paths.emplace(path);
        const auto found = pins.find(path);
        ASSERT_NE(found, pins.end()) << path;
        EXPECT_EQ(row.get_string("sha256"), found->second.source_sha);
        const auto bytes = read_file(kCases / path);
        EXPECT_EQ(loom::utils::crypto::sha256(bytes), found->second.source_sha) << path;
        EXPECT_EQ(loom::utils::crypto::sha256(body_after_frontmatter(bytes)),
                  found->second.body_sha)
            << path;
    });
    std::set<std::string> authority_paths;
    for (const auto& [path, _] : pins) authority_paths.emplace(path);
    EXPECT_EQ(draw_paths, authority_paths);
}

TEST(MermaidFs3Cohort, WholeOriginalStatesMatchLedger) {
    const auto ledger = parse_doc(kLedger);
    for (const auto& row : read_rows(ledger.root())) {
        const auto bytes = read_file(kCases / row.path);
        const CellCapture frames[3] = {capture(bytes, "mermaid", kWide),
                                      capture(bytes, "mermaid", kNarrow),
                                      capture(bytes, "mermaid", kWide)};
        for (int state = 0; state < 3; ++state) {
            const int width = state == 1 ? kNarrow : kWide;
            const bool source = same_surface(frames[state], capture(bytes, "text", width));
            EXPECT_EQ(source ? "source" : "drawn", row.original_states[state]) << row.path;
        }
        // The two wide frames must be pixel-identical whenever the original
        // draws; ordinary source is trivially stable.
        EXPECT_TRUE(same_surface(frames[0], frames[2])) << row.path;
    }
}

TEST(MermaidFs3Cohort, DrawBodiesProjectStylesWithLegendAndStaySourceAtNarrow) {
    const auto ledger = parse_doc(kLedger);
    int drawn = 0;
    for (const auto& row : read_rows(ledger.root())) {
        if (!row.draw) continue;
        ++drawn;
        const auto body = body_after_frontmatter(read_file(kCases / row.path));
        const auto wide = capture(body, "mermaid", kWide);
        ASSERT_FALSE(same_surface(wide, capture(body, "text", kWide)))
            << "registered draw body did not render: " << row.path;
        // The graph source is consumed (no header/edge syntax dumped), and the
        // applied statements show up only in the verbatim projection legend.
        EXPECT_EQ(wide.text.find("flowchart"), std::string::npos) << row.path;
        EXPECT_NE(wide.text.find("classDef"), std::string::npos) << row.path;
        EXPECT_NE(wide.text.find(" -> "), std::string::npos) << row.path;
        // Narrow keeps ordinary source fallback; restore is pixel-identical.
        EXPECT_TRUE(same_surface(capture(body, "mermaid", kNarrow),
                                 capture(body, "text", kNarrow)))
            << "draw body should stay source at narrow width: " << row.path;
        EXPECT_TRUE(same_surface(capture(body, "mermaid", kWide), wide))
            << "restore changed pixels: " << row.path;
        EXPECT_EQ(row.body_states[0], "drawn");
        EXPECT_EQ(row.body_states[1], "source");
        EXPECT_EQ(row.body_states[2], "drawn");
    }
    EXPECT_EQ(drawn, 4);
}

TEST(MermaidFs3Cohort, BlockedBodiesStaySource) {
    const auto ledger = parse_doc(kLedger);
    int blocked = 0;
    for (const auto& row : read_rows(ledger.root())) {
        if (row.draw) continue;
        ++blocked;
        const auto body = body_after_frontmatter(read_file(kCases / row.path));
        for (const int width : {kWide, kNarrow})
            EXPECT_TRUE(same_surface(capture(body, "mermaid", width),
                                     capture(body, "text", width)))
                << "blocked body unexpectedly drew: " << row.path;
        for (const auto& state : row.body_states)
            EXPECT_EQ(state, "source") << row.path;
    }
    EXPECT_EQ(blocked, 66);
}
