/// @file test_text_contrast.cpp
/// @brief Remaining code, diff and footer text contrast regressions.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.foundation.theme_provider;
import loom.ui.visual.code_highlight;
import loom.ui.visual.structured_diff;
import loom.ui.visual.markdown;
import loom.ui.visual.file_edit_tool_diff;
import loom.ui.prompt.prompt_input_footer;
import loom.ui.dialogs.plugin_dialog;
import loom.ui.features.plugins.plugin_manage_panel;
import loom.ui.features.plugins.plugin_settings_dialog;

namespace {
namespace th = loom::ui::design::theme;
namespace code = loom::ui::code_highlight;
namespace diff = loom::ui::structured_diff;
namespace footer = loom::ui::prompt::footer;
namespace edit = loom::ui::components::file_edit_tool_diff;
namespace plugins = loom::ui::dialogs::plugin_dialog;
namespace manage = loom::ui::plugins::plugin_manage_panel;
namespace settings = loom::ui::plugins::plugin_settings_dialog;

constexpr auto kRgb = std::array{th::ThemeVariant::Dark, th::ThemeVariant::Light,
    th::ThemeVariant::DarkDaltonized, th::ThemeVariant::LightDaltonized};
constexpr auto kAll = std::array{th::ThemeVariant::Dark, th::ThemeVariant::Light,
    th::ThemeVariant::DarkDaltonized, th::ThemeVariant::LightDaltonized,
    th::ThemeVariant::Monochrome, th::ThemeVariant::LightAnsi, th::ThemeVariant::DarkAnsi};
constexpr auto kNamed = std::array{code::ThemeName::Dark, code::ThemeName::Light,
    code::ThemeName::Monokai, code::ThemeName::Dracula, code::ThemeName::Nord};
constexpr auto kTokens = std::array{code::TokenType::Plain, code::TokenType::Keyword,
    code::TokenType::String, code::TokenType::Number, code::TokenType::Comment,
    code::TokenType::Operator, code::TokenType::Punctuation, code::TokenType::Function,
    code::TokenType::Type, code::TokenType::Variable, code::TokenType::Constant,
    code::TokenType::Preprocessor, code::TokenType::Attribute, code::TokenType::Error};

struct RestoreTheme {
    th::Theme old = th::current_theme();
    ~RestoreTheme() { th::set_theme(old); }
};

std::optional<double> Luminance(ftxui::Color color) {
    const auto sgr = color.Print(false);
    if (!sgr.starts_with("38;2;")) return std::nullopt;
    const char* cursor = sgr.data() + 5;
    const char* end = sgr.data() + sgr.size();
    std::array<double, 3> channels{};
    for (std::size_t i = 0; i < channels.size(); ++i) {
        int channel = 0;
        const auto parsed = std::from_chars(cursor, end, channel);
        if (parsed.ec != std::errc{} || channel < 0 || channel > 255) return std::nullopt;
        cursor = parsed.ptr;
        if (i + 1 < channels.size() && (cursor == end || *cursor++ != ';')) return std::nullopt;
        const double c = channel / 255.0;
        channels[i] = c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    }
    if (cursor != end) return std::nullopt;
    return channels[0] * 0.2126 + channels[1] * 0.7152 + channels[2] * 0.0722;
}

void ExpectRgbContrast(ftxui::Color foreground, ftxui::Color background) {
    const auto fg = Luminance(foreground);
    const auto bg = Luminance(background);
    ASSERT_TRUE(fg.has_value()) << foreground.Print(false);
    ASSERT_TRUE(bg.has_value()) << background.Print(false);
    EXPECT_GE((std::max(*fg, *bg) + 0.05) / (std::min(*fg, *bg) + 0.05), 4.5)
        << foreground.Print(false) << " against " << background.Print(false);
}

ftxui::Screen Paint(ftxui::Element element, int width = 120, int height = 12,
                    ftxui::Color canvas = ftxui::Color::Default) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                      ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, std::move(element) | ftxui::bgcolor(canvas));
    return screen;
}

std::optional<ftxui::Pixel> Word(const ftxui::Screen& screen, std::string_view word) {
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x + static_cast<int>(word.size()) <= screen.dimx(); ++x) {
            bool match = true;
            for (std::size_t i = 0; i < word.size(); ++i) {
                if (screen.PixelAt(x + static_cast<int>(i), y).character != std::string(1, word[i])) {
                    match = false;
                    break;
                }
            }
            if (!match) continue;
            for (std::size_t i = 0; i < word.size(); ++i) {
                const auto& pixel = screen.PixelAt(x + static_cast<int>(i), y);
                EXPECT_FALSE(pixel.dim) << word;
                EXPECT_EQ(pixel.foreground_color, screen.PixelAt(x, y).foreground_color) << word;
                EXPECT_EQ(pixel.background_color, screen.PixelAt(x, y).background_color) << word;
            }
            return screen.PixelAt(x, y);
        }
    }
    ADD_FAILURE() << "Missing visible word: " << word;
    return std::nullopt;
}

void ExpectWord(const ftxui::Screen& screen, std::string_view word,
                ftxui::Color foreground, ftxui::Color background,
                bool numeric = false) {
    const auto pixel = Word(screen, word);
    ASSERT_TRUE(pixel.has_value());
    EXPECT_EQ(pixel->foreground_color, foreground) << word;
    EXPECT_EQ(pixel->background_color, background) << word;
    EXPECT_NE(pixel->foreground_color, pixel->background_color) << word;
    if (numeric) ExpectRgbContrast(pixel->foreground_color, pixel->background_color);
}
}  // namespace

TEST(TextContrast, DefaultSyntaxTokensAndGutterUseReadableActualCanvasPairs) {
    RestoreTheme restore;
    for (const auto variant : kRgb) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const code::SyntaxTheme syntax;
        for (const auto token : kTokens) {
            code::HighlightedLine line;
            line.line_number = 17;
            line.tokens.push_back({"TokenWord", token});
            const auto screen = Paint(code::RenderHighlightedLine(line, syntax, true, false, 2),
                                      80, 1, syntax.bg);
            ExpectWord(screen, "TokenWord", code::token_color(token, syntax), syntax.bg, true);
            ExpectWord(screen, "17", syntax.line_number, syntax.gutter_bg, true);
        }
    }
}

TEST(TextContrast, FiveExplicitSyntaxFactoriesHaveReadableTokensAndGutters) {
    RestoreTheme restore;
    for (const auto global : {th::ThemeVariant::Dark, th::ThemeVariant::Light}) {
        th::set_theme(global);
        for (const auto name : kNamed) {
            const auto syntax = code::get_theme(name);
            SCOPED_TRACE(syntax.name);
            for (const auto token : kTokens) {
                for (int surface = 0; surface < 3; ++surface) {
                    code::HighlightedLine line;
                    line.line_number = 31;
                    line.tokens.push_back({"NamedToken", token});
                    line.is_diff_added = surface == 1;
                    line.is_diff_removed = surface == 2;
                    const auto background = surface == 1 ? syntax.added_bg
                        : surface == 2 ? syntax.removed_bg : syntax.bg;
                    const auto screen = Paint(code::RenderHighlightedLine(line, syntax, true, false, 2),
                                              80, 1, syntax.bg);
                    ExpectWord(screen, "NamedToken", code::token_color(token, syntax), background, true);
                    ExpectWord(screen, "31", syntax.line_number, syntax.gutter_bg, true);
                }
            }
        }
    }
}

TEST(TextContrast, ExplicitNamedMarkdownPaintsItsSurfaceOnOppositeGlobalTheme) {
    RestoreTheme restore;
    for (const auto global : {th::ThemeVariant::Dark, th::ThemeVariant::Light}) {
        th::set_theme(global);
        for (const auto name : kNamed) {
            const auto syntax = code::get_theme(name);
            loom::ui::MarkdownOptions options;
            options.theme_name = syntax.name;
            const auto screen = Paint(loom::ui::render_markdown(
                "```cpp\n// NamedComment\nreturn 42;\n```", options));
            ExpectWord(screen, "NamedComment", syntax.comment, syntax.bg, true);
            ExpectWord(screen, "return", syntax.keyword, syntax.bg, true);
        }
        const auto& pal = *th::current_theme().palette;
        const auto screen = Paint(loom::ui::render_markdown("```cpp\nreturn 42;\n```"));
        ExpectWord(screen, "return", pal.primary, ftxui::Color::Default);
        loom::ui::MarkdownOptions unknown;
        unknown.theme_name = "not-a-theme";
        const auto fallback = code::get_theme(code::ThemeName::Dark);
        const auto fallback_screen = Paint(loom::ui::render_markdown(
            "```cpp\n// NamedComment\nreturn 42;\n```", unknown));
        ExpectWord(fallback_screen, "NamedComment", fallback.comment, fallback.bg, true);
        ExpectWord(fallback_screen, "return", fallback.keyword, fallback.bg, true);
    }
}

TEST(TextContrast, DefaultDiffRowsWordChangesGuttersAndHunkHeadersAreReadable) {
    RestoreTheme restore;
    for (const auto variant : kRgb) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        const auto syntax = diff::default_theme();
        for (const auto type : {diff::StructuredDiffLine::Type::Added,
                 diff::StructuredDiffLine::Type::Removed, diff::StructuredDiffLine::Type::Context}) {
            diff::StructuredDiffLine line;
            line.type = type;
            line.content = "Changed Remaining";
            line.old_line_num = 17;
            line.new_line_num = 18;
            const bool added = type == diff::StructuredDiffLine::Type::Added;
            const bool removed = type == diff::StructuredDiffLine::Type::Removed;
            if (added || removed) line.word_changes.push_back({0, 7, added});
            const auto background = added ? syntax.added_bg : removed ? syntax.removed_bg : pal.background;
            const auto foreground = added ? syntax.added_fg : removed ? syntax.removed_fg : syntax.context_fg;
            const auto word_background = added ? syntax.added_word_bg
                : removed ? syntax.removed_word_bg : pal.background;
            const auto screen = Paint(diff::RenderStructuredDiffLine(line, syntax, true, 2, false),
                                      100, 1, pal.background);
            ExpectWord(screen, "Changed", foreground, word_background, true);
            ExpectWord(screen, "Remaining", foreground, background, true);
            ExpectWord(screen, "17", syntax.line_num_fg, background, true);
        }
        diff::StructuredPatchHunk hunk;
        hunk.header = "HeaderNote";
        ExpectWord(Paint(diff::RenderHunkHeader(hunk, syntax)), "HeaderNote",
                   syntax.hunk_header_fg, pal.info_tint_bg, true);
    }
}

TEST(TextContrast, CodeMetadataAndFoldInstructionsRetainReadableInk) {
    RestoreTheme restore;
    for (const auto variant : kRgb) {
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        code::HighlightResult highlighted;
        highlighted.language_detected = "cpp";
        highlighted.lines.resize(code::kFoldThresholdLines + 1);
        for (auto& line : highlighted.lines) {
            line.line_number = 1;
            line.tokens.push_back({"Body", code::TokenType::Plain});
        }
        code::CodeHighlightOptions options;
        options.visible_lines = 2;
        const auto screen = Paint(code::RenderCodeHighlight(options, highlighted), 120, 12, pal.background);
        ExpectWord(screen, "Showing", pal.warning, pal.warning_tint_bg, true);
        ExpectWord(screen, "Expand", pal.info, pal.warning_tint_bg, true);
        ExpectWord(screen, "scroll", pal.muted, pal.background, true);
        ExpectWord(screen, "2/1001", pal.muted, pal.background, true);
    }
    for (const auto global : {th::ThemeVariant::Dark, th::ThemeVariant::Light}) {
        th::set_theme(global);
        const auto& pal = *th::current_theme().palette;
        for (const auto name : {code::ThemeName::Dark, code::ThemeName::Light}) {
            code::CodeHighlightOptions options;
            options.theme = code::get_theme(name);
            const auto screen = Paint(code::RenderCodeHighlight(options, {}), 120, 8, pal.background);
            ExpectWord(screen, "empty", options.theme->comment, options.theme->bg, true);
        }
    }
}

TEST(TextContrast, FooterExitPastingVimAndShortcutInstructionsDoNotDim) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        footer::LeftSideOptions options;
        options.exit_message_show = true;
        options.exit_message_key = "Ctrl-D";
        ExpectWord(Paint(footer::RenderLeftSide(options)), "again", pal.muted, ftxui::Color::Default);
        options.exit_message_show = false;
        options.is_pasting = true;
        ExpectWord(Paint(footer::RenderLeftSide(options)), "Pasting", pal.muted, ftxui::Color::Default);
        options.is_pasting = false;
        options.vim_mode = footer::VimMode::Insert;
        ExpectWord(Paint(footer::RenderLeftSide(options)), "INSERT", pal.muted, ftxui::Color::Default);
        options.vim_mode.reset();
        options.mode_indicator.show_hint = true;
        options.mode_indicator.is_loading = true;
        ExpectWord(Paint(footer::RenderLeftSide(options)), "interrupt", pal.muted, ftxui::Color::Default);
        options.mode_indicator.is_loading = false;
        ExpectWord(Paint(footer::RenderLeftSide(options)), "shortcuts", pal.muted, ftxui::Color::Default);
        options.mode_indicator.permission_mode = footer::PermissionMode::Plan;
        ExpectWord(Paint(footer::RenderLeftSide(options)), "cycle", pal.muted, ftxui::Color::Default);
    }
}

TEST(TextContrast, QueuedNotificationStatusAndIdeMetadataRemainReadable) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        const bool numeric = std::ranges::find(kRgb, variant) != kRgb.end();
        footer::NotificationItem item;
        item.pill.variant = footer::PillVariant::ApiKey;
        item.pill.api_key_status = footer::ApiKeyStatus::Valid;
        footer::NotificationData data;
        data.queue.current = item;
        ExpectWord(Paint(footer::RenderNotifications(data), 120, 1, pal.background),
                   "API", pal.success, pal.background, numeric);
        data.queue.current->pill.variant = footer::PillVariant::AutoUpdater;
        data.queue.current->pill.auto_updater.status = footer::AutoUpdaterStatus::Downloading;
        ExpectWord(Paint(footer::RenderNotifications(data), 120, 1, pal.background),
                   "Auto-updating", pal.muted, pal.background, numeric);
        data.queue.current->pill.variant = footer::PillVariant::NewRelease;
        data.queue.current->pill.new_release.version = "1.2.3";
        ExpectWord(Paint(footer::RenderNotifications(data), 120, 1, pal.background),
                   "New", pal.info, pal.background, numeric);
        footer::IdeSelectionInfo ide;
        ide.connected = true;
        ide.file_path = "/tmp/source.cpp";
        ExpectWord(Paint(footer::RenderIdeStatusIndicator(ide), 120, 1, pal.background),
                   "source.cpp", pal.muted, pal.background, numeric);
        ide.selected_lines = 3;
        ExpectWord(Paint(footer::RenderIdeStatusIndicator(ide), 120, 1, pal.background),
                   "selected", pal.muted, pal.background, numeric);
    }
}

TEST(TextContrast, NamedDiffSurfacesStayReadableOnOppositeUiAndKeepWordRanges) {
    RestoreTheme restore;
    for (const auto global : {th::ThemeVariant::Dark, th::ThemeVariant::Light}) {
        th::set_theme(global);
        for (const auto& name : {"dark", "light", "monokai"}) {
            SCOPED_TRACE(name);
            const auto resolved = diff::get_diff_syntax_theme(name);
            ASSERT_TRUE(resolved.has_value());
            const auto& syntax = *resolved;
            ASSERT_TRUE(syntax.context_bg.has_value());
            for (const auto type : {diff::StructuredDiffLine::Type::Added,
                     diff::StructuredDiffLine::Type::Removed, diff::StructuredDiffLine::Type::Context}) {
                diff::StructuredDiffLine line;
                line.type = type;
                line.content = "Changed Remaining";
                line.old_line_num = 17;
                line.new_line_num = 18;
                const bool added = type == diff::StructuredDiffLine::Type::Added;
                const bool removed = type == diff::StructuredDiffLine::Type::Removed;
                if (added || removed) line.word_changes.push_back({0, 7, added});
                const auto foreground = added ? syntax.added_fg : removed ? syntax.removed_fg : syntax.context_fg;
                const auto background = added ? syntax.added_bg : removed ? syntax.removed_bg : *syntax.context_bg;
                const auto word_background = added ? syntax.added_word_bg
                    : removed ? syntax.removed_word_bg : *syntax.context_bg;
                const auto screen = Paint(diff::RenderStructuredDiffLine(line, syntax, true, 2, false), 100, 1);
                ExpectWord(screen, "Changed", foreground, word_background, true);
                ExpectWord(screen, "Remaining", foreground, background, true);
                ExpectWord(screen, "17", syntax.line_num_fg, background, true);
            }
            diff::StructuredPatchHunk hunk;
            hunk.header = "NamedHeader";
            ExpectWord(Paint(diff::RenderHunkHeader(hunk, syntax)), "NamedHeader",
                       syntax.hunk_header_fg, syntax.hunk_header_bg, true);
        }
    }
}

TEST(TextContrast, DefaultDiffContextRetainsCallerSurfaceAndSelectionContract) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        const auto syntax = diff::default_theme();
        EXPECT_FALSE(syntax.context_bg.has_value());
        diff::StructuredDiffLine line;
        line.type = diff::StructuredDiffLine::Type::Context;
        line.content = "Inherited";
        const auto caller_surface = pal.user_message_background;
        const auto screen = Paint(diff::RenderStructuredDiffLine(line, syntax, false, 0, false),
                                  40, 1, caller_surface);
        ExpectWord(screen, "Inherited", syntax.context_fg, caller_surface);
        const auto selected = Paint(diff::RenderStructuredDiffLine(line, syntax, false, 0, true),
                                    40, 1, caller_surface);
        ExpectWord(selected, "Inherited", syntax.context_fg, pal.message_actions_background);
        EXPECT_EQ(screen.dimx(), selected.dimx());
        EXPECT_EQ(screen.dimy(), selected.dimy());
    }
}

TEST(TextContrast, PluginErrorScopeDiagnosisAndGuidanceRemainReadable) {
    RestoreTheme restore;
    plugins::ErrorRow row;
    row.label = "BrokenPlugin";
    row.scope = "project";
    row.message = "Diagnosis";
    row.guidance = "Recovery";
    for (const auto variant : kAll) {
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        const bool numeric = std::ranges::find(kRgb, variant) != kRgb.end();
        for (bool selected : {false, true}) {
            const auto screen = Paint(plugins::RenderErrorRow(row, selected), 120, 4, pal.background);
            ExpectWord(screen, "project", pal.muted,
                       selected ? pal.danger_tint_bg : pal.background, numeric);
            ExpectWord(screen, "Diagnosis", pal.muted, pal.background, numeric);
            ExpectWord(screen, "Recovery", pal.warning, pal.background, numeric);
        }
        ExpectWord(Paint(plugins::RenderHelp(), 120, 10, pal.background),
                   "back", pal.muted, pal.background, numeric);
    }
}

TEST(TextContrast, PluginInstalledMetadataAndSelectedScopeKeepReadableInk) {
    RestoreTheme restore;
    manage::InstalledCellData plugin;
    plugin.name = "Example";
    plugin.marketplace = "Marketplace";
    plugin.version = "1.2.3";
    plugin.scope = "project";
    plugin.enabled = true;
    plugin.tool_count = 2;
    plugin.command_count = 3;
    for (const auto variant : kAll) {
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        const bool numeric = std::ranges::find(kRgb, variant) != kRgb.end();
        for (bool selected : {false, true}) {
            const auto screen = Paint(manage::detail::RenderInstalledRow(plugin, selected, false),
                                      140, 3, pal.background);
            const auto bg = selected ? pal.info_tint_bg : pal.background;
            ExpectWord(screen, "Marketplace", pal.info, bg, numeric);
            ExpectWord(screen, "project", pal.muted, bg, numeric);
            ExpectWord(screen, "tools", pal.muted, bg, numeric);
            ExpectWord(screen, "v1.2.3", pal.muted, bg, numeric);
        }
    }
}

TEST(TextContrast, PluginSettingsHintsAndKeybindingScopeStayReadable) {
    RestoreTheme restore;
    settings::KeybindingEntry binding;
    binding.name = "Format";
    binding.hotkey = "Ctrl-F";
    binding.scope = "plugin-local";
    for (const auto variant : kAll) {
        SCOPED_TRACE(th::variant_name(variant));
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        const bool numeric = std::ranges::find(kRgb, variant) != kRgb.end();
        for (bool selected : {false, true}) {
            ExpectWord(Paint(settings::detail::RenderKeybindingRow(binding, selected), 120, 1, pal.background),
                       "plugin-local", pal.muted, selected ? pal.info_tint_bg : pal.background, numeric);
            ExpectWord(Paint(settings::detail::BoolToggleRow("Toggle", "RequiredHint", true, selected),
                             120, 3, pal.background), "RequiredHint", pal.muted, pal.background, numeric);
        }
        const auto tabs = Paint(settings::detail::RenderSettingsTabBar(settings::SettingsTab::Global),
                                120, 1, pal.background);
        ExpectWord(tabs, "Global", pal.info, pal.message_actions_background, numeric);
        ExpectWord(tabs, "Plugins", pal.muted, pal.background, numeric);
    }
}

TEST(TextContrast, DiffNoticesAndEditErrorsUseReadableInformationInsteadOfDim) {
    RestoreTheme restore;
    for (const auto variant : kAll) {
        th::set_theme(variant);
        const auto& pal = *th::current_theme().palette;
        const bool numeric = std::ranges::find(kRgb, variant) != kRgb.end();
        ExpectWord(Paint(diff::RenderBinaryFile(), 120, 3, pal.background),
                   "Binary", pal.muted, pal.background, numeric);
        ExpectWord(Paint(diff::RenderUntrackedFile("source.cpp"), 120, 4, pal.background),
                   "Untracked", pal.muted, pal.background, numeric);
        ExpectWord(Paint(edit::structured_diff_list({}, diff::default_theme(), "source.cpp"),
                         120, 1, pal.background), "changes", pal.muted, pal.background, numeric);
        edit::FileEditToolDiffProps props;
        props.file_path = "source.cpp";
        ExpectWord(Paint(edit::render_file_edit_tool_diff(props, "", 120), 120, 3, pal.background),
                   "provided", pal.danger, pal.background, numeric);
    }
}
