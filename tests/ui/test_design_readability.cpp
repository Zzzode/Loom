/// @file test_design_readability.cpp
/// @brief Contrast and rendered-ink regressions for RFC 0013.
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.foundation.design_tokens;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_figures;
import loom.ui.chrome.fullscreen_layout;
import loom.ui.messages.user_text_message;
import loom.ui.features.todos.todo_panel;
import loom.ui.features.teams.live_teammates;

namespace {
namespace theme = loom::ui::design::theme;
namespace fullscreen = loom::ui::layout::fullscreen;
namespace messages = loom::ui::messages;
namespace todos = loom::ui::todos;
namespace teams = loom::ui::teams::live;
namespace figures = loom::ui::design::figures;

constexpr auto kVariants =
    std::array{theme::ThemeVariant::Dark,           theme::ThemeVariant::Light,
               theme::ThemeVariant::DarkDaltonized, theme::ThemeVariant::LightDaltonized,
               theme::ThemeVariant::Monochrome,     theme::ThemeVariant::LightAnsi,
               theme::ThemeVariant::DarkAnsi};
constexpr auto kRgbVariants =
    std::array{theme::ThemeVariant::Dark, theme::ThemeVariant::Light,
               theme::ThemeVariant::DarkDaltonized, theme::ThemeVariant::LightDaltonized};

struct RestoreTheme {
    theme::Theme original = theme::current_theme();

    ~RestoreTheme() { theme::set_theme(original); }
};

// The production API does not expose RGB channels. Decode the actual FTXUI
// truecolor SGR representation only in this test, rather than assuming that
// Color::Print emits the application's rgb(...) input syntax.
std::optional<std::array<int, 3>> Rgb(ftxui::Color color) {
    const std::string sgr = color.Print(false);
    if (!sgr.starts_with("38;2;")) return std::nullopt;
    std::array<int, 3> channels{};
    const char* cursor = sgr.data() + 5;
    const char* end = sgr.data() + sgr.size();
    for (std::size_t i = 0; i < channels.size(); ++i) {
        const auto result = std::from_chars(cursor, end, channels[i]);
        if (result.ec != std::errc{} || channels[i] < 0 || channels[i] > 255) return std::nullopt;
        cursor = result.ptr;
        if (i + 1 < channels.size()) {
            if (cursor == end || *cursor++ != ';') return std::nullopt;
        }
    }
    if (cursor != end) return std::nullopt;
    return channels;
}

double Luminance(const std::array<int, 3>& rgb) {
    auto linear = [](int channel) {
        const double value = static_cast<double>(channel) / 255.0;
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return linear(rgb[0]) * 0.2126 + linear(rgb[1]) * 0.7152 + linear(rgb[2]) * 0.0722;
}

void ExpectContrast(ftxui::Color foreground, ftxui::Color background) {
    const auto fg = Rgb(foreground);
    const auto bg = Rgb(background);
    ASSERT_TRUE(fg.has_value()) << "Expected RGB foreground: " << foreground.Print(false);
    ASSERT_TRUE(bg.has_value()) << "Expected RGB background: " << background.Print(false);
    const double a = Luminance(*fg);
    const double b = Luminance(*bg);
    EXPECT_GE((std::max(a, b) + 0.05) / (std::min(a, b) + 0.05), 4.5);
}

ftxui::Screen Paint(ftxui::Element element, int height = 4) {
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(100), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, std::move(element));
    return screen;
}

void ExpectWordInk(const ftxui::Screen& screen, std::string_view word, ftxui::Color foreground,
                   ftxui::Color background = ftxui::Color::Default, bool struck = false) {
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x + static_cast<int>(word.size()) <= screen.dimx(); ++x) {
            bool matches = true;
            for (std::size_t i = 0; i < word.size(); ++i) {
                if (screen.PixelAt(x + static_cast<int>(i), y).character !=
                    std::string(1, word[i])) {
                    matches = false;
                    break;
                }
            }
            if (!matches) continue;
            for (std::size_t i = 0; i < word.size(); ++i) {
                const auto& pixel = screen.PixelAt(x + static_cast<int>(i), y);
                EXPECT_EQ(pixel.foreground_color, foreground) << word;
                EXPECT_EQ(pixel.background_color, background) << word;
                EXPECT_FALSE(pixel.dim) << word;
                EXPECT_EQ(pixel.strikethrough, struck) << word;
            }
            return;
        }
    }
    ADD_FAILURE() << "Missing rendered word: " << word;
}

void ExpectGlyphInk(const ftxui::Screen& screen, std::string_view glyph, ftxui::Color foreground,
                    ftxui::Color background) {
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x < screen.dimx(); ++x) {
            const auto& pixel = screen.PixelAt(x, y);
            if (pixel.character != glyph) continue;
            EXPECT_EQ(pixel.foreground_color, foreground);
            EXPECT_EQ(pixel.background_color, background);
            EXPECT_FALSE(pixel.dim);
            return;
        }
    }
    ADD_FAILURE() << "Missing rendered glyph: " << glyph;
}

std::array<ftxui::Color, 8> AgentColors(const theme::Palette& pal) {
    return {pal.subagent_red,    pal.subagent_blue,   pal.subagent_green, pal.subagent_yellow,
            pal.subagent_purple, pal.subagent_orange, pal.subagent_pink,  pal.subagent_cyan};
}

std::vector<ftxui::Color> BrandColors(const theme::Palette& pal) {
    std::vector<ftxui::Color> colors{
        pal.primary,          pal.primary_shimmer, pal.bash_border,
        pal.plan_mode,        pal.fast_mode,       pal.fast_mode_shimmer,
        pal.spinner_gold,     pal.brief_label,     pal.brief_label_you,
        pal.brief_label_loom, pal.role_pill_user,  pal.role_pill_assistant,
        pal.role_pill_system, pal.role_pill_tool,  pal.role_pill_thinking,
        pal.accent_pink,      pal.rainbow_shimmer};
    colors.insert(colors.end(), pal.rainbow.begin(), pal.rainbow.end());
    colors.insert(colors.end(), pal.rainbow_shimmer_stops.begin(), pal.rainbow_shimmer_stops.end());
    return colors;
}
}  // namespace

TEST(DesignReadability, TruecolorActualSurfacePairsMeetOrdinaryTextContrast) {
    RestoreTheme restore;
    for (const auto variant : kRgbVariants) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        for (const auto background :
             {pal.background, pal.user_message_background, pal.user_message_background_hover,
              pal.message_actions_background}) {
            ExpectContrast(pal.muted, background);
            ExpectContrast(pal.text, background);
        }
        ExpectContrast(pal.suggestion, pal.message_actions_background);
        // Task/team rows are transparent: this verifies the palette reference
        // canvas, not an unknown terminal canvas or user-defined ANSI colors.
        ExpectContrast(pal.success, pal.background);
        ExpectContrast(pal.warning, pal.background);
    }
}

TEST(DesignReadability, ConcreteThemeControlForegroundsDoNotEqualTheirSurfaces) {
    for (const auto variant : kVariants) {
        SCOPED_TRACE(theme::variant_name(variant));
        const auto& pal = *theme::palette_for_variant(variant);
        for (const auto background :
             {pal.background, pal.user_message_background, pal.user_message_background_hover,
              pal.message_actions_background}) {
            EXPECT_NE(pal.muted, background);
            EXPECT_NE(pal.text, background);
        }
        EXPECT_NE(pal.suggestion, pal.message_actions_background);
        EXPECT_NE(pal.success, pal.background);
        EXPECT_NE(pal.warning, pal.background);
    }
}

TEST(DesignReadability, NavigationPillsAndHeldStickyHoverUseReadableInk) {
    RestoreTheme restore;
    for (const auto variant : kVariants) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        for (bool actionable : {false, true}) {
            const auto screen = Paint(fullscreen::NewMessagesPill(7, actionable));
            ExpectWordInk(screen, "new", pal.muted,
                          actionable ? pal.user_message_background_hover
                                     : pal.user_message_background);
        }
        auto sticky = ftxui::Make<fullscreen::StickyPromptHeaderComponent>("Breadcrumb", [] {});
        ExpectWordInk(Paint(sticky->Render()), "Breadcrumb", pal.muted,
                      pal.user_message_background);
        ftxui::Mouse mouse;
        mouse.x = 4;
        mouse.y = 0;
        ASSERT_TRUE(sticky->OnEvent(ftxui::Event::Mouse("", mouse)));
        ExpectWordInk(Paint(sticky->Render()), "Breadcrumb", pal.muted,
                      pal.user_message_background_hover);
    }
}

TEST(DesignReadability, UserPromptAndCommandPrefixesStayReadableWhenSelected) {
    RestoreTheme restore;
    messages::UserTextMessageData data;
    data.content = "Readable prompt";
    data.command_name = "inspect";
    for (const auto variant : kVariants) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        for (bool selected : {false, true}) {
            const auto screen = Paint(messages::RenderUserPromptMessage(data, selected));
            ExpectGlyphInk(screen, figures::kPointer, selected ? pal.suggestion : pal.muted,
                           selected ? pal.message_actions_background : pal.user_message_background);
            ExpectWordInk(screen, "Readable", pal.text,
                          selected ? pal.message_actions_background : pal.user_message_background);
            ExpectGlyphInk(Paint(messages::RenderUserCommandMessage(data, selected)),
                           figures::kPointer, pal.muted, pal.user_message_background);
        }
    }
}

TEST(DesignReadability, ActivityInformationAndBoundedSummariesDoNotUseDim) {
    RestoreTheme restore;
    todos::TodoDisplayItem done;
    done.content = "Completed";
    done.status = todos::TodoDisplayItem::Status::Completed;
    todos::TodoDisplayItem active;
    active.content = "Running";
    active.status = todos::TodoDisplayItem::Status::InProgress;
    for (const auto variant : kVariants) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        const auto todo_screen = Paint(todos::RenderTodoPanel({done, active}, 100));
        ExpectWordInk(todo_screen, "Completed", pal.muted, ftxui::Color::Default, true);
        ExpectGlyphInk(todo_screen, "✓", pal.muted, ftxui::Color::Default);
        ExpectWordInk(todo_screen, "Running", pal.text);
        ExpectWordInk(todo_screen, "Tasks", pal.muted);
        ExpectWordInk(Paint(todos::RenderTodoPanel({done, active}, 100, 1)), "hidden", pal.muted);
        for (const auto& status : {"running", "idle", "unknown"}) {
            teams::LiveTeammate teammate;
            teammate.name = "worker";
            teammate.status = status;
            teammate.last_output_tail = "Output";
            ExpectWordInk(Paint(teams::RenderLiveTeammateRow(teammate, 100)), status,
                          teams::status_color(status));
            ExpectWordInk(Paint(teams::RenderLiveTeammateRow(teammate, 100)), "Output", pal.muted);
            ExpectWordInk(Paint(teams::RenderLiveTeammateStrip({teammate, teammate}, 100, 1)),
                          "hidden", pal.muted);
        }
    }
}

TEST(DesignBrand, ModesAndRoleAccentsShareTheirThemePrimaryAndGold) {
    for (const auto variant : kVariants) {
        SCOPED_TRACE(theme::variant_name(variant));
        const auto& pal = *theme::palette_for_variant(variant);
        for (const auto color :
             {pal.bash_border, pal.plan_mode, pal.brief_label, pal.brief_label_loom,
              pal.role_pill_assistant, pal.role_pill_thinking, pal.accent_pink})
            EXPECT_EQ(color, pal.primary);
        for (const auto color : {pal.fast_mode, pal.brief_label_you, pal.role_pill_user,
                                 pal.role_pill_system, pal.role_pill_tool})
            EXPECT_EQ(color, pal.spinner_gold);
    }
}

TEST(DesignBrand, TruecolorWarmAccentsAndHighlightsStayReadable) {
    for (const auto variant : kRgbVariants) {
        SCOPED_TRACE(theme::variant_name(variant));
        const auto& pal = *theme::palette_for_variant(variant);
        for (const auto color : BrandColors(pal)) {
            const auto rgb = Rgb(color);
            ASSERT_TRUE(rgb.has_value());
            EXPECT_GE((*rgb)[0], (*rgb)[1]);
            EXPECT_GE((*rgb)[1], (*rgb)[2]);
            ExpectContrast(color, pal.background);
        }
        // Legacy role labels and mode glyphs use the same readable accents.
        for (const auto color : {pal.primary, pal.spinner_gold}) {
            ExpectContrast(color, pal.user_message_background);
            ExpectContrast(color, pal.surface_tint_4);
        }
        EXPECT_NE(pal.primary, pal.spinner_gold);
        EXPECT_NE(pal.success, pal.danger);
        EXPECT_NE(pal.success, pal.warning);
        EXPECT_NE(pal.info, pal.warning);
    }
}

TEST(DesignBrand, EightNamedAgentForegroundsAreUniqueAndReadableOnCardSurfaces) {
    RestoreTheme restore;
    constexpr std::array<std::string_view, 8> names{"red",    "blue",   "green", "yellow",
                                                    "purple", "orange", "pink",  "cyan"};
    for (const auto variant : kRgbVariants) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        const auto colors = AgentColors(pal);
        std::set<std::string> distinct;
        for (std::size_t i = 0; i < colors.size(); ++i) {
            SCOPED_TRACE(names[i]);
            distinct.insert(colors[i].Print(false));
            ExpectContrast(colors[i], pal.background);
            ExpectContrast(colors[i], pal.surface_tint_4);
            teams::LiveTeammate teammate;
            teammate.name = "Identity";
            teammate.color = names[i];
            const auto screen = Paint(teams::RenderLiveTeammateRow(teammate, 100));
            ExpectWordInk(screen, "Identity", colors[i]);
        }
        EXPECT_EQ(distinct.size(), names.size());
    }
}

TEST(DesignBrand, LogoBodyIdentityIsPreservedWhileDarkBodyUsesWarmOffWhite) {
    for (const auto variant : kRgbVariants) {
        SCOPED_TRACE(theme::variant_name(variant));
        const auto& pal = *theme::palette_for_variant(variant);
        EXPECT_EQ(pal.loom_body, ftxui::Color::RGB(215, 119, 87));
        EXPECT_NE(pal.primary, pal.loom_body);
        if (variant == theme::ThemeVariant::Dark ||
            variant == theme::ThemeVariant::DarkDaltonized) {
            EXPECT_EQ(pal.text, ftxui::Color::RGB(224, 218, 210));
            EXPECT_NE(pal.text, ftxui::Color::RGB(255, 255, 255));
        }
    }
}

TEST(DesignBrand, AnsiTouchedAccentsStayInNamedSixteenColorPalette) {
    const auto named =
        std::array{ftxui::Color::Black,     ftxui::Color::Red,          ftxui::Color::Green,
                   ftxui::Color::Yellow,    ftxui::Color::Blue,         ftxui::Color::Magenta,
                   ftxui::Color::Cyan,      ftxui::Color::GrayLight,    ftxui::Color::GrayDark,
                   ftxui::Color::RedLight,  ftxui::Color::GreenLight,   ftxui::Color::YellowLight,
                   ftxui::Color::BlueLight, ftxui::Color::MagentaLight, ftxui::Color::CyanLight,
                   ftxui::Color::White};
    for (const auto variant : {theme::ThemeVariant::LightAnsi, theme::ThemeVariant::DarkAnsi}) {
        SCOPED_TRACE(theme::variant_name(variant));
        const auto& pal = *theme::palette_for_variant(variant);
        auto colors = BrandColors(pal);
        const auto agents = AgentColors(pal);
        colors.insert(colors.end(), agents.begin(), agents.end());
        for (const auto color : colors) {
            EXPECT_TRUE(std::ranges::any_of(named, [color](auto index) {
                return color == ftxui::Color(index);
            })) << color.Print(false);
        }
    }
}

TEST(DesignBrand, MonochromeTouchedAccentsAndIdentitiesRemainAchromatic) {
    const auto& pal = *theme::palette_for_variant(theme::ThemeVariant::Monochrome);
    auto colors = BrandColors(pal);
    const auto agents = AgentColors(pal);
    colors.insert(colors.end(), agents.begin(), agents.end());
    for (const auto color : colors) {
        EXPECT_TRUE(color == ftxui::Color(ftxui::Color::Black) ||
                    color == ftxui::Color(ftxui::Color::GrayDark) ||
                    color == ftxui::Color(ftxui::Color::GrayLight) ||
                    color == ftxui::Color(ftxui::Color::White))
            << color.Print(false);
    }
}
