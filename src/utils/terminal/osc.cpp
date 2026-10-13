// @file osc.cpp
// @brief Terminal OSC framing and payload validation.
module loom.terminal.osc;

import loom.crypto.crypto;

namespace loom::terminal::osc {
namespace {

[[nodiscard]] bool contains_control(std::string_view payload) {
    return std::ranges::any_of(payload, [](unsigned char byte) {
        return byte < 0x20 || byte == 0x7f;
    });
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
            loom::utils::ErrorCode::invalid_argument,
            "OSC 2: title contains a control character"));
    }
    return frame("2", title, "\033\\");
}

loom::utils::Result<std::string> hyperlink(std::string_view uri, std::string_view text) {
    if (contains_control(uri)) {
        return std::unexpected(loom::utils::make_error(
            loom::utils::ErrorCode::invalid_argument,
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

}  // namespace loom::terminal::osc
