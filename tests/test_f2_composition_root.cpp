/// @file test_f2_composition_root.cpp
/// @brief RFC 0002 F2 composition-root tests.
///
/// F2 cut 4 (doctor renderer inversion): dialog_default_renderers no longer
/// imports doctor_screen. The doctor renderer registration lives on the
/// screens side (cc.ui.screens.doctor_dialog_registration) and is wired by the
/// composition root. These tests prove register_doctor_renderer() adds a
/// renderer that register_default_renderers() alone does not, and that the
/// doctor dialog renders through DialogRendererRegistry (not the fallback).
///
/// F2 cut 6 (feature-dialog protocol inversion): feature modules no longer
/// import wizard_dialog / trust_dialog. They build a neutral request and
/// resolve a factory by ViewKind from cc.ui.foundation.feature_dialog_protocol.
/// The composition root (cc.ui.app.app_dialog_registration) registers the
/// concrete factories. These tests prove the register/resolve/invoke path
/// end-to-end and that the dialogs-side adapter (feature_wizard_adapter)
/// converts a neutral FeatureWizardRequest into a wizard component.
///
/// This TU does NOT import cc.ui.dialogs.wizard_dialog or
/// cc.ui.dialogs.trust_dialog directly — the whole point is that the flows
/// go through the protocol leaf. The wizard_dialog import is transitive
/// through the dialogs-side adapter (the dialogs half of the inversion);
/// the trust_dialog import is hidden inside the composition-root impl unit.

#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>

import std;
import loom.ui.dialogs.system;
import loom.ui.dialogs.default_renderers;
import loom.ui.screens.doctor_dialog_registration;
import loom.ui.foundation.feature_dialog_protocol;
import loom.ui.dialogs.feature_wizard_adapter;
import loom.ui.app.app_dialog_registration;

namespace {

namespace dsys = cc::ui::dialogs::system;
namespace drender = cc::ui::dialogs::default_renderers;
namespace ddoc = cc::ui::screens::doctor_dialog_registration;
namespace fdp = cc::ui::feature_dialog_protocol;
namespace fwa = cc::ui::feature_wizard_adapter;
namespace app_dlg = cc::ui::app_dialogs;

/// Render an FTXUI Element to a fixed-size screen and return the text with
/// ANSI escape sequences stripped (for content assertions).
std::string render_element_text(ftxui::Element element, int w = 100, int h = 30) {
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(w), ftxui::Dimension::Fixed(h));
    ftxui::Render(screen, element);
    std::string raw = screen.ToString();
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size();) {
        if (raw[i] == '\033' && i + 1 < raw.size() && raw[i + 1] == '[') {
            i += 2;
            while (i < raw.size() && (raw[i] < 0x40 || raw[i] > 0x7E)) ++i;
            if (i < raw.size()) ++i;
            continue;
        }
        out.push_back(raw[i]);
        ++i;
    }
    return out;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// F2 cut 6: feature-dialog protocol leaf — miss path
// ═══════════════════════════════════════════════════════════════════════════

// This test MUST be the first protocol-leaf test in this binary. The registry
// is a function-local static with no reset; GoogleTest executes in source
// order by default and ctest is -j1, so this runs before
// register_feature_dialog_factories() is called by the positive tests below.
// It proves the miss path: resolve_dialog_factory returns a null factory for
// every unregistered ViewKind.
TEST(F2FeatureDialogProtocol, MissReturnsNullBeforeRegistration) {
    EXPECT_FALSE(static_cast<bool>(
        fdp::resolve_dialog_factory(fdp::ViewKind::AgentWizard)));
    EXPECT_FALSE(static_cast<bool>(
        fdp::resolve_dialog_factory(fdp::ViewKind::PluginInstall)));
    EXPECT_FALSE(static_cast<bool>(
        fdp::resolve_dialog_factory(fdp::ViewKind::PluginTrust)));
}

// ═══════════════════════════════════════════════════════════════════════════
// F2 cut 4: doctor renderer through DialogRendererRegistry
// ═══════════════════════════════════════════════════════════════════════════

TEST(F2DoctorRenderer, NotInDefaultRenderers) {
    // register_default_renderers must NOT register the Doctor dialog — that
    // registration moved to the screens side (F2 cut 4: dialogs must not
    // import screens). With only the default set, rendering a
    // DoctorDialogPayload hits the registry fallback.
    dsys::DialogRendererRegistry registry;
    drender::register_default_renderers(registry);

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 100;
    ctx.term_rows = 30;

    dsys::DoctorDialogPayload payload;
    payload.id = "doctor-f2-before";
    dsys::DialogPayloadVariant variant = payload;

    auto el = registry.render(variant, ctx);
    ASSERT_NE(el, nullptr);
    const std::string out = render_element_text(el);
    EXPECT_NE(out.find("not implemented"), std::string::npos)
        << "Doctor must NOT be registered by register_default_renderers "
           "(F2 cut 4: dialogs must not import screens)";
}

TEST(F2DoctorRenderer, RegisteredFromScreensSideRenders) {
    // After register_doctor_renderer (screens side), the Doctor dialog must
    // render through the registry — not the fallback. The renderer lazily
    // builds a DoctorScreen (RunAllChecks) on first render.
    dsys::DialogRendererRegistry registry;
    drender::register_default_renderers(registry);
    ddoc::register_doctor_renderer(registry);

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 100;
    ctx.term_rows = 30;

    dsys::DoctorDialogPayload payload;
    payload.id = "doctor-f2-after";
    dsys::DialogPayloadVariant variant = payload;

    auto el = registry.render(variant, ctx);
    ASSERT_NE(el, nullptr);
    const std::string out = render_element_text(el);
    EXPECT_EQ(out.find("not implemented"), std::string::npos)
        << "Doctor must render through the screens-side registration, "
           "not the registry fallback";
    // The doctor screen starts in Splash state, which names the diagnostics
    // suite — proof the real DoctorScreen component rendered.
    EXPECT_NE(out.find("Preparing diagnostics"), std::string::npos)
        << "DoctorScreen splash content must render through the registry";
}

// ═══════════════════════════════════════════════════════════════════════════
// F2 cut 6: wizard/trust flows through the protocol leaf
// ═══════════════════════════════════════════════════════════════════════════

TEST(F2FeatureDialogProtocol, AgentWizardFactoryRendersThroughProtocol) {
    // Register the app's concrete factories (the composition root), then
    // resolve the AgentWizard factory and invoke it with a minimal neutral
    // FeatureWizardRequest. This proves the full path: protocol-leaf
    // register/resolve -> erased shared_ptr<void> -> adapter -> wizard.
    app_dlg::register_feature_dialog_factories();

    auto factory = fdp::resolve_dialog_factory(fdp::ViewKind::AgentWizard);
    ASSERT_TRUE(static_cast<bool>(factory));

    fdp::FeatureWizardRequest request;
    request.title = "F2 Agent Wizard";
    request.show_step_counter = true;
    fdp::FeatureWizardStep step;
    step.id = "intro";
    step.title = "Introduction";
    step.description = "F2 composition-root test step";
    step.create_content = []() -> ftxui::Component {
        return ftxui::Renderer([] {
            return ftxui::vbox({
                ftxui::text("F2 wizard step body"),
            });
        });
    };
    request.steps.push_back(std::move(step));

    auto erased = std::make_shared<fdp::FeatureWizardRequest>(std::move(request));
    ftxui::Component component = factory(erased);
    ASSERT_NE(component, nullptr);

    const std::string out = render_element_text(component->Render());
    EXPECT_FALSE(out.empty());
    // The step's create_content lambda must have been invoked by the wizard
    // framework — proof the neutral request reached the wizard through the
    // protocol leaf, not through a direct wizard_dialog import.
    EXPECT_NE(out.find("F2 wizard step body"), std::string::npos);
}

TEST(F2FeatureDialogProtocol, PluginTrustFactoryRendersThroughProtocol) {
    // Re-register (idempotent — re-registering replaces the factory) so this
    // test is self-contained. Resolve the PluginTrust factory and invoke it
    // with a minimal neutral FeatureTrustRequest.
    app_dlg::register_feature_dialog_factories();

    auto factory = fdp::resolve_dialog_factory(fdp::ViewKind::PluginTrust);
    ASSERT_TRUE(static_cast<bool>(factory));

    fdp::FeatureTrustRequest request;
    request.marketplace_domain = "f2-test.example.com";
    request.has_signature = true;
    request.on_done = [](fdp::TrustChoice) {};

    auto erased = std::make_shared<fdp::FeatureTrustRequest>(std::move(request));
    ftxui::Component component = factory(erased);
    ASSERT_NE(component, nullptr);

    const std::string out = render_element_text(component->Render());
    EXPECT_FALSE(out.empty());
    // The composition root's trust factory sets action_label to
    // "Plugin Installation", which the trust dialog renders as its title.
    EXPECT_NE(out.find("Plugin Installation"), std::string::npos);
}

TEST(F2FeatureDialogProtocol, AdapterConvertsNeutralRequestToWizard) {
    // The dialogs-side adapter: a neutral FeatureWizardRequest in, a wizard
    // Component out. This is the dialogs half of the features -> dialogs
    // inversion — feature modules never import wizard_dialog; the adapter
    // (imported only by the composition root) does.
    fdp::FeatureWizardRequest request;
    request.title = "F2 Adapter Wizard";
    request.show_step_counter = true;
    fdp::FeatureWizardStep step;
    step.id = "adapter-step";
    step.title = "Adapter Step";
    step.description = "F2 adapter test";
    step.create_content = []() -> ftxui::Component {
        return ftxui::Renderer([] {
            return ftxui::text("F2 adapter step body");
        });
    };
    request.steps.push_back(std::move(step));

    ftxui::Component component = fwa::MakeFeatureWizard(request);
    ASSERT_NE(component, nullptr);

    const std::string out = render_element_text(component->Render());
    EXPECT_FALSE(out.empty());
    EXPECT_NE(out.find("F2 adapter step body"), std::string::npos);
    EXPECT_NE(out.find("F2 Adapter Wizard"), std::string::npos);
}
