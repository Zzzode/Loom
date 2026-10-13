/// @file message_plan_approval.cppm
/// @brief Plan approval message rendering  (+ Approve / Modify / Reject buttons)
///
///   ┌ Proposed plan: Refactor billing module ─────── [PENDING] ┐
///   │ 1. Read the existing billing code                        │
///   │ 2. Extract interfaces (Invoice, Payment)                 │
///   │ 3. ⚠️ Replace DB schema (risky)                          │
///   │ 4. Add unit tests                                        │
///   ├─────────────────────────────────────────────────────────┤
///   │ [✓ Approve]  [✎ Modify]  [✕ Reject]                     │
///   └─────────────────────────────────────────────────────────┘
module;


#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

export module loom.ui.messages.message_plan_approval;

import std;

import loom.ui.foundation.design_figures;

import loom.ui.foundation.theme_provider;

import loom.types.types;

export namespace loom::ui::messages {

using namespace ftxui;

/// Plan approval state
enum class PlanApprovalState {
    Pending,
    Approved,
    Rejected,
    Modified,
};

/// Plan step for display
struct PlanStep {
    std::string description;
    std::optional<std::string> tool_name;
    bool is_risky{false};
};

/// Plan approval message options
struct PlanApprovalOptions {
    std::string plan_title;
    std::vector<PlanStep> steps;
    PlanApprovalState state{PlanApprovalState::Pending};
    std::optional<std::string> rejection_reason;
};

/// ─── Stateless element renderer (kept for non-interactive contexts) ───
[[nodiscard]] inline Element render_plan_approval(const PlanApprovalOptions& opts) {
    std::vector<Element> elements;

    auto state_indicator = [&]() -> Element {
        switch (opts.state) {
            case PlanApprovalState::Pending: return text("[PENDING]") | color(loom::ui::design::theme::current_theme().palette->warning);
            case PlanApprovalState::Approved: return text("[APPROVED]") | color(loom::ui::design::theme::current_theme().palette->success);
            case PlanApprovalState::Rejected: return text("[REJECTED]") | color(loom::ui::design::theme::current_theme().palette->danger);
            case PlanApprovalState::Modified: return text("[MODIFIED]") | color(loom::ui::design::theme::current_theme().palette->info);
        }
        return text("");
    }();

    elements.push_back(hbox({text(opts.plan_title) | bold, text(" "), state_indicator}));
    elements.push_back(separator());

    for (std::size_t i = 0; i < opts.steps.size(); ++i) {
        const auto& step = opts.steps[i];
        auto prefix = std::format("{}. ", i + 1);
        auto style = step.is_risky ? color(loom::ui::design::theme::current_theme().palette->warning) : nothing;
        elements.push_back(text(prefix + step.description) | style);
    }

    if (opts.rejection_reason) {
        elements.push_back(separator());
        elements.push_back(text("Reason: " + *opts.rejection_reason) | color(loom::ui::design::theme::current_theme().palette->danger));
    }

    return vbox(elements) | border;
}

// ─── Interactive component: 3-button approval panel ────────────────────

class PlanApprovalComponent : public ComponentBase {
  public:
    using OnApproveFn = std::function<void()>;
    using OnModifyFn  = std::function<void(std::vector<PlanStep> proposed)>;
    using OnRejectFn  = std::function<void(std::string reason)>;

    explicit PlanApprovalComponent(PlanApprovalOptions opts,
                                   OnApproveFn on_approve = nullptr,
                                   OnModifyFn  on_modify  = nullptr,
                                   OnRejectFn  on_reject  = nullptr)
        : opts_(std::move(opts)),
          on_approve_(std::move(on_approve)),
          on_modify_(std::move(on_modify)),
          on_reject_(std::move(on_reject))
    {
        if (opts_.state == PlanApprovalState::Pending) {
            approve_btn_ = Button(("  " + std::string(loom::ui::design::figures::kTick) + " Approve  "), [this] {
                if (on_approve_) {
                    opts_.state = PlanApprovalState::Approved;
                    on_approve_();
                }
            }) | bold;
            Add(approve_btn_);

            modify_btn_ = Button("  ✎ Modify  ", [this] {
                if (on_modify_) {
                    opts_.state = PlanApprovalState::Modified;
                    on_modify_(opts_.steps);
                }
            });
            Add(modify_btn_);

            reject_btn_ = Button(("  " + std::string(loom::ui::design::figures::kCross) + " Reject  "), [this] {
                if (on_reject_) {
                    opts_.state = PlanApprovalState::Rejected;
                    opts_.rejection_reason = opts_.rejection_reason
                        ? opts_.rejection_reason
                        : std::optional<std::string>("User rejected plan");
                    on_reject_(*opts_.rejection_reason);
                }
            });
            Add(reject_btn_);
        }
    }

    Element Render() override {
        // State badge
        auto state_badge = [&]() -> Element {
            switch (opts_.state) {
                case PlanApprovalState::Pending:
                    return hbox({ text((std::string(loom::ui::design::figures::kCircle) + " ")) | color(loom::ui::design::theme::current_theme().palette->warning),
                                  text(" PENDING ") | color(loom::ui::design::theme::current_theme().palette->warning) | bold });
                case PlanApprovalState::Approved:
                    return hbox({ text((std::string(loom::ui::design::figures::kTick) + " ")) | color(loom::ui::design::theme::current_theme().palette->success),
                                  text(" APPROVED ") | color(loom::ui::design::theme::current_theme().palette->success) | bold });
                case PlanApprovalState::Rejected:
                    return hbox({ text((std::string(loom::ui::design::figures::kCross) + " ")) | color(loom::ui::design::theme::current_theme().palette->danger),
                                  text(" REJECTED ") | color(loom::ui::design::theme::current_theme().palette->danger) | bold });
                case PlanApprovalState::Modified:
                    return hbox({ text("✎ ") | color(loom::ui::design::theme::current_theme().palette->info),
                                  text(" MODIFIED ") | color(loom::ui::design::theme::current_theme().palette->info) | bold });
            }
            return text("");
        }();

        auto header = hbox({
            text((std::string(loom::ui::design::figures::kSquare) + " ")) | color(loom::ui::design::theme::current_theme().palette->info),
            text(opts_.plan_title) | bold,
            filler(),
            std::move(state_badge),
        });

        // Bulleted step list
        Elements steps_rows;
        for (std::size_t i = 0; i < opts_.steps.size(); ++i) {
            const auto& step = opts_.steps[i];
            auto bullet = std::format("{}. ", i + 1);
            auto risky = step.is_risky ? text((" " + std::string(loom::ui::design::figures::kWarning))) | color(loom::ui::design::theme::current_theme().palette->warning) : text("");
            auto tool_chip = step.tool_name
                ? text("  [" + *step.tool_name + "]") | dim | color(loom::ui::design::theme::current_theme().palette->muted)
                : text("");
            Decorator style = step.is_risky ? color(loom::ui::design::theme::current_theme().palette->warning) : nothing;
            steps_rows.push_back(hbox({
                text(bullet) | bold | color(loom::ui::design::theme::current_theme().palette->info),
                text(step.description) | style,
                tool_chip,
                risky,
            }));
        }

        // Risk warning banner (if any steps are risky)
        Elements risk_rows;
        bool has_risk = false;
        for (const auto& s : opts_.steps) if (s.is_risky) { has_risk = true; break; }
        if (has_risk) {
            risk_rows.push_back(hbox({
                text((std::string(loom::ui::design::figures::kWarning) + " ")) | color(loom::ui::design::theme::current_theme().palette->warning),
                text("This plan contains risky operations. Please review carefully.")
                    | color(loom::ui::design::theme::current_theme().palette->warning) | dim,
            }));
        }

        // Rejection reason (after rejection)
        Elements rejection_rows;
        if (opts_.rejection_reason) {
            rejection_rows.push_back(text("Rejection reason: " + *opts_.rejection_reason)
                                         | color(loom::ui::design::theme::current_theme().palette->danger) | dim);
        }

        // Buttons (only in Pending state)
        Elements actions;
        if (opts_.state == PlanApprovalState::Pending && approve_btn_) {
            actions.push_back(hbox({
                approve_btn_->Render() | color(loom::ui::design::theme::current_theme().palette->success), text(" "),
                modify_btn_->Render() | color(loom::ui::design::theme::current_theme().palette->info), text(" "),
                reject_btn_->Render() | color(loom::ui::design::theme::current_theme().palette->danger),
            }));
        }

        Elements all;
        all.push_back(std::move(header));
        all.push_back(separator());
        for (auto& r : steps_rows) all.push_back(std::move(r));
        if (!risk_rows.empty()) {
            all.push_back(separatorEmpty());
            for (auto& r : risk_rows) all.push_back(std::move(r));
        }
        if (!rejection_rows.empty()) {
            all.push_back(separatorEmpty());
            for (auto& r : rejection_rows) all.push_back(std::move(r));
        }
        if (!actions.empty()) {
            all.push_back(separator());
            all.push_back(hbox(std::move(actions)) | center);
        }

        return vbox(std::move(all)) | borderStyled(BorderStyle::ROUNDED, loom::ui::design::theme::current_theme().palette->info);
    }

    bool OnEvent(Event event) override {
        if (opts_.state != PlanApprovalState::Pending)
            return ComponentBase::OnEvent(event);
        if (event == Event::Character('y') || event == Event::Character('Y')) {
            if (on_approve_) {
                opts_.state = PlanApprovalState::Approved;
                on_approve_();
                return true;
            }
        }
        if (event == Event::Character('m') || event == Event::Character('M')) {
            if (on_modify_) {
                opts_.state = PlanApprovalState::Modified;
                on_modify_(opts_.steps);
                return true;
            }
        }
        if (event == Event::Character('n') || event == Event::Escape) {
            if (on_reject_) {
                opts_.state = PlanApprovalState::Rejected;
                opts_.rejection_reason = opts_.rejection_reason
                    ? opts_.rejection_reason
                    : std::optional<std::string>("User rejected plan");
                on_reject_(*opts_.rejection_reason);
                return true;
            }
        }
        return ComponentBase::OnEvent(event);
    }

  private:
    PlanApprovalOptions opts_;
    OnApproveFn on_approve_;
    OnModifyFn  on_modify_;
    OnRejectFn  on_reject_;
    Component approve_btn_;
    Component modify_btn_;
    Component reject_btn_;
};

[[nodiscard]] inline Component MakePlanApprovalMessage(
    PlanApprovalOptions opts,
    PlanApprovalComponent::OnApproveFn on_approve = nullptr,
    PlanApprovalComponent::OnModifyFn  on_modify  = nullptr,
    PlanApprovalComponent::OnRejectFn  on_reject  = nullptr)
{
    return Make<PlanApprovalComponent>(
        std::move(opts),
        std::move(on_approve), std::move(on_modify), std::move(on_reject));
}

} // namespace loom::ui::messages
