/// @file program_status.cppm
/// @brief UI-owned OSC 7501 negotiation and foreground operation outcome.
export module loom.ui.app.program_status;

import std;
import loom.terminal.osc;

export namespace loom::ui {

/// State mutations are silent. Only explicit UI-thread delivery boundaries
/// Start, Flush and Finish write terminal bytes; Render may update state.
class ProgramStatusReporter {
public:
    using Writer = std::function<bool(std::string_view)>;
    using Time = std::chrono::steady_clock::time_point;

    explicit ProgramStatusReporter(Writer writer = {});
    void Start(bool interactive, Time now) noexcept;
    [[nodiscard]] bool HandleControl(std::string_view raw, Time now) noexcept;
    void Begin() noexcept;
    void Cancel() noexcept;
    void Complete(bool error, bool paused = false) noexcept;
    void Acknowledge() noexcept;
    void Refresh(bool working,
                 terminal::osc::ProgramStatusKind blocked =
                     terminal::osc::ProgramStatusKind::None) noexcept;
    void Flush(Time now) noexcept;
    void Finish(bool normal, Time now) noexcept;

private:
    enum class Capability { NotStarted, Probing, Enabled, Disabled, Stopped };
    void Expire(Time now) noexcept;
    void Recompute() noexcept;
    void Emit() noexcept;

    Writer writer_;
    Capability capability_ = Capability::NotStarted;
    Time deadline_{};
    unsigned int supported_kinds_ = 7;
    bool active_ = false;
    bool working_ = false;
    bool cancelled_ = false;
    terminal::osc::ProgramStatusKind blocked_ = terminal::osc::ProgramStatusKind::None;
    terminal::osc::ProgramStatusState outcome_ = terminal::osc::ProgramStatusState::Idle;
    terminal::osc::ProgramStatusState desired_ = terminal::osc::ProgramStatusState::Idle;
    terminal::osc::ProgramStatusKind desired_kind_ = terminal::osc::ProgramStatusKind::None;
    std::optional<std::string> last_report_;
};

}  // namespace loom::ui
