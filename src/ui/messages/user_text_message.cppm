/// @file user_text_message.cppm
/// @brief FTXUI component for user text messages
///
/// Visual layout:
///   ┌── Right-aligned (user side)
///   │  ┌──────────────────────────────┐
///   │  │ [content, selectable]   ⏱ HH:MM │
///   │  │ [quoted reply if any]     👤 You│
///   │  └──────────────────────────────┘
///   └──
///
/// Interactions:
///   - Click on message -> toggle selection highlight
///   - Ctrl+C / copy button -> copy content to clipboard (placeholder callback)
// ────────────────────────────────────────────────────────────────────────
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

export module loom.ui.messages.user_text_message;

import std;

import loom.ui.messages.message_timestamp;
import loom.ui.foundation.theme_provider;
// P0-1: Unified prompt / user-message glyph source.
// Eliminates the local `kFiguresPointer` duplicate that diverged from the
// prompt prefix's UTF-8 byte sequence in CPP Round 1-6.
import loom.ui.foundation.design_figures;

export namespace loom::ui::messages {

using namespace ftxui;

// ─── Input data ────────────────────────────────────────────────────────

/// Plain data input, no globals.
struct UserTextMessageData {
    std::string content;
    std::chrono::system_clock::time_point timestamp =
        std::chrono::system_clock::now();
    std::optional<std::string> quoted_reply;   ///< Text being replied to (>)
    bool is_transcript_mode = false;           ///< Dimmed presentation
    std::optional<std::string> command_name;   ///< "/commit" etc. shown as chip
};

// ─── User-prompt text truncation ────────────────────────────────────────
// Hard-caps display text at 10_000 chars: head 2500 + separator + tail 2500.
// Critical for perf: pasting a 100 KB file into the prompt would otherwise
// force FTXUI to lay out thousands of lines on every keystroke.
namespace truncate_detail {
inline constexpr std::size_t kMaxDisplayChars = 10'000;
inline constexpr std::size_t kTruncateHeadChars = 2'500;
inline constexpr std::size_t kTruncateTailChars = 2'500;

/// Count occurrences of `ch` in `text` starting at byte position `start`.
[[nodiscard]] inline std::size_t CountCharFrom(std::string_view text,
                                                char ch,
                                                std::size_t start) {
    std::size_t count = 0;
    std::size_t pos = start;
    while ((pos = text.find(ch, pos)) != std::string_view::npos) {
        ++count;
        ++pos;
    }
    return count;
}
}  // namespace truncate_detail

/// Truncate long user prompt text for display.  Returns the original text
/// unchanged if <= 10_000 chars.  Otherwise returns head 2500 + separator +
/// tail 2500, where the separator shows the count of hidden lines:
///   "\n… +N lines …\n"
/// (U+2026 HORIZONTAL ELLIPSIS, UTF-8 \xe2\x80\xa6).
[[nodiscard]] inline std::string TruncateUserPromptText(std::string_view text) {
    using namespace truncate_detail;
    if (text.size() <= kMaxDisplayChars) return std::string(text);

    std::string_view head = text.substr(0, kTruncateHeadChars);
    std::string_view tail = text.substr(text.size() - kTruncateTailChars);

    // Newlines in the hidden region (from head end to text end), minus
    // newlines already visible in the tail slice.
    const std::size_t hidden_lines =
        truncate_detail::CountCharFrom(text, '\n', truncate_detail::kTruncateHeadChars) -
        truncate_detail::CountCharFrom(tail, '\n', 0);

    std::string result;
    result.reserve(truncate_detail::kTruncateHeadChars + truncate_detail::kTruncateTailChars + 40);
    result.append(head);
    result += "\n\xe2\x80\xa6 +";       // "\n… +"
    result += std::to_string(hidden_lines);
    result += " lines \xe2\x80\xa6\n";  // " lines …\n"
    result.append(tail);
    return result;
}

// ─── Text wrapping for FTXUI ───────────────────────────────────────────
// FTXUI's text() does NOT auto-wrap, and paragraphAlignLeft only wraps at
// word boundaries.  For long unbroken strings (base64, minified code, or
// the truncation head/tail of a paste with no spaces), we need manual
// character-level wrapping.
namespace wrap_detail {
inline constexpr int kPromptChromeColumns = 3;  // "❯ " and right padding.

/// Wrap at terminal cells, keeping fullwidth and combining glyphs together.
[[nodiscard]] inline std::vector<std::string> WrapToWidth(std::string_view text,
                                                         std::size_t width) {
    width = std::max<std::size_t>(1, width);
    const auto glyphs = Utf8ToGlyphs(std::string(text));
    std::vector<std::string> lines(1);
    std::size_t columns = 0;
    for (const auto& glyph : glyphs) {
        // Utf8ToGlyphs reserves a second, empty cell for fullwidth glyphs.
        if (glyph.empty()) continue;
        const auto cells = static_cast<std::size_t>(string_width(glyph));
        if (columns != 0 && columns + cells > width) {
            lines.emplace_back();
            columns = 0;
        }
        lines.back() += glyph;
        columns += cells;
    }
    return lines;
}

/// Preserve logical newlines, including empty and trailing lines.
[[nodiscard]] inline std::vector<std::string> WrappedPromptLines(
    std::string_view text, int content_columns) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    do {
        const auto newline = text.find('\n', start);
        const auto end = newline == std::string_view::npos ? text.size() : newline;
        auto wrapped = WrapToWidth(text.substr(start, end - start),
                                   std::max(1, content_columns));
        for (auto& line : wrapped) lines.push_back(std::move(line));
        if (newline == std::string_view::npos) break;
        start = newline + 1;
    } while (start <= text.size());
    return lines;
}

/// Resolve wrapping after the parent assigns the available FTXUI box width.
/// Like flexbox, request another layout pass when that width changes.
class PromptBody : public Node {
  public:
    explicit PromptBody(std::string text) : text_(std::move(text)) {}

    void ComputeRequirement() override {
        Elements lines;
        for (auto& line : WrappedPromptLines(text_, columns_))
            lines.push_back(ftxui::text(std::move(line)));
        children_ = {vbox(std::move(lines))};
        children_[0]->ComputeRequirement();
        requirement_ = children_[0]->requirement();
        requirement_.min_x = 0;
        requirement_.flex_grow_x = 1;
        computed_columns_ = columns_;
    }

    void SetBox(Box box) override {
        Node::SetBox(box);
        columns_ = std::max(1, box.x_max - box.x_min + 1);
        children_[0]->SetBox(box);
    }

    void Check(Status* status) override {
        Node::Check(status);
        status->need_iteration |= columns_ != computed_columns_;
    }

  private:
    std::string text_;
    int columns_ = std::numeric_limits<int>::max();
    int computed_columns_ = 0;
};
}  // namespace wrap_detail

// ─── Component ─────────────────────────────────────────────────────────

class UserTextMessageComponent : public ComponentBase {
  public:
    using OnCopyFn = std::function<void(std::string_view)>;
    using OnClickFn = std::function<void()>;

    explicit UserTextMessageComponent(UserTextMessageData data,
                                      OnCopyFn on_copy = nullptr,
                                      OnClickFn on_click = nullptr)
        : data_(std::move(data)),
          on_copy_(std::move(on_copy)),
          on_click_(std::move(on_click)) {}

    Element Render() override {
        auto bubble_body = BuildContent();

        // Right-side header: timestamp + role tag
        auto header_right = hbox({
            text(" ") | size(WIDTH, EQUAL, 1),
            text(render_timestamp(data_.timestamp)) | dim | color(loom::ui::design::theme::current_theme().palette->muted),
            text((" " + std::string(loom::ui::design::figures::kPointer) + " You")) | color(loom::ui::design::theme::current_theme().palette->brief_label_you),
        });

        // User message is right-aligned. We indent from the left so that
        // the content visually hugs the right edge.
        return vbox({
            hbox({
                filler(),
                header_right,
            }),
            hbox({
                filler(),
                vbox(std::move(bubble_body))
                    | (hovered_ ? bgcolor(loom::ui::design::theme::current_theme().palette->message_actions_background) : nothing)
                    | flex_shrink,
            }),
        });
    }

    bool OnEvent(Event event) override {
        if (event == Event::Return) {
            hovered_ = !hovered_;
            if (on_click_) on_click_();
            return true;
        }
        // Custom copy accelerator
        if (event == Event::Special({3})) {  // Ctrl+C convention
            if (on_copy_) on_copy_(data_.content);
            return true;
        }
        if (event.is_mouse() && event.mouse().button == Mouse::Left &&
            event.mouse().motion == Mouse::Released) {
            hovered_ = !hovered_;
            if (on_click_) on_click_();
            return true;
        }
        return ComponentBase::OnEvent(event);
    }

  private:
    auto BuildContent() -> Elements {
        Elements out;

        // Quoted reply block (indented, dimmed)
        if (data_.quoted_reply && !data_.quoted_reply->empty()) {
            auto lines = SplitLines(*data_.quoted_reply);
            for (auto& line : lines) {
                out.push_back(hbox({
                    text("│ ") | color(loom::ui::design::theme::current_theme().palette->muted),
                    text(std::move(line)) | dim,
                }));
            }
            out.push_back(separator() | dim);
        }

        // Command chip
        if (data_.command_name) {
            out.push_back(hbox({
                text(" $ ") | bold | color(loom::ui::design::theme::current_theme().palette->primary),
                text(*data_.command_name) | bold,
            }));
        }

        // Main body — truncated for long pastes.  SplitLines handles both
        // natural \n in user input and
        // the truncation separator "\n… +N lines …\n".
        std::string display_body = TruncateUserPromptText(data_.content);
        auto lines = SplitLines(display_body);
        for (auto& line : lines) {
            out.push_back(text(std::move(line))
                              | color(loom::ui::design::theme::current_theme().palette->text)
                              | (data_.is_transcript_mode ? dim : nothing));
        }

        return out;
    }

    static auto SplitLines(std::string_view text) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::size_t start = 0;
        while (start < text.size()) {
            auto nl = text.find('\n', start);
            if (nl == std::string_view::npos) {
                lines.emplace_back(text.substr(start));
                break;
            }
            lines.emplace_back(text.substr(start, nl - start));
            start = nl + 1;
        }
        if (lines.empty()) lines.emplace_back("");
        return lines;
    }

    UserTextMessageData data_;
    OnCopyFn on_copy_;
    OnClickFn on_click_;
    bool hovered_ = false;
};

/// Factory function.
[[nodiscard]] inline Component MakeUserTextMessage(UserTextMessageData data,
                                                   UserTextMessageComponent::OnCopyFn on_copy = nullptr,
                                                   UserTextMessageComponent::OnClickFn on_click = nullptr) {
    return Make<UserTextMessageComponent>(std::move(data), std::move(on_copy), std::move(on_click));
}

/// Convenience stateless element renderer (for transcript / no-interaction).
[[nodiscard]] inline Element RenderUserTextMessageBubble(const UserTextMessageData& data) {
    Elements body;
    body.push_back(text(TruncateUserPromptText(data.content)));
    return vbox({
        hbox({
            filler(),
            text(render_timestamp(data.timestamp)) | dim,
            text((" " + std::string(loom::ui::design::figures::kPointer))) | color(loom::ui::design::theme::current_theme().palette->brief_label_you),
        }),
        hbox({ filler(), vbox(std::move(body)) }),
    });
}

// ─── M4: Faithful renderers ────────────────────────────────────────────
// The non-brief layout path: a full-width, left-aligned row with a `❯`
// (subtle) prefix followed by the prompt text, optional top margin, and a
// background that swaps on selection.  No timestamp / role label in the
// non-brief path — that only appears in the brief/chat layout.
//
// NOTE: The glyph itself lives in loom.ui.foundation.design_figures::kPointer — the
// authoritative source used by the prompt prefix, user messages, and plugin
// manager.  Use `namespace figs = loom::ui::design::figures;` below.

/// Faithful render of a user prompt message.  Full-width, left-aligned, with
/// a `❯` (muted) prefix followed by the prompt text.  is_selected swaps the
/// prefix + bg color to the messageActionsBackground / suggestion tokens.
///
/// `add_margin` controls the blank separator line above the message.  It is
/// effectively always true in normal REPL use.  We still thread the parameter
/// for faithfulness and transcript-mode correctness.
///
/// FTXUI NOTES:
///   - `text()` does NOT auto-wrap.
///   - `paragraphAlignLeft()` only wraps at word boundaries; long unbroken
///     strings (base64, minified code) would overflow the screen.
///   - PromptBody wraps at the width assigned by the parent layout, using
///     terminal-cell widths rather than UTF-8 byte counts.
[[nodiscard]] inline Element RenderUserPromptMessage(const UserTextMessageData& data,
                                                     bool is_selected = false,
                                                     bool add_margin = true) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    const Color kText = pal.text;
    const Color kUserBg = pal.user_message_background;
    const Color kUserBgSel = pal.message_actions_background;
    const Color kMuted = pal.muted;
    const Color kSuggestion = pal.suggestion;

    const Color prefix_color  = is_selected ? kSuggestion : kMuted;
    const Color bg            = is_selected ? kUserBgSel : kUserBg;
    const Decorator body_style =
        data.is_transcript_mode ? (dim | color(kText)) : color(kText);
    const Decorator prefix_style =
        data.is_transcript_mode ? (dim | color(prefix_color)) : color(prefix_color);

    Element body = hbox({
        text(std::string(loom::ui::design::figures::kPointer) + " ") | prefix_style,
        std::make_shared<wrap_detail::PromptBody>(TruncateUserPromptText(data.content))
            | body_style,
        text(" ") | color(kText),
    });
    if (!data.is_transcript_mode) body = body | bgcolor(bg);

    if (data.is_transcript_mode) {
        Element content = std::move(body) | xflex;
        if (add_margin) return vbox({text(""), std::move(content)});
        return content;
    }

    if (add_margin) return vbox({text(""), std::move(body)});
    // Continuation within same user turn (e.g. after an image),
    // <MessageResponse> prepends "  ⎿  " (U+23BF connector).
    return hbox({
        text("  \xe2\x8e\xbf  ") | dim,
        std::move(body) | xflex,
    });
}

/// Faithful render of a slash-command user message:
///   A row with optional top margin, a userMessageBackground chip background,
///   a readable muted `❯` pointer prefix, and the `/command args` text.
///
/// `add_margin` controls the blank separator line above the message.
/// Threaded for faithfulness; always true in normal REPL use for the same
/// reason as RenderUserPromptMessage.
[[nodiscard]] inline Element RenderUserCommandMessage(const UserTextMessageData& data,
                                                      bool is_selected = false,
                                                      bool add_margin = true) {
    (void)is_selected;  // Selection never affects the command chip tint.
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    // The command chip always keeps the user-message background regardless
    // of selection.
    const Color kUserBg = pal.user_message_background;
    const Color kMuted = pal.muted;
    // The pointer always uses the readable muted token.
    // Selection only affects the BLACK_CIRCLE color of the assistant row,
    // never the user command chip tint — unlike UserPromptMessage which
    // swaps both the prefix and the background.
    const Color prefix_color = kMuted;

    std::string body = data.command_name
        ? ("/" + *data.command_name)
        : data.content;
    const Color kText = pal.text;
    // right padding = 1 only — no flex, so the chip collapses to content
    // width instead of stretching to terminal width (F8 compact chip).
    Element row = hbox({
        text(std::string(loom::ui::design::figures::kPointer)) | color(prefix_color),
        text(" ") | color(prefix_color),
        text(body) | color(kText),
        text(" ") | color(kText),  // right padding = 1
    });
    if (data.is_transcript_mode) {
        if (add_margin) return vbox({text(""), row});
        return row;
    }
    // Wrap row in a single bgcolor cell that is exactly row-width (no flex).
    Element chip = hbox({row}) | bgcolor(kUserBg);
    Element content = hbox({chip, filler()});
    if (add_margin) return vbox({text(""), std::move(content)});
    return content;
}

}  // namespace loom::ui::messages
