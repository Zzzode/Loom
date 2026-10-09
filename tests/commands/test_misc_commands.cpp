/// @file test_misc_commands.cpp
/// @brief TerminalSetupCommand and ResumeCommand tests.

#include <gtest/gtest.h>
#include <cstdlib>
#include <atomic>
#include <unistd.h>

import std;
import loom.commands.command;
import loom.commands.registry;
import loom.query.query_engine;
import loom.tools.tool;
import loom.types.types;
import loom.commands.agents;
import loom.commands.clear;
import loom.commands.config;
import loom.config.config;
import loom.commands.help;
import loom.commands.hooks;
import loom.commands.insights;
import loom.commands.model;
import loom.commands.mcp_cmd;
import loom.commands.rewind;
import loom.commands.plugin_cmd;
import loom.commands.plugin_ui_data;
import loom.commands.plugin_parse_args;
import loom.commands.resume;
import loom.utils.error;
import loom.serdes.json;
import loom.commands.terminal_setup;
import loom.platform.hyperlink;
import loom.services.mcp.xaa_idp_login;

namespace {

namespace fs = std::filesystem;

struct EnvironmentGuard {
    std::string name;
    std::optional<std::string> previous;

    EnvironmentGuard(std::string key, const std::string& value) : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        setenv(name.c_str(), value.c_str(), 1);
    }

    ~EnvironmentGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

struct EnvironmentUnsetGuard {
    std::string name;
    std::optional<std::string> previous;

    explicit EnvironmentUnsetGuard(std::string key) : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        unsetenv(name.c_str());
    }

    ~EnvironmentUnsetGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

loom::core::CommandContext ctx(std::vector<std::string> args = {}, std::string raw = {}) {
    return loom::core::CommandContext{
        .args = std::move(args),
        .raw_input = std::move(raw),
        .cwd = {},
    };
}

} // namespace

// ============================================================================
// terminal-setup OSC 8 hyperlink support
// TS REF: src/commands/terminalSetup/terminalSetup.tsx L54-72 formatPathLink()
// ============================================================================

namespace {

namespace fs = std::filesystem;

// RAII HOME override for terminal-setup tests; mirrors HomeGuard above but
// with its own temp prefix/label so the suites stay independent if co-located.
struct TerminalSetupHomeGuard {
    std::optional<std::string> previous;
    fs::path tmp;
    explicit TerminalSetupHomeGuard() {
        if (const char* h = std::getenv("HOME")) previous = h;
        auto base = fs::temp_directory_path();
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        static std::atomic<long> counter{0};
        tmp = base / ("cc-termsetup-test-" + std::to_string(stamp) + "-" +
                      std::to_string(counter.fetch_add(1)));
        fs::create_directories(tmp);
        setenv("HOME", tmp.c_str(), 1);
    }
    ~TerminalSetupHomeGuard() {
        if (previous) {
            setenv("HOME", previous->c_str(), 1);
        } else {
            unsetenv("HOME");
        }
        std::error_code ec;
        fs::remove_all(tmp, ec);
    }
};

} // namespace

TEST(TerminalSetupCommand, PreviewAndApplyWrapDisplayPathsInOsc8Links) {
    TerminalSetupHomeGuard home;
    EnvironmentGuard term_program("TERM_PROGRAM", "vscode");
    EnvironmentUnsetGuard wt_session("WT_SESSION");
    EnvironmentUnsetGuard vte_version("VTE_VERSION");

    namespace ts = loom::commands::terminal_setup;
    const std::string rc = (home.tmp / ".zshrc").string();

    // 1) Preview mode: header path is hyperlinked but plain text still present.
    auto r1 = ts::run("--shell=zsh");
    ASSERT_TRUE(r1.ok);
    EXPECT_NE(r1.message.find("RC file  : \x1b]8;;file://" + rc),
              std::string::npos);
    EXPECT_NE(r1.message.find(rc), std::string::npos);

    // Pre-create the rc file so apply takes the backup-existing-file path.
    {
        std::ofstream pre(rc, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(pre.is_open());
        pre << "# preexisting user content\n";
    }

    // 2) Apply mode: rc path, backup path linked; source instruction stays plain.
    auto r2 = ts::run("--apply --shell=zsh");
    ASSERT_TRUE(r2.ok);
    // OSC8 opener with ST = ESC-backslash.
    EXPECT_NE(r2.message.find("\x1b]8;;file://" + rc + "\x1b\\"),
              std::string::npos);
    // Plain path human-readable between opener and closer.
    EXPECT_NE(r2.message.find(rc), std::string::npos);
    // Closing OSC8 marker.
    EXPECT_NE(r2.message.find("\x1b]8;;\x1b\\"), std::string::npos);
    EXPECT_NE(r2.message.find("Successfully applied terminal-setup snippet"),
              std::string::npos);
    EXPECT_NE(r2.message.find("Backup created at:"), std::string::npos);
    // Backup path is also linked (timestamp suffix means prefix match).
    EXPECT_NE(r2.message.find("\x1b]8;;file://" +
                              (home.tmp / ".zshrc.bak-").string()),
              std::string::npos);
    // The executable `source <rc>` instruction must stay plain: no ESC bytes
    // between "source " and the rc path.
    EXPECT_NE(r2.message.find("  source " + rc + "\n"), std::string::npos);

    // The snippet must actually have been written to disk.
    std::ifstream ifs(rc, std::ios::binary);
    ASSERT_TRUE(ifs.is_open());
    std::string on_disk((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    EXPECT_NE(on_disk.find("# >>> loom terminal-setup"), std::string::npos);
}

TEST(TerminalSetupCommand, HyperlinkGateMatchesTsTerminalMatrix) {
    // TS REF: src/ink/supports-hyperlinks.ts — the ADDITIONAL whitelist via
    // TERM_PROGRAM and LC_TERMINAL, plus TERM containing "kitty".
    namespace cu = loom::utils;
    auto with_env = [](std::initializer_list<std::pair<const char*, const char*>> set,
                       std::initializer_list<const char*> unset) {
        std::vector<std::unique_ptr<EnvironmentGuard>> guards;
        std::vector<std::unique_ptr<EnvironmentUnsetGuard>> unsets;
        for (auto [k, v] : set)
            guards.push_back(std::make_unique<EnvironmentGuard>(k, v));
        for (auto k : unset)
            unsets.push_back(std::make_unique<EnvironmentUnsetGuard>(k));
        return cu::supports_hyperlinks();
    };

    // TERM_PROGRAM whitelist (TS ADDITIONAL_HYPERLINK_TERMINALS + base set).
    for (const char* prog :
         {"ghostty", "Hyper", "kitty", "alacritty", "iTerm.app", "iTerm2",
          "WezTerm", "vscode"}) {
        EXPECT_TRUE(with_env({{"TERM_PROGRAM", prog}},
                             {"WT_SESSION", "VTE_VERSION", "TERM", "LC_TERMINAL"}))
            << "TERM_PROGRAM=" << prog;
    }
    // LC_TERMINAL (preserved inside tmux where TERM_PROGRAM becomes tmux).
    EXPECT_TRUE(with_env({{"LC_TERMINAL", "iTerm2"},
                          {"TERM_PROGRAM", "tmux"}},
                         {"WT_SESSION", "VTE_VERSION", "TERM"}));
    // TERM contains kitty.
    EXPECT_TRUE(with_env({{"TERM", "xterm-kitty"}},
                         {"TERM_PROGRAM", "WT_SESSION", "VTE_VERSION",
                          "LC_TERMINAL"}));
    // Unknown terminal stays off.
    EXPECT_FALSE(with_env({},
                          {"TERM_PROGRAM", "WT_SESSION", "VTE_VERSION",
                           "TERM", "LC_TERMINAL"}));
    EXPECT_FALSE(with_env({{"TERM_PROGRAM", "dumb"}},
                          {"WT_SESSION", "VTE_VERSION", "TERM", "LC_TERMINAL"}));
}

TEST(TerminalSetupCommand, UnsupportedTerminalEmitsBarePathsWithoutEscapes) {
    TerminalSetupHomeGuard home;
    EnvironmentUnsetGuard term_program("TERM_PROGRAM");
    EnvironmentUnsetGuard wt_session("WT_SESSION");
    EnvironmentUnsetGuard vte_version("VTE_VERSION");

    // Guard sanity: with every recognized variable cleared the gate is false.
    EXPECT_FALSE(loom::utils::supports_hyperlinks());

    namespace ts = loom::commands::terminal_setup;
    auto r = ts::run("--apply --shell=bash");
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.message.find("\x1b]8;;"), std::string::npos);
    EXPECT_EQ(r.message.find("\x1b"), std::string::npos);
    const std::string rc = (home.tmp / ".bashrc").string();
    EXPECT_NE(r.message.find(rc), std::string::npos);
    // Header line is the bare path immediately followed by newline.
    EXPECT_NE(r.message.find("RC file  : " + rc + "\n"), std::string::npos);
}

TEST(TerminalSetupCommand, PathToFileUrlEncodingAndHyperlinkGate) {
    namespace cu = loom::utils;
    // TS REF: Node url.pathToFileURL — '/' and the Node safe set pass through;
    // every other byte is uppercased percent-encoded (UTF-8 bytes for non-ASCII).
    // Safe set verified empirically against Node v22: '[' ']' ARE encoded
    // (%5B/%5D) while '&' stays raw.
    EXPECT_EQ(cu::path_to_file_url("/home/u/.zshrc"),
              "file:///home/u/.zshrc");
    EXPECT_EQ(cu::path_to_file_url("/home/u/a b/keymap.json"),
              "file:///home/u/a%20b/keymap.json");
    EXPECT_EQ(cu::path_to_file_url("/tmp/a#b?c%&[1].fish"),
              "file:///tmp/a%23b%3Fc%25&%5B1%5D.fish");
    EXPECT_EQ(cu::path_to_file_url("/tmp/caf\xC3\xA9.rc"),
              "file:///tmp/caf%C3%A9.rc");
    // Tilde is encoded by Node (not in the unreserved set).
    EXPECT_EQ(cu::path_to_file_url("/tmp/a~b"),
              "file:///tmp/a%7Eb");

    const std::string url = "file:///home/u/.zshrc";
    const std::string text = "/home/u/.zshrc";
    {
        EnvironmentGuard term_program("TERM_PROGRAM", "vscode");
        EnvironmentUnsetGuard wt_session("WT_SESSION");
        EnvironmentUnsetGuard vte_version("VTE_VERSION");
        const std::string linked = cu::make_hyperlink(url, text);
        EXPECT_EQ(linked.rfind("\x1b]8;;" + url + "\x1b\\", 0), 0u);
        EXPECT_NE(linked.find(text), std::string::npos);
        // Closing OSC8 marker is 7 bytes: \x1b]8;; (5) + \x1b\ (2).
        EXPECT_EQ(linked.compare(linked.size() - 7, 7, "\x1b]8;;\x1b\\"), 0);
    }
    {
        EnvironmentUnsetGuard term_program("TERM_PROGRAM");
        EnvironmentUnsetGuard wt_session("WT_SESSION");
        EnvironmentUnsetGuard vte_version("VTE_VERSION");
        EXPECT_EQ(cu::make_hyperlink(url, text), text);
    }
}

// ── ResumeCommand ──────────────────────────────────────────────────────────
// Regression guard: /resume must list sessions from disk.  The command's
// recent_sessions_ was only ever populated via set_recent_sessions(), which
// has zero callers in the app — so /resume always reported "No recent
// sessions found."  execute() now loads from ~/.loom/sessions directly.
TEST(ResumeCommand, ListsSessionsFromDisk) {
    // Override HOME so load_sessions() resolves ~/.loom/sessions to a temp dir.
    const char* old_home = std::getenv("HOME");
    const auto temp_home = fs::temp_directory_path() /
        ("loom_resume_cmd_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_home / ".loom" / "sessions");
    setenv("HOME", temp_home.string().c_str(), 1);

    // Create a session with metadata and an actual messages file.
    const auto session_dir = temp_home / ".loom" / "sessions" / "abc12345-def";
    fs::create_directories(session_dir);
    {
        std::ofstream ofs(session_dir / "metadata.json");
        ofs << R"({"id":"abc12345-def","title":"My Session","model":"loom-test","message_count":42})";
        // messages.jsonl: the engine appends one JSON object per line.
        // count_messages_on_disk reads this file, not metadata.json.
        std::ofstream(session_dir / "messages.jsonl")
            << "{\"role\":\"user\",\"content\":\"hello\"}\n"
            << "{\"role\":\"assistant\",\"content\":\"hi there\"}\n";
    }

    loom::commands::ResumeCommand cmd;

    // /resume (no args) opens the interactive picker via metadata.
    auto listed = cmd.execute(ctx());
    ASSERT_TRUE(listed.has_value());
    EXPECT_EQ(listed->metadata, "UI:resume");

    // /resume last returns metadata with the session ID for direct resume.
    auto resumed = cmd.execute(ctx({"last"}));
    ASSERT_TRUE(resumed.has_value());
    EXPECT_NE(resumed->message.find("Resuming session"), std::string::npos)
        << resumed->message;
    EXPECT_NE(resumed->message.find("My Session"), std::string::npos)
        << resumed->message;
    ASSERT_TRUE(resumed->metadata.has_value());
    EXPECT_EQ(*resumed->metadata, "UI:resume:abc12345-def");

    // Restore HOME and clean up.
    if (old_home) setenv("HOME", old_home, 1);
    else unsetenv("HOME");
    fs::remove_all(temp_home);
}

TEST(ResumeCommand, FiltersEmptySessionsAndShowsUsefulInfo) {
    // Override HOME so load_sessions() resolves ~/.loom/sessions to a temp dir.
    const char* old_home = std::getenv("HOME");
    const auto temp_home = fs::temp_directory_path() /
        ("loom_resume_filter_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_home / ".loom" / "sessions");
    setenv("HOME", temp_home.string().c_str(), 1);

    // Session 1: UUID format, has messages, title "Session" (generic).
    // Should show first user message as the title.
    {
        const auto dir = temp_home / ".loom" / "sessions" / "82702bdd-679b-44d9";
        fs::create_directories(dir);
        std::ofstream(dir / "metadata.json")
            << R"({"id":"82702bdd-679b-44d9","title":"Session","model":"test","message_count":13})";
        // messages.jsonl: one JSON object per line (engine format).
        std::ofstream(dir / "messages.jsonl")
            << "{\"role\":\"user\",\"content\":\"你好世界\"}\n"
            << "{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"hi\"}]}\n";
    }

    // Session 2: session_<ts>_<hex> format, has messages, no title.
    // Should show the hex suffix (not "session_") and first user message.
    {
        const auto dir = temp_home / ".loom" / "sessions" / "session_1791013130731_410e223d";
        fs::create_directories(dir);
        std::ofstream(dir / "metadata.json")
            << R"({"id":"session_1791013130731_410e223d","model":"test","message_count":5})";
        std::ofstream(dir / "messages.jsonl")
            << "{\"role\":\"user\",\"content\":\"fix the bug\"}\n";
    }

    // Session 3: empty session (0 messages) — should be filtered out.
    {
        const auto dir = temp_home / ".loom" / "sessions" / "session_1791023518606_e3b7fcd0";
        fs::create_directories(dir);
        std::ofstream(dir / "metadata.json")
            << R"({"id":"session_1791023518606_e3b7fcd0","model":"test","message_count":0})";
    }

    loom::commands::ResumeCommand cmd;
    // /resume list still returns the text list (used by the text fallback).
    auto listed = cmd.execute(ctx({"list"}));
    ASSERT_TRUE(listed.has_value());
    const auto& msg = listed->message;

    // The empty session should not appear.
    EXPECT_EQ(msg.find("e3b7fcd0"), std::string::npos) << msg;

    // UUID session: first 8 chars of ID + first user message as title.
    EXPECT_NE(msg.find("82702bdd"), std::string::npos) << msg;
    EXPECT_NE(msg.find("你好世界"), std::string::npos) << msg;

    // session_ format: hex suffix (not "session_") + first user message.
    EXPECT_NE(msg.find("410e223d"), std::string::npos) << msg;
    EXPECT_NE(msg.find("fix the bug"), std::string::npos) << msg;
    // The useless "session_" prefix should NOT appear as an ID.
    EXPECT_EQ(msg.find("session_ —"), std::string::npos) << msg;

    // Restore HOME and clean up.
    if (old_home) setenv("HOME", old_home, 1);
    else unsetenv("HOME");
    fs::remove_all(temp_home);
}

// Regression guard: sessions whose messages.jsonl stores user content as an
// array of content blocks ({"content":[{"type":"text","text":"…"}]}) rather
// than a plain string must still extract the first user message as the title.
TEST(ResumeCommand, TitleExtractionHandlesArrayContent) {
    const char* old_home = std::getenv("HOME");
    const auto temp_home = fs::temp_directory_path() /
        ("loom_resume_array_test_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(temp_home / ".loom" / "sessions");
    setenv("HOME", temp_home.string().c_str(), 1);

    {
        const auto dir = temp_home / ".loom" / "sessions" / "arr12345-test";
        fs::create_directories(dir);
        std::ofstream(dir / "metadata.json")
            << R"({"id":"arr12345-test","title":"Session","model":"test","message_count":3})";
        // Array-format content (multi-block or non-TextBlock messages).
        std::ofstream(dir / "messages.jsonl")
            << "{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"Fix the array parsing bug\"}]}\n"
            << "{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"Done\"}]}\n";
    }

    loom::commands::ResumeCommand cmd;
    auto listed = cmd.execute(ctx({"list"}));
    ASSERT_TRUE(listed.has_value());
    EXPECT_NE(listed->message.find("Fix the array parsing bug"), std::string::npos)
        << listed->message;

    if (old_home) setenv("HOME", old_home, 1);
    else unsetenv("HOME");
    fs::remove_all(temp_home);
}
