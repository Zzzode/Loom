// @file osc.cpp
// @brief Terminal OSC framing and payload validation.
module loom.terminal.osc;

import loom.crypto.crypto;

namespace loom::terminal::osc {
namespace {

[[nodiscard]] bool contains_control(std::string_view payload) {
    return std::ranges::any_of(payload,
                               [](unsigned char byte) { return byte < 0x20 || byte == 0x7f; });
}

[[nodiscard]] std::string frame(std::string_view command, std::string_view payload,
                                std::string_view terminator) {
    std::string result{"\033]"};
    result += command;
    result += ';';
    result += payload;
    result += terminator;
    return result;
}

}  // namespace

loom::utils::Result<std::string> window_title(std::string_view title) {
    if (contains_control(title)) {
        return std::unexpected(loom::utils::make_error(
            loom::utils::ErrorCode::invalid_argument, "OSC 2: title contains a control character"));
    }
    return frame("2", title, "\033\\");
}

loom::utils::Result<std::string> hyperlink(std::string_view uri, std::string_view text) {
    if (contains_control(uri)) {
        return std::unexpected(loom::utils::make_error(loom::utils::ErrorCode::invalid_argument,
                                                       "OSC 8: URI contains a control character"));
    }
    auto result = frame("8", ";" + std::string(uri), "\033\\");
    result += text;
    result += frame("8", ";", "\033\\");
    return result;
}

std::string clipboard_request(std::string_view text) {
    return frame("52", "c;" + loom::utils::crypto::base64_encode(text), "\a");
}

std::string program_status_query() {
    return frame("7501", "?", "\033\\");
}

loom::utils::Result<std::string> program_status(ProgramStatusState state, ProgramStatusKind kind) {
    std::string_view state_name;
    switch (state) {
    case ProgramStatusState::Idle:
        state_name = "idle";
        break;
    case ProgramStatusState::Working:
        state_name = "working";
        break;
    case ProgramStatusState::Done:
        state_name = "done";
        break;
    case ProgramStatusState::Blocked:
        state_name = "blocked";
        break;
    case ProgramStatusState::Error:
        state_name = "error";
        break;
    default:
        break;
    }
    std::string_view kind_name;
    bool valid_kind = true;
    switch (kind) {
    case ProgramStatusKind::None:
        break;
    case ProgramStatusKind::Permission:
        kind_name = "permission";
        break;
    case ProgramStatusKind::Question:
        kind_name = "question";
        break;
    case ProgramStatusKind::Auth:
        kind_name = "auth";
        break;
    default:
        valid_kind = false;
        break;
    }
    if (state_name.empty() || !valid_kind ||
        (state != ProgramStatusState::Blocked && kind != ProgramStatusKind::None)) {
        return std::unexpected(loom::utils::make_error(loom::utils::ErrorCode::invalid_argument,
                                                       "OSC 7501: invalid state or blocked kind"));
    }
    std::string payload = "state=" + std::string(state_name);
    if (!kind_name.empty()) payload += ":kind=" + std::string(kind_name);
    payload += ":app=loom";
    return frame("7501", payload, "\033\\");
}

}  // namespace loom::terminal::osc
