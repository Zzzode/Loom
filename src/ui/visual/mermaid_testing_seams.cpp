module;
#include <ftxui/dom/node.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

// Out-of-line bodies of the reviewed test seam (RFC 0031 E6a). Each is a
// thin forwarder to the detail implementation of the same name; the detail
// signatures take identical types because detail aliases the exported
// testing entities. No logic is moved or duplicated here.
namespace loom::ui::mermaid::testing {

bool set_flow_style_preview_for_testing(bool enabled) {
    return detail::set_flow_style_preview_for_testing(enabled);
}

std::optional<ftxui::Element> render_mermaid_with_work(std::string_view source,
                                                       ftxui::Element fallback, bool use_ascii,
                                                       Work& work, FinalEmissionTrace* trace) {
    return detail::render_mermaid_with_budget(source, std::move(fallback), use_ascii, work, trace);
}

std::optional<Envelope> parse_envelope(std::string_view source, Work& work) {
    return detail::parse_envelope(source, work);
}

std::optional<GitGraph> parse_gitgraph(std::string_view source, Work& work) {
    return detail::parse_gitgraph(source, work);
}

std::optional<FlowAsciiRows> flow_ascii_rows(std::string_view source, Work& work) {
    return detail::flow_ascii_rows(source, work);
}

std::optional<std::vector<std::string>> class_relations_rows(std::string_view source, Work& work,
                                                             bool class_envelope) {
    return detail::relations_layout(source, work, class_envelope, false);
}

std::optional<ClassAccessibilityMetadata>
class_accessibility_metadata(std::string_view source, Work& work, bool class_envelope) {
    return detail::class_accessibility_metadata(source, work, class_envelope);
}

std::optional<MermaidProbeDecision> classify_mermaid(std::string_view source, Work& work) {
    return detail::classify_mermaid(source, work);
}

bool block_layout_succeeds(std::string_view source, Work& work) {
    return detail::block_layout(source, work).has_value();
}

bool layout_succeeds(std::string_view source, bool use_ascii, Work& work) {
    return detail::layout_mermaid_content(source, use_ascii, work).has_value();
}

}  // namespace loom::ui::mermaid::testing
