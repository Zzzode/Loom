/// @file test_dialog_renderers.cpp
/// @brief Dialog renderer golden tests, sandbox permission events, and
/// REPL screen integration. Split from test_dialog_system.cpp (SLOC budget).

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

// FTXUI element builders used unqualified in renderer lambdas below.
using ftxui::bold;
using ftxui::center;
using ftxui::color;
using ftxui::dim;
using ftxui::filler;
using ftxui::hbox;
using ftxui::paragraph;
using ftxui::text;
using ftxui::vbox;
using ftxui::xflex;
using ftxui::yflex;
using ftxui::xflex_grow;

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


TEST(DialogRenderers, Golden_ToolPermissionOverlay) {
    auto theme = dialog_test_golden::get_light_theme();

    // Hermetic renderer: uses payload fields that are always populated and
    // captures command/cwd directly in the lambda to avoid requiring an
    // explicit import of loom.ui.permissions.single_prompt in this TU.
    const std::string kCommand = "rm -rf node_modules/";
    const std::string kCwd     = "/home/user/proj";

    dsys::DialogRendererRegistry registry;
    registry.register_dialog(dsys::DialogType::ToolPermission,
        [&, kCommand, kCwd](dsys::DialogPayloadVariant& v,
                            const dsys::DialogRenderContext&) -> ftxui::Element {
            const auto* p = std::get_if<dsys::ToolPermissionPayload>(&v);
            if (!p) return text("");

            dframe::DialogFrameProps props;
            props.title = "Bash — Permission Required";
            props.subtitle = "High-risk command · needs manual approval";
            props.style = dframe::FrameStyle::Danger;
            props.content = vbox({
                hbox({
                    text("Tool: ") | dim,
                    text(p->tool_name) | bold,
                }),
                text(""),
                // command + cwd scope table
                hbox({
                    text("Command: ") | dim,
                    text(kCommand) | bold | color(ftxui::Color::Red),
                }),
                hbox({
                    text("Cwd: ") | dim,
                    text(kCwd),
                }),
                text(""),
                paragraph(p->description),
                text(""),
                            hbox({
                    text(" [y] Allow once") | color(ftxui::Color::Green),
                    p->can_always_allow
                        ? text("  [a] Always allow") | color(ftxui::Color::Cyan)
                        : text(""),
                    text("  [n] Deny") | color(ftxui::Color::Red),
                    text("  [Esc] Cancel") | dim,
                }),
            });
            return dframe::DialogFrame(props, theme);
        });

    // Build the payload (fields accessible without single_prompt import).
    dsys::ToolPermissionPayload tp;
    tp.id = "tp-gold-1";
    tp.tool_name = "bash";
    tp.description = "Run destructive command";
    tp.workspace_root = kCwd;
    tp.can_always_allow = true;
    // tp.risk_level and tp.detail default-construct and are not read by
    // this hermetic renderer (which captures data directly above).

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 100;
    ctx.term_rows = 30;
    ctx.theme = theme;

    dsys::DialogPayloadVariant variant{tp};
    auto el = registry.render(variant, ctx);
    ASSERT_NE(el, nullptr);
    dialog_test_golden::check_golden(
        "dialog_tool_permission_overlay",
        dialog_test_golden::render_to_ansi(std::move(el), 100, 30));
}

TEST(DialogRenderers, Golden_SandboxPermission) {
    // FAITHFUL PORT: uses loom.ui.dialogs.sandbox_permission (1:1 TS layout)
    // rather than the pre-existing stub renderer.

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    auto theme = dialog_test_golden::get_light_theme();

    dsys::DialogRendererRegistry registry;
    registry.register_dialog(dsys::DialogType::SandboxPermission,
        [](dsys::DialogPayloadVariant& v,
           const dsys::DialogRenderContext& ctx) -> ftxui::Element {
            const auto* p = std::get_if<dsys::SandboxPermissionPayload>(&v);
            if (!p) return text("");
            // Delegate to the faithful renderer (TS SandboxPermissionRequest.tsx)
            return sbp::RenderDefault(*p, ctx);
        },
        [](dsys::DialogPayloadVariant& v,
           const ftxui::Event& e) -> bool {
            auto* p = std::get_if<dsys::SandboxPermissionPayload>(&v);
            if (!p) return false;
            return sbp::HandleSandboxPermissionEvent(*p, e);
        });

    dsys::SandboxPermissionPayload sp;
    sp.id = "sb-gold-1";
    sp.host_pattern = "*.github.com";
    // managed_domains_only = false → all 3 options visible (TS default)
    sp.managed_domains_only = false;
    sp.focused_index = 0;   // "Yes" focused
    sp.on_response = [](bool, bool) {};

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 100;
    ctx.term_rows = 30;
    ctx.theme = theme;

    dsys::DialogPayloadVariant variant{sp};
    auto el = registry.render(variant, ctx);
    ASSERT_NE(el, nullptr);
    dialog_test_golden::check_golden(
        "dialog_sandbox_permission",
        dialog_test_golden::render_to_ansi(std::move(el), 100, 30));
}

TEST(SandboxPermissionEvents, KeyY_AllowsOnce) {

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    p.host_pattern = "*.example.com";
    bool allow = false, always = true;
    int fired = 0;
    p.on_response = [&](bool a, bool al) { allow = a; always = al; ++fired; };

    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Character('y')));
    EXPECT_EQ(fired, 1);
    EXPECT_TRUE(allow);
    EXPECT_FALSE(always);
}

TEST(SandboxPermissionEvents, KeyY_CaseInsensitive) {

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    int fired = 0;
    bool allow = false, always = true;
    p.on_response = [&](bool a, bool al) { allow = a; always = al; ++fired; };

    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Character('Y')));
    EXPECT_EQ(fired, 1);
    EXPECT_TRUE(allow);
    EXPECT_FALSE(always);
}

TEST(SandboxPermissionEvents, Enter_AllowsOnce) {

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    int fired = 0;
    bool allow = false, always = true;
    p.on_response = [&](bool a, bool al) { allow = a; always = al; ++fired; };

    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Return));
    EXPECT_EQ(fired, 1);
    EXPECT_TRUE(allow);
    EXPECT_FALSE(always);
}

TEST(SandboxPermissionEvents, KeyA_AlwaysAllow) {

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    p.managed_domains_only = false;
    int fired = 0;
    bool allow = false, always = false;
    p.on_response = [&](bool a, bool al) { allow = a; always = al; ++fired; };

    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Character('a')));
    EXPECT_EQ(fired, 1);
    EXPECT_TRUE(allow);
    EXPECT_TRUE(always);
}

TEST(SandboxPermissionEvents, KeyA_SuppressedWhenManagedDomainsOnly) {

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    p.managed_domains_only = true;
    int fired = 0;
    p.on_response = [&](bool, bool) { ++fired; };

    // Key is consumed (not leaked to prompt) but callback MUST NOT fire.
    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Character('a')));
    EXPECT_EQ(fired, 0);

    // Same for 'A'.
    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Character('A')));
    EXPECT_EQ(fired, 0);
}

TEST(SandboxPermissionEvents, KeyN_Denies) {

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    int fired = 0;
    bool allow = true, always = true;
    p.on_response = [&](bool a, bool al) { allow = a; always = al; ++fired; };

    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Character('n')));
    EXPECT_EQ(fired, 1);
    EXPECT_FALSE(allow);
    EXPECT_FALSE(always);

    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Character('N')));
    EXPECT_EQ(fired, 2);
}

TEST(SandboxPermissionEvents, Escape_DeniesLikeOnCancel) {
    // TS: onCancel → onUserResponse({allow:false, persistToSettings:false})

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    int fired = 0;
    bool allow = true, always = true;
    p.on_response = [&](bool a, bool al) { allow = a; always = al; ++fired; };

    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Escape));
    EXPECT_EQ(fired, 1);
    EXPECT_FALSE(allow);
    EXPECT_FALSE(always);
}

TEST(SandboxPermissionEvents, ArrowDown_WrapsFocusThroughAllOptions) {

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    p.managed_domains_only = false;  // 3 options: 0=Yes, 1=YesAlways, 2=No

    // Default focused_index unset → ArrowDown should go to 1
    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::ArrowDown));
    ASSERT_TRUE(p.focused_index.has_value());
    EXPECT_EQ(*p.focused_index, 1);

    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::ArrowDown));
    EXPECT_EQ(*p.focused_index, 2);

    // Wraps to 0
    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::ArrowDown));
    EXPECT_EQ(*p.focused_index, 0);
}

TEST(SandboxPermissionEvents, ArrowUp_WrapsFocusBackwards) {

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    p.managed_domains_only = false;
    p.focused_index = 0;

    // 0 → (0 + 3 - 1) % 3 = 2
    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::ArrowUp));
    EXPECT_EQ(*p.focused_index, 2);

    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::ArrowUp));
    EXPECT_EQ(*p.focused_index, 1);
}

TEST(SandboxPermissionEvents, VimJK_MoveFocus) {

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    p.managed_domains_only = false;
    p.focused_index = 0;

    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Character('j')));
    EXPECT_EQ(*p.focused_index, 1);

    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Character('k')));
    EXPECT_EQ(*p.focused_index, 0);
}

TEST(SandboxPermissionEvents, ManagedDomainsOnly_TwoOptionsWrapCorrectly) {

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    p.managed_domains_only = true;  // 2 options: 0=Yes, 1=No
    p.focused_index = 0;

    // ArrowDown: 0 → 1 → wrap → 0
    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::ArrowDown));
    EXPECT_EQ(*p.focused_index, 1);
    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::ArrowDown));
    EXPECT_EQ(*p.focused_index, 0);

    // ArrowUp: 0 → wrap → 1
    EXPECT_TRUE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::ArrowUp));
    EXPECT_EQ(*p.focused_index, 1);
}

TEST(SandboxPermissionEvents, UnrelatedKeys_NotConsumed) {

    namespace sbp = loom::ui::dialogs::sandbox_permission;

    dsys::SandboxPermissionPayload p;
    int fired = 0;
    p.on_response = [&](bool, bool) { ++fired; };

    // Space and Tab are not Sandbox shortcuts.
    EXPECT_FALSE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Character(' ')));
    EXPECT_FALSE(sbp::HandleSandboxPermissionEvent(p, ftxui::Event::Tab));
    EXPECT_EQ(fired, 0);
}

TEST(DialogRenderers, Golden_PromptDialog) {
    auto theme = dialog_test_golden::get_light_theme();

    dsys::DialogRendererRegistry registry;
    registry.register_dialog(dsys::DialogType::PromptDialog,
        [&](dsys::DialogPayloadVariant& v,
            const dsys::DialogRenderContext&) -> ftxui::Element {
            const auto* p = std::get_if<dsys::PromptDialogPayload>(&v);
            if (!p) return text("");

            dframe::DialogFrameProps props;
            props.title = p->title.empty()
                ? std::string{"Input Required"} : p->title;
            props.subtitle = "Loom needs clarification";
            props.style = dframe::FrameStyle::Info;

            auto def = p->default_value
                ? hbox({text("[") | dim, text(*p->default_value), text("]") | dim})
                : text("");

            props.content = vbox({
                paragraph(p->prompt_text),
                text(""),
                def,
                text(""),
                // accept / cancel rows
                hbox({
                    text(" [Enter] Submit") | color(ftxui::Color::Green),
                    text("  [Esc] Cancel") | color(ftxui::Color::Red),
                }),
            });
            return dframe::DialogFrame(props, theme);
        });

    dsys::PromptDialogPayload pd;
    pd.id = "prompt-gold-1";
    pd.title = "Follow-up question";
    pd.prompt_text =
        "You asked for unit tests, but didn't specify a test framework. "
        "Which framework should I use? (leave blank for project default)";
    pd.default_value = "gtest";
    pd.on_response = [](std::optional<std::string>) {};

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 100;
    ctx.term_rows = 30;
    ctx.theme = theme;

    dsys::DialogPayloadVariant variant{pd};
    auto el = registry.render(variant, ctx);
    ASSERT_NE(el, nullptr);
    dialog_test_golden::check_golden(
        "dialog_prompt",
        dialog_test_golden::render_to_ansi(std::move(el), 100, 30));
}

TEST(DialogRenderers, Golden_Elicitation) {
    auto theme = dialog_test_golden::get_light_theme();

    dsys::DialogRendererRegistry registry;
    registry.register_dialog(dsys::DialogType::Elicitation,
        [&](dsys::DialogPayloadVariant& v,
            const dsys::DialogRenderContext&) -> ftxui::Element {
            const auto* p = std::get_if<dsys::ElicitationPayload>(&v);
            if (!p) return text("");

            dframe::DialogFrameProps props;
            props.title = "MCP Server Request";
            props.subtitle = p->server_name + " needs additional input";
            props.style = dframe::FrameStyle::Info;
            props.content = vbox({
                hbox({
                    text("Server: ") | dim,
                    text(p->server_name) | bold,
                }),
                text(""),
                paragraph(p->request_description),
                text(""),
                hbox({
                    text("Request ID: ") | dim,
                    text(std::to_string(p->request_id)) | dim,
                }),
                text(""),
                hbox({
                    text(" [Enter] Approve") | color(ftxui::Color::Green),
                    text("  [Esc] Deny") | color(ftxui::Color::Red),
                }),
            });
            return dframe::DialogFrame(props, theme);
        });

    dsys::ElicitationPayload elp;
    elp.id = "elicit-gold-1";
    elp.server_name = "github.com";
    elp.request_description =
        "The github MCP server needs a repository owner and name before it "
        "can list pull requests.  Please provide these values.";
    elp.request_id = 42;
    elp.on_response = [](bool) {};

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 100;
    ctx.term_rows = 30;
    ctx.theme = theme;

    dsys::DialogPayloadVariant variant{elp};
    auto el = registry.render(variant, ctx);
    ASSERT_NE(el, nullptr);
    dialog_test_golden::check_golden(
        "dialog_elicitation",
        dialog_test_golden::render_to_ansi(std::move(el), 100, 30));
}

TEST(DialogRenderers, Golden_CostThreshold) {
    // P0x3 contract — CostThreshold render MUST be delegated to the unified
    // loom.ui.dialogs.cost_threshold_dialog module (single source of truth).
    // No locally-fabricated chrome (Continue / Reset counter / Quit) is
    // permitted.  See the dedicated golden snapshots in
    // test_cost_threshold_dialog.cpp:
    //   - cost_threshold_title_with_interpolated_dollars
    //   - cost_threshold_with_docs_link_rendered
    //
    // This test simply verifies that the registered renderer emits the
    // contractually-required content when driven through the registry.
    auto theme = dialog_test_golden::get_light_theme();

    dsys::DialogRendererRegistry registry;
    drender::register_default_renderers(registry);

    dsys::CostThresholdPayload ct;
    ct.id = "cost-gold-1";
    ct.dollars_spent = 4.7;        // rounds to $5 per %.0f
    ct.model_name = "test-model";
    std::atomic<int> done_calls{0};
    ct.on_done = [&] { done_calls.fetch_add(1); };

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 100;
    ctx.term_rows = 30;
    ctx.theme = theme;

    dsys::DialogPayloadVariant variant{ct};
    auto el = registry.render(variant, ctx);
    ASSERT_NE(el, nullptr);

    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(100), ftxui::Dimension::Fixed(30));
    ftxui::Render(screen, el);
    const std::string out = screen.ToString();

    // Contractually required content.
    EXPECT_NE(out.find("You've spent $5 on the API this session."),
              std::string::npos);
    EXPECT_NE(out.find("Learn more about how to monitor your spending:"),
              std::string::npos);
    EXPECT_EQ(out.find("code.loom.com"), std::string::npos)
        << "no docs host is configured, so no link may be rendered";
    EXPECT_NE(out.find("(model: test-model)"),
              std::string::npos);
    EXPECT_NE(out.find("Got it, thanks!"), std::string::npos);

    // Fabricated 3-action chrome MUST be absent (P0).
    EXPECT_EQ(out.find("[c] Continue"),      std::string::npos);
    EXPECT_EQ(out.find("Reset counter"),     std::string::npos);
    EXPECT_EQ(out.find("[q] Quit"),          std::string::npos);

    // Keyboard — both Enter and Escape MUST call on_done() (no data-loss).
    variant = ct;  // reset variant to fresh payload
    done_calls = 0;
    EXPECT_TRUE(registry.handle_event(variant, ftxui::Event::Return));
    EXPECT_EQ(done_calls.load(), 1);

    variant = ct;
    done_calls = 0;
    EXPECT_TRUE(registry.handle_event(variant, ftxui::Event::Escape));
    EXPECT_EQ(done_calls.load(), 1)
        << "Escape MUST ACKNOWLEDGE via on_done() — NOT quit (data loss).";
}

TEST(DialogRenderers, CostThresholdDocsLinkFollowsConfiguredBase) {
    ::unsetenv("LOOM_DOCS_BASE");
    EXPECT_EQ(loom::constants::product::doc_url("/docs/en/costs"), "")
        << "no base configured => no link";

    ::setenv("LOOM_DOCS_BASE", "https://docs.example.test", 1);
    const auto configured = loom::constants::product::doc_url("/docs/en/costs");
    ::unsetenv("LOOM_DOCS_BASE");
    EXPECT_EQ(configured, "https://docs.example.test/docs/en/costs");
}

TEST(DialogRenderers, Golden_IdleReturn) {
    auto theme = dialog_test_golden::get_light_theme();

    dsys::DialogRendererRegistry registry;
    registry.register_dialog(dsys::DialogType::IdleReturn,
        [&](dsys::DialogPayloadVariant& v,
            const dsys::DialogRenderContext&) -> ftxui::Element {
            const auto* p = std::get_if<dsys::IdleReturnPayload>(&v);
            if (!p) return text("");

            dframe::DialogFrameProps props;
            props.title = "Session Idle";
            props.subtitle = std::format(
                "Returning to idle in {} minutes — click Cancel to stay",
                p->idle_minutes);
            props.style = dframe::FrameStyle::Muted;
            props.content = vbox({
                text(std::format(
                    "Your session has been idle for {} minutes.",
                    p->idle_minutes)) | center,
                text(""),
                paragraph(
                    "Resuming will re-hydrate the conversation context and "
                    "continue from where you left off."),
                text(""),
                hbox({
                    filler(),
                    text(" [Enter] Resume") | color(ftxui::Color::Green),
                    text("  [n] Cancel (stay)") | color(ftxui::Color::Cyan),
                    filler(),
                }) | center,
            });
            return dframe::DialogFrame(props, theme);
        });

    dsys::IdleReturnPayload ir;
    ir.id = "idle-gold-1";
    ir.idle_minutes = 15;
    ir.on_response = [](bool) {};

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 100;
    ctx.term_rows = 30;
    ctx.theme = theme;

    dsys::DialogPayloadVariant variant{ir};
    auto el = registry.render(variant, ctx);
    ASSERT_NE(el, nullptr);
    dialog_test_golden::check_golden(
        "dialog_idle_return",
        dialog_test_golden::render_to_ansi(std::move(el), 100, 30));
}

TEST(DialogRenderers, Golden_SettingsPanel) {
    auto theme = dialog_test_golden::get_light_theme();

    dsys::DialogRendererRegistry registry;
    registry.register_dialog(dsys::DialogType::SettingsPanel,
        [&](dsys::DialogPayloadVariant& v,
            const dsys::DialogRenderContext&) -> ftxui::Element {
            const auto* p = std::get_if<dsys::SettingsPanelPayload>(&v);
            if (!p) return text("");

            dframe::DialogFrameProps props;
            props.title = "Settings";
            props.subtitle = "Tab: " + p->initial_tab;
            props.style = dframe::FrameStyle::Permission;
            props.inner_padding_x = 2;
            props.inner_padding_y = 1;
            props.content = vbox({
                // 3 rows: Model, API base, Max tokens
                hbox({
                    text("  Model") | dim | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 22),
                    text("test-model.6") | bold,
                    filler(),
                }),
                hbox({
                    text("  API base") | dim
                        | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 22),
                    text("(no default)"),
                    filler(),
                }),
                hbox({
                    text("  Max tokens") | dim
                        | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 22),
                    text("8192"),
                    filler(),
                }),
                text(""),
                hbox({
                    text("  Esc") | dim,
                    text(" to close") | dim,
                    filler(),
                }),
            });
            return dframe::DialogFrame(props, theme);
        });

    dsys::SettingsPanelPayload sp;
    sp.id = "settings-gold-1";
    sp.initial_tab = "general";
    sp.on_close = []() {};

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 100;
    ctx.term_rows = 30;
    ctx.theme = theme;

    dsys::DialogPayloadVariant variant{sp};
    auto el = registry.render(variant, ctx);
    ASSERT_NE(el, nullptr);
    dialog_test_golden::check_golden(
        "dialog_settings_panel",
        dialog_test_golden::render_to_ansi(std::move(el), 100, 30));
}

// ============================================================
// Step 6B: E2E integration test — ReplScreenState-like struct +
// queue push/typing suppression with a thin local RenderBottomDialog
// helper that mirrors repl_screen::dialog_queue_render logic exactly
// (~15 lines) so no new module import is required.
// ============================================================

/// Minimal replica of ReplScreenState's dialog-relevant fields.
/// We intentionally do NOT import loom.ui.screens.repl_screen to avoid
/// build-system churn (CMake INTERFACE→FILE_SET BMI propagation gaps).
struct MiniReplState {
    dsys::DialogQueue dialog_queue;
    dsys::DialogRendererRegistry dialog_renderers;
    bool is_prompt_input_active = false;
    bool is_tool_animation_active = false;
};

/// Local mirror of dialog_queue_render::RenderBottomDialog.
/// Logic: peek_bottom(is_typing, !is_animation) -> render via registry ->
/// clamp to ~40% of terminal rows.
[[nodiscard]] inline std::optional<ftxui::Element> LocalRenderBottomDialog(
    MiniReplState& s,        // mutable so renderers can attach UI state
    int term_cols,
    int term_rows)
{
    auto& queue = s.dialog_queue;   // mutable for peek_bottom_mut (renderer may attach UI state)
    if (!queue.has_any_bottom()) return std::nullopt;

    auto payload_opt = queue.peek_bottom_mut(
        /*is_prompt_input_active=*/s.is_prompt_input_active,
        /*allow_dialogs_with_animation=*/!s.is_tool_animation_active);
    if (!payload_opt) return std::nullopt;
    auto& payload = payload_opt->get();

    dsys::DialogRenderContext ctx;
    ctx.term_cols = term_cols;
    ctx.term_rows = term_rows;

    auto el = s.dialog_renderers.render(payload, ctx);
    if (!el) return std::nullopt;

    // Bottom-slot height clamp (~40% of rows)
    return el | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN,
                            std::max(4, term_rows * 2 / 5));
}

TEST(ReplScreenIntegration, TypingSuppressionSkipsBottomDialogs) {
    MiniReplState s;

    // Register a minimal renderer for every bottom-slot type we push:
    // SandboxPermission (Band2), PromptDialog (Band3), CostThreshold (Band4).
    // Each just renders the type name so visual inspection can tell which
    // dialog was rendered.
    auto reg = [&](dsys::DialogType ty, std::string label) {
        s.dialog_renderers.register_renderer(ty,
            [label = std::move(label)](
                dsys::DialogPayloadVariant&,
                const dsys::DialogRenderContext&) -> ftxui::Element {
                return ftxui::window(
                    ftxui::text(" " + label + " "),
                    ftxui::text(label + " content") | xflex);
            });
    };
    reg(dsys::DialogType::SandboxPermission, "SandboxPermission");
    reg(dsys::DialogType::PromptDialog,      "PromptDialog");
    reg(dsys::DialogType::CostThreshold,     "CostThreshold");

    // Push SandboxPermission (Band2), PromptDialog (Band3), CostThreshold (Band4)
    {
        dsys::SandboxPermissionPayload sbx;
        sbx.id = "sbx-int";
        sbx.host_pattern = "*.example.com";
        s.dialog_queue.push(dsys::DialogPayloadVariant{sbx});
    }
    {
        dsys::PromptDialogPayload pd;
        pd.id = "pd-int";
        pd.title = "Prompt";
        pd.prompt_text = "?";
        s.dialog_queue.push(dsys::DialogPayloadVariant{pd});
    }
    {
        dsys::CostThresholdPayload ct;
        ct.id = "ct-int";
        ct.dollars_spent = 7.23;
        s.dialog_queue.push(dsys::DialogPayloadVariant{ct});
    }

    // (A) typing OFF, animation OFF → Sandbox (Band2, highest) should show
    auto el = LocalRenderBottomDialog(s, 100, 40);
    EXPECT_TRUE(el.has_value())
        << "(A) Sandbox Band2 should show with typing=OFF, animation=OFF";

    // (B) typing ON → ALL Band2..6 suppressed → nullopt
    s.is_prompt_input_active = true;
    el = LocalRenderBottomDialog(s, 100, 40);
    EXPECT_FALSE(el.has_value())
        << "(B) Everything must be suppressed while typing is active";

    // (C) typing OFF, animation ON → Band3 (Prompt) skipped,
    //     Band2 still shows
    s.is_prompt_input_active = false;
    s.is_tool_animation_active = true;
    el = LocalRenderBottomDialog(s, 100, 40);
    EXPECT_TRUE(el.has_value())
        << "(C) Band2 SandboxPermission must NOT be suppressed by animation flag";

    // Pop SandboxPermission using the same suppression rules.
    s.dialog_queue.pop_bottom(
        /*typing=*/false,
        /*allow_dialogs_with_animation=*/!s.is_tool_animation_active);
    // Now the queue still contains PromptDialog (Band3, skipped) +
    // CostThreshold (Band4, reachable).
    // Next peek → CostThreshold (Band4), NOT PromptDialog (Band3, skipped).
    el = LocalRenderBottomDialog(s, 100, 40);
    EXPECT_TRUE(el.has_value())
        << "(C-post-pop) Band4 CostThreshold must be reachable when Band3 "
           "PromptDialog is skipped for animation";

    // (D) remove CostThreshold too — next: PromptDialog (still Band3, skipped)
    s.dialog_queue.pop_bottom(false, /*animation_ok=*/false);
    el = LocalRenderBottomDialog(s, 100, 40);
    EXPECT_FALSE(el.has_value())
        << "(D) After CostThreshold is removed, only Band3 PromptDialog "
           "remains; it must be suppressed while animation is ON";

    // Turn animation OFF → PromptDialog (Band3) now shows.
    s.is_tool_animation_active = false;
    el = LocalRenderBottomDialog(s, 100, 40);
    EXPECT_TRUE(el.has_value())
        << "(D-anim-off) PromptDialog Band3 must show when animation goes OFF";

    // (E) typing ON, animation ON → both suppression paths active → nullopt
    s.is_prompt_input_active = true;
    s.is_tool_animation_active = true;
    el = LocalRenderBottomDialog(s, 100, 40);
    EXPECT_FALSE(el.has_value())
        << "(E) typing+animation both ON must suppress every bottom dialog";
}


} // namespace
