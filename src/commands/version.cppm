export module loom.commands.version;

import std;

import loom.constants.product;

export namespace loom::commands::version {

struct CommandResponse { bool ok{true}; std::string message; };

[[nodiscard]] inline auto name() -> std::string_view { return "version"; }

[[nodiscard]] inline auto run(std::string_view detail = {}) -> CommandResponse {
    std::string msg = "loom ";
    msg += loom::constants::product::LOOM_VERSION;
    msg += " (C++23, built ";
    msg += loom::constants::product::BUILD_DATE;
    msg += ")";
    if (!detail.empty()) {
        msg += " [";
        msg += detail;
        msg += "]";
    }
    return {.ok = true, .message = std::move(msg)};
}

} // namespace loom::commands::version
