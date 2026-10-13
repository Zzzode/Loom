// app_run.cpp — impl unit for RunApp() and the extern "C" bridge.
// Kept out of app.cppm so loom.hooks.tool_permissions and the FTXUI
// screen-interactive / termios closure stay out of the interface BMI.
module;

#include <unistd.h>
#include <termios.h>  // tcgetattr/tcsetattr/termios/VLNEXT
#include <cerrno>
#include <csignal>

#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>

module loom.ui.app.app;

import std;

import loom.query.query_engine;
import loom.commands.registry;
import loom.hooks.tool_permissions;
import loom.hooks.lifecycle_hooks;

namespace loom::ui {

namespace {
#if defined(__APPLE__) || defined(__linux__)
static_assert(std::atomic<bool>::is_always_lock_free);
std::atomic<bool> hangup_relay_active{false};
void (*terminal_exit_signal_handler)(int) = nullptr;

void HandleTerminalHangup(int) {
    const int saved_errno = errno;
    if (hangup_relay_active.load(std::memory_order_acquire)) {
        terminal_exit_signal_handler(SIGTERM);
    }
    errno = saved_errno;
}

// FTXUI handles termination but does not register SIGHUP. Capture its installed
// handler so a hangup also takes the normal loop-exit/worker-drain path, even
// while Ctrl+Z temporarily uninstalls the terminal handlers. Keep repeated
// hangups harmless during worker draining, then restore the shell's disposition.
class TerminalHangupGuard {
public:
    TerminalHangupGuard() {
        struct sigaction termination {};
        if (sigaction(SIGTERM, nullptr, &termination) != 0 ||
            (termination.sa_flags & SA_SIGINFO) ||
            termination.sa_handler == SIG_DFL || termination.sa_handler == SIG_IGN) {
            throw std::runtime_error("Terminal termination handler is unavailable");
        }
        terminal_exit_signal_handler = termination.sa_handler;
        struct sigaction hangup {};
        hangup.sa_handler = HandleTerminalHangup;
        sigemptyset(&hangup.sa_mask);
        hangup_relay_active.store(true, std::memory_order_release);
        if (sigaction(SIGHUP, &hangup, &original_) != 0) {
            Deactivate();
            throw std::runtime_error("Cannot install terminal hangup handler");
        }
    }
    ~TerminalHangupGuard() {
        Deactivate();
        (void)sigaction(SIGHUP, &original_, nullptr);
    }
    void Deactivate() {
        hangup_relay_active.store(false, std::memory_order_release);
    }
private:
    struct sigaction original_ {};
};
#endif

[[nodiscard]] int RunApp(
    core::QueryEngine& engine,
    loom::commands::AppCommandRegistry& cmd_registry,
    const std::filesystem::path* sessions_dir,
    loom::hooks::ToolPermissionHook* permission_hook,
    loom::hooks::LifecycleHookRegistry* lifecycle_hooks,
    bool mouse_mode,
    std::uint32_t tool_round_budget
) {
    // Use the alternate-screen fullscreen - the REPL owns the terminal.
    auto screen = ScreenInteractive::Fullscreen();

    // RFC 0010: keep FTXUI mouse tracking enabled by default for in-app
    // drag selection and automatic copy. Explicit "mouseMode": false opts
    // into native terminal selection. TrackMouse() must be called before
    // Loop() (FTXUI reads the flag in Install(), which Loop() calls).
    if (!mouse_mode) {
        screen.TrackMouse(false);
    }

    // ── macOS/BSD line-discipline workaround: disable VLNEXT ─────────────
    // VLNEXT (the "literal-next" char, Ctrl+V by default) is processed by the
    // terminal line discipline EVEN in non-canonical mode (ICANON off) on
    // macOS/BSD. FTXUI puts the terminal in non-canonical mode (ICANON|ECHO
    // off) but does NOT clear c_cc[VLNEXT], so every Ctrl+V the user presses
    // gets consumed as an lnext escape: a pair of \x16 bytes collapses into a
    // single literal \x16. Net effect: pressing Ctrl+V 8× registers only 4×
    // (floor(N/2)) — half the image-paste keystrokes are silently dropped
    // before FTXUI's event loop ever sees them.
    //
    // Fix: clear VLNEXT ourselves before entering the loop. We do this BEFORE
    // screen.Loop() because FTXUI's Install() (called inside Loop) does
    // tcgetattr()+save-then-restore: it will read our VLNEXT=0, preserve it
    // for the session, and restore that same value on exit. To still give the
    // parent shell back its original Ctrl+V lnext on exit, we snapshot the
    // true original termios here and re-apply it after Loop() returns.
    //
    // Verified: sending N×\x16 through a pty with VLNEXT=0 delivers all N
    // bytes; with VLNEXT at its default, only floor(N/2) arrive. This is
    // independent of the osascript/clipboard path (setsid/closefrom there
    // remain good hygiene but were NOT the cause of keystroke loss).
#if defined(__APPLE__) || defined(__linux__)
    struct termios orig_termios;
    const bool have_orig = (tcgetattr(STDIN_FILENO, &orig_termios) == 0);
    if (have_orig) {
        struct termios t = orig_termios;
        t.c_cc[VLNEXT] = 0;  // 0 == _POSIX_VDISABLE: disable literal-next
        // Deliver Ctrl+C to AppAdapter's cancel/double-press handling rather
        // than turning the first press into SIGINT. Preserve Ctrl+Z/ISIG.
        t.c_cc[VINTR] = _POSIX_VDISABLE;
        // Disable XON/XOFF software flow control.  With IXON left on (the
        // macOS/Linux default), the tty line discipline consumes Ctrl+S
        // (XOFF) before the process ever sees it — so in-app Ctrl+S bindings
        // (e.g. /statusline "save") silently never fire.  FTXUI's raw-mode
        // setup clears only ICANON/ECHO, not IXON, so we do it here.  The
        // orig_termios snapshot + restore below returns flow control to the
        // parent shell on exit.
        t.c_iflag &= ~(IXON | IXOFF);
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }
#endif

    bool should_exit = false;
#if defined(__APPLE__) || defined(__linux__)
    // Declared before app so the handler also protects its worker-drain phase.
    std::unique_ptr<TerminalHangupGuard> hangup_guard;
#endif

    auto app = Make<AppAdapter>(
        &engine,
        lifecycle_hooks,
        &cmd_registry,
        sessions_dir ? std::optional<std::filesystem::path>{*sessions_dir}
                     : std::nullopt,
        [&screen, &should_exit]() {
            should_exit = true;
            screen.Exit();
        }
    );

    app->set_screen(&screen);
    app->set_tool_round_budget(tool_round_budget == 0
        ? std::nullopt : std::optional<std::uint32_t>{tool_round_budget});

    // RFC 0007 Phase 2: set the persistent mouse-mode hint in the status bar.
    app->set_mouse_mode_label(mouse_mode);
    // Store config value for F2 runtime toggle.
    app->set_mouse_mode(mouse_mode);

    if (permission_hook && !permission_hook->is_auto_approve_mode()) {
        auto ui_callback = app->get_permission_callback();
        permission_hook->set_ask_user_fn(
            [ui_callback](const loom::hooks::PermissionContext& ctx) -> loom::hooks::PermissionDecision {
                bool allowed = ui_callback(ctx.tool_name, ctx.args);
                return allowed ? loom::hooks::PermissionDecision::allow
                               : loom::hooks::PermissionDecision::deny;
            }
        );
    }

    app->SyncState();

    try {
        ftxui::Loop loop(&screen, app);
#if defined(__APPLE__) || defined(__linux__)
        hangup_guard = std::make_unique<TerminalHangupGuard>();
#endif
        try {
            loop.Run();
        } catch (...) {
#if defined(__APPLE__) || defined(__linux__)
            hangup_guard->Deactivate();
#endif
            throw;
        }
#if defined(__APPLE__) || defined(__linux__)
        hangup_guard->Deactivate();
#endif
    } catch (...) {
#if defined(__APPLE__) || defined(__linux__)
        if (have_orig) (void)tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
#endif
        throw;
    }

    // Restore the parent shell's original termios (FTXUI's on_exit restored
    // what IT read, which carries VLNEXT=0; re-apply the true original so
    // Ctrl+V lnext works again in the user's shell after loom exits).
#if defined(__APPLE__) || defined(__linux__)
    if (have_orig) {
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    }
#endif

    return should_exit ? 0 : 1;
}

}  // namespace

extern "C" int loom_ui_run_app_bridge(
    loom::core::QueryEngine* engine,
    loom::hooks::LifecycleHookRegistry* lifecycle_hooks,
    loom::commands::AppCommandRegistry* cmd_registry,
    const std::filesystem::path* sessions_dir,
    loom::hooks::ToolPermissionHook* permission_hook,
    bool mouse_mode,
    std::uint32_t tool_round_budget
) {
    return loom::ui::RunApp(*engine, *cmd_registry, sessions_dir, permission_hook, lifecycle_hooks, mouse_mode, tool_round_budget);
}

}  // namespace loom::ui
