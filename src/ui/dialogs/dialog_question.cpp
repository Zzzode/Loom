// The actual queued ask-user panel: a held native editor and opaque local layer.
module;

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/node.hpp>

module loom.ui.dialogs.default_renderers;

import std;
import loom.ui.dialogs.system;
import loom.ui.dialogs.frame;
import loom.ui.foundation.design_figures;
import loom.ui.foundation.theme_provider;

namespace loom::ui::dialogs::default_renderers {
namespace {

std::string SingleLine(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (std::size_t at = 0; at < input.size(); ++at) {
        const auto byte = static_cast<unsigned char>(input[at]);
        if (byte == '\r') {
            if (at + 1 < input.size() && input[at + 1] == '\n') ++at;
            result += ' ';
        } else if (byte == '\n' || byte == '\t') {
            result += ' ';
        } else if (input.substr(at).starts_with("\xe2\x80\xa8") ||
                   input.substr(at).starts_with("\xe2\x80\xa9")) {
            result += ' ';
            at += 2;
        } else if (byte >= 0x20 && byte != 0x7f) {
            result += input[at];
        }
    }
    return result;
}

struct QuestionState {
    std::string value;
    int cursor = 0;
    Component input;
    bool resolved = false;
};

std::shared_ptr<QuestionState> EnsureQuestion(const dsys::PromptDialogPayload& p) {
    if (p.ui_state) return std::static_pointer_cast<QuestionState>(p.ui_state);
    auto state = std::make_shared<QuestionState>();
    state->value = SingleLine(p.default_value.value_or(""));
    state->cursor = static_cast<int>(state->value.size());
    InputOption options;
    options.multiline = false;
    options.cursor_position = &state->cursor;
    options.transform = [](InputState input) {
        const auto& palette = *loom::ui::design::theme::current_theme().palette;
        auto element = input.element | color(input.is_placeholder ? palette.muted : palette.text);
        // FTXUI 5 hides the cursor in its empty-content placeholder branch.
        if (input.is_placeholder && input.focused) element |= focusCursorBarBlinking;
        return element;
    };
    state->input = Input(&state->value, "Type your answer...", std::move(options));
    p.ui_state = state;
    return state;
}

// Input DOM contains references to its boxes and buffer. Retain the entire
// state when callers keep an Element beyond the payload's queue lifetime.
class QuestionOwnerNode final : public Node {
public:
    QuestionOwnerNode(Element child, std::shared_ptr<QuestionState> owner)
        : Node(Elements{std::move(child)})
        , owner_(std::move(owner)) {}

    void ComputeRequirement() override {
        Node::ComputeRequirement();
        requirement_ = children_[0]->requirement();
    }

    void SetBox(Box box) override {
        Node::SetBox(box);
        children_[0]->SetBox(box);
    }

private:
    std::shared_ptr<QuestionState> owner_;
};

}  // namespace

Element RenderPromptDialog(const dsys::PromptDialogPayload& p,
                           const dsys::DialogRenderContext& ctx) {
    auto state = EnsureQuestion(p);
    const auto& palette = *loom::ui::design::theme::current_theme().palette;
    dframe::DialogFrameProps props;
    props.title = p.title.empty() ? "Input Required" : p.title;
    props.style = dframe::FrameStyle::Permission;
    props.content = vbox({
        paragraph(p.prompt_text),
        text(""),
        hbox({text(std::string(loom::ui::design::figures::figures().pointer) + " ") |
                  color(palette.permission),
              state->input->Render() | xflex}) |
            size(HEIGHT, EQUAL, 1),
        text(""),
        hbox({text(" [Enter] Submit") | color(palette.success),
              text("  [Esc] Cancel") | color(palette.danger)}),
    });
    auto panel = dframe::DialogFrame(props, ctx.theme) | clear_under;
    return std::make_shared<QuestionOwnerNode>(std::move(panel), std::move(state));
}

bool HandlePromptDialogEvent(dsys::PromptDialogPayload& p, const Event& event) {
    if (event == Event::Character('\x03') || event == Event::Character('\x04') ||
        event == Event::Character('\x0c') || event == Event::Custom)
        return false;
    auto state = EnsureQuestion(p);
    if (state->resolved) return !event.is_mouse();
    if (event == Event::Return || event == Event::Escape) {
        auto callback = p.on_response;
        auto value = event == Event::Escape ? std::optional<std::string>{}
                                            : std::optional<std::string>{state->value};
        state->resolved = true;
        if (callback) callback(std::move(value));
        return true;
    }
    if (event.is_character()) {
        auto value = SingleLine(event.character());
        if (!value.empty()) (void)state->input->OnEvent(Event::Character(std::move(value)));
        return true;
    }
    const bool handled = state->input->OnEvent(event);
    // A boundary key or Tab belongs to this panel even when Input cannot move.
    return handled || !event.is_mouse();
}

}  // namespace loom::ui::dialogs::default_renderers
