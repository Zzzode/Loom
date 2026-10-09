/// @file screen_normalize.hpp
/// @brief Screen text normalization for golden snapshot tests.
///
/// Consolidates the ANSI-stripping and snapshot-normalization logic that
/// existed in two copies (test_markdown_render_snapshot.cpp and the
/// CSI-only strip_ansi in test_ui_helpers.h). Both the markdown snapshot
/// suite and the streaming replay suite (RFC 0003) use this header so
/// golden files are produced by one implementation.
///
/// All functions are header-only (inline) and depend only on textual
/// standard-library includes — no module imports — so this header can be
/// included by any test TU without import-ordering constraints.

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace loom::testing {

/// Strip CSI (ESC[...final) and OSC (ESC]...BEL or ESC]...ESC-backslash)
/// escape sequences from rendered screen text.
///
/// This is the OSC-aware version: it handles OSC 8 hyperlink sequences
/// (ESC ] 8 ; ... BEL) that the CSI-only strip_ansi in test_ui_helpers.h
/// leaves behind. Ported verbatim from test_markdown_render_snapshot.cpp.
[[nodiscard]] inline std::string strip_ansi_osc_aware(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\x1b' && i + 1 < text.size()) {
            char next = text[i + 1];
            if (next == '[') {
                // CSI: ESC [ ... final byte (0x40-0x7E)
                i += 2;
                while (i < text.size() &&
                       !(text[i] >= 0x40 && text[i] <= 0x7e)) ++i;
                if (i < text.size()) ++i;  // skip final byte
                continue;
            }
            if (next == ']') {
                // OSC: ESC ] ... BEL or ESC-backslash
                i += 2;
                while (i < text.size()) {
                    if (text[i] == '\x07') { ++i; break; }
                    if (text[i] == '\x1b' && i + 1 < text.size() &&
                        text[i + 1] == '\\') { i += 2; break; }
                    ++i;
                }
                continue;
            }
        }
        out += text[i];
        ++i;
    }
    return out;
}

/// Normalize rendered screen text for golden snapshot comparison.
///
/// Pipeline:
///   1. Strip ANSI (CSI + OSC) escape sequences.
///   2. Replace spinner glyphs (the 10 braille frames from
///      design_figures.cppm kSpinnerFramesBraille) with "<spinner>".
///   2b. Replace the task-view chrome spinner segment ("<glyph> <verb>…<dots>")
///       with placeholders — all three parts are non-deterministic.
///   3. Replace relative timestamps ("just now", "Xs ago", "Xm ago",
///      "Xh ago", "Xd ago") with "<timestamp>".
///   4. Trim trailing whitespace per line.
///   5. Collapse trailing blank lines.
///
/// The spinner and timestamp normalization is necessary because both are
/// wall-clock-driven and non-deterministic in replay (RFC 0003 §7.4).
[[nodiscard]] inline std::string normalize_screen(std::string_view raw) {
    std::string clean = strip_ansi_osc_aware(raw);

    // ── Step 2: spinner glyph normalization ────────────────────────────
    // The 10 braille spinner frames are all 3-byte UTF-8 sequences in the
    // range E2 A0 8B – E2 A0 8F (see design_figures.cppm). Replace each
    // with the placeholder "<spinner>" so golden files are deterministic
    // regardless of which frame the wall-clock ticker selected.
    {
        std::string tmp;
        tmp.reserve(clean.size());
        for (std::size_t i = 0; i < clean.size();) {
            if (i + 3 <= clean.size() &&
                static_cast<unsigned char>(clean[i]) == 0xE2 &&
                static_cast<unsigned char>(clean[i + 1]) == 0xA0) {
                switch (static_cast<unsigned char>(clean[i + 2])) {
                    case 0x8B:  // ⠋ U+280B
                    case 0x99:  // ⠙ U+2819
                    case 0xB9:  // ⠹ U+2839
                    case 0xB8:  // ⠸ U+2838
                    case 0xBC:  // ⠼ U+283C
                    case 0xB4:  // ⠴ U+2834
                    case 0xA6:  // ⠦ U+2826
                    case 0xA7:  // ⠧ U+2827
                    case 0x87:  // ⠇ U+2807
                    case 0x8F:  // ⠏ U+280F
                        tmp += "<spinner>";
                        i += 3;
                        continue;
                    default:
                        break;
                }
            }
            tmp += clean[i];
            ++i;
        }
        clean = std::move(tmp);
    }

    // ── Step 2b: chrome spinner line normalization ────────────────────
    // The task-view chrome spinner (RenderSpinner in
    // repl_screen_welcome.cpp) is non-deterministic in three ways:
    //   - glyph: 8 teardrop-asterisk frames indexed by (wall-clock) frame
    //   - verb: randomly sampled from SPINNER_VERBS via std::random_device
    //     whenever spinner_verb is nullopt — which every text
    //     content_block_delta causes (app_handle_submit.cpp sets it to
    //     nullopt on entering "Responding" mode)
    //   - dots: 3-dot blink cycle, also wall-clock-driven
    // Replace the whole "<glyph> <verb>…<dots>" segment with placeholders.
    // The two leading spaces (spinner left padding) are left intact.
    {
        // Whether s[i..i+3) is one of the 8 teardrop-asterisk glyphs from
        // RenderSpinner's kGlyphs (✻ ❋ ✦ ✧ ✶ ✷ ✸ ✹).
        auto is_chrome_glyph = [](const std::string& s, std::size_t i) {
            if (i + 3 > s.size()) return false;
            if (static_cast<unsigned char>(s[i]) != 0xE2) return false;
            const auto b2 = static_cast<unsigned char>(s[i + 1]);
            const auto b3 = static_cast<unsigned char>(s[i + 2]);
            // ✻ U+273B, ✦ U+2726, ✧ U+2727, ✶ U+2736, ✷ U+2737,
            // ✸ U+2738, ✹ U+2739
            if (b2 == 0x9C && (b3 == 0xBB || b3 == 0xA6 || b3 == 0xA7 ||
                               b3 == 0xB6 || b3 == 0xB7 || b3 == 0xB8 ||
                               b3 == 0xB9))
                return true;
            // ❋ U+274B
            if (b2 == 0x9D && b3 == 0x8B)
                return true;
            return false;
        };

        std::string tmp;
        tmp.reserve(clean.size());
        for (std::size_t i = 0; i < clean.size();) {
            // Require the two leading spaces of the spinner padding so a
            // glyph that happens to appear in message content is not
            // mistaken for the chrome spinner.
            if (i >= 2 && clean[i - 1] == ' ' && clean[i - 2] == ' ' &&
                is_chrome_glyph(clean, i) && i + 3 < clean.size() &&
                clean[i + 3] == ' ') {
                // Match: <glyph> <verb>…<dots>
                std::size_t j = i + 4;  // skip glyph + space
                const std::size_t verb_start = j;
                while (j < clean.size() &&
                       ((clean[j] >= 'A' && clean[j] <= 'Z') ||
                        (clean[j] >= 'a' && clean[j] <= 'z') ||
                        clean[j] == '\''))
                    ++j;
                // … is U+2026 (E2 80 A6); dots are 1-3 '.' (blink cycle).
                if (j > verb_start && j + 3 <= clean.size() &&
                    clean.compare(j, 3, "\xE2\x80\xA6") == 0) {
                    std::size_t k = j + 3;
                    while (k < clean.size() && clean[k] == '.') ++k;
                    if (k > j + 3) {
                        tmp += "<spinner> <verb>\xE2\x80\xA6.";
                        i = k;
                        continue;
                    }
                }
            }
            tmp += clean[i];
            ++i;
        }
        clean = std::move(tmp);
    }

    // ── Step 2c: thinking-duration normalization ──────────────────────
    // The collapsed thinking row (RenderThinkingMessageCollapsed) shows
    // "∴ Thought for Xms" / "∴ Thought for X.Xs" when the thinking
    // duration is known, or "∴ Thinking" when it is zero.  The compressed
    // chain row (render_compressed_chain_row) shows "Thought for Xs" /
    // "Thinking for Xs" when the summed thinking duration is non-zero,
    // or omits the "for Xs" entirely when it is zero.  The duration is
    // wall-clock-driven (steady_clock ticks between content_block_start
    // and content_block_stop) and non-deterministic in replay: debug
    // builds measure ~2ms, optimized release builds ~0ms.  Normalize
    // both forms so golden files are deterministic.
    //
    // Collapsed row (ms or decimal-seconds, followed by two spaces):
    //   "Thought for 2ms  " → "Thought  "
    //   "Thought for 1.5s  " → "Thought  "
    //   "∴ Thinking  " (0ms) → "∴ Thought  "
    // Chain (whole-seconds, followed by ", "):
    //   "Thought for 0s, " → "Thought, "
    //   "Thinking for 0s, " → "Thinking, "
    {
        std::string tmp;
        tmp.reserve(clean.size());
        for (std::size_t i = 0; i < clean.size();) {
            // "Thought for <digits>..." — collapsed row or static chain
            if (i + 12 <= clean.size() &&
                clean.compare(i, 12, "Thought for ") == 0) {
                std::size_t j = i + 12;
                while (j < clean.size() &&
                       clean[j] >= '0' && clean[j] <= '9') ++j;
                if (j > i + 12) {
                    // ms form (collapsed row, < 1s)
                    if (j + 2 <= clean.size() &&
                        clean.compare(j, 2, "ms") == 0) {
                        tmp += "Thought";
                        i = j + 2;
                        continue;
                    }
                    // decimal-seconds form (collapsed row, ≥ 1s)
                    if (j < clean.size() && clean[j] == '.') {
                        std::size_t k = j + 1;
                        while (k < clean.size() &&
                               clean[k] >= '0' && clean[k] <= '9') ++k;
                        if (k > j + 1 && k < clean.size() &&
                            clean[k] == 's') {
                            tmp += "Thought";
                            i = k + 1;
                            continue;
                        }
                    }
                    // whole-seconds form (static compressed chain,
                    // followed by ", " before the tool breakdown)
                    if (j + 3 <= clean.size() &&
                        clean.compare(j, 3, "s, ") == 0) {
                        tmp += "Thought, ";
                        i = j + 3;
                        continue;
                    }
                    // whole-seconds form (lone thinking chain, no tool
                    // breakdown — followed by "  " before the expand hint)
                    if (j + 2 <= clean.size() &&
                        clean.compare(j, 2, "s ") == 0) {
                        tmp += "Thought ";
                        i = j + 2;
                        continue;
                    }
                }
            }
            // "Thinking for <digits>s, " (live compressed chain with tools)
            // or "Thinking for <digits>s " (live lone thinking chain)
            if (i + 13 <= clean.size() &&
                clean.compare(i, 13, "Thinking for ") == 0) {
                std::size_t j = i + 13;
                while (j < clean.size() &&
                       clean[j] >= '0' && clean[j] <= '9') ++j;
                if (j > i + 13 && j + 3 <= clean.size() &&
                    clean.compare(j, 3, "s, ") == 0) {
                    tmp += "Thinking, ";
                    i = j + 3;
                    continue;
                }
                if (j > i + 13 && j + 2 <= clean.size() &&
                    clean.compare(j, 2, "s ") == 0) {
                    tmp += "Thinking ";
                    i = j + 2;
                    continue;
                }
                // live lone thinking chain: "Thinking for 0s…" → "Thinking…"
                // (ellipsis = \xE2\x80\xA6, 3 bytes)
                if (j > i + 13 && j + 4 <= clean.size() &&
                    static_cast<unsigned char>(clean[j]) == 's' &&
                    static_cast<unsigned char>(clean[j + 1]) == 0xE2 &&
                    static_cast<unsigned char>(clean[j + 2]) == 0x80 &&
                    static_cast<unsigned char>(clean[j + 3]) == 0xA6) {
                    tmp += "Thinking\xE2\x80\xA6";
                    i = j + 4;
                    continue;
                }
            }
            // "∴ Thinking  " (two spaces — collapsed row with 0 duration)
            // → "∴ Thought  ".  The ∴ prefix + two-space separator is
            // unique to the collapsed row; the expanded row uses
            // "∴ Thinking…" (ellipsis) and the timeout form uses
            // "∴ Thinking (" (paren).
            if (i + 3 <= clean.size() &&
                static_cast<unsigned char>(clean[i]) == 0xE2 &&
                static_cast<unsigned char>(clean[i + 1]) == 0x88 &&
                static_cast<unsigned char>(clean[i + 2]) == 0xB4) {
                // " Thinking  " (11 bytes) follows the ∴ prefix.
                if (i + 14 <= clean.size() &&
                    clean.compare(i + 3, 11, " Thinking  ") == 0) {
                    tmp += "\xE2\x88\xB4 Thought  ";
                    i += 14;
                    continue;
                }
            }
            tmp += clean[i];
            ++i;
        }
        clean = std::move(tmp);
    }

    // ── Step 3: timestamp normalization ────────────────────────────────
    // The UI renders relative timestamps as "just now", "Xs ago",
    // "Xm ago", "Xh ago", "Xd ago" (message_timestamp.cppm). These are
    // wall-clock-driven and non-deterministic in replay.
    {
        std::string tmp;
        tmp.reserve(clean.size());
        for (std::size_t i = 0; i < clean.size();) {
            // "just now" (8 chars)
            if (i + 8 <= clean.size() &&
                clean.compare(i, 8, "just now") == 0) {
                tmp += "<timestamp>";
                i += 8;
                continue;
            }
            // "Xs ago" / "Xm ago" / "Xh ago" / "Xd ago"
            // (1+ digits + unit + " ago" = digits + 5 chars)
            if (clean[i] >= '0' && clean[i] <= '9') {
                std::size_t j = i;
                while (j < clean.size() &&
                       clean[j] >= '0' && clean[j] <= '9') ++j;
                if (j + 5 <= clean.size() &&
                    (clean[j] == 's' || clean[j] == 'm' ||
                     clean[j] == 'h' || clean[j] == 'd') &&
                    clean.compare(j + 1, 4, " ago") == 0) {
                    tmp += "<timestamp>";
                    i = j + 5;
                    continue;
                }
            }
            tmp += clean[i];
            ++i;
        }
        clean = std::move(tmp);
    }

    // ── Step 3b: blinking-dot normalization ───────────────────────────
    // The compressed chain's ● origin dot blinks (~500ms on/off) for
    // live chains.  Normalize both states to "<dot>" so golden files
    // are deterministic.  Only chain header lines (containing
    // "(click to expand)" or "(click to collapse)") are affected —
    // the ● dot in other contexts (user/assistant messages) is left
    // alone.
    {
        std::string tmp;
        tmp.reserve(clean.size());
        std::size_t line_start = 0;
        for (std::size_t i = 0; i <= clean.size(); ++i) {
            if (i == clean.size() || clean[i] == '\n') {
                std::string_view ln(clean.data() + line_start,
                                    i - line_start);
                const bool is_chain_header =
                    ln.find("(click to expand)") !=
                        std::string_view::npos ||
                    ln.find("(click to collapse)") !=
                        std::string_view::npos;
                if (is_chain_header) {
                    // Dot-on: "● " = \xE2\x97\x8F + space (4 bytes)
                    if (ln.size() >= 4 &&
                        static_cast<unsigned char>(ln[0]) == 0xE2 &&
                        static_cast<unsigned char>(ln[1]) == 0x97 &&
                        static_cast<unsigned char>(ln[2]) == 0x8F &&
                        ln[3] == ' ') {
                        tmp += "<dot> ";
                        tmp += ln.substr(4);
                    }
                    // Dot-off: two spaces
                    else if (ln.size() >= 2 && ln[0] == ' ' &&
                             ln[1] == ' ') {
                        tmp += "<dot> ";
                        tmp += ln.substr(2);
                    } else {
                        tmp += ln;
                    }
                } else {
                    tmp += ln;
                }
                if (i < clean.size()) tmp += '\n';
                line_start = i + 1;
            }
        }
        clean = std::move(tmp);
    }

    // ── Step 3c: temp-directory path normalization ────────────────────
    // Golden snapshots were recorded on macOS, where the harness cwd is
    // fs::temp_directory_path() = "/var/folders/<xx>/<…>/T/".  Replay on
    // Linux uses "/tmp/".  Replace both forms (and the folder-pill footer
    // stem derived from them) with "<tmpdir>" so snapshots are portable
    // without touching rendered content.  Applied to both the golden text
    // and the actual render, so the substitution is symmetric.
    {
        // macOS NSTemporaryDirectory: /var/folders/<xx>/<…>/T
        const std::string mac_prefix = "/var/folders/";
        std::size_t pos = 0;
        while ((pos = clean.find(mac_prefix, pos)) != std::string::npos) {
            const std::size_t end =
                clean.find_first_of(" \t\n\r\"'", pos);
            const std::size_t stop =
                (end == std::string::npos) ? clean.size() : end;
            const std::string seg = clean.substr(pos, stop - pos);
            if (seg.find("/T/") != std::string::npos ||
                (seg.size() >= 2 && seg.substr(seg.size() - 2) == "/T")) {
                clean.replace(pos, stop - pos, "<tmpdir>");
                pos += 8;  // strlen("<tmpdir>")
            } else {
                pos = stop;
            }
        }
        // Runtime temp dir (Linux: /tmp).  Boundary-checked so sibling
        // strings like "/tmp_backup" are left alone.
        try {
            const std::string tmp =
                std::filesystem::temp_directory_path().string();
            std::size_t p = 0;
            while ((p = clean.find(tmp, p)) != std::string::npos) {
                const std::size_t after = p + tmp.size();
                const bool boundary =
                    after >= clean.size() || clean[after] == '/' ||
                    clean[after] == ' ' || clean[after] == '\t' ||
                    clean[after] == '\n' || clean[after] == '\r' ||
                    clean[after] == '"' || clean[after] == '\'';
                if (boundary) {
                    clean.replace(p, tmp.size(), "<tmpdir>");
                    p += 8;
                } else {
                    p = after;
                }
            }
        } catch (...) {
            // temp_directory_path() can throw on unusual platforms; the
            // macOS pass above still applies.
        }
        // Folder-pill stem.  The prompt footer renders the cwd's last 2
        // path components as "📁 <stem>" (GetLastPathComponents in
        // prompt_input_footer.cppm).  For the harness temp cwd this is
        // "tmp" on Linux and "<hash>/T" on macOS — a derived rendering the
        // full-path passes above do not touch.  Normalize both forms so
        // the footer matches across platforms.  Only stems that are
        // exactly "tmp" or end with "/T" (the macOS NSTemporaryDirectory
        // tail) are replaced, so a real project cwd's folder pill is left
        // alone.
        {
            const std::string folder_prefix = "\xF0\x9F\x93\x81 ";  // 📁 + SP
            std::size_t p = 0;
            while ((p = clean.find(folder_prefix, p)) !=
                   std::string::npos) {
                const std::size_t stem_start = p + folder_prefix.size();
                std::size_t stem_end = stem_start;
                while (stem_end < clean.size() && clean[stem_end] != ' ' &&
                       clean[stem_end] != '\n' && clean[stem_end] != '\r')
                    ++stem_end;
                const std::string stem =
                    clean.substr(stem_start, stem_end - stem_start);
                const bool is_temp_stem =
                    stem == "tmp" ||
                    (stem.size() >= 2 &&
                     stem.substr(stem.size() - 2) == "/T");
                if (is_temp_stem) {
                    clean.replace(stem_start, stem_end - stem_start,
                                  "<tmpdir>");
                    p = stem_start + 8;  // strlen("<tmpdir>")
                } else {
                    p = stem_end;
                }
            }
        }
    }

    // ── Steps 4-5: trim trailing whitespace + collapse trailing blanks ─
    std::string result;
    std::string line;
    for (std::size_t i = 0; i < clean.size(); ++i) {
        if (clean[i] == '\n') {
            while (!line.empty() &&
                   (line.back() == ' ' || line.back() == '\t' ||
                    line.back() == '\r'))
                line.pop_back();
            result += line;
            result += '\n';
            line.clear();
        } else {
            line += clean[i];
        }
    }
    // Last line (Screen::ToString may not end with \n)
    while (!line.empty() &&
           (line.back() == ' ' || line.back() == '\t' ||
            line.back() == '\r'))
        line.pop_back();
    result += line;

    // Collapse trailing blank lines
    while (!result.empty() && result.back() == '\n')
        result.pop_back();

    return result;
}

}  // namespace loom::testing
