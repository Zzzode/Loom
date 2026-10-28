// @file osc.cppm
// @brief Pure encoders for Loom-owned terminal OSC commands.
export module loom.terminal.osc;

import std;
import loom.utils.error;

export namespace loom::terminal::osc {

/// Encode OSC 2 using ST. Reject C0 and DEL controls in the title.
[[nodiscard]] loom::utils::Result<std::string> window_title(std::string_view title);

/// Encode OSC 8 using ST. Reject C0 and DEL controls in the URI only;
/// display text is opaque. An empty URI closes any active hyperlink.
[[nodiscard]] loom::utils::Result<std::string> hyperlink(std::string_view uri,
                                                         std::string_view text);

/// Encode an OSC 52 clipboard request for selector c using Base64 and BEL.
/// Arbitrary text bytes are supported. No terminal acceptance is implied.
[[nodiscard]] std::string clipboard_request(std::string_view text);

enum class ProgramStatusState { Idle, Working, Done, Blocked, Error };
enum class ProgramStatusKind { None, Permission, Question, Auth };

/// Query OSC 7501 support. Reports address only Loom's root record.
[[nodiscard]] std::string program_status_query();
[[nodiscard]] loom::utils::Result<std::string>
program_status(ProgramStatusState state, ProgramStatusKind kind = ProgramStatusKind::None);

}  // namespace loom::terminal::osc
