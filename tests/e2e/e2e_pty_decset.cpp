/// @file e2e_pty_decset.cpp
/// @brief E2E-4: PTY byte-level tests for terminal DECSET sequences emitted by
///        the loom binary at startup, based on the mouseMode config setting.
///
/// RFC 0010 enables FTXUI mouse tracking by default for in-app drag selection
/// and automatic copy. Explicit mouseMode: false preserves native terminal
/// selection; explicit true and settings cascade behavior remain supported.
///
/// FTXUI's ScreenInteractive::Install() sends these DECSET escape sequences:
///   ESC[?1049h — alternate screen buffer (ALWAYS sent, confirms TUI running)
///   ESC[?1000h — X11 mouse tracking (ONLY when mouse tracking is enabled)
///   ESC[?1003h — any-motion tracking (ONLY when mouse tracking is enabled)
///   ESC[?1015h — urxvt mouse encoding (ONLY when mouse tracking is enabled)
///   ESC[?1006h — SGR mouse encoding (ONLY when mouse tracking is enabled)
///
/// Execution strategy: forkpty -> setenv -> execl -> read PTY master -> kill.
/// All comments and output in English.

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#if defined(__APPLE__)
#include <util.h>  // forkpty
#else
#include <pty.h>
#endif
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// DECSET sequence constants
// ---------------------------------------------------------------------------
static constexpr std::string_view SEQ_ALT_SCREEN   = "\x1b[?1049h";
static constexpr std::string_view SEQ_MOUSE_X11    = "\x1b[?1000h";
static constexpr std::string_view SEQ_MOUSE_MOTION = "\x1b[?1003h";
static constexpr std::string_view SEQ_MOUSE_URXVT  = "\x1b[?1015h";
static constexpr std::string_view SEQ_MOUSE_SGR    = "\x1b[?1006h";

static const std::vector<std::string_view> MOUSE_SEQS = {
    SEQ_MOUSE_X11, SEQ_MOUSE_MOTION, SEQ_MOUSE_URXVT, SEQ_MOUSE_SGR,
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool file_exists_executable(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & 0111);
}

/// Locate loom relative to argv[0] or via env LOOM_BINARY.
static std::string locate_repl_binary(const char* argv0) {
    if (const char* e = ::getenv("LOOM_BINARY")) {
        if (file_exists_executable(e)) return e;
    }
    char buf[PATH_MAX];
    ::snprintf(buf, sizeof(buf), "%s", argv0);
    std::string here = ::dirname(buf);

    std::vector<std::string> candidates = {
        here + "/../../build/debug/bin/loom",  // from <build>/tests/e2e/
        here + "/../build/debug/bin/loom",
        here + "/build/debug/bin/loom",
        "build/debug/bin/loom",                 // from repo root
    };
    for (const auto& c : candidates) {
        if (file_exists_executable(c)) return c;
    }
    return {};
}

/// Create a unique temporary directory. Returns empty string on failure.
static std::string make_temp_dir() {
    char tmpl[] = "/tmp/loom_e2e_pty_XXXXXX";
    char* dir = ::mkdtemp(tmpl);
    if (!dir) return {};
    return dir;
}

/// Write content to a file, creating parent directories as needed.
static bool write_file(const fs::path& path, const std::string& content) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) return false;
    std::ofstream f(path);
    if (!f) return false;
    f << content;
    return f.good();
}

static bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

// ---------------------------------------------------------------------------
// PTY spawn + capture
// ---------------------------------------------------------------------------

struct EnvVar {
    std::string key;
    std::string value;
    bool unset = false;  // if true, unsetenv instead of setenv
};

struct PtyResult {
    int         exit_code = -1;
    std::string output;      // combined stdout+stderr from the PTY
    bool        spawn_ok = false;
};

/// Install() flushes alternate-screen and mouse modes before Draw() emits
/// the first cursor visibility sequence. Waiting for that sequence observes
/// the complete installation even when the PTY splits it across reads.
static bool has_completed_terminal_install(std::string_view output) {
    const auto alt = output.find(SEQ_ALT_SCREEN);
    if (alt == std::string_view::npos) return false;
    const auto after_alt = output.substr(alt + SEQ_ALT_SCREEN.size());
    return contains(after_alt, "\x1b[?25h") || contains(after_alt, "\x1b[?25l");
}

/// Spawn `binary_path` in a PTY, capture until terminal installation is
/// observable (or the child closes the PTY), then kill and reap it. CTest's
/// existing process timeout bounds a child that never reaches readiness.
static PtyResult run_pty_and_capture(
    const std::string& binary_path,
    const std::vector<EnvVar>& env_vars)
{
    PtyResult result;

    int master_fd = -1;
    char slave_name[256] = {};
    struct winsize ws{};
    ws.ws_row = 24;
    ws.ws_col = 80;

    pid_t pid = ::forkpty(&master_fd, slave_name, nullptr, &ws);
    if (pid < 0) {
        result.output = "forkpty() failed: " + std::string(std::strerror(errno));
        return result;
    }

    if (pid == 0) {
        // child: set environment then exec
        for (const auto& ev : env_vars) {
            if (ev.unset) {
                ::unsetenv(ev.key.c_str());
            } else {
                ::setenv(ev.key.c_str(), ev.value.c_str(), 1);
            }
        }
        ::execl(binary_path.c_str(), "loom", nullptr);
        // exec failed
        const char msg[] = "execl failed for loom\n";
        ::write(STDERR_FILENO, msg, sizeof(msg) - 1);
        ::_exit(127);
    }

    // parent: read from master_fd
    result.spawn_ok = true;
    std::array<char, 8192> buffer{};

    while (!has_completed_terminal_install(result.output)) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(master_fd, &rfds);
        int sel = ::select(master_fd + 1, &rfds, nullptr, nullptr, nullptr);
        if (sel < 0) {
            if (errno == EINTR) continue;
            break;
        }

        ssize_t n = ::read(master_fd, buffer.data(), buffer.size());
        if (n > 0) {
            result.output.append(buffer.data(), static_cast<size_t>(n));
        } else if (n == 0) {
            break;  // EOF — child exited
        } else {
            if (errno == EINTR) continue;
            break;  // EIO or other — child likely exited/killed
        }
    }

    // Kill the child if still running, then reap.
    ::kill(pid, SIGKILL);
    ::close(master_fd);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) result.exit_code = -WTERMSIG(status);

    return result;
}

// ---------------------------------------------------------------------------
// Debug output helpers
// ---------------------------------------------------------------------------

/// Print captured output with escape sequences visualized (for debugging).
static void print_visible(const std::string& data, size_t max_bytes = 2048) {
    const size_t n = std::min(data.size(), max_bytes);
    printf("    \"");
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == 0x1b)       printf("\\x1b");
        else if (c == '\n')  printf("\\n");
        else if (c == '\r')  printf("\\r");
        else if (c == '\t')  printf("\\t");
        else if (c < 32 || c == 127) printf("\\x%02x", c);
        else                 putchar(c);
    }
    printf("\"\n");
    if (data.size() > max_bytes) {
        printf("    ... (%zu more bytes)\n", data.size() - max_bytes);
    }
}

/// Print a hex dump of the first `max_bytes` bytes (for debugging).
static void print_hex_dump(const std::string& data, size_t max_bytes = 512) {
    const size_t n = std::min(data.size(), max_bytes);
    for (size_t i = 0; i < n; i += 16) {
        printf("    %04zx: ", i);
        const size_t end = std::min(i + 16, n);
        for (size_t j = i; j < end; ++j) {
            printf("%02x ", static_cast<unsigned char>(data[j]));
        }
        printf("\n");
    }
    if (data.size() > max_bytes) {
        printf("    ... (%zu more bytes)\n", data.size() - max_bytes);
    }
}

// ---------------------------------------------------------------------------
// Test case helpers
// ---------------------------------------------------------------------------

/// Build the base environment shared by all tests.
static std::vector<EnvVar> base_env(const std::string& home_dir) {
    return {
        {"TERM",          "xterm-256color"},
        {"HOME",          home_dir},
        {"LOOM_API_KEY",  "test-key"},
        {"LOOM_MODEL",    "test-model"},
    };
}

/// Add or override an env var in the vector.
static void set_env(std::vector<EnvVar>& envs, const std::string& key,
                    const std::string& value) {
    for (auto& ev : envs) {
        if (ev.key == key) { ev.value = value; ev.unset = false; return; }
    }
    envs.push_back({key, value, false});
}

/// Mark an env var for unset in the vector.
static void unset_env(std::vector<EnvVar>& envs, const std::string& key) {
    for (auto& ev : envs) {
        if (ev.key == key) { ev.unset = true; return; }
    }
    envs.push_back({key, "", true});
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int /*argc*/, char* argv[]) {
    const std::string bin = locate_repl_binary(argv[0]);
    if (bin.empty()) {
        fprintf(stderr, "[E2E-4] cannot locate loom binary\n");
        return 125;  // skip code (matches e2e_repl_dry_run convention)
    }
    printf("[E2E-4] binary: %s\n", bin.c_str());

    int passed = 0;
    int failed = 0;

    // Assertion helper: prints PASS/FAIL and increments counters.
    auto check = [&](const char* desc, bool ok) {
        if (ok) {
            printf("  PASS: %s\n", desc);
            ++passed;
        } else {
            printf("  FAIL: %s\n", desc);
            ++failed;
        }
    };

    // Regression: startup logging/cursor controls and incomplete DECSET
    // bursts must not be mistaken for completion. Feed one byte at a time
    // so readiness also works when the PTY splits escape sequences.
    {
        printf("\n[E2E-4] Regression: wait for complete terminal installation\n");
        const std::string install = std::string(SEQ_ALT_SCREEN) +
            "\x1b[?7l" + std::string(SEQ_MOUSE_X11) +
            std::string(SEQ_MOUSE_MOTION) + std::string(SEQ_MOUSE_URXVT) +
            std::string(SEQ_MOUSE_SGR);
        std::string captured = "startup\x1b[?25h";
        bool premature = has_completed_terminal_install(captured);
        for (const char byte : install) {
            captured += byte;
            premature = premature || has_completed_terminal_install(captured);
        }
        const std::string first_frame_end = "\x1b[?25l";
        for (std::size_t i = 0; i + 1 < first_frame_end.size(); ++i) {
            captured += first_frame_end[i];
            premature = premature || has_completed_terminal_install(captured);
        }
        check("partial startup/DECSET/frame bytes do not end capture", !premature);
        captured += first_frame_end.back();
        check("complete first frame ends capture", has_completed_terminal_install(captured));
        check("all mouse sequences captured before readiness",
              std::all_of(MOUSE_SEQS.begin(), MOUSE_SEQS.end(),
                  [&](std::string_view seq) { return contains(captured, seq); }));
    }

    // On failure, print captured output for debugging.
    auto dump_on_fail = [&](const PtyResult& r) {
        if (r.output.empty()) {
            printf("  (no output captured)\n");
            return;
        }
        printf("  --- captured output (%zu bytes, exit=%d) ---\n",
               r.output.size(), r.exit_code);
        print_visible(r.output);
        printf("  --- hex dump (first 256 bytes) ---\n");
        print_hex_dump(r.output, 256);
    };

    // =======================================================================
    // Test 1: Default config (no mouseMode) -> mouse DECSET present
    // =======================================================================
    {
        printf("\n[E2E-4] Test 1: Default config (no mouseMode) -> mouse DECSET present\n");

        const std::string cfg_dir = make_temp_dir();
        const std::string home_dir = make_temp_dir();
        if (cfg_dir.empty() || home_dir.empty()) {
            printf("  FAIL: cannot create temp dirs\n");
            ++failed;
        } else {
            // settings.json with model but NO mouseMode key
            write_file(fs::path(cfg_dir) / "settings.json",
                       "{\"model\": \"test-model\"}");

            auto envs = base_env(home_dir);
            set_env(envs, "LOOM_CONFIG_DIR", cfg_dir);

            printf("  LOOM_CONFIG_DIR=%s (settings.json: no mouseMode)\n", cfg_dir.c_str());
            PtyResult r = run_pty_and_capture(bin, envs);
            printf("  exit=%d, captured %zu bytes\n", r.exit_code, r.output.size());

            const bool has_alt   = contains(r.output, SEQ_ALT_SCREEN);
            const bool has_mouse = std::all_of(
                MOUSE_SEQS.begin(), MOUSE_SEQS.end(),
                [&](std::string_view seq) { return contains(r.output, seq); });

            check("?1049h present (TUI started)", has_alt);
            check("all mouse DECSET sequences present by default", has_mouse);

            if (!has_alt || !has_mouse) dump_on_fail(r);

            fs::remove_all(cfg_dir);
            fs::remove_all(home_dir);
        }
    }

    // =======================================================================
    // Test 2: mouseMode: true -> mouse DECSET present
    // =======================================================================
    {
        printf("\n[E2E-4] Test 2: mouseMode=true -> mouse DECSET present\n");

        const std::string cfg_dir = make_temp_dir();
        const std::string home_dir = make_temp_dir();
        if (cfg_dir.empty() || home_dir.empty()) {
            printf("  FAIL: cannot create temp dirs\n");
            ++failed;
        } else {
            write_file(fs::path(cfg_dir) / "settings.json",
                       "{\"model\": \"test-model\", \"mouseMode\": true}");

            auto envs = base_env(home_dir);
            set_env(envs, "LOOM_CONFIG_DIR", cfg_dir);

            printf("  LOOM_CONFIG_DIR=%s (settings.json: mouseMode=true)\n", cfg_dir.c_str());
            PtyResult r = run_pty_and_capture(bin, envs);
            printf("  exit=%d, captured %zu bytes\n", r.exit_code, r.output.size());

            check("?1049h present (TUI started)",
                  contains(r.output, SEQ_ALT_SCREEN));
            check("?1000h (X11 mouse tracking)",
                  contains(r.output, SEQ_MOUSE_X11));
            check("?1003h (any-motion tracking)",
                  contains(r.output, SEQ_MOUSE_MOTION));
            check("?1015h (urxvt mouse encoding)",
                  contains(r.output, SEQ_MOUSE_URXVT));
            check("?1006h (SGR mouse encoding)",
                  contains(r.output, SEQ_MOUSE_SGR));

            const bool any_fail =
                !contains(r.output, SEQ_ALT_SCREEN) ||
                !contains(r.output, SEQ_MOUSE_X11) ||
                !contains(r.output, SEQ_MOUSE_MOTION) ||
                !contains(r.output, SEQ_MOUSE_URXVT) ||
                !contains(r.output, SEQ_MOUSE_SGR);
            if (any_fail) dump_on_fail(r);

            fs::remove_all(cfg_dir);
            fs::remove_all(home_dir);
        }
    }

    // =======================================================================
    // Test 3: ?1049h always present (confirms TUI event loop, not banner)
    // =======================================================================
    {
        printf("\n[E2E-4] Test 3: ?1049h always present (TUI event loop)\n");

        const std::string cfg_dir = make_temp_dir();
        const std::string home_dir = make_temp_dir();
        if (cfg_dir.empty() || home_dir.empty()) {
            printf("  FAIL: cannot create temp dirs\n");
            ++failed;
        } else {
            write_file(fs::path(cfg_dir) / "settings.json",
                       "{\"model\": \"test-model\"}");

            auto envs = base_env(home_dir);
            set_env(envs, "LOOM_CONFIG_DIR", cfg_dir);

            PtyResult r = run_pty_and_capture(bin, envs);
            printf("  exit=%d, captured %zu bytes\n", r.exit_code, r.output.size());

            // Explicitly assert ?1049h — this proves the app entered the TUI
            // event loop (Install() was called), not just printed a banner.
            check("?1049h present (entered TUI event loop)",
                  contains(r.output, SEQ_ALT_SCREEN));

            if (!contains(r.output, SEQ_ALT_SCREEN)) dump_on_fail(r);

            fs::remove_all(cfg_dir);
            fs::remove_all(home_dir);
        }
    }

    // =======================================================================
    // Test 4: Cascade priority — LOOM_CONFIG_DIR overrides HOME
    // =======================================================================
    {
        printf("\n[E2E-4] Test 4: LOOM_CONFIG_DIR overrides HOME (~/.loom)\n");

        const std::string home_dir = make_temp_dir();
        const std::string cfg_dir = make_temp_dir();
        if (home_dir.empty() || cfg_dir.empty()) {
            printf("  FAIL: cannot create temp dirs\n");
            ++failed;
        } else {
            // ~/.loom/settings.json says mouseMode: false
            write_file(fs::path(home_dir) / ".loom" / "settings.json",
                       "{\"model\": \"test-model\", \"mouseMode\": false}");
            // LOOM_CONFIG_DIR/settings.json says mouseMode: true
            write_file(fs::path(cfg_dir) / "settings.json",
                       "{\"model\": \"test-model\", \"mouseMode\": true}");

            auto envs = base_env(home_dir);
            set_env(envs, "LOOM_CONFIG_DIR", cfg_dir);

            printf("  HOME=%s (~/.loom/settings.json: mouseMode=false)\n", home_dir.c_str());
            printf("  LOOM_CONFIG_DIR=%s (settings.json: mouseMode=true)\n", cfg_dir.c_str());
            PtyResult r = run_pty_and_capture(bin, envs);
            printf("  exit=%d, captured %zu bytes\n", r.exit_code, r.output.size());

            // LOOM_CONFIG_DIR takes precedence: mouse DECSET must be present.
            const bool has_mouse = std::any_of(
                MOUSE_SEQS.begin(), MOUSE_SEQS.end(),
                [&](std::string_view seq) { return contains(r.output, seq); });

            check("?1049h present (TUI started)",
                  contains(r.output, SEQ_ALT_SCREEN));
            check("mouse DECSET present (LOOM_CONFIG_DIR wins)", has_mouse);

            if (!has_mouse) dump_on_fail(r);

            fs::remove_all(home_dir);
            fs::remove_all(cfg_dir);
        }
    }

    // =======================================================================
    // Test 5: ~/.agents fallback (no LOOM_CONFIG_DIR, no ~/.loom)
    // =======================================================================
    {
        printf("\n[E2E-4] Test 5: ~/.agents fallback (no LOOM_CONFIG_DIR)\n");

        const std::string home_dir = make_temp_dir();
        if (home_dir.empty()) {
            printf("  FAIL: cannot create temp dir\n");
            ++failed;
        } else {
            // Only ~/.agents/settings.json exists (no ~/.loom directory).
            write_file(fs::path(home_dir) / ".agents" / "settings.json",
                       "{\"model\": \"test-model\", \"mouseMode\": true}");

            auto envs = base_env(home_dir);
            unset_env(envs, "LOOM_CONFIG_DIR");  // must NOT be set

            printf("  HOME=%s (~/.agents/settings.json: mouseMode=true)\n", home_dir.c_str());
            printf("  LOOM_CONFIG_DIR unset\n");
            PtyResult r = run_pty_and_capture(bin, envs);
            printf("  exit=%d, captured %zu bytes\n", r.exit_code, r.output.size());

            // settings_home_read() falls back to ~/.agents/settings.json,
            // so mouse DECSET must be present.
            const bool has_mouse = std::any_of(
                MOUSE_SEQS.begin(), MOUSE_SEQS.end(),
                [&](std::string_view seq) { return contains(r.output, seq); });

            check("?1049h present (TUI started)",
                  contains(r.output, SEQ_ALT_SCREEN));
            check("mouse DECSET present (~/.agents fallback)", has_mouse);

            if (!has_mouse) dump_on_fail(r);

            fs::remove_all(home_dir);
        }
    }

    // =======================================================================
    // Test 6: Explicit mouseMode: false -> native selection, no mouse DECSET
    // =======================================================================
    {
        printf("\n[E2E-4] Test 6: mouseMode=false -> no mouse DECSET\n");

        const std::string cfg_dir = make_temp_dir();
        const std::string home_dir = make_temp_dir();
        if (cfg_dir.empty() || home_dir.empty()) {
            printf("  FAIL: cannot create temp dirs\n");
            ++failed;
        } else {
            write_file(fs::path(cfg_dir) / "settings.json",
                       "{\"model\": \"test-model\", \"mouseMode\": false}");

            auto envs = base_env(home_dir);
            set_env(envs, "LOOM_CONFIG_DIR", cfg_dir);
            PtyResult r = run_pty_and_capture(bin, envs);
            printf("  exit=%d, captured %zu bytes\n", r.exit_code, r.output.size());

            const bool has_alt = contains(r.output, SEQ_ALT_SCREEN);
            const bool has_mouse = std::any_of(
                MOUSE_SEQS.begin(), MOUSE_SEQS.end(),
                [&](std::string_view seq) { return contains(r.output, seq); });
            check("?1049h present (TUI started)", has_alt);
            check("explicit false emits no mouse DECSET sequences", !has_mouse);
            if (!has_alt || has_mouse) dump_on_fail(r);

            fs::remove_all(cfg_dir);
            fs::remove_all(home_dir);
        }
    }

    // =======================================================================
    // Test 7: ~/.agents fallback false overrides the enabled default
    // =======================================================================
    {
        printf("\n[E2E-4] Test 7: ~/.agents mouseMode=false overrides default\n");

        const std::string home_dir = make_temp_dir();
        if (home_dir.empty()) {
            printf("  FAIL: cannot create temp dir\n");
            ++failed;
        } else {
            write_file(fs::path(home_dir) / ".agents" / "settings.json",
                       "{\"model\": \"test-model\", \"mouseMode\": false}");

            auto envs = base_env(home_dir);
            unset_env(envs, "LOOM_CONFIG_DIR");
            PtyResult r = run_pty_and_capture(bin, envs);
            printf("  exit=%d, captured %zu bytes\n", r.exit_code, r.output.size());

            const bool has_alt = contains(r.output, SEQ_ALT_SCREEN);
            const bool has_mouse = std::any_of(
                MOUSE_SEQS.begin(), MOUSE_SEQS.end(),
                [&](std::string_view seq) { return contains(r.output, seq); });
            check("?1049h present (TUI started)", has_alt);
            check("fallback false emits no mouse DECSET sequences", !has_mouse);
            if (!has_alt || has_mouse) dump_on_fail(r);

            fs::remove_all(home_dir);
        }
    }

    // =======================================================================
    // Summary
    // =======================================================================
    printf("\n[E2E-4] Results: %d passed, %d failed\n", passed, failed);
    if (failed == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    return 1;
}
