/// @file ansi_render.cppm
/// @brief ANSI / SGR -> FTXUI Element render helpers (declarations).
///
/// RFC 0002 phase F1, row 8: the three helpers (sgr_color_value_to_ftxui,
/// apply_sgr_run, ansi_to_ftxui_elements) were extracted VERBATIM from
/// loom.ui.messages.message_tool_result into this chrome-area leaf. The
/// bodies live in the module implementation unit ansi_render.cpp (the
/// Phase-C recipe) so a body edit recompiles exactly one object (fan-out
/// = 1) and this declarations-only BMI stays cheap for importers.
///
/// The functions stay in namespace loom::ui::messages (NOT chrome) so the
/// existing call sites compile unchanged: unqualified calls inside
/// message_tool_result.cppm and `msgs::ansi_to_ftxui_elements` in
/// prompt_input_footer.cppm. Namespace and module are decoupled in C++23;
/// the module's AREA is chrome (derived from its module name), which is
/// what the arch lint ranks — prompt -> chrome is downward-legal, so the
/// prompt -> messages back edge is severed.
///
/// TYPE ERASURE (PSS remediation): this interface names NO ftxui type.
/// ftxui's <ftxui/dom/elements.hpp> weighs ~10.8 MB of BMI; including it
/// in the GMF here embedded that weight in this leaf's BMI, which every
/// importer deserialized — regressing message_tool_result's producer PSS
/// ~7% (463 MB -> 497 MB).  The ftxui-dependent color mapper
/// (sgr_color_value_to_ftxui) is now INTERNAL to the impl unit (called
/// only by ansi_to_ftxui_elements), and ansi_to_ftxui_elements returns
/// its per-line std::vector<ftxui::Element> type-erased as
/// std::shared_ptr<void>.  Callers (which already have ftxui in scope)
/// cast it back with std::static_pointer_cast<std::vector<ftxui::Element>>
/// and compose the lines.  The leaf's BMI is now on the order of
/// terminal_io's (~2.27 MB), keeping importer PSS low.
///
/// The leaf imports only std + loom.ui.chrome.terminal_io (for SgrAttr);
/// terminal_io is itself a std-only leaf.
module;

export module loom.ui.chrome.ansi_render;

import std;

import loom.ui.chrome.terminal_io;  // SgrAttr

export namespace loom::ui::messages {

/// Apply one SGR parameter run onto running SgrAttr state (see
/// ansi_render.cpp for the full VT-faithful merge semantics).
void apply_sgr_run(std::string_view params, loom::ui::termio::SgrAttr& attr);

/// Split an ANSI-decorated string into per-line ftxui Elements that honor
/// SGR color and basic attributes (see ansi_render.cpp for the full
/// contract).  The result is type-erased: it is a
/// std::shared_ptr<std::vector<ftxui::Element>> returned as
/// std::shared_ptr<void> so this interface names no ftxui type.  Callers
/// cast it back:
///   auto elems = std::static_pointer_cast<std::vector<ftxui::Element>>(
///       ansi_to_ftxui_elements(input));
/// and compose the lines (e.g. vbox(std::move(*elems))).  The vector is
/// never empty — an escape-only / empty input yields one blank text line.
[[nodiscard]] std::shared_ptr<void> ansi_to_ftxui_elements(std::string_view input);

} // namespace loom::ui::messages
