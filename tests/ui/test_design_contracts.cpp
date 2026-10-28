/// @file test_design_contracts.cpp
/// @brief Internal token deletion must preserve theme and identity contracts.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.foundation.design_tokens;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.component_primitives;
import loom.ui.features.agents.agent_shared_widgets;
import loom.tools.agent_color_manager;

namespace {
namespace tokens = loom::ui::design::tokens;
namespace theme = loom::ui::design::theme;
namespace primitives = loom::ui::design::primitives;
namespace colors = loom::tools::agent_color_manager;
constexpr std::array kVariants = {
    theme::ThemeVariant::Dark,           theme::ThemeVariant::Light,
    theme::ThemeVariant::DarkDaltonized, theme::ThemeVariant::LightDaltonized,
    theme::ThemeVariant::Monochrome,     theme::ThemeVariant::LightAnsi,
    theme::ThemeVariant::DarkAnsi};

struct RestoreTheme {
    theme::Theme original = theme::current_theme();

    ~RestoreTheme() { theme::set_theme(original); }
};

ftxui::Screen Paint(ftxui::Element element) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(12), ftxui::Dimension::Fixed(1));
    ftxui::Render(screen, std::move(element));
    return screen;
}
}  // namespace

TEST(DesignContracts, RetainedRoleOrdinalsAndDistinctBorderStayStable) {
    EXPECT_EQ(static_cast<int>(tokens::Role::Primary), 0);
    EXPECT_EQ(static_cast<int>(tokens::Role::AutoAccept), 24);
    const auto& dark = tokens::palette::dark();
    EXPECT_EQ(dark.border_subtle, dark.chrome);
    EXPECT_NE(dark.border_subtle, dark.subtle);
    EXPECT_EQ(tokens::palette::light().border_subtle, tokens::palette::light().subtle);
    for (auto variant : kVariants) {
        const auto& pal = *theme::palette_for_variant(variant);
        EXPECT_EQ(tokens::token_by_role(pal, tokens::Role::Muted), pal.muted);
        EXPECT_EQ(tokens::token_by_role(pal, tokens::Role::Ultra), pal.rainbow_shimmer);
        EXPECT_EQ(tokens::token_by_role(pal, tokens::Role::AutoAccept), pal.auto_accept);
    }
}

TEST(DesignContracts, RainbowLookupStringsRemainArrayBackedInAllThemes) {
    constexpr std::array<std::string_view, 7> keys = {
        "rainbow_red",  "rainbow_orange", "rainbow_yellow", "rainbow_green",
        "rainbow_blue", "rainbow_indigo", "rainbow_violet"};
    for (auto variant : kVariants) {
        const theme::Theme handle{variant, theme::palette_for_variant(variant)};
        for (std::size_t i = 0; i < keys.size(); ++i) {
            EXPECT_EQ(theme::resolve_color(keys[i], handle), handle.palette->rainbow[i]);
            EXPECT_EQ(theme::resolve_color(std::string(keys[i]) + "_shimmer", handle),
                      handle.palette->rainbow_shimmer_stops[i]);
        }
        EXPECT_EQ(theme::resolve_color("rainbow_shimmer", handle), handle.palette->rainbow_shimmer);
    }
}

TEST(DesignContracts, ThemeNamesLiteralColorsAndUnknownFallbackStayCompatible) {
    for (auto variant : kVariants) {
        EXPECT_EQ(theme::parse_variant(theme::variant_name(variant)), variant);
        const theme::Theme handle{variant, theme::palette_for_variant(variant)};
        EXPECT_EQ(theme::resolve_color("rgb(12,34,56)", handle), ftxui::Color::RGB(12, 34, 56));
        EXPECT_EQ(theme::resolve_color("unknown-token", handle), handle.palette->text);
        EXPECT_EQ(theme::resolve_color("userMessageBackgroundHover", handle),
                  handle.palette->user_message_background_hover);
        EXPECT_EQ(theme::resolve_color("user_message_background_hover", handle),
                  handle.palette->user_message_background_hover);
        EXPECT_EQ(theme::resolve_color("permission", handle), handle.palette->permission);
    }
    EXPECT_EQ(theme::parse_variant("auto"), theme::ThemeVariant::Dark);
    EXPECT_EQ(theme::parse_variant("ansi"), theme::ThemeVariant::Monochrome);
}

TEST(DesignContracts, EightAgentNamesAndLegacyThemeKeysRemainCompatible) {
    RestoreTheme restore;
    constexpr std::array<std::string_view, 8> names = {"red",    "blue",   "green", "yellow",
                                                       "purple", "orange", "pink",  "cyan"};
    for (auto variant : kVariants) {
        theme::set_theme(variant);
        for (auto name : names) {
            const auto identity = colors::parse_color_name(name);
            ASSERT_TRUE(identity);
            const auto expected = loom::ui::agents::shared::agent_color_to_ftxui(*identity);
            EXPECT_EQ(
                theme::resolve_color(colors::to_theme_color_key(*identity), theme::current_theme()),
                expected);
            EXPECT_EQ(theme::resolve_color("subagent_" + std::string(name), theme::current_theme()),
                      expected);
        }
    }
    EXPECT_FALSE(colors::parse_color_name("unknown-color"));
}

TEST(DesignContracts, KeptSpinnerAndKeyboardPrimitivesPreserveMotionAndInk) {
    for (auto variant : kVariants) {
        theme::Theme handle{variant, theme::palette_for_variant(variant)};
        for (std::uint64_t frame : {0u, 9u, 10u, 123u}) {
            const auto screen =
                Paint(primitives::spinner_glyph(frame, tokens::Role::Primary, handle));
            EXPECT_EQ(screen.PixelAt(0, 0).character,
                      primitives::k_spinner_frames[frame % primitives::k_spinner_frames.size()]);
            EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, handle.palette->primary);
        }
        handle.a11y.reduced_motion = true;
        EXPECT_EQ(Paint(primitives::spinner_glyph(55, tokens::Role::Primary, handle))
                      .PixelAt(0, 0)
                      .character,
                  "●");
    }
    const auto keyboard = Paint(primitives::kbd("Esc"));
    EXPECT_EQ(keyboard.PixelAt(1, 0).character, "E");
    EXPECT_TRUE(keyboard.PixelAt(1, 0).bold);
    EXPECT_TRUE(keyboard.PixelAt(1, 0).inverted);
}
