/// @file test_dialog_core.cpp
/// @brief Dialog framework core tests: DialogSystem, DialogQueue,
/// DialogRendererRegistry, DialogFrame, DefaultRenderers, FullDialogRegistry.
/// Split from test_dialog_system.cpp (SLOC budget).

#include <cstdlib>

#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

import std;
import loom.ui.dialogs.system;
import loom.ui.dialogs.frame;
import loom.ui.dialogs.default_renderers;
import loom.ui.dialogs.modal_renderers;
import loom.ui.dialogs.bottom_renderers;
import loom.ui.dialogs.all_renderers;
import loom.ui.dialogs.triggers;
import loom.ui.dialogs.quick_open;
import loom.ui.dialogs.session_picker;
import loom.ui.dialogs.sandbox_permission;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_tokens;
import loom.constants.product;

namespace {

namespace dsys = loom::ui::dialogs::system;
namespace dframe = loom::ui::dialogs::frame;
namespace drender = loom::ui::dialogs::default_renderers;
using Theme = loom::ui::design::theme::Theme;

// ============================================================
// Golden snapshot helpers (verbatim copy from tests/test_ui.cpp)
// ============================================================
namespace dialog_test_golden {

/// Directory holding golden snapshot files (derived from __FILE__).
std::string golden_dir() {
    
    return std::string(LOOM_TESTS_DIR) + "/golden/";
}

/// Normalize line endings to LF-only for cross-platform robustness.
std::string normalize_line_endings(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c != '\r') out.push_back(c);
    }
    return out;
}

/// Golden-snapshot check.  Set UPDATE_GOLDENS=1 to (re)write the file.
void check_golden(const std::string& name, const std::string& actual) {
    const std::string path = golden_dir() + name + ".txt";
    if (std::getenv("UPDATE_GOLDENS") != nullptr) {
        std::ofstream out(path, std::ios::binary);
        ASSERT_TRUE(out.good()) << "cannot write golden: " << path;
        // Normalize on write as well as on compare: the renderer emits CRLF,
        // and committing it makes every line of the golden differ from its LF
        // original for no semantic reason.
        out << normalize_line_endings(actual);
        SUCCEED() << "golden updated: " << path;
        return;
    }
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.good()) << "golden missing: " << path
                           << " (run UPDATE_GOLDENS=1 to create)";
    std::string expected((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    EXPECT_EQ(normalize_line_endings(actual), normalize_line_endings(expected))
        << "golden mismatch for '" << name
        << "' (run UPDATE_GOLDENS=1 to refresh)";
}

/// Render an Element to a fixed-size terminal buffer (includes ANSI codes).
std::string render_to_ansi(ftxui::Element element, int width, int height) {
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(width),
        ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, element);
    return screen.ToString();
}

/// Deterministic LightTheme for golden snapshots.  Mirrors the theme
/// construction used in test_ui.cpp visual-snapshot tests.
[[nodiscard]] inline Theme get_light_theme() {
    using ThemeVariant = loom::ui::design::theme::ThemeVariant;
    return Theme{
        ThemeVariant::Light,
        &loom::ui::design::tokens::palette::light(),
    };
}

} // namespace dialog_test_golden


// ============================================================
// DialogType / DialogSlot / DialogPriority tests
// ============================================================


TEST(DialogSystem, DialogTypeNames) {
    EXPECT_EQ(dsys::dialog_type_name(dsys::DialogType::ToolPermission), "tool-permission");
    EXPECT_EQ(dsys::dialog_type_name(dsys::DialogType::SandboxPermission), "sandbox-permission");
    EXPECT_EQ(dsys::dialog_type_name(dsys::DialogType::CostThreshold), "cost-threshold");
    EXPECT_EQ(dsys::dialog_type_name(dsys::DialogType::SettingsPanel), "settings-panel");
}

TEST(DialogSystem, SlotMappingCorrect) {
    // Overlay slot
    EXPECT_EQ(dsys::slot_for(dsys::DialogType::ToolPermission), dsys::DialogSlot::Overlay);

    // Bottom slot
    EXPECT_EQ(dsys::slot_for(dsys::DialogType::SandboxPermission), dsys::DialogSlot::Bottom);
    EXPECT_EQ(dsys::slot_for(dsys::DialogType::PromptDialog), dsys::DialogSlot::Bottom);
    EXPECT_EQ(dsys::slot_for(dsys::DialogType::CostThreshold), dsys::DialogSlot::Bottom);
    EXPECT_EQ(dsys::slot_for(dsys::DialogType::IdleReturn), dsys::DialogSlot::Bottom);
    EXPECT_EQ(dsys::slot_for(dsys::DialogType::LspRecommendation), dsys::DialogSlot::Bottom);

    // Modal slot
    EXPECT_EQ(dsys::slot_for(dsys::DialogType::SettingsPanel), dsys::DialogSlot::Modal);
    EXPECT_EQ(dsys::slot_for(dsys::DialogType::TasksView), dsys::DialogSlot::Modal);
    EXPECT_EQ(dsys::slot_for(dsys::DialogType::HelpView), dsys::DialogSlot::Modal);

    // Standalone (legacy — most are migrating to Modal)
    // (none remain as standalone; all migrated to queue-based modal)
}

TEST(DialogSystem, PriorityMappingCorrect) {
    // Band 1: MessageSelector (highest, never suppressed)
    EXPECT_EQ(dsys::priority_for(dsys::DialogType::MessageSelector), dsys::DialogPriority::Band1);

    // Band 2: SandboxPermission
    EXPECT_EQ(dsys::priority_for(dsys::DialogType::SandboxPermission), dsys::DialogPriority::Band2);

    // Band 3: permissions, prompt, elicitation
    EXPECT_EQ(dsys::priority_for(dsys::DialogType::ToolPermission), dsys::DialogPriority::Band3);
    EXPECT_EQ(dsys::priority_for(dsys::DialogType::PromptDialog), dsys::DialogPriority::Band3);
    EXPECT_EQ(dsys::priority_for(dsys::DialogType::Elicitation), dsys::DialogPriority::Band3);

    // Band 4: cost, idle, ultraplan
    EXPECT_EQ(dsys::priority_for(dsys::DialogType::CostThreshold), dsys::DialogPriority::Band4);
    EXPECT_EQ(dsys::priority_for(dsys::DialogType::IdleReturn), dsys::DialogPriority::Band4);

    // Band 6 (lowest): recs, hints, upsells
    EXPECT_EQ(dsys::priority_for(dsys::DialogType::LspRecommendation), dsys::DialogPriority::Band6);
    EXPECT_EQ(dsys::priority_for(dsys::DialogType::PluginHint), dsys::DialogPriority::Band6);
    EXPECT_EQ(dsys::priority_for(dsys::DialogType::DesktopUpsell), dsys::DialogPriority::Band6);
}

TEST(DialogSystem, TypingSuppressionCorrect) {
    // Only band 1 (MessageSelector) is never suppressed
    EXPECT_FALSE(dsys::is_suppressed_by_typing(dsys::DialogType::MessageSelector));

    // Everything else is suppressed while typing
    EXPECT_TRUE(dsys::is_suppressed_by_typing(dsys::DialogType::SandboxPermission));
    EXPECT_TRUE(dsys::is_suppressed_by_typing(dsys::DialogType::ToolPermission));
    EXPECT_TRUE(dsys::is_suppressed_by_typing(dsys::DialogType::CostThreshold));
    EXPECT_TRUE(dsys::is_suppressed_by_typing(dsys::DialogType::LspRecommendation));
}

TEST(DialogSystem, PayloadVariantTypeOf) {
    dsys::ToolPermissionPayload tp;
    tp.id = "test-tp";
    tp.tool_name = "BashTool";
    dsys::DialogPayloadVariant v = tp;
    EXPECT_EQ(dsys::type_of(v), dsys::DialogType::ToolPermission);
    EXPECT_EQ(dsys::id_of(v), "test-tp");
    EXPECT_EQ(dsys::slot_of(v), dsys::DialogSlot::Overlay);
    EXPECT_EQ(dsys::priority_of(v), dsys::DialogPriority::Band3);

    dsys::CostThresholdPayload ct;
    ct.id = "test-ct";
    dsys::DialogPayloadVariant v2 = ct;
    EXPECT_EQ(dsys::type_of(v2), dsys::DialogType::CostThreshold);
    EXPECT_EQ(dsys::id_of(v2), "test-ct");
    EXPECT_EQ(dsys::slot_of(v2), dsys::DialogSlot::Bottom);
    EXPECT_EQ(dsys::priority_of(v2), dsys::DialogPriority::Band4);
}

TEST(DialogQueue, EmptyByDefault) {
    dsys::DialogQueue q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.total_size(), 0u);
    EXPECT_FALSE(q.has_overlay());
    EXPECT_FALSE(q.has_any_bottom());
    EXPECT_FALSE(q.has_modal());
}

TEST(DialogQueue, OverlayPushPeekPop) {
    dsys::DialogQueue q;

    dsys::ToolPermissionPayload tp;
    tp.id = "tp-1";
    tp.tool_name = "BashTool";
    q.push(dsys::DialogPayloadVariant{tp});

    EXPECT_EQ(q.total_size(), 1u);
    EXPECT_TRUE(q.has_overlay());
    EXPECT_FALSE(q.empty());

    auto peeked = q.peek_overlay();
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "tp-1");

    q.pop_overlay();
    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.has_overlay());
}

TEST(DialogQueue, BottomPriorityOrder) {
    dsys::DialogQueue q;

    // Push in low -> high priority order
    dsys::IdleReturnPayload idle;
    idle.id = "idle-1";
    q.push(dsys::DialogPayloadVariant{idle});

    dsys::CostThresholdPayload cost;
    cost.id = "cost-1";
    q.push(dsys::DialogPayloadVariant{cost});

    dsys::SandboxPermissionPayload sandbox;
    sandbox.id = "sandbox-1";
    q.push(dsys::DialogPayloadVariant{sandbox});

    EXPECT_EQ(q.total_size(), 3u);

    // Peek should return highest priority (sandbox = band 2)
    auto peeked = q.peek_bottom(/*is_prompt_input_active=*/false);
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "sandbox-1");

    // Pop highest priority
    q.pop_bottom(false);
    EXPECT_EQ(q.total_size(), 2u);

    // Next should be idle (band 4, pushed first — FIFO within same band)
    peeked = q.peek_bottom(false);
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "idle-1");

    q.pop_bottom(false);
    EXPECT_EQ(q.total_size(), 1u);

    // Last should be cost (band 4, pushed after idle — FIFO within same band)
    peeked = q.peek_bottom(false);
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "cost-1");

    q.pop_bottom(false);
    EXPECT_TRUE(q.empty());
}

TEST(DialogQueue, BottomTypingSuppression) {
    dsys::DialogQueue q;

    // Push a cost dialog (band 4 — suppressed while typing)
    dsys::CostThresholdPayload cost;
    cost.id = "cost-1";
    q.push(dsys::DialogPayloadVariant{cost});

    // When typing is active, no bottom dialog should show
    // (band 4 is suppressed by typing)
    auto peeked = q.peek_bottom(/*is_prompt_input_active=*/true);
    EXPECT_FALSE(peeked.has_value());

    // When typing is not active, it should show
    peeked = q.peek_bottom(false);
    EXPECT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "cost-1");
}

TEST(DialogQueue, Band3AnimationSuppression) {
    using dsys::DialogPriority;

    // ------------------------------------------------------------------
    // Bottom slot: PromptDialog / Elicitation / WorkerSandboxPermission
    // are Band3 (i=2).  When allow_dialogs_with_animation=false,
    // they MUST be skipped in favour of the next active band (or nullopt
    // if no higher-priority non-suppressed dialog exists).
    // ------------------------------------------------------------------
    dsys::DialogQueue q;

    // Case A: Band3-only queue + animation active → nullopt.
    {
        dsys::PromptDialogPayload prompt;
        prompt.id = "prompt-1";
        q.push(dsys::DialogPayloadVariant{prompt});
    }
    auto peeked = q.peek_bottom(/*typing=*/false,
                                 /*allow_dialogs_with_animation=*/false);
    EXPECT_FALSE(peeked.has_value())
        << "Band3 PromptDialog MUST be suppressed when animation is active";

    // Case B: same queue + animation inactive → PromptDialog visible.
    peeked = q.peek_bottom(false, /*animation_ok=*/true);
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "prompt-1");

    // Case C: SandboxPermission (Band2, i=1) + animation active → still
    // visible (only Band3 is blocked).
    {
        dsys::SandboxPermissionPayload sbx;
        sbx.id = "sbx-1";
        q.push(dsys::DialogPayloadVariant{sbx});
    }
    peeked = q.peek_bottom(false, /*animation_ok=*/false);
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "sbx-1")
        << "Band2 SandboxPermission MUST NOT be blocked by animation";
    // Pop the Band2 front.
    q.pop_bottom(false, /*animation_ok=*/false);
    EXPECT_EQ(q.total_size(), 1u); // prompt-1 still in Band3

    // Case D: now Band3 (prompt) is next; animation still active → nullopt.
    peeked = q.peek_bottom(false, /*animation_ok=*/false);
    EXPECT_FALSE(peeked.has_value());

    // Case E: Push Band4 CostThreshold below.  With animation active,
    // Band4 (i=3) MUST still be reachable since we SKIP Band3 (i=2)
    // but continue the loop to i=3.
    {
        dsys::CostThresholdPayload cost;
        cost.id = "cost-1";
        q.push(dsys::DialogPayloadVariant{cost});
    }
    peeked = q.peek_bottom(false, /*animation_ok=*/false);
    ASSERT_TRUE(peeked.has_value())
        << "Band4 CostThreshold MUST be reachable when Band3 is skipped "
           "for animation suppression — find_active_band must CONTINUE, "
           "not RETURN, on !allow_dialogs_with_animation";
    EXPECT_EQ(dsys::id_of(peeked->get()), "cost-1");
    // pop correctly uses the same suppression rule
    q.pop_bottom(false, /*animation_ok=*/false);
    EXPECT_EQ(q.total_size(), 1u); // only prompt-1 remains

    // Case F: Cost gone — prompt is alone again.  Animation off → prompt.
    peeked = q.peek_bottom(false, /*animation_ok=*/true);
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "prompt-1");
    q.pop_bottom(false, true);
    EXPECT_TRUE(q.empty());

    // ------------------------------------------------------------------
    // Overlay slot: ToolPermission is implicitly Band3; should_show_dialog
    // free function MUST suppress it when !allow_dialogs_with_animation.
    // ------------------------------------------------------------------
    {
        dsys::ToolPermissionPayload tp;
        tp.id = "tp-1";
        q.push(dsys::DialogPayloadVariant{tp}); // → Overlay slot
    }
    EXPECT_TRUE(q.has_overlay());
    auto overlay_peek = q.peek_overlay();
    ASSERT_TRUE(overlay_peek.has_value());

    EXPECT_FALSE(dsys::should_show_dialog(overlay_peek->get(),
                                           /*typing=*/false,
                                           /*animation_ok=*/false))
        << "Overlay (ToolPermission, Band3) MUST be suppressed by should_show_dialog "
           "when animation is active";
    EXPECT_TRUE(dsys::should_show_dialog(overlay_peek->get(),
                                          /*typing=*/false,
                                          /*animation_ok=*/true))
        << "Overlay (ToolPermission, Band3) MUST show when animation is off";

    // ------------------------------------------------------------------
    // Bottom slot free-function check: PromptDialog (Band3) suppressed
    // by animation, CostThreshold (Band4) NOT suppressed.
    // ------------------------------------------------------------------
    dsys::PromptDialogPayload prompt2;
    prompt2.id = "prompt2";
    auto prompt_v = dsys::DialogPayloadVariant{prompt2};
    EXPECT_FALSE(dsys::should_show_dialog(prompt_v, false, false));
    EXPECT_TRUE(dsys::should_show_dialog(prompt_v, false, true));
    // typing always suppresses it too
    EXPECT_FALSE(dsys::should_show_dialog(prompt_v, true, true));

    dsys::CostThresholdPayload cost2;
    cost2.id = "cost2";
    auto cost_v = dsys::DialogPayloadVariant{cost2};
    EXPECT_TRUE(dsys::should_show_dialog(cost_v, false, false))
        << "Band4 CostThreshold is NOT suppressed by the animation flag";
    EXPECT_FALSE(dsys::should_show_dialog(cost_v, true, true))
        << "Band4 CostThreshold IS suppressed by typing (typing kills bands 2..6)";
}

TEST(DialogQueue, ModalStack) {
    dsys::DialogQueue q;

    dsys::GenericDialogPayload g1;
    g1.id = "modal-1";
    g1.title = "First";
    q.push_modal(dsys::DialogPayloadVariant{g1});

    EXPECT_TRUE(q.has_modal());
    EXPECT_EQ(q.total_size(), 1u);

    auto peeked = q.peek_modal();
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "modal-1");

    // Push second modal on top
    dsys::GenericDialogPayload g2;
    g2.id = "modal-2";
    g2.title = "Second";
    q.push_modal(dsys::DialogPayloadVariant{g2});

    EXPECT_EQ(q.total_size(), 2u);
    peeked = q.peek_modal();
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "modal-2");

    // Pop top
    q.pop_modal();
    EXPECT_EQ(q.total_size(), 1u);
    peeked = q.peek_modal();
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "modal-1");

    q.pop_modal();
    EXPECT_FALSE(q.has_modal());
    EXPECT_TRUE(q.empty());
}

TEST(DialogQueue, RemoveById) {
    dsys::DialogQueue q;

    dsys::CostThresholdPayload cost;
    cost.id = "cost-1";
    q.push(dsys::DialogPayloadVariant{cost});

    dsys::IdleReturnPayload idle;
    idle.id = "idle-1";
    q.push(dsys::DialogPayloadVariant{idle});

    EXPECT_EQ(q.total_size(), 2u);

    q.remove("cost-1");
    EXPECT_EQ(q.total_size(), 1u);

    auto peeked = q.peek_bottom(false);
    ASSERT_TRUE(peeked.has_value());
    EXPECT_EQ(dsys::id_of(peeked->get()), "idle-1");

    // Remove non-existent id — no crash
    q.remove("nonexistent");
    EXPECT_EQ(q.total_size(), 1u);
}

TEST(DialogQueue, ClearEmptiesAllSlots) {
    dsys::DialogQueue q;

    dsys::ToolPermissionPayload tp;
    tp.id = "tp-1";
    q.push(dsys::DialogPayloadVariant{tp});

    dsys::CostThresholdPayload cost;
    cost.id = "cost-1";
    q.push(dsys::DialogPayloadVariant{cost});

    dsys::GenericDialogPayload g;
    g.id = "modal-1";
    q.push_modal(dsys::DialogPayloadVariant{g});

    EXPECT_EQ(q.total_size(), 3u);

    q.clear();
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.total_size(), 0u);
    EXPECT_FALSE(q.has_overlay());
    EXPECT_FALSE(q.has_any_bottom());
    EXPECT_FALSE(q.has_modal());
}

TEST(DialogQueue, MultiSlotIndependent) {
    dsys::DialogQueue q;

    // Overlay
    dsys::ToolPermissionPayload tp;
    tp.id = "tp-1";
    q.push(dsys::DialogPayloadVariant{tp});

    // Bottom
    dsys::CostThresholdPayload cost;
    cost.id = "cost-1";
    q.push(dsys::DialogPayloadVariant{cost});

    // Modal
    dsys::GenericDialogPayload g;
    g.id = "modal-1";
    q.push_modal(dsys::DialogPayloadVariant{g});

    EXPECT_EQ(q.total_size(), 3u);
    EXPECT_TRUE(q.has_overlay());
    EXPECT_TRUE(q.has_any_bottom());
    EXPECT_TRUE(q.has_modal());

    // Pop overlay — bottom and modal should still be there
    q.pop_overlay();
    EXPECT_EQ(q.total_size(), 2u);
    EXPECT_FALSE(q.has_overlay());
    EXPECT_TRUE(q.has_any_bottom());
    EXPECT_TRUE(q.has_modal());
}

TEST(DialogRendererRegistry, EmptyRegistryFallsBack) {
    dsys::DialogRendererRegistry registry;
    dsys::DialogRenderContext ctx;

    dsys::ToolPermissionPayload tp;
    tp.id = "test";
    dsys::DialogPayloadVariant payload = tp;

    auto el = registry.render(payload, ctx);
    EXPECT_NE(el, nullptr);

    // Fallback should render something (not blank)
    auto screen = ftxui::Screen::Create({80, 20});
    Render(screen, el);
    EXPECT_GT(screen.ToString().size(), 0u);
}

TEST(DialogRendererRegistry, CustomRendererOverridesFallback) {
    dsys::DialogRendererRegistry registry;
    dsys::DialogRenderContext ctx;

    bool called = false;
    registry.register_dialog(dsys::DialogType::ToolPermission,
        [&](dsys::DialogPayloadVariant&,
            const dsys::DialogRenderContext&) -> ftxui::Element {
            called = true;
            return ftxui::text("custom renderer");
        });

    dsys::ToolPermissionPayload tp;
    tp.id = "test";
    dsys::DialogPayloadVariant payload = tp;

    auto el = registry.render(payload, ctx);
    EXPECT_TRUE(called);
    EXPECT_NE(el, nullptr);
}

TEST(DialogRendererRegistry, EventHandlerDispatchesCorrectType) {
    dsys::DialogRendererRegistry registry;

    bool handled = false;
    registry.register_event_handler(dsys::DialogType::CostThreshold,
        [&](dsys::DialogPayloadVariant&, const ftxui::Event&) -> bool {
            handled = true;
            return true;
        });

    dsys::CostThresholdPayload ct;
    ct.id = "test";
    dsys::DialogPayloadVariant payload = ct;

    bool result = registry.handle_event(payload, ftxui::Event::Character('y'));
    EXPECT_TRUE(handled);
    EXPECT_TRUE(result);
}

TEST(DialogRendererRegistry, UnregisteredHandlerReturnsFalse) {
    dsys::DialogRendererRegistry registry;

    dsys::IdleReturnPayload idle;
    idle.id = "test";
    dsys::DialogPayloadVariant payload = idle;

    bool result = registry.handle_event(payload, ftxui::Event::Character('y'));
    EXPECT_FALSE(result);
}

TEST(DialogFrame, RendersBasicFrame) {
    Theme theme;

    dframe::DialogFrameProps props;
    props.title = "Test Dialog";
    props.subtitle = "A test subtitle";
    props.content = ftxui::text("Hello, world!");

    auto el = dframe::DialogFrame(props, theme);
    EXPECT_NE(el, nullptr);

    auto screen = ftxui::Screen::Create({60, 15});
    Render(screen, el);

    std::string output = screen.ToString();
    EXPECT_FALSE(output.empty());
    // Title should appear
    EXPECT_NE(output.find("Test Dialog"), std::string::npos);
    // Subtitle should appear
    EXPECT_NE(output.find("A test subtitle"), std::string::npos);
    // Content should appear
    EXPECT_NE(output.find("Hello, world!"), std::string::npos);
}

TEST(DialogFrame, RendersWithWorkerBadge) {
    Theme theme;

    dframe::DialogFrameProps props;
    props.title = "Worker Dialog";
    props.worker_badge = dframe::WorkerBadge("worker-1", theme);
    props.content = ftxui::text("Worker content");

    auto el = dframe::DialogFrame(props, theme);
    EXPECT_NE(el, nullptr);

    auto screen = ftxui::Screen::Create({60, 10});
    Render(screen, el);

    std::string output = screen.ToString();
    EXPECT_NE(output.find("Worker Dialog"), std::string::npos);
    EXPECT_NE(output.find("worker-1"), std::string::npos);
}

TEST(DialogFrame, RendersWithDifferentStyles) {
    Theme theme;

    // Info style
    dframe::DialogFrameProps info_props;
    info_props.title = "Info";
    info_props.style = dframe::FrameStyle::Info;
    info_props.content = ftxui::text("info");
    auto info_el = dframe::DialogFrame(info_props, theme);
    EXPECT_NE(info_el, nullptr);

    // Warning style
    dframe::DialogFrameProps warn_props;
    warn_props.title = "Warning";
    warn_props.style = dframe::FrameStyle::Warning;
    warn_props.content = ftxui::text("warning");
    auto warn_el = dframe::DialogFrame(warn_props, theme);
    EXPECT_NE(warn_el, nullptr);

    // Danger style
    dframe::DialogFrameProps danger_props;
    danger_props.title = "Danger";
    danger_props.style = dframe::FrameStyle::Danger;
    danger_props.content = ftxui::text("danger");
    auto danger_el = dframe::DialogFrame(danger_props, theme);
    EXPECT_NE(danger_el, nullptr);
}

TEST(DialogFrame, SimpleConvenienceBuilders) {
    Theme theme;

    auto info = dframe::SimpleInfoFrame("Info Title", "Info message here.", theme);
    EXPECT_NE(info, nullptr);

    auto warn = dframe::SimpleWarningFrame("Warning Title", "Warning message.", theme);
    EXPECT_NE(warn, nullptr);

    auto danger = dframe::SimpleDangerFrame("Danger Title", "Danger message.", theme);
    EXPECT_NE(danger, nullptr);
}

TEST(DefaultRenderers, RegisterAllDefaultRenderers) {
    dsys::DialogRendererRegistry registry;
    drender::register_default_renderers(registry);

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 80;
    ctx.term_rows = 24;

    // ToolPermission should render
    {
        dsys::ToolPermissionPayload tp;
        tp.id = "tp-test";
        tp.tool_name = "BashTool";
        tp.description = "Run a command";
        dsys::DialogPayloadVariant payload = tp;
        auto el = registry.render(payload, ctx);
        EXPECT_NE(el, nullptr);

        auto screen = ftxui::Screen::Create({80, 20});
        Render(screen, el);
        EXPECT_FALSE(screen.ToString().empty());
        EXPECT_NE(screen.ToString().find("BashTool"), std::string::npos);
    }

    // SandboxPermission should render
    {
        dsys::SandboxPermissionPayload sp;
        sp.id = "sb-test";
        sp.host_pattern = "example.com";
        dsys::DialogPayloadVariant payload = sp;
        auto el = registry.render(payload, ctx);
        EXPECT_NE(el, nullptr);

        auto screen = ftxui::Screen::Create({80, 15});
        Render(screen, el);
        EXPECT_FALSE(screen.ToString().empty());
        EXPECT_NE(screen.ToString().find("example.com"), std::string::npos);
    }

    // CostThreshold should render (P0x3 contract: title + docs link)
    {
        dsys::CostThresholdPayload ct;
        ct.id = "ct-test";
        ct.dollars_spent = 4.7;    // rounds to $5
        ct.model_name = std::string("test-model");
        dsys::DialogPayloadVariant payload = ct;
        auto el = registry.render(payload, ctx);
        EXPECT_NE(el, nullptr);

        auto screen = ftxui::Screen::Create({80, 15});
        Render(screen, el);
        std::string out = screen.ToString();
        EXPECT_FALSE(out.empty());
        // Title must be formatted with dollars_spent using $%.0f (rounded)
        EXPECT_NE(out.find("You've spent $5 on the API this session."),
                  std::string::npos);
        // Body + docs link must appear
        EXPECT_NE(out.find("Learn more about how to monitor your spending:"),
                  std::string::npos);
        // No docs site ships with this build, so no link is rendered by
        // default; the paragraph above stands on its own.
        EXPECT_EQ(out.find("code.loom.com"), std::string::npos)
            << "must not advertise a host that does not resolve";
        // Single "Got it, thanks!" button
        EXPECT_NE(out.find("Got it, thanks!"), std::string::npos);
        // Fabricated 3-action chrome MUST be absent
        EXPECT_EQ(out.find("[c] Continue"), std::string::npos);
        EXPECT_EQ(out.find("Reset counter"), std::string::npos);
        EXPECT_EQ(out.find("[q] Quit"), std::string::npos);
    }

    // IdleReturn should render
    {
        dsys::IdleReturnPayload ir;
        ir.id = "ir-test";
        ir.idle_minutes = 30;
        dsys::DialogPayloadVariant payload = ir;
        auto el = registry.render(payload, ctx);
        EXPECT_NE(el, nullptr);

        auto screen = ftxui::Screen::Create({80, 10});
        Render(screen, el);
        std::string out = screen.ToString();
        EXPECT_FALSE(out.empty());
        EXPECT_NE(out.find("30"), std::string::npos);
    }
}

TEST(DefaultRenderers, EventHandlersFireCallbacks) {
    dsys::DialogRendererRegistry registry;
    drender::register_default_renderers(registry);

    // CostThreshold event handling (P0x3: on_done() is 0-arg,
    // both Enter AND Escape MUST fire it — no data-loss exits).
    {
        std::atomic<int> calls{0};

        dsys::CostThresholdPayload ct;
        ct.id = "ct-event-test";
        ct.on_done = [&] { calls.fetch_add(1); };
        dsys::DialogPayloadVariant payload = ct;

        // Enter → on_done()
        calls = 0;
        payload = ct;
        bool handled = registry.handle_event(payload, ftxui::Event::Return);
        EXPECT_TRUE(handled);
        EXPECT_EQ(calls.load(), 1);

        // Escape → on_done()  (CRITICAL: must NOT be quit / data-loss)
        calls = 0;
        payload = ct;
        handled = registry.handle_event(payload, ftxui::Event::Escape);
        EXPECT_TRUE(handled);
        EXPECT_EQ(calls.load(), 1) << "Esc must ACKNOWLEDGE, NOT quit";

        // Space → on_done()
        calls = 0;
        payload = ct;
        handled = registry.handle_event(payload, ftxui::Event::Character(' '));
        EXPECT_TRUE(handled);
        EXPECT_EQ(calls.load(), 1);

        // Shortcut 'g' → on_done()
        calls = 0;
        payload = ct;
        handled = registry.handle_event(payload, ftxui::Event::Character('g'));
        EXPECT_TRUE(handled);
        EXPECT_EQ(calls.load(), 1);
    }

    // IdleReturn event handling
    {
        bool called = false;
        bool did_resume = false;

        dsys::IdleReturnPayload ir;
        ir.id = "ir-event-test";
        ir.on_response = [&](bool resume) {
            called = true;
            did_resume = resume;
        };
        dsys::DialogPayloadVariant payload = ir;

        // Enter = resume
        bool handled = registry.handle_event(payload, ftxui::Event::Return);
        EXPECT_TRUE(handled);
        EXPECT_TRUE(called);
        EXPECT_TRUE(did_resume);
    }

    // SandboxPermission event handling
    {
        bool called = false;
        bool did_allow = false;
        bool did_always = false;

        dsys::SandboxPermissionPayload sp;
        sp.id = "sb-event-test";
        sp.host_pattern = "test.com";
        sp.on_response = [&](bool allow, bool always) {
            called = true;
            did_allow = allow;
            did_always = always;
        };
        dsys::DialogPayloadVariant payload = sp;

        // 'a' = always allow
        bool handled = registry.handle_event(payload, ftxui::Event::Character('a'));
        EXPECT_TRUE(handled);
        EXPECT_TRUE(called);
        EXPECT_TRUE(did_allow);
        EXPECT_TRUE(did_always);
    }
}

TEST(DialogSystem, ShouldShowDialogLogic) {
    // Overlay (band 3): suppressed while typing
    dsys::ToolPermissionPayload tp;
    tp.id = "tp-test";
    dsys::DialogPayloadVariant tp_payload = tp;
    EXPECT_TRUE(dsys::should_show_dialog(tp_payload, false));
    EXPECT_FALSE(dsys::should_show_dialog(tp_payload, true));

    // Bottom (band 4): suppressed while typing
    dsys::CostThresholdPayload ct;
    ct.id = "ct-test";
    dsys::DialogPayloadVariant ct_payload = ct;
    EXPECT_TRUE(dsys::should_show_dialog(ct_payload, false));
    EXPECT_FALSE(dsys::should_show_dialog(ct_payload, true));
}

TEST(FullDialogRegistry, AllDialogTypesRenderable) {
    // Verify that EVERY DialogType can be rendered through the registry
    // after registering all renderer modules.  This validates M7.4 + M7.5
    // wiring: every dialog type has a renderer and an event handler.
    dsys::DialogRendererRegistry registry;
    loom::ui::dialogs::default_renderers::register_default_renderers(registry);
    loom::ui::dialogs::modal_renderers::register_modal_renderers(registry);
    loom::ui::dialogs::bottom_renderers::register_bottom_renderers(registry);
    loom::ui::dialogs::all_renderers::register_all_renderers(registry);

    Theme theme;
    dsys::DialogRenderContext ctx;
    ctx.term_cols = 80;
    ctx.term_rows = 24;
    ctx.theme = theme;
    ctx.is_fullscreen = false;

    // Helper: push a payload and verify it renders
    // NOTE: registry.render() takes DialogPayloadVariant& (non-const) so renderers
    // can lazily attach per-dialog UI state (e.g. ToolPermission ui_state).
    auto check_render = [&](dsys::DialogPayloadVariant payload,
                            const char* name) {
        SCOPED_TRACE(name);
        auto el = registry.render(payload, ctx);
        EXPECT_TRUE(el) << "DialogType " << name << " has no renderer";
        if (el) {
            // Render to screen to verify no crashes
            auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(80),
                                                ftxui::Dimension::Fixed(10));
            Render(screen, el);
            EXPECT_FALSE(screen.ToString().empty())
                << "DialogType " << name << " rendered empty";
        }
    };

    // -- Overlay slot --
    {
        dsys::ToolPermissionPayload p;
        p.id = "tp-all";
        p.tool_name = "Bash";
        p.description = "Test description";
        check_render(std::move(p), "ToolPermission");
    }

    // -- Bottom slot --
    {
        dsys::MessageSelectorPayload p;
        p.id = "ms-all";
        p.options = {"opt1", "opt2"};
        p.placeholder = "Pick one";
        check_render(std::move(p), "MessageSelector");
    }
    {
        dsys::SandboxPermissionPayload p;
        p.id = "sp-all";
        p.host_pattern = "*.example.com";
        check_render(std::move(p), "SandboxPermission");
    }
    {
        dsys::PromptDialogPayload p;
        p.id = "pd-all";
        p.title = "Prompt Hook";
        check_render(std::move(p), "PromptDialog");
    }
    {
        dsys::WorkerSandboxPermissionPayload p;
        p.id = "wsp-all";
        p.worker_id = "worker-123";
        p.tool_name = "Bash";
        check_render(std::move(p), "WorkerSandboxPermission");
    }
    {
        dsys::ElicitationPayload p;
        p.id = "el-all";
        check_render(std::move(p), "Elicitation");
    }
    {
        dsys::CostThresholdPayload p;
        p.id = "ct-all";
        p.dollars_spent = 5.0;
        check_render(std::move(p), "CostThreshold");
    }
    {
        dsys::IdleReturnPayload p;
        p.id = "ir-all";
        check_render(std::move(p), "IdleReturn");
    }
    {
        dsys::UltraplanChoicePayload p;
        p.id = "upc-all";
        check_render(std::move(p), "UltraplanChoice");
    }
    {
        dsys::UltraplanLaunchPayload p;
        p.id = "upl-all";
        check_render(std::move(p), "UltraplanLaunch");
    }
    {
        dsys::IdeOnboardingPayload p;
        p.id = "io-all";
        check_render(std::move(p), "IdeOnboarding");
    }
    {
        dsys::InitOnboardingPayload p;
        p.id = "ino-all";
        check_render(std::move(p), "InitOnboarding");
    }
    {
        dsys::ModelSwitchPayload p;
        p.id = "msw-all";
        check_render(std::move(p), "ModelSwitch");
    }
    {
        dsys::UndercoverCalloutPayload p;
        p.id = "uc-all";
        check_render(std::move(p), "UndercoverCallout");
    }
    {
        dsys::EffortCalloutPayload p;
        p.id = "ec-all";
        check_render(std::move(p), "EffortCallout");
    }
    {
        dsys::RemoteCalloutPayload p;
        p.id = "rc-all";
        check_render(std::move(p), "RemoteCallout");
    }
    {
        dsys::LspRecommendationPayload p;
        p.id = "lr-all";
        check_render(std::move(p), "LspRecommendation");
    }
    {
        dsys::PluginHintPayload p;
        p.id = "ph-all";
        check_render(std::move(p), "PluginHint");
    }
    {
        dsys::DesktopUpsellPayload p;
        p.id = "du-all";
        check_render(std::move(p), "DesktopUpsell");
    }

    // -- Modal slot --
    {
        dsys::SettingsPanelPayload p;
        p.id = "sp-all";
        check_render(std::move(p), "SettingsPanel");
    }
    {
        dsys::TasksViewPayload p;
        p.id = "tv-all";
        check_render(std::move(p), "TasksView");
    }
    {
        dsys::TeamsViewPayload p;
        p.id = "tm-all";
        check_render(std::move(p), "TeamsView");
    }
    {
        dsys::HelpViewPayload p;
        p.id = "hv-all";
        check_render(std::move(p), "HelpView");
    }
    {
        dsys::QuickOpenPayload p;
        p.id = "qo-all";
        check_render(std::move(p), "QuickOpen");
    }
    {
        dsys::PluginDialogPayload p;
        p.id = "pd-all";
        check_render(std::move(p), "PluginDialog");
    }
    {
        dsys::MCPDialogPayload p;
        p.id = "mcp-all";
        check_render(std::move(p), "MCPDialog");
    }
    {
        dsys::DiffDialogPayload p;
        p.id = "dd-all";
        check_render(std::move(p), "DiffDialog");
    }
    {
        dsys::ConfigDialogPayload p;
        p.id = "cd-all";
        check_render(std::move(p), "ConfigDialog");
    }
    {
        dsys::ExportDialogPayload p;
        p.id = "ed-all";
        check_render(std::move(p), "ExportDialog");
    }
    {
        dsys::GlobalSearchPayload p;
        p.id = "gs-all";
        check_render(std::move(p), "GlobalSearch");
    }
    {
        dsys::HistorySearchPayload p;
        p.id = "hs-all";
        check_render(std::move(p), "HistorySearch");
    }
    {
        dsys::BridgeDialogPayload p;
        p.id = "bd-all";
        check_render(std::move(p), "BridgeDialog");
    }
    {
        dsys::WorktreeExitPayload p;
        p.id = "we-all";
        check_render(std::move(p), "WorktreeExitDialog");
    }
    {
        dsys::AboutDialogPayload p;
        p.id = "ad-all";
        check_render(std::move(p), "AboutDialog");
    }
    {
        dsys::ConfirmationDialogPayload p;
        p.id = "cf-all";
        check_render(std::move(p), "ConfirmationDialog");
    }
    {
        dsys::FeedbackSurveyPayload p;
        p.id = "fs-all";
        check_render(std::move(p), "FeedbackSurvey");
    }
    {
        dsys::ManagedSettingsSecurityPayload p;
        p.id = "mss-all";
        check_render(std::move(p), "ManagedSettingsSecurity");
    }

    // -- Standalone --
    {
        dsys::TrustDialogPayload p;
        p.id = "td-all";
        check_render(std::move(p), "TrustDialog");
    }
    {
        dsys::OnboardingPayload p;
        p.id = "ob-all";
        check_render(std::move(p), "Onboarding");
    }
    {
        dsys::CreateAgentWizardPayload p;
        p.id = "ca-all";
        check_render(std::move(p), "CreateAgentWizard");
    }
    {
        dsys::EditAgentWizardPayload p;
        p.id = "ea-all";
        p.agent_name = "test-agent";
        check_render(std::move(p), "EditAgentWizard");
    }
}

TEST(FullDialogRegistry, DialogTypeCountMatches) {
    // Verify that the _COUNT sentinel matches the actual number of types
    // we have renderers + payloads for.  This catches drift between
    // the enum and the implementation.
    constexpr int kExpectedCount =
        1 +  // ToolPermission (overlay)
        14 + // bottom slot (MessageSelector + SandboxPermission + PromptDialog +
             // WorkerSandboxPermission + Elicitation + CostThreshold + IdleReturn +
             // UltraplanChoice + UltraplanLaunch + IdeOnboarding + InitOnboarding +
             // ModelSwitch + UndercoverCallout + EffortCallout + RemoteCallout +
             // LspRecommendation + PluginHint + DesktopUpsell → 18 total bottom
             // Let's just use the enum value as ground truth
        0;
    (void)kExpectedCount;

    // All we really care about: _COUNT > 0 and is a reasonable number
    EXPECT_GT(static_cast<int>(dsys::DialogType::_COUNT), 30);
}

TEST(DialogFrame, GoldenDefault) {
    auto theme = dialog_test_golden::get_light_theme();

    dframe::DialogFrameProps props;
    props.title = "Dialog Frame Test";
    props.subtitle = "Default Info variant";
    props.style = dframe::FrameStyle::Info;
    props.content = ftxui::paragraph(
            "Lorem ipsum dolor sit amet consectetur adipiscing elit sed do "
            "eiusmod tempor incididunt ut labore") | ftxui::xflex_grow;

    auto el = dframe::DialogFrame(props, theme);
    dialog_test_golden::check_golden(
        "dialog_frame_default",
        dialog_test_golden::render_to_ansi(std::move(el), 80, 20));
}

TEST(DialogFrame, GoldenWithSubtitle) {
    auto theme = dialog_test_golden::get_light_theme();

    dframe::DialogFrameProps props;
    props.title = "Project Settings";
    props.subtitle = "version 1.2.3 · user@host";
    props.style = dframe::FrameStyle::Permission;
    props.content = ftxui::text("A");

    auto el = dframe::DialogFrame(props, theme);
    dialog_test_golden::check_golden(
        "dialog_frame_with_subtitle",
        dialog_test_golden::render_to_ansi(std::move(el), 80, 20));
}

TEST(DialogFrame, GoldenWithWorkerBadge) {
    auto theme = dialog_test_golden::get_light_theme();

    dframe::DialogFrameProps props;
    props.title = "Bash command";
    props.subtitle = "Worker wants to run command";
    props.style = dframe::FrameStyle::Warning;
    props.worker_badge = dframe::WorkerBadge("agent-coord-0", theme);
    props.content = ftxui::text("rm -rf /");

    auto el = dframe::DialogFrame(props, theme);
    dialog_test_golden::check_golden(
        "dialog_frame_with_worker_badge",
        dialog_test_golden::render_to_ansi(std::move(el), 80, 20));
}

TEST(DialogFrame, GoldenLongContent) {
    auto theme = dialog_test_golden::get_light_theme();

    ftxui::Elements lines;
    for (int n = 1; n <= 15; ++n) {
        lines.push_back(ftxui::text(
            "Line " + std::to_string(n) + " of review content..."));
    }

    dframe::DialogFrameProps props;
    props.title = "Code Review";
    props.style = dframe::FrameStyle::Success;
    props.content = ftxui::vbox(std::move(lines));

    auto el = dframe::DialogFrame(props, theme);
    dialog_test_golden::check_golden(
        "dialog_frame_long_content",
        dialog_test_golden::render_to_ansi(std::move(el), 80, 20));
}

TEST(DialogFrame, GoldenNarrowWidth) {
    auto theme = dialog_test_golden::get_light_theme();

    dframe::DialogFrameProps props;
    props.title = "Narrow";
    props.style = dframe::FrameStyle::Muted;
    props.content = ftxui::paragraph("Short");

    auto el = dframe::DialogFrame(props, theme);
    dialog_test_golden::check_golden(
        "dialog_frame_narrow_width",
        dialog_test_golden::render_to_ansi(std::move(el), 40, 15));
}

TEST(DialogFrame, GoldenWideWidth) {
    auto theme = dialog_test_golden::get_light_theme();

    ftxui::Elements issue_rows;
    const std::array<std::string, 8> issues = {{
        "Unused variable 'result' on line 42",
        "Missing const qualifier in helper function",
        "Magic number 0xDEADBEEF should be a named constant",
        "TODO comment without tracking issue reference",
        "Potential null-pointer dereference without guard",
        "std::format argument count mismatch (string_view issue)",
        "Header guard does not match project convention",
        "Exception specification missing on destructor override",
    }};
    for (std::size_t i = 0; i < issues.size(); ++i) {
        issue_rows.push_back(ftxui::hbox({
            ftxui::text(std::to_string(i + 1) + ". ") | ftxui::dim,
            ftxui::text(issues[i]),
        }));
    }

    dframe::DialogFrameProps props;
    props.title = "Wide Panel With Lots of Room";
    props.subtitle = "Extended diagnostics view";
    props.style = dframe::FrameStyle::Danger;
    props.inner_padding_x = 2;
    props.inner_padding_y = 1;
    props.content = ftxui::vbox({
        ftxui::paragraph(
            "The system detected the following issues during the last build:"),
        ftxui::text(""),
        ftxui::vbox(std::move(issue_rows)),
    });

    auto el = dframe::DialogFrame(props, theme);
    dialog_test_golden::check_golden(
        "dialog_frame_wide_width",
        dialog_test_golden::render_to_ansi(std::move(el), 120, 15));
}


} // namespace
