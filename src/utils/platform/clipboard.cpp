// @file clipboard.cpp
// @brief OSC 52 terminal clipboard request emission.
module;

#include <cstdio>

module loom.platform.clipboard;

namespace loom::utils::clipboard {

loom::utils::VoidResult emit_osc52(std::string_view text) {
    const auto encoded = loom::utils::crypto::base64_encode(text);
    if (std::fprintf(stdout, "\033]52;c;%s\a", encoded.c_str()) < 0) {
        return std::unexpected(loom::utils::make_error(
            loom::utils::ErrorCode::io_error, "OSC 52: fprintf failed"));
    }
    if (std::fflush(stdout) != 0) {
        return std::unexpected(loom::utils::make_error(
            loom::utils::ErrorCode::io_error, "OSC 52: fflush failed"));
    }
    return {};
}

}  // namespace loom::utils::clipboard
