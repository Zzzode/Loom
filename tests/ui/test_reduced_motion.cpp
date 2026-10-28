/// @file test_reduced_motion.cpp
/// @brief Native pixel regressions for the existing reduced-motion contract.
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_tokens;
import loom.ui.foundation.clock;
import loom.ui.foundation.component_primitives;
import loom.ui.widgets.spinner;
import loom.ui.widgets.spinner_animations;
import loom.ui.widgets.text_input;
import loom.ui.messages.tool_use_message;
import loom.ui.messages.thinking_message;
import loom.ui.messages.message_hook_progress;
import loom.ui.messages.assistant_text_message;
import loom.ui.messages.messages_list;
import loom.ui.messages.message_row;
import loom.ui.screens.repl_screen;
import loom.ui.screens.messages_store;
import loom.ui.screens.task_view_store;
import loom.ui.features.agents.agent_shared_widgets;
import loom.ui.features.agents.agent_cards;
import loom.ui.screens.doctor_screen;
import loom.ui.dialogs.mcp_dialogs;
import loom.ui.dialogs.plugin_dialog;
import loom.ui.dialogs.trust_dialog;
import loom.ui.prompt.vim_input;
import loom.ui.screens.resume_screen;
import loom.ui.screens.log_selector;
import loom.ui.features.plugins.plugin_manage_panel;
import loom.ui.features.plugins.plugin_install_flow;
import loom.ui.features.plugins.plugin_settings_dialog;

namespace {
namespace theme = loom::ui::design::theme;
namespace msg = loom::ui::messages;
namespace tool = msg::tool_use_message;
namespace thinking = msg::thinking_message;
namespace ml = loom::ui::messages_list;
namespace repl = loom::ui::repl_screen;
namespace agents = loom::ui::agents::shared;
namespace cards = loom::ui::agents::cards;
namespace anim = loom::ui::components;
namespace spinner = ::ui::components;
using namespace ftxui;
using namespace std::chrono_literals;

struct RestoreTheme {
    theme::Theme original = theme::current_theme();

    ~RestoreTheme() { theme::set_theme(original); }
};

struct FrozenClock {
    FrozenClock() {
        loom::ui::clock::set_steady_now_for_testing(std::chrono::steady_clock::time_point(110s));
    }

    ~FrozenClock() { loom::ui::clock::set_steady_now_for_testing(std::nullopt); }
};

void Motion(bool reduced) {
    auto value = theme::current_theme();
    value.a11y.reduced_motion = reduced;
    theme::set_theme(value);
}

Screen Paint(Element element, int width = 100, int height = 32) {
    auto screen = Screen::Create(Dimension::Fixed(width), Dimension::Fixed(height));
    Render(screen, std::move(element));
    return screen;
}

std::vector<std::string> Pixels(const Screen& screen) {
    std::vector<std::string> result;
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x < screen.dimx(); ++x) {
            const auto& p = screen.PixelAt(x, y);
            result.push_back(std::format(
                "{}|{}|{}|{}{}{}{}{}{}", p.character, p.foreground_color.Print(false),
                p.background_color.Print(false), static_cast<bool>(p.blink),
                static_cast<bool>(p.bold), static_cast<bool>(p.dim), static_cast<bool>(p.inverted),
                static_cast<bool>(p.underlined), static_cast<bool>(p.strikethrough)));
        }
    }
    return result;
}

std::string Text(const Screen& screen) {
    std::string result;
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x < screen.dimx(); ++x)
            result += screen.PixelAt(x, y).character;
        result += '\n';
    }
    return result;
}

bool HasBlink(const Screen& screen) {
    for (int y = 0; y < screen.dimy(); ++y)
        for (int x = 0; x < screen.dimx(); ++x)
            if (screen.PixelAt(x, y).blink) return true;
    return false;
}

void ExpectMotionGate(const std::function<Element(int)>& render) {
    Motion(false);
    const auto animated_a = Paint(render(0));
    const auto animated_b = Paint(render(5));
    EXPECT_NE(Pixels(animated_a), Pixels(animated_b));
    Motion(true);
    const auto quiet_a = Paint(render(0));
    const auto quiet_b = Paint(render(5));
    EXPECT_EQ(Pixels(quiet_a), Pixels(quiet_b));
    EXPECT_FALSE(HasBlink(quiet_a));
}
}  // namespace

TEST(ReducedMotion, FaithfulToolLoaderRetainsStatusAndExplicitAnimationGate) {
    RestoreTheme restore;
    tool::FaithfulToolUseData data;
    data.user_facing_name = "Read";
    data.message = "src/work.cpp";
    data.status = tool::FaithfulToolStatus::Running;
    ExpectMotionGate([&](int frame) {
        data.spinner_frame = frame;
        return tool::RenderFaithfulToolHeader(data);
    });
    Motion(false);
    data.should_animate = false;
    data.spinner_frame = 0;
    const auto first = Paint(tool::RenderFaithfulToolHeader(data));
    data.spinner_frame = 5;
    EXPECT_EQ(Pixels(first), Pixels(Paint(tool::RenderFaithfulToolHeader(data))));
    for (bool reduced : {false, true}) {
        Motion(reduced);
        data.status = tool::FaithfulToolStatus::Success;
        const auto done = Paint(tool::RenderFaithfulToolHeader(data));
        data.status = tool::FaithfulToolStatus::Error;
        const auto failed = Paint(tool::RenderFaithfulToolHeader(data));
        EXPECT_NE(Pixels(done), Pixels(failed));
        EXPECT_NE(Text(done).find("Read"), std::string::npos);
        EXPECT_NE(Text(failed).find("src/work.cpp"), std::string::npos);
    }
}

TEST(ReducedMotion, ActualLiveToolPayloadHonorsThemeAcrossCallerFrames) {
    RestoreTheme restore;
    FrozenClock clock;
    repl::MessageDisplayEntry entry;
    entry.id = "motion-tool";
    entry.role = "assistant";
    entry.is_tool_use = true;
    entry.is_streaming = true;
    entry.tool_name = "Read";
    entry.tool_status = "running";
    entry.tool_input_json = R"({"file_path":"src/motion.cpp"})";
    const std::vector<repl::MessageDisplayEntry> entries{entry};
    ExpectMotionGate([&](int frame) {
        return repl::RenderMessages(entries, -1, 20, 0, true, frame, std::nullopt, {}, false, {},
                                    false, false, true, true);
    });
    const auto rendered = Paint(repl::RenderMessages(entries, -1, 20, 0, true, 5, std::nullopt, {},
                                                     false, {}, false, false, true, true));
    EXPECT_NE(Text(rendered).find("Read"), std::string::npos);
    EXPECT_EQ(entries.front().tool_status, "running");
    EXPECT_TRUE(entries.front().is_streaming);
}

TEST(ReducedMotion, ActualLiveChainStopsBlinkWithoutFreezingProgressOrDisclosure) {
    RestoreTheme restore;
    FrozenClock clock;
    ml::MessagesListInput input;
    tool::ToolUseRenderOptions data;
    data.call.tool_name = "Read";
    data.call.status = tool::ToolStatus::Running;
    data.call.raw_parameters = R"({"file_path":"src/motion.cpp"})";
    data.call.start_time = std::chrono::steady_clock::time_point(100s);
    input.rows.push_back(data);
    input.shapes.push_back(msg::MessageShape::AssistantToolUse);
    input.uuids.push_back("motion-chain");
    input.streaming_tail_row = input.rows.size();
    input.query_running = true;
    ExpectMotionGate([&](int frame) { return ml::render_messages_list_view(input, frame); });
    const auto rendered = Paint(ml::render_messages_list_view(input, 5));
    EXPECT_NE(Text(rendered).find("Working"), std::string::npos);
    EXPECT_NE(Text(rendered).find("src/motion.cpp"), std::string::npos);
    EXPECT_NE(Text(rendered).find("Show details"), std::string::npos);
    const auto deadline = input.chain_update_due;
    loom::ui::clock::set_steady_now_for_testing(std::chrono::steady_clock::time_point(112s));
    const auto progressed = Paint(ml::render_messages_list_view(input, 6));
    EXPECT_NE(Text(rendered), Text(progressed));
    EXPECT_GT(input.chain_update_due, deadline);
}

TEST(ReducedMotion, LegacyToolAndExistingSpinnerRespectGlobalAndExplicitOptions) {
    RestoreTheme restore;
    tool::ToolUseCallData call;
    call.tool_name = "Read";
    call.status = tool::ToolStatus::Running;
    ExpectMotionGate([&](int frame) { return tool::RenderToolHeader(call, frame); });
    ml::RenderEnvelopeOptions envelope;
    envelope.shape = msg::MessageShape::AssistantToolUse;
    envelope.timestamp = std::chrono::system_clock::time_point{};
    envelope.status = ml::EnvelopeStatusBadge::Running;
    ExpectMotionGate([&](int frame) {
        envelope.frame_count = static_cast<std::size_t>(frame);
        return ml::render_message_envelope(envelope, text("tool payload"));
    });
    const auto legacy = Paint(ml::render_message_envelope(envelope, text("tool payload")));
    EXPECT_NE(Text(legacy).find("running"), std::string::npos);
    EXPECT_NE(Text(legacy).find("tool payload"), std::string::npos);
    for (bool global : {false, true}) {
        Motion(global);
        spinner::SpinnerOptions options;
        options.reduced_motion = true;
        options.message = "Working";
        options.frame = 0;
        const auto first = Paint(spinner::SpinnerElement(options));
        options.frame = 5;
        EXPECT_EQ(Pixels(first), Pixels(Paint(spinner::SpinnerElement(options))));
        EXPECT_NE(Text(first).find("Working"), std::string::npos);
    }
}

TEST(ReducedMotion, ThinkingHeaderStopsGlyphAndBlinkButKeepsStateChanges) {
    RestoreTheme restore;
    thinking::ThinkingMessageData data;
    data.state = thinking::ThinkingState::Active;
    ExpectMotionGate([&](int frame) { return thinking::RenderThinkingHeader(data, frame); });
    const auto active = Paint(thinking::RenderThinkingHeader(data, 5));
    EXPECT_NE(Text(active).find("Thinking"), std::string::npos);
    data.state = thinking::ThinkingState::Paused;
    EXPECT_NE(Text(Paint(thinking::RenderThinkingHeader(data, 6))).find("paused"),
              std::string::npos);
}

TEST(ReducedMotion, HeldHookProgressStopsAnimationButStillAppliesUpdates) {
    RestoreTheme restore;
    auto component = std::make_shared<msg::HookProgressComponent>(
        std::vector<msg::HookProgressEntry>{{"build-hook", msg::HookState::Running, {}, {}}});
    Motion(false);
    const auto animated = Paint(component->Render());
    EXPECT_NE(Pixels(animated), Pixels(Paint(component->Render())));
    EXPECT_TRUE(HasBlink(animated));
    Motion(true);
    const auto stable = Paint(component->Render());
    EXPECT_EQ(Pixels(stable), Pixels(Paint(component->Render())));
    EXPECT_FALSE(HasBlink(stable));
    EXPECT_NE(Text(stable).find("build-hook"), std::string::npos);
    component->Update({{"build-hook", msg::HookState::Completed, 42.0, {}}});
    const auto complete = Paint(component->Render());
    EXPECT_NE(Text(complete).find("100%"), std::string::npos);
    EXPECT_NE(Text(complete).find("42ms"), std::string::npos);
}

TEST(ReducedMotion, AgentStatusFamiliesAndLabelSpinnerKeepNamedStatus) {
    RestoreTheme restore;
    ExpectMotionGate([](int frame) {
        return agents::StatusDot({.status = agents::AgentStatus::Running,
                                  .with_label = true,
                                  .spinner_frame = static_cast<std::uint32_t>(frame)});
    });
    ExpectMotionGate(
        [](int frame) { return cards::StatusBadge(agents::AgentStatus::Running, frame); });
    ExpectMotionGate([](int frame) { return anim::render_spinner_with_label("Searching", frame); });
    EXPECT_NE(
        Text(Paint(agents::StatusDot(
                 {.status = agents::AgentStatus::Running, .with_label = true, .spinner_frame = 5})))
            .find("Running"),
        std::string::npos);
    EXPECT_EQ(anim::get_spinner_frame(anim::kLineAnimation, 0),
              anim::get_spinner_frame(anim::kLineAnimation, 3));
}

TEST(ReducedMotion, DoctorAndMcpResolutionKeepProgressWhileGlyphIsStatic) {
    RestoreTheme restore;
    namespace doctor = loom::ui::doctor_screen;
    doctor::DoctorDataModel model;
    ExpectMotionGate([&](int frame) {
        return doctor::RenderRunningState(model, frame, 2, static_cast<doctor::CheckId>(2));
    });
    namespace mcp = loom::ui::mcp_dialogs;
    mcp::ElicitationPayload request;
    request.server_name = "motion-server";
    request.url = "https://example.test/authorize";
    ExpectMotionGate(
        [&](int frame) { return mcp::RenderElicitationStub(request, true, frame, true); });
    EXPECT_NE(Text(Paint(mcp::RenderElicitationStub(request, true, 5, true))).find("resolving"),
              std::string::npos);
    EXPECT_EQ(request.server_name, "motion-server");
}

TEST(ReducedMotion, ExistingPrimaryAndPrimitiveSpinnersRemainStable) {
    RestoreTheme restore;
    ExpectMotionGate([](int frame) {
        return repl::RenderSpinner(repl::SpinnerMode::Thinking, std::string("Working"),
                                   std::nullopt, frame);
    });
    const auto original = theme::current_theme();
    auto value = original;
    value.a11y.reduced_motion = false;
    const auto normal_a = Paint(loom::ui::design::primitives::spinner_glyph(
        0, loom::ui::design::tokens::Role::Primary, value));
    const auto normal_b = Paint(loom::ui::design::primitives::spinner_glyph(
        5, loom::ui::design::tokens::Role::Primary, value));
    EXPECT_NE(Pixels(normal_a), Pixels(normal_b));
    value.a11y.reduced_motion = true;
    EXPECT_EQ(Pixels(Paint(loom::ui::design::primitives::spinner_glyph(
                  0, loom::ui::design::tokens::Role::Primary, value))),
              Pixels(Paint(loom::ui::design::primitives::spinner_glyph(
                  5, loom::ui::design::tokens::Role::Primary, value))));
}

TEST(ReducedMotion, StreamingAssistantUsesStaticVisibleCursor) {
    RestoreTheme restore;
    msg::AssistantTextMessageData data;
    data.content = "streaming response";
    data.is_streaming = true;
    auto held = msg::MakeAssistantTextMessage(data);
    Motion(false);
    EXPECT_TRUE(HasBlink(Paint(held->Render())));
    Motion(true);
    const auto first = Paint(held->Render());
    EXPECT_EQ(Pixels(first), Pixels(Paint(held->Render())));
    EXPECT_FALSE(HasBlink(first));
    EXPECT_NE(Text(first).find("▍"), std::string::npos);
    EXPECT_TRUE(data.is_streaming);
}

TEST(ReducedMotion, ManualCaretsKeepTheirGlyphAndFocusWithoutTerminalBlink) {
    RestoreTheme restore;
    namespace plugins = loom::ui::plugins;
    namespace vim = loom::ui::prompt::vim_input;
    namespace log = loom::ui::screens::log_selector;
    namespace resume = loom::ui::resume_screen;
    namespace trust = loom::ui::trust_dialog;
    vim::VimState vim_state;
    vim_state.vim.mode = vim::VimMode::Insert;
    vim_state.text = "editing";
    trust::DialogState trust_state;
    trust_state.show_second_confirm = true;
    log::SelectorState log_state;
    log_state.search_focused = true;
    const std::vector<std::pair<std::string, std::function<Element()>>> renderers{
        {"Vim", [&] { return vim::RenderVimInput(vim_state); }},
        {"Marketplace",
         [] { return loom::ui::dialogs::plugin_dialog::RenderAddMarketplace("source", true, ""); }},
        {"Trust", [&] { return trust::RenderCriticalGates(trust_state); }},
        {"Resume",
         [] { return resume::RenderEditTitleDialog(resume::SessionMetaRow{}, "title", false); }},
        {"Resume search",
         [] {
             return resume::RenderListHeader("query", true, resume::SortMode::NewestFirst, false,
                                             "", 1, 1);
         }},
        {"Log sidebar", [&] { return log::RenderSidebar(log_state); }},
        {"Log import", [&] { return log::RenderImportDialog(log_state); }},
        {"Remote loading",
         [] {
             log::SelectorState remote;
             remote.tab = log::Tab::Remote;
             remote.remote_loading = true;
             remote.all_sessions.emplace_back();
             remote.view.push_back(0);
             return log::RenderCardGrid(remote);
         }},
        {"Plugin search",
         [] {
             return plugins::plugin_manage_panel::detail::RenderToolbar(
                 "plugin", plugins::plugin_manage_panel::StatusFilter::All,
                 plugins::plugin_manage_panel::SortKey::Name, 1, 1);
         }},
        {"Plugin source",
         [] { return plugins::plugin_install_flow::detail::RenderSourceStep({}, 0); }},
        {"Plugin keybinding",
         [] {
             return plugins::plugin_settings_dialog::detail::RenderKeybindingsTab(nullptr, 0, true,
                                                                                  "binding", "");
         }},
    };
    for (const auto& [name, render] : renderers) {
        SCOPED_TRACE(name);
        Motion(false);
        const auto animated = Paint(render());
        EXPECT_TRUE(HasBlink(animated));
        Motion(true);
        const auto quiet = Paint(render());
        EXPECT_FALSE(HasBlink(quiet));
        EXPECT_EQ(Pixels(quiet), Pixels(Paint(render())));
        EXPECT_EQ(Text(animated), Text(quiet));
    }
    log_state.search_focused = false;
    EXPECT_FALSE(HasBlink(Paint(log::RenderSidebar(log_state))));
    EXPECT_EQ(vim_state.text, "editing");
    EXPECT_EQ(vim_state.cursor_pos, 0);
    EXPECT_TRUE(trust_state.show_second_confirm);
}

TEST(ReducedMotion, HeldTextInputBlinkStopsAndResumesWithoutEditing) {
    RestoreTheme restore;
    spinner::TextInputOptions options;
    options.columns = 20;
    options.cursor_blink_ms = 1;
    options.show_line_numbers = false;
    spinner::TextInputImpl held(options);
    held.set_text("input");
    Motion(false);
    std::this_thread::sleep_for(3ms);
    held.TickBlink();
    const auto hidden = Paint(held.RenderInputAreaPub(), 20, 1);
    ASSERT_FALSE(hidden.PixelAt(7, 0).inverted);
    // Toggle the theme between paints without ticking the caller-driven
    // low-level API: the drawing boundary must make the caret visible.
    Motion(true);
    const auto first = Paint(held.RenderInputAreaPub(), 20, 1);
    std::this_thread::sleep_for(3ms);
    const auto second = Paint(held.RenderInputAreaPub(), 20, 1);
    EXPECT_EQ(Pixels(first), Pixels(second));
    EXPECT_TRUE(first.PixelAt(7, 0).inverted);
    Motion(false);
    const auto resumed = Paint(held.Render(), 20, 1);
    std::this_thread::sleep_for(3ms);
    const auto tick = Paint(held.Render(), 20, 1);
    EXPECT_NE(Pixels(resumed), Pixels(tick));
    EXPECT_EQ(held.text(), "input");
    EXPECT_EQ(held.cursor(), 5);
    Motion(true);
    for (bool focused : {false, true}) {
        auto placeholder_options = options;
        placeholder_options.placeholder = "hint";
        placeholder_options.terminal_focus = focused;
        spinner::TextInputImpl placeholder(placeholder_options);
        const auto screen = Paint(placeholder.RenderInputAreaPub(), 20, 1);
        EXPECT_EQ(screen.PixelAt(2, 0).inverted, focused);
    }
}
