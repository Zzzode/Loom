// Source-first ASCII Flow contract for the four closed Grid18 inputs.
// The same-module declaration reads producer-owned canonical rows; public
// assertions still capture the real Mermaid element through PixelAt.
module;
#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
module loom.ui.visual.mermaid;
import std;
import loom.ui.visual.markdown;
import loom.serdes.json;
import loom.crypto.crypto;

#include "mermaid_flow400_authority.hpp"

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
using json::JsonVal;
namespace flow = loom::test::mermaid_flow;
namespace private_flow = loom::ui::mermaid::detail;
using flow::Grid;
using flow::Model;
using flow::OracleWork;

constexpr std::string_view kAsset = "fixtures/mermaid_native/merman_flow_ascii_rows_subset.v1.json";
constexpr std::string_view kParentAsset = "fixtures/mermaid_native/merman_flow_grid18_original_assertions.v1.json";
constexpr std::string_view kParentSha = "19bf29c8c80f6348ea2d67ee981fb4e9c792366f20eb000ef71ed755e80b3839";
constexpr std::string_view kFollowing = "AFTER_FLOW_ASCII_ROWS";
constexpr int kWide = 240, kNarrow = 40, kHeight = 64;

[[noreturn]] void fail(std::string_view message) { throw std::runtime_error(std::string(message)); }
void require(bool value, std::string_view message) { if (!value) fail(message); }
std::string digest(std::string_view value) { return loom::utils::crypto::sha256(value); }

std::string read_exact(const fs::path& path, OracleWork& work) {
    std::error_code error;
    const auto bytes = fs::file_size(path, error);
    require(!error && bytes <= 512u * 1024u, "ASCII Flow immutable input size");
    work.product(static_cast<std::size_t>(bytes), 8u);
    work.spend(4096u);
    std::ifstream file(path, std::ios::binary);
    require(static_cast<bool>(file), "ASCII Flow immutable input open");
    std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    require(text.size() == bytes, "ASCII Flow immutable input read");
    return text;
}

std::string fenced(std::string_view source, std::string_view language) {
    return "```" + std::string(language) + "\n" + std::string(source) + "\n```";
}
ftxui::Element with_following(ftxui::Element body) {
    return ftxui::vbox({std::move(body), ftxui::text(std::string(kFollowing))});
}
Grid capture_public(ftxui::Element body, int width, OracleWork& work) {
    return flow::capture(std::move(body), width, kHeight, &work);
}
void require_following(const Grid& grid, OracleWork& work) {
    static_cast<void>(flow::flow400_following(grid, work, kFollowing));
}
std::string rust_literal(std::string_view source, OracleWork& work) {
    work.product(source.size() + 2u, 8u);
    std::string escaped;
    escaped.reserve(source.size() + 2u);
    for (const char byte : source) {
        work.spend(4u);
        switch (byte) {
            case '\\': escaped += "\\\\"; break;
            case '\"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(byte); break;
        }
    }
    return escaped;
}

fs::path upstream_path(const fs::path& tests_root, std::string_view original) {
    constexpr std::string_view prefix = "crates/merman-ascii/";
    require(original.starts_with(prefix) && original.find("..") == std::string_view::npos,
            "ASCII Flow upstream path");
    return tests_root / "fixtures/mermaid_terminal/merman/upstream" /
        std::string(original.substr(prefix.size()));
}
fs::path helper_path(const fs::path& tests_root, std::string_view original) {
    require(!original.empty() && original.find("..") == std::string_view::npos,
            "ASCII Flow helper path");
    return tests_root / "fixtures/mermaid_terminal/merman/upstream" / std::string(original);
}

JsonVal unique_case(JsonVal parent, std::string_view id, OracleWork& work) {
    const auto cases = parent.get("cases");
    require(cases.is_arr() && cases.size() == 59u, "ASCII Flow parent case denominator");
    JsonVal found; std::size_t matches = 0u;
    cases.iter([&](JsonVal row) {
        work.spend(128u);
        if (flow::flow400_text(row.get("id")) == id) { found = row; ++matches; }
    });
    require(matches == 1u, "ASCII Flow parent source case uniqueness");
    flow::flow400_keys(found, {"id", "family", "input", "input_sha256", "input_utf8_bytes", "source_facts"});
    require(flow::flow400_text(found.get("family")) == "flowchart", "ASCII Flow parent source family");
    return found;
}
JsonVal unique_function(JsonVal parent, std::string_view name, OracleWork& work) {
    const auto functions = parent.get("functions");
    require(functions.is_arr() && functions.size() == 18u, "ASCII Flow parent function denominator");
    JsonVal found; std::size_t matches = 0u;
    functions.iter([&](JsonVal row) {
        work.spend(256u);
        if (flow::flow400_text(row.get("name")) == name) { found = row; ++matches; }
    });
    require(matches == 1u, "ASCII Flow parent function uniqueness");
    flow::flow400_keys(found, {"name", "source", "helper", "options", "case_ids", "direct_assertions"});
    return found;
}
JsonVal unique_helper(JsonVal parent, std::string_view name, OracleWork& work) {
    const auto helpers = parent.get("helpers");
    require(helpers.is_arr() && helpers.size() == 3u, "ASCII Flow parent helper denominator");
    JsonVal found; std::size_t matches = 0u;
    helpers.iter([&](JsonVal row) {
        work.spend(256u);
        if (flow::flow400_text(row.get("name")) == name) { found = row; ++matches; }
    });
    require(matches == 1u, "ASCII Flow parent helper uniqueness");
    flow::flow400_keys(found, {"name", "upstream_path", "file_sha256", "file_bytes", "function_text",
                                "function_sha256", "function_bytes"});
    return found;
}

std::vector<std::string> bind_parent_function(const fs::path& tests_root, JsonVal parent,
                                              JsonVal function, std::string_view case_id,
                                              std::string_view source, OracleWork& work,
                                              std::set<std::string>& assertion_sites) {
    const auto source_record = function.get("source");
    flow::flow400_keys(source_record, {"captured_path", "captured_sha256", "captured_bytes", "captured_text",
                                       "upstream_path", "original_file_sha256", "original_byte_start", "original_byte_end",
                                       "original_line_start", "original_line_end"});
    const auto original = read_exact(upstream_path(tests_root, flow::flow400_text(source_record.get("upstream_path"))), work);
    require(digest(original) == flow::flow400_text(source_record.get("original_file_sha256")),
            "ASCII Flow vendored upstream file SHA");
    const auto start = flow::flow400_number(source_record.get("original_byte_start"));
    const auto end = flow::flow400_number(source_record.get("original_byte_end"));
    require(start < end && end <= original.size(), "ASCII Flow vendored upstream function range");
    const auto function_text = original.substr(start, end - start);
    const auto source_literal = rust_literal(source, work);
    require(function_text.size() == flow::flow400_number(source_record.get("captured_bytes")) &&
                function_text == flow::flow400_text(source_record.get("captured_text")) &&
                digest(function_text) == flow::flow400_text(source_record.get("captured_sha256")) &&
                function_text.find(source_literal) != std::string::npos,
            "ASCII Flow vendored upstream function binding");
    const auto options = function.get("options");
    require(options.is_arr() && options.size() == 1u && flow::flow400_text(options.at(0)) == "encoding:ascii" &&
                function_text.find("AsciiRenderOptions::ascii()") != std::string::npos,
            "ASCII Flow upstream ASCII option identity");
    const auto helper = unique_helper(parent, flow::flow400_text(function.get("helper")), work);
    const auto helper_file = read_exact(helper_path(tests_root, flow::flow400_text(helper.get("upstream_path"))), work);
    const auto helper_text = flow::flow400_text(helper.get("function_text"));
    require(helper_file.size() == flow::flow400_number(helper.get("file_bytes")) &&
                digest(helper_file) == flow::flow400_text(helper.get("file_sha256")) &&
                helper_text.size() == flow::flow400_number(helper.get("function_bytes")) &&
                digest(helper_text) == flow::flow400_text(helper.get("function_sha256")) &&
                helper_file.find(helper_text) != std::string::npos,
            "ASCII Flow vendored helper identity");
    const auto helper_name = flow::flow400_text(helper.get("name"));
    if (helper_name == "assert_rectangular_char_grid")
        require(helper_text.find(".chars().count()") != std::string::npos,
                "ASCII Flow original scalar-width helper semantics");
    else if (helper_name == "assert_rectangular_terminal_grid")
        require(helper_file.find("fn terminal_test_width(input: &str) -> usize") != std::string::npos &&
                    helper_file.find("UnicodeWidthStr::width(input)") != std::string::npos,
                "ASCII Flow original terminal-width helper semantics");
    else fail("ASCII Flow unknown original helper");
    const auto assertions = function.get("direct_assertions");
    require(assertions.is_arr() && assertions.size() == 1u, "ASCII Flow source assertion count");
    const auto assertion = assertions.at(0);
    flow::flow400_keys(assertion, {"ordinal", "macro", "source_text", "source_sha256", "predicate_kind", "predicate"});
    const auto assertion_text = flow::flow400_text(assertion.get("source_text"));
    require(digest(assertion_text) == flow::flow400_text(assertion.get("source_sha256")),
            "ASCII Flow source assertion SHA");
    assertion_sites.emplace(flow::flow400_text(assertion.get("source_sha256")));
    const auto kind = flow::flow400_text(assertion.get("predicate_kind"));
    const auto predicate = assertion.get("predicate");
    JsonVal labels;
    if (kind == "contains_all") labels = predicate.get("labels");
    else if (kind == "contains_all_by_case") labels = predicate.get(case_id);
    else fail("ASCII Flow unsupported original predicate");
    require(labels.is_arr() && labels.size() == 3u, "ASCII Flow source predicate labels");
    work.product(labels.size(), 4u * sizeof(std::string) + 32u);
    std::vector<std::string> result;
    result.reserve(labels.size());
    labels.iter([&](JsonVal label) { result.emplace_back(flow::flow400_text(label)); });
    return result;
}

Model bind_original_after_captures(const fs::path& tests_root, JsonVal item, JsonVal parent,
                                   OracleWork& work, std::map<std::string, std::set<std::string>>& function_cases,
                                   std::set<std::string>& assertion_sites, std::vector<std::string>& labels,
                                   std::string& helper_name) {
    flow::flow400_keys(item, {"case_id", "input", "input_sha256", "input_utf8_bytes", "parent_function_name",
                              "flow400_model_json", "flow400_model_sha256"});
    const auto case_id = flow::flow400_text(item.get("case_id"));
    const auto input = flow::flow400_text(item.get("input"));
    require(input.size() == flow::flow400_number(item.get("input_utf8_bytes")) &&
                digest(input) == flow::flow400_text(item.get("input_sha256")), "ASCII Flow subset source identity");
    const auto parent_case = unique_case(parent, case_id, work);
    require(flow::flow400_text(parent_case.get("input")) == input &&
                flow::flow400_text(parent_case.get("input_sha256")) == flow::flow400_text(item.get("input_sha256")) &&
                flow::flow400_number(parent_case.get("input_utf8_bytes")) == input.size(),
            "ASCII Flow copied parent source binding");
    const auto name = flow::flow400_text(item.get("parent_function_name"));
    const auto function = unique_function(parent, name, work);
    helper_name = std::string(flow::flow400_text(function.get("helper")));
    bool owns_case = false;
    function.get("case_ids").iter([&](JsonVal id) { owns_case |= flow::flow400_text(id) == case_id; });
    require(owns_case, "ASCII Flow parent function case membership");
    function_cases[std::string(name)].emplace(case_id);
    labels = bind_parent_function(tests_root, parent, function, case_id, input, work, assertion_sites);

    const auto model_text = flow::flow400_text(item.get("flow400_model_json"));
    work.product(model_text.size(), 8u);
    require(digest(model_text) == flow::flow400_text(item.get("flow400_model_sha256")), "ASCII Flow Model SHA");
    const auto parsed = json::parse(model_text);
    require(parsed.has_value(), "ASCII Flow Model JSON");
    std::size_t groups = 0u, members = 0u;
    flow::flow400_shape(parsed->root(), groups, members);
    auto model = flow::parse_model(parsed->root());
    require(model.direction == "LR" && model.nodes.size() == 2u && model.edges.size() == 1u && model.groups.empty(),
            "ASCII Flow complete Model cardinality");
    const auto& edge = model.edges.front();
    require(edge.from == 0 && edge.to == 1 && edge.style == "solid" && !edge.start && edge.end,
            "ASCII Flow original Model edge profile");
    return model;
}

void require_function_closure(JsonVal parent, const std::map<std::string, std::set<std::string>>& selected,
                              OracleWork& work) {
    require(selected.size() == 2u, "ASCII Flow selected function count");
    for (const auto& [name, source_cases] : selected) {
        const auto function = unique_function(parent, name, work);
        std::set<std::string> full_cases;
        function.get("case_ids").iter([&](JsonVal id) { full_cases.emplace(flow::flow400_text(id)); });
        require(full_cases == source_cases, "ASCII Flow complete source function runtime closure");
    }
}

std::string canonical_text(const std::vector<std::string>& rows) {
    std::string text;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        if (index) text.push_back('\n');
        text += rows[index];
    }
    return text;
}

std::size_t scalar_width(std::string_view row) {
    std::size_t count = 0u;
    for (std::size_t at = 0u; at < row.size();) {
        const auto lead = static_cast<unsigned char>(row[at++]);
        std::size_t trailing = 0u;
        if (lead < 0x80u) trailing = 0u;
        else if (lead >= 0xc2u && lead <= 0xdfu) trailing = 1u;
        else if (lead >= 0xe0u && lead <= 0xefu) trailing = 2u;
        else if (lead >= 0xf0u && lead <= 0xf4u) trailing = 3u;
        else fail("ASCII Flow canonical row invalid UTF-8 lead");
        require(at + trailing <= row.size(), "ASCII Flow canonical row truncated UTF-8");
        for (std::size_t n = 0u; n < trailing; ++n)
            require((static_cast<unsigned char>(row[at++]) & 0xc0u) == 0x80u,
                    "ASCII Flow canonical row invalid UTF-8 continuation");
        ++count;
    }
    return count;
}

bool original_rectangle_accepts(const std::vector<std::string>& rows, std::string_view helper_name) {
    if (rows.empty()) return false;
    const auto width = helper_name == "assert_rectangular_terminal_grid"
        ? static_cast<std::size_t>(ftxui::string_width(rows.front())) : scalar_width(rows.front());
    for (const auto& row : rows) {
        const auto actual = helper_name == "assert_rectangular_terminal_grid"
            ? static_cast<std::size_t>(ftxui::string_width(row)) : scalar_width(row);
        if (actual != width) return false;
    }
    return true;
}

void require_original_rectangle(const std::vector<std::string>& rows, std::string_view helper_name) {
    require(original_rectangle_accepts(rows, helper_name), "ASCII Flow original rectangular helper predicate");
}

Grid capture_rows(const std::vector<std::string>& rows, int width, OracleWork& work) {
    std::size_t bytes = 0u;
    for (const auto& row : rows) bytes += row.size();
    work.product(rows.size(), sizeof(ftxui::Element) + sizeof(std::string));
    work.product(bytes + kFollowing.size() + 1u, 8u);
    ftxui::Elements elements;
    elements.reserve(rows.size());
    for (const auto& row : rows) elements.emplace_back(ftxui::text(row));
    return capture_public(with_following(ftxui::vbox(std::move(elements))), width, work);
}

void require_projection_join(const Grid& public_unicode_wide, const Grid& public_unicode_narrow,
                             const Grid& public_ascii_wide, const Grid& public_ascii_narrow,
                             const private_flow::FlowAsciiRows& rows, OracleWork& work) {
    const auto native_wide = capture_rows(rows.native, kWide, work);
    const auto native_narrow = capture_rows(rows.native, kNarrow, work);
    const auto ascii_wide = capture_rows(rows.ascii, kWide, work);
    const auto ascii_narrow = capture_rows(rows.ascii, kNarrow, work);
    EXPECT_EQ(native_wide, public_unicode_wide) << "private native rows must join actual public Unicode wide Grid";
    EXPECT_EQ(native_narrow, public_unicode_narrow) << "private native rows must join actual public Unicode narrow Grid";
    EXPECT_EQ(ascii_wide, public_ascii_wide) << "ASCII projection must join actual public ASCII wide Grid";
    EXPECT_EQ(ascii_narrow, public_ascii_narrow) << "ASCII projection must join actual public ASCII narrow Grid";
}

enum class ProjectionState : unsigned char { Accept, Reject, Resource, Exception };
struct ProjectionResult { ProjectionState state; std::string_view reason; };

std::optional<std::string_view> expected_ascii_geometry(std::string_view glyph) {
    if (glyph == " ") return " ";
    if (glyph == "─") return "-";
    if (glyph == "│") return "|";
    if (glyph == "┌" || glyph == "┐" || glyph == "└" || glyph == "┘" || glyph == "┬" ||
        glyph == "┤" || glyph == "┴" || glyph == "├" || glyph == "┼") return "+";
    if (glyph == "▶") return ">";
    if (glyph == "◀") return "<";
    if (glyph == "▼") return "v";
    if (glyph == "▲") return "^";
    return {};
}

ProjectionResult validate_public_ascii_projection(const Grid& unicode, const Grid& ascii,
                                                  const Model& model, OracleWork& work) {
    try {
        if (unicode.width != ascii.width || unicode.height != ascii.height ||
            unicode.cells.size() != ascii.cells.size()) return {ProjectionState::Reject, "grid dimensions"};
        const auto geometry = flow::flow400_geometry(unicode, model, work, kFollowing);
        if (!geometry.empty()) {
            for (const auto& error : geometry)
                if (flow::flow400_resource(error)) return {ProjectionState::Resource, "Unicode geometry resource"};
            return {ProjectionState::Reject, "Unicode geometry"};
        }
        // Existing label_blocks/claim_label own the literal-cell discovery. Pay
        // their bounded potential matches before they allocate or scan.
        const auto labels = model.nodes.size() + (model.edges.front().label.empty() ? 0u : 1u);
        work.product(unicode.cells.size(), labels * (sizeof(flow::Point) + sizeof(flow::Label) + 32u));
        work.product(unicode.cells.size(), 4u);
        std::vector<bool> literal(unicode.cells.size(), false);
        const flow::Rect whole{-1, -1, unicode.width, unicode.height};
        const auto claim_exactly_one = [&](std::string_view text) {
            const auto blocks = flow::label_blocks(unicode, text, whole, work);
            if (blocks.size() != 1u) return false;
            flow::claim_label(unicode, text, blocks.front(), literal);
            return true;
        };
        for (const auto& node : model.nodes)
            if (!claim_exactly_one(node.label)) return {ProjectionState::Reject, "node literal ownership"};
        if (!model.edges.front().label.empty() && !claim_exactly_one(model.edges.front().label))
            return {ProjectionState::Reject, "edge literal ownership"};
        const int following = flow::flow400_following(unicode, work, kFollowing);
        for (int x = 0; x < unicode.width; ++x)
            literal[static_cast<std::size_t>(unicode.index({x, following}))] = true;
        work.product(unicode.cells.size(), 3u);
        for (std::size_t cell = 0u; cell < unicode.cells.size(); ++cell) {
            if (literal[cell]) {
                if (ascii.cells[cell] != unicode.cells[cell]) return {ProjectionState::Reject, "literal projection"};
                continue;
            }
            const auto expected = expected_ascii_geometry(unicode.cells[cell]);
            if (!expected || ascii.cells[cell] != *expected) return {ProjectionState::Reject, "geometry projection"};
        }
        return {ProjectionState::Accept, "exact projection"};
    } catch (const std::exception& error) {
        return flow::flow400_resource(error.what()) ? ProjectionResult{ProjectionState::Resource, "oracle resource"} :
            ProjectionResult{ProjectionState::Exception, "oracle exception"};
    }
}

void require_valid_projection(const Grid& unicode, const Grid& ascii, const Model& model, OracleWork& work) {
    const auto result = validate_public_ascii_projection(unicode, ascii, model, work);
    require(result.state != ProjectionState::Resource && result.state != ProjectionState::Exception,
            "ASCII Flow projection validator became resource/exception");
    require(result.state == ProjectionState::Accept, "ASCII Flow public ASCII projection differs from owned Unicode geometry");
}

void require_projection_mutants(const Grid& unicode, const Grid& ascii, const Model& model, OracleWork& work) {
    require_valid_projection(unicode, ascii, model, work);
    const auto mutate_and_reject = [&](auto predicate, std::string_view replacement, std::string_view name) {
        flow::flow400_copy_grid(ascii, work);
        work.product(ascii.cells.size(), 3u);
        auto mutation = ascii;
        bool changed = false;
        for (auto& cell : mutation.cells) if (!changed && predicate(cell)) {
            cell = std::string(replacement); changed = true;
        }
        require(changed, name);
        const auto result = validate_public_ascii_projection(unicode, mutation, model, work);
        require(result.state != ProjectionState::Resource && result.state != ProjectionState::Exception,
                "ASCII Flow projection negative became resource/exception");
        require(result.state == ProjectionState::Reject, name);
    };
    mutate_and_reject([](std::string_view cell) { return cell == "+"; }, " ",
                      "ASCII Flow geometry mutation admitted");
    mutate_and_reject([](std::string_view cell) { return cell == "-" || cell == "|"; }, " ",
                      "ASCII Flow path mutation admitted");
    mutate_and_reject([](std::string_view cell) { return cell == ">"; }, " ",
                      "ASCII Flow head mutation admitted");
}

bool model_has_wide_literal(const Model& model, OracleWork& work) {
    const auto inspect = [&](std::string_view text) {
        work.product(text.size(), sizeof(std::string) + 2u * sizeof(std::string) + 32u);
        const auto glyphs = ftxui::Utf8ToGlyphs(std::string(text));
        return std::ranges::any_of(glyphs, [](const std::string& glyph) { return glyph.empty(); });
    };
    for (const auto& node : model.nodes) if (inspect(node.label)) return true;
    return inspect(model.edges.front().label);
}

void require_native_continuation_mutant(const Grid& native, const Grid& ascii, const Model& model, OracleWork& work) {
    require_valid_projection(native, ascii, model, work);
    flow::flow400_copy_grid(native, work);
    work.product(native.cells.size(), 3u);
    auto mutation = native;
    bool changed = false;
    for (int y = 0; y < mutation.height && !changed; ++y) for (int x = 0; x + 1 < mutation.width; ++x) {
        const auto current = mutation.at({x, y});
        if (!current.empty() && ftxui::string_width(std::string(current)) == 2 && mutation.at({x + 1, y}).empty()) {
            mutation.put({x + 1, y}, " "); changed = true;
        }
    }
    require(changed, "ASCII Flow CJK source requires an actual wide continuation");
    const auto result = validate_public_ascii_projection(mutation, ascii, model, work);
    require(result.state != ProjectionState::Resource && result.state != ProjectionState::Exception,
            "ASCII Flow continuation negative became resource/exception");
    require(result.state == ProjectionState::Reject, "ASCII Flow continuation mutation admitted");
}

void require_bad_canonical_row_rejected(const std::vector<std::string>& rows, std::string_view helper_name,
                                        OracleWork& work) {
    require_original_rectangle(rows, helper_name);
    std::size_t bytes = 0u;
    for (const auto& row : rows) bytes += row.size();
    work.product(rows.size(), 3u * sizeof(std::string) + 16u);
    work.product(bytes + 1u, 4u);
    auto malformed = rows;
    malformed.back().push_back('!');
    require(!original_rectangle_accepts(malformed, helper_name),
            "ASCII Flow malformed canonical row admitted by original rectangular helper");
}

void current_unicode_model_checks(const Grid& unicode, const Model& model, OracleWork& work) {
    const auto errors = flow::flow400_geometry(unicode, model, work, kFollowing);
    EXPECT_TRUE(errors.empty()) << "existing Unicode Flow geometry changed";
    if (errors.empty()) EXPECT_EQ(flow::flow400_mutations(unicode, model, work, kFollowing), 3u)
        << "existing Unicode Flow mutations";
}

struct Frames { Grid ascii_wide, ascii_narrow, ascii_restored, unicode_wide, unicode_narrow, text_wide, text_narrow; };
Frames capture_seven(std::string_view source, OracleWork& work) {
    // All temporary source/fence strings and public elements are paid before construction.
    work.product(source.size() + 2u * kFollowing.size() + 128u, 32u * sizeof(std::string) + 256u);
    work.spend(16384u);
    auto text = loom::ui::render_markdown(fenced(source, "text"));
    auto ascii = loom::ui::mermaid::render_mermaid(source, text, true);
    auto unicode = loom::ui::mermaid::render_mermaid(source, text);
    const auto ascii_body = ascii ? *ascii : text;
    const auto unicode_body = unicode ? *unicode : text;
    Frames frames{capture_public(with_following(ascii_body), kWide, work),
                  capture_public(with_following(ascii_body), kNarrow, work),
                  capture_public(with_following(ascii_body), kWide, work),
                  capture_public(with_following(unicode_body), kWide, work),
                  capture_public(with_following(unicode_body), kNarrow, work),
                  capture_public(with_following(text), kWide, work),
                  capture_public(with_following(text), kNarrow, work)};
    require_following(frames.ascii_wide, work); require_following(frames.ascii_narrow, work);
    require_following(frames.ascii_restored, work); require_following(frames.unicode_wide, work);
    require_following(frames.unicode_narrow, work); require_following(frames.text_wide, work);
    require_following(frames.text_narrow, work);
    return frames;
}

TEST(MermaidFlowAsciiGrid18, FourOriginalInputsAreMandatoryPublicFirstRed) {
    OracleWork asset_work{8000000u};
    const auto root_path = fs::path(LOOM_TESTS_DIR);
    const auto asset_bytes = read_exact(root_path / kAsset, asset_work);
    const auto parent_bytes = read_exact(root_path / kParentAsset, asset_work);
    asset_work.product(asset_bytes.size() + parent_bytes.size(), 12u);
    const auto asset_document = json::parse(asset_bytes);
    const auto parent_document = json::parse(parent_bytes);
    require(asset_document.has_value() && parent_document.has_value(), "ASCII Flow immutable JSON");
    const auto root = asset_document->root();
    const auto parent = parent_document->root();
    flow::flow400_keys(root, {"schema", "status", "parent_grid18_asset", "source_design", "scope", "cases", "controls"});
    flow::flow400_keys(parent, {"schema", "status", "scope", "mapping_contract", "helpers", "functions", "cases", "source_case_ledger"});
    require(flow::flow400_text(root.get("schema")) == "loom.flow_ascii_rows_source_subset/v1" &&
                flow::flow400_text(root.get("status")) == "test_first_pending_producer" && digest(parent_bytes) == kParentSha,
            "ASCII Flow asset schema and parent SHA");
    const auto parent_ref = root.get("parent_grid18_asset");
    flow::flow400_keys(parent_ref, {"path", "sha256", "bytes"});
    require(flow::flow400_text(parent_ref.get("path")) == kParentAsset &&
                flow::flow400_text(parent_ref.get("sha256")) == kParentSha &&
                flow::flow400_number(parent_ref.get("bytes")) == parent_bytes.size(), "ASCII Flow copied parent asset");
    const auto scope = root.get("scope");
    flow::flow400_keys(scope, {"original_functions", "direct_assertion_sites", "runtime_inputs",
                               "remaining_original_functions", "remaining_direct_assertion_sites", "remaining_runtime_inputs"});
    require(flow::flow400_number(scope.get("original_functions")) == 2u &&
                flow::flow400_number(scope.get("direct_assertion_sites")) == 2u &&
                flow::flow400_number(scope.get("runtime_inputs")) == 4u &&
                flow::flow400_number(scope.get("remaining_original_functions")) == 16u &&
                flow::flow400_number(scope.get("remaining_direct_assertion_sites")) == 42u &&
                flow::flow400_number(scope.get("remaining_runtime_inputs")) == 55u,
            "ASCII Flow honest partial denominator");
    const auto cases = root.get("cases");
    require(cases.is_arr() && cases.size() == 4u, "ASCII Flow selected source count");

    std::map<std::string, std::set<std::string>> function_cases;
    std::set<std::string> assertion_sites, case_ids;
    std::size_t source_bound = 0u, art = 0u, expanded_predicates = 0u, continuation_controls = 0u;
    cases.iter([&](JsonVal item) {
        OracleWork work{flow::kFlow400SceneLimit};
        const auto input = flow::flow400_text(item.get("input"));
        const auto id = std::string(flow::flow400_text(item.get("case_id")));
        SCOPED_TRACE(id);
        const auto frames = capture_seven(input, work);  // Seven public captures before every source/Model bind.
        EXPECT_EQ(frames.ascii_wide, frames.ascii_restored) << "ASCII Flow wide restore changed";
        EXPECT_NE(frames.unicode_wide, frames.text_wide) << "existing Unicode Flow art missing";
        std::vector<std::string> labels;
        std::string helper_name;
        // The seven public captures precede this bind, but source/helper/Model
        // identity always closes before a producer null return can short-circuit.
        const auto model = bind_original_after_captures(root_path, item, parent, work, function_cases,
                                                        assertion_sites, labels, helper_name);
        ++source_bound;
        EXPECT_TRUE(case_ids.emplace(id).second) << "duplicate selected source case";
        EXPECT_EQ(labels, (std::vector<std::string>{model.nodes[0].label, model.edges[0].label, model.nodes[1].label}))
            << "original assertion and complete Model differ";
        current_unicode_model_checks(frames.unicode_wide, model, work);
        const bool has_wide_literal = model_has_wide_literal(model, work);
        private_flow::Work producer_work;
        const auto rows = private_flow::flow_ascii_rows(input, producer_work);
        EXPECT_TRUE(rows.has_value()) << "closed ASCII Flow profile must expose canonical rows";
        if (!rows) return;
        require_projection_join(frames.unicode_wide, frames.unicode_narrow, frames.ascii_wide,
                                frames.ascii_narrow, *rows, work);
        const bool wide_art = frames.ascii_wide != frames.text_wide;
        const bool narrow_art = frames.ascii_narrow != frames.text_narrow;
        EXPECT_TRUE(wide_art) << "mandatory ASCII Flow source returned fallback";
        EXPECT_TRUE(narrow_art) << "mandatory narrow ASCII Flow source returned fallback";
        if (!wide_art || !narrow_art) return;  // No original predicate credit on fallback.
        ++art;
        const auto text = canonical_text(rows->ascii);
        for (const auto& label : labels) EXPECT_NE(text.find(label), std::string::npos) << "original contains predicate failed";
        require_bad_canonical_row_rejected(rows->ascii, helper_name, work);
        require_projection_mutants(frames.unicode_wide, frames.ascii_wide, model, work);
        if (has_wide_literal) {
            require_native_continuation_mutant(frames.unicode_wide, frames.ascii_wide, model, work);
            ++continuation_controls;
        }
        ++expanded_predicates;
    });
    OracleWork closure_work{8000000u};
    require_function_closure(parent, function_cases, closure_work);
    EXPECT_EQ(source_bound, 4u); EXPECT_EQ(case_ids.size(), 4u);
    EXPECT_EQ(assertion_sites.size(), 2u) << "two original direct assertion sites";
    EXPECT_EQ(art, 4u) << "no original predicate completion on fallback";
    EXPECT_EQ(expanded_predicates, 4u) << "one complete input application per source";
    EXPECT_EQ(continuation_controls, 1u) << "only the source-derived CJK/emoji Model has wide cells";
}

TEST(MermaidFlowAsciiGrid18, DottedAndThickRemainWholeSourceFallbackUntilImplemented) {
    OracleWork asset_work{8000000u};
    const auto root_path = fs::path(LOOM_TESTS_DIR);
    const auto bytes = read_exact(root_path / kAsset, asset_work);
    asset_work.product(bytes.size(), 12u);
    const auto document = json::parse(bytes);
    require(document.has_value(), "ASCII Flow style control JSON");
    const auto controls = document->root().get("controls");
    require(controls.is_arr() && controls.size() == 2u, "ASCII Flow style control count");
    std::set<std::string> styles;
    controls.iter([&](JsonVal item) {
        OracleWork work{flow::kFlow400SceneLimit};
        flow::flow400_keys(item, {"id", "input", "input_sha256", "input_utf8_bytes", "flow400_model_json",
                                  "flow400_model_sha256", "expectation"});
        const auto input = flow::flow400_text(item.get("input"));
        SCOPED_TRACE(flow::flow400_text(item.get("id")));
        const auto frames = capture_seven(input, work);
        require(input.size() == flow::flow400_number(item.get("input_utf8_bytes")) &&
                    digest(input) == flow::flow400_text(item.get("input_sha256")), "ASCII Flow style source SHA");
        const auto model_text = flow::flow400_text(item.get("flow400_model_json"));
        work.product(model_text.size(), 8u);
        require(digest(model_text) == flow::flow400_text(item.get("flow400_model_sha256")), "ASCII Flow style Model SHA");
        const auto parsed = json::parse(model_text);
        require(parsed.has_value(), "ASCII Flow style Model JSON");
        std::size_t groups = 0u, members = 0u; flow::flow400_shape(parsed->root(), groups, members);
        const auto model = flow::parse_model(parsed->root());
        require(model.direction == "LR" && model.nodes.size() == 2u && model.edges.size() == 1u &&
                    (model.edges.front().style == "dotted" || model.edges.front().style == "thick"),
                "ASCII Flow style Model profile");
        styles.emplace(model.edges.front().style);
        EXPECT_EQ(frames.ascii_wide, frames.ascii_restored) << "ASCII Flow style wide restore";
        EXPECT_NE(frames.unicode_wide, frames.text_wide) << "existing Unicode style art missing";
        current_unicode_model_checks(frames.unicode_wide, model, work);
        EXPECT_EQ(flow::flow400_text(item.get("expectation")), "explicit_ascii_whole_source_fallback");
        EXPECT_EQ(frames.ascii_wide, frames.text_wide) << "unimplemented ASCII style partially admitted";
        EXPECT_EQ(frames.ascii_narrow, frames.text_narrow) << "unimplemented ASCII style narrow fallback";
    });
    EXPECT_EQ(styles, (std::set<std::string>{"dotted", "thick"}));
}
}  // namespace
