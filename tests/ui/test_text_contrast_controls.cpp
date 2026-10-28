/// @file test_text_contrast_controls.cpp
/// @brief Current agent, permission and install-wizard text contrast regressions.
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.foundation.theme_provider;
import loom.ui.features.agents.agent_shared_widgets;
import loom.ui.features.agents.agent_cards;
import loom.ui.features.agents.agent_wizard;
import loom.tools.agent_color_manager;
import loom.teams.swarm.backends;
import loom.ui.permissions.components;
import loom.ui.permissions.rule_list;
import loom.ui.permissions.scope_editor;
import loom.ui.features.plugins.plugin_install_flow;

namespace {
namespace th = loom::ui::design::theme;
namespace agents = loom::ui::agents::shared;
namespace cards = loom::ui::agents::cards;
namespace agent_wizard = loom::ui::agents::wizard;
namespace colors = loom::tools::agent_color_manager;
namespace permission_components = loom::ui::permissions::components;
namespace rules = loom::ui::permissions::rule_list;
namespace scope = loom::ui::permissions::scope_editor;
namespace wizard = loom::ui::plugins::plugin_install_flow;
constexpr auto kAll =
    std::array{th::ThemeVariant::Dark,           th::ThemeVariant::Light,
               th::ThemeVariant::DarkDaltonized, th::ThemeVariant::LightDaltonized,
               th::ThemeVariant::Monochrome,     th::ThemeVariant::LightAnsi,
               th::ThemeVariant::DarkAnsi};

bool IsRgb(th::ThemeVariant variant) {
    return variant == th::ThemeVariant::Dark || variant == th::ThemeVariant::Light ||
           variant == th::ThemeVariant::DarkDaltonized ||
           variant == th::ThemeVariant::LightDaltonized;
}

struct RestoreTheme {
    th::Theme old = th::current_theme();

    ~RestoreTheme() { th::set_theme(old); }
};

std::optional<double> Luminance(ftxui::Color color) {
    const auto sgr = color.Print(false);
    if (!sgr.starts_with("38;2;")) return std::nullopt;
    const char* cursor = sgr.data() + 5;
    const char* end = sgr.data() + sgr.size();
    std::array<double, 3> channels{};
    for (std::size_t i = 0; i < channels.size(); ++i) {
        int channel = 0;
        const auto parsed = std::from_chars(cursor, end, channel);
        if (parsed.ec != std::errc{} || channel < 0 || channel > 255) return std::nullopt;
        cursor = parsed.ptr;
        if (i + 1 < channels.size() && (cursor == end || *cursor++ != ';')) return std::nullopt;
        const double c = channel / 255.0;
        channels[i] = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    }
    if (cursor != end) return std::nullopt;
    return channels[0] * 0.2126 + channels[1] * 0.7152 + channels[2] * 0.0722;
}

void ExpectRgbContrast(ftxui::Color foreground, ftxui::Color background) {
    const auto fg = Luminance(foreground);
    const auto bg = Luminance(background);
    ASSERT_TRUE(fg.has_value()) << foreground.Print(false);
    ASSERT_TRUE(bg.has_value()) << background.Print(false);
    EXPECT_GE((std::max(*fg, *bg) + 0.05) / (std::min(*fg, *bg) + 0.05), 4.5)
        << foreground.Print(false) << " against " << background.Print(false);
}

ftxui::Screen Paint(ftxui::Element element, int width = 120, int height = 12,
                    ftxui::Color canvas = ftxui::Color::Default) {
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, std::move(element) | ftxui::bgcolor(canvas));
    return screen;
}

std::optional<ftxui::Pixel> Word(const ftxui::Screen& screen, std::string_view word) {
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x + static_cast<int>(word.size()) <= screen.dimx(); ++x) {
            bool match = true;
            for (std::size_t i = 0; i < word.size(); ++i) {
                if (screen.PixelAt(x + static_cast<int>(i), y).character !=
                    std::string(1, word[i])) {
                    match = false;
                    break;
                }
            }
            if (!match) continue;
            for (std::size_t i = 0; i < word.size(); ++i) {
                const auto& pixel = screen.PixelAt(x + static_cast<int>(i), y);
                EXPECT_FALSE(pixel.dim) << word;
                EXPECT_EQ(pixel.foreground_color, screen.PixelAt(x, y).foreground_color) << word;
                EXPECT_EQ(pixel.background_color, screen.PixelAt(x, y).background_color) << word;
            }
            return screen.PixelAt(x, y);
        }
    }
    ADD_FAILURE() << "Missing visible word: " << word;
    return std::nullopt;
}

void ExpectWord(const ftxui::Screen& screen, std::string_view word, ftxui::Color foreground,
                ftxui::Color background, bool numeric = false) {
    const auto pixel = Word(screen, word);
    ASSERT_TRUE(pixel.has_value());
    EXPECT_EQ(pixel->foreground_color, foreground) << word;
    EXPECT_EQ(pixel->background_color, background) << word;
    EXPECT_NE(pixel->foreground_color, pixel->background_color) << word;
    if (numeric) ExpectRgbContrast(pixel->foreground_color, pixel->background_color);
}

// Dim is retained for completed history and unavailable actions only.
std::optional<ftxui::Pixel> HistoricalWord(const ftxui::Screen& screen, std::string_view word) {
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x + static_cast<int>(word.size()) <= screen.dimx(); ++x) {
            bool match = true;
            for (std::size_t i = 0; i < word.size(); ++i)
                if (screen.PixelAt(x + static_cast<int>(i), y).character != std::string(1, word[i]))
                    match = false;
            if (match) return screen.PixelAt(x, y);
        }
    }
    ADD_FAILURE() << "Missing historical word: " << word;
    return std::nullopt;
}
}  // namespace

TEST(TextContrastControls, CurrentAgentStatusLabelsKeepReadableTextAndStatusGlyphs) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        for (const auto status : {agents::AgentStatus::Idle, agents::AgentStatus::Running,
                                  agents::AgentStatus::Scheduled, agents::AgentStatus::Errored,
                                  agents::AgentStatus::Disabled}) {
            SCOPED_TRACE(agents::status_label(status));
            const auto foreground = status == agents::AgentStatus::Errored ? pal.danger : pal.muted;
            auto screen = Paint(cards::StatusBadge(status), 50, 3, pal.background);
            ExpectWord(screen, agents::status_label(status), foreground, pal.background,
                       IsRgb(variant));
            agents::StatusDotOptions options;
            options.status = status;
            options.with_label = true;
            screen = Paint(agents::StatusDot(options), 50, 1, pal.background);
            ExpectWord(screen, agents::status_label(status), foreground, pal.background,
                       IsRgb(variant));
        }
    }
}

TEST(TextContrastControls, AgentRoleCountsStatsAndToolSummariesAreReadableOnCardSurfaces) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        const auto card_background = variant == th::ThemeVariant::Monochrome
                                         ? pal.message_actions_background
                                         : pal.surface_tint_4;
        for (const auto canvas : {pal.background, card_background}) {
            agents::RunStats stats;
            stats.run_count = 17;
            stats.running_count = 2;
            stats.total_tokens = 58000;
            const auto stats_screen = Paint(agents::RunStatsBar(stats), 100, 1, canvas);
            for (const auto word : {"runs", "running", "tokens"})
                ExpectWord(stats_screen, word, pal.muted, canvas, IsRgb(variant));
            ExpectWord(stats_screen, "17", pal.text, canvas, IsRgb(variant));
            const auto tags = Paint(agents::RoleTags({"Researcher", "Reviewer"}, 1), 80, 3, canvas);
            ExpectWord(tags, "Researcher", pal.info, canvas, IsRgb(variant));
            ExpectWord(tags, "more", pal.muted, canvas, IsRgb(variant));
            agents::ToolChipsOptions options;
            options.tools = {"FirstTool", "SecondTool"};
            options.collapse_threshold = 1;
            const auto tools = Paint(agents::ToolChipsElement(options), 80, 3, canvas);
            ExpectWord(tools, "tools", pal.muted, canvas, IsRgb(variant));
        }
    }
}

TEST(TextContrastControls, AgentCardCurrentMetadataUsesActualNormalAndSelectedSurfaces) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        cards::AgentCardData agent;
        agent.name = "CurrentAgent";
        agent.agent_type = "general-purpose";
        agent.description = "ReadableDescription";
        agent.description_long = "LongDescription";
        agent.role_tags = {"CurrentRole"};
        agent.model_override = "CurrentModel";
        agent.permission_mode = "CurrentPolicy";
        agent.env_vars = {"ENV_METADATA"};
        agent.tools = {"Read"};
        agent.stats.running_count = 2;
        agent.last_run_at = "Yesterday";
        for (const bool selected : {false, true}) {
            const auto selected_background = variant == th::ThemeVariant::Monochrome
                                                 ? pal.message_actions_background
                                                 : pal.surface_tint_4;
            const auto canvas = selected ? selected_background : pal.background;
            const auto mini = Paint(cards::MiniCard(agent, selected), 140, 1, pal.background);
            ExpectWord(mini, "CurrentAgent", pal.text, canvas, IsRgb(variant));
            ExpectWord(mini, "ReadableDescription", pal.muted, canvas, IsRgb(variant));
            const auto card = Paint(cards::Card(agent, selected), 160, 16, pal.background);
            ExpectWord(card, "CurrentRole", pal.info, canvas, IsRgb(variant));
            const auto large = Paint(cards::LargeCard(agent, selected), 160, 40, pal.background);
            ExpectWord(large, "CurrentRole", pal.info, canvas, IsRgb(variant));
            for (const auto word : {"LongDescription", "model:", "CurrentPolicy", "enabled",
                                    "ENV_METADATA", "Yesterday", "running"})
                ExpectWord(large, word, pal.muted, canvas, IsRgb(variant));
            ExpectWord(large, "Permissions", pal.info, canvas, IsRgb(variant));
            ExpectWord(large, "CurrentModel", pal.info, canvas, IsRgb(variant));
            ExpectWord(large, "Run", pal.text, canvas, IsRgb(variant));
        }
    }
}

TEST(TextContrastControls, ActiveTimelineMetadataIsReadableWhileCompletedHistoryKeepsDim) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        for (const auto status : {agents::AgentStatus::Running, agents::AgentStatus::Scheduled,
                                  agents::AgentStatus::Errored}) {
            agents::TimelineStep step;
            step.index = 23;
            step.tool_name = "ActiveTool";
            step.status = status;
            step.status_text = "CurrentState";
            step.output_preview = "CurrentPreviewOverflow";
            step.duration = std::chrono::milliseconds{1500};
            agents::StepTimelineOptions options;
            options.steps = {step};
            options.truncate_preview_chars = 14;
            const auto screen = Paint(agents::StepTimeline(options), 120, 8, pal.background);
            for (const auto word : {"23", "1.5s", "CurrentPreview", "expand"})
                ExpectWord(screen, word, pal.muted, pal.background, IsRgb(variant));
            ExpectWord(screen, "CurrentState",
                       status == agents::AgentStatus::Errored ? pal.danger : pal.muted,
                       pal.background, IsRgb(variant));
        }
        agents::TimelineStep completed;
        completed.status = agents::AgentStatus::Idle;
        completed.tool_name = "FinishedTool";
        completed.output_preview = "HistoricalPreview";
        agents::StepTimelineOptions options;
        options.steps = {completed};
        const auto history = Paint(agents::StepTimeline(options), 120, 8, pal.background);
        const auto pixel = HistoricalWord(history, "HistoricalPreview");
        ASSERT_TRUE(pixel.has_value());
        EXPECT_TRUE(pixel->dim);
        const auto disabled =
            Paint(cards::FooterButton("Unavailable", pal.info, true), 40, 3, pal.background);
        const auto inactive = HistoricalWord(disabled, "Unavailable");
        ASSERT_TRUE(inactive.has_value());
        EXPECT_TRUE(inactive->dim);
    }
}

TEST(TextContrastControls, PermissionRuleMetadataRemainsReadableThroughHoverSelectionAndDisable) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        rules::RuleEntry rule;
        rule.tool_pattern = "CurrentTool";
        rule.strategy = rules::MatchStrategy::Glob;
        rule.action = rules::PermissionAction::Allow;
        rule.scope = rules::PermissionScope::Session;
        rule.source = "User";
        for (const bool enabled : {false, true}) {
            rule.enabled = enabled;
            for (const bool hovered : {false, true}) {
                for (const bool selected : {false, true}) {
                    const auto canvas = selected ? pal.perm_rule_bg_info : pal.background;
                    const auto screen =
                        Paint(rules::render::RuleRow(rule, 17, hovered, selected, false), 160, 1,
                              pal.background);
                    for (const auto word : {"17", "Glob", "Session"})
                        ExpectWord(screen, word, pal.muted, canvas, IsRgb(variant));
                    const auto pixel = Word(screen, "Session");
                    ASSERT_TRUE(pixel.has_value());
                    EXPECT_EQ(pixel->inverted, hovered);
                }
            }
        }
    }
}

TEST(TextContrastControls, PermissionPickersAndToolbarKeepReadableInstructionsAndHover) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        constexpr std::array<std::string_view, 3> names{"First", "Second", "Third"};
        const auto picker =
            Paint(rules::render::MiniPicker(1, pal.info, names), 80, 1, pal.background);
        ExpectWord(picker, "First", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(picker, "Second", pal.info, pal.background, IsRgb(variant));
        const auto selected = Word(picker, "Second");
        ASSERT_TRUE(selected.has_value());
        EXPECT_TRUE(selected->inverted);
        rules::RuleListState state;
        for (const bool hovered : {false, true}) {
            state.focus = hovered ? rules::FocusZone::Toolbar : rules::FocusZone::Rules;
            state.toolbar_cursor = 0;
            const auto toolbar = Paint(rules::render::Toolbar(state), 160, 3, pal.background);
            ExpectWord(toolbar, "trigger", pal.muted, pal.background, IsRgb(variant));
            ExpectWord(toolbar, "New rule", pal.success, pal.background, IsRgb(variant));
        }
    }
}

TEST(TextContrastControls, WorkspaceDefaultPolicyUsesReadableFilledBackground) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        using Policy = loom::utils::permissions_engine::WorkspaceEntry::Policy;
        for (const bool inherited : {false, true}) {
            const auto screen =
                Paint(rules::ui_tabs_detail::WorkspacePolicyBadge(Policy::Default, inherited), 40,
                      1, pal.background);
            ExpectWord(screen, "DEFAULT", pal.muted, pal.message_actions_background,
                       IsRgb(variant));
        }
        const bool ansi =
            variant == th::ThemeVariant::LightAnsi || variant == th::ThemeVariant::DarkAnsi;
        for (const auto policy : {Policy::Allow, Policy::Deny}) {
            const bool allow = policy == Policy::Allow;
            const auto background = ansi    ? pal.background
                                    : allow ? pal.success_tint_bg
                                            : pal.danger_tint_bg;
            const auto screen = Paint(rules::ui_tabs_detail::WorkspacePolicyBadge(policy, false),
                                      40, 1, pal.background);
            ExpectWord(screen, allow ? "ALLOW" : "DENY", allow ? pal.success : pal.danger,
                       background, IsRgb(variant));
        }
    }
}

TEST(TextContrastControls, ScopeEditorCurrentRowsAndActionsPreserveSelectionWithoutDim) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        scope::EditorState state;
        scope::AllowlistRow allow;
        allow.path_pattern = "/CurrentPath";
        allow.strategy = scope::MatchStrategy::Glob;
        allow.scope = scope::PathScope::Read;
        state.allowlist = {allow};
        scope::PreapprovalRow preapproval;
        preapproval.tool_pattern = "CurrentTool";
        preapproval.action_pattern = "ReadAction";
        preapproval.action = scope::PermissionAction::Allow;
        preapproval.scope = scope::PermissionScope::Session;
        state.preapprovals = {preapproval};
        for (const bool selected : {false, true}) {
            state.al_cursor = selected ? 0 : 1;
            const auto allowlist = Paint(scope::RenderAllowlistTab(state), 160, 10, pal.background);
            ExpectWord(
                allowlist, "[R]",
                permission_components::PathScopeColor(permission_components::PathScope::Read),
                pal.background, IsRgb(variant));
            const auto scope_badge = Word(allowlist, "[R]");
            ASSERT_TRUE(scope_badge.has_value());
            EXPECT_EQ(scope_badge->inverted, selected);
            ExpectWord(allowlist, "glob", pal.muted, pal.background, IsRgb(variant));
            ExpectWord(allowlist, "Add entry", pal.muted, pal.background, IsRgb(variant));
            ExpectWord(allowlist, "import", pal.muted, pal.background, IsRgb(variant));
            state.pa_cursor = selected ? 0 : 1;
            const auto preapprovals =
                Paint(scope::RenderPreapprovalTab(state), 160, 10, pal.background);
            ExpectWord(preapprovals, "ReadAction", pal.muted, pal.background, IsRgb(variant));
            ExpectWord(preapprovals, "Session", pal.muted, pal.background, IsRgb(variant));
            ExpectWord(preapprovals, "New-rule wizard", pal.muted, pal.background, IsRgb(variant));
            const auto pixel = Word(preapprovals, "Session");
            ASSERT_TRUE(pixel.has_value());
            EXPECT_EQ(pixel->inverted, selected);
        }
    }
}

TEST(TextContrastControls, ScopeWizardAndDangerousPathInstructionsAreReadable) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        scope::EditorState state;
        state.show_wizard = true;
        state.wizard.step = 2;
        const auto wizard_screen =
            Paint(scope::RenderPreapprovalTab(state), 160, 20, pal.background);
        ExpectWord(wizard_screen, "Path pattern:", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(wizard_screen, "cancel", pal.muted, pal.background, IsRgb(variant));
        scope::DangerousPathRow dangerous;
        dangerous.path = "/sensitive";
        dangerous.reason = "CurrentReason";
        state.dangerous = {dangerous};
        const auto screen = Paint(scope::RenderDangerousTab(state), 160, 10, pal.background);
        ExpectWord(screen, "CurrentReason", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "reset", pal.muted, pal.background, IsRgb(variant));
    }
}

TEST(TextContrastControls, InstallSourceStepKeepsNormalSelectedSubtitlesAndNavigationReadable) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        wizard::SourceStepData data;
        for (int selected = 0; selected < 4; ++selected) {
            const auto screen =
                Paint(wizard::detail::RenderSourceStep(data, selected), 160, 30, pal.background);
            ExpectWord(screen, "Browse available", pal.muted,
                       selected == 0 ? pal.surface_tint_3 : pal.background, IsRgb(variant));
            ExpectWord(screen, "Clone from", pal.muted,
                       selected == 3 ? pal.surface_tint_3 : pal.background, IsRgb(variant));
            ExpectWord(screen, "Navigation:", pal.muted, pal.background, IsRgb(variant));
            ExpectWord(screen, "focus input", pal.muted, pal.background, IsRgb(variant));
        }
    }
}

TEST(TextContrastControls, InstallReviewMetadataAndTrustWarningUseActualFilledPairs) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        wizard::ReviewStepData data;
        data.name = "ReviewedPlugin";
        data.version = "1.2.3";
        data.author = "CurrentAuthor";
        data.description = "ReviewDescription";
        data.install_scope = "user";
        data.license = "MIT";
        data.permissions = {"file:read", "file:write", "bash:unsafe"};
        const auto screen = Paint(wizard::detail::RenderReviewStep(data), 160, 40, pal.background);
        for (const auto word :
             {"Verify", "1.2.3", "CurrentAuthor", "ReviewDescription", "License:"})
            ExpectWord(screen, word, pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "file:read", pal.info, pal.background, IsRgb(variant));
        ExpectWord(screen, "file:write", pal.warning, pal.background, IsRgb(variant));
        ExpectWord(screen, "bash:unsafe", pal.danger, pal.background, IsRgb(variant));
        const bool ansi =
            variant == th::ThemeVariant::LightAnsi || variant == th::ThemeVariant::DarkAnsi;
        const auto warning_background = ansi ? pal.background : pal.warning_tint_bg;
        ExpectWord(screen, "trust reminder", pal.warning, warning_background, IsRgb(variant));
        ExpectWord(screen, "Make sure", pal.muted, warning_background, IsRgb(variant));
    }
}

TEST(TextContrastControls, InstallTrustStepRequiredHintAndVerifiedStatusStayReadable) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        wizard::TrustStepData data;
        data.is_verified = true;
        const auto screen = Paint(wizard::detail::RenderTrustStep(data, ftxui::text("TrustPanel") |
                                                                            ftxui::color(pal.text)),
                                  160, 15, pal.background);
        ExpectWord(screen, "Required", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "verified", pal.success, pal.background, IsRgb(variant));
        ExpectWord(screen, "TrustPanel", pal.text, pal.background, IsRgb(variant));
    }
}

TEST(TextContrastControls, InstallProgressPercentLogAndErrorDetailsRemainReadable) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        wizard::InstallProgressData data;
        data.stage = wizard::InstallStage::Downloading;
        data.percent = 37;
        data.stage_label = "DownloadStage";
        data.log_lines = {"InstallLog"};
        data.error = "FailureReason";
        const auto screen = Paint(wizard::detail::RenderInstallStep(data), 160, 30, pal.background);
        ExpectWord(screen, "37%", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "InstallLog", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "FailureReason", pal.danger, pal.background, IsRgb(variant));
        ExpectWord(screen, "DownloadStage", pal.info, pal.background, IsRgb(variant));
    }
}

TEST(TextContrastControls, InstallCompletionAndFailureRecoveryInstructionsStayReadable) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        wizard::CompleteStepData data;
        data.installed_id = "InstalledPlugin";
        data.installed_version = "2.3.4";
        data.install_scope = "user";
        data.post_install_hint = "NextInstruction";
        data.first_run_hotkey = "Ctrl-P";
        auto screen = Paint(wizard::detail::RenderCompleteStep(data), 160, 30, pal.background);
        for (const auto word :
             {"ready", "2.3.4", "scope:", "NextInstruction", "configure", "start using"})
            ExpectWord(screen, word, pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "Ctrl-P", pal.warning, pal.background, IsRgb(variant));
        data.success = false;
        screen = Paint(wizard::detail::RenderCompleteStep(data), 160, 12, pal.background);
        ExpectWord(screen, "could not", pal.danger, pal.background, IsRgb(variant));
        ExpectWord(screen, "different source", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "retry", pal.muted, pal.background, IsRgb(variant));
    }
}

TEST(TextContrastControls, EveryPathScopeLabelIsReadableWithoutDim) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        for (const auto value :
             {permission_components::PathScope::Read, permission_components::PathScope::Write,
              permission_components::PathScope::Execute, permission_components::PathScope::Network,
              permission_components::PathScope::All}) {
            const auto screen =
                Paint(permission_components::PathScopeBadge(value), 20, 1, pal.background);
            ExpectWord(screen, permission_components::PathScopeLabel(value),
                       permission_components::PathScopeColor(value), pal.background,
                       IsRgb(variant));
        }
    }
}

TEST(TextContrastControls, ResolvedAgentAvatarEightColorsAndNormalAvatarKeepReadableInitial) {
    RestoreTheme restore;
    using AgentColor = loom::utils::swarm_backends::AgentColor;
    constexpr std::string_view key = "contrast-test-avatar";
    const auto previous = colors::get_agent_color(key);

    struct RestoreColor {
        std::optional<AgentColor> old;

        ~RestoreColor() { colors::set_agent_color("contrast-test-avatar", old); }
    } restore_color{previous};

    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        agents::AvatarOptions options;
        options.name = "Avatar";
        options.agent_type = std::string{key};
        for (const auto hue :
             {AgentColor::Red, AgentColor::Blue, AgentColor::Green, AgentColor::Yellow,
              AgentColor::Purple, AgentColor::Orange, AgentColor::Pink, AgentColor::Cyan}) {
            colors::set_agent_color(key, hue);
            const auto screen = Paint(agents::AgentAvatar(options), 20, 3, pal.background);
            ExpectWord(screen, "A", pal.inverse_text, agents::agent_color_to_ftxui(hue),
                       IsRgb(variant));
        }
        options.force_general = true;
        const auto normal = Paint(agents::AgentAvatar(options), 20, 3, pal.background);
        ExpectWord(normal, "A", pal.text, pal.surface_tint_4, IsRgb(variant));
    }
}

TEST(TextContrastControls, AgentWizardBasicDescriptionRoleHintAndColorNavigationStayReadable) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        auto draft = std::make_shared<agent_wizard::WizardDraft>();
        draft->name = "WizardAgent";
        draft->description = "WizardDescription";
        auto component = agent_wizard::StepBasic(draft);
        const auto screen = Paint(component->Render(), 160, 50, pal.background);
        const auto name_background = variant == th::ThemeVariant::Monochrome
                                         ? pal.message_actions_background
                                         : pal.surface_tint_2;
        ExpectWord(screen, "WizardAgent", pal.info, name_background, IsRgb(variant));
        ExpectWord(screen, "WizardDescription", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "select below", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "switch palette slot", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "auto", pal.muted, pal.background, IsRgb(variant));
    }
}

TEST(TextContrastControls,
     AgentWizardToolsAndPathSummariesKeepCurrentEmptyAndOverflowMetadataReadable) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        auto draft = std::make_shared<agent_wizard::WizardDraft>();
        draft->path_allowlist = {"first", "second", "third", "fourth", "fifth"};
        auto component = agent_wizard::StepTools(draft);
        const auto screen = Paint(component->Render(), 160, 60, pal.background);
        ExpectWord(screen, "none selected", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "empty", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "more", pal.muted, pal.background, IsRgb(variant));
    }
}

TEST(TextContrastControls, AgentWizardModelTemperatureAndPromptCountsRemainReadable) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        auto draft = std::make_shared<agent_wizard::WizardDraft>();
        draft->model = "CurrentModel";
        draft->system_prompt = "PromptBody";
        auto component = agent_wizard::StepModelPrompt(draft);
        const auto screen = Paint(component->Render(), 160, 50, pal.background);
        ExpectWord(screen, "Selected:", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "deterministic", pal.muted, pal.background, IsRgb(variant));
        ExpectWord(screen, "chars", pal.muted, pal.background, IsRgb(variant));
    }
}

TEST(TextContrastControls, AgentWizardPermissionChoicesSummaryAndReadinessRemainReadable) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        const bool ansi =
            variant == th::ThemeVariant::LightAnsi || variant == th::ThemeVariant::DarkAnsi;
        for (const bool ready : {false, true}) {
            auto draft = std::make_shared<agent_wizard::WizardDraft>();
            draft->name = "ConfiguredAgent";
            draft->description = "ConfiguredDescription";
            draft->model = "ConfiguredModel";
            draft->confirmed = ready;
            auto component = agent_wizard::StepPermissions(draft);
            const auto screen = Paint(component->Render(), 160, 50, pal.background);
            for (const auto word : {"Auto-allow", "Auto-deny", "Name:", "Description:", "Prompt:",
                                    "chars", "reviewed", "toggle confirm"})
                ExpectWord(screen, word, pal.muted, pal.background, IsRgb(variant));
            ExpectWord(screen, "ConfiguredDescription", pal.text, pal.background, IsRgb(variant));
            ExpectWord(screen, "Ask (default)", pal.warning, pal.background, IsRgb(variant));
            if (ready) {
                ExpectWord(screen, "Ready to create", pal.success,
                           ansi ? pal.background : pal.success_tint_bg, IsRgb(variant));
            } else {
                ExpectWord(screen, "Tick the confirmation", pal.warning,
                           ansi ? pal.background : pal.warning_tint_bg, IsRgb(variant));
            }
        }
    }
}
