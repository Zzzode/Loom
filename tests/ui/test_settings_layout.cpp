/// @file test_settings_layout.cpp
/// @brief Public settings component regressions at real allocated narrow sizes.
#include <cstdlib>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.config.config;
import loom.ui.dialogs.settings_dialog;
import loom.ui.foundation.theme_provider;

namespace {
using namespace ftxui;
namespace settings = loom::ui::dialogs::settings_dialog;
namespace theme = loom::ui::design::theme;
constexpr std::array kThemes{
    theme::ThemeVariant::Dark, theme::ThemeVariant::Light,
    theme::ThemeVariant::DarkDaltonized, theme::ThemeVariant::LightDaltonized,
    theme::ThemeVariant::Monochrome, theme::ThemeVariant::LightAnsi,
    theme::ThemeVariant::DarkAnsi};
struct RestoreTheme {
    theme::Theme saved = theme::current_theme();
    ~RestoreTheme() { theme::set_theme(saved); }
};
// A project save also discovers a Git root from the process working directory.
// Isolate both the explicit config path and cwd; a stray /tmp/.git must not
// redirect ConfigManager's ignore maintenance into an unrelated work tree.
struct TempSettings {
    std::filesystem::path previous = std::filesystem::current_path();
    std::filesystem::path directory;
    std::filesystem::path file;

    static bool AncestryClean(const std::filesystem::path& base) {
        for (auto current = base; ; current = current.parent_path()) {
            std::error_code error;
            const bool marker = std::filesystem::exists(current / ".git", error);
            if (error || marker) return false;
            if (current == current.parent_path()) return true;
        }
    }
    static std::filesystem::path CleanBase() {
        std::vector<std::filesystem::path> candidates;
        for (const char* variable : {"XDG_RUNTIME_DIR", "TMPDIR"})
            if (const char* value = std::getenv(variable); value && *value)
                candidates.emplace_back(value);
        candidates.push_back(std::filesystem::temp_directory_path());
        candidates.emplace_back("/dev/shm");
        for (const auto& candidate : candidates) {
            std::error_code error;
            const auto canonical = std::filesystem::canonical(candidate, error);
            if (!error && std::filesystem::is_directory(canonical, error) && !error &&
                AncestryClean(canonical)) return canonical;
        }
        throw std::runtime_error("Settings layout tests require a Git-ancestry-clean temporary base");
    }
    TempSettings() {
        static std::atomic<unsigned> sequence{0};
        const auto base = CleanBase();
        for (int attempt = 0; attempt < 64; ++attempt) {
            directory = base /
                std::format("loom-settings-layout-{}-{}-{}",
                    std::chrono::steady_clock::now().time_since_epoch().count(),
                    std::random_device{}(), sequence.fetch_add(1));
            if (!std::filesystem::create_directory(directory)) continue;
            file = directory / "settings.json";
            std::error_code error;
            std::filesystem::current_path(directory, error);
            if (!error) return;
            std::error_code cleanup;
            std::filesystem::remove_all(directory, cleanup);
            throw std::filesystem::filesystem_error("Redirect settings test cwd", directory, error);
        }
        throw std::runtime_error("Could not allocate a unique settings layout directory");
    }
    TempSettings(const TempSettings&) = delete;
    TempSettings& operator=(const TempSettings&) = delete;
    ~TempSettings() {
        std::error_code error;
        std::filesystem::current_path(previous, error);
        if (error) {
            ADD_FAILURE() << "Could not restore settings test cwd: " << error.message();
            return;
        }
        std::filesystem::remove_all(directory, error);
        if (error) ADD_FAILURE() << "Could not remove settings test directory: " << error.message();
    }
};

Screen Paint(const Component& component, int columns = 40, int rows = 18) {
    auto screen = Screen::Create(Dimension::Fixed(columns), Dimension::Fixed(rows));
    // Two layout passes model FTXUI's allocated box negotiation; neither
    // pass modifies terminal-global dimensions or a product test seam.
    Render(screen, component->Render());
    screen.Clear();
    Render(screen, component->Render());
    return screen;
}
std::optional<std::pair<int, int>> Find(const Screen& screen, std::string_view word) {
    for (int y = 0; y < screen.dimy(); ++y)
        for (int x = 0; x + static_cast<int>(word.size()) <= screen.dimx(); ++x) {
            bool match = true;
            for (std::size_t i = 0; i < word.size(); ++i)
                if (screen.PixelAt(x + static_cast<int>(i), y).character != std::string(1, word[i])) {
                    match = false;
                    break;
                }
            if (match) return std::pair{x, y};
        }
    return std::nullopt;
}
void FooterVisible(const Screen& screen) {
    for (auto shortcut : {"Ctrl+S", "Ctrl+R", "Esc", "PgUp/PgDn", "scroll"}) {
        const auto where = Find(screen, shortcut);
        ASSERT_TRUE(where) << shortcut << '\n' << screen.ToString();
        EXPECT_GE(where->second, screen.dimy() / 2);
    }
    for (auto action : {"Save", "Reset", "Close"})
        EXPECT_TRUE(Find(screen, action)) << action << '\n' << screen.ToString();
}
// Remove row padding and box-drawing cells so a token split over multiple
// physical rows can be found without imposing a particular line break.
std::string Ink(const Screen& screen) {
    std::string result;
    for (int y = 0; y < screen.dimy(); ++y)
        for (int x = 0; x < screen.dimx(); ++x) {
            const auto& glyph = screen.PixelAt(x, y).character;
            if (glyph == " " || glyph == "\t" || glyph == "\n" || glyph.empty()) continue;
            if (glyph.size() == 3 && static_cast<unsigned char>(glyph[0]) == 0xe2 &&
                static_cast<unsigned char>(glyph[1]) >= 0x94 &&
                static_cast<unsigned char>(glyph[1]) <= 0x97) continue;
            result += glyph;
        }
    return result;
}
std::vector<std::string> UniqueChunks(std::string_view prefix, int count = 20,
                                      bool utf8 = false) {
    std::vector<std::string> chunks;
    for (int i = 0; i < count; ++i)
        chunks.push_back(std::format("{}{:02}{}", prefix, i, utf8 ? "诊断文本" : "aBcDeFgH"));
    return chunks;
}
std::string Join(const std::vector<std::string>& chunks) {
    std::string result;
    for (const auto& chunk : chunks) result += chunk;
    return result;
}
void AllChunksVisible(std::string_view pages, const std::vector<std::string>& chunks) {
    for (const auto& chunk : chunks)
        EXPECT_NE(pages.find(chunk), std::string::npos) << "Missing or truncated chunk: " << chunk;
}
std::string ScanPages(const Component& component, int count = 32) {
    std::string result;
    for (int page = 0; page < count; ++page) {
        const auto screen = Paint(component);
        FooterVisible(screen);
        result += Ink(screen);
        component->OnEvent(Event::PageDown);
    }
    return result;
}
Event Control(char key) { return Event::Special(std::string(1, key - 'a' + 1)); }
void FocusLastGeneral(const Component& component) {
    for (int row = 0; row < 7; ++row) component->OnEvent(Event::Tab);
}
void Click(const Component& component, std::string_view label) {
    const auto screen = Paint(component);
    const auto where = Find(screen, label);
    ASSERT_TRUE(where) << label << '\n' << screen.ToString();
    const auto [x, y] = *where;
    Mouse mouse;
    mouse.button = Mouse::Left;
    mouse.motion = Mouse::Pressed;
    mouse.x = x;
    mouse.y = y;
    component->OnEvent(Event::Mouse("", mouse));
    mouse.motion = Mouse::Released;
    component->OnEvent(Event::Mouse("", mouse));
}
std::string Read(const std::filesystem::path& path) {
    std::ifstream stream(path);
    return {std::istreambuf_iterator<char>(stream), {}};
}
}

TEST(SettingsLayout, EverySectionKeepsAllActionsVisibleAtFortyColumnsInEveryTheme) {
    RestoreTheme restore;
    TempSettings temp;
    loom::core::ConfigManager config(temp.file);
    constexpr std::array<std::pair<std::string_view, std::string_view>, 13> body_tokens{{
        {"Theme", "Minimaloutputformattingforsmallerterminals"},
        {"Defaultmodel", "Overridemodelforsub-agent/swarmteammates"},
        {"BaseURL", "Numberofretryattemptsperfailedrequest"},
        {"Defaultmode", "Whitelistedshellcommands"},
        {"Agent/Sub-agent", "Enabletheskillsystemforextensibleagentcapabilities"},
        {"Serverlifecycle", "MCPruntime"},
        {"EnableLSP", "Language-specificLSPserverconfigurationsmanagedexternally"},
        {"Enablebridge", "CORSoriginallowlistforbridgeconnections"},
        {"ManagedinsettingsJSON", "Hookexecutionisreportedthroughthelifecyclehookservice"},
        {"Telemetry", "APIkeysandsensitivetokensarenevershownaseditablesamplevalues"},
        {"C++23modules", "ProjectversionfromCMakeLists.txt"},
        {"nodiagnosticsyet", "runningchecks…"},
        {"TotalCost", "[r]efresh"},
    }};
    for (auto variant : kThemes) {
        theme::set_theme(variant);
        for (int tab = 0; tab < static_cast<int>(settings::SettingsTabId::_COUNT); ++tab) {
            settings::SettingsDialogOptions options;
            options.initial_tab = static_cast<settings::SettingsTabId>(tab);
            auto component = settings::MakeSettingsDialog(config, options);
            for (int rows : {18, 28}) {
                SCOPED_TRACE(std::format("theme={},tab={},rows={}", theme::variant_name(variant), tab, rows));
                component->OnEvent(Event::Home);
                const auto first = Paint(component, 40, rows);
                FooterVisible(first);
                EXPECT_NE(Ink(first).find(body_tokens[tab].first), std::string::npos)
                    << "Missing first body content: " << body_tokens[tab].first
                    << '\n' << first.ToString();
                component->OnEvent(Event::End);
                const auto last = Paint(component, 40, rows);
                FooterVisible(last);
                EXPECT_NE(Ink(last).find(body_tokens[tab].second), std::string::npos)
                    << "Missing last body content: " << body_tokens[tab].second
                    << '\n' << last.ToString();
            }
        }
    }
}

TEST(SettingsLayout, FocusRevealsLastGeneralFieldWithoutLosingActions) {
    TempSettings temp;
    loom::core::ConfigManager config(temp.file);
    auto component = settings::MakeSettingsDialog(config);
    (void)Paint(component);
    FocusLastGeneral(component);
    const auto screen = Paint(component);
    FooterVisible(screen);
    EXPECT_TRUE(Find(screen, "Compact mode")) << screen.ToString();
    EXPECT_TRUE(component->OnEvent(Event::Return));
    EXPECT_TRUE(component->OnEvent(Control('s')));
    EXPECT_TRUE(config.settings().display.compact_mode);
}

TEST(SettingsLayout, HeldComponentPreservesSelectionAndDraftAcrossResize) {
    TempSettings temp;
    loom::core::ConfigManager config(temp.file);
    config.settings_mut().display.compact_mode = false;
    auto component = settings::MakeSettingsDialog(config);
    (void)Paint(component, 80, 28);
    FocusLastGeneral(component);
    component->OnEvent(Event::Return);
    for (int columns : {40, 80, 40}) {
        const auto screen = Paint(component, columns, 28);
        FooterVisible(screen);
        EXPECT_TRUE(Find(screen, "Compact mode")) << screen.ToString();
    }
    component->OnEvent(Control('s'));
    EXPECT_TRUE(config.settings().display.compact_mode);
}

TEST(SettingsLayout, LongUnbrokenApiValueRemainsRecoverableAndUnmodified) {
    TempSettings temp;
    loom::core::ConfigManager config(temp.file);
    const auto chunks = UniqueChunks("API");
    const std::string endpoint = "https://example.invalid/" + Join(chunks) + "APIENDMARKER";
    config.settings_mut().network.base_url = endpoint;
    settings::SettingsDialogOptions options;
    options.initial_tab = settings::SettingsTabId::API;
    auto component = settings::MakeSettingsDialog(config, options);
    const auto pages = ScanPages(component);
    AllChunksVisible(pages, chunks);
    EXPECT_NE(pages.find("APIENDMARKER"), std::string::npos);
    component->OnEvent(Control('s'));
    EXPECT_EQ(config.settings().network.base_url, endpoint);
}

TEST(SettingsLayout, ReadOnlyStatusAllowsPagingToUtf8TailAndReturningHome) {
    TempSettings temp;
    loom::core::ConfigManager config(temp.file);
    settings::SettingsDialogOptions options;
    options.initial_tab = settings::SettingsTabId::Status;
    const auto chunks = UniqueChunks("S", 24, true);
    const std::string message = Join(chunks) + "STATUSTAILMARKER";
    options.status_rows.push_back({"Subsystem", settings::HealthState::Down,
        std::chrono::system_clock::now(), message});
    auto component = settings::MakeSettingsDialog(config, options);
    const auto pages = ScanPages(component);
    AllChunksVisible(pages, chunks);
    EXPECT_NE(pages.find("诊断"), std::string::npos);
    EXPECT_NE(pages.find("STATUSTAILMARKER"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(Event::Home));
    EXPECT_TRUE(Find(Paint(component), "Status"));
    component->OnEvent(Event::End);
    EXPECT_NE(Ink(Paint(component)).find("STATUSTAILMARKER"), std::string::npos);
}

TEST(SettingsLayout, UsageCardsAndLongModelRemainReachableByPaging) {
    TempSettings temp;
    loom::core::ConfigManager config(temp.file);
    settings::SettingsDialogOptions options;
    options.initial_tab = settings::SettingsTabId::Usage;
    options.refresh_usage = [](settings::UsageSnapshot& snapshot) {
        snapshot.total_requests = 314159;
        snapshot.total_input_tokens = 271828;
        snapshot.total_output_tokens = 161803;
        snapshot.per_model.push_back({std::string(100, 'm') + "MODELTAILMARKER", 42, 123, 456, 1.25});
    };
    auto component = settings::MakeSettingsDialog(config, options);
    const auto pages = ScanPages(component);
    for (auto value : {"314159", "271828", "161803", "MODELTAILMARKER", "$1.2500"})
        EXPECT_NE(pages.find(value), std::string::npos) << value << "\nCollected page ink:\n" << pages;
}

TEST(SettingsLayout, McpEndpointWrapsAndSelectedServerRemovalTargetsExactEntry) {
    TempSettings temp;
    loom::core::ConfigManager config(temp.file);
    config.settings_mut().mcp_servers.resize(2);
    auto& servers = config.settings_mut().mcp_servers;
    servers[0].name = "first";
    servers[0].transport = "sse";
    const auto chunks = UniqueChunks("MCP");
    servers[0].url = "https://example.invalid/" + Join(chunks) + "MCPENDMARKER";
    servers[1].name = "second";
    servers[1].command = "second-command";
    settings::SettingsDialogOptions options;
    options.initial_tab = settings::SettingsTabId::MCP;
    auto component = settings::MakeSettingsDialog(config, options);
    const auto pages = ScanPages(component);
    AllChunksVisible(pages, chunks);
    EXPECT_NE(pages.find("MCPENDMARKER"), std::string::npos);
    component->OnEvent(Event::Home);
    component->OnEvent(Event::Tab);
    component->OnEvent(Event::Tab);
    const auto selected = Paint(component);
    FooterVisible(selected);
    EXPECT_TRUE(Find(selected, "second")) << selected.ToString();
    component->OnEvent(Event::Character('r'));
    component->OnEvent(Control('s'));
    ASSERT_EQ(config.settings().mcp_servers.size(), 1u);
    EXPECT_EQ(config.settings().mcp_servers.front().name, "first");
}

TEST(SettingsLayout, KeyboardSaveResetAndCloseKeepWritesHermeticAndPreserveUnknownKeys) {
    TempSettings temp;
    { std::ofstream stream(temp.file); stream << R"({"preservedProbe":{"keep":"opaque"},"theme":"dark"})"; }
    loom::core::ConfigManager config(temp.file);
    ASSERT_TRUE(config.load());
    config.settings_mut().display.compact_mode = false;
    int closed = 0;
    std::optional<std::string> close_message;
    settings::SettingsDialogOptions options;
    options.on_close = [&](std::optional<std::string> message, settings::CommandResultDisplay) {
        ++closed; close_message = std::move(message);
    };
    auto component = settings::MakeSettingsDialog(config, options);
    (void)Paint(component);
    FocusLastGeneral(component);
    component->OnEvent(Event::Return);
    component->OnEvent(Control('r'));
    component->OnEvent(Control('s'));
    EXPECT_FALSE(config.settings().display.compact_mode);
    component->OnEvent(Event::Return);
    component->OnEvent(Control('s'));
    EXPECT_TRUE(config.settings().display.compact_mode);
    EXPECT_NE(Read(temp.file).find("preservedProbe"), std::string::npos);
    EXPECT_NE(Read(temp.file).find("opaque"), std::string::npos);
    component->OnEvent(Event::Return);
    const auto saved = Read(temp.file);
    component->OnEvent(Event::Escape);
    EXPECT_EQ(closed, 1);
    EXPECT_TRUE(close_message);
    EXPECT_EQ(Read(temp.file), saved);
}

TEST(SettingsLayout, VisibleFooterMouseActionsUseSameSaveResetAndCloseBehavior) {
    TempSettings temp;
    loom::core::ConfigManager config(temp.file);
    config.settings_mut().display.compact_mode = false;
    int closed = 0;
    settings::SettingsDialogOptions options;
    options.on_close = [&](std::optional<std::string>, settings::CommandResultDisplay) { ++closed; };
    auto component = settings::MakeSettingsDialog(config, options);
    (void)Paint(component);
    FocusLastGeneral(component);
    component->OnEvent(Event::Return);
    Click(component, "Reset");
    Click(component, "Save");
    EXPECT_FALSE(config.settings().display.compact_mode);
    component->OnEvent(Event::Return);
    Click(component, "Save");
    EXPECT_TRUE(config.settings().display.compact_mode);
    Click(component, "Close");
    EXPECT_EQ(closed, 1);
}


TEST(SettingsLayout, LongSaveFailurePreservesBodyActionsAndScrollableDiagnostic) {
    TempSettings temp;
    const auto chunks = UniqueChunks("ERR", 12);
    const auto blocker = temp.directory / (Join(chunks) + "SAVEERRTAILMARKER");
    { std::ofstream stream(blocker); stream << "ordinary file blocks config directory creation"; }
    loom::core::ConfigManager config(blocker / "settings.json");
    auto component = settings::MakeSettingsDialog(config);
    (void)Paint(component);
    component->OnEvent(Event::Return);
    ASSERT_TRUE(component->OnEvent(Control('s')));
    const auto failed = Paint(component);
    FooterVisible(failed);
    // The potentially unbounded path must not consume the pinned footer or
    // the entire document viewport. A focused setting remains usable.
    EXPECT_TRUE(Find(failed, "Theme")) << failed.ToString();
    EXPECT_TRUE(Find(failed, "Details")) << failed.ToString();
    const auto pages = ScanPages(component);
    AllChunksVisible(pages, chunks);
    EXPECT_NE(pages.find("SAVEERRTAILMARKER"), std::string::npos);
    component->OnEvent(Event::End);
    const auto end = Paint(component);
    FooterVisible(end);
    EXPECT_NE(Ink(end).find("SAVEERRTAILMARKER"), std::string::npos);
    std::error_code missing_file;
    EXPECT_FALSE(std::filesystem::exists(blocker / "settings.json", missing_file));
    EXPECT_EQ(Read(blocker), "ordinary file blocks config directory creation");
}


TEST(SettingsLayout, PermissionListsRemainReachableByFieldNavigation) {
    TempSettings temp;
    loom::core::ConfigManager config(temp.file);
    config.settings_mut().permissions.allowed_commands = {"pwd", "whoami", "date"};
    settings::SettingsDialogOptions options;
    options.initial_tab = settings::SettingsTabId::Permissions;
    auto component = settings::MakeSettingsDialog(config, options);
    (void)Paint(component);
    for (int field = 0; field < 6; ++field) component->OnEvent(Event::Tab);
    const auto screen = Paint(component);
    FooterVisible(screen);
    EXPECT_TRUE(Find(screen, "Allowed commands")) << screen.ToString();
    const auto value = Find(screen, "[3 entries]");
    ASSERT_TRUE(value) << screen.ToString();
    // The final list is reached through ordinary field navigation, and its
    // control is the selected field rather than merely nearby scroll content.
    const auto [x, y] = *value;
    for (int column = 0; column < 11; ++column)
        EXPECT_TRUE(screen.PixelAt(x + column, y).inverted);
}


TEST(SettingsLayout, McpLifecycleHelpAndAddDelegationRemainInSection) {
    TempSettings temp;
    loom::core::ConfigManager config(temp.file);
    settings::SettingsDialogOptions options;
    options.initial_tab = settings::SettingsTabId::MCP;
    auto component = settings::MakeSettingsDialog(config, options);
    const auto initial = Paint(component);
    EXPECT_TRUE(Find(initial, "Server lifecycle")) << initial.ToString();
    EXPECT_TRUE(Find(initial, "managed by runtime")) << initial.ToString();
    ASSERT_TRUE(component->OnEvent(Event::Return));
    const auto help = Paint(component);
    FooterVisible(help);
    EXPECT_EQ(Ink(help).find("*unsavedchanges"), std::string::npos) << help.ToString();
    // Runtime ownership is informational. Enter opens lifecycle help and
    // must not turn an unsupported setting into an unsaved configuration edit.
    const auto pages = ScanPages(component);
    EXPECT_NE(pages.find("Use/mcpforserverlifecycleandstatus"), std::string::npos);
    ASSERT_TRUE(component->OnEvent(Control('s')));
    const auto saved = Paint(component);
    FooterVisible(saved);
    EXPECT_EQ(Ink(saved).find("*unsavedchanges"), std::string::npos) << saved.ToString();
    EXPECT_TRUE(std::filesystem::is_regular_file(temp.file));
    ASSERT_TRUE(component->OnEvent(Event::Character('a')));
    const auto delegated = Paint(component);
    FooterVisible(delegated);
    EXPECT_TRUE(Find(delegated, "/mcp add")) << delegated.ToString();
    EXPECT_TRUE(Find(delegated, "MCP")) << delegated.ToString();
    EXPECT_FALSE(Find(delegated, "About")) << delegated.ToString();
}
