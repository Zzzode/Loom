// RFC 0030 D1a: public terminal style behavior and unchanged plain-path budgets.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>
import std;
import loom.ui.visual.mermaid.testing;
import loom.crypto.crypto;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_tokens;
import loom.ui.visual.markdown;
import loom.serdes.json;

#include "mermaid_class_defined6_proof.hpp"
#include "mermaid_class_style13_proof.hpp"
#include "mermaid_flow_style4_oracle.hpp"

namespace {
namespace testing = loom::ui::mermaid::testing;

// Measured before D1a producer edits: mechanical glyph/Work regression pins,
// not source-model or independent semantic admission evidence.
TEST(MermaidClassStyleBaseline, PlainGlyphsAndMeasuredWorkRemainUnchanged) {
    struct Case {
        std::string_view name, source;
        bool envelope;
        std::size_t spent;
        std::string_view sha;
    };

    constexpr std::array<Case, 5> cases{
        {{"class_empty", "classDiagram\nclass Class10\n", false, 5665u,
          "8496805458e312cf261790f29488422b9ab8dd97285750405ecf4007b6000609"},
         {"class_members", "classDiagram\nclass A {\nint count\nrun() bool\n}\nA --> B : uses\n",
          false, 20429u, "8aa071e9560aa38b56bde4c65ccec34aa52c00a3ba245689094200d607d6731e"},
         {"class_config",
          "---\nconfig:\n  htmlLabels: false\n  look: handDrawn\n---\nclassDiagram\nclass "
          "Class10\n",
          true, 11636u, "8496805458e312cf261790f29488422b9ab8dd97285750405ecf4007b6000609"},
         {"er", "erDiagram\nPERSON ||--o{ ORDER : places\n", false, 12964u,
          "603ce1720d358d2dd79213893a348e974ababfdfe7db7bb9cbe1894c0a9a3585"},
         {"state", "stateDiagram-v2\n[*] --> Waiting\nWaiting --> Done : next\n", false, 28796u,
          "e8fa51965fffa49f6f8d81e600d826940f36f6341deae8eeed3fd46f10277f77"}}};
    for (const auto& item : cases) {
        SCOPED_TRACE(item.name);
        testing::Work work;
        const auto rows = testing::class_relations_rows(item.source, work, item.envelope);
        ASSERT_TRUE(rows);
        ASSERT_FALSE(rows->empty());
        std::string text;
        for (const auto& row : *rows) {
            text += row;
            text += '\n';
        }
        EXPECT_EQ(testing::kWorkLimit - work.remaining, item.spent);
        EXPECT_EQ(loom::utils::crypto::sha256(text), item.sha);
    }
}
}  // namespace

namespace {
struct StyleThemeRestore {
    loom::ui::design::theme::Theme previous = loom::ui::design::theme::current_theme();

    ~StyleThemeRestore() { loom::ui::design::theme::set_theme(previous); }
};

ftxui::Screen style_screen(ftxui::Element element, int width = 240) {
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(32));
    ftxui::Render(screen, std::move(element));
    return screen;
}

constexpr std::string_view kBasicClassStyle =
    "classDiagram\nclass Class10\nstyle Class10 "
    "fill:#f9f,stroke:#333,stroke-width:4px,color:white\n";

TEST(MermaidClassStyles, DirectStyleOwnsFillAndBorderAttributesAndDisclosesProjection) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    auto element =
        loom::ui::mermaid::render_mermaid(kBasicClassStyle, ftxui::text("STYLE_FALLBACK"));
    ASSERT_TRUE(element);
    auto screen = style_screen(*element);
    const auto& palette = *current_theme().palette;
    EXPECT_TRUE(screen.PixelAt(0, 0).bold);
    EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, palette.text);
    EXPECT_EQ(screen.PixelAt(1, 1).background_color, palette.surface_tint_4);
    EXPECT_FALSE(screen.PixelAt(1, 1).bold);
    const auto text = screen.ToString();
    EXPECT_NE(text.find("style Class10 fill:#f9f,stroke:#333,stroke-width:4px,color:white ->"),
              std::string::npos);
    EXPECT_NE(text.find("not projected on fill"), std::string::npos);
    EXPECT_EQ(text.find("STYLE_FALLBACK"), std::string::npos);
}

TEST(MermaidClassStyles, CachedPayloadResolvesTheCurrentThemeAndPreservesPlainProjection) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    loom::ui::mermaid::LayoutRowsCache cache(4u, 1u << 20u);
    set_theme(ThemeVariant::Dark);
    auto dark = cache.render(kBasicClassStyle, false, ftxui::text("STYLE_FALLBACK"));
    ASSERT_TRUE(dark);
    const auto dark_screen = style_screen(*dark);
    EXPECT_EQ(dark_screen.PixelAt(1, 1).background_color, current_theme().palette->surface_tint_4);
    const auto rows = cache.get_or_layout(kBasicClassStyle, false);
    ASSERT_TRUE(rows);
    ASSERT_FALSE(rows->empty());
    set_theme(ThemeVariant::Light);
    auto light = cache.render(kBasicClassStyle, false, ftxui::text("STYLE_FALLBACK"));
    ASSERT_TRUE(light);
    const auto light_screen = style_screen(*light);
    EXPECT_EQ(light_screen.PixelAt(1, 1).background_color, current_theme().palette->surface_tint_4);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 240; ++x)
            EXPECT_EQ(dark_screen.PixelAt(x, y).character, light_screen.PixelAt(x, y).character);
    EXPECT_GE(cache.hits(), 2u);
}
}  // namespace

namespace {
TEST(MermaidClassStyles, ClosedColorSubsetAndExactPhysicalDeclarationWhitespace) {
    for (const auto value : {"none", "transparent", "red", "rgb(255,0,0)", "#ff000080"}) {
        SCOPED_TRACE(value);
        const auto source = std::string("classDiagram\nclass A\nstyle A fill:") + value + "\n";
        EXPECT_FALSE(loom::ui::mermaid::render_mermaid(source, ftxui::text("STYLE_FALLBACK")));
    }
    for (const auto value : {"#0000", "#ffffff00", "white"}) {
        SCOPED_TRACE(value);
        const auto source = std::string("classDiagram\nclass A\nstyle A fill:") + value + "\n";
        EXPECT_TRUE(loom::ui::mermaid::render_mermaid(source, ftxui::text("STYLE_FALLBACK")));
    }
    const std::string source =
        "classDiagram\nclass A\n \tstyle A fill:#f9f  \t\nstyle A FILL:#ff99ff\n";
    const auto element = loom::ui::mermaid::render_mermaid(source, ftxui::text("STYLE_FALLBACK"));
    ASSERT_TRUE(element);
    const auto text = style_screen(*element).ToString();
    EXPECT_NE(text.find(" \\tstyle A fill:#f9f  \\t -> fill:Purple(surface tint) occurrences:2"),
              std::string::npos);
    EXPECT_NE(text.find("source TAB=\\t"), std::string::npos);
    const auto oversized =
        "classDiagram\nclass A\n" + std::string(120u, '\t') + "style A fill:#f9f\n";
    EXPECT_FALSE(loom::ui::mermaid::render_mermaid(oversized, ftxui::text("STYLE_FALLBACK")));
}

TEST(MermaidClassStyles, ExactAndOneShortWorkReachThePublicElementConstruction) {
    testing::Work measured;
    const auto good = testing::render_mermaid_with_work(
        kBasicClassStyle, ftxui::text("STYLE_FALLBACK"), false, measured);
    ASSERT_TRUE(good);
    const auto spent = testing::kWorkLimit - measured.remaining;
    ASSERT_GT(spent, 8192u);
    testing::Work exact{spent};
    const auto fitted = testing::render_mermaid_with_work(
        kBasicClassStyle, ftxui::text("STYLE_FALLBACK"), false, exact);
    ASSERT_TRUE(fitted);
    EXPECT_EQ(exact.remaining, 0u);
    EXPECT_EQ(style_screen(*good).ToString(), style_screen(*fitted).ToString());
    for (const auto limit : {std::size_t{0u}, std::size_t{8191u}, spent / 2u, spent - 1u}) {
        SCOPED_TRACE(limit);
        testing::Work short_work{limit};
        EXPECT_FALSE(testing::render_mermaid_with_work(
            kBasicClassStyle, ftxui::text("STYLE_FALLBACK"), false, short_work));
        EXPECT_EQ(short_work.remaining, 0u);
    }
    const std::string inactive = "classDiagram\nclass A\nclassDef unused fill:#f9f\n";
    EXPECT_TRUE(loom::ui::mermaid::render_mermaid(inactive, ftxui::text("STYLE_FALLBACK")));
    EXPECT_FALSE(loom::ui::mermaid::render_mermaid(
        "classDiagram\nclass A\nclassDef default fill:#f9f\n", ftxui::text("STYLE_FALLBACK")));
}
}  // namespace

namespace {
namespace style13 = loom::test::mermaid_class_style13;
constexpr std::string_view kStyle13Following = "AFTER_CLASS_STYLE13";

std::string style13_fenced(std::string_view source, std::string_view language) {
    return "```" + std::string(language) + "\n" + std::string(source) + "\n```\n\n" +
           std::string(kStyle13Following) + "\n";
}

TEST(MermaidClassStyle13Prototype,
     WholeSourcesCaptureBeforeBindingAndRequireScreenGeometryAndAttributes) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    style13::Work setup{128000000u};
    const auto docs = style13::load(setup);
    const auto manifest_path =
        std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
    const auto manifest_bytes = std::filesystem::file_size(manifest_path);
    ASSERT_LT(manifest_bytes, 4000000u);
    setup.spend(manifest_bytes * 24u + 65536u);
    std::ifstream input(manifest_path, std::ios::binary);
    ASSERT_TRUE(input);
    const std::string raw{std::istreambuf_iterator<char>(input), {}};
    auto parsed = loom::utils::json::parse(raw);
    ASSERT_TRUE(parsed);
    const auto shared_spent = 128000000u - setup.left;
    std::size_t visited = 0u;
    for (std::size_t i = 0; i < style13::kPins.size(); ++i) {
        const auto& pin = style13::kPins[i];
        SCOPED_TRACE(pin.fixture);
        style13::Work work{128000000u};
        work.spend(shared_spent + pin.source_bytes * 16u + 8192u);
        std::ifstream source_input(std::filesystem::path(LOOM_TESTS_DIR) /
                                       "fixtures/mermaid_official/cases" / pin.fixture,
                                   std::ios::binary);
        ASSERT_TRUE(source_input);
        const std::string source{std::istreambuf_iterator<char>(source_input), {}};
        auto native = loom::ui::render_markdown(style13_fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(style13_fenced(source, "text"));
        native->ComputeRequirement();
        ordinary->ComputeRequirement();
        const int height = std::min(1040, std::max({32, native->requirement().min_y + 8,
                                                    ordinary->requirement().min_y + 8}));
        std::array<style13::Screen, 5> frames;
        frames[0] = style13::capture(native, 240, height, work);
        frames[1] = style13::capture(ordinary, 240, height, work);
        frames[2] = style13::capture(native, 40, height, work);
        frames[3] = style13::capture(ordinary, 40, height, work);
        frames[4] = style13::capture(native, 240, height, work);
        // These are actual public frames. Semantic decode deliberately follows
        // all five captures; this prototype itself earns no matrix admission.
        EXPECT_NE(frames[0].glyphs, frames[1].glyphs);
        EXPECT_EQ(frames[0].glyphs, frames[4].glyphs);
        EXPECT_EQ(frames[0].attributes, frames[4].attributes);
        EXPECT_EQ(frames[2].glyphs, frames[3].glyphs);
        // Mermaid and text source blocks may have different syntax-token
        // foregrounds (the existing E2a policy). Preserve the full source and
        // backgrounds/flags/links; no diagram fill or border style may leak.
        ASSERT_EQ(frames[2].attributes.size(), frames[3].attributes.size());
        for (std::size_t n = 0; n < frames[2].attributes.size(); ++n) {
            EXPECT_EQ(frames[2].attributes[n].background, frames[3].attributes[n].background);
            EXPECT_EQ(frames[2].attributes[n].flags, frames[3].attributes[n].flags);
            EXPECT_EQ(frames[2].attributes[n].hyperlink, frames[3].attributes[n].hyperlink);
        }
        const auto expected = style13::bind(docs, i, pin.ordinal, pin.global, source,
                                            parsed->root().get("fixtures").at(pin.global), work);
        for (const auto frame : {0u, 4u}) {
            SCOPED_TRACE(frame);
            const auto result = style13::check(frames[frame], expected, *current_theme().palette,
                                               work, kStyle13Following);
            EXPECT_EQ(result.outcome, style13::Outcome::Match)
                << "reason=" << static_cast<int>(result.reason) << " resource=" << result.resource
                << " remaining=" << work.left;
        }
        const auto baseline =
            style13::check(frames[0], expected, *current_theme().palette, work, kStyle13Following);
        ASSERT_EQ(baseline.outcome, style13::Outcome::Match);
        ASSERT_TRUE(baseline.box);
        std::size_t negatives = 0u;
        for (std::size_t kind = 0; kind < style13::kPhysicalNegatives; ++kind) {
            SCOPED_TRACE(kind);
            auto wrong = style13::copy_screen(frames[0], work);
            style13::mutate(wrong, *baseline.box, kind, work);
            const auto result =
                style13::check(wrong, expected, *current_theme().palette, work, kStyle13Following);
            EXPECT_EQ(result.outcome, style13::Outcome::Mismatch)
                << "reason=" << static_cast<int>(result.reason) << " resource=" << result.resource;
            if (result.outcome == style13::Outcome::Mismatch) ++negatives;
        }
        EXPECT_EQ(negatives, style13::kPhysicalNegatives);
        EXPECT_GT(work.left, 0u);
        ++visited;
    }
    EXPECT_EQ(visited, 13u);
}
}  // namespace

namespace {
TEST(MermaidClassStyle13Oracle,
     IndependentPaperRequiresEightDecisiveNegativesAndRejectsResourceCredit) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    const auto& palette = *current_theme().palette;
    style13::Expected expected;
    expected.raw_declaration = "  style Class10 fill:#f9f,stroke:#333,stroke-width:4px";
    expected.graph = {"class", "TB", {{"Class10", "Class10", "box", "", {{"Class10"}}, {}}},
                      {},      {},   {}};
    style13::Work setup{128000000u};
    const auto paper = style13::hand_paper(expected, palette, setup, kStyle13Following);
    const auto baseline = style13::check(paper, expected, palette, setup, kStyle13Following);
    ASSERT_EQ(baseline.outcome, style13::Outcome::Match);
    ASSERT_TRUE(baseline.box);
    for (std::size_t kind = 0; kind < style13::kPhysicalNegatives; ++kind) {
        SCOPED_TRACE(kind);
        auto wrong = style13::copy_screen(paper, setup);
        style13::mutate(wrong, *baseline.box, kind, setup);
        EXPECT_EQ(style13::check(wrong, expected, palette, setup, kStyle13Following).outcome,
                  style13::Outcome::Mismatch);
    }
    style13::Work measured{128000000u};
    ASSERT_EQ(style13::check(paper, expected, palette, measured, kStyle13Following).outcome,
              style13::Outcome::Match);
    const auto spent = 128000000u - measured.left;
    style13::Work exact{spent};
    EXPECT_EQ(style13::check(paper, expected, palette, exact, kStyle13Following).outcome,
              style13::Outcome::Match);
    EXPECT_EQ(exact.left, 0u);
    for (const auto limit : {std::size_t{0u}, spent - 1u}) {
        style13::Work short_work{limit};
        const auto result = style13::check(paper, expected, palette, short_work, kStyle13Following);
        EXPECT_EQ(result.outcome, style13::Outcome::Inconclusive);
        EXPECT_TRUE(result.resource);
    }
    const auto lines = style13::legend(expected, setup);
    const int fit = 2 + static_cast<int>(std::max(lines.first.size(), lines.second.size()));
    const auto exact_screen = style13::crop_width(paper, fit, setup);
    EXPECT_EQ(style13::check(exact_screen, expected, palette, setup, kStyle13Following).outcome,
              style13::Outcome::Match);
    const auto cropped = style13::crop_width(paper, fit - 1, setup);
    EXPECT_EQ(style13::check(cropped, expected, palette, setup, kStyle13Following).outcome,
              style13::Outcome::Inconclusive);
    auto wrong_direction = expected;
    wrong_direction.graph.direction = "unknown";
    EXPECT_EQ(style13::check(paper, wrong_direction, palette, setup, kStyle13Following).outcome,
              style13::Outcome::Inconclusive);
    auto wrong_shape = expected;
    wrong_shape.graph.entities.front().shape = "unknown";
    EXPECT_EQ(style13::check(paper, wrong_shape, palette, setup, kStyle13Following).outcome,
              style13::Outcome::Inconclusive);
    auto oversized_cell = style13::copy_screen(paper, setup);
    setup.spend(4096u);
    oversized_cell.glyphs.cells.front() = std::string(1024u, 'x');
    EXPECT_EQ(style13::check(oversized_cell, expected, palette, setup, kStyle13Following).outcome,
              style13::Outcome::Inconclusive);
    auto unknown = expected;
    unknown.graph.family = "unknown";
    EXPECT_EQ(style13::check(paper, unknown, palette, setup, kStyle13Following).outcome,
              style13::Outcome::Inconclusive);
    auto noop = style13::copy_screen(paper, setup);
    EXPECT_EQ(style13::check(noop, expected, palette, setup, kStyle13Following).outcome,
              style13::Outcome::Match);
}

TEST(MermaidClassStyles, StyledCacheCopiesEvictionAsciiAndExactFitPreserveOwnedData) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    loom::ui::mermaid::LayoutRowsCache cache(4u, 1u << 20u);
    ASSERT_TRUE(cache.render(kBasicClassStyle, false, ftxui::text("STYLE_FALLBACK")));
    auto copy = cache;
    cache.clear();
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(copy.size(), 1u);
    auto rows = copy.get_or_layout(kBasicClassStyle, false);
    ASSERT_TRUE(rows);
    rows->front() = "CORRUPTED_CALLER_COPY";
    set_theme(ThemeVariant::Light, Accessibility{false, false, true});
    const auto kept = copy.render(kBasicClassStyle, false, ftxui::text("STYLE_FALLBACK"));
    ASSERT_TRUE(kept);
    const auto actual = style_screen(*kept);
    EXPECT_EQ(actual.PixelAt(1, 1).background_color, current_theme().palette->surface_tint_4);
    EXPECT_EQ(actual.ToString().find("CORRUPTED_CALLER_COPY"), std::string::npos);
    EXPECT_FALSE(copy.render(kBasicClassStyle, true, ftxui::text("STYLE_FALLBACK")));
    loom::ui::mermaid::LayoutRowsCache tiny(1u, 1u);
    const auto uncached = tiny.render(kBasicClassStyle, false, ftxui::text("STYLE_FALLBACK"));
    ASSERT_TRUE(uncached);
    EXPECT_EQ(tiny.size(), 0u);
    EXPECT_EQ(tiny.byte_size(), 0u);
    EXPECT_EQ(style_screen(*uncached).ToString(), actual.ToString());
    style13::Expected expected;
    expected.raw_declaration = "style Class10 fill:#f9f,stroke:#333,stroke-width:4px,color:white";
    expected.source_color = true;
    style13::Work work;
    const auto lines = style13::legend(expected, work);
    const int width = static_cast<int>(std::max(lines.first.size(), lines.second.size()));
    const auto native =
        loom::ui::mermaid::render_mermaid(kBasicClassStyle, ftxui::text("STYLE_FALLBACK"));
    ASSERT_TRUE(native);
    EXPECT_EQ(style_screen(*native, width).ToString().find("STYLE_FALLBACK"), std::string::npos);
    EXPECT_NE(style_screen(*native, width - 1).ToString().find("STYLE_FALLBACK"),
              std::string::npos);
}
}  // namespace

namespace {
TEST(MermaidClassStyle13Prototype, SevenPalettesAndBothAccessibilityFlagsKeepWholeCachedScreens) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    constexpr std::array<ThemeVariant, 7> variants{
        {ThemeVariant::Dark, ThemeVariant::Light, ThemeVariant::DarkDaltonized,
         ThemeVariant::LightDaltonized, ThemeVariant::Monochrome, ThemeVariant::LightAnsi,
         ThemeVariant::DarkAnsi}};
    style13::Work setup{128000000u};
    const auto docs = style13::load(setup);
    const auto path =
        std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
    const auto bytes = std::filesystem::file_size(path);
    ASSERT_LT(bytes, 4000000u);
    setup.spend(bytes * 24u + 65536u);
    std::ifstream input(path, std::ios::binary);
    ASSERT_TRUE(input);
    const std::string raw{std::istreambuf_iterator<char>(input), {}};
    auto manifest = loom::utils::json::parse(raw);
    ASSERT_TRUE(manifest);
    const auto shared = 128000000u - setup.left;
    loom::ui::mermaid::LayoutRowsCache cache(16u, 1u << 20u);
    std::size_t scenes = 0u;
    for (const auto variant : variants)
        for (unsigned flags = 0u; flags < 4u; ++flags) {
            set_theme(variant, Accessibility{false, (flags & 1u) != 0u, (flags & 2u) != 0u});
            SCOPED_TRACE(static_cast<unsigned>(variant));
            SCOPED_TRACE(flags);
            for (std::size_t i = 0; i < style13::kPins.size(); ++i) {
                const auto& pin = style13::kPins[i];
                SCOPED_TRACE(pin.fixture);
                style13::Work work{128000000u};
                work.spend(shared + pin.source_bytes * 16u + 8192u);
                std::ifstream source_input(std::filesystem::path(LOOM_TESTS_DIR) /
                                               "fixtures/mermaid_official/cases" / pin.fixture,
                                           std::ios::binary);
                ASSERT_TRUE(source_input);
                const std::string source{std::istreambuf_iterator<char>(source_input), {}};
                loom::ui::MarkdownOptions options;
                options.mermaid_rows_cache = &cache;
                auto native = loom::ui::render_markdown(style13_fenced(source, "mermaid"), options);
                auto ordinary = loom::ui::render_markdown(style13_fenced(source, "text"));
                native->ComputeRequirement();
                ordinary->ComputeRequirement();
                const int height = std::min(1040, std::max({32, native->requirement().min_y + 8,
                                                            ordinary->requirement().min_y + 8}));
                std::array<style13::Screen, 5> frames;
                frames[0] = style13::capture(native, 240, height, work);
                frames[1] = style13::capture(ordinary, 240, height, work);
                frames[2] = style13::capture(native, 40, height, work);
                frames[3] = style13::capture(ordinary, 40, height, work);
                frames[4] = style13::capture(native, 240, height, work);
                EXPECT_NE(frames[0].glyphs, frames[1].glyphs);
                EXPECT_EQ(frames[0].glyphs, frames[4].glyphs);
                EXPECT_EQ(frames[0].attributes, frames[4].attributes);
                EXPECT_EQ(frames[2].glyphs, frames[3].glyphs);
                ASSERT_EQ(frames[2].attributes.size(), frames[3].attributes.size());
                for (std::size_t n = 0; n < frames[2].attributes.size(); ++n) {
                    EXPECT_EQ(frames[2].attributes[n].background,
                              frames[3].attributes[n].background);
                    EXPECT_EQ(frames[2].attributes[n].flags, frames[3].attributes[n].flags);
                    EXPECT_EQ(frames[2].attributes[n].hyperlink, frames[3].attributes[n].hyperlink);
                }
                const auto expected =
                    style13::bind(docs, i, pin.ordinal, pin.global, source,
                                  manifest->root().get("fixtures").at(pin.global), work);
                for (const auto selected : {0u, 4u}) {
                    const auto result =
                        style13::check(frames[selected], expected, *current_theme().palette, work,
                                       kStyle13Following);
                    ASSERT_EQ(result.outcome, style13::Outcome::Match)
                        << "reason=" << static_cast<int>(result.reason) << " work=" << work.left;
                }
                EXPECT_GT(work.left, 0u);
                ++scenes;
            }
        }
    EXPECT_EQ(scenes, 364u);
    EXPECT_GE(cache.hits(), 351u);
    EXPECT_LE(cache.byte_size(), cache.byte_capacity());
}
}  // namespace

namespace {
TEST(MermaidClassStyle13Admission, TypedOwnedProofRejectsForgeriesAndRealResourceFailures) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    style13::Work setup{style13::kSceneLimit};
    const auto manifest_path =
        std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
    const auto bytes = std::filesystem::file_size(manifest_path);
    ASSERT_LT(bytes, 4000000u);
    setup.spend(bytes * 24u + 65536u);
    std::ifstream input(manifest_path, std::ios::binary);
    ASSERT_TRUE(input);
    const std::string raw{std::istreambuf_iterator<char>(input), {}};
    auto manifest = loom::utils::json::parse(raw);
    ASSERT_TRUE(manifest);
    const auto& pin = style13::kPins.front();
    const auto fixture = manifest->root().get("fixtures").at(pin.global);
    const auto publish = [&](const std::array<style13::Screen, 5>& frames, style13::Work& work) {
        loom::utils::json::JsonMutDoc actual;
        auto item = actual.object();
        style13::publish_frames(item, actual, frames, work);
        actual.set_root(item);
        const auto captured = actual.to_string();
        std::ofstream output(std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                                 "mermaid-class-style13-admission-actual-five-frames.json",
                             std::ios::binary | std::ios::trunc);
        style13::require(static_cast<bool>(output), "Class13 actual capture publication");
        output.write(captured.data(), static_cast<std::streamsize>(captured.size()));
        style13::require(output.good(), "Class13 complete actual capture publication");
    };
    style13::Work work{style13::kSceneLimit};
    const auto good =
        style13::run(0u, pin.ordinal, pin.global, fixture, work, kStyle13Following, publish);
    ASSERT_TRUE(good.progress.admitted) << good.error;
    const auto encoded = style13::proof(0u, pin.ordinal, pin.global, good);
    auto parsed = loom::utils::json::parse(encoded);
    ASSERT_TRUE(parsed);
    const auto binding = [&](const style13::Run& row) {
        return style13::ProofBinding{pin.fixture,
                                     "original",
                                     "class-diagram",
                                     row.actual_source_sha,
                                     row.decoded_model_sha,
                                     style13::kAuthority,
                                     style13::kContract,
                                     pin.ordinal,
                                     pin.global,
                                     {row.status[0], row.status[1], row.status[2]},
                                     true,
                                     !row.error.empty(),
                                     style13::failure_flags(row.progress)};
    };
    EXPECT_TRUE(style13::count(parsed->root(), binding(good)).admitted);
    const auto reject = [&](auto change) {
        loom::utils::json::JsonMutDoc altered;
        auto value = altered.raw_json(encoded);
        altered.set_root(value);
        change(value);
        auto document = loom::utils::json::parse(altered.to_string());
        ASSERT_TRUE(document);
        EXPECT_THROW(style13::count(document->root(), binding(good)), std::runtime_error);
    };
    reject([&](auto value) {
        value.set("global_ordinal", static_cast<std::int64_t>(pin.global + 1u));
    });
    reject([](auto value) { value.set("expected_Model_sha256", std::string(64u, '0')); });
    reject([](auto value) { value.set("asset_sha256", std::string(64u, '0')); });
    reject([](auto value) { value.set("bound", 1); });
    reject([](auto value) { value.set("negative_actual", 7); });
    reject([](auto value) { value.set("resource", true); });
    reject([](auto value) { value.set("phase", 3); });
    reject([](auto value) { value.set("schema", "ClassNativeWholeScene/v1"); });
    reject([](auto value) { value.set("unknown", false); });
    auto lost = binding(good);
    lost.trusted_owner = false;
    EXPECT_THROW(style13::count(parsed->root(), lost), std::runtime_error);
    lost = binding(good);
    lost.fixture = "class-diagram/unowned.mmd";
    lost.schema = "legacy";
    EXPECT_THROW(style13::count(parsed->root(), lost), std::runtime_error);
    lost = binding(good);
    lost.status[0] = "source_fallback";
    EXPECT_THROW(style13::count(parsed->root(), lost), std::runtime_error);
    const auto missing = encoded.substr(0u, encoded.size() - 1u) + ",\"bound\":true}";
    auto duplicate = loom::utils::json::parse(missing);
    ASSERT_TRUE(duplicate);
    EXPECT_THROW(style13::count(duplicate->root(), binding(good)), std::runtime_error);
    std::array<std::size_t, 5> limits{
        {0u, 8191u,
         static_cast<std::size_t>(good.progress.remaining[0] - good.progress.remaining[2]) - 1u,
         static_cast<std::size_t>(good.progress.remaining[0] - good.progress.remaining[4]),
         static_cast<std::size_t>(good.progress.remaining[0] - good.progress.remaining[7]) - 1u}};
    for (const auto limit : limits) {
        SCOPED_TRACE(limit);
        style13::Work constrained{style13::kSceneLimit};
        const auto failed = style13::run(0u, pin.ordinal, pin.global, fixture, constrained,
                                         kStyle13Following, publish, limit);
        EXPECT_FALSE(failed.progress.admitted);
        EXPECT_TRUE(failed.progress.resource);
        EXPECT_EQ(failed.progress.credited_negatives, 0u);
        for (const auto& state : failed.status)
            EXPECT_EQ(state, "error");
        auto report = loom::utils::json::parse(style13::proof(0u, pin.ordinal, pin.global, failed));
        ASSERT_TRUE(report);
        const auto counted = style13::count(report->root(), binding(failed));
        EXPECT_FALSE(counted.admitted);
        EXPECT_TRUE(counted.resource);
    }
}
}  // namespace

namespace {
namespace defined6 = loom::test::mermaid_class_defined6;
constexpr std::string_view kDefined6Following = "AFTER_CLASS_DEFINED6";

std::string defined6_fenced(std::string_view source, std::string_view language) {
    return "```" + std::string(language) + "\n" + std::string(source) + "\n```\n\n" +
           std::string(kDefined6Following) + "\n";
}

TEST(MermaidClassDefined6Prototype,
     WholeSourcesCaptureBeforeBindingAndRequireScreenGeometryAndAttributes) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    defined6::Work setup{128000000u};
    const auto docs = defined6::load(setup);
    const auto manifest_path =
        std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
    const auto manifest_bytes = std::filesystem::file_size(manifest_path);
    ASSERT_LT(manifest_bytes, 4000000u);
    setup.spend(manifest_bytes * 24u + 65536u);
    std::ifstream input(manifest_path, std::ios::binary);
    ASSERT_TRUE(input);
    const std::string raw{std::istreambuf_iterator<char>(input), {}};
    auto parsed = loom::utils::json::parse(raw);
    ASSERT_TRUE(parsed);
    const auto shared_spent = 128000000u - setup.left;
    std::size_t visited = 0u;
    for (std::size_t i = 0; i < defined6::kPins.size(); ++i) {
        const auto& pin = defined6::kPins[i];
        SCOPED_TRACE(pin.fixture);
        defined6::Work work{128000000u};
        work.spend(shared_spent + pin.source_bytes * 16u + 8192u);
        std::ifstream source_input(std::filesystem::path(LOOM_TESTS_DIR) /
                                       "fixtures/mermaid_official/cases" / pin.fixture,
                                   std::ios::binary);
        ASSERT_TRUE(source_input);
        const std::string source{std::istreambuf_iterator<char>(source_input), {}};
        auto native = loom::ui::render_markdown(defined6_fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(defined6_fenced(source, "text"));
        native->ComputeRequirement();
        ordinary->ComputeRequirement();
        const int height = std::min(1040, std::max({32, native->requirement().min_y + 8,
                                                    ordinary->requirement().min_y + 8}));
        std::array<defined6::Screen, 5> frames;
        frames[0] = defined6::capture(native, 240, height, work);
        frames[1] = defined6::capture(ordinary, 240, height, work);
        frames[2] = defined6::capture(native, 40, height, work);
        frames[3] = defined6::capture(ordinary, 40, height, work);
        frames[4] = defined6::capture(native, 240, height, work);
        // These are actual public frames. Semantic decode deliberately follows
        // all five captures; this prototype itself earns no matrix admission.
        EXPECT_NE(frames[0].glyphs, frames[1].glyphs);
        EXPECT_EQ(frames[0].glyphs, frames[4].glyphs);
        EXPECT_EQ(frames[0].attributes, frames[4].attributes);
        EXPECT_EQ(frames[2].glyphs, frames[3].glyphs);
        // Mermaid and text source blocks may have different syntax-token
        // foregrounds (the existing E2a policy). Preserve the full source and
        // backgrounds/flags/links; no diagram fill or border style may leak.
        ASSERT_EQ(frames[2].attributes.size(), frames[3].attributes.size());
        for (std::size_t n = 0; n < frames[2].attributes.size(); ++n) {
            EXPECT_EQ(frames[2].attributes[n].background, frames[3].attributes[n].background);
            EXPECT_EQ(frames[2].attributes[n].flags, frames[3].attributes[n].flags);
            EXPECT_EQ(frames[2].attributes[n].hyperlink, frames[3].attributes[n].hyperlink);
        }
        const auto expected = defined6::bind(docs, i, pin.ordinal, pin.global, source,
                                             parsed->root().get("fixtures").at(pin.global), work);
        for (const auto frame : {0u, 4u}) {
            SCOPED_TRACE(frame);
            const auto result = defined6::check(frames[frame], expected, *current_theme().palette,
                                                work, kDefined6Following);
            EXPECT_EQ(result.outcome, defined6::Outcome::Match)
                << "reason=" << static_cast<int>(result.reason) << " resource=" << result.resource
                << " remaining=" << work.left;
        }
        const auto baseline = defined6::check(frames[0], expected, *current_theme().palette, work,
                                              kDefined6Following);
        ASSERT_EQ(baseline.outcome, defined6::Outcome::Match);
        ASSERT_TRUE(baseline.box);
        std::size_t negatives = 0u;
        for (std::size_t kind = 0; kind < defined6::kPhysicalNegatives; ++kind) {
            SCOPED_TRACE(kind);
            auto wrong = defined6::copy_screen(frames[0], work);
            defined6::mutate(wrong, *baseline.box, kind, work);
            const auto result = defined6::check(wrong, expected, *current_theme().palette, work,
                                                kDefined6Following);
            EXPECT_EQ(result.outcome, defined6::Outcome::Mismatch)
                << "reason=" << static_cast<int>(result.reason) << " resource=" << result.resource;
            if (result.outcome == defined6::Outcome::Mismatch) ++negatives;
        }
        EXPECT_EQ(negatives, defined6::kPhysicalNegatives);
        EXPECT_GT(work.left, 0u);
        ++visited;
    }
    EXPECT_EQ(visited, 6u);
}
}  // namespace

namespace {
TEST(MermaidClassDefinedStyles, DeferredDefinitionsAssignmentOrderAndDirectLastCascade) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    const std::string definitions =
        "classDef first fill:#f9f,stroke:#333,stroke-width:6px\nclassDef second "
        "fill:#0f0,stroke:#fff,stroke-width:2px\n";
    const std::string assignments = "class A:::first\ncssClass \"A\" second\n";
    for (const bool before : {false, true})
        for (const bool direct : {false, true}) {
            SCOPED_TRACE(before);
            SCOPED_TRACE(direct);
            const auto source = std::string("classDiagram\n") +
                                (direct ? "style A fill:#00f\n" : "") +
                                (before ? definitions + assignments : assignments + definitions);
            auto element = loom::ui::mermaid::render_mermaid(source, ftxui::text("STYLE_FALLBACK"));
            ASSERT_TRUE(element);
            const auto screen = style_screen(*element);
            const auto& palette = *current_theme().palette;
            EXPECT_EQ(screen.PixelAt(1, 1).background_color,
                      direct ? palette.info_tint_bg : palette.success_tint_bg);
            EXPECT_FALSE(screen.PixelAt(0, 0).bold);
            EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, palette.text);
            const auto text = screen.ToString();
            EXPECT_NE(text.find("class A:::first -> class:first"), std::string::npos);
            EXPECT_NE(text.find("definition:first (applied)"), std::string::npos);
            EXPECT_NE(text.find("definition:second (applied)"), std::string::npos);
            const auto a = text.find("class A:::first ->"), d = text.find("classDef first fill:");
            EXPECT_EQ(d < a, before);
        }
}

TEST(MermaidClassDefinedStyles, UnknownReferencesUnsupportedFormsAndLegacyDefinitionsStayClosed) {
    constexpr std::array<std::string_view, 17> closed{
        {"class A:::missing\n", "class A {\nclassDef pink fill:#f9f\n}\nclass B:::pink\n",
         "class A {\nclassDef default fill:#f9f\n}\nclass B:::default\n",
         "class A :::pink\nclassDef pink fill:#f9f\n",
         "class `A`:::pink\nclassDef pink fill:#f9f\n",
         "class A\ncssClass \"B\" pink\nclassDef pink fill:#f9f\n",
         "class A:::pink:::bold\nclassDef pink fill:#f9f\nclassDef bold stroke:#333\n",
         "class A:::pink {\nint x\n}\nclassDef pink fill:#f9f\n",
         "class A\nclass B\ncssClass \"A,B\" pink\nclassDef pink fill:#f9f\n",
         "class A:::pink\nclassDef pink fill:white\n", "class A:::pink\nclassDef pink fill:#f9f0\n",
         "class A:::pink\nclassDef pink fill:#f9f,stroke-width:65px\n",
         "class A:::pink\nclassDef pink fill:#f9f\nclassDef pink stroke:#333\n",
         "class A:::default\nclassDef default fill:#f9f\n",
         "class A\ncssClass A pink\nclassDef pink fill:#f9f\n",
         "class A[\"Alias\"]:::pink\nclassDef pink fill:#f9f\n",
         "namespace Group {\nclass A:::pink\n}\nclassDef pink fill:#f9f\n"}};
    for (const auto body : closed) {
        SCOPED_TRACE(body);
        EXPECT_FALSE(loom::ui::mermaid::render_mermaid(
            std::string("classDiagram\n") + std::string(body), ftxui::text("STYLE_FALLBACK")));
    }
    const auto unused = loom::ui::mermaid::render_mermaid(
        "classDiagram\nclass A\nclassDef dormant fill:#abc,stroke-width:65px\n",
        ftxui::text("STYLE_FALLBACK"));
    ASSERT_TRUE(unused);
    EXPECT_EQ(style_screen(*unused).ToString().find("definition:"), std::string::npos);
}

TEST(MermaidClassDefinedStyles, ExactAndOneShortWorkAsciiAndLegendSpaceUseThePublicPath) {
    const std::string source = "classDiagram\nclass A:::pink\ncssClass \"A\" bold\nclassDef pink "
                               "fill:#f9f\nclassDef bold stroke:#333,stroke-width:6px,color:#fff\n";
    testing::Work measured;
    const auto element =
        testing::render_mermaid_with_work(source, ftxui::text("STYLE_FALLBACK"), false, measured);
    ASSERT_TRUE(element);
    const auto spent = testing::kWorkLimit - measured.remaining;
    ASSERT_GT(spent, 0u);
    testing::Work exact{spent};
    EXPECT_TRUE(
        testing::render_mermaid_with_work(source, ftxui::text("STYLE_FALLBACK"), false, exact));
    EXPECT_EQ(exact.remaining, 0u);
    testing::Work short_work{spent - 1u};
    EXPECT_FALSE(testing::render_mermaid_with_work(source, ftxui::text("STYLE_FALLBACK"), false,
                                                   short_work));
    testing::Work ascii;
    EXPECT_FALSE(
        testing::render_mermaid_with_work(source, ftxui::text("STYLE_FALLBACK"), true, ascii));
    testing::Work rows_work;
    const auto rowset = testing::class_relations_rows(source, rows_work, false);
    ASSERT_TRUE(rowset);
    std::size_t width = 0u;
    for (const auto& row : *rowset)
        width = std::max(width, static_cast<std::size_t>(ftxui::string_width(row)));
    ASSERT_LE(width, 240u);
    ASSERT_GT(width, 40u);
}

TEST(MermaidClassDefined6Oracle,
     IndependentPaperRejectsEightNegativesAndNeverCreditsResourceOrCrop) {
    using namespace loom::ui::design::theme;
    StyleThemeRestore restore;
    set_theme(ThemeVariant::Dark);
    defined6::Expected expected;
    expected.graph = {"class", "TB", {{"Class10", "Class10", "box", "", {{"Class10"}}, {}}},
                      {},      {},   {}};
    expected.raw_records = {"class Class10:::pink", "cssClass \"Class10\" bold",
                            "classDef pink fill:#f9f",
                            "classDef bold stroke:#333,stroke-width:6px,color:#fff"};
    defined6::Work work{128000000u};
    const auto& palette = *current_theme().palette;
    const auto paper = defined6::hand_paper(expected, palette, work, kDefined6Following);
    const auto good = defined6::check(paper, expected, palette, work, kDefined6Following);
    ASSERT_EQ(good.outcome, defined6::Outcome::Match);
    ASSERT_TRUE(good.box);
    for (std::size_t n = 0; n < defined6::kPhysicalNegatives; ++n) {
        SCOPED_TRACE(n);
        auto wrong = defined6::copy_screen(paper, work);
        defined6::mutate(wrong, *good.box, n, work);
        const auto result = defined6::check(wrong, expected, palette, work, kDefined6Following);
        EXPECT_EQ(result.outcome, defined6::Outcome::Mismatch);
        EXPECT_FALSE(result.resource);
    }
    defined6::Work measured{128000000u};
    ASSERT_EQ(defined6::check(paper, expected, palette, measured, kDefined6Following).outcome,
              defined6::Outcome::Match);
    const auto spent = 128000000u - measured.left;
    defined6::Work exact{spent};
    EXPECT_EQ(defined6::check(paper, expected, palette, exact, kDefined6Following).outcome,
              defined6::Outcome::Match);
    EXPECT_EQ(exact.left, 0u);
    for (const auto limit : std::array<std::size_t, 3>{0u, 1u, spent - 1u}) {
        defined6::Work short_work{limit};
        const auto result =
            defined6::check(paper, expected, palette, short_work, kDefined6Following);
        EXPECT_EQ(result.outcome, defined6::Outcome::Inconclusive);
        EXPECT_TRUE(result.resource);
    }
    const auto lines = defined6::legend(expected, work);
    std::size_t maximum = 0u;
    for (const auto& line : lines)
        maximum = std::max(maximum, line.size());
    const auto fit = static_cast<int>(maximum) + 2;
    const auto exact_screen = defined6::crop_width(paper, fit, work);
    EXPECT_EQ(defined6::check(exact_screen, expected, palette, work, kDefined6Following).outcome,
              defined6::Outcome::Match);
    const auto crop = defined6::crop_width(paper, fit - 1, work);
    EXPECT_EQ(defined6::check(crop, expected, palette, work, kDefined6Following).outcome,
              defined6::Outcome::Inconclusive);
    const auto noop = defined6::copy_screen(paper, work);
    EXPECT_EQ(defined6::check(noop, expected, palette, work, kDefined6Following).outcome,
              defined6::Outcome::Match);
    auto unknown = expected;
    unknown.graph.family = "unknown";
    EXPECT_EQ(defined6::check(paper, unknown, palette, work, kDefined6Following).outcome,
              defined6::Outcome::Inconclusive);
    auto oversized = defined6::copy_screen(paper, work);
    work.spend(4096u);
    oversized.glyphs.cells.front() = std::string(1024u, 'x');
    EXPECT_EQ(defined6::check(oversized, expected, palette, work, kDefined6Following).outcome,
              defined6::Outcome::Inconclusive);
}
}  // namespace

namespace {
TEST(MermaidClassDefined6Prototype, SevenPalettesAndBothAccessibilityFlagsKeepWholeCachedScreens) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    constexpr std::array<ThemeVariant, 7> variants{
        {ThemeVariant::Dark, ThemeVariant::Light, ThemeVariant::DarkDaltonized,
         ThemeVariant::LightDaltonized, ThemeVariant::Monochrome, ThemeVariant::LightAnsi,
         ThemeVariant::DarkAnsi}};
    defined6::Work setup{128000000u};
    const auto docs = defined6::load(setup);
    const auto path =
        std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
    const auto bytes = std::filesystem::file_size(path);
    ASSERT_LT(bytes, 4000000u);
    setup.spend(bytes * 24u + 65536u);
    std::ifstream input(path, std::ios::binary);
    ASSERT_TRUE(input);
    const std::string raw{std::istreambuf_iterator<char>(input), {}};
    auto manifest = loom::utils::json::parse(raw);
    ASSERT_TRUE(manifest);
    const auto shared = 128000000u - setup.left;
    loom::ui::mermaid::LayoutRowsCache cache(16u, 1u << 20u);
    std::size_t scenes = 0u;
    for (const auto variant : variants)
        for (unsigned flags = 0u; flags < 4u; ++flags) {
            set_theme(variant, Accessibility{false, (flags & 1u) != 0u, (flags & 2u) != 0u});
            SCOPED_TRACE(static_cast<unsigned>(variant));
            SCOPED_TRACE(flags);
            for (std::size_t i = 0; i < defined6::kPins.size(); ++i) {
                const auto& pin = defined6::kPins[i];
                SCOPED_TRACE(pin.fixture);
                defined6::Work work{128000000u};
                work.spend(shared + pin.source_bytes * 16u + 8192u);
                std::ifstream source_input(std::filesystem::path(LOOM_TESTS_DIR) /
                                               "fixtures/mermaid_official/cases" / pin.fixture,
                                           std::ios::binary);
                ASSERT_TRUE(source_input);
                const std::string source{std::istreambuf_iterator<char>(source_input), {}};
                loom::ui::MarkdownOptions options;
                options.mermaid_rows_cache = &cache;
                auto native =
                    loom::ui::render_markdown(defined6_fenced(source, "mermaid"), options);
                auto ordinary = loom::ui::render_markdown(defined6_fenced(source, "text"));
                native->ComputeRequirement();
                ordinary->ComputeRequirement();
                const int height = std::min(1040, std::max({32, native->requirement().min_y + 8,
                                                            ordinary->requirement().min_y + 8}));
                std::array<defined6::Screen, 5> frames;
                frames[0] = defined6::capture(native, 240, height, work);
                frames[1] = defined6::capture(ordinary, 240, height, work);
                frames[2] = defined6::capture(native, 40, height, work);
                frames[3] = defined6::capture(ordinary, 40, height, work);
                frames[4] = defined6::capture(native, 240, height, work);
                EXPECT_NE(frames[0].glyphs, frames[1].glyphs);
                EXPECT_EQ(frames[0].glyphs, frames[4].glyphs);
                EXPECT_EQ(frames[0].attributes, frames[4].attributes);
                EXPECT_EQ(frames[2].glyphs, frames[3].glyphs);
                ASSERT_EQ(frames[2].attributes.size(), frames[3].attributes.size());
                for (std::size_t n = 0; n < frames[2].attributes.size(); ++n) {
                    EXPECT_EQ(frames[2].attributes[n].background,
                              frames[3].attributes[n].background);
                    EXPECT_EQ(frames[2].attributes[n].flags, frames[3].attributes[n].flags);
                    EXPECT_EQ(frames[2].attributes[n].hyperlink, frames[3].attributes[n].hyperlink);
                }
                const auto expected =
                    defined6::bind(docs, i, pin.ordinal, pin.global, source,
                                   manifest->root().get("fixtures").at(pin.global), work);
                for (const auto selected : {0u, 4u}) {
                    const auto result =
                        defined6::check(frames[selected], expected, *current_theme().palette, work,
                                        kDefined6Following);
                    ASSERT_EQ(result.outcome, defined6::Outcome::Match)
                        << "reason=" << static_cast<int>(result.reason) << " work=" << work.left;
                }
                EXPECT_GT(work.left, 0u);
                ++scenes;
            }
        }
    EXPECT_EQ(scenes, 168u);
    EXPECT_GE(cache.hits(), 162u);
    EXPECT_LE(cache.byte_size(), cache.byte_capacity());
}
}  // namespace

namespace {
TEST(MermaidClassDefinedStyles, MemberTextCannotResolveOrShadowRootStyleDefinitions) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    const std::string root = "classDef pink fill:#f9f\n";
    const std::string body = "class A {\nclassDef pink fill:#0f0\n}\nclass B:::pink\n";
    for (const bool before : {false, true}) {
        SCOPED_TRACE(before);
        const auto source = std::string("classDiagram\n") + (before ? root + body : body + root);
        const auto element =
            loom::ui::mermaid::render_mermaid(source, ftxui::text("STYLE_FALLBACK"));
        ASSERT_TRUE(element);
        const auto screen = style_screen(*element);
        std::size_t styled_labels = 0u;
        for (int y = 0; y < screen.dimy(); ++y)
            for (int x = 0; x < screen.dimx(); ++x) {
                const auto& pixel = screen.PixelAt(x, y);
                if (pixel.character == "B" && pixel.background_color != ftxui::Color::Default) {
                    ++styled_labels;
                    EXPECT_EQ(pixel.background_color, current_theme().palette->surface_tint_4);
                }
            }
        EXPECT_EQ(styled_labels, 1u);
        const auto text = screen.ToString();
        EXPECT_NE(text.find("classDef pink fill:#0f0"), std::string::npos);
        EXPECT_EQ(text.find("classDef pink fill:#0f0 ->"), std::string::npos);
        EXPECT_NE(text.find("classDef pink fill:#f9f -> definition:pink (applied)"),
                  std::string::npos);
    }
}
}  // namespace

namespace {
TEST(MermaidClassDefined6Admission, TypedOwnedProofRejectsForgeriesAndRealResourceFailures) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    defined6::Work setup{defined6::kSceneLimit};
    const auto manifest_path =
        std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
    const auto bytes = std::filesystem::file_size(manifest_path);
    ASSERT_LT(bytes, 4000000u);
    setup.spend(bytes * 24u + 65536u);
    std::ifstream input(manifest_path, std::ios::binary);
    ASSERT_TRUE(input);
    const std::string raw{std::istreambuf_iterator<char>(input), {}};
    auto manifest = loom::utils::json::parse(raw);
    ASSERT_TRUE(manifest);
    const auto& pin = defined6::kPins.front();
    const auto fixture = manifest->root().get("fixtures").at(pin.global);
    const auto publish = [&](const std::array<defined6::Screen, 5>& frames, defined6::Work& work) {
        loom::utils::json::JsonMutDoc actual;
        auto item = actual.object();
        defined6::publish_frames(item, actual, frames, work);
        actual.set_root(item);
        const auto captured = actual.to_string();
        std::ofstream output(std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                                 "mermaid-class-defined6-admission-actual-five-frames.json",
                             std::ios::binary | std::ios::trunc);
        defined6::require(static_cast<bool>(output), "ClassDefined6 actual capture publication");
        output.write(captured.data(), static_cast<std::streamsize>(captured.size()));
        defined6::require(output.good(), "ClassDefined6 complete actual capture publication");
    };
    defined6::Work work{defined6::kSceneLimit};
    const auto good =
        defined6::run(0u, pin.ordinal, pin.global, fixture, work, kDefined6Following, publish);
    ASSERT_TRUE(good.progress.admitted) << good.error;
    const auto encoded = defined6::proof(0u, pin.ordinal, pin.global, good);
    auto parsed = loom::utils::json::parse(encoded);
    ASSERT_TRUE(parsed);
    const auto binding = [&](const defined6::Run& row) {
        return defined6::ProofBinding{pin.fixture,
                                      "original",
                                      "class-diagram",
                                      row.actual_source_sha,
                                      row.decoded_model_sha,
                                      defined6::kAuthority,
                                      defined6::kContract,
                                      pin.ordinal,
                                      pin.global,
                                      {row.status[0], row.status[1], row.status[2]},
                                      true,
                                      !row.error.empty(),
                                      defined6::failure_flags(row.progress)};
    };
    EXPECT_TRUE(defined6::count(parsed->root(), binding(good)).admitted);
    const auto reject = [&](auto change) {
        loom::utils::json::JsonMutDoc altered;
        auto value = altered.raw_json(encoded);
        altered.set_root(value);
        change(value);
        auto document = loom::utils::json::parse(altered.to_string());
        ASSERT_TRUE(document);
        EXPECT_THROW(defined6::count(document->root(), binding(good)), std::runtime_error);
    };
    reject([&](auto value) {
        value.set("global_ordinal", static_cast<std::int64_t>(pin.global + 1u));
    });
    reject([](auto value) { value.set("expected_Model_sha256", std::string(64u, '0')); });
    reject([](auto value) { value.set("asset_sha256", std::string(64u, '0')); });
    reject([](auto value) { value.set("bound", 1); });
    reject([](auto value) { value.set("negative_actual", 7); });
    reject([](auto value) { value.set("resource", true); });
    reject([](auto value) { value.set("phase", 3); });
    reject([](auto value) { value.set("schema", "ClassNativeWholeScene/v1"); });
    reject([](auto value) { value.set("unknown", false); });
    auto lost = binding(good);
    lost.trusted_owner = false;
    EXPECT_THROW(defined6::count(parsed->root(), lost), std::runtime_error);
    lost = binding(good);
    lost.fixture = "class-diagram/unowned.mmd";
    lost.schema = "legacy";
    EXPECT_THROW(defined6::count(parsed->root(), lost), std::runtime_error);
    lost = binding(good);
    lost.status[0] = "source_fallback";
    EXPECT_THROW(defined6::count(parsed->root(), lost), std::runtime_error);
    const auto missing = encoded.substr(0u, encoded.size() - 1u) + ",\"bound\":true}";
    auto duplicate = loom::utils::json::parse(missing);
    ASSERT_TRUE(duplicate);
    EXPECT_THROW(defined6::count(duplicate->root(), binding(good)), std::runtime_error);
    std::array<std::size_t, 5> limits{
        {0u, 8191u,
         static_cast<std::size_t>(good.progress.remaining[0] - good.progress.remaining[2]) - 1u,
         static_cast<std::size_t>(good.progress.remaining[0] - good.progress.remaining[4]),
         static_cast<std::size_t>(good.progress.remaining[0] - good.progress.remaining[7]) - 1u}};
    for (const auto limit : limits) {
        SCOPED_TRACE(limit);
        defined6::Work constrained{defined6::kSceneLimit};
        const auto failed = defined6::run(0u, pin.ordinal, pin.global, fixture, constrained,
                                          kDefined6Following, publish, limit);
        EXPECT_FALSE(failed.progress.admitted);
        EXPECT_TRUE(failed.progress.resource);
        EXPECT_EQ(failed.progress.credited_negatives, 0u);
        for (const auto& state : failed.status)
            EXPECT_EQ(state, "error");
        auto report =
            loom::utils::json::parse(defined6::proof(0u, pin.ordinal, pin.global, failed));
        ASSERT_TRUE(report);
        const auto counted = defined6::count(report->root(), binding(failed));
        EXPECT_FALSE(counted.admitted);
        EXPECT_TRUE(counted.resource);
    }
}
}  // namespace

namespace {
TEST(MermaidClassDefined6Oracle, CompleteConfigurationDecodeAndFullScalarAttributesRemainSemantic) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    defined6::Work work{128000000u};
    const auto docs = defined6::load(work);
    for (std::size_t n = 0; n < defined6::kPins.size(); ++n) {
        SCOPED_TRACE(n);
        const auto config =
            docs.document.root().get("cases").at(n).get("Model").get("frontmatter_config");
        EXPECT_NO_THROW(defined6::validate_config(config, n, work));
        const auto reject = [&](auto change) {
            loom::utils::json::JsonMutDoc owner;
            auto value = owner.copy_val(config);
            change(value);
            owner.set_root(value);
            const auto parsed = loom::utils::json::parse(owner.to_string());
            ASSERT_TRUE(parsed);
            EXPECT_THROW(defined6::validate_config(parsed->root(), n, work), std::runtime_error);
        };
        reject([](auto value) { value.set("unknown", false); });
        reject([](auto value) { value.set("htmlLabels", 1); });
        reject([&](auto value) { value.set("htmlLabels", n % 2u == 0u); });
        reject([](auto value) { value.set("logLevel", true); });
        reject([](auto value) { value.set("logLevel", 2); });
        if (n < 2u) {
            reject([](auto value) { value.set("layout", 1); });
            reject([](auto value) { value.set("layout", "elk"); });
        }
        if (n >= 2u && n < 4u) {
            reject([](auto value) { value.set("look", false); });
            reject([](auto value) { value.set("look", "classic"); });
        }
    }
    defined6::Expected expected;
    expected.graph = {"class", "TB", {{"Class10", "Class10", "box", "", {{"Class10"}}, {}}},
                      {},      {},   {}};
    expected.raw_records = {"class Class10:::pink", "cssClass \"Class10\" bold",
                            "classDef pink fill:#f9f",
                            "classDef bold stroke:#333,stroke-width:6px,color:#fff"};
    const auto& palette = *current_theme().palette;
    const auto paper = defined6::hand_paper(expected, palette, work, kDefined6Following);
    for (const auto scalar : {256u, std::numeric_limits<unsigned>::max()}) {
        SCOPED_TRACE(scalar);
        auto link = defined6::copy_screen(paper, work);
        link.attributes.back().hyperlink = scalar;
        const auto result = defined6::check(link, expected, palette, work, kDefined6Following);
        EXPECT_EQ(result.outcome, defined6::Outcome::Mismatch);
        EXPECT_FALSE(result.resource);
        auto flags = defined6::copy_screen(paper, work);
        flags.attributes.back().flags = scalar;
        const auto wrong = defined6::check(flags, expected, palette, work, kDefined6Following);
        EXPECT_EQ(wrong.outcome, defined6::Outcome::Mismatch);
        EXPECT_FALSE(wrong.resource);
    }
    style13::Expected direct;
    direct.graph = expected.graph;
    direct.raw_declaration = "style Class10 fill:#f9f,stroke:#333,stroke-width:4px";
    const auto old = style13::hand_paper(direct, palette, work, kStyle13Following);
    auto high_link = style13::copy_screen(old, work);
    high_link.attributes.back().hyperlink = 65536u;
    const auto result = style13::check(high_link, direct, palette, work, kStyle13Following);
    EXPECT_EQ(result.outcome, style13::Outcome::Mismatch);
    EXPECT_FALSE(result.resource);
}
}  // namespace

namespace {
TEST(MermaidClassDefinedStyles, ExactCardinalityAndLegendGuardsStayIndependentOfWorkExhaustion) {
    // A deliberately injected test allowance reaches the structural branches.
    // Default production Work remains1M; these are not corpus coverage gains.
    for (const std::size_t count : {64u, 65u}) {
        std::string definitions = "classDiagram\nclass A\n";
        for (std::size_t n = 0; n < count; ++n)
            definitions += "classDef c" + std::to_string(n) + " fill:#abc\n";
        definitions += "cssClass \"A\" c0\n";
        testing::Work work{10000000u};
        const auto result = testing::render_mermaid_with_work(
            definitions, ftxui::text("STYLE_FALLBACK"), false, work);
        EXPECT_EQ(result.has_value(), count == 64u);
        EXPECT_GT(work.remaining, 0u);
        std::string assignments = "classDiagram\nclass A\nclassDef pink fill:#f9f\n";
        for (std::size_t n = 0; n < count; ++n)
            assignments += "cssClass \"A\" pink\n";
        testing::Work paid{10000000u};
        const auto repeated = testing::render_mermaid_with_work(
            assignments, ftxui::text("STYLE_FALLBACK"), false, paid);
        EXPECT_EQ(repeated.has_value(), count == 64u);
        EXPECT_GT(paid.remaining, 0u);
        if (repeated)
            EXPECT_NE(style_screen(*repeated).ToString().find("occurrences:64"), std::string::npos);
    }
    for (const std::size_t direct : {5u, 6u}) {
        std::string source = "classDiagram\nclass A:::pink\nclassDef pink fill:#f9f\n";
        for (std::size_t n = 1; n <= direct; ++n)
            source += "style A stroke-width:" + std::to_string(n) + "px\n";
        testing::Work work;
        const auto result =
            testing::render_mermaid_with_work(source, ftxui::text("STYLE_FALLBACK"), false, work);
        EXPECT_EQ(result.has_value(), direct == 5u);
        EXPECT_GT(work.remaining, 0u);
    }
}
}  // namespace

namespace {
// Frozen before Flow style edits in both presets: mechanical glyph/debit
// regression pins, not a source Model or independent semantic oracle.
TEST(MermaidFlowStyleBaseline, PlainPublicGlyphAndWorkRemainUnchanged) {
    struct Case {
        std::string_view name, source;
        std::size_t spent;
        std::string_view sha;
    };

    const std::array<Case, 4> cases{
        {{"chain_lr", "flowchart LR\nLorem --> Ipsum --> Dolor\n", 329177u,
          "2553afc1eb724e9747196a17e853cf7aaa48330f6f65206da761485b3cb648eb"},
         {"chain_td", "flowchart TD\nA[Start] --> B[Process]\nB --> C[End]\n", 344485u,
          "03a1bdfdd526242e84c3f0fee78305c01accdba070958dc833afb09fcb302c3e"},
         {"rounded", "flowchart LR\nA(First) --> B[Second]\n", 323796u,
          "946d04076eb42cd0a4752daf3b07401110a05c16d788689f0da5a9923731a074"},
         {"grouped", "flowchart TD\nsubgraph G[Group]\nA[One] --> B[Two]\nend\nB --> C[Three]\n",
          374376u, "0c41890ce8c2566b33a404d7fee262281d089a6c954c5f056264ff9a8078bd1f"}}};
    for (const auto& item : cases) {
        const auto source = item.source;
        SCOPED_TRACE(item.name);
        testing::Work work;
        const auto element = testing::render_mermaid_with_work(
            source, ftxui::text("FLOW_BASELINE_FALLBACK"), false, work);
        ASSERT_TRUE(element);
        auto screen =
            ftxui::Screen::Create(ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(64));
        ftxui::Render(screen, *element);
        std::string glyphs;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 240; ++x)
                glyphs += screen.PixelAt(x, y).character;
        EXPECT_EQ(testing::kWorkLimit - work.remaining, item.spent);
        EXPECT_EQ(loom::utils::crypto::sha256(glyphs), item.sha);
    }
}
}  // namespace

namespace {
TEST(MermaidFinalEmissionTrace, SamePublicLedgerReachesLastDebitAndPreservesCompleteCallerSource) {
    const std::string source =
        "classDiagram\nclass Class10:::pink\ncssClass \"Class10\" bold\nclassDef pink "
        "fill:#f9f\nclassDef bold stroke:#333,stroke-width:6px,color:#fff\n";
    const auto ordinary = loom::ui::render_markdown(defined6_fenced(source, "text"));
    testing::Work measured;
    testing::FinalEmissionTrace full;
    ASSERT_TRUE(testing::render_mermaid_with_work(source, ordinary, false, measured, &full));
    ASSERT_TRUE(full.styled_entered);
    ASSERT_TRUE(full.preflight_complete);
    ASSERT_TRUE(full.allocation_started);
    ASSERT_TRUE(full.returned_native);
    ASSERT_GT(full.emission_debit, 0u);
    ASSERT_GT(full.last_debit, 0u);
    EXPECT_EQ(full.preflight_debits, full.successful_debits);
    EXPECT_EQ(full.failed_debit, 0u);
    EXPECT_EQ(full.entry_remaining - measured.remaining, full.emission_debit);
    const auto spent = testing::kWorkLimit - measured.remaining;
    testing::Work exact{spent};
    testing::FinalEmissionTrace exact_trace;
    ASSERT_TRUE(testing::render_mermaid_with_work(source, ordinary, false, exact, &exact_trace));
    EXPECT_EQ(exact.remaining, 0u);
    EXPECT_TRUE(exact_trace.preflight_complete);
    EXPECT_TRUE(exact_trace.returned_native);
    EXPECT_EQ(exact_trace.entry_remaining, full.emission_debit);
    testing::Work short_work{spent - 1u};
    testing::FinalEmissionTrace short_trace;
    const auto cut =
        testing::render_mermaid_with_work(source, ordinary, false, short_work, &short_trace);
    EXPECT_FALSE(cut);
    EXPECT_EQ(short_work.remaining, 0u);
    ASSERT_TRUE(short_trace.styled_entered);
    EXPECT_FALSE(short_trace.preflight_complete);
    EXPECT_FALSE(short_trace.allocation_started);
    EXPECT_FALSE(short_trace.returned_native);
    EXPECT_EQ(short_trace.preflight_debits, full.preflight_debits);
    EXPECT_EQ(short_trace.successful_debits + 1u, full.successful_debits);
    EXPECT_EQ(short_trace.failed_debit, full.last_debit);
    EXPECT_EQ(short_trace.last_debit, full.last_debit);
    EXPECT_EQ(short_trace.entry_remaining + 1u, exact_trace.entry_remaining);
    defined6::Work oracle{128000000u};
    const auto fallback = defined6::capture(cut ? *cut : ordinary, 240, 40, oracle);
    const auto expected = defined6::capture(ordinary, 240, 40, oracle);
    EXPECT_EQ(fallback.glyphs, expected.glyphs);
    EXPECT_EQ(fallback.attributes, expected.attributes);
    EXPECT_TRUE(defined6::exact_footer(fallback, kDefined6Following, oracle));
    const auto clear = [&](const testing::FinalEmissionTrace& value) {
        EXPECT_FALSE(value.styled_entered);
        EXPECT_FALSE(value.preflight_complete);
        EXPECT_FALSE(value.allocation_started);
        EXPECT_FALSE(value.returned_native);
        EXPECT_EQ(value.entry_remaining, 0u);
        EXPECT_EQ(value.emission_debit, 0u);
        EXPECT_EQ(value.preflight_debits, 0u);
        EXPECT_EQ(value.successful_debits, 0u);
        EXPECT_EQ(value.last_debit, 0u);
        EXPECT_EQ(value.failed_debit, 0u);
    };
    testing::Work empty_fallback;
    EXPECT_FALSE(
        testing::render_mermaid_with_work(source, {}, false, empty_fallback, &exact_trace));
    clear(exact_trace);
    EXPECT_EQ(empty_fallback.remaining, testing::kWorkLimit);
    testing::Work plain_work;
    EXPECT_TRUE(testing::render_mermaid_with_work("flowchart LR\nA-->B\n", ordinary, false,
                                                  plain_work, &full));
    clear(full);
}
}  // namespace

namespace {
struct FlowStylePreview {
    bool previous = testing::set_flow_style_preview_for_testing(true);

    ~FlowStylePreview() { testing::set_flow_style_preview_for_testing(previous); }
};

TEST(MermaidFlowStyle4Prototype,
     CompleteOriginalAndBodyInputsReachRealStyledEmissionWithoutCoverageCredit) {
    FlowStylePreview preview;
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    const std::array<std::string_view, 4> paths{
        {"flowchart/nightly-check/dagre/65-dagre-text-color-from-classes.mmd",
         "flowchart/nightly-check/handdrawn/text-color-from-classes.mmd",
         "flowchart/pr-check/4-classdef-style.mmd",
         "flowchart/visual-check/65-1-text-color-from-classes.mmd"}};
    for (const auto path : paths) {
        SCOPED_TRACE(path);
        std::ifstream input(std::filesystem::path(LOOM_TESTS_DIR) /
                                "fixtures/mermaid_official/cases" / path,
                            std::ios::binary);
        ASSERT_TRUE(input);
        const std::string original{std::istreambuf_iterator<char>(input), {}};
        const auto extent = original.find("\n---\n", 4u);
        ASSERT_NE(extent, std::string::npos);
        for (const bool body : {false, true}) {
            SCOPED_TRACE(body);
            const auto source = body ? original.substr(extent + 5u) : original;
            const auto fallback = loom::ui::render_markdown(defined6_fenced(source, "text"));
            testing::Work measured;
            testing::FinalEmissionTrace trace;
            const auto native =
                testing::render_mermaid_with_work(source, fallback, false, measured, &trace);
            if (!native) {
                testing::Work diagnostic{2000000u};
                testing::FinalEmissionTrace observed;
                const auto larger = testing::render_mermaid_with_work(source, fallback, false,
                                                                      diagnostic, &observed);
                std::cout << "STYLE_BUDGET_DIAGNOSTIC " << path << ' ' << body << ' '
                          << larger.has_value() << ' ' << 2000000u - diagnostic.remaining << ' '
                          << observed.styled_entered << ' ' << observed.entry_remaining << ' '
                          << observed.emission_debit << '\n';
            }
            ASSERT_TRUE(native) << "remaining=" << measured.remaining;
            ASSERT_TRUE(trace.styled_entered);
            ASSERT_TRUE(trace.preflight_complete);
            ASSERT_TRUE(trace.returned_native);
            auto raw_screen =
                ftxui::Screen::Create(ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(96));
            ftxui::Render(raw_screen, *native);
            const auto actual = raw_screen.ToString();
            EXPECT_NE(actual.find("assignment order; direct last"), std::string::npos);
            EXPECT_EQ(actual.find("flowchart"), std::string::npos);
            const auto spent = testing::kWorkLimit - measured.remaining;
            testing::Work exact{spent};
            testing::FinalEmissionTrace complete;
            EXPECT_TRUE(
                testing::render_mermaid_with_work(source, fallback, false, exact, &complete));
            EXPECT_EQ(exact.remaining, 0u);
            EXPECT_TRUE(complete.preflight_complete);
            EXPECT_TRUE(complete.returned_native);
            testing::Work short_work{spent - 1u};
            testing::FinalEmissionTrace cut;
            const auto failed =
                testing::render_mermaid_with_work(source, fallback, false, short_work, &cut);
            EXPECT_FALSE(failed);
            EXPECT_TRUE(cut.styled_entered);
            EXPECT_FALSE(cut.preflight_complete);
            EXPECT_FALSE(cut.allocation_started);
            EXPECT_EQ(cut.failed_debit, trace.last_debit);
            EXPECT_EQ(cut.preflight_debits, trace.preflight_debits);
            EXPECT_EQ(short_work.remaining, 0u);
            defined6::Work oracle{128000000u};
            const auto preserved = defined6::capture(failed ? *failed : fallback, 240, 40, oracle);
            const auto ordinary = defined6::capture(fallback, 240, 40, oracle);
            EXPECT_EQ(preserved.glyphs, ordinary.glyphs);
            EXPECT_EQ(preserved.attributes, ordinary.attributes);
        }
    }
}
}  // namespace

namespace {
TEST(MermaidFlowStyle4Diagnostic, LookRemovalIdentifiesCandidatesWithoutOriginalCredit) {
    FlowStylePreview preview;
    const auto path =
        std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
    std::ifstream input(path, std::ios::binary);
    ASSERT_TRUE(input);
    const std::string raw{std::istreambuf_iterator<char>(input), {}};
    auto manifest = loom::utils::json::parse(raw);
    ASSERT_TRUE(manifest);
    std::size_t changed = 0u, candidates = 0u;
    manifest->root().get("fixtures").iter([&](auto fixture) {
        const auto name = fixture.get_string("path");
        if (!name.starts_with("flowchart/")) return;
        std::ifstream file(std::filesystem::path(LOOM_TESTS_DIR) /
                               "fixtures/mermaid_official/cases" / std::string(name),
                           std::ios::binary);
        ASSERT_TRUE(file);
        const std::string source{std::istreambuf_iterator<char>(file), {}};
        const auto where = source.find("  look: handDrawn\n");
        if (where == std::string::npos) return;
        ++changed;
        auto altered = source;
        altered.erase(where, std::string_view("  look: handDrawn\n").size());
        testing::Work work;
        const auto native =
            testing::render_mermaid_with_work(altered, ftxui::text("DIAGNOSTIC_ONLY"), false, work);
        if (native) {
            ++candidates;
            std::cout << "LOOK_REMOVAL_DIAGNOSTIC_NOT_ORIGINAL " << name << '\n';
        }
    });
    std::cout << "LOOK_REMOVAL_DIAGNOSTIC_COUNTS " << changed << ' ' << candidates << '\n';
}
}  // namespace

namespace {
TEST(MermaidFlowStyle4Metadata, ExactStrictHandDrawnProfilePreservesOtherClosedConfigurations) {
    FlowStylePreview preview;
    const std::string prefix =
        "---\ndescription: >-\n  Strict handDrawn metadata profile\nconfig:\n";
    const std::string strict = "  look: handDrawn\n  htmlLabels: true\n  flowchart:\n    "
                               "htmlLabels: true\n  securityLevel: loose\n";
    const std::string suffix = "---\nflowchart LR\nA-->B\n";
    EXPECT_TRUE(loom::ui::mermaid::render_mermaid(prefix + strict + suffix,
                                                  ftxui::text("CLOSED_METADATA")));
    for (const auto change : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u}) {
        SCOPED_TRACE(change);
        auto config = strict;
        if (change == 0u) config += "  look: handDrawn\n";
        if (change == 1u) config.replace(config.find("handDrawn"), 9u, "classic");
        if (change == 2u) config.replace(config.find("handDrawn"), 9u, "true");
        if (change == 3u) config += "  unknown: true\n";
        if (change == 4u) config.replace(config.find("htmlLabels: true"), 16u, "htmlLabels: false");
        if (change == 5u) config += "  layout: dagre\n";
        if (change == 6u) config += "  fontFamily: courier\n";
        if (change == 7u) config.replace(config.find("handDrawn"), 9u, "\"handDrawn\"");
        EXPECT_FALSE(loom::ui::mermaid::render_mermaid(prefix + config + suffix,
                                                       ftxui::text("CLOSED_METADATA")));
    }
    EXPECT_FALSE(loom::ui::mermaid::render_mermaid(
        prefix + "  look: handDrawn\n  fontFamily: courier\n" + suffix,
        ftxui::text("CLOSED_METADATA")));
    EXPECT_FALSE(loom::ui::mermaid::render_mermaid(
        prefix + "  look: handDrawn\n  flowchart:\n    htmlLabels: false\n  fontFamily: courier\n" +
            suffix,
        ftxui::text("CLOSED_METADATA")));
}
}  // namespace

namespace {
namespace flow_style4 = loom::test::mermaid_flow::style4;
constexpr std::string_view kFlowStyle4Following = "AFTER_FLOW_STYLE4";

std::string flow_style4_fenced(std::string_view source, std::string_view language) {
    return "```" + std::string(language) + "\n" + std::string(source) + "\n```\n\n" +
           std::string(kFlowStyle4Following) + "\n";
}

TEST(MermaidFlowStyle4OraclePrototype,
     EveryOriginalAndSeparateBodyRequiresWholeGraphLegendAndAllAttributes) {
    FlowStylePreview preview;
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    flow_style4::Work setup{128000000u};
    const auto docs = flow_style4::load(setup);
    const auto manifest_path =
        std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
    const auto bytes = std::filesystem::file_size(manifest_path);
    ASSERT_LT(bytes, 4000000u);
    setup.spend(bytes * 24u + 65536u);
    std::ifstream input(manifest_path, std::ios::binary);
    ASSERT_TRUE(input);
    const std::string raw{std::istreambuf_iterator<char>(input), {}};
    auto manifest = loom::utils::json::parse(raw);
    ASSERT_TRUE(manifest);
    const auto shared = 128000000u - setup.remaining;
    std::size_t originals = 0u, bodies = 0u;
    for (std::size_t n = 0; n < flow_style4::kPins.size(); ++n) {
        const auto& pin = flow_style4::kPins[n];
        SCOPED_TRACE(pin.fixture);
        for (const auto domain :
             {flow_style4::Domain::Original, flow_style4::Domain::BodyDiagnostic}) {
            SCOPED_TRACE(static_cast<int>(domain));
            flow_style4::Work work{128000000u};
            work.spend(shared + pin.source_bytes * 16u + 8192u);
            const auto file_path = std::filesystem::path(LOOM_TESTS_DIR) /
                                   "fixtures/mermaid_official/cases" / pin.fixture;
            ASSERT_EQ(std::filesystem::file_size(file_path), pin.source_bytes);
            std::ifstream file(file_path, std::ios::binary);
            ASSERT_TRUE(file);
            std::string original(pin.source_bytes, '\0');
            file.read(original.data(), static_cast<std::streamsize>(original.size()));
            ASSERT_EQ(file.gcount(), static_cast<std::streamsize>(original.size()));
            ASSERT_EQ(file.peek(), std::char_traits<char>::eof());
            const auto source = domain == flow_style4::Domain::Original
                                    ? std::string_view(original)
                                    : std::string_view(original).substr(pin.front_bytes);
            const auto native = loom::ui::render_markdown(flow_style4_fenced(source, "mermaid")),
                       ordinary = loom::ui::render_markdown(flow_style4_fenced(source, "text"));
            native->ComputeRequirement();
            ordinary->ComputeRequirement();
            const int height = std::min(1040, std::max({64, native->requirement().min_y + 8,
                                                        ordinary->requirement().min_y + 8}));
            std::array<flow_style4::Screen, 5> frames;
            frames[0] = flow_style4::capture(native, 240, height, work);
            frames[1] = flow_style4::capture(ordinary, 240, height, work);
            frames[2] = flow_style4::capture(native, 40, height, work);
            frames[3] = flow_style4::capture(ordinary, 40, height, work);
            frames[4] = flow_style4::capture(native, 240, height, work);
            EXPECT_NE(frames[0].glyphs, frames[1].glyphs);
            EXPECT_EQ(frames[0].glyphs, frames[4].glyphs);
            EXPECT_EQ(frames[0].attributes, frames[4].attributes);
            EXPECT_EQ(frames[2].glyphs, frames[3].glyphs);
            for (std::size_t at = 0; at < frames[2].attributes.size(); ++at) {
                EXPECT_EQ(frames[2].attributes[at].background, frames[3].attributes[at].background);
                EXPECT_EQ(frames[2].attributes[at].flags, frames[3].attributes[at].flags);
                EXPECT_EQ(frames[2].attributes[at].hyperlink, frames[3].attributes[at].hyperlink);
            }
            const auto expected =
                flow_style4::bind(docs, n, domain, source, original,
                                  manifest->root().get("fixtures").at(pin.global), work);
            const auto good = flow_style4::check(frames[0], expected, *current_theme().palette,
                                                 work, kFlowStyle4Following);
            ASSERT_EQ(good.outcome, flow_style4::Outcome::Match)
                << "reason=" << static_cast<int>(good.reason) << " resource=" << good.resource
                << " remaining=" << work.remaining;
            EXPECT_EQ(flow_style4::check(frames[4], expected, *current_theme().palette, work,
                                         kFlowStyle4Following)
                          .outcome,
                      flow_style4::Outcome::Match);
            std::size_t negatives = 0u;
            for (std::size_t kind = 0; kind < flow_style4::kPhysicalNegatives; ++kind) {
                SCOPED_TRACE(kind);
                auto wrong = flow_style4::copy_screen(frames[0], work);
                flow_style4::mutate(wrong, good, kind, expected, work);
                const auto rejected = flow_style4::check(wrong, expected, *current_theme().palette,
                                                         work, kFlowStyle4Following);
                EXPECT_EQ(rejected.outcome, flow_style4::Outcome::Mismatch)
                    << static_cast<int>(rejected.reason);
                EXPECT_FALSE(rejected.resource);
                if (rejected.outcome == flow_style4::Outcome::Mismatch && !rejected.resource)
                    ++negatives;
            }
            EXPECT_EQ(negatives, 8u);
            EXPECT_GT(work.remaining, 0u);
            if (domain == flow_style4::Domain::Original)
                ++originals;
            else
                ++bodies;
        }
    }
    EXPECT_EQ(originals, 4u);
    EXPECT_EQ(bodies, 4u);
}
}  // namespace

namespace {
TEST(MermaidFlowStyles, ExactRgbHueBoundaryBelongsToOrangeAndKeepsPaletteProjection) {
    FlowStylePreview preview;
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    const auto node = loom::ui::mermaid::render_mermaid(
        "classDiagram\nclass A\nstyle A fill:#f96\n", ftxui::text("HUE_FALLBACK"));
    ASSERT_TRUE(node);
    const auto screen = style_screen(*node);
    EXPECT_EQ(screen.PixelAt(1, 1).background_color, current_theme().palette->warning_tint_bg);
    EXPECT_NE(screen.ToString().find("fill:Orange(shared warning)"), std::string::npos);
}
}  // namespace

namespace {
TEST(MermaidFlowStyle4Oracle,
     IndependentChainPapersRequireRoutesAttributesAndNeverCreditResourceOrDomainSwaps) {
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    flow_style4::Work setup{128000000u};
    const auto docs = flow_style4::load(setup);
    for (const auto selected : {0u, 2u}) {
        SCOPED_TRACE(selected);
        flow_style4::Work work{128000000u};
        const auto model = docs.document.root().get("cases").at(selected).get("Model");
        auto expected =
            flow_style4::decode_model(model, selected, flow_style4::Domain::Original, work);
        const auto definition = model.get("style_definitions").at(0u),
                   assignment = model.get("style_assignments").at(0u);
        expected.raw_records.emplace_back(definition.get_string("verbatim"));
        expected.raw_records.emplace_back(assignment.get_string("verbatim"));
        if (selected == 2u)
            expected.raw_records.emplace_back(
                model.get("direct_styles").at(0u).get_string("verbatim"));
        const auto& palette = *current_theme().palette;
        const auto paper = flow_style4::hand_paper(expected, palette, work, kFlowStyle4Following);
        const auto good = flow_style4::check(paper, expected, palette, work, kFlowStyle4Following);
        if (good.outcome != flow_style4::Outcome::Match) {
            auto diagnostic = paper.glyphs;
            const auto legends = flow_style4::legend(expected, work);
            const int bottom = selected == 2u ? 16 : 4;
            for (std::size_t row = 0; row < legends.size(); ++row)
                for (int x = 0; x < 240; ++x)
                    diagnostic.put({x, bottom + 1 + static_cast<int>(row)}, " ");
            const int footer = bottom + static_cast<int>(legends.size()) + 4;
            const auto errors = loom::test::mermaid_flow::validate_impl(diagnostic, expected.graph,
                                                                        work, nullptr, &footer);
            std::cout << "PAPER_DIAGNOSTIC " << selected << ' '
                      << loom::test::mermaid_flow::errors_text(errors) << '\n';
        }
        ASSERT_EQ(good.outcome, flow_style4::Outcome::Match) << static_cast<int>(good.reason);
        for (std::size_t n = 0; n < 8u; ++n) {
            SCOPED_TRACE(n);
            auto wrong = flow_style4::copy_screen(paper, work);
            flow_style4::mutate(wrong, good, n, expected, work);
            const auto result =
                flow_style4::check(wrong, expected, palette, work, kFlowStyle4Following);
            EXPECT_EQ(result.outcome, flow_style4::Outcome::Mismatch);
            EXPECT_FALSE(result.resource);
        }
        flow_style4::Work measured{128000000u};
        ASSERT_EQ(
            flow_style4::check(paper, expected, palette, measured, kFlowStyle4Following).outcome,
            flow_style4::Outcome::Match);
        const auto spent = 128000000u - measured.remaining;
        flow_style4::Work exact{spent};
        EXPECT_EQ(flow_style4::check(paper, expected, palette, exact, kFlowStyle4Following).outcome,
                  flow_style4::Outcome::Match);
        EXPECT_EQ(exact.remaining, 0u);
        for (const auto limit : std::array<std::size_t, 3>{0u, 1u, spent - 1u}) {
            flow_style4::Work short_work{limit};
            const auto result =
                flow_style4::check(paper, expected, palette, short_work, kFlowStyle4Following);
            EXPECT_EQ(result.outcome, flow_style4::Outcome::Inconclusive);
            EXPECT_TRUE(result.resource);
        }
        const auto rows = flow_style4::legend(expected, work);
        std::size_t longest = 0u;
        for (const auto& row : rows)
            longest = std::max(longest, row.size());
        const int fit = static_cast<int>(longest) + 2;
        const auto exact_frame = flow_style4::crop_width(paper, fit, work);
        EXPECT_EQ(
            flow_style4::check(exact_frame, expected, palette, work, kFlowStyle4Following).outcome,
            flow_style4::Outcome::Match);
        const auto crop = flow_style4::crop_width(paper, fit - 1, work);
        EXPECT_EQ(flow_style4::check(crop, expected, palette, work, kFlowStyle4Following).outcome,
                  flow_style4::Outcome::Inconclusive);
        auto large = flow_style4::copy_screen(paper, work);
        large.glyphs.cells[0] = std::string(1024u, 'x');
        EXPECT_EQ(flow_style4::check(large, expected, palette, work, kFlowStyle4Following).outcome,
                  flow_style4::Outcome::Inconclusive);
        auto scalar = flow_style4::copy_screen(paper, work);
        scalar.attributes.back().hyperlink = std::numeric_limits<unsigned>::max();
        const auto wrong =
            flow_style4::check(scalar, expected, palette, work, kFlowStyle4Following);
        EXPECT_EQ(wrong.outcome, flow_style4::Outcome::Mismatch);
        EXPECT_FALSE(wrong.resource);
        auto unknown = expected;
        unknown.domain = static_cast<flow_style4::Domain>(2);
        EXPECT_EQ(flow_style4::check(paper, unknown, palette, work, kFlowStyle4Following).outcome,
                  flow_style4::Outcome::Inconclusive);
        const auto noop = flow_style4::copy_screen(paper, work);
        EXPECT_EQ(flow_style4::check(noop, expected, palette, work, kFlowStyle4Following).outcome,
                  flow_style4::Outcome::Match);
    }
}

TEST(MermaidFlowStyles,
     OrderedReapplicationDirectLastAndClosedSourceGrammarKeepTheirActualMeanings) {
    FlowStylePreview preview;
    StyleThemeRestore restore;
    using namespace loom::ui::design::theme;
    set_theme(ThemeVariant::Dark);
    const std::string definitions = "classDef first fill:#f9f\nclassDef second fill:#0f0\n";
    const std::string reapply = "class A first\nclass A second\nclass A first\n";
    for (const bool direct : {false, true}) {
        SCOPED_TRACE(direct);
        const auto source = std::string("flowchart LR\nA-->B\n") +
                            (direct ? "style A fill:#00f\n" : "") + reapply + definitions;
        const auto node = loom::ui::mermaid::render_mermaid(source, ftxui::text("CLOSED_FLOW"));
        ASSERT_TRUE(node);
        const auto screen = style_screen(*node);
        EXPECT_EQ(screen.PixelAt(1, 1).background_color,
                  direct ? current_theme().palette->info_tint_bg
                         : current_theme().palette->surface_tint_4);
        const auto text = screen.ToString();
        const auto first = text.find("class A first ->"), second = text.find("class A second ->"),
                   last = text.find("class A first ->", first + 1u);
        EXPECT_LT(first, second);
        EXPECT_LT(second, last);
        EXPECT_NE(last, std::string::npos);
    }
    const std::array<std::string_view, 11> closed{
        {"A-->B\nclass A missing\n", "A-->B\nclassDef c fill:#f9f\nclass A,A c\n",
         "A-->B\nclassDef c fill:#f9f\nclassDef c stroke:#333\nclass A c\n",
         "A-->B\nclassDef default fill:#f9f\n", "A-->B\nstyle C fill:#f9f\n",
         "A(One)-->B\nstyle A fill:#f9f\n", "A-->B\nclassDef c fill:#f9f,fill:#0f0\nclass A c\n",
         "A-->B\nclassDef c fill:#f9f;class A c;\n",
         "A[\"classDef fake fill:red\"]-->B\nclass B fake\n",
         "subgraph G\nA-->B\nclassDef c fill:#f9f\nend\nclass A c\n",
         "A-->B\nstyle A fill:#f9f\rclass B c\n"}};
    for (const auto body : closed) {
        SCOPED_TRACE(body);
        EXPECT_FALSE(loom::ui::mermaid::render_mermaid(
            std::string("flowchart LR\n") + std::string(body), ftxui::text("CLOSED_FLOW")));
    }
}
}  // namespace

namespace {
TEST(MermaidFlowStylePreview, DefaultRenderingAndCacheCannotPublishUnadmittedPreviewLayouts) {
    const std::string source = "flowchart LR\nA-->B\nclassDef c fill:#f9f\nclass A c\n";
    EXPECT_FALSE(loom::ui::mermaid::render_mermaid(source, ftxui::text("SOURCE")));
    loom::ui::mermaid::LayoutRowsCache cache(4u, 1u << 20u);
    {
        FlowStylePreview preview;
        EXPECT_TRUE(cache.render(source, false, ftxui::text("SOURCE")));
        EXPECT_EQ(cache.size(), 0u);
    }
    EXPECT_FALSE(cache.render(source, false, ftxui::text("SOURCE")));
    EXPECT_FALSE(loom::ui::mermaid::render_mermaid(source, ftxui::text("SOURCE")));
}
}  // namespace

namespace {
TEST(MermaidPackedIntegration, SharedLedgerPrepaysFinalEmissionAndCacheHitCopies) {
    const std::string source = "block-beta\nA[\"Text\"]\nclassDef x fill:#f00\nclass A x\n";
    const auto fallback = ftxui::text("COMPLETE_PACKED_SOURCE");
    testing::Work measured;
    testing::FinalEmissionTrace full;
    const auto node = testing::render_mermaid_with_work(source, fallback, false, measured, &full);
    ASSERT_TRUE(node);
    ASSERT_TRUE(full.styled_entered);
    ASSERT_TRUE(full.preflight_complete);
    ASSERT_TRUE(full.allocation_started);
    ASSERT_TRUE(full.returned_native);
    ASSERT_GT(full.emission_debit, 0u);
    const auto spent = testing::kWorkLimit - measured.remaining;
    testing::Work exact{spent};
    testing::FinalEmissionTrace exact_trace;
    ASSERT_TRUE(testing::render_mermaid_with_work(source, fallback, false, exact, &exact_trace));
    EXPECT_EQ(exact.remaining, 0u);
    testing::Work short_work{spent - 1u};
    testing::FinalEmissionTrace cut_trace;
    EXPECT_FALSE(
        testing::render_mermaid_with_work(source, fallback, false, short_work, &cut_trace));
    EXPECT_TRUE(cut_trace.styled_entered);
    EXPECT_FALSE(cut_trace.preflight_complete);
    EXPECT_FALSE(cut_trace.allocation_started);
    EXPECT_FALSE(cut_trace.returned_native);
    EXPECT_EQ(cut_trace.failed_debit, full.last_debit);
    loom::ui::mermaid::LayoutRowsCache cold(4u, 1u << 20u);
    testing::Work exhausted{1u};
    EXPECT_FALSE(testing::CacheBudgetAccess::render(cold, source, false, fallback, exhausted));
    EXPECT_EQ(exhausted.remaining, 0u);
    EXPECT_EQ(cold.size(), 0u);
    EXPECT_TRUE(cold.render(source, false, fallback));
    EXPECT_EQ(cold.size(), 1u);
    loom::ui::mermaid::LayoutRowsCache cache(4u, 1u << 20u);
    testing::Work miss;
    ASSERT_TRUE(testing::CacheBudgetAccess::render(cache, source, false, fallback, miss));
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_GT(cache.byte_size(), source.size());
    testing::Work hit;
    testing::FinalEmissionTrace hit_trace;
    ASSERT_TRUE(
        testing::CacheBudgetAccess::render(cache, source, false, fallback, hit, &hit_trace));
    EXPECT_EQ(hit.remaining, miss.remaining);
    EXPECT_EQ(hit_trace.emission_debit, full.emission_debit);
    EXPECT_EQ(cache.hits(), 1u);
    testing::Work insufficient{spent - 1u};
    testing::FinalEmissionTrace cached_cut;
    EXPECT_FALSE(testing::CacheBudgetAccess::render(cache, source, false, fallback, insufficient,
                                                    &cached_cut));
    EXPECT_FALSE(cached_cut.allocation_started);
    EXPECT_FALSE(cached_cut.returned_native);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_TRUE(cache.render(source, false, fallback));
}
}  // namespace
