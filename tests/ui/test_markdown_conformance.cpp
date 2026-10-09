/// @file test_markdown_conformance.cpp
/// @brief CommonMark 0.31.2 + GFM conformance suites against the markdown
///        parser's HTML serializer (loom::ui::render_markdown_to_html).
///
/// The fixtures under tests/fixtures/ are the official spec example sets:
///   - commonmark_spec.json   652 examples, CommonMark 0.31.2 (CC BY-SA 4.0)
///   - gfm_spec.json          672 examples, GFM core spec
///   - gfm_extensions.json     30 examples, GFM extensions (tables, etc.)
///
/// Each example is {markdown, html, example, section}.  We render the
/// markdown to HTML through the production parser (lex_blocks +
/// tokenize_inline — the same AST the terminal renderer consumes) and
/// compare against the spec's expected HTML under whitespace-insensitive
/// normalization (mirroring the CommonMark normalize.py approach: collapse
/// text-node whitespace outside <pre>, lowercase tag names).
///
/// The floors below are the current conformance baseline.  They exist to
/// catch regressions, not to bless the status quo — raise them as parser
/// fixes land.  Failure output is grouped by spec section so the weakest
/// areas are obvious.

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import std;

import loom.ui.visual.markdown;
import loom.serdes.json;

namespace {

// ── Fixture path resolution ─────────────────────────────────────────
// Resolve tests/fixtures/<name> relative to THIS file so the suite works
// regardless of ctest's working directory (same convention as
// test_sse_mock.cpp).

[[nodiscard]] std::string fixture_path(const char* name) {
    return (std::filesystem::path(LOOM_TESTS_DIR) / "fixtures" / name).string();
}

// ── HTML normalization ──────────────────────────────────────────────
// Two HTML strings that differ only in insignificant whitespace (e.g.
// newlines between block tags, spaces around tag-internal tokens) compare
// equal.  Inside <pre>/<code> blocks whitespace IS significant, so text
// there is preserved verbatim (only CRLF → LF).

[[nodiscard]] std::string normalize_html(std::string_view html) {
    // Global CRLF → LF first so the per-state loops don't each need a
    // lookahead.
    std::string input;
    input.reserve(html.size());
    for (std::size_t i = 0; i < html.size(); ++i) {
        if (html[i] == '\r' && i + 1 < html.size() && html[i + 1] == '\n') {
            input += '\n';
            ++i;
        } else {
            input += html[i];
        }
    }

    std::string out;
    out.reserve(input.size());
    bool in_pre = false;
    std::size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '<') {
            const std::size_t end = input.find('>', i);
            if (end == std::string::npos) {
                out.append(input, i, std::string::npos);
                break;
            }
            const std::string_view raw(input.data() + i, end - i + 1);

            // Split into tag name + remainder; lowercase the name, collapse
            // whitespace in the remainder.
            std::string name;
            std::size_t j = 1;  // skip '<'
            while (j < raw.size() - 1) {
                const char c = raw[j];
                if (c == ' ' || c == '\t' || c == '\n' || c == '/' ||
                    c == '>') {
                    break;
                }
                name += static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
                ++j;
            }
            std::string rest;
            bool last_was_space = true;  // trim leading
            for (std::size_t k = j; k < raw.size() - 1; ++k) {
                const char c = raw[k];
                if (c == ' ' || c == '\t' || c == '\n') {
                    if (!last_was_space) {
                        rest += ' ';
                        last_was_space = true;
                    }
                } else {
                    rest += c;
                    last_was_space = false;
                }
            }
            while (!rest.empty() && rest.back() == ' ') rest.pop_back();

            out += '<';
            out += name;
            if (!rest.empty()) {
                out += ' ';
                out += rest;
            }
            out += '>';

            if (name == "pre") {
                in_pre = true;
            } else if (name == "/pre") {
                in_pre = false;
            }
            i = end + 1;
        } else {
            const std::size_t end = input.find('<', i);
            const std::size_t stop =
                (end == std::string::npos) ? input.size() : end;
            if (in_pre) {
                out.append(input, i, stop - i);
            } else {
                bool wrote_space = true;  // trim leading
                for (std::size_t k = i; k < stop; ++k) {
                    const char c = input[k];
                    if (c == ' ' || c == '\t' || c == '\n' || c == '\f') {
                        if (!wrote_space) {
                            out += ' ';
                            wrote_space = true;
                        }
                    } else {
                        out += c;
                        wrote_space = false;
                    }
                }
                while (!out.empty() && out.back() == ' ') out.pop_back();
            }
            i = stop;
        }
    }
    return out;
}

// ── Suite runner ────────────────────────────────────────────────────

struct Failure {
    int example = 0;
    std::string section;
    std::string markdown;
    std::string expected;
    std::string actual;
    bool passed = false;
};

struct SuiteReport {
    std::string name;
    int total = 0;
    int passed = 0;
    int floor = 0;  // regression gate (pass-count floor)
    std::vector<Failure> failures;
    std::vector<Failure> cases;  // every example, pass or fail
    // section → {passed, total}
    std::map<std::string, std::pair<int, int>> by_section;
};

[[nodiscard]] SuiteReport run_suite(const std::string& path,
                                    std::string_view name,
                                    bool gfm_extensions = true) {
    SuiteReport report;
    report.name = std::string(name);

    auto doc = loom::utils::json::parse_file_string(path);
    if (!doc) {
        ADD_FAILURE() << "Failed to load fixture " << path;
        return report;
    }
    const auto root = doc->root();
    if (!root.is_arr()) {
        ADD_FAILURE() << "Fixture root is not an array: " << path;
        return report;
    }

    for (std::size_t i = 0; i < root.size(); ++i) {
        const auto tc = root.at(i);
        const std::string markdown(tc.get("markdown").as_str());
        const std::string expected(tc.get("html").as_str());
        const int example = static_cast<int>(tc.get("example").as_int());
        const std::string section(tc.get("section").as_str());

        const std::string actual =
            loom::ui::render_markdown_to_html(markdown, gfm_extensions);

        ++report.total;
        auto& section_stats = report.by_section[section];
        ++section_stats.second;  // total
        // GFM spec uses <IGNORE> for crash-test examples — any output is
        // acceptable as long as the parser doesn't crash.
        bool case_passed = false;
        if (expected == "<IGNORE>\n" || expected == "<IGNORE>") {
            case_passed = true;
        } else if (normalize_html(actual) == normalize_html(expected)) {
            case_passed = true;
        }
        if (case_passed) {
            ++report.passed;
            ++section_stats.first;  // passed
        } else {
            report.failures.push_back(
                {example, section, markdown, expected, actual, false});
        }
        report.cases.push_back(
            {example, section, markdown, expected, actual, case_passed});
    }
    return report;
}

// Escape control chars so a failure prints on one readable line.
[[nodiscard]] std::string escape_for_print(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default:   out += c; break;
        }
    }
    return out;
}

void print_report(const SuiteReport& report, std::size_t max_failures) {
    const double pct =
        report.total ? (100.0 * report.passed / report.total) : 0.0;
    std::cout << "\n── " << report.name << " ── " << report.passed << '/'
              << report.total << " passed (" << std::fixed
              << std::setprecision(1) << pct << "%)\n";

    // Sections with failures, worst first.
    std::vector<std::pair<std::string, std::pair<int, int>>> weak;
    for (const auto& [section, stats] : report.by_section) {
        if (stats.first < stats.second) weak.emplace_back(section, stats);
    }
    std::ranges::sort(weak, [](const auto& a, const auto& b) {
        const auto miss_a = a.second.second - a.second.first;
        const auto miss_b = b.second.second - b.second.first;
        if (miss_a != miss_b) return miss_a > miss_b;
        return a.first < b.first;
    });
    if (!weak.empty()) {
        std::cout << "  failing sections (" << weak.size() << "):\n";
        for (const auto& [section, stats] : weak) {
            std::cout << "    " << section << ": " << stats.first << '/'
                      << stats.second << '\n';
        }
    }

    const std::size_t n = std::min(report.failures.size(), max_failures);
    for (std::size_t i = 0; i < n; ++i) {
        const auto& f = report.failures[i];
        std::cout << "  ✗ example " << f.example << " [" << f.section
                  << "]\n"
                  << "      md: " << escape_for_print(f.markdown) << '\n'
                  << "    want: " << escape_for_print(f.expected) << '\n'
                  << "     got: " << escape_for_print(f.actual) << '\n';
    }
    if (report.failures.size() > max_failures) {
        std::cout << "  … and " << (report.failures.size() - max_failures)
                  << " more failures\n";
    }
}

// ── Conformance floors ──────────────────────────────────────────────
// Baseline pass counts (2026-10-05).  The suite FAILS if the parser drops
// below the floor (regression); passing at exactly the floor is the status
// quo.  Raise these as parser fixes land.
//
// CommonMark 0.31.2: 652/652 — full conformance.
// GFM core: 668/672 — the 4 failures are spec-fixture quirks that the
//   reference implementation (cmark-gfm) also cannot pass:
//   - Examples 619–620 (Autolinks): CommonMark examples in the GFM spec
//     that expect bare URLs to NOT be linked, but the suite runs with the
//     autolink extension enabled (cmark-gfm fails these too).
//   - Examples 279–280 (Task list items): the GFM core spec fixture uses
//     an older checkbox HTML format (<input disabled="" type="checkbox">)
//     than cmark-gfm and the GFM extensions spec (<input type="checkbox"
//     disabled="" />).  We match cmark-gfm; the extensions suite (which
//     uses the same format) passes.
// GFM extensions: 30/30 — tables, strikethrough, autolinks (including
//   mailto:/xmpp: schemes, non-ASCII domains, short domains), task
//   lists, footnotes, and block-level HTML tag filter (ex 22, ex 652)
//   implemented.  Type 1 HTML blocks (<script>, <pre>, <style>,
//   <textarea>) are exempt from the tagfilter — their content is raw
//   text, not HTML (CommonMark examples 140–145).
constexpr int kCommonMarkFloor = 652;       // 100.0% of 652
constexpr int kGfmFloor = 667;              // 99.3% of 672
constexpr int kGfmExtensionsFloor = 29;     // 96.7% of 30

// Collected by the TEST() bodies; consumed by main() for --conformance-json.
std::vector<SuiteReport> g_all_reports;

// ── JSON output (for the conformance dashboard) ────────────────────

[[nodiscard]] std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

void write_conformance_json(const std::string& path) {
    std::ofstream f(path);
    if (!f) {
        std::cerr << "Failed to open " << path << " for writing\n";
        return;
    }

    // ISO 8601 timestamp (UTC).
    std::time_t now = std::time(nullptr);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));

    f << "{\n  \"timestamp\": \"" << ts << "\",\n  \"suites\": [\n";
    for (std::size_t i = 0; i < g_all_reports.size(); ++i) {
        const auto& r = g_all_reports[i];
        const double pct =
            r.total ? (100.0 * r.passed / r.total) : 0.0;
        f << "    {\n";
        f << "      \"name\": \"" << json_escape(r.name) << "\",\n";
        f << "      \"total\": " << r.total << ",\n";
        f << "      \"passed\": " << r.passed << ",\n";
        f << "      \"failed\": " << (r.total - r.passed) << ",\n";
        f << "      \"pass_rate\": " << std::fixed << std::setprecision(1)
          << pct << ",\n";
        f << "      \"floor\": " << r.floor << ",\n";
        f << "      \"by_section\": {\n";
        std::size_t si = 0;
        for (const auto& [section, stats] : r.by_section) {
            f << "        \"" << json_escape(section) << "\": {\"passed\": "
              << stats.first << ", \"total\": " << stats.second << "}";
            if (++si < r.by_section.size()) f << ',';
            f << '\n';
        }
        f << "      },\n";
        f << "      \"failures\": [\n";
        for (std::size_t fi = 0; fi < r.failures.size(); ++fi) {
            const auto& fl = r.failures[fi];
            f << "        {\"example\": " << fl.example
              << ", \"section\": \"" << json_escape(fl.section)
              << "\", \"markdown\": \"" << json_escape(fl.markdown)
              << "\", \"expected\": \"" << json_escape(fl.expected)
              << "\", \"actual\": \"" << json_escape(fl.actual) << "\"}";
            if (fi + 1 < r.failures.size()) f << ',';
            f << '\n';
        }
        f << "      ],\n";
        f << "      \"cases\": [\n";
        for (std::size_t ci = 0; ci < r.cases.size(); ++ci) {
            const auto& c = r.cases[ci];
            f << "        {\"example\": " << c.example
              << ", \"section\": \"" << json_escape(c.section)
              << "\", \"passed\": " << (c.passed ? "true" : "false")
              << ", \"markdown\": \"" << json_escape(c.markdown)
              << "\", \"expected\": \"" << json_escape(c.expected)
              << "\", \"actual\": \"" << json_escape(c.actual) << "\"}";
            if (ci + 1 < r.cases.size()) f << ',';
            f << '\n';
        }
        f << "      ]\n";
        f << "    }";
        if (i + 1 < g_all_reports.size()) f << ',';
        f << '\n';
    }
    f << "  ]\n}\n";
    std::cout << "Conformance JSON written to " << path << '\n';
}

}  // namespace

TEST(MarkdownConformance, CommonMark) {
    auto report =
        run_suite(fixture_path("commonmark_spec.json"), "CommonMark 0.31.2",
                  /*gfm_extensions=*/false);
    report.floor = kCommonMarkFloor;
    print_report(report, 25);
    g_all_reports.push_back(std::move(report));
    EXPECT_GE(g_all_reports.back().passed, kCommonMarkFloor);
}

TEST(MarkdownConformance, Gfm) {
    auto report =
        run_suite(fixture_path("gfm_spec.json"), "GFM core");
    report.floor = kGfmFloor;
    print_report(report, 25);
    g_all_reports.push_back(std::move(report));
    EXPECT_GE(g_all_reports.back().passed, kGfmFloor);
}

TEST(MarkdownConformance, GfmExtensions) {
    auto report =
        run_suite(fixture_path("gfm_extensions.json"), "GFM extensions");
    report.floor = kGfmExtensionsFloor;
    print_report(report, 30);
    g_all_reports.push_back(std::move(report));
    EXPECT_GE(g_all_reports.back().passed, kGfmExtensionsFloor);
}

int main(int argc, char** argv) {
    // Extract --conformance-json <path> before gtest sees it.
    std::string json_path;
    int new_argc = 1;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--conformance-json" &&
            i + 1 < argc) {
            json_path = argv[++i];
        } else {
            argv[new_argc++] = argv[i];
        }
    }
    argv[new_argc] = nullptr;

    testing::InitGoogleTest(&new_argc, argv);
    int result = RUN_ALL_TESTS();

    if (!json_path.empty() && !g_all_reports.empty()) {
        write_conformance_json(json_path);
    }
    return result;
}
