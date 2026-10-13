// @file output.cpp
// @brief Length-aware terminal control delivery and error reporting.
module;

#include <cstdio>

module loom.terminal.output;

namespace loom::terminal::output {

loom::utils::VoidResult write_control_sequence(std::string_view bytes) {
    if (!bytes.empty() && std::fwrite(bytes.data(), 1, bytes.size(), stdout) != bytes.size()) {
        return std::unexpected(loom::utils::make_error(
            loom::utils::ErrorCode::io_error, "Terminal output: fwrite failed"));
    }
    if (std::fflush(stdout) != 0) {
        return std::unexpected(loom::utils::make_error(
            loom::utils::ErrorCode::io_error, "Terminal output: fflush failed"));
    }
    return {};
}

}  // namespace loom::terminal::output
