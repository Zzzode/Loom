#pragma once
#include "mermaid_flow400_authority.hpp"

namespace loom::test::mermaid_flow {

inline constexpr std::size_t kFlowTitleOrdinal = 11u;
inline constexpr std::size_t kFlowTitleGlobalOrdinal = 672u;
inline constexpr std::string_view kFlowTitleFixture =
    "flowchart/nightly-check/dagre/06-should-render-a-flowchart-with-title.mmd";
inline constexpr std::size_t kFlowTitleSourceBytes = 208u;
inline constexpr std::string_view kFlowTitleSourceSha =
    "de66e2b2ba467e7d70024a299575d8f7c9d0a087b802b0754d75f47e2aca3f32";
inline constexpr std::string_view kFlowTitleSourceGitBlob =
    "ed8d620d9139d60cc1f9089ec876719572146556";
inline constexpr std::string_view kFlowTitleText = "Test Title";
inline constexpr std::string_view kFlowTitleModelJson =
    R"MODEL({"direction":"LR","nodes":[["A",{"id":"A","label":"A","shape":"rectangle"}],["B",{"id":"B","label":"B","shape":"rectangle"}],["C",{"id":"C","label":"C","shape":"rectangle"}]],"edges":[{"source":"A","target":"B","label":"","style":"solid","hasArrowStart":false,"hasArrowEnd":true},{"source":"A","target":"C","label":"","style":"solid","hasArrowStart":false,"hasArrowEnd":true}],"subgraphs":[]})MODEL";
inline constexpr std::string_view kFlowTitleModelSha =
    "f527b9d1d3acaa4ee72f9a8996a8db7462ad07db653bd007f115ac7c9f7c07ad";

inline constexpr std::size_t kFlowTitleMarginOrdinal = 260u;
inline constexpr std::size_t kFlowTitleMarginGlobalOrdinal = 921u;
inline constexpr std::string_view kFlowTitleMarginFixture =
    "flowchart/visual-check/1433-should-render-a-titled-flowchart-with-titletopmargin-set-to-0.mmd";
inline constexpr std::size_t kFlowTitleMarginSourceBytes = 340u;
inline constexpr std::string_view kFlowTitleMarginSourceSha =
    "0f0d5b10ac874d984b33fd4b3eb050039e9bcf02a9e3975e6a04eccae101a948";
inline constexpr std::string_view kFlowTitleMarginSourceGitBlob =
    "cedc5298a6c5b3335c5e4dd586348df8e68115c2";
inline constexpr std::string_view kFlowTitleMarginModelJson =
    R"MODEL({"direction":"TD","nodes":[["A",{"id":"A","label":"A","shape":"rectangle"}],["B",{"id":"B","label":"B","shape":"rectangle"}]],"edges":[{"source":"A","target":"B","label":"","style":"solid","hasArrowStart":false,"hasArrowEnd":true}],"subgraphs":[]})MODEL";
inline constexpr std::string_view kFlowTitleMarginModelSha =
    "94ee82c6c9ffd80ddebd5089693d96d4c5905eb824975d27aa1ab06530891b87";

inline void flow_title_bind(JsonVal fixture, std::size_t ordinal, std::size_t global,
                            std::string_view source, OracleWork& work) {
    work.spend(4096u + 8u * sizeof(JsonVal));
    flow400_require(ordinal == kFlowTitleOrdinal && global == kFlowTitleGlobalOrdinal,
                    "Flow title authority ordinal tuple");
    flow400_require(flow400_text(fixture.get("path")) == kFlowTitleFixture &&
                        flow400_number(fixture.get("bytes")) == kFlowTitleSourceBytes &&
                        flow400_text(fixture.get("sha256")) == kFlowTitleSourceSha &&
                        flow400_text(fixture.get("git_blob")) == kFlowTitleSourceGitBlob,
                    "Flow title Official manifest tuple");
    work.product(source.size(), 4u);
    flow400_require(source.size() == kFlowTitleSourceBytes && digest(source) == kFlowTitleSourceSha,
                    "Flow title complete source binding");
    work.product(kFlowTitleModelJson.size(), 4u);
    flow400_require(digest(kFlowTitleModelJson) == kFlowTitleModelSha,
                    "Flow title canonical Model binding");
}

inline Model flow_title_model(OracleWork& work) {
    work.product(kFlowTitleModelJson.size(), 32u);
    work.spend(4u * sizeof(json::JsonDoc) + 8192u);
    auto document = json::parse(kFlowTitleModelJson);
    flow400_require(document.has_value(), "Flow title Model JSON");
    std::size_t groups = 0u, members = 0u;
    flow400_shape(document->root(), groups, members);
    flow400_require(groups == 0u && members == 0u, "Flow title Model has no subgraphs");
    work.spend(4u * sizeof(Model) + 512u);
    work.product(3u, 8u * sizeof(Node) + 64u);
    work.product(2u, 8u * sizeof(Edge) + 64u);
    return parse_model(document->root());
}

inline void flow_title_margin_bind(JsonVal fixture, std::size_t ordinal, std::size_t global,
                                   std::string_view source, OracleWork& work) {
    work.spend(4096u + 8u * sizeof(JsonVal));
    flow400_require(ordinal == kFlowTitleMarginOrdinal && global == kFlowTitleMarginGlobalOrdinal,
                    "Flow title-margin authority ordinal tuple");
    flow400_require(flow400_text(fixture.get("path")) == kFlowTitleMarginFixture &&
                        flow400_number(fixture.get("bytes")) == kFlowTitleMarginSourceBytes &&
                        flow400_text(fixture.get("sha256")) == kFlowTitleMarginSourceSha &&
                        flow400_text(fixture.get("git_blob")) == kFlowTitleMarginSourceGitBlob,
                    "Flow title-margin Official manifest tuple");
    work.product(source.size(), 4u);
    flow400_require(source.size() == kFlowTitleMarginSourceBytes &&
                        digest(source) == kFlowTitleMarginSourceSha,
                    "Flow title-margin complete source binding");
    work.product(kFlowTitleMarginModelJson.size(), 4u);
    flow400_require(digest(kFlowTitleMarginModelJson) == kFlowTitleMarginModelSha,
                    "Flow title-margin canonical Model binding");
}

inline Model flow_title_margin_model(OracleWork& work) {
    work.product(kFlowTitleMarginModelJson.size(), 32u);
    work.spend(4u * sizeof(json::JsonDoc) + 8192u);
    auto document = json::parse(kFlowTitleMarginModelJson);
    flow400_require(document.has_value(), "Flow title-margin Model JSON");
    std::size_t groups = 0u, members = 0u;
    flow400_shape(document->root(), groups, members);
    flow400_require(groups == 0u && members == 0u, "Flow title-margin Model has no groups");
    work.spend(4u * sizeof(Model) + 512u);
    work.product(2u, 8u * sizeof(Node) + 64u);
    work.product(1u, 8u * sizeof(Edge) + 64u);
    return parse_model(document->root());
}

inline std::vector<std::string> flow_title_geometry(const Grid& grid, const Model& model,
                                                    OracleWork& work,
                                                    std::string_view title = kFlowTitleText,
                                                    std::string_view token = kSentinel) {
    const int following = flow400_following(grid, work, token);
    if (grid.height < 4 || following < 3) return {"Flow title scene is truncated"};
    work.product(static_cast<std::size_t>(grid.width), 2u);
    const auto title_glyphs = ftxui::Utf8ToGlyphs(std::string(title));
    if (title_glyphs.empty() || ftxui::string_width(std::string(title)) <= 0)
        return {"Flow title Model is empty"};
    std::size_t occurrences = 0u;
    for (int y = 0; y < following; ++y) {
        const auto row = grid.row(y);
        std::size_t at = 0u;
        while ((at = row.find(title, at)) != std::string::npos) {
            ++occurrences;
            at += title.size();
        }
    }
    if (occurrences != 1u) return {"Flow diagram title must occur exactly once"};
    const auto first = grid.row(0);
    const auto title_start = first.find(title);
    if (title_start == std::string::npos ||
        first.find(title, title_start + title.size()) != std::string::npos)
        return {"Flow diagram title row differs"};
    for (int x = 0; x < grid.width; ++x) {
        const auto cell = grid.at({x, 0});
        if (x >= static_cast<int>(title_start) &&
            x < static_cast<int>(title_start + title_glyphs.size())) {
            if (cell != title_glyphs[static_cast<std::size_t>(x - static_cast<int>(title_start))])
                return {"Flow diagram title glyph differs"};
        } else if (!cell.empty() && cell != " ") {
            return {"Flow diagram title row has unowned text"};
        }
        work.spend();
    }
    for (int x = 0; x < grid.width; ++x) {
        const auto cell = grid.at({x, 1});
        if (!cell.empty() && cell != " ") return {"Flow diagram title spacer row is not blank"};
        work.spend();
    }
    flow400_copy_grid(grid, work);
    auto graph = grid;
    for (int x = 0; x < grid.width; ++x) {
        graph.put({x, 0}, " ");
        graph.put({x, 1}, " ");
    }
    return flow400_geometry(graph, model, work, token);
}

inline std::size_t flow_title_mutations(const Grid& good, const Model& model, OracleWork& work,
                                        std::string_view token = kSentinel) {
    flow400_require(flow_title_geometry(good, model, work, kFlowTitleText, token).empty(),
                    "Flow title mutation baseline");
    std::size_t rejected = 0u;
    const auto require_rejected = [&](const Grid& frame, const Model& expectation,
                                      std::string_view title) {
        const auto errors = flow_title_geometry(frame, expectation, work, title, token);
        flow400_require(!errors.empty(), "Flow title mutation accepted");
        for (const auto& error : errors)
            flow400_require(!flow400_resource(error),
                            "Flow title resource is not semantic-negative credit");
        ++rejected;
    };
    flow400_copy_grid(good, work);
    auto wrong_title = good;
    const auto title_glyphs = ftxui::Utf8ToGlyphs(std::string(kFlowTitleText));
    flow400_require(title_glyphs.size() == kFlowTitleText.size(), "Flow title ASCII witness");
    wrong_title.put({static_cast<int>(good.row(0).find(kFlowTitleText)), 0}, "X");
    require_rejected(wrong_title, model, kFlowTitleText);

    flow400_copy_grid(good, work);
    auto duplicate_title = good;
    const auto start = static_cast<int>(good.row(0).find(kFlowTitleText));
    for (std::size_t at = 0u; at < title_glyphs.size(); ++at)
        duplicate_title.put({start + static_cast<int>(at), 1}, title_glyphs[at]);
    require_rejected(duplicate_title, model, kFlowTitleText);

    flow400_copy_grid(good, work);
    auto wrong_graph = model;
    wrong_graph.nodes.front().label += "__absent_flow_title";
    require_rejected(good, wrong_graph, kFlowTitleText);

    flow400_copy_grid(good, work);
    auto extra_ink = good;
    extra_ink.put({0, 1}, "§");
    require_rejected(extra_ink, model, kFlowTitleText);
    flow400_require(rejected == 4u, "Flow title exact mutation count");
    return rejected;
}

}  // namespace loom::test::mermaid_flow
