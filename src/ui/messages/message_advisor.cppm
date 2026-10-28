/// @file message_advisor.cppm
/// @brief Advisor/suggestion message rendering  (+ dismiss button)
///
///   ┌ 💡 Tip ───────────────────────────────────────── [✕] ┐
///   │ You can enable auto-save in settings → /config        │
///   │   → Configure now                                     │
///   └───────────────────────────────────────────────────────┘
module;

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

export module loom.ui.messages.message_advisor;

import std;

import loom.ui.foundation.design_figures;

import loom.ui.foundation.theme_provider;

import loom.ui.messages.message_components;

export namespace loom::ui::messages {

using namespace ftxui;

/// Advisor message severity
enum class AdvisorSeverity {
    Info,
    Suggestion,
    Warning,
    Error,
};

/// Advisor message entry
struct AdvisorMessage {
    std::string content;
    AdvisorSeverity severity{AdvisorSeverity::Info};
    std::optional<std::string> action_label;
    std::optional<std::string> source;
};

/// ─── Stateless element renderer ────────────────────────────────────────
[[nodiscard]] inline Element render_advisor_message(const AdvisorMessage& msg) {
    auto [prefix, prefix_color] = [&]() -> std::pair<std::string, Color> {
        switch (msg.severity) {
        case AdvisorSeverity::Info:
            return {"ℹ", loom::ui::design::theme::current_theme().palette->info};
        case AdvisorSeverity::Suggestion:
            return {loom::ui::design::figures::kInfo.data(),
                    loom::ui::design::theme::current_theme().palette->suggestion};
        case AdvisorSeverity::Warning:
            return {loom::ui::design::figures::kWarning.data(),
                    loom::ui::design::theme::current_theme().palette->warning};
        case AdvisorSeverity::Error:
            return {loom::ui::design::figures::kCross.data(),
                    loom::ui::design::theme::current_theme().palette->danger};
        }
        return {"?", loom::ui::design::theme::current_theme().palette->text};
    }();

    std::vector<Element> elements;
    elements.push_back(hbox({
        text(prefix) | color(prefix_color),
        text(" " + msg.content),
    }));

    if (msg.action_label) {
        elements.push_back(text("  → " + *msg.action_label) | dim);
    }

    return vbox(elements);
}

// ─── Severity → palette helper ─────────────────────────────────────────

inline auto SeverityPalette(AdvisorSeverity s)
    -> std::tuple<std::string_view, std::string_view, Color> {
    switch (s) {
    case AdvisorSeverity::Info:
        return {"ℹ", "Info", loom::ui::design::theme::current_theme().palette->info};
    case AdvisorSeverity::Suggestion:
        return {loom::ui::design::figures::kInfo.data(), "Tip",
                loom::ui::design::theme::current_theme().palette->suggestion};
    case AdvisorSeverity::Warning:
        return {loom::ui::design::figures::kWarning.data(), "Warning",
                loom::ui::design::theme::current_theme().palette->warning};
    case AdvisorSeverity::Error:
        return {loom::ui::design::figures::kCross.data(), "Error",
                loom::ui::design::theme::current_theme().palette->danger};
    }
    return {"ℹ", "Info", loom::ui::design::theme::current_theme().palette->info};
}

// ─── Interactive component: pill + dismiss + optional action ───────────

class AdvisorMessageComponent : public ComponentBase {
public:
    using OnDismissFn = std::function<void()>;
    using OnActionFn = std::function<void()>;

    explicit AdvisorMessageComponent(AdvisorMessage msg, OnDismissFn on_dismiss = nullptr,
                                     OnActionFn on_action = nullptr)
        : msg_(std::move(msg))
        , on_dismiss_(std::move(on_dismiss))
        , on_action_(std::move(on_action))
        , dismissed_(false) {
        if (on_dismiss_) {
            dismiss_btn_ = Button((" " + std::string(loom::ui::design::figures::kCross) + " "),
                                  [this] {
                                      dismissed_ = true;
                                      if (on_dismiss_) on_dismiss_();
                                  }) |
                           dim;
            Add(dismiss_btn_);
        }
        if (on_action_ && msg_.action_label) {
            action_btn_ = Button(" → " + *msg_.action_label + " ", [this] {
                if (on_action_) on_action_();
            });
            Add(action_btn_);
        }
    }

    Element Render() override {
        if (dismissed_) return text("") | size(HEIGHT, EQUAL, 0);

        auto [icon, title, accent] = SeverityPalette(msg_.severity);

        Element dismiss_el = on_dismiss_ && dismiss_btn_ ? dismiss_btn_->Render() : text("");
        auto header = hbox({
            text(std::string(icon) + " ") | color(accent),
            text(std::string(title) + "  ") | bold | color(accent),
            text(msg_.content) | flex,
            filler(),
            dismiss_el,
        });

        Elements extras;
        if (msg_.source) {
            extras.push_back(text("  source: " + *msg_.source) | dim |
                             color(loom::ui::design::theme::current_theme().palette->muted));
        }
        if (msg_.action_label) {
            Element act_el = action_btn_ ? action_btn_->Render() | color(accent)
                                         : text(" → " + *msg_.action_label) | dim | color(accent);
            extras.push_back(hbox({text("  "), act_el}));
        }

        Elements rows;
        rows.push_back(header);
        for (auto& e : extras)
            rows.push_back(std::move(e));

        return vbox(std::move(rows)) | borderStyled(BorderStyle::ROUNDED, accent) | padding(1);
    }

    bool OnEvent(Event event) override {
        if (event == Event::Character('d') || event == Event::Character('D') ||
            event == Event::Escape) {
            dismissed_ = true;
            if (on_dismiss_) on_dismiss_();
            return true;
        }
        if (event == Event::Character('a') || event == Event::Character('A')) {
            if (on_action_) on_action_();
            return true;
        }
        return ComponentBase::OnEvent(event);
    }

private:
    AdvisorMessage msg_;
    OnDismissFn on_dismiss_;
    OnActionFn on_action_;
    bool dismissed_;
    Component dismiss_btn_;
    Component action_btn_;
};

[[nodiscard]] inline Component
MakeAdvisorMessage(AdvisorMessage msg, AdvisorMessageComponent::OnDismissFn on_dismiss = nullptr,
                   AdvisorMessageComponent::OnActionFn on_action = nullptr) {
    return Make<AdvisorMessageComponent>(std::move(msg), std::move(on_dismiss),
                                         std::move(on_action));
}

}  // namespace loom::ui::messages
