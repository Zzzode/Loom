/// @file test_theme_dialogs_permissions.cpp
/// @brief Semantic theme and caller override regressions for dialogs/permissions.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_figures;
import loom.ui.foundation.design_tokens;
import loom.ui.permissions.components;
import loom.ui.permissions.rule_list;
import loom.ui.dialogs.prompt_dialog;
import loom.ui.dialogs.plugin_dialog;
import loom.ui.dialogs.wizard_dialog;
import loom.ui.dialogs.settings_dialog;
import loom.ui.dialogs.mcp_dialogs;

namespace {
namespace theme = loom::ui::design::theme;
namespace figs = loom::ui::design::figures;
namespace pc = loom::ui::permissions::components;
namespace rules = loom::ui::permissions::rule_list;
namespace settings = loom::ui::dialogs::settings_dialog;

struct RestoreTheme {
    theme::Theme saved = theme::current_theme();
    ~RestoreTheme() { theme::set_theme(saved); }
};

constexpr std::array variants{
    theme::ThemeVariant::Light,
    theme::ThemeVariant::DarkDaltonized,
    theme::ThemeVariant::LightDaltonized,
    theme::ThemeVariant::Monochrome,
};

ftxui::Screen RenderScreen(ftxui::Element element, int width = 160, int height = 40) {
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, std::move(element));
    return screen;
}

void ExpectGlyphColor(const ftxui::Screen& screen, std::string_view glyph,
                      ftxui::Color expected) {
    bool found = false;
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x < screen.dimx(); ++x) {
            const auto& pixel = screen.PixelAt(x, y);
            if (pixel.character == glyph) {
                EXPECT_EQ(pixel.foreground_color, expected);
                found = true;
            }
        }
    }
    EXPECT_TRUE(found) << "Missing rendered glyph: " << glyph;
}

void ExpectNoDecorativeEmoji(const ftxui::Screen& screen) {
    // All former icons are astral emoji or emoji presentation sequences.
    const auto text = screen.ToString();
    EXPECT_EQ(text.find('\xF0'), std::string::npos);
    EXPECT_EQ(text.find("\xEF\xB8\x8F"), std::string::npos);
}
} // namespace

TEST(DialogPermissionTheme, SemanticRiskAndActionColorsFollowCurrentTheme) {
    RestoreTheme restore;
    for (auto variant : variants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        EXPECT_EQ(pc::ActionColor(pc::ActionKind::Read), pal.info);
        EXPECT_EQ(pc::ActionColor(pc::ActionKind::Write), pal.warning);
        EXPECT_EQ(pc::ActionColor(pc::ActionKind::Environment), pal.permission);
        EXPECT_EQ(pc::ActionColor(pc::ActionKind::Plan), pal.plan_mode);
        EXPECT_EQ(pc::RiskPillBgColor(pc::RiskLevel::Low), pal.success);
        EXPECT_EQ(pc::RiskPillBgColor(pc::RiskLevel::Medium), pal.warning);
        EXPECT_EQ(pc::RiskPillBgColor(pc::RiskLevel::Critical), pal.danger);
        auto pill = RenderScreen(pc::RiskPill(pc::RiskLevel::Critical), 20, 1);
        EXPECT_NE(pill.ToString().find("CRITICAL"), std::string::npos);
        ExpectGlyphColor(pill, "C", pal.inverse_text);
    }
}

TEST(DialogPermissionTheme, PermissionAndPluginDecorationsKeepLabelsWithoutEmoji) {
    RestoreTheme restore;
    constexpr std::array names{
        "FileReadTool", "FileWriteTool", "FileEditTool", "GlobTool", "GrepTool",
        "BashTool", "PowerShellTool", "WebFetchTool", "WebSearchTool", "AgentTool",
        "SkillTool", "EnterPlanModeTool", "exit_plan_mode", "TaskCreateTool",
        "TaskUpdateTool", "TeamCreateTool", "MCPTool", "NotebookEditTool",
        "AskUserQuestionTool", "ComputerUseApproval", "UnknownTool",
    };
    for (auto variant : variants) {
        theme::set_theme(variant);
        for (auto name : names) {
            auto screen = RenderScreen(ftxui::hbox({pc::ToolIcon(name), ftxui::text(name)}));
            ExpectNoDecorativeEmoji(screen);
            EXPECT_NE(screen.ToString().find(name), std::string::npos);
        }
        for (int kind = 0; kind <= static_cast<int>(pc::ActionKind::Other); ++kind)
            ExpectNoDecorativeEmoji(RenderScreen(pc::ActionIcon(static_cast<pc::ActionKind>(kind))));
        auto help = RenderScreen(loom::ui::dialogs::plugin_dialog::RenderHelp());
        ExpectNoDecorativeEmoji(help);
        EXPECT_NE(help.ToString().find("Plugin Command Help"), std::string::npos);
    }
}

TEST(DialogPermissionTheme, HeldPromptDialogResolvesThemeWhenRendered) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    loom::ui::dialogs::prompt_dialog::PromptDialogPayload payload;
    payload.title = "Theme probe";
    payload.options = {{"Zebra", "z", "Choose this response"}};
    auto component = loom::ui::dialogs::prompt_dialog::MakePromptDialog(std::move(payload));
    for (auto variant : variants) {
        theme::set_theme(variant);
        auto screen = RenderScreen(component->Render());
        ExpectGlyphColor(screen, "Z", theme::current_theme().palette->info);
        EXPECT_NE(screen.ToString().find("Choose this response"), std::string::npos);
    }
}

TEST(DialogPermissionTheme, HeldDefaultRuleGroupsResolveThemeWhenRendered) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    auto component = rules::MakePermissionRuleList({}, {});
    for (auto variant : variants) {
        theme::set_theme(variant);
        auto screen = RenderScreen(component->Render());
        ExpectGlyphColor(screen, figs::kTick, theme::current_theme().palette->success);
        EXPECT_NE(screen.ToString().find("Allowed"), std::string::npos);
        EXPECT_NE(screen.ToString().find("Permission Rule List"), std::string::npos);
        ExpectNoDecorativeEmoji(screen);
    }
}

TEST(DialogPermissionTheme, CallerRuleGroupsRetainExplicitColorsAndGlyphs) {
    RestoreTheme restore;
    const auto custom_color = ftxui::Color::RGB(12, 34, 56);
    rules::RuleListInput input;
    input.groups = {{"custom", "Caller", "1", "@", custom_color}};
    auto component = rules::MakePermissionRuleList(std::move(input), {});
    for (auto variant : variants) {
        theme::set_theme(variant);
        auto screen = RenderScreen(component->Render());
        ExpectGlyphColor(screen, "@", custom_color);
        EXPECT_NE(screen.ToString().find("Caller"), std::string::npos);
    }
}

TEST(DialogPermissionTheme, WizardRolesAndPluginLetterAccentsResolveCurrentTheme) {
    RestoreTheme restore;
    namespace wizard = loom::ui::wizard_dialog;
    namespace plugin = loom::ui::dialogs::plugin_dialog;
    theme::set_theme(theme::ThemeVariant::Dark);
    (void)plugin::detail::name_color("theme-probe");
    for (auto variant : variants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        EXPECT_EQ(wizard::to_ftxui_color(wizard::WizardColor::suggestion), pal.suggestion);
        EXPECT_EQ(wizard::to_ftxui_color(wizard::WizardColor::permission), pal.permission);
        EXPECT_EQ(wizard::to_ftxui_color(wizard::WizardColor::warning), pal.warning);
        auto icon = RenderScreen(plugin::detail::LetterIcon("theme-probe"), 10, 1);
        const auto color = plugin::detail::name_color("theme-probe");
        EXPECT_EQ(color, pal.primary);
        ExpectGlyphColor(icon, "T", color);
    }
}

TEST(DialogPermissionTheme, ToastRoleUpdatesAfterThemeChangeAndPreservesCustomColor) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    settings::ToastState toast;
    toast.message = "Z saved";
    toast.until = std::chrono::steady_clock::now() + std::chrono::hours(1);
    toast.semantic_role = loom::ui::design::tokens::Role::Danger;
    const auto custom_color = ftxui::Color::RGB(23, 45, 67);
    for (auto variant : variants) {
        theme::set_theme(variant);
        auto screen = RenderScreen(settings::RenderFooter(toast, false));
        ExpectGlyphColor(screen, "Z", theme::current_theme().palette->danger);
        auto custom = toast;
        custom.semantic_role.reset();
        custom.color = custom_color;
        ExpectGlyphColor(RenderScreen(settings::RenderFooter(custom, false)), "Z", custom_color);
    }
}

TEST(DialogPermissionTheme, StatusShapesAndSpinnerUseSharedFiguresInMonochrome) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Monochrome);
    namespace mcp = loom::ui::mcp_dialogs;
    auto success = RenderScreen(mcp::RenderConnectionState(mcp::McpConnectionState::connected));
    auto error = RenderScreen(mcp::RenderConnectionState(mcp::McpConnectionState::error));
    EXPECT_NE(success.ToString().find(figs::kTick), std::string::npos);
    EXPECT_NE(error.ToString().find(figs::kCross), std::string::npos);
    EXPECT_NE(success.ToString(), error.ToString());
    EXPECT_EQ(settings::health_display(settings::HealthState::OK).first, figs::kTick);
    EXPECT_EQ(settings::health_display(settings::HealthState::Down).first, figs::kCross);
    EXPECT_NE(RenderScreen(pc::StatusDot(pc::ItemStatus::Approved)).ToString(),
              RenderScreen(pc::StatusDot(pc::ItemStatus::Denied)).ToString());
    for (std::uint64_t tick = 0; tick < 2 * figs::kSpinnerFrameCount; ++tick)
        EXPECT_EQ(mcp::BrailleSpinnerFrame(tick),
                  figs::kSpinnerFrames[tick % figs::kSpinnerFrameCount]);
}

TEST(DialogPermissionTheme, CanonicalExitPlanModeUsesApprovalGlyph) {
    EXPECT_EQ(pc::ToolIconGlyph("exit_plan_mode"), figs::kTick);
    EXPECT_EQ(pc::ToolIconGlyph("ExitPlanMode"), figs::kTick);
    auto screen = RenderScreen(pc::ToolIcon("exit_plan_mode"), 3, 1);
    EXPECT_NE(screen.ToString().find(figs::kTick), std::string::npos);
}
