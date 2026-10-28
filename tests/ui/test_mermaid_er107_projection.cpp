// ER107 source-only B2 preproduction preflight. No ER production imports,
// renderer calls, pixels, reports, native-completion claim or admission update.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
import std;
import loom.serdes.json;
import loom.crypto.crypto;
#include "mermaid_er107_authority.hpp"

namespace {
namespace er = loom::test::mermaid_er107;
namespace a = er::authority;
namespace rel = loom::test::mermaid_relations;
namespace json = loom::utils::json;
namespace fs = std::filesystem;
const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures";

std::string read(const fs::path& path, std::size_t cap = er::kMaxFactBytes) {
    a::require(!fs::is_symlink(fs::symlink_status(path)) && fs::is_regular_file(path) &&
                   fs::file_size(path) <= cap,
               "resource: ER107 projection bounded file");
    std::ifstream stream(path, std::ios::binary);
    a::require(stream.good(), "protocol: ER107 projection file open");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

json::JsonDoc parse(std::string_view bytes) {
    auto d = json::parse(bytes);
    a::require(d.has_value(), "protocol: ER107 projection JSON parse");
    return std::move(*d);
}

struct Inputs {
    std::string source_bytes = read(kFixtures / std::string(er::kFactsPath));
    std::string projection_bytes = read(kFixtures / std::string(a::kProjectionPath));
    std::string presentation_bytes = read(kFixtures / std::string(a::kPresentationPath));
    std::string manifest_bytes = read(kFixtures / "mermaid_official/manifest.json");
    std::string prior_bytes = read(kFixtures / std::string(er::kPriorPath));
    json::JsonDoc source = parse(source_bytes), projection = parse(projection_bytes),
                  presentation = parse(presentation_bytes), manifest = parse(manifest_bytes),
                  prior = parse(prior_bytes);
    // Independent root phases, not reusable/reset production Work counters.
    er::OracleWork source_root_work, projection_component_work;

    Inputs() {
        er::OracleWork w;
        a::verify_asset_bytes(source_bytes, projection_bytes, presentation_bytes, manifest_bytes,
                              prior_bytes, w);
    }

    std::string raw(std::size_t i) const {
        return read(kFixtures / "mermaid_official/cases" / std::string(a::kPins[i].path),
                    er::kMaxRawBytes);
    }

    a::BoundCase bind(std::size_t i, std::string_view raw, er::OracleWork& w) const {
        return a::bind_case(source.root(), projection.root(), manifest.root(), prior.root(),
                            a::kPins[i].path, raw, w);
    }
};

template <class F> void rejects(F&& action, std::string_view reason = {}) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception& error) {
        rejected = true;
        EXPECT_FALSE(a::resource(error.what())) << error.what();
        if (!reason.empty())
            EXPECT_NE(std::string_view(error.what()).find(reason), std::string_view::npos)
                << error.what();
    }
    EXPECT_TRUE(rejected) << reason;
}

json::JsonMutDoc copy(json::JsonVal value) {
    json::JsonMutDoc d;
    d.set_root(d.copy_val(value));
    return d;
}

json::JsonDoc immutable(json::JsonMutDoc& d) {
    return parse(d.to_string());
}

a::Surfaces surfaces() {
    a::Surfaces s;
    for (std::size_t i = 1; i < 7; ++i) {
        s.foreground[i] = "actual-fg-" + std::to_string(i);
        s.background[i] = "actual-bg-" + std::to_string(i);
    }
    s.inherited_background = "actual-inherited-bg";
    s.inherited_dim = 2;
    return s;
}

a::ObservedCell cell(std::string glyph, const a::Surfaces& s, int fg = 1, int bg = 1,
                     bool inherited = false) {
    return {std::move(glyph),
            s.foreground[static_cast<std::size_t>(fg)],
            inherited ? s.inherited_background : s.background[static_cast<std::size_t>(bg)],
            false,
            s.inherited_dim,
            0,
            0};
}

a::Surface hand_notice_surface(const a::Projection& p, const a::Surfaces& s) {
    a::Surface out;
    for (const auto& record : p.notice_rows)
        for (const auto& line : record) {
            std::vector<a::ObservedCell> row;
            for (const char c : line.text)
                row.push_back(cell(std::string(1, c), s, 1, 1, true));
            out.rows.push_back(std::move(row));
        }
    return out;
}

struct Primitive {
    rel::Grid glyphs{29, 11, std::vector<std::string>(29u * 11u, " ")};
    rel::Rect group{1, 1, 23, 9, "box"}, table{3, 3, 19, 7, "box"};
    rel::ErTableSpec spec{{"A"}, {{"int", "id", {"PK"}, ""}}};
    std::vector<a::EntityRole> roles{{"A", "g", 6, 3, 0, 0, "Purple", "Orange"}};
    rel::Rect label{12, 2, 12, 2, "label"};
    a::Surface actual;

    explicit Primitive(const a::Surfaces& s) {
        // Hand worksheet: group frame (1,1)..(23,9), one table (3,3)..(19,7).
        // Column borders 3,9,14,19; header rail5; attribute content6.
        for (int x = 2; x < 23; ++x) {
            glyphs.put({x, 1}, "─");
            glyphs.put({x, 9}, "─");
        }
        for (int y = 2; y < 9; ++y) {
            glyphs.put({1, y}, "│");
            glyphs.put({23, y}, "│");
        }
        glyphs.put({1, 1}, "┌");
        glyphs.put({23, 1}, "┐");
        glyphs.put({1, 9}, "└");
        glyphs.put({23, 9}, "┘");
        const std::array<std::string, 5> rows{{"┌───────────────┐", "│       A       │",
                                               "├─────┬────┬────┤", "│ int │ id │ PK │",
                                               "└─────┴────┴────┘"}};
        for (std::size_t y = 0; y < rows.size(); ++y) {
            const auto parts = ftxui::Utf8ToGlyphs(rows[y]);
            a::require(parts.size() == 17, "protocol: ER107 hand table width");
            for (std::size_t x = 0; x < parts.size(); ++x)
                glyphs.put({3 + static_cast<int>(x), 3 + static_cast<int>(y)}, parts[x]);
        }
        glyphs.put({3, 2}, "g");
        glyphs.put({12, 2}, "x");
        // Exactly two mandatory card:one glyphs, reciprocal route arm and a
        // four-arm group gate; this isolated primitive is not a full ER scene.
        glyphs.put({19, 6}, "├");
        glyphs.put({20, 6}, "|");
        glyphs.put({21, 6}, "|");
        glyphs.put({22, 6}, "─");
        glyphs.put({23, 6}, "┼");
        glyphs.put({24, 6}, "─");
        glyphs.put({25, 6}, "─");
        glyphs.put({26, 6}, "─");
        // Independent literal FG/BG worksheet, NOT authority-produced roles.
        for (int y = 0; y < 11; ++y) {
            std::vector<a::ObservedCell> row;
            for (int x = 0; x < 29; ++x) {
                int fg = 1, bg = 1;
                if (x > 1 && x < 23 && y > 1 && y < 9) bg = 6;
                if (x >= 3 && x <= 19 && y >= 3 && y <= 7) {
                    const bool outer = x == 3 || x == 19 || y == 3 || y == 7;
                    const bool inner_rail = y == 5 || (y == 6 && (x == 9 || x == 14));
                    fg = outer || inner_rail ? 6 : 1;
                    bg = outer ? 6 : 3;
                }
                if (y == 2 && x >= 11 && x <= 13) {
                    fg = 1;
                    bg = 1;
                }
                row.push_back(cell(std::string(glyphs.at({x, y})), s, fg, bg));
            }
            actual.rows.push_back(std::move(row));
        }
    }

    void check(const a::Surfaces& s, const a::Surface& candidate) const {
        er::OracleWork w;
        a::validate_primitive(glyphs, candidate, {table}, {group}, roles, {label}, s, w);
    }
};
}  // namespace

TEST(MermaidEr107ProjectionPreflight, All107TypedSourceModelsAndNoticePlanesRemainUnreviewed) {
    Inputs input;
    const auto colors = surfaces();
    std::size_t entities = 0, attrs = 0, edges = 0, groups = 0, categorical = 0, tables = 0,
                notices = 0, notice_rows = 0, stars = 0, generics = 0, blank = 0, notice_A = 0;
    std::set<std::string> geometry_signatures;
    for (std::size_t i = 0; i < a::kPins.size(); ++i) {
        SCOPED_TRACE(a::kPins[i].path);
        const auto raw = input.raw(i);
        er::OracleWork w;
        const auto b = input.bind(i, raw, w);
        const auto& p = b.projection;
        er::OracleWork worksheet_work;
        const auto sheet = a::source_worksheet(p, worksheet_work);
        notice_A += sheet.notice_A;
        EXPECT_EQ(sheet.notice_A, sheet.notice_B);
        EXPECT_GT(sheet.body_A_min, 0u);
        EXPECT_EQ(sheet.primitives.size(), p.scene.entities.size());
        EXPECT_FALSE(sheet.complete_scene_plan_proven);
        EXPECT_FALSE(sheet.complete_M.has_value());
        EXPECT_FALSE(sheet.complete_R.has_value());
        EXPECT_FALSE(sheet.production_new_storage_known);
        geometry_signatures.insert(
            a::canonical(b.terminal_model.get("whole_scene_model"), worksheet_work));
        entities += p.scene.entities.size();
        edges += p.scene.relations.size();
        groups += p.scene.groups.size();
        categorical += p.categorical;
        notices += p.notices.size();
        for (const auto& e : p.scene.entities) {
            if (e.typed_er) {
                ++tables;
                attrs += e.typed_er->attributes.size();
                for (const auto& at : e.typed_er->attributes) {
                    stars += at.name.starts_with('*');
                    generics += at.type == "type<T>";
                }
            }
        }
        for (const auto& e : p.scene.relations)
            blank += e.label.empty();
        for (const auto& record : p.notice_rows)
            notice_rows += record.size();
        const auto good = hand_notice_surface(p, colors);
        er::OracleWork physical;
        a::validate_notice_surface(p, good, colors, physical);
        EXPECT_EQ(b.terminal_case.get_string("review_status"), "UNREVIEWED");
        EXPECT_EQ(b.terminal_case.get_int("drawing_credit"), 0);
        EXPECT_FALSE(b.terminal_model.get("metadata").get("description").get("visible").as_bool());
    }
    EXPECT_EQ(entities, 362u);
    EXPECT_EQ(attrs, 287u);
    EXPECT_EQ(edges, 253u);
    EXPECT_EQ(groups, 1u);
    EXPECT_EQ(categorical, 71u);
    EXPECT_EQ(tables, 117u);
    EXPECT_EQ(notices, 871u);
    EXPECT_EQ(notice_rows, 1592u);
    EXPECT_EQ(stars, 8u);
    EXPECT_EQ(generics, 10u);
    EXPECT_EQ(blank, 3u);
    // Deliberately final: the future B3 adapter must likewise defer all deep
    // binding until its own public captures. This preflight has no such frames.
    a::validate_roots(input.source.root(), input.projection.root(), input.presentation.root(),
                      input.manifest.root(), input.prior.root(), input.source_root_work,
                      input.projection_component_work);
    RecordProperty("source_root_oracle_units",
                   static_cast<int>(a::kRootPhaseWorkCap - input.source_root_work.left));
    RecordProperty("projection_component_oracle_units",
                   static_cast<int>(a::kRootPhaseWorkCap - input.projection_component_work.left));
    const auto contract = read(fs::path(LOOM_TESTS_DIR).parent_path() /
                                   "docs/design/mermaid-er107-terminal-contract.md",
                               262144);
    er::OracleWork doc;
    a::validate_contract_excerpts(input.projection.root(), contract, doc);
    EXPECT_EQ(geometry_signatures.size(), 31u);
    EXPECT_EQ(notice_A, 35416u);
    EXPECT_EQ(a::kDrawingCredit, 0);
    EXPECT_EQ(a::kReviewStatus, "UNREVIEWED");
}

TEST(MermaidEr107ProjectionPreflight,
     GoodFirstSourceSemanticNoticeAndRecomputedDigestMutantsReject) {
    Inputs input;
    const auto raw = input.raw(0);
    er::OracleWork good;
    const auto b = input.bind(0, raw, good);
    const auto palette = input.projection.root().get("palette_source_and_projection");
    er::OracleWork positive;
    EXPECT_NO_THROW(static_cast<void>(
        a::decode_semantics(b.terminal_model, b.source.source_model, palette, positive)));
    // These call the independent semantic decoder without pin checks. They
    // demonstrate source/terminal laws rather than merely rejecting file SHA.
    {
        auto d = copy(b.terminal_model);
        d.root_mut().get("whole_scene_model").get("relations").at(0).set("start", "card:one");
        auto m = immutable(d);
        er::OracleWork w;
        rejects([&] { a::decode_semantics(m.root(), b.source.source_model, palette, w); },
                "cardinality");
    }
    {
        auto d = copy(b.terminal_model);
        d.root_mut().get("entity_presentations").at(0).set("border_role", 1);
        auto m = immutable(d);
        er::OracleWork w;
        rejects([&] { a::decode_semantics(m.root(), b.source.source_model, palette, w); },
                "hue/role");
    }
    {
        auto d = copy(b.terminal_model);
        d.root_mut().get("metadata").get("description").set("visible", true);
        auto m = immutable(d);
        er::OracleWork w;
        rejects([&] { a::decode_semantics(m.root(), b.source.source_model, palette, w); },
                "documentation-only");
    }
    {
        auto d = copy(b.terminal_model);
        d.root_mut().get("notices").at(0).get("rows").at(0).set("joiner", " ");
        auto m = immutable(d);
        er::OracleWork w;
        rejects([&] { a::decode_semantics(m.root(), b.source.source_model, palette, w); },
                "joiner");
    }
    {
        auto d = copy(input.projection.root());
        auto c = d.root_mut().get("cases").at(0);
        c.get("terminal_model")
            .get("whole_scene_model")
            .get("entities")
            .at(0)
            .set("label", "FORGED");
        auto interim = immutable(d);
        er::OracleWork h;
        c.set("terminal_model_sha256",
              er::detail::digest(
                  a::canonical(interim.root().get("cases").at(0).get("terminal_model"), h), h));
        auto m = immutable(d);
        er::OracleWork w;
        rejects(
            [&] {
                a::bind_case(input.source.root(), m.root(), input.manifest.root(),
                             input.prior.root(), a::kPins[0].path, raw, w);
            },
            "immutable projection case");
    }
    {
        er::OracleWork w;
        rejects([&] { input.bind(0, raw + " ", w); }, "complete original");
    }
    for (std::size_t i = 0; i < a::kPins.size(); ++i) {
        bool has_generic = false;
        const auto es = input.source.root().get("cases").at(i).get("source_model").get("entities");
        for (std::size_t n = 0; n < es.size(); ++n) {
            const auto ats = es.at(n).get("attributes");
            for (std::size_t j = 0; j < ats.size(); ++j)
                has_generic |= ats.at(j).get_string("type_literal") == "type~T~";
        }
        if (!has_generic) continue;
        const auto original = input.raw(i);
        er::OracleWork positive_generic;
        const auto source = input.bind(i, original, positive_generic);
        auto doc = copy(source.terminal_model);
        bool changed = false;
        auto ns = doc.root_mut().get("whole_scene_model").get("entities");
        for (std::size_t n = 0; n < es.size() && !changed; ++n) {
            const auto ats = es.at(n).get("attributes");
            for (std::size_t j = 0; j < ats.size() && !changed; ++j)
                if (ats.at(j).get_string("type_literal") == "type~T~") {
                    ns.at(n).get("rows").at(j).get("source_fields").set("type", "type~T~");
                    ns.at(n).get("typed_er").get("attributes").at(j).set("type", "type~T~");
                    changed = true;
                }
        }
        ASSERT_TRUE(changed);
        auto mutant = immutable(doc);
        er::OracleWork w;
        rejects([&] { a::decode_semantics(mutant.root(), source.source.source_model, palette, w); },
                "raw-generic/display");
        break;
    }
    // Notice geometry/attributes are physical synthetic observations, not just
    // semantic mutations or copies of a renderer capture.
    const auto colors = surfaces();
    const auto notice_good = hand_notice_surface(b.projection, colors);
    er::OracleWork pw;
    ASSERT_NO_THROW(a::validate_notice_surface(b.projection, notice_good, colors, pw));
    for (const int kind : {0, 1, 2, 3}) {
        auto mutant = notice_good;
        if (kind == 0) mutant.rows.pop_back();
        if (kind == 1) mutant.rows[0][0].glyph = "X";
        if (kind == 2) mutant.rows[0][0].bold = true;
        if (kind == 3) mutant.rows[0].push_back(cell(" ", colors, 1, 1, true));
        er::OracleWork w;
        rejects([&] { a::validate_notice_surface(b.projection, mutant, colors, w); }, "physical:");
    }
}

TEST(MermaidEr107ProjectionPreflight,
     HandTablePortGateLabelAndBlankPlanesPassBeforePhysicalMutants) {
    const auto colors = surfaces();
    Primitive good(colors);
    rel::Work table_work;
    ASSERT_TRUE(rel::er_table_errors(good.glyphs, good.table, good.spec, table_work).empty());
    const auto port = rel::port(good.glyphs, good.table, 0, {19, 6}, 1);
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(port->marker, "card:one");
    EXPECT_EQ(port->marker_cells.size(), 2u);
    EXPECT_EQ(port->route, (rel::Point{22, 6}));
    const auto gate = rel::port(good.glyphs, good.group, 1, {23, 6}, 1);
    ASSERT_TRUE(gate.has_value());
    EXPECT_EQ(gate->marker, "none");
    ASSERT_NO_THROW(good.check(colors, good.actual));
    er::OracleWork measure;
    const auto counts = a::measure_primitive(good.glyphs, good.actual, {good.table}, {good.group},
                                             good.roles, {good.label}, measure);
    const auto fees = a::minimum_plan_fees(counts, measure);
    EXPECT_EQ(counts.M, 319u);
    EXPECT_EQ(counts.A, 319u);
    EXPECT_EQ(counts.H, 11u);
    EXPECT_EQ(counts.B, 561u);
    EXPECT_EQ(counts.R, 47u);
    EXPECT_EQ(fees.working_plane, 2u * counts.M);
    EXPECT_EQ(fees.crop_scan, 8u * counts.M);
    EXPECT_EQ(fees.output_scan, 8u * counts.A);
    EXPECT_EQ(fees.serialized_planes, 4u * counts.A);
    EXPECT_FALSE(fees.includes_extra_state_capacity_or_writes);
    RecordProperty("synthetic_M", static_cast<int>(counts.M));
    RecordProperty("synthetic_A", static_cast<int>(counts.A));
    RecordProperty("synthetic_B", static_cast<int>(counts.B));
    RecordProperty("synthetic_R", static_cast<int>(counts.R));
    RecordProperty("synthetic_H", static_cast<int>(counts.H));
    RecordProperty("sizeof_string", static_cast<int>(fees.string_size));
    RecordProperty("sizeof_Element", static_cast<int>(fees.element_size));
    RecordProperty("known_fees_only", static_cast<int>(fees.total_known));

    struct Change {
        int x, y, kind;
    };

    const std::array<Change, 13> changes{{{3, 4, 0},
                                          {9, 5, 1},
                                          {19, 6, 2},
                                          {19, 6, 0},
                                          {23, 6, 3},
                                          {22, 6, 3},
                                          {11, 2, 1},
                                          {11, 2, 4},
                                          {4, 6, 1},
                                          {5, 4, 5},
                                          {5, 4, 6},
                                          {5, 4, 7},
                                          {9, 5, 8}}};
    for (const auto& m : changes) {
        SCOPED_TRACE(std::to_string(m.x) + "," + std::to_string(m.y) + "/" +
                     std::to_string(m.kind));
        auto bad = good.actual;
        auto& c = bad.rows[static_cast<std::size_t>(m.y)][static_cast<std::size_t>(m.x)];
        if (m.kind == 0) c.background = colors.background[3];
        if (m.kind == 1) c.background = colors.background[6];
        if (m.kind == 2) c.foreground = colors.foreground[1];
        if (m.kind == 3)
            c.background =
                c.background == colors.background[1] ? colors.background[6] : colors.background[1];
        if (m.kind == 4) c.glyph = "─";
        if (m.kind == 5) c.bold = true;
        if (m.kind == 6) c.dim = 0;
        if (m.kind == 7) c.hyperlink = 1;
        if (m.kind == 8) c.glyph = "─";
        rejects([&] { good.check(colors, bad); }, "physical:");
    }
    auto malformed = good.glyphs;
    malformed.put({9, 5}, "─");
    rel::Work wrong_table;
    EXPECT_FALSE(rel::er_table_errors(malformed, good.table, good.spec, wrong_table).empty());
    auto missing_marker = good.glyphs;
    missing_marker.put({20, 6}, " ");
    EXPECT_FALSE(rel::port(missing_marker, good.table, 0, {19, 6}, 1).has_value());
    EXPECT_EQ(a::kDrawingCredit, 0);  // primitive success is never original credit.
}

TEST(MermaidEr107ProjectionPreflight,
     FiniteDisplayGrammarAndResourcesAreSeparateFromRejectionEvidence) {
    er::OracleWork w;
    EXPECT_EQ(a::generic_display("type~T~", w), "type<T>");
    EXPECT_EQ(a::generic_display("Map_1~Value2~", w), "Map_1<Value2>");
    EXPECT_EQ(a::generic_display("varchar(64)", w), "varchar(64)");
    EXPECT_EQ(a::role_display("  ", w), "");
    for (const auto raw : {"A~B~C~D~", "A~B~C~~", "A~B,C~", "A~B", "~A~", "A<B>~C~"}) {
        er::OracleWork bad;
        rejects([&] { a::generic_display(raw, bad); }, "finite whole-token");
    }
    const std::string phrase = "one-or-more-to-many-non-identifying";
    const auto wrapped = a::wrap(phrase, w);
    ASSERT_EQ(wrapped.size(), 2u);
    EXPECT_EQ(wrapped[0].text.size(), 32u);
    EXPECT_EQ(wrapped[0].joiner, "");
    EXPECT_EQ(wrapped[0].text + wrapped[1].text, phrase);
    const auto spaces = a::wrap(std::string(31, 'a') + "  z", w);
    ASSERT_EQ(spaces.size(), 2u);
    EXPECT_EQ(spaces[0].joiner, " ");
    EXPECT_EQ(spaces[0].text + spaces[0].joiner + spaces[1].text, std::string(31, 'a') + "  z");
    er::OracleWork empty;
    empty.left = 0;
    bool resource = false;
    try {
        a::generic_display("type~T~", empty);
    } catch (const std::exception& e) {
        resource = a::resource(e.what());
    }
    EXPECT_TRUE(resource);  // not a semantic-mutant rejection.
    EXPECT_NE(a::kLimitations.find("complete-scene feasibility remains unproved"),
              std::string_view::npos);
}

namespace {
// Mechanical adaptation of the qualified old AttachesErCardinalitiesToTheCorrectEntities
// and WholeSceneBindsValidatedTablesToRealPortsAndClaimsNoExtraInk fixtures.
// The complete paper is authored here; no producer/pixel/report is consulted.
struct InlinePaper {
    std::string raw_source;  // Authored non-scoring geometry source, not a frozen original.
    rel::Grid grid;
    rel::Model model;
    std::vector<rel::Rect> entities, groups;
    std::vector<a::EntityRole> roles;
    rel::Rect label;
    rel::Point start_border, end_border;
    int direction = 1, contact_row = 2;
    a::Surface actual;
};

void hand_box(rel::Grid& g, const rel::Rect& b, std::string_view name) {
    for (int x = b.l + 1; x < b.r; ++x) {
        g.put({x, b.t}, "─");
        g.put({x, b.b}, "─");
    }
    for (int y = b.t + 1; y < b.b; ++y) {
        g.put({b.l, y}, "│");
        g.put({b.r, y}, "│");
    }
    g.put({b.l, b.t}, "┌");
    g.put({b.r, b.t}, "┐");
    g.put({b.l, b.b}, "└");
    g.put({b.r, b.b}, "┘");
    const int start = b.l + 1 + (b.r - b.l - 1 - static_cast<int>(name.size())) / 2;
    for (std::size_t i = 0; i < name.size(); ++i)
        g.put({start + static_cast<int>(i), b.t + 1}, std::string(1, name[i]));
}

std::string authored_relation(std::string_view start, std::string_view end,
                              std::string_view label) {
    auto side = [](std::string_view card, bool left) -> std::string {
        if (card == "card:one") return "||";
        if (card == "card:zero-one") return left ? "|o" : "o|";
        if (card == "card:zero-many") return left ? "}o" : "o{";
        return left ? "}|" : "|{";
    };
    return "A " + side(start, true) + "--" + side(end, false) + " B : " + std::string(label) + "\n";
}

void hand_cardinality(rel::Grid& g, rel::Point border, int direction, std::string_view card) {
    // Same outward-distance encoding as qualified old self_fixture_marker;
    // old port() recognizes the complete two-glyph cardinality, never a lone o.
    const std::string first = card == "card:zero-one" || card == "card:zero-many" ? "o" : "|";
    const bool many = card == "card:zero-many" || card == "card:one-many";
    const std::string second = many ? (direction == 3 ? "}" : "{") : "|";
    g.put(rel::move(border, direction), first);
    g.put(rel::move(border, direction, 2), second);
}

rel::Entity hand_entity(std::string id, std::string owner = {}) {
    rel::Entity e;
    e.id = id;
    e.label = id;
    e.owner = std::move(owner);
    e.headers = {{id}};
    return e;
}

InlinePaper horizontal_paper(const a::Surfaces& colors, std::string_view start = "card:one",
                             std::string_view end = "card:one", bool reverse = false,
                             bool grouped = false, bool wrapped = false) {
    const std::vector<std::string> labels =
        wrapped ? std::vector<std::string>{"one-or-more-to-many-non-identify", "ing"}
                : std::vector<std::string>{"x"};
    // The first row above is32 cells; concatenation is the35-byte source role.
    // Both rows are independently spelled here, then checked against wrapping.
    er::OracleWork text_work;
    const auto derived = a::wrap(wrapped ? "one-or-more-to-many-non-identifying" : "x", text_work);
    const auto& display = labels;
    a::require(derived.size() == display.size(), "protocol: ER107 hand wrapped row count");
    for (std::size_t i = 0; i < display.size(); ++i)
        a::require(derived[i].text == display[i], "protocol: ER107 hand exact wrapped row");
    const int width = static_cast<int>(display.front().size()), offset = grouped ? 2 : 0,
              right = 18 + width;
    InlinePaper p;
    p.grid = {right + 6 + offset * 2, static_cast<int>(display.size()) + 4 + offset * 2, {}};
    p.grid.cells.assign(static_cast<std::size_t>(p.grid.width * p.grid.height), " ");
    const rel::Rect left{1 + offset, 1 + offset, 5 + offset, 3 + offset, "box"},
        last{right + offset, 1 + offset, right + 4 + offset, 3 + offset, "box"};
    p.entities = reverse ? std::vector<rel::Rect>{last, left} : std::vector<rel::Rect>{left, last};
    p.direction = reverse ? 3 : 1;
    p.model.family = "er";
    p.model.direction = reverse ? "RL" : "LR";
    p.model.entities = {hand_entity("A", grouped ? "g" : ""), hand_entity("B", grouped ? "g" : "")};
    p.raw_source =
        "---\nconfig:\n  look: neo\n---\nerDiagram\ndirection " + p.model.direction + "\n" +
        (grouped ? "subgraph g\nA\nB\nend\n" : "") +
        authored_relation(start, end, wrapped ? "one-or-more-to-many-non-identifying" : "x");
    p.model.relations.push_back(
        {"A", "B", a::multiline(derived), std::string(start), std::string(end), "solid", 0});
    p.roles = {{"A", grouped ? "g" : "", 6, 6, 0, 0, "Purple", "Purple"},
               {"B", grouped ? "g" : "", 5, 5, 1, 1, "Cyan", "Cyan"}};
    if (grouped) {
        p.groups.push_back({1, 1, p.grid.width - 2, p.grid.height - 2, "box"});
        hand_box(p.grid, p.groups.front(), "g");
        p.model.groups.push_back({"g", "g", "", "", {"A", "B"}});
    }
    hand_box(p.grid, p.entities[0], "A");
    hand_box(p.grid, p.entities[1], "B");
    const int y = 2 + offset;
    const auto left_card = reverse ? end : start, right_card = reverse ? start : end;
    p.grid.put({left.r, y}, "├");
    p.grid.put({last.l, y}, "┤");
    hand_cardinality(p.grid, {left.r, y}, 1, left_card);
    hand_cardinality(p.grid, {last.l, y}, 3, right_card);
    for (int x = 8 + offset; x <= 11 + offset; ++x)
        p.grid.put({x, y}, "─");
    for (int x = 12 + width + offset; x <= right - 3 + offset; ++x)
        p.grid.put({x, y}, "─");
    p.label = {12 + offset, y, 11 + width + offset, y + static_cast<int>(display.size()) - 1,
               "label"};
    p.contact_row = y;
    for (std::size_t row = 0; row < display.size(); ++row)
        for (std::size_t x = 0; x < display[row].size(); ++x)
            p.grid.put({p.label.l + static_cast<int>(x), y + static_cast<int>(row)},
                       std::string(1, display[row][x]));
    p.start_border = {reverse ? last.l : left.r, y};
    p.end_border = {reverse ? left.r : last.l, y};
    // Independent full-style worksheet. Source rectangle membership selects
    // fill; inline shaft contacts retain group BG, caption+blank cells root BG.
    for (int row = 0; row < p.grid.height; ++row) {
        std::vector<a::ObservedCell> cells;
        for (int col = 0; col < p.grid.width; ++col) {
            int fg = 1, bg = 1;
            const rel::Point q{col, row};
            if (grouped && p.groups[0].interior(q)) bg = 6;
            for (std::size_t i = 0; i < p.entities.size(); ++i) {
                const auto& b = p.entities[i];
                if (b.boundary(q)) {
                    fg = p.roles[i].foreground;
                    bg = grouped ? 6 : 1;
                } else if (b.interior(q)) {
                    fg = 1;
                    bg = p.roles[i].fill;
                }
            }
            if (row >= p.label.t && row <= p.label.b && col >= p.label.l - 1 &&
                col <= p.label.r + 1) {
                const bool side = col == p.label.l - 1 || col == p.label.r + 1;
                if (!side || row != y) {
                    fg = 1;
                    bg = 1;
                }
            }
            cells.push_back(cell(std::string(p.grid.at(q)), colors, fg, bg));
        }
        p.actual.rows.push_back(std::move(cells));
    }
    return p;
}

InlinePaper vertical_paper(const a::Surfaces& colors, std::string_view start, std::string_view end,
                           bool reverse) {
    InlinePaper p;
    p.grid = {7, 19, std::vector<std::string>(7u * 19u, " ")};
    const rel::Rect upper{1, 1, 5, 3, "box"}, lower{1, 15, 5, 17, "box"};
    p.entities =
        reverse ? std::vector<rel::Rect>{lower, upper} : std::vector<rel::Rect>{upper, lower};
    p.model.family = "er";
    p.model.direction = reverse ? "BT" : "TB";
    p.direction = reverse ? 0 : 2;
    p.model.entities = {hand_entity("A"), hand_entity("B")};
    p.model.relations.push_back({"A", "B", "", std::string(start), std::string(end), "solid", 0});
    p.roles = {{"A", "", 6, 6, 0, 0, "Purple", "Purple"}, {"B", "", 5, 5, 1, 1, "Cyan", "Cyan"}};
    p.raw_source = "---\nconfig:\n  look: neo\n---\nerDiagram\ndirection " + p.model.direction +
                   "\n" + authored_relation(start, end, "\"\"");
    hand_box(p.grid, p.entities[0], "A");
    hand_box(p.grid, p.entities[1], "B");
    p.grid.put({3, 3}, "┬");
    p.grid.put({3, 15}, "┴");
    hand_cardinality(p.grid, {3, 3}, 2, reverse ? end : start);
    hand_cardinality(p.grid, {3, 15}, 0, reverse ? start : end);
    for (int y = 6; y <= 12; ++y)
        p.grid.put({3, y}, "│");
    p.start_border = reverse ? rel::Point{3, 15} : rel::Point{3, 3};
    p.end_border = reverse ? rel::Point{3, 3} : rel::Point{3, 15};
    for (int y = 0; y < 19; ++y) {
        std::vector<a::ObservedCell> row;
        for (int x = 0; x < 7; ++x) {
            int fg = 1, bg = 1;
            for (std::size_t i = 0; i < p.entities.size(); ++i) {
                if (p.entities[i].boundary({x, y}))
                    fg = p.roles[i].foreground;
                else if (p.entities[i].interior({x, y}))
                    bg = p.roles[i].fill;
            }
            row.push_back(cell(std::string(p.grid.at({x, y})), colors, fg, bg));
        }
        p.actual.rows.push_back(std::move(row));
    }
    return p;
}

a::InlineContacts paper_contacts(const InlinePaper& p, rel::ErLabelContactsWitness& issued) {
    rel::Work native;
    const auto errors = rel::validate_impl(p.grid, p.model, native, false, nullptr, false, nullptr,
                                           rel::kRelationsFollowingToken, nullptr, &issued);
    a::require(errors.empty(),
               errors.empty() ? "physical: ER107 scene contact receipt" : errors.front());
    er::OracleWork adapter;
    return a::adapt_label_contacts(p.grid, p.model, issued, p.entities, p.groups, {0, p.label},
                                   adapter);
}

void old_scene_positive(const InlinePaper& p) {
    rel::Work work;
    const auto errors = rel::validate_impl(p.grid, p.model, work);
    for (const auto& e : errors)
        EXPECT_FALSE(a::resource(e)) << e;
    EXPECT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
}

void old_scene_rejects(const rel::Grid& g, const rel::Model& m) {
    rel::Work work;
    const auto errors = rel::validate_impl(g, m, work);
    for (const auto& e : errors)
        EXPECT_FALSE(a::resource(e)) << e;
    EXPECT_FALSE(errors.empty());
}
}  // namespace

TEST(MermaidEr107ProjectionPreflight, QualifiedInlineContactsKeepHostStyleAndOwnedBlankClearance) {
    const auto colors = surfaces();
    auto minimal = horizontal_paper(colors);
    old_scene_positive(minimal);
    EXPECT_NE(minimal.raw_source.find("A ||--|| B : x\n"), std::string::npos);
    EXPECT_EQ(minimal.label.l, 12);
    EXPECT_EQ(minimal.label.t, 2);
    EXPECT_EQ(minimal.grid.at({11, 2}), "─");
    EXPECT_EQ(minimal.grid.at({13, 2}), "─");
    rel::ErLabelContactsWitness issued;
    const auto qualified = paper_contacts(minimal, issued);
    const std::array<a::InlineContacts, 1> contacts{qualified};
    er::OracleWork good;
    ASSERT_NO_THROW(a::validate_primitive(minimal.grid, minimal.actual, minimal.entities,
                                          minimal.groups, minimal.roles, {minimal.label}, colors,
                                          good, contacts));
    er::OracleWork absent;
    rejects(
        [&] {
            a::validate_primitive(minimal.grid, minimal.actual, minimal.entities, minimal.groups,
                                  minimal.roles, {minimal.label}, colors, absent);
        },
        "contact proof");
    // Grouped wrapped-label paper distinguishes hostBG6 from caption/rootBG1.
    auto wrapped = horizontal_paper(colors, "card:zero-one", "card:one-many", false, true, true);
    old_scene_positive(wrapped);
    rel::ErLabelContactsWitness wrapped_issued;
    const auto witness = paper_contacts(wrapped, wrapped_issued);
    const std::array<a::InlineContacts, 1> ws{witness};
    auto validate = [&](const a::Surface& s) {
        er::OracleWork w;
        a::validate_primitive(wrapped.grid, s, wrapped.entities, wrapped.groups, wrapped.roles,
                              {wrapped.label}, colors, w, ws);
    };
    ASSERT_NO_THROW(validate(wrapped.actual));
    const auto left = rel::Point{wrapped.label.l - 1, wrapped.contact_row};
    const auto blank = rel::Point{wrapped.label.l - 1, wrapped.contact_row + 1};
    ASSERT_EQ(wrapped.grid.at(blank), " ");
    EXPECT_EQ(
        wrapped.actual.rows[static_cast<std::size_t>(left.y)][static_cast<std::size_t>(left.x)]
            .background,
        colors.background[6]);
    for (const int kind : {0, 1, 2, 3}) {
        auto bad = wrapped.actual;
        const auto at = kind == 0   ? left
                        : kind == 1 ? blank
                                    : rel::Point{wrapped.label.l, wrapped.label.t};
        auto& c = bad.rows[static_cast<std::size_t>(at.y)][static_cast<std::size_t>(at.x)];
        if (kind == 0) c.background = colors.background[1];
        if (kind == 1 || kind == 2) c.background = colors.background[6];
        if (kind == 3) c.bold = true;
        rejects([&] { validate(bad); }, "physical:");
    }
    auto missing = wrapped.grid;
    missing.put(left, " ");
    old_scene_rejects(missing, wrapped.model);
    rel::ErLabelContactsWitness false_proof;
    rel::Work missing_work;
    const auto missing_errors =
        rel::validate_impl(missing, wrapped.model, missing_work, false, nullptr, false, nullptr,
                           rel::kRelationsFollowingToken, nullptr, &false_proof);
    EXPECT_FALSE(missing_errors.empty());
    EXPECT_FALSE(false_proof.valid());
    er::OracleWork missing_adapter;
    rejects(
        [&] {
            a::adapt_label_contacts(missing, wrapped.model, false_proof, wrapped.entities,
                                    wrapped.groups, {0, wrapped.label}, missing_adapter);
        },
        "receipt required");
    auto extra = wrapped.grid;
    extra.put(blank, "X");
    old_scene_rejects(extra, wrapped.model);
    rel::ErLabelContactsWitness extra_proof;
    rel::Work extra_work;
    const auto extra_errors =
        rel::validate_impl(extra, wrapped.model, extra_work, false, nullptr, false, nullptr,
                           rel::kRelationsFollowingToken, nullptr, &extra_proof);
    EXPECT_FALSE(extra_errors.empty());
    EXPECT_FALSE(extra_proof.valid());
    for (const std::string glyph : {"│", "┼", "o"}) {
        auto corrupted = wrapped.grid;
        corrupted.put(left, glyph);
        rel::ErLabelContactsWitness bad_issued;
        rel::Work old;
        const auto errors =
            rel::validate_impl(corrupted, wrapped.model, old, false, nullptr, false, nullptr,
                               rel::kRelationsFollowingToken, nullptr, &bad_issued);
        EXPECT_FALSE(errors.empty());
        EXPECT_FALSE(bad_issued.valid());
        for (const auto& e : errors)
            EXPECT_FALSE(a::resource(e)) << e;
    }
    auto suffix = wrapped.grid;
    suffix.put({wrapped.label.l, wrapped.label.b}, "X");
    old_scene_rejects(suffix, wrapped.model);
    auto wrong_source = wrapped.model;
    wrong_source.relations[0].label = "one-or-more-to-many-non-identifying";
    old_scene_rejects(wrapped.grid, wrong_source);
    er::OracleWork copy_work;
    rejects(
        [&] {
            a::validate_primitive(extra, wrapped.actual, wrapped.entities, wrapped.groups,
                                  wrapped.roles, {wrapped.label}, colors, copy_work, ws);
        },
        "stale or foreign");
    er::OracleWork overlap_entity;
    rejects(
        [&] {
            a::primitive_roles(wrapped.grid, wrapped.entities, wrapped.groups, wrapped.roles,
                               {{wrapped.entities[0].l + 1, wrapped.entities[0].t + 1,
                                 wrapped.entities[0].l + 1, wrapped.entities[0].t + 1, "label"}},
                               overlap_entity);
        },
        "overwrites entity");
    er::OracleWork overlap_group;
    rejects(
        [&] {
            a::primitive_roles(wrapped.grid, wrapped.entities, wrapped.groups, wrapped.roles,
                               {{wrapped.groups[0].l, wrapped.groups[0].t + 1, wrapped.groups[0].l,
                                 wrapped.groups[0].t + 1, "label"}},
                               overlap_group);
        },
        "overwrites group");
}

TEST(MermaidEr107ProjectionPreflight,
     FourCompleteCardinalitiesKeepBothEndpointsInAllFourDirections) {
    const auto colors = surfaces();
    const std::array<std::string_view, 4> cards{"card:one", "card:zero-one", "card:zero-many",
                                                "card:one-many"};
    for (std::size_t c = 0; c < cards.size(); ++c)
        for (int direction = 0; direction < 4; ++direction) {
            SCOPED_TRACE(std::string(cards[c]) + "/" + std::to_string(direction));
            auto paper =
                direction % 2
                    ? horizontal_paper(colors, cards[c], cards[(c + 1) % 4], direction == 3)
                    : vertical_paper(colors, cards[c], cards[(c + 1) % 4], direction == 0);
            old_scene_positive(paper);
            const auto a_port = rel::port(paper.grid, paper.entities[0], 0, paper.start_border,
                                          direction),
                       b_port = rel::port(paper.grid, paper.entities[1], 1, paper.end_border,
                                          (direction + 2) % 4);
            ASSERT_TRUE(a_port.has_value());
            ASSERT_TRUE(b_port.has_value());
            EXPECT_EQ(a_port->marker, cards[c]);
            EXPECT_EQ(b_port->marker, cards[(c + 1) % 4]);
            EXPECT_EQ(a_port->marker_cells.size(), 2u);
            EXPECT_EQ(b_port->marker_cells.size(), 2u);
            if (direction % 2) {
                rel::ErLabelContactsWitness issued;
                const auto qualified = paper_contacts(paper, issued);
                const std::array<a::InlineContacts, 1> contacts{qualified};
                er::OracleWork w;
                ASSERT_NO_THROW(a::validate_primitive(paper.grid, paper.actual, paper.entities,
                                                      paper.groups, paper.roles, {paper.label},
                                                      colors, w, contacts));
            } else {
                er::OracleWork w;
                ASSERT_NO_THROW(a::validate_primitive(paper.grid, paper.actual, paper.entities,
                                                      paper.groups, paper.roles, {}, colors, w));
            }
            auto wrong = paper.model;
            std::swap(wrong.relations[0].start, wrong.relations[0].end);
            old_scene_rejects(paper.grid, wrong);
            wrong = paper.model;
            wrong.direction = direction == 0   ? "TB"
                              : direction == 2 ? "BT"
                              : direction == 1 ? "RL"
                                               : "LR";
            old_scene_rejects(paper.grid, wrong);
            auto missing = paper.grid;
            missing.put(rel::move(paper.start_border, direction), " ");
            old_scene_rejects(missing, paper.model);
            wrong = paper.model;
            wrong.relations[0].stroke = "dotted";
            old_scene_rejects(paper.grid, wrong);
        }
}

TEST(MermaidEr107ProjectionPreflight, SharedWitnessClearsOnFailureResourceAndPreservesNullPath) {
    const auto colors = surfaces();
    auto paper = horizontal_paper(colors);
    rel::ErLabelContactsWitness issued;
    const auto contact = paper_contacts(paper, issued);
    ASSERT_TRUE(issued.valid());
    ASSERT_EQ(issued.bridges().size(), 1u);
    EXPECT_EQ(issued.grid(), &paper.grid);
    EXPECT_EQ(issued.model(), &paper.model);
    const std::array<a::InlineContacts, 1> contacts{contact};
    auto validate = [&] {
        er::OracleWork w;
        a::validate_primitive(paper.grid, paper.actual, paper.entities, paper.groups, paper.roles,
                              {paper.label}, colors, w, contacts);
    };
    ASSERT_NO_THROW(validate());
    auto broken = paper.grid;
    broken.put({paper.label.l - 1, paper.contact_row}, "│");
    rel::Work failure;
    const auto errors =
        rel::validate_impl(broken, paper.model, failure, false, nullptr, false, nullptr,
                           rel::kRelationsFollowingToken, nullptr, &issued);
    EXPECT_FALSE(errors.empty());
    EXPECT_FALSE(issued.valid());
    EXPECT_EQ(issued.grid(), nullptr);
    EXPECT_EQ(issued.model(), nullptr);
    EXPECT_TRUE(issued.bridges().empty());
    rejects(validate, "cleared or foreign");
    rel::Work positive;
    ASSERT_TRUE(rel::validate_impl(paper.grid, paper.model, positive, false, nullptr, false,
                                   nullptr, rel::kRelationsFollowingToken, nullptr, &issued)
                    .empty());
    ASSERT_TRUE(issued.valid());
    rel::Work exhausted{0};
    bool resource = false;
    try {
        static_cast<void>(rel::validate_impl(paper.grid, paper.model, exhausted, false, nullptr,
                                             false, nullptr, rel::kRelationsFollowingToken, nullptr,
                                             &issued));
    } catch (const std::exception& e) {
        resource = a::resource(e.what());
    }
    EXPECT_TRUE(resource);
    EXPECT_FALSE(issued.valid());
    EXPECT_TRUE(issued.bridges().empty());
    rejects(validate, "cleared or foreign");
    // Exact same native-oracle ledger, not the production factory1M Work cap.
    rel::Work measured;
    ASSERT_TRUE(rel::validate_impl(paper.grid, paper.model, measured, false, nullptr, false,
                                   nullptr, rel::kRelationsFollowingToken, nullptr, &issued)
                    .empty());
    const auto used = 8'000'000u - measured.left;
    ASSERT_GT(used, 0u);
    rel::Work exact{used};
    ASSERT_TRUE(rel::validate_impl(paper.grid, paper.model, exact, false, nullptr, false, nullptr,
                                   rel::kRelationsFollowingToken, nullptr, &issued)
                    .empty());
    EXPECT_EQ(exact.left, 0u);
    EXPECT_TRUE(issued.valid());
    rel::Work short_one{used - 1};
    resource = false;
    try {
        static_cast<void>(rel::validate_impl(paper.grid, paper.model, short_one, false, nullptr,
                                             false, nullptr, rel::kRelationsFollowingToken, nullptr,
                                             &issued));
    } catch (const std::exception& e) {
        resource = a::resource(e.what());
    }
    EXPECT_TRUE(resource);
    EXPECT_FALSE(issued.valid());
    EXPECT_TRUE(issued.bridges().empty());
    rel::Work default_null, explicit_null;
    const auto a_errors = rel::validate_impl(paper.grid, paper.model, default_null);
    const auto b_errors =
        rel::validate_impl(paper.grid, paper.model, explicit_null, false, nullptr, false, nullptr,
                           rel::kRelationsFollowingToken, nullptr, nullptr);
    EXPECT_TRUE(a_errors.empty());
    EXPECT_EQ(a_errors, b_errors);
    EXPECT_EQ(default_null.left, explicit_null.left);
    EXPECT_GT(default_null.left, measured.left);
    auto empty = vertical_paper(colors, "card:zero-one", "card:one-many", false);
    rel::Work empty_work;
    ASSERT_TRUE(rel::validate_impl(empty.grid, empty.model, empty_work, false, nullptr, false,
                                   nullptr, rel::kRelationsFollowingToken, nullptr, &issued)
                    .empty());
    EXPECT_TRUE(issued.valid());
    EXPECT_TRUE(issued.bridges().empty());
    EXPECT_EQ(issued.grid(), &empty.grid);
    EXPECT_EQ(issued.model(), &empty.model);
    auto non_er = paper.model;
    non_er.family = "class";
    rel::Work family_work;
    EXPECT_THROW(rel::validate_impl(paper.grid, non_er, family_work, false, nullptr, false, nullptr,
                                    rel::kRelationsFollowingToken, nullptr, &issued),
                 std::runtime_error);
    EXPECT_FALSE(issued.valid());
    EXPECT_TRUE(issued.bridges().empty());
}

TEST(MermaidEr107ProjectionPreflight, SameTextOnAnotherRelationshipNeverAuthorizesForeignContacts) {
    const auto colors = surfaces();
    const auto top = horizontal_paper(colors), bottom = horizontal_paper(colors);
    rel::Grid grid{25, 11, std::vector<std::string>(25u * 11u, " ")};
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 25; ++x) {
            grid.put({x, y}, std::string(top.grid.at({x, y})));
            grid.put({x, y + 6}, std::string(bottom.grid.at({x, y})));
        }
    grid.put({3, 8}, "C");
    grid.put({21, 8}, "D");
    rel::Model model;
    model.family = "er";
    model.direction = "LR";
    model.entities = {hand_entity("A"), hand_entity("B"), hand_entity("C"), hand_entity("D")};
    model.relations = {{"A", "B", "x", "card:one", "card:one", "solid", 0},
                       {"C", "D", "x", "card:one", "card:one", "solid", 1}};
    const std::vector<rel::Rect> boxes{
        {1, 1, 5, 3, "box"}, {19, 1, 23, 3, "box"}, {1, 7, 5, 9, "box"}, {19, 7, 23, 9, "box"}};
    rel::ErLabelContactsWitness issued;
    rel::Work oracle;
    const auto errors = rel::validate_impl(grid, model, oracle, false, nullptr, false, nullptr,
                                           rel::kRelationsFollowingToken, nullptr, &issued);
    ASSERT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
    ASSERT_TRUE(issued.valid());
    ASSERT_EQ(issued.bridges().size(), 2u);
    er::OracleWork first_work, second_work;
    const auto first = a::adapt_label_contacts(grid, model, issued, boxes, {},
                                               {0, {12, 2, 12, 2, "label"}}, first_work);
    const auto second = a::adapt_label_contacts(grid, model, issued, boxes, {},
                                                {1, {12, 8, 12, 8, "label"}}, second_work);
    EXPECT_EQ(first.relation_index(), 0u);
    EXPECT_EQ(second.relation_index(), 1u);
    er::OracleWork wrong_owner;
    rejects(
        [&] {
            a::adapt_label_contacts(grid, model, issued, boxes, {}, {1, {12, 2, 12, 2, "label"}},
                                    wrong_owner);
        },
        "same-relationship label bridge");
    er::OracleWork wrong_label;
    rejects(
        [&] {
            a::adapt_label_contacts(grid, model, issued, boxes, {}, {0, {12, 8, 12, 8, "label"}},
                                    wrong_label);
        },
        "same-relationship label bridge");
    auto foreign_model = model;
    er::OracleWork wrong_model;
    rejects(
        [&] {
            a::adapt_label_contacts(grid, foreign_model, issued, boxes, {},
                                    {0, {12, 2, 12, 2, "label"}}, wrong_model);
        },
        "same-grid/source-Model");
    auto foreign_grid = grid;
    er::OracleWork wrong_grid;
    rejects(
        [&] {
            a::adapt_label_contacts(foreign_grid, model, issued, boxes, {},
                                    {0, {12, 2, 12, 2, "label"}}, wrong_grid);
        },
        "same-grid/source-Model");
    auto branch = grid;
    branch.put({11, 2}, "┼");
    rel::Work fresh;
    EXPECT_FALSE(rel::validate_impl(branch, model, fresh, false, nullptr, false, nullptr,
                                    rel::kRelationsFollowingToken, nullptr, &issued)
                     .empty());
    EXPECT_FALSE(issued.valid());
    EXPECT_TRUE(issued.bridges().empty());
}
