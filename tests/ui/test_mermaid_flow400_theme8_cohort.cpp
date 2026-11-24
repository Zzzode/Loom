// D-470 Flow400 theme-group re-baseline, phase 1 gate.
//
// TL round 804 authorizes a per-group Flow400 re-baseline: the eight
// `redux-color-subgraphs` originals whose only non-admitted input is
// `config.theme` from the four redux palettes. This gate certifies the
// supplemental complete typed Models against the production renderer with the
// test-private Flow400 oracle before the matrix wiring opens the palette.
// The base authority and the four earlier supplements stay untouched.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "mermaid_flow400_theme8_authority.hpp"

import std;
import loom.ui.visual.markdown;

namespace {
namespace fs = std::filesystem;
namespace fo = loom::test::mermaid_flow;

const fs::path kCases = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases";
constexpr const char* kSentinel = "D470_SENTINEL";

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string body_of(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    return source.substr(close + 5);
}

fo::Grid render(const std::string& body, int width, fo::OracleWork& work) {
    const std::string fenced = "```mermaid\n" + body + "\n```\n\n" + kSentinel;
    auto element = loom::ui::render_markdown(fenced);
    if (!element) return {};
    element->ComputeRequirement();
    const auto lines = static_cast<int>(std::count(body.begin(), body.end(), '\n')) + 1;
    const int height =
        std::min(1040, std::max({20, lines + 12, element->requirement().min_y + 8}));
    return fo::capture(element, width, height, &work);
}
}  // namespace

TEST(MermaidFlow400Theme8Cohort, SupplementSchemaAndCasePins) {
    auto docs = fo::flow400_theme8_documents();
    const auto root = docs.supplement.root();
    EXPECT_EQ(fo::flow400_text(root.get("schema")), "flow400_theme8_native_supplement/v1");
    const auto counts = root.get("formal_counts");
    EXPECT_EQ(fo::flow400_number(counts.get("cases")), 8u);
    EXPECT_EQ(fo::flow400_number(counts.get("aggregate_complete_after_all_official_runs")), 47u);
    EXPECT_EQ(fo::flow400_number(counts.get("aggregate_pending_after_all_official_runs")), 353u);
    std::size_t cases = 0;
    root.get("cases").iter([&](fo::JsonVal row) {
        const auto fixture = fo::flow400_text(row.get("base_authority").get("fixture"));
        EXPECT_TRUE(fs::exists(kCases / fixture)) << fixture;
        EXPECT_EQ(read_file(kCases / fixture), fo::flow400_text(row.get("source"))) << fixture;
        ++cases;
    });
    EXPECT_EQ(cases, 8u);
}

TEST(MermaidFlow400Theme8Cohort, CertifiedModelsMatchTheProductionRenderer) {
    auto docs = fo::flow400_theme8_documents();
    std::size_t checked = 0;
    docs.supplement.root().get("cases").iter([&](fo::JsonVal row) {
        const auto fixture = fo::flow400_text(row.get("base_authority").get("fixture"));
        const auto source = fo::flow400_text(row.get("source"));
        fo::OracleWork work;
        work.remaining = 400'000'000u;
        const auto model = fo::flow400_theme8_model(row, work);
        const auto body = body_of(std::string{source});
        const auto wide = render(body, 240, work);
        ASSERT_FALSE(wide.cells.empty()) << fixture;
        const auto errors = fo::flow400_geometry(wide, model, work, kSentinel);
        EXPECT_TRUE(errors.empty()) << fixture << ": " << (errors.empty() ? "" : errors.front());
        EXPECT_EQ(fo::flow400_mutations(wide, model, work, kSentinel), 4u) << fixture;
        const auto restored = render(body, 240, work);
        EXPECT_TRUE(fo::flow400_same(wide, restored, work)) << fixture << " restore";
        static_cast<void>(fo::flow400_following(wide, work, kSentinel));
        ++checked;
    });
    EXPECT_EQ(checked, 8u);
}

TEST(MermaidFlow400Theme8Cohort, SourceEnvelopeIsTheClosedThemeProfile) {
    auto docs = fo::flow400_theme8_documents();
    docs.supplement.root().get("cases").iter([&](fo::JsonVal row) {
        const auto envelope = row.get("envelope");
        const auto config = envelope.get("config");
        const auto theme = fo::flow400_text(config.get("theme"));
        EXPECT_TRUE(theme == "\"redux\"" || theme == "\"redux-color\"" ||
                    theme == "\"redux-dark\"" || theme == "\"redux-dark-color\"");
        EXPECT_EQ(fo::flow400_text(envelope.get("profile")), "description_config_theme");
        // The palette is not opened by this phase: the whole source still keeps
        // whole-source fallback until the matrix wiring lands.
        fo::OracleWork work;
        work.remaining = 400'000'000u;
        const std::string source{fo::flow400_text(row.get("source"))};
        const std::string fenced = "```mermaid\n" + source + "\n```\n\n" + kSentinel;
        auto element = loom::ui::render_markdown(fenced);
        ASSERT_TRUE(element);
        element->ComputeRequirement();
        const auto lines = static_cast<int>(std::count(source.begin(), source.end(), '\n')) + 1;
        const int height =
            std::min(1040, std::max({20, lines + 12, element->requirement().min_y + 8}));
        const auto whole = fo::capture(element, 240, height, &work);
        auto ordinary_element =
            loom::ui::render_markdown("```text\n" + source + "\n```\n\n" + kSentinel);
        ASSERT_TRUE(ordinary_element);
        ordinary_element->ComputeRequirement();
        const auto ordinary = fo::capture(ordinary_element, 240, height, &work);
        EXPECT_TRUE(fo::flow400_same(whole, ordinary, work))
            << fo::flow400_text(row.get("case_id")) << " palette opened early";
    });
}
