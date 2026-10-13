/// @file test_theme_widgets_features.cpp
/// @brief Theme and figure regressions for widgets and feature panels.
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_figures;
import loom.ui.widgets.components;
import loom.ui.widgets.stats;
import loom.ui.widgets.text_input;
import loom.ui.widgets.spinner;
import loom.ui.widgets.passes;
import loom.ui.features.grove;
import loom.ui.features.agents.agent_shared_widgets;
import loom.ui.features.plugins.plugin_marketplace_browse;

namespace {
namespace theme = loom::ui::design::theme;
namespace figs = loom::ui::design::figures;
struct RestoreTheme {
    theme::Theme original = theme::current_theme();
    ~RestoreTheme() { theme::set_theme(original); }
};

ftxui::Screen Paint(ftxui::Element element, int width = 80, int height = 20) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                      ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, std::move(element));
    return screen;
}

void ExpectGlyphColor(const ftxui::Screen& screen, std::string_view glyph,
                      ftxui::Color expected) {
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x < screen.dimx(); ++x) {
            if (screen.PixelAt(x, y).character == glyph) {
                EXPECT_EQ(screen.PixelAt(x, y).foreground_color, expected);
                return;
            }
        }
    }
    ADD_FAILURE() << "Missing glyph: " << glyph;
}

constexpr auto kVariants = std::array{
    theme::ThemeVariant::Light, theme::ThemeVariant::DarkDaltonized,
    theme::ThemeVariant::Monochrome};
}

TEST(ThemeWidgetsFeatures, NotificationSeverityUsesThemeAndExistingFigures) {
    RestoreTheme restore;
    for (const auto variant : kVariants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        using Level = loom::ui::NotificationLevel;
        for (const auto level : {Level::Info, Level::Success, Level::Warning, Level::Error}) {
            loom::ui::NotificationData data;
            data.level = level;
            data.title = "Notice";
            const auto expected = level == Level::Info ? pal.info
                : level == Level::Success ? pal.success
                : level == Level::Warning ? pal.warning : pal.danger;
            const auto figure = level == Level::Info ? figs::kInfo
                : level == Level::Success ? figs::kTick
                : level == Level::Warning ? figs::kWarning : figs::kCross;
            const auto screen = Paint(loom::ui::render_notification(data), 30, 3);
            ExpectGlyphColor(screen, figure, expected);
        }
    }
}

TEST(ThemeWidgetsFeatures, StatsHeatmapUsesSurfaceAndExistingGradient) {
    RestoreTheme restore;
    for (const auto variant : kVariants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        std::vector<ui::components::DailyActivity> activity;
        for (int count = 0; count <= 5; ++count) activity.push_back({"date", count});
        auto screen = Paint(ui::components::detail::RenderActivityHeatmap(activity), 30, 1);
        const auto colors = std::array{pal.surface_tint_1, pal.stats_green_1,
            pal.stats_green_2, pal.stats_green_3, pal.stats_green_4, pal.stats_green_5};
        for (int i = 0; i < 6; ++i) {
            EXPECT_EQ(screen.PixelAt(10 + i, 0).foreground_color, colors[i]);
            EXPECT_EQ(screen.PixelAt(10 + i, 0).character, figs::kSquare);
        }
        ui::components::StatsOptions opts;
        opts.data.fun_fact = "A useful fact";
        screen = Paint(ui::components::Stats(opts));
        EXPECT_NE(screen.ToString().find("Stats Overview"), std::string::npos);
        EXPECT_NE(screen.ToString().find("Note: A useful fact"), std::string::npos);
        EXPECT_EQ(screen.ToString().find('\xF0'), std::string::npos);
    }
}

TEST(ThemeWidgetsFeatures, HeldPanelsFollowThemeAndRespectExplicitOverride) {
    RestoreTheme restore;
    namespace passes = loom::ui::components::passes;
    namespace grove = loom::ui::components::grove;
    theme::set_theme(theme::ThemeVariant::Dark);
    passes::PassesViewState pass_state;
    pass_state.pass_name = "Active pass";
    auto pass = passes::BuildPassesPanel(pass_state);
    const auto pinned_theme = theme::current_theme();
    auto pinned_pass = passes::BuildPassesPanel(pass_state, pinned_theme);
    grove::GroveViewState grove_state;
    grove::GroveNode node;
    node.id = "n";
    node.title = "Zebra";
    node.relevance = 0.9;
    node.kind = grove::GroveKind::Symbol;
    grove_state.roots.push_back(std::move(node));
    auto tree = grove::BuildGroveTree(grove_state);
    auto pinned_tree = grove::BuildGroveTree(grove_state, pinned_theme);
    for (const auto variant : kVariants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        ExpectGlyphColor(Paint(pass->Render()), "A", pal.primary);
        ExpectGlyphColor(Paint(pinned_pass->Render()), "A", pinned_theme.palette->primary);
        ExpectGlyphColor(Paint(tree->Render()), "Z", pal.text);
        ExpectGlyphColor(Paint(pinned_tree->Render()), "Z", pinned_theme.palette->text);
        ExpectGlyphColor(Paint(tree->Render()), "S", pal.accent_green);
        ExpectGlyphColor(Paint(pinned_tree->Render()), "S", pinned_theme.palette->accent_green);
    }
}

TEST(ThemeWidgetsFeatures, HeldInputResolvesDefaultPrefixAndPreservesCustomColor) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    ui::components::TextInputOptions opts;
    opts.prefix = "P ";
    opts.show_line_numbers = false;
    opts.cursor_blink_ms = 0;
    std::shared_ptr<ui::components::TextInputImpl> input_state;
    auto input = ui::components::TextInput(opts, &input_state);
    const auto custom = ftxui::Color::RGB(12, 34, 56);
    opts.prefix_color = custom;
    std::shared_ptr<ui::components::TextInputImpl> custom_state;
    auto explicit_input = ui::components::TextInput(opts, &custom_state);
    opts.placeholder.clear();
    auto custom_no_placeholder = ui::components::TextInput(opts);
    opts.prefix_color.reset();
    auto no_placeholder = ui::components::TextInput(opts);
    for (const auto variant : kVariants) {
        theme::set_theme(variant);
        const auto expected = theme::current_theme().palette->prompt_border;
        for (const auto& value : {std::string{}, std::string{"body"}}) {
            input_state->set_text(value);
            custom_state->set_text(value);
            ExpectGlyphColor(Paint(input->Render()), "P", expected);
            ExpectGlyphColor(Paint(explicit_input->Render()), "P", custom);
        }
        ExpectGlyphColor(Paint(no_placeholder->Render()), "P", expected);
        ExpectGlyphColor(Paint(custom_no_placeholder->Render()), "P", custom);
    }
}

TEST(ThemeWidgetsFeatures, AgentColorsFollowSubagentTokensAndForkFigure) {
    RestoreTheme restore;
    namespace agents = loom::ui::agents::shared;
    for (const auto variant : kVariants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        EXPECT_EQ(agents::agent_color_to_ftxui(agents::AgentColor::Red), pal.subagent_red);
        EXPECT_EQ(agents::agent_color_to_ftxui(agents::AgentColor::Blue), pal.subagent_blue);
        EXPECT_EQ(agents::agent_color_to_ftxui(agents::AgentColor::Green), pal.subagent_green);
        EXPECT_EQ(agents::agent_color_to_ftxui(agents::AgentColor::Yellow), pal.subagent_yellow);
        EXPECT_EQ(agents::agent_color_to_ftxui(agents::AgentColor::Purple), pal.subagent_purple);
        EXPECT_EQ(agents::agent_color_to_ftxui(agents::AgentColor::Orange), pal.subagent_orange);
        EXPECT_EQ(agents::agent_color_to_ftxui(agents::AgentColor::Pink), pal.subagent_pink);
        EXPECT_EQ(agents::agent_color_to_ftxui(agents::AgentColor::Cyan), pal.subagent_cyan);
        EXPECT_EQ(agents::status_color(agents::AgentStatus::Errored), pal.danger);
        for (const auto status : {agents::AgentStatus::Idle, agents::AgentStatus::Scheduled,
                                  agents::AgentStatus::Errored}) {
            const auto figure = status == agents::AgentStatus::Idle ? figs::kCircle
                : status == agents::AgentStatus::Scheduled ? figs::kEllipsis : figs::kCross;
            const auto dot = Paint(agents::StatusDot({.status = status}), 10, 1);
            EXPECT_NE(dot.ToString().find(figure), std::string::npos);
        }
        const auto screen = Paint(agents::AgentAvatar({.name = "worker", .agent_type = "worker"}));
        EXPECT_NE(screen.ToString().find(figs::kForkGlyph), std::string::npos);
        EXPECT_EQ(screen.ToString().find('\xF0'), std::string::npos);
    }
}

TEST(ThemeWidgetsFeatures, SuggestionsUseExistingFiguresAndPreserveCallerIcons) {
    RestoreTheme restore;
    using Category = ui::components::SuggestionCategory;
    namespace suggest = ui::components::suggest_util;
    EXPECT_EQ(std::string_view(suggest::category_icon(Category::Command)), figs::kLightningBolt);
    EXPECT_EQ(std::string_view(suggest::category_icon(Category::Agent)), figs::kDiamond);
    EXPECT_EQ(std::string_view(suggest::category_icon(Category::MCPResource)), figs::kCircleDouble);
    for (const auto variant : kVariants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        EXPECT_EQ(suggest::category_color(Category::Agent), pal.primary);
        EXPECT_EQ(suggest::category_color(Category::Shell), pal.bash_border);
        EXPECT_EQ(suggest::category_color(Category::File), pal.text);
        const std::vector<loom::ui::SearchResult> results = {
            {.label = "Result", .detail = "Caller supplied", .icon = "🦉"}};
        const auto screen = Paint(loom::ui::render_search_box("query", results, 0));
        EXPECT_NE(screen.ToString().find("🦉"), std::string::npos);
    }
}

TEST(ThemeWidgetsFeatures, HeldSpinnerUsesCurrentThemeAndStableReducedMotionFigure) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    ui::components::SpinnerOptions options;
    options.frame = 0;
    auto spinner = ui::components::Spinner(options);
    options.reduced_motion = true;
    auto reduced = ui::components::Spinner(options);
    for (const auto variant : kVariants) {
        theme::set_theme(variant);
        auto screen = Paint(spinner->Render(), 10, 1);
        EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, theme::current_theme().palette->info);
        screen = Paint(reduced->Render(), 10, 1);
        EXPECT_EQ(screen.PixelAt(0, 0).character, figs::kBullet);
        EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, theme::current_theme().palette->muted);
    }
}

TEST(ThemeWidgetsFeatures, HeldMarketplaceResolvesPaletteDuringRender) {
    RestoreTheme restore;
    namespace browse = loom::ui::plugins::plugin_marketplace_browse;
    theme::set_theme(theme::ThemeVariant::Dark);
    browse::BrowsePanelInputs options;
    options.initial_subtab = browse::SubTab::Discover;
    auto component = browse::MakeBrowsePanel(std::move(options));
    for (const auto variant : kVariants) {
        theme::set_theme(variant);
        ExpectGlyphColor(Paint(component->Render()), "A", theme::current_theme().palette->info);
    }
}

TEST(ThemeWidgetsFeatures, HeldDiffUsesReadableForegroundAndKeepsLinePrefixes) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    loom::ui::DiffLine added;
    added.kind = loom::ui::DiffLineKind::Added;
    added.content = "added line";
    loom::ui::DiffLine removed;
    removed.kind = loom::ui::DiffLineKind::Removed;
    removed.content = "removed line";
    const std::vector<loom::ui::DiffLine> lines{added, removed};
    auto component = ftxui::Renderer([lines] {
        return loom::ui::render_diff_view("diff.cpp", lines);
    });
    for (const auto variant : {theme::ThemeVariant::Dark, theme::ThemeVariant::Light,
            theme::ThemeVariant::DarkDaltonized, theme::ThemeVariant::Monochrome}) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        const auto screen = Paint(component->Render(), 40, 8);
        ExpectGlyphColor(screen, "+", pal.success);
        ExpectGlyphColor(screen, "-", pal.danger);
        ExpectGlyphColor(screen, "a", pal.success);
        ExpectGlyphColor(screen, "r", pal.danger);
        EXPECT_NE(screen.ToString().find("+ added line"), std::string::npos);
        EXPECT_NE(screen.ToString().find("- removed line"), std::string::npos);
    }
}
