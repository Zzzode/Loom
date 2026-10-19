module;

#include <ftxui/dom/node.hpp>

export module loom.ui.visual.mermaid;

import std;

export namespace loom::ui::mermaid {

/// Render the admitted Mermaid subset, preserving the caller's source element
/// when a successfully laid out diagram does not fit the assigned width.
[[nodiscard]] std::optional<ftxui::Element> render_mermaid(
    std::string_view source, ftxui::Element fallback);

} // namespace loom::ui::mermaid
