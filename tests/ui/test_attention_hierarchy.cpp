/// @file test_attention_hierarchy.cpp
/// @brief State ownership, readable ink and style-only chain contracts (RFC 0015).
#include <cstdlib>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.dialogs.system;
import loom.ui.dialogs.default_renderers;
import loom.ui.foundation.clock;
import loom.ui.foundation.design_tokens;
import loom.ui.foundation.design_figures;
import loom.ui.foundation.theme_provider;
import loom.ui.messages.messages_list;
import loom.ui.messages.message_row;
import loom.ui.messages.tool_use_message;
import loom.ui.messages.api_error_message;
import loom.ui.permissions.single_prompt;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.screens.messages_store;
import loom.ui.screens.task_view_store;
import loom.ui.widgets.spinner;

namespace {
namespace ds = loom::ui::dialogs::system;
namespace dr = loom::ui::dialogs::default_renderers;
namespace rs = loom::ui::repl_screen;
namespace dq = rs::dialog_queue_render;
namespace ml = loom::ui::messages_list;
namespace msg = loom::ui::messages;
namespace sp = loom::ui::permissions::single_prompt;
namespace ae = msg::api_error_message;
namespace tool = msg::tool_use_message;
namespace theme = loom::ui::design::theme;
namespace figures = loom::ui::design::figures;
using namespace std::chrono_literals;

constexpr auto kThemes = std::array{
    theme::ThemeVariant::Dark, theme::ThemeVariant::Light,
    theme::ThemeVariant::DarkDaltonized, theme::ThemeVariant::LightDaltonized,
    theme::ThemeVariant::Monochrome, theme::ThemeVariant::DarkAnsi,
    theme::ThemeVariant::LightAnsi};

struct ThemeGuard {
    theme::Theme original = theme::current_theme();
    ~ThemeGuard() { theme::set_theme(original); }
};
struct ClockGuard {
    ~ClockGuard() { loom::ui::clock::set_steady_now_for_testing(std::nullopt); }
    void at(std::chrono::milliseconds value) {
        loom::ui::clock::set_steady_now_for_testing(
            std::chrono::steady_clock::time_point(value));
    }
};
struct EnvGuard {
    std::string name;
    std::optional<std::string> original;
    EnvGuard(std::string key, std::string value) : name(std::move(key)) {
        if (const auto* old = std::getenv(name.c_str())) original = old;
        setenv(name.c_str(), value.c_str(), 1);
    }
    ~EnvGuard() {
        if (original) setenv(name.c_str(), original->c_str(), 1);
        else unsetenv(name.c_str());
    }
};

ftxui::Screen Paint(ftxui::Element element, int width = 120, int height = 40) {
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, std::move(element));
    return screen;
}
std::optional<std::pair<int, int>> FindWord(
    const ftxui::Screen& screen, std::string_view word) {
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x + static_cast<int>(word.size()) <= screen.dimx(); ++x) {
            bool matches = true;
            for (std::size_t i = 0; i < word.size(); ++i) {
                if (screen.PixelAt(x + static_cast<int>(i), y).character
                        != word.substr(i, 1)) { matches = false; break; }
            }
            if (matches) return std::pair{x, y};
        }
    }
    return std::nullopt;
}
std::optional<std::array<int, 3>> Rgb(ftxui::Color color) {
    const auto value = color.Print(false);
    if (!value.starts_with("38;2;")) return std::nullopt;
    std::array<int, 3> rgb{};
    auto cursor = value.data() + 5;
    const auto end = value.data() + value.size();
    for (std::size_t i = 0; i < rgb.size(); ++i) {
        const auto parsed = std::from_chars(cursor, end, rgb[i]);
        if (parsed.ec != std::errc{}) return std::nullopt;
        cursor = parsed.ptr;
        if (i + 1 < rgb.size() && (cursor == end || *cursor++ != ';'))
            return std::nullopt;
    }
    return cursor == end ? std::optional(rgb) : std::nullopt;
}
double Luminance(const std::array<int, 3>& rgb) {
    const auto linear = [](int channel) {
        const double value = channel / 255.0;
        return value <= 0.04045 ? value / 12.92
            : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return linear(rgb[0]) * 0.2126 + linear(rgb[1]) * 0.7152 + linear(rgb[2]) * 0.0722;
}
void ExpectInk(const ftxui::Screen& screen, std::string_view word,
               ftxui::Color ink, bool bold = false) {
    SCOPED_TRACE(word);
    const auto position = FindWord(screen, word);
    ASSERT_TRUE(position.has_value()) << screen.ToString();
    const auto [x, y] = *position;
    for (std::size_t i = 0; i < word.size(); ++i) {
        const auto& pixel = screen.PixelAt(x + static_cast<int>(i), y);
        EXPECT_EQ(pixel.foreground_color, ink);
        EXPECT_EQ(pixel.bold, bold);
        EXPECT_FALSE(pixel.dim);
        const auto fg = Rgb(pixel.foreground_color);
        const auto bg = Rgb(pixel.background_color == ftxui::Color::Default
            ? theme::current_theme().palette->background : pixel.background_color);
        if (fg && bg) {
            const auto a = Luminance(*fg);
            const auto b = Luminance(*bg);
            EXPECT_GE((std::max(a, b) + 0.05) / (std::min(a, b) + 0.05), 4.5);
        }
    }
}
std::vector<std::string> CellText(const ftxui::Screen& screen) {
    std::vector<std::string> rows;
    for (int y = 0; y < screen.dimy(); ++y) {
        std::string row;
        for (int x = 0; x < screen.dimx(); ++x) row += screen.PixelAt(x, y).character;
        rows.push_back(std::move(row));
    }
    return rows;
}
ds::ToolPermissionPayload Permission(std::function<void(sp::Decision, bool)> callback = {}) {
    ds::ToolPermissionPayload request;
    request.id = "attention-permission";
    request.tool_name = "Bash";
    request.action_kind = sp::ActionKind::Execute;
    request.description = "Approve command";
    request.detail = sp::DetailBash{"printf attention"};
    request.on_response = std::move(callback);
    return request;
}
void RegisterDecisionSpy(rs::ReplScreenState& state) {
    state.dialog_store.dialog_renderers.register_dialog(ds::DialogType::ToolPermission,
        [](ds::DialogPayloadVariant&, const ds::DialogRenderContext&) {
            return ftxui::text("DECISION-PANEL");
        }, [](ds::DialogPayloadVariant& payload, const ftxui::Event& event) {
            if (event != ftxui::Event::Character('y')) return false;
            auto& request = std::get<ds::ToolPermissionPayload>(payload);
            if (request.on_response) request.on_response(sp::Decision::AllowOnce, false);
            return true;
        });
}
rs::MessageDisplayEntry RunningEntry() {
    rs::MessageDisplayEntry entry;
    entry.id = "attention-tool";
    entry.role = "assistant";
    entry.is_tool_use = true;
    entry.tool_name = "Bash";
    entry.tool_status = "running";
    entry.tool_use_id = "attention-tool-call";
    entry.tool_input_json = R"({"command":"printf target"})";
    entry.tool_start_time = std::chrono::steady_clock::time_point(100s);
    return entry;
}
rs::ReplScreenState WorkingState() {
    rs::ReplScreenState state;
    state.cwd = "/tmp/attention";
    state.query_running = true;
    state.messages_store.messages.push_back(RunningEntry());
    state.task_view_store.spinner_mode = rs::SpinnerMode::ToolUse;
    state.task_view_store.spinner_verb = "BOTTOM-ACTIVITY";
    return state;
}
ftxui::Screen PaintRepl(rs::ReplScreenState& state, int width = 120) {
    EnvGuard columns("COLUMNS", std::to_string(width));
    EnvGuard lines("LINES", "40");
    EnvGuard stable("LOOM_NO_FLICKER", "1");
    (void)Paint(rs::RenderReplScreen(state), width);
    return Paint(rs::RenderReplScreen(state), width);
}
ml::MessagesListInput LiveChain() {
    tool::ToolUseRenderOptions call;
    call.call.tool_name = "Read";
    call.call.status = tool::ToolStatus::Running;
    call.call.raw_parameters = R"({"file_path":"src/attention.cpp"})";
    call.call.start_time = std::chrono::steady_clock::time_point(100s);
    ml::MessagesListInput input;
    input.rows.push_back(call);
    input.shapes.push_back(msg::MessageShape::AssistantToolUse);
    input.uuids.push_back("attention-stage");
    input.streaming_tail_row = input.rows.size();
    input.query_running = true;
    return input;
}
std::vector<ml::VisibleRow> Chains(const ml::MessagesListInput& input) {
    auto rows = ml::build_visible_rows(input);
    std::erase_if(rows, [](const auto& row) {
        return row.kind != ml::VisibleRow::Kind::CompressedChain;
    });
    return rows;
}
void ExpectSameLifecycle(const ml::MessagesListInput& left, const ml::MessagesListInput& right) {
    EXPECT_EQ(left.chain_update_due, right.chain_update_due);
    EXPECT_EQ(left.expanded_keys, right.expanded_keys);
    ASSERT_EQ(left.chain_timings.size(), right.chain_timings.size());
    for (const auto& [key, value] : left.chain_timings) {
        const auto found = right.chain_timings.find(key);
        ASSERT_NE(found, right.chain_timings.end());
        EXPECT_EQ(value.started_at, found->second.started_at);
        EXPECT_EQ(value.elapsed, found->second.elapsed);
        EXPECT_EQ(value.finished, found->second.finished);
        EXPECT_EQ(value.disclosure_id, found->second.disclosure_id);
    }
    const auto a = Chains(left);
    const auto b = Chains(right);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].chain_elapsed, b[i].chain_elapsed);
        EXPECT_EQ(a[i].chain_current_activity, b[i].chain_current_activity);
        EXPECT_EQ(a[i].chain_uuid, b[i].chain_uuid);
        EXPECT_EQ(a[i].chain_is_expanded, b[i].chain_is_expanded);
        EXPECT_EQ(a[i].chain_is_live, b[i].chain_is_live);
        EXPECT_EQ(ml::detail::estimate_row_height(a[i], left, left.term_cols),
                  ml::detail::estimate_row_height(b[i], right, right.term_cols));
    }
}
} // namespace

TEST(AttentionHierarchy, PreparedLayersRenderExactlyOnceInExistingOrder) {
    rs::ReplScreenState state;
    std::vector<ds::DialogType> calls;
    for (const auto type : {ds::DialogType::SandboxPermission,
            ds::DialogType::ToolPermission, ds::DialogType::SettingsPanel}) {
        state.dialog_store.dialog_renderers.register_dialog(type,
            [&, type](ds::DialogPayloadVariant&, const ds::DialogRenderContext&) {
                calls.push_back(type);
                return ftxui::text(std::string(ds::dialog_type_name(type)));
            }, [](ds::DialogPayloadVariant&, const ftxui::Event&) { return true; });
    }
    ds::SandboxPermissionPayload sandbox;
    sandbox.id = "sandbox";
    sandbox.on_response = [](bool, bool) {};
    state.dialog_store.PushDialog(sandbox);
    state.dialog_store.PushDialog(Permission([](sp::Decision, bool) {}));
    ds::SettingsPanelPayload settings;
    settings.id = "settings";
    state.dialog_store.PushDialog(settings);
    auto prepared = dq::PrepareDialogLayers(state, false, true);
    EXPECT_EQ(calls, (std::vector{ds::DialogType::SandboxPermission,
        ds::DialogType::ToolPermission, ds::DialogType::SettingsPanel}));
    EXPECT_TRUE(prepared.bottom);
    EXPECT_TRUE(prepared.overlay);
    EXPECT_TRUE(prepared.modal);
    EXPECT_TRUE(prepared.takes_focus);
    EXPECT_FALSE(prepared.awaiting_permission);
    (void)Paint(dq::LayerAllDialogs(ftxui::text("base"), std::move(prepared)));
    EXPECT_EQ(calls.size(), 3u);
}

TEST(AttentionHierarchy, PermissionRequiresRendererHandlerCallbackAndRealElement) {
    for (const int scenario : {0, 1, 2, 3, 4, 5}) {
        SCOPED_TRACE(scenario);
        rs::ReplScreenState state;
        const bool callback = scenario != 3;
        state.dialog_store.PushDialog(Permission(callback
            ? std::function<void(sp::Decision, bool)>([](sp::Decision, bool) {}) : nullptr));
        if (scenario == 1 || scenario >= 3)
            state.dialog_store.dialog_renderers.register_renderer(ds::DialogType::ToolPermission,
                [scenario](ds::DialogPayloadVariant&, const ds::DialogRenderContext&) {
                    return scenario == 4 ? ftxui::Element{} : ftxui::text("permission");
                });
        if (scenario >= 2)
            state.dialog_store.dialog_renderers.register_event_handler(ds::DialogType::ToolPermission,
                [](ds::DialogPayloadVariant&, const ftxui::Event&) { return true; });
        const auto prepared = dq::PrepareDialogLayers(state, false, true);
        EXPECT_EQ(prepared.awaiting_permission, scenario == 5);
        EXPECT_EQ(prepared.takes_focus, scenario == 5);
    }
}

TEST(AttentionHierarchy, SuppressedQueuedPermissionNeverClaimsFocusOrRenders) {
    for (const bool typing : {false, true}) {
        rs::ReplScreenState state;
        int rendered = 0;
        state.dialog_store.PushDialog(Permission([](sp::Decision, bool) {}));
        state.dialog_store.dialog_renderers.register_dialog(ds::DialogType::ToolPermission,
            [&](ds::DialogPayloadVariant&, const ds::DialogRenderContext&) {
                ++rendered;
                return ftxui::text("permission");
            }, [](ds::DialogPayloadVariant&, const ftxui::Event&) { return true; });
        const auto prepared = dq::PrepareDialogLayers(state, typing, typing);
        EXPECT_FALSE(prepared.overlay);
        EXPECT_FALSE(prepared.takes_focus);
        EXPECT_FALSE(prepared.awaiting_permission);
        EXPECT_EQ(rendered, 0);
        EXPECT_TRUE(state.dialog_store.dialog_queue.contains_type(ds::DialogType::ToolPermission));
    }
}

TEST(AttentionHierarchy, RegisteredBottomPermissionsRequireTheirOwnCallback) {
    for (const bool worker : {false, true}) {
        SCOPED_TRACE(worker);
        rs::ReplScreenState state;
        const auto type = worker ? ds::DialogType::WorkerSandboxPermission
                                 : ds::DialogType::SandboxPermission;
        state.dialog_store.dialog_renderers.register_dialog(type,
            [](ds::DialogPayloadVariant&, const ds::DialogRenderContext&) {
                return ftxui::text("sandbox permission");
            }, [](ds::DialogPayloadVariant&, const ftxui::Event&) { return true; });
        if (worker) {
            ds::WorkerSandboxPermissionPayload request;
            request.id = "worker";
            state.dialog_store.PushDialog(request);
        } else {
            ds::SandboxPermissionPayload request;
            request.id = "sandbox";
            state.dialog_store.PushDialog(request);
        }
        EXPECT_FALSE(dq::PrepareDialogLayers(state, false, true).awaiting_permission);
        auto payload = state.dialog_store.dialog_queue.peek_bottom_mut(false, true);
        ASSERT_TRUE(payload.has_value());
        std::visit([](auto& request) {
            using T = std::decay_t<decltype(request)>;
            if constexpr (std::is_same_v<T, ds::SandboxPermissionPayload> ||
                          std::is_same_v<T, ds::WorkerSandboxPermissionPayload>)
                request.on_response = [](bool, bool) {};
        }, payload->get());
        const auto prepared = dq::PrepareDialogLayers(state, false, true);
        EXPECT_TRUE(prepared.bottom);
        EXPECT_TRUE(prepared.takes_focus);
        EXPECT_TRUE(prepared.awaiting_permission);
    }
}

TEST(AttentionHierarchy, SettingsOwnsDecisionKeysUntilItsActualComponentCloses) {
    EnvGuard columns("COLUMNS", "120");
    EnvGuard lines("LINES", "40");
    auto state = std::make_shared<rs::ReplScreenState>(WorkingState());
    int approvals = 0;
    state->dialog_store.PushDialog(Permission([&](sp::Decision, bool) { ++approvals; }));
    dr::register_default_renderers(state->dialog_store.dialog_renderers);
    state->mode = rs::ReplMode::SettingsView;
    auto settings = ftxui::Renderer([] { return ftxui::text("SETTINGS-OWNER"); })
        | ftxui::CatchEvent([state](const ftxui::Event& event) {
            if (event == ftxui::Event::Escape) state->mode = rs::ReplMode::Normal;
            return true;
        });
    state->dialog_store.settings_component = std::make_shared<ftxui::Component>(settings);
    int interrupts = 0, exits = 0, redraws = 0;
    rs::ReplScreenCallbacks callbacks;
    callbacks.on_interrupt = [&] { ++interrupts; };
    callbacks.on_exit = [&] { ++exits; };
    callbacks.on_redraw = [&] { ++redraws; };
    auto component = rs::ReplScreen(state, std::move(callbacks));
    const auto under_settings = Paint(component->Render());
    EXPECT_TRUE(FindWord(under_settings, "SETTINGS-OWNER"));
    EXPECT_FALSE(FindWord(under_settings, "BOTTOM-ACTIVITY"));
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('y')));
    EXPECT_EQ(approvals, 0);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('\x03')));
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('\x04')));
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('\x0C')));
    EXPECT_EQ(interrupts, 1);
    EXPECT_EQ(exits, 1);
    EXPECT_EQ(redraws, 1);
    EXPECT_EQ(approvals, 0);
    EXPECT_EQ(state->mode, rs::ReplMode::SettingsView);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Escape));
    EXPECT_EQ(state->mode, rs::ReplMode::Normal);
    EXPECT_TRUE(state->dialog_store.dialog_queue.contains_type(ds::DialogType::ToolPermission));
    (void)Paint(component->Render());
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('y')));
    EXPECT_EQ(approvals, 1);
}

TEST(AttentionHierarchy, AgentsAndTrustTakeoverCannotApproveHiddenQueue) {
    for (const auto mode : {rs::ReplMode::AgentsView, rs::ReplMode::TrustDialog}) {
        SCOPED_TRACE(static_cast<int>(mode));
        auto state = std::make_shared<rs::ReplScreenState>(WorkingState());
        state->mode = mode;
        int decisions = 0;
        int owned_events = 0;
        state->dialog_store.PushDialog(Permission([&](sp::Decision, bool) { ++decisions; }));
        RegisterDecisionSpy(*state);
        auto owner = ftxui::Renderer([] { return ftxui::text("OUTER-OWNER"); })
            | ftxui::CatchEvent([&](const ftxui::Event&) { ++owned_events; return true; });
        auto held = std::make_shared<ftxui::Component>(owner);
        if (mode == rs::ReplMode::AgentsView) state->dialog_store.agents_component = held;
        else state->dialog_store.wizard_trust = held;
        auto component = rs::ReplScreen(state, {});
        const auto screen = Paint(component->Render());
        EXPECT_TRUE(FindWord(screen, "OUTER-OWNER"));
        EXPECT_FALSE(FindWord(screen, "DECISION-PANEL"));
        EXPECT_FALSE(FindWord(screen, "BOTTOM-ACTIVITY"));
        EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('y')));
        EXPECT_EQ(owned_events, 1);
        EXPECT_EQ(decisions, 0);
        EXPECT_TRUE(state->dialog_store.dialog_queue.contains_type(ds::DialogType::ToolPermission));
    }
}

TEST(AttentionHierarchy, ActualReplPermissionQuietsWorkAndPreservesTargetAndDisclosure) {
    ThemeGuard restore;
    for (const auto variant : kThemes) {
    SCOPED_TRACE(static_cast<int>(variant));
    theme::set_theme(variant);
    ClockGuard clock;
    clock.at(110s);
    const auto& pal = *theme::current_theme().palette;
    for (const int width : {40, 80, 120}) {
        SCOPED_TRACE(width);
        auto state = WorkingState();
        const auto working = PaintRepl(state, width);
        ExpectInk(working, "Working", pal.primary, true);
        EXPECT_TRUE(FindWord(working, "BOTTOM-ACTIVITY"));
        RegisterDecisionSpy(state);
        state.dialog_store.PushDialog(Permission([](sp::Decision, bool) {}));
        const auto pending = PaintRepl(state, width);
        EXPECT_TRUE(FindWord(pending, "DECISION-PANEL"));
        EXPECT_FALSE(FindWord(pending, "BOTTOM-ACTIVITY"));
        ExpectInk(pending, "Working", pal.muted);
        EXPECT_TRUE(FindWord(pending, "printf target"));
        EXPECT_TRUE(FindWord(pending, "Show details"));
        EXPECT_EQ(state.messages_store.render_input.chain_update_due,
                  std::chrono::steady_clock::time_point(111s));
        state.dialog_store.PopOverlay();
        const auto resumed = PaintRepl(state, width);
        ExpectInk(resumed, "Working", pal.primary, true);
        EXPECT_TRUE(FindWord(resumed, "BOTTOM-ACTIVITY"));
    }
    }
}

TEST(AttentionHierarchy, IncompletePermissionControlsDoNotStealActualWorkFocus) {
    ThemeGuard restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    const auto& pal = *theme::current_theme().palette;
    for (const int scenario : {0, 1, 2, 3, 4}) {
        SCOPED_TRACE(scenario);
        auto state = WorkingState();
        state.dialog_store.PushDialog(Permission(scenario == 3 ? nullptr
            : std::function<void(sp::Decision, bool)>([](sp::Decision, bool) {})));
        if (scenario == 1 || scenario >= 3)
            state.dialog_store.dialog_renderers.register_renderer(ds::DialogType::ToolPermission,
                [scenario](ds::DialogPayloadVariant&, const ds::DialogRenderContext&) {
                    return scenario == 4 ? ftxui::Element{} : ftxui::text("placeholder");
                });
        if (scenario >= 2)
            state.dialog_store.dialog_renderers.register_event_handler(ds::DialogType::ToolPermission,
                [](ds::DialogPayloadVariant&, const ftxui::Event&) { return false; });
        const auto screen = PaintRepl(state);
        ExpectInk(screen, "Working", pal.primary, true);
        EXPECT_TRUE(FindWord(screen, "BOTTOM-ACTIVITY"));
    }
}

TEST(AttentionHierarchy, CurrentModelFailureOwnsRecoveryUntilNewQueryStarts) {
    auto state = WorkingState();
    state.query_running = false;
    rs::MessageDisplayEntry error;
    error.id = "current-model-error";
    error.role = "system";
    error.is_model_error = true;
    error.is_error = true;
    error.can_retry = true;
    error.content_preview = "Transport failed";
    state.messages_store.messages.push_back(error);
    const auto failed = PaintRepl(state);
    EXPECT_TRUE(FindWord(failed, "Model request failed"));
    EXPECT_TRUE(FindWord(failed, "Dismiss error"));
    EXPECT_FALSE(FindWord(failed, "BOTTOM-ACTIVITY"));
    state.query_running = true;
    const auto retrying = PaintRepl(state);
    EXPECT_TRUE(FindWord(retrying, "BOTTOM-ACTIVITY"));
    EXPECT_FALSE(FindWord(retrying, "Dismiss error"));
}

TEST(AttentionHierarchy, NormalCompletedStageIsQuietAndPromptRetainsUserText) {
    ThemeGuard restore;
    for (const auto variant : kThemes) {
    SCOPED_TRACE(static_cast<int>(variant));
    theme::set_theme(variant);
    ClockGuard clock;
    clock.at(110s);
    auto state = WorkingState();
    (void)PaintRepl(state);
    state.query_running = false;
    state.task_view_store.spinner_mode = rs::SpinnerMode::Hidden;
    state.input_text = "USER-PROMPT";
    const auto screen = PaintRepl(state);
    ExpectInk(screen, "Worked", theme::current_theme().palette->muted);
    EXPECT_TRUE(FindWord(screen, "USER-PROMPT"));
    EXPECT_FALSE(FindWord(screen, "BOTTOM-ACTIVITY"));
    EXPECT_FALSE(state.messages_store.render_input.chain_update_due);
    EXPECT_EQ(state.input_text, "USER-PROMPT");
    }
}

TEST(AttentionHierarchy, StandaloneTakeoverDoesNotRenderUnderlyingChromeOrPermission) {
    EnvGuard columns("COLUMNS", "80");
    EnvGuard lines("LINES", "40");
    auto state = WorkingState();
    int takeover_renders = 0;
    int hidden_renders = 0;
    state.dialog_store.dialog_renderers.register_renderer(ds::DialogType::TrustDialog,
        [&](ds::DialogPayloadVariant&, const ds::DialogRenderContext& context) {
            ++takeover_renders;
            EXPECT_EQ(context.term_cols, 80);
            return ftxui::text("STANDALONE-OWNER");
        });
    state.dialog_store.dialog_renderers.register_renderer(ds::DialogType::ToolPermission,
        [&](ds::DialogPayloadVariant&, const ds::DialogRenderContext&) {
            ++hidden_renders;
            return ftxui::text("HIDDEN-PERMISSION");
        });
    ds::TrustDialogPayload trust;
    trust.id = "standalone-owner";
    state.dialog_store.PushDialog(trust);
    state.dialog_store.PushDialog(Permission([](sp::Decision, bool) {}));
    const auto screen = Paint(rs::RenderReplScreen(state), 80);
    EXPECT_EQ(takeover_renders, 1);
    EXPECT_EQ(hidden_renders, 0);
    EXPECT_TRUE(FindWord(screen, "STANDALONE-OWNER"));
    EXPECT_FALSE(FindWord(screen, "BOTTOM-ACTIVITY"));
    EXPECT_FALSE(FindWord(screen, "Working"));
}

TEST(AttentionHierarchy, HistoricalToolFailureDoesNotOverrideCorrectedApproval) {
    auto state = WorkingState();
    rs::MessageDisplayEntry old_error;
    old_error.id = "old-tool-error";
    old_error.role = "tool";
    old_error.tool_name = "Bash";
    old_error.is_error = true;
    old_error.content_preview = "Previous command failed";
    state.messages_store.messages.insert(state.messages_store.messages.begin(), old_error);
    RegisterDecisionSpy(state);
    state.dialog_store.PushDialog(Permission([](sp::Decision, bool) {}));
    const auto pending = PaintRepl(state);
    EXPECT_TRUE(FindWord(pending, "DECISION-PANEL"));
    EXPECT_FALSE(FindWord(pending, "Model request failed"));
    EXPECT_FALSE(FindWord(pending, "BOTTOM-ACTIVITY"));
}

TEST(AttentionHierarchy, NonPermissionModalMasksUnderlyingPermissionAndWork) {
    auto state = WorkingState();
    RegisterDecisionSpy(state);
    state.dialog_store.PushDialog(Permission([](sp::Decision, bool) {}));
    state.dialog_store.dialog_renderers.register_renderer(ds::DialogType::SettingsPanel,
        [](ds::DialogPayloadVariant&, const ds::DialogRenderContext&) {
            return ftxui::text("MODAL-OWNER");
        });
    ds::SettingsPanelPayload modal;
    modal.id = "modal-owner";
    state.dialog_store.PushDialog(modal);
    const auto prepared = dq::PrepareDialogLayers(state, false, true);
    EXPECT_TRUE(prepared.takes_focus);
    EXPECT_FALSE(prepared.awaiting_permission);
    EXPECT_TRUE(prepared.overlay);
    EXPECT_TRUE(prepared.modal);
    const auto screen = PaintRepl(state);
    EXPECT_TRUE(FindWord(screen, "MODAL-OWNER"));
    EXPECT_FALSE(FindWord(screen, "BOTTOM-ACTIVITY"));
}

TEST(AttentionHierarchy, WorkEmphasisIsStyleOnlyForStaticAndVirtualAndExpansion) {
    ThemeGuard restore;
    for (const auto variant : kThemes) {
    SCOPED_TRACE(static_cast<int>(variant));
    theme::set_theme(variant);
    const auto& pal = *theme::current_theme().palette;
    ClockGuard clock;
    clock.at(110250ms);
    for (const int width : {40, 80, 120}) {
        for (const bool expanded : {false, true}) {
            for (const bool windowed : {false, true}) {
                SCOPED_TRACE(std::format("width={} expanded={} virtual={}", width, expanded, windowed));
                auto working = LiveChain();
                working.term_cols = width;
                const auto initial = Chains(working);
                ASSERT_EQ(initial.size(), 1u);
                if (expanded) working.expanded_keys.insert("chain:" + initial[0].chain_uuid);
                auto quiet = working;
                quiet.emphasize_work = false;
                const auto render = [&](const ml::MessagesListInput& input) {
                    return windowed ? ml::render_messages_list_virtual(input, 0, 20, 0)
                        : ml::render_messages_list_view(input, 0, 80, {}, true);
                };
                const auto loud_screen = Paint(render(working), width, 20);
                const auto quiet_screen = Paint(render(quiet), width, 20);
                EXPECT_EQ(CellText(loud_screen), CellText(quiet_screen));
                ExpectSameLifecycle(working, quiet);
                ExpectInk(loud_screen, "Working", pal.primary, true);
                const auto quiet_ink = pal.muted;
                ExpectInk(quiet_screen, "Working", quiet_ink);
                const auto heading = FindWord(loud_screen, "Working");
                ASSERT_TRUE(heading);
                if (expanded) {
                    EXPECT_EQ(loud_screen.PixelAt(heading->first, heading->second).background_color, pal.background);
                    EXPECT_EQ(quiet_screen.PixelAt(heading->first, heading->second).background_color, pal.background);
                }
                for (int y = 0; y < loud_screen.dimy(); ++y) {
                    for (int x = 0; x < loud_screen.dimx(); ++x) {
                        const auto& a = loud_screen.PixelAt(x, y);
                        const auto& b = quiet_screen.PixelAt(x, y);
                        EXPECT_EQ(a.background_color, b.background_color);
                        EXPECT_EQ(a.dim, b.dim);
                        EXPECT_EQ(a.inverted, b.inverted);
                        EXPECT_EQ(a.underlined, b.underlined);
                        if (a.foreground_color != b.foreground_color || a.bold != b.bold) {
                            EXPECT_EQ(y, heading->second);
                            EXPECT_GE(x, heading->first);
                            EXPECT_EQ(a.foreground_color, pal.primary);
                            EXPECT_EQ(b.foreground_color, quiet_ink);
                            EXPECT_TRUE(a.bold);
                            EXPECT_FALSE(b.bold);
                        }
                    }
                }
                EXPECT_EQ(working.chain_update_due, std::chrono::steady_clock::time_point(111s));
            }
        }
    }
    }
}

TEST(AttentionHierarchy, QuietAndEmphasizedChainsTickAndFreezeAtIdenticalTimes) {
    ClockGuard clock;
    clock.at(110s);
    auto working = LiveChain();
    const auto initial = Chains(working);
    ASSERT_EQ(initial.size(), 1u);
    auto quiet = working;
    quiet.emphasize_work = false;
    ExpectSameLifecycle(working, quiet);
    clock.at(112s);
    EXPECT_EQ(Chains(working)[0].chain_elapsed, 12s);
    EXPECT_EQ(Chains(quiet)[0].chain_elapsed, 12s);
    ExpectSameLifecycle(working, quiet);
    clock.at(115s);
    working.query_running = false;
    quiet.query_running = false;
    EXPECT_EQ(Chains(working)[0].chain_elapsed, 15s);
    EXPECT_EQ(Chains(quiet)[0].chain_elapsed, 15s);
    EXPECT_FALSE(working.chain_update_due);
    EXPECT_FALSE(quiet.chain_update_due);
    clock.at(300s);
    ExpectSameLifecycle(working, quiet);
    EXPECT_EQ(Chains(quiet)[0].chain_elapsed, 15s);
}

TEST(AttentionHierarchy, OrdinarySpinnersUseCanonicalFramesAndHonorThemeMotion) {
    ThemeGuard restore;
    for (const auto variant : kThemes) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        for (int frame = 0; frame < static_cast<int>(figures::kSpinnerFrames.size()); ++frame) {
            const auto screen = Paint(rs::RenderSpinner(rs::SpinnerMode::ToolUse,
                "Activity", "Readable tip", frame), 80, 1);
            EXPECT_EQ(screen.PixelAt(0, 0).character, figures::spinner_frame_glyph(frame));
            EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, pal.primary);
            EXPECT_FALSE(screen.PixelAt(0, 0).dim);
            ExpectInk(screen, "Activity", pal.muted);
            ExpectInk(screen, "Readable tip", pal.muted);
            auto widget = Paint(ui::components::SpinnerElement({.message = {}, .frame = frame}), 10, 1);
            EXPECT_EQ(widget.PixelAt(0, 0).character, figures::spinner_frame_glyph(frame));
        }
        auto reduced = theme::current_theme();
        reduced.a11y.reduced_motion = true;
        theme::set_theme(reduced);
        const auto first = Paint(rs::RenderSpinner(rs::SpinnerMode::ToolUse, "Activity", {}, 0), 80, 1);
        const auto later = Paint(rs::RenderSpinner(rs::SpinnerMode::ToolUse, "Activity", {}, 9), 80, 1);
        EXPECT_EQ(CellText(first), CellText(later));
        EXPECT_EQ(first.PixelAt(0, 0).character, figures::kBullet);
        for (const bool explicit_reduce : {false, true}) {
            auto widget = Paint(ui::components::SpinnerElement({
                .reduced_motion = explicit_reduce, .message = {}, .frame = 9}), 10, 1);
            EXPECT_EQ(widget.PixelAt(0, 0).character, figures::kBullet);
        }
    }
}

TEST(AttentionHierarchy, PermissionTitleTargetScopeAndInstructionsRemainReadable) {
    ThemeGuard restore;
    for (const auto variant : kThemes) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        auto state = std::make_shared<sp::PromptState>();
        state->props.tool_name = "Bash";
        state->props.action_kind = sp::ActionKind::Execute;
        state->props.risk_level = sp::RiskLevel::High;
        state->props.description = "Approve command";
        state->props.detail = sp::DetailGeneric{"TARGET-DETAIL"};
        for (int i = 0; i < 8; ++i) state->props.affected_paths.push_back("/tmp/path-" + std::to_string(i));
        for (const int width : {40, 80, 120}) {
            SCOPED_TRACE(std::format("theme={} width={}", static_cast<int>(variant), width));
            const auto screen = Paint(sp::RenderSinglePrompt(state), width, 40);
            ExpectInk(screen, "Permission", pal.warning, true);
            ExpectInk(screen, "Approve command", pal.text);
            ExpectInk(screen, "TARGET-DETAIL", pal.text);
            ExpectInk(screen, "Always deny this tool", pal.text);
            ExpectInk(screen, "Always allow this scope", pal.text);
            ExpectInk(screen, "Toggle sandbox", pal.muted);
            for (const std::string hints : {"[y] Allow", "[n] Deny", "[a] Always allow",
                    "[d] Always deny", "[s] Toggle sandbox", "[Esc] Cancel", "[PgUp/PgDn] Details"}) {
                const auto keys = FindWord(screen, hints);
                ASSERT_TRUE(keys.has_value()) << screen.ToString();
                bool in_key = false;
                for (std::size_t i = 0; i < hints.size(); ++i) {
                    const char character = hints[i];
                    if (character == ']') in_key = false;
                    const auto& pixel = screen.PixelAt(keys->first + static_cast<int>(i), keys->second);
                    EXPECT_FALSE(pixel.dim);
                    EXPECT_EQ(pixel.bold, in_key);
                    EXPECT_EQ(pixel.foreground_color, in_key ? pal.text : pal.muted);
                    if (character == '[') in_key = true;
                }
            }
            ExpectInk(screen, "more", pal.muted);
        }
    }
}

TEST(AttentionHierarchy, ModelErrorTitleBodyAndRecoveryMetadataHaveDistinctInk) {
    ThemeGuard restore;
    for (const auto variant : kThemes) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        for (const int status : {401, 429, 503}) {
            ae::APIErrorOptions options;
            options.error.http_status = status;
            options.error.message = "DIAGNOSTIC";
            options.error.trace_id = "TRACE-ID";
            options.error.request_id = "REQUEST-ID";
            options.error.current_attempt = 1;
            options.error.max_attempts = 3;
            options.error.response_bytes = 123;
            options.error.retry_after_ms = 2000;
            options.on_retry = [] {};
            options.on_dismiss = [] {};
            const auto screen = Paint(ae::RenderAPIError(options), 120, 15);
            ExpectInk(screen, "Model request failed", status == 401 ? pal.warning : pal.danger, true);
            ExpectInk(screen, "DIAGNOSTIC", pal.text);
            for (const auto word : {"attempt", "response body", "TRACE-ID", "REQUEST-ID", "Retrying in"})
                ExpectInk(screen, word, pal.muted);
            if (status == 401) EXPECT_FALSE(FindWord(screen, "Retry request"));
            else EXPECT_TRUE(FindWord(screen, "Retry request"));
        }
    }
}
