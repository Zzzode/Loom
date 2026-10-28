/// @file program_status.cpp
/// @brief Conservative capability negotiation and retained operation status.
module loom.ui.app.program_status;

namespace loom::ui {
namespace {

namespace osc = terminal::osc;
using State = osc::ProgramStatusState;
using Kind = osc::ProgramStatusKind;

std::string_view trim(std::string_view value) noexcept {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const auto first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    return value.substr(first, value.find_last_not_of(whitespace) - first + 1);
}

bool list_contains(std::string_view list, std::string_view item) noexcept {
    while (true) {
        const auto end = list.find(',');
        if (list.substr(0, end) == item) return true;
        if (end == std::string_view::npos) return false;
        list.remove_prefix(end + 1);
    }
}

bool valid_pair(std::string_view key, std::string_view value) noexcept {
    if (key.empty() || key.size() > 16) return false;
    for (const unsigned char byte : key) {
        if (byte < 'a' || byte > 'z') return false;
    }
    for (const unsigned char byte : value) {
        if (!((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
              (byte >= '0' && byte <= '9') || byte == '_' || byte == '.' || byte == ',' ||
              byte == '+' || byte == '/' || byte == '=' || byte == '-')) {
            return false;
        }
    }
    return true;
}

unsigned int kind_bit(Kind kind) noexcept {
    switch (kind) {
    case Kind::Permission:
        return 1;
    case Kind::Question:
        return 2;
    case Kind::Auth:
        return 4;
    case Kind::None:
        return 0;
    }
    return 0;
}

}  // namespace

ProgramStatusReporter::ProgramStatusReporter(Writer writer)
    : writer_(std::move(writer)) {}

void ProgramStatusReporter::Start(bool interactive, Time now) noexcept {
    if (capability_ != Capability::NotStarted) return;
    capability_ = interactive ? Capability::Probing : Capability::Disabled;
    if (!interactive) return;
    deadline_ = now + std::chrono::milliseconds(1000);
    try {
        if (!writer_ || !writer_(osc::program_status_query())) {
            capability_ = Capability::Disabled;
        }
    } catch (...) {
        capability_ = Capability::Disabled;
    }
}

void ProgramStatusReporter::Expire(Time now) noexcept {
    if (capability_ == Capability::Probing && now >= deadline_) {
        capability_ = Capability::Disabled;
    }
}

bool ProgramStatusReporter::HandleControl(std::string_view raw, Time now) noexcept {
    constexpr std::string_view command = "\033]7501";
    if (!raw.starts_with(command)) return false;
    if (raw.size() > command.size()) {
        const char boundary = raw[command.size()];
        if (boundary != ';' && boundary != '\a' && boundary != '\033') return false;
    }
    Expire(now);
    if (capability_ != Capability::Probing || raw.size() > 4096 || !raw.starts_with("\033]7501;"))
        return true;

    std::string_view body = raw.substr(7);
    if (body.ends_with("\033\\"))
        body.remove_suffix(2);
    else if (body.ends_with('\a'))
        body.remove_suffix(1);
    else
        return true;

    const auto first_colon = body.find(':');
    if (trim(body.substr(0, first_colon)) != "?") return true;
    std::optional<std::string_view> states;
    std::optional<std::string_view> kinds;
    if (first_colon != std::string_view::npos) {
        body.remove_prefix(first_colon + 1);
        while (true) {
            const auto colon = body.find(':');
            const auto pair = body.substr(0, colon);
            const auto equals = pair.find('=');
            if (equals != std::string_view::npos) {
                const auto key = trim(pair.substr(0, equals));
                const auto value = trim(pair.substr(equals + 1));
                if (valid_pair(key, value)) {
                    if (key == "states")
                        states = value;
                    else if (key == "kinds")
                        kinds = value;
                }
            }
            if (colon == std::string_view::npos) break;
            body.remove_prefix(colon + 1);
        }
    }
    if (states) {
        for (const auto state : {"idle", "working", "done", "blocked", "error"}) {
            if (!list_contains(*states, state)) {
                capability_ = Capability::Disabled;
                return true;
            }
        }
    }
    supported_kinds_ = 7;
    if (kinds) {
        supported_kinds_ = 0;
        if (list_contains(*kinds, "permission")) supported_kinds_ |= 1;
        if (list_contains(*kinds, "question")) supported_kinds_ |= 2;
        if (list_contains(*kinds, "auth")) supported_kinds_ |= 4;
    }
    capability_ = Capability::Enabled;
    return true;
}

void ProgramStatusReporter::Recompute() noexcept {
    desired_kind_ = Kind::None;
    if (blocked_ != Kind::None) {
        desired_ = State::Blocked;
        desired_kind_ = blocked_;
    } else if (active_ || working_) {
        desired_ = State::Working;
    } else {
        desired_ = outcome_;
        if (outcome_ == State::Blocked) desired_kind_ = Kind::Question;
    }
}

void ProgramStatusReporter::Begin() noexcept {
    if (capability_ == Capability::Stopped) return;
    active_ = true;
    cancelled_ = false;
    outcome_ = State::Idle;
    Recompute();
}

void ProgramStatusReporter::Cancel() noexcept {
    if (capability_ == Capability::Stopped) return;
    cancelled_ = true;
    outcome_ = State::Idle;
    Recompute();
}

void ProgramStatusReporter::Complete(bool error, bool paused) noexcept {
    if (capability_ == Capability::Stopped) return;
    active_ = false;
    working_ = false;
    outcome_ =
        cancelled_ ? State::Idle : (paused ? State::Blocked : (error ? State::Error : State::Done));
    Recompute();
}

void ProgramStatusReporter::Acknowledge() noexcept {
    if (capability_ == Capability::Stopped) return;
    outcome_ = State::Idle;
    Recompute();
}

void ProgramStatusReporter::Refresh(bool working, Kind blocked) noexcept {
    if (capability_ == Capability::Stopped) return;
    working_ = working;
    blocked_ = blocked;
    Recompute();
}

void ProgramStatusReporter::Emit() noexcept {
    try {
        const auto kind = (supported_kinds_ & kind_bit(desired_kind_)) ? desired_kind_ : Kind::None;
        auto encoded = osc::program_status(desired_, kind);
        if (!encoded) {
            capability_ = Capability::Disabled;
            return;
        }
        if (last_report_ && *last_report_ == *encoded) return;
        if (!writer_ || !writer_(*encoded)) {
            capability_ = Capability::Disabled;
            return;
        }
        last_report_ = std::move(*encoded);
    } catch (...) {
        capability_ = Capability::Disabled;
    }
}

void ProgramStatusReporter::Flush(Time now) noexcept {
    Expire(now);
    if (capability_ == Capability::Enabled) Emit();
}

void ProgramStatusReporter::Finish(bool normal, Time now) noexcept {
    if (capability_ == Capability::Stopped) return;
    Expire(now);
    // A background human request may overlay a completed foreground result.
    // Preserve that result on normal exit, not the transient blocked snapshot.
    desired_ = normal && !active_ && !working_ && !cancelled_ &&
                       (outcome_ == State::Done || outcome_ == State::Error)
                   ? outcome_
                   : State::Idle;
    desired_kind_ = Kind::None;
    if (capability_ == Capability::Enabled) Emit();
    capability_ = Capability::Stopped;
    writer_ = {};
}

}  // namespace loom::ui
