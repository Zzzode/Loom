// RFC 0031 E6d libFuzzer harness for the Mermaid renderer.
//
// Module implementation unit: it reaches the non-exported family layout
// functions through the :detail partition and the reviewed seam types through
// the testing namespace. It never asserts drawn-vs-rejected (rejection is the
// common outcome); only crashes, hangs (timeout), sanitizer reports and
// resource-limit violations fail.
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <cstddef>
#include <cstdint>
#include <string_view>

module loom.ui.visual.mermaid;

import std;
import :detail;

using namespace loom::ui::mermaid;
using loom::ui::mermaid::detail::charts_layout;
using loom::ui::mermaid::detail::gitgraph_layout;
using loom::ui::mermaid::detail::relations_layout;
using loom::ui::mermaid::detail::sequence_layout;
using loom::ui::mermaid::testing::Envelope;
using loom::ui::mermaid::testing::Work;

namespace {
constexpr std::size_t kSourceLimit = 65536;
constexpr int kMaxWidth = 240;

void touch_envelope(const Envelope& envelope) {
    volatile std::size_t touched = 0;
    touched += envelope.events.size() + envelope.config.size() + envelope.init.size();
    touched += envelope.raw_frontmatter.size() + envelope.raw_body.size();
    touched += envelope.effective_body.size();
    if (envelope.title) touched += envelope.title->text.size();
    if (envelope.description) touched += envelope.description->text.size();
}

void render_headless(std::string_view source, bool ascii) {
    auto element = render_mermaid(source, ftxui::text("fuzz-fallback"), ascii);
    if (!element) return;
    // Two fixed screens exercise the MermaidNode width flip (240 and the 40-col
    // narrow profile); dimensions never derive from fuzzer input.
    for (const int width : {kMaxWidth, 40}) {
        auto screen = ftxui::Screen::Create(
            ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(64));
        ftxui::Render(screen, *element);
    }
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
    // Mirrors the public render_mermaid source gate; layouts take string_view
    // over the bytes including any NUL.
    if (size == 0 || size > kSourceLimit) return 0;
    const std::string_view source{reinterpret_cast<const char*>(data), size};

    {
        Work work;
        if (auto envelope = detail::parse_envelope(source, work)) touch_envelope(*envelope);
    }

    auto run_layouts = [&](bool ascii) {
        Work w1; if (auto rows = sequence_layout(source, w1))
            for (const auto& row : *rows) if (row.empty()) return;
        Work w2; if (auto rows = charts_layout(source, w2))
            for (const auto& row : *rows) if (row.size() > kMaxWidth) return;
        Work w3; static_cast<void>(gitgraph_layout(source, w3, ascii));
        for (bool class_envelope : {false, true})
            for (bool requirement_input : {false, true}) {
                Work w4;
                static_cast<void>(relations_layout(source, w4, class_envelope, requirement_input));
            }
    };
    run_layouts(false);
    run_layouts(true);

    // Full dispatch (covers Flow and the Charts fallback) and the ASCII entry.
    render_headless(source, false);
    render_headless(source, true);
    return 0;
}
