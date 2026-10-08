/// @file xml_tags.cppm
/// @brief Shared parsing helpers for XML-like message protocol envelopes.
module;

export module loom.ui.messages.xml_tags;

import std;

export namespace loom::ui::messages::xml_tags {

/// Extract the content of the first complete `<tag_name>...</tag_name>` pair.
/// Returns std::nullopt when either delimiter is absent.
[[nodiscard]] inline std::optional<std::string> extract_tag(
    std::string_view text, std::string_view tag_name) noexcept {
    if (tag_name.empty()) return std::nullopt;

    const std::string open = "<" + std::string(tag_name) + ">";
    const std::string close = "</" + std::string(tag_name) + ">";
    const auto start = text.find(open);
    if (start == std::string_view::npos) return std::nullopt;

    const auto content_start = start + open.size();
    const auto end = text.find(close, content_start);
    if (end == std::string_view::npos) return std::nullopt;
    return std::string(text.substr(content_start, end - content_start));
}

} // namespace loom::ui::messages::xml_tags
