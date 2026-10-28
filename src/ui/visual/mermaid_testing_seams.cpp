module;

module loom.ui.visual.mermaid;

import std;
import :detail;

// Out-of-line bodies of the reviewed test seam (RFC 0031 E6a). Each is a
// thin forwarder to the detail implementation of the same name; the detail
// signatures take identical types because detail aliases the exported
// testing entities. No logic is moved or duplicated here.
namespace loom::ui::mermaid::testing {

std::optional<Envelope> parse_envelope(std::string_view source, Work& work) {
    return detail::parse_envelope(source, work);
}

std::optional<GitGraph> parse_gitgraph(std::string_view source, Work& work) {
    return detail::parse_gitgraph(source, work);
}

std::optional<FlowAsciiRows> flow_ascii_rows(std::string_view source, Work& work) {
    return detail::flow_ascii_rows(source, work);
}

std::optional<MermaidProbeDecision> classify_mermaid(std::string_view source, Work& work) {
    return detail::classify_mermaid(source, work);
}

bool block_layout_succeeds(std::string_view source, Work& work) {
    return detail::block_layout(source, work).has_value();
}

bool layout_succeeds(std::string_view source, bool use_ascii, Work& work) {
    return detail::layout_mermaid_payload(source,use_ascii,work).has_value();
}

} // namespace loom::ui::mermaid::testing
