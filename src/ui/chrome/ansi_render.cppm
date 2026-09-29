/// @file ansi_render.cppm
/// @brief ANSI / SGR -> FTXUI Element render helpers (declarations).
///
/// RFC 0002 phase F1, row 8: the three helpers (sgr_color_value_to_ftxui,
/// apply_sgr_run, ansi_to_ftxui_elements) were extracted VERBATIM from
/// cc.ui.messages.message_tool_result into this chrome-area leaf. The
/// bodies live in the module implementation unit ansi_render.cpp (the
/// Phase-C recipe) so a body edit recompiles exactly one object (fan-out
/// = 1) and this declarations-only BMI stays cheap for importers.
///
/// The functions stay in namespace cc::ui::messages (NOT chrome) so the
/// existing call sites compile unchanged: unqualified calls inside
/// message_tool_result.cppm and `msgs::ansi_to_ftxui_elements` in
/// prompt_input_footer.cppm. Namespace and module are decoupled in C++23;
/// the module's AREA is chrome (derived from its module name), which is
/// what the arch lint ranks — prompt -> chrome is downward-legal, so the
/// prompt -> messages back edge is severed.
///
/// The leaf imports only std + cc.ui.chrome.terminal_io (for SgrAttr /
/// ColorValue); terminal_io is itself a std-only leaf.
module;

#include <ftxui/dom/elements.hpp>

export module cc.ui.chrome.ansi_render;

import std;

import cc.ui.chrome.terminal_io;  // SgrAttr / ColorValue

export namespace cc::ui::messages {

using namespace ftxui;

/// Map a termio ColorValue (Color16 / Color256 / TrueColor) onto the
/// equivalent ftxui Color. See ansi_render.cpp for the full mapping notes.
[[nodiscard]] Color sgr_color_value_to_ftxui(const cc::ui::termio::ColorValue& cv);

/// Apply one SGR parameter run onto running SgrAttr state (see
/// ansi_render.cpp for the full VT-faithful merge semantics).
void apply_sgr_run(std::string_view params, cc::ui::termio::SgrAttr& attr);

/// Split an ANSI-decorated string into ftxui Elements that honor SGR
/// color and basic attributes (see ansi_render.cpp for the full contract).
[[nodiscard]] Element ansi_to_ftxui_elements(std::string_view input);

} // namespace cc::ui::messages
