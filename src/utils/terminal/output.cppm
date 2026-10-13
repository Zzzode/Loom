// @file output.cppm
// @brief Delivery of terminal control bytes to stdout.
export module loom.terminal.output;

import std;
import loom.utils.error;

export namespace loom::terminal::output {

/// Write the complete byte range to stdout and flush. The caller owns
/// serialization with terminal rendering. Success confirms byte delivery,
/// not the terminal's acceptance or execution of the control sequence.
[[nodiscard]] loom::utils::VoidResult write_control_sequence(std::string_view bytes);

}  // namespace loom::terminal::output
