// RFC 0030 Mindmap10: each public observation precedes source/Model binding.
#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>

import std;
import loom.ui.visual.markdown;
import loom.ui.visual.mermaid.testing;
import loom.serdes.json;
import loom.crypto.crypto;

#include "mermaid_mindmap10_authority.hpp"

namespace {
namespace mm = loom::test::mermaid_mindmap10;
namespace json = loom::utils::json;
namespace fs = std::filesystem;
constexpr std::string_view kSentinel = "AFTER_MINDMAP10";
const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures";
void need(bool yes, std::string_view reason) { if (!yes) throw std::runtime_error(std::string(reason)); }
std::string read(const fs::path& path) {
    need(fs::is_regular_file(path) && fs::file_size(path) <= 2'000'000u, "Mindmap native bounded file");
    std::ifstream stream(path, std::ios::binary);
    need(stream.good(), "Mindmap native file open");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
std::string hash(std::string_view text) { return loom::utils::crypto::sha256(text); }
std::string fence(std::string_view source, std::string_view language) {
    return "```" + std::string(language) + "\n" + std::string(source) + "\n```\n\n" + std::string(kSentinel);
}
struct Frame { mm::Grid grid; std::vector<std::string> foreground, background; };
Frame capture(ftxui::Element node, int width, int height, mm::Work& work) {
    need(width > 0 && width <= 240 && height > 0 && height <= 1040, "Mindmap native frame dimensions");
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    work.spend(count * (3u * sizeof(std::string) + 64u) + 512u);
    Frame frame; frame.grid.width = width; frame.grid.height = height;
    frame.grid.cells.reserve(count); frame.foreground.reserve(count); frame.background.reserve(count);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, node);
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const auto& cell = screen.PixelAt(x, y);
        frame.grid.cells.push_back(cell.character);
        frame.foreground.push_back(cell.foreground_color.Print(false));
        frame.background.push_back(cell.background_color.Print(true));
    }
    return frame;
}
bool same_surface(const Frame& a, const Frame& b) {
    return a.grid.width == b.grid.width && a.grid.height == b.grid.height &&
        a.grid.cells == b.grid.cells && a.background == b.background && a.foreground == b.foreground;
}
using Frames = std::array<Frame, 5>;
Frames observe(std::string_view source, mm::Work& work, std::string_view ordinary_language = "text") {
    auto node = loom::ui::render_markdown(fence(source, "mermaid"));
    auto plain = loom::ui::render_markdown(fence(source, ordinary_language));
    node->ComputeRequirement(); plain->ComputeRequirement();
    const int height = std::min(1040, std::max(40, std::max(node->requirement().min_y, plain->requirement().min_y) + 12));
    Frames frames;
    constexpr std::array<int, 5> widths{240, 40, 240, 240, 40};
    for (std::size_t n = 0; n < frames.size(); ++n)
        frames[n] = capture(n < 3 ? node : plain, widths[n], height, work);
    return frames;
}
void lifecycle(const Frames& frames) {
    need(same_surface(frames[0], frames[2]) && frames[0].foreground == frames[2].foreground,
         "Mindmap native same-node restoration");
}
std::string frame_bytes(const Frame& frame) {
    std::string value;
    for (std::size_t n = 0; n < frame.grid.cells.size(); ++n)
        for (const auto* part : {&frame.grid.cells[n], &frame.foreground[n], &frame.background[n]}) {
            value += std::to_string(part->size()); value += ':'; value += *part;
        }
    return value;
}
void publish(const Frame& frame, json::JsonMutVal item, mm::Work& work) {
    work.spend(frame.grid.cells.size() * 128u + 512u);
    item.set("width", frame.grid.width); item.set("height", frame.grid.height);
    item.set("cell_sha256", hash(frame_bytes(frame)));
    auto glyphs = item.make_arr();
    for (const auto& glyph : frame.grid.cells) glyphs.append(item.make_str(glyph));
    item.add("glyph_cells", glyphs);
    for (const auto& [name, colors] : std::array<std::pair<std::string_view, const std::vector<std::string>*>, 2>{
             {{"foreground_runs", &frame.foreground}, {"background_runs", &frame.background}}}) {
        auto runs = item.make_arr();
        for (std::size_t begin = 0; begin < colors->size();) {
            std::size_t end = begin + 1; while (end < colors->size() && (*colors)[end] == (*colors)[begin]) ++end;
            auto run = item.make_arr(); run.append(item.make_num(static_cast<int64_t>(end - begin)));
            run.append(item.make_str((*colors)[begin])); runs.append(run); begin = end;
        }
        item.add(name, runs);
    }
}
void assert_geometry(const Frames& frames, const mm::Model& model, mm::Work& work) {
    lifecycle(frames);
    need(!same_surface(frames[0], frames[3]), "Mindmap mandatory complete original remains source");
    for (const auto index : {0u, 1u, 2u}) {
        if (index == 1u && same_surface(frames[1], frames[4])) continue;
        const auto errors = mm::validate(frames[index].grid, model, kSentinel, work);
        for (const auto& error : errors) ADD_FAILURE() << error;
        need(errors.empty(), "Mindmap native complete geometry");
    }
}
}

TEST(MermaidMindmap10, CompleteOriginalsOwnGeometryAndGoodFirstMutations) {
    const auto raw_facts = read(kFixtures / std::string(mm::kFactsRelativePath));
    ASSERT_EQ(hash(raw_facts), mm::kFactsSha);
    const auto manifest_raw = read(kFixtures / "mermaid_official/manifest.json");
    ASSERT_EQ(hash(manifest_raw), "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b");
    auto facts = json::parse(raw_facts), manifest = json::parse(manifest_raw);
    ASSERT_TRUE(facts && manifest);
    const auto cases = facts->root().get("cases"); ASSERT_EQ(cases.size(), 10u);
    json::JsonMutDoc report; auto root = report.object(), records = root.make_arr();
    int admitted = 0, errors = 0; std::size_t semantic = 0, physical = 0;
    for (std::size_t n = 0; n < cases.size(); ++n) {
        const auto row = cases.at(n); const auto path = row.get_string("path"); SCOPED_TRACE(path);
        mm::Work work; auto record = root.make_obj(); record.set("path", path);
        try {
            const auto source = read(kFixtures / "mermaid_official/cases" / path);
            const auto frames = observe(source, work);
            auto recorded = record.make_arr();
            for (const auto& frame : frames) { auto entry = record.make_obj(); publish(frame, entry, work); recorded.append(entry); }
            record.add("five_frames", recorded); record.set("captures_before_binding", true);
            const auto model = mm::bind(facts->root(), row, source, manifest->root(), work);
            record.set("captures", 5); record.set("bound_after_captures", true);
            record.set("source_sha256", hash(source)); record.set("complete_model_sha256", row.get_string("complete_model_sha256"));
            record.set("terminal_projection_sha256", row.get_string("terminal_projection_sha256"));
            assert_geometry(frames, model, work);
            record.set("geometry_and_restoration_good", true);
            record.set("narrow_exact_ordinary", same_surface(frames[1], frames[4]));
            record.set("narrow_geometry_good", !same_surface(frames[1], frames[4]));
            const auto result = mm::exercise(frames[0].grid, model, row.get("required_good_first_mutations"), kSentinel, work);
            for (const auto& error : result.errors) ADD_FAILURE() << error;
            need(result.errors.empty(), "Mindmap native meaningful mutation proof");
            semantic += result.semantic; physical += result.physical; ++admitted;
            record.set("semantic_mutations_rejected", static_cast<int64_t>(result.semantic));
            record.set("physical_mutations_rejected", static_cast<int64_t>(result.physical));
            record.set("narrow_source_fallback", same_surface(frames[1], frames[4])); record.set("local_ready", true);
        } catch (const std::exception& error) { ++errors; record.set("error", error.what()); ADD_FAILURE() << error.what(); }
        record.set("work_remaining", static_cast<int64_t>(work.left)); records.append(record);
    }
    root.set("schema", "mindmap10_native_source_scenes/v1"); root.set("source_facts_sha256", mm::kFactsSha);
    root.set("originals", 10); root.set("local_ready", admitted); root.set("errors", errors);
    root.set("semantic_mutations_rejected", static_cast<int64_t>(semantic)); root.set("physical_mutations_rejected", static_cast<int64_t>(physical));
    root.set("foreign_assertions_certified", 0); root.add("cases", records); report.set_root(root);
    // The cross-consumer verifier reads only this compact verdict, never frames.
    auto compact = json::parse(report.to_string()); ASSERT_TRUE(compact);
    json::JsonMutDoc verdict; auto summary = verdict.object();
    for (const auto key : {"schema", "source_facts_sha256", "originals", "local_ready", "errors",
         "semantic_mutations_rejected", "physical_mutations_rejected", "foreign_assertions_certified"})
        summary.add(key, verdict.copy_val(compact->root().get(key)));
    auto metadata = summary.make_arr();
    for (std::size_t n = 0; n < compact->root().get("cases").size(); ++n) {
        const auto entry = compact->root().get("cases").at(n); auto value = summary.make_obj();
        for (const auto key : {"path", "source_sha256", "complete_model_sha256", "terminal_projection_sha256", "captures",
             "captures_before_binding", "bound_after_captures", "geometry_and_restoration_good", "narrow_exact_ordinary", "narrow_geometry_good",
             "semantic_mutations_rejected", "physical_mutations_rejected", "local_ready", "error"})
            if (entry.has(key)) value.add(key, verdict.copy_val(entry.get(key)));
        metadata.append(value);
    }
    summary.add("cases", metadata); verdict.set_root(summary);
    std::ofstream verdict_out(fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-mindmap10-native-verdict.json", std::ios::binary);
    verdict_out << verdict.to_string(); ASSERT_TRUE(verdict_out.good());
    std::ofstream out(fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-mindmap10-native.json", std::ios::binary);
    out << report.to_string(); ASSERT_TRUE(out.good());
    EXPECT_EQ(admitted, 10); EXPECT_EQ(errors, 0); EXPECT_EQ(semantic + physical, 60u);
}

TEST(MermaidMindmap10, UnsupportedGrammarAndResourceBoundariesKeepWholeSource) {
    std::vector<std::string> sources{
        "mindmap\nroot\nsecond root\n", "mindmap\nroot\n\tchild\n", "mindmap\nroot((circle)]\n",
        "mindmap\nroot\n  child::icon(home)\n", "mindmap\nroot\n  child\n    grandchild\n   odd\n",
        "---\nconfig: {}\n---\nmindmap\nroot\n", "---\ntitle: hidden\n---\nmindmap\nroot\n",
        "---\ndescription: good\ndescription: duplicate\n---\nmindmap\nroot\n",
        "---\ndescription: |\n  literal\n---\nmindmap\nroot\n", "mindmap\nroot[bad  spacing]\n",
        "mindmap\nroot[box]\n child{{hex}}\n", "mindmap\nroot\rbroken\n", "mindmap\nroot\n  <b>html</b>\n",
        "mindmap\nroot\n  `markdown`\n", "mindmap\nroot[" + std::string(257, 'x') + "]\n",
        "mindmap\nroot\n  " + std::string("\xcc\x81") + "bad\n", "mindmap\nroot\n  " + std::string("\xff") + "\n"};
    std::string many = "mindmap\nroot\n";
    for (int n = 0; n < 64; ++n) many += "  child" + std::to_string(n) + "\n";
    sources.push_back(many);
    std::string deep = "mindmap\n";
    for (int n = 0; n < 17; ++n) deep += std::string(static_cast<std::size_t>(n), ' ') + "node\n";
    sources.push_back(deep);
    std::string budget = "mindmap\nroot\n";
    for (int n = 0; n < 63; ++n) budget += "  " + std::string(200, 'x') + "\n";
    sources.push_back(budget); sources.push_back("mindmap\n" + std::string(65536, 'x'));
    std::string byte_overflow = "a"; for (int n = 0; n < 512; ++n) byte_overflow += "\xcc\x81";
    ASSERT_EQ(byte_overflow.size(), 1025u); sources.push_back("mindmap\n" + byte_overflow + "\n");
    for (const auto& source : sources) {
        SCOPED_TRACE(source.substr(0, 120)); mm::Work work;
        const auto frames = observe(source, work); lifecycle(frames);
        EXPECT_TRUE(same_surface(frames[0], frames[3])); EXPECT_TRUE(same_surface(frames[1], frames[4]));
    }
}

TEST(MermaidMindmap10, SourceIdentityModelProjectionAndWitnessGuardsRejectDrift) {
    const auto raw = read(kFixtures / std::string(mm::kFactsRelativePath)); ASSERT_EQ(hash(raw), mm::kFactsSha);
    auto facts = json::parse(raw), manifest = json::parse(read(kFixtures / "mermaid_official/manifest.json"));
    ASSERT_TRUE(facts && manifest);
    const auto row = facts->root().get("cases").at(0);
    const auto source = read(kFixtures / "mermaid_official/cases" / row.get_string("path"));
    mm::Work baseline; const auto frames = observe(source, baseline);
    const auto model = mm::bind(facts->root(), row, source, manifest->root(), baseline);
    assert_geometry(frames, model, baseline);
    const auto reject = [&](auto mutate, std::string_view guard) {
        json::JsonMutDoc altered; auto root = altered.copy_val(facts->root()); altered.set_root(root);
        mutate(root.get("cases").at(0));
        auto document = json::parse(altered.to_string()); ASSERT_TRUE(document);
        mm::Work work; bool rejected = false;
        try { static_cast<void>(mm::bind(document->root(), document->root().get("cases").at(0), source, manifest->root(), work)); }
        catch (const std::exception& error) {
            const std::string_view message = error.what();
            EXPECT_FALSE(message.starts_with("resource:")); EXPECT_NE(message.find(guard), std::string_view::npos) << message;
            rejected = true;
        }
        EXPECT_TRUE(rejected) << guard;
    };
    reject([](auto value) { value.set("manifest_global_ordinal", value.get("manifest_global_ordinal").as_int() + 1); }, "manifest global ordinal");
    reject([](auto value) { value.set("source_bytes", value.get("source_bytes").as_int() + 1); }, "complete byte count");
    reject([](auto value) { value.set("source_sha256", std::string(64, '0')); }, "raw SHA-256");
    reject([](auto value) { value.set("source_git_blob", std::string(40, '0')); }, "Git blob identity");
    reject([](auto value) { value.set("complete_model_sha256", std::string(64, '0')); }, "complete canonical Model");
    reject([](auto value) { value.set("terminal_projection_sha256", std::string(64, '0')); }, "complete canonical projection");
    reject([](auto value) { auto witness = value.get("source_witnesses").at(0); witness.set("source_byte_end", witness.get("source_byte_end").as_int() + 1); }, "exact original byte lexeme");
    reject([](auto value) { value.get("source_witnesses").at(0).set("authored_id", "wrong"); }, "five-tuple");
    reject([](auto value) { value.get("required_good_first_mutations").at(0).set("node_ordinal", 1); }, "exact frozen whole row");
    mm::Work exhausted; exhausted.left = 0;
    try { static_cast<void>(mm::validate(frames[0].grid, model, kSentinel, exhausted)); FAIL() << "resource exhaustion must escape"; }
    catch (const std::exception& error) { EXPECT_TRUE(std::string_view(error.what()).starts_with("resource:")); }
}

TEST(MermaidMindmap10, UnicodeDuplicateCaptionsAndLimitsAreSeparateNonScoringControls) {
    const auto node = [](int id, int parent, std::string caption) {
        return mm::Node{id, parent, caption, "default", std::nullopt, {caption}, {}};
    };
    std::vector<std::pair<std::string, mm::Model>> controls;
    controls.push_back({"mindmap\n根\n  東京\n  cafe\xcc\x81\n", {{node(0, -1, "根"), node(1, 0, "東京"), node(2, 0, "cafe\xcc\x81")}}});
    controls.push_back({"mindmap\nroot\n  same\n    left\n  same\n    right\n", {{node(0, -1, "root"), node(1, 0, "same"), node(2, 1, "left"), node(3, 0, "same"), node(4, 3, "right")}}});
    std::string star = "mindmap\nroot\n"; mm::Model star_model{{node(0, -1, "root")}};
    for (int n = 1; n < 64; ++n) { auto label = "node" + std::to_string(n); star += "  " + label + "\n"; star_model.nodes.push_back(node(n, 0, label)); }
    controls.push_back({star, star_model});
    std::string byte_boundary = "ab"; for (int n = 0; n < 511; ++n) byte_boundary += "\xcc\x81";
    ASSERT_EQ(byte_boundary.size(), 1024u);
    controls.push_back({"mindmap\n" + byte_boundary + "\n", {{node(0, -1, byte_boundary)}}});
    std::string wide_row; for (int n = 0; n < 16; ++n) wide_row += "界";
    const std::string combined_row = wide_row + "\xcc\x81";
    const auto long_caption = combined_row + "界";
    controls.push_back({"mindmap\nroot[" + long_caption + "]\n",
        {{{0, -1, long_caption, "square", std::string("root"), {combined_row, "界"}, {""}}}}});
    const std::string boundary(256, 'x');
    controls.push_back({"mindmap\nroot[" + boundary + "]\n",
        {{{0, -1, boundary, "square", std::string("root"), std::vector<std::string>(8, std::string(32, 'x')), std::vector<std::string>(7, "")}}}});
    for (const auto& [source, model] : controls) { mm::Work work; work.left = 256'000'000u; const auto frames = observe(source, work); assert_geometry(frames, model, work); }
    const auto source = "mindmap\nroot\n  child\n";
    auto fallback = loom::ui::render_markdown(fence(source, "text"));
    EXPECT_FALSE(loom::ui::mermaid::render_mermaid(source, fallback, true).has_value());
}

TEST(MermaidMindmap10, ForegroundOnlyMutationCannotCountAsOrdinaryFallback) {
    mm::Work work; const auto frames = observe("mindmap\nroot\n", work);
    auto changed = frames[3]; ASSERT_FALSE(changed.foreground.empty());
    changed.foreground[0] += "changed";
    EXPECT_EQ(changed.grid.cells, frames[3].grid.cells); EXPECT_EQ(changed.background, frames[3].background);
    EXPECT_FALSE(same_surface(changed, frames[3]));
}

// Shapes4 owns fresh public observations; it never consumes another report.
namespace shapes = loom::test::mermaid_shapes4;
TEST(MermaidMindmapShapes4, CompleteOriginalsOwnGeometryAndGoodFirstMutations) {
    const auto raw_facts = read(kFixtures / std::string(shapes::kFactsRelativePath));
    ASSERT_EQ(hash(raw_facts), shapes::kFactsSha);
    const auto manifest_raw = read(kFixtures / "mermaid_official/manifest.json");
    ASSERT_EQ(hash(manifest_raw), "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b");
    auto facts = json::parse(raw_facts), manifest = json::parse(manifest_raw);
    ASSERT_TRUE(facts && manifest);
    const auto cases = facts->root().get("cases"); ASSERT_EQ(cases.size(), 5u);
    json::JsonMutDoc report; auto root = report.object(), records = root.make_arr();
    int admitted = 0, errors = 0; std::size_t semantic = 0, physical = 0;
    for (std::size_t n = 0; n < cases.size(); ++n) {
        const auto row = cases.at(n);
        if (!row.has("terminal_projection_sha256")) continue; // Bang/Cloud remains excluded.
        const auto path = row.get_string("path"); SCOPED_TRACE(path);
        shapes::Work work; auto record = root.make_obj(); record.set("path", path);
        try {
            const auto source = read(kFixtures / "mermaid_official/cases" / path);
            const auto frames = observe(source, work, "mermaid-source");
            auto recorded = record.make_arr();
            for (const auto& frame : frames) { auto entry = record.make_obj(); publish(frame, entry, work); recorded.append(entry); }
            record.add("five_frames", recorded); record.set("captures_before_binding", true);
            const auto model = shapes::bind(facts->root(), row, source, manifest->root(), work);
            record.set("captures", 5); record.set("bound_after_captures", true);
            record.set("source_sha256", hash(source)); record.set("complete_model_sha256", row.get_string("complete_model_sha256"));
            record.set("terminal_projection_sha256", row.get_string("terminal_projection_sha256"));
            lifecycle(frames);
            need(!same_surface(frames[0], frames[3]), "Shapes4 complete original remains source");
            for (const auto index : {0u, 1u, 2u}) {
                if (index == 1u && same_surface(frames[1], frames[4])) continue;
                const auto geometry = shapes::validate(frames[index].grid, model, kSentinel, work);
                for (const auto& error : geometry) ADD_FAILURE() << error;
                need(geometry.empty(), "Shapes4 complete native geometry");
            }
            record.set("geometry_and_restoration_good", true);
            record.set("narrow_exact_ordinary", same_surface(frames[1], frames[4]));
            record.set("narrow_geometry_good", !same_surface(frames[1], frames[4]));
            const auto result = shapes::exercise(frames[0].grid, model, kSentinel, work);
            for (const auto& error : result.errors) ADD_FAILURE() << error;
            need(result.errors.empty(), "Mindmap native meaningful mutation proof");
            semantic += result.semantic; physical += result.physical; ++admitted;
            record.set("semantic_mutations_rejected", static_cast<int64_t>(result.semantic));
            record.set("physical_mutations_rejected", static_cast<int64_t>(result.physical));
            record.set("narrow_source_fallback", same_surface(frames[1], frames[4])); record.set("local_ready", true);
        } catch (const std::exception& error) { ++errors; record.set("error", error.what()); ADD_FAILURE() << error.what(); }
        record.set("work_remaining", static_cast<int64_t>(work.left)); records.append(record);
    }
    root.set("schema", "mindmap_shapes4_native_source_scenes/v1"); root.set("source_facts_sha256", shapes::kFactsSha);
    root.set("originals", 4); root.set("local_ready", admitted); root.set("errors", errors);
    root.set("semantic_mutations_rejected", static_cast<int64_t>(semantic)); root.set("physical_mutations_rejected", static_cast<int64_t>(physical));
    root.set("foreign_assertions_certified", 0); root.add("cases", records); report.set_root(root);
    // The cross-consumer verifier reads only this compact verdict, never frames.
    auto compact = json::parse(report.to_string()); ASSERT_TRUE(compact);
    json::JsonMutDoc verdict; auto summary = verdict.object();
    for (const auto key : {"schema", "source_facts_sha256", "originals", "local_ready", "errors",
         "semantic_mutations_rejected", "physical_mutations_rejected", "foreign_assertions_certified"})
        summary.add(key, verdict.copy_val(compact->root().get(key)));
    auto metadata = summary.make_arr();
    for (std::size_t n = 0; n < compact->root().get("cases").size(); ++n) {
        const auto entry = compact->root().get("cases").at(n); auto value = summary.make_obj();
        for (const auto key : {"path", "source_sha256", "complete_model_sha256", "terminal_projection_sha256", "captures",
             "captures_before_binding", "bound_after_captures", "geometry_and_restoration_good", "narrow_exact_ordinary", "narrow_geometry_good",
             "semantic_mutations_rejected", "physical_mutations_rejected", "local_ready", "error"})
            if (entry.has(key)) value.add(key, verdict.copy_val(entry.get(key)));
        metadata.append(value);
    }
    summary.add("cases", metadata); verdict.set_root(summary);
    std::ofstream verdict_out(fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-shapes4-native-verdict.json", std::ios::binary);
    verdict_out << verdict.to_string(); ASSERT_TRUE(verdict_out.good());
    std::ofstream out(fs::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-shapes4-native.json", std::ios::binary);
    out << report.to_string(); ASSERT_TRUE(out.good());
    EXPECT_EQ(admitted, 4); EXPECT_EQ(errors, 0); EXPECT_EQ(semantic, 11u); EXPECT_EQ(physical, 16u);
}

TEST(MermaidMindmapShapes4, OrdinaryControlKeepsCallerSourceHighlighting) {
    // Unknown Mermaid source uses the existing m-prefix C-family highlighter.
    // An ordinary non-diagram alias preserves that source rendering; plain text
    // is not its foreground-equivalent control. Keep Original10 captures intact.
    const std::string source = "---\ndescription: while preserving complete source\n---\nmindmap\nroot{{unsupported}}\n";
    mm::Work work;
    const auto frames = observe(source, work, "mermaid-source");
    lifecycle(frames);
    EXPECT_TRUE(same_surface(frames[0], frames[3]));
    EXPECT_TRUE(same_surface(frames[1], frames[4]));
    auto changed = frames[4];
    ASSERT_FALSE(changed.foreground.empty()); changed.foreground[0] += "changed";
    EXPECT_FALSE(same_surface(changed, frames[4]));
}

TEST(MermaidMindmapShapes4, NonScoringPortsMixedTreesFramingAndUnicode) {
    const auto node = [](int ordinal, int parent, std::string shape, std::string caption) {
        shapes::Node n; n.ordinal = ordinal; n.parent = parent; n.shape = std::move(shape);
        n.caption = caption; n.display_caption = std::move(caption);
        if (n.shape != "default") n.authored_id = "node" + std::to_string(ordinal);
        mm::Work work;
        auto projection = shapes::detail::project(n.display_caption, n.shape == "default", work);
        n.rows = std::move(projection.rows); n.joiners = std::move(projection.joiners);
        return n;
    };
    std::vector<shapes::Model> controls;
    for (const auto shape : {"square", "rounded", "circle"}) {
        controls.push_back({{node(0, -1, shape, "parent"), node(1, 0, "default", "child")}});
        controls.push_back({{node(0, -1, "default", "parent"), node(1, 0, shape, "child")}});
    }
    const std::string long_caption = "A node with an icon and with a long text that wraps to keep the node size in check";
    controls.push_back({{node(0,-1,"square","root"), node(1,0,"rounded",long_caption),
        node(2,1,"circle","東京 cafe\xcc\x81"), node(3,0,"circle","sibling"), node(4,3,"square","last")}});
    shapes::Model deep;
    const std::array<std::string_view,4> kinds{"square", "rounded", "circle", "default"};
    for (int n = 0; n < 6; ++n) deep.nodes.push_back(node(n, n-1, std::string(kinds[static_cast<std::size_t>(n)%4]), "x"));
    controls.push_back(std::move(deep));
    // A small outlined root plus default descendants exercises the actual
    // 16-level grammar within the unchanged production allocation budget.
    shapes::Model deep_boundary;
    for(int n=0;n<16;++n) deep_boundary.nodes.push_back(node(n,n-1,n==0?"square":"default","x"));
    controls.push_back(std::move(deep_boundary));
    auto wide = node(0,-1,"rounded",std::string(31,'x') + "界\xcc\x81界");
    wide.rows = {std::string(31,'x'), "界\xcc\x81界"}; wide.joiners = {""};
    controls.push_back({{wide}});
    for (const auto shape : {"square", "rounded", "circle"}) {
        auto root = node(0,-1,shape,"The root");
        root.raw_caption_prefix = "\r\n      "; root.raw_caption_suffix = "\r\n    ";
        root.caption = root.raw_caption_prefix + root.display_caption + root.raw_caption_suffix;
        controls.push_back({{root}});
    }
    for (std::size_t index=0; index<controls.size(); ++index) {
        SCOPED_TRACE(index); const auto& model=controls[index];
        std::string source="mindmap\n";
        std::array<int,64> depths{};
        for (std::size_t i=0; i<model.nodes.size(); ++i) {
            const auto& n=model.nodes[i];
            if(i) depths[i]=depths[static_cast<std::size_t>(n.parent)]+1;
            source += std::string(static_cast<std::size_t>(depths[i])*2,' ');
            const auto [open,close]=shapes::detail::delimiters(n.shape);
            source += n.authored_id.value_or("") + std::string(open) + n.caption + std::string(close) + "\n";
        }
        mm::Work work; work.left=256'000'000u;
        const auto frames=observe(source,work,"mermaid-source");
        lifecycle(frames);
        const auto valid=shapes::validate_model(model,work);
        for(const auto& error:valid) ADD_FAILURE()<<error;
        ASSERT_TRUE(valid.empty());
        ASSERT_FALSE(same_surface(frames[0],frames[3]));
        for(const auto frame:{0u,1u,2u}) {
            if(frame==1u && same_surface(frames[1],frames[4])) continue;
            const auto errors=shapes::validate(frames[frame].grid,model,kSentinel,work);
            for(const auto& error:errors) ADD_FAILURE()<<error;
            ASSERT_TRUE(errors.empty());
        }
        // Small mixed/port controls exercise every declared local mutant.
        // The deep chain instead cuts its actual first/last connector after
        // a good full-cell check, without inventing upstream assertion credit.
        if(model.nodes.size()<=5u) {
            const auto proof=shapes::exercise(frames[0].grid,model,kSentinel,work);
            for(const auto& error:proof.errors) ADD_FAILURE()<<error;
            EXPECT_TRUE(proof.errors.empty());
        } else {
            const auto scene=shapes::detail::inspect(frames[0].grid,model,kSentinel,work);
            ASSERT_FALSE(scene.cuts.empty());
            for(const auto p:{scene.cuts.front(),scene.cuts.back()}) {
                const auto& grid=frames[0].grid;
                work.spend(grid.cells.size()*(sizeof(std::string)+32u)+4096u);
                for(const auto& cell:grid.cells) work.spend(cell.size()*2u);
                auto changed=grid;
                changed.cells[static_cast<std::size_t>(p.y)*static_cast<std::size_t>(grid.width)+static_cast<std::size_t>(p.x)]=" ";
                EXPECT_FALSE(shapes::validate(changed,model,kSentinel,work).empty());
            }
        }
    }
}

TEST(MermaidMindmapShapes4, SourceIdentityModelProjectionAndWitnessGuardsRejectDrift) {
    const auto raw = read(kFixtures / std::string(shapes::kFactsRelativePath)); ASSERT_EQ(hash(raw), shapes::kFactsSha);
    auto facts = json::parse(raw), manifest = json::parse(read(kFixtures / "mermaid_official/manifest.json"));
    ASSERT_TRUE(facts && manifest);
    const auto row = facts->root().get("cases").at(1);
    const auto source = read(kFixtures / "mermaid_official/cases" / row.get_string("path"));
    shapes::Work baseline; const auto frames = observe(source, baseline, "mermaid-source");
    const auto model = shapes::bind(facts->root(), row, source, manifest->root(), baseline);
    ASSERT_TRUE(shapes::validate(frames[0].grid, model, kSentinel, baseline).empty());
    const auto reject = [&](auto mutate, std::string_view guard) {
        json::JsonMutDoc altered; auto root = altered.copy_val(facts->root()); altered.set_root(root);
        mutate(root.get("cases").at(1));
        auto document = json::parse(altered.to_string()); ASSERT_TRUE(document);
        shapes::Work work; bool rejected = false;
        try { static_cast<void>(shapes::bind(document->root(), document->root().get("cases").at(1), source, manifest->root(), work)); }
        catch (const std::exception& error) {
            const std::string_view message = error.what();
            EXPECT_FALSE(message.starts_with("resource:")); EXPECT_NE(message.find(guard), std::string_view::npos) << message;
            rejected = true;
        }
        EXPECT_TRUE(rejected) << guard;
    };
    reject([](auto value) { value.set("manifest_global_ordinal", value.get("manifest_global_ordinal").as_int() + 1); }, "exact global ordinal");
    reject([](auto value) { value.set("source_bytes", value.get("source_bytes").as_int() + 1); }, "complete raw byte count");
    reject([](auto value) { value.set("source_sha256", std::string(64, '0')); }, "raw SHA/Git identity");
    reject([](auto value) { value.set("source_git_blob", std::string(40, '0')); }, "raw SHA/Git identity");
    reject([](auto value) { value.set("complete_model_sha256", std::string(64, '0')); }, "frozen raw Model");
    reject([](auto value) { value.set("terminal_projection_sha256", std::string(64, '0')); }, "frozen display/framing projection");
    reject([](auto value) { auto witness = value.get("source_witnesses").at(0); witness.set("source_byte_end", witness.get("source_byte_end").as_int() + 1); }, "exact lexeme/caption spans");
    reject([](auto value) { value.get("source_witnesses").at(0).set("authored_id", "wrong"); }, "witnessed semantic tuple");
    reject([](auto value) { value.get("required_good_first_mutations").at(0).set("node_ordinal", 1); }, "whole selected row");
    reject([](auto value) { value.get("terminal_projection").get("nodes").at(0).set("raw_caption_prefix", "bad"); }, "exact raw reconstruction");
    shapes::Work excluded;
    const auto bang = facts->root().get("cases").at(0);
    const auto bang_source = read(kFixtures / "mermaid_official/cases" / bang.get_string("path"));
    EXPECT_THROW(static_cast<void>(shapes::bind(facts->root(), bang, bang_source, manifest->root(), excluded)), std::exception);
    shapes::Work exhausted; exhausted.left = 0;
    try { static_cast<void>(shapes::validate(frames[0].grid, model, kSentinel, exhausted)); FAIL() << "resource exhaustion must escape"; }
    catch (const std::exception& error) { EXPECT_TRUE(std::string_view(error.what()).starts_with("resource:")); }
}

TEST(MermaidMindmapShapes4, BoundsAndUnsupportedFramingKeepWholeSource) {
    std::vector<std::string> cases{
        "mindmap\nnode0(\n caption\n )junk\n",
        "mindmap\nnode0(\n caption\n extra line\n )\n",
        "mindmap\nnode0((\n caption\n )\n",
        "mindmap\nnode0[\n caption\n ))\n",
        "mindmap\nnode0(\n caption\n )\n ::icon(mdi:fire)\n",
        "mindmap\nnode0(" + std::string(257,'x') + ")\n",
        "mindmap\nnode0(" + std::string(1025,'x') + ")\n"};
    // A valid 16-level tree still obeys the one remaining production budget.
    // Its full shaped canvas exceeds that budget; it must remain exact source.
    std::string deep="mindmap\n";
    for(int n=0;n<16;++n) deep += std::string(static_cast<std::size_t>(n)*2,' ') + "node" + std::to_string(n) + "((x))\n";
    // Source-owned dimensions: max circle width9 => stride13, depth15;
    // height16*9+15*2. Even just Cell's string member exceeds the 1M prepay.
    constexpr std::size_t minimum_canvas_fee = 204u * 174u * (2u * sizeof(std::string) + 16u);
    EXPECT_GT(minimum_canvas_fee, loom::ui::mermaid::testing::kWorkLimit);
    cases.push_back(deep);
    std::string many="mindmap\nnode0(root)\n";
    for(int n=0;n<64;++n) many += "  child" + std::to_string(n) + "\n";
    cases.push_back(many);
    for(const auto& source:cases) {
        SCOPED_TRACE(source.substr(0,120)); mm::Work work;
        const auto frames=observe(source,work,"mermaid-source"); lifecycle(frames);
        EXPECT_TRUE(same_surface(frames[0],frames[3]));
        EXPECT_TRUE(same_surface(frames[1],frames[4]));
    }
}
