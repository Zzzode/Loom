/// @file test_text_contrast_dialogs.cpp
/// @brief Readable required text in active dialogs and Doctor renderers.
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.foundation.theme_provider;
import loom.config.config;
import loom.ui.foundation.design_tokens;
import loom.ui.dialogs.trust_dialog;
import loom.ui.dialogs.trust_utils;
import loom.ui.dialogs.settings_dialog;
import loom.ui.dialogs.frame;
import loom.ui.dialogs.system;
import loom.ui.dialogs.default_renderers;
import loom.ui.dialogs.elicitation;
import loom.ui.dialogs.session_picker;
import loom.ui.screens.doctor_screen;

namespace {
namespace theme = loom::ui::design::theme;
namespace trust = loom::ui::trust_dialog;
namespace risk = loom::ui::trust_utils;
namespace settings = loom::ui::dialogs::settings_dialog;
namespace doctor = loom::ui::doctor_screen;
using namespace ftxui;
using namespace std::chrono_literals;
constexpr std::array kThemes{
    theme::ThemeVariant::Dark,           theme::ThemeVariant::Light,
    theme::ThemeVariant::DarkDaltonized, theme::ThemeVariant::LightDaltonized,
    theme::ThemeVariant::Monochrome,     theme::ThemeVariant::LightAnsi,
    theme::ThemeVariant::DarkAnsi};

struct RestoreTheme {
    theme::Theme original = theme::current_theme();

    ~RestoreTheme() { theme::set_theme(original); }
};

Screen Paint(Element element) {
    auto screen = Screen::Create(Dimension::Fixed(160), Dimension::Fixed(80));
    Render(screen, std::move(element));
    return screen;
}

std::optional<std::pair<int, int>> Find(const Screen& screen, std::string_view word) {
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
            if (match) return std::pair{x, y};
        }
    }
    return std::nullopt;
}

std::optional<std::array<int, 3>> Rgb(Color color) {
    const auto sgr = color.Print(false);
    if (!sgr.starts_with("38;2;")) return std::nullopt;
    std::array<int, 3> channels{};
    auto cursor = sgr.data() + 5;
    const auto end = sgr.data() + sgr.size();
    for (std::size_t i = 0; i < channels.size(); ++i) {
        const auto parsed = std::from_chars(cursor, end, channels[i]);
        if (parsed.ec != std::errc{} || channels[i] < 0 || channels[i] > 255) return std::nullopt;
        cursor = parsed.ptr;
        if (i + 1 < channels.size() && (cursor == end || *cursor++ != ';')) return std::nullopt;
    }
    return cursor == end ? std::optional(channels) : std::nullopt;
}

double Luminance(const std::array<int, 3>& rgb) {
    const auto linear = [](int channel) {
        const auto value = channel / 255.0;
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return linear(rgb[0]) * 0.2126 + linear(rgb[1]) * 0.7152 + linear(rgb[2]) * 0.0722;
}

void Readable(const Screen& screen, std::string_view word, Color foreground,
              std::optional<Color> background = std::nullopt) {
    SCOPED_TRACE(word);
    const auto location = Find(screen, word);
    ASSERT_TRUE(location.has_value()) << screen.ToString();
    const auto [x, y] = *location;
    for (std::size_t i = 0; i < word.size(); ++i) {
        const auto& pixel = screen.PixelAt(x + static_cast<int>(i), y);
        EXPECT_EQ(pixel.foreground_color, foreground);
        EXPECT_FALSE(pixel.dim);
        if (background) EXPECT_EQ(pixel.background_color, *background);
        // Transparent rows are certified against the palette's declared
        // canvas reference. An unknown physical/ANSI canvas is not certified.
        const auto resolved_background = pixel.background_color == Color::Default
                                             ? theme::current_theme().palette->background
                                             : pixel.background_color;
        EXPECT_NE(pixel.foreground_color, resolved_background);
        const auto variant = theme::current_theme().variant;
        const bool rgb_variant = variant == theme::ThemeVariant::Dark ||
                                 variant == theme::ThemeVariant::Light ||
                                 variant == theme::ThemeVariant::DarkDaltonized ||
                                 variant == theme::ThemeVariant::LightDaltonized;
        if (rgb_variant) {
            const auto fg = Rgb(pixel.foreground_color);
            const auto bg = Rgb(resolved_background);
            ASSERT_TRUE(fg.has_value()) << pixel.foreground_color.Print(false);
            ASSERT_TRUE(bg.has_value()) << resolved_background.Print(false);
            const auto a = Luminance(*fg);
            const auto b = Luminance(*bg);
            EXPECT_GE((std::max(a, b) + 0.05) / (std::min(a, b) + 0.05), 4.5);
        }
    }
}

std::shared_ptr<trust::DialogState> TrustState() {
    auto state = std::make_shared<trust::DialogState>();
    state->props.forced_level = risk::RiskLevel::Critical;
    state->props.workspace_path_storage = std::make_shared<std::string>("/tmp/workspace");
    state->props.summary.action_summary = "Read private credentials";
    state->props.summary.risk_factors = {"External account access"};
    state->props.summary.sensitive_paths = {{"/tmp/SECRET", "Credentials",
                                             risk::RiskLevel::Critical,
                                             "Sensitive credential explanation"}};
    state->props.summary.domains = {"external.example"};
    state->show_second_confirm = true;
    return state;
}

doctor::DoctorDataModel DoctorModel() {
    doctor::DoctorDataModel model;
    model.version.current_version = "test-version";
    model.version.invoked_binary = "loom-diagnostic";
    model.version.installation_path = "/tmp/loom-install";
    auto& result = model.results.front();
    result.run_state = doctor::CheckRunState::Done;
    result.severity = doctor::DiagnosticSeverity::Error;
    result.message = "Credential probe failed";
    result.detail = "Required diagnostic explanation";
    result.fix_hint = "Repair the credential";
    result.fix_command = "loom configure";
    result.elapsed = 42ms;
    return model;
}
}  // namespace

TEST(DialogTextContrast, SharedFrameSubtitleRemainsReadableInPanelAndModalLayouts) {
    RestoreTheme restore;
    namespace frame = loom::ui::dialogs::frame;
    for (auto variant : kThemes) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& current = theme::current_theme();
        for (auto pane : {frame::PaneVariant::PanelPadded, frame::PaneVariant::ModalMinimal}) {
            frame::DialogFrameProps props;
            props.title = "Current request";
            props.subtitle = "Required request metadata";
            props.pane_variant = pane;
            props.content = text("Request body");
            const auto screen = Paint(frame::DialogFrame(props, current));
            Readable(screen, "Required request metadata", current.palette->muted);
        }
    }
}

TEST(DialogTextContrast, ActualLastRegisteredMcpElicitationKeepsRequestAndCancelReadable) {
    RestoreTheme restore;
    namespace dsys = loom::ui::dialogs::system;
    for (auto variant : kThemes) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& current = theme::current_theme();
        const auto& pal = *current.palette;
        dsys::DialogRendererRegistry registry;
        // Match AppAdapter's precedence: the faithful renderer is last.
        loom::ui::dialogs::default_renderers::register_default_renderers(registry);
        loom::ui::dialogs::elicitation::RegisterElicitationDialog(registry);
        for (const auto& server : {std::string("Current MCP"), std::string()}) {
            int approved = 0;
            int denied = 0;
            int canceled = 0;
            dsys::ElicitationPayload request;
            request.server_name = server;
            request.request_id = 731;
            request.request_description = "Required request explanation";
            request.on_response = [&](bool allow) { allow ? ++approved : ++denied; };
            request.on_cancel = [&] { ++canceled; };
            dsys::DialogPayloadVariant payload = request;
            dsys::DialogRenderContext context;
            context.theme = current;
            const auto screen = Paint(registry.render(payload, context));
            Readable(screen, "needs additional input", pal.muted);
            // This sentence and y/n shortcuts distinguish the actual override.
            Readable(screen, "Allow connecting to", pal.text);
            Readable(screen, "Required request explanation", pal.text);
            Readable(screen, "Request ID: 731", pal.muted);
            Readable(screen, "[y] Allow", pal.success);
            Readable(screen, "[n] Deny", pal.danger);
            Readable(screen, "[Esc] Cancel", pal.muted);
            EXPECT_EQ(approved + denied + canceled, 0);
            EXPECT_TRUE(registry.handle_event(payload, Event::Character('y')));
            EXPECT_TRUE(registry.handle_event(payload, Event::Character('n')));
            EXPECT_TRUE(registry.handle_event(payload, Event::Escape));
            EXPECT_EQ(approved, 1);
            EXPECT_EQ(denied, 1);
            EXPECT_EQ(canceled, 1);
            EXPECT_EQ(std::get<dsys::ElicitationPayload>(payload).request_description,
                      request.request_description);
        }
    }
}

TEST(DialogTextContrast, ActualResumePickerKeepsSessionMetadataSearchAndSelectionReadable) {
    RestoreTheme restore;
    namespace dsys = loom::ui::dialogs::system;
    for (auto variant : kThemes) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& current = theme::current_theme();
        const auto& pal = *current.palette;
        dsys::SessionPickerPayload picker;
        std::string chosen;
        picker.on_select = [&](const std::string& id) { chosen = id; };
        for (int i = 0; i < 24; ++i) {
            dsys::SessionPickerEntry entry;
            entry.session_id = "session" + std::to_string(i);
            entry.title = "Choice" + std::to_string(i);
            entry.age_string = "age" + std::to_string(i);
            entry.message_count = 100 + i;
            entry.cwd = "/project" + std::to_string(i);
            picker.sessions.push_back(std::move(entry));
        }
        dsys::DialogPayloadVariant payload = picker;
        dsys::DialogRendererRegistry registry;
        loom::ui::dialogs::default_renderers::register_default_renderers(registry);
        dsys::DialogRenderContext context;
        context.theme = current;
        const auto screen = Paint(registry.render(payload, context));
        Readable(screen, "Select a conversation to resume", pal.muted);
        Readable(screen, "Type to search sessions...", pal.muted);
        Readable(screen, "24 sessions", pal.muted);
        // The existing picker height budget clips the lower indicator in a
        // 24-row frame. Certify visible text here; the separate layout defect
        // must not be hidden by changing dimensions in this contrast test.
        Readable(screen, "Choice0", pal.text, pal.message_actions_background);
        for (auto word : {"age0", "100 msgs", "/project0"}) {
            Readable(screen, word, pal.muted, pal.message_actions_background);
        }
        for (auto word : {"age1", "101 msgs", "/project1"})
            Readable(screen, word, pal.muted);
        auto& state = std::get<dsys::SessionPickerPayload>(payload);
        state.selected_index = 17;
        Readable(Paint(registry.render(payload, context)), "2 more", pal.muted);
        EXPECT_EQ(state.selected_index, 17);
        EXPECT_EQ(state.sessions.size(), 24u);
        state.query = "Choice23";
        state.selected_index = 0;
        const auto filtered = Paint(registry.render(payload, context));
        Readable(filtered, "1 sessions", pal.muted);
        Readable(filtered, "age23", pal.muted, pal.message_actions_background);
        EXPECT_TRUE(registry.handle_event(payload, Event::Return));
        EXPECT_EQ(chosen, "session23");
        state.query = "missing";
        const auto empty = Paint(registry.render(payload, context));
        Readable(empty, "No sessions found", pal.muted);
        Readable(empty, "0 sessions", pal.muted);
        EXPECT_TRUE(registry.handle_event(payload, Event::Escape));
        EXPECT_TRUE(chosen.empty());
    }
}

TEST(DialogTextContrast, ActualTrustConsentDetailsAndCriticalInstructionsStayReadable) {
    RestoreTheme restore;
    for (auto variant : kThemes) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        auto state = TrustState();
        const auto screen = Paint(trust::RenderTrustDialogFull(state));
        Readable(screen, "Sensitive paths detected", pal.danger);
        Readable(screen, "/tmp/SECRET", pal.text);
        Readable(screen, "Sensitive credential explanation", pal.muted);
        Readable(screen, "External account access", pal.text);
        Readable(screen, "external.example", pal.text);
        Readable(screen, "Loom'll be able", pal.muted);
        Readable(screen, "Typing anything other than YES", pal.muted);
        Readable(screen, "Security guide", pal.info);
        // The current trust frame clips the footer suffix. Certify the
        // visible action word; this contrast repair does not change layout.
        Readable(screen, "toggle", pal.muted);
        EXPECT_TRUE(state->show_details);
        EXPECT_FALSE(state->understand_risk);
        EXPECT_TRUE(state->yes_input.empty());
    }
}

TEST(DialogTextContrast, TrustSelectedAndDisabledActionsRetainTheirStates) {
    RestoreTheme restore;
    for (auto variant : kThemes) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        auto state = TrustState();
        state->props.forced_level = risk::RiskLevel::Medium;
        state->show_details = false;
        const auto screen = Paint(trust::RenderTrustDialogFull(state));
        Readable(screen, "Allow once", pal.warning);
        Readable(screen, "Remember this decision", pal.muted);
        Readable(screen, "Press [D] to show details", pal.muted);
        const auto disabled =
            Paint(trust::RenderButtonRow({"Allow once", "Cancel"}, 0, risk::RiskLevel::High, true));
        const auto at = Find(disabled, "Allow once");
        ASSERT_TRUE(at.has_value());
        EXPECT_TRUE(disabled.PixelAt(at->first, at->second).dim);
        state->countdown_active = true;
        state->countdown_remaining = 0;
        Readable(Paint(trust::RenderCountdown(*state)), "You may now proceed", pal.success);
        EXPECT_FALSE(state->always_allow);
        EXPECT_EQ(state->selected, 0);
    }
}

TEST(DialogTextContrast, EnabledSettingsDescriptionsNavigationAndDirtyErrorsAreReadable) {
    RestoreTheme restore;
    for (auto variant : kThemes) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        settings::WorkingSettings working;
        const auto permissions = Paint(settings::RenderPermissionsTab(working, 0));
        Readable(permissions, "Default behaviour when tools request access", pal.muted);
        Readable(permissions, "Permit shell command execution", pal.muted);
        Readable(permissions, "Permit file creation and modification", pal.muted);
        Readable(Paint(settings::RenderSidebar(settings::SettingsTabId::General)), "Permissions",
                 pal.muted);
        const auto disabled =
            Paint(settings::SettingRow("Unavailable", text("none"), "Disabled explanation", true));
        const auto at = Find(disabled, "Disabled explanation");
        ASSERT_TRUE(at.has_value());
        EXPECT_TRUE(disabled.PixelAt(at->first, at->second).dim);
        settings::ToastState toast;
        toast.message = "WRITE FAILED";
        toast.semantic_role = loom::ui::design::tokens::Role::Danger;
        toast.until = std::chrono::steady_clock::now() + 1h;
        const auto footer = Paint(settings::RenderFooter(toast, true));
        Readable(footer, "unsaved changes", pal.warning);
        Readable(footer, "WRITE FAILED", pal.danger);
        const auto clean = Paint(settings::RenderFooter({}, false));
        const auto save = Find(clean, "Save");
        ASSERT_TRUE(save.has_value());
        EXPECT_TRUE(clean.PixelAt(save->first, save->second).dim);
    }
}

TEST(DialogTextContrast, SelectedSettingsMcpMetadataHasReadableFilledForegroundPairs) {
    RestoreTheme restore;
    for (auto variant : kThemes) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        settings::WorkingSettings working;
        working.mcp_servers.emplace_back();
        working.mcp_servers.front().name = "tool-server";
        working.mcp_servers.front().transport = "stdio";
        working.mcp_servers.front().command = "tool-runner";
        const auto selected = Paint(settings::RenderMCPTab(working, 1, 0));
        const auto selected_bg = variant == theme::ThemeVariant::Monochrome
                                     ? pal.message_actions_background
                                     : pal.surface_tint_2;
        Readable(selected, "stdio", pal.muted, selected_bg);
        Readable(selected, "tool-runner", pal.info, selected_bg);
        const auto ordinary = Paint(settings::RenderMCPTab(working, 1, -1));
        Readable(ordinary, "stdio", pal.muted);
        Readable(ordinary, "tool-runner", pal.info);
        working.mcp_servers.clear();
        Readable(Paint(settings::RenderMCPTab(working, 1, -1)), "use /mcp add to configure",
                 pal.muted);
    }
}

TEST(DialogTextContrast, ActualRunningDoctorKeepsDiagnosticMetadataAndPendingStatusReadable) {
    RestoreTheme restore;
    for (auto variant : kThemes) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        const auto model = DoctorModel();
        const auto screen =
            Paint(doctor::RenderRunningState(model, 0, 1, static_cast<doctor::CheckId>(1)));
        Readable(screen, "Credential probe failed", pal.danger);
        Readable(screen, "loom-diagnostic", pal.muted);
        Readable(screen, "/tmp/loom-install", pal.muted);
        Readable(screen, "queued", pal.muted);
        EXPECT_EQ(model.results.front().run_state, doctor::CheckRunState::Done);
        EXPECT_EQ(model.results.front().severity, doctor::DiagnosticSeverity::Error);
    }
}

TEST(DialogTextContrast, ExpandedDoctorResultsPreserveRequiredFailureReasonsAndActions) {
    RestoreTheme restore;
    for (auto variant : kThemes) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        const auto model = DoctorModel();
        std::array<bool, doctor::kCheckCount> expanded{};
        expanded.front() = true;
        const auto screen = Paint(doctor::RenderResultsState(model, expanded));
        Readable(screen, "Credential probe failed", pal.danger);
        Readable(screen, "Required diagnostic explanation", pal.muted);
        Readable(screen, "Repair the credential", pal.warning);
        Readable(screen, "loom configure", pal.success);
        Readable(screen, "42ms", pal.muted);
        Readable(screen, "proceed to summary", pal.muted);
        EXPECT_TRUE(expanded.front());
    }
}

TEST(DialogTextContrast, DoctorSummaryRetainsReadableActiveLockAndNavigationInformation) {
    RestoreTheme restore;
    for (auto variant : kThemes) {
        SCOPED_TRACE(theme::variant_name(variant));
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        auto model = DoctorModel();
        model.locks.enabled = true;
        model.locks.lock_count = 2;
        const auto screen = Paint(doctor::RenderSummaryState(model, {}, 0));
        Readable(screen, "2 active", pal.muted);
        Readable(screen, "navigate", pal.muted);
        Readable(screen, "activate", pal.muted);
        EXPECT_EQ(model.locks.lock_count, 2);
    }
}

TEST(DialogTextContrast, ActualSettingsWindowClearsOnlyItsOwnComposedSurfaceAndKeepsEvents) {
    RestoreTheme restore;
    for (auto variant : kThemes)
        for (const int columns : {40, 80}) {
            SCOPED_TRACE(std::format("theme={},columns={}", theme::variant_name(variant), columns));
            theme::set_theme(variant);
            const auto& pal = *theme::current_theme().palette;
            loom::core::ConfigManager config;
            int closed = 0;
            settings::SettingsDialogOptions options;
            options.on_close = [&](std::optional<std::string>, settings::CommandResultDisplay) {
                ++closed;
            };
            auto component = settings::MakeSettingsDialog(config, std::move(options));
            Box actual;
            auto paint = [&](bool transcript) {
                Elements rows;
                for (int row = 0; row < 34; ++row)
                    rows.push_back(text(std::string(columns + 8, transcript ? '~' : ' ')));
                auto screen = Screen::Create(Dimension::Fixed(columns + 8), Dimension::Fixed(34));
                auto panel = component->Render() | size(WIDTH, EQUAL, columns) |
                             size(HEIGHT, EQUAL, 30) | reflect(actual);
                Render(screen, dbox({vbox(std::move(rows)), std::move(panel) | center}));
                return screen;
            };
            const auto clean = paint(false);
            const auto composed = paint(true);
            ASSERT_GE(actual.x_min, 0);
            ASSERT_LT(actual.x_max, composed.dimx());
            ASSERT_GE(actual.y_min, 0);
            ASSERT_LT(actual.y_max, composed.dimy());
            int blanks = 0;
            for (int y = actual.y_min; y <= actual.y_max; ++y)
                for (int x = actual.x_min; x <= actual.x_max; ++x) {
                    const auto& expected = clean.PixelAt(x, y);
                    const auto& pixel = composed.PixelAt(x, y);
                    ASSERT_EQ(pixel.character, expected.character) << "cell " << x << "," << y;
                    EXPECT_EQ(pixel.foreground_color, expected.foreground_color);
                    EXPECT_EQ(pixel.background_color, expected.background_color);
                    EXPECT_EQ(pixel.dim, expected.dim);
                    EXPECT_EQ(pixel.bold, expected.bold);
                    EXPECT_EQ(pixel.inverted, expected.inverted);
                    EXPECT_NE(pixel.character, "~");
                    if (pixel.character == " ") {
                        ++blanks;
                        EXPECT_EQ(pixel.background_color, pal.background);
                    }
                }
            EXPECT_GT(blanks, 0);
            EXPECT_EQ(composed.PixelAt(0, 0).character, "~");
            EXPECT_TRUE(Find(clean, "General").has_value());
            EXPECT_TRUE(component->OnEvent(Event::ArrowRight));
            const auto changed = paint(false);
            EXPECT_TRUE(Find(changed, "Model").has_value());
            EXPECT_NE(clean.ToString(), changed.ToString());
            EXPECT_TRUE(component->OnEvent(Event::Escape));
            EXPECT_EQ(closed, 1);
        }
}
