/// @file ansi_render.cpp
/// @brief ANSI / SGR -> FTXUI Element render helpers — bodies.
///
/// Module implementation unit for cc.ui.chrome.ansi_render (RFC 0002 phase
/// F1, row 8). The three function bodies were extracted VERBATIM from
/// cc.ui.messages.message_tool_result; only the `inline` specifier was
/// dropped (the bodies now live in this impl unit, the Phase-C recipe, so
/// a body edit recompiles exactly this object — fan-out = 1 — and the
/// declarations-only BMI keeps importers cheap).
///
/// This is the ONLY unit in the module that sees ftxui: the interface
/// (ansi_render.cppm) names no ftxui type, so the ~10.8 MB ftxui BMI
/// weight stays out of importers.  sgr_color_value_to_ftxui is internal
/// here (called only by ansi_to_ftxui_elements), and
/// ansi_to_ftxui_elements returns its per-line vector type-erased as
/// std::shared_ptr<void> (see ansi_render.cppm for the rationale).
module;

#include <ftxui/dom/elements.hpp>

module loom.ui.chrome.ansi_render;

import std;

import loom.ui.chrome.terminal_io;  // SgrAttr / ColorValue / Color16/256/TrueColor

namespace loom::ui::messages {

using namespace ftxui;

// ─── ANSI / SGR → FTXUI Element helper ──────────────────────────────────────
// Tool and bash output frequently carries embedded ANSI escape sequences
// (e.g. "\x1b[31merror\x1b[0m").  Without interpretation they leak as literal
// text.  This helper walks the input once, splitting on ESC[...m runs, and
// emits a hbox of colored ftxui Elements so the decoration is honored.
//
// Mapping notes (mirror the SGR codes cc.ui.termio understands):
//   Color16  -> Palette256 index (standard ANSI 0-15 slots)
//   Color256 -> Palette256 index
//   TrueColor-> Color::RGB
// Decorators applied per-run: bold / dim / underlined / inverted /
// strikethrough.  FTXUI has no `italic` decorator, so SGR code 3 is parsed
// (state tracked) but produces no visual change.  Non-SGR escapes (other CSI,
// OSC, plain ESC) are dropped, matching Ink's Text-node behavior.
[[nodiscard]] Color sgr_color_value_to_ftxui(
    const loom::ui::termio::ColorValue& cv) {
    using namespace loom::ui::termio;
    return std::visit(
        [&](auto&& v) -> Color {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Color16>) {
                return Color{Color::Palette256(static_cast<std::uint8_t>(v))};
            } else if constexpr (std::is_same_v<T, Color256>) {
                return Color{Color::Palette256(v.index)};
            } else if constexpr (std::is_same_v<T, TrueColor>) {
                return Color::RGB(v.r, v.g, v.b);
            } else {
                return Color{}; // monostate / default
            }
        },
        cv);
}

/// Apply one SGR parameter run (the bytes between ESC[ and 'm', e.g. "1;31")
/// onto running SgrAttr state the way a real VT does: code 0 resets, the
/// per-attribute "off" codes (22/23/24/27/29) clear their flag, the
/// default-color codes 39/49 clear fg/bg, and every other code touches only
/// its own field (so e.g. "\x1b[31m" does not clear a prior bold).  This is
/// the correct way to merge state across consecutive SGR runs.
void apply_sgr_run(std::string_view params, loom::ui::termio::SgrAttr& attr) {
    using namespace loom::ui::termio;
    std::size_t i = 0;
    auto next_int = [&]() -> int {
        int val = 0;
        bool any = false;
        while (i < params.size() && params[i] >= '0' && params[i] <= '9') {
            val = val * 10 + (params[i] - '0');
            any = true;
            i++;
        }
        if (i < params.size() && params[i] == ';') i++;
        return any ? val : 0;
    };

    while (i < params.size()) {
        // Guard against a trailing separator that consumes no digits.
        std::size_t before = i;
        int code = next_int();
        if (i == before) break;
        switch (code) {
            case 0:  attr = SgrAttr{}; break;
            case 1:  attr.bold = true; break;
            case 2:  attr.dim = true; break;
            case 3:  attr.italic = true; break;          // tracked, not rendered
            case 4:  attr.underline = true; break;
            case 7:  attr.inverse = true; break;
            case 9:  attr.strikethrough = true; break;
            case 22: attr.bold = false; attr.dim = false; break;
            case 23: attr.italic = false; break;
            case 24: attr.underline = false; break;
            case 27: attr.inverse = false; break;
            case 29: attr.strikethrough = false; break;
            case 30: case 31: case 32: case 33:
            case 34: case 35: case 36: case 37:
                attr.fg = static_cast<Color16>(code - 30); break;
            case 38: {
                int mode = next_int();
                if (mode == 5) attr.fg = Color256{static_cast<std::uint8_t>(next_int())};
                else if (mode == 2) {
                    auto r = static_cast<std::uint8_t>(next_int());
                    auto g = static_cast<std::uint8_t>(next_int());
                    auto b = static_cast<std::uint8_t>(next_int());
                    attr.fg = TrueColor{r, g, b};
                }
                break;
            }
            case 39: attr.fg = std::monostate{}; break;
            case 40: case 41: case 42: case 43:
            case 44: case 45: case 46: case 47:
                attr.bg = static_cast<Color16>(code - 40); break;
            case 48: {
                int mode = next_int();
                if (mode == 5) attr.bg = Color256{static_cast<std::uint8_t>(next_int())};
                else if (mode == 2) {
                    auto r = static_cast<std::uint8_t>(next_int());
                    auto g = static_cast<std::uint8_t>(next_int());
                    auto b = static_cast<std::uint8_t>(next_int());
                    attr.bg = TrueColor{r, g, b};
                }
                break;
            }
            case 49: attr.bg = std::monostate{}; break;
            case 90: case 91: case 92: case 93:
            case 94: case 95: case 96: case 97:
                attr.fg = static_cast<Color16>(code - 90 + 8); break;
            case 100: case 101: case 102: case 103:
            case 104: case 105: case 106: case 107:
                attr.bg = static_cast<Color16>(code - 100 + 8); break;
            default: break;
        }
    }
}

/// Split an ANSI-decorated string into per-line ftxui Elements that honor
/// SGR color and basic attributes.  Newlines produce one hbox per line; a
/// single-line input returns a vector holding that one hbox.  Empty /
/// escape-only input produces one blank text element.  Reusable for any
/// string carrying SGR codes (tool output, markdown code blocks, etc.).
///
/// The per-line vector is returned type-erased as std::shared_ptr<void>
/// (actually std::shared_ptr<std::vector<Element>>) so the module interface
/// names no ftxui type; callers cast it back (see ansi_render.cppm).
[[nodiscard]] std::shared_ptr<void> ansi_to_ftxui_elements(std::string_view input) {
    using namespace loom::ui::termio;

    SgrAttr attr{};          // running SGR state, mutated by each SGR run
    std::vector<Element> lines;
    std::vector<Element> current_runs;
    std::string buf;

    auto flush_buf = [&]() {
        if (!buf.empty()) {
            Element e = text(buf);
            if (attr.bold)          e = std::move(e) | bold;
            if (attr.dim)           e = std::move(e) | dim;
            // FTXUI exposes no `italic` decorator; code 3 is tracked only.
            if (attr.underline)     e = std::move(e) | underlined;
            if (attr.strikethrough) e = std::move(e) | strikethrough;
            if (attr.inverse)       e = std::move(e) | inverted;
            if (!std::holds_alternative<std::monostate>(attr.fg)) {
                e = std::move(e) | color(sgr_color_value_to_ftxui(attr.fg));
            }
            if (!std::holds_alternative<std::monostate>(attr.bg)) {
                e = std::move(e) | bgcolor(sgr_color_value_to_ftxui(attr.bg));
            }
            current_runs.push_back(std::move(e));
            buf.clear();
        }
    };

    auto flush_line = [&]() {
        flush_buf();
        if (current_runs.empty()) {
            lines.push_back(text(""));
        } else {
            lines.push_back(hbox(std::move(current_runs)));
        }
        current_runs.clear();
    };

    std::size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '\n') {
            flush_line();
            i++;
        } else if (input[i] == '\033' && i + 1 < input.size() && input[i + 1] == '[') {
            // CSI: ESC[ <params 0x20-0x3F> <final 0x40-0x7E>
            std::size_t start = i + 2;
            std::size_t p = start;
            while (p < input.size() &&
                   static_cast<unsigned char>(input[p]) >= 0x20 &&
                   static_cast<unsigned char>(input[p]) <= 0x3F) {
                p++;
            }
            char final_byte = (p < input.size()) ? input[p] : '\0';
            if (p < input.size()) p++; // consume final byte
            if (final_byte == 'm') {
                flush_buf();
                apply_sgr_run(input.substr(start, (p - 1) - start), attr);
            }
            i = p;
        } else if (input[i] == '\033' && i + 1 < input.size() && input[i + 1] == ']') {
            // OSC: ESC] ... BEL or ST (ESC backslash).  Skip entirely.
            i += 2;
            while (i < input.size() && input[i] != '\a' &&
                   !(i + 1 < input.size() && input[i] == '\033' && input[i + 1] == '\\')) {
                i++;
            }
            if (i < input.size() && input[i] == '\a') i++;
            else if (i + 1 < input.size()) i += 2;
        } else if (input[i] == '\033' && i + 1 < input.size()) {
            i += 2; // other escape: skip ESC + one byte
        } else {
            buf += input[i];
            i++;
        }
    }
    flush_line();

    // lines is never empty: flush_line() always pushes at least one element
    // (a blank text element when there are no runs).  Return the per-line
    // vector type-erased so the interface names no ftxui type.
    return std::make_shared<std::vector<Element>>(std::move(lines));
}

} // namespace loom::ui::messages
