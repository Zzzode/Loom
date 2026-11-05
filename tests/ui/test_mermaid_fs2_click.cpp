// RFC 0031 FS-2: `click` interaction bindings are non-visual statements (the
// terminal has no interaction layer). A well-formed click binding on a node of
// an otherwise renderable graph must be consumed as a no-op instead of
// forcing whole-source fallback; a malformed binding keeps the source.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>
import std;
import loom.ui.visual.markdown;

namespace {
constexpr int kWide = 240;
constexpr int kHeight = 200;

std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source +
           "\n```\n\nFS2_CLICK_SENTINEL";
}

bool drawn_native(const std::string& source, int width = kWide) {
    auto native = loom::ui::render_markdown(fenced(source, "mermaid"));
    auto plain = loom::ui::render_markdown(fenced(source, "text"));
    if (!native || !plain) return false;
    auto ns = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                    ftxui::Dimension::Fixed(kHeight));
    auto ps = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                    ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(ns, native);
    ftxui::Render(ps, plain);
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < width; ++x)
            if (ns.PixelAt(x, y).character != ps.PixelAt(x, y).character) return true;
    return false;
}

std::string rendered_text(const std::string& source) {
    auto native = loom::ui::render_markdown(fenced(source, "mermaid"));
    if (!native) return {};
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(kWide),
                                        ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, native);
    std::string text;
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < kWide; ++x) text += screen.PixelAt(x, y).character;
    return text;
}
}  // namespace

TEST(MermaidFs2Click, WellFormedBindingsAreNonVisualNoOps) {
    const std::string cases[] = {
        "flowchart LR\n  A --> B\n  click A \"https://example.com\" \"open\"\n",
        "flowchart LR\n  A --> B\n  click C \"#anchor\"\n",
        "flowchart LR\n  A --> B\n  click A \"https://example.com/\" \"tip\" _blank\n",
        "flowchart LR\n  A --> B\n  click B onAction \"arg one\"\n",
        "flowchart LR\n  A --> B\n  click A href \"https://example.com\" _self\n",
        // The terminal never executes a binding, so the quoted URL scheme is
        // inert string content and does not change the drawn graph.
        "flowchart LR\n  A --> B\n  click F \"javascript:alert('x')\" \"script\"\n",
    };
    for (const auto& source : cases) {
        EXPECT_TRUE(drawn_native(source)) << source;
        const auto text = rendered_text(source);
        EXPECT_EQ(text.find("click "), std::string::npos)
            << "click statement leaked into art: " << source;
    }
}

TEST(MermaidFs2Click, MalformedBindingsKeepWholeSource) {
    const std::string cases[] = {
        "flowchart LR\n  A --> B\n  click\n",
        "flowchart LR\n  A --> B\n  click A\n",
        "flowchart LR\n  A --> B\n  click A \"https://example.com\n",
        "flowchart LR\n  A --> B\n  click A B C D E\n",
        "flowchart LR\n  A --> B\n  click A \"https://example.com\" \"tip\" garbage\n",
        "flowchart LR\n  A --> B\n  click A \"https://example.com\" \"t1\" \"t2\"\n",
    };
    for (const auto& source : cases)
        EXPECT_FALSE(drawn_native(source)) << "malformed click admitted: " << source;
}

