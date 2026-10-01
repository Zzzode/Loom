export module loom.text.semantic_boolean;

import std;

export namespace loom::utils::semantic_boolean {

[[nodiscard]] inline std::optional<bool> coerce_semantic_boolean(std::string_view value) noexcept {
    if (value == "true") return true;
    if (value == "false") return false;
    return std::nullopt;
}

} // namespace loom::utils::semantic_boolean
