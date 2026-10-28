/// @file message_channel.cppm
/// @brief Channel/thread message rendering
module;

#include <ftxui/dom/elements.hpp>

export module loom.ui.messages.message_channel;

import std;

import loom.ui.foundation.theme_provider;

export namespace loom::ui::messages {

using namespace ftxui;

/// Channel message source
enum class ChannelSource {
    User,
    Assistant,
    System,
    Tool,
};

/// Channel message entry
struct ChannelMessageEntry {
    ChannelSource source{ChannelSource::User};
    std::string content;
    std::optional<std::string> sender_name;
    std::chrono::system_clock::time_point timestamp;
};

/// Render channel message
[[nodiscard]] inline Element render_channel_message(const ChannelMessageEntry& entry) {
    auto source_color = [&]() -> Color {
        switch (entry.source) {
        case ChannelSource::User:
            return loom::ui::design::theme::current_theme().palette->brief_label_you;
        case ChannelSource::Assistant:
            return loom::ui::design::theme::current_theme().palette->brief_label_loom;
        case ChannelSource::System:
            return loom::ui::design::theme::current_theme().palette->info;
        case ChannelSource::Tool:
            return loom::ui::design::theme::current_theme().palette->info;
        }
        return loom::ui::design::theme::current_theme().palette->text;
    }();

    auto sender = entry.sender_name.value_or([&] {
        switch (entry.source) {
        case ChannelSource::User:
            return "user";
        case ChannelSource::Assistant:
            return "assistant";
        case ChannelSource::System:
            return "system";
        case ChannelSource::Tool:
            return "tool";
        }
        return "unknown";
    }());

    return hbox({
        text(sender) | bold | color(source_color),
        text(": "),
        text(entry.content),
    });
}

}  // namespace loom::ui::messages
