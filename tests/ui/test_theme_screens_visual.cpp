/// @file test_theme_screens_visual.cpp
/// @brief Default visual presentation follows the UI palette without altering explicit colors.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>
import std;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_figures;
import loom.ui.visual.markdown;
import loom.ui.visual.code_highlight;
import loom.ui.visual.structured_diff;
import loom.ui.screens.doctor_screen;

namespace {
namespace th = loom::ui::design::theme;
namespace ch = loom::ui::code_highlight;
namespace sd = loom::ui::structured_diff;
struct RestoreTheme {
    th::Theme original = th::current_theme();
    ~RestoreTheme() { th::set_theme(original); }
};
constexpr std::array kVariants = {th::ThemeVariant::Light,
    th::ThemeVariant::DarkDaltonized, th::ThemeVariant::LightDaltonized,
    th::ThemeVariant::Monochrome};
ftxui::Screen Render(ftxui::Element element) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(80),
                                      ftxui::Dimension::Fixed(15));
    ftxui::Render(screen, std::move(element));
    return screen;
}
void ExpectWordColor(const ftxui::Screen& screen, std::string_view word, ftxui::Color expected) {
    bool found = false;
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x + static_cast<int>(word.size()) <= screen.dimx(); ++x) {
            bool matches = true;
            for (std::size_t i = 0; i < word.size(); ++i)
                if (screen.PixelAt(x + static_cast<int>(i), y).character != std::string(1, word[i]))
                    matches = false;
            if (!matches) continue;
            found = true;
            for (std::size_t i = 0; i < word.size(); ++i)
                EXPECT_EQ(screen.PixelAt(x + static_cast<int>(i), y).foreground_color, expected) << word;
        }
    }
    EXPECT_TRUE(found) << word;
}
std::optional<double> Luminance(ftxui::Color color) {
    const auto encoded = color.Print(false);
    if (!encoded.starts_with("38;2;")) return std::nullopt; // ANSI colors depend on the terminal palette.
    std::string_view values{encoded};
    values.remove_prefix(5);
    std::array<double, 3> linear{};
    for (int i = 0; i < 3; ++i) {
        int channel = 0;
        const auto result = std::from_chars(values.data(), values.data() + values.size(), channel);
        if (result.ec != std::errc{}) return std::nullopt;
        const double c = channel / 255.0;
        linear[i] = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
        const auto consumed = static_cast<std::size_t>(result.ptr - values.data());
        values.remove_prefix(std::min(values.size(), consumed + 1));
    }
    return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2];
}
void ExpectReadableColors(ftxui::Color foreground, ftxui::Color background) {
    EXPECT_NE(foreground, background);
    const auto fg = Luminance(foreground);
    const auto bg = Luminance(background);
    if (fg && bg) EXPECT_GE((std::max(*fg, *bg) + 0.05) / (std::min(*fg, *bg) + 0.05), 3.0);
}

}
TEST(ScreenVisualTheme, DefaultSyntaxAndDiffColorsFollowPalette) {
    RestoreTheme restore;
    for (auto variant : kVariants) {
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        ch::SyntaxTheme code;
        EXPECT_EQ(code.plain, pal.text);
        EXPECT_EQ(code.keyword, pal.primary);
        EXPECT_EQ(code.string_lit, pal.success);
        EXPECT_EQ(code.error, pal.danger);
        EXPECT_EQ(code.bg, pal.background);
        auto diff = sd::default_theme();
        EXPECT_EQ(diff.added_fg, pal.success);
        EXPECT_EQ(diff.removed_fg, pal.danger);
        EXPECT_EQ(diff.line_num_fg, pal.muted);
        sd::StructuredDiffLine line;
        line.type = sd::StructuredDiffLine::Type::Added;
        line.content = "added";
        ExpectWordColor(Render(sd::RenderWordHighlightedContent(line, diff)), "added", pal.success);
    }
}
TEST(ScreenVisualTheme, CachedMarkdownTokensAreRenderedWithCurrentTheme) {
    RestoreTheme restore;
    loom::ui::clear_markdown_cache();
    for (auto variant : kVariants) {
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        ExpectWordColor(Render(loom::ui::render_markdown("`code` ![image](https://example.test) $math$")), "code", pal.permission);
        ExpectWordColor(Render(loom::ui::render_markdown("`code` ![image](https://example.test) $math$")), "image", pal.suggestion);
        ExpectWordColor(Render(loom::ui::render_markdown("```cpp\nreturn 1;\n```")), "return", pal.primary);
    }
}
TEST(ScreenVisualTheme, ExplicitSyntaxAndDiffOverridesRemainUnchanged) {
    RestoreTheme restore;
    const auto code = ch::get_theme(ch::ThemeName::Monokai);
    const auto diff = sd::get_diff_syntax_theme("monokai");
    ASSERT_TRUE(diff);
    for (auto variant : kVariants) {
        th::set_theme(variant);
        EXPECT_EQ(ch::get_theme(ch::ThemeName::Monokai).keyword, code.keyword);
        EXPECT_EQ(sd::get_diff_syntax_theme("monokai")->added_fg, diff->added_fg);
        loom::ui::MarkdownOptions opts;
        opts.theme_name = "monokai";
        ExpectWordColor(Render(loom::ui::render_markdown("```cpp\nreturn 1;\n```", opts)), "return", code.keyword);
        const auto custom = th::resolve_color("rgb(12,34,56)", th::current_theme());
        auto customized = sd::default_theme();
        customized.added_fg = custom;
        sd::StructuredDiffLine line;
        line.type = sd::StructuredDiffLine::Type::Added;
        line.content = "custom";
        ExpectWordColor(Render(sd::RenderWordHighlightedContent(line, customized)), "custom", custom);
    }
}
TEST(ScreenVisualTheme, DoctorSeverityUsesSemanticTokensAndDistinctFigures) {
    RestoreTheme restore;
    namespace doctor = loom::ui::doctor_screen;
    namespace figs = loom::ui::design::figures;
    for (auto variant : kVariants) {
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        const auto ok = doctor::severity_display(doctor::DiagnosticSeverity::Ok);
        const auto error = doctor::severity_display(doctor::DiagnosticSeverity::Error);
        EXPECT_EQ(ok.first, figs::kTick);
        EXPECT_EQ(ok.second, pal.success);
        EXPECT_EQ(error.first, figs::kCross);
        EXPECT_EQ(error.second, pal.danger);
        EXPECT_NE(ok.first, error.first);
    }
}

TEST(ScreenVisualTheme, DiffChangedWordsRemainReadableInEveryTheme) {
    RestoreTheme restore;
    for (auto variant : kVariants) {
        th::set_theme(variant);
        const auto diff = sd::default_theme();
        for (auto type : {sd::StructuredDiffLine::Type::Added, sd::StructuredDiffLine::Type::Removed}) {
            sd::StructuredDiffLine line;
            line.type = type;
            line.content = "word";
            line.word_changes.push_back({.start_col = 0, .end_col = 4,
                .is_added = type == sd::StructuredDiffLine::Type::Added});
            auto screen = Render(sd::RenderWordHighlightedContent(line, diff));
            bool found = false;
            for (int y = 0; y < screen.dimy(); ++y)
                for (int x = 0; x < screen.dimx(); ++x)
                    if (screen.PixelAt(x, y).character == "w") {
                        found = true;
                        ExpectReadableColors(screen.PixelAt(x, y).foreground_color,
                                             screen.PixelAt(x, y).background_color);
                    }
            EXPECT_TRUE(found);
        }
    }
}

TEST(ScreenVisualTheme, HeldStreamingMarkdownRefreshesOnlyWhenThemeOrContentChanges) {
    RestoreTheme restore;
    th::set_theme(th::ThemeVariant::Dark);
    loom::ui::StreamingMarkdown markdown;
    constexpr std::string_view content = "`stream`";
    auto first = markdown.update(content);
    EXPECT_EQ(markdown.update(content), first);
    for (auto variant : kVariants) {
        th::set_theme(variant);
        auto next = markdown.update(content);
        EXPECT_NE(next, first);
        ExpectWordColor(Render(next), "stream", th::current_theme().palette->permission);
        EXPECT_EQ(markdown.update(content), next);
        first = next;
    }
    markdown.reset();
    EXPECT_NE(markdown.update(content), first);
}
